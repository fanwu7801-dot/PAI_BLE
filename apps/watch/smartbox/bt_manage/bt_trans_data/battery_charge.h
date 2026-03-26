#ifndef BATTERY_CHARGE_H
#define BATTERY_CHARGE_H
#include  "typedef.h"
#include    "system/includes.h"
typedef struct Barrery_Charge {
    float voltage;        // 电池电压
    float current;        // 充电电流
    float temperature;    // 电池温度
    u8    charge_status;  // 充电状态

    void (*charge_start)(void);    // 充电开始
    void (*charge_stop)(void);     // 充电停止
} Battery_Charge;

void battery_charge_init(void);
Battery_Charge *get_battery_info(void);

#endif // 