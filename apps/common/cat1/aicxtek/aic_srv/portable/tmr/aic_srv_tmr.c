/******************************************************************************/
/*                                                                            */
/*    Copyright 2023 by AICXTEK TECHNOLOGIES CO.,LTD. All rights reserved.    */
/*                                                                            */
/******************************************************************************/

/**
 * @file aic_srv_tmr.c
 *
 */


/*********************
 *      INCLUDES
 *********************/
#include "timer.h"
#include "aic_srv_tmr.h"


/*********************
 *      DEFINES
 *********************/


/**********************
 *      TYPEDEFS
 **********************/
typedef struct {
    srv_tmr_handle      tmr_handle;           /* timer handle */
    u16                 ucos_timer_id;          /* ucos timer id */
    srv_func_callback   client_timer_cb;        /* user timer callback */
    bool                is_period;              /* user timer is period */
    void                *p_user_data;           /* user data */
} timer_record_t;


/**********************
 *  STATIC PROTOTYPES
 **********************/


/**********************
 *  STATIC VARIABLES
 **********************/


/**********************
 *   STATIC FUNCTIONS
 **********************/

static void ucos_timer_timeout_func(void *input)
{
    timer_record_t *p_timer_record = NULL;

    srv_tmr_cb_param_t tmr_cb_param = {0};
    uint32_t data_size = 0;

    if (input == NULL) {
        printf("[%s],input param is null", __func__);
        return;
    }

    p_timer_record = (timer_record_t *)input;

    if (NULL != p_timer_record->client_timer_cb) {
        tmr_cb_param.tmr_handle = p_timer_record->tmr_handle;
        tmr_cb_param.p_usr_data = p_timer_record->p_user_data;
        data_size = sizeof(srv_tmr_cb_param_t);
        p_timer_record->client_timer_cb(&tmr_cb_param, data_size);
    } else {
        printf("[%s],client_timer_cb is null", __func__);
    }

}

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
srv_tmr_handle aic_srv_tmr_create(srv_tmr_param_t *p_tmr_param, srv_func_callback _tmr_cb)
{
    timer_record_t *p_timer_record = NULL;
    u16 ucos_timer_id = 0;

    /* 1. Judge validity of Input param */
    if (NULL == p_tmr_param) {
        printf("[%s]:p_tmr_param is null", __func__);
        return NULL;
    }

    /* 2. malloc space for record */
    p_timer_record = (timer_record_t *)calloc(1, sizeof(timer_record_t));

    if (NULL == p_timer_record) {
        printf("[%s],p_timer_record malloc fail", __func__);
        return NULL;
    }

    p_timer_record->p_user_data     = p_tmr_param->p_usr_data;
    p_timer_record->client_timer_cb = _tmr_cb;
    p_timer_record->is_period       = p_tmr_param->is_repeat;
    p_timer_record->tmr_handle      = p_timer_record;

    /* 3. Create timer by uCos API */
    printf("[%s],p_timer_record->is_period = %d", __func__, p_timer_record->is_period);

    if (true == p_timer_record->is_period) {
        /* repeat timer */
        ucos_timer_id = sys_timer_add(p_timer_record,
                                      ucos_timer_timeout_func,
                                      p_tmr_param->time_out);
    } else {
        /* not repeat timer */
        ucos_timer_id = sys_timeout_add(p_timer_record,
                                        ucos_timer_timeout_func,
                                        p_tmr_param->time_out);
    }

    /* 4. Save ucos timer id  */
    if (0 != ucos_timer_id) {
        p_timer_record->ucos_timer_id = ucos_timer_id;
        printf("[%s],ucos timer id = %d", __func__, ucos_timer_id);
    } else {
        printf("[%s] create fail,ucos_timer_id is 0", __func__);

        return NULL;
    }

    printf("[%s],timer create success.timer handle = 0x%x", __func__, p_timer_record);
    return (srv_tmr_handle)p_timer_record;
}

int32_t aic_srv_tmr_destroy(srv_tmr_handle tmr_handle)
{
    int ret = 0;
    timer_record_t *p_timer_record = NULL;
    u16 timer_id = 0;

    if (NULL == tmr_handle) {
        printf("[%s],srv_tmr_ptr timer handle is NULL,error", __func__);
        ret = -1;
        return ret;
    }

    p_timer_record = (timer_record_t *)tmr_handle;
    timer_id = p_timer_record->ucos_timer_id;

    printf("[%s],timer handle = 0x%x, ucos timer id = %d", __func__, p_timer_record, timer_id);

    /* 1. delete srv timer id */
    if (0 < timer_id) {
        if (true == p_timer_record->is_period) {
            /* repeat timer */
            sys_timer_del(timer_id);
        } else {
            /* not repeat timer */
            sys_timeout_del(timer_id);
        }
        printf("[%s],timer destroy success", __func__);
    } else {
        printf("[%s],ucos timer id is 0,error", __func__);
        ret = -1;
    }

    /* 2. free srv timer id */
    free(p_timer_record);
    p_timer_record = NULL;

    return ret;
}

int32_t aic_srv_tmr_pause(srv_tmr_handle tmr_handle)
{
    timer_record_t *p_timer_record = NULL;
    u16  timer_id = 0;
    int  ret = 0;

    if (NULL == tmr_handle) {
        printf("[%s],srv_tmr_ptr timer handle is NULL,error", __func__);
        ret = -1;
        return ret;
    }

    p_timer_record = (timer_record_t *)tmr_handle;
    timer_id = p_timer_record->ucos_timer_id;
    printf("[%s],timer handle = 0x%x, ucos timer id = %d", __func__, p_timer_record, timer_id);

    /* 1. deactive this timer */
    //aos_deactivate_timer(p_timer->aos_timer_id);
    printf("[%s],timer pause success.", __func__);
    return ret;
}

int32_t aic_srv_tmr_resume(srv_tmr_handle tmr_handle)
{
    timer_record_t *p_timer_record = NULL;
    u16  timer_id = 0;
    int  ret = 0;

    if (NULL == tmr_handle) {
        printf("[%s],timer handle is NULL,error", __func__);
        ret = -1;
        return ret;
    }

    p_timer_record = (timer_record_t *)tmr_handle;
    timer_id = p_timer_record->ucos_timer_id;
    printf("[%s],timer handle = 0x%x, ucos timer id = %d", __func__, p_timer_record, timer_id);

    /* 1. active this timer */
    //aos_activate_timer(p_timer->aos_timer_id);
    printf("[%s],timer resume success.", __func__);
    return ret;
}

int32_t aic_srv_tmr_change(srv_tmr_handle tmr_handle, srv_tmr_param_t *p_tmr_param)
{
    timer_record_t *p_timer_record = NULL;
    u16  timer_id = 0;
    u16  new_timer_id = 0;
    int  ret = 0;

    if (NULL == tmr_handle) {
        printf("[%s],timer handle is NULL,error", __func__);
        ret = -1;
        return ret;
    }

    if (NULL == p_tmr_param) {
        printf("[%s],p_tmr_param NULL,error", __func__);
        ret = -1;
        return ret;
    }

    p_timer_record = (timer_record_t *)tmr_handle;
    timer_id = p_timer_record->ucos_timer_id;

    /* 1. change this timer */
    printf("[%s],timer change begin,timer handle = 0x%x,timer id = %d,is_repeat(old)=%d, is_repeat(new) = %d, timeout = %d", __func__, p_timer_record, timer_id, p_timer_record->is_period, p_tmr_param->is_repeat, p_tmr_param->time_out);

    if (0 < timer_id) {
        if (true == p_timer_record->is_period) {
            /* repeat timer */
            sys_timer_modify(timer_id, p_tmr_param->time_out);
        } else {
            /* not repeat timer,first del,then recreate,otherwise callback is not arriving  */
            sys_timeout_del(timer_id);
            new_timer_id = sys_timeout_add(p_timer_record, ucos_timer_timeout_func, p_tmr_param->time_out);
            p_timer_record->ucos_timer_id = new_timer_id;
            printf("[%s],new ucos timer id = %d.", __func__, new_timer_id);
            printf("[%s],timer change end,timer handle = 0x%x,timer id = %d, is_repeat = %d, timeout = %d", __func__, p_timer_record, new_timer_id, p_timer_record->is_period, p_tmr_param->time_out);
        }
    } else {
        printf("[%s],timer id is 0,error", __func__);
        ret = -1;
        return ret;
    }

    printf("[%s],timer change success.", __func__);
    return ret;
}
