#include "smartbox/config.h"
#include "smartbox/function.h"
#include "smartbox/smartbox.h"
#include "smartbox/event.h"
#include "smartbox/func_cmd_common.h"

#include "le_smartbox_module.h"
#include "smartbox_setting_opt.h"
#include "smartbox_common_info.h"
#include "smartbox_update_cat1_module.h"
#include "dev_manager.h"
#include "btstack/avctp_user.h"

#include "app_main.h"
#include "app_action.h"
#include "app_common.h"
#include "task_manager/cat1/cat1_common.h"


#if (defined(TCFG_CAT1_MODULE_UPDATE_ENABLE) && (TCFG_CAT1_MODULE_UPDATE_ENABLE == 1))

#define LOG_TAG_CONST       SMART_CAT1
#define LOG_TAG             "[SMART-CAT1]"
#define LOG_INFO_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_ERROR_ENABLE
#define LOG_DUMP_ENABLE

#define LOG_ASSERT_ENABLE
#include "debug.h"

enum : u8 {
    // op
    UPDATE_CAT1_OP_READ = 0,
    UPDATE_CAT1_OP_START,
    UPDATE_CAT1_OP_NOTIFY,
};

enum : u8 {
    // type
    UPDATE_CAT1_TYPE_BASIC_INFO = 0,
    UPDATE_CAT1_TYPE_UPDATE_RESULT,
};

u8 smartbox_cat1_info_set_cmd_deal(void *priv, u8 OpCode, u8 OpCode_SN, u8 *data, u16 len)
{
    log_debug("len %d, %d\n", len, sizeof(smartbox_common_info_set_cmd_t));
    log_info_hexdump(data, len);

    u32 wlen = 0;
    u8 ret = JL_PRO_STATUS_SUCCESS;
    u8 version = data[0];

    if (sizeof(smartbox_common_info_set_cmd_t) > len) {
        ret = JL_PRO_STATUS_UNKOWN_CMD;
        goto _cmd_err;
    }

    smartbox_common_info_set_cmd_t *common_set_cmd = (smartbox_common_info_set_cmd_t *)(&data[1]);

    log_debug("function 0x%x\n", common_set_cmd->function);
    log_debug("version    %d\n", common_set_cmd->version);
    log_debug("op         %d\n", common_set_cmd->op);

    u8 *resp = zalloc(TARGET_FEATURE_RESP_BUF_SIZE);
    if (resp == NULL) {
        ret = JL_PRO_STATUS_FAIL;
        goto _cmd_err;
    }
    resp[wlen++] = ret;  // 初始返回result

    if (READ_BIG_U16(&common_set_cmd->function) == SMARTBOX_COMMON_INFO_CAT1_MODULE) { // 获取4G模块信息
        switch (common_set_cmd->op) {
        case UPDATE_CAT1_OP_READ:  // 读取
            u16 ret_len = 0;
            log_info("read info\n");
            resp[wlen++] = version;
            memcpy(&resp[wlen], (u8 *)common_set_cmd, sizeof(smartbox_common_info_set_cmd_t));
            wlen += sizeof(smartbox_common_info_set_cmd_t);
            switch (common_set_cmd->data[0]) {
            case UPDATE_CAT1_TYPE_BASIC_INFO:
                log_info("cat1 module basic info\n");
                resp[wlen++] = UPDATE_CAT1_TYPE_BASIC_INFO;  // type, 4G模块基本信息
                ret_len = cat1_module_update_basic_info_get(&resp[wlen], TARGET_FEATURE_RESP_BUF_SIZE - wlen);
                if (ret_len == 0) {
                    ret = JL_PRO_STATUS_FAIL;
                }
                wlen += ret_len;
                break;
            case UPDATE_CAT1_TYPE_UPDATE_RESULT:
                log_info("cat1 module update result\n");
                resp[wlen++] = UPDATE_CAT1_TYPE_UPDATE_RESULT;  // type, 升级状态信息
                ret_len = cat1_module_update_get_update_result_info(&resp[wlen], TARGET_FEATURE_RESP_BUF_SIZE - wlen);
                if (ret_len == 0) {
                    ret = JL_PRO_STATUS_FAIL;
                }
                wlen += ret_len;
                break;
            default:
                log_info("undefined type\n");
                ret = JL_PRO_STATUS_UNKOWN_CMD;
                break;
            }
            break;
        case UPDATE_CAT1_OP_START:  // 开始4G模块OTA
            log_info("update start\n");
            // 直接回复就行, 不需要携带payload数据
            if (cat1_module_update_start(READ_BIG_U32(common_set_cmd->data))) {
                ret = JL_PRO_STATUS_FAIL;
            }
            memcpy(resp, data, len);
            wlen = len;
            break;
        case UPDATE_CAT1_OP_NOTIFY:  // 通知
            log_info("update notify\n");
            break;
        default:
            log_info("undefined cmd\n");
            ret = JL_PRO_STATUS_UNKOWN_CMD;
            break;
        }
    } else {
        log_info("undefined function\n");
        ret = JL_PRO_STATUS_UNKOWN_CMD;
    }

_cmd_err:
    resp[0] = ret;
    log_debug("response len %d, ret %d\n", wlen, ret);
    log_info_hexdump(resp, wlen);
    JL_CMD_response_send(OpCode, ret, OpCode_SN, resp, wlen);
    free(resp);

    return ret;
}

u8 smartbox_command_set_cmd_notify_cat1_update_result(u8 state, u8 result)
{
    log_info("%s\n", __func__);
    u8 wlen = 0;
    u8 send_buf[10];
    send_buf[wlen++] = 0; // version
    WRITE_BIG_U16(&send_buf[wlen], SMARTBOX_COMMON_INFO_CAT1_MODULE);  // function
    wlen += 2;
    send_buf[wlen++] = 0x00; // version
    send_buf[wlen++] = UPDATE_CAT1_OP_NOTIFY; // op
    send_buf[wlen++] = 0x01;  // type: 升级状态
    send_buf[wlen++] = state; // state
    if (state == 0x00) {
        send_buf[wlen++] = result;
    }

    return JL_CMD_send(JL_OPCODE_COMMON_INFO_SET, send_buf, wlen, JL_NEED_RESPOND);
}

u8 smartbox_command_set_cmd_notify_cat1_update_basic_info(void)
{
    log_info("%s\n", __func__);
    u8 wlen = 0;
    int ret = 0;
    u8 *send_buf = (u8 *)zalloc(TARGET_FEATURE_RESP_BUF_SIZE);
    send_buf[wlen++] = 0; // version
    WRITE_BIG_U16(&send_buf[wlen], SMARTBOX_COMMON_INFO_CAT1_MODULE);  // function
    wlen += 2;
    send_buf[wlen++] = 0x00; // version
    send_buf[wlen++] = UPDATE_CAT1_OP_NOTIFY; // op
    send_buf[wlen++] = 0x00;  // type: 4G模块信息

    ret = cat1_module_update_basic_info_get(&send_buf[wlen], TARGET_FEATURE_RESP_BUF_SIZE - wlen);
    if (ret == 0) {
        log_error("send cat1 update basic info err\n");
        free(send_buf);
        ret = 1;
        return ret;
    }
    wlen += ret;

    log_info_hexdump(send_buf, wlen);
    ret = JL_CMD_send(JL_OPCODE_COMMON_INFO_SET, send_buf, wlen, JL_NEED_RESPOND);
    free(send_buf);
    return ret;
}
#endif
