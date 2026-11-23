#include "FreeRTOS.h"
#include "task.h"
#include "console.h"
#include "bmp280.h"
#include "i2c.h"
#include "dwt.h"
#include "task_logger.h"
#include "task_env.h"

#define ENV_STACK_WORDS 192
#define ENV_PERIOD_MS   1000
#define NOT_READY_RETRY 5
#define FAILS_BEFORE_REINIT 3
#define REINIT_EVERY    10

static StackType_t  env_stack[ENV_STACK_WORDS];
static StaticTask_t env_tcb;

/* Sleeps through the conversion so the CPU is free for other tasks; the
 * baro's ~43 ms is the reason this is a task of its own. */
static void env_task(void *arg) {
    bmp280_calib_t *calib = arg;
    TickType_t last_wake = xTaskGetTickCount();
    uint32_t seq = 0;
    uint32_t fails = 0;

    for (;;) {
        int32_t  temp_c100 = 0;
        uint32_t press_q24_8 = 0;
        const char *stage = "trigger";
        int rc = bmp280_trigger();

        if (rc == 0) {
            stage = "fetch";
            vTaskDelay(pdMS_TO_TICKS(BMP280_CONVERSION_MS + 2));
            for (int tries = 0; tries < NOT_READY_RETRY; tries++) {
                rc = bmp280_fetch(calib, &temp_c100, &press_q24_8);
                if (rc != BMP280_NOT_READY) break;
                vTaskDelay(pdMS_TO_TICKS(2));
            }
        }

        fails = (rc == 0) ? 0 : fails + 1;
        if (rc == 0) i2c_clear_fault();

        if (rc == 0) {
            log_msg_t m = { .type = LOG_MSG_ENV, .seq = seq++, .t_cyc = dwt_cycles(),
                            .u.env = { temp_c100, press_q24_8 } };
            logger_post(&m);

            int32_t t = temp_c100;
            const char *sign = (t < 0) ? "-" : "";
            if (t < 0) t = -t;
            uint32_t pa = press_q24_8 >> 8;
            console_printf("env: T=%s%ld.%02ld C  P=%lu.%02lu hPa\r\n",
                           sign, (long)(t / 100), (long)(t % 100),
                           (unsigned long)(pa / 100), (unsigned long)(pa % 100));
        } else {
            const i2c_fault_t *f = i2c_first_fault();
            console_printf("env: BMP280 read failed (%d) i2c n=%lu waiting=0x%lx sr1=0x%lx sr2=0x%lx cr1=0x%lx reg=0x%lx len=%lu stage=%s\r\n",
                           rc, (unsigned long)f->count, (unsigned long)f->flag, (unsigned long)f->sr1,
                           (unsigned long)f->sr2, (unsigned long)f->cr1, (unsigned long)f->reg,
                           (unsigned long)f->len, stage);
            /* The sensor can stop answering while the bus is idle and the accel still
             * works; only a clean bus plus a soft reset brings it back. */
            if (fails >= FAILS_BEFORE_REINIT && (fails - FAILS_BEFORE_REINIT) % REINIT_EVERY == 0) {
                i2c_recover();
                int ri = bmp280_init(calib);
                console_printf("env: BMP280 reinit %s (%d)\r\n", ri == 0 ? "ok" : "failed", ri);
            }
        }

        xTaskDelayUntil(&last_wake, pdMS_TO_TICKS(ENV_PERIOD_MS));
    }
}

void task_env_start(bmp280_calib_t *calib) {
    xTaskCreateStatic(env_task, "env", ENV_STACK_WORDS, calib, 4, env_stack, &env_tcb);
}
