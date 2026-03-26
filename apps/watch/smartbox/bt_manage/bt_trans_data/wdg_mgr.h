#ifndef __WIGET_WDG_MGR_H__
#define __WIGET_WDG_MGR_H__

#include "typedef.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 轻量级看门狗管理器：
 * - 统一喂硬件 WDT，避免分散调用 clr_wdt / wdt_clear
 * - 模块注册心跳（heartbeat），超过各自超时时间将停止喂狗，触发硬件复位
 * - 提供维护/升级模式开关，可暂时禁用喂狗
 */

typedef struct {
    const char *name;       /* 模块名称，调试用 */
    u32 timeout_ms;         /* 该模块允许的最大无心跳时间 */
} wdg_mgr_module_cfg_t;

/* 初始化/控制 */
void wdg_mgr_init(u32 hw_timeout_ms, u32 check_period_ms);
void wdg_mgr_enable(u8 en);                 /* 全局开关：0 禁止喂硬件狗 */
void wdg_mgr_set_maintenance(u8 on);        /* 维护/升级模式：on=1 临时不喂狗 */
void wdg_mgr_force_feed(void);              /* 强制立刻喂硬件狗（谨慎使用） */

/* 模块注册与心跳 */
int  wdg_mgr_register(const char *name, u32 timeout_ms); /* 返回模块ID (>0) 或 -1 */
void wdg_mgr_unregister(int id);
void wdg_mgr_heartbeat(int id);             /* 模块正常工作时调用，重置其计时 */

#ifdef __cplusplus
}
#endif

#endif /* __WIGET_WDG_MGR_H__ */
