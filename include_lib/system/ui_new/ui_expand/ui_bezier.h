/* Copyright(C)
 * not free
 * All right reserved
 *
 * @file ui_bezier.h
 * @brief 贝塞尔算法模块
 *
 * 包含内容：
 * 1. 二、三阶贝塞尔元计算(及简化版)，用于计算单方向上(X方向或Y方向)的贝塞尔值；
 * 2. 二、三阶贝塞尔曲线点计算(及简化版)，用于计算在标准控制点下贝塞尔曲线点上的完整坐标(包含X和Y坐标)；
 * 3. 二、三阶贝塞尔插值器，用于计算指定t系数下的贝塞尔占比；
 * 4. 二、三阶贝塞尔估值器，用于计算指定t系数下的贝塞尔占比在指定范围内的具体估值。
 *
 * 注意：
 * 	1. 简化版：起点固定为0，终点固定为1，使用只需传入控制点(0到1之间)，计算结果永远居于0-1之间；
 * 	2. 插值器：与通用插值器概念类似；
 * 	3. 估值器：与通用估值器概念类似；
 *
 * @author zhuhaifang@zh-jieli.com
 * @version V1.0.0
 * @date 2023-03-13
 */


#ifndef __UI_BEZIER_H__
#define __UI_BEZIER_H__



/* ------------------------------------------------------------------------------------*/
/**
 * @brief 使用ui_typedef中定义的 fPoint_t(float_Point) 坐标结构体
 */
/* ------------------------------------------------------------------------------------*/
#include "ui_expand/ui_typedef.h"



/* ------------------------------------------------------------------------------------*/
/**
 * @brief 几个典型的三阶贝塞尔插值器控制点
 */
/* ------------------------------------------------------------------------------------*/
// ease (0.25, 0.1, 0.25, 1)
#define BEZIER_CONTROL_EASE			0.25,0.1,0.25,1.0

// linear (0, 0, 1, 1)
#define BEZIER_CONTROL_LINEAR		0.0,0.0,1.0,1.0

// ease-in (0.42, 0, 1, 1)
#define BEZIER_CONTROL_EASE_IN		0.42,0.0,1.0,1.0

// ease-out (0, 0, 0.58, 1)
#define BEZIER_CONTROL_EASE_OUT		0.0,0.0,0.58,1.0

// ease-in-out (0.42, 0, 0.58, 1)
#define BEZIER_CONTROL_EASE_IN_OUT	0.42,0.0,0.58,1.0

// typical (0, 1, 0, 1)
#define BEZIER_CONTROL_TYPICAL		1.0,0.0,0.0,1.0



/**************************** 二阶贝塞尔算法 ****************************/
/* ------------------------------------------------------------------------------------*/
/**
 * @brief TwoOrderMetaComputing 二阶贝塞尔元计算
 *
 * @param p0 起始点
 * @param p1 控制点
 * @param p2 结束点
 * @param t 系数
 *
 * @return 贝塞尔值
 */
/* ------------------------------------------------------------------------------------*/
float TwoOrderMetaComputing(float p0, float p1, float p2, float t);


/* ------------------------------------------------------------------------------------*/
/**
 * @brief TwoOrderSimpleMeta 二阶贝塞尔元计算(简化版)
 *
 * @param p1 控制点
 * @param t 系数
 *
 * @return 贝塞尔值
 */
/* ------------------------------------------------------------------------------------*/
float TwoOrderSimpleMeta(float p1, float t);


/* ------------------------------------------------------------------------------------*/
/**
 * @brief TwoOrderBezierPoint 二阶贝塞尔曲线点计算
 *
 * @param p0 起始点
 * @param p1 控制点
 * @param p2 结束点
 * @param t 系数
 *
 * @return 贝塞尔坐标点
 */
/* ------------------------------------------------------------------------------------*/
fPoint_t TwoOrderBezierPoint(fPoint_t p0, fPoint_t p1, fPoint_t p2, float t);


/* ------------------------------------------------------------------------------------*/
/**
 * @brief TwoOrderBezierSimple 二阶贝塞尔曲线点计算(简化版)
 *
 * @param p1 控制点
 * @param t 系数
 *
 * @return 贝塞尔坐标点
 */
/* ------------------------------------------------------------------------------------*/
fPoint_t TwoOrderBezierSimple(fPoint_t p1, float t);


/* ------------------------------------------------------------------------------------*/
/**
 * @brief TwoOrderBezierInterpolator 二阶贝塞尔插值器
 *
 * @param x1 控制点X坐标
 * @param y1 控制点Y坐标
 * @param t 系数
 *
 * @return 插值结果
 */
/* ------------------------------------------------------------------------------------*/
float TwoOrderBezierInterpolator(float x1, float y1, float t);


/* ------------------------------------------------------------------------------------*/
/**
 * @brief TwoOrderBezierEstimator 二阶贝塞尔估值器
 *
 * @param start 估值起始值
 * @param end 估值结束值
 * @param x1 控制点X坐标(0-1)
 * @param y1 控制点Y坐标(0-1)
 * @param t 系数
 *
 * @return 估值结果
 */
/* ------------------------------------------------------------------------------------*/
float TwoOrderBezierEstimator(float start, float end, float x1, float y1, float t);




/**************************** 三阶贝塞尔算法 ****************************/
/* ------------------------------------------------------------------------------------*/
/**
 * @brief ThirdOrderMetaComputing 三阶贝塞尔元计算
 *
 * @param p0 起始点
 * @param p1 控制点1
 * @param p2 控制点2
 * @param p3 结束点
 * @param t 系数
 *
 * @return 贝塞尔值
 */
/* ------------------------------------------------------------------------------------*/
float ThirdOrderMetaComputing(float p0, float p1, float p2, float p3, float t);


/* ------------------------------------------------------------------------------------*/
/**
 * @brief ThirdOrderSimpleMeta 三阶贝塞尔元计算(简化版)
 *
 * @param p1 控制点1
 * @param p2 控制点2
 * @param t 系数
 *
 * @return 贝塞尔值
 */
/* ------------------------------------------------------------------------------------*/
float ThirdOrderSimpleMeta(float p1, float p2, float t);


/* ------------------------------------------------------------------------------------*/
/**
 * @brief ThirdOrderBezierPoint 三阶贝塞尔曲线点计算
 *
 * @param p0 起始点
 * @param p1 控制点1
 * @param p2 控制点2
 * @param p3 结束点
 * @param t 系数
 *
 * @return 贝塞尔坐标点
 */
/* ------------------------------------------------------------------------------------*/
fPoint_t ThirdOrderBezierPoint(fPoint_t p0, fPoint_t p1, fPoint_t p2, fPoint_t p3, float t);


/* ------------------------------------------------------------------------------------*/
/**
 * @brief ThirdOrderBezierSimple 三阶贝塞尔曲线点计算(简化版)
 *
 * @param p1 控制点1
 * @param p2 控制点2
 * @param t 系数
 *
 * @return 贝塞尔坐标点
 */
/* ------------------------------------------------------------------------------------*/
fPoint_t ThirdOrderBezierSimple(fPoint_t p1, fPoint_t p2, float t);


/* ------------------------------------------------------------------------------------*/
/**
 * @brief ThirdOrderBezierInterpolator 三阶贝塞尔插值器
 *
 * @param x1 控制点1X坐标
 * @param y1 控制点1Y坐标
 * @param x2 控制点2X坐标
 * @param y2 控制点2Y坐标
 * @param t 系数
 *
 * @return 插值结果
 */
/* ------------------------------------------------------------------------------------*/
float ThirdOrderBezierInterpolator(float x1, float y1, float x2, float y2, float t);


/* ------------------------------------------------------------------------------------*/
/**
 * @brief ThirdOrderBezierEstimator 三阶贝塞尔估值器
 *
 * @param start 估值起始值
 * @param end 估值结束值
 * @param x1 控制点1 X坐标
 * @param y1 控制点1 Y坐标
 * @param x2 控制点2 X坐标
 * @param y2 控制点2 Y坐标
 * @param t 系数
 *
 * @return 估值结果
 */
/* ------------------------------------------------------------------------------------*/
float ThirdOrderBezierEstimator(float start, float end, float x1, float y1, float x2, float y2, float t);



#endif


