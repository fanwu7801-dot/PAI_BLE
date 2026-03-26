#include "system/includes.h"
#include "app_config.h"
#include "btcontroller_config.h"
#include "btstack/bt_profile_config.h"
#include "bt_common.h"
#include "btstack/avctp_user.h"
#include "btstack/le/ble_api.h"
#include "btstack/le/att.h"

#if WECHAT_SPORT_ENABLE
extern const u8 adt_profile_support;

//需要和ble的profile表相同
static const uint8_t self_define_profile_data[] = {
//////////////////////////////////////////////////////
    //
    // 0x0001 PRIMARY_SERVICE  1800
    //
    //////////////////////////////////////////////////////
    0x0a, 0x00, 0x02, 0x00, 0x01, 0x00, 0x00, 0x28, 0x00, 0x18,

    /* CHARACTERISTIC,  2a00, READ | WRITE | DYNAMIC, */
    // 0x0002 CHARACTERISTIC 2a00 READ | WRITE | DYNAMIC
    0x0d, 0x00, 0x02, 0x00, 0x02, 0x00, 0x03, 0x28, 0x0a, 0x03, 0x00, 0x00, 0x2a,
    // 0x0003 VALUE 2a00 READ | WRITE | DYNAMIC
    0x08, 0x00, 0x0a, 0x01, 0x03, 0x00, 0x00, 0x2a,

    //////////////////////////////////////////////////////
    //
    // 0x0004 PRIMARY_SERVICE  1801
    //
    //////////////////////////////////////////////////////
    0x0a, 0x00, 0x02, 0x00, 0x04, 0x00, 0x00, 0x28, 0x01, 0x18,

    /* CHARACTERISTIC,  2e05, INDICATE, */
    // 0x0005 CHARACTERISTIC 2e05 INDICATE
    0x0d, 0x00, 0x02, 0x00, 0x05, 0x00, 0x03, 0x28, 0x20, 0x06, 0x00, 0x05, 0x2a,
    // 0x0006 VALUE 2e05 INDICATE
    0x08, 0x00, 0x20, 0x00, 0x06, 0x00, 0x05, 0x2a,
    // 0x0007 CLIENT_CHARACTERISTIC_CONFIGURATION
    0x0a, 0x00, 0x0a, 0x01, 0x07, 0x00, 0x02, 0x29, 0x00, 0x00,

    //////////////////////////////////////////////////////
    //
    // 0x0008 PRIMARY_SERVICE  ae00
    //
    //////////////////////////////////////////////////////
    0x0a, 0x00, 0x02, 0x00, 0x08, 0x00, 0x00, 0x28, 0x00, 0xae,

    /* CHARACTERISTIC,  ae01, WRITE_WITHOUT_RESPONSE | DYNAMIC, */
    // 0x0009 CHARACTERISTIC ae01 WRITE_WITHOUT_RESPONSE | DYNAMIC
    0x0d, 0x00, 0x02, 0x00, 0x09, 0x00, 0x03, 0x28, 0x04, 0x0a, 0x00, 0x01, 0xae,
    // 0x000a VALUE ae01 WRITE_WITHOUT_RESPONSE | DYNAMIC
    0x08, 0x00, 0x04, 0x01, 0x0a, 0x00, 0x01, 0xae,

    /* CHARACTERISTIC,  ae02, NOTIFY, */
    // 0x000b CHARACTERISTIC ae02 NOTIFY
    0x0d, 0x00, 0x02, 0x00, 0x0b, 0x00, 0x03, 0x28, 0x10, 0x0c, 0x00, 0x02, 0xae,
    // 0x000c VALUE ae02 NOTIFY
    0x08, 0x00, 0x10, 0x00, 0x0c, 0x00, 0x02, 0xae,
    // 0x000d CLIENT_CHARACTERISTIC_CONFIGURATION
    0x0a, 0x00, 0x0a, 0x01, 0x0d, 0x00, 0x02, 0x29, 0x00, 0x00,

    //////////////////////////////////////////////////////
    //
    // 0x000e PRIMARY_SERVICE  fee7
    //
    //////////////////////////////////////////////////////
    0x0a, 0x00, 0x02, 0x00, 0x0e, 0x00, 0x00, 0x28, 0xe7, 0xfe,
    /* 0x0e, 0x00, 0x02, 0x00, 0x0f, 0x00, 0x02, 0x28, 0x0e, 0x00, 0x13, 0x00, 0xe7, 0xfe, */

    /* CHARACTERISTIC,  fec9, READ | DYNAMIC, */
    // 0x000f CHARACTERISTIC fec9 READ | DYNAMIC
    0x0d, 0x00, 0x02, 0x00, 0x0f, 0x00, 0x03, 0x28, 0x12, 0x10, 0x00, 0xc9, 0xfe,
    // 0x0010 VALUE fec9 READ | DYNAMIC
    0x08, 0x00, 0x02, 0x01, 0x10, 0x00, 0xc9, 0xfe,

    /* CHARACTERISTIC,  fea1, READ | NOTIFY | DYNAMIC, */
    // 0x0011 CHARACTERISTIC fea1 READ | NOTIFY | DYNAMIC
    0x0d, 0x00, 0x02, 0x00, 0x11, 0x00, 0x03, 0x28, 0x12, 0x12, 0x00, 0xa1, 0xfe,
    // 0x0012 VALUE fea1 READ | NOTIFY | DYNAMIC
    0x08, 0x00, 0x12, 0x01, 0x12, 0x00, 0xa1, 0xfe,
    // 0x0013 CLIENT_CHARACTERISTIC_CONFIGURATION
    0x0a, 0x00, 0x0a, 0x01, 0x13, 0x00, 0x02, 0x29, 0x00, 0x00,

    // END
    0x00, 0x00,
};
//
// characteristics <--> handles
//
/* #define ATT_CHARACTERISTIC_2a00_01_VALUE_HANDLE 0x0003 */
/* #define ATT_CHARACTERISTIC_2e05_01_VALUE_HANDLE 0x0006 */
/* #define ATT_CHARACTERISTIC_2e05_01_CLIENT_CONFIGURATION_HANDLE 0x0007 */
/* #define ATT_CHARACTERISTIC_ae01_01_VALUE_HANDLE 0x000a */
/* #define ATT_CHARACTERISTIC_ae02_01_VALUE_HANDLE 0x000c */
/* #define ATT_CHARACTERISTIC_ae02_01_CLIENT_CONFIGURATION_HANDLE 0x000d */
/* #define ATT_CHARACTERISTIC_fec9_01_VALUE_HANDLE 0x0010 */
#define ATT_CHARACTERISTIC_fea1_01_VALUE_HANDLE 0x0012
/* #define ATT_CHARACTERISTIC_fea1_01_CLIENT_CONFIGURATION_HANDLE 0x0013 */

const u8 sdp_att_service_data[60] = {
    0x36, 0x00, 0x31, 0x09, 0x00, 0x00, 0x0A, 0x00, 0x01, 0x00, 0x21, 0x09, 0x00, 0x01, 0x35, 0x03,
    0x19, 0xfe, 0xe7, 0x09, 0x00, 0x04, 0x35, 0x13, 0x35, 0x06, 0x19, 0x01, 0x00, 0x09, 0x00, 0x1F,
    0x35, 0x09, 0x19, 0x00, 0x07, 0x09, 0x00, 0x01, 0x09, 0x00, 0x13, 0x09, 0x00, 0x05, 0x35, 0x03,
    0x19, 0x10, 0x02, 0x00
};
typedef struct {
    // linked list - assert: first field
    void *offset_item;

    // data is contained in same memory
    u32        service_record_handle;
    u8         *service_record;
} service_record_item_t;
#define SDP_RECORD_HANDLER_REGISTER(handler) \
	const service_record_item_t  handler \
		sec(.sdp_record_item)
SDP_RECORD_HANDLER_REGISTER(spp_att_record_item) = {
    .service_record = (u8 *)sdp_att_service_data,
    .service_record_handle = 0x00010021,
};

//------

extern void set_adt_send_handle(u16 handle_val);
extern void set_adt_att_op_type(u8 att_op_type);
//---------------

void begin_to_connect_att(void)
{
    if (adt_profile_support) {
        printf("------------USER_CTRL_ADT_CONNECT-------------\n");
        user_send_cmd_prepare(USER_CTRL_ADT_CONNECT, 0, NULL);
    }
}

extern u32 pedometer;
extern u8 wechat_data[];
void wechat_edr_send_data(void *pive)
{
    set_adt_send_handle(ATT_CHARACTERISTIC_fea1_01_VALUE_HANDLE);
    set_adt_att_op_type(ATT_OP_INDICATE);
    user_send_cmd_prepare(USER_CTRL_ADT_SEND_DATA, 4, wechat_data);
}

enum {
    ATT_EVENT_CONNECT = 1,
    ATT_EVENT_DISCONNECT,
    ATT_EVENT_MONITOR_START,//监听开始
    ATT_EVENT_PACKET_HANDLER = 7,
};
u16 edr_att_conn_hadle = 0;
static void att_packet_handler(u8 packet_type, u16 channel, u8 *packet, u16 size)
{
    switch (packet_type) {
    case ATT_EVENT_CONNECT:
        printf(">ATT_EVENT_CONNECT>>channel = %x\n", channel);
        edr_att_conn_hadle = channel;
        ble_op_multi_att_send_conn_handle(channel, 1, 0);
        break;
    case ATT_EVENT_DISCONNECT:
        printf(">ATT_EVENT_DISCONNECT\n");
        edr_att_conn_hadle = 0;
        ble_op_multi_att_send_conn_handle(0, 1, 0);
        break;
    case ATT_EVENT_MONITOR_START:
    case ATT_EVENT_PACKET_HANDLER:
        break;
    }
}


void att_profile_init()
{
    extern void att_event_handler_register(void (*handler)(u8 packet_type, u16 channel, u8 * packet, u16 size));
    extern void edr_att_profile_init(u16 psm, uint8_t const * db);
    att_event_handler_register(att_packet_handler);
    edr_att_profile_init(0, self_define_profile_data);
}

#endif

