#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "stm32f407xx.h"
#include "console.h"
#include "adxl345.h"
#include "dwt.h"
#include "probe.h"
#include "task_logger.h"
#include "task_infer.h"
#include "task_accel.h"

#define ACCEL_STACK_WORDS 96
#define EXTI1_IRQN        7
#define EXTI1_PRIORITY    5     /* the syscall ceiling: lowest value allowed to call FromISR */
#define RATE_NOTIFY_INDEX 1     /* index 0 carries the INT1 wake-ups */
#define DEFAULT_RATE_IDX  3     /* 5 Hz, as in v3.0 */
#define WAKE_TIMEOUT_MS   1000

static StackType_t  accel_stack[ACCEL_STACK_WORDS];
static StaticTask_t accel_tcb;
static TaskHandle_t accel_handle;

/* The sensor only offers fixed output rates, so each user rate runs the lowest
 * ODR it divides exactly and forwards every decimate-th sample. */
static const struct { uint8_t hz; uint8_t bw_rate; uint8_t decimate; } rates[] = {
    {   1, ADXL345_ODR_25HZ,  25 }, {   2, ADXL345_ODR_50HZ,  25 }, {   4, ADXL345_ODR_100HZ, 25 },
    {   5, ADXL345_ODR_25HZ,   5 }, {  10, ADXL345_ODR_50HZ,   5 }, {  20, ADXL345_ODR_100HZ,  5 },
    {  25, ADXL345_ODR_25HZ,   1 }, {  50, ADXL345_ODR_50HZ,   1 }, { 100, ADXL345_ODR_100HZ,  1 },
};
#define N_RATES (sizeof rates / sizeof rates[0])

static volatile uint32_t current_rate_hz;

uint32_t task_accel_rate(void) {
    return current_rate_hz;
}

void task_accel_print_rates(void) {
    for (unsigned i = 0; i < N_RATES; i++) console_printf(" %u", rates[i].hz);
}

int task_accel_set_rate(uint32_t hz) {
    for (unsigned i = 0; i < N_RATES; i++) {
        if (rates[i].hz == hz) {
            xTaskNotifyIndexed(accel_handle, RATE_NOTIFY_INDEX, i, eSetValueWithOverwrite);
            return 0;
        }
    }
    return -1;
}

/* INT1 stays high until the XYZ registers are read, so a missed edge would
 * stall the task forever; the take timeout and the read it triggers recover. */
static void accel_task(void *arg) {
    (void)arg;
    int16_t x, y, z;
    uint32_t count = 0;
    uint32_t seq = 0;
    unsigned rate_idx = DEFAULT_RATE_IDX;

    current_rate_hz = rates[rate_idx].hz;
    adxl345_start_data_ready(rates[rate_idx].bw_rate);
    adxl345_read_xyz(&x, &y, &z);

    for (;;) {
        uint32_t n = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(WAKE_TIMEOUT_MS));
        if (n == 0) {
            console_printf("accel: no DATA_READY edge in %u ms\r\n", WAKE_TIMEOUT_MS);
        } else if (n > 1) {
            console_printf("accel: %lu sample(s) missed\r\n", (unsigned long)(n - 1));
        }

        uint32_t new_idx;
        if (xTaskNotifyWaitIndexed(RATE_NOTIFY_INDEX, 0, 0xFFFFFFFFu, &new_idx, 0) == pdTRUE) {
            rate_idx = new_idx;
            adxl345_start_data_ready(rates[rate_idx].bw_rate);
            current_rate_hz = rates[rate_idx].hz;
            count = 0;
        }

        uint32_t t_cyc = dwt_cycles();
        PROBE_SAMPLE_SET();
        adxl345_read_xyz(&x, &y, &z);
        PROBE_SAMPLE_CLR();
        if (n == 0 || ++count % rates[rate_idx].decimate != 0) continue;

        log_msg_t m = { .type = LOG_MSG_ACCEL, .seq = seq++, .t_cyc = t_cyc,
                        .u.accel = { x, y, z } };
        logger_post(&m);
        infer_post(&m);
    }
}

void EXTI1_IRQHandler(void) {
    EXTI_PR = (1U << 1);   /* write 1 to clear the pending bit */
    BaseType_t woken = pdFALSE;
    vTaskNotifyGiveFromISR(accel_handle, &woken);
    portYIELD_FROM_ISR(woken);
}

void task_accel_start(void) {
    accel_handle = xTaskCreateStatic(accel_task, "accel", ACCEL_STACK_WORDS, NULL, 5,
                                     accel_stack, &accel_tcb);

    RCC_AHB1ENR |= RCC_AHB1ENR_GPIODEN;
    RCC_APB2ENR |= RCC_APB2ENR_SYSCFGEN;
    (void)RCC_APB2ENR;

    GPIOD_MODER &= ~(0x3U << 2);                         /* PD1 input */
    GPIOD_PUPDR = (GPIOD_PUPDR & ~(0x3U << 2)) | (0x2U << 2);   /* pull-down so an unwired pin stays quiet */
    SYSCFG_EXTICR1 = (SYSCFG_EXTICR1 & ~(0xFU << 4)) | (0x3U << 4);   /* line 1 <- port D */
    EXTI_RTSR |= (1U << 1);
    EXTI_IMR  |= (1U << 1);

    NVIC_IPR_BYTE(EXTI1_IRQN) = EXTI1_PRIORITY << 4;
    NVIC_ISER0 |= (1U << EXTI1_IRQN);
}
