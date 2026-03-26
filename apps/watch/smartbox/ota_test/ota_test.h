#ifndef __SMARTBOX_OTA_TEST_H__
#define __SMARTBOX_OTA_TEST_H__

#include <stdint.h>

#ifndef TCFG_OTA_TEST_BRIDGE_ENABLE
#define TCFG_OTA_TEST_BRIDGE_ENABLE 0
#endif

typedef enum {
    OTA_TEST_IDLE = 0,
    OTA_TEST_READY,
    OTA_TEST_DOWNLOADING,
    OTA_TEST_VERIFYING,
    OTA_TEST_DONE,
    OTA_TEST_FAILED,
} ota_test_state_e;

typedef enum {
    OTA_TEST_ERR_NONE = 0,
    OTA_TEST_ERR_STATE = 1,
    OTA_TEST_ERR_PARAM = 2,
    OTA_TEST_ERR_TIMEOUT = 3,
    OTA_TEST_ERR_CRC = 4,
    OTA_TEST_ERR_FLASH = 5,
    OTA_TEST_ERR_LOW_BAT = 6,
} ota_test_error_e;

typedef struct {
    uint8_t state;
    uint8_t last_error;
    uint32_t received_size;
    uint32_t total_size;
} ota_test_status_t;

int ota_test_init(void);
int ota_test_start_rcsp(uint8_t channel_type);
int ota_test_abort(uint8_t reason);
int ota_test_get_status(ota_test_status_t *out);
void ota_test_on_disconnect(void);
void ota_test_on_rcsp_result(uint8_t ok, uint8_t err_code);
void ota_test_set_total_size(uint32_t total_size);

#endif
