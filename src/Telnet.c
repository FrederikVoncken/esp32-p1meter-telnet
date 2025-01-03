/*
 * Telnet.c
 */

#include "Telnet.h"

#include "SmartMeter.h"

#include "System.h"
#include "nvs_flash.h"
#include "esp_err.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/portmacro.h"
#include "xtensa_api.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/time.h>

#include "esp_netif.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

static const char TAG[] = "TELNET";

#define TELNET_TASK_PRIORITY    5

// https://github.com/espressif/esp-idf/blob/master/examples/protocols/sockets/tcp_server/main/tcp_server.c
// https://github.com/espressif/esp-idf/blob/master/examples/protocols/sockets/tcp_server/main/Kconfig.projbuild
// https://esp32.com/viewtopic.php?t=911

typedef struct __attribute__((__packed__)) {
    int listen_sock;
    int sock;
    struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
    int port;
} telnet_t;

static void Telnet_tcp_server_task(void *pvParameters);
static void Telnet_tcp_client_task(void *pvParameters);

void Telnet_Initialize(void) {
    xTaskCreate(Telnet_tcp_server_task, "Telnet_tcp_server_task", 2048, NULL, TELNET_TASK_PRIORITY, NULL);
}

#define IP_ADDRESS_MAX_SIZE 128
//#define TELNET_RECEIVE_TIMEOUT_TIME_MS 60000 // 60 seconds no activity will auto close the connection

static void Telnet_tcp_client_task(void *pvParameters)
{
    telnet_t telnet;
    memcpy(&telnet, pvParameters, sizeof(telnet));
    // Use malloc to keep used stack(depth) low, will otherwise cause stackoverflow panic
    uint8_t *Telnet_Telegram = (uint8_t *)malloc(_SMARTMETER_TELEGRAM_MAX_SIZE);
    int16_t Telnet_TelegramSize = 0;
    uint8_t Telnet_CurrentTelegramTag = 0;
#if defined(TELNET_RECEIVE_TIMEOUT_TIME_MS)
    TickType_t StartTime;
    TickType_t CurrentTime;
#endif

    // Convert ip address to string
    // Use malloc to keep used stack(depth) low, will otherwise cause stackoverflow panic
    char *addr_str = (char *)malloc(IP_ADDRESS_MAX_SIZE);
    if (telnet.source_addr.ss_family == PF_INET) {
        inet_ntoa_r(((struct sockaddr_in *)&telnet.source_addr)->sin_addr, addr_str, (IP_ADDRESS_MAX_SIZE - 1));
    }
    ESP_LOGI(TAG, "Socket %i accepted ip address: %s", telnet.sock, addr_str);
    if (addr_str) {
        free(addr_str);
    }

#if defined(TELNET_RECEIVE_TIMEOUT_TIME_MS)
    StartTime = pdTICKS_TO_MS( xTaskGetTickCount() );
#endif

    do {
        Telnet_TelegramSize = SmartMeter_CheckCopyTelegram(&Telnet_CurrentTelegramTag, Telnet_Telegram, _SMARTMETER_TELEGRAM_MAX_SIZE);
        if (Telnet_TelegramSize > 0) {
#if defined(TELNET_RECEIVE_TIMEOUT_TIME_MS)
            StartTime = pdTICKS_TO_MS( xTaskGetTickCount() );
#endif
            int16_t to_write = Telnet_TelegramSize;
            while (to_write > 0) {
                ssize_t written = send(telnet.sock, Telnet_Telegram + (Telnet_TelegramSize - to_write), to_write, 0);
                if (written < 0) {
                    ESP_LOGW(TAG, "Error occurred during sending: errno %d", errno);
                    shutdown(telnet.sock, 0);
                    close(telnet.sock);
                    ESP_LOGW(TAG, "Socket %i closed", telnet.sock);
                    if (Telnet_Telegram) {
                        free(Telnet_Telegram);
                    }
                    vTaskDelete(NULL);
                    return;
                }
                to_write -= written;
            }
        } else {
#if defined(TELNET_RECEIVE_TIMEOUT_TIME_MS)
            CurrentTime = pdTICKS_TO_MS( xTaskGetTickCount() );
            if ((CurrentTime - StartTime) > TELNET_RECEIVE_TIMEOUT_TIME_MS) {
                shutdown(telnet.sock, 0);
                close(telnet.sock);
                ESP_LOGW(TAG, "Timeout %lu ms, Socket %i closed", (CurrentTime - StartTime), telnet.sock);
                if (Telnet_Telegram) {
                    free(Telnet_Telegram);
                }
                vTaskDelete(NULL);
                return;
            }
#endif
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    } while (1);
}

static void Telnet_tcp_server_task(void *pvParameters)
{
    int addr_family = AF_INET;
    int ip_protocol = 0;
    int keepAlive = 1;
    int keepIdle = TELNET_KEEPALIVE_IDLE;
    int keepInterval = TELNET_KEEPALIVE_INTERVAL;
    int keepCount = TELNET_KEEPALIVE_COUNT;

    telnet_t telnet;

    struct sockaddr_storage dest_addr;
    struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
    dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr_ip4->sin_family = AF_INET;
    dest_addr_ip4->sin_port = htons(TELNET_PORT);
    ip_protocol = IPPROTO_IP;
    telnet.port = TELNET_PORT;
    
    telnet.listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
    if (telnet.listen_sock < 0) {
        ESP_LOGE(TAG, "Unable to create listen socket %i: errno %d", telnet.listen_sock, errno);
        vTaskDelete(NULL);
        return;
    }
    
    int opt = 1;
    setsockopt(telnet.listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
   
    ESP_LOGI(TAG, "Listen socket %i created", telnet.listen_sock);

    int err = bind(telnet.listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0) {
        ESP_LOGE(TAG, "Listen socket %i unable to bind: errno %d", telnet.listen_sock, errno);
        ESP_LOGE(TAG, "IPPROTO: %d", addr_family);
        close(telnet.listen_sock);
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "Listen socket %i bound, port %d", telnet.listen_sock, TELNET_PORT);

    err = listen(telnet.listen_sock, 1);
    if (err != 0) {
        ESP_LOGE(TAG, "Error occurred during listen socket %i: errno %d", telnet.listen_sock, errno);
        close(telnet.listen_sock);
        vTaskDelete(NULL);
        return;
    }

    while (1) {
        ESP_LOGI(TAG, "Socket listening for new connection");
        socklen_t addr_len = sizeof(telnet.source_addr);
        telnet.sock = accept(telnet.listen_sock, (struct sockaddr *)&telnet.source_addr, &addr_len);
        
        if (telnet.sock < 0) {
            ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
            break;
        }

        // Set tcp keepalive option
        setsockopt(telnet.sock, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(int));
        setsockopt(telnet.sock, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(int));
        setsockopt(telnet.sock, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(int));
        setsockopt(telnet.sock, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(int));
                
        // Client task(s) need to have a higher priority than the Server task, because the server blocks while listening
        xTaskCreate(Telnet_tcp_client_task, "Telnet_tcp_client_task", 2048, (void *)&telnet, TELNET_TASK_PRIORITY+1, NULL);
        
        vTaskDelay(pdMS_TO_TICKS(20));
    }  
}
