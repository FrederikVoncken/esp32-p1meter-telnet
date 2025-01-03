#ifndef _WIFI_H
#define _WIFI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_wifi.h"

#define WIFI_StatusStart           0x01
#define WIFI_StatusConnected       0x02
#define WIFI_StatusDisconnected    0x04

void WIFI_Initialize(void);
uint8_t WIFI_GetCurrentEventId(void);

uint8_t WIFI_GetStatus(void);

#ifdef __cplusplus
}
#endif
#endif /* _WIFI_H */
