#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "console.h"
#include "datalog.h"
#include "dwt.h"
#include "task_logger.h"

#define LOGGER_STACK_WORDS 640
#define LOGGER_Q_DEPTH     32
#define SUMMARY_PERIOD_MS  5000
#define ERROR_PRINT_EVERY  100

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
    uint32_t gaps[LOG_MSG_TYPES] = {0};
    uint32_t next_seq[LOG_MSG_TYPES] = {0};
    uint32_t peak = 0, rows = 0, write_errors = 0;
    int env_pending = 0;
    int32_t temp_c100 = 0;
    uint32_t press_q24_8 = 0;
    TickType_t last_report = xTaskGetTickCount();

    for (;;) {
        log_msg_t m;
        if (xQueueReceive(logger_q, &m, pdMS_TO_TICKS(SUMMARY_PERIOD_MS)) == pdTRUE) {
            uint32_t depth = uxQueueMessagesWaiting(logger_q) + 1;
            if (depth > peak) peak = depth;
            received[m.type]++;
            gaps[m.type] += m.seq - next_seq[m.type];
            next_seq[m.type] = m.seq + 1;

            if (m.type == LOG_MSG_ENV) {
                temp_c100 = m.u.env.temp_c100;
                press_q24_8 = m.u.env.press_q24_8;
                env_pending = 1;
            } else {
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
                    rows++;
                } else if (write_errors++ % ERROR_PRINT_EVERY == 0) {
                    console_printf("logger: write failed (%d), %lu error(s)\r\n", rc,
                                   (unsigned long)write_errors);
                }
            }
        }

        if (xTaskGetTickCount() - last_report >= pdMS_TO_TICKS(SUMMARY_PERIOD_MS)) {
            last_report += pdMS_TO_TICKS(SUMMARY_PERIOD_MS);
            console_printf("logger: rows %lu (errors %lu)  accel gaps %lu drops %lu  env gaps %lu drops %lu  q peak %lu  stack free %lu\r\n",
                           (unsigned long)rows, (unsigned long)write_errors,
                           (unsigned long)gaps[LOG_MSG_ACCEL], (unsigned long)drops[LOG_MSG_ACCEL],
                           (unsigned long)gaps[LOG_MSG_ENV], (unsigned long)drops[LOG_MSG_ENV],
                           (unsigned long)peak, (unsigned long)uxTaskGetStackHighWaterMark(NULL));
        }
    }
}

void task_logger_start(void) {
    logger_q = xQueueCreateStatic(LOGGER_Q_DEPTH, sizeof(log_msg_t), q_storage, &q_buf);
    xTaskCreateStatic(logger_task, "logger", LOGGER_STACK_WORDS, NULL, 2, logger_stack, &logger_tcb);
}
