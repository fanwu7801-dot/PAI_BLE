#include "system/includes.h"
#include "multi_protocol_main.h"
#include "classic/tws_api.h"
#include "classic/tws_event.h"
#include "btstack/avctp_user.h"
#include "app_main.h"

#if 1
/* #if (BT_AI_SEL_PROTOCOL & (RCSP_MODE_EN | GFPS_EN)) */

#if 0
#define log_info(x, ...)       printf("[MULTI_PROTOCOL]" x " ", ## __VA_ARGS__)
#define log_info_hexdump       put_buf
#else
#define log_info(...)
#define log_info_hexdump(...)
#endif


static int multi_protocol_bt_status_event_handler(struct bt_event *bt)
{
    /* #if (BT_AI_SEL_PROTOCOL & GFPS_EN) */
    /* gfps_bt_status_event_handler(bt); */
    /* #endif */
    return 0;
}

static void multi_protocol_bt_tws_event_handler(struct tws_event *tws)
{
    /* #if (BT_AI_SEL_PROTOCOL & GFPS_EN) */
    /* gfps_bt_tws_event_handler(tws); */
    /* #endif */
}

static int bt_app_status_event_handler(int *msg)
{
    struct bt_event *bt = (struct bt_event *)msg;
    if (tws_api_get_role() == TWS_ROLE_SLAVE) {
        return 0;
    }
    multi_protocol_bt_status_event_handler(bt);
    return 0;
}
/* APP_MSG_HANDLER(multi_protocol_msg_handler) = { */
/* .owner      = 0xff, */
/* .from       = MSG_FROM_BT_STACK, */
/* .handler    = bt_app_status_event_handler, */
/* }; */
static int bt_app_tws_status_event_handler(int *msg)
{
    struct tws_event *evt = (struct tws_event *)msg;
    multi_protocol_bt_tws_event_handler(evt);
    return 0;
}
/* APP_MSG_HANDLER(multi_protocol_tws_msg_handler_stub) = { */
/* .owner      = 0xff, */
/* .from       = MSG_FROM_TWS, */
/* .handler    = bt_app_tws_status_event_handler, */
/* }; */


static int multi_protocol_bt_hci_event_handler(struct bt_event *bt)
{
    void *cur_adv_hdl = NULL;
    log_info("multi protocol bt hci event handler\n");
    switch (bt->event) {
    case HCI_EVENT_DISCONNECTION_COMPLETE :
        log_info("HCI_EVENT_DISCONNECTION_COMPLETE \n");

        break;

    case BTSTACK_EVENT_HCI_CONNECTIONS_DELETE:
    case HCI_EVENT_CONNECTION_COMPLETE:
        log_info(" HCI_EVENT_CONNECTION_COMPLETE \n");
        break;
    default:
        break;

    }
    return 0;
}

int multi_protocol_hci_event_handler(int *event)
{
    multi_protocol_bt_hci_event_handler((struct bt_event *)event);
    return 0;
}
/* APP_MSG_HANDLER(multi_protocol_hci_msg_handler) = { */
/* .owner      = 0xff, */
/* .from       = MSG_FROM_BT_HCI, */
/* .handler    = multi_protocol_hci_event_handler, */
/* }; */

#if 0
static void __tws_rx_from_sibling(u8 *data)
{
    u16 opcode = (data[1] << 8) | data[0];
    u16 len = (data[3] << 8) | data[2];
    u8 *rx_data = data + 4;

    switch (opcode) {
    case APP_PROTOCOL_TWS_FOR_LIB:
        APP_PROTOCOL_LOG("APP_PROTOCOL_TWS_FOR_LIB");
        /* #if (BT_AI_SEL_PROTOCOL & GFPS_EN) */
        /* gfps_tws_data_deal(rx_data, len); */
        /* #endif */
        break;
    }

    free(data);
}

static void app_protocol_rx_from_sibling(void *_data, u16 len, bool rx)
{
    int err = 0;
    if (rx) {
        /* log_info(">>>%s \n", __func__); */
        /* log_info("len :%d\n", len); */
        /* put_buf(_data, len); */
        u8 *rx_data = malloc(len);
        memcpy(rx_data, _data, len);

        int msg[4];
        msg[0] = (int)__tws_rx_from_sibling;
        msg[1] = 1;
        msg[2] = (int)rx_data;
        err = os_taskq_post_type("app_core", Q_CALLBACK, 3, msg);
        if (err) {
            log_info("tws rx post fail\n");
        }
    }
}

//发送给对耳
REGISTER_TWS_FUNC_STUB(multi_protocol_tws_rx_sync_stub) = {
    .func_id = APP_PROTOCOL_TWS_SEND_ID,
    .func    = app_protocol_rx_from_sibling,
};
#endif

#endif

