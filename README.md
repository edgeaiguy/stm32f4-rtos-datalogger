# STM32F4 RTOS Datalogger

FreeRTOS port of [stm32f4-datalogger](https://github.com/edgeaiguy/stm32f4-datalogger)
(`v3.0`): the same sensors, SD card and CLI, rebuilt as tasks. Every timing and size
figure is measured against the bare-metal version.

**Status:** `m2-sensor-tasks` done; `m3-logger` next

Plan, milestones and current status: [docs/PLAN.md](docs/PLAN.md).

## m1: kernel

FreeRTOS-Kernel V11.3.2 is vendored under `Lib/FreeRTOS-Kernel/`, unmodified, and runs
on the STM32F407 at a 1 kHz tick from HSI 16 MHz. Everything is statically allocated
(`configSUPPORT_DYNAMIC_ALLOCATION 0`), so all kernel RAM shows up in `.bss`.

On the board: two LED tasks (green PD12 at 1 Hz, orange PD13 at 5 Hz) and a live CLI
on USART2 (PA2 TX, PA3 RX, 115200 8N1).

- **Console TX:** tasks write into a 256-byte stream buffer under a mutex; the USART2
  TXE interrupt drains it. A full buffer drops bytes and counts them, so printing never
  blocks a task on the UART.
- **CLI RX:** the USART2 RX interrupt pushes each byte into a stream buffer with
  `xStreamBufferSendFromISR`; `cli_task` blocks on it. USART2 sits at NVIC priority 7,
  below the syscall ceiling of 5, which `FromISR` calls require.

### Size cost vs v3.0

Same flags (`-O0`, hard-float). Flash is text + data.

| | Flash | bss |
|---|---|---|
| `v3.0` | 37200 | 4144 |
| m1 | 57236 | 7728 |
| Delta | +20036 | +3584 |

The kernel objects alone are 22169 B of flash and 880 B of bss. The comparison is not
yet clean: unused P3 drivers are still linked.

## m2: sensor tasks

Bring-up now runs inside a priority 5 `init_task`, because `sdcard_init` and `rtc_init`
time out on the kernel tick, which does not advance before the scheduler starts. It
starts the other tasks and deletes itself.

| Task | Priority | Wakes on | Owns |
|---|---|---|---|
| `init_task` | 5 | start | bring-up |
| `accel_task` | 4 | ADXL345 INT1 | SPI1 |
| `env_task` | 3 | 1 Hz `xTaskDelayUntil` | I2C1 |
| `cli_task` | 1 | RX stream buffer | |

- **Accelerometer:** INT1 (DATA_READY, ODR 100 Hz) drives EXTI1 on PD1. The ISR calls
  `vTaskNotifyGiveFromISR`; `accel_task` reads XYZ over SPI1, which also clears INT1. A
  1 s take timeout recovers from a missed edge.
- **Barometer:** the BMP280 read is split into `bmp280_trigger()` and `bmp280_fetch()`.
  `env_task` sleeps through the ~43 ms conversion instead of polling the bus.
- **Probes:** PD0 rises at the accel read start; PD2 is high while any non-idle task
  runs (`traceTASK_SWITCHED_IN`). `PROBE_ENABLED 0` compiles both out.

### Sample cadence

Logic-analyzer capture, 500 s, PD0 rising edges (`tools/jitter.py`):

| Periods | Mean | σ | p99 \|dev\| | Peak-to-peak | Busy |
|---|---|---|---|---|---|
| 51365 | 9734.0 µs | 4.1 µs | 11 µs | 215 µs | 7.6% |

The mean is 2.7% fast of the 100 Hz requested: that is the ADXL345's own clock, not the
scheduler. Periods in the 50 ms after each barometer trigger have σ 9.2 µs against
3.8 µs elsewhere, with the same mean. There is no SD card or logger yet, so this is not
directly comparable with the v3.0 figures; the like-for-like comparison comes in m3 and m4.

## m3: logger

A priority 2 `logger_task` now writes the same CSV the bare-metal version did.
`accel_task` and `env_task` post readings into a 32-item queue with a zero timeout; each
message carries a per-type sequence number, so a gap on arrival shows exactly what was
dropped. The logger converts the raw DWT cycle count to milliseconds, attaches each env
reading to the next accel row (empty columns otherwise), and syncs the file every 5 s.
FatFs is used by one task at a time: `init_task` opens the file during bring-up, then
the logger owns it.

| Task | Priority | Wakes on | Owns |
|---|---|---|---|
| `init_task` | 5 | start | bring-up, opens the log file |
| `accel_task` | 4 | ADXL345 INT1 | SPI1 |
| `env_task` | 3 | 1 Hz `xTaskDelayUntil` | I2C1 |
| `logger_task` | 2 | `logger_q` | SPI2 + FatFs |
| `cli_task` | 1 | RX stream buffer | |

Console commands: `rate <hz>` (1, 2, 4, 5, 10, 20, 25, 50, 100), `start`, `stop`,
`status`, `time YYYY-MM-DD HH:MM:SS`. `rate` reaches `accel_task` through a task
notification; the sensor runs the lowest output rate the requested rate divides exactly
and the task forwards every Nth sample. `tools/settime.py` sets the board clock from the
host over the console UART.

### 20 Hz against v3.0

500 s logic-analyzer capture with the SD card writing, logged-sample periods:

| | RTOS m3 | v3.0 |
|---|---|---|
| σ | 6.5-7.9 µs | 734 µs |
| p99 \|dev\| | 17-36 µs | 3.4 ms |
| Peak-to-peak | 159-236 µs | 64.5 ms |
| CPU busy | 8.6% | 21.3% |

The SD card still stalls the logger: a sector write takes 5-15 ms, a sync about 43 ms and
the worst seen was 68 ms. In v3.0 those stalls delayed the next sample; here they delay
only the logger, whose queue peaked at 2 of its 32 slots. The sample rate is set by the
ADXL345's own clock, which runs about 2.6% fast, so "20 Hz" logs at 20.52 Hz. The
wall-clock column comes from the RTC's internal oscillator and runs several percent fast;
`t_ms` is the reliable time base.
