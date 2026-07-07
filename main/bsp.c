#include <stdio.h>
#include "bsp.h"
#include "esp_log.h"
#include "esp_err.h"
#include "02_led.h"
#include "03_gy30.h"
#include "09_dht11.h"
#include "09_mics5524.h"

static const char *TAG = "BSP";

esp_err_t bsp_init(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "Initializing board support package...");

    ret = led_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LED init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    led_set_color(0, 255, 0);

    ret = gy30_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "GY-30 init failed: %s", esp_err_to_name(ret));
    }

    ret = dht11_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "DHT11 init failed: %s", esp_err_to_name(ret));
    }

    ret = mics5524_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "MICS5524 init failed: %s", esp_err_to_name(ret));
    }

    ESP_LOGI(TAG, "BSP initialization complete");
    return ESP_OK;
}
