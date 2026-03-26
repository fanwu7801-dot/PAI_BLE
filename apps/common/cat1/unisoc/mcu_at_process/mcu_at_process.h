#ifndef _MCU_AT_PROC_H
#define _MCU_AT_PROC_H

#include "uos_osal.h"
#include "uos_type.h"
#ifdef __cplusplus
extern "C" {
#endif

//#define MCU_AT_TEST
#define ENABLE_MCUAT_DEBIG_LOG

#ifdef ENABLE_MCUAT_DEBIG_LOG
#define MCUAT_LOG(fmt, ...)   UOS_LOG_INFO("[MCUAT]" fmt, ##__VA_ARGS__)
#else
#define MCUAT_LOG(...)
#endif

extern uos_queue_t *dongle_at_queue;
extern uos_queue_attr_t *dongle_at_attr;

int mcu_at_init(void);

#ifdef __cplusplus
}
#endif


#endif /*_MCU_AT_PROC_H*/
