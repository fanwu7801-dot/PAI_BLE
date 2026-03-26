/****************************************************************************
** File Name:      lpa_export.h                                            *
** Author:                                                                 *
** Date:           05/09/2023                                              *
** Copyright:      2023 Spreadtrum, Incoporated. All Rights Reserved.      *
** Description:                                                            *
****************************************************************************
**                         Important Edit History                          *
** ------------------------------------------------------------------------*
** DATE           NAME             DESCRIPTION                             *
** 09/2023       mingwei.jia         Create                                *
**                                                                         *
****************************************************************************/
#ifndef _LPA_EXPORT_H_
#define _LPA_EXPORT_H_


/**-------------------------------------------------------------------------*
 **                         Compile Flag                                    *
 **------------------------------------------------------------------------*/
#ifdef __cplusplus
extern "C"
{
#endif

/**-------------------------------------------------------------------------*
 **                         Include Files                                   *
 **------------------------------------------------------------------------*/

/**-------------------------------------------------------------------------*
 **                         Type Declaration                                *
 **---------------------------------------------------------------- --------*/
#define   LPA_SEND_MAX_LEN            (1000)

typedef void (*thinmodem_service_register_lpa_cb)(uint8 *p_data, uint32 cur_len);

typedef enum {
    LPA_RES_SUCCESS = 0,
    LPA_RES_ERROR,
    LPA_RES_MAX
} lpa_result_e;

typedef struct {
    uint32 total_data_len;
    uint32 cur_send_data_len;
    uint32 is_first_packet;
    uint8 send_data[LPA_SEND_MAX_LEN + 1];
} lpa_send_data_cnf_t;

/*--------------------------------------------------------------------------*/
/*                          FUNCTION DECLARE                                */
/*--------------------------------------------------------------------------*/
/*****************************************************************************/
//  Description : create LPA task
//  Global resource dependence :
//  Author:
//  Note:
/*****************************************************************************/
void lpa_init(int *p_ret);

/*****************************************************************************/
//  Description : lpa_channel_write
//  Global resource dependence :
//  Author:
//  Note:
/*****************************************************************************/
lpa_result_e lpa_channel_write(uint8 *p_cur_buf,  uint32 cur_buf_len, uint32 buf_total_len, int *p_ret);

/*****************************************************************************/
//  Description :lpa_set_device_name
// Parameter: [In] 设备名称
//            [In] 设备名称长度
//            [Return]lpa_result_e
//  Author:mingwei.jia
//  Note:
/*****************************************************************************/
lpa_result_e lpa_set_device_name(uint8 *p_name, uint32 len, int *p_ret);

/*****************************************************************************/
//  Description :lpa_set_manufacturer_name
// Parameter: [In] 厂商名称
//            [Out] 厂商名称长度
//            [Return]lpa_result_e
//  Author:mingwei.jia
//  Note:
/*****************************************************************************/
lpa_result_e lpa_set_manufacturer_name(uint8 *p_name, uint32 len, int *p_ret);

void thinmodem_register_lpa_callback(thinmodem_service_register_lpa_cb callback_fun);

#ifdef   __cplusplus
}
#endif
#endif

