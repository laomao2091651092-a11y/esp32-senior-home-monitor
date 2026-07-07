#include "beep.h"
#include "driver/gpio.h"

void beep_init(void)
{
    gpio_config_t gpio_structure = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT_OUTPUT,
        .pin_bit_mask = 1ull << GPIO_NUM_41,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&gpio_structure);

    gpio_set_level(GPIO_NUM_41,1);
}