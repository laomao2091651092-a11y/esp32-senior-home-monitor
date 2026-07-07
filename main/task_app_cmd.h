#ifndef _TASK_APP_CMD_H
#define _TASK_APP_CMD_H

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

void task_app_cmd_start(void);

esp_err_t app_send_command_nonblock(const char *cmd, void *arg);

#ifdef __cplusplus
}
#endif

#endif
