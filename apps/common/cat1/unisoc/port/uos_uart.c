#include "system/timer.h"
#include "asm/clock.h"
#include "asm/uart.h"
#include "generic/gpio.h"
#include "spinlock.h"
#include "app_config.h"
#include "uos_uart.h"
#include "os/os_api.h"

#define uart_clk clk_get("uart")
#define JL_UARTX JL_UART1
#define uartx_data uart1_data
#define IRQ_UARTX_IDX IRQ_UART1_IDX
#define UART_DMA_LEN 544
#define UNISOC_UART_DRIVER_ENABLE (1)

static struct uart_platform_data uart1_data = {
    .tx_pin = IO_PORTG_00, // 串口打印TX引脚选择
    .rx_pin = IO_PORTG_01, // 串口打印RX引脚选择uh
    .baudrate = 115200,    // 串口波特率
    .flags = UART_RX_USE_DMA, // 串口用来打印需要把改参数设置为UART_DEBUG
    .irq = IRQ_UARTX_IDX,
};
struct uart_info {
    u8 rx_cnt: 2;   // 定时器防止系统进入低功耗的标志位
    u8 send_flag: 2; // 发送完成标志位
    u8 first_flag: 1;
    u8 uart_active: 1; //低功耗标志位
    u8 lack_len_flag: 2; // 如果接收不够 开启标志位 直接接收数据足够或者接收超时
    u8 rx_start_flag; // 接收开始标志位
    u16 rx_time_cnt;  // 接收开始计时
    u16 total_len;    // 接收开始后接收到的总的数据长度
    u16 time_id;
    u32 rx_len;
    uos_uint8_t *read_buff_priv;
    uos_uint32_t read_len;
    uart_rx_callback rx_callback_priv;
};
static struct uart_info uart_flag = {
    .uart_active = 1,
    .send_flag = 1,
};
static u8 uart_dma_buf[UART_DMA_LEN] __attribute__((aligned(4))); // DMA接收数据的数组
static void low_power_timing();
void close_uart_pin();


#if UNISOC_UART_DRIVER_ENABLE

typedef enum {
    UART_IDLE = 0,
    UART_TX_START,
    UART_TX_DONE,
    UART_RX_START,
    UART_RX_MIDST,
    UART_RX_DONE,
} uart_state_e;

static uart_state_e uart_tx_sta = UART_IDLE;
static uart_state_e uart_rx_sta = UART_IDLE;
static char unisoc_head_needcheck_flag = 1;

void uart_tx_sta_set(uart_state_e sta)
{
    uart_tx_sta = sta;
}

uart_state_e uart_tx_sta_get(void)
{
    return uart_tx_sta;
}

void uart_rx_sta_set(uart_state_e sta)
{
    uart_rx_sta = sta;
}

uart_state_e uart_rx_sta_get(void)
{
    return uart_rx_sta;
}

void uart_para_reset(void)
{
    uart_flag.lack_len_flag = 0;
    uart_flag.rx_len = 0;
    uart_flag.read_len = 0;
    uart_flag.total_len = 0;
    uart_flag.read_buff_priv = NULL;
}

void uart_unisoc_head_check_set(char set_flag)
{
    unisoc_head_needcheck_flag = set_flag;
}

char uart_unisoc_head_needcheck(void)
{
    return unisoc_head_needcheck_flag;
}

int uart_unisoc_head_check(unsigned char *buf, int len)
{
    int i;

    if (len > UART_DMA_LEN) {
        return -1;
    }

    for (i = 0; i < len; i++) {
        if (buf[i] == 0x7e) { /* check success */
            uart_unisoc_head_check_set(0);
            return i;
        }
    }
    return -1;
}

void uart_rx_timer_handle(void *priv)
{
    printf("uart_rx_timer_handle arrive sta[%d]\n", uart_rx_sta_get());
    if (uart_rx_sta_get() == UART_RX_MIDST) { /* rx timeout, report data */
        uart_rx_sta_set(UART_RX_DONE);
        if (uart_flag.rx_callback_priv) {
            uart_flag.rx_callback_priv(1, 1, uart_flag.read_buff_priv, uart_flag.read_len);
        }
        uart_rx_sta_set(UART_IDLE);
    } else {
        /* err todo */
    }
}

void uart_rx_timer_active(void) /* timer delay 200ms triger */
{
    if (uart_flag.time_id == 0) {
        printf("uart_rx_timer_active add\n");
        uart_flag.time_id = sys_timer_add(NULL, uart_rx_timer_handle, 200);
        /* todo: timer create check */
    } else {
        printf("uart_rx_timer_active modify\n");
        sys_timer_modify(uart_flag.time_id, 200);
    }
}

void uart_rx_timer_deactive(void)
{
    if (uart_flag.time_id) {
        sys_timer_del(uart_flag.time_id);
        uart_flag.time_id = 0;
    }
}

___interrupt void uart_irq_handle(void)
{
    if ((JL_UARTX->CON0 & BIT(2)) && (JL_UARTX->CON0 & BIT(15))) {
        JL_UARTX->CON0 |= BIT(13);
        uart_tx_sta_set(UART_TX_DONE);
    }

    if ((JL_UARTX->CON0 & BIT(3)) && (JL_UARTX->CON0 & BIT(14))) {

        JL_UARTX->CON0 |= BIT(12);
        JL_UARTX->CON0 |= BIT(10);
    }

    if ((JL_UARTX->CON0 & BIT(5)) && (JL_UARTX->CON0 & BIT(11))) {

        if (uart_rx_sta_get() == UART_RX_START) {
            uart_rx_sta_set(UART_RX_MIDST);
        }

        JL_UARTX->CON0 |= BIT(7);
        JL_UARTX->CON0 |= BIT(10);
        JL_UARTX->CON0 |= BIT(12);
        JL_UARTX->RXSADR = (u32)(uart_dma_buf);
        JL_UARTX->RXEADR = (u32)(uart_dma_buf + UART_DMA_LEN);
        uart_flag.rx_len = JL_UARTX->HRXCNT;

        if (uart_rx_sta_get() != UART_IDLE) {
            if (uart_flag.read_buff_priv == NULL) {
                return;
            }

            /* unisoc uart head check start(only working onece after uos_uart_open be called) */
            if (uart_unisoc_head_needcheck()) {
                if (uart_unisoc_head_check(uart_dma_buf, uart_flag.rx_len) < 0) {
                    JL_UARTX->RXSADR = (u32)(uart_dma_buf);
                    uart_flag.rx_len = 0;
                    return;
                }
            }
            /* unisoc uart head check end */

            if (uart_flag.rx_len >= uart_flag.read_len && uart_flag.lack_len_flag == 0) {
                uart_rx_timer_deactive(); /* stop timer chaeck */
                memcpy(uart_flag.read_buff_priv, uart_dma_buf, uart_flag.read_len);
                uart_rx_sta_set(UART_RX_DONE);
                uart_flag.rx_callback_priv(1, 1, uart_flag.read_buff_priv, uart_flag.read_len);
                uart_rx_sta_set(UART_IDLE);
                return;
            } else {
                uart_flag.lack_len_flag = 1;
            }

            if (uart_flag.lack_len_flag) {
                if ((uart_flag.total_len + uart_flag.rx_len) >= uart_flag.read_len) {
                    uart_rx_timer_deactive(); /* stop timer chaeck */
                    memcpy((uart_flag.read_buff_priv + uart_flag.total_len), uart_dma_buf, (uart_flag.read_len - uart_flag.total_len));
                    uart_rx_sta_set(UART_RX_DONE);
                    uart_flag.rx_callback_priv(1, 1, uart_flag.read_buff_priv, uart_flag.read_len);
                    uart_rx_sta_set(UART_IDLE);
                    uart_flag.lack_len_flag = 0;
                    uart_flag.total_len = 0;
                    return; /* avoid exec "total_len += rx_len" */
                } else {
                    memcpy((uart_flag.read_buff_priv + uart_flag.total_len), uart_dma_buf, uart_flag.rx_len);
                    uart_rx_timer_active(); /* start timer chaeck */
                }
            }
            uart_flag.total_len += uart_flag.rx_len;
        }
    }
}

uos_err_t uos_uart_open(void)
{
    u8 err = 0;

    printf("uos_uart version: 2023.12.24 \n");
    err = uart_init_2(1, &uartx_data);
    request_irq(IRQ_UARTX_IDX, 2, uart_irq_handle, 0);
    JL_UARTX->CON0 |= BIT(2);
    if (err != 0) {
        return UOS_ERROR;
    }

    uart_unisoc_head_check_set(1);
    uart_dma_rx_init(1, uart_dma_buf, UART_DMA_LEN);
    JL_UARTX->CON0 &= ~(BIT(5) | BIT(6));
    JL_UARTX->CON0 |= BIT(13) | BIT(12) | BIT(10);
    uart_para_reset();
    uart_rx_sta_set(UART_IDLE);
    uart_tx_sta_set(UART_IDLE);
    return UOS_OK;
}

uos_uint32_t uos_uart_write(uos_uint8_t *write_buff, uos_uint32_t write_size)
{
    int write_cnt = 0;
    ASSERT(((u32)write_buff) % 4 == 0);
    uart_flag.send_flag = 0;
    JL_UARTX->TXADR = (u32)write_buff;
    JL_UARTX->TXCNT = write_size;
    uart_tx_sta_set(UART_TX_START);
    while (uart_tx_sta_get() != UART_TX_DONE) {
        os_time_dly(2);
        write_cnt++;
        if (write_cnt >= 25) {
            printf("uos_uart_write tx timeout!\n");
            break;
        }
    }
    uart_tx_sta_set(UART_IDLE);
    return write_size;
}

uos_err_t uos_uart_read_async(uos_uint8_t *read_buff, uos_uint32_t read_size, uart_rx_callback rx_callback)
{
    uart_para_reset();
    if ((read_buff == NULL) || (rx_callback == NULL) || (read_size == 0)) {
        printf("uos_uart_read_async para err!\n");
        return UOS_ERR_EMPTY;
    }
    uart_flag.read_buff_priv = read_buff;
    uart_flag.read_len = read_size;
    uart_flag.rx_callback_priv = rx_callback;
    uart_rx_sta_set(UART_RX_START);
    JL_UARTX->CON0 |= (BIT(5) | BIT(6));
    JL_UARTX->CON0 |= BIT(12);
    JL_UARTX->CON0 |= BIT(10);
    return UOS_OK;
}

uos_err_t uos_uart_baud_config(uos_uart_baud_enum baud)
{
    u32 uart_baud = 9600;
    switch (baud) {
    case UOS_BAUD_9600:
        uart_baud = 9600;
        break;
    case UOS_BAUD_115200:
        uart_baud = 115200;
        break;
    case UOS_BAUD_921600:
        uart_baud = 921600;
        break;
    case UOS_BAUD_1000000:
        uart_baud = 1000000;
        break;
    case UOS_BAUD_2000000:
        uart_baud = 2000000;
        break;
    }
    uartx_data.baudrate = uart_baud;
    uos_uart_open();
    return UOS_OK;
}

uos_err_t uos_uart_close(void)
{
    uart_rx_timer_deactive(); /* stop timer chaeck */
    close_uart_pin();
    JL_UARTX->CON0 = 0;
    JL_UARTX->CON0 = BIT(13) | BIT(12) | BIT(10);
    uart_rx_sta_set(UART_IDLE);
    uart_tx_sta_set(UART_IDLE);
    return UOS_OK;
}

uos_uint32_t uos_uart_read(uos_uint8_t *read_buff, uos_uint32_t read_size) /* no changed(maybe no used) */
{
    uos_uint32_t len = 0;
    u8 time_cnt = 0;
    while (len < read_size) {
        if (uart_flag.rx_len > 0 && uart_flag.rx_len >= read_size) {
            printf("[msg]>>>>>>>>>>>uart_flag.rx_len=%d", uart_flag.rx_len);
            memcpy(read_buff, uart_dma_buf, read_size);

            uart_flag.rx_len = 0;
            return read_size;
        } else if (uart_flag.rx_len > 0 && uart_flag.rx_len < read_size) {
            printf("[msg]>>>>>>>>>>>uart_flag.rx_len=%d", uart_flag.rx_len);
            if ((len + uart_flag.rx_len) < read_size) {
                memcpy((read_buff + len), uart_dma_buf, uart_flag.rx_len);
                len += uart_flag.rx_len;
                uart_flag.rx_len = 0;
            } else if ((len + uart_flag.rx_len) > read_size) {
                memcpy((read_buff + len), uart_dma_buf, (read_size - len));
                uart_flag.rx_len = 0;
                return read_size;
            }
        }
        if (time_cnt++ < 50) {
            os_time_dly(10);
        } else {
            return len;
        }
    }
    return len;
}

void close_uart_pin(void)
{
    gpio_set_direction(uartx_data.tx_pin, 1);
    gpio_set_direction(uartx_data.rx_pin, 1);
    gpio_set_pull_down(uartx_data.rx_pin, 0);
    gpio_set_pull_down(uartx_data.tx_pin, 0);
    gpio_set_pull_up(uartx_data.rx_pin, 0);
    gpio_set_pull_up(uartx_data.tx_pin, 0);
    gpio_set_dieh(uartx_data.tx_pin, 1);
    gpio_set_dieh(uartx_data.rx_pin, 1);
    gpio_disable_fun_input_port(PFI_UART1_RX);
    gpio_disable_fun_output_port(uartx_data.tx_pin);
}

static u8 uart_idle_query(void)
{
    return ((uart_rx_sta == UART_IDLE) && (uart_tx_sta == UART_IDLE));
}

REGISTER_LP_TARGET(uos_uart_lp_target) = {
    .name = "uos_uart",
    .is_idle = uart_idle_query,
};

#else /* old code */
___interrupt
void uart_irq_handle()
{
    //printf("[msg]>>>>>>>>>>>JL_UARTX->CON0=%x", JL_UARTX->CON0);
    if ((JL_UARTX->CON0 & BIT(2)) && (JL_UARTX->CON0 & BIT(15))) {

        JL_UARTX->CON0 |= BIT(13); // 清除发送标志位
        uart_flag.send_flag = 1;
    }
    if ((JL_UARTX->CON0 & BIT(3)) && (JL_UARTX->CON0 & BIT(14))) {

        JL_UARTX->CON0 |= BIT(12); // 清除发送标志位
        JL_UARTX->CON0 |= BIT(10); // 清除发送标志位
    }
    if ((JL_UARTX->CON0 & BIT(5)) && (JL_UARTX->CON0 & BIT(11))) {
        if (uart_flag.rx_start_flag == 1) {
            uart_flag.rx_cnt = 1;
        }
        JL_UARTX->CON0 |= BIT(7);  // DMA模式
        JL_UARTX->CON0 |= BIT(10); // 清OTCNT PND

        JL_UARTX->CON0 |= BIT(12); // 清RX PND(这里的顺序不能改变，这里要清一次)
        JL_UARTX->RXSADR = (u32)(uart_dma_buf);
        JL_UARTX->RXEADR = (u32)(uart_dma_buf + UART_DMA_LEN);
        uart_flag.rx_len = JL_UARTX->HRXCNT; // 读当前串口接收数据的个数
        if (uart_flag.rx_start_flag) {
            if (uart_flag.read_buff_priv == NULL) {
                return;
            }
            //printf("[msg]>>>>>>>>>>>rx_len=%d", rx_len);

            if (uart_flag.rx_len >= uart_flag.read_len && uart_flag.lack_len_flag == 0) {
                //printf("\n [ERROR] %s -[yuyu] %d\n", __FUNCTION__, __LINE__);
                memcpy(uart_flag.read_buff_priv, uart_dma_buf, uart_flag.read_len);
                uart_flag.rx_start_flag = 0;
                uart_flag.rx_callback_priv(1, 1, uart_flag.read_buff_priv, uart_flag.read_len);
                return;
            } else {
                uart_flag.lack_len_flag = 1;
            }
            if (uart_flag.lack_len_flag) {
                if ((uart_flag.total_len + uart_flag.rx_len) > uart_flag.read_len) {
                    //printf("\n [ERROR] %s -[yuyu] %d\n", __FUNCTION__, __LINE__);
                    memcpy((uart_flag.read_buff_priv + uart_flag.total_len), uart_dma_buf, (uart_flag.read_len - uart_flag.total_len));
                    uart_flag.rx_callback_priv(1, 1, uart_flag.read_buff_priv, uart_flag.read_len);
                    uart_flag.rx_start_flag = 0;
                    uart_flag.lack_len_flag = 0;
                    uart_flag.rx_time_cnt = 0;
                    uart_flag.total_len = 0;
                } else {
                    //printf("\n [ERROR] %s -[yuyu] %d\n", __FUNCTION__, __LINE__);
                    memcpy((uart_flag.read_buff_priv + uart_flag.total_len), uart_dma_buf, uart_flag.rx_len);
                }
            }
            uart_flag.total_len += uart_flag.rx_len;
        }
    }

    if (uart_flag.first_flag == 0) {
        // 上电时候 可能串口会给干扰到接收到一些错误的信号，这里就是滤除一下
        uart_flag.first_flag = 1;
        uart_flag.rx_cnt = 0;
        uart_flag.uart_active = 1;
        JL_UARTX->RXSADR = (u32)(uart_dma_buf);
        uart_flag.rx_len = 0;
    }
    //printf("\n [ERROR] %s -[yuyu] %d\n", __FUNCTION__, __LINE__);
}
uos_err_t uos_uart_open(void)
{
    u8 err = 0;
    err = uart_init_2(1, &uartx_data);                 // 初始化串口
    request_irq(IRQ_UARTX_IDX, 2, uart_irq_handle, 0); // 设置中断处理函数
    JL_UARTX->CON0 |= BIT(2);                          // 打开发送中断
    if (err != 0) {
        return UOS_ERROR;
    }
    /* JL_UARTX->CON0|=BIT(1); */
    uart_dma_rx_init(1, uart_dma_buf, UART_DMA_LEN); // 打开DMA接收
    JL_UARTX->CON0 &= ~(BIT(5) | BIT(6));            // 关闭DMA先
    JL_UARTX->CON0 |= BIT(13) | BIT(12) | BIT(10);   // 清除一些标志位 RX TX OT

    return UOS_OK;
}
uos_uint32_t uos_uart_write(uos_uint8_t *write_buff, uos_uint32_t write_size)
{
    int i = 0;
    u8 write_buf1[5] = {0xaa, 0xbb, 0xcc};
    u8 write_cnt = 0;
    ASSERT(((u32)write_buff) % 4 == 0); // 4byte对齐
    uart_flag.send_flag = 0;
    JL_UARTX->TXADR = (u32)write_buff;
    JL_UARTX->TXCNT = write_size;
    /* while(!(JL_UARTX->CON0 & BIT(15))); */
    /* JL_UARTX->CON0 |= BIT(13);  //清Tx pending */

    uart_flag.uart_active = 0;
#if 0
    if (uart_flag.time_id == 0) {

        uart_flag.time_id = sys_timer_add(NULL, low_power_timing, 100);
        printf("sys_timer_add=%d", uart_flag.time_id);
    }
#endif

    while (!uart_flag.send_flag) { // 等待发送完成
        os_time_dly(10);
        write_cnt++;
        if (write_cnt >= 5) {
            printf("over time!!");
            break;
        }
    }
    //send_flag = 0;
    uart_flag.uart_active = 1;
    return write_size;

    /* return 0; */
}
uos_err_t uos_uart_read_async(uos_uint8_t *read_buff, uos_uint32_t read_size, uart_rx_callback rx_callback)
{
    u8 err;
    /* if(rx_len==0) */
    if (read_buff != NULL) {
        uart_flag.read_buff_priv = read_buff;
    } else {
        return UOS_ERR_EMPTY;
    }
    uart_flag.read_len = read_size;
    uart_flag.total_len = 0;
    uart_flag.rx_len = 0;
    if (rx_callback != NULL) {
        uart_flag.rx_callback_priv = rx_callback;
    } else {
        return UOS_ERR_EMPTY;
    }
    /* return UOS_ERR_EMPTY; */
    uart_flag.rx_cnt = 1;
    uart_flag.rx_start_flag = 1;
    JL_UARTX->CON0 |= (BIT(5) | BIT(6)); // 打开DMA先
    JL_UARTX->CON0 |= BIT(12);           // 清RX PND(这里的顺序不能改变，这里要清一次)
    JL_UARTX->CON0 |= BIT(10);
    if (uart_flag.time_id == 0) {
        uart_flag.time_id = sys_timer_add(NULL, low_power_timing, 100);
    }
    //printf("uart read!");
    /* put_buf(uart_dma_buf,rx_len); */
    /* memcpy(read_buff,uart_dma_buf,read_size); */
    /* rx_len=0; */
    /* write_text(); */
    /* rx_callback(1,1,read_buff,read_size); */
    return UOS_OK;
}


uos_err_t uos_uart_baud_config(uos_uart_baud_enum baud)
{
    u32 uart_baud = 9600;
    switch (baud) {
    case UOS_BAUD_9600:
        uart_baud = 9600;
        break;
    case UOS_BAUD_115200:
        uart_baud = 115200;
        break;
    case UOS_BAUD_921600:
        uart_baud = 921600;
        break;
    case UOS_BAUD_1000000:
        uart_baud = 1000000;
        break;
    case UOS_BAUD_2000000:
        uart_baud = 2000000;
        break;
    }
    uartx_data.baudrate = uart_baud;
    uos_uart_open();
    //JL_UARTX->BAUD = (uart_clk / uart_baud) / 4 - 1;

    return UOS_OK;
}
uos_err_t uos_uart_close(void)
{
    if (uart_flag.time_id != 0) {
        sys_timer_del(uart_flag.time_id);
        uart_flag.time_id = 0;
    }
    close_uart_pin();
    //gpio_disable_fun_input_port(PFI_UART1_RX);
    //gpio_disable_fun_output_port(uartx_data.tx_pin);
    JL_UARTX->CON0 = 0;
    JL_UARTX->CON0 = BIT(13) | BIT(12) | BIT(10);
    return UOS_OK;
}
uos_uint32_t uos_uart_read(uos_uint8_t *read_buff, uos_uint32_t read_size)
{
    uos_uint32_t len = 0;
    u8 time_cnt = 0;
    while (len < read_size) {
        if (uart_flag.rx_len > 0 && uart_flag.rx_len >= read_size) {
            printf("[msg]>>>>>>>>>>>uart_flag.rx_len=%d", uart_flag.rx_len);
            memcpy(read_buff, uart_dma_buf, read_size);

            uart_flag.rx_len = 0;
            return read_size;
        } else if (uart_flag.rx_len > 0 && uart_flag.rx_len < read_size) {
            printf("[msg]>>>>>>>>>>>uart_flag.rx_len=%d", uart_flag.rx_len);
            if ((len + uart_flag.rx_len) < read_size) {
                memcpy((read_buff + len), uart_dma_buf, uart_flag.rx_len);
                len += uart_flag.rx_len;
                uart_flag.rx_len = 0;
            } else if ((len + uart_flag.rx_len) > read_size) {
                memcpy((read_buff + len), uart_dma_buf, (read_size - len));
                uart_flag.rx_len = 0;
                return read_size;
            }
        }
        if (time_cnt++ < 50) {
            os_time_dly(10);
        } else {
            return len;
        }
    }
    return len;
}
#if 0
//功能还没实现
uos_err_t uos_uart_write_async(uos_uint8_t *write_buff, uos_uint32_t write_size, uart_tx_callback tx_callback)
{

    printf("[msg]>>>>>>>>>>>JL_UARTX->CON0=%x", JL_UARTX->CON0);
    if ((JL_UARTX->CON0 & BIT(15)) != 0);   //TX IDLE
    JL_UARTX->TXADR = (u32)write_buff;
    JL_UARTX->TXCNT = write_size;
    if ((JL_UARTX->CON0 & BIT(15)) != 0) {
        JL_UARTX->CON0 |= BIT(13);    //清Tx pending
    }

    return UOS_OK;
}
#endif
void UART_TX_PIN_CHANGE(u32 gpio)
{
    if (gpio < IO_PORT_MAX) {
        // crossbar
        gpio_disable_fun_output_port(uartx_data.tx_pin);
        uartx_data.tx_pin = gpio;
        gpio_direction_output(uartx_data.tx_pin, 1);
        gpio_set_fun_output_port(uartx_data.tx_pin, FO_UART1_TX, 1, 1, LOW_POWER_FREE);
    }
}
void UART_RX_PIN_CHANGE(u32 gpio)
{
    if (gpio < IO_PORT_MAX) {
        // crossbar
        gpio_disable_fun_input_port(PFI_UART1_RX);
        uartx_data.rx_pin = gpio;
        gpio_direction_input(uartx_data.rx_pin);
        gpio_set_pull_up(uartx_data.rx_pin, 1);
        gpio_set_die(uartx_data.rx_pin, 1);
        gpio_set_fun_input_port(uartx_data.rx_pin, PFI_UART1_RX, LOW_POWER_FREE);
    }
}
// 接收到消息要保持500ms不能进入低功耗
static void low_power_timing()
{
    static u8 time_cnt = 0;
    static u8 cnt_flag = 0;
    if (uart_flag.rx_cnt == 1) {
        time_cnt = 0;
        cnt_flag = 1;
        uart_flag.uart_active = 0;
        uart_flag.rx_cnt = 0;
    }
    if (cnt_flag || uart_flag.rx_start_flag == 0) {
        time_cnt++;
        if (time_cnt >= 5) {
            cnt_flag = 0;
            uart_flag.uart_active = 1;
        }
    }
    if (uart_flag.rx_start_flag) {
        //printf("[msg]>>>>>>>>>>>rx_time_cnt=%d", rx_time_cnt);
        uart_flag.rx_time_cnt++;
        if (uart_flag.rx_time_cnt >= 50) {

            uart_flag.rx_time_cnt = 0;   // 计时
            uart_flag.rx_start_flag = 0; // 计时开始标志位
            uart_flag.lack_len_flag = 0;
            uart_flag.uart_active = 1;
            if (uart_flag.rx_callback_priv != NULL) {
                uart_flag.rx_callback_priv(1, 1, uart_flag.read_buff_priv, uart_flag.total_len);    // 接收函数超时回调
            }
            uart_flag.total_len = 0;
            uart_flag.rx_len = 0;
        }
    } else {
        uart_flag.rx_time_cnt = 0;
    }
#if 0
    if (uart_flag.send_flag == 0 && uart_flag.uart_active == 0) {
        write_time_cnt++;
        if (write_time_cnt >= 5) {
            printf("write err!");
            write_ot_flag = 1;
            write_time_cnt = 0;

        }
    }
#endif
    if (uart_flag.uart_active == 1 && uart_flag.rx_start_flag == 0) {
        printf("CLOSE TIME!");
        sys_timer_del(uart_flag.time_id);
        uart_flag.time_id = 0;
    }
}
void close_uart_pin()
{
    /*把PIN脚设为高阻态*/
    gpio_set_direction(uartx_data.tx_pin, 1);
    gpio_set_direction(uartx_data.rx_pin, 1);
    gpio_set_pull_down(uartx_data.rx_pin, 0);
    gpio_set_pull_down(uartx_data.tx_pin, 0);
    gpio_set_pull_up(uartx_data.rx_pin, 0);
    gpio_set_pull_up(uartx_data.tx_pin, 0);
    gpio_set_dieh(uartx_data.tx_pin, 1);
    gpio_set_dieh(uartx_data.rx_pin, 1);
    /*释放映射*/
    gpio_disable_fun_input_port(PFI_UART1_RX);
    gpio_disable_fun_output_port(uartx_data.tx_pin);

}
static u8 uos_get_uart_state()
{
    u8 state = uart_flag.uart_active;
    return state;

}
static u8 uart_idle_query(void)
{
    return uos_get_uart_state();
}
REGISTER_LP_TARGET(uos_uart_lp_target) = {
    .name = "uos_uart",
    .is_idle = uart_idle_query,
};


#endif //-- end of old code

u8 read_buf[100];
u8 write_buf[] = "ZHANRUI-UART";
uart_rx_callback text_demo(uos_uint16_t uart_module, UartStatusEnum status, uos_uint8_t *rx_buff, uos_uint16_t rx_size);
void uart_demo_init()//打开串口
{
    uos_uart_open();
}

void uart_read_buf()//非阻塞读
{
    uos_uart_read_async(read_buf, 20, text_demo);
}
void uart_write_buf()//阻塞写
{
    uos_uart_write(write_buf, sizeof(write_buf));
}
void set_uart_baud()//设置波特率
{
    uos_uart_baud_config(UOS_BAUD_9600);
}
uart_rx_callback text_demo(uos_uint16_t uart_module, UartStatusEnum status, uos_uint8_t *rx_buff, uos_uint16_t rx_size)//读完成回调函数
{
    put_buf(rx_buff, rx_size);
    return 0;
}

void close_uart()
{
    uos_uart_close();
}

#if 1
typedef enum {
    UOS_PIN_AF_NONE,
    UOS_PIN_AF_GPIO,
    UOS_PIN_AF_UART,
} uos_pin_af_e;

extern uos_err_t uos_gpio_uart_change(uos_pin_af_e pin_af);
extern void cit_sleep(unsigned int time_ms);
unsigned char uart_test_rx_buf[100];
unsigned char uart_test_tx_buf[100] = {
    0x7E, 0X7E, 1, 2, 3, 4, 5, 6, 7, 8, 0X7E
};


void uart_test_rx_cb(uos_uint16_t uart_module,
                     UartStatusEnum status,
                     uos_uint8_t *rx_buff,
                     uos_uint16_t rx_size)
{
    printf("[uart1] uart rx len[%d]\n", rx_size);
    put_buf(rx_buff, rx_size);
}


void uart_pin_change_test(void)
{
    printf("\n\n\n\n ---[uart_pin_change_test]---\n\n\n");
    while (1) {

        uos_gpio_uart_change(UOS_PIN_AF_GPIO); //cfg to gpio
        printf("[change_test] change to gpio\n");
        /* gpio_ high&low */

        uos_gpio_write(IO_PORTG_00, 0);
        printf("[change_test] PG0 = 0\n");
        cit_sleep(1000);
        uos_gpio_write(IO_PORTG_00, 1);
        printf("[change_test] PG0 = 1\n");
        cit_sleep(1000);
        uos_gpio_write(IO_PORTG_00, 0);
        printf("[change_test] PG0 = 0\n");
        cit_sleep(1000);
        uos_gpio_write(IO_PORTG_00, 1);
        printf("[change_test] PG0 = 1\n");
        cit_sleep(1000);
        uos_gpio_write(IO_PORTG_00, 0);
        printf("[change_test] PG0 = 0\n");
        cit_sleep(1000);
        uos_gpio_write(IO_PORTG_00, 1);
        printf("[change_test] PG0 = 1\n");
        cit_sleep(1000);
        uos_gpio_write(IO_PORTG_00, 0);
        printf("[change_test] PG0 = 0\n");
        cit_sleep(1000);
        uos_gpio_write(IO_PORTG_00, 1);
        printf("[change_test] PG0 = 1\n");
        cit_sleep(1000);
        uos_gpio_write(IO_PORTG_00, 0);
        printf("[change_test] PG0 = 0\n");
        cit_sleep(1000);
        uos_gpio_write(IO_PORTG_00, 1);
        printf("[change_test] PG0 = 1\n");
        cit_sleep(1000);
        uos_gpio_write(IO_PORTG_00, 0);
        printf("[change_test] PG0 = 0\n");
        cit_sleep(1000);
        uos_gpio_write(IO_PORTG_00, 1);
        printf("[change_test] PG0 = 1\n");
        cit_sleep(1000);
        uos_gpio_write(IO_PORTG_00, 0);
        printf("[change_test] PG0 = 0\n");
        cit_sleep(1000);
        uos_gpio_write(IO_PORTG_00, 1);
        printf("[change_test] PG0 = 1\n");
        cit_sleep(1000);

        cit_sleep(2000);
        uos_gpio_uart_change(UOS_PIN_AF_UART); //cfg to gpio
        printf("\n\n\n[change_test] change to uart.........\n");
        uos_uart_read_async(uart_test_rx_buf, 11, uart_test_rx_cb);
        uos_uart_write(uart_test_tx_buf, 11);
        printf("[change_test] uart data send\n");
        cit_sleep(5000);

    }
}
#endif
