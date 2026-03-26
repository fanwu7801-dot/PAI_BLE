/* Copyright(C)
 * not free
 * All right reserved
 *
 * @file ui_scrollview.h
 * @brief 模拟ScrollView效果模块，既平面移动效果
 * @author zhuhaifang@zh-jieli.com
 * @version V1.0.0
 * @date 2023-07-27
 */

#ifndef __UI_SCROLL_VIEW_H__
#define __UI_SCROLL_VIEW_H__



typedef struct ui_ScrollView_t {
    int cur_x;	// 当前的X、Y坐标
    int cur_y;

    int min_x;	// 最小的X、Y坐标
    int min_y;

    int max_x;	// 最大的X、Y坐标
    int max_y;

    int start_x;// 自动移动时起始坐标
    int start_y;

    int end_x;	// 自动移动时结束坐标
    int end_y;

    int bound_x;
    int bound_y;

    int move_x;
    int move_y;

    int current;
    int duration;

    int energy;
    float cos_val;
    float sin_val;

    unsigned short timer_id;	// 定时器ID
    unsigned short interval;	// 定时器间隔

    float coef;		// 惯性摩擦因素
    float cp[4];

    void (*callback)(struct ui_ScrollView_t *view);
} ScrollView_t;


// 获取当前X坐标
#define ui_scroll_view_get_cur_x(view)	(view->cur_x)

// 获取当前Y坐标
#define ui_scroll_view_get_cur_y(view)	(view->cur_y)


/* ------------------------------------------------------------------------------------*/
/**
 * @brief ui_scroll_view_init 创建ScrollView对象
 *
 * @Params min_x 划动时X的限制最小值
 * @Params min_y 划动时Y的限制最小值
 * @Params max_x 划动时X的限制最大值
 * @Params max_y 滑动时Y的限制最大值
 *
 * @return ScrollView对象句柄(NULL表示创建失败，内存申请失败)
 */
/* ------------------------------------------------------------------------------------*/
ScrollView_t *ui_scroll_view_init(int min_x, int min_y, int max_x, int max_y);


/* ------------------------------------------------------------------------------------*/
/**
 * @brief ui_scroll_view_free 释放ScrollView对象
 *
 * @Params view 对象句柄
 */
/* ------------------------------------------------------------------------------------*/
void ui_scroll_view_free(ScrollView_t *view);


/* ------------------------------------------------------------------------------------*/
/**
 * @brief ui_scroll_view_set_bounce 设置回弹距离
 *
 * @Params view 对象句柄
 * @Params bound_x X方向回弹距离(0则关闭回弹)
 * @Params bound_y Y方向回弹距离(0则关闭回弹)
 *
 * @return 参考ui_typedef.h中的UiExpandErrCode定义
 */
/* ------------------------------------------------------------------------------------*/
int ui_scroll_view_set_bounce(ScrollView_t *view, int bound_x, int bound_y);


/* ------------------------------------------------------------------------------------*/
/**
 * @brief ui_scroll_view_set_callback 设置ScrollView对象回调
 *
 * @Params view 对象句柄
 * @Params callback 回调函数
 *
 * @return 参考ui_typedef.h的UiExpandErrCode定义
 */
/* ------------------------------------------------------------------------------------*/
int ui_scroll_view_set_callback(ScrollView_t *view, void (*callback)(ScrollView_t *view));

/* ------------------------------------------------------------------------------------*/
/**
 * @brief ui_scroll_view_move_to_pos 将ScrollView移动到指定坐标(忽略限制区域和回弹)
 *
 * @Params view 对象句柄
 * @Params pos_x 目标X坐标
 * @Params pos_y 目标Y坐标
 *
 * @return 参考ui_typedef.h的UiExpandErrCode定义
 */
/* ------------------------------------------------------------------------------------*/
int ui_scroll_view_move_to_pos(ScrollView_t *view, int pos_x, int pos_y);

/* ------------------------------------------------------------------------------------*/
/**
 * @brief ui_scroll_view_move_offset 将ScrollView对象移动一个距离
 *
 * @Params view 对象句柄
 * @Params xoffset X方向移动距离
 * @Params yoffset Y方向移动距离
 *
 * @return 参考ui_typedef.h的UiExpandErrCode定义
 */
/* ------------------------------------------------------------------------------------*/
int ui_scroll_view_move_offset(ScrollView_t *view, int xoffset, int yoffset);

/* ------------------------------------------------------------------------------------*/
/**
 * @brief ui_scroll_view_auto_move ScrollView自动移动到目标坐标
 *
 * @Params view 对象句柄
 * @Params target_x 目标X坐标
 * @Params target_y 目标Y坐标
 * @Params duration 持续时间
 *
 * @return 参考ui_typedef.h的UiExpandErrCode定义
 */
/* ------------------------------------------------------------------------------------*/
int ui_scroll_view_auto_move(ScrollView_t *view, int target_x, int target_y, int duration);

/* ------------------------------------------------------------------------------------*/
/**
 * @brief ui_scroll_view_inertia_start ScrollView对象惯性移动启动
 *
 * @Params view 对象句柄
 * @Params x_energy X方向能量值(启动时的X移动距离)
 * @Params y_energy Y方向能量值(启动时的Y移动距离)
 *
 * @return 参考ui_typedef.h的UiExpandErrCode定义
 */
/* ------------------------------------------------------------------------------------*/
int ui_scroll_view_inertia_start(ScrollView_t *view, int x_energy, int y_energy);

/* ------------------------------------------------------------------------------------*/
/**
 * @brief ui_scroll_view_move_bounce 启动ScrollView对象的回弹功能
 *
 * @Params view 对象句柄
 *
 * @return 参考ui_typedef.h的UiExpandErrCode定义
 */
/* ------------------------------------------------------------------------------------*/
int ui_scroll_view_move_bounce(ScrollView_t *view);




#endif


