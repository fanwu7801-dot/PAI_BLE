#ifndef ZHANRUI_GPIO_H
#define ZHANRUI_GPIO_H
#include "asm/gpio.h"
#include "uos_type.h"
#include "uos_deval.h"
#include "uos_vendor_cfg.h"
/*************************************************
* 功能： 根据不同硬件版本来设置使用不同GPIO配置。可选实现
* 输入参数：
* 无
* 返回：
* 错误码
************************************************/
uos_err_t uos_board_gpio_config(void);
/*************************************************
* 功能： 初始化指定GPIO
* 输入参数：
* pin 输入。指定GPIO，uos_deval.h中的宏定义
* pin_mode 输入。GPIO工作模式，uos_gpio_mode类型
* isr_handler 输入。GPIO中断回调函数
* args 输入。GPIO中断回调函数的输入参数
* 返回：
* 错误码
************************************************/
uos_err_t uos_gpio_init(uos_uint32_t pin, uos_gpio_mode pin_mode, void
                        (*isr_handler)(void *args), void *args);
/*************************************************
* 功能： 设置GPIO
* 输入参数：
* pin 输入。指定GPIO，uos_deval.h中的宏定义
* value 输入。0或非0
* 返回：
* 错误码
************************************************/
uos_err_t uos_gpio_write(uos_uint32_t pin, uos_int32_t value);
/*************************************************
* 功能： 读取GPIO状态
* 输入参数：
* pin 输入。指定GPIO，uos_deval.h中的宏定义
* 返回：
* GPIO电平
************************************************/
uos_int32_t uos_gpio_read(uos_uint32_t pin);
/*************************************************
* 功能： disable GPIO,不作为GPIO时的处理
* 输入参数：
* pin 输入。指定GPIO，uos_deval.h中的宏定义
* 返回：
* 错误码
************************************************/
uos_err_t uos_gpio_disable(uos_uint32_t pin);
/*************************************************
* 功能： 设置GPIO中断触发沿
* 输入参数：
* pin 输入。指定GPIO，uos_deval.h中的宏定义
* * trigger_style 输入。触发沿
* 返回：
* 错误码
************************************************/
uos_err_t uos_gpio_int_trigger_set(uos_uint32_t pin, uos_uint32_t trigger_style);

#endif
