#ifndef __BLE_OTA_H__
#define __BLE_OTA_H__

#include <stdint.h>

#define OTA_REASON_MAX_LEN      256
#define OTA_DATA_MAX_LEN        256
#define OTA_CMD_DISPATCHER_NUM  2

typedef enum
{
    OTA_CMD_START     = 0x2202,
    OTA_CMD_TRANSDATA = 0x2203,
} ota_cmd_t;

typedef enum
{
    OTA_START_NOT_ALLOWED = 0,
    OTA_START_ALLOWED     = 1,
} ota_start_state_t;

typedef enum
{
    OTA_TRANS_CONTINUE = 1,
    OTA_TRANS_FINISH   = 2,
    OTA_TRANS_FAIL     = 3,
} ota_trans_feedback_t;

typedef struct
{
    uint32_t file_size;
    uint32_t file_crc32;
} ota_start_req_t;

typedef struct
{
    uint8_t can_ota_state;
    char des_ota_reason[OTA_REASON_MAX_LEN];
} ota_start_rsp_t;

typedef struct
{
    uint16_t data_len;
    uint8_t data[OTA_DATA_MAX_LEN];
} ota_transdata_req_t;

typedef struct
{
    uint8_t feedback;
    char error_reason[OTA_REASON_MAX_LEN];
} ota_transdata_rsp_t;

typedef void (*ota_cmd_handler_t)(void);

void ota_start_function(void);
void OTA_transdata_funtion(void);
int ble_ota_handle_cmd(uint16_t protocol_id, const uint8_t *payload, uint16_t payload_len);
void ble_ota_on_disconnect(void);

extern ota_cmd_handler_t cmd_dispatcher[OTA_CMD_DISPATCHER_NUM];

#endif
