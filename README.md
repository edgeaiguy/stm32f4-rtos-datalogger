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
