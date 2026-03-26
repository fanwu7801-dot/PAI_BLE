/* Copyright(C)
 * not free
 * All right reserved
 *
 * @file ui_demo_inertial.c
 * @brief 惯性模拟示例demo
 * @author zhuhaifang@zh-jieli.com
 * @version V1.0.0
 * @date 2023-09-11
 */

#include "app_config.h"
#include "system/includes.h"
#include "ui/ui_sys_param.h"
#include "ui/ui_style.h"
#include "ui/ui.h"
#include "ui/ui_api.h"
#include "app_task.h"
#include "system/timer.h"
#include "key_event_deal.h"
#include "res/resfile.h"
#include "ui/res_config.h"

#define TCFG_DEMO_UI_INERTIAL_ENABLE	0	// demo 使能

#if TCFG_DEMO_UI_INERTIAL_ENABLE

#if TCFG_UI_ENABLE && (!TCFG_LUA_ENABLE)
#define STYLE_NAME  JL

#include "math.h"
#include "ui_expand/ui_bezier.h"
#include "ui_expand/ui_inertial.h"

/* 定义模拟惯性的UI控件 */
#define UI_INERTIAL_CONTROL		INERTIAL_PIC0

#define UI_INERTIAL_PARENT_W	454
#define UI_INERTIAL_PARENT_H	454
#define UI_INERTIAL_PARENT		BASEFORM_1253

/* 按键或编码器步进量 */
#define UI_INERTIAL_KEY_STEP	10


struct ui_demo_inertial_prv {
    struct position pos;
    pUiInertial_t inertial;
};
static struct ui_demo_inertial_prv inertial_prv = {0};
#define __this		(&inertial_prv)



static int ui_inertial_callback(void *hdl, void *object, int offset)
{
    struct element *elm = NULL;
    pUiInertial_t inertial = (pUiInertial_t)hdl;

    /* elm = ui_core_get_element_by_id(UI_INERTIAL_CONTROL); */
    if (inertial->type == UI_INERTIAL_VELOCITY) {
        elm = (struct element *)object;
    } else {
        elm = ui_core_get_element_by_id((u32)object);
    }

    if (!elm) {
        /* 如果拿到句柄为空，停止惯性滚动。这里返回false时会停止惯性定时器 */
        return false;
    }

    int css_offset = offset * 10000 / UI_INERTIAL_PARENT_H;
    elm->css.top += css_offset;

    if (elm->css.top < 0) {
        elm->css.top = 10000;
    } else if (elm->css.top > 10000) {
        elm->css.top = 0;
    }

    ui_redraw(UI_INERTIAL_PARENT);
    return true;    // 返回true继续滚动，返回false强制停止
}

// onchange事件回调
static int ui_demo_inertial_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct rect rect = {0};
    struct element *elm = (struct element *)_ctrl;
    switch (event) {
    case ON_CHANGE_INIT:
        ui_core_get_element_abs_rect(elm, &rect);

        /* 设置left在屏幕中间居中 */
        elm->css.left = (UI_INERTIAL_PARENT_W - rect.width) / 2 * 10000 / UI_INERTIAL_PARENT_W;

        /* 惯性模拟模块初始化，内部申请缓存，控件的句柄作为object */
        __this->inertial = ui_inertial_init(NULL, _ctrl, ui_inertial_callback);
        break;
    case ON_CHANGE_RELEASE:
        /* 惯性模拟模块释放 */
        ui_inertial_free(__this->inertial);
        __this->inertial = NULL;
        break;
    }
    return false;
}

static int ui_demo_inertial_ontouch(void *ctr, struct element_touch_event *e)
{
    struct element *elm = (struct element *)ctr;

    switch (e->event) {
    case ELM_EVENT_TOUCH_DOWN:	// 按下
        /* __this->pos.x = e->pos.x; */
        __this->pos.y = e->pos.y;
        ui_inertial_stop(__this->inertial);	// 如果有惯性正在滚动，按下时停止滚动
        return true;
    case ELM_EVENT_TOUCH_MOVE:	// 滑动
        /* int xoffset = e->pos.x - __this->pos.x; */
        int yoffset = e->pos.y - __this->pos.y;
        /* __this->pos.x = e->pos.x; */
        __this->pos.y = e->pos.y;
        /* elm->css.left += xoffset * 10000 / UI_INERTIAL_PARENT_W; */
        elm->css.top  += yoffset * 10000 / UI_INERTIAL_PARENT_H;

        ui_redraw(UI_INERTIAL_PARENT);
        break;
    case ELM_EVENT_TOUCH_R_MOVE:	// 右滑(防止左右滑动时触发返回上一层页面)
        return true;
    case ELM_EVENT_TOUCH_UP:	// 抬起
        break;
    case ELM_EVENT_TOUCH_ENERGY:	// 惯性
        /* printf("ELM_EVENT_TOUCH_ENERGY\n"); */
        int velocity = e->pos.y >> 16;
        /* 启动速度惯性 */
        ui_inertial_move_velocity(__this->inertial, velocity);
        break;
    }

    return false;
}

static int ui_demo_inertial_onkey(void *ctr, struct element_key_event *e)
{
    switch (e->value) {
    case KEY_UI_MINUS:
        /* 正向滚动 */
        ui_inertial_move_velocity(__this->inertial, UI_INERTIAL_KEY_STEP);
        return true;    // 接管onkey事件
    case KEY_UI_PLUS:
        /* 反向滚动 */
        ui_inertial_move_velocity(__this->inertial, -UI_INERTIAL_KEY_STEP);
        return true;    // 接管onkey事件
    default:
        break;
    }
    return false;   // 其他onkey事件不接管
}
REGISTER_UI_EVENT_HANDLER(UI_INERTIAL_CONTROL)
.onchange = ui_demo_inertial_onchange,
 .onkey = ui_demo_inertial_onkey,
  .ontouch = ui_demo_inertial_ontouch,
};





#endif
#endif



