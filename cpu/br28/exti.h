#ifndef _EXTI_H_
#define _EXTI_H_

#include "typedef.h"
/**
 * 注意：JL_WAKEUP 区别于PMU管理的唤醒。可理解为一个独立的模块使用。但在低功耗的情况下，中断无效。
 */

/* IO唤醒源: 6个唤醒源.enum exti_source
 *    事件0: ich_wkup
 *    事件1: irflt //与irflt 冲突.注意复用.
 *    事件2: uart0_rxas //与uart_rx冲突.注意复用.
 *    事件3: uart1_rxas
 *    事件4: uart2_rxas
 *    事件5: SD0_DAT_I[1]
 * */
enum exti_source {
    EXTI_S_ICH_WKUP,
    EXTI_S_IRFLT,//复用
    EXTI_S_UART0_RXAS,//复用
    EXTI_S_UART1_RXAS,//复用
    EXTI_S_UART2_RXAS,//复用
    EXTI_S_SD0_DAT_I1,//复用
};
/*
 * @brief 使能ICH0~13口[唤醒/外部中断]
 * @parm port 端口 such as:IO_PORTA_00
 * @parm exti_s 见枚举exti_source
 * @parm trigger_edge 检测边缘，1 下降沿，0 上升沿
 * @parm cbfun 中断回调函数
 * @return 0 成功，< 0 失败
 */
int exti_event_init(u32 port, enum exti_source exti_s, u8 trigger_edge, void (*cbfun)(u32, u32, u32), u8 irq_priority); //注意功能复用
/**
 * @brief 关掉该引脚的中断功能
 * @param port 引脚号：IO_PORTA_00......
 * @parm exti_s 见枚举exti_source
 * @return 0 成功，< 0 失败
 */
int exti_event_close(u32 port, enum exti_source exti_s);


/******************以下接口仅支持 EXTI_S_ICH********************/
/*
 * @brief 使能ICH0~13口[唤醒/外部中断]
 * @parm port 端口 such as:IO_PORTA_00
 * @parm trigger_edge 检测边缘，1 下降沿，0 上升沿
 * @parm cbfun 中断回调函数
 * @return 0 成功，< 0 失败
 */
int exti_init(u32 port, u8 trigger_edge, void (*cbfun)(u32, u32, u32), u8 irq_priority);
/**
 * @brief 关掉该引脚的中断功能
 * @param port 引脚号：IO_PORTA_00......
 * @return 0 成功，< 0 失败
 */
int exti_uninit(u32 port);
void exti_change_callback(u32 port, void (*cbfun)(u32, u32, u32));
void exti_change_en_state(u32 port, u8 exti_en);//exti_en:1:en,0:disable
void exti_change_edge_state(u32 port, u8 edge);
u8 exti_get_edge_state(u32 port);

#endif

