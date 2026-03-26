#include "system/includes.h"
#include "multi_protocol_main.h"
#include "classic/tws_api.h"

#if (SMART_BOX_EN | GFPS_EN)
/* #if (BT_AI_SEL_PROTOCOL & (RCSP_MODE_EN | GFPS_EN)) */

#if 0
#define log_info(x, ...)       printf("[MULTI_PROTOCOL]" x " ", ## __VA_ARGS__)
#define log_info_hexdump       put_buf
#else
#define log_info(...)
#define log_info_hexdump(...)
#endif

void *multi_protocol_ble_hdl = NULL;
void *multi_protocol_spp_hdl = NULL;
extern void *rcsp_server_ble_hdl;
extern void *gfps_app_ble_hdl;

static const char *const phy_result[] = {
    "None",
    "1M",
    "2M",
    "Coded"
};

static uint16_t multi_protocol_att_read_callback(void *hdl, hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t offset, uint8_t *buffer, uint16_t buffer_size)
{

    uint16_t  att_value_len = 0;
    uint16_t handle = att_handle;
    /* log_info("read_callback, handle= 0x%04x,buffer= %08x\n", handle, (u32)buffer); */

    switch (handle) {
    default:
        break;
    }

    log_info("att_value_len= %d\n", att_value_len);
    return att_value_len;

}

static int multi_protocol_att_write_callback(void *hdl, hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size)
{
    int result = 0;
    u16 tmp16;
    u16 handle = att_handle;
    /* log_info("write_callback, handle= 0x%04x\n", handle); */

    switch (handle) {
    default:
        break;
    }

    return 0;
}

static void multi_protocol_cbk_packet_handler(void *hdl, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    int mtu;
    u32 tmp;
    const char *attribute_name;
    u16 temp_con_handle;
    int temp_index;

    switch (packet_type) {
    case HCI_EVENT_PACKET:
        switch (hci_event_packet_get_type(packet)) {
        case ATT_EVENT_HANDLE_VALUE_INDICATION_COMPLETE:
            log_info("ATT_EVENT_HANDLE_VALUE_INDICATION_COMPLETE\n");
        case ATT_EVENT_CAN_SEND_NOW:
            /* can_send_now_wakeup(); */
            break;
        case HCI_EVENT_LE_META:
            switch (hci_event_le_meta_get_subevent_code(packet)) {
            case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
                if (!hci_subevent_le_connection_complete_get_role(packet)) {
                    // GATT_ROLE_CLIENT
                    break;
                } else {
                    // GATT_ROLE_SERVER;
                }
                temp_con_handle = little_endian_read_16(packet, 4);
                log_info("HCI_SUBEVENT_LE_CONNECTION_COMPLETE: %0x\n", temp_con_handle);
                log_info_hexdump(packet + 7, 7);

                temp_index = get_att_handle_idle();
                if (temp_index == -1) {
                    printf("BLE connect handle list is FULL !!!\n");
                    ble_op_disconnect_ext(temp_con_handle, 0x13);
                }
                ble_op_multi_att_send_conn_handle(temp_con_handle, temp_index, 0);
                break;
                /* case HCI_SUBEVENT_LE_DATA_LENGTH_CHANGE: */
                /* log_info("conn_handle = %04x\n", little_endian_read_16(packet, 4)); */
                /* log_info("max tx = %d; max rx = %d\n", little_endian_read_16(packet, 6), little_endian_read_16(packet, 10)); */
                /* log_info("APP HCI_SUBEVENT_LE_DATA_LENGTH_CHANGE\n"); */
                /* put_buf(packet, size); */
                /* break; */

                /* case HCI_SUBEVENT_LE_CONNECTION_UPDATE_COMPLETE: */
                /* temp_con_handle = little_endian_read_16(packet, 4); */
                /* break; */

                /* case HCI_SUBEVENT_LE_DATA_LENGTH_CHANGE: */
                /* temp_con_handle = little_endian_read_16(packet, 3); */
                /* log_info("HCI_SUBEVENT_LE_DATA_LENGTH_CHANGE\n"); */
                /* break; */

                /* case HCI_SUBEVENT_LE_PHY_UPDATE_COMPLETE: */
                /* temp_con_handle = little_endian_read_16(packet, 4); */
                /* if (temp_con_handle) { */
                /* log_info("HCI_SUBEVENT_LE_PHY_UPDATE %s\n", hci_event_le_meta_get_phy_update_complete_status(packet) ? "Fail" : "Succ"); */
                /* log_info("Tx PHY: %s\n", phy_result[hci_event_le_meta_get_phy_update_complete_tx_phy(packet)]); */
                /* log_info("Rx PHY: %s\n", phy_result[hci_event_le_meta_get_phy_update_complete_rx_phy(packet)]); */
                /* } */
                /* break; */
            }
            break;

        case HCI_EVENT_DISCONNECTION_COMPLETE:
            temp_con_handle = little_endian_read_16(packet, 3);

            log_info("HCI_EVENT_DISCONNECTION_COMPLETE: %0x\n", packet[5]);
            multi_att_clear_ccc_config(temp_con_handle);

            extern u8 get_att_handle_index(u16 conn_handle);
            temp_index = get_att_handle_index(temp_con_handle);
            ble_op_multi_att_send_conn_handle(0, temp_index, 0);

            break;

            /* case ATT_EVENT_MTU_EXCHANGE_COMPLETE: */
            /* temp_con_handle = little_endian_read_16(packet, 2); */

            /* mtu = att_event_mtu_exchange_complete_get_MTU(packet) - 3; */
            /* log_info("ATT MTU = %u\n", mtu); */
            /* ble_user_cmd_prepare(BLE_CMD_MULTI_ATT_MTU_SIZE, 2, temp_con_handle, mtu); */
            /* break; */

            /* case HCI_EVENT_VENDOR_REMOTE_TEST: */
            /* log_info("--- HCI_EVENT_VENDOR_REMOTE_TEST\n"); */
            /* break; */

            /* case L2CAP_EVENT_CONNECTION_PARAMETER_UPDATE_RESPONSE: */
            /* temp_con_handle = little_endian_read_16(packet, 2); */
            /* tmp = little_endian_read_16(packet, 4); */
            /* log_info("-update_rsp: %02x\n", tmp); */
            /* break; */

            /* case HCI_EVENT_ENCRYPTION_CHANGE: */
            /* temp_con_handle = little_endian_read_16(packet, 3); */
            /* log_info("HCI_EVENT_ENCRYPTION_CHANGE= %d\n", packet[2]); */
            /* break; */
        }
        break;
    }
}

static void multi_protocol_cbk_sm_packet_handler(void *hdl, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    sm_just_event_t *event = (void *)packet;
    u32 tmp32;
    switch (packet_type) {
    case HCI_EVENT_PACKET:
        switch (hci_event_packet_get_type(packet)) {
        case SM_EVENT_JUST_WORKS_REQUEST:
            sm_just_works_confirm(sm_event_just_works_request_get_handle(packet));
            log_info("Just Works Confirmed.\n");
            break;
        case SM_EVENT_PASSKEY_DISPLAY_NUMBER:
            log_info_hexdump(packet, size);
            memcpy(&tmp32, event->data, 4);
            log_info("Passkey display: %06u.\n", tmp32);
            break;

        case SM_EVENT_PAIR_PROCESS:
            /* log_info("======PAIR_PROCESS: %02x\n", event->data[0]); */
            /* put_buf(event->data, 4); */
            switch (event->data[0]) {
            case SM_EVENT_PAIR_SUB_RECONNECT_START:
                break;

            case SM_EVENT_PAIR_SUB_PIN_KEY_MISS:
            case SM_EVENT_PAIR_SUB_PAIR_FAIL:
            case SM_EVENT_PAIR_SUB_PAIR_TIMEOUT:
            case SM_EVENT_PAIR_SUB_ADD_LIST_SUCCESS:
            case SM_EVENT_PAIR_SUB_ADD_LIST_FAILED:
            default:
                break;
            }
            break;
        }
        break;
    }
}


/* #if (BT_AI_SEL_PROTOCOL & RCSP_MODE_EN) */
/* extern void *bt_rcsp_spp_hdl; */
/* extern void *rcsp_server_ble_hdl; */
/* static u8 rcsp_spp_connected = 0; */
/* static u8 rcsp_ble_connected = 0; */
/* #endif */

/* #if (BT_AI_SEL_PROTOCOL & GFPS_EN) */
/* extern void *gfps_app_spp_hdl; */
/* extern void *gfps_app_ble_hdl; */
/* static u8 gfps_spp_connected = 0; */
/* static u8 gfps_ble_connected = 0; */
/* #endif */

// type   0:ble   1:spp
// state  0:disconnected    1:connected
static u8 connected_state[2] = {0};
static void multi_protocol_connect_callback(void *hdl, u8 state, u8 type)
{
    log_info("multi_protocol_connect_callback hdl:0x%x state:%d\n", (u32)hdl, state);
    switch (state) {
    case APP_PROTOCOL_CONNECTED:
        log_info("APP_PROTOCOL_CONNECTED %x\n", (u32)hdl);
        // close all other hdl
        /* #if (BT_AI_SEL_PROTOCOL & RCSP_MODE_EN) */
        /* log_info("rcsp_spp_hdl:%x\n", (u32)bt_rcsp_spp_hdl); */
        /* log_info("rcsp_ble_hdl:%x\n", (u32)rcsp_server_ble_hdl); */
        /* if (hdl == rcsp_server_ble_hdl) { */
        /* rcsp_ble_connected = 1; */
        /* [> app_ble_adv_enable(hdl, 0); <] */
        /* } */

        /* if (hdl == bt_rcsp_spp_hdl) { */
        /* rcsp_spp_connected = 1; */
        /* } */

        /* [> if ((hdl != bt_rcsp_spp_hdl) && (hdl != rcsp_server_ble_hdl)) { <] */
        /* [> log_info("close spp hdl rcsp\n"); <] */
        /* [> app_ble_sleep_hdl(rcsp_server_ble_hdl, 1); <] */
        /* [> app_spp_sleep_hdl(bt_rcsp_spp_hdl, 1); <] */
        /* [> } <] */
        /* #endif */
        /* #if (BT_AI_SEL_PROTOCOL & GFPS_EN) */
        /* log_info("gfps_spp_hdl:%x\n", (u32)gfps_app_spp_hdl); */
        /* log_info("gfps_ble_hdl:%x\n", (u32)gfps_app_ble_hdl); */
        /* if (hdl == gfps_app_ble_hdl) { */
        /* gfps_ble_connected = 1; */
        /* } */

        /* if (hdl == gfps_app_spp_hdl) { */
        /* gfps_spp_connected = 1; */
        /* } */

        /* if ((hdl != gfps_app_ble_hdl) \ */
        /* && (hdl != gfps_app_spp_hdl) \ */
        /* && (gfps_spp_connected == 0) \ */
        /* && (gfps_ble_connected == 0) \ */
        /* ) { */
        /* log_info("close ble hdl gfps\n"); */
        /* app_ble_sleep_hdl(gfps_app_ble_hdl, 1); */
        /* app_spp_sleep_hdl(gfps_app_spp_hdl, 1); */
        /* } */
        /* #endif */
        connected_state[type]++;
        break;
    case APP_PROTOCOL_DISCONNECTED:
        log_info("APP_PROTOCOL_DISCONNECTED %x\n", (u32)hdl);

        /* #if (BT_AI_SEL_PROTOCOL & RCSP_MODE_EN) */
        /* if (hdl == rcsp_server_ble_hdl) { */
        /* rcsp_ble_connected = 0; */
        /* } */

        /* if (hdl == bt_rcsp_spp_hdl) { */
        /* rcsp_spp_connected = 0; */
        /* } */
        /* #endif */

        /* #if (BT_AI_SEL_PROTOCOL & GFPS_EN) */
        /* if (hdl == gfps_app_ble_hdl) { */
        /* gfps_ble_connected = 0; */
        /* } */

        /* if (hdl == gfps_app_spp_hdl) { */
        /* gfps_spp_connected = 0; */
        /* } */
        /* #endif */

        connected_state[type]--;
        if ((connected_state[0] == 0) && (connected_state[1] == 0)) {
            log_info("wakeup all protocol !\n");
            /* #if (BT_AI_SEL_PROTOCOL & RCSP_MODE_EN) */
            /* app_ble_sleep_hdl(rcsp_server_ble_hdl, 0); */
            /* app_spp_sleep_hdl(bt_rcsp_spp_hdl, 0); */
            /* #endif */
            /* #if (BT_AI_SEL_PROTOCOL & GFPS_EN) */
            /* app_ble_sleep_hdl(gfps_app_ble_hdl, 0); */
            /* app_spp_sleep_hdl(gfps_app_spp_hdl, 0); */
            /* #endif */
        }
        break;
    }
}

static void multi_protocol_ble_connect_callback(void *hdl, u8 state)
{
    multi_protocol_connect_callback(hdl, state, 0);
}

static void multi_protocol_spp_connect_callback(void *hdl, u8 state)
{
    multi_protocol_connect_callback(hdl, state, 1);
}

void multi_protocol_common_callback_init(void)
{
    if (multi_protocol_ble_hdl == NULL) {
        multi_protocol_ble_hdl = app_ble_hdl_alloc();
        if (multi_protocol_ble_hdl == NULL) {
            log_info("multi_protocol_ble_hdl alloc err !!\n");
            return;
        }
    }

    /* app_ble_att_read_callback_register(multi_protocol_ble_hdl, multi_protocol_att_read_callback); */
    /* app_ble_att_write_callback_register(multi_protocol_ble_hdl, multi_protocol_att_write_callback); */
    /* app_ble_att_server_packet_handler_register(multi_protocol_ble_hdl, multi_protocol_cbk_packet_handler); */
    /* app_ble_hci_event_callback_register(multi_protocol_ble_hdl, multi_protocol_cbk_packet_handler); */
    /* app_ble_l2cap_packet_handler_register(multi_protocol_ble_hdl, multi_protocol_cbk_packet_handler); */
    app_ble_sm_event_callback_register(multi_protocol_ble_hdl, multi_protocol_cbk_sm_packet_handler);
    app_ble_protocol_connect_callback_register(multi_protocol_ble_hdl, multi_protocol_ble_connect_callback);

    if (multi_protocol_spp_hdl == NULL) {
        multi_protocol_spp_hdl = app_spp_hdl_alloc(0);
        if (multi_protocol_spp_hdl == NULL) {
            log_info("multi_protocol_spp_hdl alloc err !!\n");
            return;
        }
    }

    app_spp_protocol_connect_callback_register(multi_protocol_spp_hdl, multi_protocol_spp_connect_callback);
}

void multi_protocol_common_callback_exit(void)
{
    if (multi_protocol_ble_hdl != NULL) {
        app_ble_hdl_free(multi_protocol_ble_hdl);
        multi_protocol_ble_hdl = NULL;
    }
    if (multi_protocol_spp_hdl != NULL) {
        app_spp_hdl_free(multi_protocol_spp_hdl);
        multi_protocol_spp_hdl = NULL;
    }
}

#endif

