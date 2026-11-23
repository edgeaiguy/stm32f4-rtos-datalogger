#ifndef SPI_H
#define SPI_H

#include <stdint.h>

/* BR field values for CR1[5:3]. PCLK1 and PCLK2 are both 16 MHz (HSI, no PLL),
 * so these mean the same rates on either bus. */
#define SPI_BR_DIV4    1   /* 4 MHz    — SD data phase */
#define SPI_BR_DIV32   4   /* 500 kHz  — ADXL345 */
#define SPI_BR_DIV64   5   /* 250 kHz  — SD init, inside the card's 100–400 kHz window */

/* SPI1 — shared bus: ADXL345 (CS PE2) and the onboard LIS3DSH (CS PE3). */
void spi_init(void);
uint8_t spi_transfer(uint8_t data);

/* SPI2 — private bus for the SD card (CS PE4). It gets its own peripheral
 * because the breakout drives MISO even when deselected. */
void spi2_init(void);
uint8_t spi2_transfer(uint8_t data);

/* Whole-buffer transfer on DMA1 (SPI2 TX = stream 4, RX = stream 3, channel 0). The
 * calling task sleeps until the RX stream's transfer-complete interrupt notifies it, so
 * it must be a task and the scheduler must be running. 0 on success, -1 on timeout/error. */
int spi2_dma_write(const uint8_t *buf, uint32_t len);
int spi2_dma_read(uint8_t *buf, uint32_t len);

/* Retune SCK between the card's slow init and fast data phases. Only safe
 * between transactions — it briefly disables the peripheral. */
void spi2_set_baudrate(uint32_t br);

#endif // SPI_H
