#ifndef _BROADCAST_UPGRADE_API_
#define _BROADCAST_UPGRADE_API_

#include "typedef.h"
#include "fs.h"

#define B_SLAVE_OTA_OTHER_UPGRADE_INFO	(1<<8)
// 该枚举值要和loader相对应
enum {
    B_SLAVE_OTA_VERIFY_CRC_INFO = B_SLAVE_OTA_OTHER_UPGRADE_INFO,
    B_SLAVE_OTA_FW_FILE_VERIFY_CODE,
    B_SLAVE_OTA_OTP_CFG_DATA,
    B_SLAVE_OTA_OTP_BT_NAME,
    B_SLAVE_OTA_OTP_BT_ADDR,
    B_SLAVE_OTA_OTP_DUAL_PUSH_LOADER,
};

enum {
    BC_UPDATEING_NONE_ERR = 0,
    BC_UPDATEING_ERR = -1,
    BC_UPDATEING_VERIFY_FAIL = -2,
    BC_UPDATEING_WRITE_FAIL = -3,
    BC_UPDATEING_LOADER_PARM_ERR = -4,
    BC_UPDATEING_ALLOC_ERR = -5,
    BC_UPDATEING_TIMEOUT = -6,
};

enum {
    BC_UPDATE_SINGLE_BANK_NORMAL,
    BC_UPDATE_DUAL_BANK_TO_DUAL_BANK_IN_SDK,
    BC_UPDATE_SINGLE_BANK_TO_DUAL_BANK_PUSH_LOADER,
    BC_UPDATE_DUAL_BANK_TO_SINGLE_BANK_PUSH_LOADER,
    BC_UPDATE_DUAL_BANK_TO_DUAL_BANK_PUSH_LOADER,
};

enum {
    BC_UPDATE_LOADER_INFO,
    BC_UPDATE_EXIF_INFO,
};

typedef struct broadcast_upgrade_loader_exif_info_t {
    u32 src_addr;
    u32 des_addr;
    u32 len;
    u16 crc;
    union {
        struct {
            u8 Reserved[2];
        };
        struct {
            u16 dual_bank_dec_crc;
        };
    };
} loader_exif_info;

typedef struct broadcast_upgrade_loader_info_t {
    u32 remote_addr;
    u32 local_addr;
    u32 data_len;
    u16 data_crc;
    u8 dual_bank_opt;
    loader_exif_info target_info;
    loader_exif_info exif_info;
} broadcast_upgrade_loader_info;

typedef int (*write_func)(void *buf, u32 offset, u32 len);

int broadcast_upgrade_verify_data_analys(u8 *data, u8 data_len);
int broadcast_upgrade_target_info(int type, broadcast_upgrade_loader_info *loader_info, void *priv);
int broadcast_upgrade_release(void);
int broadcast_upgrade_param_fill(u8 *fill_buf, u32 area_size);
int broadcast_upgrade_write_data(u8 *data, u32 data_len, void *priv);
int broadcast_upgrade_exif_check(u32 *exif_addr, u32 *exif_len);
u16 b_update_cfg_area_data_fill_with_vm_format(u8 *output, u8 *param_data, u16 data_len, u16 param_id);
int broadcast_upgrade_dual_bank_result_dual(void *priv, int type, int (*burn_boot_info_result_hdl)(void *priv, int type, int err));
int broadcast_upgrade_dual_bank_loader_info_fill(u8 *fill_buf, u32 buf_len, u32 data_offset);

#endif
