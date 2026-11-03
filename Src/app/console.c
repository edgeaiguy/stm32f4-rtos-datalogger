#include <stdarg.h>
#include <string.h>
#include "FreeRTOS.h"
#include "semphr.h"
#include "stream_buffer.h"
#include "stm32f407xx.h"
#include "console.h"
#include "fmt.h"

#define TX_BUF_SIZE 256u
#define USART_SR_TXE   (1U << 7)
#define USART_CR1_TXEIE (1U << 7)

/* A stream buffer's storage array must be one byte larger than its size. */
static uint8_t            tx_storage[TX_BUF_SIZE + 1];
static StaticStreamBuffer_t tx_sb;
static StreamBufferHandle_t tx_handle;

static StaticSemaphore_t  tx_mutex_buf;
static SemaphoreHandle_t  tx_mutex;

static uint32_t dropped;

void console_init(void) {
    tx_handle = xStreamBufferCreateStatic(TX_BUF_SIZE, 1, tx_storage, &tx_sb);
    tx_mutex  = xSemaphoreCreateMutexStatic(&tx_mutex_buf);
}

/* Caller holds tx_mutex: a stream buffer allows only one writer at a time. */
static void put(const char *buf, size_t len) {
    size_t sent = xStreamBufferSend(tx_handle, buf, len, 0);
    dropped += (uint32_t)(len - sent);
}

/* The ISR clears TXEIE when the buffer runs dry, so every write re-arms it. */
static void kick(void) {
    USART2_CR1 |= USART_CR1_TXEIE;
}

void console_write(const char *buf, size_t len) {
    xSemaphoreTake(tx_mutex, portMAX_DELAY);
    put(buf, len);
    kick();
    xSemaphoreGive(tx_mutex);
}

void console_puts(const char *str) {
    console_write(str, strlen(str));
}

static void emit(char c, void *ctx) {
    (void)ctx;
    put(&c, 1);
}

/* The mutex spans the whole format so concurrent lines cannot interleave. */
void console_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    xSemaphoreTake(tx_mutex, portMAX_DELAY);
    fmt_vformat(emit, NULL, fmt, args);
    kick();
    xSemaphoreGive(tx_mutex);
    va_end(args);
}

uint32_t console_dropped(void) {
    return dropped;
}

void console_tx_isr(void) {
    BaseType_t woken = pdFALSE;
    uint8_t byte;
    if (xStreamBufferReceiveFromISR(tx_handle, &byte, 1, &woken) == 1) {
        USART2_DR = byte;
    } else {
        USART2_CR1 &= ~USART_CR1_TXEIE;
    }
    portYIELD_FROM_ISR(woken);
}
