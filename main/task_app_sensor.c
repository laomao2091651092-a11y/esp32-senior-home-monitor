#include <stdio.h>
#include <stdlib.h>
#include "task_app_sensor.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sensor_data_mgr.h"
#include "03_gy30.h"
#include "09_dht11.h"
#include "09_mics5524.h"
#include "task_app_mqtt.h"
#include "task_monitor.h"

static const char *TAG = "SENSOR_TASK";

static TaskHandle_t s_sensor_task_handle = NULL;
static uint32_t s_sim_counter = 0;

static float sim_temperature(void)
{
    return 25.0f + (float)(rand() % 100) / 10.0f;
}

static float sim_humidity(void)
{
    return 50.0f + (float)(rand() % 300) / 10.0f;
}

static float sim_light(void)
{
    return 100.0f + (float)(rand() % 5000) / 10.0f;
}

static int sim_gas(void)
{
    return rand() % 2;
}

static void task_app_sensor(void *pvParameter)
{
    (void)pvParameter;
    ESP_LOGI(TAG, "Sensor task started (simulation enabled for missing sensors)");

    srand((unsigned int)xTaskGetTickCount());

    while (1) {
        s_sim_counter++;

        float light = 0.0f;
        if (gy30_read_light(&light) == ESP_OK) {
            sensor_data_mgr_update_light(light);
            ESP_LOGD(TAG, "Light: %.2f lux (real)", light);
        } else {
            light = sim_light();
            sensor_data_mgr_update_light(light);
            ESP_LOGD(TAG, "Light: %.2f lux (sim)", light);
        }

        float temp = 0.0f, humi = 0.0f;
        if (dht11_read(&temp, &humi) == ESP_OK) {
            sensor_data_mgr_update_temperature(temp);
            sensor_data_mgr_update_humidity(humi);
            ESP_LOGD(TAG, "Temp: %.1f C, Humi: %.1f %% (real)", temp, humi);
        } else {
            temp = sim_temperature();
            humi = sim_humidity();
            sensor_data_mgr_update_temperature(temp);
            sensor_data_mgr_update_humidity(humi);
            ESP_LOGD(TAG, "Temp: %.1f C, Humi: %.1f %% (sim)", temp, humi);
        }

        int gas_value = 0;
        bool gas_detected = mics5524_gas_detected();
        if (gas_detected || true) {
            gas_value = sim_gas();
            sensor_data_mgr_update_gas(gas_value);
        }

        ESP_LOGI(TAG, "Sensor data #%lu: temp=%.1fC humi=%.1f%% light=%.1flux gas=%d",
                 (unsigned long)s_sim_counter, temp, humi, light, gas_value);

        TaskHandle_t mqtt_handle = task_app_mqtt_get_handle();
        if (mqtt_handle != NULL) {
            xTaskNotify(mqtt_handle, 0, eNoAction);
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void task_app_sensor_start(void)
{
    BaseType_t ret = xTaskCreate(
        task_app_sensor,
        "task_sensor",
        4096,
        NULL,
        3,
        &s_sensor_task_handle
    );

    if (ret == pdPASS) {
        ESP_LOGI(TAG, "Sensor task created");
        task_monitor_register("sensor", s_sensor_task_handle, 4096);
    } else {
        ESP_LOGE(TAG, "Failed to create sensor task");
    }
}
