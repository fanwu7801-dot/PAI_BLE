#ifndef __PORTABLE_INCLUDE_AUDIO_H__
#define __PORTABLE_INCLUDE_AUDIO_H__

#include "system/includes.h"

#include <stdint.h>
#include <stdbool.h>

#define AUDIO_CHANNEL_SYNC      1


/*
*********************************************************************
* Description: aicxtek 音频通道初始化
* Arguments  : None.
* Return	 : None.
* Note(s)    : None.
*********************************************************************
*/
void aic_audio_init(void);

/*
*********************************************************************
* Description: aicxtek 音频通道关闭
* Arguments  : None.
* Return	 : None.
* Note(s)    : None.
*********************************************************************
*/
void aic_audio_close(void);

/*
*********************************************************************
* Description: aicxtek 音频通道打开
* Arguments  : None.
* Return	 : 0		success
* Note(s)    : None.
*********************************************************************
*/
int aic_audio_open(void);


/*
*********************************************************************
* Description: aicxtek 判断音频通道运行中
* Arguments  : None.
* Return	 : true		运行中
               false	没有运行
* Note(s)    : None.
*********************************************************************
*/
bool aic_audio_is_run(void);


/*
*********************************************************************
* Description: aicxtek mic数据清零
* Arguments  : enable       1-清零，0-不清
* Return	 : None.
* Note(s)    : None.
*********************************************************************
*/
void aic_audio_input_clear_enable(u8 enable);

/*
*********************************************************************
* Description: aicxtek 判断mic数据是否清零
* Arguments  : None.
* Return	 : 1		清零
               0        不清
* Note(s)    : None.
*********************************************************************
*/
int aic_audio_get_input_clear_status(void);

#endif // #ifndef __PORTABLE_INCLUDE_AUDIO_H__

