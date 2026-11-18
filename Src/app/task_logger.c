#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "console.h"
#include "datalog.h"
#include "dwt.h"
#include "task_logger.h"

#define LOGGER_STACK_WORDS 288
#define LOGGER_Q_DEPTH     160
#define SUMMARY_PERIOD_MS  5000
#define ERROR_PRINT_EVERY  100

static StackType_t  logger_stack[LOGGER_STACK_WORDS];
static StaticTask_t logger_tcb;

static uint8_t       q_storage[LOGGER_Q_DEPTH * sizeof(log_msg_t)];
static StaticQueue_t q_buf;
static QueueHandle_t logger_q;

static volatile logger_stats_t stats;
static volatile int            want_logging = 1;

/* One writer per entry (the producer of that type), so no locking. */
static volatile uint32_t drops[LOG_MSG_TYPES];

int logger_post(const log_msg_t *msg) {
    if (xQueueSend(logger_q, msg, 0) == pdTRUE) return 0;
    drops[msg->type]++;
    return -1;
}

void logger_set_logging(int on) {
    want_logging = on;
}

void logger_get_stats(logger_stats_t *out) {
    *out = *(const logger_stats_t *)&stats;
    out->accel_drops = drops[LOG_MSG_ACCEL];
    out->env_drops = drops[LOG_MSG_ENV];
}

/* The cycle counter wraps every ~268 s; adding unsigned deltas in a 64-bit total
 * survives that as long as a sample arrives at least once per wrap. */
static uint32_t cycles_to_ms(uint32_t t_cyc) {
    static uint32_t last_cyc;
    static uint64_t total;
    total += (uint32_t)(t_cyc - last_cyc);
    last_cyc = t_cyc;
    return (uint32_t)(total / (DWT_CYCLES_PER_US * 1000u));
}

static void logger_task(void *arg) {
    (void)arg;
    uint32_t received[LOG_MSG_TYPES] = {0};
    uint32_t next_seq[LOG_MSG_TYPES] = {0};
    int logging = 1;
    int env_pending = 0;
    int32_t temp_c100 = 0;
    uint32_t press_q24_8 = 0;
    TickType_t last_report = xTaskGetTickCount();
    TickType_t last_accel_print = last_report;

    stats.logging = 1;

    for (;;) {
        log_msg_t m;
        if (xQueueReceive(logger_q, &m, pdMS_TO_TICKS(SUMMARY_PERIOD_MS)) == pdTRUE) {
            uint32_t depth = uxQueueMessagesWaiting(logger_q) + 1;
            if (depth > stats.queue_peak) stats.queue_peak = depth;
            received[m.type]++;
            uint32_t gap = m.seq - next_seq[m.type];
            if (m.type == LOG_MSG_ACCEL) stats.accel_gaps += gap; else stats.env_gaps += gap;
            next_seq[m.type] = m.seq + 1;

            /* One live sample a second: per-sample output would swamp the UART. */
            if (m.type == LOG_MSG_ACCEL && xTaskGetTickCount() - last_accel_print >= pdMS_TO_TICKS(1000)) {
                last_accel_print = xTaskGetTickCount();
                console_printf("accel: x=%d y=%d z=%d mg\r\n", (m.u.accel.x * 1000) / 256,
                               (m.u.accel.y * 1000) / 256, (m.u.accel.z * 1000) / 256);
            }

            if (m.type == LOG_MSG_ENV) {
                temp_c100 = m.u.env.temp_c100;
                press_q24_8 = m.u.env.press_q24_8;
                env_pending = 1;
            } else if (want_logging) {
                /* An env reading is written once, on the next accel row; later rows
                 * leave those columns empty rather than repeat a stale value. */
                datalog_row_t row = {
                    .t_ms = cycles_to_ms(m.t_cyc),
                    .env_valid = env_pending,
                    .temp_c100 = temp_c100, .press_q24_8 = press_q24_8,
                    .x = m.u.accel.x, .y = m.u.accel.y, .z = m.u.accel.z,
                };
                env_pending = 0;
                int rc = datalog_write_row(&row);
                if (rc == 0) {
                    stats.rows++;
                } else if (stats.write_errors++ % ERROR_PRINT_EVERY == 0) {
                    console_printf("logger: write failed (%d), %lu error(s)\r\n", rc,
                                   (unsigned long)stats.write_errors);
                }
            } else {
                env_pending = 0;
                (void)cycles_to_ms(m.t_cyc);   /* keep the cycle unwrap running while paused */
            }
        }

        /* A pause syncs once so everything written so far is safely on the card. */
        if (logging != want_logging) {
            logging = want_logging;
            stats.logging = logging;
            if (!logging) datalog_sync();
        }

        if (xTaskGetTickCount() - last_report >= pdMS_TO_TICKS(SUMMARY_PERIOD_MS)) {
            last_report += pdMS_TO_TICKS(SUMMARY_PERIOD_MS);
            uint32_t w_us, s_us;
            datalog_get_stall_us(&w_us, &s_us);
            console_printf("logger: rows %lu (errors %lu)  accel gaps %lu drops %lu  env gaps %lu drops %lu  q peak %lu  sd worst w %luus s %luus  stack free %lu\r\n",
                           (unsigned long)stats.rows, (unsigned long)stats.write_errors,
                           (unsigned long)stats.accel_gaps, (unsigned long)drops[LOG_MSG_ACCEL],
                           (unsigned long)stats.env_gaps, (unsigned long)drops[LOG_MSG_ENV],
                           (unsigned long)stats.queue_peak, (unsigned long)w_us, (unsigned long)s_us,
                           (unsigned long)uxTaskGetStackHighWaterMark(NULL));
        }
    }
}

void task_logger_start(void) {
    logger_q = xQueueCreateStatic(LOGGER_Q_DEPTH, sizeof(log_msg_t), q_storage, &q_buf);
    xTaskCreateStatic(logger_task, "logger", LOGGER_STACK_WORDS, NULL, 2, logger_stack, &logger_tcb);
}
