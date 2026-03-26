#ifndef __PER_KEY_SCAN__
#define __PER_KEY_SCAN__

#include "system/includes.h"
// 初始化蓝牙扫描
void per_key_scan_init(void);
// 仅启用/关闭 per_key_scan 的处理与日志（不主动启动扫描窗口/定时器）
void per_key_scan_enable(u8 en);
// 扫描特定的MAC地址
void per_key_scan_mac(void);
// 设置需要扫描的MAC列表（mac_list 为 count*6 字节的连续数组）
void per_key_scan_set_targets(const u8 *mac_list, u8 count);
// 停止指定MAC扫描模式
void per_key_scan_stop(void);
// 检查MAC是否在扫描列表中
u8 per_key_scan_check_mac(const u8 *mac);
// 处理扫描到的广播（用于更新RSSI）
void per_key_scan_on_adv_report(const u8 *mac, s8 rssi);
// 处理扫描到的广播（带 adv payload，可解析设备名称）
void per_key_scan_on_adv_report_ex(const u8 *mac, const u8 *adv_data, u8 adv_len, s8 rssi);
// 获取指定MAC最近一次扫描到的RSSI（返回1表示有效）
u8 per_key_scan_get_last_rssi(const u8 *mac, s8 *out_rssi);
// 周期性上报匹配设备（启动/停止）
void per_key_scan_start_report(u32 interval_ms);
void per_key_scan_stop_report(void);
#endif // !__PER_KEY_SCAN__
