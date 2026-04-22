// 娉ㄦ剰锟?uart_test.text 娈靛湪閮ㄥ垎鍥轰欢閰嶇疆涓嬩笉锟?PC 鍙墽琛岃寖鍥村唴锟?
// 鑻ユ妸鍏抽敭閫昏緫鏀惧叆璇ユ浼氳Е锟?CPU pc_limit 寮傚父锟?
// 鍥犳榛樿涓嶅惎鐢紱浠呭湪纭疄闇€瑕佽皟璇曟闅旂鏃舵樉寮忔墦寮€ UART1_TEST_SECTION_ENABLE锟?
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

// 璇存槑锛氬伐绋嬩腑閮ㄥ垎绗笁鏂瑰ご鏂囦欢浼氱敤瀹忛噸鏂板畾锟?bool锛堜緥濡傛槧灏勫埌 _Bool锛夛紝
// 锟?cpu.h 锟?bool 锟?typedef(u8)锛涗袱鑰呭彔鍔犱細瀵艰嚧鈥滃ご鏂囦欢澹版槑 vs 婧愭枃浠跺畾涔夆€濈被鍨嬩笉涓€鑷达拷?
// 杩欓噷鍦ㄧ紪璇戝崟鍏冨唴鍙栨秷 bool 瀹忥紝纭繚鏈枃浠朵娇锟?cpu.h 锟?typedef bool锟?
#ifdef bool
#undef bool
#endif

void uart_send_ble_key_list(uint16_t protocol_id);       
static void uart_send_password_key_list(uint16_t protocol_id);
static void uart1_pending_tx_try(void);
extern int dual_ota_app_data_deal(u32 msg, u8 *buf, u32 len);
extern uint8_t test_aes_key[16];
static volatile u8 g_uart1_tx_pending;
static u16 g_uart1_tx_pending_len;
static u8 g_uart1_tx_pending_buf[269];

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

  // 1) 灏濊瘯鎸夆€滃崄杩涘埗瀛楃涓睸N鈥濊В鏋愶細杩炵画鏁板瓧锛屽悗缁厑璁稿叏0濉厖
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

    // 杞负 8 瀛楄妭澶х锛氱瓑浠蜂簬鍗佸叚杩涘埗涓嶈冻 16 浣嶈ˉ 0
    for (int i = 7; i >= 0; i--) {
      out[i] = (uint8_t)(v & 0xFF);
      v >>= 8;
    }
    return 0;
  }

  // 1.5) 鎸夛拷?瀛楄妭BCD缂栫爜鐨勫崄杩涘埗SN鈥濊В鏋愶細姣忎釜鍗婂瓧鑺備竴涓崄杩涘埗鏁板瓧
  // 渚嬪锟?9 00 10 20 31 80 00 01 -> 鍗佽繘鍒跺瓧绗︿覆"0900102031800001"
  // 鍐嶆寜瑙勫垯锟?瀛楄妭HEX(涓嶈冻楂樹綅锟?)
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

  // 2) 鍚﹀垯鎸夆€滃凡缁忔槸8瀛楄妭HEX鈥濆锟?
  if (in_len >= 8) {
    memcpy(out, in, 8);
    return 0;
  }
  return -1;
}

// APP_MSG_USER 浜岀骇娑堟伅ID锛堟湰鏂囦欢鍐呴儴浣跨敤锛涘閮ㄥ闇€缁熶竴璇疯縼绉诲埌鍏叡澶达級
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

// UART->APP 闊虫晥鎾斁娑堟伅閲嶈瘯锛堥伩鍏嶆秷鎭槦鍒楁弧鏃朵涪澶憋級
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

// UART->SOC 閰嶅 PIN/passkey 鎵撳寘鏍囧織锛歛rg0 = FLAG | passkey(0~999999)
// 璇存槑锛氫繚锟?arg0=0/1 鐨勮€佽涔夛紙0x0037/0x0040锛夛紝鍥犳浣跨敤楂樹綅鏍囧織閬垮厤鍐茬獊锟?
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

  // 1) 浼樺厛鏀寔 ASCII 6 浣嶆暟锟?
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

  // 2) 鏀寔 3 瀛楄妭澶х BE24锛堜笌鐜版湁閰嶅鐮佷笅鍙戞牸寮忓榻愶級
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
static s8 g_uart_last_persist_vol = -2; // -2: 鏈垵濮嬪寲
static void set_mcu_for_volume_change(uint8_t volume)
{
  /* MCU->SOC 闊抽噺璁剧疆锛氱瓥鐣ヤ笌 fill_protocol 锟?set_volume_instruct 涓€锟?*/
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
// 瀹氭椂鍣ㄦ鏌ユ満鍒跺叏灞€鍙橀噺
static bool uart_check_enabled = false;       // 瀹氭椂鍣ㄤ娇鑳芥爣锟?
static bool uart_check_timer_enabled = false; // 瀹氭椂鍣ㄤ娇鑳芥爣锟?
static uint32_t uart_check_interval = 100;    // 妫€鏌ラ棿锟?ms)
static uint32_t last_check_time = 0;          // 涓婃妫€鏌ユ椂闂存埑
// 閲嶅彂鏈哄埗鍏ㄥ眬鍙橀噺澹版槑锛堝繀椤诲湪鍑芥暟瀹炵幇涔嬪墠锟?
static uart_retry_context_t uart_retry_ctx = {0};
static uint8_t retry_data_buffer[256];  // 閲嶅彂鏁版嵁缂撳啿锟?
static bool uart_retry_enabled = false; // 閲嶅彂鏈哄埗浣胯兘鏍囧織
bool uart1_parse_packet(uint8_t *rx_data, uint16_t data_len);

// 渚涘箍锟?涓氬姟渚у垽鏂€滄槸鍚﹀凡鏀跺埌MCU涓嬪彂鐨凷N/AES(杩愯鏈熸湁锟?锟?
// 璇存槑锛氬嵆浣縮yscfg灏氭湭鑳借鍒帮紙鎴栧啓鍏ュけ璐ワ級锛屼篃鍙互鍏堢敤鏈€鏂扮殑杩愯鏈熷€煎埛鏂板箍鎾拷?
volatile uint8_t g_uart_sn_valid = 0;
volatile uint8_t g_uart_aes_valid = 0;

static void uart1_request_mcu_update_once(void)
{
  static bool done = false;
  if (done) {
    return;
  }
  done = true;

  // 涓诲姩鍚慚CU鍙戣捣鏇存柊璇锋眰锟?x00F6/0x00F7锛堟寜鍗忚绾﹀畾涓衡€滆姹傚抚鈥濓紝payload涓虹┖锟?
  // 鏀惧湪UART绾跨▼鍒濆鍖栨垚鍔熷悗锛岄伩鍏嶄笂鐢垫棭鏈烳CU灏氭湭灏辩华瀵艰嚧涓㈠寘锟?
  uart1_send_toMCU(0x00F6, NULL, 0);
  os_time_dly(2);
  uart1_send_toMCU(0x00F7, NULL, 0);
}
uint8_t uart_sn_buffer[8] = {0x90, 0x01, 0x02, 0x03,
                             0x18, 0x00, 0x00, 0x05}; // 鐢ㄤ簬瀛樺偍sn锟?
uint8_t uart_aes_key_buffer[16] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                   0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                   0x00, 0x00, 0x00, 0x00};
int r; // 鏁版嵁鍖呴暱锟?
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

// ==================== 澶氬寘鎷兼帴閫昏緫 ====================
// 鏍规嵁鎶ユ枃鏍煎紡绾﹀畾锟?
// - 鍗曞抚鍥哄畾锟?1
// - 澶氬抚闇€瑕佹牴鎹疄闄呮儏鍐靛畾涔夐『搴忓彿
#define MAX_MULTI_PACKET_SIZE 4096  // 鏈€澶ф敮锟?4KB 澶氬寘鏁版嵁
static uint8_t g_multi_packet_buffer[MAX_MULTI_PACKET_SIZE];  // 澶氬寘缂撳啿锟?
static uint16_t g_current_packet_seq = 0;     // 褰撳墠鍖呭簭锟?
static uint16_t g_expected_packet_seq = 1;    // 鏈熸湜鐨勫寘搴忓彿锛堜粠 1 寮€濮嬶級
static uint16_t g_multi_packet_offset = 0;    // 澶氬寘褰撳墠鍋忕Щ
static uint16_t g_multi_packet_protocol = 0;  // 澶氬寘鍗忚 ID
static bool g_is_multi_packet_mode = false;   // 鏄惁澶勪簬澶氬寘妯″紡
uint8_t frame[256] = {0x55, 0xAA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xAA};
static void uart_irq_func(uart_dev uart_num, enum uart_event_v1 event);

static int uart1_dev = 1;

// UART1 鏀跺彂璋冭瘯寮€鍏筹拷?
// 璇存槑锛氬ぇ锟?printf/put_buf 浼氭樉钁楅檷浣庡悶鍚愶紝骞朵笖浼氭嫋鎱㈢郴缁熻皟搴︼紝琛ㄧ幇涓衡€滀覆锟?闃熷垪澶勭悊鎱⑩€濓拷?
// 榛樿鍏抽棴锛屼粎鍦ㄥ畾浣嶅崗璁棶棰樻椂鎵撳紑锟?
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

// RX_TIMEOUT 琛ㄧず鈥滃抚鎺ユ敹瀹屾垚鈥濓紙锟?rx_timeout_thresh 鍐冲畾锛夛拷?
// 鍦ㄥ洖璋冮噷鍙疆浣嶆爣蹇楋紝涓诲惊鐜啀璇诲彇鏁版嵁骞惰В鏋愶紝閬垮厤闃诲绛夊緟鍥哄畾闀垮害锟?
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

// 00FB鍗忚闃叉姈鏈哄埗
static u32 g_last_send_key_list_time = 0;
//===========================================
/**
 * @brief 瀹氭椂妫€鏌ュ嚱鏁帮紝闇€瑕佸湪涓诲惊鐜腑瀹氭湡璋冪敤
 */
void uart1_check_handler(void) {
  if (!uart_check_enabled || !uart_retry_enabled) {
    return;
  }

  // 妫€鏌ユ槸鍚︽鍦ㄧ瓑寰呭搷锟?
  if (!uart_retry_ctx.waiting_response) {
    return;
  }

  // 妫€鏌ユ椂闂撮棿锟?
  uint32_t current_time = jiffies;
  uint32_t elapsed_time = current_time - last_check_time;

  if (elapsed_time < uart_check_interval) {
    return; // 鏈埌妫€鏌ユ椂锟?
  }

  last_check_time = current_time; // 鏇存柊妫€鏌ユ椂锟?

  // 妫€鏌ユ槸鍚︽敹鍒板搷搴旓紙protocol_id鍖归厤锟?
  if (uart1_check_response(uart_retry_ctx.protocol_id)) {
    printf("UART check: response received, stop sending. protocol_id=0x%04X\n",
           uart_retry_ctx.protocol_id);
    uart1_reset_retry_state();
    return;
  }

  // 妫€鏌ユ槸鍚﹁揪鍒版渶澶у彂閫佹锟?
  if (uart_retry_ctx.retry_count >= uart_retry_ctx.max_retries) {
    printf("UART check: max retries reached (%d/%d), stop sending. "
           "protocol_id=0x%04X\n",
           uart_retry_ctx.retry_count, uart_retry_ctx.max_retries,
           uart_retry_ctx.protocol_id);
    uart1_reset_retry_state();
    return;
  }

  // 妫€鏌ユ槸鍚﹁秴鏃堕渶瑕侀噸锟?
  uint32_t elapsed_send_time = current_time - uart_retry_ctx.last_send_time;

  if (elapsed_send_time >= uart_retry_ctx.retry_interval) {
    // 瓒呮椂锛屾鏌ユ槸鍚﹂渶瑕侀噸锟?
    if (uart_retry_ctx.retry_count < uart_retry_ctx.max_retries) {
      uart_retry_ctx.retry_count++;
      uart_retry_ctx.last_send_time = current_time;

      printf("UART check: retry %d/%d: protocol_id=0x%04X\n",
             uart_retry_ctx.retry_count, uart_retry_ctx.max_retries,
             uart_retry_ctx.protocol_id);

      // 閲嶅彂鏁版嵁
      uart1_send_toMCU(uart_retry_ctx.protocol_id, uart_retry_ctx.data,
                       uart_retry_ctx.len);
    } else {
      // 杈惧埌鏈€澶ч噸璇曟鏁帮紝閲嶇疆鐘讹拷?
      printf("UART check: retry failed after %d attempts\n",
             uart_retry_ctx.max_retries);
      uart1_reset_retry_state();
    }
  }
}

/**
 * @brief 鍒濆鍖栧畾鏃跺櫒妫€鏌ユ満锟?
 * @param check_interval 妫€鏌ラ棿锟?ms)
 */
void uart1_check_init(uint32_t check_interval) {
  uart_check_interval = check_interval;
  uart_check_enabled = true;
  last_check_time = jiffies;

  printf("UART check initialized: interval=%dms\n", check_interval);
}

/**
 * @brief 鍚姩瀹氭椂鍣ㄦ鏌ユ満锟?
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
 * @brief 鍋滄瀹氭椂鍣ㄦ鏌ユ満锟?
 */
void uart1_check_stop(void) {
  uart_check_enabled = false;
  printf("UART check stopped\n");
}

/**
 * @brief 閿€姣佸畾鏃跺櫒妫€鏌ユ満锟?
 */
void uart1_check_deinit(void) {
  uart_check_enabled = false;
  printf("UART check deinitialized\n");
}
//===========================================

/**
 * @brief 妫€鏌ユ槸鍚︽敹鍒版寚瀹氬崗璁甀D鐨勫搷锟?
 * @param expected_protocol_id 鏈熸湜鐨勫崗璁甀D
 * @return true 鏀跺埌鍝嶅簲锛宖alse 鏈敹鍒板搷锟?
 */
bool uart1_check_response(uint16_t expected_protocol_id) {
  if (uart_protocol_id == expected_protocol_id && uart_data != NULL) {
    printf("UART response received: protocol_id=0x%04X\n",
           expected_protocol_id);
    // 娓呯┖鍏ㄥ眬鍙橀噺锛岄伩鍏嶉噸澶嶆锟?
    uart_protocol_id = 0;
    uart_data = NULL;
    data_length = 0;
    return true;
  }
  return false;
}

/**
 * @brief 鍘熷瓙鍦版嫹璐濆苟鍙栬蛋鏈€杩戜竴娆″搷搴旀暟锟?
 * @param expected_protocol_id 鏈熸湜鐨勫崗璁甀D
 * @param out 杈撳嚭缂撳啿锟?
 * @param out_size 杈撳嚭缂撳啿鍖哄ぇ锟?
 * @param out_len 瀹為檯鎷疯礉闀垮害
 * @return true 宸插彇鍒板搷搴斿苟鎷疯礉锛沠alse 鏈彇锟?
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
// 閲嶅彂鏈哄埗鐩稿叧鍑芥暟瀹炵幇
/**
 * @brief 璁剧疆閲嶅彂鏈哄埗閰嶇疆
 * @param max_retries 鏈€澶ч噸璇曟锟?
 * @param retry_interval 閲嶈瘯闂撮殧(ms)
 */
void uart1_set_retry_config(uint8_t max_retries, uint32_t retry_interval) {
  uart_retry_ctx.max_retries = max_retries;
  uart_retry_ctx.retry_interval = retry_interval;
  uart_retry_enabled = true;
  printf("UART retry config set: max_retries=%d, interval=%dms\n", max_retries,
         retry_interval);
}

/**
 * @brief 閲嶇疆閲嶅彂鐘讹拷?
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
 * @brief 闃诲绛夊緟鍝嶅簲锛堟柊澧炲姛鑳斤級
 * @param protocol_id 鍗忚ID
 * @param timeout_ms 瓒呮椂鏃堕棿锛堟绉掞級
 * @return true 鏀跺埌鍝嶅簲锛宖alse 瓒呮椂
 */
bool uart1_wait_response(uint16_t protocol_id, uint32_t timeout_ms) {
  uint32_t start_time = jiffies;
  uint32_t timeout_jiffies = timeout_ms * 1000; // 杞崲涓簀iffies

  while (jiffies - start_time < timeout_jiffies) {
    // 妫€鏌ユ槸鍚︽敹鍒板搷锟?
    if (uart1_check_response(protocol_id)) {
      return true;
    }

    // 澶勭悊閲嶅彂鏈哄埗
    uart1_retry_handler();

    // 鐭殏寤舵椂锛岄伩鍏嶈繃搴﹀崰鐢–PU
    os_time_dly(10); // 寤舵椂10ms
  }

  printf("UART response timeout: protocol_id=0x%04X\n", protocol_id);
  return false;
}
/**
 * @brief 甯﹂噸鍙戠殑闃诲鍙戦€侊紙鏂板鍔熻兘锟?
 * @param protocol_id 鍗忚ID
 * @param data 鏁版嵁鎸囬拡
 * @param len 鏁版嵁闀垮害
 * @param timeout_ms 绛夊緟鍝嶅簲鐨勮秴鏃舵椂闂达紙姣锟?
 * @return true 鍙戦€佹垚鍔熷苟鏀跺埌鍝嶅簲锛宖alse 鍙戦€佸け璐ユ垨瓒呮椂
 */
bool uart1_send_and_wait(uint16_t protocol_id, uint8_t *data, uint16_t len,
                         uint32_t timeout_ms) {
  // 鍙戦€佹暟鎹紙甯﹂噸鍙戞満鍒讹級
  if (!uart1_send_with_retry(protocol_id, data, len)) {
    printf("娌℃湁鏀跺埌鍝嶅簲\n");
  }

  // 闃诲绛夊緟鍝嶅簲
  return uart1_wait_response(protocol_id, timeout_ms);
}

/**
 * @brief 甯﹂噸鍙戞満鍒剁殑涓插彛鍙戦€佸嚱锟?
 * @param protocol_id 鍗忚ID
 * @param data 鏁版嵁鎸囬拡
 * @param len 鏁版嵁闀垮害
 * @return true 鍙戦€佹垚鍔燂紝false 鍙戦€佸け锟?
 */
bool uart1_send_with_retry(uint16_t protocol_id, uint8_t *data, uint16_t len) {
  if (!uart_retry_enabled) {
    // 閲嶅彂鏈哄埗鏈娇鑳斤紝鐩存帴鍙戯拷?
    return uart1_send_toMCU(protocol_id, data, len);
  }

  // 妫€鏌ユ槸鍚︽鍦ㄧ瓑寰呭搷锟?
  if (uart_retry_ctx.waiting_response) {
    printf("UART is waiting for response, skip new send request\n");
    return false;
  }

  // 淇濆瓨鍙戦€佹暟鎹埌缂撳啿锟?
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
  uart_retry_ctx.last_send_time = jiffies; // 浣跨敤绯荤粺鏃堕棿锟?

  // 绗竴娆″彂锟?
  printf("UART send with retry: protocol_id=0x%04X, len=%d\n", protocol_id,
         len);
  return uart1_send_toMCU(protocol_id, data, len);
}

/**
 * @brief 閲嶅彂澶勭悊鍑芥暟锛岄渶瑕佸湪涓诲惊鐜腑瀹氭湡璋冪敤
 */
void uart1_retry_handler(void) {
  if (!uart_retry_enabled || !uart_retry_ctx.waiting_response) {
    return;
  }

  // 妫€鏌ユ槸鍚︽敹鍒板搷锟?
  if (uart1_check_response(uart_retry_ctx.protocol_id)) {
    uart1_reset_retry_state();
    return;
  }

  // 妫€鏌ユ槸鍚﹁秴锟?
  uint32_t current_time = jiffies;
  uint32_t elapsed_time = current_time - uart_retry_ctx.last_send_time;

  if (elapsed_time >= uart_retry_ctx.retry_interval) {
    // 瓒呮椂锛屾鏌ユ槸鍚﹂渶瑕侀噸锟?
    if (uart_retry_ctx.retry_count < uart_retry_ctx.max_retries) {
      uart_retry_ctx.retry_count++;
      uart_retry_ctx.last_send_time = current_time;

      printf("UART retry %d/%d: protocol_id=0x%04X\n",
             uart_retry_ctx.retry_count, uart_retry_ctx.max_retries,
             uart_retry_ctx.protocol_id);

      // 閲嶅彂鏁版嵁
      uart1_send_toMCU(uart_retry_ctx.protocol_id, uart_retry_ctx.data,
                       uart_retry_ctx.len);
    } else {
      // 杈惧埌鏈€澶ч噸璇曟鏁帮紝閲嶇疆鐘讹拷?
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
      .tx_wait_mutex = 0, // 1:涓嶆敮鎸佷腑鏂皟锟?浜掓枼,0:鏀寔涓柇,涓嶄簰锟?
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
  //     4, 1000); // 鏈€澶ч噸锟?娆★紝闂撮殧1锟?鏍规嵁娲剧數鐨勫崗璁紝5s鍐呴渶瑕佺粰瀹氬洖锟?

  // 鍒濆鍖栧畾鏃跺櫒妫€鏌ユ満鍒讹紝浣跨敤MAX_SEND_NUM浣滀负鏈€澶у彂閫佹锟?
  uart1_check_init(100); // 100ms妫€鏌ラ棿锟?
  uart1_check_start();   // 鍚姩瀹氭椂鍣ㄦ锟?

  // 锟?flash 璇诲彇 AES key 鍒拌繍琛屾椂缂撳啿锟?
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

  // UART鍒濆鍖栧畬鎴愬悗锛屼富鍔ㄥ悜MCU鍙戣捣涓€锟?037/0038鏇存柊璇锋眰
  uart1_request_mcu_update_once();
  uart_dump();
  while (1) {
    // 灏濊瘯閲嶆姇閫掗煶鏁堟挱鏀炬秷鎭紙闈為樆濉烇級
    uart1_pending_tx_try();
    uart1_toneplay_retry_check();
    // 澶勭悊閲嶅彂鏈哄埗
    // uart1_retry_handler();
    // 璇存槑锛氬師鍏堜娇锟?uart_recv_blocking(uart, buf, 256, 20) 浼氱瓑寰呪€滄敹锟?56瀛楄妭鈥濇墠杩斿洖锟?
    // 瀹為檯涓€甯ч€氬父杩滃皬锟?56锛屽鑷存瘡鍖呴澶栫瓑寰呭埌瓒呮椂绐楀彛锛岃〃鐜颁负鈥滀覆鍙ｆ參/闃熷垪鎱⑩€濓拷?
    // 鏀逛负 RX_TIMEOUT 浜嬩欢椹卞姩锛氬彧鏈夊綋涓€甯ф帴鏀跺畬鎴愭椂鎵嶈鍙栧苟瑙ｆ瀽锟?

    if (!g_uart1_rx_frame_ready) {
      os_time_dly(1);
      continue;
    }

    g_uart1_rx_frame_ready = 0;

    // 锟?UART cbuf 涓妸褰撳墠鍙鐨勬暟鎹竴娆℃€ц鍑烘潵锛堟渶锟?2048 瀛楄妭锛屼笌 rx_cbuffer_size 涓€鑷达級锟?
    u32 total = 0;
    // 浣跨敤闈為樆濉炲紡璇诲彇锛岄伩鍏嶅簲鐢ㄥ眰琚樆锟?
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
        UART1_IO_LOG("鎵惧埌涓€甯ф暟鎹紝闀垮害=%d\n", frame_len);
        uart1_parse_packet(&rx_buf[parse_off], frame_len);
        UART1_IO_LOG("protocol_id:0x%04X\n", uart_protocol_id);

        if (uart_data != NULL) {
        UART1_IO_LOG("recv data:\n");
        UART1_IO_DUMP(uart_data, data_length);

        /*todo:鏂版坊钃濈墮璁剧疆闊抽噺澶у皬閫昏緫*/
        {
          /*濡傛灉 protocol_id = 0x00F1 data == 0x47 ---> 杩涘叆闊抽噺璁剧疆 data[1] = 1/2/3 瀵瑰簲 楂樹腑锟?
          鑰冭檻璋冪敤fill_protocol鐨勯煶閲忓鐞嗛€昏緫*/
          // extern void set_volume_instruct(uint16_t protocol_id);
        }
        if (uart_protocol_id == 0x00F1 && data_length >= 2 && uart_data[0] == 0x2F) {
          printf("杩涘叆闊抽噺璁剧疆");
          set_mcu_for_volume_change(uart_data[1]);
        }

          if (uart_protocol_id == 0x00F4) { // 鎶妘art_data鐨勶拷?1锟? 杞寲涓篿nt绫诲瀷锛堜娇鐢ㄥ師锟?16-bit 澶х锟?
          if (data_length >= 2) {
            u16 code = ((u16)uart_data[0] << 8) | uart_data[1];
            int tone_id = (int)code;
            UART1_IO_LOG("0x00F4: parsed tone_id raw=0x%04X (%d)\n", code, tone_id);

            // 缁熶竴锟?APP_MSG_USER_TONPLAY锛涜嚜瀹氫箟闊虫晥浼樺厛閫昏緫宸插悎骞跺埌 bt.c 锟?case 5
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
            // MCU->SOC锛氭洿鏂癝N (0x00F6)
            // 鏀寔涓ょ鏍煎紡锟?
            // 1) 鍗佽繘鍒跺瓧绗︿覆SN锛氭寜瑙勫垯锟?瀛楄妭HEX(涓嶈冻楂樹綅锟?)鍐欏叆CFG_DEVICE_SN
            // 2) 鐩存帴8瀛楄妭HEX锛氬師鏍峰啓鍏FG_DEVICE_SN
            uint8_t sn_hex8[8] = {0};
            if (sn_payload_to_hex8(uart_data, data_length, sn_hex8) != 0) {
              printf("SN 鏁版嵁鏇存柊鐨勯暱锟?%d\n", data_length);
            } else {
              uart_runtime_sn_update(sn_hex8);
              int w = syscfg_write(CFG_DEVICE_SN, sn_hex8, 8);
              printf("SN updated(淇濆瓨杞寲涓篐EX8):\n");
              put_buf(sn_hex8, 8);

              // 璇诲洖楠岃瘉锛屽府鍔╁畾浣嶁€滃啓浜嗕絾骞挎挱娌″彉鈥濈殑闂
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
          // MCU->SOC锛氭洿鏂癆ES_KEY (0x00F7, len=16)
          if (data_length < 16) {
            printf("AES_KEY update invalid len=%d\n", data_length);
          } else {
            uint8_t new_aes_key[16] = {0};
            memcpy(new_aes_key, uart_data, sizeof(new_aes_key));
            uart_runtime_aes_key_update(new_aes_key);
            int w = syscfg_write(CFG_DEVICE_AES_KEY, new_aes_key, sizeof(new_aes_key));
            printf("AES_KEY updated and saved.\n");

            // 璇诲洖楠岃瘉
            {
              uint8_t rb[16] = {0};
              int r = syscfg_read(CFG_DEVICE_AES_KEY, rb, sizeof(rb));
              printf("syscfg_write(CFG_DEVICE_AES_KEY) ret=%d, readback ret=%d\n", w, r);
              printf("AES_KEY readback:\n");
              put_buf(new_aes_key, 16);
              if (memcmp(rb, new_aes_key, 16) == 0) {
                printf("AES_key_妫€楠屾垚锟? matches written value.\n");
              } else {
                printf("AES_KEY_妫€楠屽け锟? does not match written value.\n");
              }
              
            }

            // app_send_message(APP_MSG_USER_AES_KEY, 0);
            uart_update_ble_adv_restart();
          }
        }
#endif

        if (uart_protocol_id == 0x0033)
        { // 褰撳墠闇€瑕佺洿鎺ヨ幏鍙栬溅杈嗚缃俊鎭寚锟?鍦╨e_trans_data 褰撲腑姝荤瓑涓嶅埌鍥炲簲
          // extern void  get_vehice_set_infromation_instruct(uint16_t protocol_id);
          //  app_send_message(APP_MSG_USER_VEHICLE_INFORMATION,
          //                  0); // 杞崲涓篿nt浼犵粰浜嬩欢闃熷垪-------> 鐢ㄤ簬澶勭悊杞﹁締璁剧疆淇℃伅
          
        }
        if(uart_protocol_id == 0x0037)
        { /*
          * 澶勭悊閰嶅淇℃伅鎸囦护 0x0037
          * 鏂板锛氭敮鎸佹惡锟?PIN/passkey锛圓SCII 6锟?锟?3瀛楄妭BE24锛夛紝鐢ㄤ簬鐩存帴鎸囧畾閰嶅鐮侊紱
          *      涓嶆惡锟?瑙ｆ瀽澶辫触鍒欎繚鎸佹棫閫昏緫(闅忔満鐢熸垚)锟?
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
                           0); // 杞崲涓篿nt浼犵粰浜嬩欢闃熷垪-------> 鐢ㄤ簬澶勭悊閰嶅淇℃伅鎸囦护
        }
        if (uart_protocol_id == 0x0040)
        {
          // 澶勭悊4G+BLE鎯呭喌涓嬭繘琛屼覆鍙ｄ笅鍙慳dd閽ュ寵鎸囦护
          app_send_message(APP_MSG_USER_PARING_INFORMATION,
                           1); // 杞崲涓篿nt浼犵粰浜嬩欢闃熷垪-------> 鐢ㄤ簬澶勭悊閰嶅淇℃伅鎸囦护
        }
        
        if (uart_protocol_id == 0x00F8)
        {
          // 娣诲姞钃濈墮閽ュ寵 - 鍏堝垎鏋愰挜鍖欑被鍨嬶紝鐒跺悗鏍规嵁涓嶅悓鐨勯挜鍖欑被鍨嬪幓鍐欓€昏緫
          // 鏁版嵁鏍煎紡锛歔璁惧绫诲瀷(4瀛楄妭)][MAC鍦板潃(6瀛楄妭)][鍖归厤瀵嗙爜(4瀛楄妭)]
          printf("[UART] PID_BLE_ADD_BLUETOOTH_KEY: 娣诲姞钃濈墮閽ュ寵\n");
          if (data_length >= 14) {
            uint8_t device_type[4];
            memcpy(device_type, uart_data, 4);
            uint8_t MAC_addr[6];
            memcpy(MAC_addr, uart_data + 4, 6);
            uint8_t password[4];
            memcpy(password, uart_data + 10, 4);
            
            printf("璁惧绫诲瀷: %02X %02X %02X %02X, MAC鍦板潃:%02X:%02X:%02X:%02X:%02X:%02X, 瀵嗙爜:%02X %02X %02X %02X\n",
                   device_type[0], device_type[1], device_type[2], device_type[3],
                   MAC_addr[0], MAC_addr[1], MAC_addr[2], MAC_addr[3], MAC_addr[4], MAC_addr[5],
                   password[0], password[1], password[2], password[3]);
            
            uint8_t key_type = device_type[3];
            
            switch (key_type) {
              case 0x01:
                printf("鎵嬫満钃濈墮閽ュ寵\n");
                break;
              case 0x02:
              case 0xFE:
                printf("澶栬钃濈墮閽ュ寵\n");
                break;
              case 0x03:
                printf("NFC閽ュ寵\n");
                break;
              case 0x04:
                printf("瀵嗙爜閽ュ寵\n");
                break;
              default:
                printf("鏈煡閽ュ寵绫诲瀷: 0x%02X\n", key_type);
                break;
            }
            
            if (key_type == 0x02 || key_type == 0xFE) {
              uint8_t peripheral_key_num = 0;
              int ret = syscfg_read(CFG_BLE_KEY_PERIPHERAL_USABLE, &peripheral_key_num, sizeof(peripheral_key_num));
              
              if (ret <= 0) {
                printf("璇诲彇CFG_BLE_KEY_PERIPHERAL_USABLE澶辫触锛屼娇鐢ㄩ粯璁わ拷?\n");
                peripheral_key_num = 0;
              }
              
              if (peripheral_key_num > 4) {
                printf("妫€娴嬪埌鏃犳晥鐨刱ey_num锟? %d锛岄噸缃负0\n", peripheral_key_num);
                peripheral_key_num = 0;
              }
              
              printf("褰撳墠鍙互鐢ㄧ殑澶栬钃濈墮閽ュ寵淇濆瓨绌洪棿:%d\n", peripheral_key_num);
              uint8_t Uable_num = 4 - peripheral_key_num;
              
              if (Uable_num > 0) {
                printf("妫€娴嬪埌鍙互鐢ㄧ殑澶栬钃濈墮閽ュ寵淇濆瓨绌洪棿锛孶able_num:%d\n", Uable_num);
                
                uint8_t current_MAC_adders[6] = {0};
                int mac_exists = 0;
                
                for (int i = 1; i <= 4; i++) {
                  uint16_t cfg_id = CFG_BLE_KEY_PERIPHERAL_SEND_1 + (i - 1);
                  syscfg_read(cfg_id, current_MAC_adders, 6);
                  if (!is_all_zero(current_MAC_adders, 6) && memcmp(current_MAC_adders, MAC_addr, 6) == 0) {
                    mac_exists = 1;
                    printf("MAC鍦板潃宸插瓨鍦ㄤ簬浣嶇疆%d\n", i);
                    break;
                  }
                }
                
                if (mac_exists) {
                  printf("MAC鍦板潃宸插瓨鍦紝涓嶄繚瀛榎n");
                } else {
                  // 妫€鏌AC鍦板潃鏄惁鍏ㄤ负0
                  if (is_all_zero(MAC_addr, 6)) {
                    printf("MAC鍦板潃鍏ㄤ负0锛屼笉淇濆瓨\n");
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
                        printf("淇濆瓨鍒颁綅锟?\n");
                        break;
                      case 1:
                        save_cfg_id = CFG_BLE_KEY_PERIPHERAL_SEND_2;
                        printf("淇濆瓨鍒颁綅锟?\n");
                        break;
                      case 2:
                        save_cfg_id = CFG_BLE_KEY_PERIPHERAL_SEND_3;
                        printf("淇濆瓨鍒颁綅锟?\n");
                        break;
                      case 3:
                        save_cfg_id = CFG_BLE_KEY_PERIPHERAL_SEND_4;
                        printf("淇濆瓨鍒颁綅锟?\n");
                        break;
                      default:
                        printf("鏃犳晥鐨刱ey_num锟? %d\n", peripheral_key_num);
                        break;
                    }
                    
                    if (save_cfg_id != 0) {
                      ret = syscfg_write(save_cfg_id, save_buffer, 28);
                      if (ret > 0) {
                        peripheral_key_num++;
                        syscfg_write(CFG_BLE_KEY_PERIPHERAL_USABLE, &peripheral_key_num, sizeof(peripheral_key_num));
                        printf("淇濆瓨鎴愬姛锛屽綋鍓嶅凡淇濆瓨鏁伴噺: %d\n", peripheral_key_num);
                        uart_send_ble_key_list(0x00FB);
                      } else {
                        printf("淇濆瓨澶辫触\n");
                      }
                    }
                  }
                }
              } else {
                printf("娌℃湁澶栬钃濈墮閽ュ寵绌洪棿\n");
              }
            } else if (key_type == 0x15) {
              uint8_t nfc_key_num = 0;
              int ret = syscfg_read(CFG_NFC_KEY_PERIPHERAL_USABLE_NUM, &nfc_key_num, sizeof(nfc_key_num));
              
              if (ret <= 0) {
                printf("璇诲彇CFG_NFC_KEY_PERIPHERAL_USABLE_NUM澶辫触锛屼娇鐢ㄩ粯璁わ拷?\n");
                nfc_key_num = 0;
              }
              
              if (nfc_key_num > 3) {
                printf("妫€娴嬪埌鏃犳晥鐨刵fc_key_num锟? %d锛岄噸缃负0\n", nfc_key_num);
                nfc_key_num = 0;
              }
              
              printf("褰撳墠鍙互鐢ㄧ殑NFC閽ュ寵淇濆瓨绌洪棿:%d\n", nfc_key_num);
              uint8_t Uable_num = 3 - nfc_key_num;
              
              if (Uable_num > 0) {
                printf("妫€娴嬪埌鍙互鐢ㄧ殑NFC閽ュ寵淇濆瓨绌洪棿锛孶able_num:%d\n", Uable_num);
                
                uint8_t current_NFC_adders[16] = {0};
                int nfc_exists = 0;
                
                for (int i = 1; i <= 3; i++) {
                  uint16_t cfg_id = CFG_NFC_KEY_PERIPHERAL_SEND_1 + (i - 1);
                  syscfg_read(cfg_id, current_NFC_adders, 16);
                  if (!is_all_zero(current_NFC_adders, 6) && memcmp(current_NFC_adders, MAC_addr, 6) == 0) {
                    nfc_exists = 1;
                    printf("NFC鍦板潃宸插瓨鍦ㄤ簬浣嶇疆%d\n", i);
                    break;
                  }
                }
                
                if (nfc_exists) {
                  printf("NFC鍦板潃宸插瓨鍦紝涓嶄繚瀛榎n");
                } else {
                  // 妫€鏌AC鍦板潃鏄惁鍏ㄤ负0
                  if (is_all_zero(MAC_addr, 6)) {
                    printf("NFC鍦板潃鍏ㄤ负0锛屼笉淇濆瓨\n");
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
                        printf("淇濆瓨鍒颁綅锟?\n");
                        break;
                      case 1:
                        save_cfg_id = CFG_NFC_KEY_PERIPHERAL_SEND_2;
                        printf("淇濆瓨鍒颁綅锟?\n");
                        break;
                      case 2:
                        save_cfg_id = CFG_NFC_KEY_PERIPHERAL_SEND_3;
                        printf("淇濆瓨鍒颁綅锟?\n");
                        break;
                      default:
                        printf("鏃犳晥鐨刵fc_key_num锟? %d\n", nfc_key_num);
                        break;
                    }
                    
                    if (save_cfg_id != 0) {
                      ret = syscfg_write(save_cfg_id, save_buffer, 16);
                      if (ret > 0) {
                        nfc_key_num++;
                        syscfg_write(CFG_NFC_KEY_PERIPHERAL_USABLE_NUM, &nfc_key_num, sizeof(nfc_key_num));
                        printf("淇濆瓨鎴愬姛锛屽綋鍓嶅凡淇濆瓨鏁伴噺: %d\n", nfc_key_num);
                        uart_send_ble_key_list(0x00FB);
                      } else {
                        printf("淇濆瓨澶辫触\n");
                      }
                    }
                  }
                }
              } else {
                printf("娌℃湁NFC閽ュ寵绌洪棿\n");
              }
            } else if (key_type == 0x01) {
              printf("鎵嬫満钃濈墮閽ュ寵 - 瑙﹀彂閰嶅娴佺▼\n");
              
              uint8_t phone_ble_key_usable_num = 0;
              int ret = syscfg_read(CFG_PHONE_BLE_KEY_USABLE_NUM, &phone_ble_key_usable_num, sizeof(phone_ble_key_usable_num));
              
              if (ret <= 0) {
                printf("璇诲彇CFG_PHONE_BLE_KEY_USABLE_NUM澶辫触锛屼娇鐢ㄩ粯璁わ拷?\n");
                phone_ble_key_usable_num = 0;
              }
              
              if (phone_ble_key_usable_num > 3) {
                printf("妫€娴嬪埌鏃犳晥鐨刾hone_ble_key_usable_num锟? %d锛岄噸缃负0\n", phone_ble_key_usable_num);
                phone_ble_key_usable_num = 0;
              }
              
              printf("褰撳墠鍙互鐢ㄧ殑鎵嬫満钃濈墮閽ュ寵淇濆瓨绌洪棿:%d\n", phone_ble_key_usable_num);
              uint8_t Uable_num = 3 - phone_ble_key_usable_num;
              
              if (Uable_num > 0) {
                printf("妫€娴嬪埌鍙互鐢ㄧ殑鎵嬫満钃濈墮閽ュ寵淇濆瓨绌洪棿锛孶able_num:%d\n", Uable_num);
                
                // 妫€鏌AC鍦板潃鏄惁鍏ㄤ负0
                if (is_all_zero(MAC_addr, 6)) {
                  printf("MAC鍦板潃鍏ㄤ负0锛屽皢鍦ㄩ厤瀵规垚鍔熷悗鑾峰彇骞朵繚瀛樻墜鏈虹殑MAC鍦板潃\n");
                }
                
                uint32_t passkey = 0;
                uint8_t valid_passkey = 0;
                
                printf("鍘熷password: %02X %02X %02X %02X\n", password[0], password[1], password[2], password[3]);
                
                if (password[0] == 0 && password[1] == 0 && password[2] == 0 && password[3] == 0) {
                  printf("password鍏ㄤ负0锛屼娇鐢ㄩ殢鏈虹敓鎴怽n");
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
                    printf("BCD瑙ｇ爜閰嶅锟? %06u\n", passkey);
                    valid_passkey = 1;
                  } else {
                    passkey = ((uint32_t)password[0] << 24) | ((uint32_t)password[1] << 16) | 
                               ((uint32_t)password[2] << 8) | (uint32_t)password[3];
                    if (passkey > 0 && passkey <= 999999) {
                      printf("鐩存帴浣跨敤閰嶅锟? %06u\n", passkey);
                      valid_passkey = 1;
                    } else {
                      printf("鏃犳晥鐨刾asskey锟? %u锛屼娇鐢ㄩ殢鏈虹敓鎴怽n", passkey);
                      valid_passkey = 0;
                    }
                  }
                }
                
                if (valid_passkey) {
                  printf("璁剧疆閰嶅passkey: %06u\n", passkey);
                  ble_pairing_set_uart_passkey(passkey);
                  fill_protocol_set_uart_passkey(passkey);
                  
                  printf("瑙﹀彂閰嶅娴佺▼\n");
                  app_send_message(APP_MSG_USER_PARING_INFORMATION, 0x40000000 | passkey);
                } else {
                  printf("浣跨敤闅忔満鐢熸垚閰嶅鐮乗n");
                  app_send_message(APP_MSG_USER_PARING_INFORMATION, 0);
                }
              } else {
                printf("娌℃湁鎵嬫満钃濈墮閽ュ寵绌洪棿\n");
              }
            } else if (key_type == 0x04) {
              printf("瀵嗙爜閽ュ寵鏆備笉鏀寔娣诲姞\n");
            }
          }
          /* phone key list should be sent after pair success callback */
          
        }
        
        if (uart_protocol_id == 0x00F9)
        {
          // 鍒犻櫎钃濈墮閽ュ寵 - 鍏堝垎鏋愰挜鍖欑被鍨嬶紝鐒跺悗鏍规嵁涓嶅悓鐨勯挜鍖欑被鍨嬪幓鍒犻櫎
          // 鏁版嵁鏍煎紡锛歔璁惧绫诲瀷(4瀛楄妭)][MAC鍦板潃(6瀛楄妭)][鍖归厤瀵嗙爜(4瀛楄妭)]
          printf("[UART] PID_BLE_DELETE_BLUETOOTH_KEY: 鍒犻櫎钃濈墮閽ュ寵\n");
          if (data_length >= 14) {
            uint8_t device_type[4];
            memcpy(device_type, uart_data, 4);
            uint8_t MAC_addr[6];
            memcpy(MAC_addr, uart_data + 4, 6);
            uint8_t password[4];
            memcpy(password, uart_data + 10, 4);
            
            printf("璁惧绫诲瀷: %02X %02X %02X %02X, MAC鍦板潃:%02X:%02X:%02X:%02X:%02X:%02X, 瀵嗙爜:%02X %02X %02X %02X\n",
                   device_type[0], device_type[1], device_type[2], device_type[3],
                   MAC_addr[0], MAC_addr[1], MAC_addr[2], MAC_addr[3], MAC_addr[4], MAC_addr[5],
                   password[0], password[1], password[2], password[3]);
            
            uint8_t key_type = device_type[3];
            
            switch (key_type) {
              case 0x01:
                printf("鎵嬫満钃濈墮閽ュ寵\n");
                break;
              case 0x02:
              case 0xFE:
                printf("澶栬钃濈墮閽ュ寵\n");
                break;
              case 0x03:
                printf("NFC閽ュ寵\n");
                break;
              case 0x04:
                printf("瀵嗙爜閽ュ寵\n");
                break;
              default:
                printf("鏈煡閽ュ寵绫诲瀷: 0x%02X\n", key_type);
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
                    printf("鍒犻櫎锟?d涓璁捐摑鐗欓挜鍖橽n", i);
                    current_key_num--;
                    deleted = 1;
                    break;
                  }
                }
                
                if (deleted) {
                  syscfg_write(CFG_BLE_KEY_PERIPHERAL_USABLE, &current_key_num, sizeof(current_key_num));
                  printf("鍒犻櫎鎴愬姛锛屽綋鍓嶅凡淇濆瓨鏁伴噺: %d\n", current_key_num);
                  uart_send_ble_key_list(0x00FB);
                } else {
                  printf("鏈壘鍒板尮閰嶇殑MAC鍦板潃\n");
                  uart_send_ble_key_list(0x00FB); // 鍗充娇娌℃壘鍒板尮閰嶇殑锛屼篃鍥炲褰撳墠鍒楄〃锛屽府鍔㎝CU鍚屾鐘舵€?
                }
              } else {
                printf("褰撳墠娌℃湁澶栬钃濈墮閽ュ寵\n");
                uart_send_ble_key_list(0x00FB); // 鍗充娇娌℃湁澶栬钃濈墮閽ュ寵锛屼篃鍥炲褰撳墠鍒楄〃锛屽府鍔㎝CU鍚屾鐘舵€?
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
                    printf("鍒犻櫎锟?d涓狽FC閽ュ寵\n", i);
                    current_key_num--;
                    deleted = 1;
                    break;
                  }
                }
                
                if (deleted) {
                  syscfg_write(CFG_NFC_KEY_PERIPHERAL_USABLE_NUM, &current_key_num, sizeof(current_key_num));
                  printf("鍒犻櫎鎴愬姛锛屽綋鍓嶅凡淇濆瓨鏁伴噺: %d\n", current_key_num);
                  uart_send_ble_key_list(0x00FB);
                } else {
                  printf("鏈壘鍒板尮閰嶇殑NFC鍦板潃\n");
                }
              } else {
                printf("褰撳墠娌℃湁NFC閽ュ寵\n");
              }
            } else if (key_type == 0x01) {
              printf("鎵嬫満钃濈墮閽ュ寵 - 鎵ц鍒犻櫎鎿嶄綔\n");
              
              uint8_t current_key_num = 0;
              int ret = syscfg_read(CFG_PHONE_BLE_KEY_USABLE_NUM, &current_key_num, sizeof(current_key_num));
              
              if (ret <= 0) {
                printf("璇诲彇CFG_PHONE_BLE_KEY_USABLE_NUM澶辫触锛屼娇鐢ㄩ粯璁わ拷?\n");
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
                    printf("鍒犻櫎锟?d涓墜鏈鸿摑鐗欓挜鍖橽n", i + 1);
                    current_key_num--;
                    deleted = 1;
                    break;
                  }
                }
                
                if (deleted) {
                  syscfg_write(CFG_PHONE_BLE_KEY_USABLE_NUM, &current_key_num, sizeof(current_key_num));
                  printf("鍒犻櫎鎴愬姛锛屽綋鍓嶅凡淇濆瓨鏁伴噺: %d\n", current_key_num);
                  
                  // 鍙栨秷閰嶅鎿嶄綔
                  {
                    bool unpair_ret = ble_list_delete_device(MAC_addr, 0);
                    printf("鍙栨秷涓庤璁惧鐨勯厤瀵? ret=%d, MAC=%02X:%02X:%02X:%02X:%02X:%02X\\n",
                           unpair_ret,
                           MAC_addr[0], MAC_addr[1], MAC_addr[2],
                           MAC_addr[3], MAC_addr[4], MAC_addr[5]);
                  }
                  
                  
                  // 缁橫CU鍥炲閽ュ寵鍒楄〃
                  uart_send_ble_key_list(0x00FB);
                } else {
                  printf("鏈壘鍒板尮閰嶇殑鎵嬫満钃濈墮閽ュ寵\n");
                }
              } else {
                printf("褰撳墠娌℃湁鎵嬫満钃濈墮閽ュ寵\n");
              }
            } else if (key_type == 0x04) {
              printf("瀵嗙爜閽ュ寵鏆備笉鏀寔鍒犻櫎\n");
            }
          }
        }
        
        if (uart_protocol_id == 0x00FA)
        {
          // 娓呯┖钃濈墮閽ュ寵 - 鍏堝垎鏋愰挜鍖欑被鍨嬶紝鐒跺悗鏍规嵁涓嶅悓鐨勯挜鍖欑被鍨嬪幓娓呯┖
          // 鏁版嵁鏍煎紡锛歔璁惧绫诲瀷(4瀛楄妭)]
          printf("[UART] PID_BLE_CLEARALL_BLUTOOTH_KEY: 娓呯┖钃濈墮閽ュ寵\n");
          if (data_length >= 4) {
            uint8_t device_type[4];
            memcpy(device_type, uart_data, 4);
            
            printf("璁惧绫诲瀷: %02X %02X %02X %02X\n",
                   device_type[0], device_type[1], device_type[2], device_type[3]);
            
            uint8_t key_type = device_type[3];
            
            switch (key_type) {
              case 0x01:
                printf("鎵嬫満钃濈墮閽ュ寵\n");
                break;
              case 0x02:
              case 0xFE:
                printf("澶栬钃濈墮閽ュ寵\n");
                break;
              case 0x03:
                printf("NFC閽ュ寵\n");
                break;
              case 0x04:
                printf("瀵嗙爜閽ュ寵\n");
                break;
              default:
                printf("鏈煡閽ュ寵绫诲瀷: 0x%02X\n", key_type);
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
              printf("娓呯┖鎵€鏈夊璁捐摑鐗欓挜鍖欐垚鍔焅n");
              uart_send_ble_key_list(0x00FB);
            } else if (key_type == 0x03) {
              uint8_t empty_buffer[16] = {0};
              syscfg_write(CFG_NFC_KEY_PERIPHERAL_SEND_1, empty_buffer, sizeof(empty_buffer));
              syscfg_write(CFG_NFC_KEY_PERIPHERAL_SEND_2, empty_buffer, sizeof(empty_buffer));
              syscfg_write(CFG_NFC_KEY_PERIPHERAL_SEND_3, empty_buffer, sizeof(empty_buffer));
              
              uint8_t state_flag = 0x00;
              syscfg_write(CFG_NFC_KEY_PERIPHERAL_USABLE_NUM, &state_flag, sizeof(state_flag));
              printf("娓呯┖鎵€鏈塏FC閽ュ寵鎴愬姛\n");
              uart_send_ble_key_list(0x00FB);
            } else if (key_type == 0x01) {
              printf("鎵嬫満钃濈墮閽ュ寵鏆備笉鏀寔娓呯┖\n");
            } else if (key_type == 0x04) {
              printf("瀵嗙爜閽ュ寵鏆備笉鏀寔娓呯┖\n");
            }
          }
        }
        
        if (uart_protocol_id == 0x00FB)
        {
          // 鑾峰彇鎵€鏈夎摑鐗欓挜锟?- 鍩轰簬send_ble_key_list閫昏緫
          printf("[UART] PID_BLE_GETALL_BLUTOOTH_KEY: 鑾峰彇鎵€鏈夎摑鐗欓挜鍖橽n");
          uart_send_ble_key_list(0x00FB);
        }
        
        if (uart_protocol_id == 0x00FC)
        {
          // 鑾峰彇瀵嗙爜閽ュ寵
          printf("[UART] PID_GET_PASSWORD_KEY: 鑾峰彇瀵嗙爜閽ュ寵\n");
          
          // 淇濆瓨鎺ユ敹鍒扮殑瀵嗙爜鏁版嵁鍒伴厤缃」
          if (uart_data != NULL && data_length >= 4) {
            // 瀵嗙爜鏁版嵁鍦ㄦ渶鍚庣殑4瀛楄妭
            uint8_t password_data[4] = {0};
            int start_idx = data_length - 4;
            memcpy(password_data, &uart_data[start_idx], 4);
            
            printf("淇濆瓨瀵嗙爜閽ュ寵: %02X%02X%02X%02X\n", password_data[0], password_data[1], password_data[2], password_data[3]);
            syscfg_write(CFG_VEHICLE_PASSWORD_UNLOCK, password_data, 4);
          }
          
          uart_send_password_key_list(0x00FC);
        }
        /*OTA 閫昏緫*/
        if (uart_protocol_id == 0x55AA)
        {
          int ota_ret = dual_ota_app_data_deal(uart_protocol_id, uart_data, data_length);
          if (ota_ret) {
            printf("[UART][OTA] dual_ota_app_data_deal ret=%d, len=%d\n", ota_ret, data_length);
          }
        }
        
        if (uart_protocol_id == 0x00A1)
        {
          /* 鏌ョ湅涓€ч煶鏁堜紶杈撴姤锟?*/
          printf("[UART] Received cmd 0x00A1: Query transfer report\n");
          
          // 璇诲彇鎶ュ憡鏂囦欢
          const char *report_path = "mnt/sdfile/app/uwav/transfer_report.txt";
          FILE *fp = fopen(report_path, "r");
          
          if (!fp) {
            // 鎶ュ憡鏂囦欢涓嶅瓨鍦紝鎻愪緵鏇磋缁嗙殑璇婃柇淇℃伅
            printf("[UART] Report file not found: %s\n", report_path);
            
            // 鍏堝皾璇曞垱锟?uwav 鐩綍
            int mkdir_ret = fmk_dir("mnt/sdfile/app", "/uwav", 0);
            if (mkdir_ret == 0) {
              printf("[UART] uwav directory created successfully\n");
            } else if (mkdir_ret == -1) {
              printf("[UART] uwav directory already exists\n");
            } else {
              printf("[UART] mkdir uwav ret=%d\n", mkdir_ret);
            }
            
            // 妫€鏌ユ槸鍚︽湁浼犺緭姝ｅ湪杩涜
            extern uint8_t is_file_transfer_started;
            extern uint32_t file_write_offset;
            extern uint32_t total_file_size;
            
            uint8_t status_buf[256];
            int status_len;
            
            if (is_file_transfer_started) {
              // 浼犺緭姝ｅ湪杩涜锟?
              status_len = snprintf((char*)status_buf, sizeof(status_buf),
                "浼犺緭杩涜锟? %u/%u bytes (%.1f%%)\n"
                "璇风瓑寰呬紶杈撳畬鎴愬悗鍐嶆煡璇㈡姤鍛娿€俓n",
                file_write_offset, total_file_size,
                total_file_size ? (file_write_offset * 100.0f / total_file_size) : 0.0f);
              printf("[UART] Transfer in progress: %u/%u bytes\n", file_write_offset, total_file_size);
            } else {
              // 娌℃湁浼犺緭璁板綍
              status_len = snprintf((char*)status_buf, sizeof(status_buf),
                "鏈壘鍒颁紶杈撴姤鍛婃枃浠躲€俓n"
                "璇峰厛閫氳繃APP浼犺緭涓€ч煶鏁堬紝浼犺緭瀹屾垚鍚庝細鑷姩鐢熸垚鎶ュ憡銆俓n\n"
                "鍙兘鍘熷洜锛歕n"
                "1. 灏氭湭杩涜杩囦釜鎬ч煶鏁堜紶杈揬n"
                "2. 涓婃浼犺緭鏈畬鎴愭垨涓柇\n"
                "3. 涓婃浼犺緭澶辫触\n\n"
                "鎻愮ず锛氳浣跨敤APP閲嶆柊浼犺緭闊虫晥鏂囦欢銆俓n");
              printf("[UART] No transfer history found\n");
            }
            
            uart1_send_toMCU(0x00A1, status_buf, status_len);
          } else {
            // 璇诲彇鏁翠釜鏂囦欢鍐呭骞跺垎娈靛彂锟?
            uint8_t read_buf[256];
            uint8_t send_buf[UART_MAX_DATA_LEN - 50]; // 鐣欎竴浜涗綑锟?
            uint16_t buf_pos = 0;
            int read_len;
            
            printf("[UART] Reading transfer report...\n");
            
            // 寰幆璇诲彇鏂囦欢鍐呭
            while ((read_len = fread(fp, read_buf, sizeof(read_buf))) > 0) {
              // 閫愬瓧鑺傚鐞嗭紝閬囧埌鎹㈣鎴栫紦鍐插尯婊℃椂鍙戯拷?
              for (int i = 0; i < read_len; i++) {
                send_buf[buf_pos++] = read_buf[i];
                
                // 濡傛灉缂撳啿鍖哄揩婊′簡锛屽厛鍙戯拷?
                if (buf_pos >= sizeof(send_buf) - 1) {
                  uart1_send_toMCU(0x00A1, send_buf, buf_pos);
                  os_time_dly(2); // 鐭殏寤舵椂閬垮厤鏁版嵁鎷ュ
                  buf_pos = 0;
                }
              }
            }
            
            // 鍙戦€佸墿浣欐暟锟?
            if (buf_pos > 0) {
              uart1_send_toMCU(0x00A1, send_buf, buf_pos);
            }
            
            fclose(fp);
            printf("[UART] Transfer report sent successfully\n");
          }
        }
        if (uart_protocol_id == 0x00A2)
        {
          /* 浠庨煶棰戣祫婧愬尯鎾斁闊抽 - 浣跨敤 tone_table 绱㈠紩 */
          printf("[UART] Received cmd 0x00A2: Play tone by index\n");
          
          // 鍗忚鏁版嵁鏍煎紡锟?
          // [tone_index 1瀛楄妭 锟?2瀛楄妭(澶х)] 瀵瑰簲 tone_table 鐨勭储锟?
          // 渚嬪锟?
          //   0x00 -> 鎾斁 IDEX_TONE_USER_001 (tone0.*)
          //   0x13 -> 鎾斁 tone19.*
          //   122  -> 鎾斁 IDEX_TONE_USER_123 (test11111.*)
          
          uint8_t tone_index = IDEX_TONE_USER_001; // 榛樿绱㈠紩锛坱one0.*锟?
          
          if (uart_data != NULL && data_length > 0) {
            // 鏀寔 1瀛楄妭 锟?2瀛楄妭(BCD鏍煎紡) 绱㈠紩
            uint16_t raw_value = 0;
            if (data_length >= 2) {
                // BCD瑙ｆ瀽锛氭瘡涓瓧鑺傜殑锟?浣嶅拰锟?浣嶅垎鍒唬琛ㄥ崄浣嶅拰涓綅
                // 渚嬪 0x01 0x11 -> 1*100 + 1*10 + 1 = 111
                uint8_t bcd1 = uart_data[0];
                uint8_t bcd2 = uart_data[1];
                raw_value = ((bcd1 >> 4) * 10 + (bcd1 & 0x0F)) * 100 +
                           ((bcd2 >> 4) * 10 + (bcd2 & 0x0F));
            } else {
                raw_value = uart_data[0];
            }
            
            // 鍗忚鍊肩洿鎺ヨ〃绀洪煶棰戠紪鍙凤細0 -> tone0, 19 -> tone19
            if (raw_value <= 122) {
              tone_index = IDEX_TONE_USER_001 + raw_value;
              printf("[UART] Raw value: %u -> tone_table index: %u\n", raw_value, tone_index);
            } else {
              printf("[UART] Invalid tone index %u (max 122), using default\n", raw_value);
            }
          } else {
            printf("[UART] No data, using default tone index\n");
          }
          
          // 浣跨敤 tone_play_by_path 鐩存帴鎾斁锛屽洜锟?tone_index 鏁扮粍涓嶅畬锟?
          extern int tone_play_by_path(const char *name, u8 preemption);
          extern const char *tone_table[];
          extern const int tone_table_size;
          
          // 锟?tone_table 鑾峰彇瀹為檯鏂囦欢璺緞
          const char *tone_path = NULL;
          if (tone_index < tone_table_size) {
            tone_path = tone_table[tone_index];
          }
          if (!tone_path) {
            printf("[UART] ERROR: tone_table index %u invalid or NULL (size=%d)\n", tone_index, tone_table_size);
            goto tone_play_err;
          }
          printf("[UART] Attempting to play: tone_table[%u] = %s\n", tone_index, tone_path);
          
          // 妫€鏌ユ枃浠舵槸鍚﹀瓨锟?
          void *test_fp = fopen(tone_path, "r");
          if (test_fp) {
            fclose(test_fp);
            printf("[UART] File exists: %s\n", tone_path);
          } else {
            printf("[UART] ERROR: File NOT found: %s\n", tone_path);
            printf("[UART] This is the root cause! Audio file missing from firmware!\n");
            goto tone_play_err;
          }
          
          // 鐩存帴浣跨敤璺緞鎾斁
          int ret = tone_play_by_path(tone_path, 1); // preemption=1 浼樺厛鎾斁
          
          // 鍙戦€佺粨鏋滅粰 MCU
          uint8_t resp[2];
          resp[0] = (ret == 0) ? 0x01 : 0x00; // 0x01 = 鎴愬姛, 0x00 = 澶辫触
          resp[1] = tone_index;
          uart1_send_toMCU(0x00A2, resp, 2);
          
          if (ret == 0) {
            printf("[UART] Playing tone_table[%u] successfully\n", tone_index);
          } else {
            printf("[UART] Playing tone_table[%u] failed: ret=%d\n", tone_index, ret);
tone_play_err:
            // 鎾斁澶辫触鏃剁洿鎺ヨ繑鍥烇紝閬垮厤瀵规棤鏁堣矾寰勭户缁В鐮佸鑷村紓锟?
            resp[0] = 0x00;
            uart1_send_toMCU(0x00A2, resp, 2);
          }
        }
        if (uart_protocol_id == 0x00A3)
        {
          /* 鎾斁鑷畾涔夐煶鏁堟Ы浣嶏紙宸蹭紶杈撶殑涓€ч煶鏁堬級 */
          printf("[UART] Received cmd 0x00A3: Play custom tone from slot\n");
          
          // 鍗忚鏁版嵁鏍煎紡锟?
          // [tone_type 1瀛楄妭] (0~3锛屽搴旇嚜瀹氫箟闊虫晥妲戒綅 uwtg0~UWTG3)
          // 濡傛灉娌℃湁鏁版嵁锛岄粯璁ゆ挱鏀炬Ы锟?
          
          uint8_t tone_type = 0;
          if (uart_data != NULL && data_length > 0) {
            tone_type = uart_data[0];
            if (tone_type > 3) {
              tone_type = 0; // 瓒呭嚭鑼冨洿锛屼娇鐢ㄦЫ锟?
            }
          }
          
          printf("[UART] Playing custom tone from slot: %u\n", tone_type);
          
          // 璋冪敤鑷畾涔夐煶鏁堟挱鏀惧嚱锟?
          // 璇ュ嚱鏁颁細妫€锟?meta 淇℃伅锛岀‘璁ら煶鏁堟槸鍚﹀凡浼犺緭
          bool ret = user_custom_tone_play_if_exist(tone_type, 1); // preemption=1 浼樺厛鎾斁
          
          // 鍙戦€佺粨鏋滅粰 MCU
          uint8_t resp[2];
          resp[0] = ret ? 0x01 : 0x00; // 0x01 = 鎴愬姛鎾斁, 0x00 = 闊虫晥涓嶅瓨鍦ㄦ垨鎾斁澶辫触
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
          /* 鏌ヨ鎵€鏈夎嚜瀹氫箟闊虫晥妲戒綅鐘讹拷?*/
          printf("[UART] Received cmd 0x00A4: Query custom tone slots status\n");
          
          // 杩斿洖鏍煎紡锟?
          // [slot0_status] [slot0_size_hi] [slot0_size_lo]
          // [slot1_status] [slot1_size_hi] [slot1_size_lo]
          // [slot2_status] [slot2_size_hi] [slot2_size_lo]
          // [slot3_status] [slot3_size_hi] [slot3_size_lo]
          // status: 0x00=锟? 0x01=鏈夋晥
          
          uint8_t resp[16]; // 4涓Ы锟?* 4瀛楄妭(鐘讹拷?澶у皬3瀛楄妭)
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
              // 妲戒綅鏈夋晥
              resp[slot * 4] = 0x01;  // status = 鏈夋晥
              resp[slot * 4 + 1] = (uint8_t)(meta.file_size >> 16);  // size 楂樺瓧锟?
              resp[slot * 4 + 2] = (uint8_t)(meta.file_size >> 8);   // size 涓瓧锟?
              resp[slot * 4 + 3] = (uint8_t)(meta.file_size & 0xFF); // size 浣庡瓧锟?
              
              printf("[SLOT %u] VALID - path=%s, size=%u, crc=0x%08X\n", 
                     slot, path ? path : "null", 
                     (unsigned)meta.file_size, (unsigned)meta.crc32);
            } else {
              // 妲戒綅绌烘垨鏃犳晥
              resp[slot * 4] = 0x00;  // status = 锟?
              resp[slot * 4 + 1] = 0;
              resp[slot * 4 + 2] = 0;
              resp[slot * 4 + 3] = 0;
              
              printf("[SLOT %u] EMPTY - path=%s\n", slot, path ? path : "null");
            }
          }
          
          printf("================================================\n\n");
          
          // 鍙戦€佺粨鏋滅粰 MCU
          uart1_send_toMCU(0x00A4, resp, 16);
          printf("[UART] Slots status sent\n");
        }

        }

        parse_off += frame_len;
      }
    } else {
      /* 鏈鍒版暟鎹紝鐭殏璁╁嚭璋冨害锛岄伩鍏嶉暱鏃堕棿闃诲瀵艰嚧 timer_no_response */
      os_time_dly(1);
    }
  }

  // 鐭殏寤舵椂锛岄伩鍏嶈繃搴﹀崰鐢–PU
  os_time_dly(10); // 寤舵椂10ms
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
  if (!g_uart1_tx_pending) {
    return;
  }
  if (uart_is_send_complete(uart1_dev) != 1) {
    return;
  }
  s32 ret = uart1_send_raw_nonblock(g_uart1_tx_pending_buf, g_uart1_tx_pending_len);
  if (ret == g_uart1_tx_pending_len) {
    g_uart1_tx_pending = 0;
    g_uart1_tx_pending_len = 0;
  }
}

void uart1_send(u8 *data, u16 len) {
  s32 ret = uart1_send_raw_nonblock(data, len);
  if (ret != len) {
    return;
  }
  UART1_IO_LOG("uart1 send successfully: %d\n", len);
}



void uart_send_ble_key_list(uint16_t protocol_id) {
  printf("[UART] uart_send_ble_key_list: 鍙戦€佽摑鐗欓挜鍖欏垪琛╘n");

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
    printf("鎬婚挜鍖欎覆鏁伴噺瓒呰繃5锛岄檺鍒朵负5\n");
  }

  key_list_buffer[offset++] = total_key_num;
  printf("鎬婚挜鍖欎覆鏁伴噺: %d\n", total_key_num);

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

      printf("鎵嬫満钃濈墮閽ュ寵 %d: 璁惧绫诲瀷=0x00000001, MAC=%02X:%02X:%02X:%02X:%02X:%02X, 瀵嗙爜=%02X%02X%02X\n",
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

    printf("澶栬钃濈墮閽ュ寵 %d: 璁惧绫诲瀷=0x00000002, MAC=%02X:%02X:%02X:%02X:%02X:%02X, 瀵嗙爜=%02X%02X%02X%02X\n",
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
  printf("[UART] uart_send_password_key_list: 鍙戦€佸瘑鐮侀挜鍖欏垪琛╘n");
  
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
    
    printf("瀵嗙爜閽ュ寵: 璁惧绫诲瀷=0x00000004, MAC=00:00:00:00:00:00, 瀵嗙爜=%02X%02X%02X%02X\n",
           password_data[0], password_data[1], password_data[2], password_data[3]);
  } else {
    printf("鏈缃瘑鐮侀挜鍖橽n");
  }
  
  printf("Total password key data length: %d bytes\n", offset);
  
password_key_send:
  uart1_send_toMCU(protocol_id, password_key_buffer, offset);
}

//==============================鐢ㄤ簬涓插彛鏁版嵁鍙戯拷?====================================
uint8_t send_buff_toMCU[269]; // 鐢ㄤ簬鍒涘缓鏈€澶х殑鍙戦€乥uff---------->
                              // 鏍规嵁娲剧數鐨勫崗璁渶澶х殑鏁版嵁鍖呭惈269瀛楄妭
uint16_t Uart_data_len = 0;    // 鐢ㄤ簬璁板綍鍙戦€佺殑鏁版嵁闀垮害
uint8_t crc[2] = {0};
uint8_t MAX_SEND_NUM = 3; // 鏈€澶у彂閫佹锟?
static volatile u8 g_uart1_tx_pending = 0;
static u16 g_uart1_tx_pending_len = 0;
static u8 g_uart1_tx_pending_buf[269];
/**
 * @brief 鐢ㄤ簬灏嗘暟鎹彂閫佸埌MCU
 *
 * @param protocol_id 鍗忚ID
 * @param data 鏁版嵁鎸囬拡
 * @param len 鏁版嵁闀垮害
 */
bool uart1_send_toMCU(uint16_t protocol_id, uint8_t *data, uint16_t len) {
  // 閲嶈锛氫笉瑕佸湪杩欓噷娓呴櫎 uart_data锛屽洜涓哄畠鏄敤鏉ヤ繚锟?MCU 鍥炲鏁版嵁鐨勶紒
  // 鍙湁鍦ㄦ帴鏀跺埌鏂版暟鎹椂鎵嶆洿锟?uart_data
  // uart_data = NULL;  // 锟?鍒犻櫎锛氫笉搴旇鍦ㄥ彂閫佹椂娓呯┖鎺ユ敹缂撳啿锟?
  // data_length = 0;   // 锟?鍒犻櫎锛氬悓锟?

  // 鍙傛暟鏍￠獙
  if (len > UART_MAX_DATA_LEN) {
    printf("uart1_send_toMCU: invalid len %u, max %d\n", len, UART_MAX_DATA_LEN);
    return false;
  }
  if (len > 0 && data == NULL) {
    printf("uart1_send_toMCU: data is NULL but len=%u\n", len);
    return false;
  }

  // 璁＄畻CRC妫€楠屼綅锛屼娇鐢ㄥ彈闄愮紦鍐插尯浠ラ伩鍏嶅湪鏍堜笂鍒嗛厤鍙彉闀垮害鏁扮粍
  uint16_t CRC_len = (uint16_t)(len + 4); // 鍗忚ID(2)+len(2)+data(len)
  static uint8_t crcbuffer[UART_MAX_DATA_LEN + 4];
  crcbuffer[0] = (uint8_t)(protocol_id >> 8);   // 鍗忚ID楂樺瓧锟?
  crcbuffer[1] = (uint8_t)(protocol_id & 0xFF); // 鍗忚ID浣庡瓧锟?
  crcbuffer[2] = (uint8_t)(len >> 8);           // len鐨勯珮瀛楄妭
  crcbuffer[3] = (uint8_t)(len & 0xFF);         // len鐨勪綆瀛楄妭
  if (len > 0) {
    memcpy(&crcbuffer[4], data, len); // 澶嶅埗鏁版嵁
  }

  UART1_IO_LOG("crcbuffer0:%02x,crcbuffer1:%02x,crcbuffer2:%02x,crcbuffer3:%02x\n",
               crcbuffer[0], crcbuffer[1], crcbuffer[2], crcbuffer[3]);
  // printf("CRC16:");
  calculateCRC16(crcbuffer, CRC_len, crc); // 璁＄畻CRC16鏍￠獙锟?
  // printf("0x%02X%02X\n", crc[1], crc[0]);

  // 缁勮鍙戦€佺紦鍐诧紝鍏堟鏌ユ暣浣撻暱搴︿互閬垮厤瓒婄晫
  const uint16_t overhead = 2 /*head+sync*/ + 2 /*protocol id*/ + 2 /*len*/ + 2 /*crc*/ + 2 /*tail*/;
  if ((uint32_t)len + overhead > sizeof(send_buff_toMCU)) {
    printf("uart1_send_toMCU: payload too large %u, max %zu\n", len, sizeof(send_buff_toMCU) - overhead);
    return false;
  }

  Uart_data_len = 0; // 姣忔鍙戦€佸墠閮借灏嗛暱搴︽竻0
  send_buff_toMCU[Uart_data_len++] = 0xFE; // 澶村瓧鑺備互0xFE寮€锟?
  send_buff_toMCU[Uart_data_len++] = 0xAB; // 鍚屾瀛楄妭0xAB
  // 鍗忚ID
  send_buff_toMCU[Uart_data_len++] = (uint8_t)(protocol_id >> 8);
  send_buff_toMCU[Uart_data_len++] = (uint8_t)(protocol_id & 0xFF);
  // 鏁版嵁闀垮害
  send_buff_toMCU[Uart_data_len++] = (uint8_t)(len >> 8);
  send_buff_toMCU[Uart_data_len++] = (uint8_t)(len & 0xFF);
  // 鏁版嵁锟?
  if (len > 0) {
    memcpy(&send_buff_toMCU[Uart_data_len], data, len);
    Uart_data_len += len; // 鏇存柊鏁版嵁闀垮害
  }
  // CRC (浣庡瓧锟? 楂樺瓧锟?
  send_buff_toMCU[Uart_data_len++] = crc[0];
  send_buff_toMCU[Uart_data_len++] = crc[1];
  // 灏惧瓧锟?(0x0A 0x0D)
  send_buff_toMCU[Uart_data_len++] = 0x0A;
  send_buff_toMCU[Uart_data_len++] = 0x0D;

  // 鍙戦€佹暟锟?
  s32 ret = uart1_send_raw_nonblock(send_buff_toMCU, Uart_data_len);
  if (ret == Uart_data_len) {
    UART1_IO_LOG("uart1 send to MCU:");
    UART1_IO_DUMP(send_buff_toMCU, Uart_data_len);
    return true;
  }

  if (ret == UART_ERROR_BUSY) {
    if (Uart_data_len <= sizeof(g_uart1_tx_pending_buf)) {
      if (g_uart1_tx_pending) {
        printf("uart1 tx pending overwrite, drop previous\n");
      }
      memcpy(g_uart1_tx_pending_buf, send_buff_toMCU, Uart_data_len);
      g_uart1_tx_pending_len = Uart_data_len;
      g_uart1_tx_pending = 1;
    } else {
      printf("uart1 tx pending overflow, len=%u\n", Uart_data_len);
    }
    return false;
  }

  printf("uart1_send_toMCU: send failed ret=%d\n", ret);
  return false;
}
/**
 * @brief 鐢ㄤ簬澶勭悊涓插彛涓柇浜嬩欢
 *
 * @param uart_num 涓插彛缂栧彿
 * @param event 涓柇浜嬩欢
 */
static void uart_irq_func(uart_dev uart_num, enum uart_event_v1 event) {
  if (event & UART_EVENT_TX_DONE) {
    UART1_IO_LOG("uart[%d] tx done", uart_num);
  }

  if (event & UART_EVENT_RX_TIMEOUT) {
    // RX 瓒呮椂浜嬩欢锛氳〃绀轰竴甯ф暟鎹帴鏀跺畬鎴愶紙甯ч棿闅旂敱 rx_timeout_thresh 鍐冲畾锛夛拷?
    // 鍦ㄨ繖閲屼粎缃綅鏍囧織锛岄伩鍏嶅湪鍥炶皟閲屽仛澶ч噺澶勭悊锟?
    g_uart1_rx_frame_ready = 1;
  }

  if (event & UART_EVENT_RX_FIFO_OVF) {
    printf("uart[%d] rx fifo ovf", uart_num);

    // 婧㈠嚭锟?DMA/cbuf 鍙兘閿欎贡锛屼富鍔ㄩ噸缃紝閬垮厤鍚庣画涓€鐩磋В鏋愬け璐ワ拷?
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
 * @brief 鐢ㄤ簬瑙ｆ瀽涓插彛鎺ユ敹鏁版嵁
 *
 * @param rx_data 鎺ユ敹鏁版嵁鎸囬拡
 * @param data_len 鎺ユ敹鏁版嵁闀垮害
 * @return true 瑙ｆ瀽鎴愬姛
 * @return false 瑙ｆ瀽澶辫触
 */
bool uart1_parse_packet(uint8_t *rx_data, uint16_t data_len) {
  // 鎸囬拡鍙樼┖NULL
  uart_data = NULL;
  uart_rx_data = NULL;
  
  // 妫€鏌ュ紑濮嬬
  if (rx_data[0] != 0xFE) {
    printf("Invalid start byte: 0x%02X, expected 0xFE\n", rx_data[0]);
    return false;
  }
  
  // ==================== 鑷姩璇嗗埆鏂版棫鍗忚鏍煎紡 ====================
  // 鏃у崗璁細FE BA + 鍗忚ID(2B) + 闀垮害(2B) + 鏁版嵁 + CRC + 0A0D
  // 鏂板崗璁細FE + 鎬诲抚锟?2B) + 椤哄簭锟?2B) + 闀垮害(2B) + 鍗忚ID(2B) + 鏁版嵁 + CRC + 0A0D
  // 鍒ゆ柇渚濇嵁锛氱浜屼釜瀛楄妭鏄惁锟?0xBA
  
  bool is_old_protocol = (rx_data[1] == 0xBA);
  
  if (is_old_protocol) {
    // ==================== 鏃у崗璁牸寮忚В锟?====================
    printf("[UART] Old protocol format detected\n");
    
    // 鏈€灏忓寘闀挎鏌ワ細header(6) + crc(2) + tail(2) = 10瀛楄妭
    if (data_len < 10) {
      printf("uart1_parse_packet: packet too short, len=%d\n", data_len);
      return false;
    }

    // 瑙ｆ瀽鍗忚ID (澶х锟?
    uint16_t protocol_id = (rx_data[2] << 8) | rx_data[3];
    uart_protocol_id = protocol_id;

    // 瑙ｆ瀽鏁版嵁闀垮害 (澶х锟?
    data_length = (rx_data[4] << 8) | rx_data[5];
    
    // 楠岃瘉 data_length 杈圭晫
    if (data_length > UART_MAX_DATA_LEN) {
      printf("uart1_parse_packet: invalid data_length=%d (0x%04X), max=%d\n", 
             data_length, data_length, UART_MAX_DATA_LEN);
      return false;
    }

    // 妫€鏌ユ暣涓寘鏄惁瀹屾暣锛歨eader(6) + data_length + crc(2) + tail(2)
    uint16_t expected_len = 6 + data_length + 4;
    if (data_len < expected_len) {
      printf("uart1_parse_packet: incomplete packet data_len=%d expected=%d\n", data_len, expected_len);
      return false;
    }
    
    // 鎻愬彇鏁版嵁鎸囬拡锛堟暟鎹粠锟?瀛楄妭寮€濮嬶級
    uart_data = &rx_data[6];
    
    // CRC 涓ゅ瓧鑺備綅锟?data 鍚庨潰
    uint16_t crc_idx = 6 + data_length;
    uint8_t received_crc_low = rx_data[crc_idx];
    uint8_t received_crc_high = rx_data[crc_idx + 1];
    
    // 楠岃瘉灏惧瓧鑺傦紙浣嶄簬 crc 鍚庨潰涓や釜瀛楄妭锟?
    uint16_t tail_idx = crc_idx + 2;
    if (rx_data[tail_idx] != 0x0A || rx_data[tail_idx + 1] != 0x0D) {
      printf("Invalid tail bytes: 0x%02X%02X, expected 0x0A0D\n", rx_data[tail_idx], rx_data[tail_idx + 1]);
      return false;
    }
    
    // 杈撳嚭瑙ｆ瀽缁撴灉
    printf("uart1 parse packet: protocol_id=0x%04X, data_length=%d, data=",
           protocol_id, data_length);
    put_buf(uart_data, data_length);
    printf(", crc=0x%02X%02X, tail=0x%02X%02X\n", received_crc_low,
           received_crc_high, rx_data[tail_idx], rx_data[tail_idx + 1]);
    
    // 閲嶇疆澶氬寘鐘舵€侊紙鏃у崗璁笉鏀寔澶氬寘锟?
    g_is_multi_packet_mode = false;
    g_expected_packet_seq = 1;
    g_multi_packet_offset = 0;
    g_multi_packet_protocol = 0;
    
    return true;
    
  } else {
    // ==================== 鏂板崗璁牸寮忚В锟?====================
    printf("[UART] New protocol format detected\n");
    
    // 鏈€灏忓寘闀匡細header(9) + crc(2) + tail(2) = 13瀛楄妭
    if (data_len < 13) {
      printf("uart1_parse_packet: packet too short, len=%d\n", data_len);
      return false;
    }

    // 瑙ｆ瀽鎬诲抚鏁伴噺 (澶х锟?
    uint16_t total_frames = (rx_data[1] << 8) | rx_data[2];
    
    // 瑙ｆ瀽椤哄簭锟?(澶х锟?
    uint16_t seq_num = (rx_data[3] << 8) | rx_data[4];
    
    // 瑙ｆ瀽鏁版嵁闀垮害 (澶х锟?
    data_length = (rx_data[5] << 8) | rx_data[6];
    
    // 瑙ｆ瀽鍗忚ID (澶х锟?
    uint16_t protocol_id = (rx_data[7] << 8) | rx_data[8];
    
    printf("[UART] Packet: total_frames=%d, seq=%d, len=%d, protocol=0x%04X\n",
           total_frames, seq_num, data_length, protocol_id);
    
    // 楠岃瘉 data_length 杈圭晫
    if (data_length > 256) {
      printf("uart1_parse_packet: invalid data_length=%d, max=256\n", data_length);
      return false;
    }

    // 妫€鏌ユ暣涓寘鏄惁瀹屾暣锛歨eader(9) + data_length + crc(2) + tail(2)
    uint16_t expected_len = 9 + data_length + 4;
    if (data_len < expected_len) {
      printf("uart1_parse_packet: incomplete packet data_len=%d expected=%d\n", data_len, expected_len);
      return false;
    }
    
    // 鎻愬彇鏁版嵁鎸囬拡锛堟暟鎹粠锟?瀛楄妭寮€濮嬶級
    uint8_t *payload_data = &rx_data[9];
    
    // CRC 涓ゅ瓧鑺備綅锟?data 鍚庨潰
    uint16_t crc_idx = 9 + data_length;
    uint8_t received_crc_low = rx_data[crc_idx];
    uint8_t received_crc_high = rx_data[crc_idx + 1];
    
    // 楠岃瘉灏惧瓧鑺傦紙浣嶄簬 crc 鍚庨潰涓や釜瀛楄妭锟?
    uint16_t tail_idx = crc_idx + 2;
    if (rx_data[tail_idx] != 0x0A || rx_data[tail_idx + 1] != 0x0D) {
      printf("Invalid tail bytes: 0x%02X%02X, expected 0x0A0D\n", rx_data[tail_idx], rx_data[tail_idx + 1]);
      return false;
    }
    
    // ==================== 澶氬寘鎷兼帴閫昏緫 ====================
    
    // 鍒ゆ柇鏄惁涓哄崟鍖呮ā锟?
    bool is_single_packet = (total_frames == 1 && seq_num == 1);
    
    if (is_single_packet) {
      // ========== 鍗曞寘妯″紡锛氱洿鎺ュ锟?==========
      printf("[UART] Single packet mode\n");
      uart_protocol_id = protocol_id;
      uart_data = payload_data;
      
      // 閲嶇疆澶氬寘鐘舵€侊紙閬垮厤涓婁竴娆″鍖呬紶杈撶殑娈嬬暀鐘舵€侊級
      g_is_multi_packet_mode = false;
      g_expected_packet_seq = 1;
      g_multi_packet_offset = 0;
      g_multi_packet_protocol = 0;
      
    } else {
      // ========== 澶氬寘妯″紡锛氭嫾鎺ュ锟?==========
      
      if (seq_num == 1) {
        // 绗竴鍖咃細鍒濆鍖栧鍖呯紦鍐插尯
        printf("[UART] Multi-packet mode start: total=%d frames\n", total_frames);
        g_is_multi_packet_mode = true;
        g_multi_packet_protocol = protocol_id;
        g_expected_packet_seq = 1;
        g_multi_packet_offset = 0;
        
        // 娓呯┖缂撳啿锟?
        memset(g_multi_packet_buffer, 0, MAX_MULTI_PACKET_SIZE);
      }
      
      // 楠岃瘉鍗忚ID涓€鑷达拷?
      if (protocol_id != g_multi_packet_protocol) {
        printf("[UART] ERROR: Protocol ID mismatch! expected=0x%04X, got=0x%04X\n",
               g_multi_packet_protocol, protocol_id);
        g_is_multi_packet_mode = false;
        return false;
      }
      
      // 楠岃瘉椤哄簭鍙疯繛缁拷?
      if (seq_num != g_expected_packet_seq) {
        printf("[UART] ERROR: Sequence mismatch! expected=%d, got=%d\n",
               g_expected_packet_seq, seq_num);
        g_is_multi_packet_mode = false;
        return false;
      }
      
      // 妫€鏌ョ紦鍐插尯鏄惁婧㈠嚭
      if (g_multi_packet_offset + data_length > MAX_MULTI_PACKET_SIZE) {
        printf("[UART] ERROR: Multi-packet buffer overflow! offset=%d, len=%d, max=%d\n",
               g_multi_packet_offset, data_length, MAX_MULTI_PACKET_SIZE);
        g_is_multi_packet_mode = false;
        return false;
      }
      
      // 鎷兼帴鏁版嵁鍒扮紦鍐插尯
      memcpy(&g_multi_packet_buffer[g_multi_packet_offset], payload_data, data_length);
      g_multi_packet_offset += data_length;
      g_expected_packet_seq++;
      
      printf("[UART] Packet %d/%d received, offset=%d bytes\n",
             seq_num, total_frames, g_multi_packet_offset);
      
      // 鍒ゆ柇鏄惁涓烘渶鍚庝竴锟?
      if (seq_num == total_frames) {
        // ========== 鏈€鍚庝竴鍖咃細瀹屾垚鎷兼帴锛屽紑濮嬪锟?==========
        printf("[UART] Multi-packet complete! Total %d bytes\n", g_multi_packet_offset);
        
        uart_protocol_id = g_multi_packet_protocol;
        uart_data = g_multi_packet_buffer;
        data_length = g_multi_packet_offset;
        
        // 閲嶇疆澶氬寘鐘讹拷?
        g_is_multi_packet_mode = false;
        g_expected_packet_seq = 1;
        g_multi_packet_offset = 0;
        g_multi_packet_protocol = 0;
        
      } else {
        // ========== 涓棿鍖咃細缁х画绛夊緟涓嬩竴锟?==========
        printf("[UART] Waiting for next packet...\n");
        return true; // 鎴愬姛鎺ユ敹锛屼絾涓嶈Е鍙戝崗璁锟?
      }
    }
    
    // 杈撳嚭瑙ｆ瀽缁撴灉
    printf("uart1 parse packet: protocol_id=0x%04X, data_length=%d, data=",
           uart_protocol_id, data_length);
    put_buf(uart_data, data_length);
    printf(", crc=0x%02X%02X, tail=0x%02X%02X\n", received_crc_low,
           received_crc_high, rx_data[tail_idx], rx_data[tail_idx + 1]);
    
    return true;
  }
}
void uart1_recieve(void) {
  r = uart_recv_blocking(1, (void *)uart_rx_ptr, 256, 5000); // 涓€鐩寸瓑
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
