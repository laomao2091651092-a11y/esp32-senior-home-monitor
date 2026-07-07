#include <stdio.h>
#include <string.h>
#include "10_ld2450.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "esp_timer.h"

static const char *TAG = "LD2450";

#define LD2450_FRAME_HEADER1   0xAA
#define LD2450_FRAME_HEADER2   0xFF

static bool s_initialized = false;

esp_err_t ld2450_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "LD2450 already initialized");
        return ESP_OK;
    }

    uart_config_t uart_config = {
        .baud_rate = LD2450_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };

    esp_err_t ret = uart_driver_install(LD2450_UART_NUM, LD2450_BUF_SIZE * 2, 0, 0, NULL, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install UART driver: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = uart_param_config(LD2450_UART_NUM, &uart_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to config UART param: %s", esp_err_to_name(ret));
        uart_driver_delete(LD2450_UART_NUM);
        return ret;
    }

    ret = uart_set_pin(LD2450_UART_NUM, LD2450_TX_PIN, LD2450_RX_PIN,
                       UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set UART pin: %s", esp_err_to_name(ret));
        uart_driver_delete(LD2450_UART_NUM);
        return ret;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "LD2450 initialized on UART%d (TX=%d, RX=%d, baud=%d)",
             LD2450_UART_NUM, LD2450_TX_PIN, LD2450_RX_PIN, LD2450_BAUD_RATE);
    return ESP_OK;
}

esp_err_t ld2450_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    uart_driver_delete(LD2450_UART_NUM);
    s_initialized = false;
    ESP_LOGI(TAG, "LD2450 deinitialized");
    return ESP_OK;
}

static bool parse_ld2450_frame(uint8_t *buf, int len, ld2450_data_t *data)
{
    if (len < 10 || buf[0] != LD2450_FRAME_HEADER1 || buf[1] != LD2450_FRAME_HEADER2) {
        return false;
    }

    uint8_t frame_type = buf[2];
    uint8_t data_len = buf[3];

    if (len < data_len + 4) {
        return false;
    }

    if (frame_type == 0x03) {
        uint8_t num_targets = buf[4];

        data->presence = (num_targets > 0);
        data->distance_cm = 0;
        data->energy_move = 0;
        data->energy_static = 0;

        if (num_targets >= 1 && data_len >= 10) {
            uint16_t dist = buf[5] | (buf[6] << 8);
            data->distance_cm = dist;
            data->energy_move = buf[7];
            data->energy_static = buf[8];
        }

        data->last_update_ms = esp_timer_get_time() / 1000;
        return true;
    }

    return false;
}

esp_err_t ld2450_read(ld2450_data_t *data, uint32_t timeout_ms)
{
    if (!s_initialized || data == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t buf[LD2450_BUF_SIZE];
    int total_len = 0;
    uint32_t start_ms = esp_timer_get_time() / 1000;

    while (1) {
        uint32_t elapsed = (esp_timer_get_time() / 1000) - start_ms;
        if (elapsed >= timeout_ms) {
            return ESP_ERR_TIMEOUT;
        }

        int len = uart_read_bytes(LD2450_UART_NUM, buf + total_len,
                                  LD2450_BUF_SIZE - total_len,
                                  pdMS_TO_TICKS(50));
        if (len <= 0) {
            continue;
        }

        total_len += len;

        if (total_len < 4) {
            continue;
        }

        int header_idx = -1;
        for (int i = 0; i < total_len - 1; i++) {
            if (buf[i] == LD2450_FRAME_HEADER1 && buf[i + 1] == LD2450_FRAME_HEADER2) {
                header_idx = i;
                break;
            }
        }

        if (header_idx < 0) {
            total_len = 0;
            continue;
        }

        if (header_idx > 0) {
            memmove(buf, buf + header_idx, total_len - header_idx);
            total_len -= header_idx;
        }

        if (total_len >= 4) {
            uint8_t frame_data_len = buf[3];
            int expected_total = frame_data_len + 4;

            if (total_len >= expected_total) {
                if (parse_ld2450_frame(buf, expected_total, data)) {
                    return ESP_OK;
                }

                memmove(buf, buf + 1, total_len - 1);
                total_len -= 1;
            }
        }
    }
}
