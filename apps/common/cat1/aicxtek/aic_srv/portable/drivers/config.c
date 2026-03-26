#include "system/includes.h"
#include "app_config.h"
#include "aic_cfg.h"

#if TCFG_CAT1_AICXTEK_ENABLE

/*
 * The aic uart config table.
 */
static aic_uart_cfg_t s_aic_uart_cfg = {
    .tx_pin = TCFG_CAT1_AICXTEK_UART_TX,
    .rx_pin = TCFG_CAT1_AICXTEK_UART_RX,
    .baud = 4000000
};

/*
 * The aic gpio wakeup config table.
 */
static aic_gpio_wakeup_cfg_t s_aic_gpio_wakeup_cfg = {
    .mcu_wakeup_modem = {.gpio_num = TCFG_CAT1_AICXTEK_GPIO_ACTIVE, .is_active_low = 1},
    .modem_wakeup_mcu = {.gpio_num = TCFG_CAT1_AICXTEK_GPIO_WKUP, .is_active_low = 1},
};

aic_uart_cfg_t *aic_get_uart_cfg(void)
{
    return &s_aic_uart_cfg;
}

aic_gpio_wakeup_cfg_t *aic_gpio_wakeup_cfg(void)
{
    return &s_aic_gpio_wakeup_cfg;
}

#endif
