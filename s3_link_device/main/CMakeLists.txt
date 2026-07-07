# main 组件 CMakeLists.txt
idf_component_register(SRCS "app_main.c"
                       "s3_wifi.c"
                       "s3_mqtt.c"
                       "s3_display.c"
                       "s3_buttons.c"
                       INCLUDE_DIRS "."
                       REQUIRES nvs_flash
                                esp_wifi
                                esp_netif
                                mqtt
                                driver
                                esp_event
                                freertos
                                json
                                LED
                                BEEP
                                KEY
                                LCD)
