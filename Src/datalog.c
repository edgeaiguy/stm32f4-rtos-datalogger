/* datalog.c — CSV logging of the fused sensor stream onto the SD card. */
#include "ff.h"
#include "rtc.h"
#include "fmt.h"
#include "millis.h"
#include "dwt.h"
#include "probe.h"
#include "datalog.h"

/* FatFs buffers writes, so nothing is safely on the card until f_sync(). Syncing on
 * elapsed time bounds what a card-yank loses to this long at any sample rate, and
 * amortises the directory + FAT rewrite across every row in the window. */
#define DATALOG_SYNC_MS  5000

#define CSV_HEADER "timestamp,t_ms,temp_c,press_hpa,accel_x_mg,accel_y_mg,accel_z_mg\r\n"

static FATFS    fs;
static FIL      file;
static char     fname[13];      /* 8.3 plus NUL */
static int      open_ok;
static uint32_t last_sync_ms;
static uint32_t max_write_cyc, max_sync_cyc;   /* worst elapsed time, preemption included */

/* Integer/fraction split throughout — the firmware has no float printf path.
 * Environmental columns are written empty, not repeated, when the barometer
 * did not run this tick: a held value would look like a measurement that was
 * never taken, and empty is what pandas and Excel already read as missing. */
static int format_row(char *out, size_t cap, const datalog_row_t *r) {
    /* Sized for the full int32/Q24.8 range, not the sensor's actual span, so
     * the formatting can never truncate silently. */
    char tbuf[16] = "";
    char pbuf[16] = "";

    if (r->env_valid) {
        int32_t t = r->temp_c100;
        const char *sign = (t < 0) ? "-" : "";
        if (t < 0) t = -t;
        fmt_snprintf(tbuf, sizeof tbuf, "%s%ld.%02ld",
                 sign, (long)(t / 100), (long)(t % 100));

        uint32_t pa = r->press_q24_8 >> 8;   /* Q24.8 -> whole Pa */
        fmt_snprintf(pbuf, sizeof pbuf, "%lu.%02lu",
                 (unsigned long)(pa / 100), (unsigned long)(pa % 100));
    }

    /* ±2g full-res -> 256 LSB/g */
    int xm = (r->x * 1000) / 256;
    int ym = (r->y * 1000) / 256;
    int zm = (r->z * 1000) / 256;

    /* Wall clock for humans, t_ms for ordering: the RTC only resolves to its
     * sub-second tick and could in principle be stepped, while t_ms is
     * monotonic from boot. */
    rtc_time_t w;
    rtc_now(&w);

    return fmt_snprintf(out, cap,
                    "%04u-%02u-%02uT%02u:%02u:%02u.%03u,%lu,%s,%s,%d,%d,%d\r\n",
                    w.year, w.month, w.day, w.hour, w.min, w.sec, w.ms,
                    (unsigned long)r->t_ms, tbuf, pbuf, xm, ym, zm);
}

/* The probe pin and the cycle counter both bracket the same FatFs call, so the LA
 * and the firmware report the same stall. */
static FRESULT timed_write(const void *buf, UINT len, UINT *bw) {
    uint32_t t0 = dwt_cycles();
    PROBE_SD_SET();
    FRESULT fr = f_write(&file, buf, len, bw);
    PROBE_SD_CLR();
    uint32_t dt = dwt_cycles() - t0;
    if (dt > max_write_cyc) max_write_cyc = dt;
    return fr;
}

static FRESULT timed_sync(void) {
    uint32_t t0 = dwt_cycles();
    PROBE_SD_SET();
    FRESULT fr = f_sync(&file);
    PROBE_SD_CLR();
    uint32_t dt = dwt_cycles() - t0;
    if (dt > max_sync_cyc) max_sync_cyc = dt;
    return fr;
}

void datalog_get_stall_us(uint32_t *write_us, uint32_t *sync_us) {
    *write_us = max_write_cyc / DWT_CYCLES_PER_US;
    *sync_us = max_sync_cyc / DWT_CYCLES_PER_US;
}

int datalog_open(void) {
    FRESULT fr = f_mount(&fs, "", 1);   /* 1 = mount now, so errors surface here */
    if (fr != FR_OK) return -(int)fr;

    /* Probe upward for the first free name so a reset does not clobber the
     * previous session. 8.3 names, so LOGnnnnn.CSV is the whole budget. */
    FILINFO info;
    for (unsigned n = 1; n <= 99999; n++) {
        fmt_snprintf(fname, sizeof fname, "LOG%05u.CSV", n);

        FRESULT st = f_stat(fname, &info);
        if (st == FR_OK) continue;              /* name taken, try the next */
        if (st != FR_NO_FILE) return -(int)st;  /* a real error, not absence */

        fr = f_open(&file, fname, FA_WRITE | FA_CREATE_NEW);
        if (fr != FR_OK) return -(int)fr;

        UINT bw;
        fr = f_write(&file, CSV_HEADER, sizeof(CSV_HEADER) - 1, &bw);
        if (fr == FR_OK) fr = f_sync(&file);
        if (fr != FR_OK) { f_close(&file); return -(int)fr; }

        last_sync_ms = millis();
        open_ok = 1;
        return 0;
    }

    fname[0] = '\0';
    return -1;   /* every name in the series is taken */
}

int datalog_write_row(const datalog_row_t *row) {
    if (!open_ok) return -1;

    char line[128];
    int n = format_row(line, sizeof line, row);
    if (n <= 0 || (size_t)n >= sizeof line) return -1;   /* formatting truncated */

    UINT bw;
    FRESULT fr = timed_write(line, (UINT)n, &bw);
    if (fr != FR_OK) return -(int)fr;
    if (bw != (UINT)n) return -1;                        /* short write: card full */

    if ((uint32_t)(millis() - last_sync_ms) >= DATALOG_SYNC_MS) {
        last_sync_ms = millis();
        fr = timed_sync();
        if (fr != FR_OK) return -(int)fr;
    }
    return 0;
}

int datalog_sync(void) {
    if (!open_ok) return -1;
    FRESULT fr = timed_sync();
    last_sync_ms = millis();
    return (fr == FR_OK) ? 0 : -(int)fr;
}

const char *datalog_filename(void) {
    return fname;
}
