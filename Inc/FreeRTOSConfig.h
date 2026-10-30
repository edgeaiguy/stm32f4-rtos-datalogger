#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* Scheduler. HSI 16 MHz, no PLL, so every delta vs Project 3 is the RTOS's. */
#define configCPU_CLOCK_HZ                      16000000UL
#define configTICK_RATE_HZ                      1000
#define configUSE_PREEMPTION                    1
#define configUSE_TIME_SLICING                  1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 1
#define configTICK_TYPE_WIDTH_IN_BITS           TICK_TYPE_WIDTH_32_BITS

/* 0 idle, 1 cli, 2 logger, 3 env, 4 accel, 5 init. */
#define configMAX_PRIORITIES                    6
#define configMINIMAL_STACK_SIZE                128
#define configMAX_TASK_NAME_LEN                 12

/* Static only: every byte of kernel RAM shows up in .bss, no runtime OOM. */
#define configSUPPORT_STATIC_ALLOCATION         1
#define configSUPPORT_DYNAMIC_ALLOCATION        0
#define configKERNEL_PROVIDED_STATIC_MEMORY     1

/* Features. Mutex for the shared console; no software timers, no event groups. */
#define configUSE_TASK_NOTIFICATIONS            1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES   1
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             0
#define configUSE_COUNTING_SEMAPHORES           0
#define configUSE_STREAM_BUFFERS                1
#define configUSE_EVENT_GROUPS                  0
#define configUSE_TIMERS                        0
#define configQUEUE_REGISTRY_SIZE               0
#define configUSE_NEWLIB_REENTRANT              0
#define configENABLE_BACKWARD_COMPATIBILITY     0

/* Hooks. Overflow method 2 also checks a fill pattern at the stack end; on
 * until stacks are trimmed against measured high-water marks. */
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configUSE_MALLOC_FAILED_HOOK            0
#define configCHECK_FOR_STACK_OVERFLOW          2

/* Cortex-M NVIC: 4 implemented bits, stored in the top of each 8-bit field.
 * ISRs at 5..15 may call FromISR APIs; 0..4 are never masked by the kernel. */
#define configPRIO_BITS                         4
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY 15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5
#define configKERNEL_INTERRUPT_PRIORITY \
    (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))

/* Halt with interrupts masked so the debugger stops on the failing line. */
#define configASSERT(x) \
    do { if ((x) == 0) { portDISABLE_INTERRUPTS(); for (;;) {} } } while (0)

/* The startup file's vector table names; the port asserts they are installed directly. */
#define vPortSVCHandler                         SVC_Handler
#define xPortPendSVHandler                      PendSV_Handler
#define xPortSysTickHandler                     SysTick_Handler

/* Optional API. vTaskSuspend makes portMAX_DELAY block forever instead of ~49 days. */
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskDelayUntil                 1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_xTaskGetCurrentTaskHandle       1
#define INCLUDE_uxTaskGetStackHighWaterMark     1
#define INCLUDE_vTaskPrioritySet                0
#define INCLUDE_uxTaskPriorityGet               0

#endif /* FREERTOS_CONFIG_H */
