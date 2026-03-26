#include "system/includes.h"
#include "app_config.h"
#include "app_task.h"
#include "app_main.h"
#include "clock_cfg.h"
#include "app_action.h"
#include "key_event_deal.h"
#include "ui/ui_api.h"
#include "bt/bt.h"
#include "ui_manage.h"
#include "watch_common.h"
#include "cat1/cat1_common.h"
#include "call_common.h"
#include "tone_player.h"
#include "btstack/avctp_user.h"
#include "message_vm_cfg.h"
#include "rtc/alarm.h"
#include "timestamp.h"
#include "ual_tele_call_export.h"
#include "ual_gnss_export.h"
#include "ual_tele_radio_export.h"
#include "ual_tele_sim_export.h"
#include "ual_wifi_export.h"
#include "ual_tele_sms_export.h"
#include "unisoc_at.h"
#include "thinmodem_export.h"
#include "rpc_rtos.h"
#include "uos_iis.h"
#include "modem_ctrl_api.h"
#include "message_vm_cfg.h"

#if TCFG_APP_CAT1_EN && TCFG_CAT1_UNISOC_ENABLE

#define LOG_TAG_CONST       APP_CAT1
#define LOG_TAG             "[APP_CAT1]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
#define LOG_CLI_ENABLE
#include "debug.h"

#define   jl_version   "v1.5_20231114"

struct s_cat1_info {
    /* 当前通话的id值 */
    uint8_t call_id;
    /* 对端的电话号码 */
    char tele_num[UAL_TELE_CALL_TELE_NUM_MAX + 1];
    /*定时器*/
    u16 cat1_call_check_tmr;
    /*ims状态*/
    u32 ims_is_on;
    /*sim卡状态*/
    u32 sim_is_ready;
    /*驻网状态*/
    u32 plmn_state;
    /*铃音类型*/
    u32 alert_type;

    u8 power_on;

    char operator_long_name[15];

    int32_t is_engineering_mode;

    int32_t is_set_mode;

    int32_t client_handle;

    int32_t client_data_handle;

    int32_t is_plug_out;

    int32_t is_mic_mute;

    int32_t signal_level;

    int32_t radio_is_on;

    BOOLEAN not_first_cnf;
};

int gnss_timer = 0;
static struct s_cat1_info cat1_info = {0};
struct s_wifi_info wifi_info = {0};
struct s_gps_info gps_info = {0};
ual_tele_radio_lte_cell_info_t serving_cell_info = {0};
ual_tele_radio_lte_neighbor_cell_info_t neighbor_cell_info = {0};
struct s_cat1_sms_info cat1_sms_info = {0};     //4g短信信息
struct s_cat1_status cat1_status = {.lte_can_touch = 1, .flight_mode_can_touch = 1, .is_call_register = 1};

static const u32 num0_9[] = {
    (u32)TONE_NUM_0,
    (u32)TONE_NUM_1,
    (u32)TONE_NUM_2,
    (u32)TONE_NUM_3,
    (u32)TONE_NUM_4,
    (u32)TONE_NUM_5,
    (u32)TONE_NUM_6,
    (u32)TONE_NUM_7,
    (u32)TONE_NUM_8,
    (u32)TONE_NUM_9,
} ;

//apn配置
typedef struct {
    uint32 mnc;
    uint32 mcc;
    uint8 apn_type;
    char apn[20];
    char username[20];
    char password[20];
    uint16 auth_type;
    uint16  ip_type;
} apn_info_t;

typedef enum {
    apn_type_internet,
    apn_type_ia,
    apn_type_xcap,
    apn_type_ims,
    apn_type_sos
} apn_type_e;

//apn查询结构体
static apn_info_t apn_list[60] = {
    {0, 460, apn_type_internet, "cmnet", "", "", 0, 2},
    {0, 460, apn_type_xcap, "cmnet", "", "", 0, 2},
    {0, 460, apn_type_internet, "cmwap", "", "", 0, 2},
    {0, 460, apn_type_ims, "ims", "", "", 0, 2},
    {1, 460, apn_type_internet, "3gnet", "", "", 0, 2},
    {1, 460, apn_type_xcap, "3gnet", "", "", 0, 2},
    {1, 460, apn_type_internet, "3gwap", "", "", 0, 2},
    {1, 460, apn_type_ims, "ims", "", "", 0, 2},
    {2, 460, apn_type_internet, "cmnet", "", "", 0, 2},
    {2, 460, apn_type_xcap, "cmnet", "", "", 0, 2},
    {2, 460, apn_type_internet, "cmwap", "", "", 0, 2},
    {2, 460, apn_type_ims, "ims", "", "", 0, 2},
    {3, 460, apn_type_internet, "ctwap", "ctwap@mycdma.cn", "vnet.mobi", 3, 2},
    {3, 460, apn_type_xcap, "ctwap", "ctwap@mycdma.cn", "vnet.mobi", 3, 2},
    {3, 460, apn_type_internet, "ctnet", "ctnet@mycdma.cn", "vnet.mobi", 3, 2},
    {4, 460, apn_type_internet, "cmnet", "", "", 3, 2},
    {6, 460, apn_type_internet, "3gnet", "", "", 3, 2},
    {6, 460, apn_type_internet, "3gwap", "", "", 3, 2},
    {7, 460, apn_type_internet, "cmnet", "", "", 0, 2},
    {7, 460, apn_type_internet, "cmnet", "", "", 0, 2},
    {7, 460, apn_type_internet, "cmwap", "", "", 0, 2},
    {8, 460, apn_type_internet, "cmnet", "", "", 0, 2},
    {8, 460, apn_type_internet, "cmnet", "", "", 0, 2},
    {8, 460, apn_type_internet, "cmwap", "", "", 0, 2},
    {9, 460, apn_type_internet, "3gnet", "", "", 0, 2},
    {9, 460, apn_type_internet, "3gnet", "", "", 0, 2},
    {9, 460, apn_type_internet, "3gwap", "", "", 0, 2},
    {11, 460, apn_type_internet, "ctnet", "ctnet@mycdma.cn", "vnet.mobi", 3, 2},
    {11, 460, apn_type_internet, "ctnet", "ctnet@mycdma.cn", "vnet.mobi", 3, 2},
    {12, 460, apn_type_internet, "ctwap", "ctwap@mycdma.cn", "vnet.mobi", 3, 2},
    {12, 460, apn_type_xcap, "ctwap", "ctwap@mycdma.cn", "vnet.mobi", 3, 2},
    {12, 460, apn_type_internet, "ctnet", "ctnet@mycdma.cn", "vnet.mobi", 3, 2},
    {12, 460, apn_type_ims, "IMS", "", "", 3, 2},
    {15, 460, apn_type_internet, "cbnet", "", "", 3, 2},
    {15, 460, apn_type_xcap, "cmwap", "", "", 3, 2},
    {15, 460, apn_type_internet, "cbnet", "", "", 3, 2},
    {15, 460, apn_type_ims, "ims", "", "", 3, 2},
};

//工程模式测试用
extern int is_enter_wifi_test;
extern int is_enter_gnss_test;

extern void ble_module_enable(u8 en);
extern u8 is_bredr_close(void);
extern void bredr_conn_last_dev();
extern void bt_init_bredr();
extern void bt_close_bredr();

extern void bt_set_phone_active_start_time_ms(void);
extern u8 get_call_status(void);

extern void *notice_info_create(void);
extern void notice_info_release(void *info);
extern void notice_set_info_from(void *info, void *name, void *data, u16 len);
extern void notice_add_info_from(void *info);

extern void lte_module_launch(void *ip, void *gate, void *mask, void *dns);

extern int TCPIP_netif_init(uint32 mux_net_id);
extern void lte_module_launch(void *ip, void *gate, void *mask, void *dns);

static void *cat1_dev_handle;
static void cat1_get_sys_time(struct sys_time *time)//获取时间
{
    cat1_dev_handle = dev_open("rtc", NULL);
    if (!cat1_dev_handle) {
        return ;
    }
    dev_ioctl(cat1_dev_handle, IOCTL_GET_SYS_TIME, (u32)time);
}

static void number_to_play_list(char *num, u32 *lst)
{
    u8 i = 0;

    if (num) {
        for (; i < strlen(num); i++) {
            lst[i] = num0_9[num[i] - '0'] ;
        }
    }
    lst[i++] = (u32)TONE_REPEAT_BEGIN(-1);
    lst[i++] = (u32)TONE_RING;
    lst[i++] = (u32)TONE_REPEAT_END();
    lst[i++] = (u32)NULL;
}

static void cat1_call_check_func(void *priv)
{
    // 通话状态判断
    int rpc_ret = 0;
    if (ual_tele_call_get_current_call_state(&rpc_ret) == UAL_TELE_CALL_STATE_NULL) {
        if (cat1_info.cat1_call_check_tmr) {
            sys_timer_del(cat1_info.cat1_call_check_tmr);
            cat1_info.cat1_call_check_tmr = 0;
        }
        return ;
    }
    // 准入判断
    if (false == app_cat1_check_support()) {
        return ;
    }

    app_task_switch_to(APP_CAT1_TASK);
    /* play local ring */
    char *len_lst[UAL_TELE_CALL_TELE_NUM_MAX + 4 + 1]; // 电话号码加铃声循环
    tone_play_stop();
    number_to_play_list((char *)(cat1_info.tele_num), len_lst);
    tone_file_list_play(len_lst, 1);

#if TCFG_UI_ENABLE_PHONEBOOK
    set_call_log_date(NULL);
    set_call_log_type(2);

    u8 call_name[PHONEBOOK_NAME_LEN + 1] = {0};
    if (phonebook_get_name_by_number(cat1_info.tele_num, call_name)) {
        set_call_log_name(call_name);
    }
    set_call_log_number((u8 *)cat1_info.tele_num);
#endif
}

static void cat1_gpsinfo_timer_handler(void *priv)
{
    int rpc_ret = 0;
    ual_gnss_read_info(&rpc_ret);
}

static int32_t handleCallCallback(ual_common_msg_t param)
{
    int rpc_ret = 0;
    uint8_t call_number = 0;
    char *len_lst[UAL_TELE_CALL_TELE_NUM_MAX + 4 + 1]; // 电话号码加铃声循环

    if (param.p_body == NULL) {
        log_error("param == NULL\n");
        return false;
    }

    switch (param.msg_id) {
    case MSG_UAL_TELE_CALL_MO_START_IND:      //呼叫发起请求
        ual_tele_call_mo_start_ind_t *call_mo_start = {0};
        call_mo_start = (ual_tele_call_mo_start_ind_t *)(param.p_body);
        cat1_info.call_id = call_mo_start->call_id;
        memcpy(cat1_info.tele_num, call_mo_start->tele_num, sizeof(cat1_info.tele_num));
        log_info("tele:MSG_UAL_TELE_CALL_MO_START_IND\n");
        app_task_switch_to(APP_CAT1_TASK);

#if TCFG_UI_ENABLE_PHONEBOOK
        set_call_log_date(NULL);
#endif
        break;

    case MSG_UAL_TELE_CALL_MO_ALERT_IND:   //播放铃音
        ual_tele_call_mo_alert_ind_t *call_mo_alert_ind = {0};
        call_mo_alert_ind = (ual_tele_call_mo_alert_ind_t *)(param.p_body);
        cat1_info.alert_type = call_mo_alert_ind->alert_type;
        if (cat1_info.alert_type == UAL_TELE_CALL_ALERT_TYPE_LOCAL) {
            number_to_play_list(NULL, len_lst);
            tone_file_list_play(len_lst, 1);
        } else if (cat1_info.alert_type == UAL_TELE_CALL_ALERT_TYPE_REMOTE) {
            app_cat1_audio_start();
        }
        log_info("tele:MSG_UAL_TELE_CALL_MO_ALERT_IND\n");
        break;

    case MSG_UAL_TELE_CALL_MT_START_IND:                  //来电建立请求
        ual_tele_call_mt_start_ind_t *call_mt_start = {0};
        call_mt_start = (ual_tele_call_mt_start_ind_t *)(param.p_body);
        cat1_info.call_id = call_mt_start->call_id;
        memcpy(cat1_info.tele_num, call_mt_start->tele_num, sizeof(cat1_info.tele_num));
        log_info("tele:MSG_UAL_TELE_CALL_MT_START_IND\n");

        call_number = ual_tele_call_get_call_count(&rpc_ret);           //需要获取当前通话数量
        if (call_number > 1) {
            ual_tele_call_release_call(cat1_info.call_id, &rpc_ret);     //挂断当前来电
            return -1;
        } else {
            if (false == app_cat1_check_support()) {
                // 当前条件不容许进入通话时，定时检查状态
                if (cat1_info.cat1_call_check_tmr == NULL) {
                    cat1_info.cat1_call_check_tmr = sys_timer_add(NULL, cat1_call_check_func, 1000);
                }
                break;
            }
            app_task_switch_to(APP_CAT1_TASK);

            /* play local ring */
            number_to_play_list((char *)(cat1_info.tele_num), len_lst);
            tone_file_list_play(len_lst, 1);

#if TCFG_UI_ENABLE_PHONEBOOK
            set_call_log_date(NULL);
            set_call_log_type(2);

            u8 call_name[PHONEBOOK_NAME_LEN + 1] = {0};
            if (phonebook_get_name_by_number(cat1_info.tele_num, call_name)) {
                set_call_log_name(call_name);
            }
            set_call_log_number((u8 *)cat1_info.tele_num);
#endif /* #if TCFG_UI_ENABLE_PHONEBOOK */
#if TCFG_UI_ENABLE_MOTO
            UI_MOTO_RUN(1);
#endif
        }
        break;
    case MSG_UAL_TELE_CALL_MO_CONNECTED_CNF:     //去电建立成功
        ual_tele_call_mo_connected_cnf_t *call_mo_connected = {0};
        call_mo_connected = (ual_tele_call_mo_connected_cnf_t *)(param.p_body);
        cat1_info.call_id = call_mo_connected->call_id;
        memcpy(cat1_info.tele_num, call_mo_connected->tele_num, sizeof(cat1_info.tele_num));
        log_info("tele:MSG_UAL_TELE_CALL_MO_CONNECTED_CNF\n");
        if (APP_CAT1_TASK != app_get_curr_task()) {
            log_error("not cat1 task \n");
            break;
        }
        tone_play_stop();

        app_cat1_audio_start();

        bt_set_phone_active_start_time_ms();
        UI_MSG_POST("phone_active:status=%4", 1);
        break;

    case MSG_UAL_TELE_CALL_MT_CONNECTED_CNF:         //来电建立成功
        ual_tele_call_mt_connected_cnf_t *call_mt_connected = {0};
        call_mt_connected = (ual_tele_call_mt_connected_cnf_t *)(param.p_body);
        cat1_info.call_id = call_mt_connected->call_id;
        memcpy(cat1_info.tele_num, call_mt_connected->tele_num, sizeof(cat1_info.tele_num));
        log_info("tele:MSG_UAL_TELE_CALL_MT_CONNECTED_CNF\n");

        if (APP_CAT1_TASK != app_get_curr_task()) {
            log_error("not cat1 task \n");
            break;
        }
        tone_play_stop();
#if TCFG_UI_ENABLE_MOTO
        UI_MOTO_RUN(0);
#endif
        app_cat1_audio_start();

        bt_set_phone_active_start_time_ms();
        UI_MSG_POST("phone_active:status=%4", 1);
        break;

    case MSG_UAL_TELE_CALL_DISCONNECTED_CNF:      //主动挂断
        ual_tele_call_disconnected_cnf_t *call_disconnected_cnf = {0};
        call_disconnected_cnf = (ual_tele_call_disconnected_cnf_t *)(param.p_body);
        cat1_info.call_id = call_disconnected_cnf->call_id;
        memcpy(cat1_info.tele_num, call_disconnected_cnf->tele_num, sizeof(cat1_info.tele_num));
        log_info("tele:MSG_UAL_TELE_CALL_DISCONNECTED_CNF\n");
        call_number = ual_tele_call_get_call_count(&rpc_ret);
        if (0 == call_number) {
            app_cat1_audio_stop();
        } else {
            log_info("Release the second call, do nothing and return only.\r\n");
            return -1;
        }
        if (cat1_info.cat1_call_check_tmr) {
            sys_timer_del(cat1_info.cat1_call_check_tmr);
            cat1_info.cat1_call_check_tmr = 0;
        }
        if (APP_CAT1_TASK == app_get_curr_task()) {
#if TCFG_UI_ENABLE_PHONEBOOK
            update_call_log_message();
#endif
            app_task_switch_back();
        }
#if TCFG_UI_ENABLE_MOTO
        UI_MOTO_RUN(0);
#endif
        break;

    case MSG_UAL_TELE_CALL_DISCONNECTED_IND:         //被动挂断
        ual_tele_call_disconnected_ind_t *call_disconnected_ind = {0};
        int32_t ret = 0;
        uint32_t rf_ret = 0;
        call_disconnected_ind = (ual_tele_call_disconnected_ind_t *)(param.p_body);
        cat1_info.call_id = call_disconnected_ind->call_id;
        memcpy(cat1_info.tele_num, call_disconnected_ind->tele_num, sizeof(cat1_info.tele_num));

        log_info("tele:MSG_UAL_TELE_CALL_DISCONNECTED_IND\n");

        call_number = ual_tele_call_get_call_count(&rpc_ret);
        if (0 == call_number) {
            app_cat1_audio_stop();
        } else {
            log_info("Release the second call, do nothing and return only.\r\n");
            return -1;
        }
        if (cat1_info.cat1_call_check_tmr) {
            sys_timer_del(cat1_info.cat1_call_check_tmr);
            cat1_info.cat1_call_check_tmr = 0;
        }
        if (APP_CAT1_TASK == app_get_curr_task()) {
#if TCFG_UI_ENABLE_PHONEBOOK
            update_call_log_message();
#endif
            app_task_switch_back();
        }
#if TCFG_UI_ENABLE_MOTO
        UI_MOTO_RUN(0);
#endif
        break;
    default:
        break;
    }

    return true;
}

static int32_t handleRadioCallback(ual_common_msg_t param)
{
    int rpc_ret = 0;
    if (param.p_body == NULL) {
        log_error("param is null\n");
        return false;
    }

    switch (param.msg_id) {
    case MSG_UAL_TELE_RADIO_RSSI_IND:
        ual_tele_radio_rssi_ind_t *radio_rssi_ind = {0};
        radio_rssi_ind = (ual_tele_radio_rssi_ind_t *)(param.p_body);
        log_info("TELE:the signal strength:%d\n", radio_rssi_ind->rssi_level);
        if (cat1_info.sim_is_ready == 0 && cat1_info.ims_is_on == 1 && cat1_info.is_plug_out == 0) {
            cat1_info.signal_level = radio_rssi_ind->rssi_level;
            UI_MSG_POST("cat1_event:signal_level=%4", radio_rssi_ind->rssi_level);
        }
        break;
    case MSG_UAL_TELE_RADIO_IMS_STATE_IND:
        ual_tele_radio_ims_state_ind_t *radio_ims_state_ind = {0};
        radio_ims_state_ind = (ual_tele_radio_ims_state_ind_t *)(param.p_body);
        log_info("TELE:state of ims:%d\n", radio_ims_state_ind->ims_state);
        cat1_info.ims_is_on = radio_ims_state_ind->ims_state;
        break;
    case MSG_UAL_TELE_RADIO_NETWORK_STATUS_IND:
        ual_tele_radio_network_status_ind_t *radio_network_status_ind = {0};
        radio_network_status_ind = (ual_tele_radio_network_status_ind_t *)(param.p_body);
        log_info("TELE:state of plmn_state:%d\n", radio_network_status_ind->plmn_state);
        set_rpc_log_enable(1);
        log_info("TELE:set_rpc_log_enable\n");
        cat1_info.plmn_state = radio_network_status_ind->plmn_state;
        if (cat1_info.sim_is_ready == 0 && cat1_info.ims_is_on != 1) {
            cat1_info.radio_is_on = 1;
            UI_MSG_POST("cat1_event:radio=%4", 0);
        }

        break;
    case MSG_UAL_TELE_RADIO_LTE_CELL_INFO_CNF:
        ual_tele_radio_lte_cell_info_t *lte_cell_info = {0};
        lte_cell_info = (ual_tele_radio_lte_cell_info_t *)(param.p_body);
        log_info("cell info get!!\n");
        memcpy(&serving_cell_info, lte_cell_info, sizeof(ual_tele_radio_lte_cell_info_t));

        log_info("cell_id = %d, earfcn = %d, rsrq = %d, srxlev = %d, bw = %d", serving_cell_info.cell_id.values[0],
                 serving_cell_info.earfcn.values[0], serving_cell_info.rsrq.values[0], serving_cell_info.srxlev.values[0],
                 serving_cell_info.bw.values[0]);

        UI_MSG_POST("cell_info_show");
        break;
    case MSG_UAL_TELE_RADIO_LTE_NEIGH_CELL_INFO_CNF:
        ual_tele_radio_lte_neighbor_cell_info_t *lte_neighbor_cell_info = {0};
        lte_neighbor_cell_info = (ual_tele_radio_lte_neighbor_cell_info_t *)(param.p_body);
        log_info("neigh cell info get!!\n");
        memcpy(&neighbor_cell_info, lte_neighbor_cell_info, sizeof(ual_tele_radio_lte_neighbor_cell_info_t));

        log_info("cell_id = %d, earfcn = %d, rsrq = %d, srxlev = %d", neighbor_cell_info.cell_id,
                 neighbor_cell_info.earfcn, neighbor_cell_info.rsrq, neighbor_cell_info.srxlev);

        UI_MSG_POST("neigh_cell_info_show");
        break;

    }
    return true;
}

static int32_t handleSimCallback(ual_common_msg_t param)
{
    if (param.p_body == NULL) {
        log_error("tele: func:%s, param is null\n", __func__);
    }

    switch (param.msg_id) {
    case MSG_UAL_TELE_SIM_STATUS_IND:
        ual_tele_radio_apn_info_t apn_info = {0};
        int rpc_ret = 0;
        int mnc;
        ual_tele_plmn_info_t tele_plmn_info;
        ual_tele_sim_status_ind_t *sim_status_ind = {0};
        sim_status_ind = (ual_tele_sim_status_ind_t *)(param.p_body);
        log_info("tele: func:%s, state of sim:%d\n", __func__, sim_status_ind->sim_status);
        cat1_info.sim_is_ready = sim_status_ind->sim_status;
        log_info("sim_is_ready:%d\n", cat1_info.sim_is_ready);
        if (cat1_info.sim_is_ready != 0) {
            UI_MSG_POST("cat1_event:radio=%4", 0);
        }

        log_info("set volte!!\n");
        ual_tele_radio_set_volte(0, TRUE, &rpc_ret);

        ual_tele_sim_get_sim_hplmn(0, &tele_plmn_info, &rpc_ret);
        mnc = tele_plmn_info.mnc;

        switch (mnc) {
        case 0:
            memset(&apn_info, 0x00, sizeof(ual_tele_radio_apn_info_t));

            apn_info.internet_apn_info[0].auth_type = apn_list[0].auth_type;
            apn_info.internet_apn_info[0].ip_type = apn_list[0].ip_type;
            memcpy(apn_info.internet_apn_info[0].apn, apn_list[0].apn, strlen(apn_list[0].apn));
            memcpy(apn_info.internet_apn_info[0].password, apn_list[0].password, strlen(apn_list[0].password));
            memcpy(apn_info.internet_apn_info[0].username, apn_list[0].username, strlen(apn_list[0].username));

            apn_info.xcap_apn_info[0].auth_type = apn_list[1].auth_type;
            apn_info.xcap_apn_info[0].ip_type = apn_list[1].ip_type;
            memcpy(apn_info.xcap_apn_info[0].apn, apn_list[1].apn, strlen(apn_list[1].apn));
            memcpy(apn_info.xcap_apn_info[0].password, apn_list[1].password, strlen(apn_list[1].password));
            memcpy(apn_info.xcap_apn_info[0].username, apn_list[1].username, strlen(apn_list[1].username));

            apn_info.ims_apn_info[0].auth_type = apn_list[3].auth_type;
            apn_info.ims_apn_info[0].ip_type = apn_list[3].ip_type;
            memcpy(apn_info.ims_apn_info[0].apn, apn_list[3].apn, strlen(apn_list[3].apn));
            memcpy(apn_info.ims_apn_info[0].password, apn_list[3].password, strlen(apn_list[3].password));
            memcpy(apn_info.ims_apn_info[0].username, apn_list[3].username, strlen(apn_list[3].username));

            ual_tele_radio_set_apn_info(0, &apn_info, &rpc_ret);

            log_info("tele: func:%s, set apncm, internet_apn_info[0].apn:%s\n", __func__, apn_info.internet_apn_info[0].apn);
            log_info("tele: func:%s, set apncm, internet_apn_info[0].username:%s\n", __func__, apn_info.internet_apn_info[0].username);
            log_info("tele: func:%s, set apncm, internet_apn_info[0].password:%s\n", __func__, apn_info.internet_apn_info[0].password);
            log_info("tele: func:%s, set apncm, internet_apn_info[0].ip_type:%d\n", __func__, apn_info.internet_apn_info[0].ip_type);

            break;

        case 1:
            memset(&apn_info, 0x00, sizeof(ual_tele_radio_apn_info_t));

            apn_info.internet_apn_info[0].auth_type = apn_list[4].auth_type;
            apn_info.internet_apn_info[0].ip_type = apn_list[4].ip_type;
            memcpy(apn_info.internet_apn_info[0].apn, apn_list[4].apn, strlen(apn_list[4].apn));
            memcpy(apn_info.internet_apn_info[0].password, apn_list[4].password, strlen(apn_list[4].password));
            memcpy(apn_info.internet_apn_info[0].username, apn_list[4].username, strlen(apn_list[4].username));

            apn_info.xcap_apn_info[0].auth_type = apn_list[5].auth_type;
            apn_info.xcap_apn_info[0].ip_type = apn_list[5].ip_type;
            memcpy(apn_info.xcap_apn_info[0].apn, apn_list[5].apn, strlen(apn_list[5].apn));
            memcpy(apn_info.xcap_apn_info[0].password, apn_list[5].password, strlen(apn_list[5].password));
            memcpy(apn_info.xcap_apn_info[0].username, apn_list[5].username, strlen(apn_list[5].username));

            apn_info.ims_apn_info[0].auth_type = apn_list[7].auth_type;
            apn_info.ims_apn_info[0].ip_type = apn_list[7].ip_type;
            memcpy(apn_info.ims_apn_info[0].apn, apn_list[7].apn, strlen(apn_list[7].apn));
            memcpy(apn_info.ims_apn_info[0].password, apn_list[7].password, strlen(apn_list[7].password));
            memcpy(apn_info.ims_apn_info[0].username, apn_list[7].username, strlen(apn_list[7].username));

            ual_tele_radio_set_apn_info(0, &apn_info, &rpc_ret);

            log_info("tele: func:%s, set apn3g, internet_apn_info[0].apn:%s\n", __func__, apn_info.internet_apn_info[0].apn);
            log_info("tele: func:%s, set apn3g, internet_apn_info[0].username:%s\n", __func__, apn_info.internet_apn_info[0].username);
            log_info("tele: func:%s, set apn3g, internet_apn_info[0].password:%s\n", __func__, apn_info.internet_apn_info[0].password);
            log_info("tele: func:%s, set apn3g, internet_apn_info[0].ip_type:%d\n", __func__, apn_info.internet_apn_info[0].ip_type);

            break;

        case 2:
            memset(&apn_info, 0x00, sizeof(ual_tele_radio_apn_info_t));

            apn_info.internet_apn_info[0].auth_type = apn_list[8].auth_type;
            apn_info.internet_apn_info[0].ip_type = apn_list[8].ip_type;
            memcpy(apn_info.internet_apn_info[0].apn, apn_list[8].apn, strlen(apn_list[8].apn));
            memcpy(apn_info.internet_apn_info[0].password, apn_list[8].password, strlen(apn_list[8].password));
            memcpy(apn_info.internet_apn_info[0].username, apn_list[8].username, strlen(apn_list[8].username));

            apn_info.xcap_apn_info[0].auth_type = apn_list[9].auth_type;
            apn_info.xcap_apn_info[0].ip_type = apn_list[9].ip_type;
            memcpy(apn_info.xcap_apn_info[0].apn, apn_list[9].apn, strlen(apn_list[9].apn));
            memcpy(apn_info.xcap_apn_info[0].password, apn_list[9].password, strlen(apn_list[9].password));
            memcpy(apn_info.xcap_apn_info[0].username, apn_list[9].username, strlen(apn_list[9].username));

            apn_info.ims_apn_info[0].auth_type = apn_list[11].auth_type;
            apn_info.ims_apn_info[0].ip_type = apn_list[11].ip_type;
            memcpy(apn_info.ims_apn_info[0].apn, apn_list[11].apn, strlen(apn_list[11].apn));
            memcpy(apn_info.ims_apn_info[0].password, apn_list[11].password, strlen(apn_list[11].password));
            memcpy(apn_info.ims_apn_info[0].username, apn_list[11].username, strlen(apn_list[11].username));

            ual_tele_radio_set_apn_info(0, &apn_info, &rpc_ret);

            log_info("tele: func:%s, set apncm, internet_apn_info[0].apn:%s\n", __func__, apn_info.internet_apn_info[0].apn);
            log_info("tele: func:%s, set apncm, internet_apn_info[0].username:%s\n", __func__, apn_info.internet_apn_info[0].username);
            log_info("tele: func:%s, set apncm, internet_apn_info[0].password:%s\n", __func__, apn_info.internet_apn_info[0].password);
            log_info("tele: func:%s, set apncm, internet_apn_info[0].ip_type:%d\n", __func__, apn_info.internet_apn_info[0].ip_type);

            break;

        case 3:
            memset(&apn_info, 0x00, sizeof(ual_tele_radio_apn_info_t));

            apn_info.internet_apn_info[0].auth_type = apn_list[12].auth_type;
            apn_info.internet_apn_info[0].ip_type = apn_list[12].ip_type;
            memcpy(apn_info.internet_apn_info[0].apn, apn_list[12].apn, strlen(apn_list[12].apn));
            memcpy(apn_info.internet_apn_info[0].password, apn_list[12].password, strlen(apn_list[12].password));
            memcpy(apn_info.internet_apn_info[0].username, apn_list[12].username, strlen(apn_list[12].username));

            apn_info.xcap_apn_info[0].auth_type = apn_list[13].auth_type;
            apn_info.xcap_apn_info[0].ip_type = apn_list[13].ip_type;
            memcpy(apn_info.xcap_apn_info[0].apn, apn_list[13].apn, strlen(apn_list[13].apn));
            memcpy(apn_info.xcap_apn_info[0].password, apn_list[13].password, strlen(apn_list[13].password));
            memcpy(apn_info.xcap_apn_info[0].username, apn_list[13].username, strlen(apn_list[13].username));

            ual_tele_radio_set_apn_info(0, &apn_info, &rpc_ret);

            log_info("tele: func:%s, set apnct, internet_apn_info[0].apn:%s\n", __func__, apn_info.internet_apn_info[0].apn);
            log_info("tele: func:%s, set apnct, internet_apn_info[0].username:%s\n", __func__, apn_info.internet_apn_info[0].username);
            log_info("tele: func:%s, set apnct, internet_apn_info[0].password:%s\n", __func__, apn_info.internet_apn_info[0].password);
            log_info("tele: func:%s, set apnct, internet_apn_info[0].ip_type:%d\n", __func__, apn_info.internet_apn_info[0].ip_type);

            break;

        case 4:
            memset(&apn_info, 0x00, sizeof(ual_tele_radio_apn_info_t));

            apn_info.internet_apn_info[0].auth_type = apn_list[15].auth_type;
            apn_info.internet_apn_info[0].ip_type = apn_list[15].ip_type;
            memcpy(apn_info.internet_apn_info[0].apn, apn_list[15].apn, strlen(apn_list[15].apn));
            memcpy(apn_info.internet_apn_info[0].password, apn_list[15].password, strlen(apn_list[15].password));
            memcpy(apn_info.internet_apn_info[0].username, apn_list[15].username, strlen(apn_list[15].username));

            ual_tele_radio_set_apn_info(0, &apn_info, &rpc_ret);

            log_info("tele: func:%s, set apncm, internet_apn_info[0].apn:%s\n", __func__, apn_info.internet_apn_info[0].apn);
            log_info("tele: func:%s, set apncm, internet_apn_info[0].username:%s\n", __func__, apn_info.internet_apn_info[0].username);
            log_info("tele: func:%s, set apncm, internet_apn_info[0].password:%s\n", __func__, apn_info.internet_apn_info[0].password);
            log_info("tele: func:%s, set apncm, internet_apn_info[0].ip_type:%d\n", __func__, apn_info.internet_apn_info[0].ip_type);

            break;

        case 6:
            memset(&apn_info, 0x00, sizeof(ual_tele_radio_apn_info_t));

            apn_info.internet_apn_info[0].auth_type = apn_list[16].auth_type;
            apn_info.internet_apn_info[0].ip_type = apn_list[16].ip_type;
            memcpy(apn_info.internet_apn_info[0].apn, apn_list[16].apn, strlen(apn_list[16].apn));
            memcpy(apn_info.internet_apn_info[0].password, apn_list[16].password, strlen(apn_list[16].password));
            memcpy(apn_info.internet_apn_info[0].username, apn_list[16].username, strlen(apn_list[16].username));

            ual_tele_radio_set_apn_info(0, &apn_info, &rpc_ret);

            log_info("tele: func:%s, set apncm, internet_apn_info[0].apn:%s\n", __func__, apn_info.internet_apn_info[0].apn);
            log_info("tele: func:%s, set apncm, internet_apn_info[0].username:%s\n", __func__, apn_info.internet_apn_info[0].username);
            log_info("tele: func:%s, set apncm, internet_apn_info[0].password:%s\n", __func__, apn_info.internet_apn_info[0].password);
            log_info("tele: func:%s, set apncm, internet_apn_info[0].ip_type:%d\n", __func__, apn_info.internet_apn_info[0].ip_type);

            break;

        case 7:
            memset(&apn_info, 0x00, sizeof(ual_tele_radio_apn_info_t));

            apn_info.internet_apn_info[0].auth_type = apn_list[18].auth_type;
            apn_info.internet_apn_info[0].ip_type = apn_list[18].ip_type;
            memcpy(apn_info.internet_apn_info[0].apn, apn_list[18].apn, strlen(apn_list[18].apn));
            memcpy(apn_info.internet_apn_info[0].password, apn_list[18].password, strlen(apn_list[18].password));
            memcpy(apn_info.internet_apn_info[0].username, apn_list[18].username, strlen(apn_list[18].username));

            ual_tele_radio_set_apn_info(0, &apn_info, &rpc_ret);

            log_info("tele: func:%s, set apncm, internet_apn_info[0].apn:%s\n", __func__, apn_info.internet_apn_info[0].apn);
            log_info("tele: func:%s, set apncm, internet_apn_info[0].username:%s\n", __func__, apn_info.internet_apn_info[0].username);
            log_info("tele: func:%s, set apncm, internet_apn_info[0].password:%s\n", __func__, apn_info.internet_apn_info[0].password);
            log_info("tele: func:%s, set apncm, internet_apn_info[0].ip_type:%d\n", __func__, apn_info.internet_apn_info[0].ip_type);

            break;

        case 8:
            memset(&apn_info, 0x00, sizeof(ual_tele_radio_apn_info_t));

            apn_info.internet_apn_info[0].auth_type = apn_list[21].auth_type;
            apn_info.internet_apn_info[0].ip_type = apn_list[21].ip_type;
            memcpy(apn_info.internet_apn_info[0].apn, apn_list[21].apn, strlen(apn_list[21].apn));
            memcpy(apn_info.internet_apn_info[0].password, apn_list[21].password, strlen(apn_list[21].password));
            memcpy(apn_info.internet_apn_info[0].username, apn_list[21].username, strlen(apn_list[21].username));

            ual_tele_radio_set_apn_info(0, &apn_info, &rpc_ret);

            log_info("tele: func:%s, set apncm, internet_apn_info[0].apn:%s\n", __func__, apn_info.internet_apn_info[0].apn);
            log_info("tele: func:%s, set apncm, internet_apn_info[0].username:%s\n", __func__, apn_info.internet_apn_info[0].username);
            log_info("tele: func:%s, set apncm, internet_apn_info[0].password:%s\n", __func__, apn_info.internet_apn_info[0].password);
            log_info("tele: func:%s, set apncm, internet_apn_info[0].ip_type:%d\n", __func__, apn_info.internet_apn_info[0].ip_type);

            break;

        case 9:
            memset(&apn_info, 0x00, sizeof(ual_tele_radio_apn_info_t));

            apn_info.internet_apn_info[0].auth_type = apn_list[24].auth_type;
            apn_info.internet_apn_info[0].ip_type = apn_list[24].ip_type;
            memcpy(apn_info.internet_apn_info[0].apn, apn_list[24].apn, strlen(apn_list[24].apn));
            memcpy(apn_info.internet_apn_info[0].password, apn_list[24].password, strlen(apn_list[24].password));
            memcpy(apn_info.internet_apn_info[0].username, apn_list[24].username, strlen(apn_list[24].username));

            ual_tele_radio_set_apn_info(0, &apn_info, &rpc_ret);

            log_info("tele: func:%s, set apncm, internet_apn_info[0].apn:%s\n", __func__, apn_info.internet_apn_info[0].apn);
            log_info("tele: func:%s, set apncm, internet_apn_info[0].username:%s\n", __func__, apn_info.internet_apn_info[0].username);
            log_info("tele: func:%s, set apncm, internet_apn_info[0].password:%s\n", __func__, apn_info.internet_apn_info[0].password);
            log_info("tele: func:%s, set apncm, internet_apn_info[0].ip_type:%d\n", __func__, apn_info.internet_apn_info[0].ip_type);

            break;

        case 11:
            memset(&apn_info, 0x00, sizeof(ual_tele_radio_apn_info_t));

            apn_info.internet_apn_info[0].auth_type = apn_list[27].auth_type;
            apn_info.internet_apn_info[0].ip_type = apn_list[27].ip_type;
            memcpy(apn_info.internet_apn_info[0].apn, apn_list[27].apn, strlen(apn_list[27].apn));
            memcpy(apn_info.internet_apn_info[0].password, apn_list[27].password, strlen(apn_list[27].password));
            memcpy(apn_info.internet_apn_info[0].username, apn_list[27].username, strlen(apn_list[27].username));

            ual_tele_radio_set_apn_info(0, &apn_info, &rpc_ret);

            log_info("tele: func:%s, set apncm, internet_apn_info[0].apn:%s\n", __func__, apn_info.internet_apn_info[0].apn);
            log_info("tele: func:%s, set apncm, internet_apn_info[0].username:%s\n", __func__, apn_info.internet_apn_info[0].username);
            log_info("tele: func:%s, set apncm, internet_apn_info[0].password:%s\n", __func__, apn_info.internet_apn_info[0].password);
            log_info("tele: func:%s, set apncm, internet_apn_info[0].ip_type:%d\n", __func__, apn_info.internet_apn_info[0].ip_type);

            break;

        case 12:
            memset(&apn_info, 0x00, sizeof(ual_tele_radio_apn_info_t));

            apn_info.internet_apn_info[0].auth_type = apn_list[29].auth_type;
            apn_info.internet_apn_info[0].ip_type = apn_list[29].ip_type;
            memcpy(apn_info.internet_apn_info[0].apn, apn_list[29].apn, strlen(apn_list[29].apn));
            memcpy(apn_info.internet_apn_info[0].password, apn_list[29].password, strlen(apn_list[29].password));
            memcpy(apn_info.internet_apn_info[0].username, apn_list[29].username, strlen(apn_list[29].username));

            apn_info.xcap_apn_info[0].auth_type = apn_list[30].auth_type;
            apn_info.xcap_apn_info[0].ip_type = apn_list[30].ip_type;
            memcpy(apn_info.xcap_apn_info[0].apn, apn_list[30].apn, strlen(apn_list[30].apn));
            memcpy(apn_info.xcap_apn_info[0].password, apn_list[30].password, strlen(apn_list[30].password));
            memcpy(apn_info.xcap_apn_info[0].username, apn_list[30].username, strlen(apn_list[30].username));

            apn_info.ims_apn_info[0].auth_type = apn_list[32].auth_type;
            apn_info.ims_apn_info[0].ip_type = apn_list[32].ip_type;
            memcpy(apn_info.ims_apn_info[0].apn, apn_list[32].apn, strlen(apn_list[32].apn));
            memcpy(apn_info.ims_apn_info[0].password, apn_list[32].password, strlen(apn_list[32].password));
            memcpy(apn_info.ims_apn_info[0].username, apn_list[32].username, strlen(apn_list[32].username));

            ual_tele_radio_set_apn_info(0, &apn_info, &rpc_ret);

            log_info("tele: func:%s, set apncm, internet_apn_info[0].apn:%s\n", __func__, apn_info.internet_apn_info[0].apn);
            log_info("tele: func:%s, set apncm, internet_apn_info[0].username:%s\n", __func__, apn_info.internet_apn_info[0].username);
            log_info("tele: func:%s, set apncm, internet_apn_info[0].password:%s\n", __func__, apn_info.internet_apn_info[0].password);
            log_info("tele: func:%s, set apncm, internet_apn_info[0].ip_type:%d\n", __func__, apn_info.internet_apn_info[0].ip_type);

            break;

        case 15:
            memset(&apn_info, 0x00, sizeof(ual_tele_radio_apn_info_t));

            apn_info.internet_apn_info[0].auth_type = apn_list[33].auth_type;
            apn_info.internet_apn_info[0].ip_type = apn_list[33].ip_type;
            memcpy(apn_info.internet_apn_info[0].apn, apn_list[33].apn, strlen(apn_list[33].apn));
            memcpy(apn_info.internet_apn_info[0].password, apn_list[33].password, strlen(apn_list[33].password));
            memcpy(apn_info.internet_apn_info[0].username, apn_list[33].username, strlen(apn_list[33].username));

            apn_info.xcap_apn_info[0].auth_type = apn_list[34].auth_type;
            apn_info.xcap_apn_info[0].ip_type = apn_list[34].ip_type;
            memcpy(apn_info.xcap_apn_info[0].apn, apn_list[34].apn, strlen(apn_list[34].apn));
            memcpy(apn_info.xcap_apn_info[0].password, apn_list[34].password, strlen(apn_list[34].password));
            memcpy(apn_info.xcap_apn_info[0].username, apn_list[34].username, strlen(apn_list[34].username));

            apn_info.ims_apn_info[0].auth_type = apn_list[36].auth_type;
            apn_info.ims_apn_info[0].ip_type = apn_list[36].ip_type;
            memcpy(apn_info.ims_apn_info[0].apn, apn_list[36].apn, strlen(apn_list[36].apn));
            memcpy(apn_info.ims_apn_info[0].password, apn_list[36].password, strlen(apn_list[36].password));
            memcpy(apn_info.ims_apn_info[0].username, apn_list[36].username, strlen(apn_list[36].username));

            ual_tele_radio_set_apn_info(0, &apn_info, &rpc_ret);

            log_info("tele: func:%s, set apncm, internet_apn_info[0].apn:%s\n", __func__, apn_info.internet_apn_info[0].apn);
            log_info("tele: func:%s, set apncm, internet_apn_info[0].username:%s\n", __func__, apn_info.internet_apn_info[0].username);
            log_info("tele: func:%s, set apncm, internet_apn_info[0].password:%s\n", __func__, apn_info.internet_apn_info[0].password);
            log_info("tele: func:%s, set apncm, internet_apn_info[0].ip_type:%d\n", __func__, apn_info.internet_apn_info[0].ip_type);

            break;

        default:
            log_info("no operators!!\n");
        }
        break;

    case MSG_UAL_TELE_SIM_PLUG_OUT_IND:
        log_info("PLUG OUT!!");
        cat1_info.is_plug_out = 1;
        cat1_info.signal_level = 0;
        call_ctrl_set_call_sel(CALL_SEL_BT);
        break;
    case MSG_UAL_TELE_SIM_PLUG_IN_IND:
        log_info("PLUG IN");
        cat1_info.is_plug_out = 0;
        call_ctrl_set_call_sel(CALL_SEL_CAT1);
        break;
    }
    return true;
}

static int32_t handleWifiCallback(ual_common_msg_t param)
{
    int rpc_ret = 0;
    if (param.p_body == NULL) {
        log_error("param is null\n");
    }

    switch (param.msg_id) {
    case MSG_UAL_WIFI_OPEN_CNF:     //wifi打开成功消息
        ual_wifi_open_cnf_t *wifi_open_cnf;
        wifi_open_cnf = (ual_wifi_open_cnf_t *)(param.p_body);

        log_info("param.msg_id is %d\n", param.msg_id);
        if (wifi_open_cnf->is_ok == 0) {
            log_error("wifi open fail!\n");
            return 0;
        } else {
            uint8 i = 0;
            log_info("wifi open success!\n");
            ual_wifi_scan_req_param_t wifi_scan_param;
            wifi_scan_param.scan_mode = UAL_WIFI_SCAN_BY_CHANNEL;
            wifi_scan_param.ual_wifi_scan_param_u.ual_wifi_scan_by_channle_t.nChNum = UAL_WIFI_CHANNEL_NUMBER;
            wifi_scan_param.ual_wifi_scan_param_u.ual_wifi_scan_by_channle_t.nMaxNodeNum = 2;
            for (i = 0; i < UAL_WIFI_CHANNEL_NUMBER; i++) {
                wifi_scan_param.ual_wifi_scan_param_u.ual_wifi_scan_by_channle_t.nch[i] = i + 1;
            }
            ual_wifi_start_scan(&wifi_scan_param, &rpc_ret);
        }
        break;
    case MSG_UAL_WIFI_START_SCAN_CNF:          //wifi扫描成功消息
        ual_wifi_start_scan_cnf_t *wifi_start_scan_cnf;
        wifi_start_scan_cnf = (ual_wifi_start_scan_cnf_t *)(param.p_body);

        log_info("param.msg_id is %d\n", param.msg_id);

        if (wifi_start_scan_cnf->is_ok == 0) {
            log_error("wifi start scan fail!\n");
            return 0;
        } else {
            log_info("wifi start scan success!\n");
        }
        break;
    case MSG_UAL_WIFI_SCAN_AP_INFO_IND:            //上报APnode，获取AP节点信息

        log_info("AP info get!!\n");
        log_info("param.msg_id is %d\n", param.msg_id);

        break;
    case MSG_UAL_WIFI_SCAN_AP_INFO_CNF:         //结束扫描信息
        ual_wifi_scan_ap_info_t ap_list[30] = {0};
        uint32 index = 0;

        log_info("wifi info get!!\n");
        log_info("param.msg_id is %d\n", param.msg_id);
        printf("wifi:sizeof(ual_wifi_scan_ap_info_t):%d\n", 30 * sizeof(ual_wifi_scan_ap_info_t));
        if (0 != ual_wifi_get_scaned_ap_list(ap_list, 30 * sizeof(ual_wifi_scan_ap_info_t), &rpc_ret)) {
            log_error("get scan ap list error\n");
        } else {
            for (index = 0; index < sizeof(ap_list) / sizeof(ual_wifi_scan_ap_info_t); index ++) {
                char bssid_char[25] = {0};
                char rssid_char[15] = {0};
                log_info("ap node:%d", index);
                log_info("MAC:%02x:%02x:%02x:%02x:%02x:%02x", ap_list[index].bssid_info.bssid[5], ap_list[index].bssid_info.bssid[4],
                         ap_list[index].bssid_info.bssid[3], ap_list[index].bssid_info.bssid[2],
                         ap_list[index].bssid_info.bssid[1], ap_list[index].bssid_info.bssid[0]);
                log_info("rssi:%02d db", ap_list[index].rssival.rssival);
            }
        }
        snprintf(wifi_info.mac_addr, 20, "%02x:%02x:%02x:%02x:%02x:%02x", ap_list[0].bssid_info.bssid[5], ap_list[0].bssid_info.bssid[4],
                 ap_list[0].bssid_info.bssid[3], ap_list[0].bssid_info.bssid[2],
                 ap_list[0].bssid_info.bssid[1], ap_list[0].bssid_info.bssid[0]);
        wifi_info.rssi = ap_list[0].rssival.rssival;
        log_info("wifi rssi:%d\n", wifi_info.rssi);
        for (int i = 6; i > 0; i--) {
            log_info("%02x ", ap_list[0].bssid_info.bssid[i]);
        }

        UI_MSG_POST("wifi_show");      //往ui线程发送消息

        log_info("wifi scan finish!\n");
        ual_wifi_close(&rpc_ret);
        break;
    case MSG_UAL_WIFI_STOP_SCAN_CNF:            //关闭扫描信息
        ual_wifi_stop_scan_cnf_t *wifi_stop_scan_cnf;
        wifi_stop_scan_cnf = (ual_wifi_stop_scan_cnf_t *)(param.p_body);

        log_info("param.msg_id is %d\n", param.msg_id);

        if (wifi_stop_scan_cnf->is_ok == 0) {
            log_error("wifi stop scan fail!\n");
            return 0;
        } else {
            log_info("wifi stop scan success!\n");
            ual_wifi_close(&rpc_ret);
        }
        break;
    case MSG_UAL_WIFI_CLOSE_CNF:
        log_info("param.msg_id is %d\n", param.msg_id);
        log_info("wifi close success!\n");
        break;
    }
    return true;
}

static int32_t handleGPSCallback(ual_common_msg_t param)
{
    int rpc_ret = 0;
    if (param.p_body == NULL) {
        log_error("handleGPSCallback param is null\n");
    }

    switch (param.msg_id) {
    case MSG_UAL_GNSS_START_CNF:                   //开始定位消息
        ual_gnss_start_cnf_t *gnss_start_cnf = {0};
        gnss_start_cnf = (ual_gnss_start_cnf_t *)(param.p_body);

        log_info("param.msg_id is %d\n", param.msg_id);

        log_info("MSG_UAL_GNSS_START_CNF\n");
        if (gnss_start_cnf->is_ok == 0) {
            log_error("GPS start fail!\n");
            return 0;
        } else {
            ual_gnss_read_info(&rpc_ret);
            gnss_timer = sys_timer_add(NULL, cat1_gpsinfo_timer_handler, 1000);
            log_info("GPS start success!\n");
        }
        break;

    case MSG_UAL_GNSS_READ_INFO_CNF:              //读取定位信息
        ual_gnss_read_info_cnf_t *gnss_read_info_cnf;
        gnss_read_info_cnf = (ual_gnss_read_info_cnf_t *)(param.p_body);

        log_info("param.msg_id is %d\n", param.msg_id);
        log_info("read info success!\n");
        gps_info.longitude = gnss_read_info_cnf->gps_info.longitude;
        gps_info.latitude = gnss_read_info_cnf->gps_info.latitude;
        gps_info.height = gnss_read_info_cnf->gps_info.altitude;
        gps_info.ttff_time = gnss_read_info_cnf->gps_info.ttff_time;
        gps_info.satellite_num = gnss_read_info_cnf->satellite_list_info.satellite_num;
        for (int i = 0; i < gps_info.satellite_num; i++) {
            gps_info.satellite_info[i].satellite_identifier = gnss_read_info_cnf->satellite_list_info.satellite_info[i].satellite_identifier;
            gps_info.satellite_info[i].cn0 = gnss_read_info_cnf->satellite_list_info.satellite_info[i].cn0;
            gps_info.satellite_info[i].elevation = gnss_read_info_cnf->satellite_list_info.satellite_info[i].elevation;
            gps_info.satellite_info[i].azimuth = gnss_read_info_cnf->satellite_list_info.satellite_info[i].azimuth;
            gps_info.satellite_info[i].is_used = gnss_read_info_cnf->satellite_list_info.satellite_info[i].is_used;
        }

        log_info("longitude:%f\n", gps_info.longitude);
        log_info("latitude:%f\n", gps_info.latitude);
        log_info("height:%f\n", gps_info.height);
        log_info("ttff_time:%f\n", gps_info.ttff_time);
        log_info("satellite_num:%f\n", gps_info.satellite_num);

        UI_MSG_POST("gps_show");      //往ui线程发送消息

        break;
    case MSG_UAL_GNSS_STOP_CNF:
        ual_gnss_stop_cnf_t *gnss_stop_cnf;
        gnss_stop_cnf = (ual_gnss_stop_cnf_t *)(param.p_body);

        log_info("param.msg_id is %d\n", param.msg_id);
        if (gnss_stop_cnf->is_ok == 0) {
            log_info("gnss stop fail!\n");
            return 0;
        } else {
            log_info("gnss stop success!\n");
            ual_gnss_poweroff(&rpc_ret);
        }

    case MSG_UAL_GNSS_POWEROFF_CNF:
        ual_gnss_poweroff_cnf_t *gnss_poweroff_cnf;
        gnss_poweroff_cnf = (ual_gnss_poweroff_cnf_t *)(param.p_body);

        log_info("param.msg_id is %d\n", param.msg_id);
        if (gnss_poweroff_cnf->is_ok == 0) {
            log_error("gnss poweroff fail!\n");
            return 0;
        } else {
            log_info("gnss poweroff success!\n");
        }
        break;
    }

    return true;
}

static int32_t handleSMSCallback(ual_common_msg_t param)
{
    int rpc_ret = 0;

    if (param.p_body == NULL) {
        log_error("param is null!\n");
        return false;
    }

    switch (param.msg_id) {
    case MSG_UAL_TELE_SMS_RECEIVE_IND:            //获取信息内容
        ual_tele_sms_receive_ind_t *sms_receive_ind;
        sms_receive_ind = (ual_tele_sms_receive_ind_t *)(param.p_body);

        log_info("SMS RECEIVE!!\n");

        char content[UAL_TELE_SMS_CONTENT_MAX_LEN];
        int len = Unicode2UTF8(content, sms_receive_ind->content, (sms_receive_ind->content_len) * 2);
        content[len] = '\0';

        int utc_time = sms_receive_ind->time;

        void *msg = notice_info_create();
        if (msg) {
            notice_set_info_from(msg, "UID", &utc_time, sizeof(utc_time));
            notice_set_info_from(msg, "AppIdentifier", "MobileSMS_RECEIVE", sizeof("MobileSMS_RECEIVE"));
            notice_set_info_from(msg, "IDTitle", sms_receive_ind->sender_address, strlen(sms_receive_ind->sender_address) + 1);
            notice_set_info_from(msg, "IDMessage", content, strlen(content) + 1);
            notice_add_info_from(msg);
            notice_info_release(msg);
        }
        break;
    case MSG_UAL_TELE_SMS_SEND_CNF:          //发送短信
        ual_tele_sms_report_ind_t *tele_sms_report_ind;
        tele_sms_report_ind = (ual_tele_sms_report_ind_t *)(param.p_body);
        log_info("msg send status:%d\n", tele_sms_report_ind->result);
        log_info("MSG_UAL_TELE_SMS_SEND_CNF\n");

        if (tele_sms_report_ind->result == 0) {
            void *msg = notice_info_create();
            if (msg) {
                struct sys_time curtime;
                cat1_get_sys_time(&curtime);
                u32 TimeStamp = timestamp_mytime_2_utc_sec(&curtime);
                log_info("timestamp:%d\n", TimeStamp);
                notice_set_info_from(msg, "UID", &TimeStamp, sizeof(TimeStamp));
                notice_set_info_from(msg, "AppIdentifier", "MobileSMS_SEND", sizeof("MobileSMS_SEND"));
                notice_set_info_from(msg, "IDTitle", cat1_get_send_sms_number(), strlen(cat1_get_send_sms_number()) + 1);
                notice_set_info_from(msg, "IDMessage", cat1_get_send_sms_content(), strlen(cat1_get_send_sms_content()) + 1);
                notice_add_info_from(msg);
                notice_info_release(msg);
            }
            clear_send_sms();
            break;
        }
    }
    return true;
}

static int32_t handleDataCallback(ual_common_msg_t param)
{
    ual_tele_data_pdp_net_para net_para = {0};
    if (param.p_body == NULL) {
        log_error("Param is null!\n");
    }

    switch (param.msg_id) {
    case MSG_UAL_TELE_DATA_ACTIVE_CNF:
        log_info("data_active!\n");

        if (cat1_info.not_first_cnf) {
            break;
        }

        ual_tele_data_active_cnf_t *tele_data_active_cnf;
        tele_data_active_cnf = uos_malloc(sizeof(tele_data_active_cnf));
        if (tele_data_active_cnf == NULL) {
            log_error("tele_data_active_cnf malloc failed!\n");
            break;
        }
        uos_memset(tele_data_active_cnf, 0, sizeof(tele_data_active_cnf));
        tele_data_active_cnf = (ual_tele_data_active_cnf_t *)(param.p_body);
        net_para.is_ip4 = tele_data_active_cnf->net_para.is_ip4;
        net_para.is_ip6 = tele_data_active_cnf->net_para.is_ip6;
        net_para.net_id = tele_data_active_cnf->net_para.net_id;
        net_para.mux_net_id = tele_data_active_cnf->reserved;
        net_para.ipaddr = tele_data_active_cnf->net_para.ipaddr;
        net_para.snmask = tele_data_active_cnf->net_para.snmask;
        net_para.dns1 = tele_data_active_cnf->net_para.dns1;
        net_para.dns2 = tele_data_active_cnf->net_para.dns2;
        net_para.gateway = tele_data_active_cnf->net_para.gateway;
        uos_memcpy(net_para.ip6addr, tele_data_active_cnf->net_para.ip6addr, UAL_TELE_DATA_IPV6_LEN);
        uos_memcpy(net_para.dns6_pri, tele_data_active_cnf->net_para.dns6_pri, UAL_TELE_DATA_IPV6_LEN);
        uos_memcpy(net_para.dns6_sec, tele_data_active_cnf->net_para.dns6_sec, UAL_TELE_DATA_IPV6_LEN);

        if (tele_data_active_cnf->result == 0) {
            log_info("TCPIP_netif_create start \n");
            TCPIP_netif_init(net_para.mux_net_id);
            lte_module_launch(&net_para.ipaddr, &net_para.gateway, &net_para.snmask, &net_para.dns1);
        }
        log_info("data:net_id: %d\n", tele_data_active_cnf->net_para.net_id);
        uos_free(tele_data_active_cnf);

        cat1_info.not_first_cnf = TRUE;

        break;
    case MSG_UAL_TELE_DATA_DEACTIVE_CNF:
        ual_tele_data_deactive_cnf_t *tele_data_deactive_cnf;
        tele_data_deactive_cnf = (ual_tele_data_deactive_cnf_t *)(param.p_body);
        log_info("deactivate: %d\n", tele_data_deactive_cnf->result);

        break;
    }
    return true;
}

uos_uint32_t _callback_ModemCtrl(uos_uint32_t size)
{
    log_info("_callback_ModemCtrl!!!!\n");

    switch (size) {
    case MODEM_STATE_ALIVE:
        log_info("_callback_ModemCtrl alive size = %d", 1, size);
        return 0;
    case MODEM_STATE_ASSERT:
        log_info("_callback_ModemCtrl assert size = %d", 1, size);
        return 0;
    case MODEM_STATE_RESET:
        log_info("_callback_ModemCtrl reset size = %d", 1, size);
        return 0;
    case MODEM_STATE_POWERON:
        cat1_info.power_on = 1;
        cat1_set_lte_can_touch(1);
        cat1_set_flight_can_touch(1);
        log_info("_callback_ModemCtrl POWERON size = %d", 1, size);
        UI_MSG_POST("cat1_event:power=%4", 1);
        return 0;
    case MODEM_STATE_POWEROFF:
        cat1_info.power_on = 0;
        cat1_set_lte_can_touch(1);
        cat1_set_flight_can_touch(1);
        log_info("_callback_ModemCtrl POWEROFF size = %d", 1, size);
        cat1_info.signal_level = -1;
        UI_MSG_POST("cat1_event:power=%4", 0);
        return 0;
    case MODEM_STATE_POWERONPROGRESS:
        log_info("_callback_ModemCtrl POWERONPROGRESS size = %d", 1, size);
        return 0;
    case MODEM_STATE_POWEROFFPROGRESS:
        log_info("_callback_ModemCtrl POWERONPROGRESS size = %d", 1, size);
        return 0;
    default:
        return 0;
    }
}

static int32_t handleModemCallback(void)
{
    log_info("%s \n", __func__);

    int rpc_ret = 0;

    //注册call service
    int32_t call_handle = 0;
    int32_t radio_handle = 0;
    int32_t sim_handle = 0;
    int32_t wifi_handle = 0;
    int32_t gps_handle = 0;
    int32_t sms_handle = 0;
    int32_t data_handle = 0;

    //注册call_service
    ual_tele_call_register(handleCallCallback, &cat1_info.client_handle, &rpc_ret);
    //注册radio_service
    ual_tele_radio_register(handleRadioCallback, &radio_handle, &rpc_ret);
    //注册sim_service
    ual_tele_sim_register(handleSimCallback, &sim_handle, &rpc_ret);

    //注册短信
    ual_tele_sms_register(handleSMSCallback, &sms_handle, &rpc_ret);

    //注册wifi_scan
    ual_wifi_register(handleWifiCallback, &wifi_handle, &rpc_ret);
    //注册gps
    ual_gnss_register(handleGPSCallback, &gps_handle, &rpc_ret);
    //注册数据
    ual_tele_data_register(handleDataCallback, &data_handle, &rpc_ret);

    //sim卡上电
    ual_tele_sim_power_on(SIM_ID_1, true, &rpc_ret);

    //调用AT通道
    at_channel_open();

    return 0;
}

bool cat1_call_answer(void)
{
    int rpc_ret = 0;
    int32_t ret = ual_tele_call_answer_call(&rpc_ret);
    if (ret == UAL_TELE_CALL_RES_ERROR) {
        log_error("ual_tele_call_answer_call error:%d\n", ret);
        return false;
    }
    return true;
}

bool cat1_call_hangup(void)
{
    int rpc_ret = 0;
    int32_t status = ual_tele_call_get_current_call_state(&rpc_ret);
    log_info("%s,%d \n", __func__, __LINE__);
    log_info("call state is %d\n", status);
    int32_t ret;
    if (status == UAL_TELE_CALL_STATE_INCOMING) {
        ret = ual_tele_call_reject_call(&rpc_ret);           //拒接电话
        if (ret == UAL_TELE_CALL_RES_ERROR) {
            log_error("ual_tele_call_reject_call error:%d\n", ret);
            return false;
        }
    } else if (status == UAL_TELE_CALL_STATE_NULL) {}
    else if (status == UAL_TELE_CALL_STATE_OUTGOING || status == UAL_TELE_CALL_STATE_ACTIVE) {
        ual_tele_call_data_info_t *current_call_info = NULL;                //获取当前通话info
        current_call_info = malloc(sizeof(ual_tele_call_data_info_t));
        ual_tele_call_get_current_call_info(current_call_info, &rpc_ret);

        ret = ual_tele_call_release_call(current_call_info->call_id, rpc_ret);        //挂断当前电话
        if (ret == UAL_TELE_CALL_RES_ERROR) {
            log_error("ual_tele_call_release_call error:%d\n", ret);
            free(current_call_info);
            return false;
        }
        free(current_call_info);
    }

    return true;
}

bool cat1_call_number(int num_len, char *number)
{
    int rpc_ret = 0;
    if (num_len > UAL_TELE_CALL_TELE_NUM_MAX) {
        num_len = UAL_TELE_CALL_TELE_NUM_MAX;
    }
    int32_t ret = ual_tele_call_make_call(0, number, &rpc_ret);         //需要卡槽号dual_sys
    if (ret == UAL_TELE_CALL_RES_SUCCESS) {
        log_info("ual_tele_call_make_call success:%d\n", ret);
        return true;
    }
    return false;
}

bool cat1_check_call_enable(void)
{
    if (cat1_info.ims_is_on == UAL_TELE_RADIO_IMS_STATE_REGISTERED) {
        return true;
    }
    if (cat1_info.is_plug_out == 1) {
        return false;
    }
    return false;
}

bool cat1_checkout_online(void)
{
    if (cat1_info.ims_is_on == UAL_TELE_RADIO_IMS_STATE_REGISTERED) {
        return true;
    }
    return false;
}

bool cat1_get_call_status(u8 *status)
{
    int rpc_ret = 0;
    if (false == cat1_checkout_online()) {
        return false;
    }
    //获取当前通话状态
    int32_t state = ual_tele_call_get_current_call_state(&rpc_ret);
    switch (state) {
    case UAL_TELE_CALL_STATE_ACTIVE:
    case UAL_TELE_CALL_STATE_HOLD:
        *status = BT_CALL_ACTIVE;
        break;
    case UAL_TELE_CALL_STATE_OUTGOING:
        *status = BT_CALL_OUTGOING;
        break;
    case UAL_TELE_CALL_STATE_INCOMING:
        *status = BT_CALL_INCOMING;
        break;
    case UAL_TELE_CALL_STATE_NULL:
    default:
        *status = BT_CALL_HANGUP;
        break;
    }

    return true;
}

bool cat1_get_call_phone_num(char **phone_num)
{
    *phone_num = cat1_info.tele_num;

    return true;
}

void cat1_audio_close(void)
{
    IIS_close();
}

int cat1_audio_open(void)
{
    iis_open();
    return true;
}

void cat1_call_input_mute(u8 mute)
{
    if (mute) {
        set_mic_mute();
        cat1_info.is_mic_mute = 1;
    } else {
        clear_mic_mute();
        cat1_info.is_mic_mute = 0;
    }
}

int cat1_call_get_input_mute_status(void)
{
    return cat1_info.is_mic_mute;
}

int cat1_get_signal_level(void)
{
    if (cat1_info.is_plug_out == 1) {
        return 0;
    }
    if (cat1_info.radio_is_on) {
        if (cat1_info.signal_level > 4) {
            return cat1_info.signal_level - 1;
        } else {
            return cat1_info.signal_level;
        }
    }
    return -1;
}

bool cat1_ctrl_poweron(void)
{
    cat1_info.power_on = 1;
    send_msg_to_modem_ctrl(MSG_MODEM_POWERON);
    return true;
}

bool cat1_ctrl_poweroff(void)
{
    cat1_info.power_on = 0;
    send_msg_to_modem_ctrl(MSG_MODEM_POWEROFF);
    return true;
}

bool cat1_ctrl_is_poweron(void)
{
    return cat1_info.power_on ? true : false;
}

void cat1_set_flight_mode(bool flag)
{
    if (cat1_status.is_flight_mode == flag) {
        return ;
    }
    cat1_status.is_flight_mode = flag;

    u8 enable = !flag;

    if (enable) {
        cat1_set_lte_onoff(enable);
    } else {
        cat1_set_lte_onoff(enable);
    }

#if TCFG_APP_BT_EN
    if (enable) {
        ble_module_enable(1);
        if (is_bredr_close()) {
#if 1
            bredr_conn_last_dev();
#else
            bt_init_bredr();
#endif//自动回连
        }
    } else {
        if (!is_bredr_close()) {
            bt_close_bredr();
        }
        ble_module_enable(0);
    }
#endif /* #if TCFG_APP_BT_EN */
}

bool cat1_get_flight_mode(void)
{
    return cat1_status.is_flight_mode;
}

void cat1_set_lte_net_onoff(int flag)
{
    int rpc_ret;
    cat1_status.lte_net_enable = flag;
    int res;
    if (flag == 0) {
        int data_res = ual_tele_data_deactive(0, &rpc_ret);
        log_info("data res:%d\n", data_res);
        res = ual_tele_data_unregister(cat1_info.client_data_handle, &rpc_ret);
        log_info("data unregister!\n");
    } else {
        int data_res = ual_tele_data_active(0, &rpc_ret);
        log_info("data res:%d\n", data_res);
        res = ual_tele_data_register(handleDataCallback, &cat1_info.client_data_handle, &rpc_ret);
        log_info("data register!\n");
    }
}

int cat1_get_lte_net_onoff(void)
{
    return cat1_status.lte_net_enable;
}

void cat1_set_lte_call_onoff(int flag)
{
    if (cat1_status.lte_call_enable == flag) {
        return ;
    }
    cat1_status.lte_call_enable = flag;

    if (flag) {
        cat1_set_call_register(1);
        call_ctrl_set_call_sel(CALL_SEL_CAT1);
    } else {
        cat1_set_call_register(0);
        call_ctrl_set_call_sel(CALL_SEL_BT);
    }
}

int cat1_get_lte_call_onoff(void)
{
    return cat1_status.lte_call_enable;
}

int cat1_get_lte_onoff(void)
{
    return cat1_status.lte_enable;
}

void cat1_set_lte_onoff(int flag)
{
    if (cat1_status.lte_enable == flag) {
        return ;
    }
    cat1_status.lte_enable = flag;

    cat1_set_lte_can_touch(0);

    if (flag) {
        //modem 上下电
        cat1_ctrl_poweron();
        cat1_set_lte_call_onoff(flag);
        call_ctrl_set_call_sel(CALL_SEL_CAT1);
    } else {
        //modem 上下电
        cat1_ctrl_poweroff();
        cat1_set_lte_call_onoff(flag);
        call_ctrl_set_call_sel(CALL_SEL_BT);
    }
}

char *cat1_get_operator(void)
{
    int rpc_ret = 0;
    char opn[UAL_TELE_SIM_OPN_MAX_LEN + 1];

    ual_tele_sim_get_opn(0, opn, &rpc_ret);

    log_info("operator:%s\n", opn);

    return opn;
}

void cat1_set_send_sms_number(char *number, int offset, int len)
{
    if ((offset + len) > sizeof(cat1_sms_info.number)) {
        log_info("%s, len limit:%d \n", __func__, offset + len);
        return ;
    }
    memcpy(cat1_sms_info.number + offset, number, len);
}

char *cat1_get_send_sms_number(void)
{
    return cat1_sms_info.number;
}

void cat1_set_send_sms_content(char *content, int offset, int len)
{
    if ((offset + len) > sizeof(cat1_sms_info.content)) {
        log_info("%s, len limit:%d \n", __func__, offset + len);
        return ;
    }
    memcpy(cat1_sms_info.content + offset, content, len);
}

char *cat1_get_send_sms_content(void)
{
    return cat1_sms_info.content;
}

void clear_send_sms(void)
{
    memset(&cat1_sms_info, 0, sizeof(cat1_sms_info));
}

void clear_send_sms_content(void)
{
    memset(&cat1_sms_info.content, 0, sizeof(cat1_sms_info.content));
}

int cat1_send_sms(int number_len, char *number, int content_len, char *content)
{
    int rpc_ret = 0;
    ual_tele_sms_send_param_t sms_send_param_t = {0};
    sms_send_param_t.dest_address.number_list_count = 1;
    if (number_len >= UAL_TELE_SMS_NUMBER_MAX_LEN) {
        number_len = UAL_TELE_SMS_NUMBER_MAX_LEN - 1;
    }
    memcpy(sms_send_param_t.dest_address.number_addr[0], number, number_len);

    unsigned short *send_content = malloc(UAL_TELE_SMS_CONTENT_MAX_LEN * sizeof(unsigned short));
    if (content_len >= UAL_TELE_SMS_CONTENT_MAX_LEN) {
        content_len = UAL_TELE_SMS_CONTENT_MAX_LEN - 1;
    }
    int send_content_len =  UTF82Unicode(content, send_content, content_len);
    sms_send_param_t.content_len = send_content_len * 2;
    memcpy(sms_send_param_t.content, send_content, send_content_len * 2);

    int res = ual_tele_sms_send(&sms_send_param_t, &rpc_ret);

    free(send_content);

    return true;
}

int cat1_get_engineering_mode(void)
{
    return cat1_info.is_engineering_mode;
}

void cat1_set_engineering_mode(int flag)
{
    cat1_info.is_engineering_mode = flag;
}

int cat1_get_set_mode(void)
{
    return cat1_info.is_set_mode;
}

void cat1_set_set_mode(int flag)
{
    cat1_info.is_set_mode = flag;
}

int cat1_get_lte_can_touch(void)
{
    return cat1_status.lte_can_touch;
}

void cat1_set_lte_can_touch(int flag)
{
    cat1_status.lte_can_touch = flag;
}

int cat1_get_flight_can_touch(void)
{
    return cat1_status.flight_mode_can_touch;
}

void cat1_set_flight_can_touch(int flag)
{
    cat1_status.flight_mode_can_touch = flag;
}

int cat1_get_call_register(void)
{
    return cat1_status.is_call_register;
}

void cat1_set_call_register(int flag)
{
    int rpc_ret = 0;
    int32_t call_handle = 0;
    if (flag == 0) {
        log_info("call unregister");
        int res = ual_tele_call_unregister(cat1_info.client_handle, &rpc_ret);
    } else {
        log_info("call register");
        ual_tele_call_register(handleCallCallback, &cat1_info.client_handle, &rpc_ret);
    }
    cat1_status.is_call_register = flag;
}

bool cat1_call_use_modem_volume(void)
{
    return false;
}

void cat1_call_set_modem_volume(int volume)
{
}

int cat1_call_get_modem_volume(void)
{
    return 0;
}

static int cat1_unisoc_modem_init(void)
{
    ModemTaskInit();
    modem_ctl_client_reg_handler(MC_CLIENT_MMI, _callback_ModemCtrl);
#if TCFG_CAT1_MODULE_UPDATE_ENABLE
    // update init
    cat1_module_update_init();
#endif
    /* mcu_diag_init(); */
    /* mcu_at_init(); */
    return 0;
}
late_initcall(cat1_unisoc_modem_init);

void cat1_init(void)
{
    thinmodem_register_callback(handleModemCallback);
}

void cat1_close(void)
{
    int to = 3000;

    send_msg_to_modem_ctrl(MSG_MODEM_POWEROFF);
    while (cat1_info.power_on != 0) {
        os_time_dly(1);
        if (to < 10) {
            break;
        }
        to -= 10;
    }
}

#if TCFG_CAT1_MODULE_UPDATE_ENABLE
#include "cat1/unisoc/export/version.h"
#include "cat1/unisoc/export/mdload_api.h"
#include "cat1/unisoc/export/modem_ctrl_api.h"
#include "cat1/unisoc/include/rpc.h"

#define CAT1_VENDER_UNISOC                  (1)          // 展锐
#define CAT1_MODULE_UPDATE_VENDER           CAT1_VENDER_UNISOC
#define CAT1_MODULE_UPDATE_END_TIMEOUT      (200)

#define CAT1_MODULE_UPDATE_VERSION_MAX_LEN  (100)
#define CAT1_MODULE_UPDATE_VERSION_FMT      "Platform Version:" \
                                            " %[^\r\n]\r\n"

enum {
    PLATFORM_VERSION,
    PROJECT_VERSION,
    MMI_VERSION,
    AP_SVN_VERSION,
    CP_SVN_VERSION,
    BASE_VERSION,
    HW_VERSION,
    CALIBRATION_VERSION,
    DOWNLOAD, _VERSION,
    BUILD_TIME,
    LOG_VERSION,
    GE2_VER,
    MAX_VERSION_NUM,
};

cat1_power_state_t cat1_module_get_power_state(void)
{
    u8 state = (u8)modem_state_check();
    switch (state) {
    case MODEM_STATE_OFFLINE:
    case MODEM_STATE_POWEROFF:
    case MODEM_STATE_POWEROFFPROGRESS:
    case MODEM_STATE_POWERONPROGRESS:
    case MODEM_STATE_RESET:
        return CAT1_POWER_STATE_POWEROFF;

    case MODEM_STATE_ALIVE:
    case MODEM_STATE_POWERON:
        return CAT1_POWER_STATE_POWERON;

    case MODEM_STATE_ASSERT:
    case MODEM_STATE_BLOCK:
    case MODEM_STATE_CHECK_ERR:
        return CAT1_POWER_STATE_OTHER;
    default:
        log_error(" %s err !!!\n", __func__);
        return CAT1_POWER_STATE_OTHER;
    }
}

u8 cat1_module_can_get_version(void)
{
    u8 flag = (cat1_module_get_power_state() == CAT1_POWER_STATE_POWERON) ? 1 : 0;
    return (flag && (rpc_channel_ready(0) || rpc_channel_ready(1)));
}

u8 cat1_module_get_vender_id(void)
{
    return CAT1_MODULE_UPDATE_VENDER;
}

u16 cat1_module_get_version_total_len(void)
{
    return CAT1_MODULE_UPDATE_VERSION_MAX_LEN;
}

u16 cat1_module_get_update_end_timeout(void)
{
    return CAT1_MODULE_UPDATE_END_TIMEOUT;
}

// 0:ok ; 1:err
u8 cat1_module_pasre_version(u8 *buf)
{
    if (cat1_module_get_power_state() != CAT1_POWER_STATE_POWERON) {
        log_error("cat1 poweron err\n");
        return 1;
    }

    char *ver_buf = (char *)zalloc(CAT1_MODULE_UPDATE_VERSION_MAX_LEN);
    char *cat1_version = (char *)zalloc(CAT1_MODULE_UPDATE_VERSION_MAX_LEN);
    if ((cat1_version == NULL) || (ver_buf == NULL)) {
        log_error("cat1 ver buf zalloc err !\n");
        return 1;
    }

    VERSION_GetInfoEx(PLATFORM_VERSION, ver_buf, NULL);
    sscanf(ver_buf, CAT1_MODULE_UPDATE_VERSION_FMT, cat1_version);

    memset(buf, 0x00, cat1_module_get_version_total_len());
    memcpy(buf, cat1_version, CAT1_MODULE_UPDATE_VERSION_MAX_LEN);

    log_info("====================================================\r\n");
    log_info(" > %s <\n", cat1_version);
    log_info("====================================================\r\n");

    free(ver_buf);
    free(cat1_version);
    return 0;
}

// 0:ok; 1:err
int cat1_module_fota_start(u32 packet_size, u32 timeout)
{
    u32 wait_timeout = jiffies + msecs_to_jiffies(timeout);

    int ret = mdload_open(MDLOAD_DOWNLOAD_MODE);
    if ((ret == MDLOAD_NO_ERR) && time_before(jiffies, wait_timeout)) {
        ret = mdload_start(MDLOAD_FILE_MODEM_IMG, packet_size);
        if ((ret == MDLOAD_NO_ERR) && time_before(jiffies, wait_timeout)) {
            return 0;
        }
    }

    return 1;
}

int cat1_module_fota_end(void)
{
    os_time_dly(10);   // 添加延时等待数据传输完毕, 模块厂商建议值
    int ret = mdload_end();
    log_info("mdload_end err %d", ret);
    return (MDLOAD_NO_ERR == ret) ? 0 : 1;
}

int cat1_module_fota_close(void)
{
    int ret = mdload_close();
    log_info("mdload_close err %d", ret);
    return (MDLOAD_NO_ERR == ret) ? 0 : 1;
}

int cat1_module_fota_trans_data(u8 *data, u32 len)
{
    return mdload_send(data, len);
}

#endif
#endif
