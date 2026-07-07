#ifndef _10_LD2450_H_
#define _10_LD2450_H_

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/uart.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LD2450_UART_NUM        UART_NUM_1
#define LD2450_TX_PIN          4
#define LD2450_RX_PIN          5
#define LD2450_BAUD_RATE       9600
#define LD2450_BUF_SIZE        256

typedef struct {
    bool presence;
    uint16_t distance_cm;
    uint8_t energy_move;
    uint8_t energy_static;
    uint8_t light_status;
    uint32_t last_update_ms;
} ld2450_data_t;

esp_err_t ld2450_init(void);
esp_err_t ld2450_deinit(void);
esp_err_t ld2450_read(ld2450_data_t *data, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif
