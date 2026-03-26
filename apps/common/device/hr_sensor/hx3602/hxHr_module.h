
#pragma once
#include "includes.h"
#include "hrSensor_manage.h"
#include "../gSensor/gSensor_manage.h"

typedef enum {
    MODE_POWER_OFF     = 0,
    MODE_LPDETECTION   = 1,
    MODE_HRWORK        = 2,
    MODE_SPO2WORK      = 3,
} hrs3602_mode_t;

#define PPG_BUF_MAX         (25)
#define ACCLE_BUF_MAX       (25)

typedef struct {
    u8  ppg_point;
    u8  acc_point;
    u8  wear_status;
    u8  work_mode;
    axis_info_t acc_buff[ACCLE_BUF_MAX];
    s32 ppg_buff[PPG_BUF_MAX];
} hrs3602_raw_t;
