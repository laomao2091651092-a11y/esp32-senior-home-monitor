#ifndef _TASK_APP_WIFI_MANAGE_H
#define _TASK_APP_WIFI_MANAGE_H

#include <stdbool.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WIFI_STATE_DISCONNECTED = 0,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_ERROR
} wifi_state_t;

void app_wifi_init(void);
void task_app_wifi_manage_start(void);
wifi_state_t app_get_wifi_state(void);
bool app_wifi_is_connected(void);
bool app_wifi_wait_for_connection(TickType_t timeout_ticks);

#ifdef __cplusplus
}
#endif

#endif
