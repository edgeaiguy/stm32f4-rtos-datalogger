#include "FreeRTOS.h"
#include "task.h"
#include "console.h"
#include "uart_cmd.h"
#include "task_cli.h"

#define CLI_STACK_WORDS 384

static StackType_t  cli_stack[CLI_STACK_WORDS];
static StaticTask_t cli_tcb;

/* Blocks inside uart_cmd_wait, so the task costs no CPU between commands. */
static void cli_task(void *arg) {
    (void)arg;
    uart_cmd_t cmd;
    console_puts("cli ready\r\n");
    for (;;) {
        if (uart_cmd_wait(&cmd)) {
            console_printf("cmd %u arg %u\r\n", (unsigned)cmd.id, (unsigned)cmd.arg);
        }
    }
}

void task_cli_start(void) {
    xTaskCreateStatic(cli_task, "cli", CLI_STACK_WORDS, NULL, 1, cli_stack, &cli_tcb);
}
