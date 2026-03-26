#include "pthread.h"
#include "system/init.h"
#include <timer.h>

static OS_QUEUE thread_exit_queue;
static OS_MUTEX thread_exit_mutex;

static void thread_exit(phread_args *args)
{
    int err;
    err = os_q_post(&thread_exit_queue, args);
    if (err) {
        printf("thread_exit err = 0x%x\n", err);
    }

    while (1) {
        printf("Wait task [%s] exit\n", args->task_name);
        os_time_dly(30);
    }
}

static void thread_exit_task(void *arg)
{
    int err;
    phread_args *args = NULL;

    while (1) {
        err = os_q_pend(&thread_exit_queue, 0, &args);
        if (err) {
            continue;
        }

        os_mutex_pend(&thread_exit_mutex, 0);

        if (args) {
            os_task_del(args->task_name);
            printf("\n[%s]thread exit succ\n", args->task_name);

            free(args);
            args = NULL;
        } else {
            printf("thread_exit_task err\n");
        }

        os_mutex_post(&thread_exit_mutex);
    }
}

static int pthread_init(void)
{
    int err;
    os_mutex_create(&thread_exit_mutex);
    os_q_create(&thread_exit_queue, PTHREAD_EXIT_QUEUE_SIZE_DEFAULT);
    err = os_task_create(thread_exit_task, NULL, 30, PTHREAD_EXIT_TASK_STACK_SIZE_DEFAULT, 0, "thread_exit_task");
    if (err) {
        printf("pthread_init err\n");
    }
    return 0;
}
platform_initcall(pthread_init);

long long tm_to_ms(struct timspec *tm)
{
    struct timeval now;
    long long msec;
    long def = 1000L;
    struct timespec *ts;
    gettimeofday(&now, (struct timezone *)NULL);
    //printf("[tm_to_ms]now,tv_sec:%ld,tv_usec:%ld\n", now.tv_sec,now.tv_usec);
    ts = tm;
    msec = (ts->tv_sec - now.tv_sec) * COUNT + (ts->tv_nsec / def - now.tv_usec) / COUNT ;
    //printf("[tm_to_ms]in:%ld,%ld,out:%lld\n", ts->tv_sec,ts->tv_nsec, msec);
    return msec;
}

static void pthread_task_handler(void *thread_args)
{
    phread_args *args = (phread_args *)thread_args;
    OS_TCB *task_tcb = (OS_TCB *)args->pid;
    if (args->start_routine) {
        args->start_routine(args->arg);
    }

    thread_exit(args);
}

int pthread_create(pthread_t *thread,
                   const pthread_attr_t *attr,
                   void *(*startroutine)(void *),
                   void *arg)
{
    uint32_t stack_size = PTHREAD_TASK_STACK_SIZE_DEFAULT;
    const char *task_name = PTHREAD_TASK_NAME_DEFAULT;
    OS_TCB *task_tcb;
    size_t prio = PTHREAD_TASK_STACK_PRIORITY_DEFAULT;

    if (attr) {
        if (attr->stacksize) {
            stack_size = attr->stacksize;
        }

        if (attr->priority) {
            prio = attr->priority;
        }
    }

    phread_args *thread_args = malloc(sizeof(phread_args));
    if (NULL == thread_args) {
        return -1;
    }

    //memory release in os_task_del
    task_tcb = malloc(sizeof(OS_TCB));
    if (NULL == task_tcb) {
        free(thread_args);
        return -1;
    }

    thread_args->start_routine = startroutine;
    thread_args->arg = arg;
    thread_args->pid = (int)task_tcb;
    *thread = thread_args->pid;

    snprintf(thread_args->task_name, PTHREAD_NAME_LEN_DEFAULT - 1, "ph_%x", thread_args->pid);

    int ret = os_task_create_static(pthread_task_handler, thread_args, prio, stack_size, 0, thread_args->task_name, task_tcb, 1);
    if (ret) {
        free(thread_args);
        free(task_tcb);
        thread_args = NULL;
        task_tcb = NULL;
        return -1;
    }

    return 0;
}

int pthread_cancel(pthread_t thread)
{
    OS_TCB *task_tcb = NULL;
    int ret          = 0;

    if (NULL == thread) {
        return -1;
    }

    task_tcb = (OS_TCB *)thread;
    ret = os_task_del(task_tcb->name);
    printf("[%s, %d] os_task_del and ret = %d\n", __FUNCTION__, __LINE__, ret);

    return ret;
}

int pthread_attr_init(pthread_attr_t *attr)
{
    if (NULL == attr) {
        return -1;
    }

    memset(attr, 0, sizeof(pthread_attr_t));
    return 0;
}

int pthread_attr_setschedparam(pthread_attr_t *attr,
                               const struct sched_param *param)
{
    attr->priority = param->sched_priority;
    return 0;
}

int pthread_attr_setstacksize(pthread_attr_t *attr,
                              size_t stacksize)
{
    if (stacksize > PTHREAD_STACK_MAX_DEFAULT || stacksize < PTHREAD_STACK_MIN_DEFAULT) {
        return -1;
    } else {
        attr->stacksize = (uint16_t) stacksize;
    }

    return 0;
}

int pthread_attr_setdetachstate(pthread_attr_t *attr,
                                int detachstate)
{
    printf("[%s, %d]is not supported yet\n", __FUNCTION__, __LINE__);
    return 0;
}

int pthread_equal(pthread_t t1,
                  pthread_t t2)
{
    return t1 == t2;
}

pthread_t pthread_self(void)
{
    OS_TCB *pid = OSTCBCur[OS_CPU_ID];
    return (int)pid;
}

int pthread_mutex_init(pthread_mutex_t *mutex,
                       const pthread_mutexattr_t *attr)
{
    if (NULL == mutex) {
        return -1;
    }
    return os_mutex_create(mutex);
}

int pthread_mutex_lock(pthread_mutex_t *mutex)
{
    OS_MUTEX *pMutex = (OS_MUTEX *)mutex;

    if (NULL == mutex) {
        return -1;
    }

    if (0 == pMutex->OSEventType) {
        os_mutex_create((OS_MUTEX *)mutex);
    }
    return os_mutex_pend((OS_MUTEX *)mutex, 0);
}

int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
    return os_mutex_post((OS_MUTEX *)mutex);
}

int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
    int ret = 0;

    if ((mutex != NULL) && ((void *)mutex != NULL)) {
        ret = os_mutex_del((OS_MUTEX *)mutex, OS_DEL_ALWAYS);
        //*mutex = 0;
    }
    return ret;
}

int pthread_mutexattr_init(pthread_mutexattr_t *attr)
{
    printf("[%s, %d]is not supported yet\n", __FUNCTION__, __LINE__);
    return 0;
}

mqd_t mq_open(const char *name,
              int oflag,
              mode_t mode,
              struct mq_attr *attr)
{
    mqd_t t;
    long msgsize;

    void *q = malloc(sizeof(OS_QUEUE));
    if (NULL == q) {
        return -1;
    }

    struct mq_attr DefaultAttr = {
        .mq_maxmsg = PTHREAD_MQ_MAX_MSG_DEFAULT,
        .mq_msgsize = PTHREAD_MQ_MSG_SIZE_DEFAULT,
    };

    if (attr) {
        if (attr->mq_msgsize && attr->mq_maxmsg) {
            msgsize = attr->mq_msgsize * attr->mq_maxmsg;
        } else if (attr->mq_msgsize) {
            msgsize = attr->mq_msgsize * DefaultAttr.mq_maxmsg;
        } else if (attr->mq_maxmsg) {
            msgsize = DefaultAttr.mq_msgsize * attr->mq_maxmsg;
        } else {
            msgsize = DefaultAttr.mq_msgsize * DefaultAttr.mq_maxmsg;
        }
    } else {
        msgsize = DefaultAttr.mq_msgsize * DefaultAttr.mq_maxmsg;
    }

    os_q_create((OS_QUEUE *)q, msgsize);

    return (int)q;
}

int mq_send(mqd_t mqdes,
            const char *msg_ptr,
            size_t msg_len,
            unsigned msg_prio)
{
    void *msg;
    int ret = -1;

    if (NULL == msg_ptr || 0 == msg_len || -1 == mqdes) {
        return -1;
    }

    msg = malloc(msg_len);
    if (NULL == msg) {
        return -1;
    }

    memcpy(msg, msg_ptr, msg_len);

    ret = os_q_post((OS_QUEUE *)mqdes, msg);
    if (ret) {
        free(msg);
    }

    return ret;
}

ssize_t mq_receive(mqd_t mqdes,
                   char *msg_ptr,
                   size_t msg_len,
                   unsigned int *msg_prio)
{
    if (NULL == msg_ptr || 0 == msg_len || -1 == mqdes) {
        return -1;
    }

    char *msg;
    int err;

    err = os_q_pend((OS_QUEUE *)mqdes, 0, &msg);

    /* After mq_close, here will return 0, but msg is null. */
    if (err || !msg) {
        return -1;
    }

    memcpy(msg_ptr, msg, msg_len);
    free(msg);

    return msg_len;
}

ssize_t mq_timedreceive(mqd_t mqdes,
                        char *msg_ptr,
                        size_t msg_len,
                        unsigned int *msg_prio,
                        const struct timespec *abstime)
{

    char *msg;
    int err;
    struct timespec *time_spec;
    int timeout = 1000;
    long long timeout_temp;
    ssize_t ret = 0;

    if (NULL == msg_ptr || 0 == msg_len || -1 == mqdes) {
        return -1;
    }

    time_spec = abstime;
    timeout_temp = tm_to_ms(time_spec);
    if (timeout_temp < 0) {
        timeout = 0;
    }
    if (timeout_temp == 0) {
        timeout = 0xFFFFFFFF;
    }

    if (timeout != 0 && timeout != 0xFFFFFFFF) {
        if ((timeout_temp / 10 == 0) && (timeout_temp % 10 != 0)) {
            timeout = 1;
        } else {
            timeout = timeout_temp / 10;
        }
    }

    err = os_q_pend((OS_QUEUE *)mqdes, timeout, &msg);
    if (err) {
        return -1;
    } else {
        ret = msg_len;
    }

    memcpy(msg_ptr, msg, msg_len);
    free(msg);

    return ret;
}

int mq_close(mqd_t mqdes)
{
    if (-1 == mqdes) {
        return -1;
    }

    os_q_del((OS_QUEUE *)mqdes, OS_DEL_ALWAYS);
    free(mqdes);
    return 0;
}

int mq_getattr(mqd_t mqdes,
               struct mq_attr *mqstat)
{
    printf("[%s, %d]is not supported yet\n", __FUNCTION__, __LINE__);
    return 0;
}

int pthread_cond_init(pthread_cond_t *cond,
                      const pthread_condattr_t *attr)
{
    if (NULL == cond) {
        return -1;
    }
    return os_sem_create((OS_SEM *)cond, 0);
}

int pthread_cond_signal(pthread_cond_t *cond)
{
    if (OS_ERR_EVENT_TYPE == os_sem_query((OS_SEM *)cond)) {
        os_sem_create((OS_SEM *)cond, 0);
    }

    os_sem_post((OS_SEM *)cond);

    return 0;
}

int pthread_cond_wait(pthread_cond_t *cond,
                      pthread_mutex_t *mutex)
{
    if (OS_ERR_EVENT_TYPE == os_sem_query((OS_SEM *)cond)) {
        os_sem_create((OS_SEM *)cond, 0);
    }

    if (mutex) {
        pthread_mutex_unlock(mutex);
    }

    os_sem_pend((OS_SEM *)cond, 0);

    if (mutex) {
        pthread_mutex_lock(mutex);
    }

    return 0;
}

int pthread_cond_broadcast(pthread_cond_t *cond)
{
    printf("[%s, %d]is not supported yet\n", __FUNCTION__, __LINE__);
    /* while (OSSemAccept((OS_EVENT *)cond) > 0) { */
    /* printf("pthread_cond_broadcast\n"); */
    /* } */
    return 0;
}

int pthread_cond_destroy(pthread_cond_t *cond)
{
    os_sem_del((OS_SEM *)cond, OS_DEL_ALWAYS);
    return 0;
}

int pthread_cond_timedwait(pthread_cond_t *cond,
                           pthread_mutex_t *mutex,
                           const struct timspec *abstime)
{
    int timeout = 0;
    int ret = 0;
    long long msec = 0;
    struct timespec *ts;

    if (OS_ERR_EVENT_TYPE == os_sem_query((OS_SEM *)cond)) {
        os_sem_create((OS_SEM *)cond, 0);
    }

    if (mutex) {
        pthread_mutex_unlock(mutex);
    }

    ts = abstime;
    msec = tm_to_ms(ts);

    if (msec <= 0) {
        timeout = 0;
    } else if (msec <= (1000 / OS_TICKS_PER_SEC)) {
        timeout = 1;
    } else {
        timeout = (int)(msec * OS_TICKS_PER_SEC / 1000);
    }

    ret = os_sem_pend((OS_SEM *)cond, timeout);

    if (mutex) {
        pthread_mutex_lock(mutex);
    }

    if (OS_TIMEOUT == ret) {
        return ETIMEDOUT;
    }
    return 0;
}

