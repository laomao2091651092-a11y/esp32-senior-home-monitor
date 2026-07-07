#ifndef _SENSOR_DATA_MGR_H
#define _SENSOR_DATA_MGR_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float temperature;
    uint32_t temperature_ts;
    bool temperature_valid;

    float humidity;
    uint32_t humidity_ts;
    bool humidity_valid;

    int gas;
    uint32_t gas_ts;
    bool gas_valid;

    float light;
    uint32_t light_ts;
    bool light_valid;

    bool presence;
    uint16_t presence_distance;
    uint8_t move_energy;
    uint8_t static_energy;
    uint32_t presence_ts;
    bool presence_valid;

    uint8_t sleep_state;
    uint32_t sleep_duration_min;
    uint8_t wakeup_count;
    uint8_t sleep_quality;
    uint32_t sleep_ts;
    bool sleep_valid;
} sensor_data_t;

void sensor_data_mgr_init(void);

void sensor_data_mgr_update_temperature(float temp);
void sensor_data_mgr_update_humidity(float humi);
void sensor_data_mgr_update_gas(int gas_value);
void sensor_data_mgr_update_light(float light_value);
void sensor_data_mgr_update_presence(bool presence, uint16_t distance,
                                     uint8_t move_energy, uint8_t static_energy);
void sensor_data_mgr_update_sleep(uint8_t state, uint32_t duration_min,
                                  uint8_t wakeup_count, uint8_t quality);

bool sensor_data_mgr_get_latest(sensor_data_t *out_data);
int  sensor_data_mgr_get_json(char *buf, int buf_len);
int  sensor_data_mgr_get_sleep_json(char *buf, int buf_len);

#ifdef __cplusplus
}
#endif

#endif
