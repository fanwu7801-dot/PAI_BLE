/* Copyright(C)
 * not free
 * All right reserved
 *
 * @file ui_bezier_anim.h
 *
 * @brief 贝塞尔微动画管理模块
 * 功能：为GUI对象创建简易的贝塞尔控制动画，实现复杂动画的统一管理
 *
 * 基本说明：
 * 1. 动画对象和具体动画是不同内容，可理解为：
 * 	动画对象指包含控制对象信息的动画序列头；动画内容指仅包含当前动画信息的节点
 *
 * 2. 动画类型与起始、结束值，回调函数的result参数绑定，用户需自己根据动画类型
 * 	强转
 *
 * 3. 每个动画节点可以向平行、垂直两个方向扩展，即在追加动画时使用
 * ANIM_MANAGE_FLAT 或 ANIM_MANAGE_DOWN 作为 dir 参数控制追加方向
 *
 * 4. 为方便管理，建议将动画序列设计成如下形式：
 * object_handler
 * 	|-- object
 * anim --> flat --> flat ...
 *	|
 * down --> flat --> flat ...
 *	|
 * down ...
 * 	|
 * ...
 *
 * 5. API内容：
 *	a. 对象API：创建对象、释放对象、追加动画、运行动画、查找动画、重置对象
 * 	b. 动画API：创建动画、设置动画、追加动画、删除动画、查找动画、重置动画
 *
 * 风险提示：创建动画时禁止出现循环嵌套，如A动画的子动画包含B动画，B动画的子动画禁止再包含A
 *
 * @author zhuhaifang@zh-jieli.com
 * @version V1.0.0
 * @date 2023-05-25
 */


#ifndef __UI_BEZIER_ANIM_H__
#define __UI_BEZIER_ANIM_H__



// 动画类型定义, bezier animation type
typedef enum {
    ANIM_TYPE_SLIDE,	// 移动：控件从A点直线移动到B点，速度变化（参数为move）
    ANIM_TYPE_ROUTE,	// 路径：控件从A点沿着指定路径到B点（参数为move）
    ANIM_TYPE_SCALE,	// 缩放：控件从A倍率缩放到B倍率（参数为scale）
    ANIM_TYPE_ROTATE,	// 旋转：控件从A角度旋转到B角度（参数为rotate）
    ANIM_TYPE_PLAY,		// 播放：控件图片从A索引播放到B索引（参数为play）
    ANIM_TYPE_DELAY,	// 延迟：动画空转，一般用于动画之间设置间隔（会进入回调，参数为NULL）

    ANIM_TYPE_UNKNOWN	// 未知：动画类型错误
} BezierAnimType;



/* ------------------------------------------------------------------------------------*/
/**
 * @brief 动画追加方向定义
 * 平行动画：当前动画执行时会一起执行；
 * 垂直动画：当前动画执行完才开始执行。
 */
/* ------------------------------------------------------------------------------------*/
#define ANIM_MANAGE_FLAT    0   // 管理水平的动画
#define ANIM_MANAGE_DOWN    1   // 管理垂直的动画


// 坐标点定义，8byte
typedef iPoint_t AnimMove_t;


// 缩放比例定义，8byte
typedef struct anim_scale {
    float ratio_w;
    float ratio_h;
} AnimScale_t;


// 旋转角度定义，4byte
typedef struct anim_rotate {
    int angle;
} AnimRotate_t;


// 图标索引定义，4byte
typedef struct anim_play {
    unsigned int index;
} AnimPlay_t;


// 动画参数联合体
typedef union anim_param {
    AnimMove_t move;
    AnimScale_t scale;
    AnimRotate_t rotate;
    AnimPlay_t play;
} AnimParam_t;


// 贝塞尔动画单元，48 byte
typedef struct bezier_anim_unit {
    unsigned short name;		// 当前动画名称
    unsigned short type;        // 当前的动画类型，决定处理方法和调用回调
    unsigned short current;     // 当前时间，全部从0开始，当前时间等于持续时间时，开始子动画
    unsigned short duration;	// 持续时间，通过持续时间和当前时间，计算出t
    AnimParam_t s_val, e_val;	// start value, end value。起始值和结束值

    float cp[4];	// 贝塞尔控制点
    struct bezier_anim_unit *flat;     // 平行动画，当前动画执行时也同时执行的动画
    struct bezier_anim_unit *down;     // 垂直动画，当前动画执行完才需要执行的动画
} BezierAnimUnit_t;


// 贝塞尔动画对象，12 byte
typedef struct bezier_anim_obj {
    unsigned int object;	// 动画对象
    BezierAnimUnit_t *anim;	// 起始动画

    /* ------------------------------------------------------------------------------------*/
    /**
     * brief 动画对象回调
     *
     * Params object 操作对象(注意：这里不是动画对象句柄，是句柄内的object成员)
     * Params anim 动画单元，一般情况可以通过获取动画单元的type值来判断动画类型
     * Params result 计算结果
     */
    /* ------------------------------------------------------------------------------------*/
    void (*objCallback)(unsigned int object, BezierAnimUnit_t *anim, AnimParam_t result);
} BezierAnimObj_t;




/************************************动画对象操作***************************************/

/* ------------------------------------------------------------------------------------*/
/**
 * @brief bezier_anim_obj_init 创建贝塞尔动画对象句柄
 *
 * @Params object 控制对象(仅用在回调时传参，用户可根据情况传ID或强转指针等)
 * @Params objCallback 动作回调
 *
 * @return 动画对象句柄
 */
/* ------------------------------------------------------------------------------------*/
BezierAnimObj_t *bezier_anim_obj_init(unsigned int object, void (*objCallback)(unsigned int, BezierAnimUnit_t *, AnimParam_t));


/* ------------------------------------------------------------------------------------*/
/**
 * @brief bezier_anim_obj_free 释放贝塞尔动画对象句柄
 *
 * @Params obj 动画对象句柄
 */
/* ------------------------------------------------------------------------------------*/
void bezier_anim_obj_free(BezierAnimObj_t *obj);


/* ------------------------------------------------------------------------------------*/
/**
 * @brief bezier_anim_obj_add_anim 贝塞尔动画对象添加动画
 *
 * 注意：这里是将依据创建好的动画添加到对象句柄中，并不主动创建动画。本接口仅对动画对象的
 * 根动画进行操作
 *
 * @Params obj 动画对象句柄
 * @Params anim 动画内容句柄
 * @Params dir 添加方向(ANIM_MANAGE_FLAT 平行动画，ANIM_MANAGE_DOWN 垂直动画)
 */
/* ------------------------------------------------------------------------------------*/
void bezier_anim_obj_add_anim(BezierAnimObj_t *obj, BezierAnimUnit_t *anim, int dir);


/* ------------------------------------------------------------------------------------*/
/**
 * @brief bezier_anim_obj_get_anim 从动画对象中查找动画
 *
 * @Params obj 动画对象句柄
 * @Params name 动画名称
 *
 * @return 动画句柄(若名称传入NULL则返回根节点，否则返回查找结果)
 */
/* ------------------------------------------------------------------------------------*/
BezierAnimUnit_t *bezier_anim_obj_get_anim(BezierAnimObj_t *obj, const char *name);


/* ------------------------------------------------------------------------------------*/
/**
 * @brief bezier_anim_obj_run 运行动画对象的动画内容(每次调用触发以西动画运行)
 *
 * @Params obj 动画对象句柄
 *
 * @return (1 有动画在运行，0 无动画在运行)
 */
/* ------------------------------------------------------------------------------------*/
int bezier_anim_obj_run(BezierAnimObj_t *obj);


/* ------------------------------------------------------------------------------------*/
/**
 * @brief bezier_anim_obj_reset 重置动画对象
 *
 * @Params obj 动画对象句柄
 */
/* ------------------------------------------------------------------------------------*/
void bezier_anim_obj_reset(BezierAnimObj_t *obj);





/************************************具体动画操作***************************************/

/* ------------------------------------------------------------------------------------*/
/**
 * @brief bezier_anim_new_anim 创建新的动画句柄
 *
 * @Params name 动画名称
 * @Params type 动画类型
 * @Params duration 持续时间
 *
 * @return 动画句柄
 */
/* ------------------------------------------------------------------------------------*/
BezierAnimUnit_t *bezier_anim_new_anim(const char *name, BezierAnimType type, unsigned short duration);


/* ------------------------------------------------------------------------------------*/
/**
 * @brief bezier_anim_set_duration 设置动画持续时间
 *
 * @Params anim 动画句柄
 * @Params duration 持续时间
 */
/* ------------------------------------------------------------------------------------*/
void bezier_anim_set_duration(BezierAnimUnit_t *anim, unsigned short duration);


/* ------------------------------------------------------------------------------------*/
/**
 * @brief bezier_anim_set_current 设置动画当前时间
 *
 * @Params anim 动画句柄
 * @Params current 当前时间
 */
/* ------------------------------------------------------------------------------------*/
void bezier_anim_set_current(BezierAnimUnit_t *anim, unsigned short current);


/* ------------------------------------------------------------------------------------*/
/**
 * @brief bezier_anim_set_start_value 设置动画的起始值
 *
 * @Params anim 动画句柄
 * @Params arg 起始值参数(根据动画类型设置)
 */
/* ------------------------------------------------------------------------------------*/
void bezier_anim_set_start_value(BezierAnimUnit_t *anim, AnimParam_t arg);


/* ------------------------------------------------------------------------------------*/
/**
 * @brief bezier_anim_set_end_value 设置动画的结束值
 *
 * @Params anim 动画句柄
 * @Params arg 结束值参数(根据动画类型设置)
 */
/* ------------------------------------------------------------------------------------*/
void bezier_anim_set_end_value(BezierAnimUnit_t *anim, AnimParam_t arg);


/* ------------------------------------------------------------------------------------*/
/**
 * @brief bezier_anim_set_ctrl_point 设置动画的控制点
 *
 * @Params anim 动画句柄
 * @Params x1 控制点1的X坐标
 * @Params y1 控制点1的Y坐标
 * @Params x2 控制点2的X坐标
 * @Params y2 控制点2的Y坐标
 */
/* ------------------------------------------------------------------------------------*/
void bezier_anim_set_ctrl_point(BezierAnimUnit_t *anim, float x1, float y1, float x2, float y2);


/* ------------------------------------------------------------------------------------*/
/**
 * @brief bezier_anim_add_anim 追加子动画到动画句柄
 *
 * @Params parent 父动画句柄
 * @Params anim 子动画句柄
 * @Params dir 追加方向(ANIM_MANAGE_FLAT 平行方向，ANIM_MANAGE_DOWN 垂直方向)
 */
/* ------------------------------------------------------------------------------------*/
void bezier_anim_add_anim(BezierAnimUnit_t *parent, BezierAnimUnit_t *anim, int dir);


/* ------------------------------------------------------------------------------------*/
/**
 * @brief bezier_anim_del_anim 删除指定动画(会自动将该动画的子动画一并删除)
 *
 * @Params anim 动画句柄
 */
/* ------------------------------------------------------------------------------------*/
void bezier_anim_del_anim(BezierAnimUnit_t *anim);


/* ------------------------------------------------------------------------------------*/
/**
 * @brief bezier_anim_get_anim 查找指定动画(从子动画中查找)
 *
 * @Params anim 动画句柄
 * @Params name 动画名称
 *
 * @return 查找结果
 */
/* ------------------------------------------------------------------------------------*/
BezierAnimUnit_t *bezier_anim_get_anim(BezierAnimUnit_t *anim, unsigned short name);


/* ------------------------------------------------------------------------------------*/
/**
 * @brief bezier_anim_reset_anim 重置动画(自动重置子动画)
 *
 * @Params anim 动画句柄
 */
/* ------------------------------------------------------------------------------------*/
void bezier_anim_reset_anim(BezierAnimUnit_t *anim);



#endif // __UI_BEZIER_ANIM_H__


