#ifndef PROBE_H
#define PROBE_H

#include "stm32f407xx.h"

/* Logic-analyzer probe pins on GPIOD: PD0 rises at the accel read start (the
 * sample instant), PD2 is high while any task other than idle runs, PD3 is high
 * while the logger is inside an SD write or sync. PD4/PD5/PD6 are high while
 * the accel, env or logger task runs (one task at a time). Set
 * PROBE_ENABLED to 0 to compile every probe out. */
#ifndef PROBE_ENABLED
#define PROBE_ENABLED 1
#endif

#if PROBE_ENABLED
#define PROBE_SAMPLE_SET()  (GPIOD_BSRR = (1U << 0))
#define PROBE_SAMPLE_CLR()  (GPIOD_BSRR = (1U << 16))
#define PROBE_BUSY_SET()    (GPIOD_BSRR = (1U << 2))
#define PROBE_BUSY_CLR()    (GPIOD_BSRR = (1U << 18))
#define PROBE_SD_SET()      (GPIOD_BSRR = (1U << 3))
#define PROBE_SD_CLR()      (GPIOD_BSRR = (1U << 19))

/* One store per switch: the running task's pin goes high, the other two low. */
static inline void probe_task_in(char name0) {
    unsigned int on = (name0 == 'a') ? (1U << 4) : (name0 == 'e') ? (1U << 5) : (name0 == 'l') ? (1U << 6) : 0U;
    GPIOD_BSRR = on | ((0x70U & ~on) << 16);
}

/* Configures PD0, PD2 to PD6 as outputs. Call once, before the scheduler starts. */
void probe_init(void);
#else
#define PROBE_SAMPLE_SET()  ((void)0)
#define PROBE_SAMPLE_CLR()  ((void)0)
#define PROBE_BUSY_SET()    ((void)0)
#define PROBE_BUSY_CLR()    ((void)0)
#define PROBE_SD_SET()      ((void)0)
#define PROBE_SD_CLR()      ((void)0)
#define probe_init()        ((void)0)
#define probe_task_in(c)    ((void)0)
#endif

#endif // PROBE_H
