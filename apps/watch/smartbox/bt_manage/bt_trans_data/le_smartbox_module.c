/*********************************************************************************************
    *   Filename        : le_smartbox_module.c

    *   Description     :

    *   Author          :

    *   Email           : zh-jieli.com

    *   Last modifiled  : 2022-06-08 11:14

    *   Copyright:(c)JIELI  2011-2022  @ , All Rights Reserved.
*********************************************************************************************/

// *****************************************************************************
// *****************************************************************************
#include "system/app_core.h"
#include "system/includes.h"
#include "smartbox/config.h"

#include "app_action.h"

#include "btstack/btstack_task.h"
#include "btstack/bluetooth.h"
#include "user_cfg.h"
#include "vm.h"
#include "app_power_manage.h"
#include "btcontroller_modules.h"
#include "bt_common.h"
#include "3th_profile_api.h"
#include "btstack/avctp_user.h"
#include "app_chargestore.h"

#include "btcrypt.h"
#include "custom_cfg.h"
#include "smartbox_music_info_setting.h"
#include "le_smartbox_adv.h"
#include "le_smartbox_module.h"
#include "classic/tws_api.h"
#include "smartbox_update.h"
#include "le_smartbox_multi_common.h"
#include "btstack/btstack_event.h"
#include "app_ble_spp_api.h"
#include "uos_type.h"
#include "lpa_export.h"
#include "ble_fmy_fmna.h"
#if OTA_RX_EN
#include "btstack/third_party/wireless_mic/wl_mic_api.h"
#include "update_rx_ota.h"
#endif

#if TCFG_CAT1_MODULE_UPDATE_ENABLE
#include "task_manager/cat1/cat1_common.h"
#endif

#if TCFG_PAY_ALIOS_ENABLE
#if (TCFG_PAY_ALIOS_WAY_SEL == TCFG_PAY_ALIOS_WAY_ALIYUN)
#else
#include "alipay.h"
#endif
#endif
#if 0//(TCFG_BLE_DEMO_SELECT == DEF_BLE_DEMO_RCSP_DEMO)

//ANCS profile enable
#define TRANS_ANCS_EN  			  	 1

//AMS profile enable
#define TRANS_AMS_EN  			  	 0

#if (defined(TCFG_UI_ENABLE_NOTICE) && (!TCFG_UI_ENABLE_NOTICE))
#undef TRANS_ANCS_EN
#define TRANS_ANCS_EN  			  	 0

#endif

#if 1
#define log_info(x, ...)       printf("[LE-SMART]" x " ", ## __VA_ARGS__)
#define log_info_hexdump       put_buf
#define log_error(x, ...)       printf("<error>[LE-SMART]" x " ", ## __VA_ARGS__)
#else
#define log_info(...)
#define log_info_hexdump(...)
#endif


#if OTA_RX_EN
static void wlm_1t1_rx_connect_succ_callback(void);
static void wlm_1t1_rx_disconnect_callback(void);
static void wlm_1t1_rx_iso_callback(const void *const buf, size_t length);
static void wlm_1t1_custom_callback(const void *const buf, size_t length, void *priv);


#define WL_MIC_PAIR_NAME        "BOTA_LOADER"


#define WIRELESS_CODING_FRAME_LEN   (75)
#define WIRELESS_CODING_SAMPLERATE  (48000)


#if (WIRELESS_CODING_FRAME_LEN == 50)
#define RX_FRAME_LEN_INDEX  0
#elif (WIRELESS_CODING_FRAME_LEN == 75)
#define RX_FRAME_LEN_INDEX  2
#elif (WIRELESS_CODING_FRAME_LEN == 100)
#define RX_FRAME_LEN_INDEX  4
#else
#error "wl_mic_1tN_rx WIRELESS_CODING_FRAME_LEN not allow !!!"
#endif

#define RX_ADAPTIVE_PARAM  \
    ((WIRELESS_CODING_SAMPLERATE == 44100)? \
        RX_FRAME_LEN_INDEX : (RX_FRAME_LEN_INDEX + 1))


#define SPECIFIC_STRING         "#@#@#@"
#define TCFG_WIRELESS_RSSI       50
/* 全局常量 */
static wlm_param rx_param = {
    .pair_name = WL_MIC_PAIR_NAME,
    .spec_name = SPECIFIC_STRING,

    .param = {
        [0] = 14, //scan_window
        [1] = 14, //scan_interval
        [2] = TCFG_WIRELESS_RSSI, // BIT(14)/BIT(15) | 50,
        [3] = 2,
        [4] = 50 * 5, //null packet send
        [5] = (40 << 8) | 20,//connect interval
        //re conenct
        [6] = 1000,
    },

};

static const wlm_lib_callback wlm_1t1_rx_cb = {
    .wlm_connect_succ = wlm_1t1_rx_connect_succ_callback,
    .wlm_disconnect   = wlm_1t1_rx_disconnect_callback,
    .wlm_iso_rx       = wlm_1t1_rx_iso_callback,
    .wlm_custom_cbk   = wlm_1t1_custom_callback,
};

static const wlm_lib_ops *const wlm_op = &wlm_1t1_rx_op;

#define __this              wlm_op

static const ota_rx_ops *const ota_op = &ota_rx_op;

#endif




#define EIR_TAG_STRING   0xd6, 0x05, 0x08, 0x00, 'J', 'L', 'A', 'I', 'S', 'D','K'
static const char user_tag_string[] = {EIR_TAG_STRING};

//------
/* #define ATT_LOCAL_PAYLOAD_SIZE    (247)//(517)                   //note: need >= 20 */
/* #define ATT_SEND_CBUF_SIZE        (512*2)                   //note: need >= 20,缓存大小，可修改 */
/* #define ATT_RAM_BUFSIZE           (ATT_CTRL_BLOCK_SIZE + ATT_LOCAL_PAYLOAD_SIZE + ATT_SEND_CBUF_SIZE)                   //note: */
/* static u8 att_ram_buffer[ATT_RAM_BUFSIZE] __attribute__((aligned(4))); */
//添加了edr的att后使用ble多机流程
#define SMBOX_MULTI_ATT_MTU_SIZE              (517) //ATT MTU的值
//ATT发送的包长,    note: 23 <= need >= MTU
#define SMBOX_MULTI_ATT_LOCAL_PAYLOAD_SIZE    (SMBOX_MULTI_ATT_MTU_SIZE)                   //
//ATT缓存的buffer大小,  note: need >= 23,可修改
#define SMBOX_MULTI_ATT_SEND_CBUF_SIZE        (512*2)                 //
//共配置的RAM
#define SMBOX_MULTI_ATT_RAM_BUFSIZE           (ATT_CTRL_BLOCK_SIZE + SMBOX_MULTI_ATT_LOCAL_PAYLOAD_SIZE + SMBOX_MULTI_ATT_SEND_CBUF_SIZE)                   //note:
static u8 smbox_multi_att_ram_buffer[SMBOX_MULTI_ATT_RAM_BUFSIZE] __attribute__((aligned(4)));

//---------------
//---------------
#define ADV_INTERVAL_MIN          (160 * 5) //n*0.625ms
/* #define ADV_INTERVAL_MIN          (0x30) */

#if (TCFG_BLE_ADV_DYNAMIC_SWITCH && TCFG_UI_ENABLE)
#define ADV_INTERVAL_QUICK			(160*1)
#define ADV_INTERVAL_NORMAL			(160*5)
#define ADV_INTERVAL_SLOW			(160*10)
#undef ADV_INTERVAL_MIN
#define ADV_INTERVAL_MIN			ADV_INTERVAL_QUICK // ADV_INTERVAL_NORMAL
#endif /* #if (TCFG_BLE_ADV_DYNAMIC_SWITCH && TCFG_UI_ENABLE) */

void *le_smartbox_ble_hdl = NULL;

//watch切换 app1 or app2
u8 select_app1_or_app2 = 1;
u8 app_change_key_is_press = 0;
u16 set_adv_interval;
static hci_con_handle_t con_handle;

#if UNISOC_ESIM_TEST
static u16 THINMODEM_MAX_SIZE;
#endif // UNISOC_ESIM_TEST

//加密设置
static const uint8_t sm_min_key_size = 7;

//连接参数设置
static const uint8_t connection_update_enable = 1; ///0--disable, 1--enable
static uint8_t connection_update_cnt = 0; //
static const struct conn_update_param_t connection_param_table[] = {
    {25, 32, 16, 600},
    {12, 28, 14, 600},//11
    {8,  20, 20, 600},//3.7
    /* {12, 28, 4, 600},//3.7 */
    /* {12, 24, 30, 600},//3.05 */
};

static const struct conn_update_param_t connection_param_table_update[] = {
    {96, 120, 0,  600},
    {60,  80, 0,  600},
    {60,  80, 0,  600},
    /* {8,   20, 0,  600}, */
    {6, 12, 0, 400},/*ios 提速*/
};

static u8 conn_update_param_index = 0;
static u8 conn_param_len = sizeof(connection_param_table) / sizeof(struct conn_update_param_t);
#define CONN_PARAM_TABLE_CNT      (sizeof(connection_param_table)/sizeof(struct conn_update_param_t))
#define CONN_TRY_NUM			  10 // 重复尝试次数

/* #if (ATT_RAM_BUFSIZE < 64) */
/* #error "adv_data & rsp_data buffer error!!!!!!!!!!!!" */
/* #endif */


static char *gap_device_name = "AC697N_ble_test";
static u8 gap_device_name_len = 0;
volatile static u8 ble_work_state = 0;
static u8 adv_ctrl_en;
static u8 ble_init_flag;

static u8 cur_peer_addr_info[7]; /*当前连接对方地址信息*/
static u8 check_rcsp_auth_flag; /*检查认证是否通过,ios发起回连edr*/
static u8 connect_remote_type;  /*remote's system type*/
static u8 pair_reconnect_flag;  /*配对回连标记*/

#if TCFG_PAY_ALIOS_ENABLE
static u8 upay_mode_enable;/*upay绑定模式*/
static u8 upay_new_adv_enable;/**/
static void (*upay_recv_callback)(const uint8_t *data, u16 len);
static void upay_ble_new_adv_enable(u8 en);
#endif

static void (*app_recieve_callback)(void *priv, void *buf, u16 len) = NULL;
static void (*dongle_app_recieve_callback)(void *priv, void *buf, u16 len) = NULL;
static void (*app_ble_state_callback)(void *priv, ble_state_e state) = NULL;
static void (*ble_resume_send_wakeup)(void) = NULL;
static u32 channel_priv;

extern u8 get_rcsp_connect_status(void);
extern const int config_btctler_le_hw_nums;

static const char *const phy_result[] = {
    "None",
    "1M",
    "2M",
    "Coded"
};
u32 send_wechat_timer = 0;
//------------------------------------------------------
//ANCS
#if TRANS_ANCS_EN
//profile event
#define ANCS_SUBEVENT_CLIENT_CONNECTED                              0xF0
#define ANCS_SUBEVENT_CLIENT_NOTIFICATION                           0xF1
#define ANCS_SUBEVENT_CLIENT_DISCONNECTED                           0xF2

#define ANCS_MESSAGE_MANAGE_EN                                      1
void ancs_client_init(void);
void ancs_client_exit(void);
void ancs_client_register_callback(btstack_packet_handler_t callback);
const char *ancs_client_attribute_name_for_id(int id);
void ancs_set_notification_buffer(u8 *buffer, u16 buffer_size);
u32 get_notification_uid(void);
u16 get_controlpoint_handle(void);
void ancs_set_out_callback(void *cb);

//ancs info buffer
#define ANCS_INFO_BUFFER_SIZE  (384)//support 内容长度 128byte
static u8 ancs_info_buffer[ANCS_INFO_BUFFER_SIZE];
#else
#define ANCS_MESSAGE_MANAGE_EN                                      0
#endif

//ams
#if TRANS_AMS_EN
//profile event
#define AMS_SUBEVENT_CLIENT_CONNECTED                               0xF3
#define AMS_SUBEVENT_CLIENT_NOTIFICATION                            0xF4
#define AMS_SUBEVENT_CLIENT_DISCONNECTED                            0xF5

void ams_client_init(void);
void ams_client_exit(void);
void ams_client_register_callback(btstack_packet_handler_t handler);
const char *ams_get_entity_id_name(u8 entity_id);
const char *ams_get_entity_attribute_name(u8 entity_id, u8 attr_id);
#endif

#if FINDMY_EN
static u8 fmy_owner_connect = false;
#endif
//------------------------------------------------------

extern void scc_client_init(void);
extern bool ble_set_make_random_address(uint8_t random_type);
static int app_send_user_data_check(u16 len);
static int app_send_user_data_do(void *priv, u8 *data, u16 len);
static int app_send_user_data(u16 handle, u8 *data, u16 len, u8 handle_type);
//------------------------------------------------------
//广播参数设置
static void advertisements_setup_init();
extern const char *bt_get_local_name();
static int set_adv_enable(void *priv, u32 en);
static int get_buffer_vaild_len(void *priv);
//------------------------------------------------------


#if CONFIG_USE_RANDOM_ADDRESS_ENABLE
typedef enum {
    RANDOM_ADDRESS_TYPE_OFF = 0,
    RANDOM_ADDRESS_TYPE_STATIC,
    RANDOM_ADDRESS_NON_RESOLVABLE,
    RANDOM_ADDRESS_RESOLVABLE,
} random_address_type_t;

static const u8 slave_address_random_type = RANDOM_ADDRESS_RESOLVABLE;
static const u16 slave_random_period_seconds = (15 * 60); //15 mins
static u16 slave_random_period_timer_id;
static u8 le_smart_random_address[6];

static void slave_random_make_random_address(void)
{
    if (ble_set_make_random_address(slave_address_random_type)) {
        le_controller_get_random_mac(le_smart_random_address);
        log_info("---ble's random generate address:");
        put_buf(le_smart_random_address, 6);
    } else {
        log_error("make random address fail!!!");
        ASSERT(0);
    }
}

static void slave_random_period_timer_handler(void *priev)
{
    //rotate address when in adv state
    if (app_ble_adv_state_get(le_smartbox_ble_hdl)) {
        log_info("period to change addr in adv");
        le_smartbox_bt_ble_adv_enable(0);
        slave_random_make_random_address();
        le_smartbox_bt_ble_adv_enable(1);
    } else {
        log_info("period to change addr");
        slave_random_make_random_address();
    }
}
#endif

#if FINDMY_EN
extern int bt_modify_name(u8 *new_name);
void fmy_other_device_ble_name_set(u8 *name)
{
    log_info("fmy change smartbox ble adv name");
    bt_modify_name(name);
    // open close adv when in adv state to make difference
    if (app_ble_adv_state_get(le_smartbox_ble_hdl)) {
        le_smartbox_bt_ble_adv_enable(0);
        le_smartbox_bt_ble_adv_enable(1);
    }
}
#endif

enum {
    BOTA_STA_IDLE = 0,
    BOTA_STA_TRIGGER_START,
    BOTA_STA_SCAN_READY,
};


enum {
    EVT_BOTA_MODE_INIT = 0x1,

};

#define BOTA_SERVER_CFG_LEN    40
struct bota_ctrl_t {
    u8 state;
    u8 server_cfg[BOTA_SERVER_CFG_LEN];
    u8 cfg_len;
};

static struct bota_ctrl_t  bota_ctrl;

extern int broadcast_upgrade_exit_sniff(void);
static void bota_trigger_start_handle(u8 *data, u8 len)
{
#if OTA_RX_EN
    printf("bota magaic:%c%c%c%c", data[0], data[1], data[2], data[3]);
    printf("bota version:%x", data[4]);
    printf("bota item type:%x %d\n", data[6], data[5]);
    printf("bota target rssi:%d\n", (s8)data[7]);
    printf("bota target name:%s\n", &data[8]);

    ble_module_enable(0);
    if (__this && __this->wlm_init) {
        rx_param.param[0] = 42;
        rx_param.param[1] = 42;
        log_info("OTA RX init... pair_name[%s] \
                spec_name[%s] \
                scan_window[%d]ms \
                null_packet_send[%d]ms \
                connect_interval[%d]ms \
                re_connect[%d]ms", \
                 rx_param.pair_name, \
                 rx_param.spec_name, \
                 rx_param.param[0] * 5 / 8, \
                 rx_param.param[1] * 5 / 8, \
                 rx_param.param[4] * 5 / 8, \
                 rx_param.param[5] >> 8 * 5 / 4, \
                 rx_param.param[5] & 0xff);
        __this->wlm_init(&rx_param, &wlm_1t1_rx_cb);
    }
    if (ota_op && ota_op->ota_init) {
        ota_op->ota_init();
    }

    broadcast_upgrade_exit_sniff();

    if (__this && __this->wlm_open) {
        __this->wlm_open();
    }
    if (__this && __this->wlm_process) {
        __this->wlm_process();
    }
#endif
}

void bota_event_to_user(u32 type, u8 event, u8 size)
{
    struct sys_event e;
    e.type = SYS_DEVICE_EVENT;
    e.arg  = (void *)type;
    e.u.bt.event = event;
    sys_event_notify(&e);
}

void bota_dev_event_handler(struct sys_event *evt)
{
#if OTA_RX_EN
    switch (evt->u.bt.event) {
    case EVT_BOTA_MODE_INIT:
        bota_trigger_start_handle(bota_ctrl.server_cfg, bota_ctrl.cfg_len);
        bota_ctrl.state = BOTA_STA_SCAN_READY;
        break;
    }
#endif
}

static void send_request_connect_parameter(u8 table_index)
{
    struct conn_update_param_t *param = NULL; //static ram
    switch (conn_update_param_index) {
    case 0:
        param = (void *)&connection_param_table[table_index];
        break;
    case 1:
        param = (void *)&connection_param_table_update[table_index];
        break;
    default:
        break;
    }

    log_info("update_request:-%d-%d-%d-%d-\n", param->interval_min, param->interval_max, param->latency, param->timeout);
    if (con_handle) {
        ble_user_cmd_prepare(BLE_CMD_REQ_CONN_PARAM_UPDATE, 2, con_handle, param);
    }
}

static void check_connetion_updata_deal(void)
{
    if (connection_update_enable) {
        if (connection_update_cnt < (CONN_PARAM_TABLE_CNT * CONN_TRY_NUM)) {
            send_request_connect_parameter(connection_update_cnt / CONN_TRY_NUM);
        }
    }
}

void notify_update_connect_parameter(u8 table_index)
{
    u8 conn_update_param_index_record = conn_update_param_index;
    if ((u8) - 1 != table_index) {
        conn_update_param_index = 1;
        send_request_connect_parameter(table_index);
    } else {
        if (connection_update_cnt >= (CONN_PARAM_TABLE_CNT * CONN_TRY_NUM)) {
            log_info("connection_update_cnt >= CONN_PARAM_TABLE_CNT");
            connection_update_cnt = 0;
        }
        send_request_connect_parameter(connection_update_cnt / CONN_TRY_NUM);
    }
    conn_update_param_index = conn_update_param_index_record;
}

static void connection_update_complete_success(u8 *packet)
{
    int con_handle, conn_interval, conn_latency, conn_timeout;

    con_handle = hci_subevent_le_connection_update_complete_get_connection_handle(packet);
    conn_interval = hci_subevent_le_connection_update_complete_get_conn_interval(packet);
    conn_latency = hci_subevent_le_connection_update_complete_get_conn_latency(packet);
    conn_timeout = hci_subevent_le_connection_update_complete_get_supervision_timeout(packet);

    log_info("conn_interval = %d\n", conn_interval);
    log_info("conn_latency = %d\n", conn_latency);
    log_info("conn_timeout = %d\n", conn_timeout);
}


static void set_ble_work_state(ble_state_e state)
{
    if (state != ble_work_state) {
        log_info("ble_work_st:%x->%x\n", ble_work_state, state);
        ble_work_state = state;
        if (app_ble_state_callback) {
            app_ble_state_callback((void *)channel_priv, state);
        }
    }
}

static ble_state_e get_ble_work_state(void)
{
    return ble_work_state;
}

ble_state_e smartbox_get_ble_work_state(void)
{
    return ble_work_state;
}

static void cbk_sm_packet_handler(void *_hdl, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    sm_just_event_t *event = (void *)packet;
    u32 tmp32;
    switch (packet_type) {
    case HCI_EVENT_PACKET:
        switch (hci_event_packet_get_type(packet)) {
        case SM_EVENT_JUST_WORKS_REQUEST:
            sm_just_works_confirm(sm_event_just_works_request_get_handle(packet));
            log_info("Just Works Confirmed.\n");
            break;
        case SM_EVENT_PASSKEY_DISPLAY_NUMBER:
            log_info_hexdump(packet, size);
            memcpy(&tmp32, event->data, 4);
            log_info("Passkey display: %06u.\n", tmp32);
            break;
        case SM_EVENT_PAIR_PROCESS:
            log_info("======PAIR_PROCESS: %02x\n", event->data[0]);
            put_buf(event->data, 4);
            if (event->data[0] == SM_EVENT_PAIR_SUB_RECONNECT_START) {
                pair_reconnect_flag = 1;
            }
            break;
        }
        break;
    }
}

static void can_send_now_wakeup(void)
{
    putchar('E');
    if (ble_resume_send_wakeup) {
        ble_resume_send_wakeup();
    }

}

_WEAK_
u8 ble_update_get_ready_jump_flag(void)
{
    return 0;
}

extern u8 is_bredr_close(void);
extern u8 is_bredr_inquiry_state(void);
extern void bt_init_bredr();
extern void bredr_conn_last_dev();
extern u8 connect_last_source_device_from_vm();
extern u8 connect_last_device_from_vm();
//参考识别
static void smart_att_check_remote_result(u16 con_handle, remote_type_e remote_type)
{
    log_info("smart %02x:remote_type= %02x\n", con_handle, remote_type);
    connect_remote_type = remote_type;
    //to do
    if (pair_reconnect_flag == 1 && connect_remote_type == REMOTE_TYPE_IOS) { //ble回连ios时启动edr回连
        if (is_bredr_close() == 1) {
            bredr_conn_last_dev();
        } else if (is_bredr_inquiry_state() == 1) {
            user_send_cmd_prepare(USER_CTRL_INQUIRY_CANCEL, 0, NULL);
#if TCFG_USER_EMITTER_ENABLE
            log_info(">>>connect last source edr...\n");
            connect_last_source_device_from_vm();
#else
            log_info(">>>connect last edr...\n");
            connect_last_device_from_vm();
#endif
        } else {
#if TCFG_USER_EMITTER_ENABLE
            log_info(">>>connect last edr...\n");
            connect_last_source_device_from_vm();
#else
            connect_last_device_from_vm();
#endif
        }
    }
}

/* ancs等待加密接口*/
void ancs_client_wait_request_pairing(u16 con_handle)
{
#if FINDMY_EN
    if (ble_fmy_get_con_handle() == con_handle) {
        return;
    }
#endif
    sm_api_request_pairing(con_handle);
}

static bool le_smart_check_is_fmy_owner(u8 addr_type, u8 *addr)
{
#if FINDMY_EN
    u8 temp_addr[6];
    u8 le_smart_cur_public_addr[6];

    // Swap the address bytes
    swapX(addr, temp_addr, 6);

    // Check if the owner pair is set
    if (fmy_is_owner_pair_set()) {
        // Get the current public address from the list
        if (ble_list_get_id_addr(temp_addr, addr_type, le_smart_cur_public_addr)) {
            log_info("le smartbox cur public address:");
            log_info_hexdump(le_smart_cur_public_addr, 6);

            // Check if the current address is the owner's address
            if (fmy_is_owner_address(le_smart_cur_public_addr)) {
                log_info("fmy owner connect");
                fmy_original_name_set();
                return true;
            } else {
                log_info("non fmy owner connect");
            }
        } else {
            log_info("can not get pub mac");
        }
        fmy_suffix_name_set();
    } else {
        log_info("fmy no pair, not need to change name");
    }
#endif
    return false;
}

/*
 * @section Packet Handler
 *
 * @text The packet handler is used to:
 *        - stop the counter after a disconnect
 *        - send a notification when the requested ATT_EVENT_CAN_SEND_NOW is received
 */


/* LISTING_START(packetHandler): Packet Handler */
static void cbk_packet_handler(void *ble_hdl, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    int mtu;
    u32 tmp;
    const char *attribute_name;

    switch (packet_type) {
    case HCI_EVENT_PACKET:
        switch (hci_event_packet_get_type(packet)) {

        /* case DAEMON_EVENT_HCI_PACKET_SENT: */
        /* break; */
        /* case ATT_EVENT_HANDLE_VALUE_INDICATION_COMPLETE: */
        /* log_info("ATT_EVENT_HANDLE_VALUE_INDICATION_COMPLETE\n"); */
        case ATT_EVENT_CAN_SEND_NOW:
            can_send_now_wakeup();
            break;

        case HCI_EVENT_LE_META:
            switch (hci_event_le_meta_get_subevent_code(packet)) {
            case HCI_SUBEVENT_LE_CONNECTION_COMPLETE: {

                if (!hci_subevent_le_enhanced_connection_complete_get_role(packet)) {
                    //is master
                    break;
                }
                set_app_connect_type(TYPE_BLE);
                app_change_key_is_press = 0; //更新模式为非app切换模式
                con_handle = little_endian_read_16(packet, 4);
                log_info("HCI_SUBEVENT_LE_CONNECTION_COMPLETE: %0x\n", con_handle);
                log_info_hexdump(packet + 7, 7);
                memcpy(cur_peer_addr_info, packet + 7, 7);

#if FINDMY_EN
                fmy_owner_connect =  le_smart_check_is_fmy_owner(packet[7], &packet[8]);
#endif

                connection_update_complete_success(packet + 8);
                /* ble_user_cmd_prepare(BLE_CMD_ATT_SEND_INIT, 4, con_handle, att_ram_buffer, ATT_RAM_BUFSIZE, ATT_LOCAL_PAYLOAD_SIZE); */
                bt_3th_type_dev_select(RCSP_BLE);
                set_ble_work_state(BLE_ST_CONNECT);
#if (0 == BT_CONNECTION_VERIFY)
                JL_rcsp_auth_reset();
#endif
                set_adv_notify_state(1);
                pair_reconnect_flag = 0;
                connect_remote_type = REMOTE_TYPE_UNKNOWN;
#if TCFG_BLE_BRIDGE_EDR_ENALBE
                check_rcsp_auth_flag = 0;
#endif
            }
            break;

            case HCI_SUBEVENT_LE_CONNECTION_UPDATE_COMPLETE:
                if (con_handle != little_endian_read_16(packet, 4)) {
                    break;
                }
                connection_update_complete_success(packet);
                break;

            case HCI_SUBEVENT_LE_DATA_LENGTH_CHANGE:
                if (con_handle != little_endian_read_16(packet, 3)) {
                    break;
                }
                log_info("HCI_SUBEVENT_LE_DATA_LENGTH_CHANGE\n");
                break;

            case HCI_SUBEVENT_LE_PHY_UPDATE_COMPLETE:
                if (con_handle != little_endian_read_16(packet, 4)) {
                    break;
                }

                if (con_handle) {
                    log_info("HCI_SUBEVENT_LE_PHY_UPDATE %s\n", hci_event_le_meta_get_phy_update_complete_status(packet) ? "Fail" : "Succ");
                    log_info("Tx PHY: %s\n", phy_result[hci_event_le_meta_get_phy_update_complete_tx_phy(packet)]);
                    log_info("Rx PHY: %s\n", phy_result[hci_event_le_meta_get_phy_update_complete_rx_phy(packet)]);
                }
                break;
            }
            break;

        case HCI_EVENT_DISCONNECTION_COMPLETE:
            if (con_handle != little_endian_read_16(packet, 3)) {
                break;
            }
            log_info("HCI_EVENT_DISCONNECTION_COMPLETE: %0x\n", packet[5]);
            con_handle = 0;

#if (TCFG_BLE_ADV_DYNAMIC_SWITCH && TCFG_UI_ENABLE)
            void le_adv_interval_reset(void);
            le_adv_interval_reset();
#endif

#if TCFG_PAY_ALIOS_ENABLE && UPAY_ONE_PROFILE
            upay_ble_new_adv_enable(0);
#endif

#if RCSP_ADV_MUSIC_INFO_ENABLE
            extern void stop_get_music_timer(u8 en);
            stop_get_music_timer(1);
#endif
            /* #if (0 == BT_CONNECTION_VERIFY) */
            /*             JL_rcsp_auth_reset(); */
            /* #endif */
            /* ble_user_cmd_prepare(BLE_CMD_ATT_SEND_INIT, 4, con_handle, 0, 0, 0); */
            set_ble_work_state(BLE_ST_DISCONN);
            set_app_connect_type(TYPE_NULL);
#if RCSP_UPDATE_EN
            if (get_jl_update_flag()) {
                break;
            }
#endif
#if TCFG_CAT1_MODULE_UPDATE_ENABLE
            if (cat1_module_is_updating()) {
                cat1_module_post_update_result(CAT1_UPDATE_MSG_BT_DISCONNECT, 0);
            }
#endif
            if (!ble_update_get_ready_jump_flag()) {
#if TCFG_USER_TWS_ENABLE
                if (!(tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED)) {
                    log_info(")))))))) 1\n");
                    le_smartbox_bt_ble_adv_enable(1);
                } else {
                    log_info("))))))))>>>> %d\n", tws_api_get_role());
                    if (tws_api_get_role() == TWS_ROLE_MASTER) {
                        le_smartbox_bt_ble_adv_enable(1);
                    }
                }
#else
#if SMBOX_MULTI_BLE_EN
                log_info("Disconn_to_switch_app  :%d\n", select_app1_or_app2);
                if (app_change_key_is_press) {
                    if (select_app1_or_app2) {
                        ble_profile_again_init(1);
                    } else {
                        smbox_multi_ble_profile_init(1);
                    }
                    app_change_key_is_press = 0;
                } else {
                    le_smartbox_bt_ble_adv_enable(1);
                }
#else
                le_smartbox_bt_ble_adv_enable(1);
#endif
#endif
            }

#if FINDMY_EN
            if (fmy_owner_connect) {
                fmy_suffix_name_set();
                fmy_owner_connect = false;
            }
#endif
            connection_update_cnt = 0;
            break;

#if UNISOC_ESIM_TEST
        case ATT_EVENT_MTU_EXCHANGE_COMPLETE:
            if (con_handle != little_endian_read_16(packet, 2)) {
                break;
            }
            mtu = att_event_mtu_exchange_complete_get_MTU(packet) - 3;
            log_info("ATT MTU = %u\n", mtu);
            /* ble_user_cmd_prepare(BLE_CMD_ATT_MTU_SIZE, 1, mtu); */
            THINMODEM_MAX_SIZE = mtu - 2;
            break;
#endif /* #if UNISOC_ESIM_TEST */

        case HCI_EVENT_VENDOR_REMOTE_TEST:
            log_info("--- HCI_EVENT_VENDOR_REMOTE_TEST\n");
            break;

        case L2CAP_EVENT_CONNECTION_PARAMETER_UPDATE_RESPONSE:
            if (con_handle != little_endian_read_16(packet, 2)) {
                break;
            }

            tmp = little_endian_read_16(packet, 4);
            log_info("-update_rsp: %02x\n", tmp);
            if (tmp) {
                connection_update_cnt++;
                log_info("remoter reject!!!\n");
                check_connetion_updata_deal();
            } else {
                connection_update_cnt = (CONN_PARAM_TABLE_CNT * CONN_TRY_NUM);
            }
            break;

        case HCI_EVENT_ENCRYPTION_CHANGE:
            if (con_handle != little_endian_read_16(packet, 3)) {
                break;
            }

            log_info("HCI_EVENT_ENCRYPTION_CHANGE= %d\n", packet[2]);
            if (packet[2] == 0) {
                /*ENCRYPTION is ok */
                check_connetion_updata_deal();
                /*要配对加密后才能检测对方系统类型*/
                att_server_set_check_remote(con_handle, smart_att_check_remote_result);
            }
            break;

#if TRANS_ANCS_EN
        case HCI_EVENT_ANCS_META:
            switch (hci_event_ancs_meta_get_subevent_code(packet)) {
            case ANCS_SUBEVENT_CLIENT_NOTIFICATION:
                log_info("ANCS_SUBEVENT_CLIENT_NOTIFICATION\n");
                attribute_name = ancs_client_attribute_name_for_id(ancs_subevent_client_notification_get_attribute_id(packet));
                if (!attribute_name) {
                    log_info("ancs unknow attribute_id :%d", ancs_subevent_client_notification_get_attribute_id(packet));
                    break;
                } else {
                    u16 attribute_strlen = little_endian_read_16(packet, 7);
                    u8 *attribute_str = (void *)little_endian_read_32(packet, 9);
                    log_info("Notification: %s - %s \n", attribute_name, attribute_str);
#if ANCS_MESSAGE_MANAGE_EN
                    extern void notice_set_info_from_ancs(void *name, void *data, u16 len);
                    notice_set_info_from_ancs(attribute_name, attribute_str, attribute_strlen);
#endif
                }
                break;

            case ANCS_SUBEVENT_CLIENT_CONNECTED:
                log_info("ANCS_SUBEVENT_CLIENT_CONNECTED\n");
                break;

            case ANCS_SUBEVENT_CLIENT_DISCONNECTED:
                log_info("ANCS_SUBEVENT_CLIENT_DISCONNECTED\n");
                break;

            default:
                break;
            }

            break;
#endif

#if TRANS_AMS_EN
        case HCI_EVENT_AMS_META:
            switch (packet[2]) {
            case AMS_SUBEVENT_CLIENT_NOTIFICATION: {
                log_info("AMS_SUBEVENT_CLIENT_NOTIFICATION\n");
                u16 Entity_Update_len = little_endian_read_16(packet, 7);
                u8 *Entity_Update_data = (void *)little_endian_read_32(packet, 9);
                /* log_info("EntityID:%d, AttributeID:%d, Flags:%d, utf8_len(%d):",\ */
                /* Entity_Update_data[0],Entity_Update_data[1],Entity_Update_data[2],Entity_Update_len-3); */
                log_info("%s(%s), Flags:%d, utf8_len(%d)", ams_get_entity_id_name(Entity_Update_data[0]),
                         ams_get_entity_attribute_name(Entity_Update_data[0], Entity_Update_data[1]),
                         Entity_Update_data[2], Entity_Update_len - 3);

#if 1 //for printf debug
                static u8 music_files_buf[128];
                u8 str_len = Entity_Update_len - 3;
                if (str_len > sizeof(music_files_buf)) {
                    str_len = sizeof(music_files_buf) - 1;
                }
                memcpy(music_files_buf, &Entity_Update_data[3], str_len);
                music_files_buf[str_len] = 0;
                printf("string:%s\n", music_files_buf);
#endif

                log_info_hexdump(&Entity_Update_data[3], Entity_Update_len - 3);
                /* if (Entity_Update_data[0] == 1 && Entity_Update_data[1] == 2) { */
                /* log_info("for test: send pp_key"); */
                /* ams_send_request_command(AMS_RemoteCommandIDTogglePlayPause); */
                /* } */
            }
            break;

            case AMS_SUBEVENT_CLIENT_CONNECTED:
                log_info("AMS_SUBEVENT_CLIENT_CONNECTED\n");
                break;

            case AMS_SUBEVENT_CLIENT_DISCONNECTED:
                log_info("AMS_SUBEVENT_CLIENT_DISCONNECTED\n");
                break;


            default:
                break;
            }
            break;
#endif

        }
        break;
    }
}

static void btstack_cbk_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    cbk_packet_handler(NULL, packet_type, channel, packet, size);
}

void ancs_update_status(u8 status)
{
    switch (status) {
    case 1:
        log_info("ancs trunk start \n");
        break;
    case 2:
        log_info("ancs trunk end \n");
#if ANCS_MESSAGE_MANAGE_EN
        extern void notice_add_info_from_ancs();
        notice_add_info_from_ancs();
#endif
        break;
    default:
        break;
    }
}

u8 wechat_data[] = {0x01, 0x10, 0x00, 0x00}; //1-3步数  4-6距离  7-9卡路里
extern u16 edr_att_conn_hadle;
extern void wechat_edr_send_data(void *pive);
void wechat_send_data(void *pive)
{
    if (!edr_att_conn_hadle) {
        ble_op_multi_att_send_data(con_handle, ATT_CHARACTERISTIC_fea1_01_VALUE_HANDLE, wechat_data, 4, ATT_OP_INDICATE);
    } else {
#if WECHAT_SPORT_ENABLE
        wechat_edr_send_data(NULL);
#endif
    }
}

/* LISTING_END */

/*
 * @section ATT Read
 *
 * @text The ATT Server handles all reads to constant data. For dynamic data like the custom characteristic, the registered
 * att_read_callback is called. To handle long characteristics and long reads, the att_read_callback is first called
 * with buffer == NULL, to request the total value length. Then it will be called again requesting a chunk of the value.
 * See Listing attRead.
 */


static uint16_t att_read_callback(void *ble_hdl, hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t offset, uint8_t *buffer, uint16_t buffer_size)
{

    uint16_t  att_value_len = 0;
    uint16_t handle = att_handle;

    /* log_info("READ Callback, handle %04x, offset %u, buffer size %u\n", handle, offset, buffer_size); */

    /* log_info("read_callback, handle= 0x%04x,buffer= %08x\n", handle, (u32)buffer); */
    log_info("read_callback,conn_handle =%04x, handle=%04x,buffer=%08x\n", connection_handle, handle, (u32)buffer);

    switch (handle) {
    case ATT_CHARACTERISTIC_2a00_01_VALUE_HANDLE:
        att_value_len = strlen(gap_device_name);

        if ((offset >= att_value_len) || (offset + buffer_size) > att_value_len) {
            break;
        }

        if (buffer) {
            memcpy(buffer, &gap_device_name[offset], buffer_size);
            att_value_len = buffer_size;
            log_info("\n------read gap_name: %s \n", gap_device_name);
        }

        break;
#if TCFG_PAY_ALIOS_ENABLE
    case ATT_CHARACTERISTIC_4a02_01_UNKOWN_DESCRIPTION_HANDLE:
        att_value_len = 6;

        if (buffer) {
            u8 tmp_data[6];
            memcpy(tmp_data, app_ble_adv_addr_get(le_smartbox_ble_hdl), 6);
            swapX(tmp_data, &buffer[0], 6) ;
        }
        break ;
#endif

#if TCFG_PAY_ALIOS_ENABLE
    case ATT_CHARACTERISTIC_4a02_01_CLIENT_CONFIGURATION_HANDLE:
#endif
    case ATT_CHARACTERISTIC_2a05_01_CLIENT_CONFIGURATION_HANDLE:
    case ATT_CHARACTERISTIC_ae02_01_CLIENT_CONFIGURATION_HANDLE:
    case ATT_CHARACTERISTIC_fea1_01_CLIENT_CONFIGURATION_HANDLE:
        if (buffer) {
            buffer[0] = multi_att_get_ccc_config(connection_handle, handle);
            buffer[1] = 0;
        }
        att_value_len = 2;
        break;
    case ATT_CHARACTERISTIC_fea1_01_VALUE_HANDLE:
        if (buffer) {
            memcpy(buffer, wechat_data, 10);
        }
        att_value_len = 10;
        break;
    case ATT_CHARACTERISTIC_fec9_01_VALUE_HANDLE:
        att_value_len = 6;
        u8 mac_addr_buf[6] ;
        swapX(bt_get_mac_addr(), mac_addr_buf, 6);
        if (buffer) {
            memcpy(buffer, mac_addr_buf, 6);
            put_buf(buffer, 6);
        }
        break;

    default:
        break;
    }

    log_info("att_value_len= %d\n", att_value_len);
    return att_value_len;

}


/* LISTING_END */
/*
 * @section ATT Write
 *
 * @text The only valid ATT write in this example is to the Client Characteristic Configuration, which configures notification
 * and indication. If the ATT handle matches the client configuration handle, the new configuration value is stored and used
 * in the heartbeat handler to decide if a new value should be sent. See Listing attWrite.
 */

#if UNISOC_ESIM_TEST
static void thinmodem_service_lpa_cb(uint8 *p_data, uint32 cur_len)
{
    u32 offset, remain_size, send_size;
    printf("@@@%s, line = %d, cur_len = %d\n", __FUNCTION__, __LINE__, cur_len);
    put_buf(p_data, cur_len);

    offset = 2;
    remain_size = cur_len - offset;
    while (remain_size) {
        send_size = remain_size > THINMODEM_MAX_SIZE ? THINMODEM_MAX_SIZE : remain_size;
        printf("@@@remain_size = %d, offset = %d, send_size = %d, THINMODEM_MAX_SIZE = %d\n", remain_size, offset, send_size, THINMODEM_MAX_SIZE);
        app_send_user_data(ATT_CHARACTERISTIC_6e400003_b5a3_f393_e0a9_e50e24dcca9e_01_VALUE_HANDLE, p_data + offset, send_size, ATT_OP_AUTO_READ_CCC);
        offset += send_size;
        remain_size -= send_size;
    }
}
#endif // UNISOC_ESIM_TEST

/* LISTING_START(attWrite): ATT Write */
static int att_write_callback(void *ble_hdl, hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size)
{
    int result = 0;
    u16 tmp16;

    u16 handle = att_handle;

    /* log_info("write_callback, handle= 0x%04x\n", handle); */
    log_info("write_callback,conn_handle =%04x, handle =%04x,size =%d\n", connection_handle, handle, buffer_size);

    switch (handle) {

    case ATT_CHARACTERISTIC_2a00_01_VALUE_HANDLE:
        tmp16 = BT_NAME_LEN_MAX;
        if ((offset >= tmp16) || (offset + buffer_size) > tmp16) {
            break;
        }

        if (offset == 0) {
            memset(gap_device_name, 0x00, BT_NAME_LEN_MAX);
        }
        memcpy(&gap_device_name[offset], buffer, buffer_size);
        log_info("\n------write gap_name:");
        break;

#if TCFG_PAY_ALIOS_ENABLE
    case ATT_CHARACTERISTIC_4a02_01_CLIENT_CONFIGURATION_HANDLE:
        log_info("\n------write ccc 4a20:%04x, %02x\n", handle, buffer[0]);
        set_ble_work_state(BLE_ST_NOTIFY_IDICATE);
        //att_set_ccc_config(handle, buffer[0]);
        multi_att_set_ccc_config(connection_handle, handle, buffer[0]);
        /* check_connetion_updata_deal(); */
        break;
#endif

    case ATT_CHARACTERISTIC_ae02_01_CLIENT_CONFIGURATION_HANDLE:
#if (0 == BT_CONNECTION_VERIFY)
        JL_rcsp_auth_reset();
#endif
        set_ble_work_state(BLE_ST_NOTIFY_IDICATE);
        check_connetion_updata_deal();
        log_info("\n------write ccc:%04x, %02x\n", handle, buffer[0]);
        /* att_set_ccc_config(handle, buffer[0]); */
        multi_att_set_ccc_config(connection_handle, handle, buffer[0]);
        can_send_now_wakeup();
#if TCFG_BLE_BRIDGE_EDR_ENALBE
        if (buffer[0] == 1 && pair_reconnect_flag == 1) {
            if (connect_remote_type == REMOTE_TYPE_IOS) {
                /*set tag,ios ble回连后，检查认证通过后发起edr连接*/
                check_rcsp_auth_flag = 1;
            }
        }
#endif
        extern void rcsp_update_ancs_disconn_handler(void);
        rcsp_update_ancs_disconn_handler();
        break;

    case ATT_CHARACTERISTIC_2a05_01_CLIENT_CONFIGURATION_HANDLE:
    case ATT_CHARACTERISTIC_fea1_01_CLIENT_CONFIGURATION_HANDLE:
        /* att_set_ccc_config(handle, buffer[0]); */
        multi_att_set_ccc_config(connection_handle, handle, buffer[0]);
        break;

    case ATT_CHARACTERISTIC_ae01_01_VALUE_HANDLE:
        /* if (!JL_rcsp_get_auth_flag()) { */
        /*     JL_rcsp_auth_recieve(buffer, buffer_size); */
        /*     break; */
        /* } */
#if TCFG_CAT1_UNISOC_ENABLE
        extern u8 ll_get_curr_conn_from_testbox();
        void ble_dongle_custom_recv_data(void *p, u8 * data, u8 len);
        if (ll_get_curr_conn_from_testbox()) {
            ble_dongle_custom_recv_data(0, buffer, buffer_size);
            /* dongle_app_recieve_callback(0, buffer, buffer_size); */
            /* app_send_user_data_do(NULL,buffer,buffer_size); // 发数接口 */
            break;
        }
#endif
        if (app_recieve_callback) {
            app_recieve_callback(0, buffer, buffer_size);
        }
        //      JL_rcsp_auth_recieve(data, len);
        break;

        /* if (app_recieve_callback) { */
        /*     app_recieve_callback(0, buffer, buffer_size); */
        /* } */
        /* //		JL_rcsp_auth_recieve(data, len); */
        /* break; */

#if TCFG_PAY_ALIOS_ENABLE
    case ATT_CHARACTERISTIC_4a02_01_VALUE_HANDLE:
        log_info("upay_ble_recv(%d):", buffer_size);
        log_info_hexdump(buffer, buffer_size);
        if (upay_recv_callback) {
            upay_recv_callback(buffer, buffer_size);
        }
        /* upay_ble_send_data("9876543210",10);// */
        break;
#endif  //TCFG_PAY_ALIOS_ENABLE

#if UNISOC_ESIM_TEST
    case ATT_CHARACTERISTIC_6e400002_b5a3_f393_e0a9_e50e24dcca9e_01_VALUE_HANDLE:
        printf("\n-UART_RX(%d):", buffer_size);
        put_buf(buffer, buffer_size);

        int p_ret;
        lpa_result_e lpa_res;
        static u8 *data;
        u32 write_size;
        static u32 remain_size = 0, offset_esim, init = 0, total_size;

        if (!init) {
            init = 1;
            lpa_init(&p_ret);
            printf("@@@line = %d, p_ret = %d\n", __LINE__, p_ret);

            lpa_set_device_name("JL_ESIM", strlen("JL_ESIM"), &p_ret);
            printf("@@@line = %d, p_ret = %d\n", __LINE__, p_ret);

            lpa_set_manufacturer_name("JL", strlen("JL"), &p_ret);
            printf("@@@line = %d, p_ret = %d\n", __LINE__, p_ret);

            thinmodem_register_lpa_callback(thinmodem_service_lpa_cb);
        }

        if (!remain_size) {
            remain_size = buffer[0] << 8 | buffer[1] << 0;
            total_size = remain_size + 4;
            data = zalloc(total_size);
            data[2] = buffer[0];
            data[3] = buffer[1];
            offset_esim = 4;
            printf("@@@start remain_size = %d， offset_esim = %d, buffer_size = %d, total_size = %d\n", remain_size, offset_esim, buffer_size, total_size);

            memcpy(data + offset, buffer + 2, buffer_size - 2);
            offset_esim += buffer_size - 2;
            remain_size -= buffer_size - 2;
            printf("@@@000remain_size = %d, offset_esim = %d\n", remain_size, offset_esim);
        } else {
            printf("@@@111remain_size = %d, offset = %d\n", remain_size, offset_esim);
            memcpy(data + offset_esim, buffer, buffer_size);
            offset_esim += buffer_size;
            remain_size -= buffer_size;
            printf("@@@222remain_size = %d, offset = %d\n", remain_size, offset_esim);
        }

        if (remain_size == 0) {
            put_buf(data, offset_esim);
            printf("data = %x, offset_esim = %d\n", data, offset_esim);

            offset_esim = 0;
            remain_size = total_size;
            while (remain_size) {
                write_size = remain_size > LPA_SEND_MAX_LEN ? LPA_SEND_MAX_LEN : remain_size;
                printf("@@@remain_size = %d, total_size = %d, write_size = %d, offset = %d\n", remain_size, total_size, write_size, offset_esim);
                lpa_res = lpa_channel_write(data + offset_esim, write_size, total_size, &p_ret);
                remain_size -= write_size;
                offset_esim += write_size;
            }

            free(data);
        }

        break;

    case ATT_CHARACTERISTIC_6e400003_b5a3_f393_e0a9_e50e24dcca9e_01_CLIENT_CONFIGURATION_HANDLE:
        printf("\n-UART_TX(%d):", buffer_size);
        put_buf(buffer, buffer_size);
//      printf("\n UART_TX, write ccc:%04x,%02x\n", handle, buffer[0]);
        att_set_ccc_config(handle, buffer[0]);
        break;
#endif

    default:
        break;
    }

    return 0;
}

static int app_send_user_data(u16 handle, u8 *data, u16 len, u8 handle_type)
{
    ble_cmd_ret_e ret = APP_BLE_NO_ERROR;

    if (!con_handle) {
        return APP_BLE_OPERATION_ERROR;
    }

    /* ret = ble_user_cmd_prepare(BLE_CMD_ATT_SEND_DATA, 4, handle, data, len, handle_type); */
    /* ret = ble_op_multi_att_send_data(con_handle, handle, data, len, handle_type); */
    ret = app_ble_att_send_data(le_smartbox_ble_hdl, handle, data, len, handle_type);

    if (ret == BLE_BUFFER_FULL) {
        ret = APP_BLE_BUFF_FULL;
    }

    if (ret) {
        log_info("app_send_fail:%d !!!!!!\n", ret);
    }
    return (int)ret;
}


static int make_set_adv_data(void)
{
    return smartbox_make_set_adv_data();
}

static int make_set_rsp_data(void)
{
    return smartbox_make_set_rsp_data();
}


u8 *ble_get_gatt_profile_data(u16 *len)
{
    *len = sizeof(profile_data);
    return (u8 *)profile_data;
}

bool ble_switch_new_adv_interval(u16 adv_interval)
{
    if (set_adv_interval == adv_interval) {
        return true;
    }

    log_info("switch adv_interval:%d-->%d\n", adv_interval, set_adv_interval);
    set_adv_interval = adv_interval;

#if TCFG_PAY_ALIOS_ENABLE
    if (upay_mode_enable && upay_new_adv_enable) {
        //upay new adv模式
        log_info("upay new adv switch do\n");
        upay_ble_new_adv_enable(0);
        upay_ble_new_adv_enable(1);
        return true;
    }
#endif

    if (BLE_ST_ADV == get_ble_work_state()) {
        log_info("adv switch do\n");
        le_smartbox_bt_ble_adv_enable(0);
        le_smartbox_bt_ble_adv_enable(1);
    }
    return true;
}

//广播参数设置
static void advertisements_setup_init()
{
    uint8_t adv_type = 0;
    uint8_t adv_channel = 7;
    int   ret = 0;
    u16 adv_interval = set_adv_interval;//0x30;

    /* ble_op_set_adv_param(adv_interval, adv_type, adv_channel); */
    app_ble_set_adv_param(le_smartbox_ble_hdl, adv_interval, adv_type, adv_channel);


#if TCFG_PAY_ALIOS_ENABLE
    if (upay_mode_enable) {
        ret = upay_ble_adv_data_set();
    } else
#endif
    {
        ret |= make_set_adv_data();
        ret |= make_set_rsp_data();
    }

    if (ret) {
        puts("advertisements_setup_init fail !!!!!!\n");
        return;
    }
}


void ble_sm_setup_init(io_capability_t io_type, u8 auth_req, uint8_t min_key_size, u8 security_en)
{
    if (config_le_sm_support_enable) {
        app_ble_sm_init(io_type, \
                        auth_req, \
                        min_key_size, \
                        security_en);
        return;

        //setup SM: Display only
        /* sm_init(); */
        /* sm_set_io_capabilities(io_type); */
        /* sm_set_authentication_requirements(auth_req); */
        /* sm_set_encryption_key_size_range(min_key_size, 16); */
        /* sm_set_request_security(security_en); */
        /* sm_event_callback_set(&cbk_sm_packet_handler); */
    }
}

static void ancs_notification_message(u8 *packet, u16 size)
{
#if ANCS_MESSAGE_MANAGE_EN
    u8 *value;
    u32 ancs_notification_uid;
    value = &packet[8];
    ancs_notification_uid = little_endian_read_32(value, 4);
    log_info("Notification: EventID %02x, EventFlags %02x, CategoryID %02x, CategoryCount %u, UID %04x",
             value[0], value[1], value[2], value[3], little_endian_read_32(value, 4));

    if (value[1] & BIT(2)) {
        log_info("is PreExisting Message!!!");
    }

    if (value[0] == 2) { //0:added 1:modifiled 2:removed
        log_info("remove message:ancs_notification_uid %04x", ancs_notification_uid);
        extern void notice_remove_info_from_ancs(u32 uid);
        notice_remove_info_from_ancs(ancs_notification_uid);
    } else if (value[0] == 0) {
        extern void notice_set_info_from_ancs(void *name, void *data, u16 len);
        log_info("add message:ancs_notification_uid %04x", ancs_notification_uid);
        notice_set_info_from_ancs("UID", &ancs_notification_uid, sizeof(ancs_notification_uid));
    }
#endif
}

#if OTA_RX_EN
static void wlm_1t1_rx_connect_succ_callback(void)
{
    log_info("%s\n", __FUNCTION__);
#if 1
    if (ota_op && ota_op->ota_start) {
        ota_op->ota_start();
    }
#endif
}
static void wlm_1t1_rx_disconnect_callback(void)
{
    log_info("%s\n", __FUNCTION__);
}
static void wlm_1t1_rx_iso_callback(const void *const buf, size_t length)
{
    log_info("%s\n", __FUNCTION__);
#if 1
    ota_op->ota_program((u8 *)buf, (u16)length);
#endif
}

extern int broadcast_upgrade_prepare(u8 *data, u8 data_len);
extern int broadcast_upgrade_loop(void);
static void wlm_1t1_custom_callback(const void *const buf, size_t length, void *priv)
{
    if (broadcast_upgrade_prepare(buf, length)) {
        printf("ota head crc fail\n");
    } else {
        printf("ota head crc ok\n");
        broadcast_upgrade_loop();
    }
}
static void bota_mode_trigger_callback(u8 *data, u8 len)
{
    //put_buf(data,len);
    if (bota_ctrl.state == BOTA_STA_IDLE) {
        puts("found ota server\n");
        bota_ctrl.state = BOTA_STA_TRIGGER_START;
        u8 copy_len = len > BOTA_SERVER_CFG_LEN ? BOTA_SERVER_CFG_LEN : len;
        memcpy(bota_ctrl.server_cfg, data, copy_len);
        bota_ctrl.cfg_len = copy_len;
        bota_event_to_user(DEVICE_EVENT_FROM_BOTA, EVT_BOTA_MODE_INIT, 0);
    }
}
#endif

#define SMART_TCFG_BLE_SECURITY_EN          0/*是否发请求加密命令*/
extern void le_device_db_init(void);
void le_smartbox_ble_profile_init(void)
{
    u8 tmp_ble_addr[6];
    log_info("ble profile init\n");
    printf("[BLE_DIAG] le_smartbox_ble_profile_init enter\n");

    /* le_device_db_init(); */

    /* #if TCFG_BLE_BRIDGE_EDR_ENALBE */
    /*     ble_sm_setup_init(IO_CAPABILITY_NO_INPUT_NO_OUTPUT, SM_AUTHREQ_SECURE_CONNECTION | SM_AUTHREQ_BONDING, 7, SMART_TCFG_BLE_SECURITY_EN); */
    /* #else */
    /*     ble_sm_setup_init(IO_CAPABILITY_NO_INPUT_NO_OUTPUT, SM_AUTHREQ_MITM_PROTECTION | SM_AUTHREQ_BONDING, 7, SMART_TCFG_BLE_SECURITY_EN); */
    /* #endif */
    if (le_smartbox_ble_hdl == NULL) {
        le_smartbox_ble_hdl = app_ble_hdl_alloc();
        if (le_smartbox_ble_hdl == NULL) {
            printf("le_smartbox_ble_hdl alloc err !!\n");
            return;
        }
    }

    app_ble_profile_set(le_smartbox_ble_hdl, profile_data);
    app_ble_att_read_callback_register(le_smartbox_ble_hdl, att_read_callback);
    app_ble_att_write_callback_register(le_smartbox_ble_hdl, att_write_callback);
    app_ble_att_server_packet_handler_register(le_smartbox_ble_hdl, cbk_packet_handler);
    app_ble_hci_event_callback_register(le_smartbox_ble_hdl, cbk_packet_handler);
    app_ble_l2cap_packet_handler_register(le_smartbox_ble_hdl, cbk_packet_handler);
    app_ble_sm_event_callback_register(le_smartbox_ble_hdl, cbk_sm_packet_handler);

#if DOUBLE_BT_SAME_MAC
    memcpy(tmp_ble_addr, (void *)bt_get_mac_addr(), 6);
#else
    lib_make_ble_address(tmp_ble_addr, (void *)bt_get_mac_addr());
#endif

#if CONFIG_USE_RANDOM_ADDRESS_ENABLE
    app_ble_adv_address_type_set(le_smartbox_ble_hdl, slave_address_random_type);
#else
    app_ble_adv_address_type_set(le_smartbox_ble_hdl, 0);
    app_ble_set_mac_addr(le_smartbox_ble_hdl, (void *)tmp_ble_addr);
#endif

#if 0
    /* setup ATT server */
    att_server_init(profile_data, att_read_callback, att_write_callback);
    att_server_register_packet_handler(cbk_packet_handler);
    /* gatt_client_register_packet_handler(packet_cbk); */

    // register for HCI events
    hci_event_callback_set(&cbk_packet_handler);
    /* ble_l2cap_register_packet_handler(packet_cbk); */
    /* sm_event_packet_handler_register(packet_cbk); */
    le_l2cap_register_packet_handler(&cbk_packet_handler);
#endif


#if TRANS_ANCS_EN || TRANS_AMS_EN
    if ((!config_le_sm_support_enable) || (!config_le_gatt_client_num)) {
        printf("ANCS need sm and client support!!!\n");
        ASSERT(0);
    }
#endif

    if (config_le_gatt_client_num) {
        //setup GATT client
        gatt_client_init();
    }

#if TRANS_ANCS_EN
    log_info("ANCS init...");
    //setup ANCS clent
    ancs_client_init();
    ancs_set_notification_buffer(ancs_info_buffer, sizeof(ancs_info_buffer));
    ancs_client_register_callback(&btstack_cbk_packet_handler);
    ancs_set_out_callback(ancs_notification_message);

#endif

#if TRANS_AMS_EN
    log_info("AMS init...");
    ams_client_init();
    ams_client_register_callback(&btstack_cbk_packet_handler);
    ams_entity_attribute_config(AMS_IDPlayer_ENABLE | AMS_IDQueue_ENABLE | AMS_IDTrack_ENABLE);
    /* ams_entity_attribute_config(AMS_IDTrack_ENABLE); */
#endif
#if TRANS_ANCS_EN || TRANS_AMS_EN
    scc_client_init();
#endif

#if SMBOX_BLE_CLIENT_EN
    void bt_multi_client_init();
    bt_multi_client_init();
#endif
    /* ble_op_multi_att_send_init(smbox_multi_att_ram_buffer, SMBOX_MULTI_ATT_RAM_BUFSIZE, SMBOX_MULTI_ATT_LOCAL_PAYLOAD_SIZE); */
#if OTA_RX_EN
    extern void le_bota_tigger_cbk_register(void (*cbk)(u8 *, u8));
    le_bota_tigger_cbk_register(bota_mode_trigger_callback);
#endif
}

#if SMBOX_MULTI_BLE_EN
void ble_profile_again_init(u8 enable)
{
    u8 tmp_ble_addr[6];
    if (enable) {
        log_info("ble profile init\n");
        /* setup ATT server */
        att_server_init(profile_data, att_read_callback, att_write_callback);
        att_server_register_packet_handler(btstack_cbk_packet_handler);
        /* gatt_client_register_packet_handler(packet_cbk); */

        // register for HCI events
        hci_event_callback_set(&btstack_cbk_packet_handler);
        /* ble_l2cap_register_packet_handler(packet_cbk); */
        /* sm_event_packet_handler_register(packet_cbk); */
        le_l2cap_register_packet_handler(&btstack_cbk_packet_handler);
        /* bt_ble_init(); */

#if DOUBLE_BT_SAME_MAC
        memcpy(tmp_ble_addr, (void *)bt_get_mac_addr(), 6);
#else
        lib_make_ble_address(tmp_ble_addr, (void *)bt_get_mac_addr());
#endif
        le_controller_set_mac((void *)tmp_ble_addr);
        log_info("ble address:\n");
        log_info_hexdump(tmp_ble_addr, 6);

#if TRANS_ANCS_EN
        log_info("ANCS init...");
        ancs_client_init();
        ancs_set_notification_buffer(ancs_info_buffer, sizeof(ancs_info_buffer));
        ancs_client_register_callback(&btstack_cbk_packet_handler);
        ancs_set_out_callback(ancs_notification_message);
#endif

#if TRANS_AMS_EN
        log_info("AMS init...");
        ams_client_init();
        ams_client_register_callback(&btstack_cbk_packet_handler);
        ams_entity_attribute_config(AMS_IDPlayer_ENABLE | AMS_IDQueue_ENABLE | AMS_IDTrack_ENABLE);
#endif

        le_smartbox_bt_ble_adv_enable(1);

    } else {
        le_smartbox_bt_ble_adv_enable(0);
    }
}
#endif

static int set_adv_enable(void *priv, u32 en)
{
    ble_state_e next_state, cur_state;

    if (!adv_ctrl_en && en) {
        return APP_BLE_OPERATION_ERROR;
    }

    log_info("con_handle %d\n", con_handle);
    if (con_handle) {
        return APP_BLE_OPERATION_ERROR;
    }

#if TCFG_PAY_ALIOS_ENABLE
    if (upay_mode_enable) {
        /*upay模式,跳过spp检测,可以直接开ADV*/
        goto contrl_adv;
    }
#endif

    if (en) {
        /*控制开adv*/
        if (get_app_connect_type() == TYPE_SPP) {
            log_info("spp connect type\n");
            return APP_BLE_OPERATION_ERROR;
        }

        // 防止ios只连上ble的情况下，android(spp)回连导致ble断开后重新开广播的情况
        if (get_rcsp_connect_status()) {
            log_info("spp is connecting\n");
            /* en = 0; */
            return APP_BLE_OPERATION_ERROR;
        }
    }

contrl_adv:

    if (en) {
        next_state = BLE_ST_ADV;
    } else {
        next_state = BLE_ST_IDLE;
    }

    cur_state =  get_ble_work_state();
    log_info("get_ble_work_state  %d\n", cur_state);
    switch (cur_state) {
    case BLE_ST_ADV:
    case BLE_ST_IDLE:
    case BLE_ST_INIT_OK:
    case BLE_ST_NULL:
    case BLE_ST_DISCONN:
        break;
    default:
        return APP_BLE_OPERATION_ERROR;
        break;
    }

    if (cur_state == next_state) {
        log_info("cur_state == next_state \n");
        return APP_BLE_NO_ERROR;
    }
    log_info("adv_en:%d\n", en);
    set_ble_work_state(next_state);
    if (en) {
        advertisements_setup_init();
    }
    /* ble_user_cmd_prepare(BLE_CMD_ADV_ENABLE, 1, en); */
    app_ble_adv_enable(le_smartbox_ble_hdl, en);

    log_info(">  set_adv_enable  %d\n", en);
    return APP_BLE_NO_ERROR;
}

int smartbox_set_adv_enable(void *priv, u32 en)
{
    return set_adv_enable(priv, en);
}

static int ble_disconnect(void *priv)
{
    if (con_handle) {
        if (BLE_ST_SEND_DISCONN != get_ble_work_state()) {
            log_info(">>>ble send disconnect\n");
            set_ble_work_state(BLE_ST_SEND_DISCONN);
            /* ble_user_cmd_prepare(BLE_CMD_DISCONNECT, 1, con_handle); */
            app_ble_disconnect(le_smartbox_ble_hdl);
        } else {
            log_info(">>>ble wait disconnect...\n");
        }
        return APP_BLE_NO_ERROR;
    } else {
        return APP_BLE_OPERATION_ERROR;
    }
}


static int get_buffer_vaild_len(void *priv)
{
    u32 vaild_len = 0;
    ble_user_cmd_prepare(BLE_CMD_ATT_VAILD_LEN, 1, &vaild_len);
    return vaild_len;
}

static int app_send_user_data_do(void *priv, u8 *data, u16 len)
{

#if TCFG_BLE_BRIDGE_EDR_ENALBE
    if (check_rcsp_auth_flag) {
        /*触发检测是否认证通过,通过后回连edr*/
        if (JL_rcsp_get_auth_flag()) {
            check_rcsp_auth_flag = 0;
            log_info("auth_flag is ok\n");
            if (is_bredr_close() == 1) {
                bredr_conn_last_dev();
            } else if (is_bredr_inquiry_state() == 1) {
                user_send_cmd_prepare(USER_CTRL_INQUIRY_CANCEL, 0, NULL);
#if TCFG_USER_EMITTER_ENABLE
                log_info(">>>connect last source edr...\n");
                connect_last_source_device_from_vm();
#else
                log_info(">>>connect last edr...\n");
                connect_last_device_from_vm();
#endif
            } else {
#if TCFG_USER_EMITTER_ENABLE
                log_info(">>>connect last edr...\n");
                connect_last_source_device_from_vm();
#else
                connect_last_device_from_vm();
#endif
            }
        }
    }
#endif

    return app_send_user_data(ATT_CHARACTERISTIC_ae02_01_VALUE_HANDLE, data, len, ATT_OP_AUTO_READ_CCC);
}

static int app_send_user_data_check(u16 len)
{
    u32 buf_space = get_buffer_vaild_len(0);
    if (len <= buf_space) {
        return 1;
    }
    return 0;
}

static int regiest_wakeup_send(void *priv, void *cbk)
{
    ble_resume_send_wakeup = cbk;
    return APP_BLE_NO_ERROR;
}

static int regiest_recieve_cbk(void *priv, void *cbk)
{
    channel_priv = (u32)priv;
    app_recieve_callback = cbk;
    return APP_BLE_NO_ERROR;
}

#if TCFG_CAT1_UNISOC_ENABLE
int ble_dongle_recieve_handle_regiester(void *priv, void (*cbk)(void *priv, void *buf, u16 len))
{
    dongle_app_recieve_callback = cbk;
    return APP_BLE_NO_ERROR;
}

#define SIMBA_MAX_DATA_LEN 200
#define SIMBA_MAX_RECV_DATA_LEN     (4000 + (4000/SIMBA_MAX_DATA_LEN))
#define SIMBA_MAX_UPLOAD_DATA_LEN  490

int ble_dongle_send_custom_data(u8 *data, u16 len)
{
    /* return app_send_user_data_do(NULL, data, len); */
    u8 *tmp_buf = (u8 *)malloc(SIMBA_MAX_DATA_LEN + 1);
    u8 packet_num = 0;
    u32 offset = 0;
    int ret = APP_BLE_NO_ERROR;
    len = len > SIMBA_MAX_UPLOAD_DATA_LEN ? SIMBA_MAX_UPLOAD_DATA_LEN : len;
    ASSERT(len <= SIMBA_MAX_UPLOAD_DATA_LEN);
    while (len) {
        /* log_debug("len %d, data 0x%x\n",len,(u32)data); */
        packet_num = (len + (SIMBA_MAX_DATA_LEN - 1)) / SIMBA_MAX_DATA_LEN - 1;
        tmp_buf[0] = packet_num;
        offset = len > SIMBA_MAX_DATA_LEN ? SIMBA_MAX_DATA_LEN : len;
        memcpy(tmp_buf + 1, data, offset);
        ret = app_send_user_data_do(NULL, tmp_buf, offset + 1);
        if (ret != APP_BLE_NO_ERROR) {
            return ret;
        }
        len -= offset;
        data += offset;
        os_time_dly(1);
    }
    free(tmp_buf);

    return ret;
}

// 超出SIMBA_MAX_RECV_DATA_LEN长度的数据会被丢掉
static void ble_dongle_custom_recv_data(void *p, u8 *data, u8 len)
{
    if (dongle_app_recieve_callback == NULL) {
        return;
    }

    static u8 packet_num = 0;
    static u8 *tmp_buf = NULL;
    static u16 simba_recv_len = 0;
    static unsigned long prev_jiffies = 0;

    if ((jiffies_msec() - prev_jiffies) > 100) { // 300ms超时
        printf("recv timeout\n");
        prev_jiffies = jiffies_msec();
        if (tmp_buf) {
            free(tmp_buf);
            tmp_buf = NULL;
        }
    }

    if (tmp_buf == NULL) {
        u16 buf_len = (data[0] + 1) * SIMBA_MAX_DATA_LEN;
        buf_len = buf_len > SIMBA_MAX_RECV_DATA_LEN ? SIMBA_MAX_RECV_DATA_LEN : buf_len;
        tmp_buf = malloc(buf_len);
        packet_num = data[0];
        simba_recv_len = 0;
    }

    simba_recv_len += (len - 1);
    if (simba_recv_len > SIMBA_MAX_RECV_DATA_LEN) {
        simba_recv_len = SIMBA_MAX_RECV_DATA_LEN;
        len -= (simba_recv_len - SIMBA_MAX_RECV_DATA_LEN);
    }
    memcpy(tmp_buf + ((packet_num - data[0]) * SIMBA_MAX_DATA_LEN), data + 1, len - 1);

    if ((data[0] == 0) || (simba_recv_len >= SIMBA_MAX_RECV_DATA_LEN)) {
        /* log_debug("recv end , len %d\n",simba_recv_len); */
        dongle_app_recieve_callback(0, tmp_buf, simba_recv_len);
        free(tmp_buf);
        tmp_buf = NULL;
    }
    prev_jiffies = jiffies_msec();
}
#endif

static int regiest_state_cbk(void *priv, void *cbk)
{
    channel_priv = (u32)priv;
    app_ble_state_callback = cbk;
    return APP_BLE_NO_ERROR;
}

void le_smartbox_bt_ble_adv_enable(u8 enable)
{
#if CONFIG_USE_RANDOM_ADDRESS_ENABLE
    if (enable) {
        log_info(">>>> SMARTBOX RPA ADDRESS SET");
        app_ble_adv_address_type_set(le_smartbox_ble_hdl, slave_address_random_type);
        le_controller_set_random_mac(le_smart_random_address);
        put_buf(le_smart_random_address, 6);
    }
#endif
    set_adv_enable(0, enable);
}

void ble_module_enable(u8 en)
{
    log_info("mode_en:%d\n", en);
    if (en) {
#if OTA_RX_EN
        bota_ctrl.state = BOTA_STA_IDLE;
#endif

        adv_ctrl_en = 1;
#if (TCFG_BLE_ADV_DYNAMIC_SWITCH && TCFG_UI_ENABLE)
        void le_adv_interval_reset(void);
        le_adv_interval_reset();
#endif
#if (!MUTIl_CHARGING_BOX_EN)
        le_smartbox_bt_ble_adv_enable(1);
#endif
    } else {
        if (con_handle) {
            adv_ctrl_en = 0;
            ble_disconnect(NULL);
        } else {
            le_smartbox_bt_ble_adv_enable(0);
            adv_ctrl_en = 0;
        }
    }
}


const struct ble_server_operation_t ble_operation = {
    .adv_enable = set_adv_enable,
    .disconnect = ble_disconnect,
    .get_buffer_vaild = get_buffer_vaild_len,
    .send_data = (void *)app_send_user_data_do,
    .regist_wakeup_send = regiest_wakeup_send,
    .regist_recieve_cbk = regiest_recieve_cbk,
    .regist_state_cbk = regiest_state_cbk,
};

void ble_get_server_operation_table(struct ble_server_operation_t **interface_pt)
{
    *interface_pt = (void *)&ble_operation;
}

void le_smartbox_bt_ble_init(void)
{
    log_info("***** ble_init******\n");
    printf("[BLE_DIAG] le_smartbox_bt_ble_init enter\n");
#if CONFIG_USE_RANDOM_ADDRESS_ENABLE
    slave_random_make_random_address();
    slave_random_period_timer_id = sys_timer_add(NULL, slave_random_period_timer_handler, slave_random_period_seconds * 1000);
#endif
    /* extern void le_adv_set_test_init(void); */
    /* le_adv_set_test_init(); */
    /* return; */

    if (ble_init_flag) {
        return;
    }

    set_adv_interval = ADV_INTERVAL_MIN;

#if TCFG_PAY_ALIOS_ENABLE
    upay_mode_enable = 0;
    upay_new_adv_enable = 0;

#if UPAY_ONE_PROFILE
    if (config_btctler_le_hw_nums < 2) {
        log_info("error:need add hw to adv new device!!!\n");
        ASSERT(0);
    }
#endif

#endif

    gap_device_name = bt_get_local_name();
    gap_device_name_len = strlen(gap_device_name);
    if (gap_device_name_len > BT_NAME_LEN_MAX) {
        gap_device_name_len = BT_NAME_LEN_MAX;
    }

    log_info("ble name(%d): %s \n", gap_device_name_len, gap_device_name);

    set_ble_work_state(BLE_ST_INIT_OK);
    printf("[BLE_DIAG] before bt_ble_init_do\n");
    bt_ble_init_do();
    printf("[BLE_DIAG] after bt_ble_init_do\n");

    ble_vendor_set_default_att_mtu(SMBOX_MULTI_ATT_LOCAL_PAYLOAD_SIZE);
    bt_ble_rcsp_adv_enable();

    ble_module_enable(1);

    ble_init_flag = 1;
}

void le_smartbox_bt_ble_exit(void)
{
    log_info("***** ble_exit******\n");
    if (!ble_init_flag) {
        return;
    }

    ble_module_enable(0);
#if SMART_BOX_EN
    extern void smartbox_exit(void);
    smartbox_exit();
#endif

#if FINDMY_EN
    sys_timer_del(slave_random_period_timer_id);
    slave_random_period_timer_id  = 0;
#endif

    ble_init_flag = 0;
}

void ble_app_disconnect(void)
{
    ble_disconnect(NULL);
}

hci_con_handle_t smartbox_get_con_handle(void)
{
    return con_handle;
}

#if TRANS_ANCS_EN
void hangup_ans_call_handle(u8 en)
{

    u32 notification_id;
    u16 control_point_handle;

    log_info("hang_up or answer\n");
    notification_id = get_notification_uid();
    control_point_handle = get_controlpoint_handle();
    u8 ble_hangup[] = {0x02, 0, 0, 0, 0, en};
    memcpy(&ble_hangup[1], &notification_id, 4);
    log_info_hexdump(ble_hangup, 6);
    u8 ble_hangup_size = 6;
    ble_op_multi_att_send_data(con_handle, control_point_handle, (void *)&ble_hangup, ble_hangup_size, ATT_OP_WRITE);

}
#endif


#if TCFG_PAY_ALIOS_ENABLE

#if UPAY_ONE_PROFILE
static void upay_ble_new_adv_enable(u8 en)
{
    if (upay_new_adv_enable == en) {
        return;
    }
    upay_new_adv_enable = en;
    if (en) {
        app_ble_set_adv_param(le_smartbox_ble_hdl, set_adv_interval, ADV_SCAN_IND, 7);
        upay_ble_adv_data_set();
    }
    app_ble_adv_enable(le_smartbox_ble_hdl, en);
    log_info(">>>new_adv_enable  %d\n", en);
}
#endif

/*upay recieve data regies*/
void upay_ble_regiest_recv_handle(void (*handle)(const uint8_t *data, u16 len))
{
    upay_recv_callback = handle;
}

/*upay send data api*/
int upay_ble_send_data(const uint8_t *data, u16 len)
{
    log_info("upay_ble_send(%d):", len);
    log_info_hexdump(data, len);
    if (multi_att_get_ccc_config(app_ble_get_hdl_con_handle(le_smartbox_ble_hdl), ATT_CHARACTERISTIC_4a02_01_CLIENT_CONFIGURATION_HANDLE)) {
        return app_ble_att_send_data(le_smartbox_ble_hdl, ATT_CHARACTERISTIC_4a02_01_VALUE_HANDLE, data, len, ATT_OP_NOTIFY);
    }
    return BLE_CMD_CCC_FAIL;
}

/*open or close upay*/
void upay_ble_mode_enable(u8 enable)
{
    if (enable == upay_mode_enable) {
        return;
    }

    upay_mode_enable = enable;
    log_info("upay_mode_enable= %d\n", upay_mode_enable);

#if UPAY_ONE_PROFILE
    if (upay_mode_enable) {
        if (con_handle) {
            /*已连接,要开新设备广播*/
            ble_op_latency_skip(con_handle, 0xffff);
            upay_ble_new_adv_enable(1);
        } else {
            /*未连接，只切换原设备广播*/
            ble_module_enable(0);
            ble_module_enable(1);
        }
    } else {
        upay_ble_new_adv_enable(0);
        if (!con_handle) {
            /*未连接，只切换广播*/
            ble_module_enable(0);
            ble_module_enable(1);
        } else {
            ble_op_latency_skip(con_handle, 0);
        }
    }
#else
    ble_module_enable(0);
    if (upay_mode_enable) {
        att_server_change_profile(profile_data_upay);
#if TRANS_ANCS_EN
        ancs_client_exit();
#endif
#if TRANS_AMS_EN
        ams_client_exit();
#endif

    } else {
        att_server_change_profile(profile_data);
#if TRANS_ANCS_EN
        ancs_client_init();
#endif
#if TRANS_AMS_EN
        ams_client_init();
#endif

    }
    ble_module_enable(1);

#endif

}
#endif


#if (TCFG_BLE_ADV_DYNAMIC_SWITCH && TCFG_UI_ENABLE)
#include "ui/ui_api.h"

#define TCFG_BLE_ADV_QUICK_TIMEOUT			(10 *1000L)
#define TCFG_BLE_ADV_NORMAL_TIMEOUT			(30 *1000L)

extern int lcd_sleep_status();

static u16 le_lcd_sleep_to = 0;

static void le_lcd_sleep_enter(void);
static void le_lcd_sleep_exit(void);

static int le_lcd_sleep_deal(int to)
{
    /* printf("\n\n le_lcd_sleep_deal:%d \n\n", to); */
    ble_switch_new_adv_interval(to);
    return 0;
}

static void le_lcd_sleep_normal_to_deal(void *priv)
{
    /* printf("\n\n le_lcd_sleep_normal_to_deal \n\n"); */
    le_lcd_sleep_to = 0;
    int msg[3];
    msg[0] = (int)le_lcd_sleep_deal;
    msg[1] = 1;
    msg[2] = (int)ADV_INTERVAL_SLOW;
    os_taskq_post_type("app_core", Q_CALLBACK, 3, msg);
}

static void le_lcd_sleep_quick_to_deal(void *priv)
{
    /* printf("\n\n le_lcd_sleep_quick_to_deal \n\n"); */
    le_lcd_sleep_to = 0;
    int msg[3];
    msg[0] = (int)le_lcd_sleep_deal;
    msg[1] = 1;
    msg[2] = (int)ADV_INTERVAL_NORMAL;
    os_taskq_post_type("app_core", Q_CALLBACK, 3, msg);
    le_lcd_sleep_to = sys_timeout_add(NULL, le_lcd_sleep_normal_to_deal, TCFG_BLE_ADV_NORMAL_TIMEOUT);
}

static void le_adv_interval_reset(void)
{
    set_adv_interval = ADV_INTERVAL_MIN;
    if (lcd_sleep_status()) { // 息屏中
        le_lcd_sleep_enter();
    }
}

static void le_lcd_sleep_enter(void)
{
    // 灭屏
    if (le_lcd_sleep_to) {
        sys_timeout_del(le_lcd_sleep_to);
        le_lcd_sleep_to = 0;
    }
    le_lcd_sleep_to = sys_timeout_add(NULL, le_lcd_sleep_quick_to_deal, TCFG_BLE_ADV_QUICK_TIMEOUT);
}

static void le_lcd_sleep_exit(void)
{
    // 亮屏
    if (le_lcd_sleep_to) {
        sys_timeout_del(le_lcd_sleep_to);
        le_lcd_sleep_to = 0;
    }
    int msg[3];
    msg[0] = (int)le_lcd_sleep_deal;
    msg[1] = 1;
    msg[2] = (int)ADV_INTERVAL_QUICK;
    os_taskq_post_type("app_core", Q_CALLBACK, 3, msg);
}

REGISTER_LCD_SLEEP_HEADLER(le_lcd_sleep) = {
    .name = "vad",
    .enter = le_lcd_sleep_enter,
    .exit = le_lcd_sleep_exit,
};

#endif

#endif
