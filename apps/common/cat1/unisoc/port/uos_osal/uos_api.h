#ifndef __UOS_API_H_
#define __UOS_API_H_

#include "uos_type.h"
#include "uos_osal.h"
#include "uos_deval.h"

#ifdef  __cplusplus
extern "C"
{
#endif

/* typedef int mqd_t; */
/* typedef int sem_t; */
/* typedef int pthread_mutex_t; */
/* typedef int Timer_t; */
/* typedef int pthread_t; */

#ifndef PTHREAD_EXIT_TASK_STACK_SIZE_DEFAULT
#define PTHREAD_EXIT_TASK_STACK_SIZE_DEFAULT  (256)
#endif

#ifndef PTHREAD_NAME_LEN_DEFAULT
#define PTHREAD_NAME_LEN_DEFAULT  (32) //debug Tianyu.Yang
#endif

#ifndef PTHREAD_EXIT_QUEUE_SIZE_DEFAULT
#define PTHREAD_EXIT_QUEUE_SIZE_DEFAULT  (16)
#endif

#ifndef UOS_NULL
#define UOS_NULL NULL
#endif

typedef struct {
    void *(*start_routine)(void *);
    void *arg;
    int tid;
    char task_name[PTHREAD_NAME_LEN_DEFAULT];
} thread_param;

typedef struct {
    char *data;
    size_t data_size;
} queue_element;

typedef struct {
    void (*timer_cb)(void *param);
    void *param;
    unsigned int period;
    unsigned int option;
    unsigned short id;
    unsigned char is_active;
} timer_ctrl;

#ifdef  __cplusplus
}
#endif

#endif
