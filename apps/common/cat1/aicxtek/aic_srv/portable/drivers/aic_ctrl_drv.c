/**************************************************************************/
/*                                                                        */
/* Copyright 2023 by AICXTEK TECHNOLOGIES CO.,LTD. All rights reserved.   */
/*                                                                        */
/**************************************************************************/

/**
 *  DESCRIPTION
 *
 *    This file aic ctrl module adpter source file.
 */
#include "system/includes.h"
#include "app_config.h"
#include "aic_ctrl.h"
#include <aic_ctrl_drv.h>

#if TCFG_CAT1_AICXTEK_ENABLE

#define PIN_VALUE_LOW           0
#define PIN_VALUE_HIGH          1
#define GPIO_DIR_OUTPUT         0
#define GPIO_DIR_INPUT          1

/*
 * Notice:
 * Must according the hardware desgin to config the PWR_PIN_NUM,
 * CONF_PWR_PIN_HIGH_RES_STATE and .CONF_PWR_PIN_LOW_ACTIVE.
 *
 */

/*
 * PWR_PIN_NUM: Which pin on mcu is connected with aic.
 */
#define PWR_PIN_NUM             TCFG_CAT1_AICXTEK_GPIO_POWER

/*
 * CONF_PWR_PIN_HIGH_RES_STATE:
 *    0, power up pin no need keep resistence state.
 *    1, power up pin must keep resistence state.
 *  if no special considerations, no need keep resistence state.
 *
 *  The demo board need config 1, because that if it doesn't keep
 *  resistence state,it will have current leakage.
 */
#define CONF_PWR_PIN_HIGH_RES_STATE         1

/*
 * CONF_PWR_PIN_LOW_ACTIVE:
 *    0, The pwr pin output high will power on aic, high actived.
 *    1, The pwr pin output high will power on aic, low actived.

 *  In aic side the pwr key is always low actived, but in mcu side,
 *  Must see the connection method of the mcu pin and the aic pwr key.
 *
 *  The demo board is low actived, so nned config it to 1.
 */
#define CONF_PWR_PIN_LOW_ACTIVE             1


#if CONF_PWR_PIN_LOW_ACTIVE
#define PWR_ACTIVE_VALUE        PIN_VALUE_LOW
#define PWR_DEACTIVE_VALUE      PIN_VALUE_HIGH
#else
#define PWR_ACTIVE_VALUE        PIN_VALUE_HIGH
#define PWR_DEACTIVE_VALUE      PIN_VALUE_LOW
#endif

#if CONF_PWR_PIN_HIGH_RES_STATE
void aic_ctrl_active_power_pin(bool active)
{
    static u8 s_uninit = 1;
    u32 dir = active ? GPIO_DIR_OUTPUT : GPIO_DIR_INPUT;

    if (s_uninit) {
        s_uninit = 0;
        /*
         * set power up pin to high resistence state,
         */
        alog_info("aic ctrl init pwr pin.\n");
        gpio_set_die(PWR_PIN_NUM, 0);
        gpio_set_dieh(PWR_PIN_NUM, 0);
        gpio_set_pull_up(PWR_PIN_NUM, 0);
        gpio_set_pull_down(PWR_PIN_NUM, 0);
        gpio_set_direction(PWR_PIN_NUM, GPIO_DIR_INPUT);
        gpio_set_output_value(PWR_PIN_NUM, PWR_ACTIVE_VALUE);
    }

    alog_info("aic ctrl power pin=%d, active=%d, set dir=%d.\n",
              PWR_PIN_NUM, active, dir);
    gpio_set_direction(PWR_PIN_NUM, dir);
}
#else
void aic_ctrl_active_power_pin(bool active)
{
    u32 value = active ? PWR_ACTIVE_VALUE : PWR_DEACTIVE_VALUE;

    alog_info("aic ctrl power pin=%d, active=%d, value=%d.\n",
              PWR_PIN_NUM, active, value);

    gpio_set_direction(PWR_PIN_NUM, GPIO_DIR_OUTPUT);
    gpio_set_output_value(PWR_PIN_NUM, value);
}
#endif

#endif /* #if TCFG_CAT1_AICXTEK_ENABLE */

