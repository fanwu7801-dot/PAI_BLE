#include "app_config.h"

#ifdef AUDIO_PCM_DEBUG
#include "system/includes.h"
#include "../uart_v1.h"
#include "uartPcmSender.h"

static u8 uart_cbuf[32] __attribute__((aligned(4)));
int pcm_uart_num = -1;

#define PCM_UART_EVENT 0
#if PCM_UART_EVENT
//设备事件响应demo
void uart_event_handler(struct sys_event *e)
{
    u8 uart_rxbuf[12] = {0};
    u8 uart_txbuf[12] = {0};
    const uart_bus_t *_uart_bus;
    u32 uart_rxcnt = 0;
    u8 i = 0;

    if (0) {//!strcmp(e->arg, "uart_rx_overflow")) {
        if (e->u.dev.event == DEVICE_EVENT_CHANGE) {
            printf("uart event: %s\n", e->arg);
            _uart_bus = (const uart_bus_t *)e->u.dev.value;
            uart_rxcnt = _uart_bus->read(uart_rxbuf, sizeof(uart_rxbuf), 0);
            if (uart_rxcnt) {
                g_printf("rx:%s", uart_rxbuf);
            }
        }
    }
    if (0) { //(!strcmp(e->arg, "uart_rx_outtime")) {
        if (e->u.dev.event == DEVICE_EVENT_CHANGE) {
            printf("uart event: %s\n", e->arg);
            _uart_bus = (const uart_bus_t *)e->u.dev.value;
            uart_rxcnt = _uart_bus->read(uart_rxbuf, sizeof(uart_rxbuf), 0);
            if (uart_rxcnt) {
                printf("get_buffer:\n");
                for (int i = 0; i < uart_rxcnt; i++) {
                    putbyte(uart_rxbuf[i]);
                    if (i % 16 == 15) {
                        putchar('\n');
                    }
                }
                if (uart_rxcnt % 16) {
                    putchar('\n');
                }
                _uart_bus->write(uart_rxbuf, uart_rxcnt);
            }
            printf("uart out\n");
        }
    }
}
SYS_EVENT_HANDLER(SYS_DEVICE_EVENT, uart_event_handler, 0);
#endif

void gpio_change(void *priv);
static void uart_isr_hook(int uart_num, enum uart_event_v1 event)
{
    struct sys_event e;
    u8 uart_rxbuf[12] = {0};
    u32 uart_rxcnt = 0;
    if (event & UART_EVENT_TX_DONE) {
        /* printf("uart[%d] tx done", uart_num); */
        /* putchar('T'); */
        /* gpio_change(0); */
    }

    if (event & UART_EVENT_RX_DATA) {
        printf("uart[%d] rx data", uart_num);
#if PCM_UART_EVENT
        e.type = SYS_DEVICE_EVENT;
        e.arg = "uart_rx_overflow";
        e.u.dev.event = DEVICE_EVENT_CHANGE;
        e.u.dev.value = uart_num;
        sys_event_notify(&e);
#else
        uart_rxcnt = uart_recv_bytes(uart_num, uart_rxbuf, sizeof(uart_rxbuf));
        if (uart_rxcnt) {
            g_printf("rx:%s", uart_rxbuf);
        }
#endif
    }
    if (event & UART_EVENT_RX_TIMEOUT) {
        printf("uart[%d] rx ot\n", uart_num);
#if PCM_UART_EVENT
        e.type = SYS_DEVICE_EVENT;
        e.arg = "uart_rx_outtime";
        e.u.dev.event = DEVICE_EVENT_CHANGE;
        e.u.dev.value = uart_num;
        sys_event_notify(&e);
#else
        uart_rxcnt = uart_recv_bytes(uart_num, uart_rxbuf, sizeof(uart_rxbuf));
        if (uart_rxcnt) {
            printf("get_buffer:\n");
            for (int i = 0; i < uart_rxcnt; i++) {
                putbyte(uart_rxbuf[i]);
                if (i % 16 == 15) {
                    putchar('\n');
                }
            }
            if (uart_rxcnt % 16) {
                putchar('\n');
            }
            uart_send_bytes(uart_num, uart_rxbuf, uart_rxcnt);
        }
        printf("uart out\n");
#endif
    }

    if (event & UART_EVENT_RX_FIFO_OVF) {
        printf("uart[%d] rx fifo ovf", uart_num);
    }
}

void uartSendData(void *buf, u16 len) 			//发送数据的接口。
{
    if (pcm_uart_num >= 0) {
        uart_send_blocking(pcm_uart_num, buf, len, 20);
    }
}

/* char *TickPage = "uart online"; */
/* void uartTickSend(void *priv){ */
/* if(uartTickSendFlag){ */
/* r_printf("sizeof:%d\n", sizeof(TickPage)); */
/* uartSendData(TickPage, sizeof(TickPage)); */
/* } */
/* } */

void uartSendInit()
{
    struct uart_config config = {
        .baud_rate = PCM_UART1_BAUDRATE,
        .tx_pin = PCM_UART1_TX_PORT,
        .rx_pin = PCM_UART1_RX_PORT,
        .parity = UART_PARITY_DISABLE,
    };

    struct uart_dma_config dma = {
        .rx_timeout_thresh = 1000,//us
        .frame_size = 6,
        .event_mask = UART_EVENT_RX_DATA | UART_EVENT_RX_FIFO_OVF | UART_EVENT_RX_TIMEOUT,
        .tx_wait_mutex = 0,
        .irq_priority = 3,
        .irq_callback = uart_isr_hook,
        .rx_cbuffer = uart_cbuf,
        .rx_cbuffer_size = 32,
    };

    r_printf("uart_dev_open() ...\n");
    pcm_uart_num = uart_init_new(-1, &config);
    if (pcm_uart_num < 0) {
        printf("init error %d", pcm_uart_num);
    } else {
        printf("init ok %d", pcm_uart_num);
    }
    int r = uart_dma_init(pcm_uart_num, &dma);
    r_printf("comming %s,%d\n", __func__, __LINE__);
    if (r < 0) {
        printf("dma init error %d", pcm_uart_num);
        r_printf("false\n");
    } else {
        printf("dma init ok %d", pcm_uart_num);
        r_printf("success\n");
        gpio_set_hd(PCM_UART1_TX_PORT, 1);
        gpio_set_hd0(PCM_UART1_TX_PORT, 1);
        //os_task_create(uart_u_task, NULL, 31, 512, 0, "uart_u_task");
    }

    uart_dump();
}

#endif
