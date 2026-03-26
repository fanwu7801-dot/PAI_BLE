
/*
** 包含board的头文件，确定baord里面开关的屏驱宏
*/
#include "app_config.h"
#include "clock_cfg.h"
#include "asm/psram_api.h"


/*
** 驱动代码的宏开关
*/
//<<<[qspi屏 454x454]>>>//
#if (defined  TCFG_LCD_SPI_CO5300_v1_ENABLE) && TCFG_LCD_SPI_CO5300_v1_ENABLE

// #define LCD_DRIVE_CONFIG                    QSPI_RGB565_SUBMODE0_1T8B
#define LCD_DRIVE_CONFIG                    QSPI_RGB565_SUBMODE1_1T2B
/* #define LCD_DRIVE_CONFIG                    QSPI_RGB565_SUBMODE2_1T2B */

/*
** 包含imd头文件，屏驱相关的变量和结构体都定义在imd.h
*/
#include "asm/imd.h"
#include "asm/imb.h"
#include "includes.h"
#include "ui/ui_api.h"


#if 0
#define SCR_X 120
#define SCR_Y 110
#define SCR_W 240
#define SCR_H 240
#define LCD_W 240
#define LCD_H 240
#define LCD_BLOCK_W 240
#define LCD_BLOCK_H 60
#define BUF_NUM 2
#else
#define SCR_X 12
#define SCR_Y 6
#define SCR_W 454
#define SCR_H 454
#define LCD_W 454
#define LCD_H 454
#define LCD_BLOCK_W 454
#if (defined(TCFG_SMART_VOICE_ENABLE) && (TCFG_SMART_VOICE_ENABLE == 1))
#define LCD_BLOCK_H 20
#elif (TCFG_CAT1_AICXTEK_ENABLE && (!TCFG_PSRAM_DEV_ENABLE))
#define LCD_BLOCK_H 20
#else /*TCFG_SMART_VOICE_ENABLE == 0*/
#define LCD_BLOCK_H 40
#endif /*TCFG_SMART_VOICE_ENABLE*/
#define BUF_NUM 2
#endif



#if (TCFG_PSRAM_DEV_ENABLE)
#define LCD_FORMAT OUTPUT_FORMAT_RGB565
#else
#define LCD_FORMAT OUTPUT_FORMAT_RGB565
#endif



extern struct imd_param lcd_spi_CO5300_v1_param;

/*
** 初始化代码
*/
static const u8 lcd_spi_CO5300_v1_cmd_list_poweron[] ALIGNED(4) = {
#if 1

    _BEGIN_, 0xFE, 0x20, _END_,
    _BEGIN_, 0xF4, 0x5A, _END_,
    _BEGIN_, 0xF5, 0x59, _END_,
    _BEGIN_, 0xFE, 0x40, _END_,
    _BEGIN_, 0xD8, 0x33, _END_,
    _BEGIN_, 0xD9, 0x11, _END_,
    _BEGIN_, 0xDA, 0x00, _END_,
    _BEGIN_, 0xFE, 0x20, _END_,
    _BEGIN_, 0x1A, 0x15, _END_,
    _BEGIN_, 0xFE, 0x40, _END_,
    _BEGIN_, 0x01, 0x01, _END_,
    _BEGIN_, 0x02, 0x74, _END_,
    _BEGIN_, 0x59, 0x02, _END_,
    _BEGIN_, 0x5A, 0x19, _END_,
    _BEGIN_, 0x5B, 0x06, _END_,
    _BEGIN_, 0x5C, 0x08, _END_,
    _BEGIN_, 0x70, 0x02, _END_,
    _BEGIN_, 0x71, 0x19, _END_,
    _BEGIN_, 0x72, 0x06, _END_,
    _BEGIN_, 0x73, 0x08, _END_,
    _BEGIN_, 0xFE, 0x40, _END_,
    _BEGIN_, 0x5D, 0x24, _END_,
    _BEGIN_, 0x60, 0x08, _END_,
    _BEGIN_, 0x61, 0x01, _END_,
    _BEGIN_, 0x62, 0xDF, _END_,
    _BEGIN_, 0x0C, 0xD7, _END_,
    _BEGIN_, 0x0D, 0xF0, _END_,
    _BEGIN_, 0xFE, 0xE0, _END_,
    _BEGIN_, 0x00, 0x14, _END_,
    _BEGIN_, 0x01, 0x01, _END_,
    _BEGIN_, 0x02, 0x20, _END_,
    _BEGIN_, 0x04, 0x25, _END_,
    _BEGIN_, 0x06, 0x0F, _END_,
    _BEGIN_, 0x08, 0x00, _END_,
    _BEGIN_, 0x09, 0x14, _END_,
    _BEGIN_, 0x0A, 0x01, _END_,
    _BEGIN_, 0x0B, 0x20, _END_,
    _BEGIN_, 0x0C, 0x25, _END_,
    _BEGIN_, 0x0E, 0x0F, _END_,
    _BEGIN_, 0x0F, 0x00, _END_,
    _BEGIN_, 0x10, 0x14, _END_,
    _BEGIN_, 0x11, 0x14, _END_,
    _BEGIN_, 0x24, 0x01, _END_,
    _BEGIN_, 0x21, 0x8F, _END_,
    _BEGIN_, 0x2D, 0x8F, _END_,
    _BEGIN_, 0x32, 0x8F, _END_,
    _BEGIN_, 0x26, 0x41, _END_,
    _BEGIN_, 0x22, 0x1A, _END_,
    _BEGIN_, 0x23, 0x11, _END_,
    _BEGIN_, 0x30, 0x01, _END_,
    _BEGIN_, 0xFE, 0x40, _END_,
    _BEGIN_, 0x57, 0x43, _END_,
    _BEGIN_, 0x58, 0x33, _END_,
    _BEGIN_, 0x6E, 0x43, _END_,
    _BEGIN_, 0x6F, 0x33, _END_,
    _BEGIN_, 0x74, 0x43, _END_,
    _BEGIN_, 0x75, 0x33, _END_,
    _BEGIN_, 0xFE, 0x40, _END_,
    _BEGIN_, 0x12, 0xFE, _END_,
    _BEGIN_, 0x13, 0x08, _END_,
    _BEGIN_, 0xC9, 0x61, _END_,
    _BEGIN_, 0x96, 0x38, _END_,
    _BEGIN_, 0x97, 0x02, _END_,
    _BEGIN_, 0xA5, 0xFF, _END_,
    _BEGIN_, 0xAA, 0x38, _END_,
    _BEGIN_, 0xAB, 0x61, _END_,
    _BEGIN_, 0x98, 0x00, _END_,
    _BEGIN_, 0xA7, 0x38, _END_,
    _BEGIN_, 0xA9, 0x61, _END_,
    _BEGIN_, 0xFE, 0x80, _END_,
    _BEGIN_, 0x66, 0x11, _END_,
    _BEGIN_, 0x67, 0x17, _END_,
    _BEGIN_, 0x68, 0x04, _END_,
    _BEGIN_, 0xFE, 0x70, _END_,
    _BEGIN_, 0x9B, 0x02, _END_,
    _BEGIN_, 0x9C, 0x03, _END_,
    _BEGIN_, 0x9D, 0x08, _END_,
    _BEGIN_, 0x9E, 0x19, _END_,
    _BEGIN_, 0x9F, 0x19, _END_,
    _BEGIN_, 0xA0, 0x19, _END_,
    _BEGIN_, 0xA2, 0x19, _END_,
    _BEGIN_, 0xA3, 0x19, _END_,
    _BEGIN_, 0xA4, 0x19, _END_,
    _BEGIN_, 0xA5, 0x19, _END_,
    _BEGIN_, 0xA6, 0x0E, _END_,
    _BEGIN_, 0xA7, 0x0D, _END_,
    _BEGIN_, 0xA9, 0x0C, _END_,
    _BEGIN_, 0xAA, 0x19, _END_,
    _BEGIN_, 0xAB, 0x19, _END_,
    _BEGIN_, 0xAC, 0x19, _END_,
    _BEGIN_, 0xAD, 0x19, _END_,
    _BEGIN_, 0xAE, 0x19, _END_,
    _BEGIN_, 0xAF, 0x19, _END_,
    _BEGIN_, 0xB0, 0x19, _END_,
    _BEGIN_, 0xB1, 0x19, _END_,
    _BEGIN_, 0xB2, 0x19, _END_,
    _BEGIN_, 0xB3, 0x19, _END_,
    _BEGIN_, 0xB4, 0x19, _END_,
    _BEGIN_, 0xB5, 0x19, _END_,
    _BEGIN_, 0xB6, 0x00, _END_,
    _BEGIN_, 0xB7, 0x01, _END_,
    _BEGIN_, 0xB8, 0x09, _END_,
    _BEGIN_, 0xB9, 0x19, _END_,
    _BEGIN_, 0xBA, 0x19, _END_,
    _BEGIN_, 0xBB, 0x19, _END_,
    _BEGIN_, 0xBC, 0x19, _END_,
    _BEGIN_, 0xBD, 0xF9, _END_,
    _BEGIN_, 0xBE, 0x19, _END_,
    _BEGIN_, 0xBF, 0x19, _END_,
    _BEGIN_, 0xC0, 0x0F, _END_,
    _BEGIN_, 0xC1, 0x30, _END_,
    _BEGIN_, 0xC2, 0x11, _END_,
    _BEGIN_, 0xC3, 0x19, _END_,
    _BEGIN_, 0xC4, 0x19, _END_,
    _BEGIN_, 0xC5, 0x19, _END_,
    _BEGIN_, 0xC6, 0x19, _END_,
    _BEGIN_, 0xC7, 0x19, _END_,
    _BEGIN_, 0xC8, 0x19, _END_,
    _BEGIN_, 0xFE, 0x40, _END_,
    _BEGIN_, 0x4C, 0x21, _END_,
    _BEGIN_, 0x53, 0xF0, _END_,
    _BEGIN_, 0xFE, 0xF0, _END_,
    _BEGIN_, 0x72, 0x36, _END_,
    _BEGIN_, 0x73, 0x63, _END_,
    _BEGIN_, 0x74, 0x14, _END_,
    _BEGIN_, 0x75, 0x41, _END_,
    _BEGIN_, 0x76, 0x25, _END_,
    _BEGIN_, 0x77, 0x52, _END_,
    _BEGIN_, 0x78, 0x36, _END_,
    _BEGIN_, 0x79, 0x63, _END_,
    _BEGIN_, 0x7A, 0x14, _END_,
    _BEGIN_, 0x7B, 0x41, _END_,
    _BEGIN_, 0x7C, 0x25, _END_,
    _BEGIN_, 0x7D, 0x52, _END_,
    _BEGIN_, 0x7E, 0x14, _END_,
    _BEGIN_, 0x7F, 0x41, _END_,
    _BEGIN_, 0x80, 0x36, _END_,
    _BEGIN_, 0x81, 0x63, _END_,
    _BEGIN_, 0x82, 0x25, _END_,
    _BEGIN_, 0x83, 0x52, _END_,
    _BEGIN_, 0x84, 0x14, _END_,
    _BEGIN_, 0x85, 0x41, _END_,
    _BEGIN_, 0x86, 0x36, _END_,
    _BEGIN_, 0x87, 0x63, _END_,
    _BEGIN_, 0x88, 0x25, _END_,
    _BEGIN_, 0x89, 0x52, _END_,
    _BEGIN_, 0xFE, 0x70, _END_,
    _BEGIN_, 0x00, 0xC0, _END_,
    _BEGIN_, 0x01, 0x08, _END_,
    _BEGIN_, 0x02, 0x04, _END_,
    _BEGIN_, 0x03, 0x01, _END_,
    _BEGIN_, 0x04, 0x00, _END_,
    _BEGIN_, 0x05, 0x03, _END_,
    _BEGIN_, 0x06, 0x1F, _END_,
    _BEGIN_, 0x07, 0x1F, _END_,
    _BEGIN_, 0x09, 0xC0, _END_,
    _BEGIN_, 0x0A, 0x08, _END_,
    _BEGIN_, 0x0B, 0x04, _END_,
    _BEGIN_, 0x0C, 0x01, _END_,
    _BEGIN_, 0x0D, 0x00, _END_,
    _BEGIN_, 0x0E, 0x01, _END_,
    _BEGIN_, 0x0F, 0x1F, _END_,
    _BEGIN_, 0x10, 0x1F, _END_,
    _BEGIN_, 0x12, 0xC0, _END_,
    _BEGIN_, 0x13, 0x08, _END_,
    _BEGIN_, 0x14, 0x02, _END_,
    _BEGIN_, 0x15, 0x00, _END_,
    _BEGIN_, 0x16, 0x00, _END_,
    _BEGIN_, 0x17, 0x01, _END_,
    _BEGIN_, 0x18, 0xD1, _END_,
    _BEGIN_, 0x19, 0x00, _END_,
    _BEGIN_, 0x1B, 0xC0, _END_,
    _BEGIN_, 0x1C, 0x08, _END_,
    _BEGIN_, 0x1D, 0x02, _END_,
    _BEGIN_, 0x1E, 0x00, _END_,
    _BEGIN_, 0x1F, 0x00, _END_,
    _BEGIN_, 0x20, 0x00, _END_,
    _BEGIN_, 0x21, 0xD1, _END_,
    _BEGIN_, 0x22, 0x00, _END_,
    _BEGIN_, 0x4C, 0x80, _END_,
    _BEGIN_, 0x4D, 0x00, _END_,
    _BEGIN_, 0x4E, 0x01, _END_,
    _BEGIN_, 0x4F, 0x00, _END_,
    _BEGIN_, 0x50, 0x01, _END_,
    _BEGIN_, 0x51, 0xB2, _END_,
    _BEGIN_, 0x52, 0x1F, _END_,
    _BEGIN_, 0x53, 0xC6, _END_,
    _BEGIN_, 0x54, 0x00, _END_,
    _BEGIN_, 0x55, 0x03, _END_,
    _BEGIN_, 0x56, 0x01, _END_,
    _BEGIN_, 0x58, 0x00, _END_,
    _BEGIN_, 0x65, 0xE0, _END_,
    _BEGIN_, 0x66, 0x08, _END_,
    _BEGIN_, 0x67, 0x10, _END_,
    _BEGIN_, 0xFE, 0xF0, _END_,
    _BEGIN_, 0xA3, 0x00, _END_,
    _BEGIN_, 0xFE, 0x70, _END_,
    _BEGIN_, 0x76, 0x07, _END_,
    _BEGIN_, 0x77, 0x00, _END_,
    _BEGIN_, 0x78, 0x03, _END_,
    _BEGIN_, 0x68, 0x07, _END_,
    _BEGIN_, 0x69, 0x07, _END_,
    _BEGIN_, 0x6A, 0x07, _END_,
    _BEGIN_, 0x6B, 0x07, _END_,
    _BEGIN_, 0x6C, 0x07, _END_,
    _BEGIN_, 0x6D, 0x07, _END_,
    _BEGIN_, 0xFE, 0xF0, _END_,
    _BEGIN_, 0xA9, 0x17, _END_,
    _BEGIN_, 0xAA, 0x17, _END_,
    _BEGIN_, 0xAB, 0x17, _END_,
    _BEGIN_, 0xAC, 0x17, _END_,
    _BEGIN_, 0xAD, 0x17, _END_,
    _BEGIN_, 0xAE, 0x17, _END_,
    _BEGIN_, 0xFE, 0x70, _END_,
    _BEGIN_, 0x93, 0x07, _END_,
    _BEGIN_, 0x94, 0x00, _END_,
    _BEGIN_, 0x96, 0x03, _END_,
    _BEGIN_, 0xDB, 0x07, _END_,
    _BEGIN_, 0xDC, 0x07, _END_,
    _BEGIN_, 0xDD, 0x07, _END_,
    _BEGIN_, 0xDE, 0x07, _END_,
    _BEGIN_, 0xDF, 0x07, _END_,
    _BEGIN_, 0xE0, 0x07, _END_,
    _BEGIN_, 0xE7, 0x17, _END_,
    _BEGIN_, 0xE8, 0x17, _END_,
    _BEGIN_, 0xE9, 0x17, _END_,
    _BEGIN_, 0xEA, 0x17, _END_,
    _BEGIN_, 0xEB, 0x17, _END_,
    _BEGIN_, 0xEC, 0x17, _END_,
    _BEGIN_, 0xFE, 0x70, _END_,
    _BEGIN_, 0xD1, 0xF0, _END_,
    _BEGIN_, 0xD2, 0xFF, _END_,
    _BEGIN_, 0xD3, 0xF0, _END_,
    _BEGIN_, 0xD4, 0xFF, _END_,
    _BEGIN_, 0xD5, 0xA0, _END_,
    _BEGIN_, 0xD6, 0xAA, _END_,
    _BEGIN_, 0xD7, 0xF0, _END_,
    _BEGIN_, 0xD8, 0xFF, _END_,
    _BEGIN_, 0xFE, 0x40, _END_,
    _BEGIN_, 0x4D, 0x2A, _END_,
    _BEGIN_, 0x4E, 0x00, _END_,
    _BEGIN_, 0x4F, 0x00, _END_,
    _BEGIN_, 0x50, 0x00, _END_,
    _BEGIN_, 0x51, 0xF3, _END_,
    _BEGIN_, 0x52, 0x23, _END_,
    _BEGIN_, 0x6B, 0xF3, _END_,
    _BEGIN_, 0x6C, 0x13, _END_,
    _BEGIN_, 0x8F, 0xFF, _END_,
    _BEGIN_, 0x90, 0xFF, _END_,
    _BEGIN_, 0x91, 0x3F, _END_,
    _BEGIN_, 0xA2, 0x10, _END_,
    _BEGIN_, 0x07, 0x21, _END_,
    _BEGIN_, 0x35, 0x81, _END_,
    _BEGIN_, 0xFE, 0x40, _END_,
    _BEGIN_, 0x33, 0x10, _END_,
    _BEGIN_, 0x34, 0xC1, _END_,
    _BEGIN_, 0xFE, 0x50, _END_,
    _BEGIN_, 0xA9, 0x20, _END_,
    _BEGIN_, 0xAA, 0xB8, _END_,
    _BEGIN_, 0xAB, 0x01, _END_,
    _BEGIN_, 0xFE, 0x60, _END_,
    _BEGIN_, 0xA9, 0x20, _END_,
    _BEGIN_, 0xAA, 0xB8, _END_,
    _BEGIN_, 0xAB, 0x01, _END_,
    _BEGIN_, 0xFE, 0x30, _END_,
    _BEGIN_, 0xA9, 0x20, _END_,
    _BEGIN_, 0xAA, 0xB8, _END_,
    _BEGIN_, 0xAB, 0x01, _END_,
    _BEGIN_, 0xFE, 0x90, _END_,
    _BEGIN_, 0xA4, 0xEF, _END_,
    _BEGIN_, 0xA5, 0xE9, _END_,
    _BEGIN_, 0xA6, 0x00, _END_,
    _BEGIN_, 0xA7, 0xE9, _END_,
    _BEGIN_, 0xA9, 0xE9, _END_,
    _BEGIN_, 0xAA, 0x80, _END_,
    _BEGIN_, 0xAB, 0x47, _END_,
    _BEGIN_, 0xAC, 0xFF, _END_,
    _BEGIN_, 0xAE, 0x0C, _END_,
    _BEGIN_, 0x45, 0x00, _END_,
    _BEGIN_, 0xFE, 0x90, _END_,
    _BEGIN_, 0x4E, 0x13, _END_,
    _BEGIN_, 0x4F, 0x07, _END_,
    _BEGIN_, 0x50, 0xD8, _END_,
    _BEGIN_, 0x51, 0x00, _END_,
    _BEGIN_, 0x52, 0x80, _END_,
    _BEGIN_, 0x53, 0x00, _END_,
    _BEGIN_, 0x54, 0x10, _END_,
    _BEGIN_, 0x55, 0x80, _END_,
    _BEGIN_, 0x56, 0x00, _END_,
    _BEGIN_, 0x57, 0x00, _END_,
    _BEGIN_, 0x58, 0x00, _END_,
    _BEGIN_, 0x59, 0x08, _END_,
    _BEGIN_, 0x5A, 0x08, _END_,
    _BEGIN_, 0x5B, 0x10, _END_,
    _BEGIN_, 0x5C, 0x00, _END_,
    _BEGIN_, 0x5D, 0x00, _END_,
    _BEGIN_, 0x5E, 0x00, _END_,
    _BEGIN_, 0x5F, 0x00, _END_,
    _BEGIN_, 0x60, 0x00, _END_,
    _BEGIN_, 0x61, 0x08, _END_,
    _BEGIN_, 0x62, 0x10, _END_,
    _BEGIN_, 0x63, 0x00, _END_,
    _BEGIN_, 0x64, 0x08, _END_,
    _BEGIN_, 0x65, 0x00, _END_,
    _BEGIN_, 0x66, 0x08, _END_,
    _BEGIN_, 0x67, 0x00, _END_,
    _BEGIN_, 0x68, 0x48, _END_,
    _BEGIN_, 0x69, 0x00, _END_,
    _BEGIN_, 0x6A, 0x00, _END_,
    _BEGIN_, 0x6B, 0x00, _END_,
    _BEGIN_, 0x6C, 0x00, _END_,
    _BEGIN_, 0x6D, 0x48, _END_,
    _BEGIN_, 0x6E, 0x00, _END_,
    _BEGIN_, 0x6F, 0x10, _END_,
    _BEGIN_, 0x70, 0x04, _END_,
    _BEGIN_, 0x71, 0x00, _END_,
    _BEGIN_, 0x72, 0x00, _END_,
    _BEGIN_, 0x73, 0x00, _END_,
    _BEGIN_, 0x74, 0x00, _END_,
    _BEGIN_, 0x75, 0x04, _END_,
    _BEGIN_, 0x76, 0x10, _END_,
    _BEGIN_, 0x77, 0x04, _END_,
    _BEGIN_, 0x78, 0x08, _END_,
    _BEGIN_, 0x79, 0x80, _END_,
    _BEGIN_, 0x7A, 0x00, _END_,
    _BEGIN_, 0x7B, 0x00, _END_,
    _BEGIN_, 0x7C, 0x44, _END_,
    _BEGIN_, 0x7D, 0x10, _END_,
    _BEGIN_, 0x7E, 0x00, _END_,
    _BEGIN_, 0x7F, 0x00, _END_,
    _BEGIN_, 0x80, 0x00, _END_,
    _BEGIN_, 0x81, 0x00, _END_,
    _BEGIN_, 0x82, 0x00, _END_,
    _BEGIN_, 0x83, 0x44, _END_,
    _BEGIN_, 0x84, 0x04, _END_,
    _BEGIN_, 0x85, 0x00, _END_,
    _BEGIN_, 0x86, 0x08, _END_,
    _BEGIN_, 0x87, 0x00, _END_,
    _BEGIN_, 0x88, 0x00, _END_,
    _BEGIN_, 0x89, 0x00, _END_,
    _BEGIN_, 0x8A, 0x10, _END_,
    _BEGIN_, 0x8B, 0x88, _END_,
    _BEGIN_, 0x8C, 0x00, _END_,
    _BEGIN_, 0x8D, 0x00, _END_,
    _BEGIN_, 0x8E, 0x00, _END_,
    _BEGIN_, 0x8F, 0x08, _END_,
    _BEGIN_, 0x90, 0x00, _END_,
    _BEGIN_, 0x91, 0x10, _END_,
    _BEGIN_, 0x92, 0x08, _END_,
    _BEGIN_, 0x93, 0x00, _END_,
    _BEGIN_, 0x94, 0x00, _END_,
    _BEGIN_, 0x95, 0x00, _END_,
    _BEGIN_, 0x96, 0x00, _END_,
    _BEGIN_, 0x97, 0x80, _END_,
    _BEGIN_, 0x98, 0x10, _END_,
    _BEGIN_, 0x99, 0x00, _END_,
    _BEGIN_, 0x9A, 0x08, _END_,
    _BEGIN_, 0x9B, 0x00, _END_,
    _BEGIN_, 0x9C, 0x08, _END_,
    _BEGIN_, 0x9D, 0x00, _END_,
    _BEGIN_, 0x9E, 0x40, _END_,
    _BEGIN_, 0x9F, 0x08, _END_,
    _BEGIN_, 0xA0, 0x00, _END_,
    _BEGIN_, 0xA2, 0x00, _END_,
    _BEGIN_, 0xFE, 0x70, _END_,
    _BEGIN_, 0x98, 0x74, _END_,
    _BEGIN_, 0xC9, 0x02, _END_,
    _BEGIN_, 0xCA, 0x02, _END_,
    _BEGIN_, 0xCB, 0x02, _END_,
    _BEGIN_, 0xCC, 0x02, _END_,
    _BEGIN_, 0xCD, 0x02, _END_,
    _BEGIN_, 0xCE, 0x82, _END_,
    _BEGIN_, 0xCF, 0x02, _END_,
    _BEGIN_, 0xD0, 0x42, _END_,
    _BEGIN_, 0xFE, 0xE0, _END_,
    _BEGIN_, 0x19, 0x43, _END_,
    _BEGIN_, 0x1E, 0x43, _END_,
    _BEGIN_, 0x1C, 0x41, _END_,
    _BEGIN_, 0x18, 0x00, _END_,
    _BEGIN_, 0x1B, 0x0C, _END_,
    _BEGIN_, 0x1A, 0x9A, _END_,
    _BEGIN_, 0x1D, 0xDA, _END_,
    _BEGIN_, 0x28, 0x58, _END_,
    _BEGIN_, 0x05, 0x04, _END_,
    _BEGIN_, 0x0D, 0x04, _END_,
    _BEGIN_, 0xFE, 0x40, _END_,
    _BEGIN_, 0x54, 0xAC, _END_,
    _BEGIN_, 0x55, 0xA0, _END_,
    _BEGIN_, 0x48, 0xAA, _END_,
    _BEGIN_, 0xFE, 0x90, _END_,
    _BEGIN_, 0x31, 0x08, _END_,
    _BEGIN_, 0xFE, 0x00, _END_,
    _BEGIN_, 0xC4, 0x80, _END_,
    _BEGIN_, 0x3A, 0x55, _END_,
    _BEGIN_, 0x35, 0x00, _END_,
    _BEGIN_, 0x53, 0x20, _END_,
    _BEGIN_, 0x51, 0xFF, _END_,
    _BEGIN_, 0x63, 0xFF, _END_,
    _BEGIN_, 0x2A, 0x00, 0x06, 0x01, 0xD7, _END_,
    _BEGIN_, 0x2B, 0x00, 0x00, 0x01, 0xD1, _END_,
    _BEGIN_, 0x11, _END_,
    _BEGIN_, REGFLAG_DELAY, 120, _END_,
    _BEGIN_, 0x29, _END_,
    _BEGIN_, REGFLAG_DELAY, 60, _END_,
#else
    _BEGIN_, 0xfe, 0x00,  _END_,
    _BEGIN_, 0xc4, 0x80,  _END_,

    _BEGIN_, 0x3A, 0x55, _END_,
    _BEGIN_, 0x35, 0x00, _END_,
#if defined(TCFG_LCD_FLIP_ENABLE) && TCFG_LCD_FLIP_ENABLE
    _BEGIN_, 0x36, 0x40, _END_,
#else
    _BEGIN_, 0x36, 0x00, _END_,
#endif
    _BEGIN_, 0x53, 0x20, _END_,
    _BEGIN_, 0x51, 0xff, _END_,
    _BEGIN_, 0x63, 0xff, _END_,
    _BEGIN_, 0x2A, 0x00, 0X06, 0X01, 0Xd8, _END_, //0012,01d1
    _BEGIN_, 0x2B, 0x00, 0X00, 0X01, 0Xd1, _END_, //0006,01c8
    /* _BEGIN_, 0x2A, 0x00,0X06,0X01,0XD7, _END_, */
    /* _BEGIN_, 0x2B, 0x00,0X00,0X01,0XD7, _END_, */
    _BEGIN_, 0x11, _END_,
    _BEGIN_, REGFLAG_DELAY, 80, _END_,
    _BEGIN_, 0x29, _END_,
#endif
};


static const u8 lcd_cmd_list_sleepout[] ALIGNED(4) = {
    _BEGIN_, 0xfe, 0x00,  _END_,
    _BEGIN_, 0xc4, 0x80,  _END_,

    _BEGIN_, 0x3A, 0x55, _END_,
    _BEGIN_, 0x35, 0x00, _END_,
#if defined(TCFG_LCD_FLIP_ENABLE) && TCFG_LCD_FLIP_ENABLE
    _BEGIN_, 0x36, 0x40, _END_,
#else
    _BEGIN_, 0x36, 0x00, _END_,
#endif
    _BEGIN_, 0x53, 0x20, _END_,
    _BEGIN_, 0x51, 0xff, _END_,
    _BEGIN_, 0x63, 0xff, _END_,
    _BEGIN_, 0x2A, 0x00, 0X0c, 0X01, 0XD1, _END_,
    _BEGIN_, 0x2B, 0x00, 0X06, 0X01, 0Xc8, _END_,
    /* _BEGIN_, 0x2A, 0x00,0X06,0X01,0XD7, _END_, */
    /* _BEGIN_, 0x2B, 0x00,0X00,0X01,0XD7, _END_, */

    _BEGIN_, 0x11, _END_,
    _BEGIN_, REGFLAG_DELAY, 80, _END_,
    _BEGIN_, 0x29, _END_,
};


extern struct lcd_spi_platform_data *lcd_get_platform_data();

static void lcd_send_cmd_enable()
{
    u8 para[2];
    para[0] = 0x5a;
    para[1] = 0x5a;
    lcd_write_cmd(0xc0, para, 2);
    lcd_write_cmd(0xc1, para, 2);
}

static void lcd_send_cmd_disable()
{
    u8 para[2];
    para[0] = 0xa5;
    para[1] = 0xa5;
    lcd_write_cmd(0xc0, para, 2);
    lcd_write_cmd(0xc1, para, 2);
}

static void lcd_adjust_display_brightness(u8 percent)
{
    u16 brightness;
    u8 para[2];

    brightness = percent * 0x3ff / 100;

    para[0] = brightness & 0xff;
    para[1] = (brightness >> 8) & 0x3;

    lcd_write_cmd(0x51, para, 2);
    /* lcd_write_cmd(0x4a, para, 2); //Brightness Value of AOD Mode, 无效果 */
    /* lcd_write_cmd(0x63, para, 2); //Brightness Value of HBM Mode, 无效果 */
}

/*
** lcd背光控制
** 考虑到手表应用lcd背光控制需要更灵活自由，可能需要pwm调光，随时亮灭等
** 因此内部不操作lcd背光，全部由外部自行控制
*/
static int lcd_spi_CO5300_v1_backlight_ctrl(u8 percent)
{
    if (percent) {
        lcd_write_cmd(0x29, NULL, 0);
        if (percent <= 20) {
            lcd_adjust_display_brightness(1);
        } else if (percent <= 40) {
            lcd_adjust_display_brightness(3);
        } else if (percent <= 60) {
            lcd_adjust_display_brightness(6);
        } else if (percent <= 80) {
            lcd_adjust_display_brightness(10);
        } else if (percent <= 100) {
            lcd_adjust_display_brightness(100);
        }
        /* g_printf("lcd backlight percent:%d",percent); */
    } else {
        lcd_write_cmd(0x28, NULL, 0);
    }

    return 0;
}


/*
** lcd电源控制
*/
static int lcd_spi_CO5300_v1_power_ctrl(u8 onoff)
{
    lcd_en_ctrl(onoff);
    return 0;
}


#define DEEP_STANDBY  1

/*
** 设置lcd进入睡眠
*/
static void lcd_spi_CO5300_v1_entersleep(void)
{
    u8 dstb = 0x01;

    lcd_write_cmd(0x28, NULL, 0);
    lcd_write_cmd(0x10, NULL, 0);
    delay_2ms(120 / 2);	// delay 120ms
#if DEEP_STANDBY
    lcd_write_cmd(0x4f, &dstb, sizeof(dstb));
    delay_2ms(102 / 2);	// delay 120ms
    struct lcd_platform_data *lcd_dat = lcd_get_platform_data();
    if (lcd_dat->pin_te != NO_CONFIG_PORT) {
        gpio_set_pull_up(lcd_dat->pin_te, 0);
        gpio_set_pull_down(lcd_dat->pin_te, 0);
        gpio_direction_input(lcd_dat->pin_te);
        gpio_set_die(lcd_dat->pin_te, 0);
    }
#endif

}



/*
** 设置lcd退出睡眠
*/
static void lcd_spi_CO5300_v1_exitsleep(void)
{
#if DEEP_STANDBY
    struct lcd_platform_data *lcd_dat = lcd_get_platform_data();
    if (lcd_dat && lcd_dat->pin_reset) {
        gpio_direction_output(lcd_dat->pin_reset, 0);
        delay_2ms(4);	// delay >5ms
        gpio_direction_output(lcd_dat->pin_reset, 1);
        delay_2ms(4);	// delay >5ms
    }

    lcd_write_cmd(0x11, NULL, 0);
    delay_2ms(5);
    lcd_write_cmd(0x29, NULL, 0);
    delay_2ms(5);

    lcd_init(&lcd_spi_CO5300_v1_param);
    lcd_drv_cmd_list(lcd_cmd_list_sleepout, sizeof(lcd_cmd_list_sleepout) / sizeof(lcd_cmd_list_sleepout[0]));
#else
    lcd_write_cmd(0x11, NULL, 0);
    delay_2ms(5);	// delay 120ms
    lcd_write_cmd(0x29, NULL, 0);
#endif


}
void lcd_clear_screen()
{
    lcd_clear(0, 7 + 466, 0, 466, 0);
}
static u32 lcd_spi_CO5300_v1_read_id()
{
    u8 id[3];
    lcd_read_cmd(0x04, id, sizeof(id));
    return ((id[0] << 16) | (id[1] << 8) | id[2]);
}

struct imd_param lcd_spi_CO5300_v1_param = {
    .scr_x    = SCR_X,
    .scr_y	  = SCR_Y,
    .scr_w	  = SCR_W,
    .scr_h	  = SCR_H,

    .in_width  = SCR_W,
    .in_height = SCR_H,
    .in_format = LCD_FORMAT,


    .lcd_width  = LCD_W,
    .lcd_height = LCD_H,

    .lcd_type = LCD_TYPE_SPI,

    .buffer_num = BUF_NUM,
    .buffer_size = LCD_BLOCK_W * LCD_BLOCK_H * 2,

    .fps = 60,

    .spi = {
        .spi_mode = SPI_IF_MODE(LCD_DRIVE_CONFIG),
        .pixel_type = PIXEL_TYPE(LCD_DRIVE_CONFIG),
        .out_format = OUT_FORMAT(LCD_DRIVE_CONFIG),
        .port = SPI_PORTA,
        .spi_dat_mode = SPI_MODE_UNIDIR,
    },

    .debug_mode_en = 0,
    .debug_mode_color = 0x0,
};

REGISTER_LCD_DEVICE(CO5300_v1) = {
    .logo = "CO5300_v1",
    .row_addr_align    = 2,
    .column_addr_align = 2,

    .radius		= SCR_W / 2,
    .fill_argb	= 0xffffffff,

    .lcd_cmd = (void *) &lcd_spi_CO5300_v1_cmd_list_poweron,
    .cmd_cnt = sizeof(lcd_spi_CO5300_v1_cmd_list_poweron) / sizeof(lcd_spi_CO5300_v1_cmd_list_poweron[0]),
    .param   = (void *) &lcd_spi_CO5300_v1_param,

    .reset = NULL,	// 没有特殊的复位操作，用内部普通复位函数即可
    .backlight_ctrl = lcd_spi_CO5300_v1_backlight_ctrl,
    .power_ctrl = lcd_spi_CO5300_v1_power_ctrl,
    .entersleep = lcd_spi_CO5300_v1_entersleep,
    .exitsleep = lcd_spi_CO5300_v1_exitsleep,
    .read_id = lcd_spi_CO5300_v1_read_id,
    .lcd_id = 0x000000,
};


#endif


