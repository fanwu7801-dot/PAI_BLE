#include "ble_user.h"
#include "app_config.h"
#include "system/includes.h"
#include "bt/bt.h"
#include "cpu.h"

#define GFPS_TIMEOUT_CLOSE_TIME  60*1000
#define AUTHENTICATION_MODE 1 //过认证的时候要开，不然会过不了认证。

#if (GFPS_EN)
u16 gfps_time_id = 0;
u8 gfps_flag = 0;

extern void gfps_ble_module_enable(u8 en);
extern void gfps_set_pair_mode_by_user(u8 disconn_ble);
extern int gfps_bt_ble_adv_enable(u8 enable);
extern int gfps_init_bredr_do(int priv);
extern void gfps_init_bredr_dev();
extern void set_idle_period_slot(u16 slot);

static void gfps_timeout_close_ble()
{
#if !AUTHENTICATION_MODE
    if (gfps_time_id != 0) {
        /* sys_timeout_del(gfps_time_id);	 */
        gfps_time_id = 0;
    }
    gfps_flag = 0;
    gfps_ble_module_enable(0);
    gfps_bt_ble_adv_enable(0);
#endif
}
void gfps_ctrl_pair_mode(u8 enable)
{
    if (enable == 1) {

#if !AUTHENTICATION_MODE
        if (gfps_time_id != 0) {
            sys_timeout_del(gfps_time_id);
            gfps_time_id = 0;
        }
#endif
        gfps_flag = 1;
        /* gfps_ble_module_enable(1); */
        gfps_set_pair_mode_by_user(1);
#if !AUTHENTICATION_MODE
        gfps_time_id = sys_timeout_add(NULL, gfps_timeout_close_ble, GFPS_TIMEOUT_CLOSE_TIME); //有无连接上都需要关掉不然功耗高
#endif
        gfps_init_bredr_dev();
        set_idle_period_slot(400);

    } else {
        gfps_flag = 0;
        gfps_bt_ble_adv_enable(0);
        set_idle_period_slot(1600);
        bt_close_bredr();
        /* gfps_set_pair_mode_by_user(0); */
#if !AUTHENTICATION_MODE
        if (gfps_time_id != 0) {
            sys_timeout_del(gfps_time_id);
            gfps_time_id = 0;
        }
#endif

    }
}
#endif
