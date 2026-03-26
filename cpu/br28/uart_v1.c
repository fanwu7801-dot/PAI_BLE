#include "../uart_v1.h"
#include "includes.h"
#include "gpio.h"
#include "spinlock.h"
#include "asm/clock.h"
#include "circular_buf.h"
#include "asm/power_interface.h"
// #include "power/include/power_config.h"


// #ifdef SUPPORT_MS_EXTENSIONS
// #pragma data_seg(".uart.data")
// #pragma bss_seg(".uart.data.bss")
// #pragma code_seg(".uart.text")
// #pragma const_seg(".uart.text.const")
// #endif

/* #define LOG_TAG_CONST   UART */
#define LOG_TAG         "[UART]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
#define LOG_DUMP_ENABLE
#include "debug.h"

#define UART_CLK  clk_get("uart")
#define UART_OT_CLK  clk_get("lsb")

#define memory_in_phy(x)	1

struct uart_context {
    JL_UART_TypeDef *uart;
    cbuffer_t cbuf;
    volatile u32 tx_idle;

    struct uart_config uart_config;
    struct uart_flow_ctrl flow_ctrl;
    struct uart_dma_config dma;

    spinlock_t lock;
    OS_MUTEX mutex;
    OS_SEM uart_tx;
    OS_SEM uart_rx;
};

static JL_UART_TypeDef UARTx_POWEROFF[HW_UART_NUM];
static volatile unsigned long g_uart_query_end_msec = 0;             //uart活跃结束时间，判断是否可进低功耗
static volatile u32 g_uart_idle_query_ms = 3;//*10ms

static inline u32 uart_get_jiffies(void)
{
#if 1
    return jiffies;
#endif
}

static void uart_active_reset_idle_time()//*10ms
{
    g_uart_query_end_msec = g_uart_idle_query_ms + uart_get_jiffies();
}
void uart_set_idle_query_wait(u32 ms)
{
    g_uart_idle_query_ms = msecs_to_jiffies(ms);
}


#define UART_ENTER_CRITICAL() \
	spin_lock(&uart_context[uart_num]->lock)


#define UART_EXIT_CRITICAL() \
	spin_unlock(&uart_context[uart_num]->lock)

static struct uart_context *uart_context[HW_UART_NUM];

static JL_UART_TypeDef *uart_table[HW_UART_NUM] = {
    JL_UART0,
    JL_UART1,
    JL_UART2,
};

static u8 uart_irq_idx[HW_UART_NUM] = {
    IRQ_UART0_IDX,
    IRQ_UART1_IDX,
    IRQ_UART2_IDX,
};

static u32 uart_tx_func_table[HW_UART_NUM] = {
    FO_UART0_TX,   //PORT_FUNC_UART0_TX,
    FO_UART1_TX,   //PORT_FUNC_UART1_TX,
    FO_UART2_TX,   //PORT_FUNC_UART2_TX,
};

static u32 uart_rx_func_table[HW_UART_NUM] = {
    PFI_UART0_RX,   //PORT_FUNC_UART0_RX,
    PFI_UART1_RX,   //PORT_FUNC_UART1_RX,
    PFI_UART2_RX,   //PORT_FUNC_UART2_RX,
};
static void uartx_flow_ctl_rts_suspend(uart_dev uart_num);
static void uartx_flow_ctl_rts_resume(uart_dev uart_num);
s32 uart_flow_ctrl_deinit(uart_dev uart_num);
static void uart0_isr();
static void uart1_isr();
static void uart2_isr();
typedef void (*irq_handler)(void);
static irq_handler uart_irq_handler[HW_UART_NUM] = {
    uart0_isr,
    uart1_isr,
    uart2_isr,
};

static u8 uart_is_single_wire(uart_dev uart_num)
{
    return uart_context[uart_num]->uart_config.tx_pin == uart_context[uart_num]->uart_config.rx_pin;
}

static void uart_set_dma_dir(uart_dev uart_num, u32 dir)
{
    if (uart_is_single_wire(uart_num)) {
        if (dir) {
            gpio_set_die(uart_context[uart_num]->uart_config.tx_pin, 1);
            gpio_set_direction(uart_context[uart_num]->uart_config.tx_pin, 1);
            gpio_set_pull_up(uart_context[uart_num]->uart_config.tx_pin, 1);
            gpio_set_pull_down(uart_context[uart_num]->uart_config.tx_pin, 0);
            // gpio_set_mode(IO_PORT_SPILT(uart_context[uart_num]->uart_config.tx_pin), PORT_INPUT_PULLUP_10K);

            UART_ENTER_CRITICAL();
            uart_context[uart_num]->uart->CON1 &= ~BIT(4);
            UART_EXIT_CRITICAL();
        } else {
            UART_ENTER_CRITICAL();
            uart_context[uart_num]->uart->CON1 |= BIT(4);
            UART_EXIT_CRITICAL();

            gpio_set_direction(uart_context[uart_num]->uart_config.tx_pin, 0);
            gpio_write(uart_context[uart_num]->uart_config.tx_pin, 1);
            // gpio_set_mode(IO_PORT_SPILT(uart_context[uart_num]->uart_config.tx_pin), PORT_OUTPUT_HIGH);
        }
    }
}

static void uart_fifo_reset(uart_dev uart_num)
{
    JL_UART_TypeDef *uart = uart_context[uart_num]->uart;
    uart->CON1 |= UART_RX_DISABLE;
    uart->RXSADR = (u32)uart_context[uart_num]->dma.rx_cbuffer;
    uart->RXEADR = (u32)(uart_context[uart_num]->dma.rx_cbuffer + uart_context[uart_num]->dma.rx_cbuffer_size);
    cbuf_clear(&uart_context[uart_num]->cbuf);
    uart->CON1 &= ~(UART_RX_DISABLE);
}

enum uart_state uart_dma_rx_reset(uart_dev uart_num)
{
    int os_err = os_mutex_pend(&uart_context[uart_num]->mutex, 0);
    if (os_err) {
        log_error("%s os_mutex_pend error:%d\n", __FUNCTION__, os_err);
        return UART_ERROR_OS_MUTEX;
    }
    UART_ENTER_CRITICAL();
    uart_fifo_reset(uart_num);
    os_sem_set(&uart_context[uart_num]->uart_rx, 0);
    UART_EXIT_CRITICAL();
    os_mutex_post(&uart_context[uart_num]->mutex);
    return UART_OK;
}
static void uart_emit_event(uart_dev uart_num, enum uart_event_v1 event)
{
    if ((uart_context[uart_num]->dma.event_mask & event) &&
        (uart_context[uart_num]->dma.irq_callback)) {
        uart_context[uart_num]->dma.irq_callback(uart_num, event);
    }
}

static void uart_isr(uart_dev uart_num)
{
    u32 rx_len;

    JL_UART_TypeDef *uart = uart_context[uart_num]->uart;

    UART_ENTER_CRITICAL();
    u32 uart_con0 = uart->CON0 ;
    u32 uart_con1 = uart->CON1 ;
    uart_active_reset_idle_time();
    UART_EXIT_CRITICAL();

    if ((uart_con0 & UART_TX_IE) && (uart_con0 & UART_TX_PND)) {

        UART_ENTER_CRITICAL();
        uart->CON0 |= UART_CLR_TX_PND;
        uart_context[uart_num]->tx_idle = 1;
        UART_EXIT_CRITICAL();
        uart_set_dma_dir(uart_num, 1);

        int r = os_sem_post(&uart_context[uart_num]->uart_tx);
        uart_emit_event(uart_num, UART_EVENT_TX_DONE);
    }

    u32 rx_data = 0;
    if ((uart_con0 & UART_OT_IE) && (uart_con0 & UART_OT_PND)) {
        rx_data = BIT(0);
    }

    if ((uart_con0 & UART_RX_IE) && (uart_con0 & UART_RX_PND)) {
        rx_data |= BIT(1);
    }

    if (rx_data) {

        UART_ENTER_CRITICAL();
        uart->CON0 |= UART_RX_FLUSH;
        if (rx_data & BIT(0)) {
            uart->CON0 |= UART_CLR_OT_PND;
        }
        uart->CON0 |= UART_CLR_RX_PND;
        asm volatile("csync");
        rx_len = uart->HRXCNT;

        UART_EXIT_CRITICAL();
        u32 fifo_ovf = !cbuf_is_write_able(&uart_context[uart_num]->cbuf, rx_len);
        if (fifo_ovf) {
            uart_fifo_reset(uart_num);
            uart_emit_event(uart_num, UART_EVENT_RX_FIFO_OVF);
            rx_len = 0;

        } else {
            cbuf_write_updata(&uart_context[uart_num]->cbuf, rx_len);
        }

        uartx_flow_ctl_rts_suspend(uart_num);

        if (rx_len) {

            int r = os_sem_post(&uart_context[uart_num]->uart_rx);

            if (rx_data & BIT(0)) {
                uart_emit_event(uart_num, UART_EVENT_RX_TIMEOUT);
            } else {
                uart_emit_event(uart_num, UART_EVENT_RX_DATA);
            }

        }
    }
    asm("csync");
}

___interrupt
static void uart0_isr()
{
    uart_isr(0);
}

___interrupt
static void uart1_isr()
{
    uart_isr(1);
}

___interrupt
static void uart2_isr()
{
    uart_isr(2);
}
static void uart_refresh_otcnt(uart_dev uart_num)
{
    JL_UART_TypeDef *uart = uart_context[uart_num]->uart;

    if (uart->CON0 & UART_RX_DMA) {
        const u32 otcnt = uart_context[uart_num]->dma.rx_timeout_thresh * (UART_OT_CLK / 1000000);
        uart->OTCNT = otcnt;
        UARTx_POWEROFF[uart_num].OTCNT = otcnt;
    }
}

static u32 uart_is_idle(uart_dev ut_num);
//return 实际波特率,0:error; >0:ok
s32 uart_set_baudrate(uart_dev uart_num, u32 baud_rate)
{
    ASSERT(uart_num < HW_UART_NUM, "uart num %d < %d", uart_num, HW_UART_NUM);
    JL_UART_TypeDef *uart = uart_context[uart_num]->uart;

    u32 b3 = ((UART_CLK + baud_rate / 2) / baud_rate) / 3 - 1;
    u32 b4 = ((UART_CLK + baud_rate / 2) / baud_rate) / 4 - 1;
    if ((b3 == 0xffffffff) && (b4 == 0xffffffff)) {
        return -13;//error
    }
    b3 &= 0xffff;
    b4 &= 0xffff;
    u32 b3_baud = UART_CLK / ((b3 + 1) * 3);
    u32 b4_baud = UART_CLK / ((b4 + 1) * 4);
    s32 ret = 0;

    int os_err;
    if (uart_is_idle(uart_num) == 0) {
        os_err = os_mutex_pend(&uart_context[uart_num]->mutex, 0);
        if (os_err) {
            log_error("%s os_mutex_pend error:%d\n", __FUNCTION__, os_err);
            return UART_ERROR_OS_MUTEX;
        }
    }
    uart->CON0 &= ~(UART_TX_EN | UART_RX_EN);
    uart->CON0 |= UART_CLR_ALL_PND;

    if ((__builtin_abs((int)(b3_baud - baud_rate))) > (__builtin_abs((int)(b4_baud - baud_rate)))) { //ABS_FUN
        uart->CON0 &= ~ BIT(4);
        uart->BAUD = b4;
        UARTx_POWEROFF[uart_num].BAUD = b4;
        ret = b4_baud;
    } else {
        uart->CON0 |= BIT(4);
        uart->BAUD = b3;
        UARTx_POWEROFF[uart_num].BAUD = b3;
        ret = b3_baud;
    }

    uart_refresh_otcnt(uart_num);

    uart->CON0 |= (UART_TX_EN | UART_RX_EN);
    if (uart_is_idle(uart_num) == 0) {
        os_err = os_mutex_post(&uart_context[uart_num]->mutex);
    }
    return ret;
}

s32 uart_set_rx_timeout_thresh(uart_dev uart_num, u32 timeout_thresh)
{
    ASSERT(uart_num < HW_UART_NUM, "uart num %d < %d", uart_num, HW_UART_NUM);

    JL_UART_TypeDef *uart = uart_context[uart_num]->uart;

    int os_err = os_mutex_pend(&uart_context[uart_num]->mutex, 0);
    if (os_err) {
        log_error("%s os_mutex_pend error:%d\n", __FUNCTION__, os_err);
        return UART_ERROR_OS_MUTEX;
    }
    uart->CON0 &= ~(UART_TX_EN | UART_RX_EN);
    uart->CON0 |= UART_CLR_ALL_PND;

    if (uart->CON0 & UART_RX_DMA) {
        uart_context[uart_num]->dma.rx_timeout_thresh = timeout_thresh;
        uart_refresh_otcnt(uart_num);
    }
    uart->CON0 |= (UART_TX_EN | UART_RX_EN);
    os_err = os_mutex_post(&uart_context[uart_num]->mutex);
    return 0;
}

static void clock_critical_enter()
{

}
static void clock_critical_exit()
{
    for (u8 i = 0; i < HW_UART_NUM; i++) {
        if (uart_context[i]) {
            uart_refresh_otcnt(i);
        }
    }
}
CLOCK_CRITICAL_HANDLE_REG(uart_v1_ot, clock_critical_enter, clock_critical_exit);

static u32 uart_is_idle(uart_dev ut_num)
{
    switch (ut_num) {
    case 0 :
        return !(JL_UART0->CON0 & UART_TX_EN);
    case 1 :
        return !(JL_UART1->CON0 & UART_TX_EN);
    case 2 :
        return !(JL_UART2->CON0 & UART_TX_EN);
    default :
        break;
    }
    return 0;
}
s32 uart_init_new(uart_dev _uart_num, const struct uart_config *config)
{
    /* ASSERT(uart_num < HW_UART_NUM, "uart num %d < %d", uart_num, HW_UART_NUM); */
    int uart_num = _uart_num;
    if (uart_num == -1) {
        for (int i = 0; i < HW_UART_NUM; i++) {
            if (uart_is_idle(i)) {
                uart_num = i;
                break;
            }
        }
    } else {
        if (uart_is_idle(uart_num) == 0) {
            return UART_ERROR_BUSY;
        }
    }
    if (uart_num == -1) {
        return UART_ERROR_BUSY;
    }

    if (uart_context[uart_num] != NULL) {
        return UART_ERROR_BUSY;
    }
    uart_context[uart_num] = (struct uart_context *) zalloc(sizeof(struct uart_context));
    if (uart_context[uart_num] == NULL) {
        return UART_ERROR_MALLOC;
    }

    memcpy(&uart_context[uart_num]->uart_config, config, sizeof(struct uart_config));

    uart_context[uart_num]->uart = uart_table[uart_num];
    memset(&uart_context[uart_num]->flow_ctrl, 0xff, sizeof(struct uart_flow_ctrl));
    memset(&uart_context[uart_num]->dma, 0, sizeof(struct uart_dma_config));

    JL_UART_TypeDef *uart = uart_context[uart_num]->uart;

    uart->CON0 = UART_CLR_ALL_PND;
    uart->CON1 = 0;
    uart->CON2 = 0;

    u16 tx_pin = config->tx_pin ;
    u16 rx_pin = config->rx_pin ;

    if (tx_pin != (u16) - 1) {
        gpio_set_direction(tx_pin, 0);
        gpio_set_pull_up(tx_pin, 0);
        gpio_set_pull_down(tx_pin, 0);
        gpio_set_output_value(tx_pin, 1);

        gpio_set_fun_output_port(tx_pin,  uart_tx_func_table[uart_num], 1, 1, 1);

        // gpio_set_mode(IO_PORT_SPILT(tx_pin), PORT_OUTPUT_HIGH);
        // gpio_set_function(IO_PORT_SPILT(tx_pin), uart_tx_func_table[uart_num]);
    }

    if (rx_pin != (u16) - 1) {

        gpio_set_die(rx_pin, 1);
        gpio_set_direction(rx_pin, 1);
        gpio_set_pull_up(rx_pin, 1);
        gpio_set_pull_down(rx_pin, 0);

        gpio_set_fun_input_port(rx_pin,  uart_rx_func_table[uart_num], 1);

        // gpio_set_mode(IO_PORT_SPILT(rx_pin), PORT_INPUT_PULLUP_10K);
        // gpio_set_function(IO_PORT_SPILT(rx_pin), uart_rx_func_table[uart_num]);
    }

    if (rx_pin == tx_pin) {
        uart_set_dma_dir(uart_num, 1);
    }

    int r = os_mutex_create(&uart_context[uart_num]->mutex);
    if (r) {
        goto __exit_error;
    }
    r = os_sem_create(&uart_context[uart_num]->uart_tx, 1);
    if (r) {
        goto __exit_error;
    }
    r = os_sem_create(&uart_context[uart_num]->uart_rx, 0);
    if (r) {
        goto __exit_error;
    }

    uart_context[uart_num]->tx_idle = 1;
    uart_set_baudrate(uart_num, uart_context[uart_num]->uart_config.baud_rate);

    /* printf("uart_v1:%d open ok!\n", uart_num); */
    return uart_num;
__exit_error:
    free(uart_context[uart_num]);
    uart_context[uart_num] = NULL;
    return UART_ERROR_OS_CREATE;
}

s32 uart_deinit(uart_dev uart_num)
{
    ASSERT(uart_num < HW_UART_NUM, "uart num %d < %d", uart_num, HW_UART_NUM);

    if (uart_context[uart_num] == NULL) {
        return UART_ERROR_CONTEXT;
    }

    JL_UART_TypeDef *uart = uart_context[uart_num]->uart;

    unrequest_irq(uart_irq_idx[uart_num]);
    uart->CON0 = UART_CLR_ALL_PND;
    uart->CON1 = 0;
    uart->CON2 = 0;
    uart_flow_ctrl_deinit(uart_num);

    os_sem_post(&uart_context[uart_num]->uart_tx);//
    uart_context[uart_num]->tx_idle = 1;
    os_sem_post(&uart_context[uart_num]->uart_rx);//

    //关闭io
    u16 tx_pin = uart_context[uart_num]->uart_config.tx_pin ;
    u16 rx_pin = uart_context[uart_num]->uart_config.rx_pin;
    if (tx_pin != (u16) - 1) {
        gpio_set_direction(tx_pin, 1);
        gpio_set_pull_up(tx_pin, 0);
        gpio_set_pull_down(tx_pin, 0);
        gpio_set_die(tx_pin, 0);

        gpio_disable_fun_output_port(tx_pin);

        /* gpio_disable_function(IO_PORT_SPILT(tx_pin), uart_tx_func_table[uart_num]); */
    }
    if (rx_pin != (u16) - 1) {
        gpio_set_direction(rx_pin, 1);
        gpio_set_pull_up(rx_pin, 0);
        gpio_set_pull_down(rx_pin, 0);
        gpio_set_die(rx_pin, 0);

        gpio_disable_fun_input_port(uart_rx_func_table[uart_num]);

        /* gpio_disable_function(IO_PORT_SPILT(rx_pin), uart_rx_func_table[uart_num]); */
    }

    int os_err = os_mutex_pend(&uart_context[uart_num]->mutex, 0);
    os_err = os_mutex_post(&uart_context[uart_num]->mutex);
    os_mutex_del(&uart_context[uart_num]->mutex, 1);
    os_sem_del(&uart_context[uart_num]->uart_tx, 1);
    os_sem_del(&uart_context[uart_num]->uart_rx, 1);

    free(uart_context[uart_num]);
    uart_context[uart_num] = NULL;
    return 0;
}

s32 uart_dma_init(uart_dev uart_num, const struct uart_dma_config *dma_config)
{
    ASSERT(uart_num < HW_UART_NUM, "uart num %d < %d", uart_num, HW_UART_NUM);

    if (uart_context[uart_num] == NULL) {
        return UART_ERROR_CONTEXT;
    }
    JL_UART_TypeDef *uart = uart_context[uart_num]->uart;
    memcpy(&uart_context[uart_num]->dma, dma_config, sizeof(struct uart_dma_config));


    int os_err = os_mutex_pend(&uart_context[uart_num]->mutex, 0);
    if (os_err) {
        log_error("%s os_mutex_pend error:%d\n", __FUNCTION__, os_err);
        return UART_ERROR_OS_MUTEX;
    }

    if (dma_config->rx_cbuffer) {
        if (memory_in_phy(dma_config->rx_cbuffer) == 0) {
            log_error("uart dma buffer must used dma_malloc()");
            return -10;
        }

        if (dma_config->rx_cbuffer_size % 4) {
            log_error("uart rx buffer size align 4");
            return UART_ERROR_BUF_ALIGN4;
        }

        if ((u32)dma_config->rx_cbuffer % 4) {
            log_error("uart rx buffer addr align 4");
            return UART_ERROR_BUF_ALIGN4;
        }

        cbuf_init(&uart_context[uart_num]->cbuf, dma_config->rx_cbuffer, dma_config->rx_cbuffer_size);

        uart->RXSADR = (u32)dma_config->rx_cbuffer;
        uart->RXEADR = (u32)(dma_config->rx_cbuffer + dma_config->rx_cbuffer_size);
        uart->RXCNT = dma_config->frame_size;
        uart->CON0 |= UART_RX_IE | UART_OT_IE | UART_RX_DMA ;
    }

    uart->CON0 |= UART_TX_IE;
    request_irq(uart_irq_idx[uart_num], dma_config->irq_priority, uart_irq_handler[uart_num], 0);
    os_err = os_mutex_post(&uart_context[uart_num]->mutex);
    uart_set_baudrate(uart_num, uart_context[uart_num]->uart_config.baud_rate);

    if (uart_is_single_wire(uart_num)) {//单线支持中断调用,不互斥
        uart_context[uart_num]->dma.tx_wait_mutex = 0;
    }
    return 0;
}
s32 uart_putbyte_new(uart_dev uart_num, u32 a)
{
    if (uart_context[uart_num] == NULL) {
        return UART_ERROR_CONTEXT;
    }
    /* os_mutex_pend(&uart_context[uart_num]->mutex, 0); */
    u32 i = 0x10000;
    JL_UART_TypeDef *uart = uart_context[uart_num]->uart;

    if (uart_is_idle(uart_num)) {
        return UART_ERROR_CLOSE;
    }
    uart_set_dma_dir(uart_num, 0);
    if (uart_is_single_wire(uart_num) == 0) {
        while (((uart->CON0 & UART_TX_PND) == 0) && (0 != i)) {  //TX IDLE
            i--;
        }
    }
    /* UART_ENTER_CRITICAL(); */
    uart->CON0 |= UART_CLR_TX_PND;  //清Tx pending
    uart->BUF = a;
    if (uart_is_single_wire(uart_num)) {
        while (((uart->CON0 & UART_TX_PND) == 0) && (0 != i)) {  //TX IDLE
            i--;
        }
        uart->CON0 |= UART_CLR_TX_PND;  //清Tx pending
        uart_set_dma_dir(uart_num, 1);
    }
    /* UART_EXIT_CRITICAL(); */
    /* os_mutex_post(&uart_context[uart_num]->mutex); */
    return 0;
}

// timeout_us = 0 就一直等
s32 uart_wait_tx_idle(uart_dev uart_num, u32 timeout_ms)
{
    int timeout_tick = msecs_to_jiffies(timeout_ms);
    if (uart_context[uart_num] == NULL) {
        return UART_ERROR_CONTEXT;
    }
    JL_UART_TypeDef *uart = uart_context[uart_num]->uart;

    u32 tx_ie = uart->CON0 & UART_TX_IE;
    /* u32 curr_msec = jiffies_msec(); */
    /* u32 end_msec = jiffies_offset2msec(curr_msec, timeout_ms); */
    u32 _timeout = timeout_ms + uart_get_jiffies();

    while (cpu_in_irq() || cpu_irq_disabled() || (tx_ie == 0)) {
        if (uart_context[uart_num]->tx_idle) {
            uart_set_dma_dir(uart_num, 1);
            return 0;
        }

        if (uart->CON0 & UART_TX_PND) {
            if (tx_ie) {
                uart->CON0 |= UART_CLR_TX_PND;
            }
            uart_context[uart_num]->tx_idle = 1;
        }

        /* curr_msec = jiffies_msec(); */
        /*  */
        /* int offset = jiffies_msec2offset(curr_msec, end_msec); */
        /* if (offset < 0) { */
        /*     return 0; */
        /* } */
        if (timeout_ms && time_before(_timeout, uart_get_jiffies())) {
            return UART_ERROR_TIMEOUT;
        }
        if (uart_is_idle(uart_num)) {
            log_error("uart%d be closed!", uart_num);
            return UART_ERROR_CLOSE;
        }
    }
    int r = os_sem_pend(&uart_context[uart_num]->uart_tx, timeout_tick);
    return r;
}
static s32 uart_dma_tx(uart_dev uart_num, const void *src, s32 size)
{
    if (uart_context[uart_num] == NULL) {
        return UART_ERROR_CONTEXT;
    }
    if (memory_in_phy(src) == 0) {
        log_error("uart dma buffer must used dma_malloc()");
        return -10;
    }

    JL_UART_TypeDef *uart = uart_context[uart_num]->uart;
    os_sem_set(&uart_context[uart_num]->uart_tx, 0);
    uart_set_dma_dir(uart_num, 0);

    UART_ENTER_CRITICAL();
    uart_context[uart_num]->tx_idle = 0;
    uart->CON0 |= UART_CLR_TX_PND;
    uart_active_reset_idle_time();
    UART_EXIT_CRITICAL();
    asm("csync");
    uart->TXADR = (u32)src;
    uart->TXCNT = size;

    return size;
}
s32 uart_send_bytes(uart_dev uart_num, const void *src, u32 size)
{
    if (size == 0) {
        return 0;
    }
    ASSERT(uart_num < HW_UART_NUM, "uart num %d < %d", uart_num, HW_UART_NUM);

    if (uart_context[uart_num] == NULL) {
        return UART_ERROR_CONTEXT;
    }
    if (uart_is_idle(uart_num)) {
        log_error("uart%d be closed!", uart_num);
        return UART_ERROR_CLOSE;
    }

    JL_UART_TypeDef *uart = uart_context[uart_num]->uart;

    int os_err;
    if (uart_context[uart_num]->dma.tx_wait_mutex) {
        os_err = os_mutex_pend(&uart_context[uart_num]->mutex, 0);
        if (os_err) {
            log_error("%s os_mutex_pend error:%d\n", __FUNCTION__, os_err);
            return UART_ERROR_OS_MUTEX;
        }
    } else {
        uart_wait_tx_idle(uart_num, 0);//wait tx finish
    }

    size = uart_dma_tx(uart_num, src, size);
    if (uart_context[uart_num]->dma.tx_wait_mutex) {
        os_err = os_mutex_post(&uart_context[uart_num]->mutex);
    }
    return size;
}


//阻塞式dma发送数据
// 非阻塞发送：若TX忙则直接返回UART_ERROR_BUSY
s32 uart_send_bytes_nonblock(uart_dev uart_num, const void *src, u32 size)
{
    if (size == 0) {
        return 0;
    }
    ASSERT(uart_num < HW_UART_NUM, "uart num %d < %d", uart_num, HW_UART_NUM);

    if (uart_context[uart_num] == NULL) {
        return UART_ERROR_CONTEXT;
    }
    if (uart_is_idle(uart_num)) {
        log_error("uart%d be closed!", uart_num);
        return UART_ERROR_CLOSE;
    }
    if (memory_in_phy(src) == 0) {
        log_error("uart dma buffer must used dma_malloc()");
        return -10;
    }

    JL_UART_TypeDef *uart = uart_context[uart_num]->uart;

    UART_ENTER_CRITICAL();
    if (uart_context[uart_num]->tx_idle == 0) {
        UART_EXIT_CRITICAL();
        return UART_ERROR_BUSY;
    }
    os_sem_set(&uart_context[uart_num]->uart_tx, 0);
    uart_set_dma_dir(uart_num, 0);
    uart_context[uart_num]->tx_idle = 0;
    uart->CON0 |= UART_CLR_TX_PND;
    uart_active_reset_idle_time();
    UART_EXIT_CRITICAL();
    asm("csync");
    uart->TXADR = (u32)src;
    uart->TXCNT = size;

    return size;
}

s32 uart_send_blocking(uart_dev uart_num, const void *src, u32 size, u32 timeout_ms)
{
    if (size == 0) {
        return 0;
    }
    ASSERT(uart_num < HW_UART_NUM, "uart num %d < %d", uart_num, HW_UART_NUM);

    if (uart_context[uart_num] == NULL) {
        return UART_ERROR_CONTEXT;
    }

    if (uart_is_idle(uart_num)) {
        log_error("uart%d be closed!", uart_num);
        return UART_ERROR_CLOSE;
    }
    JL_UART_TypeDef *uart = uart_context[uart_num]->uart;

    int os_err;
    if (uart_context[uart_num]->dma.tx_wait_mutex) {
        os_err = os_mutex_pend(&uart_context[uart_num]->mutex, 0);
        if (os_err) {
            log_error("%s os_mutex_pend error:%d\n", __FUNCTION__, os_err);
            return UART_ERROR_OS_MUTEX;
        }
    }

    size = uart_dma_tx(uart_num, src, size);
    uart_wait_tx_idle(uart_num, timeout_ms);

    if (uart_context[uart_num]->dma.tx_wait_mutex) {
        os_err = os_mutex_post(&uart_context[uart_num]->mutex);
    }
    return size;
}

s32 uart_is_send_complete(uart_dev uart_num)//0:busy; 1:idle
{
    ASSERT(uart_num < HW_UART_NUM, "uart num %d < %d", uart_num, HW_UART_NUM);
    if (uart_context[uart_num] == NULL) {
        return UART_ERROR_CONTEXT;//error
    }
    return uart_context[uart_num]->tx_idle;//0:busy; 1:idle
}

//return <0:error; 其他:ok
s32 uart_getbyte_new(uart_dev uart_num)
{
    if (uart_is_idle(uart_num)) {
        log_error("uart%d be closed!", uart_num);
        return UART_ERROR_CLOSE;
    }
    int os_err = os_mutex_pend(&uart_context[uart_num]->mutex, 0);
    if (os_err) {
        log_error("%s os_mutex_pend error:%d\n", __FUNCTION__, os_err);
        return UART_ERROR_OS_MUTEX;
    }
    JL_UART_TypeDef *uart = uart_context[uart_num]->uart;
    uart_set_dma_dir(uart_num, 1);
    while (!(uart->CON0 & UART_RX_PND)) {
    }
    int byte = uart->BUF;
    UART_ENTER_CRITICAL();
    uart->CON0 |= UART_CLR_RX_PND;
    UART_EXIT_CRITICAL();
    __asm__ volatile("csync");
    os_mutex_post(&uart_context[uart_num]->mutex);
    return byte;
}

s32 uart_recv_bytes(uart_dev uart_num, void *buf, u32 len)
{
    if (uart_is_idle(uart_num)) {
        log_error("uart%d be closed!", uart_num);
        return UART_ERROR_CLOSE;
    }

    u32 cbuf_cur_len = cbuf_get_data_size(&uart_context[uart_num]->cbuf);
    if (cbuf_cur_len) {
        len = MIN(len, cbuf_cur_len);
        len = cbuf_read(&uart_context[uart_num]->cbuf, buf, len);
    } else {
        len = 0;
    }

    uartx_flow_ctl_rts_resume(uart_num);
    return len;
}

s32 uart_recv_blocking(uart_dev uart_num, void *buf, u32 len, u32 timeout_ms)
{
    if (uart_is_idle(uart_num)) {
        log_error("uart%d be closed!", uart_num);
        return UART_ERROR_CLOSE;
    }
    int os_err = os_mutex_pend(&uart_context[uart_num]->mutex, 0);
    if (os_err) {
        log_error("%s os_mutex_pend error:%d\n", __FUNCTION__, os_err);
        return UART_ERROR_OS_MUTEX;
    }
    u32 timeout_tick = msecs_to_jiffies(timeout_ms);

    if (timeout_tick == 0) {
        timeout_tick = -1;
    }

    u32 offset = 0;
    while ((offset < len) && timeout_tick) {
        int r = uart_recv_bytes(uart_num, buf, len - offset);
        if (uart_is_idle(uart_num)) {
            log_error("uart%d be closed!", uart_num);
            return UART_ERROR_CLOSE;
        }
        buf += r;
        offset += r;
        uart_active_reset_idle_time();
        r  = os_sem_pend(&uart_context[uart_num]->uart_rx, 1);
        timeout_tick --;
    }
    os_err = os_mutex_post(&uart_context[uart_num]->mutex);
    return offset;
}

s32 uart_recv_blocking_timeout(uart_dev uart_num, void *buf)
{
    if (uart_is_idle(uart_num)) {
        log_error("uart%d be closed!", uart_num);
        return UART_ERROR_CLOSE;
    }
    int os_err = os_mutex_pend(&uart_context[uart_num]->mutex, 0);
    if (os_err) {
        log_error("%s os_mutex_pend error:%d\n", __FUNCTION__, os_err);
        return UART_ERROR_OS_MUTEX;
    }

    uart_active_reset_idle_time();
    int r  = os_sem_pend(&uart_context[uart_num]->uart_rx, 0);
    r = uart_recv_bytes(uart_num, buf, 0xffff);
    uart_active_reset_idle_time();

    os_err = os_mutex_post(&uart_context[uart_num]->mutex);
    return r;
}


static u32 uart_rts_func_table[HW_UART_NUM] = {
#ifdef CONFIG_CPU_BR27
    PORT_FUNC_UART0_RTS,
    PORT_FUNC_UART1_RTS,
    PORT_FUNC_UART2_RTS,
#else
    (u32) - 1,
    FO_UART1_RTS,   //PORT_FUNC_UART1_RTS,
    (u32) - 1,
#endif
};
static u32 uart_cts_func_table[HW_UART_NUM] = {
#ifdef CONFIG_CPU_BR27
    PORT_FUNC_UART0_CTS,
    PORT_FUNC_UART1_CTS,
    PORT_FUNC_UART2_CTS,
#else
    (u32) - 1,
    PFI_UART1_CTS,  //PORT_FUNC_UART1_CTS,
    (u32) - 1,
#endif
};

s32 uart_flow_ctrl_init(uart_dev uart_num, const struct uart_flow_ctrl *flow_ctrl)
{
    if (flow_ctrl == NULL) {
        return -11;
    }

#ifdef CONFIG_CPU_BR27
    ASSERT(uart_num < HW_UART_NUM, "uart num %d < %d", uart_num, HW_UART_NUM);
#else
    ASSERT(uart_num == 1, "uart num %d no rts-cts", uart_num);
#endif

    JL_UART_TypeDef *uart = uart_context[uart_num]->uart;
    memcpy(&uart_context[uart_num]->flow_ctrl, flow_ctrl, sizeof(struct uart_flow_ctrl));

    uart->CON1 = UART_FLOW_INIT;

    u32 rts_io = flow_ctrl->rts_pin;
    u32 cts_io = flow_ctrl->cts_pin;
    //RTS
    if (rts_io < IO_PORT_MAX) {
#if 0//硬件rts
        if (flow_ctrl->rts_idle_level) {
#ifdef UART_RTS_INV
            uart->CON1 &= ~ UART_RTS_INV;
            gpio_set_direction(rts_io, 0);
            gpio_write(rts_io, 1);
            /* gpio_set_mode(IO_PORT_SPILT(rts_io), PORT_OUTPUT_HIGH); */
#else
            gpio_set_direction(rts_io, 0);
            gpio_write(rts_io, 0);
            /* gpio_set_mode(IO_PORT_SPILT(rts_io), PORT_OUTPUT_LOW); */
#endif
        } else {
#ifdef UART_RTS_INV
            uart->CON1 |= UART_RTS_INV;
#endif
            gpio_set_direction(rts_io, 0);
            gpio_write(rts_io, 0);
            /* gpio_set_mode(IO_PORT_SPILT(rts_io), PORT_OUTPUT_LOW); */
        }

        /* gpio_set_function(IO_PORT_SPILT(rts_io), uart_rts_func_table[uart_num]); */
        gpio_set_fun_output_port(rts_io,  uart_rts_func_table[uart_num], 1, 1, 1);
        uart->CON1 |= (UART_CLR_RTS_PND | UART_RTS_EN);
#else //软件rts

        if (flow_ctrl->rts_idle_level) {
            gpio_set_direction(rts_io, 0);
            gpio_set_pull_up(rts_io, 0);
            gpio_set_pull_down(rts_io, 0);
            gpio_write(rts_io, 1);

            // gpio_set_mode(IO_PORT_SPILT(rts_io), PORT_OUTPUT_HIGH);
        } else {

            gpio_set_direction(rts_io, 0);
            gpio_set_pull_up(rts_io, 0);
            gpio_set_pull_down(rts_io, 0);
            gpio_write(rts_io, 0);

            // gpio_set_mode(IO_PORT_SPILT(rts_io), PORT_OUTPUT_LOW);
        }
#endif
    }
    //CTS
    if (cts_io < IO_PORT_MAX) {
        if (flow_ctrl->cts_idle_level) {
#ifdef UART_CTS_INV
            uart->CON1 &= ~ UART_CTS_INV;
            gpio_set_die(cts_io, 1);
            gpio_set_direction(cts_io, 1);
            gpio_set_pull_up(cts_io, 1);
            gpio_set_pull_down(cts_io, 0);
            /* gpio_set_mode(IO_PORT_SPILT(cts_io), PORT_INPUT_PULLUP_10K); */
#else
            gpio_set_die(cts_io, 1);
            gpio_set_direction(cts_io, 1);
            gpio_set_pull_up(cts_io, 0);
            gpio_set_pull_down(cts_io, 1);

            // gpio_set_mode(IO_PORT_SPILT(cts_io), PORT_INPUT_PULLDOWN_10K);
#endif
        } else {
#ifdef UART_CTS_INV
            uart->CON1 |= UART_CTS_INV;
#endif
            gpio_set_die(cts_io, 1);
            gpio_set_direction(cts_io, 1);
            gpio_set_pull_up(cts_io, 0);
            gpio_set_pull_down(cts_io, 1);

            // gpio_set_mode(IO_PORT_SPILT(cts_io), PORT_INPUT_PULLDOWN_10K);
        }
        /* gpio_set_function(IO_PORT_SPILT(cts_io), uart_cts_func_table[uart_num]); */
        gpio_set_fun_input_port(cts_io,  uart_cts_func_table[uart_num], 1);

        /* request_irq(uart_irq_idx[uart_num], 3, uart_irq_handler[uart_num], 0); */
        /* uart->CON1 |= UART_CTS_IE;//CTS 中断 en */
        uart->CON1 |= (UART_CLR_CTS_PND | UART_CTS_EN);
    }
    log_info("cts rts con1:0x%x", uart->CON1);

    return 0;//ok
}

s32 uart_flow_ctrl_deinit(uart_dev uart_num)
{
    ASSERT(uart_num < HW_UART_NUM, "uart num %d < %d", uart_num, HW_UART_NUM);

    if (((uart_context[uart_num]->flow_ctrl.rts_pin) == 0xffff) &&
        ((uart_context[uart_num]->flow_ctrl.cts_pin) == 0xffff)) {
        return -12;
    }

    JL_UART_TypeDef *uart = uart_context[uart_num]->uart;
    gpio_set_die(uart_context[uart_num]->flow_ctrl.rts_pin, 0);
    gpio_set_direction(uart_context[uart_num]->flow_ctrl.rts_pin, 1);
    gpio_disable_fun_output_port(uart_context[uart_num]->flow_ctrl.rts_pin);
    gpio_set_die(uart_context[uart_num]->flow_ctrl.cts_pin, 0);
    gpio_set_direction(uart_context[uart_num]->flow_ctrl.cts_pin, 1);
    gpio_set_pull_up(uart_context[uart_num]->flow_ctrl.cts_pin, 0);
    gpio_set_pull_down(uart_context[uart_num]->flow_ctrl.cts_pin, 0);
    gpio_disable_fun_input_port(uart_cts_func_table[uart_num]);
    memset(&uart_context[uart_num]->flow_ctrl, 0, sizeof(struct uart_flow_ctrl));


    uart->CON1 &= ~(UART_CTS_IE | UART_CTS_EN | UART_RTS_EN);
    uart->CON1 |= (UART_CLR_CTS_PND | UART_CLR_RTS_PND);
    return 0;//ok
}

static void uartx_flow_ctl_rts_suspend(uart_dev uart_num)
{
    ASSERT(uart_num < HW_UART_NUM, "uart num %d < %d", uart_num, HW_UART_NUM);

    if ((uart_context[uart_num]->flow_ctrl.rts_pin) == 0xffff) {
        return;
    }
    u32 cbuf_cur_len = cbuf_get_data_size(&uart_context[uart_num]->cbuf);
    // 调整流量控制阈值，当缓冲区使用超过 80% 时才暂停接收
    if (cbuf_cur_len < (uart_context[uart_num]->flow_ctrl.rx_thresh * uart_context[uart_num]->dma.rx_cbuffer_size) / 100) {
        return;
    }

    JL_UART_TypeDef *uart = uart_context[uart_num]->uart;
    if (!(uart->CON1 & UART_RTS_EN)) {
        gpio_write(uart_context[uart_num]->flow_ctrl.rts_pin, !(uart_context[uart_num]->flow_ctrl.rts_idle_level));
    }
    UART_ENTER_CRITICAL();
    uart->CON1 |= UART_RX_DISABLE;
    UART_EXIT_CRITICAL();
}

static void uartx_flow_ctl_rts_resume(uart_dev uart_num)
{
    ASSERT(uart_num < HW_UART_NUM, "uart num %d < %d", uart_num, HW_UART_NUM);

    if ((uart_context[uart_num]->flow_ctrl.rts_pin) == 0xffff) {
        return;
    }
    u32 cbuf_cur_len = cbuf_get_data_size(&uart_context[uart_num]->cbuf);
    // 调整流量控制阈值，当缓冲区使用低于 50% 时恢复接收
    if (cbuf_cur_len > (uart_context[uart_num]->flow_ctrl.rx_thresh * uart_context[uart_num]->dma.rx_cbuffer_size) / 100) {
        return;
    }

    JL_UART_TypeDef *uart = uart_context[uart_num]->uart;
    UART_ENTER_CRITICAL();
    uart->CON1 &= ~UART_RX_DISABLE;
    uart->CON1 |= UART_CLR_RTS_PND;
    UART_EXIT_CRITICAL();
    if (uart->CON1 & UART_RTS_EN) {
    } else {
        gpio_write(uart_context[uart_num]->flow_ctrl.rts_pin, uart_context[uart_num]->flow_ctrl.rts_idle_level);
    }
}


void uart_dump()
{
    int i = 0;
    log_debug("[-------------Uart Dump-----------]");
    for (; i < HW_UART_NUM; i++) {
        if (uart_is_idle(i)) {
            log_debug("uart:%d no used!\n", i);
        } else {
            if (uart_context[i] == NULL) {
                log_debug("uart:%d no use uart_init!\n", i);
                continue;
            }
            JL_UART_TypeDef *uart = uart_context[i]->uart;
            log_debug("uart:%d cfg info:", i);
            log_debug("uart:%d tx io:0x%x", i, uart_context[i]->uart_config.tx_pin);
            log_debug("uart:%d rx io:0x%x", i, uart_context[i]->uart_config.rx_pin);
            log_debug("uart:%d baud:%d", i, uart_context[i]->uart_config.baud_rate);
            //parity
            log_debug("uart:%d no parity!", i);
            //dma
            if (uart->CON0 & UART_RX_DMA) {
                log_debug("uart:%d dma enable:", i);
                log_debug("uart:%d dma cbuf size:%d byte", i, uart_context[i]->dma.rx_cbuffer_size);
                log_debug("uart:%d dma cbuf rx len:%d byte", i, cbuf_get_data_size(&uart_context[i]->cbuf));
                log_debug("uart:%d dma frame_size:%d byte", i, uart_context[i]->dma.frame_size);
                log_debug("uart:%d dma rx_timeout_thresh:%d us", i, uart_context[i]->dma.rx_timeout_thresh);
                log_debug("uart:%d dma event_mask:%s %s %s %s %s", i
                          , (uart_context[i]->dma.event_mask & UART_EVENT_RX_DATA) ? "RX_DATA" : ""
                          , (uart_context[i]->dma.event_mask & UART_EVENT_RX_FIFO_OVF) ? "| RX_FIFO_OVF" : ""
                          , (uart_context[i]->dma.event_mask & UART_EVENT_RX_TIMEOUT) ? "| RX_TIMEOUT" : ""
                          , (uart_context[i]->dma.event_mask & UART_EVENT_PARITY_ERR) ? "| PARITY_ERR" : ""
                          , (uart_context[i]->dma.event_mask & UART_EVENT_TX_DONE) ? "| TX_DONE" : "");
            } else {
                log_debug("uart:%d dma disable!", i);
            }
            //cts rts
            if ((uart->CON1 & UART_RTS_EN) || (uart->CON1 & UART_CTS_EN)) {
                log_debug("uart:%d flow_ctrl enable:", i);
                log_debug("uart:%d cts io:0x%x", i, uart_context[i]->flow_ctrl.cts_pin);
                log_debug("uart:%d rts io:0x%x", i, uart_context[i]->flow_ctrl.rts_pin);
                log_debug("uart:%d flow_ctrl cts_idle_level:%s", i, uart_context[i]->flow_ctrl.cts_idle_level ? "high" : "low");
                log_debug("uart:%d flow_ctrl rts_idle_level:%s", i, uart_context[i]->flow_ctrl.rts_idle_level ? "high" : "low");
                log_debug("uart:%d flow_ctrl rx_thresh:%d%%(rx_cbuffer_size)", i, uart_context[i]->flow_ctrl.rx_thresh);
            } else {
                log_debug("uart:%d flow_ctrl disable!", i);
            }

            log_debug("UART%d_CON0      :0x%08x", i, uart_table[i]->CON0);
            log_debug("UART%d_CON1      :0x%08x", i, uart_table[i]->CON1);
            log_debug("UART%d_CON2      :0x%08x\n", i, uart_table[i]->CON2);
            /* log_debug("UART%d_BAUD      :0x%08x", i, uart_table[i]->BAUD);//only write */
            /* log_debug("UART%d_BUF       :0x%08x", i, uart_table[i]->BUF); */
            /* log_debug("UART%d_OTCNT     :0x%08x", i, uart_table[i]->OTCNT);//only write */
            /* log_debug("UART%d_TXADR     :0x%08x", i, uart_table[i]->TXADR);//only write */
            /* log_debug("UART%d_TXCNT     :0x%08x", i, uart_table[i]->TXCNT);//only write */
            /* log_debug("UART%d_RXSADR    :0x%08x", i, uart_table[i]->RXSADR);//only write */
            /* log_debug("UART%d_RXEADR    :0x%08x", i, uart_table[i]->RXEADR);//only write */
            /* log_debug("UART%d_RXCNT     :0x%08x", i, uart_table[i]->RXCNT);//only write */
            /* log_debug("UART%d_HRXCNT    :0x%08x", i, uart_table[i]->HRXCNT); */
        }
    }
}


/*-----------------------------------------------------------*/
/* #include "include/app_config.h" */
/* #if (TCFG_LOWPOWER_LOWPOWER_SEL == DEEP_SLEEP_EN) */
#if 1

static u8 uart_enter_deepsleep(void)
{
    int i = 0;
    JL_UART_TypeDef *uart;
    for (; i < HW_UART_NUM; i++) {
        if (uart_context[i] == NULL) {
            continue;
        }
        uart = uart_context[i]->uart;

        UARTx_POWEROFF[i].CON0 =  uart->CON0;
        UARTx_POWEROFF[i].CON1 =  uart->CON1;
        UARTx_POWEROFF[i].CON2 =  uart->CON2;

        /* UARTx_POWEROFF[i].OTCNT =  uart_context[i]->dma.rx_timeout_thresh * (UART_OT_CLK / 1000000); */
        /* UARTx_POWEROFF[i].TXADR =  uart->TXADR; */
        //UARTx_POWEROFF[i].RXSADR = (u32)uart_context[i]->dma.rx_cbuffer;
        //UARTx_POWEROFF[i].RXEADR = (u32)(uart_context[i]->dma.rx_cbuffer + uart_context[i]->dma.rx_cbuffer_size);
        //UARTx_POWEROFF[i].RXCNT = uart_context[i]->dma.frame_size;

        u32 cbuf_cur_len = cbuf_get_data_size(&uart_context[i]->cbuf);
        if (cbuf_cur_len) {
            log_info("<warning>:There are %d unused data in the cbuf that will be cleared!", cbuf_cur_len);
            /* log_info_hexdump((u8 *)cbuf_get_readptr(&uart_context[i]->cbuf), cbuf_cur_len); */
        }
        //cbuf_clear(&uart_context[i]->cbuf);
    }

    return 0;
}

static u8 uart_exit_deepsleep(void)
{
    int i = 0;
    JL_UART_TypeDef *uart;
    for (; i < HW_UART_NUM; i++) {
        if (uart_context[i] == NULL) {
            continue;
        }
        uart = uart_context[i]->uart;

        uart->CON1	 =  UARTx_POWEROFF[i].CON1;
        uart->CON2	 =  UARTx_POWEROFF[i].CON2;
        uart->BAUD	 =  UARTx_POWEROFF[i].BAUD;
        uart->OTCNT	 =  UARTx_POWEROFF[i].OTCNT;
        ///* uart->TXADR	 =  UARTx_POWEROFF[i].TXADR; */
        //uart->RXSADR =	UARTx_POWEROFF[i].RXSADR;
        //uart->RXEADR =	UARTx_POWEROFF[i].RXEADR;
        //uart->RXCNT =	UARTx_POWEROFF[i].RXCNT;
        uart_fifo_reset(i);
        uart->RXCNT = uart_context[i]->dma.frame_size;
        uart->CON0   =  UARTx_POWEROFF[i].CON0;
        uart->CON0 |= UART_CLR_ALL_PND;
    }

    return 0;
}


DEEPSLEEP_TARGET_REGISTER(uart_v1) = {
    .name   = "uart_v1",
    .enter  = uart_enter_deepsleep,
    .exit   = uart_exit_deepsleep,
};
#endif

/* --------------------------------------------------------------------------*/
/**
 * @brief 串口模块是否可进低功耗查询函数
 *
 * @return 0：不可进
 *         1：可进
 */
/* ----------------------------------------------------------------------------*/
static u8 uart_idle_query(void)
{
    if (g_uart_query_end_msec) {
        if (time_before(g_uart_query_end_msec, uart_get_jiffies())) {
            g_uart_query_end_msec = 0;
            return 1;
        }
        return 0;
    }

    return 1;
}
REGISTER_LP_TARGET(uart_driver_target) = {
    .name = "uart",
    .is_idle = uart_idle_query,
};
