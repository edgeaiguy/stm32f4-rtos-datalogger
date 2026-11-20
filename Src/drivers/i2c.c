#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "stm32f407xx.h"
#include "dwt.h"
#include "i2c.h"

#define I2C_TIMEOUT 10000 // loop iterations, not microseconds - just a runaway guard

static uint32_t recoveries;

uint32_t i2c_recoveries(void) { return recoveries; }

static void scl_delay(void) {
  uint32_t t0 = dwt_cycles();
  while (dwt_cycles() - t0 < 5 * DWT_CYCLES_PER_US) {}
}

/* A slave interrupted mid-byte can hold SDA low forever, and a reset of this chip
 * does not reset the slave. Clock SCL by hand until it lets go, then send a STOP. */
static void i2c_bus_clear(void) {
  GPIOB_OTYPER |= (1U << 6) | (1U << 7);
  GPIOB_BSRR = (1U << 6) | (1U << 7);
  GPIOB_MODER = (GPIOB_MODER & ~((0x3U << 12) | (0x3U << 14))) | (0x1U << 12) | (0x1U << 14);
  scl_delay();

  for (int i = 0; i < 9 && !(GPIOB_IDR & (1U << 7)); i++) {
    GPIOB_BSRR = (1U << (6 + 16));
    scl_delay();
    GPIOB_BSRR = (1U << 6);
    scl_delay();
  }

  GPIOB_BSRR = (1U << (7 + 16));                            // STOP: SDA low then high while SCL is high
  scl_delay();
  GPIOB_BSRR = (1U << 7);
  scl_delay();
}

void i2c_init(void) {
  // enable GPIOB and I2C1 clocks
  RCC_AHB1ENR |= RCC_AHB1ENR_GPIOBEN; // enable GPIOB clock (PB6 and PB7 for I2C)
  RCC_APB1ENR |= RCC_APB1ENR_I2C1EN; // enable I2C1 clock
  // readback to ensure clock is enabled before proceeding
  volatile unsigned int tmp = RCC_APB1ENR; (void) tmp;

  i2c_bus_clear();

  // configure PB6 and PB7 for alternate function mode (AF4 for I2C)
  GPIOB_MODER &= ~(0x3 << 12); // clear bits [13:12] for PB6
  GPIOB_MODER &= ~(0x3 << 14); // clear bits [15:14] for PB7
  GPIOB_MODER |= (0x2 << 12); // set PB6 to alternate function mode
  GPIOB_MODER |= (0x2 << 14); // set PB7 to alternate function mode

  GPIOB_OTYPER |= (1 << 6); // set PB6 to open-drain
  GPIOB_OTYPER |= (1 << 7); // set PB7 to open-drain

  GPIOB_OSPEEDR |= (0x2 << 12); // set PB6 to high speed
  GPIOB_OSPEEDR |= (0x2 << 14); // set PB7 to high speed

  GPIOB_PUPDR &= ~(0x3 << 12); // clear bits [13:12] for PB6
  GPIOB_PUPDR &= ~(0x3 << 14); // clear bits [15:14] for PB7
  GPIOB_PUPDR |= (0x1 << 12); // set PB6 to pull-up
  GPIOB_PUPDR |= (0x1 << 14); // set PB7 to pull-up

  GPIOB_AFRL &= ~(0xF << 24); // clear bits [27:24] for AFRL6
  GPIOB_AFRL &= ~(0xF << 28); // clear bits [31:28] for AFRL7
  GPIOB_AFRL |= (0x4 << 24); // set AFRL6 to AF4 (I2C1_SCL)
  GPIOB_AFRL |= (0x4 << 28); // set AFRL7 to AF4 (I2C1_SDA)

  // disable I2C1 before configuring timing registers
  I2C1_CR1 &= ~I2C_CR1_PE;

  // set APB1 clock frequency (MHz) in CR2
  I2C1_CR2 = 16;  // 16 MHz if on HSI

  // set CCR for 100kHz standard mode
  I2C1_CCR = 80;  // at 16 MHz

  // set maximum rise time
  I2C1_TRISE = 17;  // at 16 MHz

  // enable I2C1
  I2C1_CR1 |= I2C_CR1_PE;
}

void i2c_recover(void) {
  recoveries++;
  I2C1_CR1 &= ~I2C_CR1_PE;
  i2c_bus_clear();
  I2C1_CR1 |= I2C_CR1_SWRST;
  I2C1_CR1 &= ~I2C_CR1_SWRST;
  i2c_init();
}

/* Spin until a status flag appears. Bails out early on AF (the slave never
 * acknowledged), otherwise a missing device would cost the full timeout. */
/* Faults outside a flag wait use codes above the SR1 bits. */
#define FAULT_STOP_PENDING 0x10000
#define FAULT_BUS_BUSY     0x20000

static i2c_fault_t fault;
static uint32_t cur_reg, cur_len;   /* transfer in progress, for the fault record */

const i2c_fault_t *i2c_first_fault(void) { return &fault; }
void i2c_clear_fault(void) { fault = (i2c_fault_t){0}; }

static void note_fault(uint32_t flag) {
  if (fault.count++ == 0) {
    fault.reg = cur_reg; fault.len = cur_len;
    fault.flag = flag; fault.sr1 = I2C1_SR1; fault.sr2 = I2C1_SR2; fault.cr1 = I2C1_CR1;
  }
}

static int i2c_wait_flag(uint32_t flag) {
  uint32_t timeout = I2C_TIMEOUT;
  while (!(I2C1_SR1 & flag)) {
    if (I2C1_SR1 & I2C_SR1_AF) {
      note_fault(flag);
      I2C1_SR1 &= ~I2C_SR1_AF;   // AF is rc_w0: writing 0 clears it, writing 1 has no effect
      I2C1_CR1 |= I2C_CR1_STOP;  // release the bus so the next transfer can start
      return -1;
    }
    if (--timeout == 0) {
      note_fault(flag);
      I2C1_CR1 |= I2C_CR1_STOP;  // don't leave SCL held low on the way out
      return -1;
    }
  }
  return 0;
}

/* ADDR is cleared by reading SR1 then SR2. Whether this happens before or after
 * touching ACK/POS is what makes the receive sequences below correct. */
static void i2c_clear_addr(void) {
  volatile uint32_t tmp = I2C1_SR1;
  tmp = I2C1_SR2;
  (void) tmp;
}

/* Wait for a previously requested STOP to actually go out on the wire. Starting
 * a new transfer while STOP is still pending wedges the peripheral. */
static int i2c_wait_stop_done(void) {
  uint32_t timeout = I2C_TIMEOUT;
  while (I2C1_CR1 & I2C_CR1_STOP) {
    if (--timeout == 0) {
      note_fault(FAULT_STOP_PENDING);
      i2c_recover();   /* a stuck STOP would fail every later transfer too */
      return -1;
    }
  }
  return 0;
}

static int i2c_wait_bus_free(void) {
  uint32_t timeout = I2C_TIMEOUT;
  while (I2C1_SR2 & I2C_SR2_BUSY) {
    if (--timeout == 0) {
      note_fault(FAULT_BUS_BUSY);
      i2c_recover();
      return (I2C1_SR2 & I2C_SR2_BUSY) ? -1 : 0;
    }
  }
  return 0;
}

int i2c_start(void) {
  // Send start condition. Also used for a repeated start, so this must not
  // wait on BUSY - BUSY is legitimately set mid-transfer.
  I2C1_CR1 |= I2C_CR1_START;
  return i2c_wait_flag(I2C_SR1_SB);
}

void i2c_stop(void) {
  // Send stop condition
  I2C1_CR1 |= I2C_CR1_STOP;
}

int i2c_send_address(uint8_t addr, uint8_t rw) {
  // Send address with R/W bit
  I2C1_DR = (addr << 1) | (rw & 0x1);  // LSB is R/W bit

  if (i2c_wait_flag(I2C_SR1_ADDR) != 0) return -1;
  i2c_clear_addr();
  return 0;
}

int i2c_write_byte(uint8_t data) {
  // Send data byte
  I2C1_DR = data;
  return i2c_wait_flag(I2C_SR1_TXE);
}

int i2c_write_register(uint8_t dev_addr, uint8_t reg_addr, uint8_t data) {
    cur_reg = reg_addr; cur_len = 0;
    if (i2c_wait_bus_free() != 0) return -1;
    if (i2c_start() != 0) return -1;
    if (i2c_send_address(dev_addr, 0) != 0) return -1;  // 0 = write
    if (i2c_write_byte(reg_addr) != 0) return -1;
    if (i2c_write_byte(data) != 0) return -1;
    i2c_stop();
    return i2c_wait_stop_done();
}

/* Read len bytes starting at reg_addr.
 *
 * Reception has to be closed differently depending on how many bytes are left,
 * because the peripheral has a two-deep pipeline (DR + shift register) and the
 * NACK for the final byte has to be programmed before that byte arrives.
 * RM0090 section 27.3.3 ("Master receiver") spells out three cases; all three
 * are implemented below. */
int i2c_read_registers(uint8_t dev_addr, uint8_t reg_addr, uint8_t *buf, uint32_t len) {
    if (len == 0) return -1;
    cur_reg = reg_addr; cur_len = len;

    if (i2c_wait_bus_free() != 0) return -1;
    I2C1_CR1 &= ~I2C_CR1_POS;  // only the N=2 path wants POS set; never inherit it

    // write phase: tell the device which register we want to read from
    if (i2c_start() != 0) return -1;
    if (i2c_send_address(dev_addr, 0) != 0) return -1;
    if (i2c_write_byte(reg_addr) != 0) return -1;

    // repeated start, then switch direction to read
    if (i2c_start() != 0) return -1;

    if (len == 1) {
        // N=1: ACK must be cleared *before* ADDR, and STOP requested immediately
        // after, so the NACK lands on the byte that is already being clocked in.
        I2C1_DR = (dev_addr << 1) | 1;
        if (i2c_wait_flag(I2C_SR1_ADDR) != 0) return -1;
        /* The NACK/STOP must land within one byte time of releasing SCL; a preempting
         * task in this window leaves the slave clocking out data and STOP never completes. */
        taskENTER_CRITICAL();
        I2C1_CR1 &= ~I2C_CR1_ACK;
        i2c_clear_addr();
        I2C1_CR1 |= I2C_CR1_STOP;
        taskEXIT_CRITICAL();

        if (i2c_wait_flag(I2C_SR1_RXNE) != 0) return -1;
        buf[0] = I2C1_DR;

    } else if (len == 2) {
        // N=2: POS shifts the meaning of ACK onto the *next* byte, which lets us
        // NACK byte 2 while byte 1 is still in flight.
        I2C1_CR1 |= I2C_CR1_POS | I2C_CR1_ACK;

        I2C1_DR = (dev_addr << 1) | 1;
        if (i2c_wait_flag(I2C_SR1_ADDR) != 0) return -1;
        taskENTER_CRITICAL();  // same one-byte-time window as the N=1 path
        i2c_clear_addr();
        I2C1_CR1 &= ~I2C_CR1_ACK;  // NACK byte 2
        taskEXIT_CRITICAL();

        // BTF means byte 1 sits in DR and byte 2 in the shift register, with SCL
        // held low - nothing more can arrive, so it is safe to close here.
        if (i2c_wait_flag(I2C_SR1_BTF) != 0) return -1;
        I2C1_CR1 |= I2C_CR1_STOP;
        buf[0] = I2C1_DR;
        buf[1] = I2C1_DR;

    } else {
        // N>2: ACK the stream, then treat the last three bytes as a unit.
        I2C1_CR1 |= I2C_CR1_ACK;

        I2C1_DR = (dev_addr << 1) | 1;
        if (i2c_wait_flag(I2C_SR1_ADDR) != 0) return -1;
        i2c_clear_addr();

        uint32_t i = 0;
        while (len - i > 3) {
            if (i2c_wait_flag(I2C_SR1_RXNE) != 0) return -1;
            buf[i++] = I2C1_DR;
        }

        // three bytes left: N-2, N-1, N. Deliberately do not read on RxNE here -
        // letting BTF build up is what stalls the bus long enough to set the NACK.
        if (i2c_wait_flag(I2C_SR1_BTF) != 0) return -1;  // N-2 in DR, N-1 in shift register
        I2C1_CR1 &= ~I2C_CR1_ACK;                       // NACK byte N
        buf[i++] = I2C1_DR;                             // reading N-2 launches reception of N

        if (i2c_wait_flag(I2C_SR1_BTF) != 0) return -1;  // N-1 in DR, N in shift register
        I2C1_CR1 |= I2C_CR1_STOP;
        buf[i++] = I2C1_DR;                             // N-1

        if (i2c_wait_flag(I2C_SR1_RXNE) != 0) return -1;
        buf[i++] = I2C1_DR;                             // N
    }

    /* POS is cleared only once STOP has completed: a read-modify-write of CR1 while STOP
     * is pending can write the bit back after the hardware cleared it, leaving STOP stuck. */
    int rc = i2c_wait_stop_done();
    I2C1_CR1 &= ~I2C_CR1_POS;
    return rc;
}

int i2c_read_register(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data) {
    return i2c_read_registers(dev_addr, reg_addr, data, 1);
}
