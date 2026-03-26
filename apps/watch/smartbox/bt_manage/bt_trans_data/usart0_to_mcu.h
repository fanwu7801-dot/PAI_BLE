// 此文件定义了soc和mcu之间进行串口通信的设计，包含接受和发送函数
#ifndef __USART0_TO_MCU_H__
#define __USART0_TO_MCU_H__
#include "cpu.h"
#include <stdint.h>
// 重发机制配置结构体
typedef struct {
  uint8_t data[256];
  uint16_t data_length;
  uint16_t protocol_id;
  bool valid;
} message_t;
typedef struct {
  uint8_t *data;           // 要发送的数据
  uint16_t len;            // 数据长度
  uint16_t protocol_id;    // 协议ID
  uint8_t retry_count;     // 当前重试次数
  uint8_t max_retries;     // 最大重试次数
  uint32_t retry_interval; // 重试间隔(ms)
  bool waiting_response;   // 是否在等待响应
  uint32_t last_send_time; // 上次发送时间戳
} uart_retry_context_t;

void uart1_init(void);
void uart1_send(u8 *data, u16 len);
bool uart1_send_toMCU(uint16_t protocol_id, uint8_t *data,
                      uint16_t len); // 修正参数类型
void uart1_recieve(void);

// 重发机制相关函数
bool uart1_send_with_retry(uint16_t protocol_id, uint8_t *data, uint16_t len);
void uart1_retry_handler(void);
void uart1_reset_retry_state(void);
bool uart1_check_response(uint16_t expected_protocol_id);
bool uart1_take_response(uint16_t expected_protocol_id, uint8_t *out,
                         uint16_t out_size, uint16_t *out_len);
void uart1_set_retry_config(uint8_t max_retries, uint32_t retry_interval);
void uart1_retry_init(void);

// 阻塞等待功能
bool uart1_wait_response(uint16_t protocol_id, uint32_t timeout_ms);
bool uart1_send_and_wait(uint16_t protocol_id, uint8_t *data, uint16_t len,
                         uint32_t timeout_ms);
// 定时器检查机制相关函数
void uart1_check_init(uint32_t check_interval);
void uart1_check_start(void);
void uart1_check_stop(void);
void uart1_check_deinit(void);
void uart1_check_handler(void);

#endif