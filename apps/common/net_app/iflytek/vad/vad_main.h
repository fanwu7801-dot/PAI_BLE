#ifndef __VAD_MAIN_H
#define __VAD_MAIN_H

#include "generic/includes.h"

#define MAX_VAD_LEN  1000

typedef enum {
    IFLY_VAD_EVT_AUDIO_START = 1,	// 音频启动。*param: ifly_vad_param*
    IFLY_VAD_EVT_RECV_OK,		// VAD接受数据完毕。*param: ifly_vad_param*
    IFLY_VAD_EVT_NETWORK_FAIL,  // 网络错误。*param: ifly_ai_param*
    IFLY_VAD_EVT_NETWORK_RECV_ERROR,  // 网络接收错误。*param: ifly_ai_param*
    IFLY_VAD_EVT_EXIT,			// 结束。*param: ifly_ai_param*
} ifly_vad_event_enum ;

typedef struct ifly_vad_struct {
    // 参数信息，所有参数都需要赋值
    char *vad_res;		// 输出数据
    u32 vad_res_len;		// 输出数据buf长度，最大MAX_VAD_LEN
    int (*event_cb)(ifly_vad_event_enum evt, void *param);		// 事件回调
} ifly_vad_param;


// VAD启动。*param参数句柄需要在stop之后才能释放
bool ifly_vad_start(ifly_vad_param *param);
// VAD结束。
void ifly_vad_stop(u8 force_stop, u32 to_ms);

// VAD停止发送语音。
void ifly_vad_audio_stop(void);

// 判断vad是否正在运行
bool ifly_vad_is_work(void);
#endif

