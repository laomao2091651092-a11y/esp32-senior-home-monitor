#ifndef __S3_WIFI_H__
#define __S3_WIFI_H__

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// WiFi初始化
void s3_wifi_init(void);

// WiFi连接
void s3_wifi_connect(const char *ssid, const char *password);

// WiFi是否已连接
bool s3_wifi_is_connected(void);

#ifdef __cplusplus
}
#endif

#endif
