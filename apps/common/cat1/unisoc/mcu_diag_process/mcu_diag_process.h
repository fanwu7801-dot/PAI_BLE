#ifndef _MCU_DIAG_PROC_H
#define _MCU_DIAG_PROC_H

#include "uos_type.h"
#ifdef __cplusplus
extern "C" {
#endif

//#define MCU_DIAG_TEST
#define ENABLE_MCUDIAG_DEBIG_LOG

#ifdef ENABLE_MCUDIAG_DEBIG_LOG
#define MCUDIAG_LOG(fmt, ...)   UOS_LOG_INFO("[MCUDIAG]" fmt, ##__VA_ARGS__)
#else
#define MCUDIAG_LOG(...)
#endif

BOOLEAN diag_channel_open(void);

BOOLEAN diag_channel_closed(void);

int mcu_diag_init(void);

#ifdef __cplusplus
}
#endif


#endif /*_MCU_DIAG_PROC_H*/



