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
** File Name:         version.h                                                *
** Author:            shuting.ma                                               *
** Date:             17/10/2023                                                *
** Description:                                                                *
********************************************************************************
**                         Important Edit History                              *
** ----------------------------------------------------------------------------*
** DATE                 NAME                  DESCRIPTION                      *
** 17/10/2023           shuting.ma              Create                         *
********************************************************************************/
#ifndef _VERSION_H
#define _VERSION_H

/**----------------------------------------------------------------------------*
 **                         Include Files                                      *
 **----------------------------------------------------------------------------*/

/**----------------------------------------------------------------------------*
 **                         MACRO DEFINITION                                   *
 **----------------------------------------------------------------------------*/

/**----------------------------------------------------------------------------*
**                         TYPE DEFINITION                                     *
**-----------------------------------------------------------------------------*/

/**----------------------------------------------------------------------------*
**                         MACRO DEFINITION                                    *
**-----------------------------------------------------------------------------*/

/*****************************************************************************/
//  Description: get version info
//  Parameter: [In] type
//             [Out] version
//             [Return] none
//  Author: shuting.ma
//  Note:
/*****************************************************************************/
void VERSION_GetInfoEx(uint32 type, char version[50], int *p_ret);


#endif
