#ifndef TASK_LED_H
#define TASK_LED_H

/* Configures PD12/PD13 and creates the green (1 Hz, prio 1) and orange (5 Hz,
 * prio 2) blink tasks. */
void task_led_start(void);

#endif // TASK_LED_H
