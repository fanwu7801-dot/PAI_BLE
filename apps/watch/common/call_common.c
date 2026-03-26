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
#include "app_common.h"
#include "btstack/avctp_user.h"
#include "message_vm_cfg.h"
#include "aec_user.h"
#include "aic_audio.h"

extern void __set_bt_esco_by_watch(bool flag);

static int call_sel = CALL_SEL_BT;

void call_dial_number(int number_len, char *number)
{
    int ret = false;
#if TCFG_APP_CAT1_EN
    if ((call_sel == CALL_SEL_AUTO) || (call_sel == CALL_SEL_CAT1)) {
        ret = cat1_call_number(number_len, number);
    }
    if (ret == true) {
        set_call_log_call_sel(CALL_SEL_CAT1);
        return ;
    }
    if (call_sel == CALL_SEL_CAT1) {
        return ;
    }
#endif /* #if TCFG_APP_CAT1_EN */

#if TCFG_APP_BT_EN
    if (get_bt_connect_status() !=  BT_STATUS_WAITINT_CONN) {
        set_call_log_call_sel(CALL_SEL_BT);
        __set_bt_esco_by_watch(1);
        user_send_cmd_prepare(USER_CTRL_DIAL_NUMBER, strlen(number), (u8 *)number);
    }
#endif /* #if TCFG_APP_BT_EN */
}


void call_ctrl_answer()
{
    int ret = false;
#if TCFG_APP_CAT1_EN
    if ((call_sel == CALL_SEL_AUTO) || (call_sel == CALL_SEL_CAT1)) {
        ret = cat1_call_answer();
    }
    if (ret == true) {
        set_call_log_call_sel(CALL_SEL_CAT1);
        return ;
    }
    if (call_sel == CALL_SEL_CAT1) {
        return ;
    }
#endif /* #if TCFG_APP_CAT1_EN */

#if TCFG_APP_BT_EN
    set_call_log_call_sel(CALL_SEL_BT);
    __set_bt_esco_by_watch(1);
    user_send_cmd_prepare(USER_CTRL_HFP_CALL_ANSWER, 0, NULL);
#endif /* #if TCFG_APP_BT_EN */
}

void call_ctrl_hangup()
{
    int ret = false;
#if TCFG_APP_CAT1_EN
    if ((call_sel == CALL_SEL_AUTO) || (call_sel == CALL_SEL_CAT1)) {
        ret = cat1_call_hangup();
    }
    if (ret == true) {
        set_call_log_call_sel(CALL_SEL_CAT1);
        return ;
    }
    if (call_sel == CALL_SEL_CAT1) {
        return ;
    }
#endif

#if TCFG_APP_BT_EN
    set_call_log_call_sel(CALL_SEL_BT);
    user_send_cmd_prepare(USER_CTRL_HFP_CALL_HANGUP, 0, NULL);
#endif
}

char *call_ctrl_get_phone_num(void)
{
    int ret = false;
    char *phone_num = NULL;
#if TCFG_APP_CAT1_EN
    if ((call_sel == CALL_SEL_AUTO) || (call_sel == CALL_SEL_CAT1)) {
        ret = cat1_get_call_phone_num(&phone_num);
    }
    if (ret == true) {
        return phone_num;
    }
    if (call_sel == CALL_SEL_CAT1) {
        return NULL;
    }
#endif /* #if TCFG_APP_CAT1_EN */

#if TCFG_APP_BT_EN
    if (bt_user_priv_var.phone_num_flag) {
        return bt_user_priv_var.income_phone_num;
    }
#endif /* #if TCFG_APP_BT_EN */
    return NULL;
}

u8 call_ctrl_get_status(void)
{
    int ret = false;
    u8 status;

#if TCFG_APP_CAT1_EN
    if ((call_sel == CALL_SEL_AUTO) || (call_sel == CALL_SEL_CAT1)) {
        ret = cat1_get_call_status(&status);
    }
    if (ret == true) {
        return status;
    }
    if (call_sel == CALL_SEL_CAT1) {
        return BT_CALL_HANGUP;
    }
#endif /* #if TCFG_APP_CAT1_EN */

#if TCFG_APP_BT_EN
    return get_call_status();
#endif /* #if TCFG_APP_BT_EN */

    return BT_CALL_HANGUP;
}

void call_ctrl_set_call_sel(int sel)
{
    if (sel >= CALL_SEL_MAX) {
        return ;
    }
    call_sel = sel;
    if (call_sel != CALL_SEL_AUTO) {
        set_call_log_call_sel(call_sel);
    }
}

int call_ctrl_get_call_sel(void)
{
    return call_sel;
}

void call_ctrl_input_mute(u8 mute)
{
#if TCFG_APP_CAT1_EN
    if (true == cat1_check_call_enable()) {
        cat1_call_input_mute(mute);
    } else
#endif /* #if TCFG_APP_CAT1_EN */
    {
        aec_input_clear_enable(mute);
    }
}

int call_ctrl_get_input_mute(void)
{
#if TCFG_APP_CAT1_EN
    if (true == cat1_check_call_enable()) {
        return cat1_call_get_input_mute_status();
    } else
#endif /* #if TCFG_APP_CAT1_EN */
    {
        return aec_get_input_clear_status();
    }
    return 0;
}

