#ifndef TASK_ENV_H
#define TASK_ENV_H

#include "bmp280.h"

/* Creates env_task at priority 3. It owns I2C1 from here on and keeps using
 * calib, so calib must outlive the caller. */
void task_env_start(bmp280_calib_t *calib);

#endif // TASK_ENV_H
