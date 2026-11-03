# Project 4 plan: FreeRTOS port of the datalogger

Port of [stm32f4-datalogger](https://github.com/edgeaiguy/stm32f4-datalogger)
(Project 3, tag `v3.0` at `1d35a76`) from a bare-metal superloop to FreeRTOS tasks.
Same sensors, SD card, CSV schema and CLI; every timing and size figure is measured
against the bare-metal version.

Plan written 2026-10-23. Status last updated 2026-11-01.

## Status

**Current milestone: `m1-kernel`, in progress.** The scheduler runs on the board
with two LED tasks. Next up: kernel-only size cost, P3 baseline, README, tag.

| Milestone | State |
|---|---|
| `m1-kernel` | In progress (see checklist below) |
| `m2-sensor-tasks` | Not started |
| `m3-logger` | Not started |
| `m4-measured` | Not started |
| `m5-dma` | Not started |
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
- [ ] P3 baseline capture: probe branch off `v3.0`, probe GPIO at accel read start, LA1010 jitter at 5/20 Hz, busy-pin duty cycle
- [ ] README section for m1; tag `m1-kernel`

**m1 demo:** two LED tasks, live CLI, kernel-only size cost, P3 baseline numbers.

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

`log_msg_t { type, seq, t_us, union { accel xyz; env temp/press } }`, `t_us` from DWT
CYCCNT. Producers send with zero timeout to one queue **per consumer** and count drops
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
   its probe branch. *Demo:* two LED tasks, live CLI, kernel-only size cost, P3
   baseline numbers.
2. **`m2-sensor-tasks`**: INT1 wire + EXTI, `init_task` bring-up, `accel_task`,
   `env_task` with BMP280 split, probe pins + trace macro, console output only.
   *Demo:* LA shows accel cadence unaffected by the baro conversion; first jitter
   number vs P3.
3. **`m3-logger`**: `logger_q`, logger owns SPI2/FatFs, time-based sync, discrete
   `rate` set, start/stop, drop counters, stall measured → queue sized. *Demo:* P3
   feature parity, same CSV schema, analysis script on the file.
4. **`m4-measured`**: run-time stats, `stats` command, stack trim, task-timeline
   capture, full table at 5/20/100 Hz, overnight soak. *Demo:* populated results.
5. **`m5-dma`**: SPI2 TX/RX on DMA1, logger blocks on TC notification. *Demo:*
   before/after SPI2 capture vs figure-06, throughput and CPU recovered.
6. **`v4.0`**: stub second consumer (`infer_q` → windowed RMS) proving fan-out and
   costing it; README-as-spec, task diagram SVG, design decisions, war stories. README
   sections written per milestone.

## Open items

- Pick and verify the EXTI pin for ADXL INT1 against UM1472.
- Confirm the ADXL345 ODR accuracy figure from the datasheet before quoting drift.

## Working conventions

- Commits as `EdgeAIGuy <theedgeaiguy@gmail.com>`; push over the `github-edgeaiguy` SSH host alias.
- No AI/tool attribution in commits, PRs or code comments.
- Comments: one or two lines of *why* per function; no ticket IDs or process history in source.
- `Lib/` is vendored and never edited.
- Build check: `make clean && make` must be warning-free; quote `arm-none-eabi-size` in commits that change size.
