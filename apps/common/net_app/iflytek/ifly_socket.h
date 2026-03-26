#ifndef __IFLY_SOCKET_H__
#define __IFLY_SOCKET_H__

#include "generic/includes.h"

typedef enum {
    IFLY_SOCKET_EVT_SEND_OK = 1,	// socket发数成功。*param: 发数buf
    IFLY_SOCKET_EVT_SEND_ERROR,		// socket发数失败。*param: 发数buf

    IFLY_SOCKET_EVT_INIT_ERROR,		// socket初始化失败。*param: ifly_socket_param*
    IFLY_SOCKET_EVT_HANSHACK_ERROR,	// socket握手失败。*param: ifly_socket_param*

    IFLY_SOCKET_EVT_INIT_OK,		// socket初始化成功。*param: ifly_socket_param*

    IFLY_SOCKET_EVT_ACCIDENT_END,			//意外结束。。*param: ifly_socket_param*

    IFLY_SOCKET_EVT_END,			// 结束。socket释放资源之前。*param: ifly_socket_param*
    IFLY_SOCKET_EVT_FORCE_END,		// 强制结束。socket释放资源之前。*param: ifly_socket_param*
    IFLY_SOCKET_EVT_EXIT,			// 结束。socket释放资源之后。*param: ifly_socket_param*
} ifly_socket_event_enum ;

typedef struct ifly_websocket_struct {
    // 参数信息，所有参数都需要赋值
    char *task_name;	// 任务名
    u8 *auth;			// 鉴权buf
    u8 socket_mode;		// 是否加密。WEBSOCKET_MODE, WEBSOCKETS_MODE
    void (*recv_cb)(u8 *buf, u32 len, u8 type);	// 收数回调
    bool (*get_send)(u8 **buf, u32 *len);		// 获取发数buf
    int (*event_cb)(ifly_socket_event_enum evt, void *param);		// 事件回调
    // 模块内部记录信息，不需要赋值
    void *socket_hdl;	// socket句柄
} ifly_socket_param;


// socket创建。*ifly_socket参数句柄需要在release之后才能释放
bool ifly_websocket_client_create(ifly_socket_param *ifly_socket);
// socket释放。
void ifly_websocket_client_release(ifly_socket_param *ifly_socket, u32 to_ms);

// 用于处理socket收发任务的删除，需要在所有socket创建之前创建，所有socket释放之后释放
void ifly_socket_kill_task_create(void);
void ifly_socket_kill_task_release(void);

#endif

