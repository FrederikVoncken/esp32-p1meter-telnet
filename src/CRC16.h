#ifndef CRC16_H
#define CRC16_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

uint16_t CRC16(uint16_t crc, unsigned char c);

#ifdef __cplusplus
}
#endif

#endif
