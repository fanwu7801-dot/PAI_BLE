
#ifndef __MASS_DATA_H__
#define __MASS_DATA_H__

#include "typedef.h"
#include "app_config.h"

#define MASS_DATA_TYPE_ORIGINAL			0 // 原始数据
#define MASS_DATA_TYPE_ALIYUN			1 // 阿里云
#define MASS_DATA_TYPE_RCSP_DATA		2 // RCSP
#define MASS_DATA_TYPE_AI_TEXT			3 // AI
#define MASS_DATA_TYPE_TTS				4 // TTS
#define MASS_DATA_TYPE_VERIFY			5 // 平台接口认证信息
#define MASS_DATA_TYPE_ESIM				6 // esim卡
#define MASS_DATA_TYPE_CAT1_UPDATE		7 // 4G模块升级数据
#define MASS_DATA_TYPE_MAX				8

#define MASS_DATA_SEND_EVENT_OK			0 // OK
#define MASS_DATA_SEND_EVENT_TO			1 // 超时
#define MASS_DATA_SEND_EVENT_ERR		2 // 发送失败
#define MASS_DATA_SEND_EVENT_RESPOND	3 // 回复出错

#define MASS_DATA_RECV_EVENT_OK			0 // 完成
#define MASS_DATA_RECV_EVENT_ERROR		1 // 出错
#define MASS_DATA_RECV_EVENT_TO			2 // 超时

struct _MASSDATA_RECV_CB {
    u8 part_cb;	// 收到一部分数据后就返回，不等全部收完
    void (*cb)(void *priv, u8 *data, int len);
    void *cb_priv;
    void (*evt_cb)(void *priv, int event, void *evt_param);
};

// 注册收数回调
void mass_data_recv_register(u8 type, void *cb_priv, void (*cb)(void *priv, u8 *data, int len));
void mass_data_recv_register_ext(u8 type, struct _MASSDATA_RECV_CB *recv_cb);
// 非阻塞式发送
int mass_data_asyn_send(u8 type, u8 *data, u16 len, void *evt_cb_priv, void (*evt_cb)(void *priv, int event));
// 非阻塞式发送。数据与头部分开填数
int mass_data_asyn_send_ex(u8 type, u8 *head, u16 head_len, u8 *data, u16 len, void *evt_cb_priv, void (*evt_cb)(void *priv, int event));

// 阻塞式发送 // 不能在app_core和蓝牙相关任务中调用
int mass_data_blocking_send(u8 type, u8 *data, u16 len);

#endif//__MASS_DATA_H__

