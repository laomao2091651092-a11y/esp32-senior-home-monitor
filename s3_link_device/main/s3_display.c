#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "s3_display.h"
#include "lcd.h"
#include "s3_wifi.h"
#include "s3_mqtt.h"

static const char *TAG = "s3_display";

// 显示任务句柄
static TaskHandle_t s_display_task_handle = NULL;

// 显示任务函数
static void s3_display_task(void *pvParameters)
{
    s3_sensor_data_t sensor_data;
    char buf[32];

    ESP_LOGI(TAG, "显示任务启动");

    while (1) {
        // 获取最新传感器数据
        sensor_data = s3_mqtt_get_sensor_data();

        // 清屏（黑色背景）
        lcd_clear(BLACK);

        // 第1行：标题
        lcd_show_string(1, 3, "Smart Home", WHITE, BLACK);

        // 第2行：WiFi状态
        if (s3_wifi_is_connected()) {
            lcd_show_string(2, 1, "WiFi:OK", GREEN, BLACK);
        } else {
            lcd_show_string(2, 1, "WiFi: --", RED, BLACK);
        }

        // 第3行：MQTT状态
        if (s3_mqtt_is_connected()) {
            lcd_show_string(3, 1, "MQTT:OK", GREEN, BLACK);
        } else {
            lcd_show_string(3, 1, "MQTT: --", RED, BLACK);
        }

        // 第4行：温度和湿度
        if (sensor_data.valid) {
            snprintf(buf, sizeof(buf), "T:%2.0f H:%2.0f%%", sensor_data.temperature, sensor_data.humidity);
            lcd_show_string(4, 1, buf, YELLOW, BLACK);
        } else {
            lcd_show_string(4, 1, "T:-- H:--%%", YELLOW, BLACK);
        }

        // 第5行：光照
        if (sensor_data.valid) {
            snprintf(buf, sizeof(buf), "LUX:%4.0f", sensor_data.light);
            lcd_show_string(5, 1, buf, CYAN, BLACK);
        } else {
            lcd_show_string(5, 1, "LUX:----", CYAN, BLACK);
        }

        // 第6行：人体存在
        if (sensor_data.valid) {
            if (sensor_data.presence) {
                lcd_show_string(6, 1, "Person:Yes", GREEN, BLACK);
            } else {
                lcd_show_string(6, 1, "Person: No", WHITE, BLACK);
            }
        } else {
            lcd_show_string(6, 1, "Person: --", WHITE, BLACK);
        }

        // 第7行：按键提示
        lcd_show_string(7, 1, "K1:ON K2:OFF", WHITE, BLACK);

        // 延时1秒
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// LCD显示初始化（只打印日志）
void s3_display_init(void)
{
    ESP_LOGI(TAG, "初始化LCD显示模块");
    ESP_LOGI(TAG, "LCD显示模块初始化完成");
}

// 启动LCD显示任务
void s3_display_start(void)
{
    ESP_LOGI(TAG, "启动LCD显示任务");

    if (s_display_task_handle == NULL) {
        xTaskCreate(s3_display_task, "s3_display", 4096, NULL, 3, &s_display_task_handle);
        ESP_LOGI(TAG, "LCD显示任务已创建");
    } else {
        ESP_LOGW(TAG, "LCD显示任务已存在");
    }
}
