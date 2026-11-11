#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "stm32f407xx.h"
#include "console.h"
#include "adxl345.h"
#include "task_accel.h"

#define ACCEL_STACK_WORDS 256
#define EXTI1_IRQN        7
#define EXTI1_PRIORITY    5     /* the syscall ceiling: lowest value allowed to call FromISR */
#define PRINT_DECIMATE    5     /* 100 Hz ODR / 5 = 20 Hz of console output */
#define WAKE_TIMEOUT_MS   1000

static StackType_t  accel_stack[ACCEL_STACK_WORDS];
static StaticTask_t accel_tcb;
static TaskHandle_t accel_handle;

/* INT1 stays high until the XYZ registers are read, so a missed edge would
 * stall the task forever; the take timeout and the read it triggers recover. */
static void accel_task(void *arg) {
    (void)arg;
    int16_t x, y, z;
    uint32_t count = 0;
    uint32_t late = 0;

    adxl345_start_data_ready(ADXL345_ODR_100HZ);
    adxl345_read_xyz(&x, &y, &z);

    for (;;) {
        uint32_t n = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(WAKE_TIMEOUT_MS));
        if (n == 0) {
            console_printf("accel: no DATA_READY edge in %u ms\r\n", WAKE_TIMEOUT_MS);
        } else if (n > 1) {
            late += n - 1;
        }

        adxl345_read_xyz(&x, &y, &z);
        if (n == 0 || ++count % PRINT_DECIMATE != 0) continue;

        console_printf("accel: X:%5d Y:%5d Z:%5d mg  late:%lu\r\n",
                       (x * 1000) / 256, (y * 1000) / 256, (z * 1000) / 256,
                       (unsigned long)late);
    }
}

void EXTI1_IRQHandler(void) {
    EXTI_PR = (1U << 1);   /* write 1 to clear the pending bit */
    BaseType_t woken = pdFALSE;
    vTaskNotifyGiveFromISR(accel_handle, &woken);
    portYIELD_FROM_ISR(woken);
}

void task_accel_start(void) {
    accel_handle = xTaskCreateStatic(accel_task, "accel", ACCEL_STACK_WORDS, NULL, 4,
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
