/* Copyright (C) 2023 UNISOC Technologies Limited and/or its
 * affiliates("UNISOC").
 * All rights reserved.
 *
 * This software is supplied "AS IS" without any warranties.
 * UNISOC assumes no responsibility or liability for the use of the software,
 * conveys no license or title under any patent, copyright, or mask work
 * right to the product. UNISOC reserves the right to make changes in the
 * software without notification.  UNISOC also make no representation or
 * warranty that such application will be suitable for the specified use
 * without further testing or modification.
 *
 *This file is external file of netif,describe and list APIs  that used to
 *transfer data between MCU and MODEM
*/

#ifndef __NETIF_CMUX_H__
#define __NETIF_CMUX_H__
#include "uos_type.h"
#include "ual_tele_data_export.h"
#include "ual_common.h"
#include "ual_cms.h"
#include "err.h"

#define JL_SUPPORT
#ifndef JL_SUPPORT

/*
* @ brief:             Create NETIF for lwip using CMUX
* @param[in] dual_sys sim card number
* @param[in] nCid     nCid
* @param[in] netif_para  NET parameters for this NETIF
* @return              0 true, -1 false
*/
uos_int32_t TCPIP_netif_create(uint8_t nSimId, uos_uint8_t nCid, ual_tele_data_pdp_net_para *netif_para);

/*
* @ brief:             destroy NETIF for lwip using CMUX
* @param[in] dual_sys sim card number
* @param[in] nCid     nCid
* @return              none
*/
void TCPIP_netif_destory(uint8_t nSimId, uint8_t nCid);

/*
* @ brief: get netif that  "TCPIP_netif_create" created
* @param[in] void
* @return struct netif*
*/
struct netif *get_netif(void);

//add for JL

#else

/*
 * @ brief:             init NETIF ,especially open CMUX channel
 * @param[in] mux_net_id  the channel id for cmux
 * @return              0 true, -1 false
 */
int TCPIP_netif_init(uint32 mux_net_id);

/*
* @ brief:             TX func that usbd for sending data to MODEM from MCU
* @param[in] Data data ptr
* @param[in] data_len     data length
* @return              0 true, other false
*/

err_t TCPIP_data_output(uos_uint8_t *Data, int data_len);

/*
* @ brief:            recv data that MODEM send to MCU
* @param[in] readed_len  received data  length
* @return             uos_uint8_t * data ptr
*/
uos_uint8_t *TCPIP_data_input(uint32 *readed_len);

/*
* @ brief:   realease data ptr that "TCPIP_data_input" return ptr
* @param[in] pdata "TCPIP_data_input" return ptr
* @return              0 true, other false
*/
int TCPIP_data_realease(uint8 *pdata);

/*
* @ brief:            get mac for lwip neitf used
* @param[in] void
* @return              uos_uint8_t * global mac array ptr
*/
uos_uint8_t *TCPIP_netif_get_mac(void);

/*
* @ brief:            close CMUX channel
* @param[in] void
* @return             void
*/
void TCPIP_netif_close(void);
#endif
#endif
