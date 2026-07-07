/**
 * @file app_main.c
 * @brief S3 Link Device 主入口文件
 *
 * 初始化顺序：
 *   1. NVS Flash 存储
 *   2. 网络接口
 *   3. 事件循环
 *   4. 硬件外设（LCD / LED / BEEP / KEY）
 *   5. WiFi 连接
 *   6. MQTT 客户端
 *   7. 显示任务和按键任务
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "driver/gpio.h"

#include "lcd.h"
#include "led.h"
#include "beep.h"
#include "key.h"

#include "s3_wifi.h"
#include "s3_mqtt.h"
#include "s3_display.h"
#include "s3_buttons.h"

static const char *TAG = "app_main";

// WiFi配置
#define WIFI_SSID       "魅族"
#define WIFI_PASSWORD   "@@@@@@@@"

/**
 * @brief 硬件外设初始化
 *
 * 初始化 LCD、LED、BEEP、KEY 等硬件模块
 */
static void hardware_init(void)
{
    ESP_LOGI(TAG, "初始化硬件外设...");

    // 初始化 LCD 显示
    lcd_init();
    ESP_LOGI(TAG, "LCD 初始化完成");

    // 初始化 LED
    led_init();
    ESP_LOGI(TAG, "LED 初始化完成");

    // 初始化蜂鸣器 BEEP
    beep_init();
    ESP_LOGI(TAG, "BEEP 初始化完成");

    // 初始化按键 KEY
    key_init();
    ESP_LOGI(TAG, "KEY 初始化完成");

    // 清屏（白色背景）
    lcd_clear(WHITE);
    ESP_LOGI(TAG, "LCD 清屏完成");

    ESP_LOGI(TAG, "硬件外设初始化完成");
}

/**
 * @brief 应用主入口函数
 */
void app_main(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "========== S3 Link Device 启动 ==========");

    // 1. 初始化 NVS Flash
    ESP_LOGI(TAG, "初始化 NVS Flash...");
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "NVS Flash 初始化完成");

    // 2. 初始化网络接口
    ESP_LOGI(TAG, "初始化网络接口...");
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_LOGI(TAG, "网络接口初始化完成");

    // 3. 创建默认事件循环
    ESP_LOGI(TAG, "创建事件循环...");
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_LOGI(TAG, "事件循环创建完成");

    // 4. 硬件外设初始化（LCD / LED / BEEP / KEY）
    hardware_init();

    // 5. 初始化 WiFi 并连接
    ESP_LOGI(TAG, "初始化 WiFi...");
    s3_wifi_init();
    ESP_LOGI(TAG, "WiFi 初始化完成，开始连接...");
    s3_wifi_connect(WIFI_SSID, WIFI_PASSWORD);

    // 6. 初始化 MQTT 并启动
    ESP_LOGI(TAG, "初始化 MQTT...");
    s3_mqtt_init();
    ESP_LOGI(TAG, "MQTT 初始化完成，启动连接...");
    s3_mqtt_start();

    // 7. 初始化显示模块并启动显示任务
    s3_display_init();
    s3_display_start();

    // 8. 初始化按键模块并启动按键任务
    s3_buttons_init();
    s3_buttons_start();

    ESP_LOGI(TAG, "========== 系统初始化完成 ==========");

    // 主循环：每隔几秒打印一次状态
    while (1) {
        ESP_LOGI(TAG, "系统状态: WiFi=%s, MQTT=%s",
                 s3_wifi_is_connected() ? "已连接" : "未连接",
                 s3_mqtt_is_connected() ? "已连接" : "未连接");

        // 切换LED状态，指示系统运行中
        gpio_toggle(GPIO_NUM_38);

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
