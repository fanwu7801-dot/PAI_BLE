#ifndef __SMARTBOX_COMMON_INFO_H__
#define __SMARTBOX_COMMON_INFO_H__

#include "typedef.h"
#include "app_config.h"

#define SMARTBOX_COMMON_INFO_CAT1_MODULE		(0x0005) // 4G模块信息
#define SMARTBOX_COMMON_INFO_WATCH_DIAL_EXT		(0x0007) // 表盘扩展信息

typedef struct {
    u16 function;
    u8 version;
    u8 op;
    u8 data[0];
} smartbox_common_info_set_cmd_t;

extern u8 smartbox_common_info_set_cmd_deal(void *priv, u8 OpCode, u8 OpCode_SN, u8 *data, u16 len);

#endif//__SMARTBOX_COMMON_INFO_H__

