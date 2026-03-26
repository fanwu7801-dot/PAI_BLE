#include "BLE_OTA.h"

#include <string.h>

#include "../../../../../cpu/periph_demo/dual_update_demo.h"

/* 复用现有串口 OTA 通道。
 * BLE 侧只做协议适配，不直接操作双备份升级底层。 */
extern int dual_ota_app_data_deal(u32 msg, u8 *buf, u32 len);

/* 复用现有 f7f1 加密回包链路，把 BLE OTA 结果回给 APP。 */
extern void fill_protocol_ble_reply_f7f1(uint16_t protocol_id, const uint8_t *payload, uint16_t payload_len);

/* 串口 OTA 固定协议号，BLE 适配层最终仍走这条底层通道。 */
#define BLE_OTA_SERIAL_PROTOCOL_ID   0x55AA

/* 0x2202 启动包固定为 8 字节：file_size(4B) + file_crc32(4B)。 */
#define BLE_OTA_START_PAYLOAD_LEN    8

/* dual_ota_app_data_deal() 期望的 START 包格式：
 * 55 AA + file_size(LE4) + file_crc(LE4) */
#define BLE_OTA_SERIAL_START_LEN     10

/* dual_ota_app_data_deal() 期望的 END 包格式：AA 55 */
#define BLE_OTA_SERIAL_END_LEN       2

/* 仅用于字节序自适应时做基本合理性判断，避免明显异常值。 */
#define BLE_OTA_MAX_FILE_SIZE        (32u * 1024u * 1024u)

/* BLE OTA 会话上下文。
 * 这里只维护协议适配层需要的状态，不保存底层写盘状态。 */
typedef struct
{
    uint8_t session_active;       /* 当前是否存在 BLE OTA 会话 */
    uint8_t start_pending;        /* 已下发 START，等待底层返回是否可升级 */
    uint8_t end_pending;          /* 已发送最后一个整包，等待底层通知继续后补 END */
    uint8_t awaiting_final_result;/* 已收到最后一包，等待最终成功/失败结果 */
    uint32_t file_size;           /* 目标固件总大小 */
    uint32_t file_crc32;          /* APP 下发的整包 CRC32 */
    uint32_t received_size;       /* 适配层累计收到的固件数据长度 */
    uint32_t cache_mod_len;       /* 对齐到底层大包缓存后的余数 */
} ble_ota_ctx_t;

static ble_ota_ctx_t s_ble_ota_ctx;

/* 按大端读取 32bit。
 * APP 协议文档若定义为网络字节序，则直接走这里。 */
static uint32_t ble_ota_get_u32_be(const uint8_t *buf)
{
    return ((uint32_t)buf[0] << 24) |
           ((uint32_t)buf[1] << 16) |
           ((uint32_t)buf[2] << 8) |
           (uint32_t)buf[3];
}

/* 按小端读取 32bit。
 * 串口 OTA 底层 START 包要求 little-endian。 */
static uint32_t ble_ota_get_u32_le(const uint8_t *buf)
{
    return ((uint32_t)buf[3] << 24) |
           ((uint32_t)buf[2] << 16) |
           ((uint32_t)buf[1] << 8) |
           (uint32_t)buf[0];
}

/* 以小端写入 32bit，供构造串口 OTA START 包使用。 */
static void ble_ota_put_u32_le(uint8_t *buf, uint32_t value)
{
    buf[0] = (uint8_t)(value & 0xFF);
    buf[1] = (uint8_t)((value >> 8) & 0xFF);
    buf[2] = (uint8_t)((value >> 16) & 0xFF);
    buf[3] = (uint8_t)((value >> 24) & 0xFF);
}

/* 对 32bit 参数做简单字节序自适应。
 * 原因：当前 APP 命令值固定，协议侧历史字节序不完全明确，
 * 这里优先选择“落在合理文件大小范围内”的值。 */
static uint32_t ble_ota_parse_u32_auto(const uint8_t *buf)
{
    uint32_t be_value = ble_ota_get_u32_be(buf);
    uint32_t le_value = ble_ota_get_u32_le(buf);
    uint8_t be_ok = (be_value > 0) && (be_value <= BLE_OTA_MAX_FILE_SIZE);
    uint8_t le_ok = (le_value > 0) && (le_value <= BLE_OTA_MAX_FILE_SIZE);

    if (be_ok && !le_ok) {
        return be_value;
    }
    if (!be_ok && le_ok) {
        return le_value;
    }
    return be_value;
}

/* 清理 BLE OTA 会话，并撤销对 dual_ota 的 BLE 通知接管。
 * 默认情况下 dual_ota 仍按原逻辑走 UART 回包。 */
static void ble_ota_clear_session(void)
{
    memset(&s_ble_ota_ctx, 0, sizeof(s_ble_ota_ctx));
    dual_ota_clear_notify_cb();
}

/* 回 0x2202 启动应答：
 * byte0 为可升级标记，后续可选错误原因字符串。 */
static void ble_ota_reply_start(uint8_t can_ota_state, const char *reason)
{
    uint8_t payload[1 + OTA_REASON_MAX_LEN] = {0};
    uint16_t payload_len = 1;

    payload[0] = can_ota_state;
    if (reason != NULL && reason[0] != '\0') {
        size_t reason_len = strnlen(reason, OTA_REASON_MAX_LEN - 1);
        memcpy(&payload[1], reason, reason_len);
        payload_len += (uint16_t)reason_len;
    }

    fill_protocol_ble_reply_f7f1(OTA_CMD_START, payload, payload_len);
}

/* 回 0x2203 数据应答：
 * byte0 为当前传输反馈，后续可选错误原因字符串。 */
static void ble_ota_reply_data(uint8_t feedback, const char *reason)
{
    uint8_t payload[1 + OTA_REASON_MAX_LEN] = {0};
    uint16_t payload_len = 1;

    payload[0] = feedback;
    if (reason != NULL && reason[0] != '\0') {
        size_t reason_len = strnlen(reason, OTA_REASON_MAX_LEN - 1);
        memcpy(&payload[1], reason, reason_len);
        payload_len += (uint16_t)reason_len;
    }

    fill_protocol_ble_reply_f7f1(OTA_CMD_TRANSDATA, payload, payload_len);
}

/* 给底层串口 OTA 适配层补 END 标记。
 * 当前 dual_ota_app_data_deal() 需要靠 AA 55 触发最终校验流程。 */
static int ble_ota_send_end_marker(void)
{
    uint8_t end_buf[BLE_OTA_SERIAL_END_LEN] = {0xAA, 0x55};
    return dual_ota_app_data_deal(BLE_OTA_SERIAL_PROTOCOL_ID, end_buf, sizeof(end_buf));
}

/* 接管 dual_ota 的通知回调。
 * 只有 BLE OTA 会话启动后才注册；未注册时仍保持 UART 原回包路径。 */
static int ble_ota_dual_notify_cb(u32 msg, u8 *buf, u32 len, void *priv)
{
    (void)buf;
    (void)len;
    (void)priv;

    /* START 阶段只关心“是否允许继续升级”。 */
    if (s_ble_ota_ctx.start_pending) {
        s_ble_ota_ctx.start_pending = 0;
        if (msg == DUAL_OTA_NOTIFY_CONTINUE) {
            ble_ota_reply_start(OTA_START_ALLOWED, "");
            return 0;
        }

        ble_ota_reply_start(OTA_START_NOT_ALLOWED, "ota start fail");
        ble_ota_clear_session();
        return 0;
    }

    /* 任意阶段收到 FAIL，都结束会话并回给 APP。 */
    if (msg == DUAL_OTA_NOTIFY_FAIL) {
        ble_ota_reply_data(OTA_TRANS_FAIL, "ota data fail");
        ble_ota_clear_session();
        return 0;
    }

    /* 如果最后一包刚好填满了底层缓存，底层会先回 CONTINUE。
     * 这里收到 CONTINUE 后，再主动补 END 包进入最终校验。 */
    if (s_ble_ota_ctx.end_pending && msg == DUAL_OTA_NOTIFY_CONTINUE) {
        int ret;

        s_ble_ota_ctx.end_pending = 0;
        ret = ble_ota_send_end_marker();
        if (ret != 0) {
            ble_ota_reply_data(OTA_TRANS_FAIL, "ota end fail");
            ble_ota_clear_session();
        }
        return 0;
    }

    /* 最终成功只在收完全部数据后才应答完成。 */
    if (s_ble_ota_ctx.awaiting_final_result && msg == DUAL_OTA_NOTIFY_SUCCESS) {
        ble_ota_reply_data(OTA_TRANS_FINISH, "");
        ble_ota_clear_session();
        return 0;
    }

    return 0;
}

/* 预留的命令处理函数。
 * 当前实际入口统一走 ble_ota_handle_cmd()，这里暂不承载逻辑。 */
void ota_start_function(void)
{
}

/* 预留的命令处理函数。
 * 当前实际入口统一走 ble_ota_handle_cmd()，这里暂不承载逻辑。 */
void OTA_transdata_funtion(void)
{
}

/* 处理 0x2202 启动 OTA。
 * 这里将 BLE payload 转换为串口 OTA START 包，再交给 dual_ota_app_data_deal()。 */
static int ble_ota_handle_start(const uint8_t *payload, uint16_t payload_len)
{
    uint8_t start_buf[BLE_OTA_SERIAL_START_LEN] = {0x55, 0xAA};
    int ret;

    if (payload == NULL || payload_len < BLE_OTA_START_PAYLOAD_LEN) {
        ble_ota_reply_start(OTA_START_NOT_ALLOWED, "invalid param");
        return -1;
    }

    if (s_ble_ota_ctx.session_active) {
        ble_ota_reply_start(OTA_START_NOT_ALLOWED, "ota busy");
        return -2;
    }

    memset(&s_ble_ota_ctx, 0, sizeof(s_ble_ota_ctx));
    s_ble_ota_ctx.session_active = 1;
    s_ble_ota_ctx.start_pending = 1;
    s_ble_ota_ctx.file_size = ble_ota_parse_u32_auto(payload);
    s_ble_ota_ctx.file_crc32 = ble_ota_parse_u32_auto(payload + 4);

    /* 注册 BLE 回调后，当前会话期间 dual_ota 的通知会优先回给 BLE。 */
    dual_ota_set_notify_cb(ble_ota_dual_notify_cb, NULL);

    ble_ota_put_u32_le(&start_buf[2], s_ble_ota_ctx.file_size);
    ble_ota_put_u32_le(&start_buf[6], s_ble_ota_ctx.file_crc32);

    ret = dual_ota_app_data_deal(BLE_OTA_SERIAL_PROTOCOL_ID, start_buf, sizeof(start_buf));
    if (ret != 0 && s_ble_ota_ctx.start_pending) {
        ble_ota_reply_start(OTA_START_NOT_ALLOWED, "ota start fail");
        ble_ota_clear_session();
        return ret;
    }

    return ret;
}

/* 处理 0x2203 数据包。
 * 数据体直接喂给 dual_ota_app_data_deal()，适配层负责做长度累计和 END 时机判断。 */
static int ble_ota_handle_data(const uint8_t *payload, uint16_t payload_len)
{
    uint32_t next_received_size;
    uint32_t next_cache_mod_len;
    uint8_t final_packet;
    int ret;

    if (payload == NULL || payload_len == 0 || payload_len > OTA_DATA_MAX_LEN) {
        ble_ota_reply_data(OTA_TRANS_FAIL, "invalid param");
        return -1;
    }

    if (!s_ble_ota_ctx.session_active || s_ble_ota_ctx.start_pending) {
        ble_ota_reply_data(OTA_TRANS_FAIL, "ota not ready");
        return -2;
    }

    if (s_ble_ota_ctx.awaiting_final_result || s_ble_ota_ctx.end_pending) {
        ble_ota_reply_data(OTA_TRANS_FAIL, "ota busy");
        return -3;
    }

    next_received_size = s_ble_ota_ctx.received_size + payload_len;
    if (next_received_size > s_ble_ota_ctx.file_size) {
        ble_ota_reply_data(OTA_TRANS_FAIL, "size overflow");
        ble_ota_clear_session();
        return -4;
    }

    /* 底层 dual OTA 会按固定大包长度缓存并异步写入。
     * 这里记录余数，用来判断最后一包是否还需要主动补 END。 */
    next_cache_mod_len = (s_ble_ota_ctx.cache_mod_len + payload_len) % DUAL_OTA_PACKET_LEN;
    final_packet = (next_received_size == s_ble_ota_ctx.file_size) ? 1 : 0;

    ret = dual_ota_app_data_deal(BLE_OTA_SERIAL_PROTOCOL_ID, (u8 *)payload, payload_len);
    if (ret != 0) {
        ble_ota_reply_data(OTA_TRANS_FAIL, "ota data fail");
        ble_ota_clear_session();
        return ret;
    }

    s_ble_ota_ctx.received_size = next_received_size;
    s_ble_ota_ctx.cache_mod_len = next_cache_mod_len;

    /* 非最后一包，维持“继续发下一包”的节奏。 */
    if (!final_packet) {
        ble_ota_reply_data(OTA_TRANS_CONTINUE, "");
        return 0;
    }

    /* 最后一包后不立即回 FINISH，必须等底层校验成功。 */
    s_ble_ota_ctx.awaiting_final_result = 1;

    /* 最后一包未填满底层缓存，dual_ota 不会自动进入最终校验，需要补 END。 */
    if (next_cache_mod_len != 0) {
        ret = ble_ota_send_end_marker();
        if (ret != 0) {
            ble_ota_reply_data(OTA_TRANS_FAIL, "ota end fail");
            ble_ota_clear_session();
            return ret;
        }
        return 0;
    }

    /* 最后一包恰好填满缓存时，等底层先回 CONTINUE，再在回调里补 END。 */
    s_ble_ota_ctx.end_pending = 1;
    return 0;
}

/* BLE OTA 命令统一入口。
 * 保持 APP 侧命令值不变，只在设备侧做适配。 */
int ble_ota_handle_cmd(uint16_t protocol_id, const uint8_t *payload, uint16_t payload_len)
{
    switch (protocol_id) {
    case OTA_CMD_START:
        return ble_ota_handle_start(payload, payload_len);

    case OTA_CMD_TRANSDATA:
        return ble_ota_handle_data(payload, payload_len);

    default:
        return -1;
    }
}

/* BLE 断链时清掉当前 OTA 会话，避免残留状态影响下次升级。 */
void ble_ota_on_disconnect(void)
{
    ble_ota_clear_session();
}

/* 预留的命令分发表。
 * 当前实际未直接按数组索引使用，先保留接口形态避免影响外部引用。 */
ota_cmd_handler_t cmd_dispatcher[OTA_CMD_DISPATCHER_NUM] = {
    ota_start_function,
    OTA_transdata_funtion,
};
