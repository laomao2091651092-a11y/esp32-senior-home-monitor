#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"

#include "s3_buttons.h"
#include "key.h"
#include "led.h"
#include "beep.h"
#include "s3_mqtt.h"

static const char *TAG = "s3_buttons";

// 按键任务句柄
static TaskHandle_t s_buttons_task_handle = NULL;

// 蜂鸣器GPIO引脚
#define BEEP_GPIO_PIN GPIO_NUM_41

// 蜂鸣器短响时长（毫秒）
#define BEEP_SHORT_DURATION_MS 100

// 蜂鸣器长响时长（毫秒）
#define BEEP_LONG_DURATION_MS 500

// 按键GPIO定义
#define KEY1_GPIO GPIO_NUM_9
#define KEY2_GPIO GPIO_NUM_10

// 长按时间阈值（毫秒）
#define LONG_PRESS_MS 1000

// 防抖时间（毫秒）
#define DEBOUNCE_MS 20

// 蜂鸣器短响一声
static void beep_short(void)
{
    gpio_set_level(BEEP_GPIO_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(BEEP_SHORT_DURATION_MS));
    gpio_set_level(BEEP_GPIO_PIN, 1);
}

// 蜂鸣器长响
static void beep_long(void)
{
    gpio_set_level(BEEP_GPIO_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(BEEP_LONG_DURATION_MS));
    gpio_set_level(BEEP_GPIO_PIN, 1);
}

// KEY1短按：发送LED开命令(1)，蜂鸣器短响
static void handle_key1_short(void)
{
    ESP_LOGI(TAG, "KEY1短按: LED ON");
    s3_mqtt_send_cmd("1");
    beep_short();
}

// KEY1长按：一键呼救，发布emergency告警，蜂鸣器长响
static void handle_key1_long(void)
{
    ESP_LOGW(TAG, "KEY1长按: 一键呼救！");
    s3_mqtt_publish_alarm("emergency", "SOS from S3");
    beep_long();
}

// KEY2短按：发送LED关命令(0)，蜂鸣器短响
static void handle_key2_short(void)
{
    ESP_LOGI(TAG, "KEY2短按: LED OFF");
    s3_mqtt_send_cmd("0");
    beep_short();
}

// KEY2长按：切换自动照明，蜂鸣器短响
static void handle_key2_long(void)
{
    ESP_LOGI(TAG, "KEY2长按: 切换自动照明");
    s3_mqtt_send_cmd("A");
    beep_short();
}

// 按键状态枚举
typedef enum {
    KEY_STATE_IDLE = 0,
    KEY_STATE_PRESSED,
    KEY_STATE_WAIT_RELEASE
} key_state_t;

// 单个按键的数据结构
typedef struct {
    gpio_num_t gpio;
    key_state_t state;
    uint32_t press_time;
    bool long_pressed;
} key_data_t;

// 按键处理任务函数
static void s3_buttons_task(void *pvParameters)
{
    key_data_t keys[2] = {
        {.gpio = KEY1_GPIO, .state = KEY_STATE_IDLE, .press_time = 0, .long_pressed = false},
        {.gpio = KEY2_GPIO, .state = KEY_STATE_IDLE, .press_time = 0, .long_pressed = false}
    };

    ESP_LOGI(TAG, "按键处理任务启动（支持短按+长按）");

    while (1) {
        for (int i = 0; i < 2; i++) {
            int level = gpio_get_level(keys[i].gpio);

            switch (keys[i].state) {
                case KEY_STATE_IDLE:
                    // 检测到按键按下（低电平）
                    if (level == 0) {
                        vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_MS));
                        if (gpio_get_level(keys[i].gpio) == 0) {
                            keys[i].state = KEY_STATE_PRESSED;
                            keys[i].press_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
                            keys[i].long_pressed = false;
                            ESP_LOGI(TAG, "KEY%d 按下", i + 1);
                        }
                    }
                    break;

                case KEY_STATE_PRESSED: {
                    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
                    uint32_t duration = now - keys[i].press_time;

                    // 检测长按
                    if (!keys[i].long_pressed && duration >= LONG_PRESS_MS) {
                        keys[i].long_pressed = true;
                        ESP_LOGI(TAG, "KEY%d 长按触发", i + 1);
                        if (i == 0) {
                            handle_key1_long();
                        } else {
                            handle_key2_long();
                        }
                        keys[i].state = KEY_STATE_WAIT_RELEASE;
                    }

                    // 检测是否松开（短按）
                    if (level == 1) {
                        vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_MS));
                        if (gpio_get_level(keys[i].gpio) == 1) {
                            if (!keys[i].long_pressed) {
                                ESP_LOGI(TAG, "KEY%d 短按触发", i + 1);
                                if (i == 0) {
                                    handle_key1_short();
                                } else {
                                    handle_key2_short();
                                }
                            }
                            keys[i].state = KEY_STATE_IDLE;
                        }
                    }
                    break;
                }

                case KEY_STATE_WAIT_RELEASE:
                    // 长按触发后，等待松开
                    if (level == 1) {
                        vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_MS));
                        if (gpio_get_level(keys[i].gpio) == 1) {
                            keys[i].state = KEY_STATE_IDLE;
                            ESP_LOGI(TAG, "KEY%d 松开", i + 1);
                        }
                    }
                    break;

                default:
                    keys[i].state = KEY_STATE_IDLE;
                    break;
            }
        }

        // 每20ms扫描一次
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// 按键处理初始化
void s3_buttons_init(void)
{
    ESP_LOGI(TAG, "Buttons module init");
    ESP_LOGI(TAG, "Buttons module init done");
}

// 启动按键处理任务
void s3_buttons_start(void)
{
    ESP_LOGI(TAG, "启动按键处理任务");

    if (s_buttons_task_handle == NULL) {
        xTaskCreate(s3_buttons_task, "s3_buttons", 4096, NULL, 5, &s_buttons_task_handle);
        ESP_LOGI(TAG, "按键处理任务已创建");
    } else {
        ESP_LOGW(TAG, "按键处理任务已存在");
    }
}
