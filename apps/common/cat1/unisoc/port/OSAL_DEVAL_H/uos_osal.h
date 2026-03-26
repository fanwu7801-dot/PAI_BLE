/********************************************************************************
** Copyright <2022>[-<2023>]                                                    *
** Licensed under the Unisoc General Software License                           *
** version 1.0 (the License);                                                   *
** you may not use this file except in compliance with the License              *
** You may obtain a copy of the License at                                      *
** https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US       *
** Software distributed under the License is distributed on an "AS IS" BASIS,   *
** WITHOUT WARRANTIES OF ANY KIND, either express or implied.                   *
** See the Unisoc General Software License, version 1.0 for more details.       *
********************************************************************************/
/********************************************************************************
** File Name:       uos_osal.h
** Author: Qiang He
** Copyright (C) 2002 Unisoc Communications Inc.
** Description:
********************************************************************************/
#ifndef _UOS_OSAL_H_
#define _UOS_OSAL_H_

#include "uos_type.h"
#include <sys/types.h>
/* #include <sys/ioctl.h> */
/* #include <sys/stat.h> */
/* #include <sys/eventfd.h> */
/* #include <unistd.h> */
#include <stdint.h>
//#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

/* #include <pthread.h> */
#include <time.h>
/* #include <semaphore.h> */
/* #include <mqueue.h> */
/* #include <poll.h> */
#include "mqueue.h"
#include "task.h"


#ifdef  __cplusplus
extern "C"
{
#endif

/*#define UOS_VENDOR_TRACE_ON_C */
/*#define UOS_VENDOR_TRACE_ON_CPP */

#ifdef UOS_VENDOR_TRACE_ON_C
#include <cutils/trace.h>
#define UOS_C_TRACE_BEGIN    ATRACE_BEGIN
#define UOS_C_TRACE_END      ATRACE_END
#endif

#ifdef UOS_VENDOR_TRACE_ON_CPP
#include <utils/Trace.h>
#define UOS_CPP_TRACE    ATRACE_CALL
#endif

//#define PRIORITY_PROPORTION_TO_NUMBER

#ifdef CONFIG_ARCH_SIM
#define CMUX_USE_UART
#define CMUX_USE_SYNC_UART
#endif
#define UOS_WAIT_FOREVER           0    /* UCOS-II */
#define UOS_THREAD_PRIORITY_MAX    255

enum auto_config {
    AUTO_START_DISABLE = 0x1001,
    AUTO_START_ENABLE = 0x1002
};

enum debug_point_info {
    //TELE模块
    TELE_SRV_INIT_ENTRY = 0,                      //Tele接收rpc消息，task的启动时间
    //CMUX模块
    CMUX_INIT_ENTRY = 1,                            //cmux开始初始化
    CMUX_HANDSHAKE_SUCCESS = 2,             //cmux握手成功
    //RPC模块
    RPC_INIT_ENTRY = 3,                               //rpc开始初始化
    RPC_HANDSHAKE_ENTRY = 4,                   //rpc开始握手
    RPC_HANDSHAKE_SUCCESS = 5,                //rpc握手成功
    //CIT模块
    CIT_LINK_SEND = 6,                                  //获取当前数据包的发送时间
    CIT_TX_CALL_BACK = 7,                            //获取发送完数据的当前时间
    CIT_REMOTE_REQUEST_ISR = 8,               //接收中断打点
    CIT_RX_CALL_BACK = 9,                            //获取读完数据的打点计时
    CIT_PARSE_PKG_AND_VERIFY = 10,            //接收到数据后打点计时
    CIT_SPI_RX_LND_TASK_ENTRY = 11,        //上报数据后打点计时
    //MODEM_CTRL模块
    MODEM_CTRL_ENTRY = 12,                       //获取modem组件最早加载时间
    MODEM_MONITOR_THREAD = 13,              //获取modem状态变化时间
    CLIENT_MONITOR_THREAD = 14,              //获取client发送状态时间
    MODEM_SEND_MSG = 15,                           //获取上层发送给client task时间
    MODEM_TASK_REC_ASSERT = 16,              //获取到modem assert 状态时间
    MODEM_TASK_REC_ALIVE = 17,                 //获取到modem alive状态时间
    MODEM_WRITE_DATA_ENTRY = 18,           //获取执行状态变化相关组件开始时间
    MODEM_WRITE_DATA_EXPORT = 19,         //获取执行状态变化相关组件结束时间
    CLIENT_TASK_REC_POWER_ON = 20,         //获取上电开始时间
    MODEM_POWER_ON_END = 21,                  //获取上电结束时间
    CLIENT_TASK_REC_POWER_OFF = 22,        //获取下电开始时间
    MODEM_POWER_OFF_END = 23,                 //获取下电结束时间
    CLIENT_TASK_REC_MODEM_BLOCK = 24,   //获取处理block开始时间
    //NVSYNC模块
    NVSYNC_INIT_ENTRY = 25,                        //NV Task启动时间
    NVSYNC_HANDLE_REQ_HEAD = 26,             //一次完整同步的起始时刻
    NVSYNC_HANDLE_REQ_TAIL = 27,              //一次完整同步的结束时刻
    NVSYNC_COPY_FILE_ENTRY = 28,              //涉及写flash，获取写文件开始时间
    NVSYNC_COPY_FILE_EXPORT = 29,            //涉及写flash，获取写文件结束时间
    NVSYNC_WRITE_DATA_ENTRY = 30,          //写64KB数据入口，获取更新g_diskbuf的时间
    NVSYNC_WRITE_DATA_EXPORT = 31,        //写64KB数据出口，获取更新g_diskbuf的时间
    NVSYNC_CHECK_NV_ENTRY = 32,               //获取checkNV开始时间
    NVSYNC_CHECK_NV_EXPORT = 33,             //获取checkNV结束时间
    //Dload模块
    DLOAD_MODEM_EN = 34,                            //mcu已经开始给modem pmic供电
    DLOAD_FDL1_CHECK_BAUD_RATE = 35,      //mcu与modem握手成功，modem进入uart下载模式
    DLOAD_FDL1_DL_BIN = 36,                         //mcu开始通过uart下载fdl1阶段bin文件
    DLOAD_FDL1_DL_BIN_END = 37,                 //fdl1阶段bin文件下载完毕，modem跳转到fdl1运行
    DLOAD_FDL2_START = 38,                          //mcu开始通过spi下载fdl2阶段bin文件
    DLOAD_FDL2_END = 39,                              //fdl2阶段bin文件下载完毕，modem跳转到kernel运行
    DLOAD_MODEM_DELOADER = 40,                //mcu主动终止dload流程，dload流程提前结束
    DLOAD_MD_CHECK_ALIVE = 41                   //mcu开始等待modem kernel阶段初始化完毕，表示dload结束
};

typedef struct TEST_POINT_TAG {
    void *pointer;
    void *return_addr;
    uint32 time;
} TEST_POINT_T;

typedef void (*uos_thread_entry_t)(void *argument);

typedef struct uos_thread_attr_struct {
    /* common */
    void *stack_addr;
    uos_uint32_t stack_size;
    uos_int32_t priority;
    uos_uint32_t time_slice;
    uos_int32_t sched_policy;
    /* extended */
    uos_thread_id_t tid;
    const char *thread_name;
    const char *queue_name;
    uos_uint32_t queue_num;
    uos_uint32_t preempt;
    uos_uint32_t option;
} uos_thread_attr_t;

typedef struct uos_thread_struct {
    pthread_t thread_id;
    uos_uint32_t thread;
} uos_thread_t;

uos_thread_t *uos_thread_create(uos_thread_entry_t entry, void *entry_param, uos_thread_attr_t *attr);
uos_err_t uos_thread_destroy(uos_thread_t *thread);
uos_err_t uos_thread_suspend(uos_thread_t *thread);
uos_err_t uos_thread_resume(uos_thread_t *thread);
uos_err_t uos_thread_sleep(uos_uint32_t ticks);
uos_err_t uos_thread_yield(void);
uos_err_t uos_thread_set_priority(uos_thread_t *thread, uos_int32_t priority, uos_int32_t *org_priority);
uos_err_t uos_thread_get_priority(uos_thread_t *thread, uos_int32_t *priority);
uos_thread_t *uos_thread_get_self(void);
uos_uint32_t uos_thread_get_self_id(void);
uos_err_t uos_thread_get_name(uos_thread_t *thread, char *buf, int buf_size);
uos_uint32_t uos_thread_get_stack_size(uos_thread_t *thread);
uos_uint32_t uos_thread_get_used_stack_size(uos_thread_t *thread);
uos_err_t uos_thread_get_time_slice(uos_thread_t *thread, uos_uint32_t *time_slice);
uos_err_t uos_thread_set_time_slice(uos_thread_t *thread, uos_uint32_t time_slice);
uos_uint64_t uos_tick_get(void);
uos_err_t uos_thread_start(uos_thread_t *thread);

typedef enum {
    UOS_IPC_FLAG_FIFO = 0x01,
    UOS_IPC_FLAG_PRIO = 0x02
} uos_ipc_flag_t;

/* semaphore manage */
typedef struct uos_sem_attr_struct {
    /* extended */
    const char *name;
    uos_ipc_flag_t options;
    uos_uint32_t sem_id;
} uos_sem_attr_t;

typedef struct uos_sem_struct {
    sem_t sem_cb;
} uos_sem_t;


uos_sem_t *uos_sem_create(uos_uint32_t initial_val, uos_uint32_t max_count, uos_sem_attr_t *attr);
uos_err_t uos_sem_set(uos_sem_t *sem, uos_uint16_t cnt);
uos_err_t uos_sem_wait(uos_sem_t *sem,  uos_uint32_t ticks);
uos_err_t uos_sem_try_wait(uos_sem_t *sem);
uos_err_t uos_sem_destroy(uos_sem_t *sem);
uos_err_t uos_sem_post(uos_sem_t *sem);
uos_err_t uos_sem_get_value(uos_sem_t *sem, uos_int32_t *value);
/* mutex manage */
typedef struct uos_mutex_attr_struct {
    /* common */
    uos_bool_t inherit;
    /* extended */
    const char *name;
    uos_int32_t options;
    uos_uint32_t mux_id;
} uos_mutex_attr_t;

typedef struct uos_mutex_struct {
    pthread_mutex_t mutex_cb;
} uos_mutex_t;

uos_mutex_t *uos_mutex_create(uos_mutex_attr_t *attr);
uos_err_t uos_mutex_lock(uos_mutex_t *mutex, uos_uint32_t ticks);
uos_err_t uos_mutex_try_lock(uos_mutex_t *mutex);
uos_err_t uos_mutex_unlock(uos_mutex_t *mutex);
uos_err_t uos_mutex_destroy(uos_mutex_t *mutex);
/********************************************************************************************/
/* event group */
typedef struct uos_event_group_attr_struct {
    const char *name;
    uos_int32_t flag;
    //uos_uint32_t event_id;
} uos_event_group_attr_t;

typedef struct uos_event_group_struct {
    pthread_mutex_t event_mutex;
} uos_event_group_t;

/* event group flags options */
typedef enum {
    UOS_EVENT_WAIT_ANY = 0x01,     // Wait for any flag (default).
    UOS_EVENT_WAIT_ALL = 0x02,     // Wait for all flags.
    UOS_EVENT_WAIT_NO_CLEAR = 0x04, // Do not clear flags which have been specified to wait for.
    UOS_EVENT_WAIT_CLEAR = 0x08    // Clear flags which have been specified to wait for
} uos_event_op_t;
#define UOS_EVENT_WAIT_INFINITE    (-1)
#define UOS_EVENT_WAIT_IMMEDIATELY (0)

uos_event_group_t *uos_event_group_create(uos_event_group_attr_t *attr);
uos_err_t uos_event_group_destroy(uos_event_group_t *event_group);
uos_err_t uos_event_group_set(uos_event_group_t *event_group,  uos_uint32_t bits);
uos_err_t uos_event_group_wait(uos_event_group_t *event_group, uos_uint32_t bits, uos_uint32_t op, uos_uint32_t *actual_bits, uos_uint32_t ticks);
uos_err_t uos_event_group_try_wait(uos_event_group_t *event_group, uos_uint32_t bits, uos_uint32_t op,  uos_uint32_t *actual_bits);

/********************************************************************************************/

/* message queue manage */
typedef struct uos_queue_attr_struct {
    uos_uint8_t *mem;
    uos_uint32_t mem_size;

    uos_uint32_t msg_size;
    uos_uint32_t msg_cnt;
    uos_uint8_t flags;
    const char *name;
    uos_uint32_t qid;
} uos_queue_attr_t;

typedef struct uos_queue_struct {
    mqd_t queue_cb;
} uos_queue_t;

uos_queue_t *uos_queue_create(uos_queue_attr_t *attr);
uos_err_t uos_queue_destroy(uos_queue_t *queue);
uos_err_t uos_queue_send(uos_queue_t *queue, const void *msg, uos_uint32_t size, uos_uint32_t ticks);
uos_err_t uos_queue_try_send(uos_queue_t *queue, const void *msg, uos_uint32_t size);
uos_err_t uos_queue_receive(uos_queue_t *queue, void *msg, uos_uint32_t *size, uos_uint32_t ticks);
uos_err_t uos_queue_try_receive(uos_queue_t *queue, void *msg, uos_uint32_t *size);

/* timer manage */
enum timer_type {
    UOS_TIMER_ONE_SHOT = 0x01,
    UOS_TIMER_PERIODIC = 0x02,
    UOS_TIMER_ACTIVATE = 0x04,
    UOS_TIMER_DEACTIVATE = 0x08
};

typedef struct uos_timer_attr_struct {
    const char *name;
    uos_uint32_t initial;
    uos_uint32_t period;
    uos_int32_t options;
    uos_uint16_t swtmrId;
} uos_timer_attr_t;

typedef struct uos_timer_struct {
    timer_t timer_cb;
    struct sigevent event;
    struct itimerspec delay;
    uos_uint8_t option;
} uos_timer_t;

typedef void (*uos_timer_expiry_t)(void *param);
uos_timer_t *uos_timer_create(uos_timer_expiry_t expiry, void *expiry_param, uos_timer_attr_t *attr);
uos_err_t uos_timer_destroy(uos_timer_t *timer);
uos_err_t uos_timer_start(uos_timer_t *timer);
uos_err_t uos_timer_stop(uos_timer_t *timer);
uos_err_t uos_timer_is_active(uos_timer_t *timer, uos_bool_t *is_active);
uos_uint32_t uos_ms_to_tick(uos_uint32_t ms);
uos_uint32_t uos_tick_to_ms(uos_uint32_t tick);
uos_err_t uos_timer_change(uos_timer_t *timer, uos_uint32_t initial, uos_uint32_t ticks);
uos_uint32_t uos_debug_tick_count(uint16 index);
void uos_test_count(uos_uint32_t param1, uos_uint32_t param2);
void uos_data_dump_trace(void);
void uos_data_dump_threadentry(void *argument);
void uos_data_dump_trace_init(void);
void uos_data_dump_trace_deinit(void);
/********************************************************************************************/
/* dynamic memory manage */
#define uos_malloc  malloc
#define uos_calloc  calloc
#define uos_free    free
#define uos_psram_malloc  malloc
#define uos_psram_free    free
#define UOS_WEAK

void uos_interrupt_enter(void);
void uos_interrupt_exit(void);

#define DLOAD_CIT_SLEEP_PERMIT_ENABLE   0x01
#define DLOAD_CIT_SLEEP_PERMIT_DISABLE  0x00

uos_err_t uos_sleep_permit_config(uos_uint32_t bits);
errno_t uos_strcpy_s(char *dest, uint32_t dmax, const char *src);
/********************************************************************************************/

#ifdef  __cplusplus
}
#endif

#endif
