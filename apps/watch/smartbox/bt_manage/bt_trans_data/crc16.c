#include <stdint.h>
#include <string.h>

/*
 * 计算 data[0..len-1] 的 CRC16-CCITT（多项式 0x1021，初值
 * 0xFFFF，无输入反转，无输出反转） 结果写入 2 字节缓冲区
 * out[2]，低字节在前，高字节在后。
 */
void calculateCRC16(const uint8_t *data, size_t len, uint8_t out[2]) {
  uint16_t crc = 0xFFFFu;        /* 初值 */
  const uint16_t poly = 0x1021u; /* 多项式 */

  for (size_t idx = 0; idx < len; ++idx) {
    uint8_t byte = data[idx];

    for (int i = 0; i < 8; ++i) {
      uint8_t bit = (byte >> (7 - i)) & 1u;
      uint8_t c15 = (crc >> 15) & 1u;

      crc <<= 1;
      if (c15 ^ bit)
        crc ^= poly;
    }
  }

  /* 写入结果，Little-Endian */
  out[0] = crc & 0xFFu;        /* 低字节 */
  out[1] = (crc >> 8) & 0xFFu; /* 高字节 */
}

/* ------------------ 使用示例 ------------------
#include <stdio.h>

int main(void)
{
    uint8_t buf[] = {0x31, 0x32, 0x33, 0x34, 0x35}; /* "12345" */
// uint8_t crc[2];
// calculateCRC16(buf, sizeof(buf), crc);
// printf("CRC16 = %02X %02X\n", crc[1], crc[0]); /* 按高-低顺序打印 */
// return 0;
// }