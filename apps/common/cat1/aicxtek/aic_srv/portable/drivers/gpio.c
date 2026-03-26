#include "system/includes.h"
#include "app_config.h"
#include "asm/gpio.h"
#include "ui/ui_api.h"

#if TCFG_CAT1_AICXTEK_ENABLE

int aic_gpio_set_dir(u32 gpio_num, u32 gpio_dir)
{
    return gpio_set_direction(gpio_num, gpio_dir);
}

int aic_gpio_set_outvalue(u32 gpio_num, u32 gpio_value)
{
    return gpio_set_output_value(gpio_num, gpio_value);
}

int aic_gpio_level_get(u32 gpio_num)
{
    return gpio_read(gpio_num);
}

int aic_gpio_set_high(u32 gpio_num)
{
    gpio_set_die(gpio_num, 0);
    gpio_set_dieh(gpio_num, 0);
    gpio_set_pull_up(gpio_num, 0);
    gpio_set_pull_down(gpio_num, 0);
    gpio_set_direction(gpio_num, 1);

    return 0;
}

#endif /* #if TCFG_CAT1_AICXTEK_ENABLE */

