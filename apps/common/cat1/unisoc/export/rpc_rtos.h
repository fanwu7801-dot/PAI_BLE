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
 * FileName     :   rpc_rtos.h
 * Author       :   wei.zhou
 * Version      :   v1.0
 * Date         :   2021/12/31
 * Description  :   RPC Rtos provides standard POSIX interface on RTOS platform
 * History      :   1.2021/12/31  Initial draft
 *=========================================================
 * 2021-12-31 wei.zhou create.
 */
#ifndef _RPC_RTOS_H_
#define _RPC_RTOS_H_
#include <stdint.h>
#include <stddef.h>
#include "rpc.h"
#include "stdarg.h"

extern BOOLEAN rpc_log_enable;

#ifndef ENABLE_MODEM_SDK_DEBUG_LOG
#define ENABLE_MODEM_SDK_DEBUG_LOG
#endif

#ifdef ENABLE_MODEM_SDK_DEBUG_LOG
#define rpc_printf(format, param_num, ...) \
    do { \
        if(rpc_log_enable) { \
            UOS_LOG_INFO(format"\r\n", param_num, ##__VA_ARGS__); \
        } \
    } while (0)
#else
#define rpc_printf(format, param_num, ...) \
    do { \
    } while (0)
#endif

#define rpc_printf_error(format, param_num, ...) \
    do { \
            UOS_LOG_ERROR(format"\r\n", param_num, ##__VA_ARGS__); \
    } while (0)

// clang-format off
#define RPC_PRIORITY_HIGHEST            1    /*thread priority: high*/
#define RPC_PRIORITY_ABOVE_NORMAL       2    /*thread priority: above middle*/
#define RPC_PRIORITY_NORMAL             3    /*thread priority: middle*/
#define RPC_PRIORITY_BELOW_NORMAL       4    /*thread priority: below middle*/
#define RPC_PRIORITY_LOWEST             5    /*thread priority: low*/

/*Auto start option on thread*/
#define RPC_DONT_START                  0    /*thread start: no auto start*/
#define RPC_AUTO_START                  1    /*thread start: auto start*/

/*Indicates if the thread is preemptable*/
#define RPC_NO_PREEMPT                  0    /*thread preempt: no  preempt*/
#define RPC_PREEMPT                     1    /*thread preempt: preempt*/

/*priority inherit mode for mutex*/
#define RPC_NO_INHERIT                  0    /*thread inherit: no  inherit*/
#define RPC_INHERIT                     1    /*thread inherit: inherit*/

/*mq para*/
#define RPC_MQ_SET_THREAD_ID            0    /*set mq receive thread id*/


#define rpc_memset uos_memset
#define rpc_memcpy uos_memcpy
#define rpc_strcpy uos_strcpy
#define rpc_strncpy uos_strncpy
#define rpc_strcat uos_strcat
#define rpc_strlen uos_strlen
#define rpc_strcmp uos_strcmp
#define rpc_strncmp uos_strncmp


//typedef signed int ssize_t;
typedef unsigned int rpc_time_t;
typedef uos_sem_t rpc_sem_t;
typedef unsigned int rpc_mode_t;
typedef uos_queue_t rpc_mqd_t;

typedef struct {
    unsigned int tid;
    uos_thread_t *thread_cb;           /* thread */
} rpc_pthread_t;

typedef struct {
    uos_mutex_t *mutex_cb;           /* mutex */
} rpc_pthread_mutex_t;

typedef struct {
    char *name_ptr;              /* mutex name */
    uint32_t inherit;            /* mutex inherit attribute */
} rpc_pthread_mutexattr_t;

typedef struct {
    int         schedpolicy;     /*thread scheduling strategy*/
    int         sched_priority;  /*thread proority*/
    char        *thread_name;    /*name string of the thread*/
    char        *queue_name;     /*queue name string of the thread*/
    void        *stackaddr;      /*location of thread stack*/
    size_t      stacksize;       /*the size of the thread stack*/
    uint32_t    queue_num;       /*Number of messages which can be enqueued*/
    uint32_t    preempt;         /*Indicates if the thread is preemptable*/
    uint32_t    auto_start;      /*immediately or stays in a pure suspended*/
    uint32_t    ticks;           /*the time slice allocated by the thread*/
    uint32_t    threadid;        /*thread id*/
} rpc_pthread_attr_t;

typedef struct {
    rpc_time_t tv_sec;           /*seconds*/
    long tv_nsec;                /*nanosecond */
} rpc_timespec_t;

typedef struct {
    unsigned int index;
    uos_thread_t *thread_cb;           /* thread */
} rpc_statistic_t;
// clang-format on

/**
 * Function     :    rpc_pthread_create
 * Author       :    wei.zhou
 * Description  :    create thread.
 * Input        :    tidp               thread id(is not NULL)
 *                   attr               thread attribute(is not NULL)
 *                   start_rtn          function pointer(is not NULL)
 *                   arg                function arg
 * Output       :    int
 * Return       :    0                  create thread successfully
 *                   other              create thread failed
 * History      :    1.2021/12/31  Initial draft
 *=========================================================
 * 2021-12-31 wei.zhou create.
 */
int rpc_pthread_create(rpc_pthread_t *tidp, const rpc_pthread_attr_t *attr,
                       void (*start_rtn)(void *), void *arg);

/**
 * Function     :    rpc_pthread_start
 * Author       :    qin.zhou
 * Description  :    start thread.
 * Input        :    tid                thread id
 * Output       :    int
 * Return       :    0                  start thread successfully
 *                   other              start thread failed
 * History      :    1.2022/11/26  Initial draft
 *=========================================================
 * 2022-11-26 qin.zhou create.
 */
int rpc_pthread_start(rpc_pthread_t tid);

/**
 * Function     :    rpc_pthread_cancel
 * Author       :    wei.zhou
 * Description  :    exit thread.
 * Input        :    tid                thread id
 * Output       :    int
 * Return       :    0                  create thread successfully
 *                   other              create thread failed
 * History      :    1.2021/12/31  Initial draft
 *=========================================================
 * 2021-12-31 wei.zhou create.
 */
int rpc_pthread_cancel(rpc_pthread_t tid);

/**
 * Function     :    rpc_pthread_self id
 * Author       :    wei.zhou
 * Description  :    get current thread id.
 * Input        :    void
 * Output       :    rpc_pthread_t
 * Return       :    current thread id
 * History      :    1.2021/12/31  Initial draft
 *=========================================================
 * 2021-12-31 wei.zhou create.
 */
uos_uint32_t rpc_pthread_self_id(void);

/**
 * Function     :    rpc_pthread_resume
 * Author       :    wei.zhou
 * Description  :    resume thread.
 * Input        :    tid                thread id
 * Output       :    int
 * Return       :    0                  resume thread successfully
 *                   other              resume thread failed
 * History      :    1.2021/12/31  Initial draft
 *=========================================================
 * 2021-12-31 wei.zhou create.
 */
int rpc_pthread_resume(rpc_pthread_t tid);

/**
 * Function     :    rpc_pthread_suspend
 * Author       :    wei.zhou
 * Description  :    suspend thread.
 * Input        :    tid                thread id
 * Output       :    int
 * Return       :    0                  suspend thread successfully
 *                   other              suspend thread failed
 * History      :    1.2021/12/31  Initial draft
 *=========================================================
 * 2021-12-31 wei.zhou create.
 */
int rpc_pthread_suspend(rpc_pthread_t tid);

/**
 * Function     :    rpc_pthread_mutex_init
 * Author       :    wei.zhou
 * Description  :    create mutex.
 * Input        :    mutex              mutex pointer(is not NULL)
 *                   attr               mutex attribute(is not NULL)
 * Output       :    int
 * Return       :    0                  create mutex successfully
 *                   other              create mutex failed
 * History      :    1.2021/12/31  Initial draft
 *=========================================================
 * 2021-12-31 wei.zhou create.
 */
int rpc_pthread_mutex_init(rpc_pthread_mutex_t *mutex,
                           const rpc_pthread_mutexattr_t *attr);

/**
 * Function     :    rpc_pthread_mutex_lock
 * Author       :    wei.zhou
 * Description  :    mutex lock.
 * Input        :    mutex              mutex pointer
 * Output       :    int
 * Return       :    0                  mutex lock successfully
 *                   other              mutex lock failed
 * History      :    1.2021/12/31  Initial draft
 *=========================================================
 * 2021-12-31 wei.zhou create.
 */
int rpc_pthread_mutex_lock(rpc_pthread_mutex_t *mutex);

/**
 * Function     :    rpc_pthread_mutex_unlock
 * Author       :    wei.zhou
 * Description  :    mutex unlock.
 * Input        :    mutex              mutex pointer
 * Output       :    int
 * Return       :    0                  mutex unlock successfully
 *                   other              mutex unlock failed
 * History      :    1.2021/12/31  Initial draft
 *=========================================================
 * 2021-12-31 wei.zhou create.
 */
int rpc_pthread_mutex_unlock(rpc_pthread_mutex_t *mutex);

/**
 * Function     :    rpc_pthread_mutex_destroy
 * Author       :    wei.zhou
 * Description  :    destroy mutex.
 * Input        :    mutex              mutex pointer
 * Output       :    int
 * Return       :    0                  destroy mutex successfully
 *                   other              destroy mutex failed
 * History      :    1.2021/12/31  Initial draft
 *=========================================================
 * 2021-12-31 wei.zhou create.
 */
int rpc_pthread_mutex_destroy(rpc_pthread_mutex_t *mutex);

/**
 * Function     :    rpc_sem_open
 * Author       :    wei.zhou
 * Description  :    create sem.
 * Input        :    name               sem name(is not NULL)
 *                   oflag              no use
 *                   mode               no use
 *                   value              sem value
 * Output       :    rpc_sem_t
 * Return       :    sem handle
 * History      :    1.2021/12/31  Initial draft
 *=========================================================
 * 2021-12-31 wei.zhou create.
 */
rpc_sem_t *rpc_sem_open(const char *name, int oflag, rpc_mode_t mode,
                        unsigned int value);

/**
 * Function     :    rpc_sem_post
 * Author       :    wei.zhou
 * Description  :    post sem.
 * Input        :    sem                sem pointer
 * Output       :    int
 * Return       :    0                  post sem successfully
 *                   other              post sem failed
 * History      :    1.2021/12/31  Initial draft
 *=========================================================
 * 2021-12-31 wei.zhou create.
 */
int rpc_sem_post(rpc_sem_t *sem);

/**
 * Function     :    rpc_sem_wait
 * Author       :    wei.zhou
 * Description  :    get sem.
 * Input        :    sem                sem pointer
 * Input        :    uint32                timeout
 * Output       :    int
 * Return       :    0                  get sem successfully
 *                   other              get sem failed
 * History      :    1.2021/12/31  Initial draft
 *=========================================================
 * 2021-12-31 wei.zhou create.
 */
int rpc_sem_wait(rpc_sem_t *sem, uint32 timeout_ms);

/**
 * Function     :    rpc_sem_timedwait
 * Author       :    wei.zhou
 * Description  :    get sem with time limit.
 * Input        :    sem                sem pointer
 *                   timeout            time limit value
 * Output       :    int
 * Return       :    0                  get sem successfully
 *                   other              get sem failed
 * History      :    1.2021/12/31  Initial draft
 *=========================================================
 * 2021-12-31 wei.zhou create.
 */
int rpc_sem_timedwait(rpc_sem_t *sem, rpc_timespec_t *timeout);

/**
 * Function     :    rpc_sem_destroy
 * Author       :    wei.zhou
 * Description  :    destroy sem.
 * Input        :    sem                sem pointer
 * Output       :    int
 * Return       :    0                  destroy sem successfully
 *                   other              destroy sem failed
 * History      :    1.2021/12/31  Initial draft
 *=========================================================
 * 2021-12-31 wei.zhou create.
 */
int rpc_sem_destroy(rpc_sem_t *sem);

/**
 * Function     :    rpc_mq_open
 * Author       :    wei.zhou
 * Description  :    create message queue.
 * Input        :    name               mq name(is not NULL)
 *                   msgsize            mq size
 *                   maxmsg             msg size
 *                   flag               control flag
 * Output       :    rpc_mqd_t
 * Return       :    mq handle
 * History      :    1.2021/12/31  Initial draft
 *=========================================================
 * 2021-12-31 wei.zhou create.
 */
rpc_mqd_t *rpc_mq_open(const char *name, int msgsize, int maxmsg, int flag);

/**
 * Function     :    rpc_mq_send
 * Author       :    wei.zhou
 * Description  :    send message throught mq.
 * Input        :    mqdes              mq handle
 *                   msg_ptr            message pointer(is not empty)
 *                   msg_len            message size(> 0)
 *                   msg_prio           message priority
 * Output       :    int
 * Return       :    0                  send message successfully
 *                   other              send message failed
 * History      :    1.2021/12/31  Initial draft
 *=========================================================
 * 2021-12-31 wei.zhou create.
 */
int rpc_mq_send(rpc_mqd_t *mqdes, const char *msg_ptr, size_t msg_len,
                unsigned int msg_prio);

/**
 * Function     :    rpc_mq_receive
 * Author       :    wei.zhou
 * Description  :    receive message throught mq.
 * Input        :    mqdes              mq handle
 *                   msg_ptr            message pointer
 *                   msg_len            message size(> 0)
 *                   msg_prio           message priority
 * Output       :    int
 * Return       :    0                  receive message successfully
 *                   other              receive message failed
 * History      :    1.2021/12/31  Initial draft
 *=========================================================
 * 2021-12-31 wei.zhou create.
 */
ssize_t rpc_mq_receive(rpc_mqd_t *mqdes, char *msg_ptr, size_t msg_len,
                       unsigned int *msg_prio);

/**
 * Function     :    rpc_mq_control
 * Author       :    wei.zhou
 * Description  :    control mq.
 * Input        :    mqdes              mq handle
 *                   cmd                mq command
 *                   arg                arg
 * Output       :    int
 * Return       :    0                  control mq successfully
 *                   other              control mq failed
 * History      :    1.2021/12/31  Initial draft
 *=========================================================
 * 2021-12-31 wei.zhou create.
 */
int rpc_mq_control(rpc_mqd_t *mqdes, int cmd, void *arg);

/**
 * Function     :    rpc_mq_close
 * Author       :    wei.zhou
 * Description  :    close mq.
 * Input        :    mqdes              mq handle
 * Output       :    int
 * Return       :    0                  close mq successfully
 *                   other              close mq failed
 * History      :    1.2021/12/31  Initial draft
 *=========================================================
 * 2021-12-31 wei.zhou create.
 */
int rpc_mq_close(rpc_mqd_t *mqdes);

/**
 * Function     :    rpc_get_tick
 * Author       :    wei.zhou
 * Description  :    get current tick.
 * Input        :    void
 * Output       :    uint32_t
 * Return       :    current tick(ms)
 * History      :    1.2021/12/31  Initial draft
 *=========================================================
 * 2021-12-31 wei.zhou create.
 */
uint32_t rpc_get_tick(void);

/**
 * Function     :    rpc_os_malloc
 * Author       :    wei.zhou
 * Description  :    rpc_os_malloc.
 * Input        :    size               malloc size
 * Output       :    void*
 * Return       :    void*
 * History      :    1.2021/12/31  Initial draft
 *=========================================================
 * 2021-12-31 wei.zhou create.
 */
void *rpc_os_malloc(unsigned int size);

/**
 * Function     :    rpc_os_calloc
 * Author       :    wei.zhou
 * Description  :    rpc_os_calloc.
 * Input        :    num                calloc num
 *                   size               calloc size
 * Output       :    void*
 * Return       :    void*
 * History      :    1.2021/12/31  Initial draft
 *=========================================================
 * 2021-12-31 wei.zhou create.
 */
void *rpc_os_calloc(unsigned int num, unsigned int size);

/**
 * Function     :    rpc_os_free
 * Author       :    wei.zhou
 * Description  :    rpc_os_free.
 * Input        :    ptr                need free pointer
 * Output       :    void
 * Return       :    void
 * History      :    1.2021/12/31  Initial draft
 *=========================================================
 * 2021-12-31 wei.zhou create.
 */
void rpc_os_free(void *ptr);

/**
 * Function     :    rpc_assert
 * Author       :    wei.zhou
 * Description  :    assert expression.
 * Input        :    expression         expression
 * Output       :    void
 * Return       :    void
 * History      :    1.2021/12/31  Initial draft
 *=========================================================
 * 2021-12-31 wei.zhou create.
 */
void rpc_assert(int expression);

/**
 * Function     :    rpc_sleep
 * Author       :    wei.zhou
 * Description  :    rpc_sleep.
 * Input        :    time_in_ms         sleep time(ms)
 * Output       :    int
 * Return       :    0                  sleep successfully
 *                   other              sleep failed
 * History      :    1.2021/12/31  Initial draft
 *=========================================================
 * 2021-12-31 wei.zhou create.
 */
int rpc_sleep(unsigned int time_in_ms);

void set_rpc_log_enable(BOOLEAN flag);

#endif
