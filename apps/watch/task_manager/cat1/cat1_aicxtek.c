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
#include "aic_audio.h"
#include "aic_srv_tele_call.h"
#include "aic_srv_tele_sim.h"
#include "aic_srv_tele_radio.h"
#include "aic_srv_tele_sms.h"
#include "aic_ctrl.h"
#include "aic_srv_voice.h"
#include "aic_init.h"
#include "tone_player.h"
#include "btstack/avctp_user.h"
#include "timestamp.h"
#include "call_common.h"
#include "message_vm_cfg.h"
#include "bt_common.h"

#if TCFG_APP_CAT1_EN && TCFG_CAT1_AICXTEK_ENABLE

#define LOG_TAG_CONST       APP_CAT1
#define LOG_TAG             "[APP_CAT1]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
#define LOG_CLI_ENABLE
#include "debug.h"

static ts_radio_status_updated_t	s_radio_info = {0};
ts_radio_get_cell_info_resp_t cell_info_t = {0};   //当区信息
ts_radio_get_neighbor_cell_info_list_resp_t neighbor_cell_info_list_t = {0};   //邻区信息
ts_radio_operator_info_t operator_info[4] = {0};      //手动搜网

struct s_cat1_info {
    // power
    u8 power_on;

    // call
    /* 当前通话的index值 */
    uint8_t call_idx;
    /* 对端的电话号码 */
    char phone_number[TS_CALL_PHONE_NUMBER_LEN_MAX + 1];
    /* 电话的direction：0-MO，1-MT */
    uint8_t direction;
    /* 当前通话状态 */
    ts_call_status_e call_status;

    // sim
    uint32_t sim_exist_status;

    // radio
    bool is_ims_on;
    bool radio_is_on;
    uint32_t signal_level; //0-4
    int32_t signal_strength; // dB

    // operater
    ts_radio_operator_id_e operator_id;
    char operator_long_name[TS_RADIO_OPERATOR_LONG_NAME_LEN_MAX + 1];

    // app
    u16 cat1_call_check_tmr;

    //smsc
    char smsc[TS_SMS_NUM_MAX_LEN + 1];

    // handle
    int32_t client_handle;

    //工模
    int32_t is_engineering_mode;

    int32_t is_set_mode;
};

struct s_cat1_info cat1_info = {0};
struct s_cat1_status cat1_status = {.lte_can_touch = 1, .flight_mode_can_touch = 1};
struct s_cat1_sms_info cat1_sms_info = {0};     //4g短信信息

extern void port_falling_wakeup_callback(u8 index, u8 gpio);
extern void port_rising_wakeup_callback(u8 index, u8 gpio);

extern void bt_set_phone_active_start_time_ms(void);

extern u8 is_bredr_close();
extern void bt_init_bredr();
extern void bredr_conn_last_dev();
extern void bt_close_bredr();
extern void __set_bt_esco_by_watch(bool flag);
extern void set_call_log_type(u8 type);

extern void *notice_info_create(void);
extern void notice_info_release(void *info);
extern void notice_set_info_from(void *info, void *name, void *data, u16 len);
extern void notice_add_info_from(void *info);

extern bool aic_test_get_is_from_engineering_mode(void);
extern void aic_test_set_is_from_engineering_mode(bool flag);
extern void set_engineering_poweronoff_can_touch(bool flag);

extern int lwip_pdp_init(void);

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

#if TCFG_UI_ENABLE_PHONEBOOK
static int cat1_call_save_info_async_deal(int param)
{
    if (APP_CAT1_TASK != app_get_curr_task()) {
        log_error("save cat1 name err \n");
        return 0;
    }
    u8 call_name[PHONEBOOK_NAME_LEN + 1] = {0};
    if (phonebook_get_name_by_number(cat1_info.phone_number, call_name)) {
        set_call_log_name(call_name);
    }
    return 0;
}

static int cat1_call_update_info_async_deal(int param)
{
    update_call_log_message();
    return 0;
}

static void cat1_call_save_info(void)
{
    set_call_log_date(NULL);
    set_call_log_type(2);
    set_call_log_number((u8 *)cat1_info.phone_number);

    // cat1里面不允许长时间delay，需要推送到app_core任务里面执行
    int msg[3] = {0};
    msg[0] = (int)cat1_call_save_info_async_deal;
    msg[1] = 1;
    int err = os_taskq_post_type("app_core", Q_CALLBACK, 3, msg);
    if (err) {
        log_error("%s, post err:%d \n", __func__, err);
    }
}

static void cat1_call_update_info(void)
{
    // cat1里面不允许长时间delay，需要推送到app_core任务里面执行
    int msg[3] = {0};
    msg[0] = (int)cat1_call_update_info_async_deal;
    msg[1] = 1;
    int err = os_taskq_post_type("app_core", Q_CALLBACK, 3, msg);
    if (err) {
        log_error("%s, post err:%d \n", __func__, err);
    }
}
#endif /* #if TCFG_UI_ENABLE_PHONEBOOK */

static int cat1_sms_save_info_async_deal(int param1, int param2)
{
    void *msg = (void *)param1;
    int is_send = param2;
    notice_add_info_from(msg);
    notice_info_release(msg);
    if (is_send) {
        UI_MSG_POST("cat1_event:send_SMS=%4", 1);
    } else {
        UI_MSG_POST("cat1_event:SMS=%4", 1);
    }
    return 0;
}

static void cat1_sms_save_info(void *sms, int is_send)
{
    // cat1里面不允许长时间delay，需要推送到app_core任务里面执行
    int msg[4] = {0};
    msg[0] = (int)cat1_sms_save_info_async_deal;
    msg[1] = 2;
    msg[2] = (int)sms;
    msg[3] = (int)is_send;
    int err = os_taskq_post_type("app_core", Q_CALLBACK, 4, msg);
    if (err) {
        log_error("%s, post err:%d \n", __func__, err);
        notice_info_release(msg); // 需要释放资源
    }
}

static void cat1_call_check_func(void *priv)
{
    // 通话状态判断
    if (cat1_info.call_status == TS_CALL_STATUS_IDLE) {
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
    char *len_lst[TS_CALL_PHONE_NUMBER_LEN_MAX + 4 + 1]; // 电话号码加铃声循环
    tone_play_stop();
    number_to_play_list((char *)(cat1_info.phone_number), len_lst);
    tone_file_list_play(len_lst, 1);

#if TCFG_UI_ENABLE_PHONEBOOK
    cat1_call_save_info();
#endif /* #if TCFG_UI_ENABLE_PHONEBOOK */
}


static int32_t handleReleaseResp(void *p_param, uint32_t size)
{
    ts_call_release_resp_t *p_release_resp = NULL;

    if (NULL == p_param) {
        log_error("handleReleaseResp: param is null");
        return -1;
    }
    p_release_resp = (ts_call_release_resp_t *)(p_param);
    if (0 != p_release_resp->ret_value) {
        /* release fail:
         *            popup prompt message */

    } else {
        /* release success:
         *            to ui task:to display disconnected win */
    }
    return 0;
}

static int32_t handleAnswerResp(void *p_param, uint32_t size)
{
    ts_call_answer_resp_t *p_answer_resp = NULL;

    if (NULL == p_param) {
        log_error("handleAnswerResp: param is null");
        return -1;
    }
    p_answer_resp = (ts_call_answer_resp_t *)(p_param);
    if (0 != p_answer_resp->ret_value) {
        /* anwser fail:
         *            to ui task:popup prompt message "auwser fail" */
    } else {
        /* anwser success:
         *           to ui task:to display connected call win */
    }
    return 0;
}

static int32_t handleRejectResp(void *p_param, uint32_t size)
{
    ts_call_reject_resp_t *p_reject_resp = NULL;

    if (NULL == p_param) {
        log_error("handleRejectResp: param is null");
        return -1;
    }
    p_reject_resp = (ts_call_reject_resp_t *)(p_param);
    if (0 != p_reject_resp->ret_value) {
        /* Reject fail:
         *            to ui task:popup prompt message */
    } else {
        /* Reject fail:
         *            to ui task::close incoming call win */
    }
    return 0;
}

static int32_t handleDialResp(void *p_param, uint32_t size)
{
    ts_call_dial_resp_t *p_dial_resp = NULL;

    if (NULL == p_param) {
        log_error("handleDialResp: param is null");
        return -1;

    }
    p_dial_resp = (ts_call_dial_resp_t *)(p_param);
    log_info("[%s], dial resp val = %d,", __func__, p_dial_resp->ret_value);
    if (0 != p_dial_resp->ret_value) {
        /* dial fail:
         *            to ui task:popup prompt message */
        return -1;

    } else {
        /* dial success:
         *            to ui task:enter outgoing call win */
    }
    return 0;
}


int32_t handleCallCallback(void *p_param, uint32_t size)
{
    ts_event_call_t *p_call_event = p_param;
    int ret_audio = 0;

    log_info("%s \n", __func__);

    if (NULL == p_call_event) {
        log_error("%s,p_param is null", __func__);
        return -1;
    }

    log_info("%s 0x%x \n", __func__, p_call_event->event_id);

    switch (p_call_event->event_id) {
    case TS_EVENT_CALL_STATUS_UPDATED: {
        ts_call_status_updated_t *p_call_status = {0};
        ts_call_info_list_t call_info_list = {0};

        p_call_status = (ts_call_status_updated_t *)p_param;
        log_info("handleCallCallback,p_call_status status = %d",  p_call_status->call_status);
        aic_srv_tele_call_get_call_info_list(0, &call_info_list);
        /* copy call info data */
        cat1_info.call_idx = call_info_list.call_info[0].call_idx;
        cat1_info.call_status = call_info_list.call_info[0].call_status;
        cat1_info.direction = call_info_list.call_info[0].direction;
        memcpy(cat1_info.phone_number, call_info_list.call_info[0].phone_number, sizeof(cat1_info.phone_number));
        log_info("number:%s, %s \n", cat1_info.phone_number, p_call_status->phone_number);

        switch (p_call_status->call_status) {
        case TS_CALL_STATUS_ACTIVE:/* remote accept call,setup call success,display connected call win(include,phone number and call duration) */
            if (APP_CAT1_TASK != app_get_curr_task()) {
                log_error("not cat1 task \n");
                break;
            }
            /* 1.stop play local ring */
            tone_play_stop();

            /* 2.stop play local ringback */

            /* 3.start voice */
            app_cat1_audio_start();

            /* 4.enter connected win and display call time*/
            bt_set_phone_active_start_time_ms();
            UI_MSG_POST("phone_active:status=%4", 1);
            break;

        case TS_CALL_STATUS_DIALING: {
            app_task_switch_to(APP_CAT1_TASK);
#if TCFG_UI_ENABLE_PHONEBOOK
            set_call_log_date(NULL);
#endif /* #if TCFG_UI_ENABLE_PHONEBOOK */
        }
        break;

        case TS_CALL_STATUS_INCOMING: {
            /* incoming a call,phone recieve a call */

            /* 1.wheather exist a connected call,if exist,hangup it;if not exist, play local call ring */
            if (1 < call_info_list.call_number) {
                int ret = -1;
                uint8_t cur_card_id = TS_CARD_ID_MAX;
                aic_srv_tele_sim_get_current_card(&cur_card_id);
                ret = aic_srv_tele_call_release(cur_card_id, p_call_status->call_idx, NULL);
                log_info("handleCallCallback,aic_srv_tele_call_release return,ret = %d.\r\n", ret);
                return -1;
            } else {
                if (false == app_cat1_check_support()) {
                    /* return -1; */
                    // 当前条件不容许进入通话时，定时检查状态
                    if (cat1_info.cat1_call_check_tmr == NULL) {
                        cat1_info.cat1_call_check_tmr = sys_timer_add(NULL, cat1_call_check_func, 1000);
                    }
                    break;
                }
                app_task_switch_to(APP_CAT1_TASK);
                /* play local ring */
                char *len_lst[TS_CALL_PHONE_NUMBER_LEN_MAX + 4 + 1]; // 电话号码加铃声循环
                tone_play_stop();
                number_to_play_list((char *)(cat1_info.phone_number), len_lst);
                tone_file_list_play(len_lst, 1);

#if TCFG_UI_ENABLE_PHONEBOOK
                cat1_call_save_info();
#endif /* #if TCFG_UI_ENABLE_PHONEBOOK */
            }

            /* 2. to ui task,display incoming call win */
        }
        break;

        case TS_CALL_STATUS_RELEASED: {
            /*hung up call,user hungup and remote hungup,this message is coming */

            /* if there is other call index, do not need to stop voice */
            log_info("handleCallCallback,TS_CALL_STATUS_RELEASED call_number = %d.\r\n", call_info_list.call_number);
            if (0 == call_info_list.call_number) {
                /* 1. stop voice */
                if (APP_CAT1_TASK == app_get_curr_task()) {
                    tone_play_stop();
                }
                app_cat1_audio_stop();

                /* AIC_TEST: update flag */
                aic_test_set_is_from_engineering_mode(false);
            } else {
                log_info("handleCallCallback,Release the second call, do nothing and return only.\r\n");
                return -1;
            }

            /* 2.release call，
             *                        ui task:close call win */
            if (cat1_info.cat1_call_check_tmr) {
                sys_timer_del(cat1_info.cat1_call_check_tmr);
                cat1_info.cat1_call_check_tmr = 0;
            }
            if (APP_CAT1_TASK == app_get_curr_task()) {
#if TCFG_UI_ENABLE_PHONEBOOK
                cat1_call_update_info();
#endif /* #if TCFG_UI_ENABLE_PHONEBOOK */
                app_task_switch_back();
            }
        }
        break;

        default:
            break;
        }
    }
    break;

    case TS_EVENT_CALL_VOLTE_REG_STATUS_UPDATED: { /* ims register state*/
        ts_call_volte_reg_status_updated_t *p_volte_status = NULL;
        log_info("handleCallCallback,TS_EVENT_CALL_VOLTE_REG_STATUS_UPDATED");
        p_volte_status = (ts_call_volte_reg_status_updated_t *)p_param;

        log_info("handleCallCallback,volte status = %d", p_volte_status->volte_reg_status);
        if (true == p_volte_status->volte_reg_status) {
            /* 1.ims register success
             *                    UI task:refresh volte icon. */

        } else {
            /* 1.ims register fail
             *                    UI task:refresh volte icon. */

        }
    }
    break;

    case TS_EVENT_CALL_RINGBACK_VOICE: {
        /* play tone,maybe customed ringback by operator from network,maybe play local ringback settinged by user*/
        ts_call_ringback_voice_t *p_ringback_voice = NULL;

        p_ringback_voice = (ts_call_ringback_voice_t *)p_param;
        log_info("handleCallCallback,TS_EVENT_CALL_RINGBACK_VOICE arrived(voice_type = %d, voice_state = %d).\r\n",
                 p_ringback_voice->voice_type,
                 p_ringback_voice->voice_state);

        if (1 == p_ringback_voice->voice_state) {
            /* 2.play local ringback */
            log_info("handleCallCallback,audio_play_tone return %d.\r\n", ret_audio);
            app_cat1_audio_start();
        } else {
            /* 3. stop play local ringback */
            log_info("handleCallCallback,audio_stop_tone return %d.\r\n", ret_audio);
        }
    }
    break;

    default:
        break;
    }

    return 0;
}


int32_t handleRadioCallback(void *p_param, uint32_t size)
{
    ts_event_radio_t *p_radio_event = NULL;

    if (NULL == p_param) {
        log_error("%s,p_param is null", __func__);
        return -1;
    }

    p_radio_event = (ts_event_radio_t *)p_param;

    log_info("%s 0x%x \n", __func__, p_radio_event->event_id);

    switch (p_radio_event->event_id) {
    case TS_EVENT_RADIO_SIGNAL_INFO_UPDATED: {
        /* signal strenth&level reported per 10s */
        ts_radio_signal_info_updated_t *p_signal_info = NULL;
        log_info("[%s],TS_EVENT_RADIO_SIGNAL_INFO_UPDATED", __func__);

        p_signal_info = (ts_radio_signal_info_updated_t *)p_param;
        log_info("[%s], signal_strength = %d, signal_level = %d.\r\n",
                 __func__,
                 p_signal_info->signal_strength,
                 p_signal_info->signal_level);

        cat1_info.signal_level = p_signal_info->signal_level;
        cat1_info.signal_strength = p_signal_info->signal_strength;

        UI_MSG_POST("cat1_event:signal_level=%4:signal_strength=%4", p_signal_info->signal_level, p_signal_info->signal_strength);
        log_info("ui redraw signal level");

        /* signal level0: signal strenth <=-120dBm */
        /* signal level1: signal strenth -111~-119dBm */
        /* signal level2: signal strenth -106~-110dBm */
        /* signal level3: signal strenth -91~-105dBm */
        /* signal level4: signal strenth >=-90dBm */

        /* update signal
           to ui task: update signal level */
    }
    break;
    case TS_EVENT_RADIO_STATUS_UPDATED: {
        ts_radio_status_updated_t *p_radio_status = NULL;
        log_info("[%s],TS_EVENT_RADIO_STATUS_UPDATED", __func__);

        p_radio_status = (ts_radio_status_updated_t *)p_param;
        cat1_info.radio_is_on = p_radio_status->is_on;
        UI_MSG_POST("cat1_event:radio=%4", p_radio_status->is_on);
        log_info("[%s],radio status = %d.\r\n", __func__, p_radio_status->is_on);
    }
    break;
    case TS_EVENT_RADIO_ON_RESP: {
        ts_radio_on_resp_t *p_on_resp = NULL;
        log_info("[%s],TS_EVENT_RADIO_ON_RESP", __func__);

        p_on_resp = (ts_radio_on_resp_t *)p_param;
        log_info("[%s],radio on resp = %d.\r\n", __func__, p_on_resp->ret_value);
    }
    break;
    case TS_EVENT_RADIO_IMS_REG_STATUS_UPDATED: {
        /* ims status reported */
        ts_radio_ims_reg_status_updated_t *p_ims_status = NULL;
        log_info("[%s],TS_EVENT_RADIO_IMS_REG_STATUS_UPDATED", __func__);

        p_ims_status = (ts_radio_ims_reg_status_updated_t *)p_param;
        cat1_info.is_ims_on = p_ims_status->is_ims_on;
        UI_MSG_POST("cat1_event:ims=%4", p_ims_status->is_ims_on);
        log_info("[%s],ims status = %d.\r\n", __func__, p_ims_status->is_ims_on);
        if (true == p_ims_status->is_ims_on) {
            /* ims open
               to ui task: update ims icon */
        } else {
            /* ims close
               to ui task: update ims icon */
        }
    }
    break;
    case TS_EVENT_RADIO_OPERATOR_INFO_UPDATED: {
        /* openator name(eg,cmcc,cucc,ctcc) reported*/
        ts_radio_operator_info_updated_t *p_operator_name = NULL;
        log_info("[%s],TS_EVENT_RADIO_OPERATOR_INFO_UPDATED", __func__);
        p_operator_name = (ts_radio_operator_info_updated_t *)p_param;
        log_info("[%s],operator short name = %s, long name = %s,", __func__,
                 p_operator_name->operator_info.operator_short_name,/*cmcc/cucc/ctcc*/
                 p_operator_name->operator_info.operator_long_name);/*China Mobile/China Unicom/china Telecom*/
        cat1_info.operator_id = p_operator_name->operator_info.operator_id;
        memcpy(cat1_info.operator_long_name, p_operator_name->operator_info.operator_long_name, sizeof(cat1_info.operator_long_name));
        UI_MSG_POST("cat1_event:operator_id=%4", p_operator_name->operator_info.operator_id);

        /* operator name
           to ui task: display operator name */
    }
    break;
    case TS_EVENT_RADIO_GET_CELL_INFO_RESP: {
        /*cell info*/
        ts_radio_get_cell_info_resp_t *p_cell_info = NULL;
        p_cell_info = (ts_radio_get_cell_info_resp_t *)(p_param);
        log_info("[%s],TS_EVENT_RADIO_GET_CELL_INFO_RESP", __func__);
        log_info("network mode:%d, plmn mcc:%s, plmn mnc:%s, tac:%d, earfcn:%d, cell id:%d\n", p_cell_info->cell_info.rat_type,
                 p_cell_info->cell_info.plmn_mcc, p_cell_info->cell_info.plmn_mnc, p_cell_info->cell_info.tac, p_cell_info->cell_info.earfcn,
                 p_cell_info->cell_info.cellid);

        memcpy(&cell_info_t, p_cell_info, sizeof(ts_radio_get_cell_info_resp_t));

        UI_MSG_POST("cell_info_show");
    }
    break;
    case TS_EVENT_RADIO_GET_NEIGHBOR_CELL_INFO_LIST_RESP: {
        /*neigh cell*/
        ts_radio_get_neighbor_cell_info_list_resp_t *neighbor_cell_info_list = NULL;
        neighbor_cell_info_list = (ts_radio_get_neighbor_cell_info_list_resp_t *)(p_param);
        log_info("[%s],TS_EVENT_RADIO_GET_CELL_INFO_RESP", __func__);
        log_info("ncell num:%d\n", neighbor_cell_info_list->ncell_info_list.ncell_num);

        memcpy(&neighbor_cell_info_list_t, neighbor_cell_info_list, sizeof(ts_radio_get_neighbor_cell_info_list_resp_t));
        UI_MSG_POST("neigh_cell_info_show");

    }
    break;
    case TS_EVENT_RADIO_START_SEARCH_RESP: {
        log_info("[%s],TS_EVENT_RADIO_START_SEARCH_RESP", __func__);
        uint32_t index = 0;
        ts_radio_start_search_resp_t *p_resp = NULL;
        ts_radio_operator_info_t *p_operator_info = NULL;

        p_resp = (ts_radio_start_search_resp_t *)p_param;
        log_info("[%s]Receive TS_EVENT_RADIO_START_SEARCH_RESP.", __func__);
        log_info("[%s]event_id = 0x%x, ret_value = %d, operator_num = %d.",
                 __func__,
                 p_resp->event_id,
                 p_resp->ret_value,
                 p_resp->operator_num);
        for (index = 0; index < p_resp->operator_num; index++) {
            p_operator_info = (ts_radio_operator_info_t *)(&(p_resp->data_header)) + index;
            memset(&operator_info[index], 0, sizeof(ts_radio_operator_info_t));
            memcpy(&operator_info[index],
                   p_operator_info,
                   sizeof(ts_radio_operator_info_t));
        }

        log_info("write success!\n");
        UI_MSG_POST("network_search");
    }
    break;
    default:
        break;
    }

    return 0;
}

static void get_sys_time(struct sys_time *time)
{
    void *fd = dev_open("rtc", NULL);
    if (!fd) {
        memset(time, 0, sizeof(*time));
        return;
    }
    dev_ioctl(fd, IOCTL_GET_SYS_TIME, (u32)time);
    dev_close(fd);
}

int32_t handleSmsCallback(void *p_param, uint32_t size)
{
    uint32_t event = *(uint32_t *)p_param;

    log_info("%s \n", __func__);

    if (NULL == p_param) {
        log_error("%s,p_param is null", __func__);
        return -1;
    }

    log_info("%s 0x%x \n", __func__, event);

    switch (event) {
    case TS_EVENT_SMS_RECEIVE_NEW: {
        ts_sms_receive_new_t *p_ind = (ts_sms_receive_new_t *)p_param;
        char *text = (char *)(&p_ind->p_content);
        log_info("[%s][Info]TS_EVENT_SMS_RECEIVE_NEW, index:%d, total_count:%d, p_ind->is_full_in_sim = %d.\n", __func__, p_ind->index, p_ind->total_count, p_ind->is_full_in_sim);
        log_info("[%s][Info]TS_EVENT_SMS_RECEIVE_NEW, sms_class(%d), ymd(%d-%d-%d), hms(%d:%d:%d), number(%s), content(%s).",
                 __func__,
                 p_ind->sms_class,
                 p_ind->time_stamp.year,
                 p_ind->time_stamp.month,
                 p_ind->time_stamp.day,
                 p_ind->time_stamp.hour,
                 p_ind->time_stamp.min,
                 p_ind->time_stamp.sec,
                 p_ind->number,
                 (char *)&p_ind->p_content);

        //在工模接收消息，不存flash，直接在工模送显
        if (cat1_get_engineering_mode() == 1) {
            memcpy(cat1_sms_info.number, p_ind->number, strlen(p_ind->number) + 1);
            memcpy(cat1_sms_info.content, text, strlen(text) + 1);
            UI_MSG_POST("sms_show");
        } else {
            log_info("%s TS_EVENT_SMS_READ_IN_SIM_RESP, index:%d, textstart:0x%x \n", __func__, p_ind->index, p_ind->p_content);
            log_info("date:%d,%d,%d, time:%d,%d,%d \n", p_ind->time_stamp.year, p_ind->time_stamp.month, p_ind->time_stamp.day, p_ind->time_stamp.hour, p_ind->time_stamp.min, p_ind->time_stamp.sec);
            if (p_ind->p_content) {
                log_info("TS_EVENT_SMS_READ_IN_SIM_RESP, text:%s \n", text);
                void *msg = notice_info_create();
                if (msg) {
                    struct sys_time curtime;
                    curtime.year = p_ind->time_stamp.year;
                    curtime.month = p_ind->time_stamp.month;
                    curtime.day = p_ind->time_stamp.day;
                    curtime.hour = p_ind->time_stamp.hour;
                    curtime.min = p_ind->time_stamp.min;
                    curtime.sec = p_ind->time_stamp.sec;
                    u32 TimeStamp = timestamp_mytime_2_utc_sec(&curtime);
                    notice_set_info_from(msg, "UID", &TimeStamp, sizeof(TimeStamp));
                    notice_set_info_from(msg, "AppIdentifier", "MobileSMS_RECEIVE", sizeof("MobileSMS_RECEIVE"));
                    notice_set_info_from(msg, "IDTitle", p_ind->number, TS_SMS_NUM_MAX_LEN + 1);
                    notice_set_info_from(msg, "IDMessage", text, strlen(text) + 1);
                    cat1_sms_save_info(msg, 0);
                }
            }
        }
    }
    break;
    case TS_EVENT_SMS_GET_SMSC_RESP: {
        ts_sms_get_smsc_resp_t *p_sms_smsc = (ts_sms_get_smsc_resp_t *)(p_param);
        memcpy(cat1_info.smsc, p_sms_smsc->smsc, sizeof(p_sms_smsc->smsc));
        log_info("the number of smsc:%s\n", cat1_info.smsc);

        UI_MSG_POST("show_sms_center_number");
    }
    break;
    case TS_EVENT_SMS_LOAD_IN_SIM_RESP: {
        ts_sms_load_in_sim_resp_t *p_load_sms = (ts_sms_load_in_sim_resp_t *)p_param;
        uint16_t index = 0;
        log_info("[%s]TS_EVENT_SMS_LOAD_IN_SIM_RESP, card_id = %d, err_code = %d, is_full_in_sim = %d, total_count = %d, count_in_sim = %d, have_reported_count = %d, cur_reported_count = %d, index_start = %d.",
                 __func__,
                 p_load_sms->base_info.card_id,
                 p_load_sms->base_info.err_code,
                 p_load_sms->is_full_in_sim,
                 p_load_sms->total_count,
                 p_load_sms->count_in_sim,
                 p_load_sms->have_reported_count,
                 p_load_sms->cur_reported_count,
                 p_load_sms->index_start);
        if (0 == p_load_sms->base_info.err_code) {
            if (true == p_load_sms->is_full_in_sim) {// load sim sms success.
                // If the sms in sim is full, user could do something here.(such as pop up some UI tips or delete some messages.)
            }
            // when user receive this response event, call this api to get sms content
            for (index = p_load_sms->index_start; index < p_load_sms->total_count; index++) {
                int ret = aic_srv_tele_sms_read_in_sim(p_load_sms->base_info.card_id, index, handleSmsCallback);
                log_info("[%s]aic_srv_tele_sms_read_in_sim[index = %d] return %d.", __func__, index, ret);
            }
        } else {
            // load sim sms failed, and user could pop up some UI tips.
        }
    }
    break;

    case TS_EVENT_SMS_SEND_RESP: {
        ts_sms_send_resp_t *p_sms_send = (ts_sms_send_resp_t *)(p_param);
        log_info("TS_EVENT_SMS_SEND_RESP");
        if (p_sms_send -> base_info.err_code == 0) {    //信息发送成功
            void *msg = notice_info_create();
            if (msg) {
                struct sys_time curtime;
                get_sys_time(&curtime);
                u32 TimeStamp = timestamp_mytime_2_utc_sec(&curtime);
                notice_set_info_from(msg, "UID", &TimeStamp, sizeof(TimeStamp));
                notice_set_info_from(msg, "AppIdentifier", "MobileSMS_SEND", sizeof("MobileSMS_SEND"));
                notice_set_info_from(msg, "IDTitle", cat1_sms_info.number, TS_SMS_NUM_MAX_LEN + 1);
                notice_set_info_from(msg, "IDMessage", cat1_sms_info.content, strlen(cat1_sms_info.content) + 1);
                cat1_sms_save_info(msg, 1);
                clear_send_sms();
            }
        }
    }
    break;
    default:
        break;
    }

    return 0;
}

static int32_t handleSimCallback(void *p_param, uint32_t size)
{
    ts_event_sim_t *p_sim_event = NULL;

    log_info("%s \n", __func__);

    if (NULL == p_param) {
        log_error("%s,p_param is null", __func__);
        return -1;
    }

    p_sim_event = (ts_event_sim_t *)p_param;
    log_info("%s 0x%x \n", __func__, p_sim_event->event_id);

    switch (p_sim_event->event_id) {
    case TS_EVENT_SIM_STATUS_UPDATED: {
        ts_sim_status_updated_t *p_sim_info = NULL;
        log_info("[%s],TS_EVENT_SIM_STATUS_UPDATED", __func__);
        p_sim_info = (ts_sim_status_updated_t *)p_param;
        cat1_info.sim_exist_status = p_sim_info->sim_exist_status;
        UI_MSG_POST("cat1_event:sim=%4", p_sim_info->sim_exist_status);
        log_info("card:%d, status:%d \n", p_sim_info->cur_card_id, p_sim_info->sim_exist_status);
        if (cat1_info.sim_exist_status) {
            aic_srv_tele_radio_on(p_sim_info->cur_card_id, true, handleRadioCallback);
            aic_srv_tele_sms_load_in_sim(p_sim_info->cur_card_id, handleSmsCallback);
        } else {
            /* aic_srv_tele_radio_on(p_sim_info->cur_card_id, false, handleRadioCallback); */
        }
    }
    break;
    default:
        break;
    }

    return 0;
}

static int32_t handleCtrlStateback(void *p_param, uint32_t size)
{
    aic_ctrl_cb_t *p_ctrl = NULL;

    log_info("%s \n", __func__);

    if (NULL == p_param) {
        log_error("%s,p_param is null", __func__);
        return -1;
    }

    p_ctrl = (aic_ctrl_cb_t *)p_param;
    log_info("%s 0x%x \n", __func__, p_ctrl->state);

    switch (p_ctrl->state) {
    case AIC_CTRL_STATE_AILVE:
        log_info("alive!\n");
        cat1_info.power_on = 1;
        cat1_set_lte_call_onoff(1);
        cat1_set_lte_net_onoff(1);
        cat1_set_lte_can_touch(1);
        cat1_set_flight_can_touch(1);
        set_engineering_poweronoff_can_touch(true);
        UI_MSG_POST("cat1_event:power=%4", 1);
        break;
    case AIC_CTRL_STATE_SHUTDOWN:
        log_info("shutdown!\n");
        cat1_info.power_on = 0;
        cat1_set_lte_net_onoff(0);
        cat1_set_lte_call_onoff(0);
        cat1_set_lte_can_touch(1);
        cat1_set_flight_can_touch(1);
        set_engineering_poweronoff_can_touch(true);
        UI_MSG_POST("cat1_event:power=%4", 0);
        break;
    default:
        break;
    }

    return 0;
}

bool cat1_call_answer(void)
{
    if (false == cat1_check_call_enable()) {
        return false;
    }
    int32_t ret = aic_srv_tele_call_answer(handleCallCallback);
    if (ret) {
        log_error("aic_srv_tele_call_answer err:%d \n", ret);
    }
    return true;
}

bool cat1_call_hangup(void)
{
    if (false == cat1_check_call_enable()) {
        return false;
    }
    if (cat1_info.call_status == TS_CALL_STATUS_INCOMING) {
        aic_srv_tele_call_reject(handleCallCallback);
    } else if (cat1_info.call_status == TS_CALL_STATUS_IDLE) {
    } else if (cat1_info.call_status == TS_CALL_STATUS_RELEASED) {
    } else {
        uint8_t cur_card_id = TS_CARD_ID_MAX;
        aic_srv_tele_sim_get_current_card(&cur_card_id);
        aic_srv_tele_call_release(cur_card_id, cat1_info.call_idx, handleCallCallback);
    }
    return true;
}

bool cat1_call_number(int number_len, char *number)
{
    if (false == cat1_check_call_enable()) {
        return false;
    }
    log_info("%s,num:%s \n", __func__, number);

    ts_call_dial_info_t dial_info = {0};
    if (number_len > TS_CALL_PHONE_NUMBER_LEN_MAX) {
        number_len = TS_CALL_PHONE_NUMBER_LEN_MAX;
    }
    memcpy(dial_info.phone_number, number, number_len);
    /* dial_info.card_id = 0; */
    int32_t ret = aic_srv_tele_call_dial(&dial_info, handleCallCallback);
    if (ret) {
        log_error("aic_srv_tele_call_dial err:%d \n", ret);
    }
    return true;
}

bool cat1_get_call_status(u8 *status)
{
    if (false == cat1_check_call_enable()) {
        return false;
    }
    switch (cat1_info.call_status) {
    case TS_CALL_STATUS_ACTIVE:
    case TS_CALL_STATUS_HELD:
        *status = BT_CALL_ACTIVE;
        break;
    case TS_CALL_STATUS_DIALING:
        *status = BT_CALL_OUTGOING;
        break;
    case TS_CALL_STATUS_ALERTING:
        *status = BT_CALL_ALERT;
        break;
    case TS_CALL_STATUS_WAITING:
    case TS_CALL_STATUS_INCOMING:
        *status = BT_CALL_INCOMING;
        break;
    case TS_CALL_STATUS_IDLE:
    case TS_CALL_STATUS_RELEASED:
    default :
        *status = BT_CALL_HANGUP;
        break;
    }
    return true;
}

bool cat1_get_call_phone_num(char **phone_num)
{
    if (false == cat1_check_call_enable()) {
        return false;
    }
    *phone_num = cat1_info.phone_number;

    return true;
}

bool cat1_check_call_enable(void)
{
    if (!cat1_info.power_on) {
        return false;
    }
    if (!cat1_info.radio_is_on) {
        return false;
    }
    if (!cat1_info.is_ims_on) {
        return false;
    }
    if (!cat1_status.lte_call_enable) {
        return false;
    }
    if (cat1_status.is_flight_mode) {
        return false;
    }
    // 开关过程中
    if (cat1_get_lte_can_touch() == 0) {
        return false;
    }
    if (cat1_get_flight_can_touch() == 0) {
        return false;
    }
    return true;
}

void cat1_audio_close(void)
{
    aic_audio_close();
}

int cat1_audio_open(void)
{
    return aic_audio_open();
}

bool cat1_ctrl_poweron(void)
{
    cat1_set_lte_onoff(true);
    return true;
}

bool cat1_ctrl_poweroff(void)
{
    cat1_set_lte_onoff(false);
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

    if (cat1_status.is_flight_mode) {
        if (cat1_status.lte_enable) {
            cat1_set_flight_can_touch(0);
        }
    } else {
        if (!cat1_status.lte_enable) {
            cat1_set_flight_can_touch(0);
        }
    }

    u8 enable = !flag;

    cat1_set_lte_onoff(enable);

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
    cat1_status.lte_net_enable = flag;
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
    uint8_t cur_card_id = TS_CARD_ID_MAX;
    aic_srv_tele_sim_get_current_card(&cur_card_id);
    if (flag) {
        aic_srv_tele_call_volte_enable(cur_card_id, 1);
        call_ctrl_set_call_sel(CALL_SEL_CAT1);
    } else {
        aic_srv_tele_call_volte_enable(cur_card_id, 0);
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
        aic_ctrl_power_on(flag);
        call_ctrl_set_call_sel(CALL_SEL_CAT1);
    } else {
        aic_ctrl_power_on(flag);
        call_ctrl_set_call_sel(CALL_SEL_BT);
    }
}

char *cat1_get_operator(void)
{
    ts_radio_operator_info_t radio_operator_info_t;
    aic_srv_tele_radio_get_operator_info(0, &radio_operator_info_t);
    memcpy(cat1_info.operator_long_name, radio_operator_info_t.operator_long_name, sizeof(radio_operator_info_t.operator_long_name));
    log_info("operator name:%s\n", cat1_info.operator_long_name);
    return cat1_info.operator_long_name;
}

char *cat1_get_smsc(void)
{
    return cat1_info.smsc;
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

int cat1_get_signal_level(void)
{
    if (cat1_info.is_ims_on) {
        return cat1_info.signal_level;
    }
    return -1;
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

int cat1_send_sms(int number_len, char *number, int content_len, char *content)
{
    int res;
    char *p_number = malloc(TS_SMS_NUM_MAX_LEN);
    char *p_text = malloc(TS_SMS_PDU_MAX_LEN);

    if (number_len >= TS_SMS_NUM_MAX_LEN) {
        number_len = TS_SMS_NUM_MAX_LEN - 1;
    }
    memcpy(p_number, number, number_len);
    p_number[number_len] = '\0';
    log_info("sms number:%s\n", p_number);

    if (content_len >= TS_SMS_PDU_MAX_LEN) {
        content_len = TS_SMS_PDU_MAX_LEN - 1;
    }
    memcpy(p_text, content, content_len);
    p_text[content_len] = '\0';
    log_info("sms content:%s\n", p_text);

    ts_sms_send_info_t sms_info = {0};
    sms_info.is_write_to_sim = false;
    sms_info.p_smsc = NULL;
    sms_info.p_number = p_number;
    sms_info.p_text = p_text;
    res = aic_srv_tele_sms_send(0, &sms_info, handleSmsCallback);

    free(p_number);
    free(p_text);

    return res;
}

bool cat1_call_use_modem_volume(void)
{
    return aic_test_get_is_from_engineering_mode();
}

void cat1_call_set_modem_volume(int volume)
{
    aic_srv_voice_set_volume(volume);
}

int cat1_call_get_modem_volume(void)
{
    return aic_srv_voice_get_volume();
}

void cat1_call_input_mute(u8 mute)
{
    aic_audio_input_clear_enable(mute);
}

int cat1_call_get_input_mute_status(void)
{
    return aic_audio_get_input_clear_status();
}

void cat1_init(void)
{
    log_info("%s \n", __func__);

    // 注册唤醒回调
    port_edge_wkup_set_callback_by_index(PORT_WKUP_FALLING_IDX, port_falling_wakeup_callback);
    port_edge_wkup_set_callback_by_index(PORT_WKUP_RISING_IDX, port_rising_wakeup_callback);
    aic_audio_init();

    // aicxtek init
    aic_init();

    // power
    aic_ctrl_state_register(NULL, handleCtrlStateback);
    // tele call init
    cat1_info.client_handle = aic_srv_tele_call_register(NULL, handleCallCallback);
    // tele radio init
    aic_srv_tele_radio_register(NULL, handleRadioCallback);
    // tele sim init
    aic_srv_tele_sim_register(NULL, handleSimCallback);

#if TCFG_CAT1_FUNC_SMS_ENABLE
    aic_srv_tele_sms_register(NULL, handleSmsCallback);
#endif

#if TCFG_CAT1_FUNC_NET_ENABLE
    lwip_pdp_init();
#endif

#if TCFG_CAT1_MODULE_UPDATE_ENABLE
    // update init
    cat1_module_update_init();
#endif

    log_info("%s end \n", __func__);
}

void cat1_close(void)
{
    int to = 3000;

    cat1_set_lte_onoff(false);
    while (cat1_info.power_on != 0) {
        os_time_dly(1);
        if (to < 10) {
            break;
        }
        to -= 10;
    }
}

#if TCFG_CAT1_MODULE_UPDATE_ENABLE
#include "aic_srv_version.h"
#include "aic_fota.h"

#define CAT1_VENDER_AICXTEK          (2)          // 归芯
#define CAT1_MODULE_UPDATE_VENDER    CAT1_VENDER_AICXTEK  // 厂商ID
#define CAT1_MODULE_UPDATE_END_TIMEOUT       (200)  // 升级结束的超时等待 (单位: s)

#define CAT1_MODULE_UPDATE_VERSION_MAX_LEN      AIC_SRV_VERSION_TOTAL_LEN  // 版本号最大长度
#define CAT1_MODULE_UPDATE_VERSION_FMT      "Version:" \
                                            "  %[^\r\n]\r\n" \
                                            "Time:" \
                                            "  %[^\r\n]\r\n" \
                                            "Aic_Version:" \
                                            "  %[^\r\n]\r\n" \
                                            "318_Version:" \
                                            " %[^\r\n]\r\n"


typedef struct cat1_aic_srv_ver_s {
    char cat1_aic_srv_sw_ver[AIC_SRV_VERSION_MAX_LEN];
    char cat1_aic_srv_build_time[AIC_SRV_BUILD_TIME_MAX_LEN];
    char cat1_aic_ver[AIC_SRV_VERSION_MAX_LEN];
    char cat1_aic_318_ver[AIC_SRV_VERSION_MAX_LEN];
} cat1_aic_srv_ver_t;

cat1_power_state_t cat1_module_get_power_state(void)
{
    u8 state = (u8)aic_ctrl_srv_get_state();
    switch (state) {
    case AIC_CTRL_STATE_SHUTDOWN:
    case AIC_CTRL_STATE_BOOTED:
    case AIC_CTRL_STATE_SHUTTING:
    case AIC_CTRL_STATE_BOOTING:
        return CAT1_POWER_STATE_POWEROFF;

    case AIC_CTRL_STATE_AILVE:
    case AIC_CTRL_STATE_FOTA:
        return CAT1_POWER_STATE_POWERON;

    case AIC_CTRL_STATE_CALI:
    case AIC_CTRL_STATE_EXCEPTION:
    case AIC_CTRL_STATE_POWERON_TIMEOUT:
    case AIC_CTRL_STATE_SHUTDOWN_TIMEOUT:
        return CAT1_POWER_STATE_OTHER;
    default:
        log_error(" %s err !!!\n", __func__);
        return CAT1_POWER_STATE_OTHER;
    }
}

u8 cat1_module_can_get_version(void)
{
    return (cat1_module_get_power_state() == CAT1_POWER_STATE_POWERON) ? 1 : 0;
}

u8 cat1_module_get_vender_id(void)
{
    return CAT1_MODULE_UPDATE_VENDER;
}

u16 cat1_module_get_version_total_len(void)
{
    return AIC_SRV_VERSION_MAX_LEN;
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
    cat1_aic_srv_ver_t *cat1_version = (cat1_aic_srv_ver_t *)zalloc(sizeof(cat1_aic_srv_ver_t));
    if ((cat1_version == NULL) || (ver_buf == NULL)) {
        log_error("cat1 ver buf zalloc err !\n");
        return 1;
    }

    aic_get_version(ver_buf, CAT1_MODULE_UPDATE_VERSION_MAX_LEN);

    sscanf(ver_buf, CAT1_MODULE_UPDATE_VERSION_FMT, \
           cat1_version->cat1_aic_srv_sw_ver, \
           cat1_version->cat1_aic_srv_build_time, \
           cat1_version->cat1_aic_ver, \
           cat1_version->cat1_aic_318_ver);

    memset(buf, 0x00, cat1_module_get_version_total_len());
    memcpy(buf, cat1_version->cat1_aic_318_ver, AIC_SRV_VERSION_MAX_LEN);

    log_info("====================================================\r\n");
    log_info("  %s\n", ver_buf);
    log_info(" >%s<\n", cat1_version->cat1_aic_srv_sw_ver);
    log_info(" >%s<\n", cat1_version->cat1_aic_srv_build_time);
    log_info(" >%s<\n", cat1_version->cat1_aic_ver);
    log_info(" >%s<\n", cat1_version->cat1_aic_318_ver);
    log_info("====================================================\r\n");

    free(ver_buf);
    free(cat1_version);
    return 0;
}

// 0:ok; 1:err
int cat1_module_fota_start(u32 packet_size, u32 timeout)
{
    u32 wait_timeout = jiffies + msecs_to_jiffies(timeout);

    if (aic_fota_start(packet_size) == 0) {
        while (time_before(jiffies, wait_timeout)) {
            if (aic_ctrl_srv_get_state() == AIC_CTRL_STATE_FOTA) {
                return 0;
            }
            os_time_dly(1);
        }
    }

    return 1;
}

int cat1_module_fota_end(void)
{
    os_time_dly(5);  // 等待数据传输完成, 模块厂商建议值
    int ret = aic_fota_wait_update_result(CAT1_MODULE_UPDATE_END_TIMEOUT);
    log_info("aic_fota_wait_update_result err %d", ret);
    ret = (AIC_FOTA_UPDATE_COMPLETE == ret) ? 0 : 1;
    return ret;
}

int cat1_module_fota_close(void)
{
    aic_fota_stop();
    return 0;
}

int cat1_module_fota_trans_data(u8 *data, u32 len)
{
    return aic_fota_trans_data(data, len);
}

#endif
#endif
