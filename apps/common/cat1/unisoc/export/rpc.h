/* Copyright (C) 2021 UNISOC Technologies Limited and/or its
 * affiliates("UNISOC"). All rights reserved.
 *
 * This software is supplied "AS IS" without any warranties.
 * UNISOC assumes no responsibility or liability for the use of the software,
 * conveys no license or title under any patent, copyright, or mask work
 * right to the product. UNISOC reserves the right to make changes in the
 * software without notification.  UNISOC also make no representation or
 * warranty that such application will be suitable for the specified use
 * without further testing or modification.
 *
 *
 * FileName     :    rpc.h
 * Author       :    wei.zhou
 * Version      :    v1.0
 * Date         :    2021/12/31
 * Description  :    RPC Fun is responsible for providing access to RPC status,
 *                   RPC channel information, event reporting, remote
 *                   invocation, setting callback functions, RPC registration
 *                   and cancellation interfaces, etc.
 * History      :    1.2021/12/31  Initial draft
 *=========================================================
 * 2021-12-31 wei.zhou create.
 */
#ifndef __RPC_FUNCTION_H__
#define __RPC_FUNCTION_H__
#include <stdint.h>
#include "uos_type.h"
#include "uos_osal.h"
#include "uos_deval.h"
//#include <stdbool.h>

/*STUB define*/
#ifdef __cplusplus
extern "C"
{
#endif

#define CHECK_IPAR_SIZE(size)
#define CHECK_OPAR_SIZE(size)

#define ALIGNED(n) __attribute__((aligned(n)))
#define ALIGNUP8(n) (((unsigned long)(n) + (8) - 1) & ~((8) - 1))

#ifdef __cplusplus
}
#endif

#define     RPC_CALL_RET_OK             0  // rpc call ok
#define     RPC_CALL_RET_INVAL_API      -1 // no function defined on the other side
#define     RPC_CALL_RET_TIMEOUT        -2 // may be block by channel or function cost too much time
#define     RPC_CALL_RET_DATA_ERROR     -3 // data error when transmission
#define     RPC_CALL_RET_NO_READY       -4 // rpc channel no ready
#define     RPC_CALL_RET_OVER_LEN       -5 // rpc channel data over len

// clang-format off
#define     EVENT_RPC_CONNECT_TAG       0xfe00    /* rpc connect event tag */
#define     EVENT_RPC_DISCON_TAG        0xfe01    /* rpc disconnect event tag */
#define     EVENT_RPC_EXITCP_TAG        0xfe02    /* rpc exit event tag */

//typedef     signed int                  ssize_t;

/*CP state*/
typedef enum {
    CP_STA_INIT = 0,            /* CP status: initial */
    CP_STA_CONNECT,             /* CP status: ap and cp is connected */
    CP_STA_DISCON,              /* CP status: ap and cp is disconnected */
    CP_STA_DAEEXIT,             /* CP status: cp is exiting */
} rpc_cp_state_e;

/*RPC return type*/
typedef enum {
    RPC_DEAL_SUCCESS = 0,       /* rpc process: success */
    RPC_DEAL_DATA_NULL,         /*rpc process: deal with data null*/
    RPC_DEAL_FAILED = -50,      /*rpc process: failed*/
    RPC_DEAL_THREAD_EXIT,       /*rpc process: thread exit*/
    RPC_ERR_NO_INIT,            /*rpc reg error: no init*/
    RPC_ERR_OVERLEN,            /*rpc reg error: data is overlen*/
    RPC_ERR_ALLOC,              /*rpc reg error: alloc data error*/
    RPC_ERR_MAP_ID,             /*rpc reg error: get channel pointer error*/
    RPC_ERR_MAP_CHN,            /*rpc reg error: get channel pointer error*/
    RPC_ERR_GET_OPS,            /*rpc reg error: get channel pointer error*/
    RPC_ERR_THREAD,             /*rpc reg error: start event thread error*/
    RPC_ERR_CONNECT,            /*rpc reg error: rpc connect error*/
    RPC_ERR_DISCONNECT,         /*rpc reg error: rpc disconnect error*/
    RPC_ERR_INVALID_PARAMETER,  /*rpc reg error: rpc parameter is invalid*/
    RPC_ERR_NO_REG,             /*rpc reg error: not reg chn*/
    RPC_ERR_EVENT_REG,          /*rpc reg error: register event error*/
    RPC_ERR_MQ,                 /*rpc reg error: rpc check mq is not create*/
    RPC_ERR_SEM,                /*rpc reg error: register sem error*/
    RPC_ERR_MUTEX,              /*rpc reg error: register mutex error*/
    RPC_ERR_IPC_OPEN,           /*rpc reg error: ipc open error*/
    RPC_ERR_LIST_EMPTY,         /*rpc reg error: register list is empty*/
    RPC_ERR_REG_DUPLICATE,      /*rpc reg error: register duplicate*/
    RPC_ERR_INVALID_API,        /*rpc call: no function defined on the other side*/
    RPC_ERR_TIMEOUT,            /*rpc call: may be block by channel or function cost too much time*/
    RPC_ERR_DATA_MOVE,          /*rpc call: data error when transmission*/
    RPC_ERR_DATA_SEND,          /*rpc call: rpc send data error*/
    RPC_ERR_CHN_NO_READY,       /*rpc call: rpc channel no ready*/
    RPC_ERR_SEQ_NOT_EQUAL,      /*rpc call: rpc packetage seq error*/
    RPC_ERR_CRC,                /*rpc call: rpc CRC code error*/
    RPC_ERR_CALLOC,            /* rpc call api: rpc calloc error, return null */
    RPC_ERR_POWEROFF,          /* rpc call api: rpc is poweroffing, stop call api */
} rpc_ret_e;

/*RPC packet header*/
typedef struct {
    uint32_t opcode;            /*operation in RPC packet*/
    uint32_t size;              /*the whole RPC packet size*/
} rpc_header_t;

/*RPC call packet header*/
typedef struct {
    rpc_header_t header;        /* common RPC packet header */
    uint32_t api_tag;           /* tag for RPC function */
    uint32_t caller_sync;       /* sync primitive of caller, usually is a semaphore */
    uint32_t caller_rsp_ptr;    /* response pointer of caller */
    uint16_t seq;               /* sequence number, just for debug */
    uint16_t rsp_size;          /* response packet size */
    uint32_t null_flag;         /* reserved */
    uint32_t check_code;        /* reserved */
} rpc_call_header_t;

/*RPC response packet header*/
typedef struct {
    rpc_header_t header;        /* common RPC packet header */
    uint32_t api_tag;           /* tag for RPC function, copied from "call", just for debug */
    uint32_t caller_sync;       /* sync primitive of caller, copied from "call" */
    uint32_t caller_rsp_ptr;    /* response pointer of caller */
    uint16_t seq;               /* sequence number, just for debug */
    uint16_t rpc_error_code;    /* 0 or ENOENT(2) */
    uint32_t reserved0;         /* reserved */
    uint32_t check_code;        /* rpc check code */
} rpc_rsp_header_t;

/*RPC event packet header
 * The 4 words after common RPC packet header is just osi_event_t.
 * It is to reduce header file dependency t expand them.
 */
typedef struct {
    rpc_header_t header;        /*common RPC packet header*/
    uint32_t event_code;        /*event id*/
    uint32_t par1;              /*1st word parameter*/
    uint32_t par2;              /*2nd word parameter*/
    uint32_t par3;              /*3rd word parameter*/
    uint32_t seq;               /*reserved*/
    uint32_t check_code;        /*reserved*/
} rpc_event_header_t;

/*RPC event content*/
typedef struct {
    uint32_t event_code;        /*event identifier*/
    uint32_t event_msg;         /*event msg*/
    uint32_t event_size;        /*event msg size*/
    uint32_t task_id;           /*event task id*/
} rpc_send_event_t;

/**
 * Function     :    rpc_channel_ready
 * Author       :    wei.zhou
 * Description  :    judge chnnel ready, yes or no
 * Input        :    channel_id         the RPC channel number
 * Output       :    bool
 * Return       :    ture               RPC channel is ready
 *                   false              RPC channel is not ready
 * History      :    1.2021/12/31  Initial draft
 *=========================================================
 * 2021-12-31 wei.zhou create.
 */
bool rpc_channel_ready(uint32_t channel_id);

/**
 * Function     :    rpc_set_evt_threadid
 * Author       :    wei.zhou
 * Description  :    set the mq recv thread
 * Input        :    channel_id         the RPC channel number
 *                   para               event para(aim thread id)
 * Output       :    int
 * Return       :    0                  set the mq recv thread success
 *                   -1                 set the mq recv thread failed
 * History      :    1.2021/12/31  Initial draft
 *=========================================================
 * 2021-12-31 wei.zhou create.
 */
int rpc_set_evt_thread_id(uint32_t channel_id, uint32_t para);

/**
 * Function     :    rpc_recv_event
 * Author       :    wei.zhou
 * Description  :    recv the event from cp side
 * Input        :    channel_id         the RPC channel number
 *                   buf                allocate from outside input
 *                   p_id[out]          message id
 *                   p_size[out]        body length
 *                   para[out]          message pointer
 * Output       :    int
 * Return       :    RPC_DEAL_SUCCESS   recv success
 *                   RPC_ERR_MAP_CHN    id map channel failed
 *                   RPC_ERR_OVERLEN receive msg is overlen
 * History      :    1.2021/12/31  Initial draft
 *=========================================================
 * 2021-12-31 wei.zhou create.
 */
int rpc_recv_event(uint32_t channel_id, void *buf, uint32_t *id, uint32_t *size,
                   uint32_t *para);

/**
 * Function     :    rpc_call
 * Author       :    wei.zhou
 * Description  :    RPC packets are 8 bytes aligned. When the size in input
 *                   header is not 8 bytes aligned, it will be changed to 8
 *                   bytes aligned. That is, event->header.size may be changed
 *                   inside. Most likely, it won't be called directly by
 *                   application. It will be called only in RPC daemon and
 *                   RPC stubs.
 * Input        :    channel_id                 the RPC channel id
 *                   call                       the constructed call header
 * Output       :    int
 * Return       :    RPC_DEAL_SUCCESS           call api successfully
 *                   RPC_ERR_INVALID_PARAMETER  invalid parameter
 *                   RPC_ERR_MAP_CHN            id map chn failed
 *                   RPC_ERR_GET_OPS            no chn pointer or no driver
 *                   RPC_ERR_CHN_NO_READY       rpc channel no ready
 *                   RPC_ERR_OVERLEN            read data is overlen
 *                   RPC_ERR_DATA_SEND          send data error
 *                   RPC_ERR_TIMEOUT            call api timeout
 *                   RPC_ERR_INVALID_API        call none api
 *                   RPC_ERR_SEQ_NOT_EQUAL      seq is not equal or call api
 *                                              is over
 * History      :    1.2021/12/31  Initial draft
 *=========================================================
 * 2021-12-31 wei.zhou create.
 */
int rpc_call(uint32_t channel_id, rpc_call_header_t *call);

/**
 * Function     :    rpc_call_ex
 * Author       :    jie.wang3
 * Description  :    RPC packets are 8 bytes aligned. When the size in input
 *                   header is not 8 bytes aligned, it will be changed to 8
 *                   bytes aligned. That is, event->header.size may be changed
 *                   inside. Most likely, it won't be called directly by
 *                   application. It will be called only in RPC daemon and
 *                   RPC stubs.
 * Input        :    channel_id                 the RPC channel id
 *                   call                       the constructed call header
 * Output       :    int
 * Return       :    RPC_DEAL_SUCCESS           call api successfully
 *                   RPC_ERR_INVALID_PARAMETER  invalid parameter
 *                   RPC_ERR_MAP_CHN            id map chn failed
 *                   RPC_ERR_GET_OPS            no chn pointer or no driver
 *                   RPC_ERR_CHN_NO_READY       rpc channel no ready
 *                   RPC_ERR_OVERLEN            read data is overlen
 *                   RPC_ERR_DATA_SEND          send data error
 *                   RPC_ERR_TIMEOUT            call api timeout
 *                   RPC_ERR_INVALID_API        call none api
 *                   RPC_ERR_SEQ_NOT_EQUAL      seq is not equal or call api
 *                                              is over
 * History      :    1.2023/3/17  Initial draft
 *=========================================================
 * 2023-3-17 jie.wang3 create.
 */
int rpc_call_ex(uint32_t channel_id, rpc_call_header_t *call);

/**
 * Function     :    rpc_init
 * Author       :    wei.zhou
 * Description  :    initial RPC module
 * Input        :    void
 * Output       :    int
 * Return       :    RPC_DEAL_SUCCESS   initial successfully
 *                   other              initial failed
 * History      :    1.2021/12/31  Initial draft
 *=========================================================
 * 2021-12-31 wei.zhou create.
 */
int urpc_init(void);

/**
 * Function     :    rpc_set_comm_handle
 * Author       :    wei.zhou
 * Description  :    set chn handle.
 * Input        :    channel_id         the RPC channel number
 *                   handle             handle
 * Output       :    int
 * Return       :    RPC_DEAL_SUCCESS   set handle successfully
 *                   RPC_ERR_MAP_CHN    id map channel failed
 * History      :    1.2021/12/31  Initial draft
 *=========================================================
 * 2021-12-31 wei.zhou create.
 */
int rpc_set_comm_handle(uint32_t channel_id, uint32_t handle);

/**
 * Function     :    rpc_get_comm_handle
 * Author       :    wei.zhou
 * Description  :    get chn handle.
 * Input        :    channel_id         the RPC channel number
 *              :    chn_handle         the RPC channel describe handle
 * Output       :    int
 * Return       :    RPC_DEAL_SUCCESS   get describe handle successfully
 *                   others             get describe handle failed
 * History      :    1.2021/12/31  Initial draft
 *=========================================================
 * 2021-12-31 wei.zhou create.
 */
int rpc_get_comm_handle(uint32_t channel_id, uint32_t *chn_handle);

/**
 * Function     :    rpc_set_chn_status
 * Author       :    wei.zhou
 * Description  :    set channel status.
 * Input        :    chn_statue         channel status
 * Output       :    void
 * Return       :    void
 * History      :    1.2021/12/31  Initial draft
 *=========================================================
 * 2021-12-31 wei.zhou create.
 */
void rpc_set_chn_status(bool chn_statue);

/**
 * Function     :    rpc_get_chn_status
 * Author       :    wei.zhou
 * Description  :    get channel status.
 * Input        :    void
 * Output       :    bool
 * Return       :    true               channel status is normal
 *                   false              channel status is abnormal
 * History      :    1.2021/12/31  Initial draft
 *=========================================================
 * 2021-12-31 wei.zhou create.
 */
bool rpc_get_chn_status(void);

/**
 * Function     :    rpc_set_chn_size
 * Author       :    wei.zhou
 * Description  :    set chn size.
 * Input        :    channel_id         the RPC channel number
 *                   len                channel size
 * Output       :    int
 * Return       :    RPC_DEAL_SUCCESS   set channel size successfully
 *                   RPC_ERR_MAP_CHN    id map channel failed
 *                   RPC_ERR_OVERLEN size is overlen
 * History      :    1.2021/12/31  Initial draft
 *=========================================================
 * 2021-12-31 wei.zhou create.
 */
int rpc_set_chn_size(uint32_t channel_id, uint32_t len);

/**
 * Function     :    rpc_connect_peer
 * Author       :    wei.zhou
 * Description  :    connect process
 * Input        :    channel_id         the RPC channel number
 * Output       :    int
 * Return       :    RPC_DEAL_SUCCESS   connect successfully
 *                   other              connect failed
 * History      :    1.2021/12/31  Initial draft
 *=========================================================
 * 2021-12-31 wei.zhou create.
 */
int rpc_connect_peer(uint32_t channel_id);

/**
 * Function     :    rpc_discon_peer
 * Author       :    wei.zhou
 * Description  :    disconnect process
 * Input        :    channel_id         the RPC channel number
 * Output       :    int
 * Return       :    RPC_DEAL_SUCCESS   disconnect successfully
 *                   other              disconnect failed
 * History      :    1.2021/12/31  Initial draft
 *=========================================================
 * 2021-12-31 wei.zhou create.
 */
int rpc_discon_peer(uint32_t channel_id);

/**
 * Function     :    rpc_release_chn
 * Author       :    wei.zhou
 * Description  :    release the chn resource.
 * Input        :    channel_id         the RPC channel number
 * Output       :    int
 * Return       :    RPC_DEAL_SUCCESS   release resource successfully
 *                   other              release resource failed
 * History      :    1.2021/12/31  Initial draft
 *=========================================================
 * 2021-12-31 wei.zhou create.
 */
int rpc_release_chn(uint32_t channel_id);

/**
 * Function     :    rpc_power_on
 * Author       :    wei.zhou
 * Description  :    rpc module power on.
 * Input        :    void
 * Output       :    int
 * Return       :    RPC_DEAL_SUCCESS   power on successfully
 *                   other              power on failed
 * History      :    1.2021/12/31  Initial draft
 *=========================================================
 * 2021-12-31 wei.zhou create.
 */
int rpc_power_on(void);

/**
 * Function     :    rpc_power_off
 * Author       :    wei.zhou
 * Description  :    rpc module power off.
 * Input        :    void
 * Output       :    int
 * Return       :    RPC_DEAL_SUCCESS   power off successfully
 *                   other              power off failed
 * History      :    1.2021/12/31  Initial draft
 *=========================================================
 * 2021-12-31 wei.zhou create.
 */
int rpc_power_off(void);

/* tele callback id */
typedef enum {
    RPC_TELE_CB_INIT = 0,  //used for modem tele init ready
    RPC_TELE_CB_CALL,
    RPC_TELE_CB_DATA,
    RPC_TELE_CB_SIM,
    RPC_TELE_CB_NETWORK,
    RPC_TELE_CB_MAX
} RPC_TELE_CB_ID_E;

/* callback func pointer */
typedef uint32_t (*RPC_TELE_CB_T)(uint32_t rpc_status);

/* callback handler */
typedef struct {
    RPC_TELE_CB_ID_E callback_id;
    RPC_TELE_CB_T callback;
} RPC_TELE_CB_INFO_T;

/**
 * Function     :    rpc_tele_client_reg_handler
 * Author       :    jie.wang3
 * Description  :    set tele register callback
 * Input        :    callback_id         callback function id
 *                   callback                callback function pointer
 * Output       :    int
 * Return       :    RPC_DEAL_SUCCESS           set callback successfully
 * History      :    1.2022/11/4  Initial draft
 *=========================================================
 * 2022-11-4 jie.wang3 create.
 */
int rpc_tele_reg_callback(RPC_TELE_CB_ID_E callback_id, RPC_TELE_CB_T callback);

/**
 * Function     :    rpc_tele_client_reg_handler
 * Author       :    jie.wang3
 * Description  :    excute tele register
 * Input        :    rpc_status rpc status
 * Output       :    void
 * Return       :    void
 * History      :    1.2022/11/4  Initial draft
 *=========================================================
 * 2022-11-4 jie.wang3 create.
 */
void rpc_tele_register(uint32_t rpc_status);

/**
 * Function     :    rpc_get_modem_abnormal_status
 * Author       :    qin.zhou
 * Description  :    get modem abnomal status
 * Input        :    void
 * Output       :    void
 * Return       :    bool
 * History      :    1.2023/1/16  Initial draft
 *=========================================================
 * 2023-1-14 qin.zhou create.
 */
bool rpc_get_modem_abnormal_status(void);

/**
 * Function     :    rpc_mdm_vote
 * Author       :    jie.wang3
 * Description  :   modem power on rpc connect success vote
 * Input        :    void
 * Output       :    void
 * Return       :    bool
 * History      :    1.2023/3/8  Initial draft
 *=========================================================
 * 2023-3-8 jie.wang3 create.
 */
bool rpc_mdm_vote(void);

int MN_RpcRecvEvent(void *buf, unsigned short *p_id, uint32_t *p_size, uint32_t *para);

/**
 * Function     :    rpc_set_poweroff_status
 * Author       :    jie.wang3
 * Description  :    rpc module set  poweroff status.
 * Input        :    bool
 * Output       :    void
 * Return       :    void
 * History      :
 *=========================================================
 * 2022-10-27 jie.wang3 create.
 */
void rpc_set_poweroff_status(bool is_poweroff);
void rpc_send_poweroff_evt_to_ap(void);
#endif
