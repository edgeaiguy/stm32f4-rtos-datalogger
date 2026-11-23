# Project 4 plan: FreeRTOS port of the datalogger

Port of [stm32f4-datalogger](https://github.com/edgeaiguy/stm32f4-datalogger)
(Project 3, tag `v3.0` at `1d35a76`) from a bare-metal superloop to FreeRTOS tasks.
Same sensors, SD card, CSV schema and CLI; every timing and size figure is measured
against the bare-metal version.

Plan written 2026-10-23. Status last updated 2026-11-20.

## Status

**Current milestone: `v4.0`, code and measurements done; README and tag left.** `m1-kernel` to `m5-dma` are done.

| Milestone | State |
|---|---|
| `m1-kernel` | Done, tagged |
| `m2-sensor-tasks` | Done, tagged |
| `m3-logger` | Done, tagged |
| `m4-measured` | Done, tagged |
| `m5-dma` | Done, tagged |
| `v4.0` | Not started |

### `m1-kernel` checklist

- [x] Project 3 tagged `v3.0` at `1d35a76` and pushed
- [x] Repo created; P3 sources imported from `v3.0` (`f6cc1fd`); image identical to P3 apart from the `__DATE__`/`__TIME__` RTC seed
- [x] `Src/drivers` / `Src/app` layout; Makefile mirrors the tree under `build/`, `-MMD -MP` header deps (`4ed872c`)
- [x] FreeRTOS-Kernel V11.3.2 vendored, unmodified (`c35750f`)
- [x] `Inc/FreeRTOSConfig.h` (`2e3cc56`)
- [x] Handler mapping (SVC/PendSV/SysTick via config `#define`s), `systick.c` deleted, `millis()` shim, stack overflow hook, USART2 at NVIC priority 7 (`c30401f`)
- [x] Two static LED tasks blinking on hardware: PD12 green 1 Hz (prio 1), PD13 orange 5 Hz (prio 2). Verified over SWD: 501 ticks in ~500 ms, idle task running between toggles (`c30401f`)
- [x] `console.c`: TX through a stream buffer drained by the TXE ISR; full buffer drops and counts, never blocks. Verified on hardware: 1 Hz ticks, 0 drops
- [x] `cli_task`: USART2 RX ISR feeds a stream buffer (replaces the hand ring in `uart_cmd.c`); parser unchanged. Verified on hardware: help, rate, status, bad input
- [x] Kernel-only flash/RAM cost vs `v3.0`: +22169 B flash, +880 B bss (see Current numbers)
- [x] README section for m1; tag `m1-kernel`

**m1 demo:** two LED tasks, live CLI, kernel-only size cost.

### `m2-sensor-tasks` checklist

- [x] `init_task` bring-up at priority 5; LED tasks moved to `task_led.c` (`e185e45`)
- [x] BMP280 split into `bmp280_trigger()` / `bmp280_fetch()`; `env_task` at 1 Hz (`a410680`)
- [x] ADXL345 INT1 on PD1 (EXTI1, priority 5) wakes `accel_task`; ODR 100 Hz (`79a2fa2`)
- [x] Probe pins: PD0 at accel read start, PD2 busy via `traceTASK_SWITCHED_IN` (`be311c3`)
- [x] First RTOS jitter capture vs P3 (below)
- [x] Cadence while the barometer converts (below)
- [x] README section for m2; tag `m2-sensor-tasks`
- [x] P3 baseline capture: probe branch off `v3.0`, probe GPIO at accel read start, LA1010 jitter at 5/20 Hz, busy-pin duty cycle (`tools/jitter.py`)

### Current numbers (`910152c`)

Same flags as `v3.0` (`-O0`, hard-float). Flash = text + data.

| | Flash | bss |
|---|---|---|
| `v3.0` | 37200 | 4144 |
| This build | 57236 (55852 text + 1384 data) | 7728 |
| Delta | +20036 | +3584 |

Kernel objects alone: `22169` flash, `880` bss (tasks.c 11485, queue.c 4776,
stream_buffer.c 4180, port.c 1364, list.c 360). Rest of the flash delta is
roughly -2470 B (P3 `main.o` and `systick.o` gone, `uart2.o` shrunk, offset by new
console, task_cli, main and hooks) plus ~340 B unattributed. bss delta: kernel 880
+ task stacks/TCBs (main 1184, task_cli 1616) + console 380 - P3 `main.o` 512.
Unused P3 drivers are still linked, so the later m2 figure replaces this one.

### `m3-logger` checklist

- [x] `log_msg_t`, static `logger_q` (32 items), DWT cycle counter, zero-timeout posts with per-type sequence numbers and drop counters; temporary `logger_task` counts arrivals and reports gaps and queue peak
- [x] `logger_task` writes the P3 CSV schema to SD, attaches env to the next accel row, syncs on elapsed time (5 s). The file is opened in `init_task` bring-up so startup is not counted as drops. Verified from the card: 5356 rows, all 7 columns, `t_ms` steps of 48-49 ms, one env row per second
- [x] CLI: discrete `rate` set, `start`/`stop`, `status`; `rate` reaches `accel_task` by `xTaskNotifyIndexed` (index 1; index 0 stays the INT1 wake-up). Default rate 5 Hz as in `v3.0`. Verified on hardware
- [x] `time YYYY-MM-DD HH:MM:SS` sets the RTC; `tools/settime.py` sends the host clock over the console UART. There is no network hardware, so this stands in for NTP
- [x] SD stall measured: probe PD3 around `f_write`/`f_sync` plus a DWT worst-case counter in `datalog.c` (shown by `status`). Worst seen: row write 15.9 ms, sync 42.8 ms in firmware; 67.9 ms on the LA over 500 s. Queue sized: rate x stall x 2 is about 3 slots at 20 Hz and 14 at 100 Hz, so the 32-item queue stays
- [x] 20 Hz capture against the P3 baseline (below); README section
- [x] Tag `m3-logger`

### RTOS m3 capture (20 Hz, SD logging active)

PD0 = accel read, PD2 = busy, PD3 = SD write/sync window. Raw capture: `rtos-m3-20hz.csv`.
PD0 fires on every sensor read (100 Hz ODR), before decimation, so the 20 Hz logged
samples are every 5th edge; the table gives the range over the five possible alignments.

| Logged-sample periods | Mean | σ | p99 \|dev\| | Peak-to-peak | Busy |
|---|---|---|---|---|---|
| 10260 per alignment | 48726.1 µs (20.52 Hz) | 6.5-7.9 µs | 17-36 µs | 159-236 µs | 8.6% |

P3 at 20 Hz had σ 734 µs, p99 3.4 ms, peak-to-peak 64.5 ms and 21.3% busy.

- CH2: 10361 SD pulses in 500 s. Median 30 µs (a row copied into the FatFs buffer); 1048 last over 5 ms (a 512-byte sector going out to the card, about one in 10 rows); the 5 s syncs cluster near 43 ms; worst 67.9 ms. The SD pin is high 1.8% of the time.
- Sample periods overlapping an SD pulse over 5 ms: σ 6.5 µs, against 5.7 µs elsewhere (100 Hz edge periods). SD stalls do not reach the sampler.
- The 20.52 Hz mean is the sensor clock running 2.6% fast, not the scheduler.
- To compare directly with P3 in later runs, the probe could move after the decimation so PD0 marks only logged samples.

### `m4-measured` checklist

- [x] Run-time stats on a hardware timebase (TIM2, 100 kHz); `stats` command with per-task CPU % and stack high-water marks
- [x] Stack sizes trimmed to measured high-water mark + ~30% (bss 16472 -> 12760 B)
- [x] ISR-to-task latency (INT1 edge to the accel read), separating it from the sensor's clock jitter
- [x] Task timeline capture (`traceTASK_SWITCHED_IN` on 3 GPIOs)
- [x] Tick overhead (DWT cycles across the tick path): avg 100 cycles (6.3 µs), max 608, 0.62% of CPU
- [x] LSI calibration so the RTC wall clock is accurate (TIM5 measures the LSI at cold start; 0 s drift over 306 s)
- [x] `tools/check_log.py`: rows on the card vs expected, `t_ms` steps, env cadence
- [x] Full results table at 5, 20 and 100 Hz
- [x] Max sustained rate: skipped, 100 Hz leaves 82% idle and zero drops
- [x] 10 h soak at 100 Hz: 3,707,373 rows, 0 errors, 0 drops, 0 BMP280 failures (see below)
- [x] README section for m4; tag `m4-measured`

### RTOS m4 latency capture (20 Hz, SD logging active)

CH0 = PD0 (accel read), CH1 = PD2 (busy), CH2 = PD3 (SD), CH3 = INT1; 10 MHz, 20 s. Raw: `rtos-m4-latency-20hz.csv`; `tools/latency.py` pairs each INT1 rising edge with the next PD0 rising edge.

| INT1 -> read start | Mean | σ | p50 | p99 | Max |
|---|---|---|---|---|---|
| 2053 pairs (100 Hz ODR) | 73.5 µs | 4.4 µs | 72.5 µs | 81.6 µs | 160.9 µs |

- INT1 to busy rising is a median 45.7 µs (interrupt entry, notify, PendSV, scheduler); the other 27 µs is `accel_task` reaching the PD0 write at -O0.
- Worst wake 161 µs against a 10 ms sample period at 100 Hz.

### RTOS m4 timeline capture (default 5 Hz logged, 25 Hz ODR)

PD4 = accel, PD5 = env, PD6 = logger (high while that task runs), plus the m4 latency channels. 10 MHz, 20 s. Raw: `rtos-m4-timeline-20hz.csv` (the file name predates the reboot to the 5 Hz default).

| Task | Slices | CPU share | Median slice | p99 | Longest |
|---|---|---|---|---|---|
| accel | 513 | 0.74% | 277 µs | 326 µs | 446 µs |
| env | 42 | 0.35% | 650 µs | 3.1 ms | 3.1 ms |
| logger | 126 | 1.22% | 690 µs | 14.7 ms | 14.7 ms |
| any task (busy) | 763 | 2.38% | 276 µs | 8 ms | 15.2 ms |

- No two task pins were ever high together. Idle is about 97.6% of the time.
- The logger's 14.7 ms slice is the SD sector flush: the card is polled without yielding. accel preempted the logger mid-write twice and env twice in 20 s.
- The PD0 read is 184 µs of the 277 µs accel slice. env's 3.1 ms slice is the blocking BMP280 fetch.

### RTOS m4 results by rate (5 min each, reset between, SD logging)

CPU shares come from `stats` (TIM2 timebase, since boot, interrupt time charged to the running task).

| Logged rate | Rows | Idle | accel | logger | env | Queue peak | Drops / gaps | Worst SD write / sync |
|---|---|---|---|---|---|---|---|---|
| 5 Hz | 1626 | 97.0% | 0.7% | 1.4% | 0.3% | 2 | 0 / 0 | 35.8 / 42.4 ms |
| 20 Hz | 6289 | 92.9% | 2.8% | 3.4% | 0.3% | 2 | 0 / 0 | 16.5 / 43.8 ms |
| 100 Hz | 31154 | 81.6% | 3.1% | 14.4% | 0.3% | 5 | 0 / 0 | 44.3 / 44.0 ms |

- Tick overhead: avg 100 cycles (6.3 µs), max 608, 0.62% of CPU. Stack headroom after the trim: logger 81, accel 40, env 47-56, cli 46-48, LED tasks 30-34, idle 72 words.
- The logger's share grows with rate because every row is formatted and copied through FatFs; the accel task's share stops growing at 20 Hz because it reads at the 100 Hz ODR from there.

### RTOS m4 soak (10 h, 100 Hz, SD logging)

Capture `rtos-m4-soak-100hz-10h.csv`, UART log `rtos-m4-soak2-uart.log` (outside the repo). `tools/check_log.py`: PASS.

| Rows | Span | Gaps | Env rows | Queue peak | Worst SD write / sync | Drops / write errors | BMP280 failures | I2C recoveries |
|---|---|---|---|---|---|---|---|---|
| 3,707,373 (matches the board's counter) | 36,080 s | 0 | 36,080 (1 per second) | 95 of 160 | 157 / 653 ms | 0 / 0 | 0 | 0 |

- RTC runs 0.6-0.7% fast against `t_ms` (HSI reference), inside the 1% target.
- Defects the soak found, all fixed: an I2C bus hang (bus recovery), SD stalls beyond a 32-item queue (160 items), and a BMP280 that stopped answering about hourly:
  a task switch inside the I2C read's NACK/STOP window (critical sections), and clearing POS right after requesting STOP, a read-modify-write that could write the STOP bit back (POS cleared after STOP completes). The env task still resets the sensor after 3 failed reads as a safety net.
- Limits: `stats` CPU % wraps after about 11.9 h (TIM2, 32 bits at 100 kHz); `t_ms` wraps after about 49 days.

### RTOS m5 "before" capture (SPI2, polling, 20 Hz)

Capture `rtos-m4-spi2-before-20hz.csv` (outside the repo): CH0 SCK (PB13), CH1 MOSI (PB15), CH2 CS (PE4), 100 MSa/s, trigger on CS falling.

- SCK is 4 MHz (0.25 µs period); the 8 bits of a byte take 1.75 µs, but bytes start a median 11.14 µs apart (mean 12.2, max 58.7 µs). The CPU spends about 9.4 µs between bytes on the polling loop at -O0, so the wire is busy about 16% of the time.
- 512 data bytes after the 0xFE token took 6.35 ms: 12.4 µs/byte average, about 80 KB/s (median spacing gives 90 KB/s, the P3 figure-06 value of 11.04 µs/byte).
- The write is CMD24 (0x58) with its address, a 0xFF gap, the 0xFE token and the data; the CPU is busy for the whole 6-7 ms. This is the baseline the DMA version must beat.

### RTOS m5 "after" capture, write path on DMA (SPI2, 20 Hz)

Capture `rtos-m5-spi2-after-write-dma-20hz.csv`, same pins and settings as the "before" capture.

| 512-byte data phase | Polling (before) | DMA (after) |
|---|---|---|
| Duration | 6.35 ms | 1.02 ms |
| Byte spacing | median 11.14 µs (max 58.7) | 2.00 µs (1.99-2.01) |
| Throughput | 81 KB/s | 499 KB/s |
| CPU during the phase | busy | asleep in `ulTaskNotifyTake` |

- The wire now runs at its 4 MHz limit with no gaps; the data phase is 6.2 times faster.
- Whole transaction (CS low): 7.67 ms before, 3.21 ms after; the rest is the card's own programming time, which is still polled.
- 20 Hz run: 8,872 rows, 0 gaps, `check_log.py` PASS; worst single sector write 3.87 ms (16.5 ms before).

### RTOS m5 results by rate (DMA writes and reads, 5 min each, reset between, SD logging)

Raw log `rtos-m5-rates-uart.log` (outside the repo). Same method as the m4 table.

| Logged rate | Rows | Idle (m4 -> m5) | logger (m4 -> m5) | accel | Queue peak | Drops / gaps | Worst SD write / sync |
|---|---|---|---|---|---|---|---|
| 5 Hz | 1615 | 97.0% -> 97.8% | 1.4% -> 0.9% | 0.7% | 2 | 0 / 0 | 5.9 / 14.2 ms |
| 20 Hz | 6266 | 92.9% -> 94.3% | 3.4% -> 2.2% | 2.8% | 2 | 0 / 0 | 6.5 / 14.9 ms |
| 100 Hz | 31098 | 81.6% -> 86.5% | 14.4% -> 9.7% | 3.1% | 77 | 0 / 0 | 145 / 481 ms |

- The logger's CPU at 100 Hz fell by 4.7 points, about 47 ms per second. That matches the data phase: about 9.4 sectors a second times the 5.3 ms each DMA write saves.
- What is left in the logger is formatting each row at -O0, FatFs, the command and token bytes, and the polled busy wait for the card's programming time (kept on purpose).
- At 5 and 20 Hz the worst write and sync stalls are far shorter than in m4 (sync 43 ms -> 14 ms). The 100 Hz run hit longer card stalls (145 / 481 ms, queue peak 77 of 160): those are the card's own housekeeping, which DMA cannot change and which varies from run to run (the 10 h soak saw 157 / 653 ms).

### RTOS v4.0: stub second consumer (`infer_task`)

Raw log `rtos-v4-rates-uart.log` (outside the repo). `accel_task` posts each logged sample to `infer_q` (32 items, own drop counter) as well as `logger_q`; `infer_task` (priority 3, between env 4 and logger 2) keeps the RMS of 64-sample windows. `configMAX_PRIORITIES` is 7; init 6, accel 5, env 4 (each moved up one, order unchanged).

| Logged rate | infer CPU | accel (m5 -> v4) | logger | Idle (m5 -> v4) | infer stack free | Drops / gaps |
|---|---|---|---|---|---|---|
| 5 Hz | 0.1% | 0.7% -> 0.7% | 0.9% -> 1.2% | 97.8% -> 97.3% | 32 of 112 | 0 / 0 |
| 20 Hz | 0.3% | 2.8% -> 2.9% | 2.2% -> 2.2% | 94.3% -> 93.9% | 33 of 112 | 0 / 0 |
| 100 Hz | 1.6% | 3.1% -> 3.5% | 9.7% -> 9.7% | 86.5% -> 84.5% | 32 of 112 | 0 / 0 |

- Cost: text +1416, bss +1520 (stack, queue, task block) over m5; 2 points of idle at 100 Hz, mostly the context switches to wake `infer_task` once per sample.
- Isolation test at 100 Hz (`infer off` for 30 s): `infer_q` dropped 3,053 samples; `logger_q` and `accel_task` had 0 drops and 0 gaps, logging kept writing rows (2,114 to 5,407). After `infer on`, infer's gap count (3,270) equalled its drop count (3,270): every missing sample is accounted for.
- Sample timing is unaffected by construction: `infer_post` runs after the PD0 read window, and `infer_task` runs below accel and env, so it cannot delay either. Not re-captured on the LA.
- CLI stack: `status` grew; the CLI task had 5 words free, raised 160 -> 208 (97 free).

### P3 baseline (probe branch off `v3.0`)

PD0 high around `adxl345_read_xyz` (CH0), PD1 high while the loop works and low
while it spins for the next tick (CH1). Kingst LA1010, 1 MHz, SD logging and console
echo active. Raw captures: `p3-20hz.csv`, `p3-5hz.csv` (outside the repo). The 5 Hz run used 200 kHz sampling.

| Rate | Periods | Mean | Min / max | σ | p99 \|dev\| | Peak-to-peak | Busy |
|---|---|---|---|---|---|---|---|
| 5 Hz | 12504 | 199923.6 µs | 199.6 / 200.2 ms | 87.5 µs | 224 µs | 645 µs | 8.4% |
| 20 Hz | 9999 | 49996.3 µs | 17.8 / 82.3 ms | 734 µs | 3.4 ms | 64.5 ms | 21.3% |

At 5 Hz no SD stall shows in 2500 s: every period rounds to 200 ms. Drift against the
analyzer was -382 ppm at 5 Hz and -75 ppm at 20 Hz; the P3 timebase is the HSI RC, so
treat it as an HSI figure, not the sensor's.

At 20 Hz, 9853 of 9999 periods land in the 50 ms bucket. The spread comes from about 56 periods
at 53-54 ms and a handful at 59-82 ms (SD card housekeeping), each followed by a short
catch-up period. P3's own overrun counter read 527 over the session.

### RTOS m2 capture (100 Hz ODR, no SD or logger yet)

Same probe pins and LA setup as the P3 runs. Raw capture: `rtos-m2-100hz.csv`.

| Periods | Mean | Min / max | σ | p99 \|dev\| | Peak-to-peak | Busy |
|---|---|---|---|---|---|---|
| 51365 | 9734.0 µs (102.73 Hz) | 9.626 / 9.841 ms | 4.1 µs | 11 µs | 215 µs | 7.6% |

- The mean is 2.7% faster than the 100 Hz asked for. That is the ADXL345's own clock,
  not the RTOS. The ODR accuracy figure is still to be confirmed against the datasheet.
- The `adxl345_read_xyz` pulse on CH0 lasts 184-312 µs (mean 186 µs).
- Not yet comparable with P3: no SD or logger runs, and the sample instant comes from
  the sensor's clock instead of a 1 ms tick. The fair comparison is at m3/m4.
- Barometer check: periods inside the ~50 ms window after each `bmp280_trigger()` have
  σ 9.2 µs against 3.8 µs elsewhere, with the same mean (9734.4 vs 9734.0 µs). The
  worst-case period (9.626 / 9.841 ms) occurred outside the windows. Only 269 trigger
  events could be identified from the busy pin, about half of those expected.
- The ISR-to-task latency, split from the sensor's clock jitter, is the m4 latency
  measurement (INT1 against the sample pin on a third channel).

## Hardware

- STM32F407G-DISC1 (ST-LINK/V2.1), HSI 16 MHz, flashed with `make flash` (OpenOCD, `board/stm32f4discovery.cfg`)
- USART2 at 115200 8N1 via external USB-serial adapter: **PA2 = MCU TX → adapter RX, PA3 = MCU RX ← adapter TX, GND common**
- BMP280 on I2C1 (temp/pressure, ~43 ms conversion, ~1 Hz)
- ADXL345 on SPI1 (3-axis accel, up to 100 Hz); on-board LIS3DSH stays parked
- SD card on SPI2 (FatFs)
- LEDs PD12-PD15 (green, orange, red, blue)
- Kingst LA1010 logic analyzer (KingstVIS) for timing captures

## Decisions

| Question | Choice | Consequence |
|---|---|---|
| Accel sample trigger | ADXL345 INT1 (DATA_READY) → EXTI → task notification | One jumper wire; pin TBD against UM1472 (avoid PE0/PE1, the LIS3DSH INTs). Samples locked to the sensor's ODR clock |
| Target rate | Up to 100 Hz | Keep 5/20 Hz runs for the P3 comparison |
| Clock | Stay on HSI 16 MHz, `-O0`, hard-float | Every delta vs P3 is attributable to the RTOS. PLL is the ML project's lever |
| SPI2 DMA | In scope, milestone 5 | P3 figure-06 (11.04 µs/byte, ~90 KB/s) is the "before" |
| Allocation | Static only, no heap (`configSUPPORT_DYNAMIC_ALLOCATION 0`) | All kernel RAM visible in `.bss`; no runtime OOM path. `configKERNEL_PROVIDED_STATIC_MEMORY 1` supplies idle task memory |
| Kernel import | Vendored under `Lib/FreeRTOS-Kernel/`, pinned **V11.3.2** (`640d0c4`), `VERSION` file, never edited | Clone-and-build, no submodules. V11.3.2 vs 11.3.1 only touches MPU/ARMv8-M ports |
| Tick | 1 kHz | `millis()` = `xTaskGetTickCount()`; `Inc/millis.h` errors at compile time otherwise |
| Priorities | `configMAX_PRIORITIES 6` | A 7th is added when `infer_task` needs a slot between logger and env |
| Repo | `stm32f4-rtos-datalogger`; first commit imports P3 citing the source SHA | Milestone-tag history |

### `rate` becomes a discrete set

The ADXL345 only offers fixed ODRs (…6.25, 12.5, 25, 50, 100, 200… Hz). With
DATA_READY driving the sampler, `rate n` uses the lowest ODR that n divides exactly,
plus decimation by ODR/n in `accel_task`. Supported: **1, 2, 4, 5, 10, 20, 25, 50,
100 Hz**; anything else is rejected with the list. Both P3 comparison points
(5 → ODR 25 ÷5, 20 → ODR 100 ÷5) land exactly.

`adxl345.c` gains `BW_RATE`, `INT_MAP` and `INT_ENABLE` setup. Reading DATAX0..DATAZ1
clears DATA_READY, so the existing multi-byte XYZ read is the acknowledge.

The sample period now comes from the ADXL345's oscillator. On the LA, separate
*jitter* (period-to-period spread, the RTOS's responsibility) from *drift* (mean
period vs nominal, the sensor's).

## Layout

```
Makefile  STM32F407VGTX_FLASH.ld  Startup/
Inc/      FreeRTOSConfig.h  millis.h  stm32f407xx.h (+NVIC_IPR, DWT, TIM2/3, DMA1, EXTI)  *.h
Src/drivers/  i2c.c spi.c uart2.c uart_cmd.c rtc.c bmp280.c adxl345.c sdcard.c
Src/app/      main.c task_accel.c task_env.c task_logger.c task_cli.c console.c
Src/          datalog.c diskio.c fmt.c rtos_hooks.c probe.c system_stm32f4xx.c syscalls.c sysmem.c
Lib/fatfs/  Lib/FreeRTOS-Kernel/  tools/  docs/
```

## Driver carry-over

| File | Change | Reason |
|---|---|---|
| `i2c.c` `spi.c` `fmt.c` `diskio.c` `ff.c`, linker script, startup | none | Spins are µs-bounded; each bus has one owning task |
| `stm32f407xx.h` | additive | new peripherals |
| `adxl345.c` | additive | ODR + DATA_READY interrupt config |
| `uart2.c` | init kept; output path moves to `console.c` | printing must not block the caller |
| `sdcard.c` `rtc.c` | `millis()` shim over `xTaskGetTickCount()` (done) | `systick.c` is gone |
| `datalog.c` | sync on elapsed time (5 s) | fixes P3 limitation re `DATALOG_SYNC_ROWS` |
| `bmp280.c` | split into `bmp280_trigger()` / `bmp280_fetch()` | 43 ms busy-poll becomes `vTaskDelay` |
| `uart_cmd.c` | ISR feeds a stream buffer instead of the hand ring; NVIC priority 7 (done) | parser unchanged |
| `systick.c` | deleted (done) | kernel owns SysTick |
| `main.c` | rewritten | |

### Porting traps

1. Tick is dead until the scheduler starts, so `millis()` timeouts in `sdcard_init`
   and `rtc_init` never expire: bring-up runs inside `init_task`, not `main()`.
2. Every IRQ resets to NVIC priority 0, above `configMAX_SYSCALL_INTERRUPT_PRIORITY`
   (5), where `FromISR` calls are illegal. Fix: set each IRQ's priority (5..15) with
   `NVIC_IPR_BYTE(irq) = prio << 4` before enabling it. `configASSERT` (on) catches
   misses via `vPortValidateInterruptPriority()`. Applies to USART2 (done: 7), EXTI, DMA.
3. CCM RAM is not DMA-reachable: stacks may go there later; DMA buffers and the
   future ML tensor arena may not.
4. FPU is enabled in `SystemInit()`, which the CM4F port requires before the
   scheduler starts.

## Task structure

| Task | Prio | Stack (words, initial) | Wakes on | Owns |
|---|---|---|---|---|
| `init_task` | 5 | 512 | start | bring-up, then creates tasks and deletes itself |
| `accel_task` | 4 | 256 | EXTI notification (ADXL INT1) | SPI1 |
| `env_task` | 3 | 256 | 1 Hz `xTaskPeriodicDelay` (returns skipped periods) | I2C1 |
| `logger_task` | 2 | 640 | `logger_q`; DMA TC notify (M5) | SPI2 + FatFs |
| `cli_task` | 1 | 384 | RX stream buffer | config changes via messages |
| idle | 0 | 128 | — | — |

Priorities follow deadline tightness: accel (10 ms at 100 Hz, jitter-measured) above
env (1 Hz, but must not wait behind an SD stall) above logger (slow, absorbs stalls
through its queue) above CLI (human timescale). `init_task` is highest so the tasks it
creates cannot preempt bring-up. Every task has a unique priority, so time slicing
never applies.

No timer service task (`configUSE_TIMERS=0`). IRQ priorities: `configPRIO_BITS=4`,
syscall ceiling 5; EXTI 5, DMA/USART2 6–7; 0–4 reserved for ISRs that never call the
kernel. Stacks trimmed to HWM + ~30% in M4; `configCHECK_FOR_STACK_OVERFLOW=2` until then.

### Messages

`log_msg_t { type, seq, t_cyc, union { accel xyz; env temp/press } }`, `t_cyc` is the raw DWT
CYCCNT (16 MHz; the logger converts to µs and unwraps the 268 s rollover). Producers send with zero timeout to one queue **per consumer** and count drops
per consumer, so a stalled SD card cannot backpressure sampling and a future inference
task cannot starve logging. Env readings attach to the next accel row (CSV schema
unchanged from P3); the skew is ≤ one accel period, stated in the README.

### Bus protection

Single owner task per bus, no mutexes: I2C1 → `env_task`, SPI1 → `accel_task`,
SPI2/FatFs → `logger_task` (`FF_FS_REENTRANT=0`). Enforced by keeping the handle static
to the task file plus a `configASSERT` on the calling task. A priority-inheritance mutex
enters the bus layer when a second task needs the same bus (likely the LIS3DSH), and its
cost is measured then.

Genuinely shared: USART2 TX (mutex → stream buffer → TXE ISR; full buffer drops and
counts, never blocks) and runtime config (`rate` → `xTaskNotify` with overwrite to
`accel_task`; start/stop → logger).

### ML readiness

A future `infer_task` (real inference is Project 5) sits between logger and env, below
the sampler so it never adds jitter. It consumes its own queue and assembles windows
itself, so `accel_task` never changes. `seq` gives gap detection; `t_us` gives window
timing. `v4.0` adds only a stub consumer (windowed RMS) to prove and cost the fan-out.

## Measurements

| Metric | Method | P3 baseline |
|---|---|---|
| Sample jitter (min/max/σ/p99) | probe GPIO at accel read start, LA1010 ≥10k periods → `tools/jitter.py`; 5/20/100 Hz, SD sync + echo active. DWT histogram via `stats` without the LA | same probe on a branch off `v3.0` |
| ISR→task latency | pin A in EXTI ISR, pin B on task wake | n/a (RTOS cost) |
| Task timeline | `traceTASK_SWITCHED_IN` writes task ID to 3 GPIOs | becomes the measured task diagram |
| Stack HWM | `uxTaskGetStackHighWaterMark` after a soak covering all commands + syncs | — |
| Flash/RAM delta | `arm-none-eabi-size` vs `v3.0`, same flags; kernel share via `nm --size-sort` + map | 37200 B flash / 4144 B bss |
| CPU idle % | run-time stats on TIM2 @ 100 kHz (or 64-bit counter @ 1 MHz) | busy-pin duty cycle on P3 branch |
| Tick overhead | SysTick ISR duration × 1 kHz | n/a |
| Worst SD stall | probe around `f_write`/`f_sync`, max over soak → queue depth = rate × stall × 2 | explains P3 overruns |
| Queue HWM + drops | peak `uxQueueMessagesWaiting`, per-consumer drop counters; target 0 | P3 overrun counter |
| Max sustained rate | step up the supported set until drops > 0 | 20 Hz |
| CLI latency | LA: RX edge → echo TX edge | up to one tick (200 ms @ 5 Hz) |
| Soak | N h, rows on card vs expected | P3 row still pending |
| DMA gain (M5) | SPI2 LA byte spacing + CPU % before/after | figure-06: 11.04 µs/byte |

## Milestones

Each milestone ends with a git tag of the same name.

1. **`m1-kernel`**: repo, vendored kernel, Makefile, config, handler mapping, hooks,
   NVIC priorities, `millis()` shim, console + `cli_task`. Capture the P3 baseline on
   its probe branch. *Demo:* two LED tasks, live CLI, kernel-only size cost. The
   P3 baseline capture moved to the start of m2.
2. **`m2-sensor-tasks`**: P3 jitter baseline capture, INT1 wire + EXTI, `init_task` bring-up, `accel_task`,
   `env_task` with BMP280 split, probe pins + trace macro, console output only.
   *Demo:* LA shows accel cadence unaffected by the baro conversion; first jitter
   number vs P3.
3. **`m3-logger`**: `logger_q`, logger owns SPI2/FatFs, time-based sync, discrete
   `rate` set, start/stop, drop counters, stall measured → queue sized. *Demo:* P3
   feature parity, same CSV schema. The log-check script moved to m4, where the soak needs it.
4. **`m4-measured`**: LSI calibration (measure the LSI against HSI with a timer capture and correct the RTC prescaler; the wall-clock column ran about 6% fast), run-time stats, `stats` command, stack trim, task-timeline
   capture, full table at 5/20/100 Hz, overnight soak. *Demo:* populated results.
5. **`m5-dma`**: SPI2 TX/RX on DMA1, logger blocks on TC notification. *Demo:*
   before/after SPI2 capture vs figure-06, throughput and CPU recovered.
6. **`v4.0`**: stub second consumer (`infer_q` → windowed RMS) proving fan-out and
   costing it; README-as-spec, task diagram SVG, design decisions, war stories. README
   sections written per milestone.

## Open items

- RTC accuracy: the LSI-driven wall clock ran about 6% fast over a 261 s log; calibration is planned in m4.
- Confirm the ADXL345 ODR accuracy figure from the datasheet before quoting drift.

## Working conventions

- Commits as `EdgeAIGuy <theedgeaiguy@gmail.com>`; push over the `github-edgeaiguy` SSH host alias.
- No AI/tool attribution in commits, PRs or code comments.
- Comments: one or two lines of *why* per function; no ticket IDs or process history in source.
- `Lib/` is vendored and never edited.
- Build check: `make clean && make` must be warning-free; quote `arm-none-eabi-size` in commits that change size.
