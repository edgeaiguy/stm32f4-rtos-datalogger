/* uart_cmd.c - USART2 command line for runtime configuration.
 *
 * The RX ISR only pushes one byte into a stream buffer; cli_task blocks on it.
 * Echo, line assembly, parsing and printing all happen in task context, so a
 * slow terminal can never stall an interrupt.
 */
#include <stdint.h>
#include "FreeRTOS.h"
#include "stream_buffer.h"
#include "stm32f407xx.h"
#include "console.h"
#include "uart_cmd.h"

#define RXBUF_SIZE  64u
#define LINE_MAX    40u   /* generous for "rate 4294967295" plus margin */

/* Storage for a stream buffer must be one byte larger than its size. */
static uint8_t              rx_storage[RXBUF_SIZE + 1];
static StaticStreamBuffer_t rx_sb;
static StreamBufferHandle_t rx_handle;
static volatile uint32_t    rx_dropped;   /* bytes lost because the buffer was full */

static char     line[LINE_MAX];
static uint32_t line_len;

void uart_cmd_init(void) {
    rx_handle = xStreamBufferCreateStatic(RXBUF_SIZE, 1, rx_storage, &rx_sb);
    USART2_CR1 |= (1 << 5);            /* RXNEIE: interrupt on RX-not-empty */
    /* Reset priority 0 is above the FreeRTOS syscall ceiling (5); 7 lets the ISR use FromISR APIs. */
    NVIC_IPR_BYTE(38) = 7 << 4;
    NVIC_ISER1 |= (1U << (38 - 32));   /* IRQ38 = USART2, lives in ISER1 (IRQ32-63) */
}

void USART2_IRQHandler(void) {
    if ((USART2_SR & (1 << 7)) && (USART2_CR1 & (1 << 7))) {   /* TXE with TXEIE armed */
        console_tx_isr();
    }
    if (USART2_SR & (1 << 5)) {        /* RXNE */
        /* Reading DR fetches the byte and clears RXNE (and ORE, if set). */
        uint8_t byte = (uint8_t)USART2_DR;
        BaseType_t woken = pdFALSE;
        if (xStreamBufferSendFromISR(rx_handle, &byte, 1, &woken) == 0) {
            rx_dropped++;              /* consumer isn't keeping up; drop, don't block */
        }
        portYIELD_FROM_ISR(woken);
    }
}

static int word_is(const char *word, uint32_t len, const char *kw) {
    uint32_t i = 0;
    while (kw[i]) {
        if (i >= len || word[i] != kw[i]) return 0;
        i++;
    }
    return i == len;
}

/* Decimal only, no sign, no leading/trailing junk - deliberately strict so a
 * typo produces a clear "usage" message instead of a silently wrong number. */
static int parse_uint(const char *s, uint32_t *out) {
    if (*s < '0' || *s > '9') return 0;
    uint32_t v = 0;
    while (*s >= '0' && *s <= '9') {
        v = v * 10 + (uint32_t)(*s - '0');
        s++;
    }
    if (*s != '\0') return 0;
    *out = v;
    return 1;
}

static void print_help(void) {
    console_puts("commands:\r\n"
                        "  rate <hz>   - set sample rate\r\n"
                        "  start       - resume logging\r\n"
                        "  stop        - pause logging (sensors keep running)\r\n"
                        "  status      - print current state\r\n");
}

/* Tokenizes "<keyword> [arg]" out of a NUL-terminated, already-trimmed-of-CR/LF
 * line, and either fills *out and returns 1 (an actionable command), or
 * handles the line itself (help, blank, malformed, unrecognised) and returns
 * 0. Syntax feedback belongs here, next to the grammar it is checking;
 * whether a value is in range is a semantic question for the caller. */
static int handle_line(const char *ln, uart_cmd_t *out) {
    const char *p = ln;
    while (*p == ' ') p++;
    if (*p == '\0') return 0;                    /* blank line: ignore silently */

    const char *kw = p;
    uint32_t kw_len = 0;
    while (kw[kw_len] && kw[kw_len] != ' ') kw_len++;
    p += kw_len;
    while (*p == ' ') p++;                        /* p now points at the argument, if any */

    if (word_is(kw, kw_len, "rate")) {
        uint32_t v;
        if (*p == '\0' || !parse_uint(p, &v)) {
            console_puts("usage: rate <hz>\r\n");
            return 0;
        }
        out->id  = UART_CMD_RATE;
        out->arg = (int32_t)v;
        return 1;
    }
    if (word_is(kw, kw_len, "start"))  { out->id = UART_CMD_START;  return 1; }
    if (word_is(kw, kw_len, "stop"))   { out->id = UART_CMD_STOP;   return 1; }
    if (word_is(kw, kw_len, "status")) { out->id = UART_CMD_STATUS; return 1; }
    if (word_is(kw, kw_len, "help") || word_is(kw, kw_len, "?")) {
        print_help();
        return 0;
    }

    console_puts("unknown command (try 'help')\r\n");
    return 0;
}

int uart_cmd_wait(uart_cmd_t *out) {
    for (;;) {
        uint8_t byte;
        xStreamBufferReceive(rx_handle, &byte, 1, portMAX_DELAY);

        if (rx_dropped) {
            console_printf("\r\n(dropped %u byte(s), input buffer was full)\r\n",
                           (unsigned)rx_dropped);
            rx_dropped = 0;
        }

        if (byte == '\r' || byte == '\n') {
            if (line_len == 0) continue;          /* swallow the \n half of \r\n */
            console_puts("\r\n");
            line[line_len] = '\0';
            int got = handle_line(line, out);
            line_len = 0;
            if (got) return 1;
            continue;
        }

        if (byte == 0x7F || byte == '\b') {        /* DEL or backspace */
            if (line_len > 0) {
                line_len--;
                console_puts("\b \b");             /* erase the character visually */
            }
            continue;
        }

        if (byte < 0x20 || byte > 0x7E) continue;  /* drop other control bytes */

        if (line_len < LINE_MAX - 1) {
            line[line_len++] = (char)byte;
            console_write((const char *)&byte, 1); /* local echo */
        } else {
            console_puts("\r\n(line too long, discarded)\r\n");
            line_len = 0;
        }
    }
}
