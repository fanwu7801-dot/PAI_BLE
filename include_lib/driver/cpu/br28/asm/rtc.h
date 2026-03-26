
#ifndef __RTC_H__
#define __RTC_H__

#include "typedef.h"
#include "system/sys_time.h"
#include "device/rtc_calculate.h"


struct rtc_dev_platform_data {
    const struct sys_time *default_sys_time;
    const struct sys_time *default_alarm;
    void (*cbfun)(u8);
    u8 clk_sel;
    u8 x32xs;
    u8 wkup_1hz_en;
    u8 wkup_2hz_en;
    u8 wkup_64hz_en;
    u8 wkup_256hz_en;
    void (*wkup_1hz_handle)(void);
    void (*wkup_2hz_handle)(void);
    void (*wkup_64hz_handle)(void);
    void (*wkup_256hz_handle)(void);
};

#define RTC_DEV_PLATFORM_DATA_BEGIN(data) \
	const struct rtc_dev_platform_data data = {

#define RTC_DEV_PLATFORM_DATA_END()  \
    .x32xs = 0, \
};

extern const struct device_operations rtc_dev_ops;

int rtc_init(const struct rtc_dev_platform_data *arg);
int rtc_ioctl(u32 cmd, u32 arg);
void set_alarm_ctrl(u8 set_alarm);
void write_sys_time(struct sys_time *curr_time);
void read_sys_time(struct sys_time *curr_time);
void write_alarm(struct sys_time *alarm_time);
void read_alarm(struct sys_time *alarm_time);

struct _rtc_trim {
    u32 rtc_drift;				/*rtc_time偏移的秒数, rtc_time+rtc_drift=real_time*/
    u32 actual_time;			/*偏移时间中间变量: trim时间内的偏移量*/
    u32 remain;
};
void get_lrc_rtc_trim(struct _rtc_trim *lrc_trim);
bool read_p11_sys_time(struct sys_time *t, struct _rtc_trim *lrc_trim);
int write_p11_sys_time(int param);



#endif // __RTC_API_H__
