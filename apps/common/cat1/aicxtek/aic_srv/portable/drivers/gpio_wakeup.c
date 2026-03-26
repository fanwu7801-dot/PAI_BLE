#include "system/includes.h"
#include "app_config.h"
#include "asm/gpio.h"
#include "ui/ui_api.h"
#include "aic_gpio_wakeup.h"

#if TCFG_CAT1_AICXTEK_ENABLE

//#define GPIO_WAKEUP_DEBUG_LOG
#ifdef GPIO_WAKEUP_DEBUG_LOG
#define debug_printf    printf
#else
#define debug_printf(...)
#endif

static volatile u8 uart_transfer_idle = 1, uart_transfer_idle_req = 1, cmux_callin_sts = 1;

static inline u32 ut_get_jiffies(void)
{
#if 1
    return jiffies;
#endif
#if 0
    return Jtime_updata_jiffies();
#endif
}

static inline u32 ut_msecs_to_jiffies(u32 msecs)
{
    if (msecs >= 10) {
        msecs /= 10;
    } else if (msecs) {
        msecs = 1;
    }
    return msecs;
}


static void gpio_uart_transfer_idle_clr(void)
{
    uart_transfer_idle =  0;
}

static void gpio_uart_transfer_idle_set(void)
{
    uart_transfer_idle =  1;
}

static u8 gpio_uart_transfer_idle_query(void)
{
    return uart_transfer_idle;
}

REGISTER_LP_TARGET(JL_UART_TRANSFER) = {
    .name = "modem uart",
    .is_idle = gpio_uart_transfer_idle_query,
};


static void gpio_uart_transfer_req_idle_clr(void)
{
    uart_transfer_idle_req =  0;
}

static void gpio_uart_transfer_req_idle_set(void)
{
    uart_transfer_idle_req =  1;
}

static u8 gpio_uart_transfer_req_idle_query(void)
{
    return uart_transfer_idle_req;
}


REGISTER_LP_TARGET(JL_UART_TRANSFER_REQ) = {
    .name = "modem uart req",
    .is_idle = gpio_uart_transfer_req_idle_query,
};




static void cmux_callin_clr(void)
{
    cmux_callin_sts = 0;
}

static void cmux_callin_set(void)
{
    cmux_callin_sts = 1;
}

static u8 cmux_callin_query(void)
{
    return cmux_callin_sts;
}

REGISTER_LP_TARGET(JL_CMUX_CALLIN) = {
    .name = "cmux callin",
    .is_idle = cmux_callin_query,
};

void aic_cmux_callin_get_wakelock(void)
{
    cmux_callin_clr();
}

void aic_cmux_callin_put_wakelock(void)
{
    cmux_callin_set();
}

void aic_gpio_get_request_wakelock(void)
{
    gpio_uart_transfer_req_idle_clr();
}

void aic_gpio_put_request_wakelock(void)
{
    gpio_uart_transfer_req_idle_set();
}

void aic_gpio_get_ack_wakelock(void)
{
    gpio_uart_transfer_idle_clr();
}

void aic_gpio_put_ack_wakelock(void)
{
    gpio_uart_transfer_idle_set();
}

int aic_gpio_wait_modem_wakeup(u32 timeout)
{
    u32 timeout_jiffies = ut_msecs_to_jiffies(timeout) + ut_get_jiffies();

    debug_printf("gpio_wait_target_mcu_wakeup, timeout = %d ms\n", timeout);
    while (!aic_gpio_get_modem_wakeup_status()) {
        if (timeout && time_before(timeout_jiffies, ut_get_jiffies())) {
            debug_printf("mcu_wakeup: timeout_jiffies = 0x%x, curT = 0x%x\n",
                         timeout_jiffies, ut_get_jiffies());
            return -1;
        }
    }

    return 0;
}

void port_falling_wakeup_callback(u8 index, u8 gpio)
{
    aic_gpio_mcu_can_sleep();
}

void port_rising_wakeup_callback(u8 index, u8 gpio)
{
    aic_gpio_mcu_keep_wakeup();
}

#endif /* #if TCFG_CAT1_AICXTEK_ENABLE */

