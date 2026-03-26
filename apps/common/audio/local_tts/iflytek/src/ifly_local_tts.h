#ifndef __IFLY_LOCAL_TTS_H__
#define __IFLY_LOCAL_TTS_H__

#include "system/includes.h"
#include "tts.h"
#include "ttsConfig.h"

enum IFLY_LOCAL_TTS_EVENT {
    IFLY_LOCAL_TTS_EVENT_OK = 0,
    IFLY_LOCAL_TTS_EVENT_ERR_OPEN_RES,
    IFLY_LOCAL_TTS_EVENT_ERR_MALLOC,
    IFLY_LOCAL_TTS_EVENT_ERR_TASK_CREATE,
    IFLY_LOCAL_TTS_EVENT_ERR_DEC_STOP, 	// 主动停止
    IFLY_LOCAL_TTS_EVENT_ERR_DEC_END,	// 解码结束
};


/*
*********************************************************************
* Description: 科大讯飞离线TTS解码停止
* Arguments  : None.
* Return	 : None.
* Note(s)    : None.
*********************************************************************
*/
void ifly_local_tts_stop(void);

/*
*********************************************************************
* Description: 科大讯飞离线TTS解码
* Arguments  : *dat				解码数据
*			   *dat_len			数据长度
*			   doce_page		数据编码格式，ttsConfig.h，ifly_TTS_CODEPAGE_xxx
*			   dat_malloc		数据是否需要申请拷贝。0-不需要拷贝；1-需要拷贝
*			   *cb_priv			事件回调私有参数
*			   *cb				事件回调接口（enum IFLY_LOCAL_TTS_EVENT）
* Return	 : 0 - ok；非0 - enum IFLY_LOCAL_TTS_EVENT
* Note(s)    : None.
*********************************************************************
*/
int ifly_local_tts_play(char *dat, int dat_len, int code_page, u8 dat_malloc, void *cb_priv, void (*cb)(void *priv, int event));

#endif // #ifndef __IFLY_LOCAL_TTS_H__
