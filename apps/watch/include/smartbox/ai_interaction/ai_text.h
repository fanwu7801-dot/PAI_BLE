
#ifndef __AI_TEXT_H__
#define __AI_TEXT_H__

#include "typedef.h"
#include "app_config.h"

// 初始化
// rx_text()返回true代表外面已经处理，内部不再保存数据
int ai_text_init(int (* rx_text)(u8 ai_text, u8 *data, int len));

// 释放
void ai_text_release(void);

// 启动APP文本转语音
int ai_text_local_to_tts(void);
int ai_text_ai_to_tts(void);

// 临界区保护
void ai_text_enter_critical(void);
void ai_text_exit_critical(void);

// 获取文本，*buf为文本数据，返回值为文本长度。
// 获取文件数据接口及文本数据的使用应该在临界区保护范围内
int ai_text_get_local_text(u8 **buf);
int ai_text_get_ai_text(u8 **buf);

#endif//__AI_TEXT_H__

