#ifndef _TASK_MONITOR_H
#define _TASK_MONITOR_H

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

void task_monitor_start(void);

void task_monitor_register(const char *name, TaskHandle_t handle,
                           uint32_t stack_size);

#ifdef __cplusplus
}
#endif

#endif
