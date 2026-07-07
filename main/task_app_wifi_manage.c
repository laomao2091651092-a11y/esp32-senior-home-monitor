#include <string.h>
#include "task_app_wifi_manage.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_task_wdt.h"
#include "freertos/event_groups.h"
#include "task_monitor.h"

static const char *WIFI_TAG = "WIFI_MGR";

#define WIFI_SSID            "魅族"
#define WIFI_PASSWORD        "@@@@@@@@"

#define WIFI_BACKOFF_BASE_MS   500
#define WIFI_BACKOFF_MAX_MS    30000
#define WIFI_BACKOFF_MAX_SHIFT 5

#define WIFI_CONNECTED_BIT     BIT0
#define WIFI_FAIL_BIT          BIT1

static EventGroupHandle_t s_wifi_event_group = NULL;
static wifi_state_t s_wifi_state = WIFI_STATE_DISCONNECTED;
static int s_reconnect_attempt = 0;
static TaskHandle_t s_wifi_task_handle = NULL;
static esp_netif_t *s_sta_netif = NULL;

static void set_wifi_state(wifi_state_t new_state)
{
    s_wifi_state = new_state;
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        s_reconnect_attempt = 0;
        set_wifi_state(WIFI_STATE_CONNECTING);
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_reconnect_attempt++;
        int shift = (s_reconnect_attempt < WIFI_BACKOFF_MAX_SHIFT) ?
                     s_reconnect_attempt : WIFI_BACKOFF_MAX_SHIFT;
        int backoff_ms = WIFI_BACKOFF_BASE_MS * (1 << shift);
        if (backoff_ms > WIFI_BACKOFF_MAX_MS) backoff_ms = WIFI_BACKOFF_MAX_MS;

        ESP_LOGW(WIFI_TAG, "WiFi disconnected (attempt #%d), backoff %dms...",
                 s_reconnect_attempt, backoff_ms);
        set_wifi_state(WIFI_STATE_DISCONNECTED);
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        vTaskDelay(pdMS_TO_TICKS(backoff_ms));
        set_wifi_state(WIFI_STATE_CONNECTING);
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        s_reconnect_attempt = 0;
        ESP_LOGI(WIFI_TAG, "WiFi connected, IP: " IPSTR,
                 IP2STR(&event->ip_info.ip));
        set_wifi_state(WIFI_STATE_CONNECTED);
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    s_sta_netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_register(WIFI_EVENT,
                                        ESP_EVENT_ANY_ID,
                                        &wifi_event_handler,
                                        NULL,
                                        NULL);
    esp_event_handler_instance_register(IP_EVENT,
                                        IP_EVENT_STA_GOT_IP,
                                        &wifi_event_handler,
                                        NULL,
                                        NULL);

    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    memcpy(wifi_config.sta.ssid, WIFI_SSID, strlen(WIFI_SSID));
    memcpy(wifi_config.sta.password, WIFI_PASSWORD, strlen(WIFI_PASSWORD));

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();

    ESP_LOGI(WIFI_TAG, "WiFi station initialized, connecting to %s...", WIFI_SSID);
}

static void task_app_wifi_manage(void *pvParameter)
{
    (void)pvParameter;
    ESP_LOGI(WIFI_TAG, "WiFi management task started");

    wifi_init_sta();
    esp_task_wdt_add(NULL);

    while (1) {
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_wifi_init(void)
{
    s_wifi_state = WIFI_STATE_DISCONNECTED;
    s_reconnect_attempt = 0;
    ESP_LOGI(WIFI_TAG, "WiFi manager init");
}

void task_app_wifi_manage_start(void)
{
    BaseType_t ret = xTaskCreate(
        task_app_wifi_manage,
        "task_wifi_manage",
        8192,
        NULL,
        5,
        &s_wifi_task_handle
    );

    if (ret == pdPASS) {
        ESP_LOGI(WIFI_TAG, "WiFi management task created");
        task_monitor_register("wifi", s_wifi_task_handle, 8192);
    } else {
        ESP_LOGE(WIFI_TAG, "Failed to create WiFi management task");
    }
}

wifi_state_t app_get_wifi_state(void)
{
    return s_wifi_state;
}

bool app_wifi_is_connected(void)
{
    return s_wifi_state == WIFI_STATE_CONNECTED;
}

bool app_wifi_wait_for_connection(TickType_t timeout_ticks)
{
    if (s_wifi_event_group == NULL) return false;

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT,
                                           pdFALSE, pdTRUE,
                                           timeout_ticks);
    return (bits & WIFI_CONNECTED_BIT) != 0;
}
