/* Copyright(C)
 * not free
 * All right reserved
 *
 * @file ui_demo_app_dock.c
 * @brief 水果新版瀑布流菜单DEMO
 * 说明：
 * 1. UI 工程设计，在布局内放置图片控件即可，无需手动摆放位置，程序代码自动摆放
 * 2. UI 效果主要在函数 waterfall_menu_icon_set_pos，如果客户需调整缩放效果等修改，均可直接修改这个函数实现
 * 3. 进入具体APP需客户在 enter_app 函数中添加，根据每个图标对应的APP页面进行跳转，本DEMO实现了获取被点击的图标ID
 * 4. 本demo使用模拟ScrollView模型，API参考ui_scrollview.h声明和注释
 * @author zhuhaifang@zh-jieli.com
 * @version V1.0.0
 * @date 2023-07-05
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

#define TCFG_DEMO_WATERFALL_MENU_ENABLE		0	// demo使能

#if TCFG_DEMO_WATERFALL_MENU_ENABLE
#if TCFG_UI_ENABLE && (!TCFG_LUA_ENABLE)

#define STYLE_NAME  JL

/* 扩展功能使用的头文件 */
#include "math.h"
#include "ui_expand/ui_extend.h"
#include "ui_expand/ui_bezier.h"
#include "ui_expand/ui_scrollview.h"

/* 父布局ID */
#define WATERFALL_LAYOUT	WATERFALL_MENU_LAYOUT

/* 父布局坐标及宽高 */
#define DEMO_LAYOUT_X	57
#define DEMO_LAYOUT_Y	75
#define DEMO_LAYOUT_W	304
#define DEMO_LAYOUT_H	304

/* 图标控件宽高 */
#define PIC_ICON_W		76
#define PIC_ICON_H		76

/* 图标之间的间隔 */
#define ICON_GAP_W		0
#define ICON_GAP_H		0

/* 图标单元的宽高(包含间隔) */
#define ICON_UNIT_W		(PIC_ICON_W + ICON_GAP_W)
#define ICON_UNIT_H		(PIC_ICON_H + ICON_GAP_H)

/* 图标宽高加间隔的一半 */
#define HALF_ICON_W		(ICON_UNIT_W / 2)
#define HALF_ICON_H		(ICON_UNIT_H / 2)


/* 私有变量 */
struct waterfall_menu {
    u32 icon_pic_w;
    u32 icon_pic_h;

    u32 icon_w;
    u32 icon_h;

    u16 line_icon;	// 一行的图标数量
    int move_dir;	// 划动方向

    struct position pos;
    ScrollView_t *view;
};
static struct waterfall_menu waterfall = {0};
#define __this	(&waterfall)


// 计算left需要移动的距离
static inline int left_move(float ratio)
{
    if ((ratio > 1.0) || (ratio < 0.0)) {
        return 0;
    }
    return (__this->icon_pic_w - (__this->icon_pic_w * ratio))/*  / 2 */;
}


// 计算top需要移动的距离
static inline int top_move(float ratio)
{
    if ((ratio > 1.0) || (ratio < 0.0)) {
        return 0;
    }
    return (__this->icon_pic_h - (__this->icon_pic_h * ratio))/*  / 2 */;
}

// 设置图标坐标、倍率、坐标偏移
static int waterfall_menu_icon_set_pos(int x, int y)
{
    int row = 0;
    int col = 0;

    int is_odd_col = 0;	// 是否为奇数列标志
    int half_icon_w = __this->icon_w / 2;	// 图标宽度的一半，用于奇偶行交错摆放

    int pos_x = x * 10000 / DEMO_LAYOUT_W;
    int pos_y = y * 10000 / DEMO_LAYOUT_H;

    int pix_x = 0;
    int pix_y = 0;

    int dw = DEMO_LAYOUT_W - ICON_UNIT_W;	// 右边开始缩放的阈值
    int dh = DEMO_LAYOUT_H - ICON_UNIT_H;	// 底边开始缩放的阈值

    float ratio = 0.0;

    struct element *p = NULL;
    struct element *elm = ui_core_get_element_by_id(WATERFALL_LAYOUT);

    if (elm) {
        list_for_each_child_element(p, (struct element *)elm) {
            if (ui_id2type(p->id) == CTRL_TYPE_PIC) {
                p->css.ratio.en = 0;
                p->css.invisible = false;

                /* 计算图标的位置，直接用转换后的图标，降低减法和除法使用; 同时统一误差，避免划动时图标跳动 */
                p->css.left = row * __this->icon_w + is_odd_col * half_icon_w + pos_x;
                p->css.top = col * __this->icon_h + pos_y;

                /* 计算图标所在位置的像素 */
                pix_x = row * ICON_UNIT_W + is_odd_col * HALF_ICON_W + x;
                pix_y = col * ICON_UNIT_H + y;

                /* 计算图标的缩放倍率，同时根据缩放倍率调整坐标位置，UI视觉效果上的主要难点部分 */
                if ((pix_x < 0) && (pix_y < 0)) {
                    if (abs(pix_x) > abs(pix_y)) {
                        ratio = 1.0 - (float)(-pix_x) / ICON_UNIT_W;
                        p->css.left += left_move(ratio);
                        p->css.top += top_move(ratio) / 2;
                        if (p->css.top < 0) {
                            p->css.top = 0;
                        }
                    } else {
                        ratio = 1.0 - (float)(-pix_y) / ICON_UNIT_H;
                        p->css.top += top_move(ratio);
                        p->css.left += left_move(ratio) / 2;
                        if (p->css.left < 0) {
                            p->css.left = 0;
                        }
                    }
                    p->css.width = __this->icon_pic_w * ratio;
                    p->css.height = __this->icon_pic_h * ratio;
                } else if (pix_x < 0) {
                    if (abs(pix_x) > (pix_y - dh)) {
                        ratio = 1.0 - (float)(-pix_x) / ICON_UNIT_W;
                        p->css.width = __this->icon_pic_w * ratio;
                        p->css.height = __this->icon_pic_h * ratio;
                        p->css.left += left_move(ratio);
                        p->css.top += top_move(ratio) / 2;
                        if (p->css.top + p->css.height > 10000) {
                            p->css.top = 10000 - p->css.height;
                        }
                    } else {
                        ratio = 1.0 - (float)(pix_y - dh) / ICON_UNIT_H;
                        p->css.width = __this->icon_pic_w * ratio;
                        p->css.height = __this->icon_pic_h * ratio;
                        p->css.left += left_move(ratio) / 2;
                        if (p->css.left < 0) {
                            p->css.left = 0;
                        }
                    }
                } else if (pix_y < 0) {
                    if (abs(pix_y) > (pix_x - dw)) {
                        ratio = 1.0 - (float)(-pix_y) / ICON_UNIT_H;
                        p->css.width = __this->icon_pic_w * ratio;
                        p->css.height = __this->icon_pic_h * ratio;
                        p->css.top += top_move(ratio);
                        p->css.left += left_move(ratio) / 2;
                        if (p->css.left + p->css.width > 10000) {
                            p->css.left = 10000 - p->css.width;
                        }
                    } else {
                        ratio = 1.0 - (float)(pix_x - dw) / ICON_UNIT_W;
                        p->css.width = __this->icon_pic_w * ratio;
                        p->css.height = __this->icon_pic_h * ratio;
                        p->css.top += top_move(ratio) / 2;
                        if (p->css.top < 0) {
                            p->css.top = 0;
                        }
                    }
                } else if ((pix_x > dw) || (pix_y > dh)) {
                    if (pix_x > pix_y) {
                        ratio = 1.0 - (float)(pix_x - dw) / ICON_UNIT_W;
                        p->css.width = __this->icon_pic_w * ratio;
                        p->css.height = __this->icon_pic_h * ratio;
                        p->css.top += top_move(ratio) / 2;
                        if (p->css.top + p->css.height > 10000) {
                            p->css.top = 10000 - p->css.height;
                        }
                    } else {
                        ratio = 1.0 - (float)(pix_y - dh) / ICON_UNIT_H;
                        p->css.width = __this->icon_pic_w * ratio;
                        p->css.height = __this->icon_pic_h * ratio;
                        p->css.left += left_move(ratio) / 2;
                        if (p->css.left + p->css.width > 10000) {
                            p->css.left = 10000 - p->css.width;
                        }
                    }
                } else {
                    ratio = 1.0;
                }
                /* printf("row=%d, col=%d, pix_x=%d, pix_y=%d, ratio=%f\n", row, col, pix_x, pix_y, ratio); */

                /* 配置缩放倍率 */
                if (ratio == 1.0) {
                    p->css.ratio.en = 0;
                    p->css.width = __this->icon_pic_w;
                    p->css.height = __this->icon_pic_h;
                } else if ((ratio < 0.25) || (ratio > 1.0)) {
                    p->css.invisible = true;
                } else {
                    p->css.invisible = false;
                    p->css.ratio.en = 1;
                    p->css.ratio.ratio_w = ratio;
                    p->css.ratio.ratio_h = ratio;
                }

                /* 计算下一个图标所在行、列 */
                if (is_odd_col) {
                    /* 奇数行，三个图标 */
                    if ((++row) >= (__this->line_icon - 1)) {
                        row = 0;
                        col ++;
                        is_odd_col = col % 2;
                    }
                } else {
                    /* 偶数行，四个图标 */
                    if ((++row) >= (__this->line_icon)) {
                        row = 0;
                        col ++;
                        is_odd_col = col % 2;
                    }
                }
            }
        }
    }

    /* if ((row != 0) || (is_odd_col && row != (__this->line_icon - 1)) || (!is_odd_col && row != __this->line_icon)) { */
    /* col += 1; */
    /* } */
    if (row != 0) {
        col += 1;	// 如果最后一行个数不为0，总行数需要加1
    }

    return col;
}


static void enter_app(u32 pic_id)
{
    // 这里通过传入的控件ID进入对应的APP
    printf(">>>>> touch pic id = 0x%x\n", pic_id);
}

static void menu_enter_app(int touch_x, int touch_y)
{
    struct element *p = NULL;
    struct rect rect = {0};

    struct element *elm = ui_core_get_element_by_id(WATERFALL_LAYOUT);

    if (elm) {
        list_for_each_child_element(p, elm) {
            ui_core_get_element_abs_rect(p, &rect);
            if ((touch_x >= rect.left) && (touch_x < (rect.left + rect.width)) && (touch_y >= rect.top) && (touch_y < (rect.top + rect.height))) {
                enter_app(p->id);
                return;
            }
        }
    }
}



void scroll_view_callback(ScrollView_t *view)
{
    /* printf("x=%d, y=%d\n", view->cur_x, view->cur_y); */
    /* 获取scrollview对象的当前位置 */
    int x = ui_scroll_view_get_cur_x(view);
    int y = ui_scroll_view_get_cur_y(view);

    /* 把图标移动到scrollview对象的位置 */
    waterfall_menu_icon_set_pos(x, y);

    /* 刷新页面 */
    ui_redraw(WATERFALL_LAYOUT);
}



static void waterfall_menu_init()
{
    /* 计算图标间隔(图标尺寸+间隔)，方便用于修改图标坐标 */
    __this->icon_w = ICON_UNIT_W * 10000 / DEMO_LAYOUT_W;
    __this->icon_h = ICON_UNIT_H * 10000 / DEMO_LAYOUT_H;

    __this->icon_pic_w = PIC_ICON_W * 10000 / DEMO_LAYOUT_W;
    __this->icon_pic_h = PIC_ICON_H * 10000 / DEMO_LAYOUT_H;

    /* 计算一行最多能容纳的图标数量，不做四舍五入 */
    __this->line_icon = DEMO_LAYOUT_W / ICON_UNIT_W;

    /* 初始化图标位置为(0, 0) */
    int col = waterfall_menu_icon_set_pos(0, 0);

    /* 创建scrollview对象 */
    int min_y = col * ICON_UNIT_H - DEMO_LAYOUT_H;
    __this->view = ui_scroll_view_init(0, -min_y, 0, 0);
    if (!__this->view) {
        return;
    }

    /* 绑定scrollview对象的回调，用于控制图标移动 */
    ui_scroll_view_set_callback(__this->view, scroll_view_callback);

    /* 设置scrollview的X、Y方向回弹距离 */
    ui_scroll_view_set_bounce(__this->view, PIC_ICON_W, 2 * PIC_ICON_H);

    /* scrollview对象初始坐标 */
    ui_scroll_view_move_to_pos(__this->view, 0, DEMO_LAYOUT_H / 2);

    /* 自动移动到0,0位置，实现动态进入效果。注意：自动移动时会受滑动区域约束 */
    ui_scroll_view_auto_move(__this->view, 0, 0, 10);
}

static void waterfall_menu_free()
{
    /* 释放scrollview对象 */
    ui_scroll_view_free(__this->view);
    __this->view = NULL;
}




static int waterfall_menu_layout_ontouch(void *ctr, struct element_touch_event *e)
{
    switch (e->event) {
    case ELM_EVENT_TOUCH_DOWN:	// 按下
        /* printf("ELM_EVENT_TOUCH_DOWN\n"); */
        __this->pos.x = e->pos.x;
        __this->pos.y = e->pos.y;
        __this->move_dir = TOUCH_MOVE_DIR_E;
        break;
    case ELM_EVENT_TOUCH_MOVE:
        /* printf("ELM_EVENT_TOUCH_MOVE\n"); */
        if (__this->move_dir == TOUCH_MOVE_DIR_E) {
            __this->move_dir = get_touch_move_dir_by_point(__this->pos.x, __this->pos.y, e->pos.x, e->pos.y);
        }
        ui_scroll_view_move_offset(__this->view, (e->pos.x - __this->pos.x), (e->pos.y - __this->pos.y));
        __this->pos.x = e->pos.x;
        __this->pos.y = e->pos.y;
        break;
    case ELM_EVENT_TOUCH_UP:
        /* printf("ELM_EVENT_TOUCH_UP\n"); */
        if (__this->move_dir == TOUCH_MOVE_DIR_E) {
            menu_enter_app(e->pos.x, e->pos.y);
        } else {
            ui_scroll_view_move_bounce(__this->view);
        }
        break;
    case ELM_EVENT_TOUCH_HOLD:
        break;
    case ELM_EVENT_TOUCH_ENERGY:
        /* printf("ELM_EVENT_TOUCH_ENERGY\n"); */
        int x_energy = e->pos.x >> 16;
        int y_energy = e->pos.y >> 16;
        /* printf("x_energy = %d, y_energy = %d\n", x_energy, y_energy); */
        ui_scroll_view_inertia_start(__this->view, x_energy, y_energy);
        break;
    case ELM_EVENT_TOUCH_R_MOVE:
        return true;
    default:
        break;
    }
    return true;
}


static int waterfall_menu_layout_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct layout *layout = (struct layout *)_ctrl;

    switch (event) {
    case ON_CHANGE_INIT:
        waterfall_menu_init();
        break;
    case ON_CHANGE_FIRST_SHOW:
        break;
    case ON_CHANGE_SHOW:
        break;
    case ON_CHANGE_RELEASE:
        waterfall_menu_free();
        break;
    }
    return false;
}
REGISTER_UI_EVENT_HANDLER(WATERFALL_LAYOUT)    // 有后台程序布局显示
.onchange = waterfall_menu_layout_onchange,
 .onkey = NULL,
  .ontouch = waterfall_menu_layout_ontouch,
};


#endif
#endif


