# STM32F4 RTOS Datalogger

FreeRTOS port of [stm32f4-datalogger](https://github.com/edgeaiguy/stm32f4-datalogger)
(`v3.0`): the same sensors, SD card and CLI, rebuilt as tasks. Every timing and size
figure is measured against the bare-metal version.

**Status:** `m1-kernel` done; `m2-sensor-tasks` in progress

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
