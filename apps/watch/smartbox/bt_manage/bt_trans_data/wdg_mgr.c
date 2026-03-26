#include "wdg_mgr.h"

#include "app_config.h"
#include "system/includes.h"
#include "system/timer.h"
// wdt.h 在 include_lib 路径下，部分编译单元的包含路径可能不一致，使用相对路径以确保可找到
#include "../../../../../../include_lib/driver/cpu/br28/asm/wdt.h"
#include <string.h>

/* 打开此宏以启用喂狗时的日志打印（默认关闭，避免刷屏/影响实时性） */
// #define WDG_LOG_ENABLE

#define WDG_MGR_MAX_MODULES 8

typedef struct {
    char     name[16];
    u32      timeout_ms;
    u32      remain_ms;
    u8       used;
} wdg_slot_t;

static wdg_slot_t wdg_slots[WDG_MGR_MAX_MODULES];
static u32 wdg_check_period_ms = 2000;
static u32 wdg_hw_timeout_ms   = 16000;
static u8  wdg_enabled         = 1;   /* 总开关 */
static u8  wdg_maintenance     = 0;   /* 维护/升级模式：不喂狗 */
static int wdg_timer_id        = 0;

/* 将毫秒转换为 WDT 枚举档位，保守取上限，避免过短 */
static u32 wdg_ms_to_enum(u32 ms)
{
    if (ms <= 1)    return WDT_1MS;
    if (ms <= 2)    return WDT_2MS;
    if (ms <= 4)    return WDT_4MS;
    if (ms <= 8)    return WDT_8MS;
    if (ms <= 16)   return WDT_16MS;
    if (ms <= 32)   return WDT_32MS;
    if (ms <= 64)   return WDT_64MS;
    if (ms <= 128)  return WDT_128MS;
    if (ms <= 256)  return WDT_256MS;
    if (ms <= 512)  return WDT_512MS;
    if (ms <= 1000) return WDT_1S;
    if (ms <= 2000) return WDT_2S;
    if (ms <= 4000) return WDT_4S;
    if (ms <= 8000) return WDT_8S;
    if (ms <= 16000)return WDT_16S;
    if (ms <= 32000)return WDT_32S;
    return WDT_32S;
}

static void wdg_mgr_feed_hw(void)
{
    /* 趣味风 ASCII logo 打印，受宏控制以便调试时开启/关闭 */
#ifdef WDG_LOG_ENABLE
    printf("[WDG] feed at %u ms -- OK\n", sys_timer_get_ms());
#endif
    wdt_clear();
}

static void wdg_mgr_tick(void *priv)
{
    if (!wdg_enabled || wdg_maintenance) {
        return;
    }

    u8 all_ok = 1;
#ifdef WDG_LOG_ENABLE
    printf("[WDG] tick: check_period=%u ms\n", wdg_check_period_ms);
#endif
    for (int i = 0; i < WDG_MGR_MAX_MODULES; i++) {
        if (!wdg_slots[i].used) {
            continue;
        }
        if (wdg_slots[i].timeout_ms == 0) {
            continue; /* 未设置超时，跳过 */
        }
#ifdef WDG_LOG_ENABLE
        printf("[WDG] slot %d: name=%s used=%d timeout=%u remain=%u\n",
               i, wdg_slots[i].name[0] ? wdg_slots[i].name : "(null)", wdg_slots[i].used, wdg_slots[i].timeout_ms, wdg_slots[i].remain_ms);
#endif
        if (wdg_slots[i].remain_ms > wdg_check_period_ms) {
            wdg_slots[i].remain_ms -= wdg_check_period_ms;
        } else {
            wdg_slots[i].remain_ms = 0;
        }
#ifdef WDG_LOG_ENABLE
        if (wdg_slots[i].remain_ms == 0) {
            printf("[WDG] slot timeout: idx=%d name=%s\n", i, wdg_slots[i].name[0] ? wdg_slots[i].name : "(null)");
            all_ok = 0; /* 有模块超时 */
        }
#else
        if (wdg_slots[i].remain_ms == 0) {
            all_ok = 0; /* 有模块超时 */
        }
#endif
    }

    if (all_ok) {
        wdg_mgr_feed_hw();
    } else {
        /* 模块超时，不再喂狗，等待硬件 WDT 复位 */
    }
}
/**
 * @brief   wdg_mgr_init - 初始化看门狗管理器
 * 
 * @param hw_timeout_ms 硬件看门狗超时时间，单位毫秒
 * @param check_period_ms 软件看门狗检查周期，单位毫秒
 */
void wdg_mgr_init(u32 hw_timeout_ms, u32 check_period_ms)
{
    memset(wdg_slots, 0, sizeof(wdg_slots));
    wdg_hw_timeout_ms   = hw_timeout_ms ? hw_timeout_ms : 16000;
    wdg_check_period_ms = check_period_ms ? check_period_ms : 2000;

    wdt_init(wdg_ms_to_enum(wdg_hw_timeout_ms));
    wdt_enable();

    if (wdg_timer_id) {
        sys_timer_del(wdg_timer_id);
        wdg_timer_id = 0;
    }
    wdg_timer_id = sys_timer_add(NULL, wdg_mgr_tick, wdg_check_period_ms);
#ifdef WDG_LOG_ENABLE
    printf("[WDG] init: hw_timeout=%u ms, check_period=%u ms, timer_id=%d\n",
           wdg_hw_timeout_ms, wdg_check_period_ms, wdg_timer_id);
#endif
}

void wdg_mgr_enable(u8 en)
{
    wdg_enabled = en ? 1 : 0;
}

void wdg_mgr_set_maintenance(u8 on)
{
    wdg_maintenance = on ? 1 : 0;
}

void wdg_mgr_force_feed(void)
{
    if (wdg_enabled && !wdg_maintenance) {
        wdg_mgr_feed_hw();
    }
}

int wdg_mgr_register(const char *name, u32 timeout_ms)
{
    if (!timeout_ms) {
        return -1;
    }
    for (int i = 0; i < WDG_MGR_MAX_MODULES; i++) {
        if (!wdg_slots[i].used) {
            wdg_slots[i].used = 1;
            wdg_slots[i].timeout_ms = timeout_ms;
            wdg_slots[i].remain_ms  = timeout_ms;
            memset(wdg_slots[i].name, 0, sizeof(wdg_slots[i].name));
            if (name) {
                strncpy(wdg_slots[i].name, name, sizeof(wdg_slots[i].name) - 1);
            }
            int id = i + 1; /* 模块ID = index+1 */
#ifdef WDG_LOG_ENABLE
            printf("[WDG] register: name=%s, timeout=%u ms -> id=%d\n",
                   wdg_slots[i].name[0] ? wdg_slots[i].name : "(null)", wdg_slots[i].timeout_ms, id);
#endif
            return id;
        }
    }
    return -1; /* 没有空槽 */
}

void wdg_mgr_unregister(int id)
{
    int idx = id - 1;
    if (idx < 0 || idx >= WDG_MGR_MAX_MODULES) {
        return;
    }
    memset(&wdg_slots[idx], 0, sizeof(wdg_slots[idx]));
}

void wdg_mgr_heartbeat(int id)
{
    int idx = id - 1;
    if (idx < 0 || idx >= WDG_MGR_MAX_MODULES) {
        return;
    }
    if (!wdg_slots[idx].used) {
        return;
    }
    wdg_slots[idx].remain_ms = wdg_slots[idx].timeout_ms;
}
