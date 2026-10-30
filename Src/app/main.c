#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "stm32f407xx.h"

#define LED_STACK_WORDS 128

typedef struct {
    uint32_t   pin;            /* GPIOD pin: 12 green, 13 orange */
    TickType_t half_period;
} blink_cfg_t;

static blink_cfg_t green  = { 12, pdMS_TO_TICKS(500) };
static blink_cfg_t orange = { 13, pdMS_TO_TICKS(100) };

static StackType_t  green_stack[LED_STACK_WORDS];
static StackType_t  orange_stack[LED_STACK_WORDS];
static StaticTask_t green_tcb;
static StaticTask_t orange_tcb;

static void led_task(void *arg) {
    const blink_cfg_t *cfg = arg;
    TickType_t last_wake = xTaskGetTickCount();
    int on = 0;

    for (;;) {
        /* BSRR sets or clears one pin atomically, so neither task can undo the other's
         * write the way a preempted read-modify-write of ODR could. */
        GPIOD_BSRR = on ? (1U << (cfg->pin + 16)) : (1U << cfg->pin);
        on = !on;
        xTaskDelayUntil(&last_wake, cfg->half_period);
    }
}

int main(void) {
    RCC_AHB1ENR |= RCC_AHB1ENR_GPIODEN;
    (void)RCC_AHB1ENR;
    GPIOD_MODER = (GPIOD_MODER & ~(0xFU << 24)) | (0x5U << 24);   /* PD12, PD13 output */

    xTaskCreateStatic(led_task, "green", LED_STACK_WORDS, &green, 1, green_stack, &green_tcb);
    xTaskCreateStatic(led_task, "orange", LED_STACK_WORDS, &orange, 2, orange_stack, &orange_tcb);

    vTaskStartScheduler();
    for (;;) {}   /* only reached if the scheduler fails to start */
}
