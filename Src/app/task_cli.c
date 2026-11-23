#include "FreeRTOS.h"
#include "task.h"
#include "console.h"
#include "datalog.h"
#include "i2c.h"
#include "rtc.h"
#include "runstats.h"
#include "task_accel.h"
#include "task_logger.h"
#include "task_infer.h"
#include "uart_cmd.h"
#include "task_cli.h"

#define CLI_STACK_WORDS 208

static StackType_t  cli_stack[CLI_STACK_WORDS];
static StaticTask_t cli_tcb;

static void print_status(void) {
    logger_stats_t st;
    logger_get_stats(&st);
    rtc_time_t now;
    rtc_now(&now);

    console_printf("state: %s\r\n", st.logging ? "running" : "stopped");
    console_printf("rate: %lu Hz\r\n", (unsigned long)task_accel_rate());
    console_printf("file: %s\r\n", datalog_filename());
    console_printf("rows: %lu  write errors: %lu\r\n", (unsigned long)st.rows, (unsigned long)st.write_errors);
    uint32_t w_us, s_us;
    datalog_get_stall_us(&w_us, &s_us);
    console_printf("sd worst: write %lu us  sync %lu us\r\n", (unsigned long)w_us, (unsigned long)s_us);
    console_printf("accel gaps: %lu  drops: %lu   env gaps: %lu  drops: %lu   queue peak: %lu\r\n",
                   (unsigned long)st.accel_gaps, (unsigned long)st.accel_drops,
                   (unsigned long)st.env_gaps, (unsigned long)st.env_drops,
                   (unsigned long)st.queue_peak);
    infer_stats_t is;
    infer_get_stats(&is);
    console_printf("infer: %s  windows: %lu  rms x %ld y %ld z %ld mg  gaps: %lu  drops: %lu  queue peak: %lu\r\n",
                   is.enabled ? "on" : "off", (unsigned long)is.windows, (long)is.rms_mg[0],
                   (long)is.rms_mg[1], (long)is.rms_mg[2], (unsigned long)is.gaps,
                   (unsigned long)is.drops, (unsigned long)is.queue_peak);
    console_printf("i2c recoveries: %lu\r\n", (unsigned long)i2c_recoveries());
    console_printf("time: %04u-%02u-%02u %02u:%02u:%02u\r\n",
                   now.year, now.month, now.day, now.hour, now.min, now.sec);
}

static void set_time(int32_t date, int32_t tod) {
    rtc_time_t t = {
        .year = (uint16_t)(date / 10000), .month = (uint8_t)(date / 100 % 100), .day = (uint8_t)(date % 100),
        .hour = (uint8_t)(tod / 10000),   .min = (uint8_t)(tod / 100 % 100),    .sec = (uint8_t)(tod % 100),
    };
    if (t.year < 2000 || t.year > 2099 || t.month < 1 || t.month > 12 || t.day < 1 || t.day > 31 ||
        t.hour > 23 || t.min > 59 || t.sec > 59) {
        console_puts("time out of range\r\n");
        return;
    }
    if (rtc_set(&t) != 0) {
        console_puts("time: RTC did not accept the update\r\n");
        return;
    }
    console_printf("time set to %04u-%02u-%02u %02u:%02u:%02u\r\n",
                   t.year, t.month, t.day, t.hour, t.min, t.sec);
}

/* Blocks inside uart_cmd_wait, so the task costs no CPU between commands. */
static void cli_task(void *arg) {
    (void)arg;
    uart_cmd_t cmd;
    console_puts("cli ready\r\n");
    for (;;) {
        if (!uart_cmd_wait(&cmd)) continue;

        switch (cmd.id) {
        case UART_CMD_RATE:
            if (cmd.arg > 0 && task_accel_set_rate((uint32_t)cmd.arg) == 0) {
                console_printf("rate set to %ld Hz\r\n", (long)cmd.arg);
            } else {
                console_puts("rate must be one of:");
                task_accel_print_rates();
                console_puts("\r\n");
            }
            break;
        case UART_CMD_START:
            logger_set_logging(1);
            console_printf("logging resumed -> %s\r\n", datalog_filename());
            break;
        case UART_CMD_STOP:
            logger_set_logging(0);
            console_puts("logging paused (sensors keep running)\r\n");
            break;
        case UART_CMD_STATUS:
            print_status();
            break;
        case UART_CMD_STATS:
            runstats_print();
            break;
        case UART_CMD_INFER:
            infer_set_enabled(cmd.arg);
            console_printf("infer %s\r\n", cmd.arg ? "on" : "off");
            break;
        case UART_CMD_TIME:
            set_time(cmd.arg, cmd.arg2);
            break;
        }
    }
}

void task_cli_start(void) {
    xTaskCreateStatic(cli_task, "cli", CLI_STACK_WORDS, NULL, 1, cli_stack, &cli_tcb);
}
