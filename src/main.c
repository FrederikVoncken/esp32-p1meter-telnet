#include "System.h"
#include "nvs_flash.h"
#include "esp_err.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"

#include "Wifi.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/portmacro.h"
#include "xtensa_api.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "SmartMeter.h"
#include "Telnet.h"

#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/time.h>

static const char TAG[] = "MAIN";

esp_err_t event_handler(void *ctx, uint8_t *event) {
    return ESP_OK;
}

/**
 * @brief RTOS task that periodically prints the heap memory available.
 * @note Pure debug information, should not be ever started on production code! This is an example on how you can integrate your code with wifi-manager
 */

void SystemMonitoring_Task(void *pvParameter)
{
	for(;;){
		ESP_LOGI(TAG, "free heap: %d", (int)esp_get_free_heap_size());
		vTaskDelay( pdMS_TO_TICKS(10000) );
	}
}

void app_main(void) {
	ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", (int)esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_INFO);
	esp_log_level_set("TRANS_TCP", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_TCP", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_SSL", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);

	xTaskCreatePinnedToCore(&SystemMonitoring_Task, "SystemMonitoring_Task", 2048, NULL, 1, NULL, 1);

    nvs_flash_init();
	
	WIFI_Initialize();
           
    SmartMeter_Initialize();

    Telnet_Initialize();
	
    vTaskDelay( pdMS_TO_TICKS(2000) );

	ESP_LOGI(TAG, "Leaving app_main");	
}
