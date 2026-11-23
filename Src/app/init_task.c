#include "FreeRTOS.h"
#include "task.h"
#include "console.h"
#include "i2c.h"
#include "spi.h"
#include "rtc.h"
#include "bmp280.h"
#include "adxl345.h"
#include "sdcard.h"
#include "datalog.h"
#include "task_accel.h"
#include "task_cli.h"
#include "task_env.h"
#include "task_logger.h"
#include "task_infer.h"
#include "task_led.h"
#include "init_task.h"

#define INIT_STACK_WORDS 512

static StackType_t  init_stack[INIT_STACK_WORDS];
static StaticTask_t init_tcb;

static bmp280_calib_t calib;

/* Halts on failure: there is nothing worth logging from a sensor that never answered. */
static void bmp280_bringup(void) {
    console_printf("Initializing BMP280...\r\n");

    uint8_t id = 0;
    if (bmp280_read_id(&id) != 0) {
        console_printf("ERROR: I2C read failed\r\n");
        for (;;) {}
    }

    console_printf("BMP280 ID: 0x%02X (expect 0x%02X)\r\n", id, BMP280_CHIP_ID);
    if (id != BMP280_CHIP_ID) {
        console_printf("Unexpected chip ID\r\n");
        for (;;) {}
    }

    if (bmp280_init(&calib) != 0) {
        console_printf("ERROR: calibration read failed\r\n");
        for (;;) {}
    }

    console_printf("calib: T1=%u T2=%d T3=%d\r\n", calib.dig_T1, calib.dig_T2, calib.dig_T3);
}

/* Five identical reads mean every other slave on the bus is really parked off MISO. */
static int adxl345_devid_ok(void) {
    int stable = 1;
    console_printf("DEVID:");
    for (int i = 0; i < 5; i++) {
        uint8_t id = adxl345_read_register(ADXL345_DEVID_REG);
        console_printf(" 0x%02X", id);
        if (id != ADXL345_DEVID) stable = 0;
    }
    console_printf("   (expect 0x%02X)\r\n", ADXL345_DEVID);
    return stable;
}

/* Retries instead of halting so a terminal attached after reset still sees the failure. */
static void adxl345_bringup(void) {
    console_printf("Initializing ADXL345...\r\n");
    while (!adxl345_devid_ok()) {
        console_printf("ADXL345 not responding - check the CS wire on PE2\r\n");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    adxl345_init();
}

static void sdcard_bringup(void) {
    console_printf("Initializing SD card...\r\n");

    int rc;
    while ((rc = sdcard_init()) != 0) {
        console_printf("sdcard_init failed (%d) - retrying\r\n", rc);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    console_printf("SD card: %s\r\n", sdcard_type_name());

    static uint8_t block[SD_BLOCK_SIZE];
    rc = sdcard_read_block(0, block);
    if (rc != 0) {
        console_printf("ERROR: block 0 read failed (%d)\r\n", rc);
        for (;;) {}
    }
    console_printf("block 0 signature: 0x%02X%02X (expect 0x55AA)\r\n", block[510], block[511]);

    console_printf("post-SD ");
    if (!adxl345_devid_ok()) {
        console_printf("ADXL345 lost after SD init - the two buses are interfering\r\n");
        for (;;) {}
    }
}

/* Opened here, before any producer runs, so startup never shows up as dropped samples. */
static void datalog_bringup(void) {
    int rc;
    while ((rc = datalog_open()) != 0) {
        console_printf("datalog_open failed (%d) - card must be FAT32; retrying\r\n", rc);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    console_printf("logging to %s\r\n", datalog_filename());
}

/* Runs bring-up inside a task because sdcard_init and rtc_init time out on the
 * kernel tick, which does not advance until the scheduler is running. */
static void init_task(void *arg) {
    (void)arg;

    i2c_init();
    spi_init();
    spi2_init();

    console_printf("\r\n");
    if (rtc_init() == RTC_SRC_NONE) {
        console_printf("WARN: no RTC clock source started\r\n");
    }
    rtc_time_t now;
    rtc_now(&now);
    console_printf("RTC: %s%s\r\n", rtc_source_name(),
                   rtc_was_running() ? " (kept running through reset)" : " (seeded from build time)");
    if (rtc_lsi_hz() != 0) console_printf("RTC: LSI divider set for %lu Hz\r\n", (unsigned long)rtc_lsi_hz());
    console_printf("time: %04u-%02u-%02u %02u:%02u:%02u\r\n",
                   now.year, now.month, now.day, now.hour, now.min, now.sec);

    bmp280_bringup();
    adxl345_bringup();
    sdcard_bringup();
    datalog_bringup();

    task_logger_start();
    task_infer_start();
    task_cli_start();
    task_env_start(&calib);
    task_accel_start();
    task_led_start();
    console_printf("bring-up done\r\n");

    vTaskDelete(NULL);
}

void init_task_start(void) {
    xTaskCreateStatic(init_task, "init", INIT_STACK_WORDS, NULL, 6, init_stack, &init_tcb);
}
