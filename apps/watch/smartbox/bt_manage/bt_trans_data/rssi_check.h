#ifndef __RSSI_CHECK_H__
#define __RSSI_CHECK_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
// 本文件用于自动解锁设备 ，根据RSSI值判断是否需要自动解锁设备
// 外部变量声明
extern uint16_t conn_handle;

char rssi_check(uint8_t *data, uint8_t len, uint16_t conn_handle);
int rssi_timer_start(void);
void rssi_timer_stop(void);
int rssi_check_init(void);
void rssi_check_exit(void);
void rssi_set_conn_handle(uint16_t handle);
char rssi_get_current(void);
char rssi_get_previous(void);
void update_rssi_THRESHOLD_data(void);

// multi-ble：按连接 cid 管理 RSSI 上报（用于区分不同手机）
void rssi_on_connect(uint8_t cid, uint16_t handle, uint8_t addr_type, const uint8_t *addr);
void rssi_on_disconnect(uint8_t cid);
int rssi_timer_start_by_cid(uint8_t cid);
void rssi_timer_stop_by_cid(uint8_t cid);

#ifdef __cplusplus
}
#endif

#endif /* __RSSI_CHECK_H__ */