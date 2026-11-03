#ifndef UART_CMD_H
#define UART_CMD_H

#include <stdint.h>

typedef enum {
    UART_CMD_RATE,     /* arg = requested sample rate in Hz */
    UART_CMD_START,
    UART_CMD_STOP,
    UART_CMD_STATUS,
} uart_cmd_id_t;

typedef struct {
    uart_cmd_id_t id;
    int32_t       arg;
} uart_cmd_t;

/* Creates the RX stream buffer and enables the USART2 receiver interrupt and
 * its NVIC line. Call once, after uart2_init() and before the scheduler. */
void uart_cmd_init(void);

/* Blocks until a complete, recognised command line has been typed, then fills
 * *out and returns 1. Blank, malformed and unrecognised lines are answered by
 * the parser itself and do not return. Call from a single task. */
int uart_cmd_wait(uart_cmd_t *out);

#endif // UART_CMD_H
