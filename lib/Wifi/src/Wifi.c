/*
Copyright (c) 2021
*/

#include "Wifi_Config.h"
#include "Wifi.h"

#include "System.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_wifi_types.h"

static const char TAG[] = "WIFI";

static EventGroupHandle_t wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
// #define WIFI_FAIL_BIT      BIT1

static uint8_t WIFI_CurrentEventId;

static uint8_t WIFI_CurrentStatus;

uint8_t WIFI_GetCurrentEventId(void) {
    return WIFI_CurrentEventId;
}

uint8_t WIFI_GetStatus(void) {
    return WIFI_CurrentStatus;
}

void WIFI_SetStatus(uint8_t Status) {
    WIFI_CurrentStatus = Status;
}


static void WIFI_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
        case WIFI_EVENT_STA_START:
            WIFI_SetStatus(WIFI_StatusStart);
            ESP_LOGI(TAG, "WIFI_EVENT_STA_START");
            esp_wifi_connect();
            break;
        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG, "WIFI_EVENT_STA_CONNECTED");
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            WIFI_SetStatus(WIFI_StatusDisconnected);
            ESP_LOGI(TAG, "WIFI_EVENT_STA_DISCONNECTED");
            esp_wifi_connect();
            xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
            break;
        default:
            ESP_LOGI(TAG, "WIFI_EVENT_STA_DEFAULT %d", (int)event_id);
            break;
        }
    } else if (event_base == IP_EVENT) {
        switch (event_id) {
        case IP_EVENT_STA_GOT_IP:
            WIFI_SetStatus(WIFI_StatusConnected);
            ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
            ESP_LOGI(TAG, "IP_EVENT_STA_GOT_IP:" IPSTR, IP2STR(&event->ip_info.ip));
            xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
            break;
        default:
            ESP_LOGI(TAG, "IP_EVENT_STA_DEFAULT %d", (int)event_id);
            break;
        }
    }
}

void WIFI_Initialize(void)
{
    WIFI_SetStatus(WIFI_StatusStart);
    wifi_event_group = xEventGroupCreate();
    xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
   
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &WIFI_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &WIFI_event_handler, NULL));
    
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = MY_WIFI_SSID,
            .password = MY_WIFI_PASSWORD,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_LOGI(TAG, "start the WIFI SSID:[%s] password:[%s]", MY_WIFI_SSID, "******");
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "Waiting for wifi");
}
