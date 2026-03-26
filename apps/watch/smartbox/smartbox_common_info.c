#include "smartbox/config.h"
#include "smartbox/smartbox_device_config.h"
#include "btstack/avctp_user.h"
#include "smartbox/event.h"
#include "custom_cfg.h"
#include "JL_rcsp_packet.h"
#include "smartbox_common_info.h"
#include "smartbox_update_cat1_module.h"
#include "ui/ui_api.h"

#if (SMART_BOX_EN)

#define LOG_TAG_CONST       APP
#define LOG_TAG     		"[SMARBOX-COMMON_INFO]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
#define LOG_DUMP_ENABLE
#define LOG_CHAR_ENABLE
#include "debug.h"

#define WATCH_DIAL_EXT_INFO_VER				0 // 表盘扩展参数版本号

enum : u8 {
    // op
    WATCH_DIAL_EXT_OP_READ = 0,
};

enum : u8 {
    // shape
    WATCH_DIAL_EXT_SHAPE_ROUND = 1,			// 圆形
    WATCH_DIAL_EXT_SHAPE_RECTANGLE,			// 矩形
    WATCH_DIAL_EXT_SHAPE_ROUND_RECTANGLE,	// 圆角矩形
};

struct cur_device_info {
    u8 device_shape;
    u16 device_radius;
    u32 screen_fill_argb;
} __attribute__((packed));


static int smartbox_watch_dial_ext_info(u8 *data, u16 *offset, u16 buf_len)
{
    struct cur_device_info device_info;
    u16 screen_width = 0;
    u16 screen_height = 0;
    u8 shape = WATCH_DIAL_EXT_SHAPE_RECTANGLE;
    u16 radius = 0;
    u32 screen_fill_argb = 0;
    u16 data_len = *offset;

    if (data_len + sizeof(shape) + sizeof(radius) + sizeof(screen_fill_argb) > buf_len) {
        return JL_PRO_STATUS_FAIL;
    }

    // get_cur_srreen_width_and_height(&screen_width, &screen_height);
    // get_cur_srreen_radius_and_fill_argb(&radius, &screen_fill_argb);

    if (radius) {
        u16 limit_radius = MIN(screen_width, screen_height) / 2;
        if (radius > limit_radius) {
            log_error("radius error: %d, %d, %d \n", radius, screen_width, screen_height);
        } else {
            if ((screen_width == screen_height) && (radius == limit_radius)) {
                shape = WATCH_DIAL_EXT_SHAPE_ROUND;
                log_debug("WATCH_DIAL_EXT_SHAPE_ROUND\n");
            } else {
                shape = WATCH_DIAL_EXT_SHAPE_ROUND_RECTANGLE;
                log_debug("WATCH_DIAL_EXT_SHAPE_RECTANGLE\n");
            }
        }
    }
    WRITE_BIG_U16(&device_info.device_shape, shape);
    WRITE_BIG_U16(&device_info.device_radius, radius);
    WRITE_BIG_U16(&device_info.screen_fill_argb, screen_fill_argb);


    memcpy(data, &device_info, sizeof(struct cur_device_info));
    data_len += sizeof(struct cur_device_info);
    log_debug("data_len:%d\n", data_len);
    *offset = data_len;

    return JL_PRO_STATUS_SUCCESS;
}

u8 smartbox_common_info_set_cmd_deal(void *priv, u8 OpCode, u8 OpCode_SN, u8 *data, u16 len)
{
    u16 wlen = 0;
    u8 ret = JL_PRO_STATUS_SUCCESS;
    u8 version = data[0];

    u8 *resp = zalloc(TARGET_FEATURE_RESP_BUF_SIZE);
    if (resp == NULL) {
        ret = JL_PRO_STATUS_FAIL;
        goto _cmd_err;
    }
    resp[wlen++] = ret;  // 初始返回result

    if (len < sizeof(smartbox_common_info_set_cmd_t)) {
        ret = JL_PRO_STATUS_UNKOWN_CMD;
        goto _cmd_err;
    }
    smartbox_common_info_set_cmd_t common_info;
    memcpy(&common_info, &data[1], sizeof(smartbox_common_info_set_cmd_t));

    log_debug("function 0x%x\n", common_info.function);
    log_debug("version    %d\n", common_info.version);
    log_debug("op         %d\n", common_info.op);

    switch (READ_BIG_U16(&common_info.function)) {
#if TCFG_CAT1_MODULE_UPDATE_ENABLE
    case SMARTBOX_COMMON_INFO_CAT1_MODULE:
        free(resp);
        return smartbox_cat1_info_set_cmd_deal(priv, OpCode, OpCode_SN, data, len);
#endif
    case SMARTBOX_COMMON_INFO_WATCH_DIAL_EXT: {
        if (common_info.version != WATCH_DIAL_EXT_INFO_VER) {
            ret = JL_PRO_STATUS_PARAM_ERR;
            break;
        }
        if (common_info.op != WATCH_DIAL_EXT_OP_READ) {
            ret = JL_PRO_STATUS_PARAM_ERR;
            break;
        }
        resp[wlen++] = version;
        memcpy(&resp[wlen], (u8 *)&common_info, sizeof(smartbox_common_info_set_cmd_t));
        wlen += sizeof(smartbox_common_info_set_cmd_t);
        put_buf(resp, wlen);
        ret = smartbox_watch_dial_ext_info(&resp[wlen], &wlen, TARGET_FEATURE_RESP_BUF_SIZE - wlen);
        put_buf(resp, wlen);
    }
    break;
    default:
        ret = JL_PRO_STATUS_UNKOWN_CMD;
        break;
    }

_cmd_err:
    if (resp) {
        resp[0] = ret;
    }
    log_debug("response len %d, ret %d\n", wlen, ret);
    log_info_hexdump(resp, wlen);
    JL_CMD_response_send(OpCode, ret, OpCode_SN, resp, wlen);
    if (resp) {
        free(resp);
    }
    return ret;
}

#endif//SMART_BOX_EN


