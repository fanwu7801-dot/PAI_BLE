
#ifndef __AI_AUDIO_H__
#define __AI_AUDIO_H__

#include "typedef.h"
#include "app_config.h"

enum {
    AI_AUDIO_EVENT_ERR_REC_START = 1,
    AI_AUDIO_EVENT_ERR_REC_STOP,
    AI_AUDIO_EVENT_ERR_REC_DATA,
};

// 初始化
int ai_audio_init(void (* evt_cb)(int event));

// 启动录音
int ai_audio_start(void);

// 停止录音
// cancel：1-取消
int ai_audio_stop(int cancel);

#endif//__AI_AUDIO_H__

