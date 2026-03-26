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
*****************************************************************************/
/*****************************************************************************
** File Name:   mdload.c                                                     *
** Author:      guangqiao.she                                                *
** Date:        2023/09/12                                                   *
** Description:                                                              *
******************************************************************************
**                         Important Edit History                            *
** --------------------------------------------------------------------------*
**   DATE                  NAME              DESCRIPTION                     *
** 9/12/2023           guangqiao.she           Create                        *
******************************************************************************/
#ifndef MDLOAD_API_H
#define MDLOAD_API_H

#include"uos_type.h"

#ifdef __cplusplus
extern "C" {
#endif

/* mdload upgrate mode */
typedef enum mdload_upgrate_mode_enum {
    MDLOAD_INIT_MODE = 0,   // initial mode
    MDLOAD_UNKNOW_MODE = 1,   // unknow mode
    MDLOAD_DOWNLOAD_MODE = 2,   // power off the modem, and then power it on to enter the download mode.

    MDLOAD_MODE_END
} mdload_upgrate_mode_e;


/* the download file type */
typedef enum mdload_file_type_enum {
    MDLOAD_FILE_MODEM_IMG = 0,

    MDLOAD_FILE_END
} mdload_file_type_e;


/* mdload error code */
typedef enum mdload_error_enum {
    MDLOAD_NO_ERR = 0,
    MDLOAD_PARAMTER_ERR =  1,  // parameter fail
    MDLOAD_OPEN_FAIL =     2,  // mdload open fail
    MDLOAD_START_FAIL =    3,   // mdload start fail
    MDLOAD_SEND_FAIL =     4,  // file download to modem fail
    MDLOAD_END_FAIL =      5,  // update fail
    MDLOAD_CLOSE_FAIL =    6, // mdload exit fail
    MDLOAD_PARTITION_OVERFLOW = 7, // partition overflow
    MDLOAD_FIXNV_BAK_FAIL = 8, // fixnv bakup fail
    MDLOAD_ERR_END
} mdload_error_e;



/**
 * @brief     set the upgrate mode, prepare to download
 * @param[in]: mode -- ref mdload_upgrate_mode_e definition
 * @return: ref mdload_error_e definition
 */
int mdload_open(mdload_upgrate_mode_e mode);

/**
 * @brief     exit  download
 * @param[in]
 * @return:ref mdload_error_e definition
 */
int mdload_close(void);


/**
 * @brief    start mdload, set the file size to modem
 * @param[in]: type -- ref mdload_file_type_e definition
 *             len -- download file total size
 * @return:ref mdload_error_e definition
 */
int mdload_start(mdload_file_type_e type, uos_uint32_t len);


/**
 * @brief     downdload the img data to modem
 * @param[in]: data  -- download data
 *             len   -- download data size
 * @return:ref mdload_error_e definition
 */
int mdload_send(void *data, uos_uint32_t len);


/**
 * @brief   the file download end
 * @param[in]
 * @return:ref mdload_error_e definition
 */
int mdload_end(void);



/**---------------------------------------------------------------------------*
 **                         Function Prototypes                               *
 **---------------------------------------------------------------------------*/


#ifdef __cplusplus
}
#endif

//#endif
#endif // MDLOAD_API_H
