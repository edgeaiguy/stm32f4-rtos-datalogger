#include "dwt.h"

void dwt_init(void) {
    DEMCR |= (1U << 24);
    DWT_CYCCNT = 0;
    DWT_CTRL |= 1U;
}
