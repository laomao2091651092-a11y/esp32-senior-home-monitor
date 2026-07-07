#include <stdio.h>
#include "task_app_btn.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "02_button.h"
#include "task_app_cmd.h"
#include "task_monitor.h"

static const char *TAG = "BTN_TASK";

#define BTN_QUEUE_LEN       5

typedef struct {
    button_state_t state;
} btn_event_t;

static TaskHandle_t s_btn_task_handle = NULL;
static QueueHandle_t s_btn_queue = NULL;

static void button_event_callback(button_state_t state, void *arg)
{
    (void)arg;
    btn_event_t evt = {.state = state};
    if (s_btn_queue != NULL) {
        xQueueSendFromISR(s_btn_queue, &evt, NULL);
    }
}

static void task_app_btn(void *pvParameter)
{
    (void)pvParameter;
    ESP_LOGI(TAG, "Button task started");

    btn_event_t evt;
    esp_task_wdt_add(NULL);

    while (1) {
        if (xQueueReceive(s_btn_queue, &evt, pdMS_TO_TICKS(3000)) == pdPASS) {
            ESP_LOGI(TAG, "Button event: %s",
                     evt.state == BUTTON_PRESSED ? "PRESSED" : "RELEASED");

            if (evt.state == BUTTON_RELEASED) {
                app_send_command_nonblock("LED_TOGGLE", NULL);
                ESP_LOGI(TAG, "LED toggle command sent");
            }
        }
        esp_task_wdt_reset();
    }
}

void task_app_btn_start(void)
{
    s_btn_queue = xQueueCreate(BTN_QUEUE_LEN, sizeof(btn_event_t));
    if (s_btn_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create button queue");
        return;
    }

    esp_err_t ret = button_init(button_event_callback, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init button: %s", esp_err_to_name(ret));
        return;
    }

    BaseType_t xret = xTaskCreate(
        task_app_btn,
        "task_btn",
        2048,
        NULL,
        4,
        &s_btn_task_handle
    );

    if (xret == pdPASS) {
        ESP_LOGI(TAG, "Button task created");
        task_monitor_register("btn", s_btn_task_handle, 2048);
    } else {
        ESP_LOGE(TAG, "Failed to create button task");
    }
}
