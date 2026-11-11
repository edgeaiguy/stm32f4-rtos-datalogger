#ifndef DWT_H
#define DWT_H

#include <stdint.h>
#include "stm32f407xx.h"

#define DWT_CYCLES_PER_US 16u   /* HSI 16 MHz; change with the system clock */

/* Starts the cycle counter. Call once, before the scheduler starts. */
void dwt_init(void);

/* CPU cycles since dwt_init(). Wraps every 2^32 cycles (~268 s at 16 MHz), so
 * compare timestamps with unsigned subtraction. */
static inline uint32_t dwt_cycles(void) {
    return DWT_CYCCNT;
}

#endif // DWT_H
