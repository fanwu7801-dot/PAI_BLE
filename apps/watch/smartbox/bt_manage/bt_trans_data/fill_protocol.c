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
// �����ⲿ���� ���ڽ������� Э��id ���ݳ���
extern uint8_t *uart_data;
extern uint16_t data_length;
extern uint16_t uart_protocol_id;
// ������AES��Կ 17ED2DD31F1EF55251BCB4862D1CE966
static const u8 *fill_protocol_get_aes_key(u8 out_key[16]);
static const u8 test_aes_key[16] = {
  0x17, 0xED, 0x2D, 0xD3, 0x1F, 0x1E, 0xF5, 0x52,
  0x51, 0xBC, 0xB4, 0x86, 0x2D, 0x1C, 0xE9, 0x66
};
    
uint8_t test_aes_key1[16] = {
    };
static const u8 *fill_protocol_get_aes_key(u8 out_key[16])
{
  int r = syscfg_read(CFG_DEVICE_AES_KEY, out_key, 16);
  int use_test = 1;
  if (r == 16) {
    for (u8 i = 0; i < 16; i++) {
      if (out_key[i] != 0) {
        use_test = 0;
        return out_key;
      }
    }
  }
#if FILL_PROTOCOL_AES_DBG
  printf("[AES] use_test_key=%d syscfg_read=%d key:", use_test, r);
  put_buf(use_test ? test_aes_key : out_key, 16);
#endif
  return test_aes_key;
}
uint8_t encrypt_data[1024] = {0};    // ���ڴ洢���ܺ������
// ȫ�ֱ�����Ψһ����
t_ble_protocl send_protocl; // ����Э��ṹ���MCU
t_ble_protocl recv_protocl; // ����Э��ṹ����ֻ�??
uint8_t protocol_content[256];
uint16_t total_frames = 0;        // ��֡����
uint16_t frame_number = 0;        // ֡��??
uint16_t protocol_id = 0;         // Э��ID??
uint8_t *content_data = NULL;     // Э������ָ��
uint8_t content_data_buffer[256]; // Э�����ݻ���??
uint16_t content_length = 0;      // Э�����ݳ���

/* ��ЧЭ�鴦�����أ�0x0075/0x2206/0x2207??
 * ֮����Ҫ�ڵ�һ�α� #if ʹ��ǰ���壺����ᵼ�²�ͬ�����������߲�ͬ��֧���������Ӳ�һ��??
 * Ĭ��ֵ��??1������ֱ����??APP Э�飬����Ҫ����ı����??
 */
#ifndef FILL_PROTOCOL_MUSIC_ENABLE
#define FILL_PROTOCOL_MUSIC_ENABLE  1
#endif

// ---- ��ЧЭ�鴦��??x0075/0x2206/0x2207��ǰ��������������ʽ�����������ӵ��ⲿ��??----
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

/* ���ﲻҪ??inline�����ֹ�����/����ѡ���¿��ܲ�����ʵ����ţ�������??undefined reference */
static void get_vehice_music_infromation_instruct(uint16_t protocol_id)
{
  log_info("get_vehice_music_infromation disabled (protocol_id=0x%04x)\n", protocol_id);
  get_vehice_music_infromation_infromation_reply_empty(protocol_id);
}
#endif

/* ���͵�����־���أ����� printf/put_buf ���ܵ��´����������Ӷ�������������??*/
#ifndef FILL_PROTOCOL_SEND_DBG
#define FILL_PROTOCOL_SEND_DBG  0
#endif

/* ���ļ��ڲ�ʹ�õĹ����볣����ԭ����δ�ṩ����ʱ��һ��Ĭ��ֵ��ͨ������??*/
#ifndef KEY_FUNC_PASSWORD
#define KEY_FUNC_PASSWORD  0x0042
#endif

/* A5-3 NFCԿ�׹�������ͷ�ļ�δ����ö��ʱ����Э��Լ��ʹ??0x0015 */
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

/* ===== ȱʧ���ܵ���Сʵ�֣�ȷ������ͨ������Ϊ����Ϊ���޲���??Ĭ��??===== */
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
  /* δ��������У�飬Ĭ����Ϊ����??*/
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

/* ���״̬����ֹ������� */
static volatile uint8_t pairing_in_progress = 0;
/* UART ָ������??passkey��һ����ʹ??*/
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
  /*add ���� �� ��MCU ��??--- > Ŀ��Ϊ�˷��Ͱ󶨵���Ƶ*/
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

  /* ���� 6 λ����� */
  u32 tmp32 = 0;
  if (reset_passkey_cb) {
    reset_passkey_cb(&tmp32);
  } else {
    tmp32 = (u32)rand32();
  }
  uint32_t passkey = tmp32 % 1000000;
  ble_pairing_set_uart_passkey(passkey);

  /* ��¼������� passkey����Э��ջ�ص�ʱʹ�ã���֤��֪ͨ??APP ��һ??*/
  uint8_t code3[3] = {0};
  code3[0] = (uint8_t)(passkey / 10000);
  code3[1] = (uint8_t)((passkey / 100) % 100);
  code3[2] = (uint8_t)(passkey % 100);
  smbox_pairing_set_pending(con, code3, passkey);

  /* ��װ���״??+ ����루3 bytes BE??*/
  uint8_t pairing_state_buffer[4] = {0x01};
  pairing_state_buffer[1] = (uint8_t)((passkey >> 16) & 0xFF);
  pairing_state_buffer[2] = (uint8_t)((passkey >> 8) & 0xFF);
  pairing_state_buffer[3] = (uint8_t)(passkey & 0xFF);

  log_info("ble_pairing: send passkey=%06u\n", passkey);

  /* ����Э��/֪ͨ */
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

  /* ��Э��ջ����ʵ����� */
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

/* �ȵ��ú���ĺ���������ǰ��������������ʽ�������µ����ͳ�ͻ??*/
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

/* f7f2: �������ò����·� + �ذ��������ڱ��ļ��󲿣�����ǰ������������ʽ����??*/
void send_vehicle_set_param_instruct(uint16_t protocol_id, uint8_t *instruct);

/* Կ��/���õ�ͨ�ûذ����� f7f1 notify ??APP���� app_core ??convert+AES����??btstack ѹ��??*/
static void ble_reply_to_app_f7f1_post(uint16_t protocol_id, const uint8_t *payload, uint16_t payload_len);

/* ���ֿ�ȱ??δ����ģ��ĺ��������ṩ��??stub �Ա�֤����ͨ�� */
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
static s8 g_last_persist_vol = -2; // -2: δ��ʼ��
static void volume_apply_cb(int req_ptr);
void set_volume_instruct(uint16_t protocol_id)
{
  /* �������ã���??SOC ���� + ת�� MCU + �ظ� APP
   * ������ʽ?? bytes������Ϊ [0]=code [1]=parm_id [2]=parm_value
   */
  if (!fill_require_content(protocol_id, 3)) {
    log_info("set_volume invalid payload (len=%d)\n", content_length);
    return;
  }

  u8 parm_id = content_data[1];
  u8 vol_val = content_data[2];       // APP �����ԭʼ��λ��1=??3=�ߣ�
  (void)parm_id;
  log_info("set_volume: parm_id=%u ������СӦ�����??%u\n", (unsigned)parm_id, (unsigned)vol_val);
  /* ��λ����??/2/3 �ֱ��ʾ ????�ߣ�ֵԽ������Խ��
   * - ����??1/2/3��ֱ��ʹ??
   * - ����??0~100��������ӳ��??1/2/3����??APP ���ܵ�ʵ�֣�
   */
  u8 mapped;
  if (vol_val >= 1 && vol_val <= 3) {
    mapped = vol_val; // 1=??2=??3=??
  } else if (vol_val <= 100) {
    if (vol_val <= 33) {
      mapped = 1; // ??
    } else if (vol_val <= 66) {
      mapped = 2; // ??
    } else {
      mapped = 3; // ??
    }
  } else {
    mapped = 2;
  }

  /* ======== ���� SOC ������Ƶ���� ======== */
  u8 sys_max = get_max_sys_vol();
  log_info("set_volume: �����??%d\n", sys_max);

  s8 soc_volume = 0;
  switch (mapped) {
  case 3:  // �͵������͵�һ���ϵ��������������
    soc_volume = 15;
    break;
  case 2:  // �е�����ߵ��������ϣ�������??
    soc_volume = 17;
    if (soc_volume < 1) {
      soc_volume = 1;
    }
    break;
  case 1:  // �ߵ�����??�������� ���Ը��������??
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
  /* ������ͬ??����Ͷ�ݵ� app_core �̣߳������� btstack �߳���ֱ�Ӳ�??flash/��Ƶ�����쳣 */
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

  /* ����������MUSIC/WTONE ͳһ���䣬�����л�����ʾ��˲ʱ������ */
  app_audio_set_volume(APP_AUDIO_STATE_MUSIC, soc_volume, 1);
  app_audio_set_volume(APP_AUDIO_STATE_WTONE, soc_volume, 1);

  /* ======== ??MCU + ??APP ======== */
  uart1_send_toMCU(protocol_id, content_data, content_length);

  uint8_t reply_buff[3];
  reply_buff[0] = content_data[0];
  reply_buff[1] = content_data[1];
  reply_buff[2] = vol_val; // ����ԭʼ��λ??APP

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
  /* ͬ��ȫ������������ػ�����ʱ�þ�ֵ���������� */
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

  /* ���� MUSIC ״̬������������������ʾ��/��������ʱ��������������??*/
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
/* ��֪ͨ���У���??pending ʱֱ�Ӷ�??*/
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

/* 0x0033 ��ȡ����������Ϣ���� btstack Ͷ�ݵ� app_core ִ�У���??btstack ջ��??*/
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
//==============================������ʱ����notify--->0x0012��api�ӿ�==============================

uint8_t vehicle_control_buff[] = {
    0x01,                   // �����ϱ�
    0x00, 0x00, 0x00, 0x00, // �����������
    0x00, 0x00, 0x00, 0x00, // ��������??
    0x49,                   // ��ص���
    0x00, 0xFF,             // ��������
    0x00,                   // ���״??
    0x01,                   // ����??
    0x00,                   // ��ѹ��ʶ
    0x00,                   // ����״??0x00��ʾ�Ѿ����
    0x04,                   // GPS
    0x04,                   // GSM
};
typedef struct {
  uint8_t report_type;      // �����ϱ����� (0x01)
  uint32_t current_mileage; // ����������� (4�ֽ�)
  uint32_t total_mileage;   // ��������??(4�ֽ�)
  uint8_t battery_level;    // ��ص��� (0x49 = 73%)
  uint16_t endurance;       // �������� (0x00FF = 255)
  uint8_t defense_status;   // ���״??(0x01)
  uint8_t power_state;      // ���µ�״??(0x01)
  uint8_t voltage_flag;     // ��ѹ��ʶ (0x00)
  uint8_t vehicle_status;   // ����״??(0x00��ʾ�Ѿ����)
  uint8_t gps_status;       // GPS״??(0x04)
  uint8_t gsm_status;       // GSM״??(0x04)
} vehicle_status_t;

// �����ṹ??vehicle_status_t ʵ���������ϵ�״̬Ĭ��ֵΪ 1
vehicle_status_t vehicle_status = {.power_state = 1};
static u32 vehicle_control_timer_handle = 0; // ��ʱ����??
static u8 vehicle_control_timer_enabled = 0; // ��ʱ��״̬��??
extern hci_con_handle_t smartbox_get_con_handle(void);
// ���Ӷ�ʱ������������??
static void vehicle_control_timer_handler(void) {
  /* ��Ҫ���ú��������ڶ�ʱ���ص������ģ����ܳ�ʱ������??
   * ԭ��??while+os_time_dly(1) �����??~200ms���ᵼ��ϵͳ���ȱ���������??app_core ���и����׶�??OS_Q_FULL)??
   * ��������Ϊ�첽��??MCU���������Ѿ��յ�����һ�ֵ� 0x0012 ���ݾ͸��»��棬���������ϴλ�������ϱ�??
   */
  
  static uint32_t last_current_mileage = 0;
  static uint32_t last_total_mileage = 0;
  static uint8_t last_battery_level = 0;
  static uint16_t last_endurance = 0;
  static bool first_update = true;
  
#define MAX_CURRENT_MILEAGE_DELTA 1000  // ����������̵������������20����
#define MAX_TOTAL_MILEAGE_DELTA 1000     // ����̵������������20����
#define MAX_BATTERY_LEVEL_DELTA 20        // �����������仯��20%
#define MAX_ENDURANCE_DELTA 500          // �����������仯��50����
  
  // 1. ��������� MCU
uart1_send_toMCU(0x0012, NULL, 0);
  
  // 2. �������״??
  hci_con_handle_t cur_con_handle = smartbox_get_con_handle();
  if (!cur_con_handle) {
    log_info("vehicle_control_timer: no connection\n");
    return;
  }
  
  // 3. �ȴ� MCU �ظ�һ�Σ���??~20ms�����ټ�鲢���ơ����⡰�����ѷ�������û�ذ�������һֱ�ò�������
  bool data_valid = false;
  const uint16_t expect_len = sizeof(vehicle_control_buff);
  for (int retry = 0; retry < 3 && !data_valid; retry++) {
    if (retry) {
      os_time_dly(2); // С��ʱ��??MCU �ذ������ⳤʱ������
    }

    OS_ENTER_CRITICAL();
    if (uart_protocol_id == 0x0012 && uart_data != NULL && data_length >= expect_len) {
      size_t copy_len = data_length;
      if (copy_len > sizeof(vehicle_control_buff)) {
        copy_len = sizeof(vehicle_control_buff);
      }
      memcpy(vehicle_control_buff, uart_data, copy_len);
      data_valid = true;

      // ��Ҫ��ֻ����Ѵ���������??uart_data�������ط����ܻ���Ҫʹ�ã�
      // ��Ҫ��� protocol_id �����ظ�����
      uart_protocol_id = 0;
    }
    OS_EXIT_CRITICAL();
  }
  
  // 4. ���ݺ����Լ�??
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
    
    // ��鱾����������Ƿ���??
    if (new_current_mileage > last_current_mileage &&
        (new_current_mileage - last_current_mileage) > MAX_CURRENT_MILEAGE_DELTA) {
      log_info("vehicle_control_timer: abnormal current_mileage %u -> %u\n", 
               last_current_mileage, new_current_mileage);
      data_abnormal = true;
    }
    
    // ���������Ƿ���??
    if (new_total_mileage > last_total_mileage &&
        (new_total_mileage - last_total_mileage) > MAX_TOTAL_MILEAGE_DELTA) {
      log_info("vehicle_control_timer: abnormal total_mileage %u -> %u\n", 
               last_total_mileage, new_total_mileage);
      data_abnormal = true;
    }
    
    // �������Ƿ���??
    if ((new_battery_level > last_battery_level &&
         (new_battery_level - last_battery_level) > MAX_BATTERY_LEVEL_DELTA) ||
        (new_battery_level < last_battery_level &&
         (last_battery_level - new_battery_level) > MAX_BATTERY_LEVEL_DELTA)) {
      log_info("vehicle_control_timer: abnormal battery_level %u -> %u\n", 
               last_battery_level, new_battery_level);
      data_abnormal = true;
    }
    
    // ��������Ƿ���??
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
      // ����������������ʷ??
      last_current_mileage = new_current_mileage;
      last_total_mileage = new_total_mileage;
      last_battery_level = new_battery_level;
      last_endurance = new_endurance;
    }
  } else if (data_valid && first_update) {
    // �״θ��£�ֱ��ʹ�����ݲ�������ʷ??
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
  
  // 5. ʹ�ÿ������ݷ��ͣ���ʹ MCU û�лظ���Ҳʹ���ϴε����ݣ�
  uint8_t vehicle_control_len = sizeof(vehicle_control_buff);
  uint8_t buffer[256] = {0};
  
  // ���������ϱ�����
  vehicle_status.report_type = vehicle_control_buff[0]; // ���������ϱ�����

  // ���±����������
  vehicle_status.current_mileage = (vehicle_control_buff[1] << 24) |
                                   (vehicle_control_buff[2] << 16) |
                                   (vehicle_control_buff[3] << 8) |
                                   vehicle_control_buff[4]; // ���±����������

  // ������������??
  vehicle_status.total_mileage = (vehicle_control_buff[5] << 24) |
                                 (vehicle_control_buff[6] << 16) |
                                 (vehicle_control_buff[7] << 8) |
                                 vehicle_control_buff[8]; // ������������??

  // ���µ�ص���
  vehicle_status.battery_level = vehicle_control_buff[9]; // ���µ�ص���

  // ������������
  vehicle_status.endurance = (vehicle_control_buff[10] << 8) |
                             vehicle_control_buff[11]; // ������������

  // ��������״??
  vehicle_status.defense_status = vehicle_control_buff[12]; // ��������״??

  // �������µ�״??
  vehicle_status.power_state = vehicle_control_buff[13]; // �������µ�״??

  // ���µ�ѹ��ʶ
  vehicle_status.voltage_flag = vehicle_control_buff[14]; // ���µ�ѹ��ʶ

  // ���³���״??
  vehicle_status.vehicle_status = vehicle_control_buff[15]; // ���³���״??

  // ����GPS״??
  vehicle_status.gps_status = vehicle_control_buff[16]; // ����GPS״??

  // ����GSM״??
  vehicle_status.gsm_status = vehicle_control_buff[17]; // ����GSM״??

  // ���Э��ṹ??
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
  /* ͳһͨ�� le_multi_smartbox_module �ַ����ڲ����ж��Ƿ�ע���˻�??*/
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

// ������ʱ����??
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

// ֹͣ��ʱ����??
void vehicle_control_timer_stop(void) {
  if (vehicle_control_timer_handle) {
    log_info("vehicle_control_timer: stopping\n");
    sys_timeout_del(vehicle_control_timer_handle);
    vehicle_control_timer_handle = 0;
    vehicle_control_timer_enabled = 0;
  }
}

// ��鶨ʱ��״̬��??
u8 vehicle_control_timer_is_running(void) {
  return vehicle_control_timer_enabled;
}
//======================================================================================
//------
/**
 * @brief �������ò���
 *
 * @param protocol_id Э��ID
 */
bool set_configuration(uint16_t protocol_id) {
  uint8_t set_data[1];
  if (content_data == NULL || content_length < 1) {
    log_info("set_configuration: invalid content_length=%d", content_length);
    return false;
  }
  set_data[0] = content_data[0];
  // �����õ������ò�??
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
 * @brief   �˺����������MCU_SOCЭ��
 * 
 * @param data {������Э������}
 * @param len {Э�����ݳ���}
 * @param handle {Э����}
 * @retval {true: ���ɹ�, false: ���ʧ��}  
 */
int fill_MCU_SOC_protocl(uint8_t *data, uint16_t len, uint16_t handle) {
  if (len < sizeof(t_ble_protocl)) {
    log_info("data len %d is too short", len);
    return 0;
  }
  // �Ƚ�����??----�������S0��Э�飬Ҳ���ǲ��ü��ܺͽ���ֱ��������ݣ�S0�ķ������豸������Ϣ����(f5f0)���豸��¼��??f6f0)���豸������??f8f0)������Ͷ??f9f0)��������??fe00)
  switch (handle) {
  // S0���� - �豸������Ϣ����(f5f0)
  case ATT_CHARACTERISTIC_0000f5f1_0000_1000_8000_00805f9b34fb_01_VALUE_HANDLE:
    // log_info("S0����: �豸������Ϣ����(f5f0) - ֱ���������");
    fill_recv_protocl(data);

    break;

  // S0���� - �豸��¼����(f6f0)
  case ATT_CHARACTERISTIC_0000f6f2_0000_1000_8000_00805f9b34fb_01_VALUE_HANDLE:
    // log_info("S0����: �豸��¼����(f6f0) - ֱ���������");
    fill_recv_protocl(data);

    break;

  // S0���� - �豸��������(f8f0)
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
 * @brief  �˺���������䷢��Э??
 * 
 * @param cmd {����ָ��}
 * @param data {����ָ��}
 * @param len {���ݳ���}
 */
void fill_send_protocl(uint16_t cmd, uint8_t *data, uint16_t len) {
  send_protocl.head_prefix = 0xFE;
  send_protocl.f_total = 0x01;
  send_protocl.f_num = 0x01;
  /* len �ֶΰ���Э�飺��??payload �ĳ��ȣ�������ͷ/??CRC �ȹ̶��ֶΣ�??*/
  send_protocl.len = len;
  send_protocl.cmd = cmd;
  send_protocl.data = data;
  send_protocl.tail_prefix = 0x0D0A;

  // ����CRC��������??
  uint8_t crc_packet[256];
  size_t packet_length = 0;

  // ��ʼ�� (1�ֽ�)
  crc_packet[packet_length++] = send_protocl.head_prefix;

  // ��֡���� (2�ֽ�) - ���??
  crc_packet[packet_length++] = (send_protocl.f_total >> 8) & 0xFF;
  crc_packet[packet_length++] = send_protocl.f_total & 0xFF;

  // ֡��??(2�ֽ�) - ���??
  crc_packet[packet_length++] = (send_protocl.f_num >> 8) & 0xFF;
  crc_packet[packet_length++] = send_protocl.f_num & 0xFF;

  // �����ֶ� (2�ֽ�) - ���??
  crc_packet[packet_length++] = (send_protocl.len >> 8) & 0xFF;
  crc_packet[packet_length++] = send_protocl.len & 0xFF;
  printf("���͵����ݳ���: %d\n", send_protocl.len);

  // Э��ID (2�ֽ�) - ���??
  crc_packet[packet_length++] = (send_protocl.cmd >> 8) & 0xFF;
  crc_packet[packet_length++] = send_protocl.cmd & 0xFF;

  // Э������ (�ɱ䳤��)
  if (send_protocl.data && len > 0) {
    memcpy(&crc_packet[packet_length], send_protocl.data, len);
    packet_length += len;
  }

  // ����??(2�ֽ�) - ���??
  crc_packet[packet_length++] = (send_protocl.tail_prefix >> 8) & 0xFF;
  crc_packet[packet_length++] = send_protocl.tail_prefix & 0xFF;

  // ����CRC��������
  uint8_t crc_data[2];
  // size_t crc_data_length = build_crc_data(crc_packet, packet_length,
  // crc_data);
  log_info("packetLength: %zu", packet_length);

  // ����CRCУ��??
  calculateCRC16(crc_packet, packet_length, crc_data);
  send_protocl.crc16 = (crc_data[1] << 8) | crc_data[0];
  log_info("CRC calculated: 0x%04X", send_protocl.crc16);
}


/**
 * @brief  �˺������ڽ�Э��ת��Ϊ���ͻ�??
 * 
 * @param protocol {Э��ṹ��ָ��}
 * @param buffer {���ͻ���ָ��}
 * @param buffer_size {���ͻ����С}
 * @retval {ʵ����䵽���ͻ���ĳ���}
 */
uint16_t convert_protocol_to_buffer(t_ble_protocl *protocol,uint8_t *buffer,uint16_t buffer_size)
{
    if (!protocol || !buffer ||
    buffer_size < 13) { // ��С���ȣ�1+2+2+2+2+2+2=13�ֽ�
    log_info("Invalid parameters for protocol conversion");
    return 0;
  }

  if ((uint32_t)buffer_size < (uint32_t)(13 + protocol->len)) {
    log_info("Buffer too small for protocol conversion: need=%u, have=%u",
             (unsigned)(13 + protocol->len), (unsigned)buffer_size);
    return 0;
  }

  uint16_t offset = 0;

  // ��ʼ�� (1�ֽ�)
  buffer[offset++] = protocol->head_prefix;

  // ��֡���� (2�ֽ�) - ���??
  buffer[offset++] = (protocol->f_total >> 8) & 0xFF;
  buffer[offset++] = protocol->f_total & 0xFF;

  // ֡��??(2�ֽ�) - ���??
  buffer[offset++] = (protocol->f_num >> 8) & 0xFF;
  buffer[offset++] = protocol->f_num & 0xFF;

  // �����ֶ� (2�ֽ�) - ���??
  buffer[offset++] = (protocol->len >> 8) & 0xFF;
  buffer[offset++] = protocol->len & 0xFF;

  // Э��ID (2�ֽ�) - ���??
  buffer[offset++] = (protocol->cmd >> 8) & 0xFF;
  buffer[offset++] = protocol->cmd & 0xFF;

  // Э������ (�ɱ䳤��)
  if (protocol->data && protocol->len > 0) {
    memcpy(&buffer[offset], protocol->data, protocol->len);
    offset += protocol->len;
  }

  // CRCУ�� (2�ֽ�) - ����򣨸��ֽ���ǰ�����ֽ��ں�
  buffer[offset++] = (protocol->crc16 >> 8) & 0xFF;
  buffer[offset++] = protocol->crc16 & 0xFF;

  // ����??(2�ֽ�) - ���??
  buffer[offset++] = (protocol->tail_prefix >> 8) & 0xFF;
  buffer[offset++] = protocol->tail_prefix & 0xFF;

  log_info_hexdump(buffer, offset);

  return offset;
}


int fill_recv_protocl(uint8_t *data)
{
     // ���ID??
  protocol_id = 0;
  if (data == NULL) {
    log_info("fill_recv_protocl: data is NULL");
    return 0;
  }

  // ��鿪ʼ�� (0xFE)
  if (data[0] != 0xFE) {
    log_info("fill_recv_protocl: invalid start byte 0x%02X, expected 0xFE",
             data[0]);
    return 0;
  }

  // ��ȡ���ݰ����ȣ��ӿ�ʼ�������������ܳ��ȣ�
  uint16_t packet_length = (data[5] << 8) | data[6]; // ??-6�ֽ��ǳ�����??
  // �������� (0x0D 0x0A)
  if (data[packet_length - 2] != 0x0D || data[packet_length - 1] != 0x0A) {
    log_info("fill_recv_protocl: invalid end bytes 0x%02X 0x%02X, expected "
             "0x0D 0x0A",
             data[packet_length - 2], data[packet_length - 1]);
    return false;
  }

  // ��ȡ���յ���CRCֵ�����ݰ���??�ֽڵ�ǰ2�ֽ���CRC??
  uint8_t received_crc_low = data[packet_length - 4];
  uint8_t received_crc_high = data[packet_length - 3];

  // CRC У�� - У�鷶Χ����ʼ��������������������CRC����
  // CRC���㳤�� = �ܳ�??- 2 (ֻȥ��CRC�ֶ�??�ֽ�)
  uint16_t crc_calc_length = packet_length - 4;

  // ����CRC���ӿ�ʼ��������������������������������CRC�ֶ�??
  uint8_t calculated_crc[2];
  // ��ҪΪ���ݲ��ּ�������β��??0x0D,0x0A)
  uint16_t crc_buffer_len = (uint16_t)(crc_calc_length + 2);
  uint8_t *crc_buffer = malloc(crc_buffer_len);
  if (crc_buffer == NULL) {
    log_info("fill_recv_protocl: malloc crc_buffer failed");
    return 0;
  }

  // �������ݲ��֣��ų�CRC�ֶ�??
  for (uint16_t i = 0; i < crc_calc_length; i++) {
    crc_buffer[i] = data[i];
  }
  // ���ӽ���??(0x0D 0x0A) ����ʱ����ĩ??
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

  // ��ȡЭ���ֶ�
  total_frames = (data[1] << 8) | data[2]; // ��֡����
  frame_number = (data[3] << 8) | data[4]; // ֡��??
  protocol_id = (data[7] << 8) | data[8];  // Э��ID??

  // ����Э�����ݳ��ȣ��ܳ�??- �̶��ֶγ���(13�ֽ�)
  content_length = packet_length - 13;
  // Э��������ʼλ��
  content_data = &data[9]; // Э��������ʼλ��

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

  // Э�������Ѿ�ָ�� data ���ڲ����壨content_data = &data[9]���������ٴθ���
  free(crc_buffer);
  return 1;
}

/**
 * @brief ���ֻ���������ݵ�SOC_Phone_protocl�ṹ??
 *
 * @param data ��������ָ��
 * @param len �������ݳ���
 * @param handle ����ֵ��??
 * @return true ���ɹ�
 * @return false ���ʧ��
 *
 * @note
 * �ú�����������ֵ����ж��Ƿ�ΪS0������ΪS0������ֱ��������ݣ�������Ҫ���ܺ����
 */
int fill_SOC_Phone_protocl(uint8_t *data, uint16_t len,uint16_t handle)
{
     // �Ƚ�����??----�������S0��Э�飬Ҳ���ǲ��ü��ܺͽ���ֱ��������ݣ�S0�ķ������豸������Ϣ����(f5f0)���豸��¼��??f6f0)���豸������??f8f0)������Ͷ??f9f0)��������??fe00)
  switch (handle) {
  // S0���� - �豸������Ϣ����(f5f0)
  case ATT_CHARACTERISTIC_0000f5f1_0000_1000_8000_00805f9b34fb_01_VALUE_HANDLE:
    // log_info("S0����: �豸������Ϣ����(f5f0) - ֱ���������");
    fill_recv_protocl(data);
    f5f1_ID_dispose(protocol_id);
    break;

  // S0���� - �豸��¼����(f6f0)
  case ATT_CHARACTERISTIC_0000f6f2_0000_1000_8000_00805f9b34fb_01_VALUE_HANDLE:
    log_info("S0����: �豸��¼����(f6f0) - ֱ���������");
    fill_recv_protocl(data);
    f6f2_ID_dispose(protocol_id, content_data);

    break;
  // S0���� - �豸��������(f8f0)
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
  // S1���� - �п�-APP��������(f7f0)
  case ATT_CHARACTERISTIC_0000f7f2_0000_1000_8000_00805f9b34fb_01_VALUE_HANDLE:
    /*
     * f7f2 �����Ǽ������ݣ����ֽ�ͨ������ 0xFE��??
     * ֮ǰ�ȵ�??fill_recv_protocl(data) ���??invalid start byte������������ btstack �������??
     * �����������??0xFE)ʱֱ�ӽ���������ֱ�ӽ��ܺ����??
     */
    printf("join_f7f2");
    if (data[0] == 0xFE) {
      if (fill_recv_protocl(data)) {
        f7f2_ID_dispose(protocol_id);
      }
      break;
    }
    // log_info("S1����: �п�-APP��������(f7f0) - ��Ҫ��??);
    /* btstack ջ�ܽ���������ջ�ϷŴ���??��ṹ�� */
    static AES_KEY aes_key;
    static uint8_t decrypted_data[269]; // ���ܺ�����ݻ���??
    uint16_t decrypted_len = sizeof(decrypted_data);

    // ���ý�����Կ������ʹ��MCUͨ��0x00F7�·������浽syscfg����Կ��
    {
      u8 keybuf[16];
      const u8 *key = fill_protocol_get_aes_key(keybuf);
      aes_set_decrypt_key(&aes_key, key, AES128_KEY_SIZE);
    }
    // ��������
    int aes_ret = aes_decrypt_pkcs(&aes_key, data, len, decrypted_data, &decrypted_len);
    if (aes_ret != 0) {
      /* �����ֻ����á��� AES-ECB ����䡱�� 16 �ֽڣ�PKCS7 У��ᱨ padd_num error??
         ����ʱ��һ���������ܶ��ף�������������ֱ�Ӷ���??*/
      decrypted_len = len;
      for (uint16_t i = 0; i < len / AES_BLOCK_SIZE; i++) {
        aes_decrypt(&aes_key, &data[i * AES_BLOCK_SIZE], &decrypted_data[i * AES_BLOCK_SIZE]);
      }
    }
    printf("decrypted_data:");
    /* btstack ջ���ţ����� hexdump �ĳ��ȣ�������־·������ */
    {
      uint16_t dump_len = decrypted_len;
      if (dump_len > 32) {
        dump_len = 32;
      }
      log_info_hexdump(decrypted_data, dump_len);
    }
    // ʹ�ý��ܺ���������Э��ṹ
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
  /* ֮ǰ���� app_core ���лص�����ʱ������??�����»ذ��߼�����??
   * Ϊȷ���ش�����Ϊֱ��ִ�лذ��߼���������һ���첽Ͷ�ݣ��ɹ���˳������һ�飬��Ӱ����ȷ�ԣ�??
   */
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
        // ��ͨѶ״??
        printf("set_configuration success");
        //play_tone_file_alone(get_tone_files()->no_com);
        printf("no_com");
        // ���uart_data
        // memset(uart_data, 0, data_length);
      } else {
        // ��ͨѶ״??
        printf("set_configuration success");
        //play_tone_file_alone(get_tone_files()->yes_com);
        printf("yes_com");
        // ���uart_data
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
        // ���uart_data
        // memset(uart_data, 0, data_length);
      }
      if (content_data[0] == 1) {
        //play_tone_file_alone(get_tone_files()->statu_60v);
        printf("statu_60v");
        // ���uart_data
        // memset(uart_data, 0, data_length);
      }
      if (content_data[0] == 2) {
        //play_tone_file_alone(get_tone_files()->statu_72v);
        printf("statu_72v");
        // ���uart_data
        // memset(uart_data, 0, data_length);
      }
      if (content_data[0] == 3) {
        //play_tone_file_alone(get_tone_files()->statu_48v);
        printf("statu_48v");
        // ���uart_data
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
        // ���uart_data
        // memset(uart_data, 0, data_length);
      }
      if (content_data[0] == 1) {
        //play_tone_file_alone(get_tone_files()->relieve);
        printf("relieve");
        // ���uart_data
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
        // ���uart_data
        // memset(uart_data, 0, data_length);
      }
      if (content_data[0] == 1) {
        //play_tone_file_alone(get_tone_files()->erro_try);
        printf("erro_try");
        // ���uart_data
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
        // ���uart_data
        // memset(uart_data, 0, data_length);
      }
      if (content_data[0] == 1) {
        //play_tone_file_alone(get_tone_files()->recover);
        printf("recover");
        // ���uart_data
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
    // �����õ������ò�??
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
      // ���uart_data
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
      // ���uart_data
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
    log_info("���͵�ǰ��¼ָ�MCU");
    log_info_hexdump(data, 4);
    break;
  case support_function:
    // �ô��ڷ���֧�ֹ�??
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
    /* ��֧�ֹ������ݷ�װΪЭ����͸� App��������??*/
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
//==============================����f7f2��ID========================================================================
/**
 * @brief  �������ò���ָ�������
 * 
 * @param protocol_id {Э��ID}
 */
void send_vehicle_set_param_instruct(uint16_t protocol_id, uint8_t *instruct) {
  // if (content_data == NULL || content_length < 3) {
  //   log_info("send_vehicle_set_param_instruct: invalid content_length=%d", content_length);
  //   return;
  // }
  
  // ���⴦������ʱ��������Ϊ��ʱ��������??
  if (protocol_id == 0x00F2) {
    uint8_t param_id = content_data[1];
    uint8_t param_value = content_data[2];
    
    if (param_id == APP_FUNC_CODE_PARAID_OVER_TIME_LOCK) {
      log_info("send_vehicle_set_param_instruct: OVER_TIME_LOCK setting detected, value=%d\n", param_value);
      // ȷ������ֵ����Ч��Χ??
      if (param_value > 2) {
        log_info("send_vehicle_set_param_instruct: OVER_TIME_LOCK value %d out of range, clamping to 2\n", param_value);
        content_data[2] = 2;
      }
    }
  }
  uart1_send_toMCU(protocol_id, content_data, content_length);
  // �ȴ���Ӧ
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
    // ������ݲ������ط�״??
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
  // �����ݽ��м�??
  AES_KEY aes_key;

  u16 encrypt_len = sizeof(encrypt_data);
  {
    u8 keybuf[16];
    const u8 *key = fill_protocol_get_aes_key(keybuf);
    aes_set_encrypt_key(&aes_key, key, AES128_KEY_SIZE);
  }
  // ��������
  aes_encrypt_pkcs(&aes_key, send_code, send_code_len, encrypt_data,
                   &encrypt_len);
  printf("encrypt_data���ܺ������:");
  log_info_hexdump(encrypt_data, encrypt_len);
  printf("encrypt_len: %d, send_data_len: %d\n", encrypt_len, send_code_len);

  // ��֤�������ݳ���
  if (encrypt_len > sizeof(send_code)) {
    log_info("�������ݳ��� %zu ������������??%zu", encrypt_len,
             sizeof(send_code));
    // ʹ��ԭʼ���ݳ��ȷ�??
    encrypt_len = send_code_len;
    memcpy(send_code, send_code, send_code_len);
  } else {
    memcpy(send_code, encrypt_data, encrypt_len);
  }
  printf("���շ�����??");
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
  case vehicle_control: // ��������
    if (!fill_require_content(protocol_id, 2)) {
      break;
    }
    log_info("now content_data:");
    log_info("content_data1: %d", content_data[0]);
    log_info("content_data2: %d", content_data[1]);
    uint8_t instruct = content_data[1];
    v5_1_function_control(instruct, protocol_id);
    break;
  case vehicle_state: // ��ȡ����״??
    /* code */        // �˴������f7f2��write_callbake���в��ϵĵ�??
    break;
  case vehicle_set_param: // �������ò���
    /* code */
    vehicle_set_param_instrcut(protocol_id);
    break;
  case get_key_list: // ��ȡ����Կ���б�
    send_ble_key_list(protocol_id);
    /* code */
    break;
  case delete_NFC: // ɾ��NFCԿ��
    if (!fill_require_content(protocol_id, 16)) {
      break;
    }
    delete_NFC_instruct(protocol_id);
    break;
  case empty_NFC: // ���NFCԿ��
    empty_NFC_instruct(protocol_id);
    break;
  case ble_pairing: // �������
    ble_proto_ble_pair_req_Proc(protocol_id);

    // ble_proto_ble_pair_req_Proc(NULL);

    break;
  case ble_key_peripheral_send: // app�·���������Կ��
    /* code */
    if (!fill_require_content(protocol_id, 6)) {
      break;
    }
    ble_key_peripheral_send_instruct_ext(protocol_id);
    break;
  case delete_ble_key: // ɾ������Կ��
    if (!fill_require_content(protocol_id, 6)) {
      break;
    }
    delete_ble_key_instruct(protocol_id);
    break;
  case empty_ble_key: // �������Կ��
    empty_ble_key_instruct(protocol_id);
    break;
  case send_vehicle_password_unlock: // �·������������ָ��
    /* code */
    if (!fill_require_content(protocol_id, 4)) {
      break;
    }
    send_vehicle_password_unlock_instruct(protocol_id);
    break;
  case remove_vehicle_binding: // �������������
    /* code */
    remove_vehicle_binding_instruct(protocol_id);
    break;
  case delet_phone_key: // ɾ���ֻ�Կ��
    if (!fill_require_content(protocol_id, 3)) {
      break;
    }
    delet_phone_bleKey_instrcut(protocol_id);
    break;
  case empty_phone_key: // ����ֻ�Կ��
    /* code */  
    break;
  case get_vehice_set_infromation: // ��ȡ����������Ϣ
      /* ����??payload Ϊ�գ���������·���أ�UART �ȴ� + AES + notify�����ŵ� app_core ??*/
      get_vehicle_set_info_post(protocol_id);
    break;
  case get_vehice_music_infromation: // ��ȡ����������Ϣ
    get_vehice_music_infromation_instruct(protocol_id);
    break;
  case select_tone: // ѡ����Ч��Ĭ??���ԣ�
    select_tone_instruct(protocol_id);
    break;
  case set_vehice_music: // ������Ч��??
    set_vehice_music_instruct(protocol_id);
    break;
  case music_file_send: // �����ļ���??
    music_file_send_instruct(protocol_id);
    break;
  case open_lock: // �򿪳���??
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
 * @brief �����������Ʋ�������ָ��V5.2
 * ����3byte 2byte->code  1byte->parm
 * @param content_data {placeholder} �������Ʋ�������ָ������
 */
void param_set_v5_2_instruct(uint16_t portocol_id, uint8_t *content_data) {
  // ����content_data����??
  uint8_t parm_id = 0;
  // ??6���Ƶ�HEXת��??0����
  printf("content_data1:%d", content_data[1]);
  printf("content_data2:%d", content_data[2]);
  parm_id = content_data[1];
  printf("parm_id:%d", parm_id);
  switch (parm_id) {
  case CFG_VEHICLE_PASSWORD_UNLOCK: // ���������������??
    /* code */
    printf("ƥ��??CFG_VEHICLE_PASSWORD_UNLOCK\n");
    send_vehicle_set_param_instruct(portocol_id, content_data);
    break;
  case APP_FUNC_CODE_PARAID_AUTO_LOCK: // �Զ�����
    printf("ƥ��??PP_FUNC_CODE_PARAID_AUTO_LOCK\n");
    send_vehicle_set_param_instruct(portocol_id, content_data);
    break;
  case APP_FUNC_CODE_PARAID_OVER_TIME_LOCK: // ��ʱ����
    printf("ƥ��??APP_FUNC_CODE_PARAID_OVER_TIME_LOCK\n");
    send_vehicle_set_param_instruct(portocol_id, content_data);
    break;
  case APP_FUNC_CODE_PARAID_AUTO_P: // �Զ�P��λ
    printf("ƥ��??APP_FUNC_CODE_PARAID_AUTO_P\n");
    send_vehicle_set_param_instruct(portocol_id, content_data);
    break;
  case APP_FUNC_CODE_PARAID_SIDE_PROP: // �߳Ÿ�Ӧ
    printf("ƥ��??APP_FUNC_CODE_PARAID_SIDE_PROP\n");
    send_vehicle_set_param_instruct(portocol_id, content_data);
    break;
  case APP_FUNC_CODE_PARAID_CUSHION: // �����Ӧ
    printf("ƥ��??APP_FUNC_CODE_PARAID_CUSHION\n");
    send_vehicle_set_param_instruct(portocol_id, content_data);
    break;
  case APP_FUNC_CODE_PARAID_DELAY_HEADLIGHT: // ��ʱ���
    printf("ƥ��??APP_FUNC_CODE_PARAID_DELAY_HEADLIGHT\n");
    send_vehicle_set_param_instruct(portocol_id, content_data);
    break;
  case APP_FUNC_CODE_PARAID_SENSE_HEADLIGHT: // ��Ӧ���
    printf("ƥ��??APP_FUNC_CODE_PARAID_SENSE_HEADLIGHT\n");
    send_vehicle_set_param_instruct(portocol_id, content_data);
    break;
  case APP_FUNC_CODE_PARAID_NFC: // NFC��??
    printf("ƥ��??APP_FUNC_CODE_PARAID_NFC\n");
    send_vehicle_set_param_instruct(portocol_id, content_data);
    break;
  case APP_FUNC_CODE_PARAID_SOUND: // ��Ч����
    printf("ƥ��??APP_FUNC_CODE_PARAID_SOUND\n");
    send_vehicle_set_param_instruct(portocol_id, content_data);
    break;
  case APP_FUNC_CODE_PARAID_HID_DISTANCE: // �޸н�������
    printf("ƥ��??APP_FUNC_CODE_PARAID_HID_DISTANCE\n");
    APP_FUNC_CODE_PARAID_HID_UNLOCK_instruct(portocol_id, content_data);
    send_vehicle_set_param_instruct(portocol_id, content_data);
    break;
  case APP_FUNC_CODE_PARAID_CRUISE: // ����Ѳ??
    printf("ƥ��??APP_FUNC_CODE_PARAID_CRUISE\n");
    send_vehicle_set_param_instruct(portocol_id, content_data);
    break;
  case APP_FUNC_CODE_PARAID_ASSIST: // ��������
    printf("ƥ��??APP_FUNC_CODE_PARAID_ASSIST\n");
    send_vehicle_set_param_instruct(portocol_id, content_data);
    break;
  case APP_FUNC_CODE_PARAID_ASTERN: // ��������
    printf("ƥ��??APP_FUNC_CODE_PARAID_ASTERN\n");
    send_vehicle_set_param_instruct(portocol_id, content_data);
    break;
  case APP_FUNC_CODE_PARAID_HID_UNLOCK: // �޸н���
    printf("ƥ��??APP_FUNC_CODE_PARAID_HID_UNLOCK\n");
    send_vehicle_set_param_instruct(portocol_id, content_data);
    Seamless_Unlocking(portocol_id, content_data);
    break;
  case APP_FUNC_CODE_PARAID_AUTO_LOCK_TIME: // �Զ�����ʱ��
    printf("ƥ��??APP_FUNC_CODE_PARAID_AUTO_LOCK_TIME\n");
    send_vehicle_set_param_instruct(portocol_id, content_data);
    break;
  case APP_FUNC_CODE_PARAID_OVER_TIME_LOCK_TIME: // ��ʱ����ʱ��
    printf("ƥ��??APP_FUNC_CODE_PARAID_OVER_TIME_LOCK_TIME\n");
    send_vehicle_set_param_instruct(portocol_id, content_data);
    break;

  case APP_FUNC_CODE_PARAID_VOLUME: // ��������
    printf("ƥ��??APP_FUNC_CODE_PARAID_VOLUME\n");
    set_volume_instruct(portocol_id);

    break;
  case APP_FUNC_CODE_PARAID_ALARM_SHAKE: // �𶯱�����??
    printf("ƥ��??APP_FUNC_CODE_PARAID_ALARM_SHAKE\n");
    send_vehicle_set_param_instruct(portocol_id, content_data);
    break;
  case APP_FUNC_CODE_PARAID_ALARM_DUMP: // �㵹��??
    printf("ƥ��??APP_FUNC_CODE_PARAID_ALARM_DUMP\n");
    send_vehicle_set_param_instruct(portocol_id, content_data);
    break;
  case APP_FUNC_CODE_PARAID_ALARM_MOVE: // �ƶ�����
    printf("ƥ��??APP_FUNC_CODE_PARAID_ALARM_MOVE\n");
    send_vehicle_set_param_instruct(portocol_id, content_data);
    break;
  case APP_FUNC_CODE_PARAID_ALARM_SHAKE_SENSE: // �𶯱�������??
    printf("ƥ��??APP_FUNC_CODE_PARAID_ALARM_SHAKE_SENSE\n");
    send_vehicle_set_param_instruct(portocol_id, content_data);
    break;
  case APP_FUNC_CODE_PARAID_FEST_EFFECT: // ������Ч
    printf("ƥ��??APP_FUNC_CODE_PARAID_FEST_EFFECT\n");
    send_vehicle_set_param_instruct(portocol_id, content_data);
    break;
  case APP_FUNC_CODE_PARAID_FACTORY_RESET: // �̶�??
    printf("ƥ��??APP_FUNC_CODE_PARAID_FACTORY_RESET\n");
    send_vehicle_set_param_instruct(portocol_id, content_data);
    break;
  case APP_FUNC_CODE_PARAID_TCS: // �໬
    printf("ƥ��??APP_FUNC_CODE_PARAID_TCS\n");
    send_vehicle_set_param_instruct(portocol_id, content_data);
    break;
  case APP_FUNC_CODE_PARAID_ABS: // ����ɲ��
    printf("ƥ��??APP_FUNC_CODE_PARAID_ABS\n");
    send_vehicle_set_param_instruct(portocol_id, content_data);
    break;
  case APP_FUNC_CODE_PARAID_HHC: // �µ�פ��
    printf("ƥ��??APP_FUNC_CODE_PARAID_HHC\n");
    send_vehicle_set_param_instruct(portocol_id, content_data);
    break;
  default:
    printf("δƥ�䵽�κ�caseparm_id = %d\n", parm_id);
    break;
  }
}
void vehicle_set_param_instrcut(uint16_t portocol_id) {
  param_set_v5_2_instruct(portocol_id, content_data);
}
// ���µ�״̬�ظ����ӳٷ���������
static struct {
  uint8_t pending;
  uint8_t original_state;
  uint8_t new_state;
  uint8_t reply_buff[20]; // �㹻���vehicle_control_buff
} g_power_state_reply_ctx = {0};

// �ӳٻظ��ص�����
static void reply_power_state_delay_cb(void *priv) {
  (void)priv;
  
  if (!g_power_state_reply_ctx.pending) {
    return;
  }
  g_power_state_reply_ctx.pending = 0;
  
  log_info("�ӳٷ������µ�״̬��?? ??%d ??%d\n", 
           g_power_state_reply_ctx.original_state, 
           g_power_state_reply_ctx.new_state);
  
  // ���Э��ṹ�岢��??x0012
  fill_send_protocl(0x0012, g_power_state_reply_ctx.reply_buff, sizeof(vehicle_control_buff));
  
  static uint8_t f7f1_send_data[269];
  memset(f7f1_send_data, 0, sizeof(f7f1_send_data));
  u16 f7f1_len = convert_protocol_to_buffer(&send_protocl, f7f1_send_data, sizeof(f7f1_send_data));
  
  if (f7f1_len == 0) {
    log_info("reply_power_state_delay: convert failed\n");
    return;
  }
  
  // ͨ���ص��ַ�
  le_multi_dispatch_recieve_callback(0, f7f1_send_data, f7f1_len);
  
  // ����notify��APP
  if (le_multi_app_send_user_data_check(f7f1_len)) {
    if (le_multi_app_send_user_data(
            ATT_CHARACTERISTIC_0000f7f1_0000_1000_8000_00805f9b34fb_01_VALUE_HANDLE,
            f7f1_send_data, f7f1_len, ATT_OP_NOTIFY) == APP_BLE_NO_ERROR) {
      log_info("���µ�״̬�ظ���?? ??%d ??%d\n", 
               g_power_state_reply_ctx.original_state, 
               g_power_state_reply_ctx.new_state);
    } else {
      log_info("���µ�״̬�ظ�����ʧ��\n");
    }
  } else {
    log_info("���µ�״̬��?? ��ǰ�޷�����\n");
  }
}

/**
 * @brief ���µ���ƺ�ظ�APPԭ״̬����״̬���ӳ�200ms���ͣ�����BLEӵ��??
 * @param instruct ����ָ��(8=�ϵ�/����, 9=�µ�/����)
 * @param protocol_id Э��ID
 * @param original_power_state ԭʼ���µ�״??
 * @param new_power_state �µ����µ�״??
 */
static void reply_power_state_change_to_app(uint16_t instruct, uint16_t protocol_id, 
                                             uint8_t original_power_state, uint8_t new_power_state) {
  (void)instruct;
  (void)protocol_id;
  
  log_info("���µ�״̬��?? ԭ״??%d, ��״??%d (�ӳ�200ms��??\n", original_power_state, new_power_state);
  
  // ��??x0012����״̬��??
  memcpy(g_power_state_reply_ctx.reply_buff, vehicle_control_buff, sizeof(vehicle_control_buff));
  
  // ����Ϊ�ظ���??��������??
  g_power_state_reply_ctx.reply_buff[0] = 0x00; // 0=�ظ�??=�����ϱ�
  
  // �������µ�״̬Ϊ��״??
  g_power_state_reply_ctx.reply_buff[13] = new_power_state;
  
  // ���� vehicle_status �ṹ??
  vehicle_status.power_state = new_power_state;
  
  // ����״̬������??
  g_power_state_reply_ctx.original_state = original_power_state;
  g_power_state_reply_ctx.new_state = new_power_state;
  g_power_state_reply_ctx.pending = 1;
  
  // �ӳ�200ms���ͣ�����??x00F1�ظ�ͬʱ���͵���BLE bufferӵ��
  sys_timeout_add(NULL, reply_power_state_delay_cb, 200);
}

/**
 * @brief ������������ָ�� f7f1 vehicle_control
 *
 * @param instruct ��������ָ��
 */
void v5_1_function_control(uint16_t instruct, uint16_t protocol_id) {
  /* ������������ payload �쳣����ָ��/���� 0����ֱ�ӷ��أ������??UART ��??�ذ�ʹ�÷Ƿ��ڴ浼�±��� */
  if (content_data == NULL || content_length == 0) {
    log_error("v5_1_function_control invalid payload (len=%u)\n", content_length);
    return;
  }
   bool instruct_valid = false;
  switch (instruct) {
  case APP_FUNC_CODE_EBIKE_UNLOCK: // �����������ϵ磩
  {
    log_info("��������ָ��ϵ磩\n");
    // 1. ����ԭ�������µ�״??
    uint8_t original_power_state = vehicle_status.power_state;
    
    // 2. ���Ϳ���ָ�MCU����������ֻ??�ֽڣ�ֱ���� UART ����??
    log_info("���͸�MCU����??0x%04x len=%u data =%d%d", protocol_id, content_length, content_data[0], content_data[1]);
    if (content_length) {
      log_info_hexdump(content_data, content_length);
    }
    bool unlock_success = uart1_send_toMCU(protocol_id, content_data, content_length);
    if (unlock_success) {
      log_info("��������ָ��ͳɹ�\n");
    } else {
      log_info("��������ָ���ʧ��\n");
    }
    g_ble_skip_uart_forward_once = 1;
    send_data_to_ble_post(instruct, protocol_id);
    
    // 3. �����Ƿ��յ�MCU??x0012�ظ�������APP�ظ����µ�״̬��??
    // �ϵ����״̬Ӧ??x01�����ϵ�??
    uint8_t new_power_state = 0x01;
    reply_power_state_change_to_app(instruct, protocol_id, original_power_state, new_power_state);
  }
    break;
  case APP_FUNC_CODE_EBIKE_LOCK: // �����������µ磩
  {
    log_info("��������ָ��µ磩\n");
    // 1. ����ԭ�������µ�״??
    uint8_t original_power_state = vehicle_status.power_state;
    
    // 2. ���Ϳ���ָ�MCU����������ֻ??�ֽڣ�ֱ���� UART ����??
    log_info("���͸�MCU����??0x%04x len=%u data =%d%d", protocol_id, content_length, content_data[0], content_data[1]);
    if (content_length) {
      log_info_hexdump(content_data, content_length);
    }
    bool lock_success = uart1_send_toMCU(protocol_id, content_data, content_length);
    if (lock_success) {
      log_info("��������ָ��ͳɹ�\n");
    } else {
      log_info("��������ָ���ʧ��\n");
    }
    g_ble_skip_uart_forward_once = 1;
    send_data_to_ble_post(instruct, protocol_id);
    
    // 3. �����Ƿ��յ�MCU??x0012�ظ�������APP�ظ����µ�״̬��??
    // �µ����״̬Ӧ??x00�����µ�??
    uint8_t new_power_state = 0x00;
    reply_power_state_change_to_app(instruct, protocol_id, original_power_state, new_power_state);
  }
    break;  
  case APP_FUNC_CODE_OPEN_SEAT: // ����Ͱ
    log_info("����Ͱָ��\n");
    log_info("���͸�MCU����??0x%04x len=%u data =%d%d", protocol_id, content_length, content_data[0], content_data[1]);
    if (content_length) {
      log_info_hexdump(content_data, content_length);
    }
   bool seat_open_success = uart1_send_toMCU(protocol_id, content_data, content_length);
    if (seat_open_success) {
      log_info("��Ͱ��ָ��ͳɹ�\n");
    } else {
      log_info("��Ͱ��ָ���ʧ��\n");
    }
    g_ble_skip_uart_forward_once = 1;
    send_data_to_ble_post(instruct, protocol_id);
    break;
  case APP_FUNC_CODE_FIND_EBIKE: // ���ҳ���
    log_info("���ҳ���ָ��\n");
    /* ��������ֻ��һ���ֽ�ָ�send_data_to_ble_post ����Ϊ��??2��ֱ�ӷ��أ�����û��ת����MCU??
     * ������ֱ��ת����MCU����֤Ѱ��ָ��ش���� BLE �ظ�����??
     */
    log_info("���͸�MCU����??0x%04x len=%u data =%d%d", protocol_id, content_length, content_data[0], content_data[1]);
    if (content_length) {
      log_info_hexdump(content_data, content_length);
    }
    bool find_ebike_success = uart1_send_toMCU(protocol_id, content_data, content_length);
    if (find_ebike_success) {
      log_info("���ҳ���ָ��ͳɹ�\n");
    } else {
      log_info("���ҳ���ָ���ʧ��\n");
    }
    g_ble_skip_uart_forward_once = 1;
    send_data_to_ble_post(instruct, protocol_id);
    break;
  case APP_FUNC_CODE_REBOOT_EBIKE: // �����п�
    log_info("�����п�ָ��\n");
    uart1_send_toMCU(protocol_id, content_data, content_length);
    g_ble_skip_uart_forward_once = 1;
    send_data_to_ble_post(instruct, protocol_id);
    break;
  case APP_FUNC_CODE_PAIR_NFC: // ���NFC
    log_info("���NFCָ��\n");
    uart1_send_toMCU(protocol_id, content_data, content_length);
    g_ble_skip_uart_forward_once = 1;
    send_data_to_ble_post(instruct, protocol_id);
    break;
  case APP_FUNC_CODE_PAIR_REMOTE_KEY: // ���ң��??
    log_info("���ң����ָ��\n");
    uart1_send_toMCU(protocol_id, content_data, content_length);
    g_ble_skip_uart_forward_once = 1;
    send_data_to_ble_post(instruct, protocol_id);
    break;
  case APP_FUNC_CODE_CHECK_EBIKE: // ϵͳ��??
    log_info("ϵͳ���ָ��\n");
    send_data_to_ble_post(instruct, protocol_id);
    break;
  case APP_FUNC_CODE_EBIKE_LOCKING: // ��������
    log_info("��������ָ��\n");
    send_data_to_ble_post(instruct, protocol_id);
    vehicle_control_timer_handler(); // ����0x0012 ����APP�˸��³�����״??
    break;
  case APP_FUNC_CODE_EBIKE_UNLOCKING: // ��������
    log_info("��������ָ��\n");
    send_data_to_ble_post(instruct, protocol_id);
    break;
  case APP_FUNC_CODE_SOUND_THEME: // ��Ч��������
    log_info("��Ч��������ָ��\n");
    send_data_to_ble_post(instruct, protocol_id);
    break;
  case APP_FUNC_CODE_NAVIGATION_SCREEN: // ����Ͷ��
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
      /* app_core ��Ϣ����������������ֱ��??BLE ����·������??
       * ˵����ԭ��ͨ�� app_core �ص��첽ִ�� send_data_to_ble()����������ʱ�ᱻ drop������ָ�ʧ??
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

  /* ���/ȡ����Ժ��ֻ����������� CCC ��д���??buffer ����ʱ����дָ��??
   * �������ֱ�� drop��APP �ͱ���Ϊ��д������� SOC ������ȥ��Ϣ��??
   * ��Ϊ��buffer ���� -> ���� ATT_EVENT_CAN_SEND_NOW -> ���¼�������??
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

  /* ??buffer �����µ�ʧ�ܣ���??CCC δ��/�Ѷ������������ԣ����⿨??pending */
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
      /* app_core ������������������һ��ʱ��󣩣�Կ���б�/ͨ�� f7f1 ���뱣֤�ذ�??
       * ���ײ��ԣ�ֱ���ڵ�ǰ������ִ�м�??notify �ķ������̣�notify ���� btstack ���е��ȣ�??
       */
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
  uint8_t set_code_buffer[64] = {0x00, 0x00, 0x01};  // Ĭ�ϣ������� + ָ��??+ �ɹ���־
  set_code_buffer[0] = payload[0];
  set_code_buffer[1] = payload[1];
  // ע�⣺������??uart_data����??MCU �ظ����첽��
  // ���ڳ�������ָ�����Ͱ�����ҳ����ȣ���ֱ�ӷ��سɹ�
  // set_code_buffer[2] ����??0x01���ɹ���
  fill_send_protocl(protocol_id, set_code_buffer, 3);
  /* ע�⣺�˺���������ջ�ϵ��ӽ϶�ֲ����������״���ջ������ؼ��󻺳��??static ����ջռ??*/
  static uint8_t send_code[269];
  memset(send_code, 0, sizeof(send_code));
  uint16_t send_code_len =
      convert_protocol_to_buffer(&send_protocl, send_code, 269);

  // �����ݽ��м�??
  static AES_KEY aes_key;

  u16 encrypt_len = sizeof(encrypt_data);
  {
    u8 keybuf[16];
    const u8 *key = fill_protocol_get_aes_key(keybuf);
    aes_set_encrypt_key(&aes_key, key, AES128_KEY_SIZE);
  }
  // ��������
  aes_encrypt_pkcs(&aes_key, send_code, send_code_len, encrypt_data,
                   &encrypt_len);
#if FILL_PROTOCOL_SEND_DBG
  printf("encrypt_data���ܺ������:");
  log_info_hexdump(encrypt_data, encrypt_len);
  printf("encrypt_len: %d, send_data_len: %d\n", encrypt_len, send_code_len);
#endif

  // ��֤�������ݳ���
  if (encrypt_len > sizeof(send_code)) {
    log_info("�������ݳ��� %zu ������������??%zu", encrypt_len,
             sizeof(send_code));
    // ʹ��ԭʼ���ݷ�??
    encrypt_len = send_code_len;
  } else {
    memcpy(send_code, encrypt_data, encrypt_len);
  }

#if FILL_PROTOCOL_SEND_DBG
  printf("���շ�����??");
  log_info_hexdump(send_code, encrypt_len);
#endif

  ble_notify_post(
      ATT_CHARACTERISTIC_0000f7f1_0000_1000_8000_00805f9b34fb_01_VALUE_HANDLE,
      send_code, encrypt_len, ATT_OP_NOTIFY, instruct);
  // ���������ͽ�����״̬֮��Ȼ����ִ�з��ͳ���״̬��protocol_id ����
  if (payload_len >= 2 && payload[1] == 9) {
    vehicle_control_timer_handler(); // ����0x0012 ����APP�˸��³�����״??
    printf("����״̬_set_sucessfuly\n");
  }
  if (payload_len >= 2 && payload[1] == 8) {
    vehicle_control_timer_handler(); // ����0x0012 ����APP�˸��³�����״??
    printf("����״̬_set_sucessfuly\n");
  }
}

// ================================����Կ���б����========================================================================
/* ��������¼���һ??00FB Կ���б�����ʱ�䣬���� 1 �����ظ���??*/
static u32 g_last_send_key_list_time = 0;
void send_ble_key_list(uint16_t protocol_id) {
  // 00FBЭ������������ʱ�����ظ���??
  if (protocol_id == 0x00FB) {
    u32 current_time = jiffies;
    if (current_time - g_last_send_key_list_time < 100) { // 1���ڲ��ظ���??
      log_info("[BLE] 00FBЭ�鷢�͹���Ƶ��������\n");
      return;
    }
    g_last_send_key_list_time = current_time;
  }

  // uint8_t id_addr[6] = {0};save_MAC_address
  // ble_list_get_last_id_addr(id_addr);
  // printf("��ȡ������豸MAC��ַ: ");
  // printf_buf(id_addr, sizeof(id_addr));
  // ���MAC��ַ�Ƿ��ظ�ss
  uint8_t ret;
  // ble_proto_ble_pair_req_Proc(NULL);
  static uint8_t key_list_buffer[269];
  memset(key_list_buffer, 0, sizeof(key_list_buffer));
  uint16_t offset = 0;

  // ��ȡ����Կ�׵Ŀ�����??
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

  /* ��ȡ�ֻ�����Կ�� (����?? 0x0013)
   * ע�⣺��Ҫ��ȫ��??CFG_PHONE_BLE_KEY_USABLE_NUM�����ܲ�ͬ������ֱ�ӱ��� 3 ����λ??
   */
  {
    uint32_t config_ids[] = {
        CFG_PHONE_BLE_KEY_DELETE_ID_1,
        CFG_PHONE_BLE_KEY_DELETE_ID_2,
        CFG_PHONE_BLE_KEY_DELETE_ID_3,
    };
    const uint16_t slot_cnt = sizeof(config_ids) / sizeof(config_ids[0]);
    uint8_t phone_ble_key_data[9] = {0}; // 3�ֽ����??+ 6�ֽ�MAC��ַ

    for (uint16_t i = 0; i < slot_cnt; i++) {
      memset(phone_ble_key_data, 0, sizeof(phone_ble_key_data));
      ret = syscfg_read(config_ids[i], phone_ble_key_data, sizeof(phone_ble_key_data));
      if (ret > 0 && !is_all_zero(phone_ble_key_data, sizeof(phone_ble_key_data))) {
        if (offset + 2 + 3 > sizeof(key_list_buffer)) {
          goto key_list_send;
        }
        /* �����밴Э���ĵ���UINT16 ��� */
        fill_put_u16_be(key_list_buffer + offset, APP_FUNC_CODE_KEY_BLE_HID);
        offset += 2;
        memcpy(key_list_buffer + offset, phone_ble_key_data, 3);
        offset += 3;
        printf("�ֻ�����Կ�� %d: %02X%02X%02X\n", (int)(i + 1), phone_ble_key_data[0],
               phone_ble_key_data[1], phone_ble_key_data[2]);
      }
    }
  }

  // ��ȡ��������Կ�� (����?? 0x0014)
  if (ble_key_peripheral_usable_num > 0) {
    // ��ȡ��һ����������Կ??
    if (ble_key_peripheral_usable_num >= 1) {
      uint8_t ble_peripheral_data[28] = {0}; // ÿ�ζ�ȡǰ���³�ʼ��
      ret = syscfg_read(CFG_BLE_KEY_PERIPHERAL_SEND_1, ble_peripheral_data, 28);
      if (ret > 0 && !is_all_zero(ble_peripheral_data, 6)) {
        // ���ӹ���??(���??
        if (offset + 2 > sizeof(key_list_buffer)) {
          goto key_list_send;
        }
        fill_put_u16_be(key_list_buffer + offset, APP_FUNC_CODE_KEY_EXT_BLE_HID);
        offset += 2;

        // ����MAC��ַ (6�ֽ�)
        memcpy(key_list_buffer + offset, ble_peripheral_data, 6);
        offset += 6;

        // �����������Ƴ��� (�ӵ�7�ֽڿ�ʼ������)
        uint8_t name_length = 0;
        for (int i = 6; i < 28; i++) {
          if (ble_peripheral_data[i] != 0) {
            name_length++;
          } else {
            break;
          }
        }

        // �����������Ƴ��� (1�ֽ�)
        key_list_buffer[offset++] = name_length;

        // ������������ (GBK����)
        if (name_length > 0) {
          memcpy(key_list_buffer + offset, ble_peripheral_data + 6,
                 name_length);
          offset += name_length;
        }

        printf(
            "��������Կ�� 1: MAC=%02X:%02X:%02X:%02X:%02X:%02X, ���Ƴ���=%d\n",
            ble_peripheral_data[0], ble_peripheral_data[1],
            ble_peripheral_data[2], ble_peripheral_data[3],
            ble_peripheral_data[4], ble_peripheral_data[5], name_length);
      }
    }

    // ��ȡ�ڶ�����������Կ??
    if (ble_key_peripheral_usable_num >= 2) {
      uint8_t ble_peripheral_data[28] = {0}; // ÿ�ζ�ȡǰ���³�ʼ��
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

        printf("��������Կ�� 2\n");
      }
    }

    // ��ȡ��������������Կ??
    if (ble_key_peripheral_usable_num >= 3) {
      uint8_t ble_peripheral_data[28] = {0}; // ÿ�ζ�ȡǰ���³�ʼ��
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

        printf("��������Կ�� 3\n");
      }
    }

    // ��ȡ���ĸ���������Կ??
    if (ble_key_peripheral_usable_num >= 4) {
      uint8_t ble_peripheral_data[28] = {0}; // ÿ�ζ�ȡǰ���³�ʼ��
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

        printf("��������Կ�� 4\n");
      }
    }
  }

  // ��ȡNFCԿ�� (����?? 0x0015)
  if (nfc_key_peripheral_usable_num > 0) {
    uint8_t nfc_key_data[16] = {0}; // 16�ֽ�UID

    // ��ȡ��һ��NFCԿ��
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
        printf("NFCԿ�� 1\n");
      }
    }
    // ��ȡ�ڶ���NFCԿ��
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
        printf("NFCԿ�� 2\n");
      }
    }

    // ��ȡ������NFCԿ��
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
        printf("NFCԿ�� 3\n");
      }
    }

    // ��ȡ���ĸ�NFCԿ��
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
        printf("NFCԿ�� 4\n");
      }
    }
  }

  // ��ȡ����Կ�� (����?? 0x0016)
  {
    uint8_t password_data[4] = {0};
    ret = syscfg_read(CFG_VEHICLE_PASSWORD_UNLOCK, password_data, 4);
    if (ret > 0 && !is_all_zero(password_data, 4)) {
      if (offset + 2 + 4 > sizeof(key_list_buffer)) {
        goto key_list_send;
      }
      fill_put_u16_be(key_list_buffer + offset, APP_FUNC_CODE_KEY_PASSWD);
      offset += 2;
      // ��Э���ĵ���UINT8[4] ֱ��͸������ ASCII??
      memcpy(key_list_buffer + offset, password_data, 4);
      offset += 4;
      printf("����Կ��: %u%u%u%u\n", (unsigned)password_data[0], (unsigned)password_data[1],
             (unsigned)password_data[2], (unsigned)password_data[3]);
    }
  }

  printf("Total key list data length: %d bytes\n", offset);

key_list_send:

  // ���û��Կ�����ݣ�����һ���յ��ֻ�����Կ����ΪĬ??����Ͳ����� �����׶�no
  if (offset == 0) {

    printf("Added default phone ble key\n");
  }
  /* ͬ��??MCU����Э����Ҫ�� */
  uart1_send_toMCU(protocol_id, key_list_buffer, offset);

  /* �ظ� APP��ͨ�� f7f1 notify �·�??APP */
  ble_reply_to_app_f7f1_post(protocol_id, key_list_buffer, offset);
}
//=======================����send_vehicle_password_unlock ����Կ��

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
  // ����??�������??000 ��ɾ��Կ??
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
  // ���ͳɹ��󣬷��سɹ�ָ??
  uint8_t unlock_key_buffer[] = {0x01, 0x00, 0x42, 0x01, 0x00};
  if (ret == content_length) {
    unlock_key_buffer[0] = 0x01;
  } else {
    unlock_key_buffer[0] = 0x00;
  }
  // �����������
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
  // �����ݽ��м�??
  AES_KEY aes_key;

  u16 encrypt_len = sizeof(encrypt_data);
  aes_set_encrypt_key(&aes_key, test_aes_key1, AES128_KEY_SIZE);
  // ��������
  aes_encrypt_pkcs(&aes_key, send_key_code, send_data_len, encrypt_data,
                   &encrypt_len);
  printf("encrypt_data���ܺ������:");
  log_info_hexdump(encrypt_data, encrypt_len);
  printf("encrypt_len: %d, send_data_len: %d\n", encrypt_len, send_data_len);

  // ��֤�������ݳ���
  if (encrypt_len > sizeof(send_key_code)) {
    log_info("�������ݳ��� %u ������������??%zu", encrypt_len,
             sizeof(send_key_code));
    encrypt_len = send_data_len;
  } else {
    memcpy(send_key_code, encrypt_data, encrypt_len);
  }
  printf("���շ�����??");

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
//===================================app�·���������Կ��ָ��0x0039===================================
typedef struct {
  uint16_t funcode;
  uint8_t MAC_addr[6];
  uint8_t ble_name_len;
  uint8_t ble_name_GBK[21];
} Ble_key_peripheral;
uint8_t mac_address_exists_gbk[] = {
    0x00, 0x4D, 0x41, 0x43, 0xB5, 0xD8, 0xD6, 0xB7, 0xD2,
    0xD1, 0xBE, 0xAD, 0xB4, 0xE6, 0xD4, 0xDA}; // MAC��ַ�Ѿ����ڵ�GBK����+0x00
                                               // ����ʧ��
uint8_t
    no_peripheral_key_space_gbk[] =
        {
            0x00, 0xC3, 0xBB, 0xD3, 0xD0, 0xCD, 0xE2, 0xC9,
            0xE8, 0xC0, 0xB6, 0xD1, 0xC0, 0xD4, 0xC4, 0xCA,
            0xD7, 0xBF, 0xD5, 0xBC, 0xBC, 0xE4}; // û����������Կ�׿ռ�
Ble_key_peripheral ble_key_peripheral;
/**
 * @brief ����app�·���������Կ��ָ��0x0039
 *
 * @param protocol_id {Э��id}
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
  printf("���յ���������Կ�׵�MAC��ַ:%02X:%02X:%02X:%02X:%02X:%02X\n",
         MAC_addr[0], MAC_addr[1], MAC_addr[2], MAC_addr[3], MAC_addr[4],
         MAC_addr[5]);
  uint8_t peripheral_key_num = 0;

  // ��ȡnumber��������
  uint8_t send_buffer_choice; // �����жϷ���ʲ??
  ret = syscfg_read(CFG_BLE_KEY_PERIPHERAL_USABLE, &peripheral_key_num,
                    sizeof(peripheral_key_num));

  // ���Ӵ�����ͷ�Χ��֤
  if (ret <= 0) {
    printf("��ȡCFG_BLE_KEY_PERIPHERAL_USABLEʧ�ܣ�ʹ��Ĭ��??\n");
    peripheral_key_num = 0;
  }

  if (peripheral_key_num > 4) {
    printf("��⵽��Ч��key_num?? %d������Ϊ0\n", peripheral_key_num);
    peripheral_key_num = 0;
  }

  printf("��ǰ�����õ���������Կ�ױ���ռ�:%d\n", peripheral_key_num);
  uint8_t Uable_num = 4 - (peripheral_key_num); // �����õ�Uable_num
  if (Uable_num > 0) {
    printf("��⵽�����õ���������Կ�ױ���ռ�");
    printf("Uable_num:%d\n", Uable_num);
    ret = judge_periheral_flash_macadd(MAC_addr);
    if (ret) {
      printf("MAC��ַ�Ѿ�����");
      // MAC��ַ�Ѿ����ڵ�GBK����
      uint8_t mac_address_exists_gbk[] = {0x4D, 0x41, 0x43, 0xB5, 0xD8,
                                          0xD6, 0xB7, 0xD2, 0xD1, 0xBE,
                                          0xAD, 0xB4, 0xE6, 0xD4, 0xDA};
      send_buffer_choice = 1; // ��ʾMAC��ַ�Ѵ�??
    } else {
      printf("MAC address not found\n");

      // ���ݵ�ǰ��key_numֵ�������浽�ĸ�λ�� -
      // �Ƴ�case�ڵ�(*peripheral_key_num)++
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
        printf("��Ч��key_num?? %d", peripheral_key_num);
        ret = -1;
        break;
      }

      if (ret > 0) {
        // ֻ�ڱ���ɹ�������һ??
        peripheral_key_num++;
        syscfg_write(CFG_BLE_KEY_PERIPHERAL_USABLE, &peripheral_key_num,
                     sizeof(peripheral_key_num));
        printf("����ɹ�����ǰ�ѱ�������: %d", peripheral_key_num);
        send_buffer_choice = 0;
      } else {
        printf("����ʧ��");
        send_buffer_choice = 3;
      }
    }
  } else {
    printf("û����������Կ�׿ռ�");
    // û����������Կ�׿ռ��GBK����
    uint8_t no_peripheral_key_space_gbk[] = {
        0xC3, 0xBB, 0xD3, 0xD0, 0xCD, 0xE2, 0xC9, 0xE8, 0xC0, 0xB6, 0xD1,
        0xC0, 0xD4, 0xC4, 0xCA, 0xD7, 0xBF, 0xD5, 0xBC, 0xBC, 0xE4};
    send_buffer_choice = 2; // ��ʾû�пռ�
  }
  // free(peripheral_key_num); // �ͷ��ڴ�
  // ��䷢��������߼�
  uint8_t send_buffer[269] = {0};
  uint16_t send_buffer_len = 0;

  // ����send_buffer_choice��䲻ͬ����??
  switch (send_buffer_choice) {
  case 0:                  // ����ɹ�
    send_buffer[0] = 0x01; // �ɹ�״̬��
    send_buffer_len = 1;
    printf("send save-success response\n");
    break;

  case 1:                  // MAC��ַ�Ѵ�??
    send_buffer[0] = 0x00; // MAC��ַ�Ѵ���״̬��
    // ���MAC��ַ�Ѵ��ڵ�GBK������Ϣ
    uint8_t mac_address_exists_gbk[] = {0x4D, 0x41, 0x43, 0xB5, 0xD8,
                                        0xD6, 0xB7, 0xD2, 0xD1, 0xBE,
                                        0xAD, 0xB4, 0xE6, 0xD4, 0xDA};
    memcpy(send_buffer + 1, mac_address_exists_gbk,
           sizeof(mac_address_exists_gbk));
    send_buffer_len = 1 + sizeof(mac_address_exists_gbk);
    printf("send mac-exists response\n");
    break;

  case 2:                  // Կ�׿ռ�����
    send_buffer[0] = 0x00; // û�пռ�״̬��
    // ���Կ�׿ռ�������GBK������Ϣ
    uint8_t key_space_full_gbk[] = {
        0xD4, 0xBF, 0xB3, 0xD7, 0xBF, 0xD5, 0xBC, 0xE4, 0xD2, 0xD1, 0xC2, 0xFA};
    memcpy(send_buffer + 1, key_space_full_gbk,
           sizeof(key_space_full_gbk));
    send_buffer_len = 1 + sizeof(key_space_full_gbk);
    printf("send no-space response\n");
    break;

  case 3:                  // ����ʧ��
    send_buffer[0] = 0x00; // ����ʧ��״̬��
    send_buffer_len = 1;
    printf("send save-failed response\n");
    break;

  default:
    send_buffer[0] = 0x00; // δ֪����״̬��
    send_buffer_len = 1;
    printf("send unknown-error response\n");
    break;
  }

  // ���Э�鲢��??
  fill_send_protocl(protocol_id, send_buffer, send_buffer_len);
  uint8_t send_key_code[269] = {0};
  memcpy(send_key_code, send_buffer, send_buffer_len);
  u16 send_data_len =
      convert_protocol_to_buffer(&send_protocl, send_key_code, 269);

  // �����ݽ��м�??
  AES_KEY aes_key;
  u16 encrypt_len = sizeof(encrypt_data);
  aes_set_encrypt_key(&aes_key, test_aes_key1, AES128_KEY_SIZE);
  aes_encrypt_pkcs(&aes_key, send_key_code, send_data_len, encrypt_data,
                   &encrypt_len);

  printf("���ܺ������:");
  log_info_hexdump(encrypt_data, encrypt_len);
  printf("���ܳ���: %d, ԭʼ����: %d", encrypt_len, send_data_len);

  // ��֤�������ݳ���
  if (encrypt_len > sizeof(send_key_code)) {
    log_info("�������ݳ��� %zu ������������??%zu", encrypt_len,
             sizeof(send_key_code));
    encrypt_len = send_data_len;
    // send_key_code ����ԭʼ����
  } else {
    memcpy(send_key_code, encrypt_data, encrypt_len);
  }

  // ������??
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
//===================================ɾ����������Կ��=============================================
void delete_ble_key_instruct(uint16_t protocol_id) {
  printf("delete_ble_key_instruct\n");
  if (!fill_require_content(protocol_id, 6)) {
    return;
  }
  uint8_t delet_ble_key_peripheral_buffer[] = {0x00};
  // ��ȡ��Ҫɾ����MAC��ַ
  memcpy(ble_key_peripheral.MAC_addr, content_data, 6);
  // ȥread �����ֵ�Ƕ���
  uint8_t current_key_num[1] = {0};
  syscfg_read(CFG_BLE_KEY_PERIPHERAL_USABLE, (void *)&current_key_num,
              sizeof(current_key_num));
  uint8_t delet_buffer[28] = {0};
  uint8_t compare_data = current_key_num[0];
  // �ж����current_key_num ����0 �ſ���ɾ??
  if (current_key_num[0] > 0) {

    uint8_t current_MAC_adders[6] = {0};
    // ��䵱ǰ��MAC��ַ
    syscfg_read(CFG_BLE_KEY_PERIPHERAL_SEND_1, (void *)current_MAC_adders,
                sizeof(current_MAC_adders));
    // �ж������ǰ��MAC��ַ����Ҫɾ����MAC��ַ��ͬ�������ɾ��ָ��
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
    // �ж������ǰ��MAC��ַ����Ҫɾ����MAC��ַ��ͬ�������ɾ��ָ��
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
    // �ж������ǰ��MAC��ַ����Ҫɾ����MAC��ַ��ͬ�������ɾ��ָ��
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
    // �ж������ǰ��MAC��ַ����Ҫɾ����MAC��ַ��ͬ�������ɾ��ָ��
    if (memcmp(current_MAC_adders, ble_key_peripheral.MAC_addr,
               sizeof(current_MAC_adders)) == 0) {
      delet_ble_key_peripheral_buffer[0] = 0x01;
      syscfg_write(CFG_BLE_KEY_PERIPHERAL_SEND_4, delet_buffer,
                   sizeof(delet_buffer));
      printf("delete peripheral key slot4\n");
      current_key_num[0]--;
    }
    // ����current_key_num ����??���current_key_num ������compare_data
    // ����Ҫ��??
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

  // �����ݽ��м�??
  AES_KEY aes_key;

  u16 encrypt_len = sizeof(encrypt_data);
  aes_set_encrypt_key(&aes_key, test_aes_key1, AES128_KEY_SIZE);
  // ��������
  aes_encrypt_pkcs(&aes_key, send_key_code, send_data_len, encrypt_data,
                   &encrypt_len);
  printf("encrypt_data���ܺ������:");
  log_info_hexdump(encrypt_data, encrypt_len);
  printf("encrypt_len: %d, send_data_len: %d\n", encrypt_len, send_data_len);

  // ��֤�������ݳ���
  if (encrypt_len > sizeof(send_key_code)) {
    log_info("�������ݳ��� %zu ������������??%zu", encrypt_len,
             sizeof(send_key_code));
    // ʹ��ԭʼ���ݳ��ȷ�??
    encrypt_len = send_data_len;
    // send_key_code ����ԭʼ����
  } else {
    memcpy(send_key_code, encrypt_data, encrypt_len);
  }
  printf("���շ�����??");

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
//-------------------------------------�����������Կ��===========================================
void empty_ble_key_instruct(uint16_t protocol_id) {
  printf("empty_ble_key_instruct\n");
  uint8_t state_flag =
      0x00; // ���ñ�־λ�����0x00�������ʧ�ܣ���??x01������ճɹ�
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
  // ��������
  aes_encrypt_pkcs(&aes_key, send_key_code, send_data_len, encrypt_data,
                   &encrypt_len);
  printf("encrypt_data���ܺ������:");
  log_info_hexdump(encrypt_data, encrypt_len);
  printf("encrypt_len: %d, send_data_len: %d\n", encrypt_len, send_data_len);

  // ��֤�������ݳ���
  if (encrypt_len > sizeof(send_key_code)) {
    log_info("�������ݳ��� %zu ������������??%zu", encrypt_len,
             sizeof(send_key_code));
    // ʹ��ԭʼ���ݳ��ȷ�??
    encrypt_len = send_data_len;
    // send_key_code ����ԭʼ����
  } else {
    memcpy(send_key_code, encrypt_data, encrypt_len);
  }
  printf("���շ�����??");

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
//======================================����NFCԿ��===========================================
void add_NFC_instruct(uint16_t protocol_id) {
  uart1_send_toMCU(protocol_id, NULL, 0);
  // ���ж�NFC_num��ֵ�Ƕ���
  uint8_t current_NFC_num[1] = {0};
  syscfg_read(CFG_NFC_KEY_PERIPHERAL_USABLE_NUM, (void *)&current_NFC_num,
              sizeof(current_NFC_num));
  uint8_t add_buffer[16] = {0};
  if (uart_data != 0) {
    memcpy(add_buffer, uart_data, data_length);
    // Ȼ�����uart_data
    // memset(uart_data, 0, data_length);
  } else {
    printf("uart_data is NULL\n");
  }
  uint8_t compare_data = current_NFC_num[0];
  // ����NFCԿ��
  if (compare_data < 3) {
    uint8_t current_NFC_adders[16] = {0};
    // �ж�NFC��flash�����Ƿ��ǿ�??
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
//======================================ɾ��NFCԿ��==============================================
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
  // ��ȡ��Ҫɾ����NFC UID
  memcpy(nfc_peripheral.NFC_UID, content_data, content_length);
  // ȥread �����ֵ�Ƕ���
  uint8_t current_NFC_num[1] = {0};
  syscfg_read(CFG_NFC_KEY_PERIPHERAL_USABLE_NUM, (void *)&current_NFC_num,
              sizeof(current_NFC_num));
  uint8_t delet_buffer[16] = {0};
  uint8_t compare_data = current_NFC_num[0];
  // �ж����current_NFC_num ����0 �ſ���ɾ??
  if (current_NFC_num[0] > 0) {

    uint8_t current_NFC_adders[16] = {0};
    // ��䵱ǰ��NFC��ַ
    syscfg_read(CFG_NFC_KEY_PERIPHERAL_SEND_1, (void *)current_NFC_adders,
                sizeof(current_NFC_adders));
    // �ж������ǰ��NFC��ַ����Ҫɾ����NFC��ַ��ͬ�������ɾ��ָ��
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
    // �ж������ǰ��NFC��ַ����Ҫɾ����NFC��ַ��ͬ�������ɾ��ָ��
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
    // �ж������ǰ��NFC��ַ����Ҫɾ����NFC��ַ��ͬ�������ɾ��ָ��
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
    // �ж������ǰ��NFC��ַ����Ҫɾ����NFC��ַ��ͬ�������ɾ��ָ��
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
  // �����ݽ��м�??
  AES_KEY aes_key;

  u16 encrypt_len = sizeof(encrypt_data);
  aes_set_encrypt_key(&aes_key, test_aes_key1, AES128_KEY_SIZE);
  // ��������
  aes_encrypt_pkcs(&aes_key, send_key_code, send_data_len, encrypt_data,
                   &encrypt_len);
  printf("encrypt_data���ܺ������:");
  log_info_hexdump(encrypt_data, encrypt_len);
  printf("encrypt_len: %d, send_data_len: %d\n", encrypt_len, send_data_len);

  // ��֤�������ݳ���
  if (encrypt_len > sizeof(encrypt_data)) {
    log_info("�������ݳ��� %zu ������������??%zu", encrypt_len,
             sizeof(encrypt_data));
    // ʹ��ԭʼ���ݳ��ȷ�??
    encrypt_len = send_data_len;
    // send_key_code ����ԭʼ����
  } else {
    memcpy(send_key_code, encrypt_data, encrypt_len);
  }
  printf("���շ�����??");
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
//======================================���NFCԿ��==============================================
void empty_NFC_instruct(uint16_t protocol_id) {
  printf("empty_ble_key_instruct\n");
  uint8_t state_flag =
      0x00; // ���ñ�־λ�����0x00�������ʧ�ܣ���??x01������ճɹ�
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
  // �����ݽ��м�??
  AES_KEY aes_key;

  u16 encrypt_len = sizeof(encrypt_data);
  aes_set_encrypt_key(&aes_key, test_aes_key1, AES128_KEY_SIZE);
  // ��������
  aes_encrypt_pkcs(&aes_key, send_key_code, send_data_len, encrypt_data,
                   &encrypt_len);
  printf("encrypt_data���ܺ������:");
  log_info_hexdump(encrypt_data, encrypt_len);
  printf("encrypt_len: %d, send_data_len: %d\n", encrypt_len, send_data_len);

  // ��֤�������ݳ���
  if (encrypt_len > sizeof(encrypt_data)) {
    log_info("�������ݳ��� %zu ������������??%zu", encrypt_len,
             sizeof(encrypt_data));
    // ʹ��ԭʼ���ݳ��ȷ�??
    encrypt_len = send_data_len;
    // send_key_code ����ԭʼ����
  } else {
    memcpy(send_key_code, encrypt_data, encrypt_len);
  }
  printf("���շ�����??");
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
//============================================ɾ���ֻ�����Կ��=========================================
void delet_phone_bleKey_instrcut(uint16_t protocol_id) {
  // ����������ID
  uint32_t config_ids[] = {
      CFG_PHONE_BLE_KEY_DELETE_ID_1,
      CFG_PHONE_BLE_KEY_DELETE_ID_2,
      CFG_PHONE_BLE_KEY_DELETE_ID_3,
  };
  int config_count = sizeof(config_ids) / sizeof(config_ids[0]);

  // �洢��ȡ����ID��ַ��ǰ3�ֽ�Ϊ����룬��6�ֽ�ΪMAC��ַ??
  u8 id_addr[9] = {0};
  u8 mac_addr[6] = {0};

  // �յ�������루ǰ3���ֽڣ�
  uint8_t received_pair_code[3] = {0};
  received_pair_code[0] = content_data[0];
  received_pair_code[1] = content_data[1];
  received_pair_code[2] = content_data[2];

  uint8_t send_code[1] = {0x00}; // Ĭ��ʧ��
  int found_match = 0;
  int delete_success = 0;

  // ������������ID������ƥ������??
  for (int i = 0; i < config_count; i++) {
    // ��ȡ�����е��ֻ�����Կ����Ϣ
    memset(id_addr, 0, sizeof(id_addr));
    syscfg_read(config_ids[i], (void *)id_addr, sizeof(id_addr));

    // ��������Ƿ�Ϊ�գ�??��ʾû�б�������??
    int is_empty = 1;
    for (int j = 0; j < sizeof(id_addr); j++) {
      if (id_addr[j] != 0) {
        is_empty = 0;
        break;
      }
    }

    if (is_empty) {
      printf("����ID_%dΪ�գ�����\n", i + 1);
      continue;
    }

    // �Ƚ�������Ƿ�һ??
    if (memcmp(received_pair_code, id_addr, sizeof(received_pair_code)) == 0) {
      found_match = 1;
      printf("������ID_%d���ҵ�ƥ��������\n", i + 1);

      // �����һ�£���ȡMAC��ַ
      for (int k = 0; k < 6; k++) {
        mac_addr[k] = id_addr[k + 3];
        printf("%02X ", mac_addr[k]);
      }
      printf("\n");

      // ��������Ա���ɾ�����MAC��ַ
      int ret = ble_list_delete_device(mac_addr, 0); // �����ַ����??
      printf("ret��ֵΪ%d\n", ret);
      if (ret == 1) {
        delete_success = 1;
        send_code[0] = 0x01; // ɾ���ɹ�
        printf("ɾ���ֻ�����Կ�׳ɹ�\n");

        // ��������еļ�¼
        memset(id_addr, 0, sizeof(id_addr));
        syscfg_write(config_ids[i], (void *)id_addr, sizeof(id_addr));
        printf("���������ID_%d�еļ�¼\n", i + 1);

        // ���¿���������ȷ�����������??
        uint8_t phone_usable_num = 0;
        syscfg_read(CFG_PHONE_BLE_KEY_USABLE_NUM, &phone_usable_num,
                    sizeof(phone_usable_num));
        if (phone_usable_num > 0) {
          phone_usable_num--;
          syscfg_write(CFG_PHONE_BLE_KEY_USABLE_NUM, &phone_usable_num,
                       sizeof(phone_usable_num));
          printf("��ǰ���õ��ֻ�����Կ������Ϊ%d\n", phone_usable_num);
        } else {
          printf("����������Ϊ0���������\n");
        }
      } else {
        send_code[0] = 0x00; // ɾ��ʧ��
        printf("ɾ���ֻ�����Կ��ʧ��\n");
      }
      break; // �ҵ�ƥ����˳�ѭ??
    } else {
      printf("����ID_%d�е�����벻ƥ��\n", i + 1);
    }
  }

  if (!found_match) {
    printf("�����������ж�û���ҵ�ƥ��������\n");
    send_code[0] = 0x00; // δ�ҵ�ƥ??
  }

  // ���ͽ�??
  printf("send_code��ֵΪ%02X\n", send_code[0]);
  // ע�⣺��Ҫǿ�Ƹ�??send_code������ʵ��ɾ����??
  fill_send_protocl(protocol_id, send_code, sizeof(send_code));

  u16 send_data_len = convert_protocol_to_buffer(&send_protocl, send_code, 269);
  printf("send_data_len��ֵΪ%d\n", send_data_len);
  uint8_t send_buffer[] = {0xFE, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
                           0x43, 0x01, 0xC2, 0x2E, 0x0D, 0x0A

  };
  printf("�ȴ����ܵ���??");
  log_info_hexdump(send_code, send_data_len);
  AES_KEY aes_key;

  u16 encrypt_len = sizeof(encrypt_data);
  aes_set_encrypt_key(&aes_key, test_aes_key1, AES128_KEY_SIZE);
  // ��������
  aes_encrypt_pkcs(&aes_key, send_buffer, sizeof(send_buffer), encrypt_data,
                   &encrypt_len);
  printf("encrypt_data���ܺ������:");
  log_info_hexdump(encrypt_data, encrypt_len);
  printf("encrypt_len: %d, send_data_len: %d\n", encrypt_len, send_data_len);

  // ��֤�������ݳ���
  if (encrypt_len > sizeof(encrypt_data)) {
    log_info("�������ݳ��� %zu ������������??%zu", encrypt_len,
             sizeof(encrypt_data));

    encrypt_len = send_data_len;

  } else {
    memcpy(send_code, encrypt_data, encrypt_len);
  }
  printf("���շ�����??");

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
//*********************************************�����������*************************************/
//=======================================��ȡ����������Ϣ==============================================
/**
 * @brief ��ȡ����������Ϣָ��
 *
 */
void get_vehice_set_infromation_instruct(uint16_t protocol_id) {
  VEH_SET_TRACE("enter", protocol_id, 0);
  printf("Sending request for protocol_id: 0x%04x\n", protocol_id);
  printf("���복������\n");
  /* 0x0033 ��ȡ����������Ϣʱ�����ⱻ��??0x0012 ��ռ���� uart_data */
  u8 timer_was_running = vehicle_control_timer_is_running();
  if (timer_was_running) {
    VEH_SET_TRACE("timer_stop", 1, 0);
    vehicle_control_timer_stop();
  }

  VEH_SET_TRACE("uart_send", protocol_id, 0);
  uart1_send_toMCU(protocol_id, NULL, 0);

  static uint8_t uart_copy_buf[256];
  uint16_t uart_copy_len = 0;
  int wait_count = 150; // ~1.5s���� 10ms tick ����??
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
    vehicle_control_timer_start(11000);// ����0.5s
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
  // ��������
  aes_encrypt_pkcs(&aes_key, send_key_code, send_data_len, encrypt_data,
                   &encrypt_len);
  VEH_SET_TRACE("aes_done", encrypt_len, 0);
  printf("encrypt_data���ܺ������:");
  log_info_hexdump(encrypt_data, encrypt_len);
  printf("encrypt_len: %d, send_data_len: %d\n", encrypt_len, send_data_len);

  // ��֤�������ݳ���
  if (encrypt_len > sizeof(send_key_code)) {
    log_info("�������ݳ��� %zu ������������??%zu", encrypt_len,
             sizeof(send_key_code));
    // ʹ��ԭʼ���ݳ��ȷ�??
    encrypt_len = send_data_len;
    // send_key_code ����ԭʼ����
  } else {
    memcpy(send_key_code, encrypt_data, encrypt_len);
  }
  printf("���շ�����??");

  if (app_recieve_callback) {
    app_recieve_callback(0, send_key_code, encrypt_len);
  }

  VEH_SET_TRACE("ble_post", encrypt_len, 0);
  ble_notify_post(
      ATT_CHARACTERISTIC_0000f7f1_0000_1000_8000_00805f9b34fb_01_VALUE_HANDLE,
      send_key_code, encrypt_len, ATT_OP_NOTIFY, protocol_id);
  VEH_SET_TRACE("exit", protocol_id, 0);
}
// ==================================������Ч���ָ��===========================================
#if FILL_PROTOCOL_MUSIC_ENABLE

typedef struct indivi_tone {
  uint8_t tone_type;     // ��Ч���� 1�ֽ�
  uint32_t file_size;    // �ļ���С 4�ֽ�
  uint32_t file_crc32;   // crc32 4 �ֽ�
  uint64_t tone_id;      // ��ЧID  8�ֽ�
  uint8_t file_name[32]; // string���͵��ļ���??32�ֽ�
} indivi_tone;

/* �Զ�����Ч���ԣ��ڲ� FLASH USER reserved ����??
 * - ��Ƶ���ݣ��� tone_type д���Ӧ reserved ���ļ�ӳ��ڵ㣨����Ŀ¼/���ļ���
 *   - UWTG0 ���� isd_config.ini ??[RESERVED_CONFIG]��VFS ??mnt/sdfile/app/uwtg0
 *   - UWTG1~UWTG3 ���� isd_config.ini ??[RESERVED_EXPAND_CONFIG]��VFS ??mnt/sdfile/EXT_RESERVED/UWTG{1..3}
 * - Ԫ��Ϣ��д�� syscfg��CFG_CUSTOM_TONE_META_0..3�������� 0x0075 ��ѯ
 * - ��������??user_info_file.h���˴�ʹ�ù�����??
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

  /* ˵�������������ļ�����׺ѡ���ʽ??
   * �ֻ��·�??WAV ʱ������·����׺??"wav"����??mnt/sdfile/app/uwav������������ߴ�������??
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

file_config_t file_config; // ���ڱ��������ļ�
FILE *file_handle;
// ȫ�ֱ��������ļ�����״̬�������Ƴ� static �Ա��ⲿ��ѯ??
uint32_t file_write_offset = 0;       // ��ǰд��ƫ��??
uint32_t total_file_size = 0;         // �ļ��ܴ�??
uint8_t is_file_transfer_started = 0; // �ļ������Ƿ�??
static char current_file_path[96] = {0};     // ��ǰ������ļ�·??
static struct indivi_tone Indivi_tone;

static uint32_t g_music_rx_start_ms = 0;
static uint8_t g_music_rx_last_pct = 0xFF;
static uint8_t g_music_rx_head_probed = 0;
static uint32_t g_music_rx_nonzero_bytes = 0;
static uint32_t g_music_rx_total_bytes = 0;
static uint16_t g_music_rx_all_zero_pkts = 0;

/* 0x0075����ȡ�Զ�����Ч�б����� meta �ļ�����??
 * �����壺�ظ��ṹ LIST[]
 * - ��Ч���� UINT8
 * - ��ƵID  UINT64 (BE)
 * - �ļ�����??UINT8
 * - �ļ�??STRING(GBK) 1~32
 * 
 * ÿ�������ȷ�??Ĭ����Ч"��tone_id=0�����ٷ����û��ϴ��ĸ�����??
 */
static void get_vehice_music_infromation_instruct(uint16_t protocol_id)
{
  static uint8_t out[269];
  uint16_t offset = 0;
  memset(out, 0, sizeof(out));

  // Ĭ����Ч���ƣ�GBK���룬����ʹ��ʮ�����Ʊ�??UTF-8/GBK ��������??
  // "Ĭ�Ͽ�����?? GBK: C4 AC C8 CF BF AA BB FA D2 F4 D0 A7
  static const uint8_t default_tone_name_0[] = {0xC4, 0xAC, 0xC8, 0xCF, 0xBF, 0xAA, 0xBB, 0xFA, 0xD2, 0xF4, 0xD0, 0xA7};
  // "Ĭ�Ϲػ���Ч" GBK: C4 AC C8 CF B9 D8 BB FA D2 F4 D0 A7
  static const uint8_t default_tone_name_1[] = {0xC4, 0xAC, 0xC8, 0xCF, 0xB9, 0xD8, 0xBB, 0xFA, 0xD2, 0xF4, 0xD0, 0xA7};
  // "Ĭ�ϱ�����Ч" GBK: C4 AC C8 CF B1 A8 BE AF D2 F4 D0 A7
  static const uint8_t default_tone_name_2[] = {0xC4, 0xAC, 0xC8, 0xCF, 0xB1, 0xA8, 0xBE, 0xAF, 0xD2, 0xF4, 0xD0, 0xA7};
  // "Ĭ��Hello��Ч" GBK: C4 AC C8 CF 48 65 6C 6C 6F D2 F4 D0 A7
  static const uint8_t default_tone_name_3[] = {0xC4, 0xAC, 0xC8, 0xCF, 0x48, 0x65, 0x6C, 0x6C, 0x6F, 0xD2, 0xF4, 0xD0, 0xA7};
  
  static const uint8_t *default_tone_names[] = {
    default_tone_name_0,  // type 0: Ĭ�Ͽ�����??
    default_tone_name_1,  // type 1: Ĭ�Ϲػ���Ч
    default_tone_name_2,  // type 2: Ĭ�ϱ�����Ч
    default_tone_name_3,  // type 3: Ĭ��Hello��Ч
  };
  static const uint8_t default_tone_name_lens[] = {
    sizeof(default_tone_name_0),  // 12
    sizeof(default_tone_name_1),  // 12
    sizeof(default_tone_name_2),  // 12
    sizeof(default_tone_name_3),  // 13
  };

  // �����壺�ظ� LIST[]����??count����ÿ�������ȷ�Ĭ�ϣ��ٷŸ�����??
  for (uint8_t type = 0; type < CUSTOM_TONE_MAX_TYPES; type++) {
    // 1. ������Ĭ����Чѡ�tone_id = 0 ��ʾĬ��??
    const uint8_t *def_name = default_tone_names[type];
    uint8_t def_name_len = default_tone_name_lens[type];
    if (def_name_len > 32) def_name_len = 32;
    
    if (offset + 1 + 8 + 1 + def_name_len <= sizeof(out)) {
      out[offset++] = type;  // ��Ч����
      // tone_id = 0 ��ʾĬ����Ч??�ֽڴ��
      for (int i = 0; i < 8; i++) {
        out[offset++] = 0x00;
      }
      out[offset++] = def_name_len;
      memcpy(out + offset, def_name, def_name_len);
      offset += def_name_len;
    }

    // 2. �������û��ϴ��ĸ�����Ч���������??
    custom_tone_meta_t meta = {0};
    int cfg_id = custom_tone_meta_cfg_id(type);
    if (custom_tone_read_meta_by_id(cfg_id, &meta) != 0 || meta.file_size == 0) {
      log_info("[CUSTOM_TONE] type=%u no custom tone meta û�б�����Ƶ\n", type);
      continue;  // û�и�����Ч������
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
    // ���Ӹ�����Ч��??
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
  // ������Ӧ��app 
  tone_reply_ble_simple(protocol_id, out, offset);
}

// �����İ�װ������??le_trans_data.c ����
void get_vehice_music_infromation_instruct_ext(uint16_t protocol_id)
{
  get_vehice_music_infromation_instruct(protocol_id);
}

/**
 * @brief ������Чѡ��ָ�� (0x0076)
 * �������ݸ�ʽ��[tone_type 1�ֽ�][tone_id 8�ֽڴ��]
 * - tone_id = 0 ��ʾѡ��Ĭ����Ч
 * - tone_id != 0 ��ʾѡ�������??
 * ��Ӧ��[result 1�ֽ�] 0x01=�ɹ�, 0x00=ʧ��
 */
static void select_tone_instruct(uint16_t protocol_id)
{
  printf("=== ��Чѡ��ָ�� 0x0076 ===\n");
  
  // ������ݳ��ȣ�tone_type(1) + tone_id(8) = 9 �ֽ�
  if (content_length < 9) {
    printf("[SELECT_TONE] ���ݳ��Ȳ��㣬��??�ֽڣ���?? %d\n", content_length);
    uint8_t resp = 0x00;
    tone_reply_ble_simple(protocol_id, &resp, 1);
    return;
  }
  
  // ������Ч����
  uint8_t tone_type = content_data[0];
  if (tone_type >= CUSTOM_TONE_MAX_TYPES) {
    printf("[SELECT_TONE] ��Ч??tone_type=%u\n", tone_type);
    uint8_t resp = 0x00;
    tone_reply_ble_simple(protocol_id, &resp, 1);
    return;
  }
  
  // ���� tone_id����??8 �ֽ�??
  uint64_t tone_id = 
      ((uint64_t)content_data[1] << 56) | ((uint64_t)content_data[2] << 48) |
      ((uint64_t)content_data[3] << 40) | ((uint64_t)content_data[4] << 32) |
      ((uint64_t)content_data[5] << 24) | ((uint64_t)content_data[6] << 16) |
      ((uint64_t)content_data[7] << 8)  | content_data[8];
  
  printf("[SELECT_TONE] tone_type=%u, tone_id=%llu\n", tone_type, tone_id);
  
  // ȷ��ѡ�����ͣ�tone_id = 0 ��ʾĬ����Ч������Ϊ������??
  uint8_t select = (tone_id == 0) ? TONE_SELECT_DEFAULT : TONE_SELECT_CUSTOM;
  
  // �����û�ѡ��
  int ret = user_tone_select_save(tone_type, select);
  
  if (ret == 0) {
    printf("[SELECT_TONE] ����ɹ�: tone_type=%u, select=%s\n", 
           tone_type, select ? "custom" : "default");
    uint8_t resp = 0x01;
    tone_reply_ble_simple(protocol_id, &resp, 1);
  } else {
    printf("[SELECT_TONE] ����ʧ��\n");
    uint8_t resp = 0x00;
    tone_reply_ble_simple(protocol_id, &resp, 1);
  }
}

// �����İ�װ������??le_trans_data.c ����
void select_tone_instruct_ext(uint16_t protocol_id)
{
  select_tone_instruct(protocol_id);
}

/**
 * @brief ����������Ч���и�����Ч����ָ??
 *
 * @param protocol_id Э��ID 2206
 */
static void set_vehice_music_instruct(uint16_t protocol_id) {
  if (content_length < 17) {
    log_info(
        "set_vehice_music_instruct: ���ݳ��Ȳ��㣬��Ҫ��??7�ֽڣ���ǰ���ȣ�%d",
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
  // ���жϣ����id == 0x00000001�����ʾ��Ĭ����Ч��ֱ���л���Ĭ����Ч�������سɹ�
  if (Indivi_tone.tone_id == 0x00000001ULL) {
    memset(Indivi_tone.file_name, 0, sizeof(Indivi_tone.file_name));
    if (custom_tone_clear_slot(Indivi_tone.tone_type) != 0 ||
        user_tone_select_save(Indivi_tone.tone_type, TONE_SELECT_DEFAULT) != 0) {
      log_info("set_vehice_music_instruct: clear default tone failed, tone_type=%u",
               Indivi_tone.tone_type);
      uint8_t resp = 0x01; // ��ʾ���óɹ� 
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

  printf("=== ��Ч����ָ�������� ===\n");
  printf("��Ч���� (tone_type): %u\n", Indivi_tone.tone_type);
  printf("�ļ���С (file_size): %u �ֽ� (??%.2f MB)\n", Indivi_tone.file_size,
         (float)Indivi_tone.file_size / (1024 * 1024));
  printf("�ļ�У��??(file_crc32): 0x%08X\n", Indivi_tone.file_crc32);
  printf("��ƵID (tone_id): %llu\n", Indivi_tone.tone_id);
  printf("�ļ����� (file_name): ");
  for (int i = 0; i < file_name_length && Indivi_tone.file_name[i] != '\0'; i++) {
    printf("%c", Indivi_tone.file_name[i]);
  }
  printf("\n");
  printf("�ļ�����?? %d �ֽ�\n", file_name_length);
  printf("�ļ���ʮ����?? ");
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
    log_info("�ļ�����ʧ��: %s", slot_path);
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
  printf("�ļ������ɹ�: %s\n", slot_path);
  printf("�ļ���С: %u �ֽ� (%.2f MB)\n", total_file_size, total_file_size / (1024.0f * 1024.0f));
  printf("��Ч����: %u\n", Indivi_tone.tone_type);
  printf("��ЧID: %llu\n", Indivi_tone.tone_id);
  printf("����CRC32: 0x%08X\n", Indivi_tone.file_crc32);
  printf("�ļ�?? %s\n", Indivi_tone.file_name);
  printf("�ļ�����״̬�ѳ�ʼ�����ȴ�����д��...\n");
  printf("=======================================================\n\n");

  uint8_t resp = 0x01;
  tone_reply_ble_simple(protocol_id, &resp, 1);
}
// ��������ָ??
void open_lock_instruct(uint16_t protocol_id) {
  // ����ָ��
  uint8_t lock_status = content_data[0];
  printf("lock_status = %d\n", lock_status);
}
/**
 * @brief ���������ļ�����ָ??���ڱ��������ļ���flash/APP/ufile/Ŀ¼
 *
 * @param protocol_id Э��ID 2207
 */
static void music_file_send_instruct(uint16_t protocol_id) {
  // ����ļ������Ƿ��ѿ�??
  if (!is_file_transfer_started || !file_handle) {
    log_info("�ļ�����δ��ʼ�����ȵ���set_vehice_music_instruct�����ļ�");
    uint8_t resp = 0x00;
    tone_reply_ble_simple(protocol_id, &resp, 1);
    return;
  }

  // ������ݳ����Ƿ���??
  if (content_length < 1) {
    log_info("music_file_send: ���ݳ��Ȳ���");
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
  /* �����������ݴ�??*/
  if (file_write_offset + chunk_len > total_file_size) {
    u32 desired_size = file_write_offset + chunk_len;
    if (desired_size > CUSTOM_TONE_SLOT_MAX_SIZE) {
      log_info("���ݰ�������λ���ޣ���ǰƫ��: %u, ���ݳ���: %u, ������С: %u, ����: %u",
               file_write_offset, chunk_len, desired_size,
               (unsigned)CUSTOM_TONE_SLOT_MAX_SIZE);
      uint8_t resp = 0x00;
      tone_reply_ble_simple(protocol_id, &resp, 1);
      return;
    }
    /* ���ݣ��� APP ʵ�ʷ��ͳ��ȴ����������ȣ���̬��չ��ʵ�ʽ��ճ��� */
    log_info("music_file_send: expand file_size %u -> %u",
             total_file_size, desired_size);
    total_file_size = desired_size;
    Indivi_tone.file_size = desired_size;
    file_config.max_size = (int)desired_size;
  }

  // ÿ�δ��俪ʼʱ���ð�������
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
      printf("[MUSIC_RX] ====== ��ʼ������Ƶ��??======\n");
      printf("[MUSIC_RX] first chunk_len=%u, nonzero=%u, head16:", chunk_len, nz);
      for (uint16_t j = 0; j < 16 && j < chunk_len; j++) {
        printf(" %02X", content_data[j]);
      }
      printf("\n");
    }
    
    // ÿ��??50 ������ӡһ����ϸ��??
    pkt_count++;
    if (pkt_count % 50 == 0 || file_write_offset == 0) {
      printf("[MUSIC_RX] pkt#%u: offset=%u, chunk=%u, nz=%u (%.1f%%)\n",
             pkt_count, file_write_offset, chunk_len, nz,
             chunk_len ? (nz * 100.0f / chunk_len) : 0.0f);
    }
  }

  if (file_write_data(file_handle, content_data, chunk_len, file_write_offset) != 0) {
    log_info("�ļ�д��ʧ�ܣ�ƫ?? %u", file_write_offset);
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

    /* ���ݣ���ʵ�ʽ��ճ��ȳ����������ȣ���ʵ�ʳ�����β */
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
      
      // ������������??
      if (g_music_rx_nonzero_bytes < g_music_rx_total_bytes / 10) {
        printf("[MUSIC_RX] WARNING: �����ֽڱ������� (<10%%), ���ݿ�����!\n");
      }
      if (g_music_rx_all_zero_pkts > 10) {
        printf("[MUSIC_RX] WARNING: ����ȫ������??(%u), ��������쳣!\n", g_music_rx_all_zero_pkts);
      }
      printf("================================================================\n\n");
    }

    printf("[CUSTOM_TONE] transfer done, probe head, size=%u\n", total_file_size);
    custom_tone_probe_file_format(current_file_path);

    if (file_write_offset == total_file_size) {
      log_info("�ļ��������: %s, ��С: %u �ֽ�", current_file_path, file_write_offset);
    } else {
      log_info("�ļ����䲻��?? %s, ʵ��: %u �ֽ�, ����: %u �ֽ�",
               current_file_path, file_write_offset, total_file_size);
    }

    // CRC32 У��
    {
      printf("[CUSTOM_TONE] Verifying file integrity...\n");
      FILE *verify_fp = fopen(current_file_path, "rb");
      if (verify_fp) {
        // ��ȡ�ļ����� CRC32
        uint32_t calc_crc = 0;
        uint8_t buf[256];
        int total_read = 0;
        while (1) {
          int r = fread(verify_fp, buf, sizeof(buf));
          if (r <= 0) break;
          total_read += r;
          // �򵥵� CRC32 ���� ()
          for (int i = 0; i < r; i++) {
            calc_crc = ((calc_crc << 8) | buf[i]) ^ (calc_crc >> 24);
          }
        }
        fclose(verify_fp);
        
       printf("[CUSTOM_TONE] �ļ�����??d �ֽ�\n",total_read);
        printf("[CUSTOM_TONE] Ԥ�� CRC32: 0x%08X\n", Indivi_tone.file_crc32);
        printf("[CUSTOM_TONE] ����??CRC32: 0x%08X\n", calc_crc);
        
        if (total_read != (int)total_file_size) {
          printf("[CUSTOM_TONE] ERROR: �ļ���С��ƥ?? ʵ��=%d ����=%u\n", 
                 total_read, total_file_size);
        }
        
        // ע����� APP ��û�з�����ȷ�� CRC32��������ܲ�ƥ��
        // �����ٿ��Կ����������ֵ�����ڶԱ�
      } else {
        printf("[CUSTOM_TONE] ERROR: �޷����ļ�����У��: %s\n", current_file_path);
      }
    }
    
    // ���洫�䱨�浽�ļ���������ʱ�鿴
    {
      const char *report_path = "mnt/sdfile/app/uwav/transfer_report.txt";
      
      // ȷ�� uwav Ŀ¼����
      int mkdir_ret = fmk_dir("mnt/sdfile/app", "/uwav", 0);
      if (mkdir_ret != 0 && mkdir_ret != -1) {
        // -1 ͨ����ʾĿ¼�Ѵ��ڣ���������??
        printf("[CUSTOM_TONE] mkdir uwav ret=%d\n", mkdir_ret);
      }
      
      FILE *report_fp = fopen(report_path, "w");
      if (report_fp) {
        char buf[256];
        int len;
        uint32_t elapsed = timer_get_ms() - g_music_rx_start_ms;
        
        #define WRITE_LINE(str) fwrite(report_fp, str, strlen(str))
        
        WRITE_LINE("==================== ������Ч���䱨??====================\n");
        
        len = snprintf(buf, sizeof(buf), "����ʱ��: %u ms\n", elapsed);
        fwrite(report_fp, buf, len);
        
        len = snprintf(buf, sizeof(buf), "��Ч����: %u\n", Indivi_tone.tone_type);
        fwrite(report_fp, buf, len);
        
        len = snprintf(buf, sizeof(buf), "�ļ�·��: %s\n", current_file_path);
        fwrite(report_fp, buf, len);
        
        len = snprintf(buf, sizeof(buf), "�ļ�?? %s\n", Indivi_tone.file_name);
        fwrite(report_fp, buf, len);
        
        WRITE_LINE("\n--- ����ͳ�� ---\n");
        
        len = snprintf(buf, sizeof(buf), "���ֽ���: %u\n", g_music_rx_total_bytes);
        fwrite(report_fp, buf, len);
        
        len = snprintf(buf, sizeof(buf), "�����ֽ�: %u (%.2f%%)\n", 
                g_music_rx_nonzero_bytes,
                g_music_rx_total_bytes ? (g_music_rx_nonzero_bytes * 100.0f / g_music_rx_total_bytes) : 0.0f);
        fwrite(report_fp, buf, len);
        
        len = snprintf(buf, sizeof(buf), "ȫ������?? %u\n", g_music_rx_all_zero_pkts);
        fwrite(report_fp, buf, len);
        
        len = snprintf(buf, sizeof(buf), "�����ٶ�: %.2f KB/s\n",
                elapsed ? (g_music_rx_total_bytes / 1024.0f / (elapsed / 1000.0f)) : 0.0f);
        fwrite(report_fp, buf, len);
        
        WRITE_LINE("\n--- �����Լ�??---\n");
        
        len = snprintf(buf, sizeof(buf), "������С: %u �ֽ�\n", total_file_size);
        fwrite(report_fp, buf, len);
        
        len = snprintf(buf, sizeof(buf), "ʵ�ʴ�С: %u �ֽ�\n", file_write_offset);
        fwrite(report_fp, buf, len);
        
        len = snprintf(buf, sizeof(buf), "����CRC32: 0x%08X\n", Indivi_tone.file_crc32);
        fwrite(report_fp, buf, len);
        
        WRITE_LINE("\n--- ������������ ---\n");
        
        if (file_write_offset == total_file_size) {
          WRITE_LINE("??�ļ���С��ȷ\n");
        } else {
          WRITE_LINE("??�ļ���С��ƥ??\n");
        }
        
        if (g_music_rx_nonzero_bytes >= g_music_rx_total_bytes / 2) {
          WRITE_LINE("??�����ֽڱ������� (>50%)\n");
        } else if (g_music_rx_nonzero_bytes >= g_music_rx_total_bytes / 10) {
          WRITE_LINE("??�����ֽڱ���ƫ�� (10%~50%)\n");
        } else {
          WRITE_LINE("??�����ֽڱ������� (<10%), ���ݿ�����!\n");
        }
        
        if (g_music_rx_all_zero_pkts <= 5) {
          WRITE_LINE("??ȫ�����ݰ���������\n");
        } else {
          len = snprintf(buf, sizeof(buf), "??ȫ�����ݰ���??(%u??, ��������쳣\n", g_music_rx_all_zero_pkts);
          fwrite(report_fp, buf, len);
        }
        
        WRITE_LINE("\n========================================================\n");
        
        len = snprintf(buf, sizeof(buf), "��������ʱ��: (�̼�����ʱ�� %u ms)\n", timer_get_ms());
        fwrite(report_fp, buf, len);
        
        #undef WRITE_LINE
        
        fclose(report_fp);
        printf("[CUSTOM_TONE] ���䱨���ѱ��浽: %s\n", report_path);
      } else {
        printf("[CUSTOM_TONE] WARNING: �޷��������䱨���ļ�\n");
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
      /* ����Ԫ���ݵ�syscfg ���ں���������Ƶ���ŵ�Ԫ���ݽ��� */
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
//==================================��������??==================================================
void remove_vehicle_binding_instruct(uint16_t protocol_id) {
  uint8_t remove_binding_buffer[] = {0x01};
  // ������??
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
      log_info("�������ָ��: 0x%02X", remove_binding_buffer[0]);
    } else {
      log_info("�������ִ��ʧ��");
    }
  }
}
//===============================================================================================








