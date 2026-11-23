#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "task_infer.h"

#define INFER_STACK_WORDS 112
#define INFER_Q_DEPTH     32
#define WINDOW_SAMPLES    64

static StackType_t  infer_stack[INFER_STACK_WORDS];
static StaticTask_t infer_tcb;

static uint8_t       q_storage[INFER_Q_DEPTH * sizeof(log_msg_t)];
static StaticQueue_t q_buf;
static QueueHandle_t infer_q;

static volatile infer_stats_t stats;
static volatile int           enabled = 1;
static volatile uint32_t      drops;   /* written by the producer only */

int infer_post(const log_msg_t *msg) {
    if (xQueueSend(infer_q, msg, 0) == pdTRUE) return 0;
    drops++;
    return -1;
}

void infer_set_enabled(int on) {
    enabled = on;
}

void infer_get_stats(infer_stats_t *out) {
    *out = *(const infer_stats_t *)&stats;
    out->drops = drops;
    out->enabled = enabled;
}

static uint32_t isqrt64(uint64_t v) {
    uint64_t r = 0, bit = 1ULL << 62;
    while (bit > v) bit >>= 2;
    while (bit) {
        if (v >= r + bit) { v -= r + bit; r = (r >> 1) + bit; } else { r >>= 1; }
        bit >>= 2;
    }
    return (uint32_t)r;
}

static void infer_task(void *arg) {
    (void)arg;
    uint64_t sumsq[3] = {0, 0, 0};
    uint32_t n = 0, next_seq = 0;

    for (;;) {
        if (!enabled) { vTaskDelay(pdMS_TO_TICKS(100)); continue; }

        log_msg_t m;
        if (xQueueReceive(infer_q, &m, pdMS_TO_TICKS(100)) != pdTRUE) continue;

        uint32_t depth = uxQueueMessagesWaiting(infer_q) + 1;
        if (depth > stats.queue_peak) stats.queue_peak = depth;
        stats.gaps += m.seq - next_seq;
        next_seq = m.seq + 1;

        int32_t v[3] = { m.u.accel.x, m.u.accel.y, m.u.accel.z };
        for (int i = 0; i < 3; i++) sumsq[i] += (uint64_t)((int64_t)v[i] * v[i]);

        if (++n == WINDOW_SAMPLES) {
            for (int i = 0; i < 3; i++) {
                stats.rms_mg[i] = (int32_t)((isqrt64(sumsq[i] / WINDOW_SAMPLES) * 1000u) / 256u);
                sumsq[i] = 0;
            }
            stats.windows++;
            n = 0;
        }
    }
}

void task_infer_start(void) {
    infer_q = xQueueCreateStatic(INFER_Q_DEPTH, sizeof(log_msg_t), q_storage, &q_buf);
    xTaskCreateStatic(infer_task, "infer", INFER_STACK_WORDS, NULL, 3, infer_stack, &infer_tcb);
}
