#include "app_config.h"
#include "ota_test.h"

#if TCFG_OTA_TEST_BRIDGE_ENABLE && RCSP_UPDATE_EN
#include "smartbox_update/smartbox_update.h"
#include "bt_manage/smartbox_bt_manage.h"
#include "btstack_3th_protocol_user.h"
#endif

#include <string.h>

/*
 * OTA 桥接层上下文（仅本文件可见）
 *
 * 设计意图：
 * 1. 将 OTA 会话状态集中在一个地方管理，避免跨模块随意改状态。
 * 2. 对上层（22Ax）暴露统一状态快照，对下层（RCSP）只做桥接调用。
 *
 * 字段说明：
 * - status:
 *   对外可查询的状态信息，包含 state/last_error/received_size/total_size。
 * - start_time_ms:
 *   会话起始时间戳预留字段，当前版本未使用。
 * - channel_type:
 *   当前升级通道类型（例如 BLE/SPP）。
 * - init_done:
 *   惰性初始化标记。保证调用方不依赖初始化顺序。
 */
typedef struct {
    ota_test_status_t status;
    uint32_t start_time_ms;
    uint8_t channel_type;
    uint8_t init_done;
} ota_test_context_t;

static ota_test_context_t s_ota_test_ctx;

/*
 * 会话基础复位函数。
 *
 * 注意：
 * - 只复位 state/last_error/received_size/start_time_ms。
 * - 不主动清 total_size，是否清理由调用方决定。
 */
static void ota_test_reset_locked(uint8_t state, uint8_t err)
{
    s_ota_test_ctx.status.state = state;
    s_ota_test_ctx.status.last_error = err;
    s_ota_test_ctx.status.received_size = 0;
    s_ota_test_ctx.start_time_ms = 0;
}

int ota_test_init(void)
{
    /*
     * 初始化为可观测的空闲态：
     * - state = IDLE
     * - last_error = NONE
     * - received/total 由 memset 清零
     */
    memset(&s_ota_test_ctx, 0, sizeof(s_ota_test_ctx));
    s_ota_test_ctx.status.state = OTA_TEST_IDLE;
    s_ota_test_ctx.status.last_error = OTA_TEST_ERR_NONE;
#if TCFG_OTA_TEST_BRIDGE_ENABLE && RCSP_UPDATE_EN
    /* 默认优先 BLE，后续 start 时可覆盖为其它通道。 */
    s_ota_test_ctx.channel_type = RCSP_BLE;
#else
    s_ota_test_ctx.channel_type = 0;
#endif
    s_ota_test_ctx.init_done = 1;
    return 0;
}

void ota_test_set_total_size(uint32_t total_size)
{
    /*
     * 由上层在 ota_enter 时设置总大小。
     * 主要用于：
     * 1. 上报状态给手机侧展示进度。
     * 2. 成功时将 received_size 对齐到 total_size（100%）。
     */
    if (!s_ota_test_ctx.init_done) {
        ota_test_init();
    }
    s_ota_test_ctx.status.total_size = total_size;
}

int ota_test_start_rcsp(uint8_t channel_type)
{
#if TCFG_OTA_TEST_BRIDGE_ENABLE && RCSP_UPDATE_EN
    int ret;

    /* 惰性初始化，避免调用方漏调 init。 */
    if (!s_ota_test_ctx.init_done) {
        ota_test_init();
    }

    /*
     * 会话重入保护：
     * - DOWNLOADING/VERIFYING 期间拒绝再次 start。
     * - 避免并发 OTA 会话破坏状态机一致性。
     */
    if (s_ota_test_ctx.status.state == OTA_TEST_DOWNLOADING ||
        s_ota_test_ctx.status.state == OTA_TEST_VERIFYING) {
        s_ota_test_ctx.status.last_error = OTA_TEST_ERR_STATE;
        return -2;
    }

    /* 先进入 READY，再桥接到 RCSP 主升级链路。 */
    s_ota_test_ctx.channel_type = channel_type;
    s_ota_test_ctx.status.state = OTA_TEST_READY;
    s_ota_test_ctx.status.last_error = OTA_TEST_ERR_NONE;
    s_ota_test_ctx.status.received_size = 0;

    /*
     * 真实升级入口：
     * smartbox_update 内部会触发 MSG_JL_LOADER_DOWNLOAD_START 等流程。
     */
    ret = smartbox_rcsp_ota_bridge_start(channel_type, s_ota_test_ctx.status.total_size);
    if (ret) {
        /* 启动失败立即落 FAILED，错误统一映射为 FLASH。 */
        s_ota_test_ctx.status.state = OTA_TEST_FAILED;
        s_ota_test_ctx.status.last_error = OTA_TEST_ERR_FLASH;
        return ret;
    }

    /* 底层接受任务后，状态切到下载中。 */
    s_ota_test_ctx.status.state = OTA_TEST_DOWNLOADING;
    return 0;
#else
    /* 编译期关闭桥接能力时，明确返回不支持。 */
    (void)channel_type;
    return -1;
#endif
}

int ota_test_abort(uint8_t reason)
{
    /*
     * 主动退出会话：
     * - 状态回到 IDLE
     * - 清空 total_size，避免影响下一次会话
     */
    if (!s_ota_test_ctx.init_done) {
        ota_test_init();
    }
    ota_test_reset_locked(OTA_TEST_IDLE, reason);
    s_ota_test_ctx.status.total_size = 0;
    return 0;
}

int ota_test_get_status(ota_test_status_t *out)
{
    if (!out) {
        return -1;
    }
    /*
     * 对外只读快照接口：
     * - 调用方获取一份拷贝
     * - 不允许直接修改内部全局状态
     */
    if (!s_ota_test_ctx.init_done) {
        ota_test_init();
    }
    *out = s_ota_test_ctx.status;
    return 0;
}

void ota_test_on_disconnect(void)
{
    /*
     * 断链处理策略：
     * - 仅在活跃会话阶段（READY/DOWNLOADING/VERIFYING）转 FAILED。
     * - 空闲态断链不改状态，避免误报。
     */
    if (!s_ota_test_ctx.init_done) {
        ota_test_init();
    }
    if (s_ota_test_ctx.status.state == OTA_TEST_READY ||
        s_ota_test_ctx.status.state == OTA_TEST_DOWNLOADING ||
        s_ota_test_ctx.status.state == OTA_TEST_VERIFYING) {
        s_ota_test_ctx.status.state = OTA_TEST_FAILED;
        s_ota_test_ctx.status.last_error = OTA_TEST_ERR_TIMEOUT;
    }
}

void ota_test_on_rcsp_result(uint8_t ok, uint8_t err_code)
{
    /*
     * RCSP 结果汇聚点：
     * - 统一将底层结果收敛到 DONE/FAILED 两个终态。
     */
    if (!s_ota_test_ctx.init_done) {
        ota_test_init();
    }

    /* 先进入结果处理阶段，再根据 ok 进入终态。 */
    s_ota_test_ctx.status.state = OTA_TEST_VERIFYING;
    if (ok) {
        s_ota_test_ctx.status.state = OTA_TEST_DONE;
        s_ota_test_ctx.status.last_error = OTA_TEST_ERR_NONE;
        /* 成功时如果 total_size 有效，则进度对齐到 100%。 */
        if (s_ota_test_ctx.status.total_size) {
            s_ota_test_ctx.status.received_size = s_ota_test_ctx.status.total_size;
        }
    } else {
        s_ota_test_ctx.status.state = OTA_TEST_FAILED;
        /* 底层未给错误码时，回落为通用 FLASH 错误。 */
        s_ota_test_ctx.status.last_error = err_code ? err_code : OTA_TEST_ERR_FLASH;
    }
}
