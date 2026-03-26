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
#include "btstack/avctp_user.h"
#include "aic_audio.h"
#include "call_common.h"

#if TCFG_APP_CAT1_EN

#define LOG_TAG_CONST       APP_CAT1
#define LOG_TAG             "[APP_CAT1]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"

static u8 temp_a2dp_en_flag = 0;

extern u8 get_call_status();
extern void lmp_esco_rejust_establish(u8 value);

static void app_cat1_task_event_to_user(u8 event)
{
    struct sys_event e;
    e.type = SYS_KEY_EVENT;
    e.u.key.event = event;
    e.u.key.value = 0;
    sys_event_notify(&e);
}

int app_cat1_check_support(void)
{
    u8 cur_task = app_get_curr_task();
    switch (cur_task) {
    case APP_POWERON_TASK:
    case APP_POWEROFF_TASK:
    case APP_WATCH_UPDATE_TASK:
    case APP_SMARTBOX_ACTION_TASK:
        log_error("cur task:%d no support \n", cur_task);
        return false;
    /* break; */
    default:
        break;
    }
    if (bt_must_work()) {
        log_error("bt busy \n");
        return false;
    }
    return true;
}

static void app_cat1_task_start(void)
{
    ///按键使能
    sys_key_event_enable();

    UI_WINDOW_PREEMPTION_POSH(ID_WINDOW_PHONE, NULL, NULL, UI_WINDOW_PREEMPTION_TYPE_PHONE);

#if TCFG_APP_BT_EN
    lmp_esco_rejust_establish(1); // 挂起蓝牙esco链路
    temp_a2dp_en_flag = bt_get_a2dp_en_status();
    bt_set_a2dp_en_status(0);
#endif /* #if TCFG_APP_BT_EN */
}

static void app_cat1_task_stop(void)
{
    u8 status;
    int ret = false;
    ret = cat1_get_call_status(&status);
    if ((ret == true) && (status != BT_CALL_HANGUP)) {
        cat1_call_hangup();
    }

    cat1_audio_close();

#if TCFG_APP_BT_EN
    lmp_esco_rejust_establish(0); // 恢复蓝牙esco链路
    bt_set_a2dp_en_status(temp_a2dp_en_flag);
#endif /* #if TCFG_APP_BT_EN */

    UI_WINDOW_PREEMPTION_POP(ID_WINDOW_PHONE);
}

int app_cat1_audio_start(void)
{
    if (APP_CAT1_TASK != app_get_curr_task()) {
        return -1;
    }
    app_cat1_task_event_to_user(KEY_MUSIC_PLAYER_START);
    return 0;
}

int app_cat1_audio_stop(void)
{
    if (APP_CAT1_TASK != app_get_curr_task()) {
        return -1;
    }
    app_cat1_task_event_to_user(KEY_MUSIC_PLAYER_END);
    return 0;
}

static int app_cat1_task_event_handle(struct sys_event *event)
{
    switch (event->type) {
    case SYS_KEY_EVENT:
        switch (event->u.key.event) {
        case KEY_MUSIC_PLAYER_START:
            cat1_audio_open();
            break;
        case KEY_MUSIC_PLAYER_END:
            cat1_audio_close();
            break;
        }
        break;
    default://switch (event->type)
        break;
    }

    return 0;
}

void app_cat1_task(void)
{
    int msg[32];

    app_cat1_task_start();

    while (1) {
        app_task_get_msg(msg, ARRAY_SIZE(msg), 1);

        switch (msg[0]) {
        case APP_MSG_SYS_EVENT:
            if (app_cat1_task_event_handle((struct sys_event *)(msg + 1)) == false) {
                app_default_event_deal((struct sys_event *)(&msg[1]));
            }
            break;
        default:
            break;
        }

        if (app_task_exitting()) {
            app_cat1_task_stop();
            return;
        }
    }
}


#endif



