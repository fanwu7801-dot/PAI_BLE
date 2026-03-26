#ifndef _UNISOC_AT_H
#define _UNISOC_AT_H

#include "uos_type.h"
#include "cmux_api.h"
#ifdef __cplusplus
extern "C" {
#endif

#define AT_RESPONSE_AYYAY_MAX_NUM    10

/*****************************************************************************/
//  Description :开启cmux at channel
//  Parameter: [In] NONE
//            [Return]BOOLEAN
//  Author:yang.li3
//  Note:
/*****************************************************************************/
BOOLEAN at_channel_open(void);

/*****************************************************************************/
//  Description :关闭cmux at channel
//  Parameter: [In] NONE
//            [Return]BOOLEAN
//  Author:yang.li3
//  Note:
/*****************************************************************************/
BOOLEAN at_channel_closed(void);

/*****************************************************************************/
//  Description :发送at指令并同步获取第一条at回应结果
//  Parameter: [In] at(要有结束符在最后一位)
//            [In] response
//            [Out] response
//            [Out] response_len
//            [Return]BOOLEAN
//  Author:yang.li3
//  Note: response must be free(use at_response_free)
/*****************************************************************************/
BOOLEAN at_send_sync(uint8 *at, uint8 **response, uint32 *response_len);

/*****************************************************************************/
//  Description :发送at指令并同步获取所有at回应结果
//  Parameter: [In] at(要有结束符在最后一位)
//            [In] response_array
//            [Out] response_array
//            [Out] response_array_num
//            [Return]BOOLEAN
//  Author:yang.li3
//  Note: each element in response_array must be free(use at_response_free_array)
/*****************************************************************************/
BOOLEAN at_send_sync_response_all(uint8 *at, uint8 *response_array[AT_RESPONSE_AYYAY_MAX_NUM], uint32 *response_array_num);


void at_response_free(void *response);

void at_response_free_array(uint8 *response_array[AT_RESPONSE_AYYAY_MAX_NUM]);


#ifdef __cplusplus
}
#endif


#endif /*_UNISOC_AT_H*/

