#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "cit_api.h"
#include "uos_deval.h"
#include "uos_osal.h"
#include "rpc.h"
#include "uos_type.h"
#include "cmux_api.h"
#include "mcu_diag_process.h"
#include "modem_ctrl_api.h"
#include "mdload_api.h"
#include "system/timer.h"
#include "le_smartbox_module.h"

//#ifndef MCU_DIAG_TEST
#if TCFG_CAT1_UNISOC_ENABLE

uos_queue_t *dongle_diag_queue = NULL;
uos_queue_attr_t *dongle_diag_attr = NULL;
static uint8 rw_buf[4 * 1024];
extern int ble_dongle_send_custom_data(u8 *data, u16 len);
//#endif
static u8 send_flag = 0;
static u8 diag_can_exit = 0;
static BOOLEAN mcu_diag_first_power = TRUE;
uos_thread_t *diag_thread = NULL;

BOOLEAN diag_channel_open(void)
{
    BOOLEAN ret = FALSE;
#ifdef CMUX_USE_UART
    cmux_device_e device = CMUX_DEVICE_UART;
#else
    cmux_device_e device = CMUX_DEVICE_SPI;
#endif
    cmux_channel_e channel = CMUX_CHANNEL_DIAG;
    ret = cmux_open(device, channel, 0, 0, 0);
    MCUDIAG_LOG("diag_channel_open:device=%d,channel=%d,ret=%d\n", 3, device, channel, ret);
    return ret;
}

BOOLEAN diag_channel_closed(void)
{
    BOOLEAN ret = FALSE;
#ifdef CMUX_USE_UART
    cmux_device_e device = CMUX_DEVICE_UART;
#else
    cmux_device_e device = CMUX_DEVICE_SPI;
#endif
    cmux_channel_e channel = CMUX_CHANNEL_DIAG;
    ret = cmux_close(device, channel);
    MCUDIAG_LOG("diag_channel_closed:device=%d,channel=%d,ret=%d\n", 3, device, channel, ret);
    return ret;
}

#ifndef MCU_DIAG_TEST
# if 0
int bt_send_diag_data(uint8 *ret, int send_len)
{
    int offset = 0;
    int err = 0;
    for (;;) {
        if (send_len > 200) { //bt maximum is 230

            err = ble_dongle_send_custom_data(ret + offset, 200);
            ASSERT(!err);
            send_len -= 200;
            offset += 200;
        } else {
            err =  ble_dongle_send_custom_data(ret + offset, send_len);
            ASSERT(!err);
            break;
        }
        //if(err!=0)

        os_time_dly(8);
    }
}
#else
#define SIMBA_MAX_UPLOAD_DATA_LEN   490
static u16 timer_data = 0;
void ble_send_data_timer(void *p)
{
    u32 *buf = (u32 *)p;
    int err = 0;
    static err_cnt = 0;
    printf("addr 0x%x, len %d\n", buf[0], buf[1]);
    if (buf[1] > SIMBA_MAX_UPLOAD_DATA_LEN) {
        sys_timer_modify(timer_data, 110);
        err = ble_dongle_send_custom_data(buf[0], SIMBA_MAX_UPLOAD_DATA_LEN);
        if (err == 0) {
            err_cnt = 0;
            buf[1] -= SIMBA_MAX_UPLOAD_DATA_LEN;
            buf[0] += SIMBA_MAX_UPLOAD_DATA_LEN;
        } else {
            err_cnt++;
            if (err_cnt >= 3) {
                MCUDIAG_LOG("send data err!!!", 0);
                sys_timer_del(timer_data);
                free((void *)buf[2]);
                timer_data = 0;
                send_flag = 0;
            }
        }

    } else {
        err = ble_dongle_send_custom_data(buf[0], buf[1]);
        if (err == 0) {
            err_cnt = 0;
            sys_timer_del(timer_data);
            free((void *)buf[2]);
            timer_data = 0;
            send_flag = 0;
        } else {
            err_cnt++;
            if (err_cnt >= 3) {
                MCUDIAG_LOG("send data err!!!", 0);
                sys_timer_del(timer_data);
                free((void *)buf[2]);
                timer_data = 0;
                send_flag = 0;
            }
        }

        // 可以在这里post一个消息到某个任务,来同步消息发送完成
    }
}

// 该接口返回时并不代表数据已经发送完成了
void app_ble_send_data(u8 *data, u16 len)
{
    if (timer_data) {
        printf("app ble send data err, send busy\n");
        return;
    }
    static u32 buf[3];
    u8 *tmp_buf = (u8 *)malloc(len);
    if (tmp_buf == NULL) {
        printf("app ble send data malloc buf err\n");
        return;
    }
    memcpy(tmp_buf, data, len);
    buf[0] = (u32)tmp_buf;
    buf[1] = len;
    buf[2] = (u32)tmp_buf;
    printf("send len %d\n", len);
    timer_data = sys_timer_add(buf, ble_send_data_timer, 5);
    if (timer_data == 0) {
        free(tmp_buf);
    }
}

#if 0
void timeout_send_data(int *buf)
{
    u16 len = buf[1];
    u8 *data = buf[0];
    printf("len=%d", len);
    if (len > 500) {
        ble_dongle_send_custom_data(data, 500);
        // len-=500;
        buf[0] += 500;
        buf[1] -= 500;
        sys_timeout_add(buf, timeout_send_data, 50);

    } else {
        ble_dongle_send_custom_data(data, len);
        send_flag = 0;
    }
}
#endif
int bt_send_diag_data(uint8 *ret, int send_len)
{
    //int offset=0;
    send_flag = 1;
    app_ble_send_data(ret, send_len);
    while (send_flag) {
        putchar('L');
        uos_thread_sleep(uos_ms_to_tick(10));
    }
    return 0;
#if 0
    u32 buf[2];
    buf[0] = (u32)ret;
    buf[1] = send_len;
    sys_timeout_add(buf, timeout_send_data, 50);
    while (send_flag) {
        putchar('L');
        uos_thread_sleep(uos_ms_to_tick(10));
    }
    //for(;;)
    {
        if (send_len > 200) { //bt maximum is 230

            ble_dongle_send_custom_data(ret + offset, 200);
            send_len -= 200;
            offset += 200;

        } else {
            ble_dongle_send_custom_data(ret + offset, send_len);
            break;
        }
        os_time_dly(8);
    }
#endif
}

#endif
#if 0
BOOLEAN is_set_modem_enter_calimode(uint8 *rx_buf, uint32 rec_len)
{
    BOOLEAN ret = FALSE;
    uint8 cmdlen = 10;
    uint8 EnterCaliCmd[] = {0x7E, 0x0, 0x0, 0x0, 0x0, 0x08, 0x0, 0xFE, 0x81, 0x7E};
    cmdlen = sizeof(EnterCaliCmd) / sizeof(EnterCaliCmd[0]);
    if (rec_len == cmdlen) {
        MCUDIAG_LOG("rx_buf[7]: %x\n", 1, (*(rx_buf + 7)));
        if ((*(rx_buf + 7) == 0xFE) && (*(rx_buf + rec_len - 2) != 0x1)) {
            if ((*rx_buf == 0x7E) && (*(rx_buf + rec_len - 1) == 0x7E)) {
                MCUDIAG_LOG("is_set_modem_enter_calimode OK cmdlen: %d\n", 1, cmdlen);
                ret = TRUE;
            }

        }
    }

    return ret;
}

void ack_tool_modem_enter_calimode(char *tx_buf, uint32 ack_len)
{
    //char EnterCaliCmd[]={0x7E,0x0,0x0,0x0,0x0,0x08,0x0,0xFE,0x81,0x7E};
    bt_send_diag_data(tx_buf, ack_len);
}
#endif


BOOLEAN is_set_modem_enter_calimode(uint8 *rx_buf, uint32 rec_len)
{
    BOOLEAN ret = FALSE;
    uint8 cmdlen = 0;
    uint8 EnterCaliCmd[] = {0x7E, 0x0, 0x0, 0x0, 0x0, 0x18, 0x0, 0x68, \
                            0x0, 0x41, 0x54, 0x2B, 0x45, 0x4E, 0x54, 0x45, 0x52, \
                            0x4D, 0x4F, 0x44, 0x45, 0x3D, 0x31, 0x0D, 0x0A, 0x7E
                           };

    cmdlen = sizeof(EnterCaliCmd) / sizeof(EnterCaliCmd[0]);
    //MCUDIAG_LOG("cmdlen: %d\n", 1, cmdlen);
    //MCUDIAG_LOG("rx_buf[9]: %s\n", 1, rx_buf+9);
    if (rec_len == cmdlen) {
        if ((strstr(rx_buf + 9, "AT+ENTERMODE") != NULL) || (strstr(rx_buf + 9, "at+entermode") != NULL)) {
            if ((*rx_buf == 0x7E) && (*(rx_buf + rec_len - 1) == 0x7E)) {
                MCUDIAG_LOG("is_set_modem_enter_calimode Ok cmdlen: %d\n", 1, cmdlen);
                ret = TRUE;
            }

        }
    }

    return ret;
}

void ack_tool_modem_enter_calimode(char *tx_buf, uint32 ack_len)
{
    uint8 acklen = 0;
    uint8 EnterCaliCmd[] = {0x7E, 0x0, 0x0, 0x0, 0x0, 0x0E, 0x0, 0x9C, 0x0, 0x0D, 0x0A, 0x4F, 0x4B, 0x0D, 0x0A, 0x7E};
    acklen = sizeof(EnterCaliCmd) / sizeof(EnterCaliCmd[0]);
    bt_send_diag_data(EnterCaliCmd, acklen);
}


BOOLEAN diag_cmux_write(cmux_device_e device, cmux_channel_e channel, uint8 *data, uint32 len)
{
    BOOLEAN res;
    int w_len = 0;
    int w_offset = 0;

    if (len <= 0) {
        MCUDIAG_LOG("%s: len < 0 ", __FUNCTION__);
        return 0;
    }

    w_len = len;

    while (w_len > MAX_PACKET_DATA_SIZE) {

        res = cmux_write_user_memory(device, channel, data + w_offset, MAX_PACKET_DATA_SIZE);
        MCUDIAG_LOG("while cmux_write_user_memory return %d", res);

        if (res) {
            w_offset += MAX_PACKET_DATA_SIZE;
        }
        w_len -= MAX_PACKET_DATA_SIZE;
    }

    if (w_len > 0) {

        res = cmux_write_user_memory(device, channel, data + w_offset, w_len);
        MCUDIAG_LOG("while cmux_write_user_memory return %d", res);
        if (res) {
            w_offset += w_len;
        }
        w_len = 0;
    }

    if (w_offset == len) {
        return len;
    } else {
        MCUDIAG_LOG("cmux_write fail !!w_offset %d,len=%d", w_offset, len);
        return 0;
    }
}

void mcu_diag_threadEntry(void *argument)
{
    uint32 len = 0;
    static uint32 total_len = 0;
    uint8 *ret;
    uint8 ret_channel;
    const char *name = "diag_receive";
    uos_uint32_t RecSzFromBt = 0;
    BOOLEAN ModemEnterCali;
    uint8 count = 0;
    int cmd;
    int subcmd;
    uint8 rectime;
    dongle_diag_attr = malloc(sizeof(uos_queue_attr_t));
    dongle_diag_attr->msg_cnt = 30;
    dongle_diag_queue = uos_queue_create(dongle_diag_attr);

    extern int cmux_is_ready(int flag);
    while (cmux_is_ready(0) == FALSE) {
        MCUDIAG_LOG("diag wait cmux ready\n", 0);
        uos_thread_sleep(uos_ms_to_tick(1000));
    }

    ret_channel = diag_channel_closed();
    MCUDIAG_LOG("diag_channel_closed, ret = %d\n", 1, ret_channel);

    ret_channel = diag_channel_open();
    MCUDIAG_LOG("diag_channel_open, ret = %d\n", 1, ret_channel);

    while (1) {
        printf("uos_queue_receive wait data from bt!");
        uos_memset(rw_buf, 0, sizeof(rw_buf));
        diag_can_exit = 1;
        uos_queue_receive(dongle_diag_queue, rw_buf, &RecSzFromBt, 0);
        diag_can_exit = 0;

        cmd = 0;
        subcmd = 0;
        rectime = 1;

        MCUDIAG_LOG("+MODEM DIAG: [%X][%X][%X][%X][%X]\n", 5, rw_buf[0], rw_buf[1], rw_buf[7], rw_buf[8], rw_buf[9]);
        MCUDIAG_LOG("+MODEM DIAG: %d\n", 1, RecSzFromBt);
        ModemEnterCali = is_set_modem_enter_calimode(rw_buf, RecSzFromBt);

        MCUDIAG_LOG("ModemEnterCali : %d\n", 1, ModemEnterCali);
        if (TRUE == ModemEnterCali) {
            extern int mdmode_open(int flag);
            if (UNISOC_MODEM_OK != mdmode_open(1)) {
                MCUDIAG_LOG("ENTER CALI CMUX CONNECT FAIL TIMEOUT\n", 0);
                continue;
            }
            MCUDIAG_LOG("ModemEnterCali s\n", 0, ModemEnterCali);
            MCUDIAG_LOG("rw_buf:%s RecSzFromBt:%d\n", 2, rw_buf, RecSzFromBt);
            ack_tool_modem_enter_calimode(rw_buf, RecSzFromBt);
            MCUDIAG_LOG("ModemEnterCali e\n", 0, ModemEnterCali);
            continue;
        }

        while (cmux_is_ready(0) == FALSE) {
            MCUDIAG_LOG("diag wait cmux ready\n", 0);
            uos_thread_sleep(uos_ms_to_tick(1000));
        }

        ret_channel = diag_channel_open();
        MCUDIAG_LOG("diag_channel_open, ret = %d\n", 1, ret_channel);
        if (1) {
            extern void cmux_clear_channel(int flag);
            cmux_clear_channel(CMUX_CHANNEL_DIAG);
            //if (!cmux_write_user_memory(CMUX_DEVICE_SPI, CMUX_CHANNEL_DIAG, rw_buf, RecSzFromBt)) {
            if (!diag_cmux_write(CMUX_DEVICE_SPI, CMUX_CHANNEL_DIAG, rw_buf, RecSzFromBt)) {
                MCUDIAG_LOG("diag write errort\n", 0);
                continue;
            }

            uos_memset(rw_buf, 0, sizeof(rw_buf));
            total_len = 0;

            while (1) {
                MCUDIAG_LOG("diag read start rectime:%d\n", 1, rectime);
                rectime++;
                if ((cmd == 0x2d) && (subcmd == 0x0)) {
                    ret = cmux_read(CMUX_DEVICE_SPI, CMUX_CHANNEL_DIAG, &len, 1500);
                } else {
                    ret = cmux_read(CMUX_DEVICE_SPI, CMUX_CHANNEL_DIAG, &len, 5000);
                }
                if (ret == NULL) {
                    if (total_len != 0) {
                        MCUDIAG_LOG("rx_buf not complete\n", 0);
                        bt_send_diag_data(rw_buf, total_len);
                    }
                    MCUDIAG_LOG("diag no reponse\n", 0);
                    break;
                }

                if (len >= 10) {
                    MCUDIAG_LOG("+MODEM DIAG REC: [%X][%X][%X][%X][%X]\n", 5, ret[0], ret[1], ret[7], ret[8], ret[9]);
                    cmd = ret[7];
                    subcmd = ret[8];
                }

                uos_memcpy(rw_buf + total_len, ret, len);
                total_len += len;

                MCUDIAG_LOG("diag read total_len:%d\n", 1, total_len);
                //cmux_dump_buffer(name, ret, len);

                cmux_buffer_free(ret);
                MCUDIAG_LOG("diag need read two times RecSzFromBt:%d\n", 1, RecSzFromBt);
                if (rw_buf[total_len - 1] == 0x7e) {
                    if (((len <= 13) && (cmd == 0x2d) && (subcmd == 0x0) && (RecSzFromBt < 3000)) || ((subcmd == 0x2d))) {
                        MCUDIAG_LOG("diag need read two times len:%d\n", 1, len);
                        continue;
                    }
                    MCUDIAG_LOG("diag read done total_len=%d\n", 1, total_len);
                    bt_send_diag_data(rw_buf, total_len);
                    break;
                }
            }
            //diag_channel_closed();
        } else {
            MCUDIAG_LOG("cmux_open channel fail\n", 0);
            uos_memset(rw_buf, 0, sizeof(rw_buf));
            uos_snprintf(rw_buf, sizeof(rw_buf), "+MODEM DIAG: CHANNEL ERROR");
            bt_send_diag_data(&rw_buf, sizeof(rw_buf));
        }

    }
}

#else
uint8 mcu_diag_test_str[][100] = {
    {0x7E, 0x01, 0x01, 0x01, 0x01, 0x08, 0x00, 0x00, 0x02, 0x7E}, \
    {0x7E, 0x02, 0x02, 0x02, 0x02, 0x08, 0x00, 0x00, 0x02, 0x7E}, \
    {0x7E, 0x03, 0x03, 0x03, 0x03, 0x08, 0x00, 0x00, 0x02, 0x7E}, \
    {0x7E, 0x04, 0x04, 0x04, 0x04, 0x08, 0x00, 0x00, 0x02, 0x7E},
};

void mcu_diag_threadEntry(void *argument)
{
    int i = 0;
    uint32 len = 0;
    uint8 *ret;
    const char *name = "liyang_diag_receive";
    int j = sizeof(mcu_diag_test_str) / sizeof(mcu_diag_test_str[0]);
    while (1) {
        uos_thread_sleep(uos_ms_to_tick(2000));//RECEIVE FROM BT
        uos_memset(rw_buf, 0, sizeof(rw_buf));
        uos_memcpy(rw_buf, mcu_diag_test_str[i], 10);
        i++;
        if (i == j) {
            i = 0;
        }

        UOS_LOG_INFO("+MODEM DIAG: [%X][%X][%X][%X][%X]\n", 5, rw_buf[0], rw_buf[1], rw_buf[7], rw_buf[8], rw_buf[9]);

        while (cmux_is_ready(0) == FALSE) {
            UOS_LOG_INFO("liyang diag wait rpc ready\n", 0);
            uos_thread_sleep(uos_ms_to_tick(1000));
        }


        if (diag_channel_open()) {
            cmux_clear_channel(CMUX_CHANNEL_DIAG);
            if (!cmux_write_user_memory(CMUX_DEVICE_SPI, CMUX_CHANNEL_DIAG, rw_buf, 10)) {
                UOS_LOG_INFO("liyang diag write errort\n", 0);
                continue;
            }

            while (1) {
                UOS_LOG_INFO("liyang diag read start\n", 0);
                ret = cmux_read(CMUX_DEVICE_SPI, CMUX_CHANNEL_DIAG, &len, 10000);
                if (ret == NULL) {
                    UOS_LOG_INFO("diag no reponse\n", 0);
                    break;
                }
                UOS_LOG_INFO("liyang diag read len:%d\n", 1, len);
                cmux_dump_buffer(name, ret, len);
                UOS_LOG_INFO("liyang diag read end\n", 0);
                //SENG TO BT
                cmux_buffer_free(ret);
            }
            diag_channel_closed();
        } else {
            UOS_LOG_INFO("cmux_open channel fail\n", 0);
            uos_memset(rw_buf, 0, sizeof(rw_buf));
            uos_snprintf(rw_buf, sizeof(rw_buf), "+MODEM DIAG: CHANNEL ERROR");
            //modem_rx_mq_send(&rw_buf, sizeof(rw_buf));//SENG TO BT
        }
    }
}
#endif

int mcu_diag_init(void)
{
    if (mcu_diag_first_power) {
        uos_thread_attr_t mcu_diag_thread = {NULL};
        mcu_diag_thread.stack_size = 512;
        mcu_diag_thread.priority = 10; /* default priority */
        mcu_diag_thread.time_slice = 10;
        mcu_diag_thread.thread_name = "mcu_diag_thread";
        mcu_diag_thread.option = AUTO_START_ENABLE;
        uos_thread_create(mcu_diag_threadEntry, UOS_NULL, &mcu_diag_thread);
        mcu_diag_first_power = FALSE;
        return 1;
    } else {
        return 0;
    }
}
void mcu_diag_exit()
{
    if (diag_can_exit) {

        if (diag_thread) {

            uos_thread_destroy(diag_thread);
            uos_queue_destroy(dongle_diag_queue);
            uos_free(dongle_diag_attr);
        }
    }
}
#endif
