/*
Copyright (c) 2019
*/

#include "SystemTool.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "esp_log.h"

#define MAX_ID_STRING (32)

static const char TAG[] = "SYSTEM_TOOL";

const char *SystemTool_IdString(void)
{
    uint8_t mac[6];
    char *id_string = calloc(1, MAX_ID_STRING);
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    sprintf(id_string, "ESP32_%02x%02X%02X%02x%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    ESP_LOGI(TAG, "ID string %s", id_string);
    return id_string;
}
