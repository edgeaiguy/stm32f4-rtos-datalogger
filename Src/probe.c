#include "probe.h"

#if PROBE_ENABLED
void probe_init(void) {
    RCC_AHB1ENR |= RCC_AHB1ENR_GPIODEN;
    (void)RCC_AHB1ENR;
    GPIOD_MODER = (GPIOD_MODER & ~((0x3U << 0) | (0x3U << 4) | (0x3U << 6) |
                               (0x3U << 8) | (0x3U << 10) | (0x3U << 12))) |
                  (0x1U << 0) | (0x1U << 4) | (0x1U << 6) |
                  (0x1U << 8) | (0x1U << 10) | (0x1U << 12);
}
#endif
