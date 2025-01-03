/*
 * SmartMeter_Config.h
 */

#ifndef _SMARTMETER_CONFIG_H
#define _SMARTMETER_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define _SMARTMETER_UART_TXD        GPIO_NUM_17
#define _SMARTMETER_UART_RXD        GPIO_NUM_16

#define _SMARTMETER_UART_PORT_NUM       2
#define _SMARTMETER_UART_BAUD_RATE      115200
#define _SMARTMETER_UART_BUFFER_SIZE    2048
#define _SMARTMETER_TELEGRAM_MAX_SIZE   2048

#ifdef __cplusplus
}
#endif

#endif /* _SMARTMETER_CONFIG_H */
