#include "hrSensor_manage.h"
#include "hrs3300.h"
#include "asm/iic_hw.h"
#include "asm/iic_soft.h"
#include "watch_common.h"



static const struct hrsensor_platform_data *platform_data;
static hrsensor_algo_info   hrsensor_info;
HR_SENSOR_INTERFACE *hrSensor_hdl = NULL;
HR_SENSOR_INFO  __hrsensor_info = {.iic_delay = 0};
#define hrSensor_info (&__hrsensor_info)

#if TCFG_HR_SENOR_USER_IIC_TYPE
#define iic_init(iic)                       hw_iic_init(iic)
#define iic_uninit(iic)                     hw_iic_uninit(iic)
#define iic_start(iic)                      hw_iic_start(iic)
#define iic_stop(iic)                       hw_iic_stop(iic)
#define iic_tx_byte(iic, byte)              hw_iic_tx_byte(iic, byte)
#define iic_rx_byte(iic, ack)               hw_iic_rx_byte(iic, ack)
#define iic_read_buf(iic, buf, len)         hw_iic_read_buf(iic, buf, len)
#define iic_write_buf(iic, buf, len)        hw_iic_write_buf(iic, buf, len)
#define iic_suspend(iic)                    hw_iic_suspend(iic)
#define iic_resume(iic)                     hw_iic_resume(iic)
#else
#define iic_init(iic)                       soft_iic_init(iic)
#define iic_uninit(iic)                     soft_iic_uninit(iic)
#define iic_start(iic)                      soft_iic_start(iic)
#define iic_stop(iic)                       soft_iic_stop(iic)
#define iic_tx_byte(iic, byte)              soft_iic_tx_byte(iic, byte)
#define iic_rx_byte(iic, ack)               soft_iic_rx_byte(iic, ack)
#define iic_read_buf(iic, buf, len)         soft_iic_read_buf(iic, buf, len)
#define iic_write_buf(iic, buf, len)        soft_iic_write_buf(iic, buf, len)
#define iic_suspend(iic)                    soft_iic_suspend(iic)
#define iic_resume(iic)                     soft_iic_resume(iic)
#endif

u8 hrsensor_write_nbyte(u8 w_chip_id, u8 register_address, u8 *buf, u8 data_len)
{
    u8 write_len = 0;
    u8 i;

    iic_start(hrSensor_info->iic_hdl);
    if (0 == iic_tx_byte(hrSensor_info->iic_hdl, w_chip_id)) {
        write_len = 0;
        r_printf("\n hrSensor iic w err 0\n");
        goto __wend;
    }

    delay(hrSensor_info->iic_delay);

    if (0 == iic_tx_byte(hrSensor_info->iic_hdl, register_address)) {
        write_len = 0;
        r_printf("\n hrSensor iic w err 1\n");
        goto __wend;
    }

    for (i = 0; i < data_len; i++) {
        delay(hrSensor_info->iic_delay);
        if (0 == iic_tx_byte(hrSensor_info->iic_hdl, buf[i])) {
            write_len = 0;
            r_printf("\n hrSensor iic w err 2\n");
            goto __wend;
        }
        write_len++;
    }

__wend:
    iic_stop(hrSensor_info->iic_hdl);
    return write_len;
}

u8 hrsensor_read_nbyte(u8 r_chip_id, u8 register_address, u8 *buf, u8 data_len)
{
    u8 read_len = 0;

    iic_start(hrSensor_info->iic_hdl);
    if (0 == iic_tx_byte(hrSensor_info->iic_hdl, r_chip_id - 1)) {
        r_printf("\n hrSensor iic r err 0\n");
        read_len = 0;
        goto __rend;
    }

    delay(hrSensor_info->iic_delay);
    if (0 == iic_tx_byte(hrSensor_info->iic_hdl, register_address)) {
        r_printf("\n hrSensor iic r err 1\n");
        read_len = 0;
        goto __rend;
    }

    iic_start(hrSensor_info->iic_hdl);
    if (0 == iic_tx_byte(hrSensor_info->iic_hdl, r_chip_id)) {
        r_printf("\n hrSensor iic r err 2\n");
        read_len = 0;
        goto __rend;
    }

    delay(hrSensor_info->iic_delay);

    for (; data_len > 1; data_len--) {
        *buf++ = iic_rx_byte(hrSensor_info->iic_hdl, 1);
        read_len ++;
    }

    *buf = iic_rx_byte(hrSensor_info->iic_hdl, 0);
    read_len ++;

__rend:
    iic_stop(hrSensor_info->iic_hdl);
    delay(hrSensor_info->iic_delay);

    return read_len;
}

int hr_sensor_io_ctl(u8 cmd, void *arg)
{
    if ((!hrSensor_hdl) || (!hrSensor_hdl->heart_rate_sensor_ctl)) {
        return -1;
    }
    return hrSensor_hdl->heart_rate_sensor_ctl(cmd, arg);
}

int hr_sensor_init(void *_data)
{
    int retval = 0;
    platform_data = (const struct hrsensor_platform_data *)_data;

    hrSensor_info->iic_hdl = platform_data->iic;
    retval = iic_init(hrSensor_info->iic_hdl);
    if (retval < 0) {
        g_printf("\n  open iic for hrSensor err\n");
        return retval;
    } else {
        g_printf("\n iic open succ\n");
    }

    retval = -EINVAL;
    list_for_each_hrsensor(hrSensor_hdl) {
        printf("%s==%s", hrSensor_hdl->logo, platform_data->hrSensor_name);
        if (!memcmp(hrSensor_hdl->logo, platform_data->hrSensor_name, strlen(platform_data->hrSensor_name))) {
            retval = 0;
            break;
        }
    }

    if (retval < 0) {
        log_e(">>>hrSensor_hdl logo err\n");
        return retval;
    }

    if (hrSensor_hdl->heart_rate_sensor_init()) {
        g_printf(">>>>hrSensor_Init SUCC");
        hrSensor_info->init_flag = 1;
    } else {
        g_printf(">>>>hrSensor_Init ERROR");
    }

    return 0;
}


u8 hr_sensor_measure_hr_start(u8 manual, u8 sport)
{
    hrsensor_seting_info seting = {manual, sport};
    return hr_sensor_io_ctl(HR_SENSOR_ENABLE, &seting);
}

u8 hr_sensor_measure_hr_stop(void)
{
    return hr_sensor_io_ctl(HR_SENSOR_DISABLE, NULL);
}


u8 hr_sensor_measure_spo2_start(u8 manual, u8 sport)
{
    hrsensor_seting_info seting = {manual, sport};
    return hr_sensor_io_ctl(SPO2_SENSOR_ENABLE, &seting);
}

u8 hr_sensor_measure_spo2_stop(void)
{
    return hr_sensor_io_ctl(SPO2_SENSOR_DISABLE, NULL);
}

extern struct watch_SPO2_data wspo2;
extern struct sensor_heart_rate whr;
u8 hr_sensor_data_process(void)
{
    static u8 wear = 0;

    hr_sensor_io_ctl(HR_SENSOR_READ_DATA, &hrsensor_info);

    whr.HR       = hrsensor_info.hr_value;
    wspo2.SPO2   = hrsensor_info.spo2_value;
    wspo2.out[0] = hrsensor_info.wear;
    whr.out[0]   = hrsensor_info.wear;

    if (wear != hrsensor_info.wear) {
        wear = hrsensor_info.wear;
        printf("watch wear: %d", wear);

        if (wear == 0) {
            //在UI 界面应提示未佩戴，并使sensor进入低功耗检测模式或者掉电模式
            whr.read_status = 0;
            wspo2.read_status = 0;
        }
    }
    return 0;
}

