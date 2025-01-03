/*
 * SmartMeter.h
 */

#ifndef _SMARTMETER_H
#define _SMARTMETER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "SmartMeter_Config.h"

void SmartMeter_Initialize(void);
int16_t SmartMeter_CheckCopyTelegram(uint8_t *CurrentTag, uint8_t *Buffer, uint16_t BufferSize);

#ifdef __cplusplus
}
#endif

#endif /* _SMARTMETER_H */
