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

/*******************************************************************************
** File Name:       ual_tele_data_export.h                                     *
** Author:            shuting.ma                                               *
** Date:             07/09/2023                                                *
** Description:    This file is used to define ual data export api             *
********************************************************************************
**                         Important Edit History                              *
** ----------------------------------------------------------------------------*
** DATE                 NAME                  DESCRIPTION                      *
** 07/09/2023           shuting.ma              Create                         *
********************************************************************************/
#ifndef _UAL_TELE_DATA_EXPORT_H_
#define _UAL_TELE_DATA_EXPORT_H_

#include "ual_tele_common.h"

/**----------------------------------------------------------------------------*
 **                         Include Files                                      *
 **----------------------------------------------------------------------------*/

/**----------------------------------------------------------------------------*
 **                         MACRO DEFINITION                                   *
 **----------------------------------------------------------------------------*/
#define UAL_TELE_DATA_CARRIER_APN_MAX_LEN         0x20   //接入点的最大长度
#define UAL_TELE_DATA_CARRIER_USERNAME_MAX_LEN    0x20   //用户名的最大长度
#define UAL_TELE_DATA_CARRIER_PASSWORD_MAX_LEN    0x20   //密码的最大长度
#define UAL_TELE_DATA_IPV6_LEN                    16     //IPV6地址的长度
/**----------------------------------------------------------------------------*
**                         TYPE DEFINITION                                     *
**-----------------------------------------------------------------------------*/
typedef enum {
    UAL_TELE_DATA_RES_SUCCESS = UAL_TELE_RES_SUCCESS,
    UAL_TELE_DATA_RES_ERROR = UAL_MODULE_TELE_DATA << 16,
    UAL_TELE_DATA_RES_PARAM_ERROR,
    UAL_TELE_DATA_RES_MAX
} ual_tele_data_result_e;

typedef enum {
    MSG_UAL_TELE_DATA_ACTIVE_CNF = UAL_MODULE_TELE_DATA << 16,      //client请求激活
    MSG_UAL_TELE_DATA_DEACTIVE_CNF,    //client请求去激活
    MSG_UAL_TELE_DATA_DEACTIVE_IND,    //主动上报去激活
    MSG_UAL_TELE_DATA_END,
} ual_tele_data_msg_e;

typedef enum {
    UAL_TELE_DATA_ROAMING_DISABLE,     //禁用数据漫游
    UAL_TELE_DATA_ROAMING_ENABLE,      //启用数据漫游
    UAL_TELE_DATA_ROAMING_MAX
} ual_tele_data_roaming_mode_e;

typedef enum {
    UAL_TELE_DATA_AUTH_TYPE_PAP = 0,           //PAP类型
    UAL_TELE_DATA_AUTH_TYPE_CHAP = 1,          //CHAP类型
    UAL_TELE_DATA_AUTH_TYPE_PAP_CHAP = 2,      //CHAP或PAP类型
    UAL_TELE_DATA_AUTH_TYPE_NONE = 3,          //无认证类型
    UAL_TELE_DATA_AUTH_TYPE_MAX
} ual_tele_data_auth_type_e;

typedef enum {
    UAL_TELE_DATA_IP_V4,             //Ipv4
    UAL_TELE_DATA_IP_V6,             //Ipv6
    UAL_TELE_DATA_IP_V4V6,           //Ipv4或Ipv6
    UAL_TELE_DATA_IP_MAX
} ual_tele_data_ip_type_e;

typedef struct {
    char  apn[UAL_TELE_DATA_CARRIER_APN_MAX_LEN + 1];            //接入点名称
    char  username[UAL_TELE_DATA_CARRIER_USERNAME_MAX_LEN + 1];	 //用户名
    char  password[UAL_TELE_DATA_CARRIER_PASSWORD_MAX_LEN + 1];  //密码
    ual_tele_data_auth_type_e auth_type;                         //认证类型
    ual_tele_data_ip_type_e ip_type;                             //ip type
} ual_tele_data_apn_info_t;

typedef struct {
    boolean is_ip4;                                //是否支持Ipv4
    boolean is_ip6;                                //是否支持Ipv6
    uint32  ipaddr;                                //Ipv4地址
    uint32  snmask;                                //Ipv4子网掩码
    uint32  dns1;                                  //Ipv4域名服务器1
    uint32  dns2;                                  //Ipv4域名服务器2
    uint32  gateway;                               //Ipv4网关地址
    uint8   ip6addr[UAL_TELE_DATA_IPV6_LEN];       //Ipv6地址
    uint8   dns6_pri[UAL_TELE_DATA_IPV6_LEN];      //Ipv6域名服务器1
    uint8   dns6_sec[UAL_TELE_DATA_IPV6_LEN];      //Ipv6域名服务器2
    uint32  net_id;                                //nsapi,提供给DAPS
} ual_tele_data_net_para_t;

typedef enum {
    UAL_TELE_DATA_ERR_SUCCESS,                          //操作成功
    UAL_TELE_DATA_ERR_AUTHENTICATION_FAILED,            //认证失败
    UAL_TELE_DATA_ERR_PDP_INFO_RELATED,                 //与PDP信息相关的错误，如PDP类型、地址、Nsapi等
    UAL_TELE_DATA_ERR_ACTIVE_REJECT,                    //PDP激活被拒绝
    UAL_TELE_DATA_ERR_APN_RELATED,                      //与APN相关的错误
    UAL_TELE_DATA_ERR_PROTOCOL_ERRORS,                  //协议错误
    UAL_TELE_DATA_ERR_NEED_FALLBACK,                    //信息不完全的重试，需要进行查询后再重试
    UAL_TELE_DATA_ERR_RETRING,                          //可以间隔较短时间再尝试激活PDP(为UAL PDP内部使用)
    UAL_TELE_DATA_ERR_QOS_NOT_ACCEPTED,                 //Qos参数不当
    UAL_TELE_DATA_ERR_PDP_ATTACH_ABORT,                 //PDP attach被终止
    UAL_TELE_DATA_ERR_POWER_DOWN,                       //掉电
    UAL_TELE_DATA_ERR_IPV4_ONLY_ALLOWED,                //只允许IPV4类型
    UAL_TELE_DATA_ERR_IPV6_ONLY_ALLOWED,                //只允许IPV6类型
    UAL_TELE_DATA_ERR_DETACHED,                         //被deattach
    UAL_TELE_DATA_ERR_NEED_RETRY,                       //需要应用进行重试
    UAL_TELE_DATA_ERR_ACTIVE_INTERFACE_RETURN_FAIL,     //调用激活接口返回失败
    UAL_TELE_DATA_ERR_TIMEOUT,                          //激活PDP超时
    UAL_TELE_DATA_ERR_NOT_FERMIT,                       //不允许使用网络，如打开飞行、漫游模式等
    UAL_TELE_DATA_ERR_UNSPECIFIED,                       //未指定错误，可通过调用接口获取具体错误码
    UAL_TELE_DATA_ERR_MAX
} ual_tele_data_err_e;

typedef struct {
    ual_tele_sim_id_e              sim_id;
    ual_tele_data_err_e            result;
    ual_tele_data_net_para_t       net_para;
    uint32                         reserved; //保留位，用于业务拓展数据
} ual_tele_data_active_cnf_t;

typedef struct {
    ual_tele_sim_id_e   sim_id;
    ual_tele_data_err_e result;
} ual_tele_data_deactive_cnf_t;


typedef struct {
    ual_tele_sim_id_e   sim_id;
    ual_tele_data_err_e result;
} ual_tele_data_deactive_ind_t;

typedef struct {
    boolean  is_ip4;
    boolean  is_ip6;
    uint32   net_id;
    uint32   mux_net_id;
    uint32   ipaddr;
    uint32   snmask;
    uint32   dns1;
    uint32   dns2;
    uint32   gateway;
    uint8    ip6addr[UAL_TELE_DATA_IPV6_LEN];
    uint8    dns6_pri[UAL_TELE_DATA_IPV6_LEN];
    uint8    dns6_sec[UAL_TELE_DATA_IPV6_LEN];
} ual_tele_data_pdp_net_para;
/**----------------------------------------------------------------------------*
**                         MACRO DEFINITION                                    *
**-----------------------------------------------------------------------------*/

/*******************************************************************************/
//	Description: ual_tele_data_register
//	Parameter: [In] p_callback private call back
//	           [Out] p_handle out of phandle
//	           [Return] ual_tele_data_result_e
//	Author: shuting.ma
//	Note:
/*******************************************************************************/
ual_tele_data_result_e ual_tele_data_register(_ual_client_register_callback p_callback, uint32 *p_handle, int *p_ret);

/*******************************************************************************/
//	Description: ual_tele_data_unregister
//	Parameter: [in] handle
//	           [Out] none
//	           [Return] ual_tele_data_result_e
//	Author: shuting.ma
//	Note:
/*******************************************************************************/
ual_tele_data_result_e ual_tele_data_unregister(uint32 handle, int *p_ret);

/*******************************************************************************/
//  Description : 打开蜂窝数据业务
//  Parameter: [In] sim_id sim卡ID
//             [Out] none
//             [Return]ual_tele_data_result_e
//  Author: shuting.ma
//  Note: 激活蜂窝数据端的pdp链接
/*******************************************************************************/
ual_tele_data_result_e ual_tele_data_active(ual_tele_sim_id_e sim_id, int *p_ret);

/*******************************************************************************/
//  Description : 关闭蜂窝数据业务
//  Parameter: [In] sim_id sim卡ID
//             [Out] none
//             [Return]ual_tele_data_result_e
//  Author: shuting.ma
//  Note: 去激活蜂窝数据端的pdp链接
/*******************************************************************************/
ual_tele_data_result_e ual_tele_data_deactive(ual_tele_sim_id_e sim_id, int *p_ret);

/*******************************************************************************/
//  Description : 蜂窝数据业务是否激活
//  Parameter: [In] sim_id sim卡ID
//             [Out] none
//             [Return]ual_tele_data_result_e
//  Author: shuting.ma
//  Note:
/*******************************************************************************/
boolean ual_tele_data_get_is_active(ual_tele_sim_id_e sim_id, int *p_ret);

/*******************************************************************************/
//  Description : 切换为用户所选的sim卡id
//  Parameter: [In] sim_id
//             [Out] none
//             [Return]ual_tele_sim_id_e
//  Author: shuting.ma
//  Note:
/*******************************************************************************/
ual_tele_data_result_e ual_tele_data_set_current_data_card(ual_tele_sim_id_e  sim_id, int *p_ret);

/*******************************************************************************/
//  Description : 获取当前使用
//  Parameter: [In] none
//             [Out] none
//             [Return]ual_tele_sim_id_e
//  Author: shuting.ma
//  Note:
/*******************************************************************************/
ual_tele_sim_id_e ual_tele_data_get_current_data_card(int *p_ret);

/*******************************************************************************/
//  Description : 设置internet类型APN，并激活该APN类型的pdp链接
//  Parameter: [In] sim_id: sim卡ID
//                  p_apn_info: 更新后的apn信息
//             [Out] none
//             [Return]ual_tele_data_result_e
//  Author: shuting.ma
//  Note:该接口仅在internet类型apn信息有更新时调用
/*******************************************************************************/
ual_tele_data_result_e ual_tele_data_connect_apn(ual_tele_sim_id_e sim_id, ual_tele_data_apn_info_t *p_new_apn_info, int *p_ret);

/*******************************************************************************/
//  Description :获取数据漫游模式，只有开启改模式才可以激活PDP
//  Parameter: [In] sim_id: sim卡ID
//             [Out] none
//             [Return] data_roaming_mode: roaming status(enable/disable)
//  Author:shuting.ma
//  Note:该接口与radio中漫游模式不同
/*******************************************************************************/
ual_tele_data_roaming_mode_e ual_tele_data_get_roaming_mode(ual_tele_sim_id_e sim_id, int *p_ret);
/*******************************************************************************/
//  Description :设置数据漫游模式，只有开启改模式才可以激活PDP
//  Parameter: [In] sim_id: sim卡ID
//                  data_roaming_mode: roaming status(enable/disable)
//             [Out] none
//             [Return]ual_tele_data_result_e
//  Author:shuting.ma
//  Note:该接口与radio中漫游模式不同
/*******************************************************************************/
ual_tele_data_result_e ual_tele_data_set_roaming_mode(ual_tele_sim_id_e sim_id, ual_tele_data_roaming_mode_e data_roaming_mode, int *p_ret);


#endif
