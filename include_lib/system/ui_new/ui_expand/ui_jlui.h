/* Copyright(C)
 * not free
 * All right reserved
 *
 * @file ui_jlui.h
 * @brief 杰理UI框架微动画操作函数，仅适用于杰理UI框架
 * @author zhuhaifang@zh-jieli.com
 * @version V1.0.0
 * @date 2023-06-06
 */


#ifndef __UI_JLUI_H__
#define __UI_JLUI_H__


#include "ui/ui_core.h"



/* ------------------------------------------------------------------------------------*/
/**
 * @brief 杰理UI框架绝对值和相对值转换
 */
/* ------------------------------------------------------------------------------------*/
#define RECT_TO_CSS(x, X)	((x) * 10000 / (X))		// 绝对坐标转相对坐标
#define CSS_TO_RECT(c, C)	((c) * (C) / 10000)		// 相对坐标转绝对坐标




/* ------------------------------------------------------------------------------------*/
/**
 * @brief ui_element_move_to UI元素移动到指定坐标
 *
 * @Params elm UI元素句柄
 * @Params x 目标X
 * @Params y 目标Y
 * @Params parent_w 父控件宽度
 * @Params parent_h 父控件高度
 */
/* ------------------------------------------------------------------------------------*/
static inline void ui_element_move(struct element *elm, int x, int y, int parent_w, int parent_h)
{
    elm->css.left = x * 10000 / parent_w;
    elm->css.top  = y * 10000 / parent_h;
}


/* ------------------------------------------------------------------------------------*/
/**
 * @brief ui_element_move 移动UI元素到指定坐标
 *
 * @Params elm UI元素
 * @Params x 目标X
 * @Params y 目标Y
 */
/* ------------------------------------------------------------------------------------*/
static inline void ui_element_move_pos(struct element *elm, int x, int y)
{
    struct rect rect = {0};

    ui_core_get_element_abs_rect(elm->parent, &rect);

    if (rect.width && rect.height) {
        ui_element_move(elm, x, y, rect.width, rect.height);
    }
}


/* ------------------------------------------------------------------------------------*/
/**
 * @brief ui_element_move_offset UI元素移动指定距离
 *
 * @Params elm UI元素句柄
 * @Params xoffset X方向移动距离
 * @Params yoffset Y方向移动距离
 */
/* ------------------------------------------------------------------------------------*/
static inline void ui_element_move_offset(struct element *elm, int xoffset, int yoffset)
{
    struct rect rect = {0};
    struct rect pare = {0};

    ui_core_get_element_abs_rect(elm, &rect);
    ui_core_get_element_abs_rect(elm->parent, &pare);

    ui_element_move(elm, rect.left + xoffset, rect.top + yoffset, pare.width, pare.height);
}



/* ------------------------------------------------------------------------------------*/
/**
 * @brief ui_element_zoom UI元素缩放
 *
 * @Params elm UI元素句柄
 * @Params ratio_w 宽度方向缩放倍率
 * @Params ratio_h 高度方向缩放倍率
 */
/* ------------------------------------------------------------------------------------*/
static inline void ui_element_zoom(struct element *elm, float ratio_w, float ratio_h)
{
    if (ratio_w == 1 && ratio_h == 1) {
        elm->css.ratio.en = 0;
        elm->css.ratio.ratio_w = 1.0;
        elm->css.ratio.ratio_h = 1.0;
    } else {
        elm->css.ratio.en = 1;
        elm->css.ratio.ratio_w = ratio_w;
        elm->css.ratio.ratio_h = ratio_h;
    }
}



/* ------------------------------------------------------------------------------------*/
/**
 * @brief ui_element_rotate UI元素旋转
 *
 * @Params elm UI元素句柄
 * @Params cx 图标旋转中心X
 * @Params cy 图标旋转中心Y
 * @Params dx 屏幕旋转中心X
 * @Params dy 屏幕旋转中心Y
 * @Params angle 旋转角度
 */
/* ------------------------------------------------------------------------------------*/
static inline void ui_element_rotate(struct element *elm, int cx, int cy, int dx, int dy, unsigned short angle)
{
    elm->css.rotate.en = 1;
    // elm->css.rotate.chunk = 0;
    elm->css.rotate.cent_x = cx;
    elm->css.rotate.cent_y = cy;
    elm->css.rotate.dx = dx;
    elm->css.rotate.dy = dy;
    elm->css.rotate.angle = angle;
}



/* ------------------------------------------------------------------------------------*/
/**
 * @brief ui_element_own_rotate UI元素自转
 *
 * @Params elm UI元素句柄
 * @Params angle 旋转角度
 */
/* ------------------------------------------------------------------------------------*/
static inline void ui_element_own_rotate(struct element *elm, int angle)
{
    struct rect rect = {0};

    int new_angle = angle % 360;
    if (new_angle < 0) {
        new_angle += 360;
    }

    // printf("@@@@@ new_angle: %d\n", new_angle);

    ui_core_get_element_abs_rect(elm, &rect);
    unsigned short half_w = rect.width / 2;
    unsigned short half_h = rect.height / 2;

    ui_element_rotate(elm, half_w, half_h, rect.left + half_w, rect.top + half_h, angle);
}



/* ------------------------------------------------------------------------------------*/
/**
 * @brief ui_layout_child_set_index 设置布局内图片、文本控件的索引。适用于如菜单等场景
 *
 * @Params elm 布局元素句柄
 * @Params index 图标、文本索引
 */
/* ------------------------------------------------------------------------------------*/
static inline void ui_layout_child_set_index(struct element *elm, int index)
{
    struct element *p;
    int element_type;

    if (elm) {
        list_for_each_child_element(p, elm) {
            element_type = ui_id2type(p->id);
            switch (element_type) {
            case CTRL_TYPE_PIC:		// 图片
                ui_pic_set_image_index(p, index);
                break;
            case CTRL_TYPE_TEXT:	// 文本
                ui_text_set_index(p, index);
                break;
            }
        }
    }
}





#endif

