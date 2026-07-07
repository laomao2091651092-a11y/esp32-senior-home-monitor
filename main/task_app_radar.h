#ifndef TASK_APP_RADAR_H
#define TASK_APP_RADAR_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RADAR_PRESENCE_TIMEOUT_MS    30000
#define RADAR_AUTO_LIGHT_OFF_DELAY_MS  30000

typedef enum {
    SLEEP_STATE_AWAKE = 0,
    SLEEP_STATE_SLEEPING = 1,
    SLEEP_STATE_WAKING = 2,
} sleep_state_t;

typedef struct {
    bool auto_light_enabled;
    uint32_t auto_light_off_delay_ms;
    bool sleep_monitor_enabled;
    uint8_t sleep_start_hour;
    uint8_t sleep_end_hour;
} radar_config_t;

esp_err_t task_app_radar_init(void);
bool task_app_radar_get_presence(void);
uint16_t task_app_radar_get_distance(void);
sleep_state_t task_app_radar_get_sleep_state(void);
uint32_t task_app_radar_get_sleep_duration_min(void);
uint8_t task_app_radar_get_wakeup_count(void);
const char* task_app_radar_get_sleep_quality(void);
void task_app_radar_set_auto_light(bool enable);
void task_app_radar_set_sleep_monitor(bool enable);

#ifdef __cplusplus
}
#endif

#endif
