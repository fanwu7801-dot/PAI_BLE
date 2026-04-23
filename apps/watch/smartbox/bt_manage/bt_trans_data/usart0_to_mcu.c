// 注意：.uart_test.text 段在部分固件配置下不在 PC 可执行范围内，
// 若把关键逻辑放入该段会触发 CPU pc_limit 异常。
// 因此默认不启用；仅在确实需要调试段隔离时显式打开 UART1_TEST_SECTION_ENABLE。
#if defined(SUPPORT_MS_EXTENSIONS) && defined(UART1_TEST_SECTION_ENABLE)
#pragma bss_seg(".uart_test.data.bss")
#pragma data_seg(".uart_test.data")
#pragma const_seg(".uart_test.text.const")
#pragma code_seg(".uart_test.text")
#endif
#include "usart0_to_mcu.h"
#include "app_msg.h"
// #include "app_tone.h"
#include "crc16.h"
#include "debug.h"
#include "log.h"
#include "syscfg_id.h"
#include "system/includes.h"
#include "tone_player.h"
#include "tone_user_table.h"
#include "app_task.h"
#include "cpu/uart_v1.h"
#include "user_cfg_id.h"
#include "user_info_file.h"
#include "bt_common.h"
#include "btstack/le/ble_api.h"
#include <stdint.h>
#include <string.h>
#include "event.h"
#include "fill_protocol.h"
#include "audio_config.h"
#include "app_main.h"
#define UART_MAX_DATA_LEN 512
// 说明：工程中部分第三方头文件会用宏重新定义 bool（例如映射到 _Bool），
// 而 cpu.h 中 bool 是 typedef(u8)；两者叠加会导致“头文件声明 vs 源文件定义”类型不一致。
// 这里在编译单元内取消 bool 宏，确保本文件使用 cpu.h 的 typedef bool。
#ifdef bool
#undef bool
#endif

void uart_send_ble_key_list(uint16_t protocol_id);       
static void uart_send_password_key_list(uint16_t protocol_id);
static void uart1_pending_tx_try(void);
static bool uart1_tx_queue_push(const u8 *data, u16 len);
extern int dual_ota_app_data_deal(u32 msg, u8 *buf, u32 len);
extern uint8_t test_aes_key[16];
#define UART1_TX_QUEUE_DEPTH   4
#define UART1_TX_QUEUE_BUF_SIZE 269

static volatile u8 g_uart1_tx_queue_count;
static u8 g_uart1_tx_queue_head;
static u8 g_uart1_tx_queue_tail;
static u16 g_uart1_tx_queue_len[UART1_TX_QUEUE_DEPTH];
static u8 g_uart1_tx_queue_buf[UART1_TX_QUEUE_DEPTH][UART1_TX_QUEUE_BUF_SIZE];

extern hci_con_handle_t smartbox_get_con_handle(void);
extern void ancs_client_wait_request_pairing(u16 con_handle);
extern u32 smbox_pairing_generate_passkey(uint8_t code3[3]);
extern void smbox_pairing_set_pending(u16 conn_handle, const uint8_t code3[3], u32 passkey);
extern u8 smbox_pairing_is_pending(u16 conn_handle);
extern void ble_pairing_set_uart_passkey(uint32_t passkey);
extern void fill_protocol_set_uart_passkey(uint32_t passkey);
extern void smbox_pairing_clear_pending(void);
static int is_all_zero(const uint8_t *data, uint16_t len)
{
  if (data == NULL || len == 0) {
    return 1;
  }
  for (uint16_t i = 0; i < len; i++) {
    if (data[i] != 0) {
      return 0;
    }
  }
  return 1;
}


static int is_all_ff(const uint8_t *data, uint16_t len)
{
  if (data == NULL || len == 0) {
    return 1;
  }
  for (uint16_t i = 0; i < len; i++) {
    if (data[i] != 0xFF) {
      return 0;
    }
  }
  return 1;
}
 int sn_payload_to_hex8(const uint8_t *in, uint16_t in_len, uint8_t out[8])
{
  if (!in || !out || in_len == 0) {
    return -1;
  }

  // 1) 尝试按�?�十进制字�?串SN”解析：连续数字，后�?��许全0�?��
  uint16_t digit_len = 0;
  while (digit_len < in_len) {
    uint8_t c = in[digit_len];
    if (c == 0) {
      break;
    }
    if (c < '0' || c > '9') {
      break;
    }
    digit_len++;
  }

  bool looks_decimal = (digit_len > 0);
  if (looks_decimal) {
    for (uint16_t i = digit_len; i < in_len; i++) {
      if (in[i] != 0) {
        looks_decimal = false;
        break;
      }
    }
  }

  if (looks_decimal) {
    uint64_t v = 0;
    for (uint16_t i = 0; i < digit_len; i++) {
      uint8_t d = (uint8_t)(in[i] - '0');
      if (v > (UINT64_MAX - d) / 10) {
        return -1;
      }
      v = v * 10 + d;
    }

    // �?�� 8 字节大�?：等价于十六进制不足 16 位补 0
    for (int i = 7; i >= 0; i--) {
      out[i] = (uint8_t)(v & 0xFF);
      v >>= 8;
    }
    return 0;
  }

  // 1.5) 按�?字节BCD编码的十进制SN”解析：每个半字节一�?��进制数字
  // 例�?�?9 00 10 20 31 80 00 01 -> 十进制字符串"0900102031800001"
  // 再按规则�?字节HEX(不足高位�?)
  if (in_len >= 8) {
    bool looks_bcd = true;
    for (uint16_t i = 0; i < 8; i++) {
      uint8_t hi = (uint8_t)(in[i] >> 4);
      uint8_t lo = (uint8_t)(in[i] & 0x0F);
      if (hi > 9 || lo > 9) {
        looks_bcd = false;
        break;
      }
    }

    if (looks_bcd) {
      uint64_t v = 0;
      for (uint16_t i = 0; i < 8; i++) {
        uint8_t hi = (uint8_t)(in[i] >> 4);
        uint8_t lo = (uint8_t)(in[i] & 0x0F);

        if (v > (UINT64_MAX - hi) / 10) {
          return -1;
        }
        v = v * 10 + hi;

        if (v > (UINT64_MAX - lo) / 10) {
          return -1;
        }
        v = v * 10 + lo;
      }

      for (int i = 7; i >= 0; i--) {
        out[i] = (uint8_t)(v & 0xFF);
        v >>= 8;
      }
      return 0;
    }
  }

  // 2) 否则按�?�已经是8字节HEX”�?�?
  if (in_len >= 8) {
    memcpy(out, in, 8);
    return 0;
  }
  return -1;
}

// APP_MSG_USER 二级消息ID（本文件内部使用；�?部�?�?统一请迁移到�?��头）
#ifndef APP_MSG_USER_SN
#define APP_MSG_USER_SN 1
#endif
#ifndef APP_MSG_USER_AES_KEY
#define APP_MSG_USER_AES_KEY 2
#endif
#ifndef APP_MSG_USER_PARING_INFORMATION
#define APP_MSG_USER_PARING_INFORMATION 3
#endif
#ifndef APP_MSG_USER_READ_KEY_LIST
#define APP_MSG_USER_READ_KEY_LIST 4
#endif

#ifndef APP_MSG_USER_TONPLAY
#define APP_MSG_USER_TONPLAY 5
#endif

#ifndef APP_MSG_USER_CUSTOM_TONEPLAY
#define APP_MSG_USER_CUSTOM_TONEPLAY 6

#define UPDATA_SN_AES_KEY    1 // enable SN/AES key update from MCU

// UART->APP 音效�?��消息重试（避免消�?��列满时丢失）
#define TONEPLAY_RETRY_MAX         5
#define TONEPLAY_RETRY_INTERVAL_MS 50
static bool g_toneplay_pending = false;
static int g_toneplay_pending_id = 0;
static uint8_t g_toneplay_retry_count = 0;
static uint32_t g_toneplay_retry_tick = 0;
#endif

#ifndef UPDATA_SN_AES_KEY
#define UPDATA_SN_AES_KEY    1
#endif

// UART->SOC 配�? PIN/passkey 打包标志：arg0 = FLAG | passkey(0~999999)
// 说明：保�?arg0=0/1 的�?��?义（0x0037/0x0040），因�?使用高位标志避免冲突�?
#ifndef UART_PAIR_PASSKEY_FLAG
#define UART_PAIR_PASSKEY_FLAG 0x40000000
#endif

static int uart_parse_pair_passkey(const uint8_t *data, uint16_t len, uint32_t *passkey_out)
{
  if (!passkey_out) {
    return -1;
  }
  *passkey_out = 0;
  if (!data || !len) {
    return -1;
  }

  // 1) 优先�?�� ASCII 6 位数�?
  if (len >= 6) {
    uint32_t v = 0;
    uint8_t ok = 1;
    for (uint16_t i = 0; i < 6; i++) {
      if (data[i] < '0' || data[i] > '9') {
        ok = 0;
        break;
      }
      v = v * 10 + (data[i] - '0');
    }
    if (ok && v <= 999999) {
      *passkey_out = v;
      return 0;
    }
  }

  // 2) �?�� 3 字节大�? BE24（与现有配�?码下发格式�?齐）
  if (len >= 3) {
    uint32_t v = ((uint32_t)data[0] << 16) | ((uint32_t)data[1] << 8) | (uint32_t)data[2];
    if (v <= 999999 && v) {
      *passkey_out = v;
      return 0;
    }
  }

  return -1;
}

static int app_send_message(int user_sub_msg, int arg0) {
  return app_task_put_usr_msg(APP_MSG_USER, 2, user_sub_msg, arg0);
}

static void uart1_toneplay_retry_check(void) {
  if (!g_toneplay_pending) {
    return;
  }

  uint32_t now = jiffies;
  if ((uint32_t)(now - g_toneplay_retry_tick) < TONEPLAY_RETRY_INTERVAL_MS) {
    return;
  }

  g_toneplay_retry_tick = now;
  int ret = app_send_message(APP_MSG_USER_TONPLAY, g_toneplay_pending_id);
  if (ret == 0) {
    g_toneplay_pending = false;
    g_toneplay_retry_count = 0;
    return;
  }

  g_toneplay_retry_count++;
  if (g_toneplay_retry_count >= TONEPLAY_RETRY_MAX) {
    printf("TONPLAY post fail: drop id=%d\n", g_toneplay_pending_id);
    g_toneplay_pending = false;
    g_toneplay_retry_count = 0;
  }
}
/*******************************************************
 * @brief Set the mcu for volume change object 
 * 
 * @param volume {placeholder}
*******************************************************/
static s8 g_uart_last_persist_vol = -2; // -2: �?��始化
static void set_mcu_for_volume_change(uint8_t volume)
{
  /* MCU->SOC 音量设置：策略与 fill_protocol �?set_volume_instruct �?�?*/
  uint8_t sys_max = get_max_sys_vol();
  uint8_t vol_val = volume;
  uint8_t mapped;

  if (vol_val >= 1 && vol_val <= 3) {
    mapped = vol_val; // 1/2/3
  } else if (vol_val <= 100) {
    if (vol_val <= 33) {
      mapped = 1;
    } else if (vol_val <= 66) {
      mapped = 2;
    } else {
      mapped = 3;
    }
  } else {
    mapped = 2;
  }

  s8 soc_volume = 0;
  switch (mapped) {
  case 3:
    soc_volume = 15;
    break;
  case 2:
    soc_volume = 18;
    if (soc_volume < 1) {
      soc_volume = 1;
    }
    break;
  case 1:
  default:
    soc_volume = 20;
    if (soc_volume < 1) {
      soc_volume = 1;
    }
    break;
  }

  if (soc_volume > sys_max) {
    soc_volume = sys_max;
  }
  if (soc_volume < 0) {
    soc_volume = 0;
  }

  printf("mcu_set_volume: raw=%u mapped=%u soc_vol=%d (sys_max=%u)\n",
         (unsigned)vol_val, (unsigned)mapped, soc_volume, (unsigned)sys_max);

  app_var.music_volume = soc_volume;
  app_var.wtone_volume = soc_volume;

  if (g_uart_last_persist_vol == -2) {
    u8 tmp = 0;
    if (syscfg_read(CFG_MUSIC_VOL, &tmp, 1) == 1) {
      g_uart_last_persist_vol = (s8)tmp;
    } else {
      g_uart_last_persist_vol = -1;
    }
  }
  if (g_uart_last_persist_vol != soc_volume) {
    int wret = syscfg_write(CFG_MUSIC_VOL, &soc_volume, 1);
    u8 verify = 0xFF;
    int rret = syscfg_read(CFG_MUSIC_VOL, &verify, 1);
    printf("mcu_set_volume: persist CFG_MUSIC_VOL=%d wret=%d rret=%d verify=%d\n",
           soc_volume, wret, rret, verify);
    g_uart_last_persist_vol = soc_volume;
  }

  app_audio_set_volume(APP_AUDIO_STATE_MUSIC, soc_volume, 1);
  app_audio_set_volume(APP_AUDIO_STATE_WTONE, soc_volume, 1);
}
//=====================================================================
// 定时器�?查机制全�?变量
static bool uart_check_enabled = false;       // 定时器使能标�?
static bool uart_check_timer_enabled = false; // 定时器使能标�?
static uint32_t uart_check_interval = 100;    // �?查间�?ms)
static uint32_t last_check_time = 0;          // 上�?�?查时间戳
// 重发机制全局变量声明（必须在函数实现之前�?
static uart_retry_context_t uart_retry_ctx = {0};
static uint8_t retry_data_buffer[256];  // 重发数据缓冲�?
static bool uart_retry_enabled = false; // 重发机制使能标志
bool uart1_parse_packet(uint8_t *rx_data, uint16_t data_len);

// 供广�?业务侧判�??�是否已收到MCU下发的SN/AES(运�?期有�?�?
// 说明：即使syscfg尚未能�?到（或写入失败），也�?��先用�?新的运�?期�?�刷新广�?��?
volatile uint8_t g_uart_sn_valid = 0;
volatile uint8_t g_uart_aes_valid = 0;

static void uart1_request_mcu_update_once(void)
{
  static bool done = false;
  if (done) {
    return;
  }
  done = true;

  // 主动向MCU发起更新请求�?x00F6/0x00F7（按协�?约定为�?��?求帧”，payload为空�?
  // 放在UART线程初�?化成功后，避免上电早期MCU尚未就绪导致丢包�?
  uart1_send_toMCU(0x00F6, NULL, 0);
  os_time_dly(2);
  uart1_send_toMCU(0x00F7, NULL, 0);
}
uint8_t uart_sn_buffer[8] = {0x90, 0x01, 0x02, 0x03,
                             0x18, 0x00, 0x00, 0x05}; // 用于存储sn�?
uint8_t uart_aes_key_buffer[16] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                   0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                   0x00, 0x00, 0x00, 0x00};
int r; // 数据包长�?
static bool uart_runtime_key_is_valid(const uint8_t *key, uint16_t len)
{
  uint16_t i;
  bool all_zero = true;
  bool all_ff = true;

  if (!key || !len) {
    return false;
  }

  for (i = 0; i < len; i++) {
    if (key[i] != 0x00) {
      all_zero = false;
    }
    if (key[i] != 0xFF) {
      all_ff = false;
    }
  }

  return (!all_zero && !all_ff);
}

void uart_runtime_sn_update(const uint8_t sn[8])
{
  if (!sn) {
    return;
  }

  OS_ENTER_CRITICAL();
  memcpy(uart_sn_buffer, sn, sizeof(uart_sn_buffer));
  g_uart_sn_valid = 1;
  OS_EXIT_CRITICAL();
}

bool uart_runtime_sn_get(uint8_t sn_out[8])
{
  bool valid = false;

  if (!sn_out) {
    return false;
  }

  OS_ENTER_CRITICAL();
  if (g_uart_sn_valid) {
    memcpy(sn_out, uart_sn_buffer, sizeof(uart_sn_buffer));
    valid = true;
  }
  OS_EXIT_CRITICAL();

  return valid;
}

void uart_runtime_aes_key_update(const uint8_t aes_key[16])
{
  if (!aes_key) {
    return;
  }

  OS_ENTER_CRITICAL();
  memcpy(uart_aes_key_buffer, aes_key, sizeof(uart_aes_key_buffer));
  g_uart_aes_valid = 1;
  OS_EXIT_CRITICAL();
  
  printf("[UART] Runtime AES key updated:\n");
  put_buf(uart_aes_key_buffer, 16);
}

bool uart_runtime_aes_key_get(uint8_t aes_key_out[16])
{
  bool valid = false;

  if (!aes_key_out) {
    return false;
  }

  OS_ENTER_CRITICAL();
  if (g_uart_aes_valid && uart_runtime_key_is_valid(uart_aes_key_buffer, sizeof(uart_aes_key_buffer))) {
    memcpy(aes_key_out, uart_aes_key_buffer, sizeof(uart_aes_key_buffer));
    valid = true;
  }
  OS_EXIT_CRITICAL();

  return valid;
}

uint16_t uart_protocol_id = 0;
uint8_t *uart_data = NULL;
uint8_t *uart_rx_data = NULL;
uint16_t data_length = 0;
void *uart_rx_ptr = NULL;

// ==================== 多包拼接逻辑 ====================
// 根据报文格式约定�?
// - 单帧固定�?1
// - 多帧�?要根�?��际情况定义顺序号
#define MAX_MULTI_PACKET_SIZE 4096  // �?大支�?4KB 多包数据
static uint8_t g_multi_packet_buffer[MAX_MULTI_PACKET_SIZE];  // 多包缓冲�?
static uint16_t g_current_packet_seq = 0;     // 当前包序�?
static uint16_t g_expected_packet_seq = 1;    // 期望的包序号（从 1 �?始）
static uint16_t g_multi_packet_offset = 0;    // 多包当前偏移
static uint16_t g_multi_packet_protocol = 0;  // 多包协�? ID
static bool g_is_multi_packet_mode = false;   // �?��处于多包模式
uint8_t frame[256] = {0x55, 0xAA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xAA};
static void uart_irq_func(uart_dev uart_num, enum uart_event_v1 event);

static int uart1_dev = 1;

// UART1 收发调试�?关�?
// 说明：大�?printf/put_buf 会显著降低吞吐，并且会拖慢系统调度，表现为�?�串�?队列处理慢�?��?
// 默�?关闭，仅在定位协�?��题时打开�?
#ifndef UART1_IO_DEBUG_ENABLE
#define UART1_IO_DEBUG_ENABLE 0
#endif

#if UART1_IO_DEBUG_ENABLE
#define UART1_IO_LOG(...) printf(__VA_ARGS__)
#define UART1_IO_DUMP(buf, len) put_buf((const u8 *)(buf), (len))
#else
#define UART1_IO_LOG(...)
#define UART1_IO_DUMP(buf, len)
#endif

// RX_TIMEOUT 表示“帧接收完成”（�?rx_timeout_thresh 决定）�?
// 在回调里�?��位标志，主循�?��读取数据并解析，避免阻�?等待固定长度�?
static volatile u8 g_uart1_rx_frame_ready = 0;

static uint16_t uart1_calc_frame_len(const uint8_t *buf, uint16_t remain)
{
  if (!buf || remain < 2) {
    return 0;
  }
  if (buf[0] != 0xFE) {
    return 0;
  }

  if (buf[1] == 0xBA) {
    if (remain < 10) {
      return 0;
    }
    uint16_t payload_len = ((uint16_t)buf[4] << 8) | (uint16_t)buf[5];
    if (payload_len > UART_MAX_DATA_LEN) {
      return 0;
    }
    uint16_t frame_len = (uint16_t)(6 + payload_len + 4);
    if (remain < frame_len) {
      return 0;
    }
    return frame_len;
  }

  if (remain < 13) {
    return 0;
  }
  uint16_t payload_len = ((uint16_t)buf[5] << 8) | (uint16_t)buf[6];
  if (payload_len > UART_MAX_DATA_LEN) {
    return 0;
  }
  uint16_t frame_len = (uint16_t)(9 + payload_len + 4);
  if (remain < frame_len) {
    return 0;
  }
  return frame_len;
}

static void uart_update_ble_adv_restart(void)
{
  extern void le_smartbox_ble_adv_restart(void);
  le_smartbox_ble_adv_restart();
}

// 00FB协�?防抖机制
static u32 g_last_send_key_list_time = 0;
//===========================================
/**
 * @brief 定时�?查函数，�?要在主循�?��定期调用
 */
void uart1_check_handler(void) {
  if (!uart_check_enabled || !uart_retry_enabled) {
    return;
  }

  // �?查是否�?在等待响�?
  if (!uart_retry_ctx.waiting_response) {
    return;
  }

  // �?查时间间�?
  uint32_t current_time = jiffies;
  uint32_t elapsed_time = current_time - last_check_time;

  if (elapsed_time < uart_check_interval) {
    return; // �?���?查时�?
  }

  last_check_time = current_time; // 更新�?查时�?

  // �?查是否收到响应（protocol_id匹配�?
  if (uart1_check_response(uart_retry_ctx.protocol_id)) {
    printf("UART check: response received, stop sending. protocol_id=0x%04X\n",
           uart_retry_ctx.protocol_id);
    uart1_reset_retry_state();
    return;
  }

  // �?查是否达到最大发送�?�?
  if (uart_retry_ctx.retry_count >= uart_retry_ctx.max_retries) {
    printf("UART check: max retries reached (%d/%d), stop sending. "
           "protocol_id=0x%04X\n",
           uart_retry_ctx.retry_count, uart_retry_ctx.max_retries,
           uart_retry_ctx.protocol_id);
    uart1_reset_retry_state();
    return;
  }

  // �?查是否超时需要重�?
  uint32_t elapsed_send_time = current_time - uart_retry_ctx.last_send_time;

  if (elapsed_send_time >= uart_retry_ctx.retry_interval) {
    // 超时，�?查是否需要重�?
    if (uart_retry_ctx.retry_count < uart_retry_ctx.max_retries) {
      uart_retry_ctx.retry_count++;
      uart_retry_ctx.last_send_time = current_time;

      printf("UART check: retry %d/%d: protocol_id=0x%04X\n",
             uart_retry_ctx.retry_count, uart_retry_ctx.max_retries,
             uart_retry_ctx.protocol_id);

      // 重发数据
      uart1_send_toMCU(uart_retry_ctx.protocol_id, uart_retry_ctx.data,
                       uart_retry_ctx.len);
    } else {
      // 达到�?大重试�?数，重置状�?
      printf("UART check: retry failed after %d attempts\n",
             uart_retry_ctx.max_retries);
      uart1_reset_retry_state();
    }
  }
}

/**
 * @brief 初�?化定时器�?查机�?
 * @param check_interval �?查间�?ms)
 */
void uart1_check_init(uint32_t check_interval) {
  uart_check_interval = check_interval;
  uart_check_enabled = true;
  last_check_time = jiffies;

  printf("UART check initialized: interval=%dms\n", check_interval);
}

/**
 * @brief �?��定时器�?查机�?
 */
void uart1_check_start(void) {
  if (!uart_check_enabled) {
    printf("UART check not initialized\n");
    return;
  }

  last_check_time = jiffies;
  printf("UART check started\n");
}

/**
 * @brief 停�?定时器�?查机�?
 */
void uart1_check_stop(void) {
  uart_check_enabled = false;
  printf("UART check stopped\n");
}

/**
 * @brief �?毁定时器�?查机�?
 */
void uart1_check_deinit(void) {
  uart_check_enabled = false;
  printf("UART check deinitialized\n");
}
//===========================================

/**
 * @brief �?查是否收到指定协议ID的响�?
 * @param expected_protocol_id 期望的协议ID
 * @return true 收到响应，false �?��到响�?
 */
bool uart1_check_response(uint16_t expected_protocol_id) {
  if (uart_protocol_id == expected_protocol_id && uart_data != NULL) {
    printf("UART response received: protocol_id=0x%04X\n",
           expected_protocol_id);
    // 清空全局变量，避免重复�?�?
    uart_protocol_id = 0;
    uart_data = NULL;
    data_length = 0;
    return true;
  }
  return false;
}

/**
 * @brief 原子地拷贝并取走�?近一次响应数�?
 * @param expected_protocol_id 期望的协议ID
 * @param out 输出缓冲�?
 * @param out_size 输出缓冲区大�?
 * @param out_len 实际拷贝长度
 * @return true 已取到响应并拷贝；false �?���?
 */
bool uart1_take_response(uint16_t expected_protocol_id, uint8_t *out,
                         uint16_t out_size, uint16_t *out_len) {
  if (out_len) {
    *out_len = 0;
  }
  if (out == NULL || out_size == 0 || out_len == NULL) {
    return false;
  }

  bool ok = false;
  OS_ENTER_CRITICAL();
  if (uart_protocol_id == expected_protocol_id && uart_data != NULL &&
      data_length > 0) {
    uint16_t copy_len = data_length;
    if (copy_len > out_size) {
      copy_len = out_size;
    }
    memcpy(out, uart_data, copy_len);
    *out_len = copy_len;

    uart_protocol_id = 0;
    uart_data = NULL;
    data_length = 0;
    ok = true;
  }
  OS_EXIT_CRITICAL();
  return ok;
}
// 重发机制相关函数实现
/**
 * @brief 设置重发机制配置
 * @param max_retries �?大重试�?�?
 * @param retry_interval 重试间隔(ms)
 */
void uart1_set_retry_config(uint8_t max_retries, uint32_t retry_interval) {
  uart_retry_ctx.max_retries = max_retries;
  uart_retry_ctx.retry_interval = retry_interval;
  uart_retry_enabled = true;
  printf("UART retry config set: max_retries=%d, interval=%dms\n", max_retries,
         retry_interval);
}

/**
 * @brief 重置重发状�?
 */
void uart1_reset_retry_state(void) {
  uart_retry_ctx.waiting_response = false;
  uart_retry_ctx.retry_count = 0;
  uart_retry_ctx.protocol_id = 0;
  uart_retry_ctx.len = 0;
  if (uart_retry_ctx.data != NULL) {
    free(uart_retry_ctx.data);
    uart_retry_ctx.data = NULL;
  }
}
/**
 * @brief 阻�?等待响应（新增功能）
 * @param protocol_id 协�?ID
 * @param timeout_ms 超时时间（�?秒）
 * @return true 收到响应，false 超时
 */
bool uart1_wait_response(uint16_t protocol_id, uint32_t timeout_ms) {
  uint32_t start_time = jiffies;
  uint32_t timeout_jiffies = timeout_ms * 1000; // �?��为jiffies

  while (jiffies - start_time < timeout_jiffies) {
    // �?查是否收到响�?
    if (uart1_check_response(protocol_id)) {
      return true;
    }

    // 处理重发机制
    uart1_retry_handler();

    // �?��延时，避免过度占用CPU
    os_time_dly(10); // 延时10ms
  }

  printf("UART response timeout: protocol_id=0x%04X\n", protocol_id);
  return false;
}
/**
 * @brief 带重发的阻�?发�?�（新�?功能�?
 * @param protocol_id 协�?ID
 * @param data 数据指针
 * @param len 数据长度
 * @param timeout_ms 等待响应的超时时间（�??�?
 * @return true 发�?�成功并收到响应，false 发�?�失败或超时
 */
bool uart1_send_and_wait(uint16_t protocol_id, uint8_t *data, uint16_t len,
                         uint32_t timeout_ms) {
  // 发�?�数�?��带重发机制）
  if (!uart1_send_with_retry(protocol_id, data, len)) {
    printf("没有收到响应\n");
  }

  // 阻�?等待响应
  return uart1_wait_response(protocol_id, timeout_ms);
}

/**
 * @brief 带重发机制的串口发�?�函�?
 * @param protocol_id 协�?ID
 * @param data 数据指针
 * @param len 数据长度
 * @return true 发�?�成功，false 发�?�失�?
 */
bool uart1_send_with_retry(uint16_t protocol_id, uint8_t *data, uint16_t len) {
  if (!uart_retry_enabled) {
    // 重发机制�?��能，直接发�?
    return uart1_send_toMCU(protocol_id, data, len);
  }

  // �?查是否�?在等待响�?
  if (uart_retry_ctx.waiting_response) {
    printf("UART is waiting for response, skip new send request\n");
    return false;
  }

  // 保存发�?�数�?��缓冲�?
  if (len > sizeof(retry_data_buffer)) {
    printf("UART retry data too large: %d > %zu\n", len,
           sizeof(retry_data_buffer));
    return false;
  }

  memcpy(retry_data_buffer, data, len);
  uart_retry_ctx.data = retry_data_buffer;
  uart_retry_ctx.len = len;
  uart_retry_ctx.protocol_id = protocol_id;
  uart_retry_ctx.retry_count = 0;
  uart_retry_ctx.waiting_response = true;
  uart_retry_ctx.last_send_time = jiffies; // 使用系统时间�?

  // �?��次发�?
  printf("UART send with retry: protocol_id=0x%04X, len=%d\n", protocol_id,
         len);
  return uart1_send_toMCU(protocol_id, data, len);
}

/**
 * @brief 重发处理函数，需要在主循�?��定期调用
 */
void uart1_retry_handler(void) {
  if (!uart_retry_enabled || !uart_retry_ctx.waiting_response) {
    return;
  }

  // �?查是否收到响�?
  if (uart1_check_response(uart_retry_ctx.protocol_id)) {
    uart1_reset_retry_state();
    return;
  }

  // �?查是否超�?
  uint32_t current_time = jiffies;
  uint32_t elapsed_time = current_time - uart_retry_ctx.last_send_time;

  if (elapsed_time >= uart_retry_ctx.retry_interval) {
    // 超时，�?查是否需要重�?
    if (uart_retry_ctx.retry_count < uart_retry_ctx.max_retries) {
      uart_retry_ctx.retry_count++;
      uart_retry_ctx.last_send_time = current_time;

      printf("UART retry %d/%d: protocol_id=0x%04X\n",
             uart_retry_ctx.retry_count, uart_retry_ctx.max_retries,
             uart_retry_ctx.protocol_id);

      // 重发数据
      uart1_send_toMCU(uart_retry_ctx.protocol_id, uart_retry_ctx.data,
                       uart_retry_ctx.len);
    } else {
      // 达到�?大重试�?数，重置状�?
      printf("UART retry failed after %d attempts\n",
             uart_retry_ctx.max_retries);
      uart1_reset_retry_state();
    }
  }
}

static void uart1_sync_demo(void *p) {
  const struct uart_config config = {
      .baud_rate = 115200,
      .tx_pin = IO_PORTA_01,
      .rx_pin = IO_PORTA_02,
      .parity = UART_PARITY_DISABLE,
  };

  uart_rx_ptr = dma_malloc(2048);

  const struct uart_dma_config dma = {
      .rx_timeout_thresh =
          30 * 10000000 /
          config.baud_rate, // ??:us,? ~30 ? byte ???????????
      .event_mask =
          UART_EVENT_TX_DONE | UART_EVENT_RX_FIFO_OVF | UART_EVENT_RX_TIMEOUT,
      .tx_wait_mutex = 0, // 1:不支持中�?���?互斥,0:�?���?��,不互�?
      .irq_priority = 3,
      .irq_callback = uart_irq_func,
      .rx_cbuffer = uart_rx_ptr,
      .rx_cbuffer_size = 2048,
      .frame_size = 2048, //=rx_cbuffer_size
  };

  printf("************uart has being init ***********\n");

  uart_dev uart_id = 1;
  int ut = uart_init_new(uart_id, &config);
  if (ut < 0) {
    printf("uart(%d) init error\n", ut);
  } else {
    printf("uart(%d) init ok\n", ut);
  }
  uart1_dev = ut;
  r = uart_dma_init(uart1_dev, &dma);
  if (r < 0) {
    printf("uart(%d) dma init error\n", ut);
  } else {
    printf("uart(%d) dma init ok\n", ut);
  }

  // uart1_set_retry_config(
  //     4, 1000); // �?大重�?次，间隔1�?根据派电的协�?��5s内需要给定回�?

  // 初�?化定时器�?查机制，使用MAX_SEND_NUM作为�?大发送�?�?
  uart1_check_init(100); // 100ms�?查间�?
  uart1_check_start();   // �?��定时器�?�?

  // �?flash 读取 AES key 到运行时缓冲�?
  {
    uint8_t flash_aes_key[16] = {0};
    int r = syscfg_read(CFG_DEVICE_AES_KEY, flash_aes_key, sizeof(flash_aes_key));
    if (r == sizeof(flash_aes_key) && uart_runtime_key_is_valid(flash_aes_key, sizeof(flash_aes_key))) {
      memcpy(uart_aes_key_buffer, flash_aes_key, sizeof(uart_aes_key_buffer));
      g_uart_aes_valid = 1;
      printf("[UART] AES key loaded from flash:\n");
      put_buf(uart_aes_key_buffer, 16);
    } else {
      printf("[UART] No valid AES key in flash, using zero buffer\n");
      g_uart_aes_valid = 0;
    }
  }

  // UART初�?化完成后，主动向MCU发起�?�?037/0038更新请求
  uart1_request_mcu_update_once();
  uart_dump();
  while (1) {
    // 尝试重投递音效播放消�?��非阻塞）
    uart1_pending_tx_try();
    uart1_toneplay_retry_check();
    // 处理重发机制
    // uart1_retry_handler();
    // 说明：原先使�?uart_recv_blocking(uart, buf, 256, 20) 会等待�?�收�?56字节”才返回�?
    // 实际�?帧�?�常远小�?56，�?致每包�?外等待到超时窗口，表现为“串口慢/队列慢�?��?
    // 改为 RX_TIMEOUT 事件驱动：只有当�?帧接收完成时才�?取并解析�?

    if (!g_uart1_rx_frame_ready) {
      os_time_dly(1);
      continue;
    }

    g_uart1_rx_frame_ready = 0;

    // �?UART cbuf �?��当前�??的数�?��次�?��?出来（最�?2048 字节，与 rx_cbuffer_size �?致）�?
    u32 total = 0;
    // 使用非阻塞式读取，避免应用层�?���?
    s32 rr = uart_recv_bytes(uart1_dev, (u8 *)uart_rx_ptr, 2048);
    if (rr > 0) {
      total = (u32)rr;
    }

    if (total > 0) {
      r = (int)total;
      UART1_IO_LOG("r:%d\n", r);
      UART1_IO_DUMP((u8 *)uart_rx_ptr, r);

      uint32_t parse_off = 0;
      uint8_t *rx_buf = (uint8_t *)uart_rx_ptr;
      while (parse_off < total) {
        while (parse_off < total && rx_buf[parse_off] != 0xFE) {
          parse_off++;
        }
        if (parse_off >= total) {
          break;
        }

        uint16_t frame_len = uart1_calc_frame_len(&rx_buf[parse_off], (uint16_t)(total - parse_off));
        if (frame_len == 0) {
          break;
        }
        UART1_IO_LOG("找到�?帧数�?��长度=%d\n", frame_len);
        uart1_parse_packet(&rx_buf[parse_off], frame_len);
        UART1_IO_LOG("protocol_id:0x%04X\n", uart_protocol_id);

        if (uart_data != NULL) {
        UART1_IO_LOG("recv data:\n");
        UART1_IO_DUMP(uart_data, data_length);

        /*todo:新添蓝牙设置音量大小逻辑*/
        {
          /*如果 protocol_id = 0x00F1 data == 0x47 ---> 进入音量设置 data[1] = 1/2/3 对应 高中�?
          考虑调用fill_protocol的音量�?理�?�辑*/
          // extern void set_volume_instruct(uint16_t protocol_id);
        }
        if (uart_protocol_id == 0x00F1 && data_length >= 2 && uart_data[0] == 0x2F) {
          printf("进入音量设置");
          set_mcu_for_volume_change(uart_data[1]);
        }

          if (uart_protocol_id == 0x00F4) { // 把uart_data的�?1�? �?��为int类型（使用原�?16-bit 大�?�?
          if (data_length >= 2) {
            u16 code = ((u16)uart_data[0] << 8) | uart_data[1];
            int tone_id = (int)code;
            UART1_IO_LOG("0x00F4: parsed tone_id raw=0x%04X (%d)\n", code, tone_id);

            // 统一�?APP_MSG_USER_TONPLAY；自定义音效优先逻辑已合并到 bt.c �?case 5
            int ret = app_send_message(APP_MSG_USER_TONPLAY, tone_id);
            if (ret == 0) {
              g_toneplay_pending = false;
              g_toneplay_retry_count = 0;
            } else {
              g_toneplay_pending = true;
              g_toneplay_pending_id = tone_id;
              g_toneplay_retry_count = 0;
              g_toneplay_retry_tick = jiffies;
            }
          }

        }
#if UPDATA_SN_AES_KEY
        if (uart_protocol_id == 0x00F6) {
          {
            // MCU->SOC：更新SN (0x00F6)
            // �?��两�?格式�?
            // 1) 十进制字符串SN：按规则�?字节HEX(不足高位�?)写入CFG_DEVICE_SN
            // 2) 直接8字节HEX：原样写�?FG_DEVICE_SN
            uint8_t sn_hex8[8] = {0};
            if (sn_payload_to_hex8(uart_data, data_length, sn_hex8) != 0) {
              printf("SN 数据更新的长�?%d\n", data_length);
            } else {
              uart_runtime_sn_update(sn_hex8);
              int w = syscfg_write(CFG_DEVICE_SN, sn_hex8, 8);
              printf("SN updated(保存�?��为HEX8):\n");
              put_buf(sn_hex8, 8);

              // 读回验证，帮助定位�?�写了但广播没变”的�??
              {
                uint8_t rb[8] = {0};
                int r = syscfg_read(CFG_DEVICE_SN, rb, sizeof(rb));
                printf("syscfg_write(CFG_DEVICE_SN) ret=%d, readback ret=%d\n", w, r);
                if (r == sizeof(rb)) {
                  printf("SN readback(HEX8):\n");
                  put_buf(rb, sizeof(rb));
                }
              }

              app_send_message(APP_MSG_USER_SN, 0);
              // uart_update_ble_adv_restart();
            }
          }
        }
        if (uart_protocol_id == 0x00F7) {
          // MCU->SOC：更新AES_KEY (0x00F7, len=16)
          if (data_length < 16) {
            printf("AES_KEY update invalid len=%d\n", data_length);
          } else {
            uint8_t new_aes_key[16] = {0};
            memcpy(new_aes_key, uart_data, sizeof(new_aes_key));
            uart_runtime_aes_key_update(new_aes_key);
            int w = syscfg_write(CFG_DEVICE_AES_KEY, new_aes_key, sizeof(new_aes_key));
            printf("AES_KEY updated and saved.\n");

            // 读回验证
            {
              uint8_t rb[16] = {0};
              int r = syscfg_read(CFG_DEVICE_AES_KEY, rb, sizeof(rb));
              printf("syscfg_write(CFG_DEVICE_AES_KEY) ret=%d, readback ret=%d\n", w, r);
              printf("AES_KEY readback:\n");
              put_buf(new_aes_key, 16);
              if (memcmp(rb, new_aes_key, 16) == 0) {
                printf("AES_key_�?验成�? matches written value.\n");
              } else {
                printf("AES_KEY_�?验失�? does not match written value.\n");
              }
              
            }

            // app_send_message(APP_MSG_USER_AES_KEY, 0);
            uart_update_ble_adv_restart();
          }
        }
#endif

        if (uart_protocol_id == 0x0033)
        { // 当前�?要直接获取车辆�?�?���?���?在le_trans_data 当中死等不到回应
          // extern void  get_vehice_set_infromation_instruct(uint16_t protocol_id);
          //  app_send_message(APP_MSG_USER_VEHICLE_INFORMATION,
          //                  0); // �?��为int传给事件队列-------> 用于处理车辆设置信息
          
        }
        if(uart_protocol_id == 0x0037)
        { /*
          * 处理配�?信息指令 0x0037
          * 新�?：支持携�?PIN/passkey（ASCII 6�?�?3字节BE24），用于直接指定配�?码；
          *      不携�?解析失败则保持旧逻辑(随机生成)�?
          */
          uint32_t passkey = 0;
          int arg0 = 0;
          if (uart_parse_pair_passkey(uart_data, data_length, &passkey) == 0) {
            arg0 = (int)(UART_PAIR_PASSKEY_FLAG | (passkey & 0x000FFFFF));
            printf("[UART] pair passkey from MCU: %06u\n", passkey);
          }
          app_send_message(APP_MSG_USER_PARING_INFORMATION, arg0);
        }
        if(uart_protocol_id == 0x0038)
        {
          app_send_message(APP_MSG_USER_READ_KEY_LIST,
                           0); // �?��为int传给事件队列-------> 用于处理配�?信息指令
        }
        if (uart_protocol_id == 0x0040)
        {
          // 处理4G+BLE情况下进行串口下发add钥匙指令
          app_send_message(APP_MSG_USER_PARING_INFORMATION,
                           1); // �?��为int传给事件队列-------> 用于处理配�?信息指令
        }
        
        if (uart_protocol_id == 0x00F8)
        {
          // 添加蓝牙钥匙 - 先分析钥匙类型，然后根据不同的钥匙类型去写�?�辑
          // 数据格式：[设�?类型(4字节)][MAC地址(6字节)][匹配密码(4字节)]
          printf("[UART] PID_BLE_ADD_BLUETOOTH_KEY: 添加蓝牙钥匙\n");
          if (data_length >= 14) {
            uint8_t device_type[4];
            memcpy(device_type, uart_data, 4);
            uint8_t MAC_addr[6];
            memcpy(MAC_addr, uart_data + 4, 6);
            uint8_t password[4];
            memcpy(password, uart_data + 10, 4);
            
            printf("设�?类型: %02X %02X %02X %02X, MAC地址:%02X:%02X:%02X:%02X:%02X:%02X, 密码:%02X %02X %02X %02X\n",
                   device_type[0], device_type[1], device_type[2], device_type[3],
                   MAC_addr[0], MAC_addr[1], MAC_addr[2], MAC_addr[3], MAC_addr[4], MAC_addr[5],
                   password[0], password[1], password[2], password[3]);
            
            uint8_t key_type = device_type[3];
            
            switch (key_type) {
              case 0x01:
                printf("手机蓝牙钥匙\n");
                break;
              case 0x02:
              case 0xFE:
                printf("外�?蓝牙钥匙\n");
                break;
              case 0x03:
                printf("NFC钥匙\n");
                break;
              case 0x04:
                printf("密码钥匙\n");
                break;
              default:
                printf("�?��钥匙类型: 0x%02X\n", key_type);
                break;
            }
            
            if (key_type == 0x02 || key_type == 0xFE) {
              uint8_t peripheral_key_num = 0;
              int ret = syscfg_read(CFG_BLE_KEY_PERIPHERAL_USABLE, &peripheral_key_num, sizeof(peripheral_key_num));
              
              if (ret <= 0) {
                printf("读取CFG_BLE_KEY_PERIPHERAL_USABLE失败，使用默认�?\n");
                peripheral_key_num = 0;
              }
              
              if (peripheral_key_num > 4) {
                printf("�?测到无效的key_num�? %d，重�?��0\n", peripheral_key_num);
                peripheral_key_num = 0;
              }
              
              printf("当前�?��用的外�?蓝牙钥匙保存空间:%d\n", peripheral_key_num);
              uint8_t Uable_num = 4 - peripheral_key_num;
              
              if (Uable_num > 0) {
                printf("�?测到�?��用的外�?蓝牙钥匙保存空间，Uable_num:%d\n", Uable_num);
                
                uint8_t current_MAC_adders[6] = {0};
                int mac_exists = 0;
                
                for (int i = 1; i <= 4; i++) {
                  uint16_t cfg_id = CFG_BLE_KEY_PERIPHERAL_SEND_1 + (i - 1);
                  syscfg_read(cfg_id, current_MAC_adders, 6);
                  if (!is_all_zero(current_MAC_adders, 6) && memcmp(current_MAC_adders, MAC_addr, 6) == 0) {
                    mac_exists = 1;
                    printf("MAC地址已存在于位置%d\n", i);
                    break;
                  }
                }
                
                if (mac_exists) {
                  printf("MAC地址已存�?��不保存\n");
                } else {
                  // �?�?AC地址�?��全为0
                  if (is_all_zero(MAC_addr, 6)) {
                    printf("MAC地址全为0，不保存\n");
                  } else {
                    uint8_t save_buffer[28] = {0};
                    memcpy(save_buffer, MAC_addr, 6);
                    if (data_length > 10) {
                      memcpy(save_buffer + 6, uart_data + 10, (data_length - 10) > 22 ? 22 : (data_length - 10));
                    }
                    
                    uint16_t save_cfg_id = 0;
                    switch (peripheral_key_num) {
                      case 0:
                        save_cfg_id = CFG_BLE_KEY_PERIPHERAL_SEND_1;
                        printf("保存到位�?\n");
                        break;
                      case 1:
                        save_cfg_id = CFG_BLE_KEY_PERIPHERAL_SEND_2;
                        printf("保存到位�?\n");
                        break;
                      case 2:
                        save_cfg_id = CFG_BLE_KEY_PERIPHERAL_SEND_3;
                        printf("保存到位�?\n");
                        break;
                      case 3:
                        save_cfg_id = CFG_BLE_KEY_PERIPHERAL_SEND_4;
                        printf("保存到位�?\n");
                        break;
                      default:
                        printf("无效的key_num�? %d\n", peripheral_key_num);
                        break;
                    }
                    
                    if (save_cfg_id != 0) {
                      ret = syscfg_write(save_cfg_id, save_buffer, 28);
                      if (ret > 0) {
                        peripheral_key_num++;
                        syscfg_write(CFG_BLE_KEY_PERIPHERAL_USABLE, &peripheral_key_num, sizeof(peripheral_key_num));
                        printf("保存成功，当前已保存数量: %d\n", peripheral_key_num);
                        uart_send_ble_key_list(0x00FB);
                      } else {
                        printf("保存失败\n");
                        uart_send_ble_key_list(0x00FB);
                      }
                    }
                  }
                }
              } else {
                printf("没有外�?蓝牙钥匙空间\n");
              }
            } else if (key_type == 0x15) {
              uint8_t nfc_key_num = 0;
              int ret = syscfg_read(CFG_NFC_KEY_PERIPHERAL_USABLE_NUM, &nfc_key_num, sizeof(nfc_key_num));
              
              if (ret <= 0) {
                printf("读取CFG_NFC_KEY_PERIPHERAL_USABLE_NUM失败，使用默认�?\n");
                nfc_key_num = 0;
              }
              
              if (nfc_key_num > 3) {
                printf("�?测到无效的nfc_key_num�? %d，重�?��0\n", nfc_key_num);
                nfc_key_num = 0;
              }
              
              printf("当前�?��用的NFC钥匙保存空间:%d\n", nfc_key_num);
              uint8_t Uable_num = 3 - nfc_key_num;
              
              if (Uable_num > 0) {
                printf("�?测到�?��用的NFC钥匙保存空间，Uable_num:%d\n", Uable_num);
                
                uint8_t current_NFC_adders[16] = {0};
                int nfc_exists = 0;
                
                for (int i = 1; i <= 3; i++) {
                  uint16_t cfg_id = CFG_NFC_KEY_PERIPHERAL_SEND_1 + (i - 1);
                  syscfg_read(cfg_id, current_NFC_adders, 16);
                  if (!is_all_zero(current_NFC_adders, 6) && memcmp(current_NFC_adders, MAC_addr, 6) == 0) {
                    nfc_exists = 1;
                    printf("NFC地址已存在于位置%d\n", i);
                    break;
                  }
                }
                
                if (nfc_exists) {
                  printf("NFC地址已存�?��不保存\n");
                } else {
                  // �?�?AC地址�?��全为0
                  if (is_all_zero(MAC_addr, 6)) {
                    printf("NFC地址全为0，不保存\n");
                  } else {
                    uint8_t save_buffer[16] = {0};
                    memcpy(save_buffer, MAC_addr, 6);
                    if (data_length > 10) {
                      memcpy(save_buffer + 6, uart_data + 10, (data_length - 10) > 10 ? 10 : (data_length - 10));
                    }
                    
                    uint16_t save_cfg_id = 0;
                    switch (nfc_key_num) {       
                      case 0:
                        save_cfg_id = CFG_NFC_KEY_PERIPHERAL_SEND_1;
                        printf("保存到位�?\n");
                        break;
                      case 1:
                        save_cfg_id = CFG_NFC_KEY_PERIPHERAL_SEND_2;
                        printf("保存到位�?\n");
                        break;
                      case 2:
                        save_cfg_id = CFG_NFC_KEY_PERIPHERAL_SEND_3;
                        printf("保存到位�?\n");
                        break;
                      default:
                        printf("无效的nfc_key_num�? %d\n", nfc_key_num);
                        break;
                    }
                    
                    if (save_cfg_id != 0) {
                      ret = syscfg_write(save_cfg_id, save_buffer, 16);
                      if (ret > 0) {
                        nfc_key_num++;
                        syscfg_write(CFG_NFC_KEY_PERIPHERAL_USABLE_NUM, &nfc_key_num, sizeof(nfc_key_num));
                        printf("保存成功，当前已保存数量: %d\n", nfc_key_num);
                        uart_send_ble_key_list(0x00FB);
                      } else {
                        printf("保存失败\n");
                      }
                    }
                  }
                }
              } else {
                printf("没有NFC钥匙空间\n");
              }
            } else if (key_type == 0x01) {
              printf("手机蓝牙钥匙 - 触发配�?流程\n");
              
              uint8_t phone_ble_key_usable_num = 0;
              int ret = syscfg_read(CFG_PHONE_BLE_KEY_USABLE_NUM, &phone_ble_key_usable_num, sizeof(phone_ble_key_usable_num));
              
              if (ret <= 0) {
                printf("读取CFG_PHONE_BLE_KEY_USABLE_NUM失败，使用默认�?\n");
                phone_ble_key_usable_num = 0;
              }
              
              if (phone_ble_key_usable_num > 3) {
                printf("�?测到无效的phone_ble_key_usable_num�? %d，重�?��0\n", phone_ble_key_usable_num);
                phone_ble_key_usable_num = 0;
              }
              
              printf("当前�?��用的手机蓝牙钥匙保存空间:%d\n", phone_ble_key_usable_num);
              uint8_t Uable_num = 3 - phone_ble_key_usable_num;
              
              if (Uable_num > 0) {
                printf("�?测到�?��用的手机蓝牙钥匙保存空间，Uable_num:%d\n", Uable_num);
                
                // �?�?AC地址�?��全为0
                if (is_all_zero(MAC_addr, 6)) {
                  printf("MAC地址全为0，将在配对成功后获取并保存手机的MAC地址\n");
                }
                
                uint32_t passkey = 0;
                uint8_t valid_passkey = 0;
                
                printf("原�?password: %02X %02X %02X %02X\n", password[0], password[1], password[2], password[3]);
                
                if (password[0] == 0 && password[1] == 0 && password[2] == 0 && password[3] == 0) {
                  printf("password全为0，使用随机生成\n");
                  valid_passkey = 0;
                } else {
                  uint8_t is_bcd = 1;
                  for (int i = 0; i < 4; i++) {
                    if ((password[i] & 0xF0) > 0x90 || (password[i] & 0x0F) > 0x09) {
                      is_bcd = 0;
                      break;
                    }
                  }
                  
                  if (is_bcd) {
                    passkey = (uint32_t)((password[0] >> 4) * 100000 + (password[0] & 0x0F) * 10000 +
                                        (password[1] >> 4) * 1000 + (password[1] & 0x0F) * 100 +
                                        (password[2] >> 4) * 10 + (password[2] & 0x0F));
                    printf("BCD解码配�?�? %06u\n", passkey);
                    valid_passkey = 1;
                  } else {
                    passkey = ((uint32_t)password[0] << 24) | ((uint32_t)password[1] << 16) | 
                               ((uint32_t)password[2] << 8) | (uint32_t)password[3];
                    if (passkey > 0 && passkey <= 999999) {
                      printf("直接使用配�?�? %06u\n", passkey);
                      valid_passkey = 1;
                    } else {
                      printf("无效的passkey�? %u，使用随机生成\n", passkey);
                      valid_passkey = 0;
                    }
                  }
                }
                
                if (valid_passkey) {
                  printf("设置配�?passkey: %06u\n", passkey);
                  ble_pairing_set_uart_passkey(passkey);
                  fill_protocol_set_uart_passkey(passkey);
                  
                  printf("触发配�?流程\n");
                  app_send_message(APP_MSG_USER_PARING_INFORMATION, 0x40000000 | passkey);
                } else {
                  printf("使用随机生成配�?码\n");
                  app_send_message(APP_MSG_USER_PARING_INFORMATION, 0);
                }
              } else {
                printf("没有手机蓝牙钥匙空间\n");
              }
            } else if (key_type == 0x04) {
              printf("密码钥匙暂不�?��添加\n");
            }
          }
          /* phone key list should be sent after pair success callback */
          
        }
        
        if (uart_protocol_id == 0x00F9)
        {
          // 删除蓝牙钥匙 - 先分析钥匙类型，然后根据不同的钥匙类型去删除
          // 数据格式：[设�?类型(4字节)][MAC地址(6字节)][匹配密码(4字节)]
          printf("[UART] PID_BLE_DELETE_BLUETOOTH_KEY: 删除蓝牙钥匙\n");
          if (data_length >= 14) {
            uint8_t device_type[4];
            memcpy(device_type, uart_data, 4);
            uint8_t MAC_addr[6];
            memcpy(MAC_addr, uart_data + 4, 6);
            uint8_t password[4];
            memcpy(password, uart_data + 10, 4);
            
            printf("设�?类型: %02X %02X %02X %02X, MAC地址:%02X:%02X:%02X:%02X:%02X:%02X, 密码:%02X %02X %02X %02X\n",
                   device_type[0], device_type[1], device_type[2], device_type[3],
                   MAC_addr[0], MAC_addr[1], MAC_addr[2], MAC_addr[3], MAC_addr[4], MAC_addr[5],
                   password[0], password[1], password[2], password[3]);
            
            uint8_t key_type = device_type[3];
            
            switch (key_type) {
              case 0x01:
                printf("手机蓝牙钥匙\n");
                break;
              case 0x02:
              case 0xFE:
                printf("外�?蓝牙钥匙\n");
                break;
              case 0x03:
                printf("NFC钥匙\n");
                break;
              case 0x04:
                printf("密码钥匙\n");
                break;
              default:
                printf("�?��钥匙类型: 0x%02X\n", key_type);
                break;
            }
            
            if (key_type == 0x02 || key_type == 0xFE) {
              uint8_t current_key_num = 0;
              syscfg_read(CFG_BLE_KEY_PERIPHERAL_USABLE, &current_key_num, sizeof(current_key_num));
              
              if (current_key_num > 0) {
                uint8_t current_MAC_adders[6] = {0};
                uint8_t delet_buffer[28] = {0};
                uint8_t deleted = 0;
                
                for (int i = 1; i <= 4; i++) {
                  uint16_t cfg_id = CFG_BLE_KEY_PERIPHERAL_SEND_1 + (i - 1);
                  syscfg_read(cfg_id, current_MAC_adders, 6);
                  
                  if (memcmp(current_MAC_adders, MAC_addr, 6) == 0) {
                    syscfg_write(cfg_id, delet_buffer, sizeof(delet_buffer));
                    printf("删除�?d�??设蓝牙钥匙\n", i);
                    current_key_num--;
                    deleted = 1;
                    break;
                  }
                }
                
                if (deleted) {
                  syscfg_write(CFG_BLE_KEY_PERIPHERAL_USABLE, &current_key_num, sizeof(current_key_num));
                  printf("删除成功，当前已保存数量: %d\n", current_key_num);
                  uart_send_ble_key_list(0x00FB);
                } else {
                  printf("�?��到匹配的MAC地址\n");
                  uart_send_ble_key_list(0x00FB); // 即使没找到匹配的，也回�?当前列表，帮助MCU同�?状�??
                }
              } else {
                printf("当前没有外�?蓝牙钥匙\n");
                uart_send_ble_key_list(0x00FB); // 即使没有外�?蓝牙钥匙，也回�?当前列表，帮助MCU同�?状�??
              }
            } else if (key_type == 0x03) {
              uint8_t current_key_num = 0;
              syscfg_read(CFG_NFC_KEY_PERIPHERAL_USABLE_NUM, &current_key_num, sizeof(current_key_num));
              
              if (current_key_num > 0) {
                uint8_t current_NFC_adders[16] = {0};
                uint8_t delet_buffer[16] = {0};
                uint8_t deleted = 0;
                
                for (int i = 1; i <= 3; i++) {
                  uint16_t cfg_id = CFG_NFC_KEY_PERIPHERAL_SEND_1 + (i - 1);
                  syscfg_read(cfg_id, current_NFC_adders, 16);
                  
                  if (memcmp(current_NFC_adders, MAC_addr, 6) == 0) {
                    syscfg_write(cfg_id, delet_buffer, sizeof(delet_buffer));
                    printf("删除�?d个NFC钥匙\n", i);
                    current_key_num--;
                    deleted = 1;
                    break;
                  }
                }
                
                if (deleted) {
                  syscfg_write(CFG_NFC_KEY_PERIPHERAL_USABLE_NUM, &current_key_num, sizeof(current_key_num));
                  printf("删除成功，当前已保存数量: %d\n", current_key_num);
                  uart_send_ble_key_list(0x00FB);
                } else {
                  printf("�?��到匹配的NFC地址\n");
                }
              } else {
                printf("当前没有NFC钥匙\n");
              }
            } else if (key_type == 0x01) {
              printf("手机蓝牙钥匙 - 执�?删除操作\n");
              
              uint8_t current_key_num = 0;
              int ret = syscfg_read(CFG_PHONE_BLE_KEY_USABLE_NUM, &current_key_num, sizeof(current_key_num));
              
              if (ret <= 0) {
                printf("读取CFG_PHONE_BLE_KEY_USABLE_NUM失败，使用默认�?\n");
                current_key_num = 0;
              }
              
              if (current_key_num > 0) {
                uint8_t current_phone_ble_record[9] = {0};
                uint8_t delet_buffer[9] = {0};
                uint8_t deleted = 0;
                
                uint32_t config_ids[] = {
                  CFG_PHONE_BLE_KEY_DELETE_ID_1,
                  CFG_PHONE_BLE_KEY_DELETE_ID_2,
                  CFG_PHONE_BLE_KEY_DELETE_ID_3
                };
                
                for (int i = 0; i < 3; i++) {
                  syscfg_read(config_ids[i], current_phone_ble_record, sizeof(current_phone_ble_record));
                  
                  if (!is_all_zero(current_phone_ble_record, sizeof(current_phone_ble_record)) && memcmp(current_phone_ble_record + 3, MAC_addr, 6) == 0) {
                    syscfg_write(config_ids[i], delet_buffer, sizeof(delet_buffer));
                    printf("删除�?d�?��机蓝牙钥匙\n", i + 1);
                    current_key_num--;
                    deleted = 1;
                    break;
                  }
                }
                
                if (deleted) {
                  syscfg_write(CFG_PHONE_BLE_KEY_USABLE_NUM, &current_key_num, sizeof(current_key_num));
                  printf("删除成功，当前已保存数量: %d\n", current_key_num);
                  
                  // 取消配�?操作
                  {
                    bool unpair_ret = ble_list_delete_device(MAC_addr, 0);
                    printf("取消与�?设�?的配�? ret=%d, MAC=%02X:%02X:%02X:%02X:%02X:%02X\\n",
                           unpair_ret,
                           MAC_addr[0], MAC_addr[1], MAC_addr[2],
                           MAC_addr[3], MAC_addr[4], MAC_addr[5]);
                  }
                  
                  
                  // 给MCU回�?钥匙列表
                  uart_send_ble_key_list(0x00FB);
                } else {
                  printf("�?��到匹配的手机蓝牙钥匙\n");
                }
              } else {
                printf("当前没有手机蓝牙钥匙\n");
              }
            } else if (key_type == 0x04) {
              printf("密码钥匙暂不�?��删除\n");
            }
          }
        }
        
        if (uart_protocol_id == 0x00FA)
        {
          // 清空蓝牙钥匙 - 先分析钥匙类型，然后根据不同的钥匙类型去清空
          // 数据格式：[设�?类型(4字节)]
          printf("[UART] PID_BLE_CLEARALL_BLUTOOTH_KEY: 清空蓝牙钥匙\n");
          if (data_length >= 4) {
            uint8_t device_type[4];
            memcpy(device_type, uart_data, 4);
            
            printf("设�?类型: %02X %02X %02X %02X\n",
                   device_type[0], device_type[1], device_type[2], device_type[3]);
            
            uint8_t key_type = device_type[3];
            
            switch (key_type) {
              case 0x01:
                printf("手机蓝牙钥匙\n");
                break;
              case 0x02:
              case 0xFE:
                printf("外�?蓝牙钥匙\n");
                break;
              case 0x03:
                printf("NFC钥匙\n");
                break;
              case 0x04:
                printf("密码钥匙\n");
                break;
              default:
                printf("�?��钥匙类型: 0x%02X\n", key_type);
                break;
            }
            
            if (key_type == 0x02 || key_type == 0xFE) {
              uint8_t empty_buffer[28] = {0};
              syscfg_write(CFG_BLE_KEY_PERIPHERAL_SEND_1, empty_buffer, sizeof(empty_buffer));
              syscfg_write(CFG_BLE_KEY_PERIPHERAL_SEND_2, empty_buffer, sizeof(empty_buffer));
              syscfg_write(CFG_BLE_KEY_PERIPHERAL_SEND_3, empty_buffer, sizeof(empty_buffer));
              syscfg_write(CFG_BLE_KEY_PERIPHERAL_SEND_4, empty_buffer, sizeof(empty_buffer));
              
              uint8_t state_flag = 0x00;
              syscfg_write(CFG_BLE_KEY_PERIPHERAL_USABLE, &state_flag, sizeof(state_flag));
              printf("清空�?有�?设蓝牙钥匙成功\n");
              uart_send_ble_key_list(0x00FB);
            } else if (key_type == 0x03) {
              uint8_t empty_buffer[16] = {0};
              syscfg_write(CFG_NFC_KEY_PERIPHERAL_SEND_1, empty_buffer, sizeof(empty_buffer));
              syscfg_write(CFG_NFC_KEY_PERIPHERAL_SEND_2, empty_buffer, sizeof(empty_buffer));
              syscfg_write(CFG_NFC_KEY_PERIPHERAL_SEND_3, empty_buffer, sizeof(empty_buffer));
              
              uint8_t state_flag = 0x00;
              syscfg_write(CFG_NFC_KEY_PERIPHERAL_USABLE_NUM, &state_flag, sizeof(state_flag));
              printf("清空�?有NFC钥匙成功\n");
              uart_send_ble_key_list(0x00FB);
            } else if (key_type == 0x01) {
              printf("手机蓝牙钥匙暂不�?��清空\n");
            } else if (key_type == 0x04) {
              printf("密码钥匙暂不�?��清空\n");
            }
          }
        }
        
        if (uart_protocol_id == 0x00FB)
        {
          // 获取�?有蓝牙钥�?- 基于send_ble_key_list逻辑
          printf("[UART] PID_BLE_GETALL_BLUTOOTH_KEY: 获取�?有蓝牙钥匙\n");
          uart_send_ble_key_list(0x00FB);
        }
        
        if (uart_protocol_id == 0x00FC)
        {
          // 获取密码钥匙
          printf("[UART] PID_GET_PASSWORD_KEY: 获取密码钥匙\n");
          
          // 保存接收到的密码数据到配�?��
          if (uart_data != NULL && data_length >= 4) {
            // 密码数据在最后的4字节
            uint8_t password_data[4] = {0};
            int start_idx = data_length - 4;
            memcpy(password_data, &uart_data[start_idx], 4);
            
            printf("保存密码钥匙: %02X%02X%02X%02X\n", password_data[0], password_data[1], password_data[2], password_data[3]);
            syscfg_write(CFG_VEHICLE_PASSWORD_UNLOCK, password_data, 4);
          }
          
          uart_send_password_key_list(0x00FC);
        }
        /*OTA 逻辑*/
        if (uart_protocol_id == 0x55AA)
        {
          int ota_ret = dual_ota_app_data_deal(uart_protocol_id, uart_data, data_length);
          if (ota_ret) {
            printf("[UART][OTA] dual_ota_app_data_deal ret=%d, len=%d\n", ota_ret, data_length);
          }
        }
        
        if (uart_protocol_id == 0x00A1)
        {
          /* 查看�??�音效传输报�?*/
          printf("[UART] Received cmd 0x00A1: Query transfer report\n");
          
          // 读取报告文件
          const char *report_path = "mnt/sdfile/app/uwav/transfer_report.txt";
          FILE *fp = fopen(report_path, "r");
          
          if (!fp) {
            // 报告文件不存�?��提供更�?细的诊断信息
            printf("[UART] Report file not found: %s\n", report_path);
            
            // 先尝试创�?uwav �?��
            int mkdir_ret = fmk_dir("mnt/sdfile/app", "/uwav", 0);
            if (mkdir_ret == 0) {
              printf("[UART] uwav directory created successfully\n");
            } else if (mkdir_ret == -1) {
              printf("[UART] uwav directory already exists\n");
            } else {
              printf("[UART] mkdir uwav ret=%d\n", mkdir_ret);
            }
            
            // �?查是否有传输正在进�?
            extern uint8_t is_file_transfer_started;
            extern uint32_t file_write_offset;
            extern uint32_t total_file_size;
            
            uint8_t status_buf[256];
            int status_len;
            
            if (is_file_transfer_started) {
              // 传输正在进�?�?
              status_len = snprintf((char*)status_buf, sizeof(status_buf),
                "传输进�?�? %u/%u bytes (%.1f%%)\n"
                "请等待传输完成后再查询报告�?�\n",
                file_write_offset, total_file_size,
                total_file_size ? (file_write_offset * 100.0f / total_file_size) : 0.0f);
              UART1_IO_LOG("[UART] Transfer in progress: %u/%u bytes\n", file_write_offset, total_file_size);
            } else {
              // 没有传输记录
              status_len = snprintf((char*)status_buf, sizeof(status_buf),
                "�?��到传输报告文件�?�\n"
                "请先通过APP传输�??�音效，传输完成后会�?��生成报告。\n\n"
                "�?��原因：\n"
                "1. 尚未进�?过个性音效传输\n"
                "2. 上�?传输�?��成或�?��\n"
                "3. 上�?传输失败\n\n"
                "提示：�?使用APP重新传输音效文件。\n");
              printf("[UART] No transfer history found\n");
            }
            
            uart1_send_toMCU(0x00A1, status_buf, status_len);
          } else {
            // 读取整个文件内�?并分段发�?
            uint8_t read_buf[256];
            uint8_t send_buf[UART_MAX_DATA_LEN - 50]; // 留一些余�?
            uint16_t buf_pos = 0;
            int read_len;
            
            UART1_IO_LOG("[UART] Reading transfer report...\n");
            
            // �?��读取文件内�?
            while ((read_len = fread(fp, read_buf, sizeof(read_buf))) > 0) {
              // 逐字节�?理，遇到换�?或缓冲区满时发�?
              for (int i = 0; i < read_len; i++) {
                send_buf[buf_pos++] = read_buf[i];
                
                // 如果缓冲区快满了，先发�?
                if (buf_pos >= sizeof(send_buf) - 1) {
                  uart1_send_toMCU(0x00A1, send_buf, buf_pos);
                  os_time_dly(2); // �?��延时避免数据拥�?
                  buf_pos = 0;
                }
              }
            }
            
            // 发�?�剩余数�?
            if (buf_pos > 0) {
              uart1_send_toMCU(0x00A1, send_buf, buf_pos);
            }
            
            fclose(fp);
            UART1_IO_LOG("[UART] Transfer report sent successfully\n");
          }
        }
        if (uart_protocol_id == 0x00A2)
        {
          /* 从音频资源区�?��音�? - 使用 tone_table 索引 */
          printf("[UART] Received cmd 0x00A2: Play tone by index\n");
          
          // 协�?数据格式�?
          // [tone_index 1字节 �?2字节(大�?)] 对应 tone_table 的索�?
          // 例�?�?
          //   0x00 -> �?�� IDEX_TONE_USER_001 (tone0.*)
          //   0x13 -> �?�� tone19.*
          //   122  -> �?�� IDEX_TONE_USER_123 (test11111.*)
          
          uint8_t tone_index = IDEX_TONE_USER_001; // 默�?索引（tone0.*�?
          
          if (uart_data != NULL && data_length > 0) {
            // �?�� 1字节 �?2字节(BCD格式) 索引
            uint16_t raw_value = 0;
            if (data_length >= 2) {
                // BCD解析：每�?��节的�?位和�?位分�?��表十位和�?��
                // 例�? 0x01 0x11 -> 1*100 + 1*10 + 1 = 111
                uint8_t bcd1 = uart_data[0];
                uint8_t bcd2 = uart_data[1];
                raw_value = ((bcd1 >> 4) * 10 + (bcd1 & 0x0F)) * 100 +
                           ((bcd2 >> 4) * 10 + (bcd2 & 0x0F));
            } else {
                raw_value = uart_data[0];
            }
            
            // 协�?值直接表示音频编号：0 -> tone0, 19 -> tone19
            if (raw_value <= 122) {
              tone_index = IDEX_TONE_USER_001 + raw_value;
              printf("[UART] Raw value: %u -> tone_table index: %u\n", raw_value, tone_index);
            } else {
              printf("[UART] Invalid tone index %u (max 122), using default\n", raw_value);
            }
          } else {
            printf("[UART] No data, using default tone index\n");
          }
          
          // 使用 tone_play_by_path 直接�?��，因�?tone_index 数组不完�?
          extern int tone_play_by_path(const char *name, u8 preemption);
          extern const char *tone_table[];
          extern const int tone_table_size;
          
          // �?tone_table 获取实际文件�?��
          const char *tone_path = NULL;
          if (tone_index < tone_table_size) {
            tone_path = tone_table[tone_index];
          }
          if (!tone_path) {
            printf("[UART] ERROR: tone_table index %u invalid or NULL (size=%d)\n", tone_index, tone_table_size);
            goto tone_play_err;
          }
          printf("[UART] Attempting to play: tone_table[%u] = %s\n", tone_index, tone_path);
          
          // �?查文件是否存�?
          void *test_fp = fopen(tone_path, "r");
          if (test_fp) {
            fclose(test_fp);
            printf("[UART] File exists: %s\n", tone_path);
          } else {
            printf("[UART] ERROR: File NOT found: %s\n", tone_path);
            printf("[UART] This is the root cause! Audio file missing from firmware!\n");
            goto tone_play_err;
          }
          
          // 直接使用�?���?��
          int ret = tone_play_by_path(tone_path, 1); // preemption=1 优先�?��
          
          // 发�?�结果给 MCU
          uint8_t resp[2];
          resp[0] = (ret == 0) ? 0x01 : 0x00; // 0x01 = 成功, 0x00 = 失败
          resp[1] = tone_index;
          uart1_send_toMCU(0x00A2, resp, 2);
          
          if (ret == 0) {
            printf("[UART] Playing tone_table[%u] successfully\n", tone_index);
          } else {
            printf("[UART] Playing tone_table[%u] failed: ret=%d\n", tone_index, ret);
tone_play_err:
            // �?��失败时直接返回，避免对无效路径继�?��码�?致异�?
            resp[0] = 0x00;
            uart1_send_toMCU(0x00A2, resp, 2);
          }
        }
        if (uart_protocol_id == 0x00A3)
        {
          /* �?���?��义音效槽位（已传输的�??�音效） */
          printf("[UART] Received cmd 0x00A3: Play custom tone from slot\n");
          
          // 协�?数据格式�?
          // [tone_type 1字节] (0~3，�?应自定义音效槽位 uwtg0~UWTG3)
          // 如果没有数据，默认播放槽�?
          
          uint8_t tone_type = 0;
          if (uart_data != NULL && data_length > 0) {
            tone_type = uart_data[0];
            if (tone_type > 3) {
              tone_type = 0; // 超出范围，使用槽�?
            }
          }
          
          printf("[UART] Playing custom tone from slot: %u\n", tone_type);
          
          // 调用�?��义音效播放函�?
          // 该函数会�?�?meta 信息，确认音效是否已传输
          bool ret = user_custom_tone_play_if_exist(tone_type, 1); // preemption=1 优先�?��
          
          // 发�?�结果给 MCU
          uint8_t resp[2];
          resp[0] = ret ? 0x01 : 0x00; // 0x01 = 成功�?��, 0x00 = 音效不存在或�?��失败
          resp[1] = tone_type;
          uart1_send_toMCU(0x00A3, resp, 2);
          
          if (ret) {
            printf("[UART] Custom tone slot %u played successfully\n", tone_type);
          } else {
            printf("[UART] Custom tone slot %u not found or play failed\n", tone_type);
          }
        }
        if (uart_protocol_id == 0x00A4)
        {
          /* 查�?�?有自定义音效槽位状�?*/
          printf("[UART] Received cmd 0x00A4: Query custom tone slots status\n");
          
          // 返回格式�?
          // [slot0_status] [slot0_size_hi] [slot0_size_lo]
          // [slot1_status] [slot1_size_hi] [slot1_size_lo]
          // [slot2_status] [slot2_size_hi] [slot2_size_lo]
          // [slot3_status] [slot3_size_hi] [slot3_size_lo]
          // status: 0x00=�? 0x01=有效
          
          uint8_t resp[16]; // 4�?���?* 4字节(状�?大小3字节)
          memset(resp, 0, sizeof(resp));
          
          printf("\n========== CUSTOM TONE SLOTS STATUS ==========\n");
          
          for (uint8_t slot = 0; slot < 4; slot++) {
            custom_tone_meta_t meta;
            memset(&meta, 0, sizeof(meta));
            
            int cfg_id = custom_tone_meta_cfg_id(slot);
            int rlen = syscfg_read(cfg_id, &meta, sizeof(meta));
            
            const char *path = custom_tone_slot_path(slot);
            
            if (rlen == (int)sizeof(meta) && 
                meta.magic == CUSTOM_TONE_META_MAGIC && 
                meta.tone_type == slot &&
                meta.file_size > 0) {
              // 槽位有效
              resp[slot * 4] = 0x01;  // status = 有效
              resp[slot * 4 + 1] = (uint8_t)(meta.file_size >> 16);  // size 高字�?
              resp[slot * 4 + 2] = (uint8_t)(meta.file_size >> 8);   // size �?���?
              resp[slot * 4 + 3] = (uint8_t)(meta.file_size & 0xFF); // size 低字�?
              
              printf("[SLOT %u] VALID - path=%s, size=%u, crc=0x%08X\n", 
                     slot, path ? path : "null", 
                     (unsigned)meta.file_size, (unsigned)meta.crc32);
            } else {
              // 槽位空或无效
              resp[slot * 4] = 0x00;  // status = �?
              resp[slot * 4 + 1] = 0;
              resp[slot * 4 + 2] = 0;
              resp[slot * 4 + 3] = 0;
              
              printf("[SLOT %u] EMPTY - path=%s\n", slot, path ? path : "null");
            }
          }
          
          printf("================================================\n\n");
          
          // 发�?�结果给 MCU
          uart1_send_toMCU(0x00A4, resp, 16);
          printf("[UART] Slots status sent\n");
        }

        }

        parse_off += frame_len;
      }
    } else {
      /* �??到数�?���?��让出调度，避免长时间阻�?导致 timer_no_response */
      os_time_dly(1);
    }
  }

  // �?��延时，避免过度占用CPU
  os_time_dly(10); // 延时10ms
}

void uart1_init(void) {
  static bool uart1_inited = false;
  if (uart1_inited) {
    return;
  }
  uart1_inited = true;
  int err = task_create(uart1_sync_demo, NULL, "usart0com");
  if (err != OS_NO_ERR) {
    r_printf("creat fail %x\n", err);
  }
}

static s32 uart1_send_raw_nonblock(const u8 *data, u16 len) {
  s32 ret = uart_send_bytes_nonblock(uart1_dev, data, len);
  if (ret == UART_ERROR_BUSY) {
    return UART_ERROR_BUSY;
  }
  if (ret != len) {
    printf("uart1 nonblock send error: %d, len: %d\n", ret, len);
  }
  return ret;
}

static void uart1_pending_tx_try(void) {
  if (g_uart1_tx_queue_count == 0) {
    return;
  }
  if (uart_is_send_complete(uart1_dev) != 1) {
    return;
  }

  s32 ret = uart1_send_raw_nonblock(g_uart1_tx_queue_buf[g_uart1_tx_queue_head],
                                    g_uart1_tx_queue_len[g_uart1_tx_queue_head]);
  if (ret == g_uart1_tx_queue_len[g_uart1_tx_queue_head]) {
    g_uart1_tx_queue_head = (g_uart1_tx_queue_head + 1) % UART1_TX_QUEUE_DEPTH;
    g_uart1_tx_queue_count--;
  }
}

static bool uart1_tx_queue_push(const u8 *data, u16 len)
{
  if (!data || len == 0 || len > UART1_TX_QUEUE_BUF_SIZE) {
    return false;
  }

  OS_ENTER_CRITICAL();
  if (g_uart1_tx_queue_count >= UART1_TX_QUEUE_DEPTH) {
    OS_EXIT_CRITICAL();
    return false;
  }

  memcpy(g_uart1_tx_queue_buf[g_uart1_tx_queue_tail], data, len);
  g_uart1_tx_queue_len[g_uart1_tx_queue_tail] = len;
  g_uart1_tx_queue_tail = (g_uart1_tx_queue_tail + 1) % UART1_TX_QUEUE_DEPTH;
  g_uart1_tx_queue_count++;
  OS_EXIT_CRITICAL();
  return true;
}

void uart1_send(u8 *data, u16 len) {
  s32 ret = uart1_send_raw_nonblock(data, len);
  if (ret != len) {
    return;
  }
  UART1_IO_LOG("uart1 send successfully: %d\n", len);
}



void uart_send_ble_key_list(uint16_t protocol_id) {
  printf("[UART] uart_send_ble_key_list: 发�?�蓝牙钥匙列表\n");

  static uint8_t key_list_buffer[269];
  memset(key_list_buffer, 0, sizeof(key_list_buffer));
  uint16_t offset = 0;

  uint8_t phone_ble_key_usable_num = 0;
  uint8_t ble_key_peripheral_usable_num = 0;
  uint8_t ble_key_peripheral_actual_num = 0;
  uint8_t ble_peripheral_valid[4] = {0};
  uint8_t ble_peripheral_slots[4][28] = {0};

  syscfg_read(CFG_PHONE_BLE_KEY_USABLE_NUM, &phone_ble_key_usable_num, 1);
  syscfg_read(CFG_BLE_KEY_PERIPHERAL_USABLE, &ble_key_peripheral_usable_num, 1);

  for (uint8_t i = 0; i < 4; i++) {
    int ret = syscfg_read(CFG_BLE_KEY_PERIPHERAL_SEND_1 + i, ble_peripheral_slots[i], 28);
    if (ret > 0 && !is_all_zero(ble_peripheral_slots[i], 6) && !is_all_ff(ble_peripheral_slots[i], 6)) {
      ble_peripheral_valid[i] = 1;
      ble_key_peripheral_actual_num++;
    }
  }

  printf("phone_ble_key_usable_num: %d\n", phone_ble_key_usable_num);
  printf("ble_key_peripheral_usable_num: %d\n", ble_key_peripheral_usable_num);
  printf("ble_key_peripheral_actual_num: %d\n", ble_key_peripheral_actual_num);

  uint8_t total_key_num = phone_ble_key_usable_num + ble_key_peripheral_actual_num;
  if (total_key_num > 5) {
    total_key_num = 5;
    printf("总钥匙串数量超过5，限制为5\n");
  }

  key_list_buffer[offset++] = total_key_num;
  printf("总钥匙串数量: %d\n", total_key_num);

  uint32_t config_ids[] = {
      CFG_PHONE_BLE_KEY_DELETE_ID_1,
      CFG_PHONE_BLE_KEY_DELETE_ID_2,
      CFG_PHONE_BLE_KEY_DELETE_ID_3,
  };
  const uint16_t slot_cnt = sizeof(config_ids) / sizeof(config_ids[0]);
  uint8_t phone_ble_key_data[9] = {0};

  for (uint16_t i = 0; i < slot_cnt; i++) {
    memset(phone_ble_key_data, 0, sizeof(phone_ble_key_data));
    int ret = syscfg_read(config_ids[i], phone_ble_key_data, sizeof(phone_ble_key_data));
    if (ret > 0 && !is_all_zero(phone_ble_key_data, sizeof(phone_ble_key_data))) {
      if (offset + 14 > sizeof(key_list_buffer)) {
        goto key_list_send;
      }

      key_list_buffer[offset++] = 0x00;
      key_list_buffer[offset++] = 0x00;
      key_list_buffer[offset++] = 0x00;
      key_list_buffer[offset++] = 0x01;
      memcpy(key_list_buffer + offset, phone_ble_key_data + 3, 6);
      offset += 6;
      memcpy(key_list_buffer + offset, phone_ble_key_data, 3);
      offset += 3;
      key_list_buffer[offset++] = 0x00;

      printf("手机蓝牙钥匙 %d: 设�?类型=0x00000001, MAC=%02X:%02X:%02X:%02X:%02X:%02X, 密码=%02X%02X%02X\n",
             (int)(i + 1),
             phone_ble_key_data[3], phone_ble_key_data[4], phone_ble_key_data[5],
             phone_ble_key_data[6], phone_ble_key_data[7], phone_ble_key_data[8],
             phone_ble_key_data[0], phone_ble_key_data[1], phone_ble_key_data[2]);
    }
  }

  for (uint8_t i = 0; i < 4; i++) {
    if (!ble_peripheral_valid[i]) {
      continue;
    }
    if (offset + 14 > sizeof(key_list_buffer)) {
      goto key_list_send;
    }

    key_list_buffer[offset++] = 0x00;
    key_list_buffer[offset++] = 0x00;
    key_list_buffer[offset++] = 0x00;
    key_list_buffer[offset++] = 0x02;
    memcpy(key_list_buffer + offset, ble_peripheral_slots[i], 6);
    offset += 6;
    memcpy(key_list_buffer + offset, ble_peripheral_slots[i] + 6, 4);
    offset += 4;

    printf("外�?蓝牙钥匙 %d: 设�?类型=0x00000002, MAC=%02X:%02X:%02X:%02X:%02X:%02X, 密码=%02X%02X%02X%02X\n",
           i + 1,
           ble_peripheral_slots[i][0], ble_peripheral_slots[i][1],
           ble_peripheral_slots[i][2], ble_peripheral_slots[i][3],
           ble_peripheral_slots[i][4], ble_peripheral_slots[i][5],
           ble_peripheral_slots[i][6], ble_peripheral_slots[i][7],
           ble_peripheral_slots[i][8], ble_peripheral_slots[i][9]);
  }

  printf("Total key list data length: %d bytes\n", offset);

key_list_send:
  uart1_send_toMCU(protocol_id, key_list_buffer, offset);
}

static void uart_send_password_key_list(uint16_t protocol_id) {
  printf("[UART] uart_send_password_key_list: 发�?�密码钥匙列表\n");
  
  static uint8_t password_key_buffer[269];
  memset(password_key_buffer, 0, sizeof(password_key_buffer));
  uint16_t offset = 0;
  
  uint8_t password_data[4] = {0};
  int ret = syscfg_read(CFG_VEHICLE_PASSWORD_UNLOCK, password_data, 4);
  
  if (ret > 0 && !is_all_zero(password_data, 4)) {
    if (offset + 14 > sizeof(password_key_buffer)) {
      goto password_key_send;
    }
    
    password_key_buffer[offset++] = 0x00;
    password_key_buffer[offset++] = 0x00;
    password_key_buffer[offset++] = 0x00;
    password_key_buffer[offset++] = 0x04;
    memset(password_key_buffer + offset, 0, 6);
    offset += 6;
    memcpy(password_key_buffer + offset, password_data, 4);
    offset += 4;
    
    printf("密码钥匙: 设�?类型=0x00000004, MAC=00:00:00:00:00:00, 密码=%02X%02X%02X%02X\n",
           password_data[0], password_data[1], password_data[2], password_data[3]);
  } else {
    printf("�??�?��码钥匙\n");
  }
  
  printf("Total password key data length: %d bytes\n", offset);
  
password_key_send:
  uart1_send_toMCU(protocol_id, password_key_buffer, offset);
}

//==============================用于串口数据发�?====================================
uint8_t send_buff_toMCU[269]; // 用于创建�?大的发�?�buff---------->
                              // 根据派电的协�?��大的数据包含269字节
uint16_t Uart_data_len = 0;    // 用于记录发�?�的数据长度
uint8_t crc[2] = {0};
uint8_t MAX_SEND_NUM = 3; // �?大发送�?�?
/**
 * @brief 用于将数�?��送到MCU
 *
 * @param protocol_id 协�?ID
 * @param data 数据指针
 * @param len 数据长度
 */
bool uart1_send_toMCU(uint16_t protocol_id, uint8_t *data, uint16_t len) {
  // 重�?：不要在这里清除 uart_data，因为它�?��来保�?MCU 回�?数据的！
  // �?��在接收到新数�?��才更�?uart_data
  // uart_data = NULL;  // �?删除：不应�?在发送时清空接收缓冲�?
  // data_length = 0;   // �?删除：同�?

  // 参数校验
  if (len > UART_MAX_DATA_LEN) {
    printf("uart1_send_toMCU: invalid len %u, max %d\n", len, UART_MAX_DATA_LEN);
    return false;
  }
  if (len > 0 && data == NULL) {
    printf("uart1_send_toMCU: data is NULL but len=%u\n", len);
    return false;
  }

  // 计算CRC�?验位，使用受限缓冲区以避免在栈上分配�?��长度数组
  uint16_t CRC_len = (uint16_t)(len + 4); // 协�?ID(2)+len(2)+data(len)
  static uint8_t crcbuffer[UART_MAX_DATA_LEN + 4];
  crcbuffer[0] = (uint8_t)(protocol_id >> 8);   // 协�?ID高字�?
  crcbuffer[1] = (uint8_t)(protocol_id & 0xFF); // 协�?ID低字�?
  crcbuffer[2] = (uint8_t)(len >> 8);           // len的高字节
  crcbuffer[3] = (uint8_t)(len & 0xFF);         // len的低字节
  if (len > 0) {
    memcpy(&crcbuffer[4], data, len); // 复制数据
  }

  UART1_IO_LOG("crcbuffer0:%02x,crcbuffer1:%02x,crcbuffer2:%02x,crcbuffer3:%02x\n",
               crcbuffer[0], crcbuffer[1], crcbuffer[2], crcbuffer[3]);
  // printf("CRC16:");
  calculateCRC16(crcbuffer, CRC_len, crc); // 计算CRC16校验�?
  // printf("0x%02X%02X\n", crc[1], crc[0]);

  // 组�?发�?�缓冲，先�?查整体长度以避免越界
  const uint16_t overhead = 2 /*head+sync*/ + 2 /*protocol id*/ + 2 /*len*/ + 2 /*crc*/ + 2 /*tail*/;
  if ((uint32_t)len + overhead > sizeof(send_buff_toMCU)) {
    printf("uart1_send_toMCU: payload too large %u, max %zu\n", len, sizeof(send_buff_toMCU) - overhead);
    return false;
  }

  Uart_data_len = 0; // 每�?发�?�前都�?将长度清0
  send_buff_toMCU[Uart_data_len++] = 0xFE; // 头字节以0xFE�?�?
  send_buff_toMCU[Uart_data_len++] = 0xAB; // 同�?字节0xAB
  // 协�?ID
  send_buff_toMCU[Uart_data_len++] = (uint8_t)(protocol_id >> 8);
  send_buff_toMCU[Uart_data_len++] = (uint8_t)(protocol_id & 0xFF);
  // 数据长度
  send_buff_toMCU[Uart_data_len++] = (uint8_t)(len >> 8);
  send_buff_toMCU[Uart_data_len++] = (uint8_t)(len & 0xFF);
  // 数据�?
  if (len > 0) {
    memcpy(&send_buff_toMCU[Uart_data_len], data, len);
    Uart_data_len += len; // 更新数据长度
  }
  // CRC (低字�? 高字�?
  send_buff_toMCU[Uart_data_len++] = crc[0];
  send_buff_toMCU[Uart_data_len++] = crc[1];
  // 尾字�?(0x0A 0x0D)
  send_buff_toMCU[Uart_data_len++] = 0x0A;
  send_buff_toMCU[Uart_data_len++] = 0x0D;

  // 发�?�数�?
  if (g_uart1_tx_queue_count > 0) {
    return uart1_tx_queue_push(send_buff_toMCU, Uart_data_len) ? true : false;
  }

  s32 ret = uart1_send_raw_nonblock(send_buff_toMCU, Uart_data_len);
  if (ret == Uart_data_len) {
    UART1_IO_LOG("uart1 send to MCU:");
    UART1_IO_DUMP(send_buff_toMCU, Uart_data_len);
    return true;
  }

  if (ret == UART_ERROR_BUSY) {
    return uart1_tx_queue_push(send_buff_toMCU, Uart_data_len) ? true : false;
  }

  printf("uart1_send_toMCU: send failed ret=%d\n", ret);
  return false;
}
/**
 * @brief 用于处理串口�?��事件
 *
 * @param uart_num 串口编号
 * @param event �?��事件
 */
static void uart_irq_func(uart_dev uart_num, enum uart_event_v1 event) {
  if (event & UART_EVENT_TX_DONE) {
    UART1_IO_LOG("uart[%d] tx done", uart_num);
    uart1_pending_tx_try();
  }

  if (event & UART_EVENT_RX_TIMEOUT) {
    // RX 超时事件：表示一帧数�?��收完成（帧间隔由 rx_timeout_thresh 决定）�?
    // 在这里仅�?��标志，避免在回调里做大量处理�?
    g_uart1_rx_frame_ready = 1;
  }

  if (event & UART_EVENT_RX_FIFO_OVF) {
    printf("uart[%d] rx fifo ovf", uart_num);

    // 溢出�?DMA/cbuf �?��错乱，主动重�?��避免后续�?直解析失败�?
    uart_dma_rx_reset(uart_num);
  }
}
void uart_rx_ptr_analysis(void) {
  uint8_t *ptr = uart_rx_ptr;
  while (ptr < (uint8_t *)uart_rx_ptr + r) {
    if (*ptr == 0xFE) {
      printf("find 0xFE at %p\n", ptr);
    }
    ptr++;
  }
}
/**
 * @brief 用于解析串口接收数据
 *
 * @param rx_data 接收数据指针
 * @param data_len 接收数据长度
 * @return true 解析成功
 * @return false 解析失败
 */
bool uart1_parse_packet(uint8_t *rx_data, uint16_t data_len) {
  // 指针变空NULL
  uart_data = NULL;
  uart_rx_data = NULL;
  
  // �?查开始�?
  if (rx_data[0] != 0xFE) {
    printf("Invalid start byte: 0x%02X, expected 0xFE\n", rx_data[0]);
    return false;
  }
  
  // ==================== �?��识别新旧协�?格式 ====================
  // 旧协�?��FE BA + 协�?ID(2B) + 长度(2B) + 数据 + CRC + 0A0D
  // 新协�?��FE + 总帧�?2B) + 顺序�?2B) + 长度(2B) + 协�?ID(2B) + 数据 + CRC + 0A0D
  // 判断依据：�?二个字节�?���?0xBA
  
  bool is_old_protocol = (rx_data[1] == 0xBA);
  
  if (is_old_protocol) {
    // ==================== 旧协�?��式解�?====================
    printf("[UART] Old protocol format detected\n");
    
    // �?小包长�?查：header(6) + crc(2) + tail(2) = 10字节
    if (data_len < 10) {
      printf("uart1_parse_packet: packet too short, len=%d\n", data_len);
      return false;
    }

    // 解析协�?ID (大�?�?
    uint16_t protocol_id = (rx_data[2] << 8) | rx_data[3];
    uart_protocol_id = protocol_id;

    // 解析数据长度 (大�?�?
    data_length = (rx_data[4] << 8) | rx_data[5];
    
    // 验证 data_length 边界
    if (data_length > UART_MAX_DATA_LEN) {
      printf("uart1_parse_packet: invalid data_length=%d (0x%04X), max=%d\n", 
             data_length, data_length, UART_MAX_DATA_LEN);
      return false;
    }

    // �?查整�?���?��完整：header(6) + data_length + crc(2) + tail(2)
    uint16_t expected_len = 6 + data_length + 4;
    if (data_len < expected_len) {
      printf("uart1_parse_packet: incomplete packet data_len=%d expected=%d\n", data_len, expected_len);
      return false;
    }
    
    // 提取数据指针（数�?���?字节�?始）
    uart_data = &rx_data[6];
    
    // CRC 两字节位�?data 后面
    uint16_t crc_idx = 6 + data_length;
    uint8_t received_crc_low = rx_data[crc_idx];
    uint8_t received_crc_high = rx_data[crc_idx + 1];
    
    // 验证尾字节（位于 crc 后面两个字节�?
    uint16_t tail_idx = crc_idx + 2;
    if (rx_data[tail_idx] != 0x0A || rx_data[tail_idx + 1] != 0x0D) {
      printf("Invalid tail bytes: 0x%02X%02X, expected 0x0A0D\n", rx_data[tail_idx], rx_data[tail_idx + 1]);
      return false;
    }
    
    // 输出解析结果
    printf("uart1 parse packet: protocol_id=0x%04X, data_length=%d, data=",
           protocol_id, data_length);
    put_buf(uart_data, data_length);
    printf(", crc=0x%02X%02X, tail=0x%02X%02X\n", received_crc_low,
           received_crc_high, rx_data[tail_idx], rx_data[tail_idx + 1]);
    
    // 重置多包状�?�（旧协�?���?��多包�?
    g_is_multi_packet_mode = false;
    g_expected_packet_seq = 1;
    g_multi_packet_offset = 0;
    g_multi_packet_protocol = 0;
    
    return true;
    
  } else {
    // ==================== 新协�?��式解�?====================
    printf("[UART] New protocol format detected\n");
    
    // �?小包长：header(9) + crc(2) + tail(2) = 13字节
    if (data_len < 13) {
      printf("uart1_parse_packet: packet too short, len=%d\n", data_len);
      return false;
    }

    // 解析总帧数量 (大�?�?
    uint16_t total_frames = (rx_data[1] << 8) | rx_data[2];
    
    // 解析顺序�?(大�?�?
    uint16_t seq_num = (rx_data[3] << 8) | rx_data[4];
    
    // 解析数据长度 (大�?�?
    data_length = (rx_data[5] << 8) | rx_data[6];
    
    // 解析协�?ID (大�?�?
    uint16_t protocol_id = (rx_data[7] << 8) | rx_data[8];
    
    printf("[UART] Packet: total_frames=%d, seq=%d, len=%d, protocol=0x%04X\n",
           total_frames, seq_num, data_length, protocol_id);
    
    // 验证 data_length 边界
    if (data_length > 256) {
      printf("uart1_parse_packet: invalid data_length=%d, max=256\n", data_length);
      return false;
    }

    // �?查整�?���?��完整：header(9) + data_length + crc(2) + tail(2)
    uint16_t expected_len = 9 + data_length + 4;
    if (data_len < expected_len) {
      printf("uart1_parse_packet: incomplete packet data_len=%d expected=%d\n", data_len, expected_len);
      return false;
    }
    
    // 提取数据指针（数�?���?字节�?始）
    uint8_t *payload_data = &rx_data[9];
    
    // CRC 两字节位�?data 后面
    uint16_t crc_idx = 9 + data_length;
    uint8_t received_crc_low = rx_data[crc_idx];
    uint8_t received_crc_high = rx_data[crc_idx + 1];
    
    // 验证尾字节（位于 crc 后面两个字节�?
    uint16_t tail_idx = crc_idx + 2;
    if (rx_data[tail_idx] != 0x0A || rx_data[tail_idx + 1] != 0x0D) {
      printf("Invalid tail bytes: 0x%02X%02X, expected 0x0A0D\n", rx_data[tail_idx], rx_data[tail_idx + 1]);
      return false;
    }
    
    // ==================== 多包拼接逻辑 ====================
    
    // 判断�?��为单包模�?
    bool is_single_packet = (total_frames == 1 && seq_num == 1);
    
    if (is_single_packet) {
      // ========== 单包模式：直接�?�?==========
      printf("[UART] Single packet mode\n");
      uart_protocol_id = protocol_id;
      uart_data = payload_data;
      
      // 重置多包状�?�（避免上一次�?包传输的残留状�?�）
      g_is_multi_packet_mode = false;
      g_expected_packet_seq = 1;
      g_multi_packet_offset = 0;
      g_multi_packet_protocol = 0;
      
    } else {
      // ========== 多包模式：拼接�?�?==========
      
      if (seq_num == 1) {
        // �?��包：初�?化�?包缓冲区
        printf("[UART] Multi-packet mode start: total=%d frames\n", total_frames);
        g_is_multi_packet_mode = true;
        g_multi_packet_protocol = protocol_id;
        g_expected_packet_seq = 1;
        g_multi_packet_offset = 0;
        
        // 清空缓冲�?
        memset(g_multi_packet_buffer, 0, MAX_MULTI_PACKET_SIZE);
      }
      
      // 验证协�?ID�?致�?
      if (protocol_id != g_multi_packet_protocol) {
        printf("[UART] ERROR: Protocol ID mismatch! expected=0x%04X, got=0x%04X\n",
               g_multi_packet_protocol, protocol_id);
        g_is_multi_packet_mode = false;
        return false;
      }
      
      // 验证顺序号连�?��?
      if (seq_num != g_expected_packet_seq) {
        printf("[UART] ERROR: Sequence mismatch! expected=%d, got=%d\n",
               g_expected_packet_seq, seq_num);
        g_is_multi_packet_mode = false;
        return false;
      }
      
      // �?查缓冲区�?��溢出
      if (g_multi_packet_offset + data_length > MAX_MULTI_PACKET_SIZE) {
        printf("[UART] ERROR: Multi-packet buffer overflow! offset=%d, len=%d, max=%d\n",
               g_multi_packet_offset, data_length, MAX_MULTI_PACKET_SIZE);
        g_is_multi_packet_mode = false;
        return false;
      }
      
      // 拼接数据到缓冲区
      memcpy(&g_multi_packet_buffer[g_multi_packet_offset], payload_data, data_length);
      g_multi_packet_offset += data_length;
      g_expected_packet_seq++;
      
      printf("[UART] Packet %d/%d received, offset=%d bytes\n",
             seq_num, total_frames, g_multi_packet_offset);
      
      // 判断�?��为最后一�?
      if (seq_num == total_frames) {
        // ========== �?后一包：完成拼接，开始�?�?==========
        printf("[UART] Multi-packet complete! Total %d bytes\n", g_multi_packet_offset);
        
        uart_protocol_id = g_multi_packet_protocol;
        uart_data = g_multi_packet_buffer;
        data_length = g_multi_packet_offset;
        
        // 重置多包状�?
        g_is_multi_packet_mode = false;
        g_expected_packet_seq = 1;
        g_multi_packet_offset = 0;
        g_multi_packet_protocol = 0;
        
      } else {
        // ========== �?��包：继续等待下一�?==========
        printf("[UART] Waiting for next packet...\n");
        return true; // 成功接收，但不触发协�??�?
      }
    }
    
    // 输出解析结果
    printf("uart1 parse packet: protocol_id=0x%04X, data_length=%d, data=",
           uart_protocol_id, data_length);
    put_buf(uart_data, data_length);
    printf(", crc=0x%02X%02X, tail=0x%02X%02X\n", received_crc_low,
           received_crc_high, rx_data[tail_idx], rx_data[tail_idx + 1]);
    
    return true;
  }
}
void uart1_recieve(void) {
  r = uart_recv_blocking(1, (void *)uart_rx_ptr, 256, 5000); // �?直等
  if (r > 0) {                                               // ok
    printf("r:%d\n", r);
    // r = uart_send_blocking(uart_id, (void *)uart_rx_ptr, r, 20);
    put_buf((u8 *)uart_rx_ptr, r);
    uart1_parse_packet((uint8_t *)uart_rx_ptr, r);
  }
}

//==================================end=============================================
// void uart1_init(void) {
//   uart_sync_demo(NULL);
//   printf("uart1 complite successfully\n");
// }
