#ifndef MODEM_CTRL_API_H_
#define MODEM_CTRL_API_H_
#include "uos_type.h"

#define dele_task_event_flag    0XFF

typedef enum {
    MSG_MODEM_RESET        = 0,
    MSG_MODEM_ALIVE        = 1,
    MSG_MODEM_BLOCK        = 2,
    MSG_MODEM_ASSERT       = 3,
    MSG_MODEM_POWERON      = 4,
    MSG_MODEM_POWEROFF     = 5,
    MSG_MAX_NUM
} MODEM_CTRL_MSG_E;

typedef enum {
    UOS_PIN_AF_NONE,
    UOS_PIN_AF_GPIO,
    UOS_PIN_AF_UART,
} uos_pin_af_e;

typedef enum {
    UNISOC_MODEM_BOOT_NORMAL = 0,
    UNISOC_MODEM_BOOT_CAIL,
    UNISOC_MODEM_BOOT_MDLOAD, // modem enter mcu download mode
    UNISOC_MODEM_BOOT_UDLOAD, // modem enter usb download mode
    UNISOC_MODEM_BOOT_MAX,

    UNISOC_MODEM_OK,
    UNISOC_MODEM_VERIFY_ERROR,
    UNISOC_MODEM_UNRECOGNIZE,
    UNISOC_MCU_VERIFY_ERROR,
} unisoc_boot_e; /* modem boot cfg(add by tao.shi) */

typedef enum {
    UNI_RM_NORMAL_E          = 0,
    UNI_RM_DOWNLOAD_E        = 1,
    UNI_RM_CALIB_E           = 2,
    UNI_RM_FOTA_UPDATE_E     = 3,
    UNI_RM_MD_READY_OK_E     = 20,
    UNI_RM_MD_RSP_OK_E,
    UNI_RM_MD_RSP_ERR_E,
    UNI_RM_MD_BOOT_WAIT_E,
    UNI_RM_MAX_E,
} MODEM_CTRL_RM_E;

typedef enum {
    MC_CLIENT_INVAIL       = 0,
    MC_CLIENT_ENGPC        = 1,
    MC_CLIENT_MMI          = 2,
    MC_CLIENT_MDMPOWER     = 3,
    MC_CLIENT_RILD         = 4,
    MC_CLIENT_MAX
} MC_CLIENT_ID_E ;

typedef enum {
    MODEM_STATE_OFFLINE           = 0,
    MODEM_STATE_ALIVE             = 1,
    MODEM_STATE_ASSERT            = 2,
    MODEM_STATE_RESET             = 3,
    MODEM_STATE_BLOCK             = 4,
    MODEM_STATE_POWERON           = 5,
    MODEM_STATE_POWEROFF          = 6,
    MODEM_STATE_POWERONPROGRESS   = 7,
    MODEM_STATE_POWEROFFPROGRESS  = 8,
    MODEM_STATE_CHECK_ERR         = 9
} ModemState;

typedef uos_uint32_t (*CLIENT_FUNC_T)(uos_uint32_t modem_status);

typedef struct {
    MC_CLIENT_ID_E client_id;
    CLIENT_FUNC_T func;
} CLIENT_HANDLE_T;

void mdctrl_enable_modem_assert(void);
void send_msg_to_modem_ctrl(MODEM_CTRL_MSG_E msg);
uos_uint32_t modem_ctl_client_reg_handler(MC_CLIENT_ID_E client_id, CLIENT_FUNC_T func);
uos_uint32_t SendMsgPowerSyn(uos_uint32_t mode);
void ForcedMcuPowerOFF(void);
ModemState modem_state_check(void);

void ModemTaskInit(void);
#endif //MODEM_CTRL_API_H_
