#include "app_config.h"
#include "ui/ui_style.h"
#include "ui/ui.h"
#include "ui/ui_api.h"
#include "app_task.h"
#include "system/timer.h"
#include "device/device.h"
#include "key_event_deal.h"
#include "res/resfile.h"
#include "ui/res_config.h"
#include "ui/ui_resource.h"
#include "system/includes.h"
#include "audio_config.h"
#include "asm/mcpwm.h"
#include "ui/ui_sys_param.h"
#include "ui/watch_syscfg_manage.h"
#include "font/language_list.h"
#include "bt_common.h"
#include "btstack/btstack_task.h"
#include "btstack/avctp_user.h"
#include "custom_cfg.h"
#include "product_info_api.h"
#include "cat1/cat1_common.h"

#if TCFG_APP_CAT1_EN && TCFG_CAT1_FUNC_SMS_ENABLE

#define STYLE_NAME  JL

#define ui_grid_for_id(id) \
	({ \
		struct element *elm = ui_core_get_element_by_id(id); \
	 	elm ? container_of(elm, struct ui_grid, elm): NULL; \
	 })

//-----------------------短信通话选择界面-------------------------//
static int sms_call_sel_onchange(void *ctr, enum element_change_event e, void *arg)
{
    struct window *window = (struct window *)ctr;
    switch (e) {
    case ON_CHANGE_INIT:
        break;
    case ON_CHANGE_RELEASE:
        //clear_send_sms();
        UI_WINDOW_BACK_DEL(ID_WINDOW_CAT1_CALL_SMS_SEL);
        break;
    default:
        return false;
    }
    return false;
}
REGISTER_UI_EVENT_HANDLER(ID_WINDOW_CAT1_CALL_SMS_SEL)
.onchange = sms_call_sel_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

static int sms_sel_ontouch(void *ctr, struct element_touch_event *e)
{
    struct ui_text *text = (struct ui_text *)ctr;

    switch (e->event) {
    case ELM_EVENT_TOUCH_UP:
        printf("sms select\n");
        //u8 *number = get_call_log_number();
        UI_HIDE_CURR_WINDOW();
        UI_SHOW_WINDOW(ID_WINDOW_CAT1_SMS_REPLY);

        break;
    case ELM_EVENT_TOUCH_HOLD:
        break;
    case ELM_EVENT_TOUCH_MOVE:
        break;
    case ELM_EVENT_TOUCH_DOWN:
        return true;
    case ELM_EVENT_TOUCH_U_MOVE:
        break;
    case ELM_EVENT_TOUCH_D_MOVE:
        break;
    }

    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_SMS_SEL)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = sms_sel_ontouch,
};


static int call_sel_ontouch(void *ctr, struct element_touch_event *e)
{
    struct ui_text *text = (struct ui_text *)ctr;
    extern struct s_cat1_sms_info cat1_sms_info;
    extern void call_dial_number(int number_len, char *number);

    switch (e->event) {
    case ELM_EVENT_TOUCH_UP:
        printf("call select\n");
        //u8 *number = get_call_log_number();
#if TCFG_APP_BT_EN && TCFG_APP_CAT1_EN
        if ((get_bt_connect_status() !=  BT_STATUS_WAITINT_CONN) && (true == cat1_check_call_enable())) {
            // bt & cat1 online
            UI_HIDE_CURR_WINDOW();
            UI_SHOW_WINDOW(ID_WINDOW_CALL_CHANNEL_SEL);
            return true;
        }
#endif /* #if TCFG_APP_CAT1_EN */

        call_dial_number(strlen(cat1_get_send_sms_number()) + 1, (u8 *)cat1_get_send_sms_number());
        //clear_send_sms();

        break;
    case ELM_EVENT_TOUCH_HOLD:
        break;
    case ELM_EVENT_TOUCH_MOVE:
        break;
    case ELM_EVENT_TOUCH_DOWN:
        return true;
    case ELM_EVENT_TOUCH_U_MOVE:
        break;
    case ELM_EVENT_TOUCH_D_MOVE:
        break;
    }

    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_CALL_SEL)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = call_sel_ontouch,
};


static int dial_sms_number_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct ui_text *text = (struct ui_text *)_ctrl;
    //u8 *number = get_call_log_number();
    extern struct s_cat1_sms_info cat1_sms_info;

    switch (event) {
    case ON_CHANGE_INIT:
        switch (text->elm.id) {
        case CAT1_TEXT_DIAL_MSG_NUMBER:
            ui_text_set_text_attrs(text, cat1_get_send_sms_number(), strlen(cat1_get_send_sms_number()), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        }
        break;
    case ON_CHANGE_RELEASE:
        break;
    }
    return 0;
}
REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_DIAL_MSG_NUMBER)
.onchange = dial_sms_number_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

#endif // TCFG_APP_CAT1_EN

