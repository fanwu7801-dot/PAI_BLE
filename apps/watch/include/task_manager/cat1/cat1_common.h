#ifndef __CAT1_COMMON_H__
#define __CAT1_COMMON_H__

#include "typedef.h"
#include "app_config.h"
#include "ual_gnss_export.h"
#include "aic_srv_tele_sms.h"
#include "ual_tele_sms_export.h"

#if TCFG_CAT1_UNISOC_ENABLE
#define SMS_MAX_NUMBER   UAL_TELE_SMS_NUMBER_MAX_LEN
#elif TCFG_CAT1_AICXTEK_ENABLE
#define SMS_MAX_NUMBER   TS_SMS_NUM_MAX_LEN
#else
#define SMS_MAX_NUMBER   21
#endif

#if TCFG_CAT1_UNISOC_ENABLE
#define SMS_MAX_CONTENT   UAL_TELE_SMS_CONTENT_MAX_LEN
#elif TCFG_CAT1_AICXTEK_ENABLE
#define SMS_MAX_CONTENT   TS_SMS_PDU_MAX_LEN
#else
#define SMS_MAX_CONTENT   441
#endif

// 升级相关的枚举
enum : u8 {
    // 升级任务的消息处理
    CAT1_UPDATE_MSG_SUCC = 0,
    CAT1_UPDATE_MSG_FAIL,
    CAT1_UPDATE_MSG_TIMEOUT,
    CAT1_UPDATE_MSG_DATA_TRANS_END,
    CAT1_UPDATE_MSG_START,
    CAT1_UPDATE_MSG_STARTING,   // 5
    CAT1_UPDATE_MSG_BT_DISCONNECT,
    CAT1_GET_VERSION,
    CAT1_UPDATE_MSG_DATA_TRANS,
    CAT1_UPDATE_MSG_UPDATE_END,
};

// 模块电源状态
typedef enum cat1_power_state {
    CAT1_POWER_STATE_POWEROFF = 0,
    CAT1_POWER_STATE_POWERON,
    CAT1_POWER_STATE_OTHER = 0xff,
} cat1_power_state_t;

struct s_wifi_info {
    int rssi;
    char mac_addr[20];
};

//gps相关结构体
struct s_gps_info {
    float longitude;
    float latitude;
    float height;
    int ttff_time;
    int satellite_num;
    ual_gnss_satellite_info_t satellite_info[UAL_GNSS_SATELLITE_MAX_NUM];
};

//cat1状态结构体
struct s_cat1_status {
    int lte_net_enable;
    int is_flight_mode;
    int lte_call_enable;
    int lte_enable;
    int lte_can_touch;
    int flight_mode_can_touch;
    int is_call_register;
};

//短信结构体
struct s_cat1_sms_info {
    char number[SMS_MAX_NUMBER];
    char content[SMS_MAX_CONTENT];
};

//短信发送结构体
struct input_content_t {
    char content[SMS_MAX_CONTENT];
    int index;
};

struct cat1_send_sms_info_t {
    int input_index;
    int offline_show_timer;
    int sending_count;
    int sending_show_timer;
    int timeout_show_timer;
    struct input_content_t input_content;
};

bool cat1_call_answer(void);  //接听电话

bool cat1_call_hangup(void);  //挂断电话

bool cat1_call_number(int number_len, char *number);  //拨打电话

bool cat1_get_call_status(u8 *status);  //获取通话状态

bool cat1_get_call_phone_num(char **phone_num);  //获取电话号码

bool cat1_check_call_enable(void);  //检查是否可以进行通话

//4G模块上电
bool cat1_ctrl_poweron(void);
bool cat1_ctrl_poweroff(void);
bool cat1_ctrl_is_poweron(void);

//设置飞行模式
void cat1_set_flight_mode(bool flag);
bool cat1_get_flight_mode(void);

//设置移动网络
int cat1_get_lte_net_onoff(void);
void cat1_set_lte_net_onoff(int flag);

//设置通话能力
int cat1_get_lte_call_onoff(void);
void cat1_set_lte_call_onoff(int flag);

//设置移动网络
int cat1_get_lte_onoff(void);
void cat1_set_lte_onoff(int flag);

char *cat1_get_operator(void);  //获取运营商名称

char *cat1_get_smsc(void);  //获取短信中心号码
int cat1_send_sms(int number_len, char *number, int content_len, char *content);  //发送短信
void cat1_set_send_sms_number(char *number, int offset, int len);
char *cat1_get_send_sms_number(void);
void cat1_set_send_sms_content(char *content, int offset, int len);
char *cat1_get_send_sms_content(void);
void clear_send_sms(void);
void clear_send_sms_content(void);

int cat1_get_signal_level(void);  //获取信号强度

//从工模中配置参数
int cat1_get_engineering_mode(void);
void cat1_set_engineering_mode(int flag);

//从设置菜单中配置参数
int cat1_get_set_mode(void);
void cat1_set_set_mode(int flag);

int cat1_get_call_register(void);
void cat1_set_call_register(int flag);

//4G模块是否可以上下电
int cat1_get_lte_can_touch(void);
void cat1_set_lte_can_touch(int flag);

//飞行模式是否可以设置
int cat1_get_flight_can_touch(void);
void cat1_set_flight_can_touch(int flag);

void cat1_init(void);  //4G模块初始化
void cat1_close(void);  //4G模块关闭




void cat1_audio_close(void);
int cat1_audio_open(void);

int app_cat1_audio_start(void);
int app_cat1_audio_stop(void);

int app_cat1_check_support(void);

bool cat1_call_use_modem_volume(void);
void cat1_call_set_modem_volume(int volume);
int cat1_call_get_modem_volume(void);

void cat1_call_input_mute(u8 mute);
int cat1_call_get_input_mute_status(void);

// 提供给cat1_update.c的接口
cat1_power_state_t cat1_module_get_power_state(void);
u8 cat1_module_get_vender_id(void);
u16 cat1_module_get_version_total_len(void);
u8 cat1_module_pasre_version(u8 *ver_buf);
int cat1_module_fota_start(u32 packet_size, u32 timeout);
int cat1_module_fota_end(void);
int cat1_module_fota_trans_data(u8 *data, u32 len);
u8 cat1_module_can_get_version(void);
int cat1_module_fota_close(void);
u16 cat1_module_get_update_end_timeout(void);

// 提供给其它模块的接口
void cat1_module_update_init(void);
u16 cat1_module_update_basic_info_get(u8 *buf, u16 len);
u16 cat1_module_update_get_update_result_info(u8 *buf, u16 len);
u8 cat1_module_update_start(u32 size);
void cat1_module_post_update_result(u8 type, int result);
u8 cat1_module_is_updating(void);
void cat1_module_update_task_del(u8 result);

#endif

