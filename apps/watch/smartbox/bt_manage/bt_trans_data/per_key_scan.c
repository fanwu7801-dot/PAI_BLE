#include "per_key_scan.h"
#include "system/includes.h"
#include "btstack/le/le_user.h"
#include "le_common.h"
#include "le_smartbox_multi_common.h"
#include "usart0_to_mcu.h"

// 用于判断是否已建立 BLE 连接（连接态下扫描会被调度/限流）
extern hci_con_handle_t smartbox_get_con_handle(void);

/* 确保 BLE client 模块处于激活状态，否则可能打开了 scan 但收不到 ADV report。 */
extern void ble_client_module_enable(u8 en);

// 从 flash(syscfg) 实时刷新目标列表（在 fill_protocol.c 内实现）
extern void seamless_unlock_load_targets_from_syscfg(void);

/* 本文件独立编译：避免依赖 debug.h / 其他模块的 log_info 宏 */

/* 扫描参数：跟工程内 smartbox multi client 对齐 */
#ifndef SET_SCAN_TYPE
#define SET_SCAN_TYPE       SCAN_PASSIVE // 改为被动扫描，提高发现速度，减少交互
#endif
#ifndef SET_SCAN_INTERVAL
#define SET_SCAN_INTERVAL   ADV_SCAN_MS(10) // unit: ms
#endif
#ifndef SET_SCAN_WINDOW
#define SET_SCAN_WINDOW     ADV_SCAN_MS(10) // unit: ms
#endif

#define MAX_MAC_COUNT 10

// ========== RSSI 发送到 MCU（仅测试）==========
// - 仅对 per_key_scan_set_targets() 配置的目标 MAC 生效
// - RSSI 做 3 点平均 + EMA（与 rssi_check.c 思路一致）
// - 通过 uart1_send_toMCU(0x00F3) 上报：payload[0]=rssi+128, payload[1..6]=MAC(与日志顺序一致)
#ifndef PER_KEY_SCAN_RSSI_TO_MCU_TEST_EN
#define PER_KEY_SCAN_RSSI_TO_MCU_TEST_EN (1)
#endif
#ifndef PER_KEY_SCAN_RSSI_MCU_PROTOCOL_ID
#define PER_KEY_SCAN_RSSI_MCU_PROTOCOL_ID (0x00F3)
#endif
#ifndef PER_KEY_SCAN_RSSI_MCU_MIN_INTERVAL_MS
// 需求：扫描到就立刻发给 MCU 让 MCU 判断，因此默认不做最小间隔限制。
// 如现场发现 MCU/串口压力过大，可把该值调大（例如 50/100/200）。
#define PER_KEY_SCAN_RSSI_MCU_MIN_INTERVAL_MS (100)
#endif

#ifndef PER_KEY_SCAN_MCU_SENT_LOG_THROTTLE_MS
#define PER_KEY_SCAN_MCU_SENT_LOG_THROTTLE_MS (1000)
#endif

typedef struct {
    s8 s3_buf[3];
    u8 s3_cnt;
    u8 s3_idx;
    s16 ema;
    u8 ema_inited;
    u32 last_send_ms;
} per_key_rssi_filt_t;

// 周围设备打印相关：每 50ms 触发一次扫描窗口（加快扫描速度）
#ifndef PER_KEY_NEARBY_SCAN_PERIOD_MS
#define PER_KEY_NEARBY_SCAN_PERIOD_MS   (50)
#endif
#ifndef PER_KEY_NEARBY_SCAN_WINDOW_MS
#define PER_KEY_NEARBY_SCAN_WINDOW_MS   (500)
#endif
#ifndef PER_KEY_NEARBY_MAX_SEEN
#define PER_KEY_NEARBY_MAX_SEEN         (30)
#endif

// ========== flash 目标列表实时刷新（避免重启才生效）==========
// 注意：不要在每个 ADV 上报里读 flash；只在触发扫描窗口前读取。
#ifndef PER_KEY_SCAN_TARGET_REFRESH_EN
#define PER_KEY_SCAN_TARGET_REFRESH_EN  (1)
#endif
#ifndef PER_KEY_SCAN_TARGET_REFRESH_MS
// 需求：每一次扫描前都读取 flash，则设为 0（不做间隔限制）
#define PER_KEY_SCAN_TARGET_REFRESH_MS  (0)
#endif

static u8 per_key_scan_enabled = 0;
static u8 per_key_mac_list[MAX_MAC_COUNT][6];
static u8 per_key_mac_count = 0;
static s8 per_key_mac_rssi[MAX_MAC_COUNT];
static u8 per_key_mac_rssi_valid[MAX_MAC_COUNT];
static per_key_rssi_filt_t per_key_rssi_filt[MAX_MAC_COUNT];
static u32 per_key_report_timer = 0;

static u32 per_key_scan_period_timer = 0;
static u32 per_key_scan_stop_timer = 0;
static u8  per_key_scan_hw_enabled = 0;
static u8  per_key_scan_last_type = 0xFF;
static u8 nearby_seen_mac_list[PER_KEY_NEARBY_MAX_SEEN][6];
static u8 nearby_seen_mac_cnt = 0;
static u32 nearby_seen_last_reset_ms = 0;
static u32 g_per_key_target_refresh_last_ms = 0;

#ifndef PER_KEY_SCAN_MATCH_LOG_THROTTLE_MS
#define PER_KEY_SCAN_MATCH_LOG_THROTTLE_MS (2000)
#endif
static u32 g_per_key_match_log_last_ms[MAX_MAC_COUNT];
static u32 g_per_key_mcu_sent_log_last_ms[MAX_MAC_COUNT];
static u8 g_per_key_targets_empty_warned = 0;

#ifndef PER_KEY_SCAN_NEARBY_RESET_MS
#define PER_KEY_SCAN_NEARBY_RESET_MS  (1000)
#endif

// 现场验证完毕后可关闭持续打印，避免刷屏/影响长期稳定性
#ifndef PER_KEY_SCAN_NEARBY_LOG_EN
#define PER_KEY_SCAN_NEARBY_LOG_EN (0)
#endif

static void nearby_seen_reset(void)
{
    nearby_seen_mac_cnt = 0;
    memset(nearby_seen_mac_list, 0, sizeof(nearby_seen_mac_list));
}

static void per_key_rssi_filt_reset(per_key_rssi_filt_t *f)
{
    if (!f) {
        return;
    }
    memset(f, 0, sizeof(*f));
    f->ema = -70;
    f->ema_inited = 0;
    f->last_send_ms = 0;
}

static s8 per_key_rssi_filter_3avg_ema(per_key_rssi_filt_t *f, s8 raw)
{
    if (!f) {
        return raw;
    }

    f->s3_buf[f->s3_idx] = raw;
    f->s3_idx = (f->s3_idx + 1) % 3;
    if (f->s3_cnt < 3) {
        f->s3_cnt++;
    }

    s16 sum = 0;
    for (u8 i = 0; i < f->s3_cnt; i++) {
        sum += f->s3_buf[i];
    }
    s8 avg3 = (s8)(sum / f->s3_cnt);

    if (!f->ema_inited) {
        f->ema = avg3;
        f->ema_inited = 1;
    } else {
        s16 diff = (s16)avg3 - f->ema;
        f->ema = f->ema + (diff * 3) / 10; // alpha=0.3
    }

    if (f->ema > 127) {
        f->ema = 127;
    } else if (f->ema < -127) {
        f->ema = -127;
    }

    return (s8)f->ema;
}

static u8 nearby_seen_check_and_add(const u8 *mac)
{
    if (!mac) {
        return 0;
    }
    for (u8 i = 0; i < nearby_seen_mac_cnt; i++) {
        if (memcmp(mac, nearby_seen_mac_list[i], 6) == 0) {
            return 1;
        }
    }
    if (nearby_seen_mac_cnt < PER_KEY_NEARBY_MAX_SEEN) {
        memcpy(nearby_seen_mac_list[nearby_seen_mac_cnt], mac, 6);
        nearby_seen_mac_cnt++;
    }
    return 0;
}

static u8 adv_get_local_name(const u8 *adv_data, u8 adv_len, char *out_name, u8 out_len)
{
    if (!out_name || out_len == 0) {
        return 0;
    }
    out_name[0] = '\0';
    if (!adv_data || adv_len < 2) {
        return 0;
    }

    // AD structure: [len][type][value...]
    const u8 *p = adv_data;
    u8 left = adv_len;

    // 优先找 complete name(0x09)，找不到再用 short name(0x08)
    const u8 *name_ptr = NULL;
    u8 name_len = 0;
    const u8 *short_ptr = NULL;
    u8 short_len = 0;

    while (left) {
        u8 field_len = p[0];
        if (field_len == 0) {
            break;
        }
        if (field_len + 1 > left) {
            break;
        }
        if (field_len >= 2) {
            u8 type = p[1];
            const u8 *val = &p[2];
            u8 val_len = field_len - 1;
            if (type == 0x09) {
                name_ptr = val;
                name_len = val_len;
                break;
            } else if (type == 0x08) {
                short_ptr = val;
                short_len = val_len;
            }
        }
        p += (field_len + 1);
        left -= (field_len + 1);
    }

    if (!name_ptr && short_ptr) {
        name_ptr = short_ptr;
        name_len = short_len;
    }
    if (!name_ptr || name_len == 0) {
        return 0;
    }

    if (name_len >= out_len) {
        name_len = out_len - 1;
    }
    memcpy(out_name, name_ptr, name_len);
    out_name[name_len] = '\0';
    return 1;
}

static void per_key_scan_stop_cb(void *priv)
{
    (void)priv;
    per_key_scan_stop_timer = 0;
    /* 按需求：不打印 scan start/stop，避免刷屏；只输出周边设备明细(mac/name/rssi)。 */
    // 连接态下保持持续扫描，避免频繁启停导致实际 report 变少
    if (smartbox_get_con_handle()) {
        return;
    }
    smbox_bt_ble_scan_enable(0);
    per_key_scan_hw_enabled = 0;
}

static void per_key_scan_refresh_targets_maybe(void)
{
#if PER_KEY_SCAN_TARGET_REFRESH_EN
    // 每次扫描前都读 flash：不做间隔限制
#if (PER_KEY_SCAN_TARGET_REFRESH_MS == 0)
    seamless_unlock_load_targets_from_syscfg();

    /* 现场排查：如果 flash 没有加载出任何目标，会导致“nearby能看到，但永远不可能 match”。
     * 这里仅打一条提示(不打印 flash 内容)，避免刷屏。
     */
    if (per_key_mac_count == 0 && !g_per_key_targets_empty_warned) {
        g_per_key_targets_empty_warned = 1;
        printf("[PER_KEY_SCAN][warn] no targets loaded from flash/syscfg\n");
    } else if (per_key_mac_count != 0) {
        g_per_key_targets_empty_warned = 0;
    }
    return;
#else
    u32 now_ms = timer_get_ms();
    if ((u32)(now_ms - g_per_key_target_refresh_last_ms) < PER_KEY_SCAN_TARGET_REFRESH_MS) {
        return;
    }
    g_per_key_target_refresh_last_ms = now_ms;
    seamless_unlock_load_targets_from_syscfg();

    if (per_key_mac_count == 0 && !g_per_key_targets_empty_warned) {
        g_per_key_targets_empty_warned = 1;
        printf("[PER_KEY_SCAN][warn] no targets loaded from flash/syscfg\n");
    } else if (per_key_mac_count != 0) {
        g_per_key_targets_empty_warned = 0;
    }
#endif
#endif
}

static void per_key_scan_trigger_once(void)
{
    /* 某些场景下若 BLE client 模块处于非激活态，扫描事件可能不会上报到 client_report_adv_data。
     * 这里在触发扫描窗口前确保模块激活。
     */
    ble_client_module_enable(1);

    // 每次扫描前都从 flash 读取目标列表（满足现场“实时修改立即生效”）
    per_key_scan_refresh_targets_maybe();

    // 每次触发前清一次“已打印”列表，方便看到附近设备快照
    nearby_seen_reset();

    // 连接态下，Controller 会优先保证 connection event，扫描会被“时间片”挤占。
    // 主动扫描(SCAN_ACTIVE)还会发送 scan request，占用更多空口，反而更容易让扫描事件变少。
    // 因此：未连接用主动扫，已连接切到被动扫提升稳定性。
    u8 scan_type = SET_SCAN_TYPE;
    if (smartbox_get_con_handle()) {
        scan_type = SCAN_PASSIVE;
    }
    if (scan_type != per_key_scan_last_type) {
        per_key_scan_last_type = scan_type;
        ble_op_set_scan_param(scan_type, SET_SCAN_INTERVAL, SET_SCAN_WINDOW);
    }
    /* 按需求：不打印 scan start/stop，避免刷屏；只输出周边设备明细(mac/name/rssi)。 */
    if (!per_key_scan_hw_enabled) {
        smbox_bt_ble_scan_enable(1);
        per_key_scan_hw_enabled = 1;
    }

    // 连接态下持续扫描，不启用 stop 定时器；未连接时保留窗口扫描（省电）
    if (!smartbox_get_con_handle()) {
        if (per_key_scan_stop_timer) {
            sys_timeout_del(per_key_scan_stop_timer);
            per_key_scan_stop_timer = 0;
        }
        per_key_scan_stop_timer = sys_timeout_add(NULL, per_key_scan_stop_cb, PER_KEY_NEARBY_SCAN_WINDOW_MS);
    }
}

static void per_key_scan_period_cb(void *priv)
{
    (void)priv;
    if (!per_key_scan_enabled) {
        return;
    }
    // 实时从 flash 读取目标 MAC 列表（带最小间隔），确保动态更新能生效
    per_key_scan_refresh_targets_maybe();
    per_key_scan_trigger_once();
}

void per_key_scan_init(void)
{
    // 初始化蓝牙扫描相关设置
    // 设置扫描参数
    ble_op_set_scan_param(SET_SCAN_TYPE, SET_SCAN_INTERVAL, SET_SCAN_WINDOW);
    per_key_scan_enabled = 0;
    per_key_mac_count = 0;
    memset(per_key_mac_list, 0, sizeof(per_key_mac_list));
    memset(per_key_mac_rssi_valid, 0, sizeof(per_key_mac_rssi_valid));
    for (u8 i = 0; i < MAX_MAC_COUNT; i++) {
        per_key_rssi_filt_reset(&per_key_rssi_filt[i]);
    }
}

void per_key_scan_enable(u8 en)
{
    per_key_scan_enabled = en ? 1 : 0;
    if (per_key_scan_enabled) {
        nearby_seen_reset();
        /* 确保 enable 后马上开始扫描：
         * - 启动周期触发（持续输出附近设备 MAC/RSSI）
         * - 立即触发一次扫描窗口（避免等待首个周期）
         */
        if (per_key_scan_period_timer == 0) {
            per_key_scan_period_timer = sys_timer_add(NULL, per_key_scan_period_cb, PER_KEY_NEARBY_SCAN_PERIOD_MS);
        }
        per_key_scan_refresh_targets_maybe();
        per_key_scan_trigger_once();
        printf("[PER_KEY_SCAN] enable\n");
        return;
    }

    /* 关闭时做一次保守收尾：如果之前启动过内部扫描/定时器，避免残留 */
    if (per_key_scan_period_timer) {
        sys_timer_del(per_key_scan_period_timer);
        per_key_scan_period_timer = 0;
    }
    if (per_key_scan_stop_timer) {
        sys_timeout_del(per_key_scan_stop_timer);
        per_key_scan_stop_timer = 0;
    }
    smbox_bt_ble_scan_enable(0);
    per_key_scan_hw_enabled = 0;
    printf("[PER_KEY_SCAN] disable\n");
}

void per_key_scan_set_targets(const u8 *mac_list, u8 count)
{
    if (mac_list == NULL || count == 0) {
        per_key_mac_count = 0;
        memset(per_key_mac_list, 0, sizeof(per_key_mac_list));
        memset(per_key_mac_rssi_valid, 0, sizeof(per_key_mac_rssi_valid));
        for (u8 i = 0; i < MAX_MAC_COUNT; i++) {
            per_key_rssi_filt_reset(&per_key_rssi_filt[i]);
        }
        return;
    }

    if (count > MAX_MAC_COUNT) {
        count = MAX_MAC_COUNT;
    }

    per_key_mac_count = count;
    for (u8 i = 0; i < count; i++) {
        memcpy(per_key_mac_list[i], mac_list + i * 6, 6);
        per_key_mac_rssi_valid[i] = 0;
        per_key_rssi_filt_reset(&per_key_rssi_filt[i]);
    }
}

void per_key_scan_mac(void)
{
    // 启用扫描（目标MAC列表需由上层通过 per_key_scan_set_targets 先设置）
    // 改为：使用 5s 定时器触发扫描窗口（方便观察周围设备 MAC + 名称）
    per_key_scan_enabled = 1;
    if (per_key_scan_period_timer == 0) {
        per_key_scan_period_timer = sys_timer_add(NULL, per_key_scan_period_cb, PER_KEY_NEARBY_SCAN_PERIOD_MS);
    }

    // 启动时先刷新一次 flash 目标列表
    per_key_scan_refresh_targets_maybe();
    per_key_scan_trigger_once();
}

void per_key_scan_stop(void)
{
    per_key_scan_enabled = 0;

    if (per_key_scan_period_timer) {
        sys_timer_del(per_key_scan_period_timer);
        per_key_scan_period_timer = 0;
    }
    if (per_key_scan_stop_timer) {
        sys_timeout_del(per_key_scan_stop_timer);
        per_key_scan_stop_timer = 0;
    }

    smbox_bt_ble_scan_enable(0);
    per_key_scan_hw_enabled = 0;
}

u8 per_key_scan_check_mac(const u8 *mac)
{
    if (!per_key_scan_enabled || mac == NULL) {
        return 0;
    }
    for (u8 i = 0; i < per_key_mac_count; i++) {
        if (memcmp(mac, per_key_mac_list[i], 6) == 0) {
            return 1;
        }
    }
    return 0;
}

void per_key_scan_on_adv_report(const u8 *mac, s8 rssi)
{
    if (!per_key_scan_enabled || mac == NULL) {
        return;
    }

    for (u8 i = 0; i < per_key_mac_count; i++) {
        if (memcmp(mac, per_key_mac_list[i], 6) == 0) {
            per_key_mac_rssi[i] = rssi;
            per_key_mac_rssi_valid[i] = 1;
            return;
        }
    }
}

void per_key_scan_on_adv_report_ex(const u8 *mac, const u8 *adv_data, u8 adv_len, s8 rssi)
{
    if (!per_key_scan_enabled || mac == NULL) {
        return;
    }

    /* 按需求：直接打印所有附近设备的 MAC + RSSI。
     * - 不做去重
     * - 不解析 name
     */
#if PER_KEY_SCAN_NEARBY_LOG_EN
    printf("[PER_KEY_SCAN][nearby] mac=%02x:%02x:%02x:%02x:%02x:%02x rssi=%d\n",
           mac[5], mac[4], mac[3], mac[2], mac[1], mac[0], (int)rssi);
#endif

    /* 若外部模块在持续扫描但未调用 per_key_scan_trigger_once()，nearby 去重列表会一直增长。
     * 这里按时间周期重置一次，保证日志持续可见。
     */
    {
        u32 now_ms = timer_get_ms();
        if ((u32)(now_ms - nearby_seen_last_reset_ms) >= PER_KEY_SCAN_NEARBY_RESET_MS) {
            nearby_seen_last_reset_ms = now_ms;
            nearby_seen_reset();
        }
    }

    // 1) 命中目标 MAC：同时兼容“正序/反序”(HCI 上报地址端序可能与 flash 保存不一致)
    s8 matched_idx = -1;
    u8 matched_mac_from_flash[6] = {0};
    for (u8 i = 0; i < per_key_mac_count; i++) {
        const u8 *t = per_key_mac_list[i];

        u8 rev[6];
        rev[0] = mac[5];
        rev[1] = mac[4];
        rev[2] = mac[3];
        rev[3] = mac[2];
        rev[4] = mac[1];
        rev[5] = mac[0];

        if (memcmp(mac, t, 6) == 0 || memcmp(rev, t, 6) == 0) {
            per_key_mac_rssi[i] = rssi;
            per_key_mac_rssi_valid[i] = 1;
            matched_idx = (s8)i;
            memcpy(matched_mac_from_flash, t, 6);
            break;
        }
    }

#if PER_KEY_SCAN_RSSI_TO_MCU_TEST_EN
    /* 目标 MAC：按需求“扫描到就直接发给 MCU 让 MCU 判断”。
     * - 不再等待 3 个样本/不再依赖滤波有效门槛
     * - 仍保留最小间隔限速（默认 0ms）用于保护串口/MCU
     * 上报格式对齐 rssi_check.c：payload[0]=rssi+128, payload[1..6]=MAC(使用flash保存顺序)。
     */
    if (matched_idx >= 0) {
        /* 先给一个“命中但未必发送MCU”的低频日志：
         * 你看到 MOONDROP EDGE 的 name 说明此 MAC 正在广播；若 flash 里也存了同 MAC，
         * 这里应当会出现 match。若不出现，优先怀疑：flash未加载出目标/目标MAC不一致/端序问题。
         */
        u32 now_ms = timer_get_ms();
        u8 mi = (u8)matched_idx;
        if ((u32)(now_ms - g_per_key_match_log_last_ms[mi]) >= PER_KEY_SCAN_MATCH_LOG_THROTTLE_MS) {
            g_per_key_match_log_last_ms[mi] = now_ms;
            per_key_rssi_filt_t *f_dbg = &per_key_rssi_filt[mi];
            printf("\n!!!!!!!!!!!!!!!!!!!! [PER_KEY_SCAN][MATCHED_FLASH_TARGET] !!!!!!!!!!!!!!!!!!!!\n");
            printf("[MATCH] idx=%d flash_mac=%02x:%02x:%02x:%02x:%02x:%02x adv_mac=%02x:%02x:%02x:%02x:%02x:%02x\n",
                   (int)matched_idx,
                   matched_mac_from_flash[0], matched_mac_from_flash[1], matched_mac_from_flash[2],
                   matched_mac_from_flash[3], matched_mac_from_flash[4], matched_mac_from_flash[5],
                   mac[5], mac[4], mac[3], mac[2], mac[1], mac[0]);
            printf("[RSSI] raw=%d s3_cnt=%u\n", (int)rssi, (unsigned)f_dbg->s3_cnt);
            printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n\n");
        }

        per_key_rssi_filt_t *f = &per_key_rssi_filt[(u8)matched_idx];
        (void)per_key_rssi_filter_3avg_ema(f, rssi); // 仍更新内部滤波状态，便于后续需要时启用

        now_ms = timer_get_ms();
        if ((u32)(now_ms - f->last_send_ms) >= PER_KEY_SCAN_RSSI_MCU_MIN_INTERVAL_MS) {
            u8 payload[7] = {0};
            payload[0] = (u8)(rssi + 128);
            memcpy(&payload[1], matched_mac_from_flash, 6);
            uart1_send_toMCU(PER_KEY_SCAN_RSSI_MCU_PROTOCOL_ID, payload, sizeof(payload));
            f->last_send_ms = now_ms;

            // 显眼但限频：避免每条ADV都刷屏
            u8 mi2 = (u8)matched_idx;
            if ((u32)(now_ms - g_per_key_mcu_sent_log_last_ms[mi2]) >= PER_KEY_SCAN_MCU_SENT_LOG_THROTTLE_MS) {
                g_per_key_mcu_sent_log_last_ms[mi2] = now_ms;
                printf("\n#################### [PER_KEY_SCAN][MCU_SENT_RAW] ####################\n");
                printf("[MCU_SENT] idx=%d mac=%02x:%02x:%02x:%02x:%02x:%02x raw_rssi=%d val=%u\n",
                       (int)matched_idx,
                       matched_mac_from_flash[0], matched_mac_from_flash[1], matched_mac_from_flash[2],
                       matched_mac_from_flash[3], matched_mac_from_flash[4], matched_mac_from_flash[5],
                       (int)rssi, (unsigned)payload[0]);
                printf("[MCU] proto=0x%04x payload_len=%u\n", (unsigned)PER_KEY_SCAN_RSSI_MCU_PROTOCOL_ID, (unsigned)sizeof(payload));
                printf("###############################################################\n\n");
            }
        }
    }
#endif

    (void)adv_data;
    (void)adv_len;
}

u8 per_key_scan_get_last_rssi(const u8 *mac, s8 *out_rssi)
{
    if (mac == NULL || out_rssi == NULL) {
        return 0;
    }

    for (u8 i = 0; i < per_key_mac_count; i++) {
        if (memcmp(mac, per_key_mac_list[i], 6) == 0) {
            if (per_key_mac_rssi_valid[i]) {
                *out_rssi = per_key_mac_rssi[i];
                return 1;
            }
            return 0;
        }
    }

    return 0;
}

static void per_key_report_cb(void *priv)
{
    // 打印所有匹配MAC的最新RSSI
    for (u8 i = 0; i < per_key_mac_count; i++) {
        if (per_key_mac_rssi_valid[i]) {
            printf("[PER_KEY_SCAN]PerKey match MAC[%d]:", i);
            put_buf(per_key_mac_list[i], 6);
            printf("[PER_KEY_SCAN] RSSI: %d\n", per_key_mac_rssi[i]);
        }
    }
}

void per_key_scan_start_report(u32 interval_ms)
{
    if (per_key_report_timer) {
        sys_timeout_del(per_key_report_timer);
        per_key_report_timer = 0;
    } 
    if (interval_ms == 0) {
        return;
    }
    per_key_report_timer = sys_timeout_add(NULL, per_key_report_cb, interval_ms);
}

void per_key_scan_stop_report(void)
{
    if (per_key_report_timer) {
        sys_timeout_del(per_key_report_timer);
        per_key_report_timer = 0;
    }
}