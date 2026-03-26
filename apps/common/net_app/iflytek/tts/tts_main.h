#ifndef __TTS_MAIN_H
#define __TTS_MAIN_H

#include "generic/includes.h"

typedef enum {
    IFLY_TTS_EVT_PLAY_START = 1,	// 播放开始。*param: ifly_tts_param*
    IFLY_TTS_EVT_PLAY_STOP,			// 播放结束。*param: ifly_tts_param*
    IFLY_TTS_EVT_PLAY_FAIL_STOP,	// 播放非正常结束。例如超时、强制结束等。*param: ifly_tts_param*
    IFLY_TTS_EVT_EXIT,				// 结束。*param: ifly_tts_param*
} ifly_tts_event_enum ;

typedef struct ifly_tts_struct {
    // 参数信息，所有参数都需要赋值
    char *text_res;		// 文本数据
    int (*event_cb)(ifly_tts_event_enum evt, void *param);		// 事件回调
} ifly_tts_param;


// TTS启动。*param参数句柄需要在stop之后才能释放
bool ifly_tts_start(ifly_tts_param *param);
// TTS结束。
void ifly_tts_stop(u8 force_stop, u32 to_ms);
// 判断tts是否正在运行
bool ifly_tts_is_work(void);
#endif
