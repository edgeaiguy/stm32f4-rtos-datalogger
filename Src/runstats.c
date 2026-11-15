#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "stm32f407xx.h"
#include "console.h"
#include "runstats.h"

#define MAX_TASKS 8
#define TIM_CR1_CEN (1U << 0)
#define TIM_EGR_UG  (1U << 0)

static TaskStatus_t snapshot[MAX_TASKS];

/* A timer far finer than the 1 ms tick, so short task slices are not rounded to zero. */
void runstats_timer_init(void) {
    RCC_APB1ENR |= 1U;
    volatile unsigned int tmp = RCC_APB1ENR; (void)tmp;
    TIM2_PSC = 160 - 1;
    TIM2_ARR = 0xFFFFFFFFu;
    TIM2_EGR = TIM_EGR_UG;
    TIM2_CR1 |= TIM_CR1_CEN;
}

void runstats_print(void) {
    uint32_t total;
    UBaseType_t n = uxTaskGetSystemState(snapshot, MAX_TASKS, &total);
    if (total == 0) return;

    console_puts("task      cpu%  stack free (words)\r\n");
    for (UBaseType_t i = 0; i < n; i++) {
        uint32_t tenths = (uint32_t)((uint64_t)snapshot[i].ulRunTimeCounter * 1000u / total);
        size_t len = strlen(snapshot[i].pcTaskName);
        console_puts(snapshot[i].pcTaskName);
        for (size_t k = len; k < 10; k++) console_puts(" ");
        console_printf("%lu.%lu  %lu\r\n", (unsigned long)(tenths / 10), (unsigned long)(tenths % 10),
                       (unsigned long)snapshot[i].usStackHighWaterMark);
    }
}
