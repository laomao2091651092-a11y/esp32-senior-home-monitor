#include <stdio.h>
#include <string.h>
#include "task_app_cmd.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "02_led.h"
#include "task_app_radar.h"
#include "task_monitor.h"

static const char *TAG = "CMD_TASK";

#define CMD_QUEUE_LEN       10
#define CMD_NAME_MAX_LEN    16

typedef struct {
    char name[CMD_NAME_MAX_LEN];
    void *arg;
} cmd_event_t;

static TaskHandle_t s_cmd_task_handle = NULL;
static QueueHandle_t s_cmd_queue = NULL;

static void process_led_command(const char *cmd)
{
    if (strcmp(cmd, "LED_ON") == 0) {
        ESP_LOGI(TAG, "Executing: LED_ON");
        led_set_state(true);
    } else if (strcmp(cmd, "LED_OFF") == 0) {
        ESP_LOGI(TAG, "Executing: LED_OFF");
        led_set_state(false);
    } else if (strcmp(cmd, "LED_TOGGLE") == 0) {
        ESP_LOGI(TAG, "Executing: LED_TOGGLE");
        led_toggle();
    } else {
        ESP_LOGW(TAG, "Unknown LED command: %s", cmd);
    }
}

static void task_app_cmd(void *pvParameter)
{
    (void)pvParameter;
    ESP_LOGI(TAG, "Command task started");

    cmd_event_t evt;
    esp_task_wdt_add(NULL);

    while (1) {
        if (xQueueReceive(s_cmd_queue, &evt, pdMS_TO_TICKS(3000)) == pdPASS) {
            ESP_LOGI(TAG, "Got command: %s", evt.name);

            if (strncmp(evt.name, "LED_", 4) == 0) {
                process_led_command(evt.name);
            } else if (strcmp(evt.name, "AUTO_LIGHT_TOGGLE") == 0) {
                static bool s_auto_light = true;
                s_auto_light = !s_auto_light;
                task_app_radar_set_auto_light(s_auto_light);
                ESP_LOGI(TAG, "Auto light %s", s_auto_light ? "ON" : "OFF");
            } else {
                ESP_LOGW(TAG, "Unhandled command: %s", evt.name);
            }
        }
        esp_task_wdt_reset();
    }
}

esp_err_t app_send_command_nonblock(const char *cmd, void *arg)
{
    if (cmd == NULL || s_cmd_queue == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    cmd_event_t evt;
    strncpy(evt.name, cmd, CMD_NAME_MAX_LEN - 1);
    evt.name[CMD_NAME_MAX_LEN - 1] = '\0';
    evt.arg = arg;

    BaseType_t ret = xQueueSend(s_cmd_queue, &evt, 0);
    if (ret != pdPASS) {
        ESP_LOGW(TAG, "Command queue full, dropping: %s", cmd);
        return ESP_FAIL;
    }

    return ESP_OK;
}

void task_app_cmd_start(void)
{
    s_cmd_queue = xQueueCreate(CMD_QUEUE_LEN, sizeof(cmd_event_t));
    if (s_cmd_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create command queue");
        return;
    }

    BaseType_t ret = xTaskCreate(
        task_app_cmd,
        "task_cmd",
        2048,
        NULL,
        3,
        &s_cmd_task_handle
    );

    if (ret == pdPASS) {
        ESP_LOGI(TAG, "Command task created");
        task_monitor_register("cmd", s_cmd_task_handle, 2048);
    } else {
        ESP_LOGE(TAG, "Failed to create command task");
    }
}
