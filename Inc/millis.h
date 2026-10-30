#ifndef MILLIS_H
#define MILLIS_H

#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"

#if configTICK_RATE_HZ != 1000
#error "millis() assumes a 1 ms kernel tick"
#endif

/* Milliseconds from the kernel tick. Frozen until the scheduler starts. */
static inline uint32_t millis(void) {
    return (uint32_t)xTaskGetTickCount();
}

#endif // MILLIS_H
