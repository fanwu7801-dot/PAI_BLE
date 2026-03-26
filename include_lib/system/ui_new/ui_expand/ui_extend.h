/* Copyright(C)
 * not free
 * All right reserved
 *
 * @file ui_extend.h
 * @brief UI扩展接口API头文件，重新封装部分UI动作接口，方便实现UI特效动画
 * @author zhuhaifang@zh-jieli.com
 * @version V100
 * @date 2022-12-15
 */

#ifndef __UI_EXTEND_H__
#define __UI_EXTEND_H__

#include "ui_expand/ui_typedef.h"




#define DEFAULT_HASH_VALUE	5381

#define STR_HASH_VALUE(str) ({ \
    unsigned int hash = DEFAULT_HASH_VALUE; \
    const char *s = str; \
    while (*s) { \
        hash = ((hash << 5) + hash) + (*s++); \
    } \
    hash; \
})

/* 示例用法：
   #define STR "hello"
   const unsigned int STR_HASH = STR_HASH_VALUE(STR);
*/
// 该宏函数接受一个参数：str 表示需要计算哈希值的字符串。该宏函数基于djb2哈希算法实现，对于任意长度的字符串，通过将每个字符转换为整数，并根据其顺序进行加权运算和哈希操作，最终得到一个哈希值。



/* ------------------------------------------------------------------------------------*/
/**
 * @brief 数值符号运算
 */
/* ------------------------------------------------------------------------------------*/
#ifndef abs
#define abs(x)		((x) > 0 ? (x) : (-(x)))		// 整形绝对值
#endif

#define inv(x)		((x) > 0 ? (-(x)) : (x))		// 取反向值
#define ass(a, x)	((a) > 0 ? abs(x) : inv(x))		// 取a同号
#define ope(x)		((x) > 0 ? 1 : (-1))			// 取x符号

#ifndef fabs
#define fabs(x)		(((x) < 0.0f) ? (-x) : (x))		// 浮点绝对值
#endif




/* ------------------------------------------------------------------------------------*/
/**
 * @brief 移动方向定义(仅用于开发动效时做标记使用，与tp驱动无关)
 */
/* ------------------------------------------------------------------------------------*/
typedef enum {
    TOUCH_MOVE_DIR_E,	// 未知方向
    TOUCH_MOVE_DIR_U,	// 向上移动
    TOUCH_MOVE_DIR_D,	// 向下移动
    TOUCH_MOVE_DIR_L,	// 向左移动
    TOUCH_MOVE_DIR_R,	// 向右移动
    TOUCH_MOVE_DIR_UD,// 上下移动
    TOUCH_MOVE_DIR_LR,// 左右移动
} TouchMoveDir;


/* ------------------------------------------------------------------------------------*/
/**
 * @brief get_two_point_dir 计算两点之间的方向
 *
 * @Params x0 起点X坐标
 * @Params y0 起点Y坐标
 * @Params x1 终点X坐标
 * @Params y1 终点Y坐标
 *
 * @return 滑动方向，参考 TouchMoveDir 定义
 */
/* ------------------------------------------------------------------------------------*/
TouchMoveDir get_two_point_dir(int x0, int y0, int x1, int y1);

// get_touch_move_dir_by_point 通过起点和终点，获取TP滑动方向
#define get_touch_move_dir_by_point(x0, y0, x1, y1) \
	get_two_point_dir(x0, y0, x1, y1)


/* ------------------------------------------------------------------------------------*/
/**
 * @brief get_two_point_distance 计算两点之间的直线距离
 *
 * @Params x0 起点X坐标
 * @Params y0 起点Y坐标
 * @Params x1 终点X坐标
 * @Params y1 终点Y坐标
 *
 * @return 两点之间的距离
 */
/* ------------------------------------------------------------------------------------*/
int get_two_point_distance(int x0, int y0, int x1, int y1);


/* ------------------------------------------------------------------------------------*/
/**
 * @brief get_two_point_radian 计算两点之间向量的弧度
 *
 * @Params x0 起点X坐标
 * @Params y0 起点Y坐标
 * @Params x1 终点X坐标
 * @Params y1 终点Y坐标
 *
 * @return 两点形成向量的弧度
 */
/* ------------------------------------------------------------------------------------*/
float get_two_point_radian(int x0, int y0, int x1, int y1);


/* ------------------------------------------------------------------------------------*/
/**
 * @brief get_two_point_angle 计算两点形成向量的角度
 *
 * @Params x0 起点X坐标
 * @Params y0 起点Y坐标
 * @Params x1 终点X坐标
 * @Params y1 终点Y坐标
 *
 * @return 两点之间向量的角度
 */
/* ------------------------------------------------------------------------------------*/
float get_two_point_angle(int x0, int y0, int x1, int y1);

// @brief get_touch_move_angle_by_point 获取两点之间连线与X轴的夹角(向量的角度)
#define get_touch_move_angle_by_point(x0, y0, x1, y1) \
	get_two_point_angle(x0, y0, x1, y1)



/* ------------------------------------------------------------------------------------*/
/**
 * @brief inertia_ease_function 常用的惯性能量值变化缓动曲线函数
 *
 * @Params energy 上一次的惯性能量值
 * @Params coef 摩擦系数(0-1，值越大，摩擦力越大，越快停止)
 * @Params rate 虚拟帧率(屏幕刷新帧率，越大需要刷新次数越多才能停止)
 *
 * @return 新的惯性能量值
 */
/* ------------------------------------------------------------------------------------*/
float inertia_ease_function(float energy, float coef, float rate);



/* ------------------------------------------------------------------------------------*/
/**
 * @brief ui_extend_random 生成[min, max]之间的随机数
 *
 * @Params min 最小值
 * @Params max 最大值
 *
 * @return 随机数
 */
/* ------------------------------------------------------------------------------------*/
int ui_extend_random(int min, int max);




#if 0
/* ------------------------------------------------------------------------------------*/
/**
 * @brief 控件缩放时对齐方式(仅用于开发动效时做标记使用，与UI框架核心无关)
 */
/* ------------------------------------------------------------------------------------*/
typedef enum {
    ELM_ZOOM_ALIGN_UN,	// 控件宽高不进行缩放
    ELM_ZOOM_ALIGN_C,	// 居中
    ELM_ZOOM_ALIGN_T,	// 上对齐
    ELM_ZOOM_ALIGN_D,	// 下对齐
    ELM_ZOOM_ALIGN_L,	// 左对齐
    ELM_ZOOM_ALIGN_R,	// 右对齐
    ELM_ZOOM_ALIGN_TL,	// 左上对齐
    ELM_ZOOM_ALIGN_TR,	// 右上对齐
    ELM_ZOOM_ALIGN_DL,	// 左下对齐
    ELM_ZOOM_ALIGN_DR,	// 右下对齐
} Element_Zoom_Mode;


/* ------------------------------------------------------------------------------------*/
/**
 * @brief 缩放坐标和倍率结构(用于动效开发时，指定位置和缩放倍率，方便同时移动和缩放)
 */
/* ------------------------------------------------------------------------------------*/
typedef struct {
    int x;	// 坐标
    int y;
    float ratio_w;	// 宽、高缩放倍率
    float ratio_h;
} Ratio_Point;



/* ------------------------------------------------------------------------------------*/
/**
 * @brief para_curve_function 抛物线方程函数(普通抛物线方程，参数根据抛物线规则确定)
 *
 * @param a 系数a，一般不为0，否则变成一次函数
 * @param b 系数b
 * @param c 系数c
 * @param x X参数
 *
 * @return Y坐标
 */
/* ------------------------------------------------------------------------------------*/
float para_curve_function(float a, float b, float c, float x);


/* ------------------------------------------------------------------------------------*/
/**
 * @brief florid_curve_function 繁花曲线，参考网址 https://ge.hxxy.edu.cn/info/1017/1748.htm
 * 注意：默认繁花曲线的圆心点为(0,0)，即返回的x,y坐标可以加上屏幕半径来偏移到屏幕中间
 *
 * @param R	外侧大圆的半径
 * @param r 内部笔尖位置形成的小圆半径
 * @param t 绘制过程中转动角度
 * @param x X坐标（输出）
 * @param y Y坐标（输出）
 */
/* ------------------------------------------------------------------------------------*/
void florid_curve_function(float R, float r, float t, float *x, float *y);


/* ------------------------------------------------------------------------------------*/
/**
 * @brief 半衰期点坐标结构定义(需要记录对应时间)
 */
/* ------------------------------------------------------------------------------------*/
struct half_life_pos {
    int time;	// 当前点的触发时间，可以使用jsffies记录
    int x;	// 当前点的X坐标
    int y;	// 当前点的Y坐标
};
/* ------------------------------------------------------------------------------------*/
/**
 * @brief half_life_function 基于半衰期原理预测惯性滑动距离
 *
 * @param pos TP坐标点列表
 * @param pos_count TP坐标点数量
 * @param half_life 半衰期，值越小，越看淡以前的速度
 * @param x 预测的目标点X坐标（输出）
 * @param y 预测的目标点Y坐标（输出）
 *
 */
/* ------------------------------------------------------------------------------------*/
void half_life_function(struct half_life_pos *pos, int pos_count, int half_life, int *x, int *y);


/* ------------------------------------------------------------------------------------*/
/**
 * @brief integral_function 通用定积分函数
 *
 * @param function 用于计算的f(x)函数
 * @param a 积分区间起始值
 * @param b 积分区间结束值
 * @param dx 横坐标的间隔数，越小计算结果越准确，但耗时越长
 *
 * @return 积分结果
 */
/* ------------------------------------------------------------------------------------*/
float integral_function(float (*function)(float x), float a, float b, float dx);



/* ------------------------------------------------------------------------------------*/
/**
 * @brief ui_random 随机整数生成函数，生成的随机数在[min, max)之间
 *
 * @param min 随机数最小值
 * @param max 随机数最大值
 *
 * @return 随机数
 */
/* ------------------------------------------------------------------------------------*/
int ui_random(int min, int max);



/* ------------------------------------------------------------------------------------*/
/**
 * @brief control_move 控件移动
 *
 * @param elm 控件元素指针
 * @param x 目标x坐标
 * @param y 目标y坐标
 * @param parent_w 父控件宽度
 * @param parent_h 父控件高度
 */
/* ------------------------------------------------------------------------------------*/
#define control_move(elm, x, y, parent_w, parent_h) \
{\
	elm->css.left = x * 10000 / parent_w; \
	elm->css.top  = y * 10000 / parent_h; \
}


/* ------------------------------------------------------------------------------------*/
/**
 * @brief control_zoom 控件(图片、背景图)缩放
 *
 * @param elm 控件元素指针
 * @param ratio_w 宽度缩放倍率
 * @param ratio_h 高度缩放倍率
 */
/* ------------------------------------------------------------------------------------*/
#define control_zoom(elm, ratio_w, ratio_h) \
{\
	elm->css.ratio.en = 1; \
	elm->css.ratio.ratio_w = ratio_w; \
	elm->css.ratio.ratio_h = ratio_h; \
}

/* ------------------------------------------------------------------------------------*/
/**
 * @brief ui_control_move UI控件移动，将UI控件移动到指定点坐标。注意：内部不检查控件是否
 * 存在，需外部确保控件及其父控件存在
 *
 * @param elm 控件元素句柄
 * @param target_x 目标X坐标
 * @param target_y 目标Y坐标
 */
/* ------------------------------------------------------------------------------------*/
void ui_control_move(struct element *elm, int target_x, int target_y);


/* ------------------------------------------------------------------------------------*/
/**
 * @brief ui_control_zoom UI控件缩放（注意，只能用于普通控件，不能用于容器控件）
 *
 * @param elm 控件句柄
 * @param src_rect 控件原来的rect(注意：是未缩放前的rect属性)
 * @param parent 父控件的rect
 * @param ratio_w 宽度缩放倍率
 * @param ratio_h 高度缩放倍率
 * @param mode 缩放对齐模式
 */
/* ------------------------------------------------------------------------------------*/
void ui_control_zoom(struct element *elm, struct rect src_rect, struct rect parent, float ratio_w, float ratio_h, Element_Zoom_Mode align);






/********************************* UI扩展函数相关 ******************************/
/* ------------------------------------------------------------------------------------*/
/**
 * @brief 自动移动参数
 */
/* ------------------------------------------------------------------------------------*/
typedef struct {
    u32 ctrl_id;		// 待移动的控件ID
    u16 cursor;			// 游标，内部计数用
    u16 count;			// 总移动点数
    u16 src_w;			// 控件原来的宽度
    u16 src_h;			// 控件原来的高度
    float move[2];		// 移动控制点
    float zoom[2];		// 缩放控制点
    Ratio_Point start;	// 起点
    Ratio_Point end;	// 终点
    Element_Zoom_Mode mode;	// 缩放对齐模式
} Control_Move_Arg;



/* ------------------------------------------------------------------------------------*/
/**
 * @brief ui_control_move_and_zoom UI控件同时移动和缩放
 *
 * @param elm 控件句柄
 * @param src_rect 控件原来的rect属性（缩放需要用到）
 * @param point 控件移动的目标点坐标和缩放比例
 * @param align 缩放模式
 */
/* ------------------------------------------------------------------------------------*/
void ui_control_move_and_zoom(struct element *elm, struct rect src_rect, Ratio_Point *point, Element_Zoom_Mode align);



/* ------------------------------------------------------------------------------------*/
/**
 * @brief ui_control_auto_move_and_zoom UI控件自动移动，每调用一次函数，控件移动一个位置
 *
 * @param arg 自动移动参数，参考“自动移动参数”结构体定义
 *
 * @return true 正常移动，false 移动异常或结束
 */
/* ------------------------------------------------------------------------------------*/
int ui_control_auto_move_and_zoom(Control_Move_Arg *arg);



/* ------------------------------------------------------------------------------------*/
/**
 * @brief ui_set_layout_index 设置布局内（图片、文本）索引
 *
 * @param elm 布局句柄
 * @param index 索引号
 */
/* ------------------------------------------------------------------------------------*/
void ui_set_layout_index(struct element *elm, u8 index);


/* ------------------------------------------------------------------------------------*/
/**
 * @brief rotate_pic_list 根据指定圆旋转图片列表（从angle开始旋转），模拟旋转显示数字
 *
 * 注意：如果要刷新，需要在父控件的ON_CHANGE_SHOW事件里面调用旋转，在redraw父控件时会
 * 跑ON_CHANGE_SHOW事件。如果在定时器里调，然后刷新父控件会导致AXI设备读内存异常死机
 *
 * @param list[] 图片控件的ID列表
 * @param count	图片控件数量
 * @param angle 旋转显示的起始角度
 * @param center_x 圆心X坐标
 * @param center_y 圆心Y坐标
 * @param radius 圆的半径
 */
/* ------------------------------------------------------------------------------------*/
void rotate_pic_list(u32 list[], u8 count, int angle, int center_x, int center_y, int radius);

#endif





#endif

