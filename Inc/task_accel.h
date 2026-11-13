#ifndef TASK_ACCEL_H
#define TASK_ACCEL_H

/* Creates accel_task at priority 4 and arms EXTI1 (ADXL345 INT1 on PD1). It
 * owns SPI1 from here on. Call from a task, after adxl345_init(). */
void task_accel_start(void);

#include <stdint.h>

/* Sets the sample rate if hz is in the supported set (1, 2, 4, 5, 10, 20, 25, 50,
 * 100). Returns 0 if accepted, -1 if not. Takes effect on the next sample. */
int task_accel_set_rate(uint32_t hz);

/* The rate accel_task is currently running at. */
uint32_t task_accel_rate(void);

/* Prints the supported rates, space separated, to the console. */
void task_accel_print_rates(void);

#endif // TASK_ACCEL_H
