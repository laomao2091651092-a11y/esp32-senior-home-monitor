#include <stdio.h>
#include <string.h>
#include <time.h>
#include "task_app_radar.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "10_ld2450.h"
#include "02_led.h"
#include "03_gy30.h"
#include "sensor_data_mgr.h"
#include "task_monitor.h"

static const char *TAG = "RADAR_TASK";

static TaskHandle_t s_radar_task_handle = NULL;
static volatile bool s_presence = false;
static volatile uint16_t s_distance_cm = 0;
static volatile uint32_t s_last_presence_ms = 0;
static volatile uint32_t s_last_light_on_ms = 0;
static volatile bool s_auto_light_enabled = true;
static volatile bool s_manual_override = false;
static volatile uint32_t s_manual_override_until_ms = 0;
static volatile bool s_sim_mode = false;
static volatile uint32_t s_sim_counter = 0;

static volatile sleep_state_t s_sleep_state = SLEEP_STATE_AWAKE;
static volatile bool s_sleep_monitor_enabled = true;
static volatile uint32_t s_sleep_start_ms = 0;
static volatile uint32_t s_sleep_duration_ms = 0;
static volatile uint8_t s_wakeup_count = 0;
static volatile uint8_t s_sleep_quality_score = 0;
static volatile uint32_t s_last_activity_ms = 0;
static volatile float s_activity_level = 0.0f;

#define SLEEP_START_HOUR  22
#define SLEEP_END_HOUR    6

static bool is_sleep_hours(void)
{
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    int hour = timeinfo.tm_hour;
    return (hour >= SLEEP_START_HOUR || hour < SLEEP_END_HOUR);
}

static uint32_t get_current_s(void)
{
    time_t now;
    time(&now);
    return (uint32_t)now;
}

static void update_activity_level(uint8_t move_energy, uint8_t static_energy)
{
    float new_activity = (move_energy * 1.0f + static_energy * 0.5f) / 100.0f;
    if (new_activity > 1.0f) new_activity = 1.0f;
    s_activity_level = s_activity_level * 0.9f + new_activity * 0.1f;
    s_last_activity_ms = esp_timer_get_time() / 1000;
}

static void update_sleep_state(bool presence, float activity)
{
    if (!s_sleep_monitor_enabled) {
        return;
    }

    uint32_t now_ms = esp_timer_get_time() / 1000;

    switch (s_sleep_state) {
        case SLEEP_STATE_AWAKE:
            if (is_sleep_hours() && presence && activity < 0.15f) {
                uint32_t elapsed = now_ms - s_last_activity_ms;
                if (elapsed > 300000) {
                    s_sleep_state = SLEEP_STATE_SLEEPING;
                    s_sleep_start_ms = now_ms;
                    s_wakeup_count = 0;
                    ESP_LOGI(TAG, "Sleep started");
                }
            }
            break;

        case SLEEP_STATE_SLEEPING:
            if (presence && activity > 0.4f) {
                s_sleep_state = SLEEP_STATE_WAKING;
                s_wakeup_count++;
                s_sleep_duration_ms += now_ms - s_sleep_start_ms;
                ESP_LOGI(TAG, "Wake up detected (count=%d)", s_wakeup_count);
            } else if (!is_sleep_hours()) {
                s_sleep_state = SLEEP_STATE_AWAKE;
                s_sleep_duration_ms += now_ms - s_sleep_start_ms;
                uint32_t total_min = s_sleep_duration_ms / 60000;
                if (s_wakeup_count <= 1 && total_min >= 360) {
                    s_sleep_quality_score = 90;
                } else if (s_wakeup_count <= 3 && total_min >= 300) {
                    s_sleep_quality_score = 75;
                } else if (s_wakeup_count <= 5 && total_min >= 240) {
                    s_sleep_quality_score = 60;
                } else {
                    s_sleep_quality_score = 40;
                }
                ESP_LOGI(TAG, "Sleep ended: duration=%lu min, wakeups=%d, quality=%d",
                         (unsigned long)total_min, s_wakeup_count, s_sleep_quality_score);
            }
            break;

        case SLEEP_STATE_WAKING:
            if (activity < 0.2f && presence) {
                s_sleep_state = SLEEP_STATE_SLEEPING;
                s_sleep_start_ms = now_ms;
                ESP_LOGI(TAG, "Fell back asleep");
            } else if (!is_sleep_hours() || activity > 0.6f) {
                s_sleep_state = SLEEP_STATE_AWAKE;
                ESP_LOGI(TAG, "Fully awake");
            }
            break;
    }
}

static void handle_auto_light(bool presence, float light_lux)
{
    if (!s_auto_light_enabled) {
        return;
    }

    uint32_t now_ms = esp_timer_get_time() / 1000;

    if (s_manual_override && now_ms < s_manual_override_until_ms) {
        return;
    }
    s_manual_override = false;

    bool is_dark = (light_lux < 100.0f);

    if (presence && is_dark) {
        if (!led_get_state()) {
            led_set_state(true);
            s_last_light_on_ms = now_ms;
            ESP_LOGI(TAG, "Auto light ON (presence detected, dark)");
        }
        s_last_presence_ms = now_ms;
    } else if (!presence && led_get_state()) {
        uint32_t elapsed = now_ms - s_last_presence_ms;
        if (elapsed > RADAR_AUTO_LIGHT_OFF_DELAY_MS) {
            led_set_state(false);
            ESP_LOGI(TAG, "Auto light OFF (no presence for %lu ms)",
                     (unsigned long)RADAR_AUTO_LIGHT_OFF_DELAY_MS);
        }
    }
}

static void task_app_radar(void *pvParameter)
{
    (void)pvParameter;
    ESP_LOGI(TAG, "Radar task started");

    esp_task_wdt_add(NULL);

    ld2450_data_t radar_data;
    float light_lux = 0.0f;
    uint32_t sim_fail_count = 0;

    while (1) {
        esp_err_t ret = ld2450_read(&radar_data, 1000);

        if (ret == ESP_OK) {
            sim_fail_count = 0;
            if (s_sim_mode) {
                s_sim_mode = false;
                ESP_LOGI(TAG, "LD2450 reconnected, exiting simulation mode");
            }

            s_presence = radar_data.presence;
            s_distance_cm = radar_data.distance_cm;

            if (s_presence) {
                s_last_presence_ms = esp_timer_get_time() / 1000;
            }

            update_activity_level(radar_data.energy_move, radar_data.energy_static);
            update_sleep_state(s_presence, s_activity_level);

            sensor_data_mgr_update_presence(s_presence, s_distance_cm,
                                             radar_data.energy_move, radar_data.energy_static);

            sensor_data_mgr_update_sleep(s_sleep_state, s_sleep_duration_ms / 60000,
                                          s_wakeup_count, s_sleep_quality_score);

            gy30_read_light(&light_lux);
            handle_auto_light(s_presence, light_lux);

            ESP_LOGD(TAG, "Presence:%d dist:%ucm move:%d static:%d activity:%.2f sleep:%d",
                     s_presence, s_distance_cm, radar_data.energy_move,
                     radar_data.energy_static, s_activity_level, s_sleep_state);
        } else {
            sim_fail_count++;
            if (sim_fail_count >= 3 && !s_sim_mode) {
                s_sim_mode = true;
                ESP_LOGW(TAG, "LD2450 not responding, entering simulation mode");
            }

            if (s_sim_mode) {
                s_sim_counter++;

                s_presence = (s_sim_counter % 20) < 15;
                s_distance_cm = s_presence ? (100 + (s_sim_counter * 7) % 300) : 0;
                uint8_t sim_move = s_presence ? (20 + (s_sim_counter * 3) % 60) : 0;
                uint8_t sim_static = s_presence ? (10 + (s_sim_counter * 5) % 40) : 0;

                if (s_presence) {
                    s_last_presence_ms = esp_timer_get_time() / 1000;
                }

                update_activity_level(sim_move, sim_static);
                update_sleep_state(s_presence, s_activity_level);

                sensor_data_mgr_update_presence(s_presence, s_distance_cm,
                                                 sim_move, sim_static);

                sensor_data_mgr_update_sleep(s_sleep_state, s_sleep_duration_ms / 60000,
                                              s_wakeup_count, s_sleep_quality_score);

                gy30_read_light(&light_lux);
                handle_auto_light(s_presence, light_lux);
            } else {
                uint32_t elapsed = (esp_timer_get_time() / 1000) - s_last_presence_ms;
                if (elapsed > RADAR_PRESENCE_TIMEOUT_MS && s_presence) {
                    s_presence = false;
                    ESP_LOGW(TAG, "Presence timeout, marking as absent");
                }
            }
        }

        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

esp_err_t task_app_radar_init(void)
{
    esp_err_t ret = ld2450_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init LD2450: %s", esp_err_to_name(ret));
        return ret;
    }

    BaseType_t xReturn = xTaskCreate(
        task_app_radar,
        "radar_task",
        4096,
        NULL,
        5,
        &s_radar_task_handle
    );

    if (xReturn != pdPASS) {
        ESP_LOGE(TAG, "Failed to create radar task");
        ld2450_deinit();
        return ESP_FAIL;
    }

    task_monitor_register("radar", s_radar_task_handle, 4096);

    ESP_LOGI(TAG, "Radar task created");
    return ESP_OK;
}

bool task_app_radar_get_presence(void)
{
    return s_presence;
}

uint16_t task_app_radar_get_distance(void)
{
    return s_distance_cm;
}

sleep_state_t task_app_radar_get_sleep_state(void)
{
    return s_sleep_state;
}

uint32_t task_app_radar_get_sleep_duration_min(void)
{
    return s_sleep_duration_ms / 60000;
}

uint8_t task_app_radar_get_wakeup_count(void)
{
    return s_wakeup_count;
}

const char* task_app_radar_get_sleep_quality(void)
{
    if (s_sleep_quality_score >= 85) return "excellent";
    if (s_sleep_quality_score >= 70) return "good";
    if (s_sleep_quality_score >= 55) return "fair";
    return "poor";
}

void task_app_radar_set_auto_light(bool enable)
{
    s_auto_light_enabled = enable;
    s_manual_override = false;
    ESP_LOGI(TAG, "Auto light %s", enable ? "enabled" : "disabled");
}

void task_app_radar_set_sleep_monitor(bool enable)
{
    s_sleep_monitor_enabled = enable;
    ESP_LOGI(TAG, "Sleep monitor %s", enable ? "enabled" : "disabled");
}
