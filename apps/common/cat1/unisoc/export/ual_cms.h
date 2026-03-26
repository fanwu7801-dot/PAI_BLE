/*****************************************************************************
** Copyright 2022 Unisoc(Shanghai) Technologies Co.,Ltd                      *
** Licensed under the Unisoc General Software License,                       *
** version 1.0 (the License);                                                *
** you may not use this file except in compliance with the License.          *
** You may obtain a copy of the License at                                   *
** https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US    *
** Software distributed under the License is distributed on an "AS IS" BASIS,*
** WITHOUT WARRANTIES OF ANY KIND, either express or implied.                *
** See the Unisoc General Software License, version 1.0 for more details.    *
******************************************************************************/

/*****************************************************************************
** File Name:      ual_cms.h
** Author: Xiuyun.Wang
** Description:
*****************************************************************************/

#ifndef _UAL_CMS_H_
#define _UAL_CMS_H_
/**--------------------------------------------------------------------------*
**                         Include Files                                    *
**--------------------------------------------------------------------------*/

#include "ual_common.h"
#include "uos_type.h"
#include "uos_deval.h"
/*---------------------------------------------------------------------------*/
/*                          MACRO DEFINITION                                 */
/*---------------------------------------------------------------------------*/
#define CMS_LOG(format, ...) UOS_LOG_INFO(format"\r\n", ##__VA_ARGS__)


/*---------------------------------------------------------------------------*/
/*                         TYPE AND CONSTANT                                 */
/*---------------------------------------------------------------------------*/
typedef struct {
    _ual_client_register_callback callback;
    uint32      msg_id;
    uint32      body_size;
    void       *p_body;
} ual_cms_rpc_event_t;

/**--------------------------------------------------------------------------*
 **                         FUNCTION DECLARE                                 *
 **--------------------------------------------------------------------------*/

/*****************************************************************************/
//  Description : dispatch the meesage to every module
//  Parameter: [In] p_dispatch_signal: the signal for cms dispatch
//             [Out] NONE
//             [Return]NONE
//  Author:xiuyun.wang
// Note:
/*****************************************************************************/
void ual_cms_handle_rpc_msg(uint32  p_dispath_body);

#endif
