#include "FreeRTOS.h"
#include "task.h"
#include "console.h"
#include "dwt.h"
#include "init_task.h"
#include "probe.h"
#include "uart2.h"
#include "uart_cmd.h"

int main(void) {
    uart2_init();
    console_init();
    uart_cmd_init();
    probe_init();
    dwt_init();

    init_task_start();
    vTaskStartScheduler();
    for (;;) {}   /* only reached if the scheduler fails to start */
}
