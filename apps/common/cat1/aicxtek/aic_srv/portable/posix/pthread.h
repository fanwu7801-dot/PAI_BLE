#ifndef _PTHREAD_H
#define _PTHREAD_H

//#include "os/os_compat.h"
#include "os/os_api.h"
#include <time.h>

#ifndef PTHREAD_STACK_MAX_DEFAULT
#define PTHREAD_STACK_MAX_DEFAULT (10*1024)
#endif

#ifndef PTHREAD_STACK_MIN_DEFAULT
#define PTHREAD_STACK_MIN_DEFAULT (0)
#endif

#ifndef PTHREAD_TASK_NAME_DEFAULT
#define PTHREAD_TASK_NAME_DEFAULT "pthread"
#endif

#ifndef PTHREAD_TASK_STACK_PRIORITY_DEFAULT
#define PTHREAD_TASK_STACK_PRIORITY_DEFAULT (30)
#endif

#ifndef PTHREAD_TASK_STACK_SIZE_DEFAULT
#define PTHREAD_TASK_STACK_SIZE_DEFAULT (512)
#endif

#ifndef PTHREAD_MQ_MAX_MSG_DEFAULT
#define PTHREAD_MQ_MAX_MSG_DEFAULT (10)
#endif

#ifndef PTHREAD_MQ_MSG_SIZE_DEFAULT
#define PTHREAD_MQ_MSG_SIZE_DEFAULT (16)
#endif

#ifndef PTHREAD_NAME_LEN_DEFAULT
#define PTHREAD_NAME_LEN_DEFAULT (16)
#endif

#ifndef PTHREAD_EXIT_QUEUE_SIZE_DEFAULT
#define PTHREAD_EXIT_QUEUE_SIZE_DEFAULT (16)
#endif

#ifndef PTHREAD_EXIT_TASK_STACK_SIZE_DEFAULT
#define PTHREAD_EXIT_TASK_STACK_SIZE_DEFAULT (256)
#endif

#define PTHREAD_COND_INITIALIZER    {0}
#define PTHREAD_MUTEX_INITIALIZER   {0}
#define PTHREAD_CREATE_DETACHED     (1)


// app task priority of macro: begin  priority is higher if value is bigger.
#define PTHREAD_TASK_LOWESTPRI                  0
#define PTHREAD_TASK_1_PRI                      1
#define PTHREAD_TASK_2_PRI                      2
#define PTHREAD_TASK_3_PRI                      3
#define PTHREAD_TASK_4_PRI                      4
#define PTHREAD_TASK_5_PRI                      5
#define PTHREAD_TASK_6_PRI                      6
#define PTHREAD_TASK_7_PRI                      7
#define PTHREAD_TASK_8_PRI                      8
#define PTHREAD_TASK_9_PRI                      9
#define PTHREAD_TASK_10_PRI                     10
#define PTHREAD_TASK_11_PRI                     11
#define PTHREAD_TASK_12_PRI                     12
#define PTHREAD_TASK_13_PRI                     13
#define PTHREAD_TASK_14_PRI                     14
#define PTHREAD_TASK_15_PRI                     15
#define PTHREAD_TASK_16_PRI                     16
#define PTHREAD_TASK_17_PRI                     17
#define PTHREAD_TASK_18_PRI                     18
#define PTHREAD_TASK_19_PRI                     19
#define PTHREAD_TASK_20_PRI                     20
#define PTHREAD_TASK_21_PRI                     21
#define PTHREAD_TASK_22_PRI                     22
#define PTHREAD_TASK_23_PRI                     23
#define PTHREAD_TASK_24_PRI                     24
#define PTHREAD_TASK_25_PRI                     25
#define PTHREAD_TASK_26_PRI                     26
#define PTHREAD_TASK_27_PRI                     27
#define PTHREAD_TASK_28_PRI                     28
#define PTHREAD_TASK_29_PRI                     29
#define PTHREAD_TASK_30_PRI                     30
#define PTHREAD_TASK_HIGHEST_PRI                31

//end


typedef unsigned int uint32_t;
typedef unsigned long size_t;
typedef int pthread_t;
typedef OS_MUTEX pthread_mutex_t;
typedef void pthread_mutexattr_t;
typedef OS_SEM pthread_cond_t;
typedef void pthread_condattr_t;
typedef int  mqd_t;

typedef struct {
    void *(*start_routine)(void *);
    void *arg;
    int pid;
    char task_name[PTHREAD_NAME_LEN_DEFAULT];
} phread_args;

typedef struct pthread_attr {
    uint16_t stacksize;
    uint16_t priority;
} pthread_attr_t;

struct sched_param {
    int sched_priority;
};

struct mq_attr {
    long mq_maxmsg;
    long mq_msgsize;
};

struct mq_msg {
    char *msg_ptr;
    size_t msg_len;
};

#define O_EXEC      0x1000
#define O_RDONLY    0x2000
#define O_RDWR      0xA000
#define O_SEARCH    0x4000
#define O_WRONLY    0x8000
#define O_CREAT     0x0002


#define NS_PER_S 1000000000
#define COUNT 1000


int pthread_create(pthread_t *thread,
                   const pthread_attr_t *attr,
                   void *(*startroutine)(void *),
                   void *arg);

int pthread_cancel(pthread_t thread);

int pthread_attr_init(pthread_attr_t *attr);

int pthread_attr_setschedparam(pthread_attr_t *attr,
                               const struct sched_param *param);


int pthread_attr_setstacksize(pthread_attr_t *attr,
                              size_t stacksize);

int pthread_attr_setdetachstate(pthread_attr_t *attr,
                                int detachstate);

int pthread_equal(pthread_t t1,
                  pthread_t t2);

pthread_t pthread_self(void);

int pthread_mutex_init(pthread_mutex_t *mutex,
                       const pthread_mutexattr_t *attr);

int pthread_mutex_lock(pthread_mutex_t *mutex);

int pthread_mutex_unlock(pthread_mutex_t *mutex);


int pthread_cond_init(pthread_cond_t *cond,
                      const pthread_condattr_t *attr);

int pthread_cond_signal(pthread_cond_t *cond);

int pthread_cond_destroy(pthread_cond_t *cond);

int pthread_cond_broadcast(pthread_cond_t *cond);

int pthread_cond_wait(pthread_cond_t *cond,
                      pthread_mutex_t *mutex);

#if 0 // TODO: Check this
int pthread_cond_timedwait(pthread_cond_t *cond,
                           pthread_mutex_t *mutex,
                           const struct timspec *abstime);
#endif

mqd_t mq_open(const char *name,
              int oflag,
              mode_t mode,
              struct mq_attr *attr);

int mq_send(mqd_t mqdes,
            const char *msg_ptr,
            size_t msg_len,
            unsigned msg_prio);

ssize_t mq_receive(mqd_t mqdes,
                   char *msg_ptr,
                   size_t msg_len,
                   unsigned int *msg_prio);

ssize_t mq_timedreceive(mqd_t mqdes,
                        char *msg_ptr,
                        size_t msg_len,
                        unsigned int *msg_prio,
                        const struct timespec *abstime);

int mq_close(mqd_t mqdes);

int mq_getattr(mqd_t mqdes,
               struct mq_attr *mqstat);

int os_task_create_static(void (*task_func)(void *p_arg),
                          void *p_arg,
                          u8 prio,
                          u32 stksize,
                          int qsize,
                          const char *name,
                          u8 *tcb_stk_q,
                          u8 core);

#endif
