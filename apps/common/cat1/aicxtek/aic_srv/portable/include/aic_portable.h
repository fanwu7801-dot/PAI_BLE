/******************************************************************************/
/*                                                                            */
/*    Copyright 2023 by AICXTEK TECHNOLOGIES CO.,LTD. All rights reserved.    */
/*                                                                            */
/******************************************************************************/

/**
 *  DESCRIPTION
 *
 *    This file defines the aic portable Interface.
 */

#ifndef __AIC_PORTABLE_H__
#define __AIC_PORTABLE_H__

#include "printf.h"
#include "typedef.h"

#define AIC_SRV_SW_BRANCH       "jieli"

/*
*********************************************************************
* Function   : AIC_ASSERT
* Arguments  : 入参n用作条件判断，当其为0时满足assert条件，会发生assert.
* Return     : None.
* Note(s)    : None.
* Description: AIC_ASSERT 为aic的通用assert接口，满足assert条件时，
*              需要出发系统assert。
*********************************************************************
*/
#define AIC_ASSERT_HANDLER      while(1);
#define ASSERT_PREFIX           "AIC Assert at File(%s), Line(%d)!"
#define ASSERT_POS              __FILE__, __LINE__
#define AIC_ASSERT(n) do { \
    if(!(n)) {\
       printf(ASSERT_PREFIX, ASSERT_POS); \
       AIC_ASSERT_HANDLER \
    }\
} while(0)

/*
*********************************************************************
* Function   : usleep
* Arguments  : usec，定义睡眠的微秒时间.
* Return     : None.
* Note(s)    : None.
* Description: 当前task睡眠usec时间，到期后唤醒该task
*********************************************************************
*/
#define usleep(usec)                udelay(usec)


/*
*********************************************************************
* Function   : alog_debug、alog_info、alog_warn、alog_error
* Arguments  : fmt ...
* Return     : None.
* Note(s)    : None.
* Description: 分别实现不同log level等级的log打印
*********************************************************************
*/

typedef enum {
    /* Default: close all log */
    ALOG_LEVEL_NONE,
    /* Just open error log */
    ALOG_LEVEL_ERROR,
    /* Open error and warning log */
    ALOG_LEVEL_WARNING,
    /* Open info ,error and warning log */
    ALOG_LEVEL_INFO,
    /* Open all log */
    ALOG_LEVEL_DEBUG,

    ALOG_LEVEL_MAX,
    /* Ensure takeup 4 bytes. */
    ALOG_LEVEL_END = 0xFFFFFFFF
} alog_level_e;

int32_t alog_set_level(alog_level_e log_level);
alog_level_e alog_get_level(void);

/* alog default level */
#define ALOG_LEVEL_DEFAULT      ALOG_LEVEL_NONE


/* alog_debug define */
#define alog_debug(fmt, ...) \
        if (ALOG_LEVEL_DEBUG <= alog_get_level()) { \
            do { \
                if (true == aic_ctrl_is_version_unmatch()) \
                printf("X"fmt, ##__VA_ARGS__); \
                else \
                printf(fmt, ##__VA_ARGS__); \
            } while (0); \
        }

/* alog_info define */
#define alog_info(fmt, ...) \
    if (ALOG_LEVEL_INFO <= alog_get_level()) { \
        do { \
            if (true == aic_ctrl_is_version_unmatch()) \
            printf("X"fmt, ##__VA_ARGS__); \
            else \
            printf(fmt, ##__VA_ARGS__); \
        } while (0); \
    }

/* alog_warn define */
#define alog_warn(fmt, ...) \
        if (ALOG_LEVEL_WARNING <= alog_get_level()) { \
            do { \
                if (true == aic_ctrl_is_version_unmatch()) \
                printf("X"fmt, ##__VA_ARGS__); \
                else \
                printf(fmt, ##__VA_ARGS__); \
            } while (0); \
        }

/* alog_error define */
#define alog_error(fmt, ...) \
    if (ALOG_LEVEL_ERROR <= alog_get_level()) { \
        do { \
            if (true == aic_ctrl_is_version_unmatch()) \
            printf("X"fmt, ##__VA_ARGS__); \
            else \
            printf(fmt, ##__VA_ARGS__); \
        } while (0); \
    }

/*
 *********************************************************************
 * Function   : aos_malloc aos_zalloc aos_free
               aos_dma_malloc aos_dma_free
* Arguments  : size ...
* Return     : None.
* Note(s)    : None.
* Description: 分别实现不同申请memory接口
*********************************************************************
*/
#define aos_malloc(size)\
        malloc((u32)(size))
#define aos_zalloc(size)\
        zalloc((u32)(size))
#define aos_free(mem_ptr)\
        free((u8 *)(mem_ptr))

/*
*********************************************************************
* Function   : AOS_INTERRUPT_DECLARE AOS_INTERRUPT_DISABLE
               AOS_INTERRUPT_RESTORE
* Arguments  :
* Return     : None.
* Note(s)    : None.
* Description: 实现系统中断的使能和屏蔽
*********************************************************************
*/
#define AOS_INTERRUPT_DECLARE
#define AOS_INTERRUPT_DISABLE   __local_irq_disable();
#define AOS_INTERRUPT_RESTORE   __local_irq_enable();

#endif/* __AIC_PORTABLE_H__ */

