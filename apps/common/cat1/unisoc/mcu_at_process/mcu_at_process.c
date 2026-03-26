#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "cit_api.h"
#include "uos_deval.h"
#include "uos_osal.h"
#include "rpc.h"
#include "unisoc_at.h"
#include "mcu_at_process.h"
#include "modem_ctrl_api.h"
#include "le_smartbox_module.h"

#if TCFG_CAT1_UNISOC_ENABLE

uos_queue_t *dongle_at_queue = NULL;
uos_queue_attr_t *dongle_at_attr = NULL;
uos_thread_t *at_thread = NULL;
static u8 at_can_exit = 0;

//#ifndef MCU_AT_TEST
static char at_rwbuf[512];
//#endif

static BOOLEAN mcu_at_first_power = TRUE;

#ifndef MCU_AT_TEST

int bt_send_at_data(uint8 *ret, int send_len)
{
    int offset = 0;
    for (;;) {
        if (send_len > 230) { //bt maximum is 230

            ble_dongle_send_custom_data(ret + offset, 230);
            send_len -= 230;
            offset += 230;
        } else {
            ble_dongle_send_custom_data(ret + offset, send_len);
            break;
        }
        os_time_dly(1);
    }
    return 0;
}


int AtWaitCmuxIsNotReady(int count)
{
    while (count) {
        count--;
        MCUAT_LOG("AtWaitCmuxIsNotReady count:%d\n", 1, count);
        uos_thread_sleep(uos_ms_to_tick(1000));
        extern int cmux_is_ready(int);
        if (cmux_is_ready(0) == FALSE) {
            return 1;
        }
    }
    return 0;
}


int AtWaitCmuxIsReady(int count)
{
    while (count) {
        count--;
        MCUAT_LOG("AtWaitCmuxIsReady count:%d\n", 1, count);
        if (cmux_is_ready(0) == TRUE) {
            return 1;
        }
        uos_thread_sleep(uos_ms_to_tick(1000));
    }
    return 0;
}

int TestModemPowerOnOffByAt(uint8 *at_rwbuf)
{
    int ret = 0;

    if (strstr(at_rwbuf, "AT+MODEMREBOOT") || strstr(at_rwbuf, "at+modemreboot")) {
        ret = 1;
        MCUAT_LOG("modem reboot!!\n", 0);
        send_msg_to_modem_ctrl(MSG_MODEM_RESET);
        if (AtWaitCmuxIsNotReady(5)) {
            uos_thread_sleep(uos_ms_to_tick(2000));
            MCUAT_LOG("modem reboot poweroff ok!!\n", 0);
        } else {
            MCUAT_LOG("modem reboot poweroff fail!!\n", 0);
            bt_send_at_data("error", 5);
            return ret;
        }
        if (AtWaitCmuxIsReady(10)) {
            MCUAT_LOG("modem reboot OK to BT!!\n", 0);
            bt_send_at_data("ok", 2);
        } else {
            MCUAT_LOG("modem reboot ERROR to BT!!\n", 0);
            bt_send_at_data("error", 5);
        }
    }

    if (strstr(at_rwbuf, "AT+MODEMPOWEROFF") || strstr(at_rwbuf, "at+modempoweroff")) {
        ret = 1;
        MCUAT_LOG("modem poweroff!!\n", 0);
        send_msg_to_modem_ctrl(MSG_MODEM_POWEROFF);
        if (AtWaitCmuxIsNotReady(5)) {
            uos_thread_sleep(uos_ms_to_tick(2000));
            MCUAT_LOG("modem poweroff OK to BT!!\n", 0);
            bt_send_at_data("ok", 2);
        } else {
            bt_send_at_data("error", 5);
            MCUAT_LOG("modem poweron ERROR to BT!!\n", 0);
        }
    }

    if (strstr(at_rwbuf, "AT+MODEMPOWERON") || strstr(at_rwbuf, "at+modempoweron")) {
        ret = 1;
        MCUAT_LOG("modem poweron!!\n", 0);
        send_msg_to_modem_ctrl(MSG_MODEM_POWERON);
        if (AtWaitCmuxIsReady(10)) {
            MCUAT_LOG("modem poweron OK to BT!!\n", 0);
            bt_send_at_data("ok", 2);
        } else {
            MCUAT_LOG("modem poweron ERROR to BT!!\n", 0);
            bt_send_at_data("error", 5);
        }
    }

    return ret;
}

void mcu_at_threadEntry(void *argument)
{
    uint32 len = 0;
    BOOLEAN ret;
    const char *name = "at_receive";
    char *cmderr = "+MODEM AT: CMD ERROR";

    dongle_at_attr = uos_malloc(sizeof(uos_queue_attr_t));
    dongle_at_attr->msg_cnt = 30;
    dongle_at_queue = uos_queue_create(dongle_at_attr);

    while (cmux_is_ready(0) == FALSE) {
        MCUAT_LOG("at wait cumx ready\n", 0);
        uos_thread_sleep(uos_ms_to_tick(1000));
    }

    ret = at_channel_closed();
    MCUAT_LOG("at_channel_closed ret = %d\n", 1, ret);
    ret = at_channel_open();
    MCUAT_LOG("at_channel_open ret = %d\n", 1, ret);

    while (1) {
        printf("uos_queue_receive wait at data from bt!");
        uos_memset(at_rwbuf, 0, sizeof(at_rwbuf));
        at_can_exit = 1;
        uos_queue_receive(dongle_at_queue, at_rwbuf, 0, 0);
        at_can_exit = 0;

        MCUAT_LOG("+MODEM AT: [%s]\n", 1, at_rwbuf);

        if (1 == TestModemPowerOnOffByAt(at_rwbuf)) {
            continue;
        }
        if (AtWaitCmuxIsReady(15) == 0) {
            char *TmpResp = "CmuxNotReady 15s Timeout,you can sent AT+MODEMPOWERON";
            MCUAT_LOG("%s", 1, TmpResp);
            bt_send_at_data(TmpResp, strlen(TmpResp));
            continue;
        }
        while (cmux_is_ready(0) == FALSE) {
            MCUAT_LOG("at wait cumx ready\n", 0);
            uos_thread_sleep(uos_ms_to_tick(1000));
        }

        ret = at_channel_open();
        MCUAT_LOG("at_channel_open while ret = %d\n", 1, ret);

        if (1) {
            char *at_response = NULL;

            uint32 at_response_len = 0;

            boolean at_ret = at_send_sync((uint8 *)&at_rwbuf, (uint8 **)&at_response, &at_response_len);
            UOS_LOG_INFO("at_response = %s, response_len = %d, at_ret = %d\n", 3, at_response,
                         at_response_len, at_ret);

            uos_memset(at_rwbuf, 0, sizeof(at_rwbuf));
            if (at_response_len > 0) {
                MCUAT_LOG("at_response: %s, len: %d\n", 2, (char *)at_response, strlen(at_response));
                bt_send_at_data((char *)at_response, strlen(at_response));
            } else {
                MCUAT_LOG("+MODEM AT: CMD ERROR\n", 0);
                bt_send_at_data((char *)cmderr, strlen(cmderr));
            }

            at_response_free(at_response);
        } else {
            MCUAT_LOG("cmux_open at channel fail\n", 0);
            uos_memset(at_rwbuf, 0, sizeof(at_rwbuf));
            uos_snprintf(at_rwbuf, sizeof(at_rwbuf), "+MODEM AT: CHANNEL ERROR");
            bt_send_at_data(&at_rwbuf, strlen(at_rwbuf));
        }
    }
}

#else
uint8 mcu_at_test_str[][100] = {
    "AT+ARMLOG=1", \
    "AT+ARMLOG=?", \
    "AT+ARMLOG?", \
    "AT",
};

void mcu_at_threadEntry(void *argument)
{
    int i = 0;
    int j = sizeof(mcu_at_test_str) / sizeof(mcu_at_test_str[0]);
    while (1) {
        uos_thread_sleep(uos_ms_to_tick(2000));
        uos_memset(at_rwbuf, 0, sizeof(at_rwbuf));
        uos_memcpy(at_rwbuf, mcu_at_test_str[i], uos_strlen(mcu_at_test_str[i]));
        i++;
        if (i == j) {
            i = 0;
        }

        //wait forever BT
        /*
         int ret = mq_receive(tx_fd, &at_rwbuf, sizeof(at_rwbuf), NULL);
         if (ret < 0) {
             UOS_LOG_INFO("mq_receive error: %d\n", 1, errno);
             continue;
         }*/

        UOS_LOG_INFO("+MODEM AT: [%s]\n", 1, at_rwbuf);

        while (cmux_is_ready(0) == FALSE) {
            MCUAT_LOG("at wait cumx ready\n", 0);
            uos_thread_sleep(uos_ms_to_tick(1000));
        }

        if (at_channel_open()) {
            char *at_response = NULL;
            uint32 at_response_len = 0;

            boolean at_ret = at_send_sync((uint8 *)&at_rwbuf, (uint8 **)&at_response, &at_response_len);
            UOS_LOG_INFO("at_response = %s, response_len = %d, at_ret = %d\n", 3, at_response,
                         at_response_len, at_ret);
            uos_memset(at_rwbuf, 0, sizeof(at_rwbuf));
            if (at_response_len > 0) {
                uos_snprintf(at_rwbuf, sizeof(at_rwbuf), "%s", at_response);
            } else {
                uos_snprintf(at_rwbuf, sizeof(at_rwbuf), "+MODEM AT: CMD ERROR");
            }
            //modem_rx_mq_send(&at_rwbuf, sizeof(at_rwbuf));
            at_response_free(at_response);
            at_channel_closed();
        } else {
            UOS_LOG_INFO("cmux_open channel fail\n", 0);
            uos_memset(at_rwbuf, 0, sizeof(at_rwbuf));
            uos_snprintf(at_rwbuf, sizeof(at_rwbuf), "+MODEM AT: CHANNEL ERROR");
            //modem_rx_mq_send(&at_rwbuf, sizeof(at_rwbuf));
        }
    }
}
#endif

int mcu_at_init(void)
{
    if (mcu_at_first_power) {
        uos_thread_attr_t mcu_at_thread = {NULL};
        mcu_at_thread.stack_size = 512;
        mcu_at_thread.priority = 10; /* default priority */
        mcu_at_thread.time_slice = 10;
        mcu_at_thread.thread_name = "mcu_at_thread";
        mcu_at_thread.option = AUTO_START_ENABLE;
        at_thread = uos_thread_create(mcu_at_threadEntry, UOS_NULL, &mcu_at_thread);
        mcu_at_first_power = FALSE;
        return 1;
    } else {
        return 0;
    }
}
void mcu_at_exit()
{
    if (at_can_exit) {

        if (at_thread) {

            uos_thread_destroy(at_thread);
            uos_queue_destroy(dongle_at_queue);
            uos_free(dongle_at_attr);
        }
    }
}
#endif
