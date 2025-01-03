/*
 * CRC16.c
 */

#include <stdint.h>

uint16_t CRC16(uint16_t crc, unsigned char c)
{
    crc ^= (uint16_t)c; // XOR byte into least sig. byte of crc
    for (int i = 8; i != 0; i--) { // Loop over each bit
        if ((crc & 0x0001) != 0) {   // If the LSB is set
            crc >>= 1;
            crc ^= 0xA001;
        } else {
            crc >>= 1;
        }
    }
    return crc;
}