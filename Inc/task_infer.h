#ifndef TASK_INFER_H
#define TASK_INFER_H

#include "log_msg.h"

/* Creates infer_q and infer_task (priority 3, between env and logger). Call before
 * starting the accel producer. */
void task_infer_start(void);

/* Queues an accel message without ever waiting; -1 if the queue was full (counted).
 * Single producer: accel_task. */
int infer_post(const log_msg_t *msg);

typedef struct {
    uint32_t windows;        /* complete RMS windows computed */
    int32_t  rms_mg[3];      /* last window, x y z */
    uint32_t gaps;           /* sequence numbers missing on arrival */
    uint32_t drops;          /* posts rejected because the queue was full */
    uint32_t queue_peak;
    int      enabled;
} infer_stats_t;

/* Stops or resumes consuming; while stopped the queue fills and posts are dropped,
 * which shows that a stalled consumer cannot touch sampling or logging. */
void infer_set_enabled(int on);

void infer_get_stats(infer_stats_t *out);

#endif // TASK_INFER_H
