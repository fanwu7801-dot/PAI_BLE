#ifndef VCHR_MODULE
#define VCHR_MODULE
#include "includes.h"
#include "algo.h"
#include "vcHr02Hci.h"
#include "hrSensor_manage.h"
#include "gSensor/gSensor_manage.h"

#define ACCLE_BUF_MAX      (25*2)
typedef struct {
    u8  workMode;
    u8  wearState;
    u8  maxLedCur;
    u8  psValue;	      //PS Sample value.
    u8  preValue[2];   //Environment Sample value.
    u8  envValue[3];	   //Environment Sample value.
    u8  ppgLength;
    u8  accelLength;
    u16 ppgValue[ACCLE_BUF_MAX * 2];	//PPG sample value.
    axis_info_t accel_buf[ACCLE_BUF_MAX];
} vcHrData_t;
#endif
