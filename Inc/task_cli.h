#ifndef TASK_CLI_H
#define TASK_CLI_H

/* Creates cli_task at priority 1. Call after console_init() and uart_cmd_init(). */
void task_cli_start(void);

#endif // TASK_CLI_H
