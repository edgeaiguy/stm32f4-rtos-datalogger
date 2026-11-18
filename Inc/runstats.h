#ifndef RUNSTATS_H
#define RUNSTATS_H

#include <stdint.h>
#include "dwt.h"

/* Kernel run-time counter hooks: TIM2 free-runs at 100 kHz (10 us per count). */
void runstats_timer_init(void);

/* Brackets the kernel's tick work: begin runs at the top of xTaskIncrementTick, end in the tick hook. */
extern volatile uint32_t runstats_tick_t0;
#define runstats_tick_begin() (runstats_tick_t0 = dwt_cycles())

/* Prints each task's share of CPU time since boot and its stack headroom. */
void runstats_print(void);

#endif // RUNSTATS_H
