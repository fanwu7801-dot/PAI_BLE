/* Copyright(C)
 * not free
 * All right reserved
 *
 * @file ui_demo_app_dock.c
 * @brief
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

#define TCFG_DEMO_APP_DOCK_ENABLE_V2	0	// demo使能

#if TCFG_DEMO_APP_DOCK_ENABLE_V2
#if TCFG_UI_ENABLE && (!TCFG_LUA_ENABLE)

#include "math.h"
#include "ui_expand/ui_extend.h"
#include "ui_expand/ui_bezier.h"
#include "ui_expand/ui_rollerview.h"

#define STYLE_NAME  JL


struct app_dock_config {
    u32 index;	// 索引号(文字在文字列表，图片在图片列表中的索引)
    u32 window;	// APP功能页面ID
};
// 索引表配置
const static struct app_dock_config dock_symbol[] = {
    {0, PAGE_0},
    {1, PAGE_1},
    {2, PAGE_2},
    {3, PAGE_3},
    {4, PAGE_4},
    {5, PAGE_5},
    {6, PAGE_6},
    {7, PAGE_7},
    {8, PAGE_8},
    {9, PAGE_9},
};
#define APP_DOCK_SYMBOL_LEN		(sizeof(dock_symbol) / sizeof(dock_symbol[0]))


/* 通过文字/图片索引找到这个元素在映射表中的位置索引 */
static inline int find_symbol_index_by_index(int index)
{
    for (int i = 0; i < APP_DOCK_SYMBOL_LEN; i++) {
        if (dock_symbol[i].index == index) {
            return i;
        }
    }
    return -1;
}

/* 通过APP页面ID，找到这个元素在映射表中的位置索引 */
static inline int find_symbol_index_by_window(u32 window)
{
    for (int i = 0; i < APP_DOCK_SYMBOL_LEN; i++) {
        if (dock_symbol[i].window == window) {
            return i;
        }
    }
    return -1;
}



// UI ID配置
#define DOCK_V2_MAIN_LAYOUT		APP_DOCK_V2_LAYOUT
static u32 item_id_list[] = {
    APP_DOCK_V2_ITEM5,
    APP_DOCK_V2_ITEM4,
    APP_DOCK_V2_ITEM3,
    APP_DOCK_V2_ITEM2,
    APP_DOCK_V2_ITEM1,
    APP_DOCK_V2_ITEM0,
};
// UI设计上的项数量
#define DOCK_V2_ITEM_NUMBER		(sizeof(item_id_list) / sizeof(item_id_list[0]))

// 父布局宽、高
#define DOCK_V2_LAYOUT_W		240
#define DOCK_V2_LAYOUT_H		296

// 项的宽、高
#define DOCK_V2_ITEM_W			239
#define DOCK_V2_ITEM_H			120

// 每个项占的滚筒角度
#define DOCK_V2_ITEM_ANGLE      46	// 尽量偶数，防止误差

// 滚筒半径
#define DOCK_V2_ROLLER_RADIUS	150.0f

// 屏幕映射坐标的最大、小坐标
#define DOCK_V2_MIN_MAP			(-(DOCK_V2_LAYOUT_H - DOCK_V2_ITEM_H))
#define DOCK_V2_MAX_MAP			(DOCK_V2_LAYOUT_H - DOCK_V2_ITEM_H)

// 惯性的摩擦系数
#define DOCK_V2_COEF_FRI        0.5f    //coefficient of friction

// 缩放的最大、小倍率
#define DOCK_V2_MIN_RATIO       0.8f
#define DOCK_V2_MAX_RATIO       1.0f

// 倍率最大、小时对应的top坐标
#define DOCK_V2_MIN_RATIO_TOP   (DOCK_V2_MAX_MAP)
#define DOCK_V2_MAX_RATIO_TOP	(DOCK_V2_MAX_MAP - DOCK_V2_ITEM_H / 2)

// 缩放倍率的控制点
#define DOCK_V2_RATIO_CTRL      0.0,1.0,1.0,0.0


struct item_v2_info {
    u32 id;
    short x;
    short y;
    float angle;
};

struct app_dock_v2_private {
    int item_number;
    int dock_init;
    int move_dir;
    int y_energy;
    int timer_id;
    int app_index;		// 当前最顶上一项的app索引
    int app_number;		// app总数量
    int app_table[APP_DOCK_SYMBOL_LEN];   // 映射表
    struct item_v2_info item[DOCK_V2_ITEM_NUMBER];
    struct position pos;
    RollerView_t *view;
};
static struct app_dock_v2_private dock_v2_prv = {0};
#define __this	(&dock_v2_prv)



/* ============================ 工具函数 ============================ */

// 删除数组位置
static inline void array_del_position(int array[], int *size, int position)
{
    // 判断位置是否有效
    if (position >= 0 && position < *size) {
        // 将指定位置后的元素向前移动
        for (int i = position; i < *size - 1; i++) {
            array[i] = array[i + 1];
        }
        // 缩小数组大小，即丢弃最后一个元素
        *size--;
    }
}

// 删除数组元素
static inline void array_del_element(int array[], int *size, int element)
{
    int found = 0; // 标记是否找到要删除的元素

    // 查找要删除的元素在数组中的索引
    for (int i = 0; i < *size; i++) {
        if (array[i] == element) {
            found = 1;
            // 将后面的元素前移覆盖当前位置
            for (int j = i; j < *size - 1; j++) {
                array[j] = array[j + 1];
            }
            break;
        }
    }
    // 如果找到要删除的元素，则缩小数组大小
    if (found) {
        (*size)--;
    }
}

// 追加数组元素
static inline void array_add_element(int array[], int *size, int element)
{
    // 将元素追加到数组末尾
    array[*size] = element;
    // 增加数组大小
    (*size)++;
}





/* ------------------------------------------------------------------------------------*/
/**
 * @brief dock_add_by_index 通过文字(图片)索引添加APP到程序坞
 *
 * @Params index 文字(图片)的索引
 */
/* ------------------------------------------------------------------------------------*/
static void dock_add_by_index(int index)
{
    int symbol_index = find_symbol_index_by_index(index);
    array_del_element(__this->app_table, &__this->app_number, symbol_index);
    array_add_element(__this->app_table, &__this->app_number, symbol_index);
}


/* ------------------------------------------------------------------------------------*/
/**
 * @brief dock_add_by_window 通过页面ID添加APP到程序坞
 *
 * @Params window 页面ID
 */
/* ------------------------------------------------------------------------------------*/
static void dock_add_by_window(u32 window)
{
    int symbol_index = find_symbol_index_by_window(window);
    array_del_element(__this->app_table, &__this->app_number, symbol_index);
    array_add_element(__this->app_table, &__this->app_number, symbol_index);
}


/* ------------------------------------------------------------------------------------*/
/**
 * @brief dock_del_by_index 通过文字(图片)索引从程序坞删除APP
 *
 * @Params index 文字(图片)索引
 */
/* ------------------------------------------------------------------------------------*/
static void dock_del_by_index(int index)
{
    int symbol_index = find_symbol_index_by_index(index);
    array_del_element(__this->app_table, &__this->app_number, symbol_index);
}


/* ------------------------------------------------------------------------------------*/
/**
 * @brief dock_del_by_window 通过页面ID从程序坞删除APP
 *
 * @Params window 页面ID
 */
/* ------------------------------------------------------------------------------------*/
static void dock_del_by_window(u32 window)
{
    int symbol_index = find_symbol_index_by_window(window);
    array_del_element(__this->app_table, &__this->app_number, symbol_index);
}


/* ------------------------------------------------------------------------------------*/
/**
 * @brief dock_get_window_by_table_index 通过table指定位置的元素获取应用页面ID
 *
 * @Params index table表的元素索引
 *
 * @return 页面ID
 */
/* ------------------------------------------------------------------------------------*/
static u32 dock_get_window_by_table_index(int index)
{
    return dock_symbol[__this->app_table[index]].window;
}


/* ------------------------------------------------------------------------------------*/
/**
 * @brief dock_get_index_by_table_index 通过table指定位置的元素获取应用文字/图片索引
 *
 * @Params index table表的元素索引
 *
 * @return 文字/图片索引
 */
/* ------------------------------------------------------------------------------------*/
static u32 dock_get_index_by_table_index(int index)
{
    return dock_symbol[__this->app_table[index]].index;
}


/* ------------------------------------------------------------------------------------*/
/**
 * @brief dock_get_ratio_by_top 通过控件的top坐标，获取控件缩放倍率
 *
 * @Params top 控件的top坐标
 *
 * @return 控件的缩放倍率
 */
/* ------------------------------------------------------------------------------------*/
static float dock_get_ratio_by_top(int top)
{
    float t = (float)(top - DOCK_V2_MAX_RATIO_TOP) / (DOCK_V2_MIN_RATIO_TOP - DOCK_V2_MAX_RATIO_TOP);
    float ratio = ThirdOrderBezierEstimator(DOCK_V2_MIN_RATIO, DOCK_V2_MAX_RATIO, DOCK_V2_RATIO_CTRL, (1.0 - t));
    return ratio;
}


/* ============================ 动作函数 ============================ */

// 控件缩放大小
static void ui_element_zoom_scale(struct element *elm, float ratio)
{
    if (ratio == 1.0) {
        elm->css.ratio.en = 0;
    } else {
        elm->css.ratio.en = 1;
        elm->css.ratio.ratio_w = ratio;
        elm->css.ratio.ratio_h = ratio;
    }
}

// 项移动到指定位置
static void ui_element_move_to_pos(u32 elm_id, int x_pos, int y_pos, u8 pic_index)
{
    struct element *p;
    struct element *elm = ui_core_get_element_by_id(elm_id);
    if (!elm) {
        return;
    }

    elm->css.left = x_pos * 10000 / DOCK_V2_LAYOUT_W;
    elm->css.top = y_pos * 10000 / DOCK_V2_LAYOUT_H;

    // 控制图片缩放
    list_for_each_child_element(p, (struct element *)elm) {
        if (p && ui_id2type(p->id) == CTRL_TYPE_PIC) {
            float ratio = dock_get_ratio_by_top(y_pos);
            if (ratio > DOCK_V2_MAX_RATIO) {
                ratio = DOCK_V2_MAX_RATIO;
            } else if (ratio < DOCK_V2_MIN_RATIO) {
                ratio = DOCK_V2_MIN_RATIO;
            }
            ui_element_zoom_scale(p, ratio);	// 设置缩放倍率

            ui_pic_set_image_index((struct ui_pic *)p, pic_index % 6);	// 设置图片索引
        } else if (ui_id2type(p->id) == CTRL_TYPE_TEXT) {
            // 设置文本索引
            ui_text_set_index((struct ui_text *)p, pic_index);
        } else if (ui_id2type(p->id) == CTRL_TYPE_LAYOUT) {
            // 如果有布局，默认当它作为遮罩处理
            /* p->css.alpha = alpha; */
        }
    }
}

// 更新所有项位置
static void app_dock_item_elm_move(u8 redraw)
{
    for (int i = 0; i < __this->item_number; i++) {
        ui_element_move_to_pos(__this->item[i].id, __this->item[i].x, __this->item[i].y, __this->app_index + i);
        /* printf("item[%d] = 0x%x, x=%d, y=%d\n", i, __this->item[i].id, __this->item[i].x, __this->item[i].y); */
    }

    if (redraw) {
        ui_redraw(DOCK_V2_MAIN_LAYOUT);
    }
}

// Y方向移动
static void dock_v2_enter_dial();
static void dock_v2_enter_menu();
static void dock_v2_item_y_move(int yoffset)
{
    if (yoffset == 0) {
        return;
    }

    float cur_angle = ui_roller_view_get_cur_angle(__this->view);
    float min_angle = ui_roller_view_get_min_angle(__this->view);
    float max_angle = ui_roller_view_get_max_angle(__this->view);

    __this->move_dir = (yoffset > 0) ? TOUCH_MOVE_DIR_D : TOUCH_MOVE_DIR_U;
    if ((cur_angle <= min_angle) && (__this->move_dir == TOUCH_MOVE_DIR_U)) {
        // 划到最顶上，再网上划，进入菜单界面
        dock_v2_enter_menu();
    } else if ((cur_angle >= max_angle) && (__this->move_dir == TOUCH_MOVE_DIR_D)) {
        // 划到最底下，再往下划，进入表盘界面
        dock_v2_enter_dial();
    } else {
        ui_roller_view_move_arc_length(__this->view, yoffset);
    }
}

// Y方向对齐
static void dock_v2_item_y_align()
{
    int cur_angle = ui_roller_view_get_cur_angle(__this->view);
    int d_angle = cur_angle % (int)DOCK_V2_ITEM_ANGLE;
    int target_angle = cur_angle;

    if (d_angle == 0) {
        return;
    }

    if (abs(d_angle) > (DOCK_V2_ITEM_ANGLE / 2)) {
        if (d_angle > 0) {
            d_angle = DOCK_V2_ITEM_ANGLE - d_angle;
            target_angle = cur_angle + d_angle;
        } else {
            d_angle = DOCK_V2_ITEM_ANGLE + d_angle;
            target_angle = cur_angle - d_angle;
        }
    } else {
        target_angle = cur_angle - d_angle;
    }
    printf("cur angle=%d, d_angle=%d, target_angle=%d\n", cur_angle, d_angle, target_angle);
    ui_roller_view_move_to_angle(__this->view, target_angle);
}


// 滚筒模型回调
static void dock_v2_roller_callback(RollerView_t *view, float angle)
{
    int curr_angle = ui_roller_view_get_cur_angle(view);
    int half_angle = DOCK_V2_ITEM_ANGLE / 2;
    int max_index  = __this->app_number - DOCK_V2_ITEM_NUMBER;
    int start_angle = 0;
    /* printf("@@@@@ cur angle = %d\n", curr_angle); */

    if ((curr_angle == 90.0) && (__this->dock_init)) {
        float min_angle = -((__this->app_number - 4) * DOCK_V2_ITEM_ANGLE);
        ui_roller_view_set_limit_angle(__this->view, min_angle, 90.0);
    }

    if ((__this->y_energy != 0) && (view->energy == 0)) {
        /* 惯性结束、自动对齐 */
        __this->y_energy = 0;
        dock_v2_item_y_align();
        return;
    }

    /* 计算图标索引 */
    if (curr_angle < 0) {
        __this->app_index = abs(curr_angle - half_angle) / DOCK_V2_ITEM_ANGLE;
        if (__this->app_index > max_index) {
            __this->app_index = max_index;
        }
    } else {
        __this->app_index = 0;
    }
    /* printf("@@@ app_index = %d\n", __this->app_index); */

    /* 计算起始角度 */
    if (__this->app_index < max_index) {
        if (curr_angle < 0) {
            start_angle = curr_angle % (int)DOCK_V2_ITEM_ANGLE;
        } else {
            start_angle = curr_angle;
        }
        if (start_angle <= -half_angle) {
            start_angle += DOCK_V2_ITEM_ANGLE;
        }
    } else {
        start_angle = curr_angle + (max_index * DOCK_V2_ITEM_ANGLE);
    }

    /* 计算各项坐标 */
    for (int i = 0; i < __this->item_number; i++) {
        __this->item[i].angle = (i * DOCK_V2_ITEM_ANGLE) + start_angle;

        if (__this->item[i].angle < 0.0) {
            __this->item[i].y = ui_roller_view_get_pos_by_angle(view, 0.0);
        } else if (__this->item[i].angle > 180) {
            __this->item[i].y = ui_roller_view_get_pos_by_angle(view, 180.0);
        } else {
            __this->item[i].y = ui_roller_view_get_pos_by_angle(view, __this->item[i].angle);
        }
        /* printf("item[%d] angle=%f, y=%d, index=%d\n", i, __this->item[i].angle, __this->item[i].y, __this->app_index + i); */
    }

    app_dock_item_elm_move(true);
}


/* ============================ 响应函数 ============================ */

// 进入表盘界面
static void dock_v2_enter_dial()
{
    UI_HIDE_CURR_WINDOW();
    UI_SHOW_WINDOW(PAGE_0);
}

// 进入菜单界面
static void dock_v2_enter_menu()
{
    UI_HIDE_CURR_WINDOW();
    UI_SHOW_WINDOW(PAGE_7);
}

// 进入编辑界面
static void dock_v2_enter_edit()
{
    // TODO
}

// 进入APP界面
static void dock_v2_enter_app(int touch_x, int touch_y)
{
    int touch_index = -1;
    for (int i = 0; i < __this->item_number; i++) {
        /* printf("item[%d] y = %d, touch_y = %d, app_index = %d\n", i, __this->item[i].y, touch_y, __this->app_index); */
        if ((touch_y >= __this->item[i].y) && (touch_y < __this->item[i].y + DOCK_V2_ITEM_H)) {
            touch_index = __this->app_index + i;
            break;
        }
    }

    printf("@@@@@@ touch_index = %d\n", touch_index);
    if (touch_index != -1) {
        u32 window = dock_get_window_by_table_index(touch_index);
        // TODO
        /* UI_HIDE_CURR_WINDOW(); */
        /* UI_SHOW_WINDOW(window); */
    }
}

static int app_dock_v2_layout_ontouch(void *ctr, struct element_touch_event *e)
{
    switch (e->event) {
    case ELM_EVENT_TOUCH_DOWN:	// 按下
        __this->pos.x = e->pos.x;
        __this->pos.y = e->pos.y;
        __this->move_dir = TOUCH_MOVE_DIR_E;
        break;
    case ELM_EVENT_TOUCH_MOVE:
        if (__this->move_dir == TOUCH_MOVE_DIR_E) {
            __this->move_dir = get_touch_move_dir_by_point(__this->pos.x, __this->pos.y, e->pos.x, e->pos.y);
        }
        if ((__this->move_dir == TOUCH_MOVE_DIR_U) || (__this->move_dir == TOUCH_MOVE_DIR_D)) {
            dock_v2_item_y_move(e->pos.y - __this->pos.y);
        }
        __this->pos.x = e->pos.x;
        __this->pos.y = e->pos.y;
        break;
    case ELM_EVENT_TOUCH_UP:
        printf("ELM_EVENT_TOUCH_UP\n");
        if (__this->move_dir == TOUCH_MOVE_DIR_E) {
            dock_v2_enter_app(e->pos.x, e->pos.y);	// 点击抬起，进入APP
        } else if ((__this->move_dir == TOUCH_MOVE_DIR_U) || (__this->move_dir == TOUCH_MOVE_DIR_D)) {
            dock_v2_item_y_align();	// 滑动后抬起，自动对齐
        }
        break;
    case ELM_EVENT_TOUCH_HOLD:
        dock_v2_enter_edit();	// 长按时进入编辑界面
        break;
    case ELM_EVENT_TOUCH_ENERGY:
        printf("ELM_EVENT_TOUCH_ENERGY\n");
        if ((__this->move_dir == TOUCH_MOVE_DIR_U) || (__this->move_dir == TOUCH_MOVE_DIR_D)) {
            __this->y_energy = e->pos.y >> 16;
            ui_roller_view_inertia_start(__this->view, __this->y_energy);
        }
        break;
    case ELM_EVENT_TOUCH_R_MOVE:
        return true;
    default:
        break;
    }
    return true;
}


static void app_dock_v2_layout_init()
{
    __this->dock_init = false;
    __this->timer_id = 0;
    __this->item_number = sizeof(item_id_list) / sizeof(item_id_list[0]);

    __this->app_number = 20;

    for (int i = 0; i < DOCK_V2_ITEM_NUMBER; i++) {
        __this->item[i].id = item_id_list[i];
        __this->item[i].x  = (DOCK_V2_LAYOUT_W - DOCK_V2_ITEM_W) / 2;
    }

    __this->view = ui_roller_view_init(DOCK_V2_ROLLER_RADIUS, DOCK_V2_MIN_MAP, DOCK_V2_MAX_MAP);
    ASSERT(__this->view, "Error, ui_roller_view_init faild!\n");

    ui_roller_view_set_callback(__this->view, dock_v2_roller_callback);
    ui_roller_view_move_to_angle(__this->view, 180.0);

    ui_roller_view_auto_move_angle(__this->view, 90.0, 10);	// 页面进入时，自动滚动到90度位置

    __this->dock_init = true;
}


static void app_dock_v2_layout_free()
{
    if (__this->timer_id) {
        sys_timer_del(__this->timer_id);
        __this->timer_id = 0;
    }
    ui_roller_view_free(__this->view);
}


static int app_dock_v2_layout_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct layout *layout = (struct layout *)_ctrl;

    switch (event) {
    case ON_CHANGE_INIT:
        app_dock_v2_layout_init();
        break;
    case ON_CHANGE_FIRST_SHOW:
        break;
    case ON_CHANGE_SHOW:
        break;
    case ON_CHANGE_RELEASE:
        app_dock_v2_layout_free();
        break;
    }
    return false;
}
REGISTER_UI_EVENT_HANDLER(DOCK_V2_MAIN_LAYOUT)    // 有后台程序布局显示
.onchange = app_dock_v2_layout_onchange,
 .onkey = NULL,
  .ontouch = app_dock_v2_layout_ontouch,
};


#endif
#endif


