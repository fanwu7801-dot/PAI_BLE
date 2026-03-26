#include "tone_user_table.h"
#include "syscfg_id.h"
#include "tone_player.h"
#include "usart0_to_mcu.h"
#include "user_cfg_id.h"
#include <string.h>
// ================ 串口接收数据处理====================
// extern uint16_t uart_protocol_id;
// extern uint8_t *uart_data;
// extern uint8_t *uart_rx_data;
// extern uint16_t data_length;

// ================ 播放音频处理========================
/**
 * @brief 处理音频播放协议ID
 * @return true 协议ID匹配
 * @return false 协议ID不匹配
 */
bool uart_protocol_id_cmp(uint16_t protocol_id) {
  if (protocol_id == TONE_USER_TABLE_PROTOCOL_ID) {
    printf("进入音频播放处理\n");
    return true;
  }
  return false;
}

bool tone_player_cmp(int tone_id) {
  // 用于判断tone_id是否在tone_user_table.h中定义的范围内
  if (tone_id >= IDEX_TONE_POWER_ON && tone_id <= IDEX_TONE_NONE) {
    return true;
  }
  return false;
}