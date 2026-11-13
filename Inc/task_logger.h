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

typedef struct {
    uint32_t rows;           /* rows written to the card */
    uint32_t write_errors;
    uint32_t accel_gaps;     /* sequence numbers missing on arrival */
    uint32_t accel_drops;    /* posts rejected because the queue was full */
    uint32_t env_gaps;
    uint32_t env_drops;
    uint32_t queue_peak;
    int      logging;
} logger_stats_t;

/* Pauses or resumes writing rows. Sensors and the queue keep running while
 * paused; the file is synced when logging stops. */
void logger_set_logging(int on);

/* Snapshot of the counters; fields may be one update apart from each other. */
void logger_get_stats(logger_stats_t *out);

#endif // TASK_LOGGER_H
