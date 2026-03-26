/* Copyright(C)
 * not free
 * All right reserved
 *
 * @file ui_bezier_anim_demo.c
 * @brief 贝塞尔动画demo
 * 演示使用ui_bezier_anim.h模块创建贝塞尔小动画，实现多个小动画统一管理示例
 * @author zhuhaifang@zh-jieli.com
 * @version V1.0.0
 * @date 2023-05-29
 */

#include "app_config.h"
#include "system/includes.h"
#include "ui/ui_sys_param.h"
#include "ui/ui_style.h"
#include "ui/ui.h"
#include "ui/ui_api.h"
#include "app_task.h"
#include "system/timer.h"
#include "device/device.h"
#include "key_event_deal.h"
#include "res/resfile.h"
#include "ui/res_config.h"
#include "music/music.h"


#include "ui_expand/ui_bezier.h"
#include "ui_expand/ui_bezier_anim.h"
#include "ui_expand/ui_jlui.h"

#define STYLE_NAME  JL

// DEMO使能开关
/* #define TCFG_BEZIER_ANIM_ENABLE */

#ifdef TCFG_BEZIER_ANIM_ENABLE


/* 贝塞尔动画DEMO的布局(容器) */
#define BEZIER_ANIM_DEMO_LAYOUT		BASEFORM_1250


/* 贝塞尔动画DEMO的对象(图标) */
#define BEZIER_ANIM_DEMO_OBJECT		TEST_ZOOM_PIC


#define LAYOUT_WH	454	// 布局宽高
#define OBJECT_WH	110	// 图标宽高


/* 贝塞尔动画全局私有变量定义 */
struct bezier_anim_demo_prv {
    BezierAnimObj_t *obj;
    unsigned int timer_id;

    /* 旋转角度，旋转只能在控件的ON_CHANGE_SHOW事件中配置，所以需要临时缓存 */
    int rotate_angle;
    int is_rotate_anim;
};
static struct bezier_anim_demo_prv demo_var = {0};
#define __this	(&demo_var)


/* ------------------------------------------------------------------------------------*/
/**
 * @brief bezier_anim_demo_cb 贝塞尔动画核心回调
 *
 * 每个动画动作都会触发该回调，因此要求这里尽可能快速执行完毕，否则影响整个动画性能。
 * 一般只在这里进行简单的属性记录或配置即可。切不可在这里刷屏！！！
 *
 * @Params obj 贝塞尔动画控制的对象(可以是ID、句柄等，创建动画时用于和动画绑定对象)
 * @Params type 动画类型
 * @Params arg 与动画类型匹配的计算结果(移动->坐标，缩放->倍率，旋转->角度，播放->索引)
 */
/* ------------------------------------------------------------------------------------*/
void bezier_anim_demo_cb(unsigned int obj, BezierAnimUnit_t *anim, AnimParam_t arg)
{
    struct element *elm = ui_core_get_element_by_id(obj);
    if (!elm) {
        return;
    }


    switch (anim->type) {
    case ANIM_TYPE_SLIDE:	// 变速动画
    /* break; */
    case ANIM_TYPE_ROUTE:	// 路径动画
        ui_element_move(elm, arg.move.x, arg.move.y, LAYOUT_WH, LAYOUT_WH);
        break;

    case ANIM_TYPE_SCALE:	// 缩放动画
        __this->rotate_angle = 0;
        ui_element_zoom(elm, arg.scale.ratio_w, arg.scale.ratio_h);
        break;

    case ANIM_TYPE_ROTATE:	// 旋转动画
        /* 旋转动作不能在回调里处理，需要在控件的ON_CHANGE_SHOW事件上回调 */
        __this->rotate_angle = arg.rotate.angle;
        break;

    case ANIM_TYPE_PLAY: 	// 播放动画
        int index = arg.play.index;
        /* printf(">>>>> play index: %d\n", play->index); */
        break;

    case ANIM_TYPE_DELAY:	// 空转的延迟动画，没有参数
        /* printf("ratio: %d, rotate: %d\n", elm->css.ratio.en, elm->css.rotate.en); */
        break;

    default:
        break;
    }
    /* printf("ratio: %d, rotate: %d\n", elm->css.ratio.en, elm->css.rotate.en); */
}


// 定时器回调
void bezier_anim_timer_cb(void *arg)
{
    /* 运行动画，有动画运行时返回1，全部动画运行完时返回0 */
    if (!bezier_anim_obj_run(__this->obj)) {

        /* 当全部动画运行完后，重置动画，使动画循环运行 */
        bezier_anim_obj_reset(__this->obj);
    }

    /* 刷新布局，让动画结果显示出来 */
    ui_redraw(BEZIER_ANIM_DEMO_LAYOUT);
}


/* 动画demo开始 */
void bezier_anim_demo_init()
{
    printf("==============>>> %s, %d\n", __FUNCTION__, __LINE__);
    /* 指定控制对象 */
    u32 object = BEZIER_ANIM_DEMO_OBJECT;

    /* 创建动画对象 */
    __this->obj = bezier_anim_obj_init(object, bezier_anim_demo_cb);
    __this->rotate_angle = 0;
    __this->is_rotate_anim = false;

    /* 动画参数 */
    AnimParam_t arg = {0};

    /* 声明动画单元指针，用于临时管理动画单元 */
    BezierAnimUnit_t *anim, *flat, *down;


#if 1
    /* 创建路径测试动画 */
    anim = bezier_anim_new_anim("route_test", ANIM_TYPE_ROUTE, 20);
    arg.move.x = 0;
    arg.move.y = 0;
    bezier_anim_set_start_value(anim, arg);	// 设置起始坐标
    arg.move.x = (LAYOUT_WH - OBJECT_WH) / 2;
    arg.move.y = (LAYOUT_WH - OBJECT_WH) / 2;
    bezier_anim_set_end_value(anim, arg);	// 设置结束坐标
    bezier_anim_set_ctrl_point(anim, BEZIER_CONTROL_TYPICAL);	// 设置控制点
    bezier_anim_obj_add_anim(__this->obj, anim, ANIM_MANAGE_FLAT);	// 将动画追加给动画对象
#endif

#if 1
    /* 创建缩放动画 */
    flat = bezier_anim_new_anim("scale_test", ANIM_TYPE_SCALE, 20);	// 由于是跟移动动画平行的，持续时间最好相同
    arg.scale.ratio_w = 0.2;
    arg.scale.ratio_h = 0.2;
    bezier_anim_set_start_value(flat, arg);	// 设置起始坐标
    arg.scale.ratio_w = 1.0;
    arg.scale.ratio_h = 1.0;
    bezier_anim_set_end_value(flat, arg);	// 设置结束坐标
    bezier_anim_set_ctrl_point(flat, BEZIER_CONTROL_TYPICAL);	// 设置控制点
    bezier_anim_add_anim(anim, flat, ANIM_MANAGE_FLAT);
#endif

#if 1
    /* 创建速度测试动画 */
    anim = bezier_anim_new_anim("slide_test", ANIM_TYPE_SLIDE, 20);
    arg.move.x = (LAYOUT_WH - OBJECT_WH) / 2;
    arg.move.y = (LAYOUT_WH - OBJECT_WH) / 2;
    bezier_anim_set_start_value(anim, arg);	// 设置起始坐标
    arg.move.x = LAYOUT_WH - OBJECT_WH;
    arg.move.y = (LAYOUT_WH - OBJECT_WH) / 2;
    bezier_anim_set_end_value(anim, arg);	// 设置结束坐标
    bezier_anim_set_ctrl_point(anim, BEZIER_CONTROL_EASE_OUT);	// 设置控制点
    bezier_anim_obj_add_anim(__this->obj, anim, ANIM_MANAGE_DOWN);	// 将动画追加给动画对象
#endif

#if 1
    /* 创建速度测试动画 */
    anim = bezier_anim_new_anim("slide_back", ANIM_TYPE_SLIDE, 30);
    arg.move.x = (LAYOUT_WH - OBJECT_WH);
    arg.move.y = (LAYOUT_WH - OBJECT_WH) / 2;
    bezier_anim_set_start_value(anim, arg);	// 设置起始坐标
    arg.move.x = 0;
    arg.move.y = 0;
    bezier_anim_set_end_value(anim, arg);	// 设置结束坐标
    bezier_anim_set_ctrl_point(anim, BEZIER_CONTROL_TYPICAL);	// 设置控制点
    bezier_anim_obj_add_anim(__this->obj, anim, ANIM_MANAGE_DOWN);	// 将动画追加给动画对象
#endif

#if 1
    /* 创建缩放动画 */
    flat = bezier_anim_new_anim("scale_back", ANIM_TYPE_SCALE, 30);	// 由于是跟移动动画平行的，持续时间最好相同
    arg.scale.ratio_w = 1.0;
    arg.scale.ratio_h = 1.0;
    bezier_anim_set_start_value(flat, arg);	// 设置起始坐标
    arg.scale.ratio_w = 0.2;
    arg.scale.ratio_h = 0.2;
    bezier_anim_set_end_value(flat, arg);	// 设置结束坐标
    bezier_anim_set_ctrl_point(flat, BEZIER_CONTROL_LINEAR);	// 设置控制点
    bezier_anim_add_anim(anim, flat, ANIM_MANAGE_FLAT);
#endif

#if 0
    /* 创建播放动画 */
    flat = bezier_anim_new_anim("play_test", ANIM_TYPE_PLAY, 30);	// 由于是跟移动动画平行的，持续时间最好相同
    arg.play.index = 0;
    bezier_anim_set_start_value(flat, arg);	// 设置起始坐标
    arg.play.index = 20;
    bezier_anim_set_end_value(flat, arg);	// 设置结束坐标
    bezier_anim_set_ctrl_point(flat, BEZIER_CONTROL_LINEAR);	// 设置控制点
    bezier_anim_add_anim(anim, flat, ANIM_MANAGE_FLAT);
#endif






#if 0
    /* 创建旋转动画 */
    flat = bezier_anim_new_anim("rotate_test", ANIM_TYPE_ROTATE, 20);
    arg.rotate.angle = 0;
    bezier_anim_set_start_value(flat, arg);	// 设置起始坐标
    arg.rotate.angle = 360 * 5;
    bezier_anim_set_end_value(flat, arg);	// 设置结束坐标
    bezier_anim_set_ctrl_point(flat, BEZIER_CONTROL_TYPICAL);	// 设置控制点
    /* bezier_anim_obj_add_anim(__this->obj, anim, ANIM_MANAGE_FLAT);	// 将动画追加给动画对象 */
    bezier_anim_add_anim(anim, flat, ANIM_MANAGE_FLAT);
#endif

#if 1
    /* 创建延迟测试动画 */
    anim = bezier_anim_new_anim("delay_step2", ANIM_TYPE_DELAY, 2);
    bezier_anim_obj_add_anim(__this->obj, anim, ANIM_MANAGE_DOWN);	// 将动画追加给动画对象
#endif




    /* 启动定时器 */
    if (!__this->timer_id) {
        __this->timer_id = sys_timer_add(NULL, bezier_anim_timer_cb, 50);
    }
}


/* 动画demo结束 */
void bezier_anim_demo_free()
{
    printf("==============>>> %s, %d\n", __FUNCTION__, __LINE__);
    /* 停止定时器 */
    if (__this->timer_id) {
        sys_timer_del(__this->timer_id);
        __this->timer_id = 0;
    }

    /* 释放贝塞尔动画对象 */
    bezier_anim_obj_free(__this->obj);
}


static int bezier_anim_object_onchange(void *ctr, enum element_change_event e, void *arg)
{
    struct element *elm = (struct element *)ctr;
    /* printf("%s, e = %d\n", __FUNCTION__, e); */

    switch (e) {
    case ON_CHANGE_INIT:
        break;
    case ON_CHANGE_SHOW:
        /* ui_element_own_rotate(elm, __this->rotate_angle); */
        break;
    case ON_CHANGE_RELEASE:
        break;
    case ON_CHANGE_HIDE:
        break;
    default:
        return false;
    }
    return false;
}
REGISTER_UI_EVENT_HANDLER(BEZIER_ANIM_DEMO_OBJECT)
.onchange = bezier_anim_object_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};



static int bezier_anim_demo_onchange(void *ctr, enum element_change_event e, void *arg)
{
    struct layout *layout = (struct layout *)ctr;

    switch (e) {
    case ON_CHANGE_INIT:
        bezier_anim_demo_init();
        break;
    case ON_CHANGE_RELEASE:
        bezier_anim_demo_free();
        break;
    default:
        return false;
    }
    return false;
}

REGISTER_UI_EVENT_HANDLER(BEZIER_ANIM_DEMO_LAYOUT)
.onchange = bezier_anim_demo_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

#endif


