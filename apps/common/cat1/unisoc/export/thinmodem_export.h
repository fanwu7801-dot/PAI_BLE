/* service_base.h */

#ifndef _SERVICE_BASE_H_
#define _SERVICE_BASE_H_

#include "uos_deval.h"
#include "uos_osal.h"
#include "uos_type.h"

#undef LOG_TAG
#define LOG_TAG "thinmodem-service"
#define LOG_LVL LOG_LVL_DBG
#ifndef ENABLE_MODEM_SDK_DEBUG_LOG
#define ENABLE_MODEM_SDK_DEBUG_LOG
#endif
#ifdef ENABLE_MODEM_SDK_DEBUG_LOG
#define THINMODEM_LOG(format,param_num, ...) UOS_LOG_INFO("THINMODEM_TASK:"format"\r\n",param_num, ##__VA_ARGS__)
#else
#define THINMODEM_LOG(...)
#endif
typedef int32_t (*thinmodem_service_register_cb)(void);
typedef enum {
    RPC_EVENT_ID_START = 0,
    RPC_UAL_INIT_READY_EVENT_ID,    //ual init ready,notice mcu to register tele service
    RPC_UAL_MSG_EVENT_ID,           //ual cms used
    RPC_LPA_MSG_EVENT_ID,           //lpa used
    RPC_EVENT_ID_MAX
} THINMODEM_RPC_EVENT_ID;

/*****************************************************************************/
// Description:register callback to thinmodem service,
//             this callback indicate all thinmodem service is ok,
//             in this callback, can register tele,gnss service
// Parameter: [In] none
//            [Out] none
//            [Return]none
//  Author:xiuyun.wang
//  Note:
/*****************************************************************************/
void thinmodem_register_callback(thinmodem_service_register_cb callback_fun);

uint32_t thinmodem_handle_rpc_status(uint32_t rpc_status);

#endif
