/* Copyright(C)
 * not free
 * All right reserved
 *
 * @file ui_typedef.h
 * @brief 杰理UI框架跨模块公用标志、宏、数据类型定义
 * 用于类似跨输入、合成、输出三个功能模块的任意两个或三个，则其相关宏等内容在本文件定义，
 * 例如像素格式(输入格式决定读取方式、合成格式决定合成输出、输出格式决定输出给屏等)。
 * @author zhuhaifang@zh-jieli.com
 * @version V1.0.1
 * @date 2023-02-14
 */


#ifndef __UI_TYPEDEF_H__
#define __UI_TYPEDEF_H__


/* ------------------------------------------------------------------------------------*/
/**
 * @brief 杰理UI框架使用的π定义
 */
/* ------------------------------------------------------------------------------------*/
#define UI_PI	3.14159265f



/* ------------------------------------------------------------------------------------*/
/**
 * @brief UI像素格式定义(暂时未使用)
 */
/* ------------------------------------------------------------------------------------*/
typedef enum {
    PIXEL_FORMAT_ARGB888,	// 24位真彩色，带8位alpha
    PIXEL_FORMAT_ARGB565,	// 16位真彩色，带8位alpha
    PIXEL_FORMAT_RGB888,	// 24位真彩色，无alpha
    PIXEL_FORMAT_RGB565,	// 16位真彩色，无alpha
    PIXEL_FORMAT_OSD16,		// 16位真彩色，带镂空效果（一色值透明）
    PIXEL_FORMAT_AL88,		// 索引图像，8位颜色表，带8位alpha
    PIXEL_FORMAT_L8,		// 索引图像，8位颜色表，无alpha
    PIXEL_FORMAT_AL44,		// 索引图像，4位颜色表，带4位alpha
    PIXEL_FORMAT_L1,		// 索引图像，1位颜色表
    PIXEL_FORMAT_A8			// 仅8位alpha
} UI_pixel_format;


/* ------------------------------------------------------------------------------------*/
/**
 * @brief UI扩展模块错误代码定义
 */
/* ------------------------------------------------------------------------------------*/
typedef enum {
    ERR_SUCCESS,	// 正常的，没有错误
    ERR_OBJECT,		// 错误：对象或句柄不存在
    ERR_MIN_LIMIT,	// 到达最小限制值
    ERR_MAX_LIMIT,	// 到达最大限制值
    ERR_UNKNOW		// 错误：未知的错误
} UiExpandErrCode;



// int 型坐标点结构
struct int_Point {
    int x;
    int y;
};
typedef struct int_Point iPoint_t;


// flaot 型坐标点结构
struct float_Point {
    float x;
    float y;
};
typedef struct float_Point fPoint_t;


// unsigned int 型坐标点结构
struct unsigned_Point {
    unsigned int x;
    unsigned int y;
};
typedef struct unsigned_Point uPoint_t;


// short 型坐标点结构
struct short_Point {
    short x;
    short y;
};
typedef struct short_Point sPoint_t;


// rgb565
struct color_rgb565 {
    unsigned short r: 5;
    unsigned short g: 6;
    unsigned short b: 5;
};
typedef struct color_rgb565 cRGB565_t;


// argb565
struct color_argb565 {
    unsigned int a: 8;
    unsigned int r: 5;
    unsigned int g: 6;
    unsigned int b: 5;
};
typedef struct color_argb565 cARGB8565_t;


// rgb888
struct color_rgb888 {
    unsigned int r: 8;
    unsigned int g: 8;
    unsigned int b: 8;
};
typedef struct color_rgb888 cRGB888_t;


// argb8888
struct color_argb888 {
    unsigned int a: 8;
    unsigned int r: 8;
    unsigned int g: 8;
    unsigned int b: 8;
};
typedef struct color_argb888 cARGB8888_t;






#endif

