#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "cJSON.h"

#include "s3_mqtt.h"

static const char *TAG = "s3_mqtt";

#define MQTT_TOPIC_PREFIX     "2023108380254_linzijie"
#define MQTT_BROKER_URL       "mqtt://broker.emqx.io:1883"

#define MQTT_TOPIC_EVT        MQTT_TOPIC_PREFIX "_evt"
#define MQTT_TOPIC_ALARM      MQTT_TOPIC_PREFIX "_alarm"
#define MQTT_TOPIC_CMD        MQTT_TOPIC_PREFIX "_cmd"

static EventGroupHandle_t s_mqtt_event_group;
#define MQTT_CONNECTED_BIT    BIT0

static esp_mqtt_client_handle_t s_mqtt_client = NULL;

static s3_sensor_data_t s_sensor_data;
static SemaphoreHandle_t s_data_mutex = NULL;

static void parse_sensor_json(const char *data, int len)
{
    cJSON *root = cJSON_ParseWithLength(data, len);
    if (root == NULL) {
        ESP_LOGW(TAG, "JSON parse failed");
        return;
    }

    s3_sensor_data_t new_data = {0};

    cJSON *temp = cJSON_GetObjectItem(root, "temperature");
    if (cJSON_IsNumber(temp)) {
        new_data.temperature = (float)temp->valuedouble;
    }

    cJSON *hum = cJSON_GetObjectItem(root, "humidity");
    if (cJSON_IsNumber(hum)) {
        new_data.humidity = (float)hum->valuedouble;
    }

    cJSON *light = cJSON_GetObjectItem(root, "light");
    if (cJSON_IsNumber(light)) {
        new_data.light = (float)light->valuedouble;
    }

    cJSON *gas = cJSON_GetObjectItem(root, "gas");
    if (cJSON_IsNumber(gas)) {
        new_data.gas = gas->valueint;
    }

    cJSON *presence = cJSON_GetObjectItem(root, "presence");
    if (cJSON_IsBool(presence)) {
        new_data.presence = cJSON_IsTrue(presence);
    }

    cJSON *presence_dist = cJSON_GetObjectItem(root, "presence_distance");
    if (cJSON_IsNumber(presence_dist)) {
        new_data.presence_distance = (uint16_t)presence_dist->valueint;
    }

    cJSON *sleep_state = cJSON_GetObjectItem(root, "sleep_state");
    if (cJSON_IsString(sleep_state) && sleep_state->valuestring != NULL) {
        strncpy(new_data.sleep_state, sleep_state->valuestring, sizeof(new_data.sleep_state) - 1);
    }

    cJSON *sleep_duration = cJSON_GetObjectItem(root, "sleep_duration_min");
    if (cJSON_IsNumber(sleep_duration)) {
        new_data.sleep_duration_min = (uint32_t)sleep_duration->valueint;
    }

    cJSON *wakeup_count = cJSON_GetObjectItem(root, "wakeup_count");
    if (cJSON_IsNumber(wakeup_count)) {
        new_data.wakeup_count = (uint8_t)wakeup_count->valueint;
    }

    cJSON *sleep_quality = cJSON_GetObjectItem(root, "sleep_quality");
    if (cJSON_IsString(sleep_quality) && sleep_quality->valuestring != NULL) {
        strncpy(new_data.sleep_quality, sleep_quality->valuestring, sizeof(new_data.sleep_quality) - 1);
    }

    new_data.valid = true;

    if (xSemaphoreTake(s_data_mutex, portMAX_DELAY) == pdTRUE) {
        memcpy(&s_sensor_data, &new_data, sizeof(s3_sensor_data_t));
        xSemaphoreGive(s_data_mutex);
    }

    ESP_LOGI(TAG, "Sensor data updated: T=%.1f H=%.1f L=%.1f P=%d",
             new_data.temperature, new_data.humidity, new_data.light, new_data.presence);

    cJSON_Delete(root);
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    (void)handler_args;
    (void)base;
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT connected");
            xEventGroupSetBits(s_mqtt_event_group, MQTT_CONNECTED_BIT);
            esp_mqtt_client_subscribe(client, MQTT_TOPIC_EVT, 1);
            esp_mqtt_client_subscribe(client, MQTT_TOPIC_ALARM, 1);
            ESP_LOGI(TAG, "Subscribed to %s, %s", MQTT_TOPIC_EVT, MQTT_TOPIC_ALARM);
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT disconnected");
            xEventGroupClearBits(s_mqtt_event_group, MQTT_CONNECTED_BIT);
            break;

        case MQTT_EVENT_DATA: {
            char topic[64];
            int topic_len = event->topic_len < 63 ? event->topic_len : 63;
            strncpy(topic, event->topic, topic_len);
            topic[topic_len] = '\0';

            if (strstr(topic, "_evt") != NULL) {
                parse_sensor_json(event->data, event->data_len);
            } else if (strstr(topic, "_alarm") != NULL) {
                ESP_LOGW(TAG, "Alarm received: %.*s",
                         event->data_len < 100 ? event->data_len : 100,
                         (char*)event->data);
            }
            break;
        }

        default:
            break;
    }
}

void s3_mqtt_init(void)
{
    ESP_LOGI(TAG, "MQTT init");

    s_mqtt_event_group = xEventGroupCreate();
    s_data_mutex = xSemaphoreCreateMutex();

    memset(&s_sensor_data, 0, sizeof(s3_sensor_data_t));
    s_sensor_data.valid = false;

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URL,
        .session.keepalive = 30,
        .network.reconnect_timeout_ms = 2000,
    };

    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (s_mqtt_client == NULL) {
        ESP_LOGE(TAG, "MQTT client init failed");
        return;
    }

    esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);

    ESP_LOGI(TAG, "MQTT init done");
}

void s3_mqtt_start(void)
{
    if (s_mqtt_client == NULL) {
        ESP_LOGE(TAG, "MQTT not initialized");
        return;
    }

    ESP_LOGI(TAG, "Starting MQTT: %s", MQTT_BROKER_URL);
    esp_mqtt_client_start(s_mqtt_client);
}

bool s3_mqtt_is_connected(void)
{
    EventBits_t bits = xEventGroupGetBits(s_mqtt_event_group);
    return (bits & MQTT_CONNECTED_BIT) ? true : false;
}

s3_sensor_data_t s3_mqtt_get_sensor_data(void)
{
    s3_sensor_data_t data;

    if (xSemaphoreTake(s_data_mutex, portMAX_DELAY) == pdTRUE) {
        memcpy(&data, &s_sensor_data, sizeof(s3_sensor_data_t));
        xSemaphoreGive(s_data_mutex);
    } else {
        memset(&data, 0, sizeof(s3_sensor_data_t));
        data.valid = false;
    }

    return data;
}

void s3_mqtt_send_cmd(const char *cmd)
{
    if (s_mqtt_client == NULL || cmd == NULL) {
        return;
    }

    if (!s3_mqtt_is_connected()) {
        ESP_LOGW(TAG, "MQTT not connected, cannot send cmd");
        return;
    }

    int msg_id = esp_mqtt_client_publish(s_mqtt_client, MQTT_TOPIC_CMD,
                                          cmd, strlen(cmd), 1, 0);
    ESP_LOGI(TAG, "Sent cmd: %s (msg_id=%d)", cmd, msg_id);
}

void s3_mqtt_publish_alarm(const char *type, const char *message)
{
    if (s_mqtt_client == NULL || type == NULL) {
        return;
    }

    if (!s3_mqtt_is_connected()) {
        return;
    }

    char buf[256];
    int len = snprintf(buf, sizeof(buf),
        "{\"alarm_type\":\"%s\",\"message\":\"%s\",\"device\":\"S3_Link\"}",
        type, message ? message : "");

    esp_mqtt_client_publish(s_mqtt_client, MQTT_TOPIC_ALARM,
                            buf, len, 1, 0);
    ESP_LOGW(TAG, "Alarm published: type=%s", type);
}
