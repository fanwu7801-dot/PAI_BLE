#ifndef _CRC16_H
#define _CRC16_H
#include <stdint.h>
#include <string.h>
void calculateCRC16(const uint8_t *data, size_t len,
                    uint8_t out[2]); // 新版本crc算法

#endif