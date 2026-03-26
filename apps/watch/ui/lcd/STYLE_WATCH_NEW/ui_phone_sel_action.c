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


#if TCFG_APP_BT_EN && TCFG_APP_CAT1_EN

#define STYLE_NAME  JL

#define ui_grid_for_id(id) \
	({ \
		struct element *elm = ui_core_get_element_by_id(id); \
	 	elm ? container_of(elm, struct ui_grid, elm): NULL; \
	 })


//----------------通话选择界面-------------------//
#if TCFG_APP_BT_EN
static int mobile_select_ontouch(void *ctr, struct element_touch_event *e)
{
    struct ui_text *text = (struct ui_text *)ctr;
    extern u8 *get_call_log_number(void);
    extern void __set_bt_esco_by_watch(bool flag);

    switch (e->event) {
    case ELM_EVENT_TOUCH_UP:
        printf("mobile select\n");
        u8 *number = get_call_log_number();
        if (get_bt_connect_status() !=  BT_STATUS_WAITINT_CONN) {
            __set_bt_esco_by_watch(1);
            user_send_cmd_prepare(USER_CTRL_DIAL_NUMBER, strlen(number), (u8 *)number);
            UI_WINDOW_BACK_DEL(ID_WINDOW_CALL_CHANNEL_SEL);
        }
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
REGISTER_UI_EVENT_HANDLER(CAT1_BUTTON_MOBILE)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = mobile_select_ontouch,
};
#endif /* #if TCFG_APP_BT_EN */

#if TCFG_APP_CAT1_EN
static int watch_select_ontouch(void *ctr, struct element_touch_event *e)
{
    struct ui_text *text = (struct ui_text *)ctr;
    extern int cat1_call_number(int number_len, char *number);

    switch (e->event) {
    case ELM_EVENT_TOUCH_UP:
        printf("watch select\n");
        u8 *number = get_call_log_number();
        int ret = cat1_call_number(strlen(number), number);
        if (ret == true) {
            UI_WINDOW_BACK_DEL(ID_WINDOW_CALL_CHANNEL_SEL);
        }
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
REGISTER_UI_EVENT_HANDLER(CAT1_BUTTON_WATCH)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = watch_select_ontouch,
};
#endif /* #if TCFG_APP_CAT1_EN */

static int cancel_button_ontouch(void *ctr, struct element_touch_event *e)
{
    struct ui_text *text = (struct ui_text *)ctr;

    switch (e->event) {
    case ELM_EVENT_TOUCH_UP:
        printf("cancel\n");
        UI_WINDOW_BACK_SHOW(2);
        return true;
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
REGISTER_UI_EVENT_HANDLER(CAT1_BUTTON_CANCEL)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = cancel_button_ontouch,
};


#endif








