/*
 * SmartMeter.c
 */

#include "SmartMeter.h"

#include "esp_system.h"
#include "esp_log.h"

#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/portmacro.h"
#include "xtensa_api.h"

#include <stdio.h>
#include <string.h>
#include "driver/uart.h"
#include "driver/gpio.h"

#include "CRC16.h"

static const char TAG[] = "SMARTMETER";

#define SMARTMETER_TASK_PRIORITY    10

#define COM_STX   '/'
#define COM_ETX   '!'
#define COM_SPACE 32
#define COM_NULL  0

typedef enum
{
    COM_WAITING,
    COM_RECEIVING,
    COM_CHECCKSUM_RECEIVING,
} COM_State_t;

typedef struct __attribute__((__packed__)) {
    uint8_t Telegram[_SMARTMETER_TELEGRAM_MAX_SIZE];
    uint16_t TelegramSize;
    uint8_t TelegramTag;
    uint16_t TelegramCRC_Error;
} smartmeter_t;

static smartmeter_t SmartMeter;

static SemaphoreHandle_t SmartMeter_xMutex = NULL;

static void SmartMeter_UART_Initialize(void);
static void SmartMeter_RX_Task(void *pvParameters);

void SmartMeter_Initialize(void) {
    SmartMeter_xMutex = xSemaphoreCreateRecursiveMutex();
    SmartMeter_UART_Initialize();
    xTaskCreate(SmartMeter_RX_Task, "SmartMeter_Uart_RX_Task", 2048, NULL, SMARTMETER_TASK_PRIORITY, NULL);
}

static void SmartMeter_UART_Initialize(void) {
// Configure parameters of an UART driver, communication pins and install the driver
    uart_config_t uart_config = {
        .baud_rate = _SMARTMETER_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };

    int intr_alloc_flags = 0;
//#if CONFIG_UART_ISR_IN_IRAM
    intr_alloc_flags = ESP_INTR_FLAG_IRAM;
//#endif

    uart_driver_install(_SMARTMETER_UART_PORT_NUM, _SMARTMETER_UART_BUFFER_SIZE, _SMARTMETER_UART_BUFFER_SIZE, 0, NULL, intr_alloc_flags);
    uart_param_config(_SMARTMETER_UART_PORT_NUM, &uart_config);
    uart_set_pin(_SMARTMETER_UART_PORT_NUM, _SMARTMETER_UART_TXD, _SMARTMETER_UART_RXD, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_set_line_inverse(_SMARTMETER_UART_PORT_NUM, UART_SIGNAL_RXD_INV + UART_SIGNAL_TXD_INV);
}

static void SmartMeter_RX_Task(void *pvParameters)
{
    uint8_t c;
    COM_State_t State = 0;
    uint16_t RX_Index = 0;
    uint16_t RX_Checksum = 0;
    // Use malloc to keep used stack(depth) low, will otherwise cause stackoverflow panic
    uint8_t *RX_Data = (uint8_t *)malloc(_SMARTMETER_UART_BUFFER_SIZE);
    uint8_t *TelegramBuffer = (uint8_t *)malloc(_SMARTMETER_TELEGRAM_MAX_SIZE);
    uint8_t TelegramChecksumBuffer[5];
    uint16_t TelegramCRC_Calc = 0;
    uint16_t TelegramChecksum = 0;
    uint8_t TelegramCounter = 0;

    SmartMeter.TelegramSize = 0;
    SmartMeter.TelegramTag = 0;
    SmartMeter.TelegramCRC_Error = 0;

    while (1) {
        // Read data from the UART
        int len = uart_read_bytes(_SMARTMETER_UART_PORT_NUM, RX_Data, (_SMARTMETER_UART_BUFFER_SIZE - 1), pdMS_TO_TICKS(20));        
        if (len) {
            for(int cnt=0; cnt<len;cnt++) {
                c = RX_Data[cnt];
                if (c == COM_STX) {
                    if (State == COM_RECEIVING) {
                        ESP_LOGE(TAG, "Incomplete Telegram %u received", TelegramCounter);    
                    }
                    RX_Index = 0;
                    TelegramBuffer[RX_Index++] = c;
                    TelegramCRC_Calc = 0;
                    TelegramCRC_Calc = CRC16(TelegramCRC_Calc, c);
                    TelegramCounter++;
                    State = COM_RECEIVING;
                    //ESP_LOGI(TAG, "Telegram STX found");
                } else {
                    if (RX_Index < _SMARTMETER_TELEGRAM_MAX_SIZE) {
                        switch(State) {
                        default:
                        case COM_WAITING:
                            // Do nothing, wait for start, skip other characters
                            break;
                        case COM_RECEIVING:
                            if (c == COM_ETX) {
                                TelegramBuffer[RX_Index++] = c;
                                TelegramCRC_Calc = CRC16(TelegramCRC_Calc, c);
                                RX_Checksum = 0;
                                State = COM_CHECCKSUM_RECEIVING;
                                //ESP_LOGI(TAG, "Telegram ETX found");
                            } else {
                                TelegramBuffer[RX_Index++] = c;
                                TelegramCRC_Calc = CRC16(TelegramCRC_Calc, c);
                            }
                            break;
                        case COM_CHECCKSUM_RECEIVING:
                            if (RX_Checksum < 4) {
                                TelegramBuffer[RX_Index++] = c;
                                TelegramChecksumBuffer[RX_Checksum] = c;
                                RX_Checksum++;
                            } else {
                                TelegramBuffer[RX_Index++] = '\r';
                                TelegramBuffer[RX_Index++] = '\n';
                                TelegramChecksumBuffer[RX_Checksum] = '\0'; // Make a string from checksum
                                TelegramChecksum = strtoul((const char *)TelegramChecksumBuffer, 0, 16);
                                State = COM_WAITING;
                                //ESP_LOGI(TAG, "Recv: %s", (char *)TelegramBuffer);
                                xSemaphoreTakeRecursive( SmartMeter_xMutex, pdTICKS_TO_MS( 1000 )); // Should never wait more than 1000ms
                                if (TelegramCRC_Calc == TelegramChecksum) {
                                    memcpy(SmartMeter.Telegram, TelegramBuffer, RX_Index);
                                    SmartMeter.TelegramSize = RX_Index;
                                    SmartMeter.TelegramTag++;
                                } else {
                                    SmartMeter.TelegramCRC_Error++;
                                }
                                xSemaphoreGiveRecursive(SmartMeter_xMutex);
                                if (TelegramCRC_Calc == TelegramChecksum) {
                                    ESP_LOGI(TAG, "Telegram %u %u received", TelegramCounter, SmartMeter.TelegramCRC_Error);
                                } else {
                                   ESP_LOGI(TAG, "Telegram %u Checksum %u %u %u %u", TelegramCounter, SmartMeter.TelegramCRC_Error, RX_Index, TelegramCRC_Calc, TelegramChecksum);
                                }
                            }
                            break;
                        }
                    } else { // Overflow
                        RX_Index = 0;
                        State = COM_WAITING;
                        ESP_LOGE(TAG, "Telegram buffer receive overflow");
                    }
                }
            }
        }
    }
}

int16_t SmartMeter_CheckCopyTelegram(uint8_t *CurrentTag, uint8_t *Buffer, uint16_t BufferSize)
{
    int16_t TelegramSize = 0;
    uint8_t OldCurrentTag;
    if (SmartMeter_xMutex == NULL) {
        ESP_LOGE(TAG, "SmartMeter_CheckCopyTelegram called before SmartMeter_Initialize");
        return 0;
    }
    OldCurrentTag = *CurrentTag;
    xSemaphoreTakeRecursive( SmartMeter_xMutex, pdTICKS_TO_MS( 1000 )); // Should never wait more than 1000ms
    if (SmartMeter.TelegramTag != *CurrentTag) {
        if (BufferSize < SmartMeter.TelegramSize) {    
            TelegramSize = -1;
        } else {
            *CurrentTag = SmartMeter.TelegramTag;
            memcpy(Buffer, SmartMeter.Telegram, SmartMeter.TelegramSize);
            TelegramSize = SmartMeter.TelegramSize;
        }
    } else {
        TelegramSize = 0;
    }
    xSemaphoreGiveRecursive(SmartMeter_xMutex);
    
    if (TelegramSize < 0) {
        ESP_LOGE(TAG, "Copy buffer size %i < Telegram %u size ",  BufferSize, *CurrentTag);
    } else if (TelegramSize > 0) {
        ESP_LOGI(TAG, "Copying Telegram %u %u", *CurrentTag, OldCurrentTag);
    }
    return TelegramSize;
}
