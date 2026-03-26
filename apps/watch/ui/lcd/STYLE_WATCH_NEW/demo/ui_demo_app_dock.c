/* Copyright(C)
 * not free
 * All right reserved
 *
 * @file ui_demo_app_dock.c
 * @brief 程序坞DEMO
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

#define LOG_TAG_CONST       UI
#define LOG_TAG     		"[UI-ACTION]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"

#define TCFG_DEMO_APP_DOCK_ENABLE	0	// demo使能

#if TCFG_DEMO_APP_DOCK_ENABLE
#if TCFG_UI_ENABLE && (!TCFG_LUA_ENABLE)

#include "math.h"
#include "ui_expand/ui_bezier.h"
#include "ui_expand/ui_rollerview.h"


#define STYLE_NAME  JL


/* ------------------------------------------------------------------------------------*/
/**
 * @brief 映射表配置，在这里配置文字/图片索引和与之对应的目标APP页面ID
 * 这里应该列举所有的文字/图片索引与APP页面ID之间的对应关系
 */
/* ------------------------------------------------------------------------------------*/
struct app_dock_config {
    u32 index;	// 索引号(文字在文字列表，图片在图片列表中的索引)
    u32 window;	// APP功能页面ID
};

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
#define APP_DOCK_SYMBOL_LEN	(sizeof(dock_symbol) / sizeof(dock_symbol[0]))

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



/* ============================ 参数配置 ============================ */
#define APP_DOCK_MAIN_LAYOUT	BACKGROUND_APP_LAYOUT	// 主布局ID

#define APP_DOCK_ITEM_ID0		BACKGROUND_APP_ITEM0	// 四个项布局ID
#define APP_DOCK_ITEM_ID1		BACKGROUND_APP_ITEM1
#define APP_DOCK_ITEM_ID2		BACKGROUND_APP_ITEM2
#define APP_DOCK_ITEM_ID3		BACKGROUND_APP_ITEM3

#define APP_DOCK_DEL_LAYOUT		BACKGROUND_APP_DELETE	// “删除”按钮
#define APP_DOCK_ALL_LAYOUT		BACKGROUND_APP_ALL		// “所有APP”按钮

/* 父布局宽高 */
#define APP_DOCK_LAYOUT_W		454
#define APP_DOCK_LAYOUT_H		454

/* 各项的宽高 */
#define APP_DOCK_ITEM_W			240
#define APP_DOCK_ITEM_H			240


/* UI工具中设计的项数量 */
#define APP_DOCK_ITEM_NUMBER	4

/* 项移动时的最大、小top坐标 */
#define APP_DOCK_ITEM_MIN_TOP	27
#define APP_DOCK_ITEM_MAX_TOP	427


/* X方向移动设置 */
#define APP_DOCK_ITEM_X_MIN		7	// 项划到最左边时X坐标
#define APP_DOCK_ITEM_X_THR		57	// 小于阈值时自动到坐标，大于阈值时自动到右边
#define APP_DOCK_ITEM_X_MAX		107	// 默认位置(上下滑动时的位置)


/* 最多记录的程序个数 */
#define APP_DOCK_MAX_RECORD		10

/* 各项上下移动时的缩放配置 */
#define APP_DOCK_SCALE_RATIO_MAX	1.0		// 缩放最大倍率
#define APP_DOCK_SCALE_RATIO_MIN	0.8		// 缩放最小倍率
#define APP_DOCK_SCALE_CONTROL_P	BEZIER_CONTROL_EASE_OUT		// 缩放规则的控制点

/* 滚筒半径，半径越大，滚动相同距离转过的角度越小（感觉滚动阻力越大） */
#define APP_DOCK_ROLL_RADIUS	50.0f

/* 各项占的角度 */
#define APP_DOCK_ITEM_ANGLE		(180.0/(APP_DOCK_ITEM_NUMBER - 1))


/* 惯性摩擦系数，值越大，阻力越大，越容易停止 */
#define APP_DOCK_INERTIAL_FIR	0.5	// 摩擦系数，0-1之间，值越大，越快衰减到停止



/* ------------------------------------------------------------------------------------*/
/**
 * @brief 私有变量
 */
/* ------------------------------------------------------------------------------------*/
struct item_info {
    u32 id;
    int x;
    int y;
    float angle;
};

struct app_dock_private {
    int timer_id;	// 定时器ID
    int app_index;	// app索引
    int app_number;	// app数量
    int app_table[APP_DOCK_MAX_RECORD];	// app索引表
    int y_energy;	// 惯性时的Y轴能量值
    struct position pos;	// TP点坐标记录
    struct item_info item[4];	// 项信息
    float radian;	// 弧度记录
    int move_dir;	// 移动方向
    int is_delete;	// 删除模式
    int select_index;	// 左右滑动时被选中的项索引

    RollerView_t *view;	// 滚筒模型
    int roller_init;	// 滚筒是否已初始化
};
static struct app_dock_private app_dock_prv = {0};
#define __this (&app_dock_prv)


enum {
    MOVE_DIR_U,	// 上
    MOVE_DIR_D,	// 下
    MOVE_DIR_L,	// 左
    MOVE_DIR_R,	// 右
    MOVE_DIR_E, // 错误或未知
};

#define abs(x)  ((x)>0?(x):-(x))

// 获取滑动方向
static int get_move_dir(int x0, int y0, int x1, int y1)
{
    int dx = x1 - x0;
    int dy = y1 - y0;

    if (abs(dy) >= abs(dx)) {
        return (dy >= 0) ? MOVE_DIR_D : MOVE_DIR_U;
    } else {
        return (dx >= 0) ? MOVE_DIR_R : MOVE_DIR_L;
    }
}




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
    float t = (float)(top - APP_DOCK_ITEM_MIN_TOP) / (APP_DOCK_ITEM_MAX_TOP - APP_DOCK_ITEM_MIN_TOP);
    float ratio = ThirdOrderBezierEstimator(APP_DOCK_SCALE_RATIO_MIN, APP_DOCK_SCALE_RATIO_MAX, APP_DOCK_SCALE_CONTROL_P, t);
    return ratio;
}




/* ============================ UI 函数 ============================ */

// 控件缩放大小
static void ui_element_zoom_scale(struct element *elm, float ratio)
{
    elm->css.ratio.en = 1;
    elm->css.ratio.ratio_w = ratio;
    elm->css.ratio.ratio_h = ratio;
}

// 项移动到指定位置
static void ui_element_move_to_pos(u32 elm_id, int x_pos, int y_pos, u8 pic_index)
{
    struct element *p;
    struct element *elm = ui_core_get_element_by_id(elm_id);
    if (!elm) {
        return;
    }

    elm->css.left = x_pos * 10000 / APP_DOCK_LAYOUT_W;
    elm->css.top = y_pos * 10000 / APP_DOCK_LAYOUT_H;

    // 控制图片缩放
    list_for_each_child_element(p, (struct element *)elm) {
        if (p && ui_id2type(p->id) == CTRL_TYPE_PIC) {
            float ratio = dock_get_ratio_by_top(y_pos);
            ui_element_zoom_scale(p, ratio);
            ui_pic_set_image_index((struct ui_pic *)p, pic_index % 10);
        }
    }
}


// 更新所有项位置
static void app_dock_item_elm_move(u8 redraw)
{
    for (int i = 0; i < 4; i++) {
        ui_element_move_to_pos(__this->item[i].id, __this->item[i].x, __this->item[i].y, __this->app_index + i);
        /* printf("item[%d] = 0x%x, x=%d, y=%d\n", i, __this->item[i].id, __this->item[i].x, __this->item[i].y); */
    }

    if (redraw) {
        ui_redraw(APP_DOCK_MAIN_LAYOUT);
    }
}

// 单元控制(设置项单元透明度、缩放倍率、是否隐藏)
static void app_dock_item_elm_ctrl(u32 id, int alpha, float ratio, int hide)
{
    int elm_type = 0;
    struct element *p, *elm;
    elm = ui_core_get_element_by_id(id);
    if (elm) {
        if (hide) {
            elm->css.invisible = true;
            return;
        } else {
            elm->css.invisible = false;
        }

        list_for_each_child_element(p, elm) {
            elm_type = ui_id2type(p->id);
            if (elm_type == CTRL_TYPE_PIC) {
                p->css.ratio.en = true;
                p->css.ratio.ratio_w = ratio;
                p->css.ratio.ratio_h = ratio;
                /* p->css.alpha = alpha; */
            } else {
                p->css.alpha = alpha;
            }
        }
    }
}

// 移动“所有APP”按钮
static void app_dock_all_app_move(/* int x,  */int y, u8 redraw)
{
    struct element *elm = ui_core_get_element_by_id(APP_DOCK_ALL_LAYOUT);
    if (elm) {
        /* elm->css.left = x * 10000 / APP_DOCK_LAYOUT_W; */
        elm->css.top = y * 10000 / APP_DOCK_LAYOUT_H;
    }

    if (redraw) {
        ui_redraw(APP_DOCK_MAIN_LAYOUT);
    }
}



/* ============================ 动作函数 ============================ */

// 上下滑动
static void dock_item_y_move(int yoffset)
{
    int pos_y;
    int dont_move = 0;

    if (yoffset == 0) {
        return;    // 移动距离为0时，直接返回
    }
    __this->move_dir = (yoffset > 0) ? MOVE_DIR_D : MOVE_DIR_U;
    ui_roller_view_move_arc_length(__this->view, yoffset);
}

// 自动对齐
static void dock_item_auto_align()
{
    if ((__this->move_dir == MOVE_DIR_L) || (__this->move_dir == MOVE_DIR_R)) {
        /* 左右滑动时对齐 */
        int target_x = APP_DOCK_ITEM_X_MAX;
        if (__this->item[__this->select_index].x < APP_DOCK_ITEM_X_THR) {
            target_x = APP_DOCK_ITEM_X_MIN;
            __this->is_delete = true;
            app_dock_item_elm_ctrl(APP_DOCK_DEL_LAYOUT, 0, 1.0, false);
            /* 隐藏上方的项 */
            int i;
            for (i = 0; i < __this->select_index; i++) {
                app_dock_item_elm_ctrl(__this->item[i].id, 100, 1.0, false);
            }
            /* 隐藏下方的项 */
            for (i = (__this->select_index + 1); i < APP_DOCK_ITEM_NUMBER; i++) {
                __this->item[i].y = ui_roller_view_get_pos_by_angle(__this->view, 180.0);
            }
            /* 隐藏“所有APP”按钮 */
            if (__this->select_index == 3) {
                int all_top = __this->item[3].y + APP_DOCK_ITEM_H + 5 + (APP_DOCK_ITEM_X_MAX - APP_DOCK_ITEM_X_MIN);
                app_dock_all_app_move(all_top, false);
            }
        } else {
            target_x = APP_DOCK_ITEM_X_MAX;
            __this->is_delete = false;
            app_dock_item_elm_ctrl(APP_DOCK_DEL_LAYOUT, 100, 0.0, true);
            for (int i = 0; i < APP_DOCK_ITEM_NUMBER; i++) {
                if (__this->item[i].angle < -(APP_DOCK_ITEM_ANGLE / 2)) {
                    __this->item[i].y = ui_roller_view_get_pos_by_angle(__this->view, -(APP_DOCK_ITEM_ANGLE / 2));
                } else if (__this->item[i].angle > 180.0) {
                    __this->item[i].y = ui_roller_view_get_pos_by_angle(__this->view, 180.0);
                } else {
                    __this->item[i].y = ui_roller_view_get_pos_by_angle(__this->view, __this->item[i].angle);
                }

                if (i < __this->select_index) {
                    float ratio = dock_get_ratio_by_top(__this->item[i].y);
                    app_dock_item_elm_ctrl(__this->item[i].id, 0, ratio, false);
                }
            }
            app_dock_all_app_move(__this->item[3].y + APP_DOCK_ITEM_H + 5, false);
        }
        __this->item[__this->select_index].x = target_x;
        app_dock_item_elm_move(true);
        return;
    } else {
        /* 上下滑动时对齐 */
        int current_angle = ui_roller_view_get_cur_angle(__this->view);
        int d_angle = current_angle % (int)APP_DOCK_ITEM_ANGLE;

        /* printf("%s: cur_angle=%d, d_angle=%d\n", __FUNCTION__, current_angle, d_angle); */
        if (d_angle == 0) {
            return;
        }

        if (abs(d_angle) > (APP_DOCK_ITEM_ANGLE / 2)) {
            if (d_angle > 0) {
                d_angle = APP_DOCK_ITEM_ANGLE - d_angle;
                ui_roller_view_move_to_angle(__this->view, current_angle + d_angle);
            } else {
                d_angle = APP_DOCK_ITEM_ANGLE + d_angle;
                ui_roller_view_move_to_angle(__this->view, current_angle - d_angle);
            }
        } else {
            if (d_angle > 0) {
                ui_roller_view_move_to_angle(__this->view, current_angle - d_angle);
            } else {
                ui_roller_view_move_to_angle(__this->view, current_angle - d_angle);
            }
        }
    }
}

// 左右滑动
static void dock_item_x_move(int xoffset)
{
    /* printf("%s, @@@ xoffset = %d\n", __FUNCTION__, xoffset); */
    if (!xoffset) {
        return;
    }

    /* 找到中间的项索引 */
    int ref_top = ui_roller_view_get_pos_by_angle(__this->view, 60.0);
    for (int i = 0; i < APP_DOCK_ITEM_NUMBER; i++) {
        if ((__this->item[i].y > (ref_top - 5)) && (__this->item[i].y < (ref_top + 5))) {
            __this->select_index = i;
            break;
        }
    }

    if ((__this->select_index == -1) || \
        ((__this->item[__this->select_index].x == APP_DOCK_ITEM_X_MIN) && (xoffset < 0)) || \
        ((__this->item[__this->select_index].x == APP_DOCK_ITEM_X_MAX) && (xoffset > 0))) {
        return;
    }
    __this->item[__this->select_index].x += xoffset;

    if (__this->item[__this->select_index].x < APP_DOCK_ITEM_X_MIN) {
        __this->item[__this->select_index].x = APP_DOCK_ITEM_X_MIN;
    } else if (__this->item[__this->select_index].x > APP_DOCK_ITEM_X_MAX) {
        __this->item[__this->select_index].x = APP_DOCK_ITEM_X_MAX;
    }

    float percent = (float)(__this->item[__this->select_index].x - APP_DOCK_ITEM_X_MIN) / (APP_DOCK_ITEM_X_MAX - APP_DOCK_ITEM_X_MIN);

    /* 删除图标动作 */
    if (percent < 0.9) {
        app_dock_item_elm_ctrl(APP_DOCK_DEL_LAYOUT, percent * 100, 1 - percent, false);
    } else {
        app_dock_item_elm_ctrl(APP_DOCK_DEL_LAYOUT, 0, 0.0, true);
    }

    int i;
    for (i = 0; i < __this->select_index; i++) {
        app_dock_item_elm_ctrl(__this->item[i].id, 100 - (percent * 100), percent, false);
    }
    for (i = (__this->select_index + 1); i < APP_DOCK_ITEM_NUMBER; i++) {
        __this->item[i].y -= xoffset;
        if (xoffset > 0) {
            int top_limit = ui_roller_view_get_pos_by_angle(__this->view, __this->item[i].angle);
            if (__this->item[i].y < top_limit) {
                __this->item[i].y = top_limit;
            }
        }
    }

    if (__this->select_index == (APP_DOCK_ITEM_NUMBER - 1)) {
        struct element *all = ui_core_get_element_by_id(APP_DOCK_ALL_LAYOUT);
        if (all) {
            struct rect rect = {0};
            ui_core_get_element_abs_rect(all, &rect);
            int all_top = rect.top - xoffset;
            if (all_top < (__this->item[3].y + APP_DOCK_ITEM_H + 5)) {
                all_top = __this->item[3].y + APP_DOCK_ITEM_H + 5;
            }
            all->css.top = all_top * 10000 / APP_DOCK_LAYOUT_H;
        }
    }

    app_dock_item_elm_move(1);
}


/* ============================ 惯性函数 ============================ */

// 惯性缓动函数
static float ease_out(float v)
{
    float newValue = v * pow(1 - APP_DOCK_INERTIAL_FIR, 1.0 / 30);
    if (fabs(newValue) < 0.01) {
        newValue = 0.0;
    }
    return newValue;
}

static void dock_energy_timer_isr(void *p)
{
    __this->y_energy = ease_out(__this->y_energy);

    int cur_angle = ui_roller_view_get_cur_angle(__this->view);
    int min_angle = ui_roller_view_get_min_angle(__this->view);
    int max_angle = ui_roller_view_get_max_angle(__this->view);

    if ((__this->y_energy == 0.0) || (cur_angle <= min_angle) || (cur_angle >= max_angle)) {
        sys_timer_del(__this->timer_id);
        __this->timer_id = 0;
        dock_item_auto_align();	// 惯性结束后需要对齐
        return;
    }

    // 运行惯性
    dock_item_y_move(__this->y_energy);
}

/* ============================ 滚筒回调 ============================ */

void roller_view_callback(RollerView_t *view, float d_angle)
{
    printf("%s: view angle = %f, d_angle = %f\n", __FUNCTION__, view->angle, d_angle);
    int current_angle = ui_roller_view_get_cur_angle(view);
    int half_item_angle = APP_DOCK_ITEM_ANGLE / 2;
    int max_app_index = __this->app_number - APP_DOCK_ITEM_NUMBER;
    int start_angle = 0;

    if (current_angle == 0) {
        /* 设置限制角度，正常应该在init的时候设置，但初始化时需要
           有个进入动画，隐藏在这里等进入动画结束后再设置限制 */
        ui_roller_view_set_limit_angle(__this->view, -480.0, 60.0);
    }

    /* 计算图标索引 */
    if (current_angle < 0) {
        __this->app_index = abs(current_angle - half_item_angle) / APP_DOCK_ITEM_ANGLE;
        if (__this->app_index > max_app_index) {
            __this->app_index = max_app_index;
        }
    } else {
        __this->app_index = 0;
    }

    /* 计算起始角度 */
    if (__this->app_index < max_app_index) {
        if (current_angle < 0) {
            start_angle = current_angle % (int)APP_DOCK_ITEM_ANGLE;
        } else {
            start_angle = current_angle;
        }
        if (start_angle <= -half_item_angle) {
            start_angle += APP_DOCK_ITEM_ANGLE;
        }
    } else {
        start_angle = current_angle + (max_app_index * APP_DOCK_ITEM_ANGLE);
    }
    /* printf("@@@ start_angle = %d, app_index = %d\n", start_angle, __this->app_index); */

    /* 计算各项的y坐标 */
    for (int i = 0; i < APP_DOCK_ITEM_NUMBER; i ++) {
        __this->item[i].angle = (i * APP_DOCK_ITEM_ANGLE) + start_angle;
        if (__this->item[i].angle > 180.0) {
            __this->item[i].y = ui_roller_view_get_pos_by_angle(__this->view, 180.0);
        } else if (__this->item[i].angle < -half_item_angle) {
            __this->item[i].y = ui_roller_view_get_pos_by_angle(__this->view, -half_item_angle);
        } else {
            __this->item[i].y = ui_roller_view_get_pos_by_angle(__this->view, __this->item[i].angle);
        }
        printf("item[%d]: angle=%f, y = %d\n", i, __this->item[i].angle, __this->item[i].y);
    }

    /* 移动“所有APP”图标 */
    app_dock_all_app_move(__this->item[3].y + APP_DOCK_ITEM_H + 5, false);

    app_dock_item_elm_move(__this->roller_init);
}


// 进入APP或菜单
static void dock_enter_app_or_menu(int touch_x, int touch_y)
{
    printf("%s, x=%d, y=%d\n", __FUNCTION__, touch_x, touch_y);

    int touch_index = -1;
    if (touch_y > (__this->item[3].y + APP_DOCK_ITEM_H)) {
        // 进入菜单
        printf("@@@@@ enter menu...\n");
        UI_HIDE_CURR_WINDOW();
        UI_SHOW_WINDOW(PAGE_7);
    } else if ((touch_y > __this->item[3].y) && (touch_y < __this->item[3].y + APP_DOCK_ITEM_H)) {
        touch_index = __this->app_index + 3;
    } else if ((touch_y > __this->item[2].y) && (touch_y < __this->item[3].y)) {
        touch_index = __this->app_index + 2;
    } else if ((touch_y > __this->item[1].y) && (touch_y < __this->item[2].y)) {
        touch_index = __this->app_index + 1;
    } else if ((touch_y > __this->item[0].y) && (touch_y < __this->item[1].y)) {
        touch_index = __this->app_index;
    }

    if (touch_index != -1) {
        // 进入app
        u32 window = dock_get_window_by_table_index(touch_index);
        printf("@@@@@ touch_index=%d, window=0x%x\n", touch_index, window);
        UI_HIDE_CURR_WINDOW();
        UI_SHOW_WINDOW(window);
    }
}


static int app_dock_switch_layout_ontouch(void *ctr, struct element_touch_event *e)
{
    int x_offset, y_offset;
    if (!__this) {
        return false;
    }

    switch (e->event) {
    case ELM_EVENT_TOUCH_DOWN:	// 按下
        if (__this->timer_id) {
            sys_timer_del(__this->timer_id);
            __this->timer_id = 0;
        }

        __this->pos.x = e->pos.x;
        __this->pos.y = e->pos.y;
        __this->move_dir = MOVE_DIR_E;
        return true;
    case ELM_EVENT_TOUCH_MOVE:	// 滑动
        if (__this->move_dir == MOVE_DIR_E) {
            __this->move_dir = get_move_dir(__this->pos.x, __this->pos.y, e->pos.x, e->pos.y);
        }

        if ((__this->move_dir == MOVE_DIR_U) || (__this->move_dir == MOVE_DIR_D)) {
            if (!__this->is_delete) {
                dock_item_y_move(e->pos.y - __this->pos.y);
            }
        } else {
            dock_item_x_move(e->pos.x - __this->pos.x);
        }
        __this->pos.x = e->pos.x;
        __this->pos.y = e->pos.y;
        break;
    case ELM_EVENT_TOUCH_R_MOVE:	// 右滑(防止左右滑动时触发返回上一层页面)
        return true;
    case ELM_EVENT_TOUCH_UP:	// 抬起
        if (__this->move_dir == MOVE_DIR_E) {
            /* 按下后抬起 */
            dock_enter_app_or_menu(e->pos.x, e->pos.y);
        } else {
            /* 滑动后抬起 */
            dock_item_auto_align();
        }
        break;
    case ELM_EVENT_TOUCH_ENERGY:	// 惯性
        printf("ELM_EVENT_TOUCH_ENERGY\n");
        if ((__this->move_dir == MOVE_DIR_U) || (__this->move_dir == MOVE_DIR_D)) {
            __this->y_energy = e->pos.y >> 16;
            if (!__this->is_delete && !__this->timer_id && (abs(__this->y_energy) > 3)) {
                __this->timer_id = sys_timer_add(NULL, dock_energy_timer_isr, 50);
            }
        }
        break;
    }

    return false;
}

// 初始化函数
static void app_dock_layout_init()
{
    __this->app_number = 0;
    __this->is_delete = false;

    /* 测试时设置后台应用个数 */
    for (int i = 0; i < APP_DOCK_MAX_RECORD; i++) {
        __this->app_table[i] = find_symbol_index_by_index(i);
        __this->app_number += 1;
        printf("app_table[%d] = %d, number = %d\n", i, __this->app_table[i], __this->app_number);
    }

    __this->item[0].x = APP_DOCK_ITEM_X_MAX;
    __this->item[1].x = APP_DOCK_ITEM_X_MAX;
    __this->item[2].x = APP_DOCK_ITEM_X_MAX;
    __this->item[3].x = APP_DOCK_ITEM_X_MAX;

    __this->item[0].id = APP_DOCK_ITEM_ID0;
    __this->item[1].id = APP_DOCK_ITEM_ID1;
    __this->item[2].id = APP_DOCK_ITEM_ID2;
    __this->item[3].id = APP_DOCK_ITEM_ID3;

    for (int j = 0; j < APP_DOCK_ITEM_NUMBER; j++) {
        app_dock_item_elm_ctrl(__this->item[j].id, 0, 1.0, false);
    }

    struct element *elm = NULL;

    elm = ui_core_get_element_by_id(APP_DOCK_DEL_LAYOUT);
    elm->css.left = (APP_DOCK_ITEM_X_MIN + APP_DOCK_ITEM_W) * 10000 / APP_DOCK_LAYOUT_W;
    elm->css.invisible = true;

    __this->app_index = 0;
    __this->roller_init = false;

    __this->view = ui_roller_view_init(APP_DOCK_ROLL_RADIUS, APP_DOCK_ITEM_MIN_TOP, APP_DOCK_ITEM_MAX_TOP);
    ui_roller_view_set_callback(__this->view, roller_view_callback);
    ui_roller_view_move_to_angle(__this->view, 180.0);		// 先把滚筒设置到180度
    ui_roller_view_auto_move_angle(__this->view, 0.0, 10);	// 自动滚动到0度，形成进入动画 (10*50)ms
    /* ui_roller_view_set_limit_angle(__this->view, -480.0, 60.0); */

    __this->roller_init = true;
}

// 资源释放函数
static void app_dock_layout_free()
{
    if (__this->timer_id) {
        sys_timer_del(__this->timer_id);
        __this->timer_id = 0;
    }
    ui_roller_view_free(__this->view);
}


static int app_dock_switch_layout_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct layout *layout = (struct layout *)_ctrl;

    switch (event) {
    case ON_CHANGE_INIT:
        app_dock_layout_init();
        break;
    case ON_CHANGE_FIRST_SHOW:
        break;
    case ON_CHANGE_SHOW:
        break;
    case ON_CHANGE_RELEASE:
        app_dock_layout_free();
        break;
    }
    return false;
}
REGISTER_UI_EVENT_HANDLER(APP_DOCK_MAIN_LAYOUT)    // 有后台程序布局显示
.onchange = app_dock_switch_layout_onchange,
 .onkey = NULL,
  .ontouch = app_dock_switch_layout_ontouch,
};



#endif
#endif



