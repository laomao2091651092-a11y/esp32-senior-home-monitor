#include <stdio.h>
#include <string.h>
#include "task_monitor.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "TASK_MONITOR";

#define MAX_MONITORED_TASKS  16

typedef struct {
    char name[16];
    TaskHandle_t handle;
    uint32_t stack_size;
    bool used;
} monitored_task_t;

static monitored_task_t s_tasks[MAX_MONITORED_TASKS];
static TaskHandle_t s_monitor_task_handle = NULL;

void task_monitor_register(const char *name, TaskHandle_t handle,
                           uint32_t stack_size)
{
    if (name == NULL || handle == NULL) return;

    for (int i = 0; i < MAX_MONITORED_TASKS; i++) {
        if (!s_tasks[i].used) {
            strncpy(s_tasks[i].name, name, sizeof(s_tasks[i].name) - 1);
            s_tasks[i].name[sizeof(s_tasks[i].name) - 1] = '\0';
            s_tasks[i].handle = handle;
            s_tasks[i].stack_size = stack_size;
            s_tasks[i].used = true;
            ESP_LOGI(TAG, "Registered task: %s (stack: %lu bytes)",
                     s_tasks[i].name, (unsigned long)s_tasks[i].stack_size);
            return;
        }
    }

    ESP_LOGW(TAG, "Task monitor full, cannot register %s", name);
}

static void task_monitor(void *pvParameter)
{
    (void)pvParameter;
    ESP_LOGI(TAG, "Task monitor started");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));

        ESP_LOGI(TAG, "=== Task Status ===");
        for (int i = 0; i < MAX_MONITORED_TASKS; i++) {
            if (s_tasks[i].used) {
                UBaseType_t free_watermark =
                    uxTaskGetStackHighWaterMark(s_tasks[i].handle);
                uint32_t free_bytes = free_watermark * sizeof(StackType_t);
                uint32_t used_bytes = s_tasks[i].stack_size - free_bytes;
                uint8_t usage = (uint8_t)((used_bytes * 100) / s_tasks[i].stack_size);

                ESP_LOGI(TAG, "  %-16s stack: %lu/%lu bytes (%u%%)",
                         s_tasks[i].name,
                         (unsigned long)used_bytes,
                         (unsigned long)s_tasks[i].stack_size,
                         usage);
            }
        }
        ESP_LOGI(TAG, "==================");
    }
}

void task_monitor_start(void)
{
    memset(s_tasks, 0, sizeof(s_tasks));

    BaseType_t ret = xTaskCreate(
        task_monitor,
        "task_monitor",
        2048,
        NULL,
        1,
        &s_monitor_task_handle
    );

    if (ret == pdPASS) {
        ESP_LOGI(TAG, "Task monitor created");
    } else {
        ESP_LOGE(TAG, "Failed to create task monitor");
    }
}
