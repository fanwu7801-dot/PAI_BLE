#ifndef __SPARKDESK_MAIN_H
#define __SPARKDESK_MAIN_H

#include "generic/includes.h"

#define MAX_SPARKDESK_LEN   2000

typedef enum {
    IFLY_AI_EVT_RECV_OK = 1,	// AI对话完成。*param: ifly_ai_param*
    IFLY_AI_EVT_NETWORK_FAIL,			// 结束。*param: ifly_ai_param*
    IFLY_AI_EVT_EXIT,			// 结束。*param: ifly_ai_param*
} ifly_ai_event_enum ;

typedef struct ifly_ai_struct {
    // 参数信息，所有参数都需要赋值
    char *content;		// 输入对话数据
    char *ai_res;		// 输出数据
    u32 ai_res_len;		// 输出数据buf长度，最大MAX_SPARKDESK_LEN
    int (*event_cb)(ifly_ai_event_enum evt, void *param);		// 事件回调
} ifly_ai_param;


// AI对话启动。*param参数句柄需要在stop之后才能释放
bool ifly_sparkdesk_start(ifly_ai_param *param);
// AI对话结束。
void ifly_sparkdesk_stop(u8 force_stop, u32 to_ms);
//判断是否正在运行
bool ifly_sparkdesk_is_work(void);


#endif
