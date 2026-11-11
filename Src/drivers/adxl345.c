/* adxl345.c */
#include <stdint.h>
#include "stm32f407xx.h"
#include "spi.h"
#include "adxl345.h"

/* --- register map --- */
#define ADXL345_BW_RATE      0x2C
#define ADXL345_POWER_CTL    0x2D
#define ADXL345_INT_ENABLE   0x2E
#define ADXL345_INT_MAP      0x2F
#define ADXL345_DATA_FORMAT  0x31
#define ADXL345_DATAX0       0x32   /* X/Y/Z span 0x32–0x37, low byte first */

/* --- SPI address-byte bits --- */
#define ADXL345_READ         0x80   /* MSB set  = read  */
#define ADXL345_MULTIBYTE    0x40   /* bit 6    = auto-increment address */

void adxl345_init(void) {
    /* CS resting state */
    ADXL345_CS_HIGH();
    
    /* configure the device for ±2g range, full resolution, and start measuring */
    adxl345_write_register(ADXL345_DATA_FORMAT, 0x08); /* full resolution, ±2g */
    adxl345_write_register(ADXL345_POWER_CTL, 0x08);   /* measure */
}

void adxl345_start_data_ready(uint8_t bw_rate) {
    adxl345_write_register(ADXL345_BW_RATE, bw_rate);
    adxl345_write_register(ADXL345_INT_MAP, 0x00);      /* every source on INT1 */
    adxl345_write_register(ADXL345_INT_ENABLE, 0x80);   /* DATA_READY */
}

uint8_t adxl345_read_register(uint8_t reg) {
    ADXL345_CS_LOW();
    spi_transfer(reg | ADXL345_READ);      /* MSB set = read */
    uint8_t value = spi_transfer(0xFF);    /* dummy byte clocks the response out */
    ADXL345_CS_HIGH();
    return value;
}

void adxl345_write_register(uint8_t reg, uint8_t data) {
    ADXL345_CS_LOW();
    spi_transfer(reg);                     /* MSB clear = write */
    spi_transfer(data);
    ADXL345_CS_HIGH();
}

void adxl345_read_xyz(int16_t *x, int16_t *y, int16_t *z) {
    uint8_t buf[6];

    ADXL345_CS_LOW();
    /* address byte: read + multibyte + starting register (0x32) */
    spi_transfer(ADXL345_DATAX0 | ADXL345_READ | ADXL345_MULTIBYTE);
    for (int i = 0; i < 6; i++) {
        buf[i] = spi_transfer(0xFF);       /* six dummies clock out 0x32..0x37 */
    }
    ADXL345_CS_HIGH();

    /* each axis is 16-bit two's complement, LOW byte first */
    *x = (int16_t)((buf[1] << 8) | buf[0]);
    *y = (int16_t)((buf[3] << 8) | buf[2]);
    *z = (int16_t)((buf[5] << 8) | buf[4]);
}