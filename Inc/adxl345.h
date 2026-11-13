#ifndef ADXL345_H
#define ADXL345_H

#include <stdint.h>
#include "stm32f407xx.h"

#define ADXL345_DEVID_REG  0x00   /* device ID register */
#define ADXL345_DEVID      0xE5   /* its fixed value — proves the bus end-to-end */

/* CS is on PE2, not PE3 — PE3 is the Discovery's onboard LIS3DSH chip select.
 * The pin is configured in spi_init(); keep the two in sync. */
#define ADXL345_CS_HIGH()  (GPIOE_BSRR = (1 << 2))        /* deselect */
#define ADXL345_CS_LOW()   (GPIOE_BSRR = (1 << (2 + 16))) /* select   */

void    adxl345_init(void);
uint8_t adxl345_read_register(uint8_t reg);
void    adxl345_write_register(uint8_t reg, uint8_t value);
void    adxl345_read_xyz(int16_t *x, int16_t *y, int16_t *z);

#define ADXL345_ODR_25HZ   0x08   /* BW_RATE codes */
#define ADXL345_ODR_50HZ   0x09
#define ADXL345_ODR_100HZ  0x0A

/* Set the output data rate and route DATA_READY to INT1 (active high). Reading
 * the XYZ registers clears it. */
void adxl345_start_data_ready(uint8_t bw_rate);

#endif