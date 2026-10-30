#include "FreeRTOS.h"
#include "task.h"

/* Halts like configASSERT; pcTaskName in the debugger names the overflowing task. */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    (void)xTask;
    (void)pcTaskName;
    portDISABLE_INTERRUPTS();
    for (;;) {}
}
