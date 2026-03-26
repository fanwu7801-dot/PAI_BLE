#include "system/init.h"
#include "system/timer.h"
#include "os/os_compat.h"
#include "uos_api.h"

#if TCFG_CAT1_UNISOC_ENABLE

static OS_QUEUE thread_exit_queue;
static OS_MUTEX thread_exit_mutex;
extern int os_task_create_static(void (*task_func)(void *p_arg),
                                 void *p_arg,
                                 u8 prio,
                                 u32 stksize,
                                 int qsize,
                                 const char *name,
                                 u8 *tcb_stk_q,
                                 u8 core);
extern OS_FLAGS  OSFlagPendGetFlagsRdy(void);
extern OS_FLAGS  OSFlagAccept(OS_FLAG_GRP *pgrp, OS_FLAGS flags, u8 wait_type, u8 *err);



void syslog(int  level, const char *format, int a, ...)
{
    va_list args;
    va_start(args, format);
    extern int ___printf(const char *format, va_list args);
    ___printf(format, args);
}


static void thread_suicide(thread_param *args)
{
    int err;
    //err = os_q_post(&thread_exit_queue, args);

    if (err) {
        printf("thread_suicide err = 0x%x\n", err);
    }

    while (1) {
        printf("Wait task [%s] exit\n", args->task_name);
        //if (os_task_del_req(OS_TASK_SELF) == OS_TASK_DEL_REQ) {
        //    os_task_del_res(OS_TASK_SELF);
        //}
        os_time_dly(30);
    }
}

static void thread_exit(thread_param *args)
{
    int err;
    err = os_q_post(&thread_exit_queue, args);
    if (err) {
        printf("thread_exit err = 0x%x\n", err);
    }
}

static void thread_exit_daemon(void *arg)
{
    int err;
    int count = 0;
    thread_param *args = NULL;

    while (1) {
        err = os_q_pend(&thread_exit_queue, 0, &args);
        if (err) {
            continue;
        }

        os_mutex_pend(&thread_exit_mutex, 0);
        if (args) {
            while (OS_TASK_NOT_EXIST != os_task_del_req(args->task_name)) {
                // printf("\n[%s]thread exit 72 err = %d, count=%d\n", args->task_name, err,count);
                os_time_dly(1);
                count ++;
                if (count == 4) {
                    err = task_kill(args->task_name);
                    if (!err) {
                        printf("task_kill succ!!\n");
                    } else {
                        printf("task_kill fail!!\n");
                    }
                    count = 0;
                    break;
                }
            }
            //printf("\n[%s]thread exit succ\n", args->task_name);
            free(args->tid);
            args->tid = NULL;
            free(args);
            args = NULL;
        } else {
            printf("thread_exit_daemon err\n");
        }

        os_mutex_post(&thread_exit_mutex);
    }
}

static int uos_init(void)
{
    int err;
    printf("uos_init xiaohuan\n");
    os_mutex_create(&thread_exit_mutex);
    os_q_create(&thread_exit_queue, PTHREAD_EXIT_QUEUE_SIZE_DEFAULT);

    err = os_task_create(thread_exit_daemon, NULL, 30, PTHREAD_EXIT_TASK_STACK_SIZE_DEFAULT, 0, "thread_exit_daemon");
    if (err) {
        printf("uos_init err\n");
    }
    return 0;
}

module_initcall(uos_init);

static void thread_task_handler(void *thread_args)
{
    thread_param *args = (thread_param *)thread_args;
    OS_TCB *task_tcb = (OS_TCB *)args->tid;
    task_tcb->pid = args;

    if (args->start_routine) {
        args->start_routine(args->arg);
    }

    thread_suicide(args);
}

uos_thread_t *uos_thread_create(uos_thread_entry_t entry, void *entry_param,
                                uos_thread_attr_t *attr)
{
    thread_param *thread_args = malloc(sizeof(thread_param));
    if (NULL == thread_args) {
        printf("uos_init thread_args err\n");
        return UOS_NULL;
    }

    memset(thread_args, 0x0, sizeof(thread_param));
    OS_TCB *task_tcb = malloc(sizeof(OS_TCB));
    if (NULL == task_tcb) {
        printf("uos_init thread_args 121 err\n");
        free(thread_args);
        return UOS_NULL;
    }

    memset(task_tcb, 0x0, sizeof(OS_TCB));

    task_tcb->OSTCBPrio = attr->priority;
    task_tcb->stk_size = attr->stack_size;
    thread_args->start_routine = entry;
    thread_args->arg = entry_param;
    thread_args->tid = (int)task_tcb;

    if (attr->thread_name) {
        memcpy(thread_args->task_name, attr->thread_name, sizeof(thread_args->task_name));
        thread_args->task_name[PTHREAD_NAME_LEN_DEFAULT] = '\0';
    }

    if (attr) {
        attr->tid = (uos_thread_id_t)thread_args;
    }
    if (AUTO_START_ENABLE == attr->option) {
        uos_thread_start(thread_args);
    }
    return (uos_thread_t *)thread_args;
}


uos_err_t uos_thread_destroy(uos_thread_t *thread)
{
    int err;
    thread_param *args = NULL;
    args = thread;
    //printf("\n[%s]thread exit star\n", args->task_name);
    err = task_kill(args->task_name);
    if (!err) {
        printf("task_kill succ!!\n");
    } else {
        printf("task_kill fail!!\n");
    }
    //printf("\n[%s]thread exit succ\n", args->task_name);
    //free(args->tid);
    args->tid = NULL;
    free(args);
    args = NULL;
    return UOS_OK;
}
uos_err_t uos_thread_suspend(uos_thread_t *thread)
{
    printf("[%s, %d]is not supported yet\n", __FUNCTION__, __LINE__);
    return UOS_OK;
}

uos_err_t uos_thread_resume(uos_thread_t *thread)
{
    printf("[%s, %d]is not supported yet\n", __FUNCTION__, __LINE__);
    return UOS_OK;
}

uos_err_t uos_thread_sleep(uos_uint32_t ticks)
{
    os_time_dly(ticks);
    return UOS_OK;
}

uos_err_t uos_thread_yield(void)
{
    os_time_dly(1);
    return UOS_OK;
}

uos_err_t uos_thread_set_priority(uos_thread_t *thread, uos_int32_t priority,
                                  uos_int32_t *org_priority)
{
    if (NULL == thread) {
        return UOS_ERR_INVAL;
    }

    u8 err;
    thread_param *args = (thread_param *)thread;
    OS_TCB *task_tcb = (OS_TCB *)args->tid;

    if (org_priority) {
        *org_priority = task_tcb->OSTCBPrio;
    }

    err = OSTaskChangePrio(args->task_name, priority);
    if (err != OS_NO_ERR) {
        return UOS_ERROR;
    }

    return UOS_OK;
}

uos_err_t uos_thread_get_priority(uos_thread_t *thread, uos_int32_t *priority)
{
    if (NULL == thread) {
        return UOS_ERR_INVAL;
    }

    thread_param *args = (thread_param *)thread;
    OS_TCB *task_tcb = (OS_TCB *)args->tid;

    if (priority) {
        *priority = task_tcb->OSTCBPrio;
        return UOS_OK;
    }

    return UOS_ERROR;
}

uos_thread_t *uos_thread_get_self(void)
{
    return (uos_thread_t *) OSTCBCur[OS_CPU_ID]->pid;
}

uos_uint32_t uos_thread_get_self_id(void)
{
    /* return (uos_uint32_t)OSTCBCur[OS_CPU_ID]; */
    return (uos_uint32_t)OSTCBCur[OS_CPU_ID]->pid;
}

uos_err_t uos_thread_get_name(uos_thread_t *thread, char *buf, int buf_size)
{
    if (NULL == thread || NULL == buf || 0 == buf_size) {
        return UOS_ERR_INVAL;
    }

    thread_param *args = (thread_param *)thread;
    memcpy(buf, args->task_name, buf_size - 1);
    buf[buf_size] = '\0';

    return UOS_OK;
}

uos_uint32_t uos_thread_get_stack_size(uos_thread_t *thread)
{
    if (NULL == thread) {
        return UOS_ERR_INVAL;
    }

    thread_param *args = (thread_param *)thread;
    OS_TCB *task_tcb = (OS_TCB *)args->tid;

    return task_tcb->stk_size;
}

uos_uint32_t uos_thread_get_used_stack_size(uos_thread_t *thread)
{
    printf("[%s, %d]is not supported yet\n", __FUNCTION__, __LINE__);
    return UOS_OK;
}

uos_err_t uos_thread_get_time_slice(uos_thread_t *thread, uos_uint32_t *time_slice)
{
    if (NULL == thread || NULL == time_slice) {
        return UOS_ERR_INVAL;
    }

    thread_param *args = (thread_param *)thread;
    OS_TCB *task_tcb = (OS_TCB *)args->tid;

    *time_slice = task_tcb->slice_quanta;

    return UOS_OK;
}

uos_err_t uos_thread_set_time_slice(uos_thread_t *thread, uos_uint32_t time_slice)
{
    printf("[%s, %d]is not supported yet\n", __FUNCTION__, __LINE__);
    return UOS_OK;
}

uos_uint64_t uos_tick_get(void)
{
    return OSTimeGet();
}

uos_err_t uos_thread_start(uos_thread_t *thread)
{
    thread_param *args = (thread_param *)thread;
    OS_TCB *task_tcb = (OS_TCB *)args->tid;
    uint32_t stack_size = task_tcb->stk_size;
    const char *task_name = args->task_name;
    size_t prio = task_tcb->OSTCBPrio;

    int err = os_task_create_static(thread_task_handler, args, prio, stack_size, 0, task_name, task_tcb, 0);
    if (err) {
        free(args);
        free(task_tcb);
        args = NULL;
        task_tcb = NULL;

        return UOS_ERROR;
    }

    return UOS_OK;
}

uos_sem_t *uos_sem_create(uos_uint32_t initial_val, uos_uint32_t max_count,
                          uos_sem_attr_t *attr)
{
    OS_SEM *psem;
    int ret = 0;
    psem = (OS_SEM *)malloc(sizeof(OS_SEM));
    if (psem == NULL) {
        return NULL;
    }

    ret = os_sem_create(psem, initial_val);

    if (ret != 0) {
        UOS_ASSERT(0);
    }

    if (attr) {
        attr->sem_id = (uos_uint32_t)psem;
    }

    return (uos_sem_t *)psem;
}


uos_err_t uos_sem_set(uos_sem_t *sem, uos_uint16_t cnt)
{
    os_sem_set((OS_SEM *)sem, cnt);
    return UOS_OK;
}


uos_err_t uos_sem_wait(uos_sem_t *sem, uos_uint32_t ticks)
{
    int ret;
    printf("xiaohuan uos_sem_wait tick %d\n", ticks);
    ret = os_sem_pend((OS_SEM *)sem, ticks);

    if (OS_NO_ERR  == ret) {
        return UOS_OK;
    }
    if (OS_TIMEOUT == ret) {
        return UOS_ERR_TIMEOUT;
    }
    return ret;
}

uos_err_t uos_sem_try_wait(uos_sem_t *sem)
{
    u8 err = os_sem_pend((OS_SEM *)sem, -1);
    if (err != 0) {
        return UOS_ERR_TIMEOUT;
    }

    return UOS_OK;
}

uos_err_t uos_sem_destroy(uos_sem_t *sem)
{
    int ret;
    ret = os_sem_del((OS_SEM *)sem, 0);

    if (sem) {
        free((void *)sem);
    }

    return ret;
}

uos_err_t uos_sem_post(uos_sem_t *sem)
{
    int ret;
    ret = os_sem_post((OS_SEM *)sem);
    return ret;
}

uos_err_t uos_sem_get_value(uos_sem_t *sem, uos_int32_t *value)
{
    *value = os_sem_query((OS_SEM *)sem);
    return UOS_OK;
}

uos_mutex_t *uos_mutex_create(uos_mutex_attr_t *attr)
{
    OS_MUTEX *pmutex;

    pmutex = (OS_MUTEX *)malloc(sizeof(OS_MUTEX));
    if (pmutex == NULL) {
        return NULL;
    }
    os_mutex_create(pmutex);

    if (attr) {
        attr->mux_id = (uos_uint32_t)pmutex;
    }

    return (uos_mutex_t *)pmutex;
}

uos_err_t uos_mutex_lock(uos_mutex_t *mutex, uos_uint32_t ticks)
{
    os_mutex_pend((OS_MUTEX *)mutex, ticks);
    return UOS_OK;
}

uos_err_t uos_mutex_try_lock(uos_mutex_t *mutex)
{
    u8 err = os_mutex_pend((OS_MUTEX *)mutex, -1);
    if (err == 0) {
        return UOS_OK;
    }

    return UOS_ERR_BUSY;
}

uos_err_t uos_mutex_unlock(uos_mutex_t *mutex)
{
    os_mutex_post((OS_MUTEX *)mutex);
    return UOS_OK;
}

uos_err_t uos_mutex_destroy(uos_mutex_t *mutex)
{
    os_mutex_del((OS_MUTEX *)mutex, OS_DEL_ALWAYS);

    if (mutex) {
        free((void *)mutex);
    }

    return UOS_OK;
}

uos_event_group_t *uos_event_group_create(uos_event_group_attr_t *attr)
{
    OS_FLAGS flags = attr->flag;
    OS_FLAG_GRP  *pgrp = NULL;
    u8 err;

    pgrp = OSFlagCreate(flags, &err);
    if (err != OS_NO_ERR || NULL == pgrp) {
        printf("err = %d, pgrp = %d", err, pgrp);
        return NULL;
    }

    /* if (attr) */
    /* attr->event_id = (uos_uint32_t)pgrp; */

    return (uos_event_group_t *)pgrp;
}

uos_err_t uos_event_group_destroy(uos_event_group_t *event_group)
{
    if (NULL == event_group) {
        return UOS_ERR_INVAL;
    }

    u8 err;

    OSFlagDel((OS_FLAG_GRP *)event_group, OS_DEL_ALWAYS, &err);
    if (err != OS_NO_ERR) {
        return UOS_ERROR;
    }

    return UOS_OK;
}

uos_err_t uos_event_group_set(uos_event_group_t *event_group, uos_uint32_t bits)
{
    if (NULL == event_group) {
        return UOS_ERR_INVAL;
    }

    OS_FLAGS flags = bits;
    u8 err;

    OSFlagPost((OS_FLAG_GRP *)event_group, flags, OS_FLAG_SET, &err);
    if (err != OS_NO_ERR) {
        return UOS_ERROR;
    }

    return UOS_OK;
}

uos_err_t uos_event_group_wait(uos_event_group_t *event_group, uos_uint32_t bits,
                               uos_uint32_t op, uos_uint32_t *actual_bits, uos_uint32_t ticks)
{
    if (NULL == event_group) {
        return UOS_ERR_INVAL;
    }

    u8 err;
    OS_FLAGS flags;
    u8 type;

    if (op & UOS_EVENT_WAIT_NO_CLEAR) {
        op &= ~UOS_EVENT_WAIT_NO_CLEAR;
        type = 0;
    } else if (op & UOS_EVENT_WAIT_CLEAR) {
        op &= ~UOS_EVENT_WAIT_CLEAR;
        type = OS_FLAG_CONSUME;
    }

    switch (op) {
    case UOS_EVENT_WAIT_ANY :
        type += OS_FLAG_WAIT_SET_ANY;
        break;

    case UOS_EVENT_WAIT_ALL :
        type += OS_FLAG_WAIT_SET_ALL;
        break;

    default:
        return UOS_ERROR;
    }

    printf("bits:0x%x, type:0x%x, ticks:0x%x\n", bits, type, ticks);
    flags = OSFlagPend((OS_FLAG_GRP *)event_group, bits, type, ticks, &err);
    if (err != OS_NO_ERR) {
        return UOS_ERROR;
    }

    if (actual_bits) {
        *actual_bits = flags;
    }

    return UOS_OK;
}

uos_err_t uos_event_group_try_wait(uos_event_group_t *event_group, uos_uint32_t bits,
                                   uos_uint32_t op, uos_uint32_t *actual_bits)
{
    if (NULL == event_group) {
        return UOS_ERR_INVAL;
    }

    u8 err;
    OS_FLAGS flags;
    u8 type;

    if (op & UOS_EVENT_WAIT_NO_CLEAR) {
        op &= ~UOS_EVENT_WAIT_NO_CLEAR;
        type = 0;
    } else if (op & UOS_EVENT_WAIT_CLEAR) {
        op &= ~UOS_EVENT_WAIT_CLEAR;
        type = OS_FLAG_CONSUME;
    }

    switch (op) {
    case UOS_EVENT_WAIT_ANY :
        type += OS_FLAG_WAIT_SET_ANY;
        break;
    case UOS_EVENT_WAIT_ALL :
        type += OS_FLAG_WAIT_SET_ALL;
        break;
    default:
        return UOS_ERROR;
    }

    flags = OSFlagAccept((OS_FLAG_GRP *)event_group, bits, type, &err);
    if (err != OS_NO_ERR) {
        return UOS_ERR_TIMEOUT;
    }

    if (actual_bits) {
        *actual_bits = flags;
    }

    return UOS_OK;
}

uos_queue_t *uos_queue_create(uos_queue_attr_t *attr)
{
    void *q = malloc(sizeof(OS_QUEUE));
    if (NULL == q) {
        return NULL;
    }

    os_q_create((OS_QUEUE *)q, attr->msg_cnt);

    return (uos_queue_t *)q;
}

uos_err_t uos_queue_destroy(uos_queue_t *queue)
{
    os_q_del((OS_QUEUE *)queue, OS_DEL_ALWAYS);
    if (queue) {
        free(queue);
    }
    return UOS_OK;
}

uos_err_t uos_queue_send(uos_queue_t *queue, const void *msg, uos_uint32_t size,
                         uos_uint32_t ticks)
{
    if (NULL == queue || NULL == msg || 0 == size) {
        return UOS_ERROR;
    }

    queue_element *send_msg = malloc(sizeof(queue_element) + size);
    if (NULL == send_msg) {
        return UOS_ERROR;
    }

    send_msg->data_size = size;
    send_msg->data = (char *)send_msg + sizeof(queue_element);

    memcpy(send_msg->data, msg, size);

    int err = os_q_post((OS_QUEUE *)queue, send_msg);
    if (err) {
        free(send_msg);
    }

    return UOS_OK;
}

uos_err_t uos_queue_try_send(uos_queue_t *queue, const void *msg, uos_uint32_t size)
{
    if (NULL == queue || NULL == msg || 0 == size) {
        return UOS_ERROR;
    }

    queue_element *send_msg = malloc(sizeof(queue_element) + size);
    if (NULL == send_msg) {
        return UOS_ERROR;
    }

    send_msg->data_size = size;
    send_msg->data = (char *)send_msg + sizeof(queue_element);

    memcpy(send_msg->data, msg, size);

    int err = os_q_post((OS_QUEUE *)queue, send_msg);
    if (err) {
        free(send_msg);
    }

    return UOS_OK;
}

uos_err_t uos_queue_receive(uos_queue_t *queue, void *msg, uos_uint32_t *size,
                            uos_uint32_t ticks)
{
    queue_element *receive_msg;
    int err;

    err = os_q_pend((OS_QUEUE *)queue, ticks, &receive_msg);
    if (err) {
        return UOS_ERROR;
    }

    if (size) {
        *size = receive_msg->data_size;
    }

    memcpy(msg, receive_msg->data, receive_msg->data_size);
    free(receive_msg);

    return UOS_OK;
}

uos_err_t uos_queue_try_receive(uos_queue_t *queue, void *msg, uos_uint32_t *size)
{
    queue_element *receive_msg = NULL;
    int err;

    err = os_q_pend((OS_QUEUE *)queue, -1, &receive_msg);
    if (err != 0) {
        return UOS_ERROR;
    }

    if (size) {
        *size = receive_msg->data_size;
    }

    memcpy(msg, receive_msg->data, receive_msg->data_size);
    free(receive_msg);

    return UOS_OK;
}

static void timer_task_handler(void *args)
{
    timer_ctrl *ptimer = (timer_ctrl *)args;

    if (ptimer->timer_cb) {
        ptimer->timer_cb(ptimer->param);
    }

    /* sys_timer_del(ptimer->id); */
}

uos_timer_t *uos_timer_create(uos_timer_expiry_t expiry, void *expiry_param,
                              uos_timer_attr_t *attr)
{
    timer_ctrl *ptimer = malloc(sizeof(timer_ctrl));
    if (NULL == ptimer) {
        return NULL;
    }

    memset(ptimer, 0x0, sizeof(timer_ctrl));

    ptimer->timer_cb = expiry;
    ptimer->param = expiry_param;
    /* ptimer->period = attr->period; */
    ptimer->period = uos_tick_to_ms(attr->period); //一个tick为10ms
    ptimer->id = 0;
    ptimer->is_active = 0;

    if (attr) {
        attr->swtmrId = ptimer;
        ptimer->option = attr->options;
    }
    return (uos_timer_t *)ptimer;
}

uos_err_t uos_timer_destroy(uos_timer_t *timer)
{
    if (NULL == timer) {
        return UOS_ERROR;
    }

    timer_ctrl *ptimer = (timer_ctrl *)timer;

    if (ptimer->option == UOS_TIMER_PERIODIC) {
        sys_timer_del(ptimer->id);
    } else {
        sys_timeout_del(ptimer->id);
    }
    free(ptimer);

    return UOS_OK;
}

uos_err_t uos_timer_start(uos_timer_t *timer)
{
    if (NULL == timer) {
        return UOS_ERROR;
    }

    timer_ctrl *ptimer = (timer_ctrl *)timer;

    if (ptimer->is_active == 0) {
        if (ptimer->option == UOS_TIMER_PERIODIC) {
            ptimer->id = sys_timer_add(ptimer, timer_task_handler, ptimer->period);
        } else {
            ptimer->id = sys_timeout_add(ptimer, timer_task_handler, ptimer->period);
        }
        ptimer->is_active = 1;
    } else if (ptimer->is_active == 1) {
        printf("timer 0x%x still running\n", ptimer);
    }

    return UOS_OK;
}

uos_err_t uos_timer_stop(uos_timer_t *timer)
{
    if (NULL == timer) {
        return UOS_ERROR;
    }

    timer_ctrl *ptimer = (timer_ctrl *)timer;
    if (ptimer->option == UOS_TIMER_PERIODIC) {
        sys_timer_del(ptimer->id);
    } else {
        sys_timeout_del(ptimer->id);
    }
    ptimer->is_active = 0;

    return UOS_OK;
}

uos_err_t uos_timer_is_active(uos_timer_t *timer, uos_bool_t *is_active)
{
    if (NULL == timer) {
        return UOS_ERROR;
    }

    timer_ctrl *ptimer = (timer_ctrl *)timer;
    if (is_active) {
        *is_active = ptimer->is_active;
    }

    return UOS_OK;
}

uos_err_t uos_timer_change(uos_timer_t *timer, uos_uint32_t initial,
                           uos_uint32_t ticks)
{
    if (NULL == timer) {
        return UOS_ERROR;
    }

    timer_ctrl *ptimer = (timer_ctrl *)timer;

    if (ptimer->is_active) {
        ptimer->period = uos_tick_to_ms(ticks);
        sys_timer_modify(ptimer->id, ptimer->period);
    } else {
        ptimer->period = uos_tick_to_ms(ticks);
    }

    return UOS_OK;
}

uos_uint32_t uos_ms_to_tick(uos_uint32_t ms)
{
    uos_uint32_t tick;

    if (UOS_WAIT_FOREVER == ms) {
        return UOS_WAIT_FOREVER;
    }

    if (ms > 0 && ms < (1000 / OS_TICKS_PER_SEC)) {
        tick = 1;
    } else {
        tick = ms * OS_TICKS_PER_SEC / 1000;
    }

    return tick;
}

uos_uint32_t uos_tick_to_ms(uos_uint32_t tick)
{
    uos_uint32_t ms;
    ms = tick * 1000 / OS_TICKS_PER_SEC;
    return ms;
}

uos_uint32_t uos_debug_tick_count(uint16 index)
{
    printf("[%s, %d]is not supported yet\n", __FUNCTION__, __LINE__);
    return uos_tick_get();
}

uos_err_t uos_sleep_permit_config(uos_uint32_t bits)
{
    printf("[%s, %d]is not supported yet\n", __FUNCTION__, __LINE__);
    return UOS_OK;
}
#endif
