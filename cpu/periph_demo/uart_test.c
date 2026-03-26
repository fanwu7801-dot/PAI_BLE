#include "system/includes.h"
#include "../uart_v1.h"
/* #define LOG_TAG_CONST   UART */
#define LOG_TAG         "[UART_test]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
#define LOG_DUMP_ENABLE
#include "debug.h"


OS_SEM uart_callback;
static void uart_irq_func(int uart_num, enum uart_event_v1 event)
{
    if (event & UART_EVENT_TX_DONE) {
        printf("uart[%d] tx done", uart_num);
    }

    if (event & UART_EVENT_RX_DATA) {
        printf("uart[%d] rx data", uart_num);
    }
    if (event & UART_EVENT_RX_TIMEOUT) {
        printf("uart[%d] rx ot\n", uart_num);
    }

    if (event & UART_EVENT_RX_FIFO_OVF) {
        printf("uart[%d] rx fifo ovf", uart_num);
    }
    int r = os_sem_post(&uart_callback);
}

struct uart_frame {
    u16 crc;
    u16 length;
    u8 data[0];
};
void uart_sync_demo(void *p)
{
    struct uart_config config = {
        .baud_rate = 1000000,
        .tx_pin = IO_PORTG_00,
        .rx_pin = IO_PORTG_01,
        .parity = UART_PARITY_DISABLE,
    };

    void *uart_rx_ptr = dma_malloc(768);
    os_sem_create(&uart_callback, 0);

    struct uart_dma_config dma = {
        .rx_timeout_thresh = 100,
        .frame_size = 32,
        .event_mask = UART_EVENT_RX_DATA | UART_EVENT_RX_FIFO_OVF | UART_EVENT_RX_TIMEOUT,
        .tx_wait_mutex = 0,
        .irq_priority = 3,
        .irq_callback = uart_irq_func,
        .rx_cbuffer = uart_rx_ptr,
        .rx_cbuffer_size = 768,
    };

    int ut = uart_init_new(-1, &config);
    if (ut < 0) {
        printf("init error %d", ut);
    } else {
        printf("init ok %d", ut);
    }
    int r = uart_dma_init(ut, &dma);
    if (r < 0) {
        printf("dma init error %d", ut);
    } else {
        printf("dma init ok %d", ut);
    }

    uart_dump();
    struct uart_frame *frame = (struct uart_frame *)dma_malloc(512);

    while (1) {
        os_sem_pend(&uart_callback, 0);
        r = uart_recv_blocking(ut, frame, 512, 10);
        if (r > 0) {
            log_info("uart rx ok!***len:%d*****\n", r);
            printf_buf((u8 *)frame, r);
            r = uart_send_blocking(ut, frame, r, 20);
        }
    }
}
void uart_demo()
{
    int err = os_task_create(uart_sync_demo, NULL, 31, 512, 0, "periph_demo");
    if (err != OS_ERR_NONE) {
        r_printf("creat fail %x\n", err);
    }
}

