#include "system/includes.h"
/* #include "clock_manager/clock_manager.h" */
#include "multi_protocol_main.h"
#include "bt_tws.h"

#if (SMART_BOX_EN | GFPS_EN)

/* #if (BT_AI_SEL_PROTOCOL & (RCSP_MODE_EN | GFPS_EN)) */

#define ATT_LOCAL_PAYLOAD_SIZE    (517)//(517)              //note: need >= 20
#define ATT_SEND_CBUF_SIZE        (512*2)                   //note: need >= 20,缓存大小，可修改
#define ATT_RAM_BUFSIZE           (ATT_CTRL_BLOCK_SIZE + ATT_LOCAL_PAYLOAD_SIZE + ATT_SEND_CBUF_SIZE)                   //note:
static u8 att_ram_buffer[ATT_RAM_BUFSIZE] __attribute__((aligned(4)));

typedef struct {
    // linked list - assert: first field
    void *offset_item;
    // data is contained in same memory
    u32        service_record_handle;
    u8         *service_record;
} sdp_protocal_item_t;
#define SDP_RECORD_REGISTER(handler) \
	const sdp_protocal_item_t  handler sec(.sdp_record_item)


#if (GFPS_EN)

#include "gfps_platform_api.h"

u8 gfp_debug_enable = 0x3;

extern const u8 sdp_gfp_spp_service_data[];
SDP_RECORD_REGISTER(gfps_record_item) = {
    .service_record = (u8 *)sdp_gfp_spp_service_data,
    .service_record_handle = 0x00010039,
};

static const uint8_t google_model_id_used[3] = {0xF2, 0x63, 0x8D};
static const char google_public_key_used[64] = {
    0x5f, 0x17, 0xf0, 0x5a, 0x22, 0x03, 0xe2, 0xdc, 0x93, 0x1d, 0x5c, 0x00, 0xe8, 0xa4, 0x06, 0x7c,
    0x3c, 0x09, 0xa0, 0xad, 0x91, 0x82, 0x3e, 0x45, 0xda, 0xe7, 0xe5, 0x2f, 0x5e, 0xe9, 0x80, 0x1b,
    0xa6, 0xe2, 0x21, 0x9f, 0x65, 0x85, 0xa8, 0xab, 0x65, 0xf1, 0x47, 0x7e, 0x46, 0x7a, 0xdb, 0xb2,
    0x4b, 0x46, 0x72, 0xb8, 0x35, 0x89, 0x0b, 0x99, 0x57, 0xb7, 0x0e, 0xa9, 0x45, 0xe1, 0x7a, 0xbb
};
static const char google_private_key_used[32] = {
    0x86, 0x89, 0x57, 0x30, 0xFB, 0x95, 0x8A, 0x9E, 0x42, 0x57, 0x56, 0x97,
    0x39, 0x54, 0x77, 0xE7, 0x62, 0x44, 0x7A, 0xA1, 0x2B, 0xDF, 0x35, 0xFE,
    0xE9, 0xC5, 0x82, 0x28, 0x1B, 0xD7, 0xF5, 0xBE
};
#endif


bool check_tws_master_role()
{
#if TCFG_USER_TWS_ENABLE
    return (tws_api_get_role() == TWS_ROLE_MASTER);
#endif
    return 1;
}

void ble_profile_init(void)
{
    printf("[BLE_DIAG] ble_profile_init enter\n");
    /* clock_alloc("m_protocol", 12 * 1000000L); */
    le_device_db_init();
    /* app_ble_sm_init(IO_CAPABILITY_NO_INPUT_NO_OUTPUT, SM_AUTHREQ_MITM_PROTECTION | SM_AUTHREQ_BONDING, 7, 0); */
    /* app_ble_sm_init(IO_CAPABILITY_NO_INPUT_NO_OUTPUT, SM_AUTHREQ_BONDING, 7, 0); */
#if TCFG_BLE_BRIDGE_EDR_ENALBE
    ble_sm_setup_init(IO_CAPABILITY_NO_INPUT_NO_OUTPUT, SM_AUTHREQ_SECURE_CONNECTION | SM_AUTHREQ_MITM_PROTECTION | SM_AUTHREQ_BONDING, 7, 0);
#else
    ble_sm_setup_init(IO_CAPABILITY_NO_INPUT_NO_OUTPUT, SM_AUTHREQ_MITM_PROTECTION | SM_AUTHREQ_BONDING, 7, 0);
#endif
    app_ble_init();
    app_spp_init();

    multi_protocol_common_callback_init();

    printf("[BLE_DIAG] ble_profile_init after app init\n");

    ble_op_multi_att_send_init(att_ram_buffer, ATT_RAM_BUFSIZE, ATT_LOCAL_PAYLOAD_SIZE);

#if SMART_BOX_EN
    extern void le_smartbox_ble_profile_init(void);
    le_smartbox_ble_profile_init();
#endif

#if (GFPS_EN)
    gfps_ble_profile_init();
    gfps_is_tws_master_callback_register(check_tws_master_role);
    gfps_message_callback_register(gfps_message_deal_handler);
#endif
}

void bt_ble_init(void)
{
    printf("[BLE_DIAG] bt_ble_init enter\n");

#if SMART_BOX_EN
    extern void le_smartbox_bt_ble_init(void);
    le_smartbox_bt_ble_init();
#endif

    extern void ancs_ams_set_ios_pair_request_enable(u8 enable);
    ancs_ams_set_ios_pair_request_enable(0);

#if FINDMY_EN
    extern void fmy_bt_ble_init(void);
    fmy_bt_ble_init();
#endif /* #if FINDMY_EN */

#if (GFPS_EN)
    gfps_set_model_id((uint8_t *)google_model_id_used);
    gfps_set_anti_spoofing_public_key((char *)google_public_key_used);
    gfps_set_anti_spoofing_private_key((char *)google_private_key_used);
#if CONFIG_ANC_ENABLE
    gfps_hearable_controls_enable(1);
    gfps_hearable_controls_update(GFPS_ANC_ALL_MODE, GFPS_ANC_ALL_MODE, GFPS_OFF_MODE);
#endif
    gfps_all_init();
    extern void gfps_ctrl_pair_mode(u8 enable);
    gfps_ctrl_pair_mode(1);
#endif
    printf("[BLE_DIAG] bt_ble_init exit\n");

}

void bt_ble_exit(void)
{
#if SMART_BOX_EN
    extern void le_smartbox_bt_ble_exit(void);
    le_smartbox_bt_ble_exit();
#endif

#if FINDMY_EN
    extern void fmy_bt_ble_exit(void);
    fmy_bt_ble_exit();
#endif /* #if FINDMY_EN */

#if (GFPS_EN)
    gfps_all_exit();
#endif

    multi_protocol_common_callback_exit();

    app_ble_exit();
    app_spp_exit();
}

void bt_ble_adv_enable(u8 enable)
{
#if SMART_BOX_EN
    extern void le_smartbox_bt_ble_adv_enable(u8 enable);
    le_smartbox_bt_ble_adv_enable(enable);
#endif

}

#endif
