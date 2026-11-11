#ifndef PROBE_H
#define PROBE_H

#include "stm32f407xx.h"

/* Logic-analyzer probe pins on GPIOD: PD0 rises at the accel read start (the
 * sample instant), PD2 is high while any task other than idle runs. Set
 * PROBE_ENABLED to 0 to compile every probe out. */
#ifndef PROBE_ENABLED
#define PROBE_ENABLED 1
#endif

#if PROBE_ENABLED
#define PROBE_SAMPLE_SET()  (GPIOD_BSRR = (1U << 0))
#define PROBE_SAMPLE_CLR()  (GPIOD_BSRR = (1U << 16))
#define PROBE_BUSY_SET()    (GPIOD_BSRR = (1U << 2))
#define PROBE_BUSY_CLR()    (GPIOD_BSRR = (1U << 18))

/* Configures PD0 and PD2 as outputs. Call once, before the scheduler starts. */
void probe_init(void);
#else
#define PROBE_SAMPLE_SET()  ((void)0)
#define PROBE_SAMPLE_CLR()  ((void)0)
#define PROBE_BUSY_SET()    ((void)0)
#define PROBE_BUSY_CLR()    ((void)0)
#define probe_init()        ((void)0)
#endif

#endif // PROBE_H
