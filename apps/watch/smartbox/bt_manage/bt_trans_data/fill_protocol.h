/**
 * @file fill_protocol.h 
 * @author 
 * @brief   此文件用于定义填充协议相关的内容 填充串口协议和填充蓝牙协�?
 * @version 0.1
 * @date 2025-12-16
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#ifndef __FILL_PROTOCOL__
#define __FILL_PROTOCOL__
#include <stdint.h>
#include "le_multi_smartbox_module.h"
/// @brief 用于处理蓝牙协议的结构体
typedef struct ble_protocl_t {
  uint8_t head_prefix; // FE开�?
  uint16_t f_total;    // 总帧
  uint16_t f_num;      // 帧序�?
  uint16_t len;        // 长度
  uint16_t cmd;        // 协议id
  uint8_t *data;
  uint16_t crc16;       // crc16校验
  uint16_t tail_prefix; // OD OA结尾

} t_ble_protocl;    
/// 发送和接收协议结构体声明（在单�?C 文件中定义）
extern t_ble_protocl send_protocl; // 发送协议结构体给MCU
extern t_ble_protocl recv_protocl; // 接收协议结构体从手机�?
extern uint8_t protocol_content[256];
extern uint16_t total_frames;        // 总帧数量
extern uint16_t frame_number;        // 帧序�?
extern uint16_t protocol_id;         // 协议ID�?
extern uint8_t *content_data;     // 协议内容指针
extern uint8_t content_data_buffer[256]; // 协议内容缓冲�?
extern uint16_t content_length;      // 协议内容长度
// 用于处理f5f1_ID_dispose协议ID
void f5f1_ID_dispose(uint16_t protocol_id);
// 用于处理f6f2_ID_dispose协议ID
void f6f2_ID_dispose(uint16_t protocol_id, uint8_t *data);
// 用于处理f7f2_ID_dispose协议ID
void f7f2_ID_dispose(uint16_t protocol_id);
// 用于处理v5_1_function_control协议ID
void v5_1_function_control(uint16_t instruct, uint16_t protocol_id);
// 用于填充发送协�?
void fill_send_protocl(uint16_t cmd, uint8_t *data, uint16_t len);
// 用于将协议转换为发送缓�?
uint16_t convert_protocol_to_buffer(t_ble_protocl *protocol,uint8_t *buffer,uint16_t buffer_size);
// 用于处理MCU_SOC协议
int fill_MCU_SOC_protocl(uint8_t *data, uint16_t len, uint16_t handle);
// 用于填充接收协议
int fill_recv_protocl(uint8_t *data);
//从手机端填充数据到SOC_Phone_protocl结构�?
int fill_SOC_Phone_protocl(uint8_t *data, uint16_t len,uint16_t handle);
// 发送BLE密钥列表
void send_ble_key_list(uint16_t protocol_id);
// BLE配对请求处理
uint8_t ble_proto_ble_pair_req_Proc(uint16_t protocol_id);

// UART 下发配对 PIN/passkey�?~999999）：下次发起 BLE 配对时优先使用该 passkey�?// 说明：用于兼容“旧逻辑随机生成配对码”的同时，支�?MCU 指定配对码�?void ble_pairing_set_uart_passkey(uint32_t passkey);
void ble_pairing_set_uart_passkey(uint32_t passkey);
// 记录 UART 下发�?passkey，供 fill_protocol 统一使用
void fill_protocol_set_uart_passkey(uint32_t passkey);
// 消费 UART 下发的 passkey（一次性读取后自动清除），供 reset_passkey_cb 使用
uint8_t ble_pairing_consume_uart_passkey(uint32_t *out_key);
// 获取车辆音效列表�?x0075�?
void get_vehice_music_infromation_instruct_ext(uint16_t protocol_id);
// 音效选择�?x0076�?
void select_tone_instruct_ext(uint16_t protocol_id);
// f7f1定时器处理函�?
void vehicle_control_timer_start(u32 interval_ms);
// f7f1定时器停止（断连/解绑后必须停止，避免无连接持续发包导�?BLE buffer 满）
void vehicle_control_timer_stop(void);

// BLE 断连/解绑后的发送状态复位（�?pending，避免后续发送一�?drop�?
void fill_protocol_ble_tx_reset(void);

// btstack 可发送事件回调：用于�?buffer 满时延迟重试 notify
void fill_protocol_ble_on_can_send_now(void);
/// @brief APP功能码枚举定�?
typedef enum ENUM_APP_FUNC_CODE {

  /*************控制指令 00F1 枚举 A5-1功能表定�?
     部分不支持请备注*******************************/

  APP_FUNC_CODE_EBIKE_UNLOCK = 8, // 解锁
  APP_FUNC_CODE_EBIKE_LOCK = 9,   // 锁车

  APP_FUNC_CODE_OPEN_SEAT = 11,    // 开坐桶�?
  APP_FUNC_CODE_FIND_EBIKE = 12,   // 寻车
  APP_FUNC_CODE_REBOOT_EBIKE = 13, // 重启中控
  APP_FUNC_CODE_PAIR_NFC = 21,     // NFC配对

  APP_FUNC_CODE_PAIR_REMOTE_KEY = 18,   // 遥控器配�?
  APP_FUNC_CODE_CHECK_EBIKE = 24,       // 系统检�?
  APP_FUNC_CODE_EBIKE_LOCKING = 63,     // 车辆锁定
  APP_FUNC_CODE_EBIKE_UNLOCKING = 64,   // 解除车辆锁定
  APP_FUNC_CODE_SOUND_THEME = 66,       // 音效 个�?
  APP_FUNC_CODE_NAVIGATION_SCREEN = 70, // 投屏导航

  /*************设置参数指令 00F2 枚举 A5-2功能表定�?部分不支持请备注
   *******************************/

  APP_FUNC_CODE_PARAID_AUTO_LOCK = 31,           // 自动上锁
  APP_FUNC_CODE_PARAID_OVER_TIME_LOCK = 32,      // 超时上锁
  APP_FUNC_CODE_PARAID_AUTO_P = 33,              // 自动P�?
  APP_FUNC_CODE_PARAID_SIDE_PROP = 34,           // 边撑感应
  APP_FUNC_CODE_PARAID_CUSHION = 35,             // 坐垫感应
  APP_FUNC_CODE_PARAID_DELAY_HEADLIGHT = 36,     // 延时大灯
  APP_FUNC_CODE_PARAID_SENSE_HEADLIGHT = 37,     // 感应大灯
  APP_FUNC_CODE_PARAID_NFC = 38,                 // NFC开�?
  APP_FUNC_CODE_PARAID_SOUND = 39,               // 音效设置
  APP_FUNC_CODE_PARAID_HID_DISTANCE = 40,        // 无感解锁距离
  APP_FUNC_CODE_PARAID_CRUISE = 41,              // 定速巡�?
  APP_FUNC_CODE_PARAID_ASSIST = 42,              // 助力推行
  APP_FUNC_CODE_PARAID_ASTERN = 43,              // 辅助倒车
  APP_FUNC_CODE_PARAID_HID_UNLOCK = 44,          // 无感解锁
  APP_FUNC_CODE_PARAID_AUTO_LOCK_TIME = 45,      // 自动上锁时间
  APP_FUNC_CODE_PARAID_OVER_TIME_LOCK_TIME = 46, // 超时上锁时间

  APP_FUNC_CODE_PARAID_VOLUME = 47,            // 音量设置
  APP_FUNC_CODE_PARAID_ALARM_SHAKE = 48,       // 震动报警开�?
  APP_FUNC_CODE_PARAID_ALARM_DUMP = 49,        // 倾倒报�?
  APP_FUNC_CODE_PARAID_ALARM_MOVE = 50,        // 移动报警
  APP_FUNC_CODE_PARAID_ALARM_SHAKE_SENSE = 51, // 震动报警灵敏�?
  APP_FUNC_CODE_PARAID_FEST_EFFECT = 52,       // 节日音效
  APP_FUNC_CODE_PARAID_FACTORY_RESET = 56,     // 固定�?
  APP_FUNC_CODE_PARAID_TCS = 57,               // 侧滑
  APP_FUNC_CODE_PARAID_ABS = 58,               // 电子刹车
  APP_FUNC_CODE_PARAID_HHC = 59,               // 坡道驻车

  /*************钥匙  枚举 A5-3功能表定�?部分不支持请备注
   *******************************/

  APP_FUNC_CODE_KEY_BLE_HID = 19,     // 蓝牙钥匙
  APP_FUNC_CODE_KEY_EXT_BLE_HID = 20, // 外设蓝牙钥匙
  APP_FUNC_CODE_KEY_PASSWD = 22,      // 密码钥匙

} en_app_func_code;


// 创建f5f1服务的枚�?
typedef enum {
  equipment_information = 0x210F,               // 设备信息
  Battery_communication_configuration = 0x2002, // 电池通信配置
  Battery_voltage_configuration = 0x2003,       // 电池电压配置
  speed_limit = 0x2004,                         // 25 miles per hour speed limit
  Clear_total_mileage = 0x2005,                 // Clear total mileage
  tone_control = 0x2006,                        // 15-size tone control
  Battery_nominal_capacity_configuration = 0x2007, // 电池标称容量配置
  Battery_cell_type_configuration = 0x2008,        // 电池电芯类型配置

} f5f1_ID;

// 创建f6f0服务的枚�?
typedef enum {
  login_ID = 0x210E,
  support_function = 0x2109,
} f6f0_ID;

// 创建f7f0服务的枚�?除了0x0012其他都是app给soc发送的逻辑
typedef enum {
  vehicle_control = 0x00F1,              // 车辆控制
  vehicle_state = 0x0012,                // 获取车辆状�?
  vehicle_set_param = 0x00F2,            // 车辆设置参数
  get_key_list = 0x0038,                 // 获取蓝牙钥匙列表
  delete_NFC = 0x0059,                   // 删除NFC
  empty_NFC = 0x0060,                    // 清空NFC
  ble_pairing = 0x0037,                  // 蓝牙绑定
  ble_key_peripheral_send = 0x0039,      // 下发蓝牙钥匙�?
  delete_ble_key = 0x0040,               // 删除蓝牙钥匙
  empty_ble_key = 0x0041,                // 清空蓝牙钥匙
  send_vehicle_password_unlock = 0x0042, // 下发车辆密码解锁
  remove_vehicle_binding = 0x0057,       // 解除车辆绑定请求
  delet_phone_key = 0x0043,              // 删除手机钥匙
  empty_phone_key = 0x0044,              // 清空手机钥匙
  get_vehice_set_infromation = 0x0033,   // 获取车辆设置信息
  // active_acquirement_vehicle_information = 0x00F2, // 获取车辆激活获取信�?
  get_vehice_music_infromation = 0x0075, // 获取车辆音乐信息
  select_tone = 0x0076,                  // 选择音效（默�?个性）
  set_vehice_music = 0x2206,             // 个性音效设�?
  music_file_send = 0x2207,              // 音乐文件发�?
  open_lock = 0x0009,                    // 打开车辆�?
  ota_query_cap = 0x22A0,               // OTA capability query
  ota_enter = 0x22A1,                   // OTA enter
  ota_block_req = 0x22A2,               // OTA block request
  ota_block_data = 0x22A3,              // OTA block data
  ota_status_query = 0x22A4,            // OTA status query
  ota_exit = 0x22A5,                    // OTA exit

} f7f0_ID;
typedef enum {
  OTA_IDLE = 0,
  OTA_READY,
  OTA_DOWNLOADING,
  OTA_VERIFYING,
  OTA_DONE,
  OTA_FAILED,
} ota_state_e;

typedef enum {
  OTA_ERR_NONE = 0,
  OTA_ERR_STATE = 1,
  OTA_ERR_PARAM = 2,
  OTA_ERR_TIMEOUT = 3,
  OTA_ERR_CRC = 4,
  OTA_ERR_FLASH = 5,
  OTA_ERR_LOW_BAT = 6,
} ota_error_e;

typedef struct {
  uint8_t state;
  uint8_t last_error;
  uint32_t received_size;
  uint32_t total_size;
} ota_status_info_t;

void fill_protocol_ota_on_disconnect(void);
int fill_protocol_ota_bridge_start(uint8_t channel_type, uint32_t total_size);
int fill_protocol_ota_bridge_get_status(ota_status_info_t *info);

// 车辆状态结构体
typedef struct vehicle_state_t {
  uint8_t report;                 // 是否主动上报�?：主动上报，0：回�?
  uint32_t Riding_distance;       // 本次骑行距离，单位：�?
  uint32_t total_Riding_distance; // 总骑行距离，单位：米
  uint8_t battery_level;          // 电池电量，单位：%
  uint8_t cruising_ability;       // 续航能力
  uint8_t Power_status;           // 电源状�?0x00:下电�?x01:上电
  uint8_t
      voltage_indication; // 电压指示
                          // 0x00:表示电压正常�?x01表示电压低，0x02表示充电中，0x03表示电池不在�?
  uint8_t vehicle_lock_status; // 车辆锁状�?0x00:未锁�?x01:已锁
  uint8_t
      GPS_signal_strength; // GPS信号强度 0x00:无信号；0x01:信号极弱
                           // 0x02:信号较低弱；0x03:信号良好0x04:信号强；其它:保留
  uint8_t
      GSM_signal_strength; // GSM信号强度 0x00:无信号；0x01:信号极弱
                           // 0x02:信号较低弱；0x03:信号良好0x04:信号强；其它:保留
} vehicle_state_t;

#endif // !__FILL_PROTOCOL__



