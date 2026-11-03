#ifndef CONSOLE_H
#define CONSOLE_H

#include <stddef.h>
#include <stdint.h>

/* Creates the TX stream buffer and mutex. Call once, before any other
 * console function and before the scheduler starts. */
void console_init(void);

/* Queue bytes for the USART2 TXE ISR to send. Never waits for the UART: bytes
 * that do not fit are dropped and counted. Task context only. */
void console_write(const char *buf, size_t len);
void console_puts(const char *str);
void console_printf(const char *fmt, ...);

/* Total bytes dropped because the TX buffer was full. */
uint32_t console_dropped(void);

/* Called from USART2_IRQHandler when TXE is set. */
void console_tx_isr(void);

#endif // CONSOLE_H
