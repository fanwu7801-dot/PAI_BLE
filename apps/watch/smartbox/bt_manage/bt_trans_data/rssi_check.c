#include "rssi_check.h"
#include "btstack/le/ble_api.h"
#include "driver/cpu/br28/asm/cpu.h"
#include "syscfg_id.h"
#include "system/includes.h"
#include "usart0_to_mcu.h"
#include "user_cfg_id.h"
#include <stdlib.h> // 添加abs函数所需的头文件

// RSSI相关变量定义（保留单连接兼容接口）
#ifndef RSSI_LOG_ENABLE
#define RSSI_LOG_ENABLE  0
#endif
static char rssi_cur = 0;
static char rssi_per = 0;

// 连接句柄变量定义（保留单连接兼容接口）
uint16_t conn_handle = 0;

// multi-ble: 最大从机连接数（与 le_multi 的 SUPPORT_MAX_SLAVE_CONN 对齐）
#ifndef RSSI_MAX_CONN
#define RSSI_MAX_CONN 4
#endif

// 定时器相关定义
#define RSSI_PERIOD 100 // 1s 上报一次

// RSSI阈值定义
int8_t RSSI_THRESHOLD_CLOSE = -90;   // 接近阈值：-60dBm
int8_t RSSI_THRESHOLD_WARNING = -70; // 警告阈值：-70dBm
int8_t RSSI_THRESHOLD_LEAVE = -80;   // 离开阈值：-80dBm

// 连续判断阈值
#define RSSI_CLOSE_THRESHOLD 7 // 连续7次改善判断为接近
#define RSSI_LEAVE_THRESHOLD 7 // 连续7次恶化判断为离开

// 计数器变量
static u8 rssi_close_count = 0;
static u8 rssi_leave_count = 0;

// 状态机定义
typedef enum {
  Rssi_Close = 0,  // 接近ebike
  Rssi_Leave = 1,  // 离开ebike
  Rssi_Unknown = 2 // 未知状态
} Rssi_Status;

static Rssi_Status rssi_status = Rssi_Unknown;

// ========== multi-ble 上报上下文 ==========
typedef struct {
  u8 in_use;
  u8 cid;
  u16 handle;
  u8 addr_type;
  u8 id_addr[6];
  u8 id_addr_valid;
  u32 timer;

  // 3点平均
  s8 s3_buf[3];
  u8 s3_cnt;
  u8 s3_idx;

  // EMA（整数）：ema = ema + alpha*(x-ema)，alpha=3/10
  s16 ema;
  u8 ema_inited;
  s8 last_sent;
} rssi_ctx_t;

static rssi_ctx_t g_rssi_ctx[RSSI_MAX_CONN];

static void rssi_timer_handler(void *arg);

extern void swapX(const uint8_t *src, uint8_t *dst, int len);

static rssi_ctx_t *rssi_get_ctx(u8 cid)
{
  if (cid >= RSSI_MAX_CONN) {
    return NULL;
  }
  return &g_rssi_ctx[cid];
}

static s8 rssi_filter_3avg_ema(rssi_ctx_t *ctx, s8 raw)
{
  if (!ctx) {
    return raw;
  }

  ctx->s3_buf[ctx->s3_idx] = raw;
  ctx->s3_idx = (ctx->s3_idx + 1) % 3;
  if (ctx->s3_cnt < 3) {
    ctx->s3_cnt++;
  }

  s16 sum = 0;
  for (u8 i = 0; i < ctx->s3_cnt; i++) {
    sum += ctx->s3_buf[i];
  }
  s8 avg3 = (s8)(sum / ctx->s3_cnt);

  if (!ctx->ema_inited) {
    ctx->ema = avg3;
    ctx->ema_inited = 1;
  } else {
    s16 diff = (s16)avg3 - ctx->ema;
    ctx->ema = ctx->ema + (diff * 3) / 10; // alpha=0.3
  }

  if (ctx->ema > 127) {
    ctx->ema = 127;
  } else if (ctx->ema < -127) {
    ctx->ema = -127;
  }

  return (s8)ctx->ema;
}

static void rssi_send_to_mcu(rssi_ctx_t *ctx, s8 rssi_dbm)
{
  if (!ctx || !ctx->in_use) {
    return;
  }

  // 单独打印RSSI值，便于调试
#if RSSI_LOG_ENABLE
  printf("RSSI的值 CID %d: %d dBm (MAC: %02X:%02X:%02X:%02X:%02X:%02X)\n",
         ctx->cid, rssi_dbm,
         ctx->id_addr[0], ctx->id_addr[1], ctx->id_addr[2],
         ctx->id_addr[3], ctx->id_addr[4], ctx->id_addr[5]);
#endif

  // payload: 1字节信号值(0~255) + 6字节MAC
  u8 payload[7] = {0};
  payload[0] = (u8)(rssi_dbm + 128);

  if (ctx->id_addr_valid) {
    memcpy(&payload[1], ctx->id_addr, 6);
  } else {
    memset(&payload[1], 0, 6);
  }

  uart1_send_toMCU(0x00F3, payload, sizeof(payload));
}

// RSSI检查函数（兼容：单次读取并做 3avg+EMA）
char rssi_check(uint8_t *data, uint8_t len, uint16_t handle)
{
  (void)data;
  (void)len;

  s8 raw = ble_vendor_get_peer_rssi(handle);
  rssi_ctx_t *ctx = &g_rssi_ctx[0];
  s8 filtered = rssi_filter_3avg_ema(ctx, raw);

  rssi_per = rssi_cur;
  rssi_cur = filtered;
  return filtered;
}

int rssi_timer_start_by_cid(uint8_t cid)
{
  rssi_ctx_t *ctx = rssi_get_ctx(cid);
  if (!ctx || !ctx->in_use || ctx->handle == 0) {
    return -1;
  }

  if (ctx->timer == 0) {
    ctx->timer = sys_timer_add((void *)(u32)cid, rssi_timer_handler, RSSI_PERIOD);
    if (!ctx->timer) {
      return -1;
    }
  }
  return 0;
}

void rssi_timer_stop_by_cid(uint8_t cid)
{
  rssi_ctx_t *ctx = rssi_get_ctx(cid);
  if (!ctx) {
    return;
  }
  if (ctx->timer) {
    sys_timer_del(ctx->timer);
    ctx->timer = 0;
  }
}

// 兼容旧接口：默认使用 cid=0
int rssi_timer_start(void)
{
  return rssi_timer_start_by_cid(0);
}

void rssi_timer_stop(void)
{
  rssi_timer_stop_by_cid(0);
}

// ========== 优化的定时器回调函数 ==========

static void rssi_timer_handler(void *arg)
{
  u8 cid = (u8)(u32)arg;
  rssi_ctx_t *ctx = rssi_get_ctx(cid);
  if (!ctx || !ctx->in_use || ctx->handle == 0) {
    return;
  }

  s8 raw = ble_vendor_get_peer_rssi(ctx->handle);
  s8 filtered = rssi_filter_3avg_ema(ctx, raw);

  // 更新兼容接口变量（仅用于调试/旧调用）
  if (cid == 0) {
    rssi_per = rssi_cur;
    rssi_cur = filtered;
    conn_handle = ctx->handle;
  }

  // 每连接带 MAC 上报，MCU 可按 MAC 区分是哪台手机
  rssi_send_to_mcu(ctx, filtered);
}

// 初始化RSSI检查模块
int rssi_check_init(void)
{
  rssi_cur = 0;
  rssi_per = 0;
  conn_handle = 0;
  rssi_status = Rssi_Unknown;
  rssi_close_count = 0;
  rssi_leave_count = 0;

  memset(g_rssi_ctx, 0, sizeof(g_rssi_ctx));
  for (u8 i = 0; i < RSSI_MAX_CONN; i++) {
    g_rssi_ctx[i].cid = i;
    g_rssi_ctx[i].last_sent = -127;
  }
  return 0;
}

// 退出RSSI检查模块
void rssi_check_exit(void) {
  for (u8 i = 0; i < RSSI_MAX_CONN; i++) {
    rssi_timer_stop_by_cid(i);
  }
}

// 设置连接句柄
void rssi_set_conn_handle(uint16_t handle) {
  conn_handle = handle;
  g_rssi_ctx[0].in_use = 1;
  g_rssi_ctx[0].handle = handle;
}

// 获取当前RSSI值（滤波后）
char rssi_get_current(void) { return rssi_cur; }

// 获取上一次RSSI值
char rssi_get_previous(void) { return rssi_per; }

// 获取当前状态
Rssi_Status rssi_get_status(void) { return rssi_status; }

// 获取滤波后的RSSI值（现在直接返回原始值）
char rssi_get_filtered(uint16_t handle) {
  return ble_vendor_get_peer_rssi(handle);
}

// 重置计数器（用于手动重置状态判断）
void rssi_reset_counters(void) {
  rssi_close_count = 0;
  rssi_leave_count = 0;
#if RSSI_LOG_ENABLE
  printf("RSSI counters reset\n");
#endif
}

// 重置滤波算法
void rssi_reset_filters(void) {
  for (u8 i = 0; i < RSSI_MAX_CONN; i++) {
    rssi_ctx_t *ctx = &g_rssi_ctx[i];
    ctx->s3_cnt = 0;
    ctx->s3_idx = 0;
    ctx->ema_inited = 0;
    ctx->ema = -70;
  }
}

void rssi_on_connect(uint8_t cid, uint16_t handle, uint8_t addr_type, const uint8_t *addr)
{
  rssi_ctx_t *ctx = rssi_get_ctx(cid);
  if (!ctx) {
    return;
  }

  ctx->in_use = 1;
  ctx->cid = cid;
  ctx->handle = handle;
  ctx->addr_type = addr_type;
  ctx->id_addr_valid = 0;

  // addr 来自 HCI event，通常需要 swap 后再查 id_addr
  if (addr) {
    u8 tmp_addr[6] = {0};
    swapX(addr, tmp_addr, 6);
    if (ble_list_get_id_addr(tmp_addr, addr_type, ctx->id_addr)) {
      ctx->id_addr_valid = 1;
    }
  }

  ctx->s3_cnt = 0;
  ctx->s3_idx = 0;
  ctx->ema_inited = 0;
  ctx->ema = -70;
  ctx->last_sent = -127;

  rssi_timer_start_by_cid(cid);
}

void rssi_on_disconnect(uint8_t cid)
{
  rssi_ctx_t *ctx = rssi_get_ctx(cid);
  if (!ctx) {
    return;
  }
  rssi_timer_stop_by_cid(cid);
  memset(ctx, 0, sizeof(*ctx));
  ctx->cid = cid;
  ctx->last_sent = -127;
}
