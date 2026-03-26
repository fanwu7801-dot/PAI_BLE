#include "ui_action_findmy_setting.h"
#include "app_config.h"
#include "ui/ui.h"
#include "ui/ui_style.h"
#include "syscfg_id.h"
#include "system/timer.h"
#include "ui/ui_api.h"
#include "ui/res_config.h"
#include "ui/ui_resource.h"
#include "ui/ui_sys_param.h"
#include "ui/watch_syscfg_manage.h"
#include "vm.h"
#include "asm/sfc_spi.h"
#include "asm/sfc_norflash_api.h"
#include "system/init.h"
#include "ble_fmy_fmna.h"

#if TCFG_UI_ENABLE && (!TCFG_LUA_ENABLE)
#if FINDMY_EN

#define STYLE_NAME  JL
#define SOFTWARE_AUTH_UUID_BLEN                       16
#define TOKEN_VM_SAVE_LEN                   		  1024
#define CFG_FMY_INFO                     			  205
#define  FIRST_INIT               					  0

typedef struct {
    u8 find_my_opend_flag: 4;//findmy广播标志位
    u8 find_my_bind_flag: 4;//findmy是否完成配对标志位
    u8 find_my_binding_flag: 4;//findmy是否处于绑定过程
    u8 find_my_connecting_flag: 4;//findmy是否处于连接过程
    u16 findmy_return_set_id;//记录定时ID
    u16 check_cnt;//定时器计数值
    u16 findmy_id;//记录定时器ID
} findmy_state;
static findmy_state findmy_flag;

static void findmy_pair_success();
static void findmy_unpair_success();
static void findmy_binding_time();
static void findmy_return_setting();
static void set_findmy_connecting_flag(const char *type, u32 flag);

static const struct uimsg_handl ui_msg_handler[] = {
    {"PAIR_SUCCESS", findmy_pair_success },
    {"UNPAIR_SUCCESS", findmy_unpair_success },
    {"SET_CONNECTING_FLAG", set_findmy_connecting_flag},
    {NULL, NULL},
};

static void set_findmy_connecting_flag(const char *type, u32 flag)
{
    findmy_flag.find_my_connecting_flag = flag;
}

u8 is_findmy_connecting_flag()
{
    return findmy_flag.find_my_connecting_flag;
}

static void toogle_findmy_open_flag()
{
    u8 vm_flag[3];
    syscfg_read(CFG_FMY_INFO, vm_flag, 3);
    findmy_flag.find_my_opend_flag = vm_flag[2];
    findmy_flag.find_my_opend_flag = findmy_flag.find_my_opend_flag ^ BIT(0);
}

u8 is_findmy_open()
{
    static first_flag = 0;

    if (first_flag == 0) {
        u8 vm_flag[3];
        syscfg_read(CFG_FMY_INFO, vm_flag, 3);
        findmy_flag.find_my_opend_flag = vm_flag[2];
        first_flag = 1;
    }

    return findmy_flag.find_my_opend_flag;
}

u8 is_findmy_bind()
{
    return fmy_get_pair_state();
}

static int findmy_set_ontouch(void *ctr, struct element_touch_event *e)
{
    struct ui_grid *grid = (struct ui_grid *)ctr;
    static u8 touch_action = 0;
    int sel_item;

    switch (e->event) {
    case ELM_EVENT_TOUCH_UP:
        if (touch_action != 1) {
            break;
        }
        sel_item = ui_grid_cur_item(grid);
        if (sel_item <= 0) {
            break;
        }
        switch (sel_item) {
        case 1:
            toogle_findmy_open_flag();
            if (is_findmy_open()) {
                fmy_enable(1);
                ui_text_show_index_by_id(FINDMY_PAIRING_BUTTON, is_findmy_bind());//这里需要预留接口去判断
                ui_pic_show_image_by_id(PIC_FINDMY_SWITCH, 1);
            } else {
                fmy_enable(0);
                ui_hide(FINDMY_PAIRING_BUTTON);
                ui_pic_show_image_by_id(PIC_FINDMY_SWITCH, 0);
            }
            break;
        }
        break;
    case ELM_EVENT_TOUCH_HOLD:
        break;
    case ELM_EVENT_TOUCH_MOVE:
        touch_action = 2;
        break;
    case ELM_EVENT_TOUCH_DOWN:
        touch_action = 1;
        break;
    case ELM_EVENT_TOUCH_U_MOVE:
        break;
    case ELM_EVENT_TOUCH_D_MOVE:
        break;
    }

    return false;
}

static void text_name_change(void *p)
{
    ui_text_show_index_by_id(FINDMY_PAIRING_BUTTON, is_findmy_bind());
}

static int findmy_switch_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct ui_pic *pic = (struct ui_pic *)_ctrl;

    switch (event) {
    case ON_CHANGE_INIT:
        if (!is_findmy_open()) {
            printf("findmy_toogle=%d", is_findmy_open());
            ui_pic_show_image(pic, 0);
        } else {
            printf("findmy_toogle=%d", is_findmy_open());
            sys_timeout_add(NULL, text_name_change, 1);
            ui_pic_show_image(pic, 1);
        }
        ui_register_msg_handler(ID_WINDOW_FINDMY_SETTING, ui_msg_handler);
        break;
    }

    return 0;
}

static int findmy_binding_baseform_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    switch (event) {
    case ON_CHANGE_INIT:
        findmy_flag.check_cnt = 0;
        findmy_flag.findmy_id = sys_timer_add(NULL, findmy_binding_time, 200);
        set_findmy_connecting_flag(NULL, 0);
        ui_auto_shut_down_disable();
        break;
    case ON_CHANGE_RELEASE:
        fmy_open_close_pairing_mode(0);
        sys_timer_del(findmy_flag.findmy_id);
        ui_auto_shut_down_enable();
        break;
    }

    return 0;
}

void findmy_binding_time(void)
{
    struct element *elm = NULL;
    u8 binding_baseform_invisible = 1;

    findmy_flag.check_cnt++;//时间计数值，30s超时判断配对失败
    ui_pic_show_image_by_id(FINDMY_BINDING, findmy_flag.check_cnt % 10);
    if (((findmy_flag.check_cnt >= 150) && (findmy_flag.find_my_connecting_flag == 0)) ||
        ((findmy_flag.check_cnt >= 200) && (findmy_flag.find_my_connecting_flag == 1))) { //30s超时配对失败 或者如果检测到正在连接增加多5s来保证配对过程UI不会错乱
        findmy_flag.check_cnt = 0;
        fmy_open_close_pairing_mode(0);
        elm = ui_core_get_element_by_id(FINDMY_BINDING_BASEFORM);
        if (elm != NULL) {
            binding_baseform_invisible = elm->css.invisible;
        }
        if (binding_baseform_invisible == 0) {
            ui_hide(FINDMY_BINDING_BASEFORM);
            ui_show(FINDMY_BIND_FAIL);
        }
        findmy_flag.findmy_return_set_id = sys_timeout_add(NULL, findmy_return_setting, 1000); //记录ID 退出删除。
        return;
    }
}

//findmy绑定成功事件回调函数，显示绑定成功的UI
void findmy_pair_success()
{
    struct element *elm = NULL;
    u8 binding_baseform_invisible = 1;

    elm = ui_core_get_element_by_id(FINDMY_BINDING_BASEFORM);
    if (elm != NULL) {
        binding_baseform_invisible = elm->css.invisible;
    }
    if (binding_baseform_invisible == 0) {
        ui_hide(FINDMY_BINDING_BASEFORM);
        ui_show(FINDMY_BIND_SUCCEED);
    }
    findmy_flag.check_cnt = 0;
    findmy_flag.findmy_return_set_id = sys_timeout_add(NULL, findmy_return_setting, 1000);
}

//findmy绑定失败事件回调函数，显示绑定失败的UI
static void findmy_unpair_success()
{
    if (is_findmy_open()) {
        ui_text_show_index_by_id(FINDMY_PAIRING_BUTTON, 0);//把UI字体更新为开始配对
    }
}

static int findmy_binding_ontouch(void *ctr, struct element_touch_event *e)
{
    return true;//在绑定过程界面不允许右划 左划退出
}

static int pairing_button_ontouch(void *ctr, struct element_touch_event *e)
{
    switch (e->event) {
    case ELM_EVENT_TOUCH_UP:
        if (is_findmy_bind()) {
            ui_show(FINDMY_CANCEL);
            ui_hide(FINDMY_SETTING);
        } else {
            fmy_open_close_pairing_mode(1);
            ui_show(FINDMY_BINDING_BASEFORM);
            ui_hide(FINDMY_SETTING);
        }
        break;
    }

    return false;
}

static int findmy_cancle_no_ontouch(void *ctr, struct element_touch_event *e)
{
    switch (e->event) {
    case ELM_EVENT_TOUCH_UP:
        ui_show(FINDMY_SETTING);
        ui_hide(FINDMY_CANCEL);
        break;
    }

    return false;
}

static int findmy_cancle_yes_ontouch(void *ctr, struct element_touch_event *e)
{
    switch (e->event) {
    case ELM_EVENT_TOUCH_UP:
        fmy_open_close_pairing_mode(0);
        fmy_factory_reset();
        ui_show(FINDMY_SETTING);
        ui_hide(FINDMY_CANCEL);
        break;
    }

    return false;
}

static int findmy_cancel_ontouch(void *ctr, struct element_touch_event *e)
{
    switch (e->event) {
    case ELM_EVENT_TOUCH_R_MOVE:
        ui_hide(FINDMY_CANCEL);
        ui_show(FINDMY_SETTING);
        break;
    }

    return true;
}

void findmy_return_setting()
{
    findmy_flag.findmy_return_set_id = 0;
    if (is_findmy_bind()) {
        ui_hide(FINDMY_BIND_SUCCEED);
        ui_show(FINDMY_SETTING);
    } else {
        ui_hide(FINDMY_BIND_FAIL);
        ui_show(FINDMY_SETTING);
    }
    ui_text_show_index_by_id(FINDMY_PAIRING_BUTTON, is_findmy_bind());//刷新一下开始配对的字体，防止刷新不成功。
}

static int findmy_bind_fail_baseform_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    switch (event) {
    case ON_CHANGE_RELEASE:
        if (findmy_flag.findmy_return_set_id != 0) {
            sys_timeout_del(findmy_flag.findmy_return_set_id);
            findmy_flag.findmy_return_set_id = 0;
        }
        break;
    }

    return 0;
}

static int findmy_bind_succ_baseform_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    switch (event) {
    case ON_CHANGE_RELEASE:
        if (findmy_flag.findmy_return_set_id != 0) {
            sys_timeout_del(findmy_flag.findmy_return_set_id);
            findmy_flag.findmy_return_set_id = 0;
        }
        break;
    }

    return 0;
}

// *INDENT-OFF*
REGISTER_UI_EVENT_HANDLER(VLIST_FINDMY_SETTING)//设置-findmy初始界面
.onchange = NULL,
.onkey = NULL,
.ontouch = findmy_set_ontouch,
};
REGISTER_UI_EVENT_HANDLER(PIC_FINDMY_SWITCH)//设置-findmy开关按钮
.onchange = findmy_switch_onchange, //,
.onkey = NULL,
.ontouch = NULL,
};
REGISTER_UI_EVENT_HANDLER(FINDMY_BINDING_BASEFORM)//findmy绑定界面
.onchange = findmy_binding_baseform_onchange,
.onkey = NULL,
.ontouch = findmy_binding_ontouch,
};
REGISTER_UI_EVENT_HANDLER(FINDMY_PAIRING_BUTTON)//配对开关控制
.onchange = NULL,
.onkey = NULL,
.ontouch = pairing_button_ontouch,
};
REGISTER_UI_EVENT_HANDLER(FINDMY_CANCLE_NO)//取消配对-否
.onchange = NULL,
.onkey = NULL,
.ontouch = findmy_cancle_no_ontouch,
};
REGISTER_UI_EVENT_HANDLER(FINDMY_CANCLE_YES)//取消配对-是
.onchange = NULL,
.onkey = NULL,
.ontouch = findmy_cancle_yes_ontouch,
};
REGISTER_UI_EVENT_HANDLER(FINDMY_CANCEL)//取消配对界面
.onchange = NULL,
.onkey = NULL,
.ontouch = findmy_cancel_ontouch,
};
REGISTER_UI_EVENT_HANDLER(FINDMY_BIND_FAIL)//取消配对界面
.onchange = findmy_bind_fail_baseform_onchange,
.onkey = NULL,
.ontouch = NULL,
};
REGISTER_UI_EVENT_HANDLER(FINDMY_BIND_SUCCEED)//取消配对界面
.onchange = findmy_bind_succ_baseform_onchange,
.onkey = NULL,
.ontouch = NULL,
};
#endif
#endif

