#include <stdio.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_task_wdt.h"

#include "sensor_data_mgr.h"
#include "bsp.h"
#include "task_monitor.h"
#include "task_app_wifi_manage.h"
#include "task_app_cmd.h"
#include "task_app_mqtt.h"
#include "task_app_sensor.h"
#include "task_app_btn.h"
#include "task_app_radar.h"

static const char *TAG = "APP_MAIN";

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_task_wdt_config_t twdt_config = {
        .timeout_ms = 15000,
        .idle_core_mask = 0,
        .trigger_panic = true,
    };
    ret = esp_task_wdt_init(&twdt_config);
    if (ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "Task watchdog already initialized, skipping init");
    } else {
        ESP_ERROR_CHECK(ret);
    }

    sensor_data_mgr_init();

    ret = bsp_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "BSP init failed (some sensors may not be available)");
    }

    task_monitor_start();

    app_wifi_init();
    task_app_wifi_manage_start();

    task_app_cmd_start();

    app_mqtt_init();
    task_app_mqtt_start();

    task_app_sensor_start();

    task_app_btn_start();

    ret = task_app_radar_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Radar init failed (LD2450 may not be connected)");
    }

    ESP_LOGI(TAG, "All tasks started, app_main returning...");
}
