/*****************************************************************************
** Copyright 2023 Unisoc(Shanghai) Technologies Co.,Ltd                      *
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
** File Name:       ual_tele_sms_export.h                                             *
** Author:          yang.wang3                                                 *
** Date:             03/01/2023                                                *
** Description:    This file is used to define ual tele sms export api         *
********************************************************************************
**                         Important Edit History                              *
** ----------------------------------------------------------------------------*
** DATE                 NAME                  DESCRIPTION                      *
** 03/01/2023          yang.wang3               Create                         *
********************************************************************************/
#ifndef _UAL_TELE_SMS_EXPORT_H_
#define _UAL_TELE_SMS_EXPORT_H_

/**--------------------------------------------------------------------------*
 **                         Include Files                                    *
 **--------------------------------------------------------------------------*/
#include "ual_tele_common.h"

/**--------------------------------------------------------------------------*
 **                         Compiler Flag                                    *
 **--------------------------------------------------------------------------*/
#ifdef __cplusplus
extern   "C"
{
#endif

/**--------------------------------------------------------------------------*
 **                         MACRO DEFINITION                                 *
 **--------------------------------------------------------------------------*/
#define UAL_TELE_SMS_DEST_ADDR_MAX_NUM  10  //the group numbers of destination address
#define UAL_TELE_SMS_NUMBER_MAX_LEN     40  //the length of destination address
#define UAL_TELE_SMS_SPLIT_MAX_NUM      4   //the max numbers of long sms
#define UAL_TELE_SMS_MAX_DEF_BYTE_LEN   160 // the length of default normal SMS
#define UAL_TELE_SMS_CONTENT_MAX_LEN    (UAL_TELE_SMS_SPLIT_MAX_NUM*UAL_TELE_SMS_MAX_DEF_BYTE_LEN)

#define UAL_TELE_SMS_PDU_SIZE           (251 * 2)
/**--------------------------------------------------------------------------*
**                         TYPE DEFINITION                                   *
**---------------------------------------------------------------------------*/
#ifndef wchar
typedef unsigned short wchar;
#endif

/*---------------------------------------------------------------------------*
**                         ENUM AND STRUCT                                  *
**---------------------------------------------------------------------------*/

//API result
typedef enum {
    UAL_TELE_SMS_RES_SUCCESS = UAL_TELE_RES_SUCCESS,
    UAL_TELE_SMS_RES_SYSTEM_ERROR = UAL_MODULE_TELE_SMS << 16,     //系统错误
    UAL_TELE_SMS_RES_SYSTEM_BUSY,       //UAL层处于忙碌状态
    UAL_TELE_SMS_RES_STORAGE_ERROR,     //存储位置错误
    UAL_TELE_SMS_RES_ME_MEM_FULL,       //手机存储区已满
    UAL_TELE_SMS_RES_SIM_MEM_FULL,      //SIM卡存储区已满
    UAL_TELE_SMS_RES_NO_SPACE,          //没有存储空间
    UAL_TELE_SMS_RES_SIM_NOT_READY,     //SIM卡未完成初始化
    UAL_TELE_SMS_RES_PARAM_ERROR,       //参数错误
    UAL_TELE_SMS_RES_SEND_ADDR_ERROR,   //发送端号码错误
    UAL_TELE_SMS_RES_DEST_ADDR_ERROR,   //接收端号码错误
    UAL_TELE_SMS_RES_NOT_EXIST,         //短信不存在
    UAL_TELE_SMS_RES_STATUS_ERROR,      //状态错误
    UAL_TELE_SMS_RES_OTHER_ERROR,       //其他错误
    UAL_TELE_SMS_RES_NOT_SUPPORT,       //功能不支持
    UAL_TELE_SMS_RES_MAX
} ual_tele_sms_result_e;

//短信状态
typedef enum {
    UAL_TELE_SMS_STATUS_FREE_SPACE,     //短信不存在或已删除
    UAL_TELE_SMS_STATUS_MT_READED,      //接收的短信，已读
    UAL_TELE_SMS_STATUS_MT_TO_BE_READ,  //短信未读
    UAL_TELE_SMS_STATUS_MO_SENDED,      //短信已发送
    UAL_TELE_SMS_STATUS_MO_TO_BE_SEND,  //短信未发送
    UAL_TELE_SMS_STATUS_MAX
} ual_tele_sms_status_e;

//UAL 上报的消息
typedef enum {
    MSG_UAL_TELE_SMS_READY_IND = UAL_MODULE_TELE_SMS << 16,     //短信初始化完成上报的消息
    MSG_UAL_TELE_SMS_READ_CNF,      //读取短信
    MSG_UAL_TELE_SMS_DELETE_CNF,    //删除短信
    MSG_UAL_TELE_SMS_RECEIVE_IND,   //接收短信
    MSG_UAL_TELE_SMS_REPORT_IND,    //发送短信的短信报告上报给APP的消息
    MSG_UAL_TELE_SMS_SEND_CNF,      //发送短短信，或者发送完长短信
    MSG_UAL_TELE_SMS_SEND_ONE_CNF,  //短信发送完长短信中的一条短短信，上报的消息
    MSG_UAL_TELE_SMS_SAVE_CNF,      //保持短短信，或者保存完长短信
    MSG_UAL_TELE_SMS_SAVE_ONE_CNF,  //保存长短信中的一条短短信，上报的消息
    MSG_UAL_TELE_SMS_UPDATE_CNF,    //短信状态更新上报的消息
    MSG_UAL_TELE_SMS_MEM_FULL_IND,  //短信存储区满后上报的消息
    MSG_UAL_TELE_SMS_MAX
} ual_tele_sms_msg_e;

//短信的类型MO/MT
typedef enum {
    UAL_TELE_SMS_TYPE_MO,
    UAL_TELE_SMS_TYPE_MT,
    UAL_TELE_SMS_TYPE_MAX
} ual_tele_sms_type_e;

//短信存储位置
typedef enum {
    UAL_TELE_SMS_NO_STORED,
    UAL_TELE_SMS_STORAGE_ME,
    UAL_TELE_SMS_STORAGE_SIM,
    UAL_TELE_SMS_STORAGE_ALL,
    UAL_TELE_SMS_STORAGE_MAX
} ual_tele_sms_storage_e;

//短信存储空间的使用状况
typedef enum {
    UAL_TELE_SMS_ME_FULL,   // ME memory full
    UAL_TELE_SMS_SIM_FULL,  // SIM memory full
    UAL_TELE_SMS_ALL_FULL,  // all memory full
    UAL_TELE_SMS_MEM_AVAIL, // memory available
    UAL_TELE_SMS_MEM_MAX
} ual_tele_sms_mem_status_e;

//定义字符串编码格式
typedef enum {
    UAL_TELE_SMS_ALPHABET_GSM,
    UAL_TELE_SMS_ALPHABET_8BIT,
    UAL_TELE_SMS_ALPHABET_UCS2,
    UAL_TELE_SMS_ALPHABET_MAX
} ual_tele_sms_alphabet_e;

//短信传送状态
typedef enum {
    UAL_TELE_SMS_RECEIVE_BY_REMOTE,     //短信已经到达目的端
    UAL_TELE_SMS_NOT_CONFIRM_RECEIVE,   //短信中心已经发送了短信，但不确定是否到达目的端
    UAL_TELE_SMS_REPLACE_BY_SC,         //短信中心已经替换了该条短信
    UAL_TELE_SMS_SEND_LATER,            //短信尚未发送，稍后再发送该条短信
    UAL_TELE_SMS_NOT_SEND_AGAIN,        //短信发送有错误，以后不会再发送该条短信
    UAL_TELE_SMS_INVALID_STATUS_REPORT, //尚未收到该条短信有效的状态报告
    UAL_TELE_SMS_TRANSFER_STATUS_MAX
} ual_tele_sms_transfer_status_e;

//sms content mode
typedef enum {
    UAL_TELE_SMS_CONTENT_RAW,
    UAL_TELE_SMS_CONTENT_PDU,
    UAL_TELE_SMS_CONTENT_MAX
} ual_tele_sms_content_mode_e;

//sms rec save mode
typedef enum {
    UAL_TELE_SMS_RECV_AND_SAVE,
    UAL_TELE_SMS_RECV_NOT_SAVE,
    UAL_TELE_SMS_RECV_MODE_MAX
} ual_tele_sms_recv_savemode_e;

// sms config
typedef struct {
    ual_tele_sms_content_mode_e content_mode;
    ual_tele_sms_recv_savemode_e recv_save_mode;
} ual_tele_sms_config_t;

//sms param set
typedef struct {
    ual_tele_sms_alphabet_e     alphabet_param;
} ual_tele_sms_param_set_t;

//time stamp struct
typedef struct {
    uint8  year;
    uint8  month;
    uint8  day;
    uint8  hour;
    uint8  minute;
    uint8  second;
    uint8  timezone;//时区
} ual_tele_sms_time_stamp_t;

//多号码记录
typedef struct {
    uint8   number_list_count;//多号码个数
    uint8   number_addr[UAL_TELE_SMS_DEST_ADDR_MAX_NUM][UAL_TELE_SMS_NUMBER_MAX_LEN];
} ual_tele_sms_number_list_t;

//send param
/*
UAL_TELE_SMS_CONTENT_RAW模式下，所有参数都起作用；
UAL_TELE_SMS_CONTENT_PDU模式下，只有sim_id、content、content_len起作用,该模式下content为PDU串
*/
typedef struct {
    ual_tele_sim_id_e           sim_id;
    ual_tele_sms_storage_e      storage_id; //短信存储位置----0表示不存
    ual_tele_sms_number_list_t  dest_address;
    wchar                       content[UAL_TELE_SMS_CONTENT_MAX_LEN];
    uint16                      content_len;
    boolean                     is_report; //是否需要发送报告
    uint32                      user_data;
} ual_tele_sms_send_param_t;

//delete param
typedef struct {
    ual_tele_sim_id_e       sim_id;
    ual_tele_sms_storage_e  storage_id;
    uint16                  record_id;
    uint32                  user_data;
} ual_tele_sms_delete_param_t;

//update param
typedef struct {
    ual_tele_sim_id_e       sim_id;
    ual_tele_sms_storage_e  storage_id;
    uint16                  record_id;
    ual_tele_sms_status_e   update_status;
    uint32                  user_data;
} ual_tele_sms_update_param_t;

//read param
typedef struct {
    ual_tele_sim_id_e       sim_id;
    ual_tele_sms_storage_e  storage_id;
    uint16                  record_id;
    uint32                  user_data;
} ual_tele_sms_read_param_t;

//save param
typedef struct {
    ual_tele_sim_id_e           sim_id;
    ual_tele_sms_storage_e      storage_id;
    ual_tele_sms_number_list_t  dest_address;
    wchar                       content[UAL_TELE_SMS_CONTENT_MAX_LEN];
    uint16                      content_len;
    ual_tele_sms_time_stamp_t   sms_time;
    ual_tele_sms_status_e       sms_status;
    uint32                      user_data;
} ual_tele_sms_save_param_t;


//以下为消息载荷结构体
//MSG_UAL_TELE_SMS_READY_IND 的消息载荷结构体
typedef struct {
    ual_tele_sms_result_e   result;
    ual_tele_sim_id_e       sim_id;
} ual_tele_sms_ready_ind_t;

//MSG_UAL_TELE_SMS_READ_CNF 的消息载荷结构体
typedef struct {
    ual_tele_sms_result_e   result;
    ual_tele_sim_id_e       sim_id;
    ual_tele_sms_storage_e  storage_id;
    uint16                  record_id;
    uint8                   sender_address[UAL_TELE_SMS_NUMBER_MAX_LEN];
    uint8                   dest_address[UAL_TELE_SMS_NUMBER_MAX_LEN];
    uint16                  content[UAL_TELE_SMS_CONTENT_MAX_LEN];
    uint16                  content_len;
    uint32                  time;
    ual_tele_sms_type_e     sms_type;//短信类型----MO/MT/REPORT
    ual_tele_sms_status_e   sms_status;//短信状态----已读、未读、未发送、发送失败、已发送
    uint16                  head_ref;//短信的标识，做是否为同一条长短信使用，若不是长短信，此值为0
    uint8                   longsms_max_num;
    uint8                   longsms_seq_num;
    uint32                  user_data;
} ual_tele_sms_read_cnf_t;

//MSG_UAL_TELE_SMS_DELETE_CNF 的消息载荷结构体
typedef struct {
    ual_tele_sms_result_e   result;
    ual_tele_sim_id_e       sim_id;
    ual_tele_sms_storage_e  storage_id;
    uint16                  record_id;
    uint32                  user_data;
} ual_tele_sms_delete_cnf_t;

//MSG_UAL_TELE_SMS_UPDATE_CNF 的消息载荷结构体
typedef struct {
    ual_tele_sms_result_e   result;
    ual_tele_sim_id_e       sim_id;
    ual_tele_sms_storage_e  storage_id;
    uint16                  record_id;
    uint32                  user_data;
} ual_tele_sms_update_cnf_t;

//MSG_UAL_TELE_SMS_RECEIVE_IND 的消息载荷结构体
typedef struct {
    ual_tele_sms_result_e   result;
    ual_tele_sim_id_e       sim_id;
    ual_tele_sms_storage_e  storage_id;
    uint16                  record_id;
    uint8                   sender_address[UAL_TELE_SMS_NUMBER_MAX_LEN];
    uint16                  content[UAL_TELE_SMS_CONTENT_MAX_LEN];
    uint16                  content_len;
    uint32                  time;
    uint16                  head_ref;//短信的标识，做是否为同一条长短信使用，若不是长短信，此值为0
    uint8                   longsms_max_num;
    uint8                   longsms_seq_num;
} ual_tele_sms_receive_ind_t;

//pdu模式 MSG_UAL_TELE_SMS_RECEIVE_IND 的消息载荷结构体
typedef struct {
    ual_tele_sms_result_e   result;
    ual_tele_sim_id_e       sim_id;
    char                    sms_pdu[UAL_TELE_SMS_PDU_SIZE + 1];
} ual_tele_sms_pdu_receive_ind_t;


//MSG_UAL_TELE_SMS_REPORT_IND 的消息载荷结构体
typedef struct {
    ual_tele_sms_result_e           result;
    ual_tele_sim_id_e               sim_id;
    ual_tele_sms_storage_e          storage_id;
    uint16                          record_id;
    uint8                           tp_mr;
    ual_tele_sms_transfer_status_e  transfer_status;      //短信传送状态
    ual_tele_sms_number_list_t      sc_address;     //短信服务中心号码
    ual_tele_sms_number_list_t      dest_address;   //发送目标号码
    ual_tele_sms_time_stamp_t       arr_sc_time;    //短信到达短信服务中心的时间
    ual_tele_sms_time_stamp_t       sc_send_time;   //短信被短信服务中心发送的时间
} ual_tele_sms_report_ind_t;

//pdu模式 MSG_UAL_TELE_SMS_REPORT_IND 的消息载荷结构体
typedef struct {
    ual_tele_sim_id_e      sim_id;
    char                   sms_pdu[UAL_TELE_SMS_PDU_SIZE + 1];
} ual_tele_sms_pdu_report_ind_t;


//MSG_UAL_TELE_SMS_SEND_ONE_CNF 的消息载荷结构体
typedef struct {
    ual_tele_sms_result_e       result;
    ual_tele_sim_id_e           sim_id;
    uint8                       number_list_count;
    uint8                       number_list_index;
    uint8                       number_addr[UAL_TELE_SMS_DEST_ADDR_MAX_NUM][UAL_TELE_SMS_NUMBER_MAX_LEN];
    uint8                       total_num;//长短信分割成短短信的总条数
    uint8                       seq_num;//当前发送完成的短短信条数
    uint8                       tp_mr;
    uint32                      user_data;
} ual_tele_sms_send_one_cnf_t;

//MSG_UAL_TELE_SMS_SEND_CNF 的消息载荷结构体
typedef struct {
    ual_tele_sms_result_e   result;
    ual_tele_sim_id_e       sim_id;
    uint8                       tp_mr;
    uint32                      user_data;
} ual_tele_sms_send_cnf_t;

//MSG_UAL_TELE_SMS_SAVE_ONE_CNF 的消息载荷结构体
typedef struct {
    ual_tele_sms_result_e       result;
    ual_tele_sim_id_e           sim_id;
    ual_tele_sms_storage_e      storage_id;
    uint16                      record_id;
    uint8                       number_list_count;//多号码个数
    uint8                       number_list_index;
    uint8                       number_addr[UAL_TELE_SMS_DEST_ADDR_MAX_NUM][UAL_TELE_SMS_NUMBER_MAX_LEN];
    uint8                       total_num;//长短信分割成短短信的总条数
    uint8                       seq_num;//当前发送完成的短短信条数
    uint32                      user_data;
} ual_tele_sms_save_one_cnf_t;

//MSG_UAL_TELE_SMS_SAVE_CNF 的消息载荷结构体
typedef struct {
    ual_tele_sms_result_e       result;
    ual_tele_sim_id_e           sim_id;
    ual_tele_sms_storage_e      storage_id;
    uint16                      record_id;
    uint32                      user_data;
} ual_tele_sms_save_cnf_t;

//MSG_UAL_TELE_SMS_MEM_FULL_IND 的消息载荷结构体
typedef struct {
    ual_tele_sms_result_e           result;
    ual_tele_sim_id_e               sim_id;
    ual_tele_sms_mem_status_e       mem_status;
} ual_tele_sms_mem_full_ind_t;

/*---------------------------------------------------------------------------*/
/*                          FUNCTION DECLARE                                 */
/*---------------------------------------------------------------------------*/
/*******************************************************************************/
//	Description: set sms content info
//	Parameter: [In] ual_tele_sms_config_t
//	           [Out] None
//	           [Return] ual_tele_sms_result_e
//	Author: yang.wang3
//	Note:
/*******************************************************************************/
void ual_tele_sms_set_config(ual_tele_sms_config_t config_info, int *p_ret);

/*******************************************************************************/
//	Description: get sms content info
//	Parameter: [In] None
//	           [Out] p_sms_config
//	           [Return] ual_tele_sms_config_t
//	Author: yang.wang3
//	Note:
/*******************************************************************************/
void ual_tele_sms_get_config(ual_tele_sms_config_t *p_sms_config, int *p_ret);

/*******************************************************************************/
//	Description: ual_tele_sms_init
//	Parameter: [In] None
//	           [Out] None
//	           [Return] None
//	Author: yang.wang3
//	Note:
/*******************************************************************************/
void ual_tele_sms_init(int *p_ret);

/*******************************************************************************/
//	Description: 去初始化
//	Parameter: [In] None
//	           [Out] None
//	           [Return] None
//	Author: yang.wang3
//	Note:
/*******************************************************************************/
void ual_tele_sms_deinit(int *p_ret);

/*******************************************************************************/
//	Description: ual_tele_sms_register
//	Parameter: [In] p_callback private call back
//	           [Out] p_handle out of p_handle
//	           [Return] ual_tele_sms_result_e
//	Author: yang.wang3
//	Note:
/*******************************************************************************/
ual_tele_sms_result_e ual_tele_sms_register(_ual_client_register_callback p_callback, uint32 *p_handle, int *p_ret);

/*******************************************************************************/
//	Description: ual_tele_sms_unregister
//	Parameter: [In] handle
//	           [Out] none
//	           [Return] ual_tele_sms_result_e
//	Author: yang.wang3
//	Note:
/*******************************************************************************/
ual_tele_sms_result_e ual_tele_sms_unregister(uint32 handle, int *p_ret);

/*******************************************************************************/
//	Description: send sms api
//	Parameter: [In] p_send_param:短信发送参数
//	           [Out] none
//	           [Return] ual_tele_sms_result_e
//	Author: yang.wang3
//	Note:
/*******************************************************************************/
ual_tele_sms_result_e ual_tele_sms_send(ual_tele_sms_send_param_t *p_send_param, int *p_ret);

/*******************************************************************************/
//	Description: read sms api
//	Parameter: [In] p_read_param:短信读取参数
//	           [Out] none
//	           [Return] ual_tele_sms_result_e
//	Author: yang.wang3
//	Note:
/*******************************************************************************/
ual_tele_sms_result_e ual_tele_sms_read(ual_tele_sms_read_param_t *p_read_param, int *p_ret);

/*******************************************************************************/
//	Description: delete sms api
//	Parameter: [In] p_delete_param:短信删除参数
//	           [Out] none
//	           [Return] ual_tele_sms_result_e
//	Author: yang.wang3
//	Note:
/*******************************************************************************/
ual_tele_sms_result_e ual_tele_sms_delete(ual_tele_sms_delete_param_t *p_delete_param, int *p_ret);

/*******************************************************************************/
//	Description: save sms api
//	Parameter: [In] p_save_param:短信保存参数
//	           [Out] none
//	           [Return] ual_tele_sms_result_e
//	Author: yang.wang3
//	Note:
/*******************************************************************************/
ual_tele_sms_result_e ual_tele_sms_save(ual_tele_sms_save_param_t *p_save_param, int *p_ret);

/*******************************************************************************/
//	Description: update sms api
//	Parameter: [In] p_update_param:短信更新参数
//	           [Out] none
//	           [Return] ual_tele_sms_result_e
//	Author: yang.wang3
//	Note:
/*******************************************************************************/
ual_tele_sms_result_e ual_tele_sms_update(ual_tele_sms_update_param_t *p_update_param, int *p_ret);

/*******************************************************************************/
//	Description: 设置信息服务中心号码
//	Parameter: [In] sim_id: 卡槽号
//	           [In] p_addr: 短信中心号码
//	           [Out] none
//	           [Return] ual_tele_sms_result_e
//	Author: yang.wang3
//	Note:
/*******************************************************************************/
ual_tele_sms_result_e ual_tele_sms_set_service_center_num(ual_tele_sim_id_e sim_id, char *p_addr, int *p_ret);

/*******************************************************************************/
//	Description: 获取短信存储容量
//	Parameter: [In] sim_id: 卡槽号
//	           [In] storage_id: 短信存储区域
//	           [Out] p_max: 该存储区域最大可存储短信数量
//	           [Out] p_used: 该存储区域已存储短信数量
//	           [Return] ual_tele_sms_result_e
//	Author: yang.wang3
//	Note:
/*******************************************************************************/
ual_tele_sms_result_e ual_tele_sms_get_capacity(ual_tele_sim_id_e sim_id,
        ual_tele_sms_storage_e storage_id,
        uint16 *p_max,
        uint16 *p_used,
        int *p_ret);

/**--------------------------------------------------------------------------*
 **                         Compiler Flag                                    *
 **--------------------------------------------------------------------------*/
#ifdef __cplusplus
}
#endif
#endif //_UAL_TELE_SMS_EXPORT_H_
