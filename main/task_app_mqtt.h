#ifndef _TASK_APP_MQTT_H
#define _TASK_APP_MQTT_H

#include <stdbool.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MQTT_BROKER_URL          "mqtt://broker.emqx.io:1883"
#define MQTT_DEVICE_ID           "ESP32_SeniorHome_001"
#define MQTT_STUDENT_PREFIX      "2023108380254_linzijie"

#define MQTT_TOPIC_EVT           MQTT_STUDENT_PREFIX "_evt"
#define MQTT_TOPIC_CMD           MQTT_STUDENT_PREFIX "_cmd"
#define MQTT_TOPIC_STATUS        MQTT_STUDENT_PREFIX "_status"
#define MQTT_TOPIC_SLEEP         MQTT_STUDENT_PREFIX "_sleep"
#define MQTT_TOPIC_ALARM         MQTT_STUDENT_PREFIX "_alarm"

#define JSON_BUF_SIZE            512

void app_mqtt_init(void);
void task_app_mqtt_start(void);
bool app_mqtt_is_connected(void);
esp_err_t app_mqtt_publish_data(const char *data, int len);
TaskHandle_t task_app_mqtt_get_handle(void);
void app_mqtt_publish_alarm(const char *type, const char *message);

#ifdef __cplusplus
}
#endif

#endif
