#ifndef TASK_ACCEL_H
#define TASK_ACCEL_H

/* Creates accel_task at priority 4 and arms EXTI1 (ADXL345 INT1 on PD1). It
 * owns SPI1 from here on. Call from a task, after adxl345_init(). */
void task_accel_start(void);

#endif // TASK_ACCEL_H
