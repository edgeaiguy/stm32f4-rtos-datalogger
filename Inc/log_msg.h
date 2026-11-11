#ifndef LOG_MSG_H
#define LOG_MSG_H

#include <stdint.h>

typedef enum {
    LOG_MSG_ACCEL,
    LOG_MSG_ENV,
    LOG_MSG_TYPES,
} log_msg_type_t;

/* One sensor reading. seq counts every attempt per type, including ones the
 * producer dropped, so the consumer sees a gap wherever data was lost. t_cyc is
 * the raw DWT cycle count; the consumer converts it. */
typedef struct {
    uint8_t  type;
    uint32_t seq;
    uint32_t t_cyc;
    union {
        struct { int16_t x, y, z; }                  accel;   /* raw LSB, 256 LSB/g */
        struct { int32_t temp_c100; uint32_t press_q24_8; }  env;
    } u;
} log_msg_t;

#endif // LOG_MSG_H
