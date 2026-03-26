#include "app_config.h"
#include "hxHr_module.h"
#include "tyhx_hrs_alg.h"
#include "user_p11_cmd.h"

#if TCFG_P11HR_HX3602_ALGO_ENABLE

#define LOG(fmt,...)   printf("[vcHr] %s " fmt "\n",__func__, ##__VA_ARGS__)

#define VC_HR_ODR      (25)
#define GsensorEn      (1)


static hrs3602_raw_t hrs3602;

static hrsensor_seting_info  seting_info;
static hrsensor_algo_info    algo_info;

u8 hxHr_module_Initialize_variables(hrsensor_seting_info  *seting)
{
    if (seting != NULL) {
        seting_info.sport_mode = seting->sport_mode;
        seting_info.manual_measurement = seting->manual_measurement;
    }

    algo_info.hr_value   = 0;
    algo_info.spo2_value = 0;
    algo_info.wear       = 1;
}

u8 hxHr_module_hrs_init(hrsensor_seting_info  *seting)
{
    tyhx_hrs_alg_open();
    tyhx_hrs_alg_open_deep();
    hxHr_module_Initialize_variables(seting);
}


void hxHr_module_accel_alignment(hrs3602_raw_t *data)
{
    if (data->acc_point > 0 && data->acc_point < data->ppg_point) {
        u8 m = data->acc_point - 1;
        for (u8 i = data->acc_point; i < data->ppg_point; i++) {
            data->acc_buff[i] = data->acc_buff[m];
        }
    }
}

u8 hxHr_module_hrs_calculate(hrs3602_raw_t *data)
{
    s32 ppg_g, acc_x, acc_y, acc_z;
    hrs_results_t  alg_results = {MSG_HRS_ALG_NOT_OPEN, 0, 0, 0, 0};

    LOG("accLen=%d ppgLen=%d", data->acc_point, data->ppg_point);
    hxHr_module_accel_alignment(data);

    for (u16 i = 0; i < data->ppg_point; i++) {
        ppg_g = data->ppg_buff[i];
        acc_x = data->acc_buff[i].x;
        acc_y = data->acc_buff[i].y;
        acc_z = data->acc_buff[i].z;

        tyhx_hrs_alg_send_data(&ppg_g, 1, &acc_x, &acc_y, &acc_z);
        alg_results = tyhx_hrs_alg_get_results();
        LOG("no%02d\tppg=%d\tacc=%d %d %d", i + 1, ppg_g, acc_x, acc_y, acc_z);
    }
    LOG("out\t%dbpm", alg_results.hr_result);
    algo_info.hr_value = alg_results.hr_result;
    return algo_info.hr_value;
}



static void hxHr_module_data_process(void)
{
    int cbuf_len = master_p11cbuf_get_data_len(1);
    LOG("cbuf_len =%d", cbuf_len);

    while (cbuf_len >= sizeof(hrs3602_raw_t)) {

        master_p11cbuf_read(1, &hrs3602, sizeof(hrs3602_raw_t));
        cbuf_len = master_p11cbuf_get_data_len(1);

        algo_info.wear = hrs3602.wear_status;

        switch (hrs3602.work_mode) {
        case MODE_HRWORK:
            if (hrs3602.ppg_point > 0) {
                hxHr_module_hrs_calculate(&hrs3602);
            }
            break;
        default:
            break;
        }
    }
}

static u8 hxHr_module_init(void)
{
    return 1;
}

static int hxHr_module_io_ctl(u8 cmd, void *arg)
{
    LOG("cmd %d", cmd);
    switch (cmd) {
    case HR_SENSOR_ENABLE:
        hxHr_module_hrs_init((hrsensor_seting_info *)arg);
        user_main_post_to_p11_system(P11_SYS_HRSENSOR_START, 0, NULL, 0);
        break;
    case HR_SENSOR_DISABLE:
        user_main_post_to_p11_system(P11_SYS_HRSENSOR_STOP, 0, NULL, 0);
        break;

    case HR_SENSOR_READ_DATA:
    case SPO2_SENSOR_READ_DATA:
        hxHr_module_data_process();
        memcpy(arg, &algo_info, sizeof(hrsensor_algo_info));
        break;
    case HR_SENSOR_LP_DETECTION:
        user_main_post_to_p11_system(P11_SYS_HRSENSOR_LP_DETECT, 0, NULL, 0);
        break;
    case HR_SENSOR_FACTORY_TEST:
        user_main_post_to_p11_system(P11_SYS_HRSENSOR_FACTORY_TEST, 0, NULL, 0);
        break;
    case HR_SEARCH_SENSOR:
        int valid = 0;
        user_main_post_to_p11_system(P11_SYS_HRSENSOR_SEARCH, sizeof(int), &valid, 1);
        memcpy(arg, &valid, 1);//有效值值判断0/1，如需获取id号可以拓展到4
        break;
    default:
        LOG("cmd:%d no support!");
        break;
    }
    return 0;
}


REGISTER_HR_SENSOR(hrSensor) = {
    .logo = "p11hxHr",
    .heart_rate_sensor_init  = hxHr_module_init,
    .heart_rate_sensor_check = NULL,
    .heart_rate_sensor_ctl   = hxHr_module_io_ctl,
};

#endif
