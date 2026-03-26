#include "system/includes.h"
#include "app_config.h"
#include "aic_cfg.h"
#include "asm/gpio.h"
#include "asm/power_interface.h"
#include "../uart_v1.h"
#ifdef UART_SELF_TEST
#include "gpio_wakeup.h"
#endif

#if TCFG_CAT1_AICXTEK_ENABLE

#define TEST_BUF_SIZE        4096
#define RX_CBUF_SIZE        4096
#define RX_FRAME_LENGTH       3072
#define AIC_UART_NUM           1

//#define UART_SELF_TEST
//#define UART_POWNERON_AICXTEK

#ifdef UART_SELF_TEST
static u8 aic_uart_test_buffer[TEST_BUF_SIZE] __attribute__((aligned(4)));
#endif

struct uart_hardware_abstract {
    int uart_dev;
    OS_SEM sem;
    u8 *uart_phy_cbuf;
};

struct uart_hardware_abstract uha = {.uart_dev = -1};

static void uart_isr_hook(uart_dev uart_num, enum uart_event_v1 event)
{
    u8 rx_flag = 0;
    if (event & UART_EVENT_TX_DONE) {
        /* printf("uart[%d] tx done", uart_num); */
    }

    if (event & UART_EVENT_RX_DATA) {
        rx_flag = 1;
        /* printf("uart[%d] rx data", uart_num); */
    }
    if (event & UART_EVENT_RX_TIMEOUT) {
        rx_flag = 1;
        /* printf("uart[%d] rx ot\n", uart_num); */
    }

    if (event & UART_EVENT_RX_FIFO_OVF) {
        rx_flag = 1;
        /* printf("uart[%d] rx fifo ovf", uart_num); */
    }
    if (rx_flag && (uha.uart_dev >= 0)) {
        os_sem_post(&uha.sem);
    }
}

size_t aic_uart_read(void *handle, void *buffer, size_t size)
{
    struct uart_hardware_abstract *u = (struct uart_hardware_abstract *)handle;
    if (u->uart_dev < 0) {
        os_time_dly(1);
        return 0;
    }

#if 0
    return (size_t)uart_recv_blocking(u->uart_dev, buffer, (u32)size, 0);
#else

    os_sem_pend(&u->sem, 0);
    if (u->uart_dev < 0) {
        return 0;
    }
    os_sem_set(&u->sem, 0);
    return (size_t)uart_recv_bytes(u->uart_dev, buffer, (u32)size);
#endif
}

size_t aic_uart_write(void *handle, const void *buffer, size_t size)
{
    struct uart_hardware_abstract *u = (struct uart_hardware_abstract *)handle;
    if (u->uart_dev < 0) {
        return 0;
    }

    uart_send_blocking(u->uart_dev, buffer, (u32)size, 0);
    return size;
}

void *aic_uart_open(int com)
{
    aic_uart_cfg_t *uart_cfg;
    uha.uart_phy_cbuf = zalloc(RX_CBUF_SIZE);
    if (!uha.uart_phy_cbuf) {
        printf("uart_dev_open() uart_phy_cbuf  alloc fail\n");
        return NULL;
    }

    uart_cfg = aic_get_uart_cfg();

    struct uart_config config = {
        .baud_rate = uart_cfg->baud,
        .tx_pin = uart_cfg->tx_pin,
        .rx_pin = uart_cfg->rx_pin,
        .parity = UART_PARITY_DISABLE,
    };
    struct uart_dma_config dma = {
        .rx_timeout_thresh = 1000,
        .frame_size = RX_FRAME_LENGTH,
        .event_mask = UART_EVENT_RX_DATA | UART_EVENT_RX_FIFO_OVF | UART_EVENT_RX_TIMEOUT,
        .irq_callback = uart_isr_hook,
        .tx_wait_mutex = 1,
        .irq_priority = 3,
        .rx_cbuffer = uha.uart_phy_cbuf,
        .rx_cbuffer_size = RX_CBUF_SIZE,
    };

    uha.uart_dev = uart_init_new(-1, &config);

    if (uha.uart_dev < 0) {
        printf("uart_dev_open() fail\n");
        free(uha.uart_phy_cbuf);
        return NULL;
    }
    uart_dma_init(uha.uart_dev, &dma);
    uart_dump();

    os_sem_create(&uha.sem, 0);

    /* gpio_set_pull_up(TCFG_CAT1_AICXTEK_UART_RX, 0); */

    printf("aic_uart_open() success\n");

    return (void *)&uha;
}

void aic_uart_close(void *handle)
{
    struct uart_hardware_abstract *u = (struct uart_hardware_abstract *)handle;

    if (u->uart_dev >= 0) {
        int uart_dev = u->uart_dev;
        u->uart_dev = -1;
        os_sem_del(&u->sem, OS_DEL_ALWAYS);
        uart_deinit(uart_dev);
        free(u->uart_phy_cbuf);
    }
}

#ifdef UART_SELF_TEST
void uart_test_main(void *priv)
{
    int ret, i;
    u32 rx_len;
    aic_uart_open(AIC_UART_NUM);

    while (1) {
        printf("aic_uart_read start\n");
        rx_len = aic_uart_read(&uha, aic_uart_test_buffer, TEST_BUF_SIZE);
        printf("aic_uart_read rxdata = 0x%x len = %d\n", aic_uart_test_buffer[0], rx_len);
        os_time_dly(500);
        aic_gpio_wakeup_modem(0);
        printf("aic_uart_write txdata = 0x%x\n", aic_uart_test_buffer[0]);
        aic_uart_write(&uha, aic_uart_test_buffer, rx_len);
        aic_gpio_modem_can_sleep();
    }
}

int uart_self_test_init(void)
{
    task_create(uart_test_main, NULL, "uart_selftest");
    return 0;
}
#endif

#endif /* #if TCFG_CAT1_AICXTEK_ENABLE */

