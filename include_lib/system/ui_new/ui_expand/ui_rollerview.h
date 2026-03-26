
#ifndef __UI_ROLLER_VIEW_H__
#define __UI_ROLLER_VIEW_H__


typedef struct ui_RollerView_t {
    unsigned int radius;	// 半径，不会为负数
    signed short map_min;	// 映射最小坐标
    signed short map_max;	// 映射最大坐标

    unsigned short timer_id;	// 定时器ID
    unsigned short interval;	// 定时器时间间隔
    unsigned short duration;	// 持续时间(duration * interval)ms
    unsigned short current;		// 当前时间

    float angle;	// 当前角度，可以为正、负
    float max_angle;// 转动的最大角度
    float min_angle;// 转动的最小角度

    float begin;	// 自动转动时的起始角度
    float target;	// 自动转动时的目标角度
    float cp[4];	// 贝塞尔控制点，控制自动转动

    float coef;	// 惯性摩擦系数
    int energy;	// 惯性能量值

    void (*callback)(struct ui_RollerView_t *view, float angle);
} RollerView_t;



#define ANGLE_TO_RADIAN(angle)	(angle * UI_PI / 180.0)		/* 角度转弧度 */
#define RADIAN_TO_ANGLE(radian)	(radian * 180.0 / UI_PI)	/* 弧度转角度 */


/* 获取滚筒的当前角度 */
#define ui_roller_view_get_cur_angle(view)	(view->angle)

/* 获取滚筒的最小角度 */
#define ui_roller_view_get_min_angle(view)	(view->min_angle)

/* 获取滚筒的最大角度 */
#define ui_roller_view_get_max_angle(view)	(view->max_angle)



/* ------------------------------------------------------------------------------------*/
/**
 * @brief ui_roller_view_init 创建滚筒视图对象
 *
 * @Params radius 滚筒半径(必须大于0)
 * @Params map_min 映射的最小值(滚筒角度等于 0 时的映射值)
 * @Params map_max 映射的最大值(滚筒角度等于180时的映射值)
 *
 * @return 对象句柄
 */
/* ------------------------------------------------------------------------------------*/
RollerView_t *ui_roller_view_init(unsigned int radius, int map_min, int map_max);


/* ------------------------------------------------------------------------------------*/
/**
 * @brief ui_roller_view_free 释放滚筒视图对象
 *
 * @Params view 对象句柄
 */
/* ------------------------------------------------------------------------------------*/
void ui_roller_view_free(RollerView_t *view);


/* ------------------------------------------------------------------------------------*/
/**
 * @brief ui_roller_view_move_to_angle 滚筒对象转动到指定角度
 *
 * 注意：如果已经设置了最大、小角度限制，这里滚动时会被限制。
 *
 * @Params view 对象句柄
 * @Params angle 目标角度
 *
 * @return 参照ui_typedef.h的UiExpandErrCode定义
 */
/* ------------------------------------------------------------------------------------*/
int ui_roller_view_move_to_angle(RollerView_t *view, float angle);


/* ------------------------------------------------------------------------------------*/
/**
 * @brief ui_roller_view_move_angle_offset 滚筒对象转动角度
 *
 * @Params view 对象句柄
 * @Params angle 角度差
 *
 * @return 参照ui_typedef.h的UiExpandErrCode定义
 */
/* ------------------------------------------------------------------------------------*/
int ui_roller_view_move_angle_offset(RollerView_t *view, float angle);


/* ------------------------------------------------------------------------------------*/
/**
 * @brief ui_roller_view_move_radian 滚筒对象转动弧度
 *
 * @Params view 对象句柄
 * @Params radian 弧度
 *
 * @return 参照ui_typedef.h的UiExpandErrCode定义
 */
/* ------------------------------------------------------------------------------------*/
int ui_roller_view_move_radian(RollerView_t *view, float radian);


/* ------------------------------------------------------------------------------------*/
/**
 * @brief ui_roller_view_move_arc_length 滚筒对象转动弧长
 *
 * @Params view 对象句柄
 * @Params len 弧长
 *
 * @return 参照ui_typedef.h的UiExpandErrCode定义
 */
/* ------------------------------------------------------------------------------------*/
int ui_roller_view_move_arc_length(RollerView_t *view, float len);


/* ------------------------------------------------------------------------------------*/
/**
 * @brief ui_roller_view_get_pos_by_angle 获取滚筒对象指定角度的映射值
 *
 * @Params view 对象句柄
 * @Params angle 角度
 *
 * @return 输入角度在map_min到map_max之间的映射值
 */
/* ------------------------------------------------------------------------------------*/
float ui_roller_view_get_pos_by_angle(RollerView_t *view, float angle);


/* ------------------------------------------------------------------------------------*/
/**
 * @brief ui_roller_view_auto_move_angle 滚筒对象自动滚动到指定角度
 *
 * @Params view 对象句柄
 * @Params target_angle 目标角度
 * @Params duration 持续时间(duration * 50)ms
 *
 * @return 参照ui_typedef.h的UiExpandErrCode定义
 */
/* ------------------------------------------------------------------------------------*/
int ui_roller_view_auto_move_angle(RollerView_t *view, float target_angle, unsigned short duration);


/* ------------------------------------------------------------------------------------*/
/**
 * @brief ui_roller_view_set_callback 设置滚筒对象的回调函数
 *
 * @Params view 对象句柄
 * @Params callback 回调函数指针
 *
 * @return 参照ui_typedef.h的UiExpandErrCode定义
 */
/* ------------------------------------------------------------------------------------*/
int ui_roller_view_set_callback(RollerView_t *view, void (*callback)(RollerView_t *view, float angle));


/* ------------------------------------------------------------------------------------*/
/**
 * @brief ui_roller_view_set_control_point 设置滚筒对象的贝塞尔控制点
 *
 * @Params view 对象句柄
 * @Params x0 控制点0 X坐标
 * @Params y0 控制点0 Y坐标
 * @Params x1 控制点1 X坐标
 * @Params y1 控制点1 Y坐标
 *
 * @return 参照ui_typedef.h的UiExpandErrCode定义
 */
/* ------------------------------------------------------------------------------------*/
int ui_roller_view_set_control_point(RollerView_t *view, float x0, float y0, float x1, float y1);


/* ------------------------------------------------------------------------------------*/
/**
 * @brief ui_roller_view_set_limit_angle 设置滚筒对象的滚动限制角度
 *
 * @Params view 对象句柄
 * @Params min_angle 最小角度
 * @Params max_angle 最大角度
 *
 * @return 参照ui_typedef.h的UiExpandErrCode定义
 */
/* ------------------------------------------------------------------------------------*/
int ui_roller_view_set_limit_angle(RollerView_t *view, float min_angle, float max_angle);


/* ------------------------------------------------------------------------------------*/
/**
 * @brief ui_roller_view_set_interval 设置滚筒对象自动滚动时的定时器时间间隔(默认50ms)
 *
 * @Params view 对象句柄
 * @Params interval 定时器时间
 *
 * @return 参照ui_typedef.h的UiExpandErrCode定义
 */
/* ------------------------------------------------------------------------------------*/
int ui_roller_view_set_interval(RollerView_t *view, unsigned short interval);


/* ------------------------------------------------------------------------------------*/
/**
 * @brief ui_roller_view_inertia_start 滚筒视图惯性启动（不够灵活，建议在应用层实现惯性）
 *
 * @Params view 对象句柄
 * @Params energy 初始能量值(上一次移动的距离)
 *
 * @return 参照ui_typedef.h的UiExpandErrCode定义
 */
/* ------------------------------------------------------------------------------------*/
int ui_roller_view_inertia_start(RollerView_t *view, int energy);



#endif

