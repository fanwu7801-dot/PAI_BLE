#include "app_config.h"
#include "os/os_api.h"
#include "asm/clock.h"
#include "system/timer.h"
#include "asm/cpu.h"
#include "generic/typedef.h"
#include "generic/gpio.h"
#include "debug.h"
#include "hrs3603.h"
#include "hrSensor_manage.h"
#include "system/os/os_api.h"
#include "printf.h"

#if TCFG_P11HR_VC_ALGO_ENABLE
#include "vcHr_module.h"
#include "user_p11_cmd.h"
#define LOG(fmt,...)   //printf("[vcHr] %s " fmt "\n",__func__, ##__VA_ARGS__)

#define VC_HR_ODR      (25)
#define GsensorEn      (1)

static AlgoInputData_t  algoInputData;
static AlgoOutputData_t algoOutputData;
static s16 input_accel[60][3];
static vcHrData_t vcHrData;

static hrsensor_seting_info  seting_info;
static hrsensor_algo_info    algo_info;

u8 vcHr_module_Initialize_variables(hrsensor_seting_info  *seting)
{
    if (seting != NULL) {
        seting_info.sport_mode = seting->sport_mode;
        seting_info.manual_measurement = seting->manual_measurement;
    }

    algo_info.hr_value   = 0;
    algo_info.spo2_value = 0;
    algo_info.wear       = 1;
}

u8 vcHr_module_hrs_init(hrsensor_seting_info  *seting)
{
    Algo_Init();
    vcHr_module_Initialize_variables(seting);
}

u8 vcHr_module_spo2_init(hrsensor_seting_info  *seting)
{
    spo2AlgoInit();
    vcSportMotionAlgoInit();
    vcHr_module_Initialize_variables(seting);
}

void vcHr_module_accel_alignment(vcHrData_t *vcdata)
{
    if (vcdata->accelLength > 0 && vcdata->accelLength < vcdata->ppgLength) {
        u8 m = vcdata->accelLength - 1;
        for (u8 i = vcdata->accelLength; i < vcdata->ppgLength; i++) {
            vcdata->accel_buf[i] = vcdata->accel_buf[m];
        }
    }
}

u8 vcHr_module_hrs_calculate(vcHrData_t *vcdata)
{
    LOG("accLen=%d ppgLen=%d", vcdata->accelLength, vcdata->ppgLength);

    vcHr_module_accel_alignment(vcdata);
    algoInputData.envSample = vcdata->envValue[0];
    for (u16 i = 0; i < vcdata->ppgLength; i++) {
        algoInputData.ppgSample = vcdata->ppgValue[i];
        algoInputData.axes.x    = vcdata->accel_buf[i].x / 4; //The direction vertical with ARM.
        algoInputData.axes.y    = vcdata->accel_buf[i].y / 4; //The direction parallel with ARM.
        algoInputData.axes.z    = vcdata->accel_buf[i].z / 4; //The direction upside.
        Algo_Input(&algoInputData, 1000 / VC_HR_ODR, seting_info.sport_mode, 0, 0);
        LOG("no%02d\tppg=%d\tacc=%d %d %d", i + 1, algoInputData.ppgSample, algoInputData.axes.x, algoInputData.axes.y, algoInputData.axes.z);
    }

    Algo_Output(&algoOutputData);
    if (algoOutputData.hrData == -1) {
        Algo_Init();
        algo_info.wear = 0;
        algo_info.hr_value = 0;
    } else {
        algo_info.hr_value = algoOutputData.hrData;
    }
    LOG("out\t%dbpm", algoOutputData.hrData);
    return algo_info.hr_value;
}

u8 vcHr_module_spo2_calculate(vcHrData_t *vcdata)
{
    u8 vcSportFlag = 0;

    vcdata->ppgLength /= 2;
    LOG("accLen=%d ppgLen=%d", vcdata->accelLength, vcdata->ppgLength);

    vcHr_module_accel_alignment(vcdata);
    for (u16 i = 0; i < vcdata->ppgLength; i++) {
        s32 vcIrPPG     = vcdata->ppgValue[i * 2];
        s32 vcRedPPG    = vcdata->ppgValue[i * 2 + 1];
        s32 vcSpo2Value = spo2Algo(vcRedPPG, vcIrPPG, 0);
        s16 accX = vcdata->accel_buf[i].x >> 2;
        s16 accY = vcdata->accel_buf[i].y >> 2;
        s16 accZ = vcdata->accel_buf[i].z >> 2;
#if GsensorEn
        vcSportFlag = vcSportMotionCalculate(accX, accY, accZ);
#endif
        if ((!vcSportFlag) && (vcSpo2Value > 0)) {
            algo_info.spo2_value = vcSpo2Value;
        }
        LOG("no%02d\tppg=%d %d\tacc=%d %d %d", i + 1, vcIrPPG, vcRedPPG, accX, accY, accZ);
    }
    LOG("out\t%d%", algo_info.spo2_value);
    return algo_info.spo2_value;
}


static void vcHr_module_data_process(void)
{
    int cbuf_len = master_p11cbuf_get_data_len(1);
    LOG("cbuf_len =%d", cbuf_len);

    while (cbuf_len >= sizeof(vcHrData_t)) {

        master_p11cbuf_read(1, &vcHrData, sizeof(vcHrData_t));
        cbuf_len = master_p11cbuf_get_data_len(1);

        algo_info.wear = vcHrData.wearState;

        switch (vcHrData.workMode) {
        case VCWORK_MODE_HRWORK:
            if (vcHrData.ppgLength > 0) {
                vcHr_module_hrs_calculate(&vcHrData);
            }
            break;
        case VCWORK_MODE_SPO2WORK:
            if (vcHrData.ppgLength > 0) {
                vcHr_module_spo2_calculate(&vcHrData);
            }
            break;
        case VCWORK_MODE_CROSSTALKTEST:
            LOG("CROSSTALKTEST: %d %d", vcHrData.maxLedCur, vcHrData.preValue[0]);
            break;

        default:
            break;
        }
    }
}

static u8 vcHr_module_init(void)
{
    return 1;
}

static int vcHr_module_io_ctl(u8 cmd, void *arg)
{
    // LOG("cmd %d",cmd);
    switch (cmd) {
    case HR_SENSOR_ENABLE:
        vcHr_module_hrs_init((hrsensor_seting_info *)arg);
        user_main_post_to_p11_system(P11_SYS_HRSENSOR_START, 0, NULL, 0);
        break;
    case HR_SENSOR_DISABLE:
        user_main_post_to_p11_system(P11_SYS_HRSENSOR_STOP, 0, NULL, 0);
        break;
    case SPO2_SENSOR_ENABLE:
        vcHr_module_spo2_init((hrsensor_seting_info *)arg);
        user_main_post_to_p11_system(P11_SYS_SPO2_START, 0, NULL, 0);
        break;
    case SPO2_SENSOR_DISABLE:
        user_main_post_to_p11_system(P11_SYS_SPO2_STOP, 0, NULL, 0);
        break;
    case HR_SENSOR_READ_DATA:
    case SPO2_SENSOR_READ_DATA:
        vcHr_module_data_process();
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
    .logo = "p11vcHr",
    .heart_rate_sensor_init  = vcHr_module_init,
    .heart_rate_sensor_check = NULL,
    .heart_rate_sensor_ctl   = vcHr_module_io_ctl,
};

#endif
