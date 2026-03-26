#include "websocket_api.h"
#include "os/os_compat.h"
#ifdef WEBSOCKET_MEMHEAP_DEBUG
#include "mem_leak_test.h"
#endif
#include "ifly_socket.h"
#include "ui/ui_api.h"
#include "cJSON.h"
#include "app_config.h"

#if TCFG_IFLYTEK_ENABLE


#define LOG_TAG_CONST       NET_IFLY
#define LOG_TAG             "[IFLY_SOCKET]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
#define LOG_CLI_ENABLE
#include "debug.h"

#define IFLY_SOCKET_KILL_TASK_NAME        "ifly_socket_kill"

struct ifly_socket_info {
    struct websocket_struct socket_info;
    u8 force_stop; // 强制结束
};

extern int task_kill(const char *name);
extern void websockets_sleep(unsigned int ms);

static void task_kill_callback(char *buf)
{
    log_info("[msg]>>>>>>>>>>>*buf=%s", buf);
    task_kill(buf);
}

static void ifly_socket_kill_task_main(void *priv)
{
    int msg[16];
    while (1) {
        os_taskq_pend(NULL, msg, ARRAY_SIZE(msg));
    }
}

void ifly_socket_kill_task_create(void)
{
    os_task_create(ifly_socket_kill_task_main,
                   NULL,
                   5,
                   256,
                   256,
                   IFLY_SOCKET_KILL_TASK_NAME);
}

void ifly_socket_kill_task_release(void)
{
    if (!strcmp(os_current_task(), IFLY_SOCKET_KILL_TASK_NAME)) {
        task_kill(IFLY_SOCKET_KILL_TASK_NAME);
        return ;
    }
    int msg[4];
    msg[0] = (int)task_kill_callback;
    msg[1] = 1;
    msg[2] = (int)IFLY_SOCKET_KILL_TASK_NAME;
    os_taskq_post_type("app_core", Q_CALLBACK, 3, msg);
}


static void websockets_client_reg(struct websocket_struct *websockets_info, ifly_socket_param *ifly_socket)
{
    memset(websockets_info, 0, sizeof(struct websocket_struct));
    websockets_info->_init           = websockets_client_socket_init;
    websockets_info->_exit           = websockets_client_socket_exit;
    websockets_info->_handshack      = webcockets_client_socket_handshack;
    websockets_info->_send           = websockets_client_socket_send;
    websockets_info->_recv_thread    = websockets_client_socket_recv_thread;
    websockets_info->_heart_thread   = websockets_client_socket_heart_thread;
    websockets_info->_recv_cb        = ifly_socket->recv_cb;
    websockets_info->_recv           = NULL;

    websockets_info->websocket_mode  = ifly_socket->socket_mode;
    websockets_info->kill_task       = IFLY_SOCKET_KILL_TASK_NAME;
}


static int websockets_client_init(struct websocket_struct *websockets_info, u8 *url, const char *origin_str, const char *user_agent_str)
{
    websockets_info->ip_or_url = url;
    websockets_info->origin_str = origin_str;
    websockets_info->user_agent_str = user_agent_str;
    websockets_info->recv_time_out = 1000;

    //应用层和库的版本检测，结构体不一样则返回出错
    int err = websockets_struct_check(sizeof(struct websocket_struct));
    if (err == FALSE) {
        return err;
    }
    return websockets_info->_init(websockets_info);
}

static int websockets_client_handshack(struct websocket_struct *websockets_info)
{
    return websockets_info->_handshack(websockets_info);
}

static int websockets_client_send(struct websocket_struct *websockets_info, u8 *buf, int len, char type)
{
    //SSL加密时一次发送数据不能超过16K，用户需要自己分包
    return websockets_info->_send(websockets_info, buf, len, type);
}

static void websockets_client_exit(struct websocket_struct *websockets_info)
{
    websockets_info->_exit(websockets_info);
}

static void ifly_socket_task_main(void *priv)
{
    int err;
    ifly_socket_event_enum ifly_evt = IFLY_SOCKET_EVT_END;
    ifly_socket_param *ifly_socket = (ifly_socket_param *)priv;
    struct ifly_socket_info *info = (struct ifly_socket_info *)ifly_socket->socket_hdl;
    const char *origin_str = "http://coolaf.com";

    log_info(" . ----------------- Client Websocket ------------------\r\n");

    /* 0 . malloc buffer */
    struct websocket_struct *websockets_info = &info->socket_info;
    /* 1 . register */
    websockets_client_reg(websockets_info, ifly_socket);

    /* 2 . init */
    err = websockets_client_init(websockets_info, ifly_socket->auth, origin_str, NULL);
    if (FALSE == err) {
        log_error("  . ! Cilent websocket init error !!!\r\n");
        ifly_evt = IFLY_SOCKET_EVT_INIT_ERROR;
        goto exit_ws;
    }

    /* 3 . hanshack */
    err = websockets_client_handshack(websockets_info);
    if (FALSE == err) {
        log_error("  . ! Handshake error !!!\r\n");
        ifly_evt = IFLY_SOCKET_EVT_HANSHACK_ERROR;
        goto exit_ws;
    }
    log_info(" . Handshake success \r\n");

    /* 4 . CreateThread */
    err = os_task_create(websockets_info->_heart_thread,
                         websockets_info,
                         3,
                         1024,
                         0,
                         "websocket_client_heart");
    if (err == 0) {
        websockets_info->ping_thread_id = 1;
    } else {
        websockets_info->ping_thread_id = 0;
    }
    err = os_task_create(websockets_info->_recv_thread,
                         websockets_info,
                         2,
                         1024,
                         0,
                         "websocket_client_recv");
    if (err == 0) {
        websockets_info->recv_thread_id = 1;
    } else {
        websockets_info->recv_thread_id = 0;
    }

    ifly_socket->event_cb(IFLY_SOCKET_EVT_INIT_OK, ifly_socket);

    /* 5 . recv or send data */
    while (1) {
        u8 *send_buf = NULL;
        u32 send_len = 0;
        if (websockets_info->websocket_valid == INVALID_ESTABLISHED) {
            //websocket底层断开连接
            ifly_evt = IFLY_SOCKET_EVT_ACCIDENT_END;
            goto exit_ws;
        }
        if (info->force_stop) {
            ifly_evt = IFLY_SOCKET_EVT_FORCE_END;
            goto exit_ws;
        }
        err = ifly_socket->get_send(&send_buf, &send_len);
        if (err == false) {
            // 获取不到数据，正常退出
            goto exit_ws;
        }
        if ((send_buf == NULL) || (send_len == 0)) {
            continue;
        }
        err = websockets_client_send(websockets_info, send_buf, send_len, WCT_TXTDATA);
        if (FALSE == err) {
            log_error("  . ! send err !!!\r\n");
            ifly_socket->event_cb(IFLY_SOCKET_EVT_SEND_ERROR, send_buf);
            goto exit_ws;
        }
        ifly_socket->event_cb(IFLY_SOCKET_EVT_SEND_OK, send_buf);
    }

exit_ws:
    /* 6 . exit */
    ifly_socket->event_cb(ifly_evt, ifly_socket);

    if (websockets_info->ping_thread_id) {
        websockets_info->ping_kill_flag = 1;
    }
    if (websockets_info->recv_thread_id) {
        websockets_info->recv_kill_flag = 1;
    }

    while (websockets_info->recv_kill_flag || websockets_info->ping_kill_flag) {
        os_time_dly(1);
    }
    websockets_client_exit(websockets_info);

    ifly_socket->event_cb(IFLY_SOCKET_EVT_EXIT, ifly_socket);

    free(ifly_socket->socket_hdl);
    ifly_socket->socket_hdl = NULL;

    int msg[5];
    msg[0] = (int)task_kill_callback;
    msg[1] = 1;
    msg[2] = (int)os_current_task();
    err = os_taskq_post_type("app_core", Q_CALLBACK, 3, msg);

    os_time_dly(-1);
}

bool ifly_websocket_client_create(ifly_socket_param *ifly_socket)
{
    ifly_socket->socket_hdl = zalloc(sizeof(struct ifly_socket_info));
    if (ifly_socket->socket_hdl == NULL) {
        return false;
    }

    os_task_create(ifly_socket_task_main,
                   ifly_socket,
                   3,
                   2048,
                   0,
                   ifly_socket->task_name);

    return true;
}

void ifly_websocket_client_release(ifly_socket_param *ifly_socket, u32 to_ms)
{
    if (ifly_socket && ifly_socket->socket_hdl) {
        struct ifly_socket_info *info = (struct ifly_socket_info *)ifly_socket->socket_hdl;
        info->force_stop = 1;

        if (!strcmp(os_current_task(), ifly_socket->task_name)) {
            // 任务内部调用
            return ;
        }
        // 其他任务调用
        while (ifly_socket->socket_hdl) {
            os_time_dly(1);
            if (to_ms <= 10) {
                break;
            }
            to_ms -= 10;
        }
    }
}

#endif

