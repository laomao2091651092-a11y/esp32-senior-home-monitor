#ifndef __S3_MQTT_H__
#define __S3_MQTT_H__

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 传感器数据结构体
typedef struct {
    float temperature;          // 温度
    float humidity;             // 湿度
    float light;                // 光照
    int gas;                    // 气体
    bool presence;              // 人体存在
    uint16_t presence_distance; // 人体距离
    char sleep_state[16];       // 睡眠状态
    uint32_t sleep_duration_min;// 睡眠时长（分钟）
    uint8_t wakeup_count;       // 唤醒次数
    char sleep_quality[16];     // 睡眠质量
    bool valid;                 // 数据是否有效
} s3_sensor_data_t;

// MQTT初始化
void s3_mqtt_init(void);

// MQTT启动连接
void s3_mqtt_start(void);

// MQTT是否已连接
bool s3_mqtt_is_connected(void);

// 获取最新传感器数据
s3_sensor_data_t s3_mqtt_get_sensor_data(void);

// 发送命令
void s3_mqtt_send_cmd(const char *cmd);

// 发布告警
void s3_mqtt_publish_alarm(const char *type, const char *message);

#ifdef __cplusplus
}
#endif

#endif
