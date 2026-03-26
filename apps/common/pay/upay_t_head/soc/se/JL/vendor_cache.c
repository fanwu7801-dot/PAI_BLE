
/*
 * @Description:
 * @Author: HJY
 * @Date: 2022-12-15 09:59:39
 * @LastEditTime: 2022-12-15 17:33:17
 * @LastEditors: HJY
 */
// #include "include.h"
#include "hs_private.h"
#include "iotsec.h"
#include "system/includes.h"
#include "alipay.h"
#include "event.h"
//--------------------------------------------------------------------------
//
//									版本V1.0.02
//
//--------------------------------------------------------------------------


#undef _DEBUG_H_
#define LOG_TAG_CONST       ALIPAY_CACHE          //修改成文件名
#define LOG_TAG             "[ALIPAY_CACHE]"
#include "debug.h"
#define LOG_v(t)  log_tag_const_v_ ## t
#define LOG_i(t)  log_tag_const_i_ ## t
#define LOG_d(t)  log_tag_const_d_ ## t
#define LOG_w(t)  log_tag_const_w_ ## t
#define LOG_e(t)  log_tag_const_e_ ## t
#define LOG_c(t)  log_tag_const_c_ ## t
#define LOG_tag(tag, n) n(tag)
const char LOG_tag(LOG_TAG_CONST, LOG_v) AT(.LOG_TAG_CONST) = 1;
const char LOG_tag(LOG_TAG_CONST, LOG_i) AT(.LOG_TAG_CONST) = 1; //log_info
const char LOG_tag(LOG_TAG_CONST, LOG_d) AT(.LOG_TAG_CONST) = 1; //log_debug
const char LOG_tag(LOG_TAG_CONST, LOG_w) AT(.LOG_TAG_CONST) = 1;
const char LOG_tag(LOG_TAG_CONST, LOG_e) AT(.LOG_TAG_CONST) = 1;
const char LOG_tag(LOG_TAG_CONST, LOG_c) AT(.LOG_TAG_CONST) = 1;

extern int uart_tr_send_data(u8 *data, u32 len);
extern int get_uart_tr_send_en(void);

#include "board_config.h"
#include "system/includes.h"
#include "asm/power_interface.h"
#include "app_config.h"

/*
****************************************************
*支付宝发送数据到平头哥接收缓存
 ***************************************************
 */
#define UPAY2ALI_BUF_LEN  512
static u8 *upay2ali_rbuf = NULL;//线程读取与发送
static u8 *upay2ali_cbuf = NULL;//缓存使用,可以扩大几倍


cbuffer_t upay2ali_cbuft;
#define cbuf_get_space(a) (a)->total_len
typedef struct {
    u8   app_id;
    u32  rx_len;
} __attribute__((packed)) upay2ali_head_t;

static int upay2ali_cbuf_is_write_able(u32 len)
{
    u32 wlen, buf_space;
    /* OS_ENTER_CRITICAL(); */
    /* cbuf_is_write_able() */
    buf_space = cbuf_get_space(&upay2ali_cbuft) - cbuf_get_data_size(&upay2ali_cbuft);
    if (buf_space < len) {
        wlen = 0;
    } else {
        wlen = len;
    }
    /* OS_EXIT_CRITICAL(); */
    log_debug("%s[%d %d %d]", __func__, len, buf_space, wlen);
    return wlen;
}
int upay2ali_ibuf_to_cbuf(u8 *buf, u32 len)
{
    int ret = 0;
    u16 wlen = 0;
    u16 head_size = sizeof(upay2ali_head_t);
    upay2ali_head_t rx_head;
    rx_head.app_id = 1; //use app1
    OS_ENTER_CRITICAL();
    if (upay2ali_cbuf_is_write_able(len + head_size)) {
        rx_head.rx_len = len;
        wlen = cbuf_write(&upay2ali_cbuft, &rx_head, head_size);
        wlen += cbuf_write(&upay2ali_cbuft, buf, rx_head.rx_len);
        if (wlen != (rx_head.rx_len + head_size)) {
            ret = -1;
        } else {
            ret = wlen;
        }
    } else {
        ret = -2;
    }
    OS_EXIT_CRITICAL();
    log_debug("%s[%d %d %d]", __func__, len, head_size, ret);
    return ret;
}
static int upay2ali_cbuf_read(u8 *buf, u32 *len)
{
    int ret = 0;
    u16 rlen = 0;
    u16 head_size = sizeof(upay2ali_head_t);
    upay2ali_head_t rx_head;
    OS_ENTER_CRITICAL();
    if (0 == cbuf_get_data_size(&upay2ali_cbuft)) {
        /* OS_EXIT_CRITICAL(); */
        ret =  -1;
    } else {
        rlen = cbuf_read(&upay2ali_cbuft, &rx_head, sizeof(upay2ali_head_t));
        if (rlen != sizeof(upay2ali_head_t)) {
            ret =  -2;
        } else if (rx_head.rx_len) {
            rlen = cbuf_read(&upay2ali_cbuft, buf, rx_head.rx_len);
            if (rlen != rx_head.rx_len) {
                ret =  -3;
            } else {
                *len = rx_head.rx_len;
            }
        }
    }
    OS_EXIT_CRITICAL();
    log_debug("%s[%d 0x%x %d]", __func__, ret, len, rx_head.rx_len);
    return ret;
}

void upay2ali_cbuf_init(void)
{
    upay2ali_rbuf = malloc(UPAY2ALI_BUF_LEN);
    upay2ali_cbuf = malloc(UPAY2ALI_BUF_LEN);
    cbuf_init(&upay2ali_cbuft, upay2ali_cbuf, UPAY2ALI_BUF_LEN);

}

void upay2ali_cbuf_uninit(void)
{
    memset(&upay2ali_cbuft, 0x00, sizeof(cbuffer_t));
    if (upay2ali_rbuf != NULL) {
        free(upay2ali_rbuf);
        upay2ali_rbuf = NULL;
    }
    if (upay2ali_cbuf != NULL) {
        free(upay2ali_cbuf);
        upay2ali_cbuf = NULL;
    }
}


//#define DEVICE_EVENT_FROM_UTR   (('A' << 24) | ('T' << 16) | ('R' << 8) | '\0')
void upay2ali_send_event(void)
{
    struct sys_event e;
    e.type = SYS_DEVICE_EVENT;
    e.arg  = (void *)DEVICE_EVENT_FROM_U2A;
    e.u.dev.event = DEVICE_EVENT_CHANGE;
    e.u.dev.value = 0;

    sys_event_notify(&e);
}


void upay2ali_event_handler(struct sys_event *e)
{
    int ret = 0;
    u32 rlen = 0;
    if ((u32)e->arg == DEVICE_EVENT_FROM_U2A) {
        while (1) {
            extern void clr_wdt(void);
            clr_wdt();
            ret = upay2ali_cbuf_read(upay2ali_rbuf, &rlen);//栈变量取值可能会死机
            if (ret < 0) {
                return;
            }

            alipay_recv_data_handle(upay2ali_rbuf, rlen);
        }
    }
}
// SYS_EVENT_HANDLER(SYS_DEVICE_EVENT, upay2ali_event_handler, 0);

///测试验证函数
#if 0
int upay2ali_send_data(u8 *data, u32 len)
{
    // return ble_comm_att_send_data(trans_con_handle, APP_TR_TX_HANDLE, data, len, ATT_OP_AUTO_READ_CCC);
}
#endif

/*
****************************************************
*平头哥发送数据到支付宝接收缓存
 ***************************************************
 */

#define ALI2UPAY_BUF_LEN  512
static u8 *ali2upay_rbuf = NULL;//线程读取与发送
static u8 *ali2upay_cbuf = NULL;//缓存使用,可以扩大几倍

cbuffer_t ali2upay_cbuft;
#define cbuf_get_space(a) (a)->total_len
typedef struct {
    u8   app_id;
    u32  rx_len;
} __attribute__((packed)) ali2upay_head_t;

static int ali2upay_cbuf_is_write_able(u32 len)
{
    u32 wlen, buf_space;
    /* OS_ENTER_CRITICAL(); */
    /* cbuf_is_write_able() */
    buf_space = cbuf_get_space(&ali2upay_cbuft) - cbuf_get_data_size(&ali2upay_cbuft);
    if (buf_space < len) {
        wlen = 0;
    } else {
        wlen = len;
    }
    /* OS_EXIT_CRITICAL(); */
    log_debug("%s[%d %d %d]", __func__, len, buf_space, wlen);
    return wlen;
}
int ali2upay_ibuf_to_cbuf(u8 *buf, u32 len)
{
    int ret = 0;
    u16 wlen = 0;
    u16 head_size = sizeof(ali2upay_head_t);
    ali2upay_head_t rx_head;
    rx_head.app_id = 1; //use app1
    OS_ENTER_CRITICAL();
    if (ali2upay_cbuf_is_write_able(len + head_size)) {
        rx_head.rx_len = len;
        wlen = cbuf_write(&ali2upay_cbuft, &rx_head, head_size);
        wlen += cbuf_write(&ali2upay_cbuft, buf, rx_head.rx_len);
        if (wlen != (rx_head.rx_len + head_size)) {
            ret = -1;
        } else {
            ret = wlen;
        }
    } else {
        ret = -2;
    }
    OS_EXIT_CRITICAL();
    log_debug("%s[%d %d %d]", __func__, len, head_size, ret);
    return ret;
}
static int ali2upay_cbuf_read(u8 *buf, u32 *len)
{
    int ret = 0;
    u16 rlen = 0;
    u16 head_size = sizeof(ali2upay_head_t);
    ali2upay_head_t rx_head;
    OS_ENTER_CRITICAL();
    if (0 == cbuf_get_data_size(&ali2upay_cbuft)) {
        /* OS_EXIT_CRITICAL(); */
        ret =  -1;
    } else {
        rlen = cbuf_read(&ali2upay_cbuft, &rx_head, sizeof(ali2upay_head_t));
        if (rlen != sizeof(ali2upay_head_t)) {
            ret =  -2;
        } else if (rx_head.rx_len) {
            rlen = cbuf_read(&ali2upay_cbuft, buf, rx_head.rx_len);
            if (rlen != rx_head.rx_len) {
                ret =  -3;
            } else {
                *len = rx_head.rx_len;
            }
        }
    }
    OS_EXIT_CRITICAL();
    log_debug("%s[%d 0x%x %d]", __func__, ret, len, rx_head.rx_len);
    return ret;
}

void ali2upay_cbuf_init(void)
{
    ali2upay_rbuf = malloc(ALI2UPAY_BUF_LEN);
    ali2upay_cbuf = malloc(ALI2UPAY_BUF_LEN * 8);
    cbuf_init(&ali2upay_cbuft, ali2upay_cbuf, (ALI2UPAY_BUF_LEN * 8));

}

void ali2upay_cbuf_uninit(void)
{
    memset(&ali2upay_cbuft, 0x00, sizeof(cbuffer_t));
    if (ali2upay_rbuf != NULL) {
        free(ali2upay_rbuf);
        ali2upay_rbuf = NULL;
    }
    if (ali2upay_cbuf != NULL) {
        free(ali2upay_cbuf);
        ali2upay_cbuf = NULL;
    }
}

//#define DEVICE_EVENT_FROM_UTR   (('A' << 24) | ('T' << 16) | ('R' << 8) | '\0')
void ali2upay_send_event(void)
{
    struct sys_event e;
    e.type = SYS_DEVICE_EVENT;
    e.arg  = (void *)DEVICE_EVENT_FROM_A2U;
    e.u.dev.event = DEVICE_EVENT_CHANGE;
    e.u.dev.value = 0;

    sys_event_notify(&e);
}
#define MUL_CNT_MAX 10
void ali2upay_event_handler(struct sys_event *e)
{
    int ret = 0;
    u32 rlen = 0;
    static u16 mul_cnt = 0;
    if ((u32)e->arg == DEVICE_EVENT_FROM_A2U) {
        while (1) {
            extern void clr_wdt(void);
            clr_wdt();
            ret = ali2upay_cbuf_read(ali2upay_rbuf, &rlen);//栈变量取值可能会死机
            if (ret < 0) {
                return;
            }
            extern int upay_ble_send_data(const uint8_t *data, u16 len);
            os_time_dly(2);

            do {
                /* code */
                int sret = upay_ble_send_data(ali2upay_rbuf, rlen);
                if (!sret) {
                    mul_cnt = 0;
                } else {
                    if (mul_cnt > MUL_CNT_MAX) {
                        log_e("%s[line:%d] send upay data error\n", __func__, __LINE__);
                        mul_cnt = 0;
                        break;
                    }
                    os_time_dly(3);
                    mul_cnt ++;
                    log_debug("%s[line:%d] send upay data mul cnt %d\n", __func__, __LINE__, mul_cnt);
                }
            } while (mul_cnt);


        }
    }
}

#if 0
void ali2upay_event_handler(struct sys_event *e)
{
    int ret = 0;
    u32 rlen = 0;
    if ((u32)e->arg == DEVICE_EVENT_FROM_A2U) {
        while (1) {
            extern void clr_wdt(void);
            clr_wdt();
            ret = ali2upay_cbuf_read(ali2upay_rbuf, &rlen);//栈变量取值可能会死机
            if (ret < 0) {
                return;
            }
            extern int alipay_ble_send_data(const uint8_t *data, u16 len);
            alipay_ble_send_data(ali2upay_rbuf, rlen);

        }
    }
}
#endif
// SYS_EVENT_HANDLER(SYS_DEVICE_EVENT, ali2upay_event_handler, 0);

///测试验证函数
#if 0
int ali2upay_send_data(u8 *data, u32 len)
{

}
#endif
void alipay_cache_buf_init(void)
{
    log_info("%s[line:%d]\n", __func__, __LINE__);
    // app_cbuf_init();
    upay2ali_cbuf_init();
    ali2upay_cbuf_init();
}

void alipay_cache_buf_uninit(void)
{
    log_info("%s[line:%d]\n", __func__, __LINE__);
    // app_cbuf_init();
    upay2ali_cbuf_uninit();
    ali2upay_cbuf_uninit();

}
