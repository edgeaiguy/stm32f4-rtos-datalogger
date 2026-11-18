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

volatile uint32_t runstats_tick_t0;
static volatile uint32_t tick_count, tick_max;
static volatile uint64_t tick_sum;

/* Runs inside the tick interrupt; the ISR's own entry and exit are not counted. */
void vApplicationTickHook(void) {
    uint32_t d = dwt_cycles() - runstats_tick_t0;
    tick_sum += d;
    tick_count++;
    if (d > tick_max) tick_max = d;
}


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

    uint32_t cnt = tick_count;
    if (cnt == 0) return;
    uint32_t avg = (uint32_t)(tick_sum / cnt);
    uint32_t hundredths = avg * 100u / 160u;
    console_printf("tick: avg %lu cycles, max %lu cycles, %lu.%02lu%% of CPU at 1 kHz\r\n",
                   (unsigned long)avg, (unsigned long)tick_max,
                   (unsigned long)(hundredths / 100), (unsigned long)(hundredths % 100));
}
