#ifndef RUNSTATS_H
#define RUNSTATS_H

#include <stdint.h>

/* Kernel run-time counter hooks: TIM2 free-runs at 100 kHz (10 us per count). */
void runstats_timer_init(void);

/* Prints each task's share of CPU time since boot and its stack headroom. */
void runstats_print(void);

#endif // RUNSTATS_H
