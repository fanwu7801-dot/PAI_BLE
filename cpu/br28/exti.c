#include "exti.h"
#include "irq.h"
#include "gpio.h"

#define LOG_TAG_CONST       WKUP //EXTI
#define LOG_TAG             "[EXTI]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"

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

static u32 exti_source_pfi[] = {
    INPUT_CH_SIGNAL_WAKEUP,//INPUT_CH_SIGNAL_WAKEUP
    INPUT_CH_SIGNAL_IRFLT,//INPUT_CH_SIGNAL_IRFLT
    PFI_UART0_RX, //in  PFI_UART0_RX,
    PFI_UART1_RX, //in  PFI_UART1_RX,
    PFI_UART2_RX, //in  PFI_UART2_RX,
    PFI_SD0_DA1, //  PFI_SD0_DA1,
};
#define EXTI_SOURCE_NUM 6
static void (*exti_irq_cbfun[EXTI_SOURCE_NUM])(u32, u32, u32) = {NULL};
static u8 exti_user_port[EXTI_SOURCE_NUM] = {[0 ...(EXTI_SOURCE_NUM - 1)] = 0xff};
/**
 * @brief 引脚中断函数
 */
___interrupt
void exti_irq_fun(void)
{
    u8 wakeup_index = 0;
    //printf("png:0x%x\n", JL_WAKEUP->CON3);
    for (; wakeup_index < EXTI_SOURCE_NUM; wakeup_index++) {
        if (JL_WAKEUP->CON3 & BIT(wakeup_index)) {
            JL_WAKEUP->CON2 |= BIT(wakeup_index);
            if (exti_irq_cbfun[wakeup_index]) {
                //printf(">");
                exti_irq_cbfun[wakeup_index](exti_user_port[wakeup_index] / IO_GROUP_NUM, exti_user_port[wakeup_index] % IO_GROUP_NUM, (!!(JL_WAKEUP->CON1 & BIT(wakeup_index))) + 1);
            }
        }
    }
}
/*
 * @brief 使能ICH0~13口[唤醒/外部中断]
 * @parm port 端口 such as:IO_PORTA_00
 * @parm exti_s 见枚举exti_source
 * @parm trigger_edge 检测边缘，1 下降沿，0 上升沿
 * @parm cbfun 中断回调函数
 * @return 0 成功，< 0 失败
 */
int exti_event_init(u32 port, enum exti_source exti_s, u8 trigger_edge, void (*cbfun)(u32, u32, u32), u8 irq_priority) //注意功能复用
{
    if (port >= IO_PORT_MAX) {
        log_error("%s parameter:port error!", __func__);
        return -1;
    }
    if (JL_WAKEUP->CON0 & BIT(exti_s)) {
        log_error("port exti(%d) has been open.please close!\n", exti_s);
        return -2;
    }
    gpio_set_die(port, 1);
    gpio_set_direction(port, 1);
    if (trigger_edge == 0) {
        JL_WAKEUP->CON1 &= ~BIT(exti_s);//0:上升沿
        gpio_set_pull_down(port, 1);
        gpio_set_pull_up(port, 0);
    } else {
        JL_WAKEUP->CON1 |= BIT(exti_s);
        gpio_set_pull_down(port, 0);
        gpio_set_pull_up(port, 1);
    }
    if (exti_s < EXTI_S_UART0_RXAS) {
        gpio_ich_sel_iutput_signal(port, exti_source_pfi[exti_s], INPUT_CH_TYPE_GP_ICH);
    } else {
        gpio_set_fun_input_port(port, exti_source_pfi[exti_s], 0); //注意功能复用
    }
    exti_user_port[exti_s] = port;
    if (cbfun) {
        exti_irq_cbfun[exti_s] = cbfun;
    }
    request_irq(IRQ_PORT_IDX, irq_priority, exti_irq_fun, 0); //注册中断函数
    JL_WAKEUP->CON2 |= BIT(exti_s);                          //清一次pnd
    JL_WAKEUP->CON0 |= BIT(exti_s);                          //引脚中断使能
    JL_WAKEUP->CON2 |= BIT(exti_s);                          //清一次pnd
    printf("init:port:%d,exti_s:%d ok\n", port, exti_s);
    return 0;
}

/**
 * @brief 关掉该引脚的中断功能
 * @param port 引脚号：IO_PORTA_00......
 * @parm exti_s 见枚举exti_source
 * @return 0 成功，< 0 失败
 */
int exti_event_close(u32 port, enum exti_source exti_s)
{
    if (port >= IO_PORT_MAX) {
        log_error("%s parameter:port error!", __func__);
        return -1;
    }
    if (JL_WAKEUP->CON0 & BIT(exti_s)) {
        if (port == exti_user_port[exti_s]) {
            JL_WAKEUP->CON0 &= ~BIT(exti_s);
            gpio_set_direction(port, 1);
            gpio_set_die(port, 0);
            gpio_set_pull_down(port, 0);
            gpio_set_pull_up(port, 0);
            if (exti_s < EXTI_S_UART0_RXAS) {
                gpio_ich_disable_iutput_signal(port, exti_source_pfi[exti_s], INPUT_CH_TYPE_GP_ICH);
            } else {
                gpio_disable_fun_input_port(exti_source_pfi[exti_s]);//注意功能复用
            }
            exti_user_port[exti_s] = 0xff;
            exti_irq_cbfun[exti_s] = NULL;
            log_info("port:%d, exti_s:%d close\n", port, exti_s);
        } else {
            log_error("%s parameter:port error!\n", __func__);
            return -2;
        }
    } else {
        log_error("port exti source:%d has been closed!\n", exti_s);
        return -3;
    }
    return 0;
}





/******************以下接口仅支持 EXTI_S_ICH********************/
/*
 * @brief 使能ICH0~13口[唤醒/外部中断]
 * @parm port 端口 such as:IO_PORTA_00
 * @parm trigger_edge 检测边缘，1 下降沿，0 上升沿
 * @parm cbfun 中断回调函数
 * @return 0 成功，< 0 失败
 */
int exti_init(u32 port, u8 trigger_edge, void (*cbfun)(u32, u32, u32), u8 irq_priority)
{
    if ((JL_WAKEUP->CON0 & BIT(0)) == 0) {
        exti_event_init(port, EXTI_S_ICH_WKUP, trigger_edge, cbfun, irq_priority); //上升沿触发
        return 0;//ok
    } else if ((JL_WAKEUP->CON0 & BIT(1)) == 0) {
        exti_event_init(port, EXTI_S_IRFLT, trigger_edge, cbfun, irq_priority); //上升沿触发
        return 0;//ok
    }
    log_error("No free channels !\n");
    ASSERT(0, "ICH_WKUP/IRFLT are occupied!");
    return -1;//fail
}
/**
 * @brief 关掉该引脚的中断功能
 * @param port 引脚号：IO_PORTA_00......
 * @return 0 成功，< 0 失败
 */
int exti_uninit(u32 port)
{
    u8 uninit_cnt = 0;
    for (u8 i = 0; i < EXTI_SOURCE_NUM; i++) {
        if (port == exti_user_port[i]) {
            uninit_cnt++;
            exti_event_close(port, i);
        }
    }
    if (!uninit_cnt) {
        log_error("%s parameter:port error!\n", __func__);
    } else {
        return 0;//ok
    }
    return -1;//fail
}
void exti_change_callback(u32 port, void (*cbfun)(u32, u32, u32))
{
    u8 change_cnt = 0;
    for (u8 i = 0; i < EXTI_SOURCE_NUM; i++) {
        if (port == exti_user_port[i]) {
            change_cnt++;
            exti_irq_cbfun[i] = cbfun;
            log_debug("change_callback:port:%d,exti_s:%d\n", port, i);
        }
    }
    if (!change_cnt) {
        log_error("%s parameter:port error!\n", __func__);
    }
}
void exti_change_en_state(u32 port, u8 exti_en)//exti_en:1:en,0:disable
{
    u8 change_cnt = 0;
    for (u8 i = 0; i < EXTI_SOURCE_NUM; i++) {
        if (port == exti_user_port[i]) {
            change_cnt++;
            if (exti_en) {
                JL_WAKEUP->CON2 |= BIT(i);//清一次pnd
                JL_WAKEUP->CON0 |= BIT(i);//引脚中断使能
                JL_WAKEUP->CON2 |= BIT(i);//清一次pnd
            } else {
                JL_WAKEUP->CON0 &= ~ BIT(i); //引脚中断使能
                JL_WAKEUP->CON2 |= BIT(i);//清一次pnd
            }
            log_debug("change_en_state:port:%d,exti_s:%d\n", port, i);
        }
    }
    if (!change_cnt) {
        log_error("%s parameter:port error!\n", __func__);
    }
}
void exti_change_edge_state(u32 port, u8 edge)
{
    u8 change_cnt = 0;
    for (u8 i = 0; i < EXTI_SOURCE_NUM; i++) {
        if (port == exti_user_port[i]) {
            change_cnt++;
            if (edge != (!!(JL_WAKEUP->CON1 & BIT(i)))) {
                if (edge) {
                    JL_WAKEUP->CON1 |= BIT(i);
                } else {
                    JL_WAKEUP->CON1 &= ~BIT(i);//0:上升沿
                }
            }
            log_debug("change_edge_state:port:%d,exti_s:%d\n", port, i);
        }
    }
    if (!change_cnt) {
        log_error("%s parameter:port error!\n", __func__);
    }
}
u8 exti_get_edge_state(u32 port)
{
    u8 change_cnt = 0;
    u8 edge_res = 0;
    for (u8 i = 0; i < EXTI_SOURCE_NUM; i++) {
        if (port == exti_user_port[i]) {
            change_cnt++;
            if (JL_WAKEUP->CON1 & BIT(i)) { //fall
                edge_res |= 0x02;
            }
            if (!(JL_WAKEUP->CON1 & BIT(i))) { //rise
                edge_res |= 0x01;
            }
            log_debug("get_edge_state:%d,exti_s:%d \n", port, i);
        }
    }
    if (!change_cnt) {
        log_error("%s parameter:port error!\n", __func__);
    }
    return edge_res;
}

/*********************************************************************************************************
 * ******************************           使用举例如下           ***************************************
 * ******************************************************************************************************/
#if 0

void _gpich0(void)
{
    JL_PORTB->OUT ^= BIT(0);
    printf("%s\n", __func__);
}
void _gpich1(void)
{
    JL_PORTB->OUT ^= BIT(1);
    printf("%s\n", __func__);
}

void wdt_clear();
void mdelay(u32 ms);
void exti_test()
{
    printf("\n--func=%s(),%d\n", __FUNCTION__, __LINE__);
    exti_event_init(IO_PORTA_03, EXTI_S_ICH_WKUP, 0, _gpich0, 3); //上升沿触发
    /* exti_event_init(IO_PORTA_04, EXTI_S_IRFLT, 1, _gpich1,3);//下升沿触发 */
    /* exti_event_close(IO_PORTA_04, EXTI_S_IRFLT); */
    while (1) {
        printf("-----cpu enter idle\n");
        printf("-----cpu exit idle\n");
        mdelay(2000);
        wdt_clear();
    }
}
#endif

