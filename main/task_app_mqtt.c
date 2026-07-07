#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <time.h>
#include "task_app_mqtt.h"
#include "task_app_wifi_manage.h"
#include "task_app_cmd.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "mqtt_client.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sensor_data_mgr.h"
#include "task_monitor.h"

static const char *MQTT_TAG = "MQTT_MGR";

static esp_mqtt_client_handle_t s_mqtt_client = NULL;
static bool s_mqtt_connected = false;
static TaskHandle_t s_mqtt_task_handle = NULL;
static char s_client_id[64] = {0};
static char s_cached_json[JSON_BUF_SIZE] = {0};
static int s_cached_json_len = 0;

static uint32_t get_timestamp_s(void)
{
    time_t now;
    time(&now);
    return (uint32_t)now;
}

static int build_sensor_json(char *buf, int buf_len)
{
    sensor_data_t data;
    sensor_data_mgr_get_latest(&data);

    const char *status = "normal";
    if (data.temperature_valid && data.temperature > 35.0f) {
        status = "temp_high";
    } else if (data.humidity_valid && data.humidity < 30.0f) {
        status = "humi_low";
    }

    char sensor_json[JSON_BUF_SIZE];
    int sensor_len = sensor_data_mgr_get_json(sensor_json, sizeof(sensor_json));
    if (sensor_len <= 2) {
        return snprintf(buf, buf_len,
            "{\"device_id\":\"%s\",\"timestamp\":%" PRIu32 ",\"status\":\"%s\"}",
            MQTT_DEVICE_ID, get_timestamp_s(), status);
    }

    if (sensor_json[sensor_len - 1] == '}') {
        sensor_json[sensor_len - 1] = '\0';
        sensor_len--;
    }

    return snprintf(buf, buf_len,
        "{\"device_id\":\"%s\",\"timestamp\":%" PRIu32 ",%s,\"status\":\"%s\"}",
        MQTT_DEVICE_ID, get_timestamp_s(), sensor_json + 1, status);
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
            ESP_LOGI(MQTT_TAG, "MQTT connected (broker: %s, client_id: %s)",
                     MQTT_BROKER_URL, s_client_id);
            s_mqtt_connected = true;

            esp_mqtt_client_subscribe(client, MQTT_TOPIC_CMD, 1);

            char status_msg[128];
            int status_len = snprintf(status_msg, sizeof(status_msg),
                "{\"device_id\":\"%s\",\"status\":\"online\"}", MQTT_DEVICE_ID);
            esp_mqtt_client_publish(client, MQTT_TOPIC_STATUS,
                                    status_msg, status_len, 1, 1);

            if (s_cached_json_len > 0) {
                ESP_LOGI(MQTT_TAG, "  re-publishing cached sensor data (len=%d)",
                         s_cached_json_len);
                esp_mqtt_client_publish(client, MQTT_TOPIC_EVT,
                                        s_cached_json, s_cached_json_len, 1, 0);
            }
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(MQTT_TAG, "MQTT disconnected — auto-reconnect in ~2s");
            s_mqtt_connected = false;
            break;

        case MQTT_EVENT_DATA: {
            ESP_LOGI(MQTT_TAG, "← DATA [%.*s] len=%d data='%.*s'",
                     event->topic_len, event->topic,
                     event->data_len,
                     (event->data_len > 64) ? 64 : event->data_len,
                     (char*)event->data);

            size_t cmd_topic_len = strlen(MQTT_TOPIC_CMD);
            if ((size_t)event->topic_len == cmd_topic_len &&
                strncmp(event->topic, MQTT_TOPIC_CMD, cmd_topic_len) == 0) {
                if (event->data_len >= 1) {
                    char first = ((char*)event->data)[0];
                    if (first == '1') {
                        app_send_command_nonblock("LED_ON", NULL);
                    } else if (first == '0') {
                        app_send_command_nonblock("LED_OFF", NULL);
                    } else if (first == 'T' || first == 't') {
                        app_send_command_nonblock("LED_TOGGLE", NULL);
                    } else if (first == 'A' || first == 'a') {
                        app_send_command_nonblock("AUTO_LIGHT_TOGGLE", NULL);
                    }
                }
            }
            break;
        }

        default:
            break;
    }
}

static void mqtt_app_start(void)
{
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    snprintf(s_client_id, sizeof(s_client_id),
             "%s_%02X%02X%02X",
             MQTT_DEVICE_ID, mac[3], mac[4], mac[5]);

    char lwt_msg[128];
    int lwt_len = snprintf(lwt_msg, sizeof(lwt_msg),
        "{\"device_id\":\"%s\",\"status\":\"offline\"}", MQTT_DEVICE_ID);

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URL,
        .session.keepalive = 30,
        .session.last_will = {
            .topic = MQTT_TOPIC_STATUS,
            .msg = lwt_msg,
            .msg_len = lwt_len,
            .qos = 1,
            .retain = 1,
        },
        .network.reconnect_timeout_ms = 2000,
        .credentials.client_id = s_client_id,
    };

    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID,
                                   mqtt_event_handler, NULL);
    esp_mqtt_client_start(s_mqtt_client);
}

static void task_app_mqtt(void *pvParameter)
{
    (void)pvParameter;
    ESP_LOGI(MQTT_TAG, "MQTT task started");

    bool connected = app_wifi_wait_for_connection(pdMS_TO_TICKS(60000));
    if (connected) {
        ESP_LOGI(MQTT_TAG, "WiFi ready, starting MQTT client...");
        mqtt_app_start();
    } else {
        ESP_LOGE(MQTT_TAG, "WiFi not ready within 60s, MQTT client NOT started");
    }

    char json_buf[JSON_BUF_SIZE];
    uint32_t pub_cycle = 0;
    esp_task_wdt_add(NULL);

    while (1) {
        BaseType_t got_notify = xTaskNotifyWait(0, 0, NULL, pdMS_TO_TICKS(5000));

        if (got_notify == pdPASS || pub_cycle % 12 == 0) {
            int json_len = build_sensor_json(json_buf, sizeof(json_buf));
            if (json_len > 0 && s_mqtt_connected) {
                app_mqtt_publish_data(json_buf, json_len);
                pub_cycle++;
                ESP_LOGI(MQTT_TAG, "Published sensor data (#%lu, len=%d)",
                         (unsigned long)pub_cycle, json_len);
            }
        }
        esp_task_wdt_reset();
    }
}

void app_mqtt_init(void)
{
    s_mqtt_connected = false;
    s_cached_json_len = 0;
    memset(s_cached_json, 0, sizeof(s_cached_json));
    ESP_LOGI(MQTT_TAG, "MQTT manager init");
}

void task_app_mqtt_start(void)
{
    BaseType_t ret = xTaskCreate(
        task_app_mqtt,
        "task_mqtt",
        8192,
        NULL,
        5,
        &s_mqtt_task_handle
    );

    if (ret == pdPASS) {
        ESP_LOGI(MQTT_TAG, "MQTT task created");
        task_monitor_register("mqtt", s_mqtt_task_handle, 8192);
    } else {
        ESP_LOGE(MQTT_TAG, "Failed to create MQTT task");
    }
}

bool app_mqtt_is_connected(void)
{
    return s_mqtt_connected;
}

esp_err_t app_mqtt_publish_data(const char *data, int len)
{
    if (data == NULL || len <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (len > JSON_BUF_SIZE) len = JSON_BUF_SIZE;
    memcpy(s_cached_json, data, len);
    s_cached_json_len = len;

    if (!s_mqtt_connected || s_mqtt_client == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    int msg_id = esp_mqtt_client_publish(s_mqtt_client,
                                          MQTT_TOPIC_EVT,
                                          data, len, 1, 0);
    if (msg_id < 0) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

void app_mqtt_publish_alarm(const char *type, const char *message)
{
    if (!s_mqtt_connected || s_mqtt_client == NULL || type == NULL) {
        return;
    }

    char alarm_buf[256];
    int len = snprintf(alarm_buf, sizeof(alarm_buf),
        "{\"device_id\":\"%s\",\"timestamp\":%" PRIu32 ",\"alarm_type\":\"%s\",\"message\":\"%s\"}",
        MQTT_DEVICE_ID, get_timestamp_s(),
        type, message ? message : "");

    esp_mqtt_client_publish(s_mqtt_client, MQTT_TOPIC_ALARM,
                            alarm_buf, len, 1, 0);
    ESP_LOGW(MQTT_TAG, "ALARM published: type=%s, msg=%s", type, message ? message : "");
}

TaskHandle_t task_app_mqtt_get_handle(void)
{
    return s_mqtt_task_handle;
}
