#include "fill_protocol.h"
#include "string.h"
// #include "manage_PAIBT/aes_pakcs7.h"
#include "btstack/le/att.h"
#include <stdbool.h>
#include "aes_pkcs7.h"
#include "usart0_to_mcu.h"
#include "le_multi_smartbox_module.h"
#include "user_cfg_id.h"
#include "os/os_api.h"
#include "os/os_error.h"
#include "bt_common.h"
#include "btstack/le/ble_api.h"
#include "btstack/le/le_user.h"
#include "app_main.h"
#include "asm/cpu.h"
#include "per_key_scan.h"
#include "user_info_file.h"
#include "audio_config.h"  // 音量相关 API: app_audio_set_volume, get_max_sys_vol, get_tone_vol
#include "update.h"
#include "app_power_manage.h"
#include "ota_test/ota_test.h"
#include "BLE_OTA.h"
#include "btstack_3th_protocol_user.h"
#define log_info(x, ...)       printf("[FILL_PROTOCOL]" x " ", ## __VA_ARGS__)
#define log_info_hexdump       put_buf
#define log_error(x, ...)       printf("<error>[FILL_PROTOCOL]" x " ", ## __VA_ARGS__)

/* AES 调试开关，需要确认当前实际使用的密钥来源时打开 */
#ifndef FILL_PROTOCOL_AES_DBG
#define FILL_PROTOCOL_AES_DBG  0
#endif

/* 车辆设置(0x0033)链路调试开关 */
#ifndef VEH_SET_DBG
#define VEH_SET_DBG  1
#endif

#if VEH_SET_DBG
#define VEH_SET_TRACE(tag, a, b)  printf("[VEH_SET] %s a=%u b=%u\n", (tag), (unsigned)(a), (unsigned)(b))
#else
#define VEH_SET_TRACE(tag, a, b)  do { } while (0)
#endif

/* 兼容旧 BLE 发送接口的宏；当前统一走 multi-ble 接口 */
#ifndef app_send_user_data_check
#define app_send_user_data_check  le_multi_app_send_user_data_check
#endif
#ifndef app_send_user_data
#define app_send_user_data        le_multi_app_send_user_data
#endif
#ifndef app_recieve_callback
#define app_recieve_callback      le_multi_dispatch_recieve_callback
#endif
// 外部 UART 缓冲，用于接收 MCU 返回的数据、协议 ID 和数据长度
extern uint8_t *uart_data;
extern uint16_t data_length;
extern uint16_t uart_protocol_id;
// 默认 AES CA6E67ED5CF4668BB7057361E6C65A4E
static const u8 *fill_protocol_get_aes_key(u8 out_key[16]);
static const u8 test_aes_key[16] = {
  0xCA, 0x6E, 0x67, 0xED, 0x5C, 0xF4, 0x66, 0x8B,
  0xB7, 0x05, 0x73, 0x61, 0xE6, 0xC6, 0x5A, 0x4A
};
    
uint8_t test_aes_key1[16] = {
    };
static const u8 *fill_protocol_get_aes_key(u8 out_key[16])
{
  int r = syscfg_read(CFG_DEVICE_AES_KEY, out_key, 16);
  if (r == 16) {
    for (u8 i = 0; i < 16; i++) {
      if (out_key[i] != 0) {
        return out_key;
      }
    }
  }
#if FILL_PROTOCOL_AES_DBG
  printf("[AES] use default key, syscfg_read=%d key:", r);
  put_buf(test_aes_key, 16);
#endif
  return test_aes_key;
}
uint8_t encrypt_data[1024] = {0};    // 用于存储加密后的发送数据
// 全局协议收发状态
t_ble_protocl send_protocl; // 发送协议结构，最终下发到 MCU / APP
t_ble_protocl recv_protocl; // 接收协议结构，解析手机或 MCU 上报
uint8_t protocol_content[256];
uint16_t total_frames = 0;        // 总帧数
uint16_t frame_number = 0;        // 当前帧号
uint16_t protocol_id = 0;         // 协议 ID
uint8_t *content_data = NULL;     // 协议负载指针
uint8_t content_data_buffer[256]; // 协议负载缓存
uint16_t content_length = 0;      // 协议负载长度

/* 音效相关协议处理开关：0x0075 / 0x2206 / 0x2207
 * 之所以在第一次 #if 使用前定义，是因为不同编译单元的引用顺序不一致，
 * 提前统一默认值，避免同一套代码在不同裁剪配置下出现编译差异。
 * 默认值为 1，表示直接支持 APP 音效协议；如需裁剪可在工程配置里覆盖。
 */
#ifndef FILL_PROTOCOL_MUSIC_ENABLE
#define FILL_PROTOCOL_MUSIC_ENABLE  1
#endif

// ---- 音效相关协议（0x0075/0x2206/0x2207）前置声明，避免下方调用时缺少原型 ----
#if FILL_PROTOCOL_MUSIC_ENABLE
static void get_vehice_music_infromation_instruct(uint16_t protocol_id);
#else
static void get_vehice_music_infromation_infromation_reply_empty(uint16_t protocol_id)
{
  uint8_t out[1] = {0}; // count=0
  fill_send_protocl(protocol_id, out, sizeof(out));
  uint8_t send_code[269] = {0};
  uint16_t send_code_len = convert_protocol_to_buffer(&send_protocl, send_code, sizeof(send_code));
  if (app_recieve_callback) {
    app_recieve_callback(0, send_code, send_code_len);
  }
  if (app_send_user_data_check(send_code_len)) {
    (void)app_send_user_data(
        ATT_CHARACTERISTIC_0000f7f1_0000_1000_8000_00805f9b34fb_01_VALUE_HANDLE,
        send_code, send_code_len, ATT_OP_NOTIFY);
  }
}

/* 这里不要加 inline，避免在裁剪或优化配置下未生成实体符号，导致 undefined reference */
static void get_vehice_music_infromation_instruct(uint16_t protocol_id)
{
  log_info("get_vehice_music_infromation disabled (protocol_id=0x%04x)\n", protocol_id);
  get_vehice_music_infromation_infromation_reply_empty(protocol_id);
}
#endif

/* 发送路径日志开关，避免 printf/put_buf 过多导致时序抖动或刷屏 */
#ifndef FILL_PROTOCOL_SEND_DBG
#define FILL_PROTOCOL_SEND_DBG  0
#endif

/* 本文件内部使用的兼容宏；原工程未提供定义时给一个默认值保证可编译 */
#ifndef KEY_FUNC_PASSWORD
#define KEY_FUNC_PASSWORD  0x0042
#endif

/* A5-3 NFC 钥匙功能码；头文件未定义枚举时，按协议约定使用 0x0015 */
#ifndef APP_FUNC_CODE_KEY_NFC
#define APP_FUNC_CODE_KEY_NFC  21
#endif

static inline void fill_put_u16_le(uint8_t *buf, uint16_t value)
{
  buf[0] = (uint8_t)(value & 0xFF);
  buf[1] = (uint8_t)((value >> 8) & 0xFF);
}

static inline void fill_put_u16_be(uint8_t *buf, uint16_t value)
{
  buf[0] = (uint8_t)((value >> 8) & 0xFF);
  buf[1] = (uint8_t)(value & 0xFF);
}

/* ===== 缺失外部实现时的最小兜底实现，保证工程可以正常编译 ===== */
static int is_all_zero(const uint8_t *data, uint16_t len)
{
  if (!data || !len) {
    return 1;
  }
  for (uint16_t i = 0; i < len; i++) {
    if (data[i] != 0) {
      return 0;
    }
  }
  return 1;
}

int judge_periheral_flash_macadd(const uint8_t *mac_addr)
{
  (void)mac_addr;
  /* 当前未实现外设 flash MAC 校验，默认返回未命中 */
  return 0;
}

void APP_FUNC_CODE_PARAID_HID_UNLOCK_instruct(uint16_t protocol_id, const uint8_t *payload)
{
  (void)protocol_id;
  (void)payload;
  log_info("APP_FUNC_CODE_PARAID_HID_UNLOCK stub\n");
}

void Seamless_Unlocking(uint16_t protocol_id, const uint8_t *payload)
{
  (void)protocol_id;
  (void)payload;
  log_info("Seamless_Unlocking stub\n");
}

/* 配对状态，防止重复发起 */
static volatile uint8_t pairing_in_progress = 0;
/* UART 指定的 passkey，下次配对时优先使用 */
static volatile uint8_t g_uart_pair_passkey_valid = 0;
static uint32_t g_uart_pair_passkey = 0;
extern void sm_api_request_pairing(hci_con_handle_t con_handle);
extern void reset_passkey_cb(u32 *key);
extern u32 rand32(void);
__attribute__((weak)) void reset_passkey_cb(u32 *key)
{
  if (!key) {
    return;
  }
  if (g_uart_pair_passkey_valid && g_uart_pair_passkey <= 999999) {
    *key = g_uart_pair_passkey;
    g_uart_pair_passkey_valid = 0;
    g_uart_pair_passkey = 0;
    return;
  }
  *key = rand32();
}

static volatile u8 g_pair_pending_valid = 0;
static u32 g_pair_pending_passkey = 0;

__attribute__((weak)) void smbox_pairing_set_pending(u16 conn_handle, const uint8_t code3[3], u32 passkey)
{
  (void)conn_handle;
  (void)code3;
  if (passkey > 0 && passkey <= 999999) {
    g_pair_pending_passkey = passkey;
    g_pair_pending_valid = 1;
  } else {
    g_pair_pending_passkey = 0;
    g_pair_pending_valid = 0;
  }
}

__attribute__((weak)) u32 smbox_pairing_get_pending_passkey(u16 conn_handle)
{
  (void)conn_handle;
  if (g_pair_pending_valid) {
    return g_pair_pending_passkey;
  }
  return 0;
}

__attribute__((weak)) void smbox_pairing_clear_pending(void)
{
  g_pair_pending_valid = 0;
  g_pair_pending_passkey = 0;
}

uint8_t ble_proto_ble_pair_req_Proc(uint16_t protocol_id)
{ 
  /* 先通知 MCU 已收到配对请求，保证绑定音频等流程同步 */
  uart1_send_toMCU(protocol_id, NULL, 0);
  if (pairing_in_progress) {
    log_info("ble_pairing: already in progress\n");
    return 1;
  }

  hci_con_handle_t con = smartbox_get_con_handle();
  if (con == 0) {
    log_info("ble_pairing: invalid con_handle\n");
    return 1;
  }

  pairing_in_progress = 1;

  /* 生成 6 位配对码 */
  u32 tmp32 = 0;
  if (reset_passkey_cb) {
    reset_passkey_cb(&tmp32);
  } else {
    tmp32 = (u32)rand32();
  }
  uint32_t passkey = tmp32 % 1000000;
  ble_pairing_set_uart_passkey(passkey);

  /* 记录本次 passkey，供协议栈回调时使用，确保通知 APP 的是同一组数据 */
  uint8_t code3[3] = {0};
  code3[0] = (uint8_t)(passkey / 10000);
  code3[1] = (uint8_t)((passkey / 100) % 100);
  code3[2] = (uint8_t)(passkey % 100);
  smbox_pairing_set_pending(con, code3, passkey);

  /* 组装配对状态 + 配对码（3 字节大端） */
  uint8_t pairing_state_buffer[4] = {0x01};
  pairing_state_buffer[1] = (uint8_t)((passkey >> 16) & 0xFF);
  pairing_state_buffer[2] = (uint8_t)((passkey >> 8) & 0xFF);
  pairing_state_buffer[3] = (uint8_t)(passkey & 0xFF);

  log_info("ble_pairing: send passkey=%06u\n", passkey);

  /* 组协议并通过 notify 回给 APP */
  fill_send_protocl(protocol_id, pairing_state_buffer, sizeof(pairing_state_buffer));

  uint8_t send_pairing_code[269] = {0};
  u16 send_len = convert_protocol_to_buffer(&send_protocl, send_pairing_code, sizeof(send_pairing_code));
  if (send_len == 0) {
    pairing_in_progress = 0;
    return 1;
  }

  AES_KEY aes_key;
  u16 encrypt_len = sizeof(encrypt_data);
  aes_set_encrypt_key(&aes_key, test_aes_key1, AES128_KEY_SIZE);
  aes_encrypt_pkcs(&aes_key, send_pairing_code, send_len, encrypt_data, &encrypt_len);

  if (encrypt_len > sizeof(send_pairing_code)) {
    encrypt_len = send_len;
  } else {
    memcpy(send_pairing_code, encrypt_data, encrypt_len);
  }

  if (app_recieve_callback) {
    app_recieve_callback(0, send_pairing_code, encrypt_len);
  }

  if (app_send_user_data_check(encrypt_len)) {
    int ret = app_send_user_data(
        ATT_CHARACTERISTIC_0000f7f1_0000_1000_8000_00805f9b34fb_01_VALUE_HANDLE,
        send_pairing_code, encrypt_len, ATT_OP_NOTIFY);
    if (ret == APP_BLE_NO_ERROR) {
      log_info("ble_pairing: passkey notify sent\n");
    } else {
      log_info("ble_pairing: notify failed ret=%d\n", ret);
      pairing_in_progress = 0;
      return 1;
    }
  }

  /* 最终向协议栈发起实际配对 */
  sm_api_request_pairing(con);

  pairing_in_progress = 0;
  return 0;
}

__attribute__((weak)) void ble_pairing_set_uart_passkey(uint32_t passkey)
{
  if (passkey == 0 || passkey > 999999) {
    g_uart_pair_passkey_valid = 0;
    g_uart_pair_passkey = 0;
    log_info("ble_pairing_set_uart_passkey invalid: %u\n", passkey);
    return;
  }
  g_uart_pair_passkey = passkey;
  g_uart_pair_passkey_valid = 1;
  log_info("ble_pairing_set_uart_passkey set: %06u\n", passkey);
}

void fill_protocol_set_uart_passkey(uint32_t passkey)
{
  if (passkey == 0 || passkey > 999999) {
    g_uart_pair_passkey_valid = 0;
    g_uart_pair_passkey = 0;
    log_info("fill_protocol_set_uart_passkey invalid: %u\n", passkey);
    return;
  }
  g_uart_pair_passkey = passkey;
  g_uart_pair_passkey_valid = 1;
  log_info("fill_protocol_set_uart_passkey set: %06u\n", passkey);
}

void smbox_pairing_init(void)
{
  log_info("smbox_pairing_init stub\n");
}

void smbox_pairing_on_pair_process(u8 subcode)
{
  (void)subcode;
  log_info("smbox_pairing_on_pair_process stub\n");
}

int seamless_unlock_load_targets_from_syscfg(uint8_t *mac_list, uint8_t *mac_cnt)
{
  if (mac_cnt) {
    *mac_cnt = 0;
  }
  if (mac_list) {
    memset(mac_list, 0, 24);
  }
  return 0;
}

static int fill_require_content(uint16_t protocol_id, uint16_t min_len)
{
  if (content_data == NULL || content_length < min_len) {
    log_info("protocol=0x%04x invalid payload (len=%d, need>=%d)\n", protocol_id,
             content_length, min_len);
    return 0;
  }
  return 1;
}

/* 提前声明本文件内使用的函数，避免后续定义顺序导致编译告警或冲突 */
static void vehicle_set_param_instrcut(uint16_t portocol_id);
static void send_ble_key_list(uint16_t protocol_id);
static void delete_NFC_instruct(uint16_t protocol_id);
static void empty_NFC_instruct(uint16_t protocol_id);
// static void ble_key_peripheral_send_instruct(uint16_t protocol_id);
void ble_key_peripheral_send_instruct_ext(uint16_t protocol_id);
static void delete_ble_key_instruct(uint16_t protocol_id);
static void empty_ble_key_instruct(uint16_t protocol_id);
static void send_vehicle_password_unlock_instruct(uint16_t protocol_id);
static void delet_phone_bleKey_instrcut(uint16_t protocol_id);
static void get_vehice_set_infromation_instruct(uint16_t protocol_id);
static void set_vehice_music_instruct(uint16_t protocol_id);
static void music_file_send_instruct(uint16_t protocol_id);
static void select_tone_instruct(uint16_t protocol_id);
static void remove_vehicle_binding_instruct(uint16_t protocol_id);

/* f7f2：车辆设置参数下发相关回调在本文件后部实现，这里先做前向声明 */
void send_vehicle_set_param_instruct(uint16_t protocol_id, uint8_t *instruct);

/* 钥匙/配置类通用回包：统一通过 f7f1 notify 到 APP，在 app_core 中完成 convert + AES */
static void ble_reply_to_app_f7f1_post(uint16_t protocol_id, const uint8_t *payload, uint16_t payload_len);

/* 某些模块缺失时提供的兜底声明 / stub，保证单文件可独立编译 */
uint8_t ble_proto_ble_pair_req_Proc(uint16_t protocol_id);
static int is_all_zero(const uint8_t *data, uint16_t len);
int judge_periheral_flash_macadd(const uint8_t *mac_addr);
void APP_FUNC_CODE_PARAID_HID_UNLOCK_instruct(uint16_t protocol_id, const uint8_t *payload);
void Seamless_Unlocking(uint16_t protocol_id, const uint8_t *payload);
void ble_pairing_set_uart_passkey(uint32_t passkey);
void smbox_pairing_init(void);
void smbox_pairing_on_pair_process(u8 subcode);
int seamless_unlock_load_targets_from_syscfg(uint8_t *mac_list, uint8_t *mac_cnt);

typedef struct {
  s8 soc_volume;
  u8 mapped;
} volume_apply_req_t;
static volume_apply_req_t g_vol_req;
static volatile u8 g_vol_req_pending = 0;
static s8 g_last_persist_vol = -2; // -2: 未初始化
static void volume_apply_cb(int req_ptr);
void set_volume_instruct(uint16_t protocol_id)
{
  /* 音量设置：同时更新 SOC 音量、透传 MCU，并回包给 APP
   * 当前格式为 3 bytes，定义为 [0]=code [1]=parm_id [2]=parm_value
   */
  if (!fill_require_content(protocol_id, 3)) {
    log_info("set_volume invalid payload (len=%d)\n", content_length);
    return;
  }

  u8 parm_id = content_data[1];
  u8 vol_val = content_data[2];       // APP 传入的原始档位值，1~3 表示高/中/低
  (void)parm_id;
  log_info("set_volume: parm_id=%u，APP原始档位=%u\n", (unsigned)parm_id, (unsigned)vol_val);
  /* 档位映射：1/2/3 分别表示高/中/低，值越大音量越低
   * - 如果本身就是 1/2/3，直接使用
   * - 如果传的是 0~100，则按区间映射到 1/2/3，兼容不同 APP 实现
   */
  u8 mapped;
  if (vol_val >= 1 && vol_val <= 3) {
    mapped = vol_val; // 1=高 2=中 3=低
  } else if (vol_val <= 100) {
    if (vol_val <= 33) {
      mapped = 1; // 高
    } else if (vol_val <= 66) {
      mapped = 2; // 中
    } else {
      mapped = 3; // 低
    }
  } else {
    mapped = 2;
  }

  /* ======== 计算 SOC 侧实际音量档位 ======== */
  u8 sys_max = get_max_sys_vol();
  log_info("set_volume: 系统最大音量=%d\n", sys_max);

  s8 soc_volume = 0;
  switch (mapped) {
  case 3:  // 低档：映射到相对更小的一组音量值
    soc_volume = 15;
    break;
  case 2:  // 中档：映射到中间音量值
    soc_volume = 17;
    if (soc_volume < 1) {
      soc_volume = 1;
    }
    break;
  case 1:  // 高档：映射到较大的音量值，可按产品需求继续微调
  default:
    soc_volume = 21;
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

  log_info("set_volume: raw=%u mapped=%u soc_vol=%d (sys_max=%d)\n",
           (unsigned)vol_val, (unsigned)mapped, soc_volume, sys_max);
  /* 为避免线程竞争，统一投递到 app_core 线程执行，避免在 btstack 线程直接操作 flash/音频 */
  if (!g_vol_req_pending) {
    g_vol_req.soc_volume = soc_volume;
    g_vol_req.mapped = mapped;
    g_vol_req_pending = 1;
    int msg[3];
    msg[0] = (int)volume_apply_cb;
    msg[1] = 1;
    msg[2] = (int)&g_vol_req;
    int r = os_taskq_post_type("app_core", Q_CALLBACK, 3, msg);
    if (r) {
      g_vol_req_pending = 0;
      log_info("set_volume: post volume apply failed %d, fallback direct", r);
      volume_apply_cb((int)&g_vol_req);
    }
  } else {
    log_info("set_volume: volume apply pending, skip reschedule");
  }

  /* 额外同步 MUSIC/WTONE 两类音量，减少切换场景时的瞬时差异 */
  app_audio_set_volume(APP_AUDIO_STATE_MUSIC, soc_volume, 1);
  app_audio_set_volume(APP_AUDIO_STATE_WTONE, soc_volume, 1);

  /* ======== 同步 MCU + 回包 APP ======== */
  uart1_send_toMCU(protocol_id, content_data, content_length);

  uint8_t reply_buff[3];
  reply_buff[0] = content_data[0];
  reply_buff[1] = content_data[1];
  reply_buff[2] = vol_val; // 回包给 APP 时保持原始档位值

  fill_send_protocl(protocol_id, reply_buff, 3);
  uint8_t send_code[269] = {0};
  uint16_t send_code_len = convert_protocol_to_buffer(&send_protocl, send_code, sizeof(send_code));

  if (app_recieve_callback) {
    app_recieve_callback(0, send_code, send_code_len);
  }
  if (app_send_user_data_check(send_code_len)) {
    int ret = app_send_user_data(
        ATT_CHARACTERISTIC_0000f7f1_0000_1000_8000_00805f9b34fb_01_VALUE_HANDLE,
        send_code, send_code_len, ATT_OP_NOTIFY);
    if (ret == APP_BLE_NO_ERROR) {
      log_info("set_volume reply sent OK\n");
    } else {
      log_info("set_volume reply send failed: %d\n", ret);
    }
  }
}

static void volume_apply_cb(int req_ptr)
{
  volume_apply_req_t *req = (volume_apply_req_t *)req_ptr;
  if (!req) {
    g_vol_req_pending = 0;
    return;
  }

  s8 soc_volume = req->soc_volume;
  /* 同步全局音量缓存，供重启或切场景时复用 */
  app_var.music_volume = soc_volume;
  app_var.wtone_volume = soc_volume;

  if (g_last_persist_vol == -2) {
    u8 tmp = 0;
    if (syscfg_read(CFG_MUSIC_VOL, &tmp, 1) == 1) {
      g_last_persist_vol = (s8)tmp;
    } else {
      g_last_persist_vol = -1;
    }
  }
  if (g_last_persist_vol != soc_volume) {
    int wret = syscfg_write(CFG_MUSIC_VOL, &soc_volume, 1);
    u8 verify = 0xFF;
    int rret = syscfg_read(CFG_MUSIC_VOL, &verify, 1);
    log_info("set_volume_apply: persist CFG_MUSIC_VOL=%d wret=%d rret=%d verify=%d\n",
             soc_volume, wret, rret, verify);
    g_last_persist_vol = soc_volume;
  }

  /* 仅在 MUSIC 状态下立即应用，避免影响其它音频场景 */
  u8 cur_state = app_audio_get_state();
  log_info("set_volume_apply: state=%u apply MUSIC volume=%d (mapped=%u)\n", cur_state, soc_volume, req->mapped);
  if (cur_state == APP_AUDIO_STATE_MUSIC) {
    app_audio_set_volume(APP_AUDIO_STATE_MUSIC, soc_volume, 1);
  } else {
    log_info("set_volume_apply: skip apply (state=%u)", cur_state);
  }

  g_vol_req_pending = 0;
}
typedef struct {
  uint16_t instruct;
  uint16_t protocol_id;
  uint16_t payload_len;
  uint8_t payload[64];
} fill_ble_send_req_t;

static fill_ble_send_req_t g_ble_send_req;
static volatile uint8_t g_ble_send_req_pending = 0;
static volatile uint8_t g_ble_skip_uart_forward_once = 0;

typedef struct {
  uint16_t handle;
  uint16_t len;
  uint16_t instruct;
  uint8_t op;
  uint8_t data[269];
} fill_ble_notify_req_t;

static fill_ble_notify_req_t g_ble_notify_req;
static volatile uint8_t g_ble_notify_req_pending = 0;
/* BLE 通知队列，已有 pending 时先入队 */
#define BLE_NOTIFY_QUEUE_SIZE 3
static fill_ble_notify_req_t g_ble_notify_queue[BLE_NOTIFY_QUEUE_SIZE];
static uint8_t g_ble_notify_queue_head = 0;
static uint8_t g_ble_notify_queue_tail = 0;
static uint8_t g_ble_notify_queue_cnt = 0;

typedef struct {
  uint16_t protocol_id;
  uint16_t payload_len;
  uint8_t payload[269];
} fill_ble_reply_req_t;

static fill_ble_reply_req_t g_ble_reply_req;
static volatile uint8_t g_ble_reply_req_pending = 0;

static void ble_notify_try_send(fill_ble_notify_req_t *req, uint8_t from_can_send_now);
static void volume_apply_cb(int req_ptr);

void fill_protocol_ble_tx_reset(void)
{
  g_ble_send_req_pending = 0;
  g_ble_notify_req_pending = 0;
  g_ble_reply_req_pending = 0;
  g_ble_notify_queue_cnt = 0;
  g_ble_notify_queue_head = 0;
  g_ble_notify_queue_tail = 0;
  fill_protocol_ota_on_disconnect();
}

void fill_protocol_ble_on_can_send_now(void)
{
  if (!g_ble_notify_req_pending) {
    return;
  }
  ble_notify_try_send(&g_ble_notify_req, 1);
}

static void send_data_to_ble(uint16_t instruct, uint16_t protocol_id,
                             const uint8_t *payload, uint16_t payload_len);
static void send_data_to_ble_post(uint16_t instruct, uint16_t protocol_id);
static void send_data_to_ble_post_cb(int req_ptr);

static void ble_reply_to_app_f7f1_post_cb(int req_ptr);
static void ble_reply_to_app_f7f1_do(uint16_t protocol_id, const uint8_t *payload, uint16_t payload_len);

static void ble_notify_post(uint16_t handle, const uint8_t *data, uint16_t len,
                            uint8_t op, uint16_t instruct);
static void ble_notify_post_cb(int req_ptr);
static void ble_notify_kick_queue(void);
static void volume_apply_cb(int req_ptr);

/* 0x0033 获取车辆设置信息时，切到 app_core 执行，避免阻塞 btstack 栈线程 */
static void get_vehicle_set_info_post(uint16_t protocol_id);
static void get_vehicle_set_info_post_cb(int protocol_id_int);
#define OTA_DEFAULT_MAX_PKT_LEN   180
#define OTA_BRIDGE_RET_NOT_DIRECT  3

static uint8_t g_ota_bridge_channel = RCSP_BLE;

static uint32_t fill_get_u32_be(const uint8_t *buf)
{
  return ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) | ((uint32_t)buf[2] << 8) | (uint32_t)buf[3];
}

static uint8_t ota_state_from_test(uint8_t state)
{
  switch (state) {
  case OTA_TEST_IDLE:
    return OTA_IDLE;
  case OTA_TEST_READY:
    return OTA_READY;
  case OTA_TEST_DOWNLOADING:
    return OTA_DOWNLOADING;
  case OTA_TEST_VERIFYING:
    return OTA_VERIFYING;
  case OTA_TEST_DONE:
    return OTA_DONE;
  case OTA_TEST_FAILED:
  default:
    return OTA_FAILED;
  }
}

static uint8_t ota_error_from_test(uint8_t err)
{
  switch (err) {
  case OTA_TEST_ERR_NONE:
    return OTA_ERR_NONE;
  case OTA_TEST_ERR_STATE:
    return OTA_ERR_STATE;
  case OTA_TEST_ERR_PARAM:
    return OTA_ERR_PARAM;
  case OTA_TEST_ERR_TIMEOUT:
    return OTA_ERR_TIMEOUT;
  case OTA_TEST_ERR_CRC:
    return OTA_ERR_CRC;
  case OTA_TEST_ERR_FLASH:
    return OTA_ERR_FLASH;
  case OTA_TEST_ERR_LOW_BAT:
    return OTA_ERR_LOW_BAT;
  default:
    return OTA_ERR_FLASH;
  }
}

int fill_protocol_ota_bridge_start(uint8_t channel_type, uint32_t total_size)
{
  ota_test_set_total_size(total_size);
  return ota_test_start_rcsp(channel_type);
}

int fill_protocol_ota_bridge_get_status(ota_status_info_t *info)
{
  ota_test_status_t test_status = {0};

  if (!info) {
    return -1;
  }
  if (ota_test_get_status(&test_status)) {
    return -1;
  }
  info->state = ota_state_from_test(test_status.state);
  info->last_error = ota_error_from_test(test_status.last_error);
  info->received_size = test_status.received_size;
  info->total_size = test_status.total_size;
  return 0;
}

static void ota_reply_status(uint16_t protocol_id, uint8_t ret_code, const uint8_t *extra, uint16_t extra_len)
{
  uint8_t out[32] = {0};
  uint16_t out_len = 1;
  out[0] = ret_code;
  if (extra && extra_len) {
    if (extra_len > (sizeof(out) - 1)) {
      extra_len = sizeof(out) - 1;
    }
    memcpy(out + 1, extra, extra_len);
    out_len += extra_len;
  }
  ble_reply_to_app_f7f1_post(protocol_id, out, out_len);
}

static void ota_reply_status_info(uint16_t protocol_id, uint8_t ret_code)
{
  ota_status_info_t info = {0};
  uint8_t extra[10] = {0};
  if (fill_protocol_ota_bridge_get_status(&info)) {
    info.state = OTA_IDLE;
    info.last_error = OTA_ERR_FLASH;
  }
  extra[0] = info.state;
  extra[1] = info.last_error;
  extra[2] = (uint8_t)((info.received_size >> 24) & 0xFF);
  extra[3] = (uint8_t)((info.received_size >> 16) & 0xFF);
  extra[4] = (uint8_t)((info.received_size >> 8) & 0xFF);
  extra[5] = (uint8_t)(info.received_size & 0xFF);
  extra[6] = (uint8_t)((info.total_size >> 24) & 0xFF);
  extra[7] = (uint8_t)((info.total_size >> 16) & 0xFF);
  extra[8] = (uint8_t)((info.total_size >> 8) & 0xFF);
  extra[9] = (uint8_t)(info.total_size & 0xFF);
  ota_reply_status(protocol_id, ret_code, extra, sizeof(extra));
}

static void handle_ota_cmd(uint16_t protocol_id)
{
  uint8_t rsp[8] = {0};
  ota_status_info_t info = {0};

  switch (protocol_id) {
  case ota_query_cap:
    fill_protocol_ota_bridge_get_status(&info);
    rsp[0] = 0;
    rsp[1] = 1;
    rsp[2] = 0;
    rsp[3] = (uint8_t)(OTA_DEFAULT_MAX_PKT_LEN >> 8);
    rsp[4] = (uint8_t)(OTA_DEFAULT_MAX_PKT_LEN & 0xFF);
    rsp[5] = info.state;
    ble_reply_to_app_f7f1_post(protocol_id, rsp, 6);
    break;

  case ota_enter:
    if (get_charge_online_flag() == 0 && get_self_battery_level() <= 1) {
      ota_test_on_rcsp_result(0, OTA_TEST_ERR_LOW_BAT);
      ota_reply_status_info(protocol_id, 1);
      break;
    }
    if (content_length >= 1 && (content_data[0] == RCSP_BLE || content_data[0] == RCSP_SPP)) {
      g_ota_bridge_channel = content_data[0];
    }
    if (content_length >= 4) {
      ota_test_set_total_size(fill_get_u32_be(content_data));
    }
    if (fill_protocol_ota_bridge_start(g_ota_bridge_channel, 0) == 0) {
      ota_reply_status_info(protocol_id, 0);
    } else {
      ota_reply_status_info(protocol_id, 1);
    }
    break;

  case ota_block_req:
    rsp[0] = OTA_BRIDGE_RET_NOT_DIRECT;
    rsp[1] = 0;
    rsp[2] = 0;
    rsp[3] = 0;
    rsp[4] = 0;
    rsp[5] = (uint8_t)(OTA_DEFAULT_MAX_PKT_LEN >> 8);
    rsp[6] = (uint8_t)(OTA_DEFAULT_MAX_PKT_LEN & 0xFF);
    ble_reply_to_app_f7f1_post(protocol_id, rsp, 7);
    break;

  case ota_block_data:
    ota_reply_status_info(protocol_id, OTA_BRIDGE_RET_NOT_DIRECT);
    break;

  case ota_status_query:
    ota_reply_status_info(protocol_id, 0);
    break;

  case ota_exit:
    ota_test_abort(OTA_TEST_ERR_NONE);
    ota_reply_status_info(protocol_id, 0);
    break;

  default:
    break;
  }
}

void fill_protocol_ota_on_disconnect(void)
{
  ble_ota_on_disconnect();
  ota_test_on_disconnect();
}
//============================== 车辆定时状态通知 -> 0x0012 接口 ==============================

uint8_t vehicle_control_buff[] = {
    0x01,                   // 主动上报
    0x00, 0x00, 0x00, 0x00, // 当前里程
    0x00, 0x00, 0x00, 0x00, // 总里程
    0x49,                   // 电量百分比
    0x00, 0xFF,             // 续航里程
    0x00,                   // 设防状态
    0x01,                   // 电门状态
    0x00,                   // 欠压标识
    0x00,                   // 车辆状态，0x00 表示已关闭
    0x04,                   // GPS
    0x04,                   // GSM
};
typedef struct {
  uint8_t report_type;      // 上报类型 (0x01)
  uint32_t current_mileage; // 当前里程 (4 字节)
  uint32_t total_mileage;   // 总里程 (4 字节)
  uint8_t battery_level;    // 电量百分比 (0x49 = 73%)
  uint16_t endurance;       // 续航里程 (0x00FF = 255)
  uint8_t defense_status;   // 设防状态 (0x01)
  uint8_t power_state;      // 电门状态 (0x01)
  uint8_t voltage_flag;     // 欠压标识 (0x00)
  uint8_t vehicle_status;   // 车辆状态 (0x00 表示已关闭)
  uint8_t gps_status;       // GPS 状态 (0x04)
  uint8_t gsm_status;       // GSM 状态 (0x04)
} vehicle_status_t;

// 初始化车辆状态结构，默认电门状态为 1
vehicle_status_t vehicle_status = {.power_state = 1};
static u32 vehicle_control_timer_handle = 0; // 定时器句柄
static u8 vehicle_control_timer_enabled = 0; // 定时器状态标记
extern hci_con_handle_t smartbox_get_con_handle(void);
// 定时拉取车辆状态并主动上报
static void vehicle_control_timer_handler(void) {
  /* 该函数运行在定时器回调里，尽量避免长时间阻塞。
   * 之前的 while + os_time_dly(1) 最长会拖到约 200ms，容易影响系统调度和 app_core 队列。
   * 当前改为尽量异步获取：如果 MCU 本轮已经回了 0x0012，就更新缓存；否则继续使用上一次有效数据上报。
   */
  
  static uint32_t last_current_mileage = 0;
  static uint32_t last_total_mileage = 0;
  static uint8_t last_battery_level = 0;
  static uint16_t last_endurance = 0;
  static bool first_update = true;
  
#define MAX_CURRENT_MILEAGE_DELTA 1000  // 当前里程单次最大增量限制
#define MAX_TOTAL_MILEAGE_DELTA 1000     // 总里程单次最大增量限制
#define MAX_BATTERY_LEVEL_DELTA 20        // 电量单次最大变化 20%
#define MAX_ENDURANCE_DELTA 500          // 续航单次最大变化 500
  
  // 1. 向 MCU 请求最新车辆状态
uart1_send_toMCU(0x0012, NULL, 0);
  
  // 2. 检查连接状态
  hci_con_handle_t cur_con_handle = smartbox_get_con_handle();
  if (!cur_con_handle) {
    log_info("vehicle_control_timer: no connection\n");
    return;
  }
  
  // 3. 等 MCU 回一帧，最多等约 20ms，避免一直使用旧数据
  bool data_valid = false;
  const uint16_t expect_len = sizeof(vehicle_control_buff);
  for (int retry = 0; retry < 3 && !data_valid; retry++) {
    if (retry) {
      os_time_dly(2); // 短暂等待 MCU 回包，避免长时间阻塞
    }

    OS_ENTER_CRITICAL();
    if (uart_protocol_id == 0x0012 && uart_data != NULL && data_length >= expect_len) {
      size_t copy_len = data_length;
      if (copy_len > sizeof(vehicle_control_buff)) {
        copy_len = sizeof(vehicle_control_buff);
      }
      memcpy(vehicle_control_buff, uart_data, copy_len);
      data_valid = true;

      // 这里只清理已消费的协议 ID，uart_data 本体可能还会被别处使用
      // 避免同一 protocol_id 被重复消费
      uart_protocol_id = 0;
    }
    OS_EXIT_CRITICAL();
  }
  
  // 4. 对回包数据做基础合理性校验
  if (data_valid && !first_update) {
    uint32_t new_current_mileage = (vehicle_control_buff[1] << 24) |
                                     (vehicle_control_buff[2] << 16) |
                                     (vehicle_control_buff[3] << 8) |
                                     vehicle_control_buff[4];
    uint32_t new_total_mileage = (vehicle_control_buff[5] << 24) |
                                  (vehicle_control_buff[6] << 16) |
                                  (vehicle_control_buff[7] << 8) |
                                  vehicle_control_buff[8];
    uint8_t new_battery_level = vehicle_control_buff[9];
    uint16_t new_endurance = (vehicle_control_buff[10] << 8) |
                              vehicle_control_buff[11];
    
    bool data_abnormal = false;
    
    // 检查当前里程是否异常
    if (new_current_mileage > last_current_mileage &&
        (new_current_mileage - last_current_mileage) > MAX_CURRENT_MILEAGE_DELTA) {
      log_info("vehicle_control_timer: abnormal current_mileage %u -> %u\n", 
               last_current_mileage, new_current_mileage);
      data_abnormal = true;
    }
    
    // 检查总里程是否异常
    if (new_total_mileage > last_total_mileage &&
        (new_total_mileage - last_total_mileage) > MAX_TOTAL_MILEAGE_DELTA) {
      log_info("vehicle_control_timer: abnormal total_mileage %u -> %u\n", 
               last_total_mileage, new_total_mileage);
      data_abnormal = true;
    }
    
    // 检查电量是否异常
    if ((new_battery_level > last_battery_level &&
         (new_battery_level - last_battery_level) > MAX_BATTERY_LEVEL_DELTA) ||
        (new_battery_level < last_battery_level &&
         (last_battery_level - new_battery_level) > MAX_BATTERY_LEVEL_DELTA)) {
      log_info("vehicle_control_timer: abnormal battery_level %u -> %u\n", 
               last_battery_level, new_battery_level);
      data_abnormal = true;
    }
    
    // 检查续航是否异常
    if ((new_endurance > last_endurance &&
         (new_endurance - last_endurance) > MAX_ENDURANCE_DELTA) ||
        (new_endurance < last_endurance &&
         (last_endurance - new_endurance) > MAX_ENDURANCE_DELTA)) {
      log_info("vehicle_control_timer: abnormal endurance %u -> %u\n", 
               last_endurance, new_endurance);
      data_abnormal = true;
    }
    
    if (data_abnormal) {
      log_info("vehicle_control_timer: abnormal data detected, skip update\n");
      data_valid = false;
    } else {
      // 记录本次有效数据作为历史值
      last_current_mileage = new_current_mileage;
      last_total_mileage = new_total_mileage;
      last_battery_level = new_battery_level;
      last_endurance = new_endurance;
    }
  } else if (data_valid && first_update) {
    // 首次更新时直接采用当前数据，并初始化历史值
    last_current_mileage = (vehicle_control_buff[1] << 24) |
                           (vehicle_control_buff[2] << 16) |
                           (vehicle_control_buff[3] << 8) |
                           vehicle_control_buff[4];
    last_total_mileage = (vehicle_control_buff[5] << 24) |
                         (vehicle_control_buff[6] << 16) |
                         (vehicle_control_buff[7] << 8) |
                         vehicle_control_buff[8];
    last_battery_level = vehicle_control_buff[9];
    last_endurance = (vehicle_control_buff[10] << 8) |
                     vehicle_control_buff[11];
    first_update = false;
  }
  
  // 5. 用当前缓存上报；即使 MCU 本轮没回复，也沿用上一次有效数据
  uint8_t vehicle_control_len = sizeof(vehicle_control_buff);
  uint8_t buffer[256] = {0};
  
  // 更新上报类型
  vehicle_status.report_type = vehicle_control_buff[0]; // 上报类型

  // 更新当前里程
  vehicle_status.current_mileage = (vehicle_control_buff[1] << 24) |
                                   (vehicle_control_buff[2] << 16) |
                                   (vehicle_control_buff[3] << 8) |
                                   vehicle_control_buff[4]; // 当前里程

  // 更新总里程
  vehicle_status.total_mileage = (vehicle_control_buff[5] << 24) |
                                 (vehicle_control_buff[6] << 16) |
                                 (vehicle_control_buff[7] << 8) |
                                 vehicle_control_buff[8]; // 总里程

  // 更新电量
  vehicle_status.battery_level = vehicle_control_buff[9]; // 电量

  // 更新续航
  vehicle_status.endurance = (vehicle_control_buff[10] << 8) |
                             vehicle_control_buff[11]; // 续航

  // 更新设防状态
  vehicle_status.defense_status = vehicle_control_buff[12]; // 设防状态

  // 更新电门状态
  vehicle_status.power_state = vehicle_control_buff[13]; // 电门状态

  // 更新欠压标识
  vehicle_status.voltage_flag = vehicle_control_buff[14]; // 欠压标识

  // 更新车辆状态
  vehicle_status.vehicle_status = vehicle_control_buff[15]; // 车辆状态

  // 更新 GPS 状态
  vehicle_status.gps_status = vehicle_control_buff[16]; // GPS 状态

  // 更新 GSM 状态
  vehicle_status.gsm_status = vehicle_control_buff[17]; // GSM 状态

  // 组协议结构
  fill_send_protocl(0x0012, vehicle_control_buff, vehicle_control_len);

  uint8_t f7f1_send_data[269] = {0};
  u16 f7f1_len = convert_protocol_to_buffer(&send_protocl, f7f1_send_data, 269);

  memcpy(buffer, f7f1_send_data, f7f1_len);
  static AES_KEY aes_key;
  u16 encrypt_len = sizeof(encrypt_data);
  {
    u8 keybuf[16];
    const u8 *key = fill_protocol_get_aes_key(keybuf);
    aes_set_encrypt_key(&aes_key, key, AES128_KEY_SIZE);
  }
  aes_encrypt_pkcs(&aes_key, buffer, f7f1_len, encrypt_data, &encrypt_len);
  if (encrypt_len <= sizeof(buffer)) {
    memcpy(buffer, encrypt_data, encrypt_len);
    f7f1_len = encrypt_len;
  } else {
    log_info("vehicle_control_timer: encrypt len overflow %d\n", encrypt_len);
  }
  /* 统一走 le_multi_smartbox_module 分发，由内部判断是否有订阅者 */
  le_multi_dispatch_recieve_callback(0, buffer, f7f1_len);

  if (le_multi_app_send_user_data_check(f7f1_len)) {
    if (le_multi_app_send_user_data(
            ATT_CHARACTERISTIC_0000f7f1_0000_1000_8000_00805f9b34fb_01_VALUE_HANDLE,
            buffer, f7f1_len, ATT_OP_NOTIFY) == APP_BLE_NO_ERROR) {
      // keep silent: avoid printing large payloads periodically
    } else {
      log_info("vehicle_control_timer: failed to send data\n");
    }
  } else {
    log_info("vehicle_control_timer: cannot send data now\n");
  }
}

// 启动定时器
void vehicle_control_timer_start(u32 interval_ms) {
  if (vehicle_control_timer_handle) {
    log_info("vehicle_control_timer: already running\n");
    return;
  }

  log_info("vehicle_control_timer: starting with interval %d ms\n",
           interval_ms);
  vehicle_control_timer_handle =
      sys_timer_add(NULL, vehicle_control_timer_handler, interval_ms);
  vehicle_control_timer_enabled = 1;
}

// 停止定时器
void vehicle_control_timer_stop(void) {
  if (vehicle_control_timer_handle) {
    log_info("vehicle_control_timer: stopping\n");
    sys_timeout_del(vehicle_control_timer_handle);
    vehicle_control_timer_handle = 0;
    vehicle_control_timer_enabled = 0;
  }
}

// 查询定时器状态
u8 vehicle_control_timer_is_running(void) {
  return vehicle_control_timer_enabled;
}
//======================================================================================
//------
/**
 * @brief 设置配置参数
 *
 * @param protocol_id 协议 ID
 */
bool set_configuration(uint16_t protocol_id) {
  uint8_t set_data[1];
  if (content_data == NULL || content_length < 1) {
    log_info("set_configuration: invalid content_length=%d", content_length);
    return false;
  }
  set_data[0] = content_data[0];
  // 将配置参数透传给 MCU
  uart1_send_toMCU(protocol_id, set_data, 1);
  printf("f5f1_configuration");
  if (uart_data != NULL && data_length > 0) {
    log_info("uart_data len:%d", data_length);
    log_info_hexdump(uart_data, data_length);
  }
  if (uart_data != NULL) {
    return true;
  }
  return false;
}
/**
 * @brief   处理 MCU -> SOC 协议数据
 * 
 * @param data {接收到的协议数据}
 * @param len {协议数据长度}
 * @param handle {协议句柄}
 * @retval {true: 解析成功, false: 解析失败}
 */
int fill_MCU_SOC_protocl(uint8_t *data, uint16_t len, uint16_t handle) {
  if (len < sizeof(t_ble_protocl)) {
    log_info("data len %d is too short", len);
    return 0;
  }
  // 这里先处理 S0 协议；该类数据不需要解密，收到后直接解析
  switch (handle) {
  // S0 通道 - 设备基础信息通道(f5f0/f5f1)
  case ATT_CHARACTERISTIC_0000f5f1_0000_1000_8000_00805f9b34fb_01_VALUE_HANDLE:
    // 直接解析原始协议数据
    fill_recv_protocl(data);

    break;

  // S0 通道 - 设备记录信息通道(f6f0/f6f2)
  case ATT_CHARACTERISTIC_0000f6f2_0000_1000_8000_00805f9b34fb_01_VALUE_HANDLE:
    // 直接解析原始协议数据
    fill_recv_protocl(data);

    break;

  // S0 通道 - 设备控制/透传通道(f8f0/f8f1/f8f2)
  case ATT_CHARACTERISTIC_0000f8f1_0000_1000_8000_00805f9b34fb_01_VALUE_HANDLE:
  case ATT_CHARACTERISTIC_0000f8f2_0000_1000_8000_00805f9b34fb_01_VALUE_HANDLE:
    fill_recv_protocl(data);

    break;
  default:
    break;
  }

  return 1;
}

/**
 * @brief  组装发送协议结构
 * 
 * @param cmd {协议命令}
 * @param data {协议负载}
 * @param len {负载长度}
 */
void fill_send_protocl(uint16_t cmd, uint8_t *data, uint16_t len) {
  send_protocl.head_prefix = 0xFE;
  send_protocl.f_total = 0x01;
  send_protocl.f_num = 0x01;
  /* len 字段按协议定义，仅表示 payload 长度，不包含包头、CRC、包尾等固定字段 */
  send_protocl.len = len;
  send_protocl.cmd = cmd;
  send_protocl.data = data;
  send_protocl.tail_prefix = 0x0D0A;

  // 组包后再统一计算 CRC
  uint8_t crc_packet[256];
  size_t packet_length = 0;

  // 包头 (1 字节)
  crc_packet[packet_length++] = send_protocl.head_prefix;

  // 总帧数 (2 字节，大端)
  crc_packet[packet_length++] = (send_protocl.f_total >> 8) & 0xFF;
  crc_packet[packet_length++] = send_protocl.f_total & 0xFF;

  // 当前帧号 (2 字节，大端)
  crc_packet[packet_length++] = (send_protocl.f_num >> 8) & 0xFF;
  crc_packet[packet_length++] = send_protocl.f_num & 0xFF;

  // 数据长度字段 (2 字节，大端)
  crc_packet[packet_length++] = (send_protocl.len >> 8) & 0xFF;
  crc_packet[packet_length++] = send_protocl.len & 0xFF;
  printf("发送数据长度: %d\n", send_protocl.len);

  // 协议 ID (2 字节，大端)
  crc_packet[packet_length++] = (send_protocl.cmd >> 8) & 0xFF;
  crc_packet[packet_length++] = send_protocl.cmd & 0xFF;

  // 协议负载（可变长）
  if (send_protocl.data && len > 0) {
    memcpy(&crc_packet[packet_length], send_protocl.data, len);
    packet_length += len;
  }

  // 包尾 (2 字节，大端)
  crc_packet[packet_length++] = (send_protocl.tail_prefix >> 8) & 0xFF;
  crc_packet[packet_length++] = send_protocl.tail_prefix & 0xFF;

  // 计算并写入 CRC
  uint8_t crc_data[2];
  // size_t crc_data_length = build_crc_data(crc_packet, packet_length,
  // crc_data);
  log_info("packetLength: %zu", packet_length);

  // CRC16 校验值
  calculateCRC16(crc_packet, packet_length, crc_data);
  send_protocl.crc16 = (crc_data[1] << 8) | crc_data[0];
  log_info("CRC calculated: 0x%04X", send_protocl.crc16);
}


/**
 * @brief  将协议结构转换为发送缓冲区
 * 
 * @param protocol {协议结构体指针}
 * @param buffer {输出缓冲区指针}
 * @param buffer_size {输出缓冲区大小}
 * @retval {实际写入输出缓冲区的长度}
 */
uint16_t convert_protocol_to_buffer(t_ble_protocl *protocol,uint8_t *buffer,uint16_t buffer_size)
{
    if (!protocol || !buffer ||
    buffer_size < 13) { // 最小长度：1+2+2+2+2+2+2=13 字节
    log_info("Invalid parameters for protocol conversion");
    return 0;
  }

  if ((uint32_t)buffer_size < (uint32_t)(13 + protocol->len)) {
    log_info("Buffer too small for protocol conversion: need=%u, have=%u",
             (unsigned)(13 + protocol->len), (unsigned)buffer_size);
    return 0;
  }

  uint16_t offset = 0;

  // 包头 (1 字节)
  buffer[offset++] = protocol->head_prefix;

  // 总帧数 (2 字节，大端)
  buffer[offset++] = (protocol->f_total >> 8) & 0xFF;
  buffer[offset++] = protocol->f_total & 0xFF;

  // 当前帧号 (2 字节，大端)
  buffer[offset++] = (protocol->f_num >> 8) & 0xFF;
  buffer[offset++] = protocol->f_num & 0xFF;

  // 数据长度字段 (2 字节，大端)
  buffer[offset++] = (protocol->len >> 8) & 0xFF;
  buffer[offset++] = protocol->len & 0xFF;

  // 协议 ID (2 字节，大端)
  buffer[offset++] = (protocol->cmd >> 8) & 0xFF;
  buffer[offset++] = protocol->cmd & 0xFF;

  // 协议负载（可变长）
  if (protocol->data && protocol->len > 0) {
    memcpy(&buffer[offset], protocol->data, protocol->len);
    offset += protocol->len;
  }

  // CRC16 校验值 (2 字节，高字节在前)
  buffer[offset++] = (protocol->crc16 >> 8) & 0xFF;
  buffer[offset++] = protocol->crc16 & 0xFF;

  // 包尾 (2 字节，大端)
  buffer[offset++] = (protocol->tail_prefix >> 8) & 0xFF;
  buffer[offset++] = protocol->tail_prefix & 0xFF;

  log_info_hexdump(buffer, offset);

  return offset;
}


int fill_recv_protocl(uint8_t *data)
{
     // 清空协议 ID
  protocol_id = 0;
  if (data == NULL) {
    log_info("fill_recv_protocl: data is NULL");
    return 0;
  }

  // 检查起始字节 (0xFE)
  if (data[0] != 0xFE) {
    log_info("fill_recv_protocl: invalid start byte 0x%02X, expected 0xFE",
             data[0]);
    return 0;
  }

  // 读取整包长度，从长度字段获取总长度
  uint16_t packet_length = (data[5] << 8) | data[6]; // 第 5~6 字节为长度字段
  // 检查结束符 (0x0D 0x0A)
  if (data[packet_length - 2] != 0x0D || data[packet_length - 1] != 0x0A) {
    log_info("fill_recv_protocl: invalid end bytes 0x%02X 0x%02X, expected "
             "0x0D 0x0A",
             data[packet_length - 2], data[packet_length - 1]);
    return false;
  }

  // 读取包内携带的 CRC 值，CRC 位于结束符前两个字节
  uint8_t received_crc_low = data[packet_length - 4];
  uint8_t received_crc_high = data[packet_length - 3];

  // CRC 校验：从起始字节到 CRC 字段前为止参与计算
  // 计算长度 = 整包长度 - 4（去掉 CRC 两字节和结束符两字节）
  uint16_t crc_calc_length = packet_length - 4;

  // 计算 CRC 时，从包头开始拷贝到 CRC 字段前
  uint8_t calculated_crc[2];
  // 计算时把包尾 0x0D 0x0A 补回去
  uint16_t crc_buffer_len = (uint16_t)(crc_calc_length + 2);
  uint8_t *crc_buffer = malloc(crc_buffer_len);
  if (crc_buffer == NULL) {
    log_info("fill_recv_protocl: malloc crc_buffer failed");
    return 0;
  }

  // 拷贝有效数据部分，排除 CRC 字段
  for (uint16_t i = 0; i < crc_calc_length; i++) {
    crc_buffer[i] = data[i];
  }
  // 追加结束符 (0x0D 0x0A) 参与 CRC 计算
  crc_buffer[crc_calc_length] = 0x0D;
  crc_buffer[crc_calc_length + 1] = 0x0A;

  calculateCRC16(crc_buffer, crc_buffer_len, calculated_crc);

  printf("calculated_crc: %02X%02X\n", calculated_crc[1], calculated_crc[0]);
  printf("received_crc: %02X%02X\n", received_crc_high, received_crc_low);

  if (calculated_crc[1] != received_crc_low ||
      calculated_crc[0] != received_crc_high) {
    log_info("fill_recv_protocl: CRC mismatch - received: %02X%02X, "
             "calculated: %02X%02X",
             received_crc_high, received_crc_low, calculated_crc[1],
             calculated_crc[0]);
    free(crc_buffer);
    return 0;
  }

  // 解析协议字段
  total_frames = (data[1] << 8) | data[2]; // 总帧数
  frame_number = (data[3] << 8) | data[4]; // 当前帧号
  protocol_id = (data[7] << 8) | data[8];  // 协议 ID

  // 计算协议负载长度：整包长度 - 固定字段长度(13 字节)
  content_length = packet_length - 13;
  // 协议负载起始位置
  content_data = &data[9]; // 协议负载起始位置

  log_info("fill_recv_protocl: protocol validated successfully");
  log_info("  - Total frames: %d", total_frames);
  log_info("  - Frame number: %d", frame_number);
  log_info("  - Packet length: %d bytes", packet_length);
  log_info("  - Protocol ID: 0x%04X", protocol_id);
  log_info("  - Content length: %d bytes", content_length);
  log_info("  - CRC calculated length: %d bytes (excluding CRC field only)",
           crc_calc_length);

  if (content_length > 0) {
    log_info("  - Content: ");
    log_info_hexdump(content_data, content_length);
  }

  // content_data 已直接指向 data 内部负载区，无需再次拷贝
  free(crc_buffer);
  return 1;
}

/**
 * @brief 处理手机侧收到的数据并分发到 SOC_Phone_protocol 结构
 *
 * @param data 接收数据指针
 * @param len 接收数据长度
 * @param handle 通道句柄
 * @return true 解析成功
 * @return false 解析失败
 *
 * @note
 * 先根据 handle 判断是否为 S0 / S1 协议；S0 直接解析，S1 需要先解密再解析。
 */
int fill_SOC_Phone_protocl(uint8_t *data, uint16_t len,uint16_t handle)
{
     // 先根据通道区分 S0 / S1；S0 不加密，S1 需要先解密再进入协议解析
  switch (handle) {
  // S0 通道 - 设备基础信息通道(f5f0/f5f1)
  case ATT_CHARACTERISTIC_0000f5f1_0000_1000_8000_00805f9b34fb_01_VALUE_HANDLE:
    // 直接解析原始协议数据
    fill_recv_protocl(data);
    f5f1_ID_dispose(protocol_id);
    break;

  // S0 通道 - 设备记录信息通道(f6f0/f6f2)
  case ATT_CHARACTERISTIC_0000f6f2_0000_1000_8000_00805f9b34fb_01_VALUE_HANDLE:
    log_info("S0 通道: 设备记录信息(f6f0) - 直接解析原始数据");
    fill_recv_protocl(data);
    f6f2_ID_dispose(protocol_id, content_data);

    break;
  // S0 通道 - 设备控制/透传通道(f8f0/f8f1/f8f2)
  case ATT_CHARACTERISTIC_0000f8f1_0000_1000_8000_00805f9b34fb_01_VALUE_HANDLE:
  case ATT_CHARACTERISTIC_0000f8f2_0000_1000_8000_00805f9b34fb_01_VALUE_HANDLE:
    fill_recv_protocl(data);
    switch (protocol_id) {
    case OTA_CMD_START:
    case OTA_CMD_TRANSDATA:
      ble_ota_handle_cmd(protocol_id, content_data, content_length);
      break;
    default:
      break;
    }

    break;
  // S1 通道 - 中控与 APP 的加密通信通道(f7f0/f7f2)
  case ATT_CHARACTERISTIC_0000f7f2_0000_1000_8000_00805f9b34fb_01_VALUE_HANDLE:
    /*
     * f7f2 有两种情况：一种是已经解密后的明文，首字节通常为 0xFE；
     * 之前直接先走 fill_recv_protocl(data) 会因为明文/密文混用，触发 invalid start byte；
     * 这里改为：若首字节就是 0xFE，则直接按明文解析，否则先解密后再解析。
     */
    printf("join_f7f2");
    if (data[0] == 0xFE) {
      if (fill_recv_protocl(data)) {
        f7f2_ID_dispose(protocol_id);
      }
      break;
    }
    // 密文场景下需要先解密，再继续协议解析
    /* btstack 栈空间较小，AES 相关对象放在静态区，避免栈占用过大 */
    static AES_KEY aes_key;
    static uint8_t decrypted_data[269]; // 解密后的协议数据缓冲区
    uint16_t decrypted_len = sizeof(decrypted_data);

    // 设置解密密钥，优先使用 MCU 通过 0x00F7 下发并保存到 syscfg 的密钥
    {
      u8 keybuf[16];
      const u8 *key = fill_protocol_get_aes_key(keybuf);
      aes_set_decrypt_key(&aes_key, key, AES128_KEY_SIZE);
    }
    // 尝试 PKCS7 解密
    int aes_ret = aes_decrypt_pkcs(&aes_key, data, len, decrypted_data, &decrypted_len);
    if (aes_ret != 0) {
      /* 部分手机使用“纯 AES-ECB 分组”且长度刚好是 16 字节对齐，PKCS7 校验会报 padd_num error。
         这种情况下退回到逐块 ECB 解密，兼容旧手机侧实现。 */
      decrypted_len = len;
      for (uint16_t i = 0; i < len / AES_BLOCK_SIZE; i++) {
        aes_decrypt(&aes_key, &data[i * AES_BLOCK_SIZE], &decrypted_data[i * AES_BLOCK_SIZE]);
      }
    }
    printf("decrypted_data:");
    /* btstack 栈线程里限制 hexdump 长度，避免日志过长影响时序 */
    {
      uint16_t dump_len = decrypted_len;
      if (dump_len > 32) {
        dump_len = 32;
      }
      log_info_hexdump(decrypted_data, dump_len);
    }
    // 使用解密后的数据继续走协议解析
    if (fill_recv_protocl(decrypted_data)) {
      f7f2_ID_dispose(protocol_id);
    }

    break;

  default:
    break;
    return 1;
  }

  return 0;
}

static void get_vehicle_set_info_post(uint16_t protocol_id) {
  /* 之前这里同时做 app_core 异步投递和直接回调，容易造成重复处理。
   * 现在保留当前路径直接执行，额外异步投递一次用于兼容旧流程，但以当前执行结果为准。 */

  get_vehice_set_infromation_instruct(protocol_id);

  int msg[3];
  msg[0] = (int)get_vehicle_set_info_post_cb;
  msg[1] = 1;
  msg[2] = (int)protocol_id;
  int r = os_taskq_post_type("app_core", Q_CALLBACK, 3, msg);
  if (r) {
    log_info("get_vehicle_set_info_post: post failed %x\n", r);
  }
}

static void get_vehicle_set_info_post_cb(int protocol_id_int) {
  uint16_t pid = (uint16_t)protocol_id_int;
  get_vehice_set_infromation_instruct(pid);
}


void f5f1_ID_dispose(uint16_t protocol_id)
{
     bool ret;
  switch (protocol_id) {
  case equipment_information:
    set_configuration(protocol_id);

    break;
  case Battery_communication_configuration:
    ret = set_configuration(protocol_id);
    if (ret) {
      if (uart_data == 0) {
        // 无通信状态
        printf("set_configuration success");
        //play_tone_file_alone(get_tone_files()->no_com);
        printf("no_com");
        // 如需清空 uart_data，可在这里处理
        // memset(uart_data, 0, data_length);
      } else {
        // 有通信状态
        printf("set_configuration success");
        //play_tone_file_alone(get_tone_files()->yes_com);
        printf("yes_com");
        // 如需清空 uart_data，可在这里处理
        // memset(uart_data, 0, data_length);
      }

    } else {
      printf("set_configuration fail");
    }
    break;
  case Battery_voltage_configuration:
    ret = set_configuration(protocol_id);
    if (ret) {
      printf("set_configuration success");
      if (content_data[0] == 0) {
        //play_tone_file_alone(get_tone_files()->statu_48v);
        printf("statu_48v");
        // 如需清空 uart_data，可在这里处理
        // memset(uart_data, 0, data_length);
      }
      if (content_data[0] == 1) {
        //play_tone_file_alone(get_tone_files()->statu_60v);
        printf("statu_60v");
        // 如需清空 uart_data，可在这里处理
        // memset(uart_data, 0, data_length);
      }
      if (content_data[0] == 2) {
        //play_tone_file_alone(get_tone_files()->statu_72v);
        printf("statu_72v");
        // 如需清空 uart_data，可在这里处理
        // memset(uart_data, 0, data_length);
      }
      if (content_data[0] == 3) {
        //play_tone_file_alone(get_tone_files()->statu_48v);
        printf("statu_48v");
        // 如需清空 uart_data，可在这里处理
        // memset(uart_data, 0, data_length);
      }
    }

    break;
  case speed_limit:
    ret = set_configuration(protocol_id);
    if (ret) {
      printf("set_configuration success");
      if (content_data[0] == 0) {
        //play_tone_file_alone(get_tone_files()->recover);
        printf("recover");
        // 如需清空 uart_data，可在这里处理
        // memset(uart_data, 0, data_length);
      }
      if (content_data[0] == 1) {
        //play_tone_file_alone(get_tone_files()->relieve);
        printf("relieve");
        // 如需清空 uart_data，可在这里处理
        // memset(uart_data, 0, data_length);
      }
    }

    break;
  case Clear_total_mileage:
    ret = set_configuration(protocol_id);
    if (ret) {
      printf("set_configuration success");
      if (content_data[0] == 0) {
        //play_tone_file_alone(get_tone_files()->set_ok);
        printf("set_ok");
        // 如需清空 uart_data，可在这里处理
        // memset(uart_data, 0, data_length);
      }
      if (content_data[0] == 1) {
        //play_tone_file_alone(get_tone_files()->erro_try);
        printf("erro_try");
        // 如需清空 uart_data，可在这里处理
        // memset(uart_data, 0, data_length);
      }
    }
    break;
  case tone_control:
    ret = set_configuration(protocol_id);
    if (ret) {
      printf("set_configuration success");
      if (content_data[0] == 0) {
        //play_tone_file_alone(get_tone_files()->relieve);
        printf("relieve");
        // 如需清空 uart_data，可在这里处理
        // memset(uart_data, 0, data_length);
      }
      if (content_data[0] == 1) {
        //play_tone_file_alone(get_tone_files()->recover);
        printf("recover");
        // 如需清空 uart_data，可在这里处理
        // memset(uart_data, 0, data_length);
      }
    }
    break;
  case Battery_nominal_capacity_configuration: {
    uint8_t set_data[2];
    if (content_data == NULL || content_length < 2) {
      log_info("Battery_nominal_capacity_configuration: invalid content_length=%d", content_length);
      break;
    }
    set_data[0] = content_data[0];
    set_data[1] = content_data[1];
    // 将配置参数透传给 MCU
    uart1_send_toMCU(protocol_id, set_data, 2);
    printf("f5f1_configuration");
    if (uart_data != NULL && data_length > 0) {
      log_info("uart_data len:%d", data_length);
      log_info_hexdump(uart_data, data_length);
    }
    if (uart_data != NULL && data_length >= 1 && uart_data[0] == 1) {
      printf("set_configuration success");
      //play_tone_file_alone(get_tone_files()->recover);
      printf("recover");
      // 如需清空 uart_data，可在这里处理
      // memset(uart_data, 0, data_length);
    } else {
      printf("set_configuration failed");
      //play_tone_file_alone(get_tone_files()->erro_try);
      printf("erro_try");
    }
    break;
  }
  case Battery_cell_type_configuration:
    ret = set_configuration(protocol_id);
    if (ret) {
      printf("set_configuration success");
      //play_tone_file_alone(get_tone_files()->add_ok);
      printf("add_ok");
      // 如需清空 uart_data，可在这里处理
      // memset(uart_data, 0, data_length);
    } else {
      printf("set_configuration fail");
      //play_tone_file_alone(get_tone_files()->erro_try);
      printf("erro_try");
    }
    break;
  default:
    break;
  }
}

void f6f2_ID_dispose(uint16_t protocol_id, uint8_t *data)
{
     switch (protocol_id) {
  case login_ID:
    /* code */
    //uart1_send_toMCU(protocol_id, NULL, 0);
    uart1_send_toMCU(protocol_id, data, 4);
    log_info("已将当前登录指令发送到 MCU");
    log_info_hexdump(data, 4);
    break;
  case support_function:
    // 通过串口查询支持功能列表
    uart1_send_toMCU(protocol_id, NULL, 0);
    printf("support_function");
    if (uart_data != NULL && data_length > 0) {
      log_info("uart_data len:%d", data_length);
      log_info_hexdump(uart_data, data_length);
    }
    uint8_t ble_support_data[]=
    {
    0x00,0x3A,0x00,0x39, 0x00,0x01, 0x00, 0x01, 0x00, 0x4D, 0x21, 0x09, 0x00, 0x08,
        0x00, 0x09, 0x00, 0x0B, 0x00, 0x0C, 0x00, 0x0D, 0x00, 0x15, 0x00,
        0x13, 0x00, 0x14, 0x00, 0x16, 0x00, 0x38, 0x00, 0x1F, 0x00, 0x20,
        0x00, 0x21, 0x00, 0x22, 0x00, 0x23, 0x00, 0x24, 0x00, 0x25, 0x00,
        0x26, 0x00, 0x27, 0x00, 0x28, 0x00, 0x29, 0x00, 0x2A, 0x00, 0x2B,
        0x00, 0x2C, 0x00, 0x2D, 0x00, 0x2E, 0x00, 0x2F, 0x00, 0x30, 0x00,
        0x31, 0x00, 0x32, 0x00, 0x33, 0x00, 0x34
    };
    /* 将支持功能数据封装成协议并回传给 App */
    fill_send_protocl(support_function, ble_support_data, sizeof(ble_support_data));
    uint8_t send_code[269] = {0};
    uint16_t send_code_len =
        convert_protocol_to_buffer(&send_protocl, send_code, sizeof(send_code));
    if (send_code_len == 0) {
      log_info("support_function: convert failed\n");
      break;
    }
    le_multi_dispatch_recieve_callback(0, send_code, send_code_len);
    if (le_multi_app_send_user_data_check(send_code_len)) {
      le_multi_app_send_user_data(
          ATT_CHARACTERISTIC_0000f6f1_0000_1000_8000_00805f9b34fb_01_VALUE_HANDLE,
          send_code, send_code_len, ATT_OP_NOTIFY);
      log_info("back_to_app data:");
      log_info_hexdump(send_code, send_code_len);
    }
    break;
  default:
    break;
  }
}
//============================== f7f2 协议 ID 分发处理 ========================================
/**
 * @brief  车辆参数设置指令发送处理
 * 
 * @param protocol_id {协议 ID}
 */
void send_vehicle_set_param_instruct(uint16_t protocol_id, uint8_t *instruct) {
  // if (content_data == NULL || content_length < 3) {
  //   log_info("send_vehicle_set_param_instruct: invalid content_length=%d", content_length);
  //   return;
  // }
  
  // 特殊处理超时落锁参数，防止参数越界
  if (protocol_id == 0x00F2) {
    uint8_t param_id = content_data[1];
    uint8_t param_value = content_data[2];
    
    if (param_id == APP_FUNC_CODE_PARAID_OVER_TIME_LOCK) {
      log_info("send_vehicle_set_param_instruct: OVER_TIME_LOCK setting detected, value=%d\n", param_value);
      // 确保参数值在有效范围内
      if (param_value > 2) {
        log_info("send_vehicle_set_param_instruct: OVER_TIME_LOCK value %d out of range, clamping to 2\n", param_value);
        content_data[2] = 2;
      }
    }
  }
  uart1_send_toMCU(protocol_id, content_data, content_length);
  // 等待响应
  int timeout = 20; // 200ms
  while (timeout--) {
    if (uart_protocol_id == protocol_id && uart_data != NULL) {
      break;
    }
    os_time_dly(1);
  }

  uint8_t ret = 0;
  uint8_t set_code_buffer[64] = {0x00, 0x09, 0x01};
  set_code_buffer[0] = content_data[0];
  set_code_buffer[1] = content_data[1];
  set_code_buffer[2] = content_data[2];
  if (uart_data != NULL && data_length > 0) {
    uint16_t copy_len = data_length;
    if (copy_len > sizeof(set_code_buffer)) {
      copy_len = sizeof(set_code_buffer);
    }
    memcpy(set_code_buffer, uart_data, copy_len);
    // 复位串口响应状态
    uart_protocol_id = 0;
    uart_data = NULL;
    data_length = 0;
    uart1_reset_retry_state();
  } else {
    printf("uart_data is null or timeout\n");
    uart1_reset_retry_state();
  }

  fill_send_protocl(protocol_id, set_code_buffer, 3);
  uint8_t send_code[269] = {0};
  uint16_t send_code_len =
      convert_protocol_to_buffer(&send_protocl, send_code, 269);
  le_multi_dispatch_recieve_callback(0, send_code, send_code_len);
  // 对回包数据做 AES 加密
  AES_KEY aes_key;

  u16 encrypt_len = sizeof(encrypt_data);
  {
    u8 keybuf[16];
    const u8 *key = fill_protocol_get_aes_key(keybuf);
    aes_set_encrypt_key(&aes_key, key, AES128_KEY_SIZE);
  }
  // 开始加密
  aes_encrypt_pkcs(&aes_key, send_code, send_code_len, encrypt_data,
                   &encrypt_len);
  printf("encrypt_data 加密后的数据:");
  log_info_hexdump(encrypt_data, encrypt_len);
  printf("encrypt_len: %d, send_data_len: %d\n", encrypt_len, send_code_len);

  // 校验加密后长度
  if (encrypt_len > sizeof(send_code)) {
    log_info("加密数据长度 %zu 超过发送缓冲区 %zu", encrypt_len,
             sizeof(send_code));
    // 回退为原始协议长度
    encrypt_len = send_code_len;
    memcpy(send_code, send_code, send_code_len);
  } else {
    memcpy(send_code, encrypt_data, encrypt_len);
  }
  printf("最终发送数据:");
  log_info_hexdump(send_code, encrypt_len);
  le_multi_dispatch_recieve_callback(0, send_code, encrypt_len);

  if (le_multi_app_send_user_data_check(encrypt_len)) {
    if (le_multi_app_send_user_data(
            ATT_CHARACTERISTIC_0000f7f1_0000_1000_8000_00805f9b34fb_01_VALUE_HANDLE,
            send_code, encrypt_len, ATT_OP_NOTIFY) == APP_BLE_NO_ERROR) {
      log_info("set instruct: %d, data sent successfully\n", instruct);
      log_info_hexdump(send_code, encrypt_len);
    } else {
      log_info("set instruct: %d, failed to send data\n", instruct);
    }
  }
}


void f7f2_ID_dispose(uint16_t protocol_id)
{
     switch (protocol_id) {
  case vehicle_control: // 车辆控制
    if (!fill_require_content(protocol_id, 2)) {
      break;
    }
    log_info("now content_data:");
    log_info("content_data1: %d", content_data[0]);
    log_info("content_data2: %d", content_data[1]);
    uint8_t instruct = content_data[1];
    v5_1_function_control(instruct, protocol_id);
    break;
  case vehicle_state: // 获取车辆状态
    /* code */        // 这里由 f7f2 的 write_callback 周期性触发
    break;
  case vehicle_set_param: // 设置车辆参数
    /* code */
    vehicle_set_param_instrcut(protocol_id);
    break;
  case get_key_list: // 获取钥匙列表
    send_ble_key_list(protocol_id);
    /* code */
    break;
  case delete_NFC: // 删除 NFC 钥匙
    if (!fill_require_content(protocol_id, 16)) {
      break;
    }
    delete_NFC_instruct(protocol_id);
    break;
  case empty_NFC: // 清空 NFC 钥匙
    empty_NFC_instruct(protocol_id);
    break;
  case ble_pairing: // 蓝牙配对
    ble_proto_ble_pair_req_Proc(protocol_id);

    // ble_proto_ble_pair_req_Proc(NULL);

    break;
  case ble_key_peripheral_send: // APP 下发蓝牙外设钥匙
    /* code */
    if (!fill_require_content(protocol_id, 6)) {
      break;
    }
    ble_key_peripheral_send_instruct_ext(protocol_id);
    break;
  case delete_ble_key: // 删除蓝牙外设钥匙
    if (!fill_require_content(protocol_id, 6)) {
      break;
    }
    delete_ble_key_instruct(protocol_id);
    break;
  case empty_ble_key: // 清空蓝牙外设钥匙
    empty_ble_key_instruct(protocol_id);
    break;
  case send_vehicle_password_unlock: // 下发车辆密码钥匙
    /* code */
    if (!fill_require_content(protocol_id, 4)) {
      break;
    }
    send_vehicle_password_unlock_instruct(protocol_id);
    break;
  case remove_vehicle_binding: // 解除车辆绑定
    /* code */
    remove_vehicle_binding_instruct(protocol_id);
    break;
  case delet_phone_key: // 删除手机钥匙
    if (!fill_require_content(protocol_id, 3)) {
      break;
    }
    delet_phone_bleKey_instrcut(protocol_id);
    break;
  case empty_phone_key: // 清空手机钥匙
    /* code */  
    break;
  case get_vehice_set_infromation: // 获取车辆设置信息
      /* 该协议 payload 为空，整条链路包含 UART 等待、AES 和 notify，统一放到 app_core 执行 */
      get_vehicle_set_info_post(protocol_id);
    break;
  case get_vehice_music_infromation: // 获取车辆音效信息
    get_vehice_music_infromation_instruct(protocol_id);
    break;
  case select_tone: // 选择音效：默认或自定义
    select_tone_instruct(protocol_id);
    break;
  case set_vehice_music: // 设置音效信息
    set_vehice_music_instruct(protocol_id);
    break;
  case music_file_send: // 发送音效文件数据
    music_file_send_instruct(protocol_id);
    break;
  case open_lock: // 打开车锁
   // set_vehice_music_instruct(protocol_id);
    break;
  case OTA_CMD_START:
  case OTA_CMD_TRANSDATA:
    ble_ota_handle_cmd(protocol_id, content_data, content_length);
    break;
  case ota_query_cap:
  case ota_enter:
  case ota_block_req:
  case ota_block_data:
  case ota_status_query:
  case ota_exit:
    handle_ota_cmd(protocol_id);
    break;
  default:
    break;
  }
}
/**
 * @brief 车辆控制参数设置指令 V5.2
 * 数据格式：3 byte，2 byte->code，1 byte->parm
 * @param content_data {placeholder} 车辆控制参数设置指令内容
 */
void param_set_v5_2_instruct(uint16_t portocol_id, uint8_t *content_data) {
  // 解析 content_data 参数
  uint8_t parm_id = 0;
  // 第 6 字节 HEX 转为十进制参数
  printf("content_data1:%d", content_data[1]);
  printf("content_data2:%d", content_data[2]);
  parm_id = content_data[1];
  printf("parm_id:%d", parm_id);
  switch (parm_id) {
  case CFG_VEHICLE_PASSWORD_UNLOCK: // 设置车辆密码解锁
    /* code */
    printf("匹配到 CFG_VEHICLE_PASSWORD_UNLOCK\n");
    send_vehicle_set_param_instruct(portocol_id, content_data);
    break;
  case APP_FUNC_CODE_PARAID_AUTO_LOCK: // 自动落锁
    printf("匹配到 APP_FUNC_CODE_PARAID_AUTO_LOCK\n");
    send_vehicle_set_param_instruct(portocol_id, content_data);
    break;
  case APP_FUNC_CODE_PARAID_OVER_TIME_LOCK: // 超时落锁
    printf("匹配到 APP_FUNC_CODE_PARAID_OVER_TIME_LOCK\n");
    send_vehicle_set_param_instruct(portocol_id, content_data);
    break;
  case APP_FUNC_CODE_PARAID_AUTO_P: // 自动 P 档
    printf("匹配到 APP_FUNC_CODE_PARAID_AUTO_P\n");
    send_vehicle_set_param_instruct(portocol_id, content_data);
    break;
  case APP_FUNC_CODE_PARAID_SIDE_PROP: // 边撑感应
    printf("匹配到 APP_FUNC_CODE_PARAID_SIDE_PROP\n");
    send_vehicle_set_param_instruct(portocol_id, content_data);
    break;
  case APP_FUNC_CODE_PARAID_CUSHION: // 坐垫感应
    printf("匹配到 APP_FUNC_CODE_PARAID_CUSHION\n");
    send_vehicle_set_param_instruct(portocol_id, content_data);
    break;
  case APP_FUNC_CODE_PARAID_DELAY_HEADLIGHT: // 延时大灯
    printf("匹配到 APP_FUNC_CODE_PARAID_DELAY_HEADLIGHT\n");
    send_vehicle_set_param_instruct(portocol_id, content_data);
    break;
  case APP_FUNC_CODE_PARAID_SENSE_HEADLIGHT: // 感应大灯
    printf("匹配到 APP_FUNC_CODE_PARAID_SENSE_HEADLIGHT\n");
    send_vehicle_set_param_instruct(portocol_id, content_data);
    break;
  case APP_FUNC_CODE_PARAID_NFC: // NFC 开关
    printf("匹配到 APP_FUNC_CODE_PARAID_NFC\n");
    send_vehicle_set_param_instruct(portocol_id, content_data);
    break;
  case APP_FUNC_CODE_PARAID_SOUND: // 音效开关
    printf("匹配到 APP_FUNC_CODE_PARAID_SOUND\n");
    send_vehicle_set_param_instruct(portocol_id, content_data);
    break;
  case APP_FUNC_CODE_PARAID_HID_DISTANCE: // 无感解锁距离
    printf("匹配到 APP_FUNC_CODE_PARAID_HID_DISTANCE\n");
    APP_FUNC_CODE_PARAID_HID_UNLOCK_instruct(portocol_id, content_data);
    send_vehicle_set_param_instruct(portocol_id, content_data);
    break;
  case APP_FUNC_CODE_PARAID_CRUISE: // 定速巡航
    printf("匹配到 APP_FUNC_CODE_PARAID_CRUISE\n");
    send_vehicle_set_param_instruct(portocol_id, content_data);
    break;
  case APP_FUNC_CODE_PARAID_ASSIST: // 辅助推行
    printf("匹配到 APP_FUNC_CODE_PARAID_ASSIST\n");
    send_vehicle_set_param_instruct(portocol_id, content_data);
    break;
  case APP_FUNC_CODE_PARAID_ASTERN: // 倒车辅助
    printf("匹配到 APP_FUNC_CODE_PARAID_ASTERN\n");
    send_vehicle_set_param_instruct(portocol_id, content_data);
    break;
  case APP_FUNC_CODE_PARAID_HID_UNLOCK: // 无感解锁
    printf("匹配到 APP_FUNC_CODE_PARAID_HID_UNLOCK\n");
    send_vehicle_set_param_instruct(portocol_id, content_data);
    Seamless_Unlocking(portocol_id, content_data);
    break;
  case APP_FUNC_CODE_PARAID_AUTO_LOCK_TIME: // 自动落锁时间
    printf("匹配到 APP_FUNC_CODE_PARAID_AUTO_LOCK_TIME\n");
    send_vehicle_set_param_instruct(portocol_id, content_data);
    break;
  case APP_FUNC_CODE_PARAID_OVER_TIME_LOCK_TIME: // 超时落锁时间
    printf("匹配到 APP_FUNC_CODE_PARAID_OVER_TIME_LOCK_TIME\n");
    send_vehicle_set_param_instruct(portocol_id, content_data);
    break;

  case APP_FUNC_CODE_PARAID_VOLUME: // 音量设置
    printf("匹配到 APP_FUNC_CODE_PARAID_VOLUME\n");
    set_volume_instruct(portocol_id);

    break;
  case APP_FUNC_CODE_PARAID_ALARM_SHAKE: // 震动报警开关
    printf("匹配到 APP_FUNC_CODE_PARAID_ALARM_SHAKE\n");
    send_vehicle_set_param_instruct(portocol_id, content_data);
    break;
  case APP_FUNC_CODE_PARAID_ALARM_DUMP: // 倾倒报警
    printf("匹配到 APP_FUNC_CODE_PARAID_ALARM_DUMP\n");
    send_vehicle_set_param_instruct(portocol_id, content_data);
    break;
  case APP_FUNC_CODE_PARAID_ALARM_MOVE: // 移动报警
    printf("匹配到 APP_FUNC_CODE_PARAID_ALARM_MOVE\n");
    send_vehicle_set_param_instruct(portocol_id, content_data);
    break;
  case APP_FUNC_CODE_PARAID_ALARM_SHAKE_SENSE: // 震动报警灵敏度
    printf("匹配到 APP_FUNC_CODE_PARAID_ALARM_SHAKE_SENSE\n");
    send_vehicle_set_param_instruct(portocol_id, content_data);
    break;
  case APP_FUNC_CODE_PARAID_FEST_EFFECT: // 节日音效
    printf("匹配到 APP_FUNC_CODE_PARAID_FEST_EFFECT\n");
    send_vehicle_set_param_instruct(portocol_id, content_data);
    break;
  case APP_FUNC_CODE_PARAID_FACTORY_RESET: // 恢复出厂
    printf("匹配到 APP_FUNC_CODE_PARAID_FACTORY_RESET\n");
    send_vehicle_set_param_instruct(portocol_id, content_data);
    break;
  case APP_FUNC_CODE_PARAID_TCS: // 牵引力控制
    printf("匹配到 APP_FUNC_CODE_PARAID_TCS\n");
    send_vehicle_set_param_instruct(portocol_id, content_data);
    break;
  case APP_FUNC_CODE_PARAID_ABS: // 防抱死刹车
    printf("匹配到 APP_FUNC_CODE_PARAID_ABS\n");
    send_vehicle_set_param_instruct(portocol_id, content_data);
    break;
  case APP_FUNC_CODE_PARAID_HHC: // 坡道驻车
    printf("匹配到 APP_FUNC_CODE_PARAID_HHC\n");
    send_vehicle_set_param_instruct(portocol_id, content_data);
    break;
  default:
    printf("未匹配到任何 case，parm_id = %d\n", parm_id);
    break;
  }
}
void vehicle_set_param_instrcut(uint16_t portocol_id) {
  param_set_v5_2_instruct(portocol_id, content_data);
}
// 电门状态回包采用延时发送，避免与控制指令挤占同一时隙
static struct {
  uint8_t pending;
  uint8_t original_state;
  uint8_t new_state;
  uint8_t reply_buff[20]; // 足够容纳 vehicle_control_buff
} g_power_state_reply_ctx = {0};

// 延时回包回调
static void reply_power_state_delay_cb(void *priv) {
  (void)priv;
  
  if (!g_power_state_reply_ctx.pending) {
    return;
  }
  g_power_state_reply_ctx.pending = 0;
  
  log_info("延时发送电门状态回包: %d -> %d\n", 
           g_power_state_reply_ctx.original_state, 
           g_power_state_reply_ctx.new_state);
  
  // 组协议结构并回复 0x0012
  fill_send_protocl(0x0012, g_power_state_reply_ctx.reply_buff, sizeof(vehicle_control_buff));
  
  static uint8_t f7f1_send_data[269];
  memset(f7f1_send_data, 0, sizeof(f7f1_send_data));
  u16 f7f1_len = convert_protocol_to_buffer(&send_protocl, f7f1_send_data, sizeof(f7f1_send_data));
  
  if (f7f1_len == 0) {
    log_info("reply_power_state_delay: convert failed\n");
    return;
  }
  
  // 通过回调分发
  le_multi_dispatch_recieve_callback(0, f7f1_send_data, f7f1_len);
  
  // notify 给 APP
  if (le_multi_app_send_user_data_check(f7f1_len)) {
    if (le_multi_app_send_user_data(
            ATT_CHARACTERISTIC_0000f7f1_0000_1000_8000_00805f9b34fb_01_VALUE_HANDLE,
            f7f1_send_data, f7f1_len, ATT_OP_NOTIFY) == APP_BLE_NO_ERROR) {
      log_info("电门状态回包成功: %d -> %d\n", 
               g_power_state_reply_ctx.original_state, 
               g_power_state_reply_ctx.new_state);
    } else {
      log_info("电门状态回包发送失败\n");
    }
  } else {
    log_info("电门状态回包：当前无法发送\n");
  }
}

/**
 * @brief 电门控制后回给 APP 当前状态，延时 200ms 发送，避免 BLE 拥塞
 * @param instruct 控制指令(8=上电/解锁, 9=下电/落锁)
 * @param protocol_id 协议 ID
 * @param original_power_state 原始电门状态
 * @param new_power_state 新的电门状态
 */
static void reply_power_state_change_to_app(uint16_t instruct, uint16_t protocol_id, 
                                             uint8_t original_power_state, uint8_t new_power_state) {
  (void)instruct;
  (void)protocol_id;
  
  log_info("电门状态变化：原状态=%d，新状态=%d（延时200ms回复）\n", original_power_state, new_power_state);
  
  // 复制 0x0012 状态缓存
  memcpy(g_power_state_reply_ctx.reply_buff, vehicle_control_buff, sizeof(vehicle_control_buff));
  
  // 标记为回复包而不是主动上报
  g_power_state_reply_ctx.reply_buff[0] = 0x00; // 0=回复，1=主动上报
  
  // 更新电门状态为新状态
  g_power_state_reply_ctx.reply_buff[13] = new_power_state;
  
  // 同步 vehicle_status 结构
  vehicle_status.power_state = new_power_state;
  
  // 保存状态变化上下文
  g_power_state_reply_ctx.original_state = original_power_state;
  g_power_state_reply_ctx.new_state = new_power_state;
  g_power_state_reply_ctx.pending = 1;
  
  // 延时 200ms 发送，避免与 0x00F1 回复同时占用 BLE buffer
  sys_timeout_add(NULL, reply_power_state_delay_cb, 200);
}

/**
 * @brief 处理车辆控制指令 f7f1 vehicle_control
 *
 * @param instruct 车辆控制指令
 */
void v5_1_function_control(uint16_t instruct, uint16_t protocol_id) {
  /* 车辆控制 payload 异常时直接返回，避免后续 UART / 回包路径访问非法内存 */
  if (content_data == NULL || content_length == 0) {
    log_error("v5_1_function_control invalid payload (len=%u)\n", content_length);
    return;
  }
   bool instruct_valid = false;
  switch (instruct) {
  case APP_FUNC_CODE_EBIKE_UNLOCK: // 车辆解锁并上电
  {
    log_info("车辆控制指令（上电）\n");
    // 1. 记录原始电门状态
    uint8_t original_power_state = vehicle_status.power_state;
    
    // 2. 下发控制指令到 MCU，当前 payload 很短，直接走 UART 透传
    log_info("发送给 MCU: protocol=0x%04x len=%u data=%d,%d", protocol_id, content_length, content_data[0], content_data[1]);
    if (content_length) {
      log_info_hexdump(content_data, content_length);
    }
    bool unlock_success = uart1_send_toMCU(protocol_id, content_data, content_length);
    if (unlock_success) {
      log_info("上电指令发送成功\n");
    } else {
      log_info("上电指令发送失败\n");
    }
    g_ble_skip_uart_forward_once = 1;
    send_data_to_ble_post(instruct, protocol_id);
    
    // 3. 无论 MCU 是否及时回 0x0012，都先给 APP 回复新的电门状态
    // 上电后的状态应为 0x01
    uint8_t new_power_state = 0x01;
    reply_power_state_change_to_app(instruct, protocol_id, original_power_state, new_power_state);
  }
    break;
  case APP_FUNC_CODE_EBIKE_LOCK: // 车辆落锁并下电
  {
    log_info("车辆控制指令（下电）\n");
    // 1. 记录原始电门状态
    uint8_t original_power_state = vehicle_status.power_state;
    
    // 2. 下发控制指令到 MCU，当前 payload 很短，直接走 UART 透传
    log_info("发送给 MCU: protocol=0x%04x len=%u data=%d,%d", protocol_id, content_length, content_data[0], content_data[1]);
    if (content_length) {
      log_info_hexdump(content_data, content_length);
    }
    bool lock_success = uart1_send_toMCU(protocol_id, content_data, content_length);
    if (lock_success) {
      log_info("下电指令发送成功\n");
    } else {
      log_info("下电指令发送失败\n");
    }
    g_ble_skip_uart_forward_once = 1;
    send_data_to_ble_post(instruct, protocol_id);
    
    // 3. 无论 MCU 是否及时回 0x0012，都先给 APP 回复新的电门状态
    // 下电后的状态应为 0x00
    uint8_t new_power_state = 0x00;
    reply_power_state_change_to_app(instruct, protocol_id, original_power_state, new_power_state);
  }
    break;  
  case APP_FUNC_CODE_OPEN_SEAT: // 打开座桶
    log_info("打开座桶指令\n");
    log_info("发送给 MCU: protocol=0x%04x len=%u data=%d,%d", protocol_id, content_length, content_data[0], content_data[1]);
    if (content_length) {
      log_info_hexdump(content_data, content_length);
    }
   bool seat_open_success = uart1_send_toMCU(protocol_id, content_data, content_length);
    if (seat_open_success) {
      log_info("开座桶指令发送成功\n");
    } else {
      log_info("开座桶指令发送失败\n");
    }
    g_ble_skip_uart_forward_once = 1;
    send_data_to_ble_post(instruct, protocol_id);
    break;
  case APP_FUNC_CODE_FIND_EBIKE: // 寻车
    log_info("寻车指令\n");
    /* 寻车控制通常只有一个短 payload，send_data_to_ble_post 可能不会帮忙透传到 MCU。
     * 这里直接发给 MCU，优先保证寻车指令可靠执行，再走 BLE 回包。
     */
    log_info("发送给 MCU: protocol=0x%04x len=%u data=%d,%d", protocol_id, content_length, content_data[0], content_data[1]);
    if (content_length) {
      log_info_hexdump(content_data, content_length);
    }
    bool find_ebike_success = uart1_send_toMCU(protocol_id, content_data, content_length);
    if (find_ebike_success) {
      log_info("寻车指令发送成功\n");
    } else {
      log_info("寻车指令发送失败\n");
    }
    g_ble_skip_uart_forward_once = 1;
    send_data_to_ble_post(instruct, protocol_id);
    break;
  case APP_FUNC_CODE_REBOOT_EBIKE: // 重启中控
    log_info("重启中控指令\n");
    uart1_send_toMCU(protocol_id, content_data, content_length);
    g_ble_skip_uart_forward_once = 1;
    send_data_to_ble_post(instruct, protocol_id);
    break;
  case APP_FUNC_CODE_PAIR_NFC: // 配对 NFC
    log_info("配对 NFC 指令\n");
    uart1_send_toMCU(protocol_id, content_data, content_length);
    g_ble_skip_uart_forward_once = 1;
    send_data_to_ble_post(instruct, protocol_id);
    break;
  case APP_FUNC_CODE_PAIR_REMOTE_KEY: // 配对遥控钥匙
    log_info("配对遥控钥匙指令\n");
    uart1_send_toMCU(protocol_id, content_data, content_length);
    g_ble_skip_uart_forward_once = 1;
    send_data_to_ble_post(instruct, protocol_id);
    break;
  case APP_FUNC_CODE_CHECK_EBIKE: // 系统自检
    log_info("系统自检指令\n");
    send_data_to_ble_post(instruct, protocol_id);
    break;
  case APP_FUNC_CODE_EBIKE_LOCKING: // 车辆设防
    log_info("车辆设防指令\n");
    send_data_to_ble_post(instruct, protocol_id);
    vehicle_control_timer_handler(); // 触发 0x0012，通知 APP 刷新车辆状态
    break;
  case APP_FUNC_CODE_EBIKE_UNLOCKING: // 车辆撤防
    log_info("车辆撤防指令\n");
    send_data_to_ble_post(instruct, protocol_id);
    break;
  case APP_FUNC_CODE_SOUND_THEME: // 音效主题切换
    log_info("音效主题切换指令\n");
    send_data_to_ble_post(instruct, protocol_id);
    break;
  case APP_FUNC_CODE_NAVIGATION_SCREEN: // 导航投屏
  default:
    break;
  }
}
static void send_data_to_ble_post(uint16_t instruct, uint16_t protocol_id) {
  if (content_data == NULL || content_length == 0) {
    log_info("send_data_to_ble_post: invalid content_length=%d", content_length);
    return;
  }
  if (g_ble_send_req_pending) {
    log_info("send_data_to_ble_post: pending, drop instruct=%d", instruct);
    return;
  }

  uint16_t copy_len = content_length;
  if (copy_len > sizeof(g_ble_send_req.payload)) {
    log_info("send_data_to_ble_post: payload too long %d, trunc to %zu", copy_len,
             sizeof(g_ble_send_req.payload));
    copy_len = sizeof(g_ble_send_req.payload);
  }

  g_ble_send_req.instruct = instruct;
  g_ble_send_req.protocol_id = protocol_id;
  g_ble_send_req.payload_len = copy_len;
  if (copy_len) {
    memcpy(g_ble_send_req.payload, content_data, copy_len);
  }
  g_ble_send_req_pending = 1;

  int msg[3];
  msg[0] = (int)send_data_to_ble_post_cb;
  msg[1] = 1;
  msg[2] = (int)&g_ble_send_req;
  int r = os_taskq_post_type("app_core", Q_CALLBACK, 3, msg);
  if (r) {
    if (r == OS_Q_FULL) {
      /* app_core 消息队列满时，不要直接丢弃 BLE 回包流程。
       * 这里退化为当前线程直接执行 send_data_to_ble()，避免控制指令丢失。
       */
      send_data_to_ble(g_ble_send_req.instruct, g_ble_send_req.protocol_id,
                       g_ble_send_req.payload, g_ble_send_req.payload_len);
      g_ble_send_req_pending = 0;
      log_info("send_data_to_ble_post: app_core q full(0x%02x), fallback direct", r);
      return;
    }

    g_ble_send_req_pending = 0;
    log_info("send_data_to_ble_post: post failed %x", r);
  }
}

static void send_data_to_ble_post_cb(int req_ptr) {
  fill_ble_send_req_t *req = (fill_ble_send_req_t *)req_ptr;
  if (req == NULL) {
    g_ble_send_req_pending = 0;
    return;
  }
  send_data_to_ble(req->instruct, req->protocol_id, req->payload, req->payload_len);
  g_ble_send_req_pending = 0;
}

static uint8_t ble_notify_enqueue(const fill_ble_notify_req_t *src)
{
  if (!src || g_ble_notify_queue_cnt >= BLE_NOTIFY_QUEUE_SIZE) {
    return 0;
  }
  g_ble_notify_queue[g_ble_notify_queue_tail] = *src;
  g_ble_notify_queue_tail = (uint8_t)((g_ble_notify_queue_tail + 1) % BLE_NOTIFY_QUEUE_SIZE);
  g_ble_notify_queue_cnt++;
  return 1;
}

static uint8_t ble_notify_dequeue(fill_ble_notify_req_t *dst)
{
  if (!dst || g_ble_notify_queue_cnt == 0) {
    return 0;
  }
  *dst = g_ble_notify_queue[g_ble_notify_queue_head];
  g_ble_notify_queue_head = (uint8_t)((g_ble_notify_queue_head + 1) % BLE_NOTIFY_QUEUE_SIZE);
  g_ble_notify_queue_cnt--;
  return 1;
}

static void ble_notify_post(uint16_t handle, const uint8_t *data, uint16_t len,
                            uint8_t op, uint16_t instruct) {
  if (data == NULL || len == 0) {
    log_info("ble_notify_post: invalid len=%d", len);
    return;
  }
  if (g_ble_notify_req_pending) {
    fill_ble_notify_req_t tmp = {0};
    tmp.handle = handle;
    tmp.len = len;
    tmp.op = op;
    tmp.instruct = instruct;
    if (len > sizeof(tmp.data)) {
      len = sizeof(tmp.data);
    }
    memcpy(tmp.data, data, len);
    if (ble_notify_enqueue(&tmp)) {
      log_info("ble_notify_post: pending, enqueue instruct=%d (q=%u)", instruct, g_ble_notify_queue_cnt);
    } else {
      log_info("ble_notify_post: pending & queue full, drop instruct=%d", instruct);
    }
    return;
  }

  if (len > sizeof(g_ble_notify_req.data)) {
    log_info("ble_notify_post: len %d too large, trunc to %zu", len,
             sizeof(g_ble_notify_req.data));
    len = sizeof(g_ble_notify_req.data);
  }

  g_ble_notify_req.handle = handle;
  g_ble_notify_req.len = len;
  g_ble_notify_req.op = op;
  g_ble_notify_req.instruct = instruct;
  memcpy(g_ble_notify_req.data, data, len);
  g_ble_notify_req_pending = 1;

  int msg[3];
  msg[0] = (int)ble_notify_post_cb;
  msg[1] = 1;
  msg[2] = (int)&g_ble_notify_req;

  int r = 0;
  int retry = 3;
  while (retry--) {
    r = os_taskq_post_type("btstack", Q_CALLBACK, 3, msg);
    if (r == OS_NO_ERR) {
      break;
    }
    if (r == OS_Q_FULL) {
      os_time_dly(5);
    } else {
      break;
    }
  }

  if (r) {
    g_ble_notify_req_pending = 0;
    log_info("ble_notify_post: post btstack failed %x", r);
    ble_notify_kick_queue();
  }
}

static void ble_notify_post_cb(int req_ptr) {
  fill_ble_notify_req_t *req = (fill_ble_notify_req_t *)req_ptr;
  if (req == NULL) {
    g_ble_notify_req_pending = 0;
    ble_notify_kick_queue();
    return;
  }

  ble_notify_try_send(req, 0);
}

static void ble_notify_try_send(fill_ble_notify_req_t *req, uint8_t from_can_send_now)
{
  if (req == NULL) {
    g_ble_notify_req_pending = 0;
    ble_notify_kick_queue();
    return;
  }

  /* 连接/重连后手机侧可能还没完成 CCC 配置，BLE buffer 满时也可能暂时发不出去。
   * 这里不要直接丢包，否则 APP 会误以为已写成功但 SOC 没有真正发出通知。
   * 处理方式是等待 buffer 可用后，通过 ATT_EVENT_CAN_SEND_NOW 再继续发送。
   */
  if (!le_multi_app_send_user_data_check(req->len)) {
    if (!from_can_send_now) {
      log_info("set instruct: %d, ble buf full (len=%d), wait can_send_now", req->instruct, req->len);
    }
    hci_con_handle_t cur_con_handle = smartbox_get_con_handle();
    if (cur_con_handle) {
      att_server_request_can_send_now_event(cur_con_handle);
      return; /* keep pending */
    }
    g_ble_notify_req_pending = 0;
    return;
  }

  int ret = le_multi_app_send_user_data(req->handle, req->data, req->len, req->op);
  if (ret != APP_BLE_NO_ERROR) {
    log_info("set instruct: %d, failed to send data ret=%d", req->instruct, ret);
  } else {
#if FILL_PROTOCOL_SEND_DBG
    log_info("set instruct: %d, data sent successfully (len=%d)", req->instruct, req->len);
#endif
  }

  /* 即使本次发送失败，也要清 pending；如 CCC 未配好或链路已断，避免一直卡死。 */
  g_ble_notify_req_pending = 0;
  ble_notify_kick_queue();
}

static void ble_notify_kick_queue(void)
{
  if (g_ble_notify_req_pending) {
    return;
  }
  if (ble_notify_dequeue(&g_ble_notify_req)) {
    g_ble_notify_req_pending = 1;
    ble_notify_try_send(&g_ble_notify_req, 0);
  }
}

static void ble_reply_to_app_f7f1_post(uint16_t protocol_id, const uint8_t *payload, uint16_t payload_len)
{
  if (g_ble_reply_req_pending) {
    log_info("ble_reply_to_app_f7f1_post: pending, drop protocol=0x%04x", protocol_id);
    return;
  }

  uint16_t copy_len = payload_len;
  if (payload == NULL) {
    copy_len = 0;
  }
  if (copy_len > sizeof(g_ble_reply_req.payload)) {
    log_info("ble_reply_to_app_f7f1_post: payload too long %d, trunc to %zu", copy_len,
             sizeof(g_ble_reply_req.payload));
    copy_len = sizeof(g_ble_reply_req.payload);
  }

  g_ble_reply_req.protocol_id = protocol_id;
  g_ble_reply_req.payload_len = copy_len;
  if (copy_len) {
    memcpy(g_ble_reply_req.payload, payload, copy_len);
  }
  g_ble_reply_req_pending = 1;

  int msg[3];
  msg[0] = (int)ble_reply_to_app_f7f1_post_cb;
  msg[1] = 1;
  msg[2] = (int)&g_ble_reply_req;
  int r = os_taskq_post_type("app_core", Q_CALLBACK, 3, msg);
  if (r) {
    if (r == OS_Q_FULL || r == 0x15) {
      /* app_core 队列在高峰期可能塞满，但钥匙列表等 f7f1 回包仍需要尽量保证送达。
       * 这里退回到当前上下文直接执行封包和 notify，由 btstack 继续调度发送。 */
      ble_reply_to_app_f7f1_do(g_ble_reply_req.protocol_id, g_ble_reply_req.payload,
                              g_ble_reply_req.payload_len);
      g_ble_reply_req_pending = 0;
      log_info("ble_reply_to_app_f7f1_post: app_core q full(0x%02x), fallback direct", r);
      return;
    }

    g_ble_reply_req_pending = 0;
    log_info("ble_reply_to_app_f7f1_post: post failed %x (OS_Q_FULL=%x)", r, OS_Q_FULL);
  }
}

void fill_protocol_ble_reply_f7f1(uint16_t protocol_id, const uint8_t *payload, uint16_t payload_len)
{
  ble_reply_to_app_f7f1_post(protocol_id, payload, payload_len);
}

static void ble_reply_to_app_f7f1_post_cb(int req_ptr)
{
  fill_ble_reply_req_t *req = (fill_ble_reply_req_t *)req_ptr;
  if (req == NULL) {
    g_ble_reply_req_pending = 0;
    return;
  }
  ble_reply_to_app_f7f1_do(req->protocol_id, req->payload, req->payload_len);
  g_ble_reply_req_pending = 0;
}

static void ble_reply_to_app_f7f1_do(uint16_t protocol_id, const uint8_t *payload, uint16_t payload_len)
{
  fill_send_protocl(protocol_id, (uint8_t *)payload, payload_len);

  static uint8_t send_code[269];
  memset(send_code, 0, sizeof(send_code));
  uint16_t send_code_len = convert_protocol_to_buffer(&send_protocl, send_code, sizeof(send_code));
  if (send_code_len == 0) {
    log_info("ble_reply_to_app_f7f1_do: convert failed, protocol=0x%04x", protocol_id);
    return;
  }

  static AES_KEY aes_key;
  static uint8_t encrypt_buf[288];
  u16 encrypt_len = sizeof(encrypt_buf);
  {
    u8 keybuf[16];
    const u8 *key = fill_protocol_get_aes_key(keybuf);
    aes_set_encrypt_key(&aes_key, key, AES128_KEY_SIZE);
  }
  aes_encrypt_pkcs(&aes_key, send_code, send_code_len, encrypt_buf, &encrypt_len);

  if (encrypt_len > sizeof(send_code)) {
    encrypt_len = send_code_len;
  } else {
    memcpy(send_code, encrypt_buf, encrypt_len);
  }

  ble_notify_post(
      ATT_CHARACTERISTIC_0000f7f1_0000_1000_8000_00805f9b34fb_01_VALUE_HANDLE,
      send_code, encrypt_len, ATT_OP_NOTIFY, protocol_id);
}

static void send_data_to_ble(uint16_t instruct, uint16_t protocol_id,
                             const uint8_t *payload, uint16_t payload_len) {
  if (payload == NULL || payload_len < 2) {
    log_info("send_data_to_ble: invalid payload_len=%d", payload_len);
    return;
  }
  if (g_ble_skip_uart_forward_once) {
    g_ble_skip_uart_forward_once = 0;
  } else {
    uart1_send_toMCU(protocol_id, (uint8_t *)payload, payload_len);
  }
  uint8_t ret = 0;
  uint8_t set_code_buffer[64] = {0x00, 0x00, 0x01};  // 默认回包：结果码 + 指令码 + 成功标志
  set_code_buffer[0] = payload[0];
  set_code_buffer[1] = payload[1];
  // 注意：这里不依赖 uart_data 作为同步结果，MCU 回包是异步到达的
  // 对于开座桶、寻车等控制指令，这里先按成功路径回给 APP
  // set_code_buffer[2] 默认填 0x01，表示本次控制成功
  fill_send_protocl(protocol_id, set_code_buffer, 3);
  /* 该函数在当前线程会用到较多临时数据，发送缓冲使用 static，避免栈占用过大。 */
  static uint8_t send_code[269];
  memset(send_code, 0, sizeof(send_code));
  uint16_t send_code_len =
      convert_protocol_to_buffer(&send_protocl, send_code, 269);

  // 对回包数据进行 AES 加密
  static AES_KEY aes_key;

  u16 encrypt_len = sizeof(encrypt_data);
  {
    u8 keybuf[16];
    const u8 *key = fill_protocol_get_aes_key(keybuf);
    aes_set_encrypt_key(&aes_key, key, AES128_KEY_SIZE);
  }
  // 开始加密
  aes_encrypt_pkcs(&aes_key, send_code, send_code_len, encrypt_data,
                   &encrypt_len);
#if FILL_PROTOCOL_SEND_DBG
  printf("encrypt_data 加密结果:");
  log_info_hexdump(encrypt_data, encrypt_len);
  printf("encrypt_len: %d, send_data_len: %d\n", encrypt_len, send_code_len);
#endif

  // 校验加密后长度是否越界
  if (encrypt_len > sizeof(send_code)) {
    log_info("加密数据长度 %zu 超过发送缓冲区 %zu", encrypt_len,
             sizeof(send_code));
    // 超长时回退到原始协议数据发送
    encrypt_len = send_code_len;
  } else {
    memcpy(send_code, encrypt_data, encrypt_len);
  }

#if FILL_PROTOCOL_SEND_DBG
  printf("最终发送数据:");
  log_info_hexdump(send_code, encrypt_len);
#endif

  ble_notify_post(
      ATT_CHARACTERISTIC_0000f7f1_0000_1000_8000_00805f9b34fb_01_VALUE_HANDLE,
      send_code, encrypt_len, ATT_OP_NOTIFY, instruct);
  // 控制指令发送完成后，如涉及上下电，继续补发一次车辆状态给 APP
  if (payload_len >= 2 && payload[1] == 9) {
    vehicle_control_timer_handler(); // 主动上报一次 0x0012 车辆状态，刷新 APP 显示
    printf("车辆状态已刷新\n");
  }
  if (payload_len >= 2 && payload[1] == 8) {
    vehicle_control_timer_handler(); // 主动上报一次 0x0012 车辆状态，刷新 APP 显示
    printf("车辆状态已刷新\n");
  }
}

// ================================ 钥匙列表管理 ================================
/* 记录最近一次发送 0x00FB 钥匙列表的时间，1 秒内避免重复回复 */
static u32 g_last_send_key_list_time = 0;
void send_ble_key_list(uint16_t protocol_id) {
  // 0x00FB 请求过于频繁时直接丢弃，避免重复回包
  if (protocol_id == 0x00FB) {
    u32 current_time = jiffies;
    if (current_time - g_last_send_key_list_time < 100) { // 1 秒内不重复回复
      log_info("[BLE] 00FB 协议发送过于频繁，已忽略\n");
      return;
    }
    g_last_send_key_list_time = current_time;
  }

  // uint8_t id_addr[6] = {0};save_MAC_address
  // ble_list_get_last_id_addr(id_addr);
  // printf("获取最近绑定设备 MAC 地址: ");
  // printf_buf(id_addr, sizeof(id_addr));
  // 检查 MAC 地址是否重复
  uint8_t ret;
  // ble_proto_ble_pair_req_Proc(NULL);
  static uint8_t key_list_buffer[269];
  memset(key_list_buffer, 0, sizeof(key_list_buffer));
  uint16_t offset = 0;

  // 读取各类钥匙当前的可用数量
  uint8_t phone_ble_key_usable_num = 0;
  uint8_t ble_key_peripheral_usable_num = 0;
  uint8_t nfc_key_peripheral_usable_num = 0;

  syscfg_read(CFG_PHONE_BLE_KEY_USABLE_NUM, &phone_ble_key_usable_num, 1);
  syscfg_read(CFG_BLE_KEY_PERIPHERAL_USABLE, &ble_key_peripheral_usable_num, 1);
  syscfg_read(CFG_NFC_KEY_PERIPHERAL_USABLE_NUM, &nfc_key_peripheral_usable_num,
              1);

  printf("phone_ble_key_usable_num: %d\n", phone_ble_key_usable_num);
  printf("ble_key_peripheral_usable_num: %d\n", ble_key_peripheral_usable_num);
  printf("nfc_key_peripheral_usable_num: %d\n", nfc_key_peripheral_usable_num);

  /* 读取手机蓝牙钥匙（功能码 0x0013）
   * 注意：不要完全依赖 CFG_PHONE_BLE_KEY_USABLE_NUM，直接遍历 3 个槽位更稳妥。
   */
  {
    uint32_t config_ids[] = {
        CFG_PHONE_BLE_KEY_DELETE_ID_1,
        CFG_PHONE_BLE_KEY_DELETE_ID_2,
        CFG_PHONE_BLE_KEY_DELETE_ID_3,
    };
    const uint16_t slot_cnt = sizeof(config_ids) / sizeof(config_ids[0]);
    uint8_t phone_ble_key_data[9] = {0}; // 3 字节配对码 + 6 字节 MAC 地址

    for (uint16_t i = 0; i < slot_cnt; i++) {
      memset(phone_ble_key_data, 0, sizeof(phone_ble_key_data));
      ret = syscfg_read(config_ids[i], phone_ble_key_data, sizeof(phone_ble_key_data));
      if (ret > 0 && !is_all_zero(phone_ble_key_data, sizeof(phone_ble_key_data))) {
        if (offset + 2 + 3 > sizeof(key_list_buffer)) {
          goto key_list_send;
        }
        /* 功能码按协议要求使用 UINT16 大端写入 */
        fill_put_u16_be(key_list_buffer + offset, APP_FUNC_CODE_KEY_BLE_HID);
        offset += 2;
        memcpy(key_list_buffer + offset, phone_ble_key_data, 3);
        offset += 3;
        printf("手机蓝牙钥匙 %d: %02X%02X%02X\n", (int)(i + 1), phone_ble_key_data[0],
               phone_ble_key_data[1], phone_ble_key_data[2]);
      }
    }
  }

  // 读取蓝牙外设钥匙（功能码 0x0014）
  if (ble_key_peripheral_usable_num > 0) {
    // 读取第一个蓝牙外设钥匙
    if (ble_key_peripheral_usable_num >= 1) {
      uint8_t ble_peripheral_data[28] = {0}; // 每次读取前先清空缓存
      ret = syscfg_read(CFG_BLE_KEY_PERIPHERAL_SEND_1, ble_peripheral_data, 28);
      if (ret > 0 && !is_all_zero(ble_peripheral_data, 6)) {
        // 追加功能码（外设蓝牙钥匙）
        if (offset + 2 > sizeof(key_list_buffer)) {
          goto key_list_send;
        }
        fill_put_u16_be(key_list_buffer + offset, APP_FUNC_CODE_KEY_EXT_BLE_HID);
        offset += 2;

        // 追加 MAC 地址（6 字节）
        memcpy(key_list_buffer + offset, ble_peripheral_data, 6);
        offset += 6;

        // 统计设备名称长度（从第 7 字节开始）
        uint8_t name_length = 0;
        for (int i = 6; i < 28; i++) {
          if (ble_peripheral_data[i] != 0) {
            name_length++;
          } else {
            break;
          }
        }

        // 写入设备名称长度（1 字节）
        key_list_buffer[offset++] = name_length;

        // 写入设备名称（GBK 编码）
        if (name_length > 0) {
          memcpy(key_list_buffer + offset, ble_peripheral_data + 6,
                 name_length);
          offset += name_length;
        }

        printf(
            "蓝牙外设钥匙 1: MAC=%02X:%02X:%02X:%02X:%02X:%02X, 名称长度=%d\n",
            ble_peripheral_data[0], ble_peripheral_data[1],
            ble_peripheral_data[2], ble_peripheral_data[3],
            ble_peripheral_data[4], ble_peripheral_data[5], name_length);
      }
    }

    // 读取第二个蓝牙外设钥匙
    if (ble_key_peripheral_usable_num >= 2) {
      uint8_t ble_peripheral_data[28] = {0}; // 每次读取前先清空缓存
      ret = syscfg_read(CFG_BLE_KEY_PERIPHERAL_SEND_2, ble_peripheral_data, 28);
      if (ret > 0 && !is_all_zero(ble_peripheral_data, 6)) {
        if (offset + 2 > sizeof(key_list_buffer)) {
          goto key_list_send;
        }
        fill_put_u16_be(key_list_buffer + offset, APP_FUNC_CODE_KEY_EXT_BLE_HID);
        offset += 2;

        memcpy(key_list_buffer + offset, ble_peripheral_data, 6);
        offset += 6;

        uint8_t name_length = 0;
        for (int i = 6; i < 28; i++) {
          if (ble_peripheral_data[i] != 0) {
            name_length++;
          } else {
            break;
          }
        }

        key_list_buffer[offset++] = name_length;

        if (name_length > 0) {
          memcpy(key_list_buffer + offset, ble_peripheral_data + 6,
                 name_length);
          offset += name_length;
        }

        printf("蓝牙外设钥匙 2\n");
      }
    }

    // 读取第三个蓝牙外设钥匙
    if (ble_key_peripheral_usable_num >= 3) {
      uint8_t ble_peripheral_data[28] = {0}; // 每次读取前先清空缓存
      ret = syscfg_read(CFG_BLE_KEY_PERIPHERAL_SEND_3, ble_peripheral_data, 28);
      if (ret > 0 && !is_all_zero(ble_peripheral_data, 6)) {
        if (offset + 2 > sizeof(key_list_buffer)) {
          goto key_list_send;
        }
        fill_put_u16_be(key_list_buffer + offset, APP_FUNC_CODE_KEY_EXT_BLE_HID);
        offset += 2;

        memcpy(key_list_buffer + offset, ble_peripheral_data, 6);
        offset += 6;

        uint8_t name_length = 0;
        for (int i = 6; i < 28; i++) {
          if (ble_peripheral_data[i] != 0) {
            name_length++;
          } else {
            break;
          }
        }

        key_list_buffer[offset++] = name_length;

        if (name_length > 0) {
          memcpy(key_list_buffer + offset, ble_peripheral_data + 6,
                 name_length);
          offset += name_length;
        }

        printf("蓝牙外设钥匙 3\n");
      }
    }

    // 读取第四个蓝牙外设钥匙
    if (ble_key_peripheral_usable_num >= 4) {
      uint8_t ble_peripheral_data[28] = {0}; // 每次读取前先清空缓存
      ret = syscfg_read(CFG_BLE_KEY_PERIPHERAL_SEND_4, ble_peripheral_data, 28);
      if (ret > 0 && !is_all_zero(ble_peripheral_data, 6)) {
        if (offset + 2 > sizeof(key_list_buffer)) {
          goto key_list_send;
        }
        fill_put_u16_be(key_list_buffer + offset, APP_FUNC_CODE_KEY_EXT_BLE_HID);
        offset += 2;

        memcpy(key_list_buffer + offset, ble_peripheral_data, 6);
        offset += 6;

        uint8_t name_length = 0;
        for (int i = 6; i < 28; i++) {
          if (ble_peripheral_data[i] != 0) {
            name_length++;
          } else {
            break;
          }
        }

        key_list_buffer[offset++] = name_length;

        if (name_length > 0) {
          memcpy(key_list_buffer + offset, ble_peripheral_data + 6,
                 name_length);
          offset += name_length;
        }

        printf("蓝牙外设钥匙 4\n");
      }
    }
  }

  // 读取 NFC 钥匙（功能码 0x0015）
  if (nfc_key_peripheral_usable_num > 0) {
    uint8_t nfc_key_data[16] = {0}; // 16 字节 UID

    // 读取第一个 NFC 钥匙
    if (nfc_key_peripheral_usable_num >= 1) {
      ret = syscfg_read(CFG_NFC_KEY_PERIPHERAL_SEND_1, nfc_key_data, 16);
      if (ret > 0 && !is_all_zero(nfc_key_data, 16)) {
        if (offset + 2 > sizeof(key_list_buffer)) {
          goto key_list_send;
        }
        fill_put_u16_be(key_list_buffer + offset, APP_FUNC_CODE_KEY_NFC);
        offset += 2;
        memcpy(key_list_buffer + offset, nfc_key_data, 16);
        offset += 16;
        printf("NFC 钥匙 1\n");
      }
    }
    // 读取第二个 NFC 钥匙
    if (nfc_key_peripheral_usable_num >= 2) {
      ret = syscfg_read(CFG_NFC_KEY_PERIPHERAL_SEND_2, nfc_key_data, 16);
      if (ret > 0 && !is_all_zero(nfc_key_data, 16)) {
        if (offset + 2 > sizeof(key_list_buffer)) {
          goto key_list_send;
        }
        fill_put_u16_be(key_list_buffer + offset, APP_FUNC_CODE_KEY_NFC);
        offset += 2;
        memcpy(key_list_buffer + offset, nfc_key_data, 16);
        offset += 16;
        printf("NFC 钥匙 2\n");
      }
    }

    // 读取第三个 NFC 钥匙
    if (nfc_key_peripheral_usable_num >= 3) {
      ret = syscfg_read(CFG_NFC_KEY_PERIPHERAL_SEND_3, nfc_key_data, 16);
      if (ret > 0 && !is_all_zero(nfc_key_data, 16)) {
        if (offset + 2 > sizeof(key_list_buffer)) {
          goto key_list_send;
        }
        fill_put_u16_be(key_list_buffer + offset, APP_FUNC_CODE_KEY_NFC);
        offset += 2;
        memcpy(key_list_buffer + offset, nfc_key_data, 16);
        offset += 16;
        printf("NFC 钥匙 3\n");
      }
    }

    // 读取第四个 NFC 钥匙
    if (nfc_key_peripheral_usable_num >= 4) {
      ret = syscfg_read(CFG_NFC_KEY_PERIPHERAL_SEND_4, nfc_key_data, 16);
      if (ret > 0 && !is_all_zero(nfc_key_data, 16)) {
        if (offset + 2 > sizeof(key_list_buffer)) {
          goto key_list_send;
        }
        fill_put_u16_be(key_list_buffer + offset, APP_FUNC_CODE_KEY_NFC);
        offset += 2;
        memcpy(key_list_buffer + offset, nfc_key_data, 16);
        offset += 16;
        printf("NFC 钥匙 4\n");
      }
    }
  }

  // 读取密码钥匙（功能码 0x0016）
  {
    uint8_t password_data[4] = {0};
    ret = syscfg_read(CFG_VEHICLE_PASSWORD_UNLOCK, password_data, 4);
    if (ret > 0 && !is_all_zero(password_data, 4)) {
      if (offset + 2 + 4 > sizeof(key_list_buffer)) {
        goto key_list_send;
      }
      fill_put_u16_be(key_list_buffer + offset, APP_FUNC_CODE_KEY_PASSWD);
      offset += 2;
      // 按协议定义，密码使用 UINT8[4] 原样透传，不转 ASCII
      memcpy(key_list_buffer + offset, password_data, 4);
      offset += 4;
      printf("密码钥匙: %u%u%u%u\n", (unsigned)password_data[0], (unsigned)password_data[1],
             (unsigned)password_data[2], (unsigned)password_data[3]);
    }
  }

  printf("Total key list data length: %d bytes\n", offset);

key_list_send:

  // 如果当前没有任何钥匙数据，则保持空列表返回；此阶段不再补默认项
  if (offset == 0) {

    printf("Added default phone ble key\n");
  }
  /* 同步给 MCU，满足协议侧联动要求 */
  uart1_send_toMCU(protocol_id, key_list_buffer, offset);

  /* 回复 APP：通过 f7f1 notify 下发 */
  ble_reply_to_app_f7f1_post(protocol_id, key_list_buffer, offset);
}
//======================= 下发车辆密码钥匙 =======================

// 0x0042=============================================
typedef struct {
  uint16_t funcode;
  uint8_t Pws[4];
} Password_key;
Password_key password_key;
void send_vehicle_password_unlock_instruct(uint16_t protocol_id) {
  printf("send_vehicle_password_unlock\n");
  if (!fill_require_content(protocol_id, 4)) {
    return;
  }
  // 如果收到 0000，可按删除密码钥匙流程处理
  if (content_data[0] == '0' && content_data[1] == '0' && content_data[2] == '0' &&
      content_data[3] == '0') {
    /* code */
  }
  int ret = 0;
  password_key.funcode = APP_FUNC_CODE_KEY_PASSWD;
  password_key.Pws[0] = content_data[0] - '0';
  password_key.Pws[1] = content_data[1] - '0';
  password_key.Pws[2] = content_data[2] - '0';
  password_key.Pws[3] = content_data[3] - '0';
  ret = syscfg_write(CFG_VEHICLE_PASSWORD_UNLOCK, (void *)password_key.Pws,
                     sizeof(password_key.Pws));
  log_info("ret_data:%d\n", ret);
  uart1_send_toMCU(protocol_id, password_key.Pws, sizeof(password_key.Pws));
  // 保存成功后，回包成功标志
  uint8_t unlock_key_buffer[] = {0x01, 0x00, 0x42, 0x01, 0x00};
  if (ret == content_length) {
    unlock_key_buffer[0] = 0x01;
  } else {
    unlock_key_buffer[0] = 0x00;
  }
  // 回填用户设置的密码内容
  unlock_key_buffer[1] = content_data[0] - '0';
  unlock_key_buffer[2] = content_data[1] - '0';
  unlock_key_buffer[3] = content_data[2] - '0';
  unlock_key_buffer[4] = content_data[3] - '0';

  uint16_t key_len = sizeof(unlock_key_buffer);
  fill_send_protocl(protocol_id, unlock_key_buffer, key_len);
  uint8_t send_key_code[269] = {0};
  memcpy(send_key_code, unlock_key_buffer, key_len);
  u16 send_data_len =
      convert_protocol_to_buffer(&send_protocl, send_key_code, 269);
  if (app_recieve_callback) {
    app_recieve_callback(0, send_key_code, send_data_len);
  }
  // 对回包数据进行 AES 加密
  AES_KEY aes_key;

  u16 encrypt_len = sizeof(encrypt_data);
  aes_set_encrypt_key(&aes_key, test_aes_key1, AES128_KEY_SIZE);
  // 开始加密
  aes_encrypt_pkcs(&aes_key, send_key_code, send_data_len, encrypt_data,
                   &encrypt_len);
  printf("encrypt_data 加密结果:");
  log_info_hexdump(encrypt_data, encrypt_len);
  printf("encrypt_len: %d, send_data_len: %d\n", encrypt_len, send_data_len);

  // 校验加密数据长度
  if (encrypt_len > sizeof(send_key_code)) {
    log_info("加密数据长度 %u 超过发送缓冲区 %zu", encrypt_len,
             sizeof(send_key_code));
    encrypt_len = send_data_len;
  } else {
    memcpy(send_key_code, encrypt_data, encrypt_len);
  }
  printf("最终发送数据:");

  if (app_send_user_data_check(encrypt_len)) {
    if (app_send_user_data(
            ATT_CHARACTERISTIC_0000f7f1_0000_1000_8000_00805f9b34fb_01_VALUE_HANDLE,
            send_key_code, encrypt_len, ATT_OP_NOTIFY) == APP_BLE_NO_ERROR) {
      log_info("send_vehicle_password_unlock: data sent successfully\n");
      log_info_hexdump(send_key_code, encrypt_len);
    } else {
      log_info("send_vehicle_password_unlock: failed to send data\n");
    }
  }
}
//=================================== APP 下发蓝牙外设钥匙 0x0039 ===================================
typedef struct {
  uint16_t funcode;
  uint8_t MAC_addr[6];
  uint8_t ble_name_len;
  uint8_t ble_name_GBK[21];
} Ble_key_peripheral;
uint8_t mac_address_exists_gbk[] = {
    0x00, 0x4D, 0x41, 0x43, 0xB5, 0xD8, 0xD6, 0xB7, 0xD2,
    0xD1, 0xBE, 0xAD, 0xB4, 0xE6, 0xD4, 0xDA}; // “MAC 地址已经存在”的 GBK 文案，前导 0x00 表示失败
                                              // 失败提示
uint8_t
    no_peripheral_key_space_gbk[] =
        {
            0x00, 0xC3, 0xBB, 0xD3, 0xD0, 0xCD, 0xE2, 0xC9,
            0xE8, 0xC0, 0xB6, 0xD1, 0xC0, 0xD4, 0xC4, 0xCA,
            0xD7, 0xBF, 0xD5, 0xBC, 0xBC, 0xE4}; // “没有蓝牙外设钥匙空间”
Ble_key_peripheral ble_key_peripheral;
/**
 * @brief 处理 APP 下发蓝牙外设钥匙指令 0x0039
 *
 * @param protocol_id {协议 id}
 */
void ble_key_peripheral_send_instruct_ext(uint16_t protocol_id) {
  printf("ble_key_peripheral_send_instruct_ext\n");
  if (!fill_require_content(protocol_id, 6)) {
    return;
  }
  int ret = 0;
  ble_key_peripheral.funcode = APP_FUNC_CODE_KEY_EXT_BLE_HID;
  memcpy(ble_key_peripheral.MAC_addr, content_data, 6);
  uint8_t MAC_addr[6];
  memcpy(MAC_addr, ble_key_peripheral.MAC_addr, sizeof(MAC_addr));
  printf("接收到蓝牙外设钥匙 MAC 地址:%02X:%02X:%02X:%02X:%02X:%02X\n",
         MAC_addr[0], MAC_addr[1], MAC_addr[2], MAC_addr[3], MAC_addr[4],
         MAC_addr[5]);
  uint8_t peripheral_key_num = 0;

  // 读取已使用槽位数量
  uint8_t send_buffer_choice; // 用于决定返回哪种结果
  ret = syscfg_read(CFG_BLE_KEY_PERIPHERAL_USABLE, &peripheral_key_num,
                    sizeof(peripheral_key_num));

  // 增加读失败和范围保护
  if (ret <= 0) {
    printf("读取 CFG_BLE_KEY_PERIPHERAL_USABLE 失败，使用默认值 0\n");
    peripheral_key_num = 0;
  }

  if (peripheral_key_num > 4) {
    printf("检测到非法 key_num: %d，重置为 0\n", peripheral_key_num);
    peripheral_key_num = 0;
  }

  printf("当前已占用的蓝牙外设钥匙槽位:%d\n", peripheral_key_num);
  uint8_t Uable_num = 4 - (peripheral_key_num); // 剩余可用槽位
  if (Uable_num > 0) {
    printf("检测到可用的蓝牙外设钥匙槽位\n");
    printf("Uable_num:%d\n", Uable_num);
    ret = judge_periheral_flash_macadd(MAC_addr);
    if (ret) {
      printf("MAC 地址已经存在\n");
      // “MAC 地址已经存在”的 GBK 提示
      uint8_t mac_address_exists_gbk[] = {0x4D, 0x41, 0x43, 0xB5, 0xD8,
                                          0xD6, 0xB7, 0xD2, 0xD1, 0xBE,
                                          0xAD, 0xB4, 0xE6, 0xD4, 0xDA};
      send_buffer_choice = 1; // 表示 MAC 地址已存在
    } else {
      printf("MAC address not found\n");

      // 根据当前 key_num 选择写入哪个槽位
      // 不再在 case 内直接修改 peripheral_key_num
      switch (peripheral_key_num) {
      case 0:
        ret = syscfg_write(CFG_BLE_KEY_PERIPHERAL_SEND_1, content_data,
                           content_length);
        printf("save to slot 1\n");
        break;
      case 1:
        ret = syscfg_write(CFG_BLE_KEY_PERIPHERAL_SEND_2, content_data,
                           content_length);
        printf("save to slot 2\n");
        break;
      case 2:
        ret = syscfg_write(CFG_BLE_KEY_PERIPHERAL_SEND_3, content_data,
                           content_length);
        printf("save to slot 3\n");
        break;
      case 3:
        ret = syscfg_write(CFG_BLE_KEY_PERIPHERAL_SEND_4, content_data,
                           content_length);
        printf("save to slot 4\n");
        break;
      default:
        printf("无效的 key_num: %d", peripheral_key_num);
        ret = -1;
        break;
      }

      if (ret > 0) {
        // 仅在保存成功后再增加计数
        peripheral_key_num++;
        syscfg_write(CFG_BLE_KEY_PERIPHERAL_USABLE, &peripheral_key_num,
                     sizeof(peripheral_key_num));
        printf("保存成功，当前已保存数量: %d", peripheral_key_num);
        send_buffer_choice = 0;
      } else {
        printf("保存失败");
        send_buffer_choice = 3;
      }
    }
  } else {
    printf("没有蓝牙外设钥匙空间");
    // “没有蓝牙外设钥匙空间”的 GBK 提示
    uint8_t no_peripheral_key_space_gbk[] = {
        0xC3, 0xBB, 0xD3, 0xD0, 0xCD, 0xE2, 0xC9, 0xE8, 0xC0, 0xB6, 0xD1,
        0xC0, 0xD4, 0xC4, 0xCA, 0xD7, 0xBF, 0xD5, 0xBC, 0xBC, 0xE4};
    send_buffer_choice = 2; // 表示没有空间
  }
  // free(peripheral_key_num); // 无需释放栈变量
  // 统一组织返回数据
  uint8_t send_buffer[269] = {0};
  uint16_t send_buffer_len = 0;

  // 根据 send_buffer_choice 组装不同返回内容
  switch (send_buffer_choice) {
  case 0:                  // 保存成功
    send_buffer[0] = 0x01; // 成功状态码
    send_buffer_len = 1;
    printf("send save-success response\n");
    break;

  case 1:                  // MAC 地址已存在
    send_buffer[0] = 0x00; // MAC 地址重复状态码
    // 追加 “MAC 地址已经存在” 的 GBK 提示信息
    uint8_t mac_address_exists_gbk[] = {0x4D, 0x41, 0x43, 0xB5, 0xD8,
                                        0xD6, 0xB7, 0xD2, 0xD1, 0xBE,
                                        0xAD, 0xB4, 0xE6, 0xD4, 0xDA};
    memcpy(send_buffer + 1, mac_address_exists_gbk,
           sizeof(mac_address_exists_gbk));
    send_buffer_len = 1 + sizeof(mac_address_exists_gbk);
    printf("send mac-exists response\n");
    break;

  case 2:                  // 钥匙空间已满
    send_buffer[0] = 0x00; // 没有空间状态码
    // 追加钥匙空间已满的 GBK 提示信息
    uint8_t key_space_full_gbk[] = {
        0xD4, 0xBF, 0xB3, 0xD7, 0xBF, 0xD5, 0xBC, 0xE4, 0xD2, 0xD1, 0xC2, 0xFA};
    memcpy(send_buffer + 1, key_space_full_gbk,
           sizeof(key_space_full_gbk));
    send_buffer_len = 1 + sizeof(key_space_full_gbk);
    printf("send no-space response\n");
    break;

  case 3:                  // 保存失败
    send_buffer[0] = 0x00; // 保存失败状态码
    send_buffer_len = 1;
    printf("send save-failed response\n");
    break;

  default:
    send_buffer[0] = 0x00; // 未知错误状态码
    send_buffer_len = 1;
    printf("send unknown-error response\n");
    break;
  }

  // 组协议并发送
  fill_send_protocl(protocol_id, send_buffer, send_buffer_len);
  uint8_t send_key_code[269] = {0};
  memcpy(send_key_code, send_buffer, send_buffer_len);
  u16 send_data_len =
      convert_protocol_to_buffer(&send_protocl, send_key_code, 269);

  // 对回包数据进行 AES 加密
  AES_KEY aes_key;
  u16 encrypt_len = sizeof(encrypt_data);
  aes_set_encrypt_key(&aes_key, test_aes_key1, AES128_KEY_SIZE);
  aes_encrypt_pkcs(&aes_key, send_key_code, send_data_len, encrypt_data,
                   &encrypt_len);

  printf("加密后数据:");
  log_info_hexdump(encrypt_data, encrypt_len);
  printf("加密长度: %d, 原始长度: %d", encrypt_len, send_data_len);

  // 校验加密结果长度
  if (encrypt_len > sizeof(send_key_code)) {
    log_info("加密数据长度 %zu 超过发送缓冲区 %zu", encrypt_len,
             sizeof(send_key_code));
    encrypt_len = send_data_len;
    // send_key_code 保持原始数据
  } else {
    memcpy(send_key_code, encrypt_data, encrypt_len);
  }

  // 回环到本地回调
  if (app_recieve_callback) {
    app_recieve_callback(0, send_key_code, encrypt_len);
  }

  if (app_send_user_data_check(encrypt_len)) {
    if (app_send_user_data(
            ATT_CHARACTERISTIC_0000f7f1_0000_1000_8000_00805f9b34fb_01_VALUE_HANDLE,
            send_key_code, encrypt_len, ATT_OP_NOTIFY) == APP_BLE_NO_ERROR) {
      log_info("peripheral ble key data send success\n");
      log_info_hexdump(send_key_code, encrypt_len);
    } else {
      log_info("peripheral ble key data send failed\n");
    }
  }
}
//=================================== 删除蓝牙外设钥匙 =============================================
void delete_ble_key_instruct(uint16_t protocol_id) {
  printf("delete_ble_key_instruct\n");
  if (!fill_require_content(protocol_id, 6)) {
    return;
  }
  uint8_t delet_ble_key_peripheral_buffer[] = {0x00};
  // 读取要删除的 MAC 地址
  memcpy(ble_key_peripheral.MAC_addr, content_data, 6);
  // 读取当前已保存的钥匙数量
  uint8_t current_key_num[1] = {0};
  syscfg_read(CFG_BLE_KEY_PERIPHERAL_USABLE, (void *)&current_key_num,
              sizeof(current_key_num));
  uint8_t delet_buffer[28] = {0};
  uint8_t compare_data = current_key_num[0];
  // 当前存在蓝牙外设钥匙时才执行删除
  if (current_key_num[0] > 0) {

    uint8_t current_MAC_adders[6] = {0};
    // 读取第 1 个槽位的 MAC 地址
    syscfg_read(CFG_BLE_KEY_PERIPHERAL_SEND_1, (void *)current_MAC_adders,
                sizeof(current_MAC_adders));
    // 如果当前槽位 MAC 与待删除 MAC 相同，则清空该槽位
    if (memcmp(current_MAC_adders, ble_key_peripheral.MAC_addr,
               sizeof(current_MAC_adders)) == 0) {
      delet_ble_key_peripheral_buffer[0] = 0x01;
      syscfg_write(CFG_BLE_KEY_PERIPHERAL_SEND_1, delet_buffer,
                   sizeof(delet_buffer));
      printf("delete peripheral key slot1\n");
      current_key_num[0]--;
    }
    syscfg_read(CFG_BLE_KEY_PERIPHERAL_SEND_2, (void *)current_MAC_adders,
                sizeof(current_MAC_adders));
    // 如果当前槽位 MAC 与待删除 MAC 相同，则清空该槽位
    if (memcmp(current_MAC_adders, ble_key_peripheral.MAC_addr,
               sizeof(current_MAC_adders)) == 0) {
      delet_ble_key_peripheral_buffer[0] = 0x01;
      syscfg_write(CFG_BLE_KEY_PERIPHERAL_SEND_2, delet_buffer,
                   sizeof(delet_buffer));
      printf("delete peripheral key slot2\n");
      current_key_num[0]--;
    }
    syscfg_read(CFG_BLE_KEY_PERIPHERAL_SEND_3, (void *)current_MAC_adders,
                sizeof(current_MAC_adders));
    // 如果当前槽位 MAC 与待删除 MAC 相同，则清空该槽位
    if (memcmp(current_MAC_adders, ble_key_peripheral.MAC_addr,
               sizeof(current_MAC_adders)) == 0) {
      delet_ble_key_peripheral_buffer[0] = 0x01;
      syscfg_write(CFG_BLE_KEY_PERIPHERAL_SEND_3, delet_buffer,
                   sizeof(delet_buffer));
      printf("delete peripheral key slot3\n");
      current_key_num[0]--;
    }
    syscfg_read(CFG_BLE_KEY_PERIPHERAL_SEND_4, (void *)current_MAC_adders,
                sizeof(current_MAC_adders));
    // 如果当前槽位 MAC 与待删除 MAC 相同，则清空该槽位
    if (memcmp(current_MAC_adders, ble_key_peripheral.MAC_addr,
               sizeof(current_MAC_adders)) == 0) {
      delet_ble_key_peripheral_buffer[0] = 0x01;
      syscfg_write(CFG_BLE_KEY_PERIPHERAL_SEND_4, delet_buffer,
                   sizeof(delet_buffer));
      printf("delete peripheral key slot4\n");
      current_key_num[0]--;
    }
    // 只有数量发生变化时，才回写当前有效钥匙数量
    // 否则说明没有匹配到待删除设备
    if (current_key_num[0] != compare_data) {
      syscfg_write(CFG_BLE_KEY_PERIPHERAL_USABLE, (void *)&current_key_num,
                   sizeof(current_key_num));
      delet_ble_key_peripheral_buffer[0] = 0x01;
    }
  }
  uint16_t buffer_len = sizeof(delet_ble_key_peripheral_buffer);
  fill_send_protocl(protocol_id, delet_ble_key_peripheral_buffer, buffer_len);
  uint8_t send_key_code[269] = {0};
  memcpy(send_key_code, delet_ble_key_peripheral_buffer, buffer_len);
  u16 send_data_len =
      convert_protocol_to_buffer(&send_protocl, send_key_code, 269);

  // 对回包数据做 AES 加密
  AES_KEY aes_key;

  u16 encrypt_len = sizeof(encrypt_data);
  aes_set_encrypt_key(&aes_key, test_aes_key1, AES128_KEY_SIZE);
  // 开始加密
  aes_encrypt_pkcs(&aes_key, send_key_code, send_data_len, encrypt_data,
                   &encrypt_len);
  printf("encrypt_data 加密后的数据:");
  log_info_hexdump(encrypt_data, encrypt_len);
  printf("encrypt_len: %d, send_data_len: %d\n", encrypt_len, send_data_len);

  // 校验加密后长度
  if (encrypt_len > sizeof(send_key_code)) {
    log_info("加密数据长度 %zu 超过发送缓冲区 %zu", encrypt_len,
             sizeof(send_key_code));
    // 回退为原始协议长度
    encrypt_len = send_data_len;
    // send_key_code 保持原始数据
  } else {
    memcpy(send_key_code, encrypt_data, encrypt_len);
  }
  printf("最终发送数据:");

  if (app_recieve_callback) {
    app_recieve_callback(0, send_key_code, encrypt_len);
  }

  if (app_send_user_data_check(encrypt_len)) {
    if (app_send_user_data(
            ATT_CHARACTERISTIC_0000f7f1_0000_1000_8000_00805f9b34fb_01_VALUE_HANDLE,
            send_key_code, encrypt_len, ATT_OP_NOTIFY) == APP_BLE_NO_ERROR) {
      log_info("ble_key_peripheral_buffer: data sent successfully\n");
      log_info_hexdump(send_key_code, encrypt_len);
    } else {
      log_info("ble_key_peripheral_buffer: failed to send data\n");
    }
  }
}
//------------------------------------- 清空蓝牙外设钥匙 ===========================================
void empty_ble_key_instruct(uint16_t protocol_id) {
  printf("empty_ble_key_instruct\n");
  uint8_t state_flag =
      0x00; // 状态标志位：0x00 失败，0x01 成功
  if (state_flag == 0x00) {
    uint8_t empty_buffer[28] = {0};
    syscfg_write(CFG_BLE_KEY_PERIPHERAL_SEND_1, empty_buffer, sizeof(empty_buffer));
    syscfg_write(CFG_BLE_KEY_PERIPHERAL_SEND_2, empty_buffer, sizeof(empty_buffer));
    syscfg_write(CFG_BLE_KEY_PERIPHERAL_SEND_3, empty_buffer, sizeof(empty_buffer));
    syscfg_write(CFG_BLE_KEY_PERIPHERAL_SEND_4, empty_buffer, sizeof(empty_buffer));
    syscfg_write(CFG_BLE_KEY_PERIPHERAL_USABLE, (void *)&state_flag,
                 sizeof(state_flag));
    state_flag = 0x01;
  }
  uint8_t empty_ble_key_peripheral_buffer[] = {0x00};
  empty_ble_key_peripheral_buffer[0] = state_flag;
  fill_send_protocl(protocol_id, empty_ble_key_peripheral_buffer,
                    sizeof(empty_ble_key_peripheral_buffer));
  uint8_t send_key_code[269] = {0};
  memcpy(send_key_code, empty_ble_key_peripheral_buffer,
         sizeof(empty_ble_key_peripheral_buffer));
  u16 send_data_len =
      convert_protocol_to_buffer(&send_protocl, send_key_code, 269);
  AES_KEY aes_key;

  u16 encrypt_len = sizeof(encrypt_data);
  aes_set_encrypt_key(&aes_key, test_aes_key1, AES128_KEY_SIZE);
  // 开始加密
  aes_encrypt_pkcs(&aes_key, send_key_code, send_data_len, encrypt_data,
                   &encrypt_len);
  printf("encrypt_data 加密后的数据:");
  log_info_hexdump(encrypt_data, encrypt_len);
  printf("encrypt_len: %d, send_data_len: %d\n", encrypt_len, send_data_len);

  // 校验加密后长度
  if (encrypt_len > sizeof(send_key_code)) {
    log_info("加密数据长度 %zu 超过发送缓冲区 %zu", encrypt_len,
             sizeof(send_key_code));
    // 回退为原始协议长度
    encrypt_len = send_data_len;
    // send_key_code 保持原始数据
  } else {
    memcpy(send_key_code, encrypt_data, encrypt_len);
  }
  printf("最终发送数据:");

  if (app_recieve_callback) {
    app_recieve_callback(0, send_key_code, encrypt_len);
  }

  if (app_send_user_data_check(encrypt_len)) {
    if (app_send_user_data(
            ATT_CHARACTERISTIC_0000f7f1_0000_1000_8000_00805f9b34fb_01_VALUE_HANDLE,
            send_key_code, encrypt_len, ATT_OP_NOTIFY) == APP_BLE_NO_ERROR) {
      log_info("ble_key_peripheral_buffer: data sent successfully\n");
      log_info_hexdump(send_key_code, encrypt_len);
    } else {
      log_info("ble_key_peripheral_buffer: failed to send data\n");
    }
  }
}
//====================================== 添加 NFC 钥匙 ===========================================
void add_NFC_instruct(uint16_t protocol_id) {
  uart1_send_toMCU(protocol_id, NULL, 0);
  // 先判断当前 NFC 钥匙数量
  uint8_t current_NFC_num[1] = {0};
  syscfg_read(CFG_NFC_KEY_PERIPHERAL_USABLE_NUM, (void *)&current_NFC_num,
              sizeof(current_NFC_num));
  uint8_t add_buffer[16] = {0};
  if (uart_data != 0) {
    memcpy(add_buffer, uart_data, data_length);
    // 后续如有需要可在这里清理 uart_data
    // memset(uart_data, 0, data_length);
  } else {
    printf("uart_data is NULL\n");
  }
  uint8_t compare_data = current_NFC_num[0];
  // 添加 NFC 钥匙
  if (compare_data < 3) {
    uint8_t current_NFC_adders[16] = {0};
    // 判断 NFC 槽位在 flash 中是否为空
    uint8_t is_empty;
    is_empty =
        syscfg_read(CFG_NFC_KEY_PERIPHERAL_SEND_1, (void *)current_NFC_adders,
                    sizeof(current_NFC_adders));
    if (is_empty == 0) {
      syscfg_write(CFG_NFC_KEY_PERIPHERAL_SEND_1, add_buffer,
                   sizeof(add_buffer));
      return;
    }
    is_empty =
        syscfg_read(CFG_NFC_KEY_PERIPHERAL_SEND_2, (void *)current_NFC_adders,
                    sizeof(current_NFC_adders));
    if (is_empty == 0) {
      syscfg_write(CFG_NFC_KEY_PERIPHERAL_SEND_2, add_buffer,
                   sizeof(add_buffer));
      return;
    }
    is_empty =
        syscfg_read(CFG_NFC_KEY_PERIPHERAL_SEND_3, (void *)current_NFC_adders,
                    sizeof(current_NFC_adders));
    if (is_empty == 0) {
      syscfg_write(CFG_NFC_KEY_PERIPHERAL_SEND_3, add_buffer,
                   sizeof(add_buffer));
      return;
    }
  }
}
//====================================== 删除 NFC 钥匙 =============================================
typedef struct {
  uint8_t fun_code;
  uint8_t NFC_UID[16];
} NFC_peripheral_t;
NFC_peripheral_t nfc_peripheral;
void delete_NFC_instruct(uint16_t protocol_id) {
  printf("delete_NFC_instruct\n");
  if (!fill_require_content(protocol_id, 16)) {
    return;
  }
  uint8_t delet_NFC_peripheral_buffer[] = {0x00};
  nfc_peripheral.fun_code = APP_FUNC_CODE_KEY_NFC;
  // 读取要删除的 NFC UID
  memcpy(nfc_peripheral.NFC_UID, content_data, content_length);
  // 读取当前已保存的 NFC 数量
  uint8_t current_NFC_num[1] = {0};
  syscfg_read(CFG_NFC_KEY_PERIPHERAL_USABLE_NUM, (void *)&current_NFC_num,
              sizeof(current_NFC_num));
  uint8_t delet_buffer[16] = {0};
  uint8_t compare_data = current_NFC_num[0];
  // 只有当前存在 NFC 钥匙时才执行删除
  if (current_NFC_num[0] > 0) {

    uint8_t current_NFC_adders[16] = {0};
    // 读取当前槽位 NFC UID
    syscfg_read(CFG_NFC_KEY_PERIPHERAL_SEND_1, (void *)current_NFC_adders,
                sizeof(current_NFC_adders));
    // 如果当前槽位 UID 与待删除 UID 相同，则清空该槽位
    if (memcmp(current_NFC_adders, nfc_peripheral.NFC_UID,
               sizeof(current_NFC_adders)) == 0) {
      delet_NFC_peripheral_buffer[0] = 0x01;
      syscfg_write(CFG_NFC_KEY_PERIPHERAL_SEND_1, delet_buffer,
                   sizeof(delet_buffer));
      current_NFC_num[0]--;
      uart1_send_toMCU(protocol_id, delet_NFC_peripheral_buffer,
                            sizeof(delet_NFC_peripheral_buffer));
    }
    syscfg_read(CFG_NFC_KEY_PERIPHERAL_SEND_2, (void *)current_NFC_adders,
                sizeof(current_NFC_adders));
    // 如果当前槽位 UID 与待删除 UID 相同，则清空该槽位
    if (memcmp(current_NFC_adders, nfc_peripheral.NFC_UID,
               sizeof(current_NFC_adders)) == 0) {
      delet_NFC_peripheral_buffer[0] = 0x01;
      syscfg_write(CFG_NFC_KEY_PERIPHERAL_SEND_2, delet_buffer,
                   sizeof(delet_buffer));
      current_NFC_num[0]--;
      uart1_send_toMCU(protocol_id, delet_NFC_peripheral_buffer,
                            sizeof(delet_NFC_peripheral_buffer));
    }
    syscfg_read(CFG_NFC_KEY_PERIPHERAL_SEND_3, (void *)current_NFC_adders,
                sizeof(current_NFC_adders));
    // 如果当前槽位 UID 与待删除 UID 相同，则清空该槽位
    if (memcmp(current_NFC_adders, nfc_peripheral.NFC_UID,
               sizeof(current_NFC_adders)) == 0) {
      delet_NFC_peripheral_buffer[0] = 0x01;
      syscfg_write(CFG_NFC_KEY_PERIPHERAL_SEND_3, delet_buffer,
                   sizeof(delet_buffer));
      current_NFC_num[0]--;
      uart1_send_toMCU(protocol_id, delet_NFC_peripheral_buffer,
                            sizeof(delet_NFC_peripheral_buffer));
    }
    syscfg_read(CFG_NFC_KEY_PERIPHERAL_SEND_4, (void *)current_NFC_adders,
                sizeof(current_NFC_adders));
    // 如果当前槽位 UID 与待删除 UID 相同，则清空该槽位
    if (memcmp(current_NFC_adders, nfc_peripheral.NFC_UID,
               sizeof(current_NFC_adders)) == 0) {
      delet_NFC_peripheral_buffer[0] = 0x01;
      syscfg_write(CFG_NFC_KEY_PERIPHERAL_SEND_4, delet_buffer,
                   sizeof(delet_buffer));
      current_NFC_num[0]--;
      uart1_send_toMCU(protocol_id, delet_NFC_peripheral_buffer,
                            sizeof(delet_NFC_peripheral_buffer));
    }
    if (current_NFC_num[0] != compare_data) {
      syscfg_write(CFG_NFC_KEY_PERIPHERAL_USABLE_NUM, (void *)&current_NFC_num,
                   sizeof(current_NFC_num));
      delet_NFC_peripheral_buffer[0] = 0x01;
      uart1_send_toMCU(protocol_id, delet_NFC_peripheral_buffer,
                            sizeof(delet_NFC_peripheral_buffer));
    }
  }

  uint16_t buffer_len = sizeof(delet_NFC_peripheral_buffer);
  fill_send_protocl(protocol_id, delet_NFC_peripheral_buffer, buffer_len);
  uint8_t send_key_code[269] = {0};
  memcpy(send_key_code, delet_NFC_peripheral_buffer, buffer_len);
  u16 send_data_len =
      convert_protocol_to_buffer(&send_protocl, send_key_code, 269);
  // 对回包数据做 AES 加密
  AES_KEY aes_key;

  u16 encrypt_len = sizeof(encrypt_data);
  aes_set_encrypt_key(&aes_key, test_aes_key1, AES128_KEY_SIZE);
  // 开始加密
  aes_encrypt_pkcs(&aes_key, send_key_code, send_data_len, encrypt_data,
                   &encrypt_len);
  printf("encrypt_data 加密后的数据:");
  log_info_hexdump(encrypt_data, encrypt_len);
  printf("encrypt_len: %d, send_data_len: %d\n", encrypt_len, send_data_len);

  // 校验加密后长度
  if (encrypt_len > sizeof(encrypt_data)) {
    log_info("加密数据长度 %zu 超过发送缓冲区 %zu", encrypt_len,
             sizeof(encrypt_data));
    // 回退为原始协议长度
    encrypt_len = send_data_len;
    // send_key_code 保持原始数据
  } else {
    memcpy(send_key_code, encrypt_data, encrypt_len);
  }
  printf("最终发送数据:");
  log_info_hexdump(send_key_code, encrypt_len);
  if (app_recieve_callback) {
    app_recieve_callback(0, send_key_code, encrypt_len);
  }

  if (app_send_user_data_check(encrypt_len)) {
    if (app_send_user_data(
            ATT_CHARACTERISTIC_0000f7f1_0000_1000_8000_00805f9b34fb_01_VALUE_HANDLE,
            send_key_code, encrypt_len, ATT_OP_NOTIFY) == APP_BLE_NO_ERROR) {
      log_info("ble_key_peripheral_buffer: data sent successfully\n");
      log_info_hexdump(send_key_code, encrypt_len);
    } else {
      log_info("ble_key_peripheral_buffer: failed to send data\n");
    }
  }
}
//====================================== 清空 NFC 钥匙 =============================================
void empty_NFC_instruct(uint16_t protocol_id) {
  printf("empty_ble_key_instruct\n");
  uint8_t state_flag =
      0x00; // 状态标志位：0x00 失败，0x01 成功
  if (state_flag == 0x00) {
    uint8_t empty_buffer[16] = {0};
    syscfg_write(CFG_NFC_KEY_PERIPHERAL_SEND_1, empty_buffer, sizeof(empty_buffer));
    syscfg_write(CFG_NFC_KEY_PERIPHERAL_SEND_2, empty_buffer, sizeof(empty_buffer));
    syscfg_write(CFG_NFC_KEY_PERIPHERAL_SEND_3, empty_buffer, sizeof(empty_buffer));
    syscfg_write(CFG_NFC_KEY_PERIPHERAL_USABLE_NUM, (void *)&state_flag,
                 sizeof(state_flag));
    state_flag = 0x01;
    uart1_send_toMCU(protocol_id, NULL, 0);
  }
  uint8_t empty_NFC_peripheral_buffer[] = {0x00};
  empty_NFC_peripheral_buffer[0] = state_flag;
  fill_send_protocl(protocol_id, empty_NFC_peripheral_buffer,
                    sizeof(empty_NFC_peripheral_buffer));
  uint8_t send_key_code[269] = {0};
  memcpy(send_key_code, empty_NFC_peripheral_buffer,
         sizeof(empty_NFC_peripheral_buffer));
  u16 send_data_len =
      convert_protocol_to_buffer(&send_protocl, send_key_code, 269);
  // 对回包数据做 AES 加密
  AES_KEY aes_key;

  u16 encrypt_len = sizeof(encrypt_data);
  aes_set_encrypt_key(&aes_key, test_aes_key1, AES128_KEY_SIZE);
  // 开始加密
  aes_encrypt_pkcs(&aes_key, send_key_code, send_data_len, encrypt_data,
                   &encrypt_len);
  printf("encrypt_data 加密后的数据:");
  log_info_hexdump(encrypt_data, encrypt_len);
  printf("encrypt_len: %d, send_data_len: %d\n", encrypt_len, send_data_len);

  // 校验加密后长度
  if (encrypt_len > sizeof(encrypt_data)) {
    log_info("加密数据长度 %zu 超过发送缓冲区 %zu", encrypt_len,
             sizeof(encrypt_data));
    // 回退为原始协议长度
    encrypt_len = send_data_len;
    // send_key_code 保持原始数据
  } else {
    memcpy(send_key_code, encrypt_data, encrypt_len);
  }
  printf("最终发送数据:");
  log_info_hexdump(send_key_code, encrypt_len);
  if (app_recieve_callback) {
    app_recieve_callback(0, send_key_code, encrypt_len);
  }
  if (app_send_user_data_check(encrypt_len)) {
    if (app_send_user_data(
            ATT_CHARACTERISTIC_0000f7f1_0000_1000_8000_00805f9b34fb_01_VALUE_HANDLE,
            send_key_code, encrypt_len, ATT_OP_NOTIFY) == APP_BLE_NO_ERROR) {
      log_info("ble_key_peripheral_buffer: data sent successfully\n");
      log_info_hexdump(send_key_code, encrypt_len);
    } else {
      log_info("ble_key_peripheral_buffer: failed to send data\n");
    }
  }
}
//============================================ 删除手机钥匙 =========================================
void delet_phone_bleKey_instrcut(uint16_t protocol_id) {
  // 配置项 ID 列表
  uint32_t config_ids[] = {
      CFG_PHONE_BLE_KEY_DELETE_ID_1,
      CFG_PHONE_BLE_KEY_DELETE_ID_2,
      CFG_PHONE_BLE_KEY_DELETE_ID_3,
  };
  int config_count = sizeof(config_ids) / sizeof(config_ids[0]);

  // 存放读取到的绑定 ID；前 3 字节为配对码，后 6 字节为 MAC 地址
  u8 id_addr[9] = {0};
  u8 mac_addr[6] = {0};

  // 收到的配对码（前 3 个字节）
  uint8_t received_pair_code[3] = {0};
  received_pair_code[0] = content_data[0];
  received_pair_code[1] = content_data[1];
  received_pair_code[2] = content_data[2];

  uint8_t send_code[1] = {0x00}; // 默认失败
  int found_match = 0;
  int delete_success = 0;

  // 遍历所有保存的 ID，查找匹配项
  for (int i = 0; i < config_count; i++) {
    // 读取槽位里的手机钥匙信息
    memset(id_addr, 0, sizeof(id_addr));
    syscfg_read(config_ids[i], (void *)id_addr, sizeof(id_addr));

    // 判断当前槽位是否为空；为空表示未保存
    int is_empty = 1;
    for (int j = 0; j < sizeof(id_addr); j++) {
      if (id_addr[j] != 0) {
        is_empty = 0;
        break;
      }
    }

    if (is_empty) {
      printf("绑定ID_%d为空，跳过\n", i + 1);
      continue;
    }

    // 比较配对码是否一致
    if (memcmp(received_pair_code, id_addr, sizeof(received_pair_code)) == 0) {
      found_match = 1;
      printf("在绑定ID_%d中找到匹配记录\n", i + 1);

      // 匹配成功后，取出 MAC 地址
      for (int k = 0; k < 6; k++) {
        mac_addr[k] = id_addr[k + 3];
        printf("%02X ", mac_addr[k]);
      }
      printf("\n");

      // 调用蓝牙管理层删除该 MAC 地址
      int ret = ble_list_delete_device(mac_addr, 0); // 删除指定地址设备
      printf("ret值=%d\n", ret);
      if (ret == 1) {
        delete_success = 1;
        send_code[0] = 0x01; // 删除成功
        printf("删除手机钥匙成功\n");

        // 清空对应槽位记录
        memset(id_addr, 0, sizeof(id_addr));
        syscfg_write(config_ids[i], (void *)id_addr, sizeof(id_addr));
        printf("已清空绑定ID_%d中的记录\n", i + 1);

        // 回写可用数量，确保计数正确
        uint8_t phone_usable_num = 0;
        syscfg_read(CFG_PHONE_BLE_KEY_USABLE_NUM, &phone_usable_num,
                    sizeof(phone_usable_num));
        if (phone_usable_num > 0) {
          phone_usable_num--;
          syscfg_write(CFG_PHONE_BLE_KEY_USABLE_NUM, &phone_usable_num,
                       sizeof(phone_usable_num));
          printf("当前可用的手机钥匙数量=%d\n", phone_usable_num);
        } else {
          printf("当前数量已经为0，无需继续减少\n");
        }
      } else {
        send_code[0] = 0x00; // 删除失败
        printf("删除手机钥匙失败\n");
      }
      break; // 找到匹配项后退出循环
    } else {
      printf("绑定ID_%d中的配对码不匹配\n", i + 1);
    }
  }

  if (!found_match) {
    printf("遍历所有绑定项后仍未找到匹配记录\n");
    send_code[0] = 0x00; // 未找到匹配项
  }

  // 发送结果
  printf("send_code 值为 %02X\n", send_code[0]);
  // 注意：不要强制覆盖 send_code，保持真实删除结果
  fill_send_protocl(protocol_id, send_code, sizeof(send_code));

  u16 send_data_len = convert_protocol_to_buffer(&send_protocl, send_code, 269);
  printf("send_data_len 值为 %d\n", send_data_len);
  uint8_t send_buffer[] = {0xFE, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
                           0x43, 0x01, 0xC2, 0x2E, 0x0D, 0x0A

  };
  printf("等待加密发送...\n");
  log_info_hexdump(send_code, send_data_len);
  AES_KEY aes_key;

  u16 encrypt_len = sizeof(encrypt_data);
  aes_set_encrypt_key(&aes_key, test_aes_key1, AES128_KEY_SIZE);
  // 开始加密
  aes_encrypt_pkcs(&aes_key, send_buffer, sizeof(send_buffer), encrypt_data,
                   &encrypt_len);
  printf("encrypt_data 加密后的数据:");
  log_info_hexdump(encrypt_data, encrypt_len);
  printf("encrypt_len: %d, send_data_len: %d\n", encrypt_len, send_data_len);

  // 校验加密后长度
  if (encrypt_len > sizeof(encrypt_data)) {
    log_info("加密数据长度 %zu 超过发送缓冲区 %zu", encrypt_len,
             sizeof(encrypt_data));

    encrypt_len = send_data_len;

  } else {
    memcpy(send_code, encrypt_data, encrypt_len);
  }
  printf("最终发送数据:");

  if (app_recieve_callback) {
    app_recieve_callback(0, send_code, encrypt_len);
  }

  if (app_send_user_data_check(encrypt_len)) {
    if (app_send_user_data(
            ATT_CHARACTERISTIC_0000f7f1_0000_1000_8000_00805f9b34fb_01_VALUE_HANDLE,
            send_code, encrypt_len, ATT_OP_NOTIFY) == APP_BLE_NO_ERROR) {
      log_info("ble_key_buffer: data sent successfully\n");
      log_info_hexdump(send_code, encrypt_len);
    } else {
      log_info("ble_key_buffer: failed to send data\n");
    }
  }
}
//======================================end===========================================
//********************************************* 车辆设置相关 *********************************************/
//======================================= 获取车辆设置信息 ==============================================
/**
 * @brief 获取车辆设置信息指令
 *
 */
void get_vehice_set_infromation_instruct(uint16_t protocol_id) {
  VEH_SET_TRACE("enter", protocol_id, 0);
  printf("Sending request for protocol_id: 0x%04x\n", protocol_id);
  printf("开始获取车辆设置信息\n");
  /* 0x0033 获取车辆设置信息时，避免被 0x0012 周期上报占用 uart_data */
  u8 timer_was_running = vehicle_control_timer_is_running();
  if (timer_was_running) {
    VEH_SET_TRACE("timer_stop", 1, 0);
    vehicle_control_timer_stop();
  }

  VEH_SET_TRACE("uart_send", protocol_id, 0);
  uart1_send_toMCU(protocol_id, NULL, 0);

  static uint8_t uart_copy_buf[256];
  uint16_t uart_copy_len = 0;
  int wait_count = 150; // 约 1.5s，按 10ms tick 估算
  while (wait_count--) {
    if (uart1_take_response(protocol_id, uart_copy_buf, sizeof(uart_copy_buf),
                            &uart_copy_len)) {
      break;
    }
    os_time_dly(1);
  }
  VEH_SET_TRACE("uart_wait_done", uart_copy_len, 0);

  if (timer_was_running) {
    VEH_SET_TRACE("timer_start", 1, 2000);
    vehicle_control_timer_start(11000);// 恢复定时上报
  }

  static const uint8_t default_data[] = {
      0x00, 0x1F, 0x01, 0x00, 0x22, 0x01, 0x01, 0x00, 0x22, 0x01, 0x00, 0x23,
      0x01, 0x00, 0x24, 0x01, 0x00, 0x25, 0x01, 0x00, 0x26, 0x01, 0x00, 0x27,
      0x01, 0x00, 0x28, 0x01, 0x00, 0x29, 0x01, 0x00, 0x2B, 0x01, 0x00, 0x2C,
      0x01, 0x00, 0x2D, 0x01, 0x00, 0x2E, 0x01, 0x00, 0x2F, 0x01, 0x00, 0x30,
      0x01, 0x00, 0x31, 0x00, 0x32, 0x01, 0x00, 0x33, 0x01, 0x00, 0x34, 0x01,
  };

  const uint8_t *p_data = NULL;
  uint16_t p_len = 0;

  if (uart_copy_len > 0) {
    printf("0x0033 got UART payload len=%d\n", uart_copy_len);
    log_info_hexdump(uart_copy_buf, uart_copy_len);
    p_data = uart_copy_buf;
    p_len = uart_copy_len;
  } else {
    printf("0x0033 UART timeout/no data, use default_data\n");
    log_info_hexdump((uint8_t *)default_data, sizeof(default_data));
    p_data = default_data;
    p_len = sizeof(default_data);
  }
  VEH_SET_TRACE("payload", p_len, 0);

  if (p_data == NULL || p_len == 0) {
    log_error("0x0033 invalid payload p_data=%p p_len=%u\n", p_data, (unsigned)p_len);
    return;
  }

  VEH_SET_TRACE("fill_send", protocol_id, p_len);
  fill_send_protocl(protocol_id, (uint8_t *)p_data, p_len);
  static uint8_t send_key_code[269];
  memset(send_key_code, 0, sizeof(send_key_code));
  if (p_len > sizeof(send_key_code)) {
      p_len = sizeof(send_key_code);
  }
  memcpy(send_key_code, p_data, p_len);

  u16 send_data_len =
      convert_protocol_to_buffer(&send_protocl, send_key_code, 269);
  VEH_SET_TRACE("proto_to_buf", send_data_len, 0);
  if (send_data_len == 0) {
    log_error("0x0033 convert_protocol_to_buffer failed\n");
    return;
  }
  AES_KEY aes_key;

  u16 encrypt_len = sizeof(encrypt_data);
  aes_set_encrypt_key(&aes_key, test_aes_key1, AES128_KEY_SIZE);
  // 开始加密
  aes_encrypt_pkcs(&aes_key, send_key_code, send_data_len, encrypt_data,
                   &encrypt_len);
  VEH_SET_TRACE("aes_done", encrypt_len, 0);
  printf("encrypt_data 加密后的数据:");
  log_info_hexdump(encrypt_data, encrypt_len);
  printf("encrypt_len: %d, send_data_len: %d\n", encrypt_len, send_data_len);

  // 校验加密后长度
  if (encrypt_len > sizeof(send_key_code)) {
    log_info("加密数据长度 %zu 超过发送缓冲区 %zu", encrypt_len,
             sizeof(send_key_code));
    // 回退为原始协议长度
    encrypt_len = send_data_len;
    // send_key_code 保持原始数据
  } else {
    memcpy(send_key_code, encrypt_data, encrypt_len);
  }
  printf("最终发送数据:");

  if (app_recieve_callback) {
    app_recieve_callback(0, send_key_code, encrypt_len);
  }

  VEH_SET_TRACE("ble_post", encrypt_len, 0);
  ble_notify_post(
      ATT_CHARACTERISTIC_0000f7f1_0000_1000_8000_00805f9b34fb_01_VALUE_HANDLE,
      send_key_code, encrypt_len, ATT_OP_NOTIFY, protocol_id);
  VEH_SET_TRACE("exit", protocol_id, 0);
}
// ================================== 自定义音效相关指令 ===========================================
#if FILL_PROTOCOL_MUSIC_ENABLE

typedef struct indivi_tone {
  uint8_t tone_type;     // 音效类型 1 字节
  uint32_t file_size;    // 文件大小 4 字节
  uint32_t file_crc32;   // CRC32 4 字节
  uint64_t tone_id;      // 音效 ID 8 字节
  uint8_t file_name[32]; // 文件名，最多 32 字节
} indivi_tone;

/* 自定义音效约定：存放在内部 FLASH USER reserved 区
 * - 音频数据：按 tone_type 写入对应 reserved 文件映射节点（不是目录/普通文件）
 *   - UWTG0 对应 isd_config.ini 的 [RESERVED_CONFIG]，VFS 路径为 mnt/sdfile/app/uwtg0
 *   - UWTG1~UWTG3 对应 isd_config.ini 的 [RESERVED_EXPAND_CONFIG]，VFS 路径为 mnt/sdfile/EXT_RESERVED/UWTG{1..3}
 * - 元信息写入 syscfg 的 CFG_CUSTOM_TONE_META_0..3，供 0x0075 查询
 * - 共享定义见 user_info_file.h，这里直接复用
 */

static int custom_tone_read_meta_by_id(int cfg_id, custom_tone_meta_t *meta_out)
{
  if (!meta_out) {
    return -1;
  }
  memset(meta_out, 0, sizeof(*meta_out));

  int rlen = syscfg_read(cfg_id, meta_out, sizeof(*meta_out));
  if (rlen != (int)sizeof(*meta_out)) {
    memset(meta_out, 0, sizeof(*meta_out));
    return -1;
  }
  if (meta_out->magic != CUSTOM_TONE_META_MAGIC) {
    return -1;
  }
  return 0;
}

static int custom_tone_clear_slot(uint8_t tone_type)
{
  custom_tone_meta_t empty_meta = {0};
  int cfg_id;

  if (tone_type >= CUSTOM_TONE_MAX_TYPES) {
    return -1;
  }

  cfg_id = custom_tone_meta_cfg_id(tone_type);
  if (syscfg_write(cfg_id, &empty_meta, sizeof(empty_meta)) != (int)sizeof(empty_meta)) {
    return -1;
  }

  return 0;
}

static void custom_tone_probe_file_format(const char *path)
{
  if (!path || !path[0]) {
    return;
  }

  u8 head[16] = {0};
  int rlen = 0;
  FILE *fp = fopen(path, "r");
  if (!fp) {
    printf("[CUSTOM_TONE] open failed: %s\n", path);
    return;
  }

  rlen = fread(fp, head, sizeof(head));
  fclose(fp);

  if (rlen <= 0) {
    printf("[CUSTOM_TONE] read head failed: %s\n", path);
    return;
  }

  printf("[CUSTOM_TONE] file head(%d): %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
         rlen,
         head[0], head[1], head[2], head[3], head[4], head[5], head[6], head[7],
         head[8], head[9], head[10], head[11], head[12], head[13], head[14], head[15]);

  /* 说明：这里根据文件头判断音频格式。
   * 手机侧下发 WAV 时通常带有完整 RIFF/WAVE 头，可据此确认写入是否正常。
   */
  if (rlen >= 12 && !memcmp(head, "RIFF", 4) && !memcmp(head + 8, "WAVE", 4)) {
    printf("[CUSTOM_TONE] looks like WAV (RIFF/WAVE).\n");
  } else if (rlen >= 3 && !memcmp(head, "ID3", 3)) {
    printf("[CUSTOM_TONE] looks like MP3 (ID3).\n");
  } else if (rlen >= 2 && head[0] == 0xFF && ((head[1] & 0xE0) == 0xE0)) {
    printf("[CUSTOM_TONE] looks like MP3 (frame sync).\n");
  } else if (rlen >= 4 && !memcmp(head, "OggS", 4)) {
    printf("[CUSTOM_TONE] looks like OGG/Opus (OggS).\n");
  }
}

static void tone_reply_ble_simple(uint16_t protocol_id, const uint8_t *payload, uint16_t payload_len)
{
  fill_send_protocl(protocol_id, (uint8_t *)payload, payload_len);
  uint8_t send_code[269] = {0};
  uint16_t send_code_len = convert_protocol_to_buffer(&send_protocl, send_code, sizeof(send_code));
  if (app_recieve_callback) {
    app_recieve_callback(0, send_code, send_code_len);
  }
  if (app_send_user_data_check(send_code_len)) {
    (void)app_send_user_data(
        ATT_CHARACTERISTIC_0000f7f1_0000_1000_8000_00805f9b34fb_01_VALUE_HANDLE,
        send_code, send_code_len, ATT_OP_NOTIFY);
  }
}

file_config_t file_config; // 文件创建配置
FILE *file_handle;
// 全局文件传输状态，去掉 static 以便外部调试查询
uint32_t file_write_offset = 0;       // 当前写入偏移
uint32_t total_file_size = 0;         // 文件总大小
uint8_t is_file_transfer_started = 0; // 文件传输是否已开始
static char current_file_path[96] = {0};     // 当前写入文件路径
static struct indivi_tone Indivi_tone;

static uint32_t g_music_rx_start_ms = 0;
static uint8_t g_music_rx_last_pct = 0xFF;
static uint8_t g_music_rx_head_probed = 0;
static uint32_t g_music_rx_nonzero_bytes = 0;
static uint32_t g_music_rx_total_bytes = 0;
static uint16_t g_music_rx_all_zero_pkts = 0;

/* 0x0075：获取自定义音效列表，返回 meta + 文件名信息
 * 返回结构：LIST[]
 * - 音效类型 UINT8
 * - 音频ID  UINT64 (BE)
 * - 文件名长度 UINT8
 * - 文件名 STRING(GBK) 1~32
 * 
 * 每种音效都会先返回一个默认音效项，tone_id=0；随后再追加用户上传的自定义音效。
 */
static void get_vehice_music_infromation_instruct(uint16_t protocol_id)
{
  static uint8_t out[269];
  uint16_t offset = 0;
  memset(out, 0, sizeof(out));

  // 默认音效名称使用 GBK 编码，下面保留十六进制字节，避免 UTF-8/GBK 混淆
  // "默认开机音效" GBK: C4 AC C8 CF BF AA BB FA D2 F4 D0 A7
  static const uint8_t default_tone_name_0[] = {0xC4, 0xAC, 0xC8, 0xCF, 0xBF, 0xAA, 0xBB, 0xFA, 0xD2, 0xF4, 0xD0, 0xA7};
  // "默认关机音效" GBK: C4 AC C8 CF B9 D8 BB FA D2 F4 D0 A7
  static const uint8_t default_tone_name_1[] = {0xC4, 0xAC, 0xC8, 0xCF, 0xB9, 0xD8, 0xBB, 0xFA, 0xD2, 0xF4, 0xD0, 0xA7};
  // "默认报警音效" GBK: C4 AC C8 CF B1 A8 BE AF D2 F4 D0 A7
  static const uint8_t default_tone_name_2[] = {0xC4, 0xAC, 0xC8, 0xCF, 0xB1, 0xA8, 0xBE, 0xAF, 0xD2, 0xF4, 0xD0, 0xA7};
  // "默认Hello音效" GBK: C4 AC C8 CF 48 65 6C 6C 6F D2 F4 D0 A7
  static const uint8_t default_tone_name_3[] = {0xC4, 0xAC, 0xC8, 0xCF, 0x48, 0x65, 0x6C, 0x6C, 0x6F, 0xD2, 0xF4, 0xD0, 0xA7};
  
  static const uint8_t *default_tone_names[] = {
    default_tone_name_0,  // type 0: 默认开机音效
    default_tone_name_1,  // type 1: 默认关机音效
    default_tone_name_2,  // type 2: 默认报警音效
    default_tone_name_3,  // type 3: 默认 Hello 音效
  };
  static const uint8_t default_tone_name_lens[] = {
    sizeof(default_tone_name_0),  // 12
    sizeof(default_tone_name_1),  // 12
    sizeof(default_tone_name_2),  // 12
    sizeof(default_tone_name_3),  // 13
  };

  // 返回结构：先放默认音效项，再追加自定义音效项
  for (uint8_t type = 0; type < CUSTOM_TONE_MAX_TYPES; type++) {
    // 1. 先放入默认音效（tone_id = 0 表示默认）
    const uint8_t *def_name = default_tone_names[type];
    uint8_t def_name_len = default_tone_name_lens[type];
    if (def_name_len > 32) def_name_len = 32;
    
    if (offset + 1 + 8 + 1 + def_name_len <= sizeof(out)) {
      out[offset++] = type;  // 音效类型
      // tone_id = 0，按 8 字节大端返回
      for (int i = 0; i < 8; i++) {
        out[offset++] = 0x00;
      }
      out[offset++] = def_name_len;
      memcpy(out + offset, def_name, def_name_len);
      offset += def_name_len;
    }

    // 2. 再追加用户上传的自定义音效
    custom_tone_meta_t meta = {0};
    int cfg_id = custom_tone_meta_cfg_id(type);
    if (custom_tone_read_meta_by_id(cfg_id, &meta) != 0 || meta.file_size == 0) {
      log_info("[CUSTOM_TONE] type=%u no custom tone meta，当前没有保存音频\n", type);
      continue;  // 当前类型没有自定义音效
    }

    uint8_t name_len = meta.name_len;
    if (name_len == 0) {
      name_len = (uint8_t)strlen(meta.file_name);
    }
    if (name_len > 32) {
      name_len = 32;
    }

    if (offset + 1 + 8 + 1 + name_len > sizeof(out)) {
      break;
    }
    // 追加自定义音效项
    out[offset++] = meta.tone_type;
    out[offset++] = (uint8_t)((meta.tone_id >> 56) & 0xff);
    out[offset++] = (uint8_t)((meta.tone_id >> 48) & 0xff);
    out[offset++] = (uint8_t)((meta.tone_id >> 40) & 0xff);
    out[offset++] = (uint8_t)((meta.tone_id >> 32) & 0xff);
    out[offset++] = (uint8_t)((meta.tone_id >> 24) & 0xff);
    out[offset++] = (uint8_t)((meta.tone_id >> 16) & 0xff);
    out[offset++] = (uint8_t)((meta.tone_id >> 8) & 0xff);
    out[offset++] = (uint8_t)(meta.tone_id & 0xff);
    out[offset++] = name_len;
    memcpy(out + offset, meta.file_name, name_len);
    offset += name_len;
  }
  // 回复给 APP
  tone_reply_ble_simple(protocol_id, out, offset);
}

// 给 le_trans_data.c 用的外部包装接口
void get_vehice_music_infromation_instruct_ext(uint16_t protocol_id)
{
  get_vehice_music_infromation_instruct(protocol_id);
}

/**
 * @brief 音效选择指令 (0x0076)
 * 请求数据格式：[tone_type 1字节][tone_id 8字节大端]
 * - tone_id = 0 表示选择默认音效
 * - tone_id != 0 表示选择自定义音效
 * 响应：[result 1字节] 0x01=成功, 0x00=失败
 */
static void select_tone_instruct(uint16_t protocol_id)
{
  printf("=== 音效选择指令 0x0076 ===\n");
  
  // 校验数据长度：tone_type(1) + tone_id(8) = 9 字节
  if (content_length < 9) {
    printf("[SELECT_TONE] 数据长度不足，期望 9 字节，实际 %d\n", content_length);
    uint8_t resp = 0x00;
    tone_reply_ble_simple(protocol_id, &resp, 1);
    return;
  }
  
  // 检查音效类型
  uint8_t tone_type = content_data[0];
  if (tone_type >= CUSTOM_TONE_MAX_TYPES) {
    printf("[SELECT_TONE] 无效的 tone_type=%u\n", tone_type);
    uint8_t resp = 0x00;
    tone_reply_ble_simple(protocol_id, &resp, 1);
    return;
  }
  
  // 解析 tone_id，按 8 字节大端
  uint64_t tone_id = 
      ((uint64_t)content_data[1] << 56) | ((uint64_t)content_data[2] << 48) |
      ((uint64_t)content_data[3] << 40) | ((uint64_t)content_data[4] << 32) |
      ((uint64_t)content_data[5] << 24) | ((uint64_t)content_data[6] << 16) |
      ((uint64_t)content_data[7] << 8)  | content_data[8];
  
  printf("[SELECT_TONE] tone_type=%u, tone_id=%llu\n", tone_type, tone_id);
  
  // 确认选择类型：tone_id = 0 表示默认音效，否则为自定义音效
  uint8_t select = (tone_id == 0) ? TONE_SELECT_DEFAULT : TONE_SELECT_CUSTOM;
  
  // 保存用户选择
  int ret = user_tone_select_save(tone_type, select);
  
  if (ret == 0) {
    printf("[SELECT_TONE] 保存成功: tone_type=%u, select=%s\n", 
           tone_type, select ? "custom" : "default");
    uint8_t resp = 0x01;
    tone_reply_ble_simple(protocol_id, &resp, 1);
  } else {
    printf("[SELECT_TONE] 保存失败\n");
    uint8_t resp = 0x00;
    tone_reply_ble_simple(protocol_id, &resp, 1);
  }
}

// 给 le_trans_data.c 用的外部包装接口
void select_tone_instruct_ext(uint16_t protocol_id)
{
  select_tone_instruct(protocol_id);
}

/**
 * @brief 设置自定义音效元信息指令
 *
 * @param protocol_id 协议 ID 2206
 */
static void set_vehice_music_instruct(uint16_t protocol_id) {
  if (content_length < 17) {
    log_info(
        "set_vehice_music_instruct: 数据长度不足，至少需要 17 字节，当前长度=%d",
        content_length);
    return;
  }

  Indivi_tone.tone_type = content_data[0];
  if (Indivi_tone.tone_type >= CUSTOM_TONE_MAX_TYPES) {
    log_info("set_vehice_music_instruct: invalid tone_type=%u", Indivi_tone.tone_type);
    uint8_t resp = 0x00;
    tone_reply_ble_simple(protocol_id, &resp, 1);
    return;
  }

  Indivi_tone.file_size = (content_data[1] << 24) | (content_data[2] << 16) |
                          (content_data[3] << 8) | content_data[4];
  Indivi_tone.file_crc32 = (content_data[5] << 24) | (content_data[6] << 16) |
                           (content_data[7] << 8) | content_data[8];
  Indivi_tone.tone_id =
      ((uint64_t)content_data[9] << 56) | ((uint64_t)content_data[10] << 48) |
      ((uint64_t)content_data[11] << 40) | ((uint64_t)content_data[12] << 32) |
      ((uint64_t)content_data[13] << 24) | ((uint64_t)content_data[14] << 16) |
      ((uint64_t)content_data[15] << 8) | content_data[16];
  // 特殊判断：tone_id == 0x00000001 表示切回默认音效，直接清空当前自定义槽位并返回成功
  if (Indivi_tone.tone_id == 0x00000001ULL) {
    memset(Indivi_tone.file_name, 0, sizeof(Indivi_tone.file_name));
    if (custom_tone_clear_slot(Indivi_tone.tone_type) != 0 ||
        user_tone_select_save(Indivi_tone.tone_type, TONE_SELECT_DEFAULT) != 0) {
      log_info("set_vehice_music_instruct: clear default tone failed, tone_type=%u",
               Indivi_tone.tone_type);
      uint8_t resp = 0x01; // 表示设置成功 
      tone_reply_ble_simple(protocol_id, &resp, 1);
      return;
    }

    log_info("set_vehice_music_instruct: switch tone_type=%u to default",
             Indivi_tone.tone_type);
    uint8_t resp = 0x01;
    tone_reply_ble_simple(protocol_id, &resp, 1);
    return;
  }

  if (Indivi_tone.file_size == 0 || Indivi_tone.file_size > CUSTOM_TONE_SLOT_MAX_SIZE) {
    log_info("set_vehice_music_instruct: invalid file_size=%u (max=%u)",
             Indivi_tone.file_size, (unsigned)CUSTOM_TONE_SLOT_MAX_SIZE);
    uint8_t resp = 0x00;
    tone_reply_ble_simple(protocol_id, &resp, 1);
    return;
  }

  uint8_t file_name_length = content_length - 17;
  if (file_name_length > 32) {
    file_name_length = 32;
  }

  memset(Indivi_tone.file_name, 0, sizeof(Indivi_tone.file_name));
  if (file_name_length > 0) {
    memcpy(Indivi_tone.file_name, content_data + 17, file_name_length);
    if (file_name_length < 32) {
      Indivi_tone.file_name[file_name_length] = '\0';
    } else {
      Indivi_tone.file_name[31] = '\0';
    }
  } else {
    Indivi_tone.file_name[0] = '\0';
  }

  printf("=== 音效信息指令解析结果 ===\n");
  printf("音效类型 (tone_type): %u\n", Indivi_tone.tone_type);
  printf("文件大小 (file_size): %u 字节 (约 %.2f MB)\n", Indivi_tone.file_size,
         (float)Indivi_tone.file_size / (1024 * 1024));
  printf("文件校验 (file_crc32): 0x%08X\n", Indivi_tone.file_crc32);
  printf("音频ID (tone_id): %llu\n", Indivi_tone.tone_id);
  printf("文件名 (file_name): ");
  for (int i = 0; i < file_name_length && Indivi_tone.file_name[i] != '\0'; i++) {
    printf("%c", Indivi_tone.file_name[i]);
  }
  printf("\n");
  printf("文件名长度: %d 字节\n", file_name_length);
  printf("文件名十六进制: ");
  for (int i = 0; i < file_name_length && i < 32; i++) {
    printf("%02X ", Indivi_tone.file_name[i]);
  }
  printf("\n");

  const char *slot_path = custom_tone_slot_path(Indivi_tone.tone_type);
  file_system_init(slot_path);
  memset(&file_config, 0, sizeof(file_config));
  file_config.file_path = slot_path;
  file_config.mode = "w+";
  file_config.max_size = (int)Indivi_tone.file_size;
  if (file_create(&file_config, &file_handle) != FILE_OP_SUCCESS) {
    file_handle = NULL;
  }

  if (!file_handle) {
    log_info("文件创建失败: %s", slot_path);
    uint8_t resp = 0x00;
    tone_reply_ble_simple(protocol_id, &resp, 1);
    return;
  }

  file_write_offset = 0;
  total_file_size = Indivi_tone.file_size;
  strncpy(current_file_path, slot_path, sizeof(current_file_path) - 1);
  is_file_transfer_started = 1;
  g_music_rx_start_ms = timer_get_ms();
  g_music_rx_last_pct = 0xFF;
  g_music_rx_head_probed = 0;
  g_music_rx_nonzero_bytes = 0;
  g_music_rx_total_bytes = 0;
  g_music_rx_all_zero_pkts = 0;

  printf("\n================== MUSIC TRANSFER START ==================\n");
  printf("文件创建成功: %s\n", slot_path);
  printf("文件大小: %u 字节 (%.2f MB)\n", total_file_size, total_file_size / (1024.0f * 1024.0f));
  printf("音效类型: %u\n", Indivi_tone.tone_type);
  printf("音效ID: %llu\n", Indivi_tone.tone_id);
  printf("期望 CRC32: 0x%08X\n", Indivi_tone.file_crc32);
  printf("文件名: %s\n", Indivi_tone.file_name);
  printf("文件传输状态已初始化，等待数据写入...\n");
  printf("=======================================================\n\n");

  uint8_t resp = 0x01;
  tone_reply_ble_simple(protocol_id, &resp, 1);
}
// 开锁指令
void open_lock_instruct(uint16_t protocol_id) {
  // 解析开锁状态
  uint8_t lock_status = content_data[0];
  printf("lock_status = %d\n", lock_status);
}
/**
 * @brief 音效文件数据发送指令，用于把音频文件写入 flash/APP/ufile 目录
 *
 * @param protocol_id 协议 ID 2207
 */
static void music_file_send_instruct(uint16_t protocol_id) {
  // 检查文件传输是否已开始
  if (!is_file_transfer_started || !file_handle) {
    log_info("文件传输尚未开始，请先调用 set_vehice_music_instruct 创建文件");
    uint8_t resp = 0x00;
    tone_reply_ble_simple(protocol_id, &resp, 1);
    return;
  }

  // 检查数据长度是否合法
  if (content_length < 1) {
    log_info("music_file_send: 数据长度非法");
    uint8_t resp = 0x00;
    tone_reply_ble_simple(protocol_id, &resp, 1);
    return;
  }

  uint16_t chunk_len = content_length;
  if (chunk_len == 0) {
    log_info("empty chunk, ignore\n");
    uint8_t resp = 0x00;
    tone_reply_ble_simple(protocol_id, &resp, 1);
    return;
  }
  /* 防止写入越界 */
  if (file_write_offset + chunk_len > total_file_size) {
    u32 desired_size = file_write_offset + chunk_len;
    if (desired_size > CUSTOM_TONE_SLOT_MAX_SIZE) {
      log_info("数据包超过文件大小上限，当前偏移: %u, 数据长度: %u, 目标大小: %u, 最大限制: %u",
               file_write_offset, chunk_len, desired_size,
               (unsigned)CUSTOM_TONE_SLOT_MAX_SIZE);
      uint8_t resp = 0x00;
      tone_reply_ble_simple(protocol_id, &resp, 1);
      return;
    }
    /* 兼容 APP 实际发送长度大于声明长度的情况，动态扩展接收长度 */
    log_info("music_file_send: expand file_size %u -> %u",
             total_file_size, desired_size);
    total_file_size = desired_size;
    Indivi_tone.file_size = desired_size;
    file_config.max_size = (int)desired_size;
  }

  // 每次传输开始时重置包计数
  static uint16_t pkt_count = 0;
  if (file_write_offset == 0) {
    pkt_count = 0;
  }
  
  {
    uint16_t nz = 0;
    for (uint16_t i = 0; i < chunk_len; i++) {
      if (content_data[i] != 0) {
        nz++;
      }
    }
    g_music_rx_total_bytes += chunk_len;
    g_music_rx_nonzero_bytes += nz;
    if (nz == 0) {
      g_music_rx_all_zero_pkts++;
    }

    if (file_write_offset == 0) {
      printf("[MUSIC_RX] ====== 开始接收音频数据 ======\n");
      printf("[MUSIC_RX] first chunk_len=%u, nonzero=%u, head16:", chunk_len, nz);
      for (uint16_t j = 0; j < 16 && j < chunk_len; j++) {
        printf(" %02X", content_data[j]);
      }
      printf("\n");
    }
    
    // 每隔 50 包打印一次详细统计
    pkt_count++;
    if (pkt_count % 50 == 0 || file_write_offset == 0) {
      printf("[MUSIC_RX] pkt#%u: offset=%u, chunk=%u, nz=%u (%.1f%%)\n",
             pkt_count, file_write_offset, chunk_len, nz,
             chunk_len ? (nz * 100.0f / chunk_len) : 0.0f);
    }
  }

  if (file_write_data(file_handle, content_data, chunk_len, file_write_offset) != 0) {
    log_info("文件写入失败，偏移 %u", file_write_offset);
    uint8_t resp = 0x00;
    tone_reply_ble_simple(protocol_id, &resp, 1);
    return;
  }

  file_write_offset += chunk_len;

  if (!g_music_rx_head_probed && file_write_offset >= 16) {
    printf("[CUSTOM_TONE] probe head early, offset=%u/%u\n", file_write_offset, total_file_size);
    custom_tone_probe_file_format(current_file_path);
    g_music_rx_head_probed = 1;
  }

  uint8_t pct = 0;
  if (total_file_size) {
    pct = (uint8_t)((file_write_offset * 100u) / total_file_size);
  }
  if (pct != g_music_rx_last_pct && (pct == 100 || (pct % 5) == 0)) {
    uint32_t elapsed = timer_get_ms() - g_music_rx_start_ms;
    log_info("[MUSIC_RX] %3u%% (%u/%u) elapsed=%ums", pct, file_write_offset, total_file_size, elapsed);
    g_music_rx_last_pct = pct;
  }

  uint8_t resp = 0x01;
  if (file_write_offset >= total_file_size) {
    resp = 0x02;
    file_close(file_handle);
    file_handle = NULL;

    /* 如果实际接收长度超过声明长度，则以实际长度为准收尾 */
    if (file_write_offset > total_file_size) {
      log_info("music_file_send: finalize size %u -> %u",
               total_file_size, file_write_offset);
      total_file_size = file_write_offset;
      Indivi_tone.file_size = total_file_size;
      file_config.max_size = (int)total_file_size;
    }

    {
      uint32_t elapsed = timer_get_ms() - g_music_rx_start_ms;
      printf("\n==================== MUSIC TRANSFER SUMMARY ====================\n");
      printf("[MUSIC_RX] Transfer completed!\n");
      printf("[MUSIC_RX] Total bytes received: %u\n", g_music_rx_total_bytes);
      printf("[MUSIC_RX] Non-zero bytes: %u (%.2f%%)\n", 
             g_music_rx_nonzero_bytes,
             g_music_rx_total_bytes ? (g_music_rx_nonzero_bytes * 100.0f / g_music_rx_total_bytes) : 0.0f);
      printf("[MUSIC_RX] All-zero packets: %u\n", g_music_rx_all_zero_pkts);
      printf("[MUSIC_RX] Time elapsed: %u ms\n", elapsed);
      printf("[MUSIC_RX] Average speed: %.2f KB/s\n", 
             elapsed ? (g_music_rx_total_bytes / 1024.0f / (elapsed / 1000.0f)) : 0.0f);
      
      // 输出异常统计
      if (g_music_rx_nonzero_bytes < g_music_rx_total_bytes / 10) {
        printf("[MUSIC_RX] WARNING: 非零字节比例过低 (<10%%)，数据可能异常!\n");
      }
      if (g_music_rx_all_zero_pkts > 10) {
        printf("[MUSIC_RX] WARNING: 全零包过多 (%u)，传输可能异常!\n", g_music_rx_all_zero_pkts);
      }
      printf("================================================================\n\n");
    }

    printf("[CUSTOM_TONE] transfer done, probe head, size=%u\n", total_file_size);
    custom_tone_probe_file_format(current_file_path);

    if (file_write_offset == total_file_size) {
      log_info("文件传输完成: %s, 大小: %u 字节", current_file_path, file_write_offset);
    } else {
      log_info("文件传输长度不一致: %s, 实际: %u 字节, 声明: %u 字节",
               current_file_path, file_write_offset, total_file_size);
    }

    // CRC32 校验
    {
      printf("[CUSTOM_TONE] Verifying file integrity...\n");
      FILE *verify_fp = fopen(current_file_path, "rb");
      if (verify_fp) {
        // 读取文件内容并计算 CRC32
        uint32_t calc_crc = 0;
        uint8_t buf[256];
        int total_read = 0;
        while (1) {
          int r = fread(verify_fp, buf, sizeof(buf));
          if (r <= 0) break;
          total_read += r;
          // 简单 CRC32 累加实现
          for (int i = 0; i < r; i++) {
            calc_crc = ((calc_crc << 8) | buf[i]) ^ (calc_crc >> 24);
          }
        }
        fclose(verify_fp);
        
       printf("[CUSTOM_TONE] 文件读取了 %d 字节\n",total_read);
        printf("[CUSTOM_TONE] 期望 CRC32: 0x%08X\n", Indivi_tone.file_crc32);
        printf("[CUSTOM_TONE] 计算CRC32: 0x%08X\n", calc_crc);
        
        if (total_read != (int)total_file_size) {
          printf("[CUSTOM_TONE] ERROR: 文件大小不匹配，实际=%d，声明=%u\n", 
                 total_read, total_file_size);
        }
        
        // 注意：如果 APP 没有发送准确 CRC32，这里可能不匹配
        // 先保留日志，方便后续排查
      } else {
        printf("[CUSTOM_TONE] ERROR: 无法打开文件做校验: %s\n", current_file_path);
      }
    }
    
    // 额外把传输报告落到文件，方便离线查看
    {
      const char *report_path = "mnt/sdfile/app/uwav/transfer_report.txt";
      
      // 确保 uwav 目录存在
      int mkdir_ret = fmk_dir("mnt/sdfile/app", "/uwav", 0);
      if (mkdir_ret != 0 && mkdir_ret != -1) {
        // -1 通常表示目录已存在，这里不算失败
        printf("[CUSTOM_TONE] mkdir uwav ret=%d\n", mkdir_ret);
      }
      
      FILE *report_fp = fopen(report_path, "w");
      if (report_fp) {
        char buf[256];
        int len;
        uint32_t elapsed = timer_get_ms() - g_music_rx_start_ms;
        
        #define WRITE_LINE(str) fwrite(report_fp, str, strlen(str))
        
        WRITE_LINE("==================== 自定义音效传输报告 ====================\n");
        
        len = snprintf(buf, sizeof(buf), "耗时: %u ms\n", elapsed);
        fwrite(report_fp, buf, len);
        
        len = snprintf(buf, sizeof(buf), "音效类型: %u\n", Indivi_tone.tone_type);
        fwrite(report_fp, buf, len);
        
        len = snprintf(buf, sizeof(buf), "文件路径: %s\n", current_file_path);
        fwrite(report_fp, buf, len);
        
        len = snprintf(buf, sizeof(buf), "文件名: %s\n", Indivi_tone.file_name);
        fwrite(report_fp, buf, len);
        
        WRITE_LINE("\n--- 传输统计 ---\n");
        
        len = snprintf(buf, sizeof(buf), "总字节数: %u\n", g_music_rx_total_bytes);
        fwrite(report_fp, buf, len);
        
        len = snprintf(buf, sizeof(buf), "非零字节: %u (%.2f%%)\n", 
                g_music_rx_nonzero_bytes,
                g_music_rx_total_bytes ? (g_music_rx_nonzero_bytes * 100.0f / g_music_rx_total_bytes) : 0.0f);
        fwrite(report_fp, buf, len);
        
        len = snprintf(buf, sizeof(buf), "全零包数量: %u\n", g_music_rx_all_zero_pkts);
        fwrite(report_fp, buf, len);
        
        len = snprintf(buf, sizeof(buf), "平均速度: %.2f KB/s\n",
                elapsed ? (g_music_rx_total_bytes / 1024.0f / (elapsed / 1000.0f)) : 0.0f);
        fwrite(report_fp, buf, len);
        
        WRITE_LINE("\n--- 文件校验 ---\n");
        
        len = snprintf(buf, sizeof(buf), "声明大小: %u 字节\n", total_file_size);
        fwrite(report_fp, buf, len);
        
        len = snprintf(buf, sizeof(buf), "实际大小: %u 字节\n", file_write_offset);
        fwrite(report_fp, buf, len);
        
        len = snprintf(buf, sizeof(buf), "声明CRC32: 0x%08X\n", Indivi_tone.file_crc32);
        fwrite(report_fp, buf, len);
        
        WRITE_LINE("\n--- 结果判断 ---\n");
        
        if (file_write_offset == total_file_size) {
          WRITE_LINE("文件大小正确\n");
        } else {
          WRITE_LINE("文件大小不匹配\n");
        }
        
        if (g_music_rx_nonzero_bytes >= g_music_rx_total_bytes / 2) {
          WRITE_LINE("非零字节比例正常 (>50%)\n");
        } else if (g_music_rx_nonzero_bytes >= g_music_rx_total_bytes / 10) {
          WRITE_LINE("非零字节比例偏低 (10%~50%)\n");
        } else {
          WRITE_LINE("非零字节比例过低 (<10%), 数据可能异常!\n");
        }
        
        if (g_music_rx_all_zero_pkts <= 5) {
          WRITE_LINE("全零数据包数量正常\n");
        } else {
          len = snprintf(buf, sizeof(buf), "全零数据包过多 (%u)，传输可能异常\n", g_music_rx_all_zero_pkts);
          fwrite(report_fp, buf, len);
        }
        
        WRITE_LINE("\n========================================================\n");
        
        len = snprintf(buf, sizeof(buf), "报告生成时间: 固件运行时长 %u ms\n", timer_get_ms());
        fwrite(report_fp, buf, len);
        
        #undef WRITE_LINE
        
        fclose(report_fp);
        printf("[CUSTOM_TONE] 传输报告已保存到: %s\n", report_path);
      } else {
        printf("[CUSTOM_TONE] WARNING: 无法创建传输报告文件\n");
      }
    }

    {
      custom_tone_meta_t meta = {0};
      meta.magic = CUSTOM_TONE_META_MAGIC;
      meta.tone_type = Indivi_tone.tone_type;
      meta.tone_id = Indivi_tone.tone_id;
      meta.file_size = Indivi_tone.file_size;
      meta.crc32 = Indivi_tone.file_crc32;
      strncpy(meta.file_name, (const char *)Indivi_tone.file_name, sizeof(meta.file_name) - 1);
      meta.file_name[sizeof(meta.file_name) - 1] = '\0';
      meta.name_len = (uint8_t)strlen(meta.file_name);
      /* 将元数据写入 syscfg，供后续音频播放与列表查询使用 */
      int cfg_id = custom_tone_meta_cfg_id(meta.tone_type);
      syscfg_write(cfg_id, &meta, sizeof(meta));
    }

    is_file_transfer_started = 0;
    file_write_offset = 0;
    total_file_size = 0;
    current_file_path[0] = '\0';
  }

  tone_reply_ble_simple(protocol_id, &resp, 1);
}
#else
void set_vehice_music_instruct(uint16_t protocol_id)
{
  log_info("set_vehice_music_instruct disabled (protocol_id=0x%04x)\n", protocol_id);
}

void music_file_send_instruct(uint16_t protocol_id)
{
  log_info("music_file_send_instruct disabled (protocol_id=0x%04x)\n", protocol_id);
}
#endif
//================================== 解除车辆绑定 ================================================ 
void remove_vehicle_binding_instruct(uint16_t protocol_id) {
  uint8_t remove_binding_buffer[] = {0x01};
  // 默认成功
  fill_send_protocl(protocol_id, remove_binding_buffer,
                    sizeof(remove_binding_buffer));
  uint8_t send_code[269] = {0};
  uint16_t send_code_len =
      convert_protocol_to_buffer(&send_protocl, send_code, 269);

  if (app_recieve_callback) {
    app_recieve_callback(0, send_code, send_code_len);
  }

  if (app_send_user_data_check(send_code_len)) {
    if (app_send_user_data(
            ATT_CHARACTERISTIC_0000f7f1_0000_1000_8000_00805f9b34fb_01_VALUE_HANDLE,
            send_code, send_code_len, ATT_OP_NOTIFY) == APP_BLE_NO_ERROR) {
      log_info("解除绑定指令: 0x%02X", remove_binding_buffer[0]);
    } else {
      log_info("解除绑定执行失败");
    }
  }
}
//===============================================================================================








