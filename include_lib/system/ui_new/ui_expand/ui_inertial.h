/* Copyright(C)
 * not free
 * All right reserved
 *
 * @file ui_inertial.h
 * @brief 惯性模拟模块，可用于模拟惯性滚动，编码器旋钮模拟TP滑动效果等场景
 *
 * 本模块包含两种惯性形式：
 *
 * 1. 速度型惯性，预设好系统摩擦力，给惯性系统一个初始速度，系统将在指定摩
 * 擦力下将速度衰减，直至速度为0时停止。这种模式更贴近自然惯性形式，但在UI
 * 设计交互中，可能需要考虑在惯性停止时，恰好UI控件移动到某个对齐的位置。
 * 基于位置对齐的需求，可将速度型惯性转为距离型惯性。
 *
 * 2. 距离型惯性，预设距离移动规律，给惯性系统指定移动距离，系统将在预设规
 * 律下移动指定距离，直至剩余距离为0时停止。这种模式通过贝塞尔曲线控制移动
 * 规律，可利用速度型惯性先预计惯性距离，然后结合实际计算出需要移动的距离。
 * 再通过距离型惯性实现对齐移动，当惯性停止时，UI控件恰好停在对齐位置。
 *
 * @author zhuhaifang@zh-jieli.com
 * @version V1.0.0
 * @date 2023-09-08
 */

#ifndef __PUBILC_UI_INERTIAL_H__
#define __PUBILC_UI_INERTIAL_H__


typedef enum {
    UI_INERTIAL_INIT,		// 初始化
    UI_INERTIAL_FORWARD,	// 正向滚动，velocity 为正
    UI_INERTIAL_BACKWARD,	// 反向滚动，velocity 为负
    UI_INERTIAL_SLOW,		// 缓慢阶段，速度即将减为0
    UI_INERTIAL_STOP,		// 停止(出现此事件，说明滚动将停止，可以在回调中调用对齐等函数动作)
    UI_INERTIAL_ERROR,		// 错误，未知状态
} UI_INERTIAL_STATUS;


typedef enum {
    UI_INERTIAL_VELOCITY = 0,	// 速度类型惯性
    UI_INERTIAL_DISTANCE = 1,	// 距离类型惯性
} UI_INERTIAL_TYPE;


typedef struct ui_inertial_module_t {
    s16 velocity;	// 当前惯性值，速度型时为当前速度，距离型时为剩余距离
    u16 timer_id;	// 定时器ID
    u16 interval;	// 定时器时间间隔
    u8 status;		// 当前状态，参考 UI_INERTIAL_STATUS
    u8 type;		// 惯性类型，参考 UI_INERTIAL_TYPE
    u8 fps;			// 屏幕刷新帧率，与惯性速度有关，调整会影响惯性停止时间，值越大越慢停止。默认20fps
    u8 malloc;		// buf申请标志，true init时申请了buf，false init时的hdl传入了buf
    float coef;     // 惯性摩擦系数，与惯性速度有关，调整会影响惯性停止时间，值越大越快停止。默认0.5f
    s16 distance;	// 距离模式下的移动距离
    u16 count;		// 距离模式下的运行次数
    u16 cnt;		// 距离模式下的当前次数
    s16 pos;		// 距离模式下的当前位置
    float cp[4];	// 距离惯性时的曲线控制点
    void *object;   // 自定义对象，使用指针形式，通过回调回传到逻辑层，模块内不使用

    /* ------------------------------------------------------------------------------------*/
    /**
     * @brief int 惯性回调函数的返回值，true 继续运行，false 惯性停止
     *
     * @Params hdl 惯性模块句柄
     * @Params object 惯性对象，初始化时传入的object参数
     * @Params offset 移动距离，惯性模块本身不管移动方向，由开发者在回调函数中将offset叠加到
     * 某个方向上实现指定方向的惯性动作
     */
    /* ------------------------------------------------------------------------------------*/
    int (*callback)(void *hdl, void *object, int offset);   // 回调函数
} UiInertial_t, *pUiInertial_t;


/* ------------------------------------------------------------------------------------*/
/**
 * @brief ui_inertial_init 初始化惯性模块句柄
 *
 * @Params hdl 惯性模块句柄buf指针，允许外部准备好buf转化为惯性模块指针，方便其他模块使用
 * 惯性模块时缓存统一管理，避免惯性模块单独申请buf造成内存碎片。
 * @Params object 惯性对象，惯性模块内部不处理本参数，回调时作为参数传入给回调函数
 * @Params callback 回调函数，惯性模块动作时触发本函数运行，由外部注册
 *
 * @return
 */
/* ------------------------------------------------------------------------------------*/
pUiInertial_t ui_inertial_init(void *hdl, void *object, int (*callback)(void *hdl, void *object, int offset));


/* ------------------------------------------------------------------------------------*/
/**
 * @brief ui_inertial_free 释放惯性对象，如果是模块申请的缓存buf，将会被一起释放
 *
 * @Params inertial 惯性模块句柄
 */
/* ------------------------------------------------------------------------------------*/
void ui_inertial_free(pUiInertial_t inertial);


/* ------------------------------------------------------------------------------------*/
/**
 * @brief ui_inertial_stop 惯性停止，可用于惯性刹车等动作，例如惯性滚动过程中有
 * 新的触摸交互，需要立即停下惯性，响应触摸信息时可调用
 *
 * @Params inertial 惯性模块句柄
 */
/* ------------------------------------------------------------------------------------*/
void ui_inertial_stop(pUiInertial_t inertial);


/* ------------------------------------------------------------------------------------*/
/**
 * @brief ui_inertial_move_velocity 惯性模块的速度衰减模式启动
 *
 * @Params inertial 惯性模块句柄
 * @Params velocity 初始速度
 */
/* ------------------------------------------------------------------------------------*/
void ui_inertial_move_velocity(pUiInertial_t inertial, int velocity);


/* ------------------------------------------------------------------------------------*/
/**
 * @brief ui_inertial_get_distance 获取惯性模块在指定初始速度减为0时移过的距离
 * 主要用于在距离模式下惯性需要移动的距离计算，通过计算在速度模式下的运行距离，在结合
 * UI对齐需求调整总距离，将总距离和合理的运行次数作为参数，启动距离模式惯性。
 *
 * @Params inertial 惯性模块句柄
 * @Params velocity 初始速度
 * @Params distance 移过的距离缓存(速度模式下，速度衰减为0时移过的总距离)
 *
 * @return 惯性运行次数，速度从初速度衰减到0时惯性运行的次数
 */
/* ------------------------------------------------------------------------------------*/
int ui_inertial_get_distance(pUiInertial_t inertial, int velocity, int *distance);


/* ------------------------------------------------------------------------------------*/
/**
 * @brief ui_inertial_move_distance 惯性模块距离模式惯性启动
 *
 * @Params inertial 惯性模块句柄
 * @Params distance 目标总距离
 * @Params count 运行次数
 */
/* ------------------------------------------------------------------------------------*/
void ui_inertial_move_distance(pUiInertial_t inertial, int distance, int count);


/* ------------------------------------------------------------------------------------*/
/**
 * @brief ui_inertial_get_status 获取惯性模块状态标志，值参考 UI_INERTIAL_STATUS
 *
 * @Params inertial 惯性模块句柄
 *
 * @return 惯性模块运行状态
 */
/* ------------------------------------------------------------------------------------*/
#define ui_inertial_get_status(inertial) \
    (inertial ? inertial->status : UI_INERTIAL_ERROR)



#endif // __PUBILC_UI_INERTIAL_H__


