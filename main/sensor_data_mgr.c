#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include "sensor_data_mgr.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "SENSOR_MGR";

#define DATA_FRESHNESS_MS  30000

static sensor_data_t s_sensor_data;
static SemaphoreHandle_t s_data_mutex = NULL;

static void lock_data(void)
{
    if (s_data_mutex) {
        xSemaphoreTake(s_data_mutex, portMAX_DELAY);
    }
}

static void unlock_data(void)
{
    if (s_data_mutex) {
        xSemaphoreGive(s_data_mutex);
    }
}

static uint32_t get_timestamp_ms(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

void sensor_data_mgr_init(void)
{
    memset(&s_sensor_data, 0, sizeof(s_sensor_data));
    s_data_mutex = xSemaphoreCreateMutex();
    ESP_LOGI(TAG, "Sensor data manager initialized");
}

void sensor_data_mgr_update_temperature(float temp)
{
    lock_data();
    s_sensor_data.temperature = temp;
    s_sensor_data.temperature_valid = true;
    s_sensor_data.temperature_ts = get_timestamp_ms();
    unlock_data();
}

void sensor_data_mgr_update_humidity(float humi)
{
    lock_data();
    s_sensor_data.humidity = humi;
    s_sensor_data.humidity_valid = true;
    s_sensor_data.humidity_ts = get_timestamp_ms();
    unlock_data();
}

void sensor_data_mgr_update_gas(int gas_value)
{
    lock_data();
    s_sensor_data.gas = gas_value;
    s_sensor_data.gas_valid = true;
    s_sensor_data.gas_ts = get_timestamp_ms();
    unlock_data();
}

void sensor_data_mgr_update_light(float light_value)
{
    lock_data();
    s_sensor_data.light = light_value;
    s_sensor_data.light_valid = true;
    s_sensor_data.light_ts = get_timestamp_ms();
    unlock_data();
}

void sensor_data_mgr_update_presence(bool presence, uint16_t distance,
                                     uint8_t move_energy, uint8_t static_energy)
{
    lock_data();
    s_sensor_data.presence = presence;
    s_sensor_data.presence_distance = distance;
    s_sensor_data.move_energy = move_energy;
    s_sensor_data.static_energy = static_energy;
    s_sensor_data.presence_valid = true;
    s_sensor_data.presence_ts = get_timestamp_ms();
    unlock_data();
}

void sensor_data_mgr_update_sleep(uint8_t state, uint32_t duration_min,
                                  uint8_t wakeup_count, uint8_t quality)
{
    lock_data();
    s_sensor_data.sleep_state = state;
    s_sensor_data.sleep_duration_min = duration_min;
    s_sensor_data.wakeup_count = wakeup_count;
    s_sensor_data.sleep_quality = quality;
    s_sensor_data.sleep_valid = true;
    s_sensor_data.sleep_ts = get_timestamp_ms();
    unlock_data();
}

bool sensor_data_mgr_get_latest(sensor_data_t *out_data)
{
    if (out_data == NULL) return false;

    lock_data();
    memcpy(out_data, &s_sensor_data, sizeof(sensor_data_t));
    unlock_data();

    return true;
}

int sensor_data_mgr_get_json(char *buf, int buf_len)
{
    if (buf == NULL || buf_len <= 0) return -1;

    sensor_data_t data;
    lock_data();
    memcpy(&data, &s_sensor_data, sizeof(sensor_data_t));
    unlock_data();

    uint32_t now = get_timestamp_ms();
    int pos = 0;
    bool first = true;

    buf[pos++] = '{';

    bool temp_fresh = data.temperature_valid &&
                      (now - data.temperature_ts < DATA_FRESHNESS_MS);
    if (temp_fresh) {
        int needed = snprintf(buf + pos, buf_len - pos,
                       "%s\"temperature\":%.2f,\"temperature_ts\":%" PRIu32,
                       first ? "" : ",",
                       data.temperature, data.temperature_ts);
        if (needed > 0 && needed < buf_len - pos) pos += needed;
        first = false;
    }

    bool humi_fresh = data.humidity_valid &&
                      (now - data.humidity_ts < DATA_FRESHNESS_MS);
    if (humi_fresh) {
        int needed = snprintf(buf + pos, buf_len - pos,
                       "%s\"humidity\":%.2f,\"humidity_ts\":%" PRIu32,
                       first ? "" : ",",
                       data.humidity, data.humidity_ts);
        if (needed > 0 && needed < buf_len - pos) pos += needed;
        first = false;
    }

    bool gas_fresh = data.gas_valid &&
                     (now - data.gas_ts < DATA_FRESHNESS_MS);
    if (gas_fresh) {
        int needed = snprintf(buf + pos, buf_len - pos,
                       "%s\"gas\":%d,\"gas_ts\":%" PRIu32,
                       first ? "" : ",",
                       data.gas, data.gas_ts);
        if (needed > 0 && needed < buf_len - pos) pos += needed;
        first = false;
    }

    bool light_fresh = data.light_valid &&
                       (now - data.light_ts < DATA_FRESHNESS_MS);
    if (light_fresh) {
        int needed = snprintf(buf + pos, buf_len - pos,
                       "%s\"light\":%.2f,\"light_ts\":%" PRIu32,
                       first ? "" : ",",
                       data.light, data.light_ts);
        if (needed > 0 && needed < buf_len - pos) pos += needed;
        first = false;
    }

    bool presence_fresh = data.presence_valid &&
                          (now - data.presence_ts < DATA_FRESHNESS_MS);
    if (presence_fresh) {
        int needed = snprintf(buf + pos, buf_len - pos,
                       "%s\"presence\":%s,\"presence_distance\":%u,"
                       "\"move_energy\":%u,\"static_energy\":%u,"
                       "\"presence_ts\":%" PRIu32,
                       first ? "" : ",",
                       data.presence ? "true" : "false",
                       (unsigned)data.presence_distance,
                       (unsigned)data.move_energy,
                       (unsigned)data.static_energy,
                       data.presence_ts);
        if (needed > 0 && needed < buf_len - pos) pos += needed;
        first = false;
    }

    bool sleep_fresh = data.sleep_valid &&
                       (now - data.sleep_ts < DATA_FRESHNESS_MS * 10);
    if (sleep_fresh) {
        const char *state_str = "awake";
        if (data.sleep_state == 1) state_str = "sleeping";
        else if (data.sleep_state == 2) state_str = "waking";
        const char *quality_str = "poor";
        if (data.sleep_quality >= 85) quality_str = "excellent";
        else if (data.sleep_quality >= 70) quality_str = "good";
        else if (data.sleep_quality >= 55) quality_str = "fair";
        int needed = snprintf(buf + pos, buf_len - pos,
                       "%s\"sleep_state\":\"%s\",\"sleep_duration_min\":%lu,"
                       "\"wakeup_count\":%u,\"sleep_quality\":\"%s\","
                       "\"sleep_quality_score\":%u",
                       first ? "" : ",",
                       state_str,
                       (unsigned long)data.sleep_duration_min,
                       (unsigned)data.wakeup_count,
                       quality_str,
                       (unsigned)data.sleep_quality);
        if (needed > 0 && needed < buf_len - pos) pos += needed;
        first = false;
    }

    if (pos >= buf_len - 2) pos = buf_len - 2;
    buf[pos++] = '}';
    buf[pos] = '\0';

    return pos;
}

int sensor_data_mgr_get_sleep_json(char *buf, int buf_len)
{
    if (buf == NULL || buf_len <= 0) return -1;

    sensor_data_t data;
    lock_data();
    memcpy(&data, &s_sensor_data, sizeof(sensor_data_t));
    unlock_data();

    const char *state_str = "awake";
    if (data.sleep_state == 1) state_str = "sleeping";
    else if (data.sleep_state == 2) state_str = "waking";

    const char *quality_str = "poor";
    if (data.sleep_quality >= 85) quality_str = "excellent";
    else if (data.sleep_quality >= 70) quality_str = "good";
    else if (data.sleep_quality >= 55) quality_str = "fair";

    int pos = snprintf(buf, buf_len,
        "{"
        "\"sleep_state\":\"%s\","
        "\"sleep_duration_min\":%lu,"
        "\"wakeup_count\":%u,"
        "\"sleep_quality\":\"%s\","
        "\"sleep_quality_score\":%u"
        "}",
        state_str,
        (unsigned long)data.sleep_duration_min,
        (unsigned)data.wakeup_count,
        quality_str,
        (unsigned)data.sleep_quality);

    return pos;
}
