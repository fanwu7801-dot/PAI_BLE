#include "battery_charge.h"
#include "asm/charge.h"
#include "asm/adc_api.h"
#include "app_config.h"

/* 全局电池信息实例 */
Battery_Charge battery_info = {
    .voltage = 0.0f,
    .current = 0.0f,
    .temperature = 25.0f,
    .charge_status = 0,
    .charge_start = NULL,
    .charge_stop = NULL,
};

/* 封装底层充电开始函数 */
static void my_charge_start(void)
{
#if TCFG_CHARGE_ENABLE
    charge_start();
#endif
}

/* 封装底层充电停止函数 */
static void my_charge_stop(void)
{
#if TCFG_CHARGE_ENABLE
    charge_close();
#endif
}

/* 电池信息更新函数，建议在定时器中调用（如每秒一次） */
static void battery_charge_update(void *priv)
{
    // 1. 获取电压 (SDK接口返回的是 1/4 分压后的值，所以 * 4，单位 mV)
    u32 vol_mv = adc_get_voltage(AD_CH_VBAT) * 4;
    battery_info.voltage = (float)vol_mv / 1000.0f; // 转换为 V

    // 2. 获取充电状态
    // get_charge_full_flag(): 1=充满
    // get_charge_online_flag(): 1=插入充电
    extern u8 get_charge_full_flag(void);
    extern u8 get_charge_online_flag(void);

    if (get_charge_full_flag()) {
        battery_info.charge_status = 2; // 充满
    } else if (get_charge_online_flag()) {
        battery_info.charge_status = 1; // 充电中
    } else {
        battery_info.charge_status = 0; // 未充电
    }

    printf("[BATTERY] Status:%d, Vol:%.2f V\n", battery_info.charge_status, battery_info.voltage);

    // 3. 电流和温度 (如果没有专用电量计芯片，通常只能给固定值或估算值)
    battery_info.current = 0.0f; 
    battery_info.temperature = 25.0f; 
}

/* 初始化函数 */
void battery_charge_init(void)
{
    battery_info.charge_start = my_charge_start;
    battery_info.charge_stop = my_charge_stop;
    
    // 初始更新一次
    battery_charge_update(NULL);

    // 注册定时器，每1000ms更新一次
    sys_timer_add(NULL, battery_charge_update, 1000);
}

/* 获取电池信息指针 */
Battery_Charge *get_battery_info(void)
{
    return &battery_info;
}
