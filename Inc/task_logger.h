#ifndef TASK_LOGGER_H
#define TASK_LOGGER_H

#include "log_msg.h"

/* Creates logger_q and logger_task (priority 2). Call before starting any
 * producer. */
void task_logger_start(void);

/* Queues msg without ever waiting. Returns 0 if queued, -1 if the queue was
 * full and the message was dropped (and counted). Each msg->type must have a
 * single producer task. */
int logger_post(const log_msg_t *msg);

#endif // TASK_LOGGER_H
