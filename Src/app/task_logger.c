#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "console.h"
#include "task_logger.h"

#define LOGGER_STACK_WORDS 640
#define LOGGER_Q_DEPTH     32
#define SUMMARY_PERIOD_MS  1000

static StackType_t  logger_stack[LOGGER_STACK_WORDS];
static StaticTask_t logger_tcb;

static uint8_t       q_storage[LOGGER_Q_DEPTH * sizeof(log_msg_t)];
static StaticQueue_t q_buf;
static QueueHandle_t logger_q;

/* One writer per entry (the producer of that type), so no locking. */
static volatile uint32_t drops[LOG_MSG_TYPES];

int logger_post(const log_msg_t *msg) {
    if (xQueueSend(logger_q, msg, 0) == pdTRUE) return 0;
    drops[msg->type]++;
    return -1;
}

/* Counts what arrives and reports once a second; the SD writer replaces this. */
static void logger_task(void *arg) {
    (void)arg;
    uint32_t received[LOG_MSG_TYPES] = {0};
    uint32_t gaps[LOG_MSG_TYPES] = {0};
    uint32_t next_seq[LOG_MSG_TYPES] = {0};
    uint32_t peak = 0;
    TickType_t last_report = xTaskGetTickCount();

    for (;;) {
        log_msg_t m;
        if (xQueueReceive(logger_q, &m, pdMS_TO_TICKS(SUMMARY_PERIOD_MS)) == pdTRUE) {
            uint32_t depth = uxQueueMessagesWaiting(logger_q) + 1;
            if (depth > peak) peak = depth;
            received[m.type]++;
            gaps[m.type] += m.seq - next_seq[m.type];
            next_seq[m.type] = m.seq + 1;
        }

        if (xTaskGetTickCount() - last_report >= pdMS_TO_TICKS(SUMMARY_PERIOD_MS)) {
            last_report += pdMS_TO_TICKS(SUMMARY_PERIOD_MS);
            console_printf("logger: accel %lu (gaps %lu, drops %lu)  env %lu (gaps %lu, drops %lu)  q peak %lu\r\n",
                           (unsigned long)received[LOG_MSG_ACCEL], (unsigned long)gaps[LOG_MSG_ACCEL],
                           (unsigned long)drops[LOG_MSG_ACCEL],
                           (unsigned long)received[LOG_MSG_ENV], (unsigned long)gaps[LOG_MSG_ENV],
                           (unsigned long)drops[LOG_MSG_ENV], (unsigned long)peak);
        }
    }
}

void task_logger_start(void) {
    logger_q = xQueueCreateStatic(LOGGER_Q_DEPTH, sizeof(log_msg_t), q_storage, &q_buf);
    xTaskCreateStatic(logger_task, "logger", LOGGER_STACK_WORDS, NULL, 2, logger_stack, &logger_tcb);
}
