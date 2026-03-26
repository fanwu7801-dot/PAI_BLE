#ifndef __DUAL_UPDATE_DEMO_H
#define __DUAL_UPDATE_DEMO_H

#include "system/includes.h"

enum {
    DUAL_OTA_NOTIFY_SUCCESS = 0,
    DUAL_OTA_NOTIFY_FAIL,
    DUAL_OTA_NOTIFY_CONTINUE,
    DUAL_OTA_NOTIFY_EXIT,
    DUAL_OTA_NOTIFY_UNINIT,
};

typedef enum {
    DUAL_OTA_STEP_START = 0,
    DUAL_OTA_STEP_WRITE,
    DUAL_OTA_STEP_VERIFY,
} DUAL_OTA_STEP;

typedef struct {
    u32 file_size;
    u32 file_crc;
    u32 max_pkt_len;
} dual_ota_info;

#define DUAL_OTA_PACKET_LEN   (512 * 4)

typedef int (*dual_ota_notify_cb_t)(u32 msg, u8 *buf, u32 len, void *priv);

extern int dual_ota_update_deal(u8 step, u8 *buf, u32 len);
extern int dual_ota_receive_bin_data(u8 *buf, u32 len);
extern void dual_ota_no_rsp_fail_exit(void);
extern void dual_ota_set_notify_cb(dual_ota_notify_cb_t cb, void *priv);
extern void dual_ota_clear_notify_cb(void);

#endif
