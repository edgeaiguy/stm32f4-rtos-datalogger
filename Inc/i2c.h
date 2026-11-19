#ifndef I2C_H
#define I2C_H

#include <stdint.h>

void i2c_init(void);

/* Number of times a hung bus was cleared. */
uint32_t i2c_recoveries(void);
void i2c_recover(void);

/* Registers captured at the first transfer error since the last successful env read. */
typedef struct { uint32_t count, flag, sr1, sr2, cr1, reg, len; } i2c_fault_t;
const i2c_fault_t *i2c_first_fault(void);
void i2c_clear_fault(void);
int i2c_start(void);
void i2c_stop(void);
int i2c_send_address(uint8_t addr, uint8_t rw);
int i2c_write_byte(uint8_t data);
int i2c_write_register(uint8_t dev_addr, uint8_t reg_addr, uint8_t data);
int i2c_read_register(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data);
int i2c_read_registers(uint8_t dev_addr, uint8_t reg_addr, uint8_t *buf, uint32_t len);

#endif // I2C_H