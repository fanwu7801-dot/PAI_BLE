
#include "asm/includes.h"
#include "device/device.h"
#include "system/spinlock.h"
#include "uos_gpio.h"
#include <stdlib.h>

#if TCFG_CAT1_UNISOC_ENABLE
#define PIN_AF_ID0 IO_PORTG_00
#define PIN_AF_ID1 IO_PORTG_01

/* #define AD_CHANNEL               AD_CH_PB2 */
typedef void (*uos_gpio_isr_handler)(void *args);
extern struct port_wakeup pin_unisoc_ap_req; /* IO_PORTG_00 */
extern struct port_wakeup pin_unisoc_ap_ack; /* IO_PORTG_01 */
extern struct port_wakeup pin_unisoc_cp_req; /* IO_PORTG_06 */
extern struct port_wakeup pin_unisoc_cp_ack; /* IO_PORTG_07 */
extern struct port_wakeup pin_unisoc_cp_pbint; /* IO_PORTC_07 */
extern const struct wakeup_param wk_param;   /* MCU pin reg */

/* code add by tao.shi@2023.11.17 */
/* #define USE_UNISOC_GPIO_FRAMEWORK       (1) */

#if USE_UNISOC_GPIO_FRAMEWORK

#define USE_UNISOC_GPIO_TRACE_ENABLE            (0) /* io trace enable */
#define USE_UNISOC_GPIO_IO_EXPANDER_ENABLE      (0) /* io expander enable */

#if USE_UNISOC_GPIO_TRACE_ENABLE
#define unisoc_gpio_trace printf
#else
#define unisoc_gpio_trace
#endif
#define unisoc_gpio_trace_err printf

typedef enum {
    /* style */
    IO_COMMON = 0,
    IO_EXPANDER,
    IO_STYLE_MAX,

    /* operate [for io_expander]*/
    IO_READ,
    IO_WRITE,
    IO_INIT,
    IO_INT_REPORT,
    IO_INT_TRIGSET,
    IO_DEINIT,
    IO_OPERATE_MAX,
} unisoc_io_e;

typedef int (*unisoc_io_expander_cb)(unsigned int pin, unisoc_io_e operate, int args);

typedef struct {
    unisoc_gpio_e id;
    unisoc_io_e style;
    unisoc_io_expander_cb expander_cb;
    unsigned int pin;
} unisoc_pinmap_t;

/* internal func */
uos_err_t uos_gpio_init(uos_uint32_t pin, uos_gpio_mode pin_mode, void(*isr_handler)(void *args), void *args);
uos_err_t uos_gpio_write(uos_uint32_t pin, uos_int32_t value);
uos_int32_t uos_gpio_read(uos_uint32_t pin);
uos_err_t uos_gpio_disable(uos_uint32_t pin);
uos_err_t uos_gpio_int_trigger_set(uos_uint32_t pin, uos_uint32_t trigger_style);
int user_io_expander_cb(unsigned int pin, unisoc_io_e operate, int args);
/* external func */

/* export func   */
uos_err_t uos_gpio_init_ex(unisoc_gpio_e id, uos_gpio_mode pin_mode, void(*isr_handler)(void *args), void *args);
uos_err_t uos_gpio_write_ex(unisoc_gpio_e id, uos_int32_t value);
uos_int32_t uos_gpio_read_ex(unisoc_gpio_e id);
uos_err_t uos_gpio_disable_ex(unisoc_gpio_e id);

#if USE_UNISOC_GPIO_IO_EXPANDER_ENABLE
static unisoc_pinmap_t unisoc_pinmap_tab[] = {
    /* id  --- style --- expander_cb --- phy_id */
    { GPIO_UNISOC_AP_REQ,   IO_COMMON, NULL, IO_PORTG_00},
    { GPIO_UNISOC_AP_ACK,   IO_COMMON, NULL, IO_PORTG_01},
    { GPIO_UNISOC_CP_REQ,   IO_COMMON, NULL, IO_PORTG_06},
    { GPIO_UNISOC_CP_ACK,   IO_COMMON, NULL, IO_PORTG_07},
    { GPIO_UNISOC_MODEM_EN, IO_EXPANDER, user_io_expander_cb, IO_PORTC_06},
    { GPIO_UNISOC_PBINT, IO_EXPANDER, user_io_expander_cb, IO_PORTC_07},
    { GPIO_UNISOC_MAX, IO_COMMON, NULL,        NULL}
};
#else
static unisoc_pinmap_t unisoc_pinmap_tab[] = {
    /* id  --- style --- expander_cb --- phy_id */
    { GPIO_UNISOC_AP_REQ,   IO_COMMON, NULL, IO_PORTG_00},
    { GPIO_UNISOC_AP_ACK,   IO_COMMON, NULL, IO_PORTG_01},
    { GPIO_UNISOC_CP_REQ,   IO_COMMON, NULL, IO_PORTG_06},
    { GPIO_UNISOC_CP_ACK,   IO_COMMON, NULL, IO_PORTG_07},
    { GPIO_UNISOC_MODEM_EN, IO_COMMON, NULL, IO_PORTC_06},
    { GPIO_UNISOC_PBINT, IO_COMMON, NULL, IO_PORTC_07},
    { GPIO_UNISOC_MAX, IO_COMMON, NULL,        NULL}
};
#endif

static int uos_gpio_get_index_ex(unisoc_gpio_e id)
{
    int i;
    for (i = 0; i < GPIO_UNISOC_MAX; i++) {
        if (unisoc_pinmap_tab[i].id == id) {
            return i;
        }
    }
    unisoc_gpio_trace_err("[%s]:id[%d] err io_style_max\n", __FUNCTION__, (int)id);
    return (int)GPIO_UNISOC_MAX; /* error */
}

uos_err_t uos_gpio_init_ex(unisoc_gpio_e id, uos_gpio_mode pin_mode, void(*isr_handler)(void *args), void *args)
{
    int io_index;
    int err = 0;
    unisoc_pinmap_t *map_ptr = unisoc_pinmap_tab;
    unisoc_gpio_trace("[%s]:id[%d] mode[%d]\n", __FUNCTION__, (int)id, (int)pin_mode);
    io_index = uos_gpio_get_index_ex(id);
    if (io_index >= GPIO_UNISOC_MAX) {
        /* error(assert) */
        err = 1;
        goto __error;
    }
    switch (map_ptr[io_index].style) {
    case IO_COMMON:
        return uos_gpio_init(map_ptr[io_index].pin, pin_mode, isr_handler, args);
        break;
    case IO_EXPANDER:
        if (map_ptr[io_index].expander_cb) {
            return map_ptr[io_index].expander_cb((map_ptr[io_index].pin), IO_INIT, (int)pin_mode);
        } else {
            /* callback invalid(assert) */
            err = 2;
            goto __error;
        }
        break;
    default:
        /* unknown id. assert */
        err = 3;
        goto __error;
        break;
    }

__error:
    unisoc_gpio_trace_err("err[%d] id[%d]-[%s][%d]\n", err, id, __FUNCTION__, __LINE__);
    /* unknown error. assert */
    return -1;
}

uos_err_t uos_gpio_write_ex(unisoc_gpio_e id, uos_int32_t value)
{
    int io_index;
    int err = 0;
    unisoc_pinmap_t *map_ptr = unisoc_pinmap_tab;
    unisoc_gpio_trace("[%s]:id[%d] value[%d]\n", __FUNCTION__, (int)id, (int)value);
    io_index = uos_gpio_get_index_ex(id);
    if (io_index >= GPIO_UNISOC_MAX) {
        /* error(assert) */
        err = 1;
        goto __error;
    }
    switch (map_ptr[io_index].style) {
    case IO_COMMON:
        return uos_gpio_write((map_ptr[io_index].pin), value);
        break;
    case IO_EXPANDER:
        if (map_ptr[io_index].expander_cb) {
            return map_ptr[io_index].expander_cb((map_ptr[io_index].pin), IO_WRITE, (int)value);
        } else {
            /* callback invalid(assert) */
            err = 2;
            goto __error;
        }
        break;
    default:
        /* unknown id. assert */
        err = 3;
        goto __error;
        break;
    }

__error:
    unisoc_gpio_trace_err("err[%d] id[%d]-[%s][%d]\n", err, id, __FUNCTION__, __LINE__);
    /* unknown error. assert */
    return -1;
}

uos_int32_t uos_gpio_read_ex(unisoc_gpio_e id)
{
    int io_index;
    int err = 0;
    unisoc_pinmap_t *map_ptr = unisoc_pinmap_tab;
    unisoc_gpio_trace("[%s]:id[%d]\n", __FUNCTION__, (int)id);
    io_index = uos_gpio_get_index_ex(id);
    if (io_index >= GPIO_UNISOC_MAX) {
        /* error(assert) */
        err = 1;
        goto __error;
    }
    switch (map_ptr[io_index].style) {
    case IO_COMMON:
        return uos_gpio_read(map_ptr[io_index].pin);
        break;
    case IO_EXPANDER:
        if (map_ptr[io_index].expander_cb) {
            return map_ptr[io_index].expander_cb((map_ptr[io_index].pin), IO_READ, NULL);
        } else {
            /* callback invalid(assert) */
            err = 2;
            goto __error;
        }
        break;
    default:
        /* unknown id. assert */
        err = 3;
        goto __error;
        break;
    }
__error:
    unisoc_gpio_trace_err("err[%d] id[%d]-[%s][%d]\n", err, id, __FUNCTION__, __LINE__);
    /* unknown error. assert */
    return -1;
}

int uos_gpio_get_pin_ex(unisoc_gpio_e id)
{
    int io_index;
    int err;
    unisoc_pinmap_t *map_ptr = unisoc_pinmap_tab;
    io_index = uos_gpio_get_index_ex(id);
    if (io_index >= GPIO_UNISOC_MAX) {
        /* error(assert) */
        err = -1;
        goto __error;
    }
    return (int)map_ptr[io_index].pin;
__error:
    unisoc_gpio_trace_err("err[%d] id[%d]-[%s][%d]\n", err, id, __FUNCTION__, __LINE__);
    /* unknown error. assert */
    return err;
}

uos_err_t uos_gpio_int_trigger_set_ex(unisoc_gpio_e id, uos_uint32_t trigger_style)
{
    int io_index;
    int err = 0;
    unisoc_pinmap_t *map_ptr = unisoc_pinmap_tab;
    unisoc_gpio_trace("[%s]:id[%d]\n", __FUNCTION__, (int)id);
    io_index = uos_gpio_get_index_ex(id);
    if (io_index >= GPIO_UNISOC_MAX) {
        /* error(assert) */
        err = 1;
        goto __error;
    }
    switch (map_ptr[io_index].style) {
    case IO_COMMON:
        return uos_gpio_int_trigger_set(map_ptr[io_index].pin, trigger_style);
        break;
    case IO_EXPANDER:
        if (map_ptr[io_index].expander_cb) {
            return map_ptr[io_index].expander_cb((map_ptr[io_index].pin), IO_INT_TRIGSET, trigger_style);
        } else {
            /* callback invalid(assert) */
            err = 2;
            goto __error;
        }
        break;
    default:
        /* unknown id. assert */
        err = 3;
        goto __error;
        break;
    }
__error:
    unisoc_gpio_trace_err("err[%d] id[%d]-[%s][%d]\n", err, id, __FUNCTION__, __LINE__);
    /* unknown error. assert */
    return -1;
}

uos_err_t uos_gpio_disable_ex(unisoc_gpio_e id)
{
    int io_index;
    int err = 0;
    unisoc_pinmap_t *map_ptr = unisoc_pinmap_tab;
    unisoc_gpio_trace("[%s]:id[%d]\n", __FUNCTION__, (int)id);
    io_index = uos_gpio_get_index_ex(id);
    if (io_index >= GPIO_UNISOC_MAX) {
        /* error(assert) */
        err = 1;
        goto __error;
    }
    switch (map_ptr[io_index].style) {
    case IO_COMMON:
        return uos_gpio_disable(map_ptr[io_index].pin);
        break;
    case IO_EXPANDER:
        if (map_ptr[io_index].expander_cb) {
            return map_ptr[io_index].expander_cb((map_ptr[io_index].pin), IO_DEINIT, NULL);
        } else {
            /* callback invalid(assert) */
            err = 2;
            goto __error;
        }
        break;
    default:
        /* unknown id. assert */
        err = 3;
        goto __error;
        break;
    }
__error:
    unisoc_gpio_trace_err("err[%d] id[%d]-[%s][%d]\n", err, id, __FUNCTION__, __LINE__);
    /* unknown error. assert */
    return -1;
}

uos_err_t unisoc_io_expander_cb_register(unisoc_gpio_e id, unisoc_io_expander_cb cb)
{
    int io_index;
    int err = 0;
    unisoc_pinmap_t *map_ptr = unisoc_pinmap_tab;

    io_index = uos_gpio_get_index_ex(id);
    if (io_index >= GPIO_UNISOC_MAX) {
        /* error(assert) */
        err = 1;
        goto __error;
    }
    switch (map_ptr[io_index].style) {
    case IO_COMMON:
        /* error pin style is common(assert) */
        err = 2;
        goto __error;
        break;
    case IO_EXPANDER:
        if (map_ptr[io_index].expander_cb) {
            /* error cb is already registed(assert) */
            err = 3;
            goto __error;
        } else {
            map_ptr[io_index].expander_cb = cb;
            return 0; /* success */
        }
        break;
    default:
        /* unknown id. assert */
        err = 4;
        goto __error;
        break;
    }
    /* unknown error. assert */
__error:
    unisoc_gpio_trace_err("err[%d] id[%d]-[%s][%d]\n", err, id, __FUNCTION__, __LINE__);
    /* unknown error. assert */
    return -1;
}

uos_err_t unisoc_io_expander_cb_unregister(unisoc_gpio_e id)
{
    int io_index;
    int err = 0;
    unisoc_pinmap_t *map_ptr = unisoc_pinmap_tab;

    io_index = uos_gpio_get_index_ex(id);
    if (io_index >= GPIO_UNISOC_MAX) {
        /* error(assert) */
        err = 1;
        goto __error;
    }
    switch (map_ptr[io_index].style) {
    case IO_COMMON:
        /* error pin style is common(assert) */
        err = 2;
        goto __error;
        break;
    case IO_EXPANDER:
        map_ptr[io_index].expander_cb = NULL;
        return 0;
        break;
    default:
        /* unknown id. assert */
        err = 3;
        goto __error;
        break;
    }
    /* unknown error. assert */
__error:
    unisoc_gpio_trace_err("err[%d] id[%d]-[%s][%d]\n", err, id, __FUNCTION__, __LINE__);
    /* unknown error. assert */
    return -1;
}

/* for io_expander ic(demo) */
int user_io_expander_cb(unsigned int pin, unisoc_io_e operate, int args)
{
    int ret = -1;
    unisoc_gpio_trace("[%s]user action[%d][%d]-[%d]\n", __FUNCTION__, pin, (int)operate, args);
    switch (operate) {
    case IO_READ:
        /* user todo */
        return uos_gpio_read(pin); //for test
        break;
    case IO_WRITE:
        /* user todo */
        return uos_gpio_write(pin, (uos_int32_t)args); //for test
        break;
    case IO_INIT:
        /* user todo */
        uos_gpio_init(pin, args, NULL, NULL);  //for test
        break;
    case IO_INT_REPORT:
        /* user todo */
        break;
    case IO_INT_TRIGSET:
        /* user todo */
        break;
    case IO_DEINIT:
        /* user todo */
        uos_gpio_disable(pin);
        break;
    default:
        break;
    }
    return ret;
}

#endif /* end USE_UNISOC_GPIO_FRAMEWORK */



uos_err_t uos_gpio_init(uos_uint32_t pin, uos_gpio_mode pin_mode, void(*isr_handler)(void *args), void *args)
{
    uos_err_t err = UOS_OK;
    unsigned int bit_offset = 0;
    if (pin_mode == UOS_GPIO_MODE_INT) {
        for (bit_offset = 0; bit_offset < MAX_WAKEUP_PORT; bit_offset++) {
            if ((wk_param.port[bit_offset] != NULL) && (wk_param.port[bit_offset]->iomap == pin)) {
                break;
            }
        }
        if (bit_offset == MAX_WAKEUP_PORT) {
            return UOS_ERROR;
        }
    }
    switch (pin_mode) {
    case UOS_GPIO_MODE_OUTPUT:
        gpio_set_direction(pin, 0);
        gpio_set_pull_down(pin, 0);
        gpio_set_pull_up(pin, 0);
        gpio_set_die(pin, 0);
        //gpio_direction_output(pin, 0);
        //gpio_set_direction(pin, 0);
        break;
    case UOS_GPIO_MODE_INPUT:
        gpio_set_direction(pin, 1);
        gpio_set_pull_up(pin, 0);
        gpio_set_pull_down(pin, 0);
        gpio_set_die(pin, 1);

        break;
    case UOS_GPIO_MODE_INT:
        gpio_set_direction(pin, 1);
        gpio_set_pull_up(pin, 0);
        gpio_set_pull_down(pin, 1);
        gpio_set_die(pin, 1);
        /* port4.iomap=pin; */
        /* extern void hw_power_set_wakeup_io(u8 index, struct port_wakeup *port); */
        /* hw_power_set_wakeup_io(5, &port4); */
        port_edge_wkup_set_callback_by_index(bit_offset, isr_handler);

        /* hw_power_set_wakeup_io(6, &port5); */
        power_wakeup_index_enable(bit_offset, 1);

        break;
    case UOS_GPIO_MODE_ANALOG:
        ASSERT(0);
        /* adc_add_sample_ch(AD_CHANNEL);          //注意：初始化AD_KEY之前，先初始化ADC */
        gpio_set_die(pin, 0);
        gpio_set_direction(pin, 1);
        gpio_set_pull_down(pin, 0);
        gpio_set_pull_up(pin, 0);
        break;
    default:
        return UOS_ERROR;
        break;
    }
    return err;
}


typedef enum {
    UOS_PIN_AF_NONE,
    UOS_PIN_AF_GPIO,
    UOS_PIN_AF_UART,
} uos_pin_af_e;


extern uos_err_t uos_uart_open(void); /* PG0 PG1 */


uos_uint32_t uos_get_pin_internal_bitoffset(uos_uint32_t pin)
{
    unsigned int bit_offset = 0;
    unsigned char find_flag = 0;
    for (bit_offset = 0; bit_offset < MAX_WAKEUP_PORT; bit_offset++) {
        if ((wk_param.port[bit_offset] != NULL) && (wk_param.port[bit_offset]->iomap == pin)) {
            find_flag = 1;
            break;
        }
    }
    if (find_flag == 1) {
        return bit_offset;
    } else {
        return 0;
    }
}

uos_err_t uos_gpio_uart_change(uos_pin_af_e pin_af)
{
    uos_uint32_t bit_offset = 0;
    uos_err_t ret = UOS_ERROR;
    switch (pin_af) {
    case UOS_PIN_AF_NONE:
    case UOS_PIN_AF_GPIO:
        /* todo... */
        ret = uos_uart_close();
        uos_gpio_init(PIN_AF_ID0, UOS_GPIO_MODE_OUTPUT, NULL, NULL);
        uos_gpio_init(PIN_AF_ID1, UOS_GPIO_MODE_OUTPUT, NULL, NULL);
        uos_gpio_write(PIN_AF_ID0, 0);
        uos_gpio_write(PIN_AF_ID1, 0);
        break;
    case UOS_PIN_AF_UART:
        /* todo... */
        //uos_gpio_disable(PIN_AF_ID0);
        //uos_gpio_disable(PIN_AF_ID1);
        bit_offset = uos_get_pin_internal_bitoffset(PIN_AF_ID0);
        if (bit_offset < MAX_WAKEUP_PORT) {
            power_wakeup_index_enable(bit_offset, 0);
        }

        bit_offset = uos_get_pin_internal_bitoffset(PIN_AF_ID1);
        if (bit_offset < MAX_WAKEUP_PORT) {
            power_wakeup_index_enable(bit_offset, 0);
        }

        ret = uos_uart_open();
        break;
    default:
        /* assert */
        break;
    }
    return ret;
}


uos_err_t uos_gpio_write(uos_uint32_t pin, uos_int32_t value)
{
    uos_err_t err = 0;

    //err = gpio_write(pin, value);
    err = gpio_direction_output(pin, value);

    return err;
}
uos_int32_t uos_gpio_read(uos_uint32_t pin)
{

    return (uos_int32_t)gpio_read(pin);

}

uos_err_t uos_gpio_disable(uos_uint32_t pin)
{
    /* uos_err_t err=0; */
    uos_uint32_t interrupt_index = 0, AF_find_flag = 0;
    u8 i = 0;
    interrupt_index = uos_get_pin_internal_bitoffset(pin);
    if (interrupt_index != 0) {
        power_wakeup_index_enable(interrupt_index, 0);
        gpio_set_direction(pin, 1);
        gpio_set_pull_up(pin, 0);
        gpio_set_pull_down(pin, 0);
        return 0;
    }
    gpio_disable_fun_output_port(pin);
    u32 *p_fun = &(JL_IMAP->FI_GP_ICH0);
    u32 iport = gpio2crossbar_inport(pin);
    for (; i < 64; i++) {
        if (*p_fun == iport) {
            AF_find_flag = 1; // 只有IO与功能对应上才去取消对应功能不然可能会造成功能误取消。
            break;
        }
        p_fun++;
    }
    if (AF_find_flag != 0) {
        gpio_disable_fun_input_port(p_fun);
    }
    gpio_set_direction(pin, 1);
    gpio_set_pull_up(pin, 0);
    gpio_set_pull_down(pin, 0);
    return 0;
}





uos_err_t uos_gpio_int_trigger_set(uos_uint32_t pin, uos_uint32_t trigger_style)
{
    /* uos_err_t err = 0; */
    unsigned int bit_offset = 0;
    struct port_wakeup *pin_ptr = NULL;

    for (bit_offset = 0; bit_offset < MAX_WAKEUP_PORT; bit_offset++) {
        if ((wk_param.port[bit_offset] != NULL) && (wk_param.port[bit_offset]->iomap == pin)) {
            break;
        }
    }

    if (bit_offset == MAX_WAKEUP_PORT) {
        return UOS_ERROR;
    }

    pin_ptr = wk_param.port[bit_offset];
    if (trigger_style == PIN_IRQ_MODE_FALLING) {
        pin_ptr->edge = FALLING_EDGE;
        gpio_set_pull_up(pin, 1);
        gpio_set_pull_down(pin, 0);
        P33_OR_WKUP_EDGE(BIT(bit_offset));      //下降沿
    } else {
        pin_ptr->edge = RISING_EDGE;
        gpio_set_pull_up(pin, 0);
        gpio_set_pull_down(pin, 1);
        P33_AND_WKUP_EDGE(BIT(bit_offset));     //上升沿
    }
    power_wakeup_index_enable(bit_offset, 1);
    return UOS_OK;
}



//使用中断需要在board_701m_demo.c里面去配置的wk_parm配置 可以参考
void scan_io()
{
    printf("\n [scan_io] %s -[xiaohuan] %d\n", __FUNCTION__, __LINE__);
    if ((uos_gpio_read(IO_PORTB_02) == 0)) {
        uos_gpio_write(IO_PORTA_14, 0);

    } else if (uos_gpio_read(IO_PORTB_02)) {
        uos_gpio_write(IO_PORTA_14, 1);
    }
}
void port4_handele()
{
    printf("\n [ERROR] %s -[yuyu] %d\n", __FUNCTION__, __LINE__);
    uos_gpio_disable(IO_PORTB_05);//关闭串口的输出功能。
    /* gpio_disable_fun_input_port(PFI_UART1_RX); */
    //UART_RX_PIN_CHANGE(IO_PORTB_00);


    /* uos_gpio_disable(IO_PORTB_03); */
}

void gpio_output_init()
{
    uos_gpio_init(IO_PORTA_14, UOS_GPIO_MODE_OUTPUT, NULL, NULL);

}

void gpio_input_init()
{
    uos_gpio_init(IO_PORTB_02, UOS_GPIO_MODE_INPUT, NULL, NULL);

}

void gpio_INT_init()
{
    uos_gpio_init(IO_PORTC_07, UOS_GPIO_MODE_INT, port4_handele, NULL);
}

/* void test_demo()//添加一个定时去扫码输入IO的状态，然后去翻转另外一个IO的输出达到测试目的 */
/* { */
/*     sys_timer_add(NULL, scan_io, 3000); */
/*  */
/* } */
#endif
