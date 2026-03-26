#include "app_config.h"
#include "os/os_api.h"
#include "asm/clock.h"
#include "system/timer.h"
#include "asm/cpu.h"
#include "typedef.h"
#include "generic/typedef.h"
#include "generic/gpio.h"
#include "debug.h"
#include "hrSensor_manage.h"
#include "system/os/os_api.h"
#include "printf.h"
#include <stdint.h>
#include <stdlib.h>


/*********************************************************************************************************************
*               Copyright(c) 2020, Vcare Corporation. All rights reserved.
**********************************************************************************************************************
* @file     module_heart_vc.c
* @brief    Application example.
* @details
              The resources needed to use this chip include an RTC or a general timer, an external interrupt line,
			and an I2C interface.
			  First of all, confirm the count frequency of the RTC or the general timer, assign it to 'mcuOscData',
			if the count frequency is 30kHz, mcuOscData = 30000; then confirm the frequency of the I2C clock, assign
			it to 'mcuI2cClock', if the frequency of the I2C clock is 400kHz, mcuI2cClock = 400.
			  Add the code to get the RTC or the general timer count value in function 'vcHr02GetRtcCountFromMCU()',
			add I2C read and write function code in 'vcHr02ReadRegisters(uint8_t startAddress, uint8_t *pRegisters,
			uint8_t len)' and 'vcHr02ReadRegisters(uint8_t startAddress, uint8_t *pRegisters, uint8_t len)', but do
			not change the name of the function.
			   How To Use The Chip: Call the program to initialize the peripherals of the main control chip first,
			these peripherals include I2C, an external interrupt, RTC or general timer. Then the chip ID can be read
			to verify whether the chip is working properly or whether I2C communication is normal, the register add-
			ress is 0x00, if your chip is VC31B/VC32S/VC51B/VC52S, the chip ID is 0x21, but also need to verify whe-
			ther your I2C supports continuous multi byte read and write, because the driver code involves continuous
			multi byte read and write. After initializing the peripherals, call the chip initialization function
			'vcHr02Init(&vcHr02,vcMode)', the parameter 'vcHr02' is a defined heart rate data structure, 'vcMode'
			is also defined variable which can be used to switch the chip working mode. After the initialization of
			the chip is completed, the peripheral lights will be on, and the lights will be dimmed when the fingers
			are close to the chip from far to near, the interrupt pin of the chip will have interrupt level.
			  About the function 'vcHr02_process(AlgoSportMode vcSportMode)': It can be used as an interrupt serv-
			ice function or as an independent task, which is added to the task queue by the interrupt trigger, but
			we strongly recommend the second one. The parameter 'vcSportMode' is a defined variable which can be us-
			ed to switch the sport mode in heart rate mode.
*---------------------------------------------------------------------------------------------------------------------
*---------------------------------------------------------------------------------------------------------------------
		   使用芯片需要的外设资源包括一个实时时钟源或者一个通用定时器(计数频率不得低于10K，最佳是在30K左右)，一个外部
		中断，一路I2C。
		   首先，确认实时时钟源或者通用定时器的计数频率，并将这个频率赋值给变量‘mcuOscData’，如果计数频率是30KHz，则
		mcuOscData = 30000；然后确认I2C的时钟频率，并将这个频率赋值给变量‘mcuI2cClock’，如果I2C的时钟频率是400KHz，则
		mcuI2cClock = 400。
		   在函数‘vcHr02GetRtcCountFromMCU()'’里添加获取实时时钟源或通用定时器的计数值的代码，注意不是返回差值（与VC31
		和VC51不同），在函数‘vcHr02ReadRegisters(uint8_t startAddress, uint8_t *pRegisters, uint8_t len)’添加I2C的读函
		数的代码，在函数‘vcHr02ReadRegisters(uint8_t startAddress, uint8_t *pRegisters, uint8_t len)’添加I2C的写函数的
		代码，但不要改变函数名称。读函数中三个变量的意义分别是：寄存器地址、指向寄存器内容存储位置的指针（存寄存器里读
		回来的数据的变量地址）、读的长度，写函数中三个变量的意义分别是：寄存器地址、指向要写入寄存器的内容的指针、写的
		长度。
		   芯片使用：先调用初始化主控芯片外设的代码，初始化实时时钟源或通用定时器，I2C，外部中断，然后可以通过读芯片的
		ID来判断芯片是否正常工作或者I2C通讯是否正常，寄存器地址是0x00，如果使用的芯片是VC31B/VC32S/VC51B/VC52S，芯片的
		ID是0x21，但要确认I2C是否支持连续读写多个字节，因为驱动里涉及连续读写多个字节的操作。外设初始化完成之后，调用
		‘vcHr02Init(&vcHr02,vcMode)’来初始化芯片，变量‘vcHr02’是已经定义的心率数据结构体，变量‘vcMode’也是已经定义的，
		可以通过改变这个变量的值，切换芯片的工作模式，初始化芯片之后，外围的等会亮，并且手指由远到近靠近时，灯会由亮到暗
		调整，芯片的中断引脚也会有中断电平。
		   关于‘vcHr02_process(AlgoSportMode vcSportMode)’函数：可以作为中断服务函数，也可以作为独立任务，通过中断触发
		将它添加到任务队列，但我们强烈推荐第二种方式。‘vcSportMode’是一个已经定义的在心率模式下切换运动模式的变量。

       *若用户是用定时读取方式读取Gsensor数据，用于计步等功能，为了更好的动态心率性能，我们推荐按照我们的中断的时间间隔，
	   在‘vcHr02_process(AlgoSportMode vcSportMode)’函数中对应位置，读取Gsensor数据，中断的时间间隔（佩戴状态下）可以
	   根据FIFO的长度可调（40ms整数倍）。如果用户心率非24小时连续测量方式，即进入心率测量功能界面测量心率，退出界面停止
	   心率测量或定时唤醒心率芯片测量心率方式，则调用‘vcHr02StopSample(vcHr02_t *pvcHr02)’函数让芯片进入休眠时，可以重新
	   使能定时器，定时读取Gsensor数据，因为调用上述函数使心率芯片休眠时，中断脚将不会有中断电平，‘vcHr02_process(AlgoS-
	   portMode_t vcSportMode)’函数将不会执行，如果用户是24小时连续心率测量，即不会休眠，可以忽略。若用户是按照Gsensor中
	   断触发读取Gsensor数据的方式，请将Gsensor读取的数据保存到buffer中，Gsensor每读一次数据就将buffer中的上一次数据用这
	   一次的数据覆盖，在‘vcHr02_process(AlgoSportMode vcSportMode)’函数中传入算法函数中。

* @author
* @date
* @version  V1.9.7
**********************************************************************************************************************
*/
#include "vcHr02Hci.h"
#include "vcSportMotionAlgo.h"
/* Include your INT,I2C,Timer header file */
#include "asm/iic_hw.h"
#include "asm/iic_soft.h"
#include "system/includes.h"
#include "media/includes.h"
#include "asm/timer.h"

#undef LOG_TAG_CONST
#define LOG_TAG     "[vchr02]"
#define LOG_ERROR_ENABLE
#define LOG_INFO_ENABLE
#include "debug.h"

extern spinlock_t sensor_iic;
void delay(volatile u32 t);
void udelay(u32 us);
#define   MDELAY(n)    mdelay(n)

/* 如果Gsensor数据读取代码已经添加，请将这个宏置1 */
#define GsensorEn 0

/* Heart rate value */
int HeartRateValue = 0;

//*******************************************************************************//
//定时器定时频率及变量mcuOscData iic频率及变量mcuI2cClock iic操作不可关定时器中断//
//*******************************************************************************//
/* Timer clock frequency
   Be used to adjust INT frequency */
const uint32_t mcuOscData = 30000;

/* MCU I2C clock frequency */
/* const uint16_t mcuI2cClock = 400; */
const uint16_t mcuI2cClock = 190;

/* Heart rate sample frequency */
const uint8_t vcHr02SampleRate = 25;

/* INT Flag */
bool vcHr02IRQFlag = 0;

#if VCHR02_SPO2_EN
/* Spo2 value */
float vcSpo2Value = 0.0;
int real_spo = 0;

/* Spo2 data */
float vcRedPPG = 0.0;
float vcIrPPG = 0.0;
#endif

uint16_t GsensorLength = 0;

/* G-Sensor Data */
int16_t xData[40] = {0};
int16_t yData[40] = {0};
int16_t zData[40] = {0};

/* The algorithm of hert rate data struct */
AlgoInputData_t algoInputData;
AlgoOutputData_t algoOutputData;

/* Heart rate data struct */
vcHr02_t vcHr02;

/* Heart rate mode */
vcHr02Mode_t vcMode = VCWORK_MODE_HRWORK;

/* Sport Mode In Heart Rate Mode */
AlgoSportMode vcSportMode = SPORT_TYPE_NORMAL;

/*
 * @brief  Initialize the chip working mode and set some parameters.
 *
 *
 **/
void vcHr02Init(vcHr02_t *pVcHr02, vcHr02Mode_t vcHr02WorkMode)
{
    vcHr02StopSample(pVcHr02);
    vcHr02SoftReset(pVcHr02);
    /* vcSportMotionAlgoTerminate(); */
    pVcHr02->vcSampleRate = vcHr02SampleRate;
    pVcHr02->mcuOscValue = mcuOscData;
    pVcHr02->mcuSclRate = mcuI2cClock;
    pVcHr02->workMode = vcHr02WorkMode;
    /* HeartRate Mode */
    if (pVcHr02->workMode == VCWORK_MODE_HRWORK) {
        vcHr02StartSample(pVcHr02);
        Algo_Init();
    }
#if VCHR02_SPO2_EN
    /* Spo2 Mode */
    else if (pVcHr02->workMode == VCWORK_MODE_SPO2WORK) {
        vcHr02StartSample(pVcHr02);
#if GsensorEn
        vcSportMotionAlgoInit();//need gsensor
#endif
        /* vcSpo2AlgoInit(); */
        spo2AlgoInit();
    }
#endif
    /* Light Crosstalk Test Mode */
    else if (pVcHr02->workMode == VCWORK_MODE_LPDETECTION) {
        vcHr02StartSample(pVcHr02);
        Algo_Init();
    }
    /* Light Crosstalk Test Mode */
    else if (pVcHr02->workMode == VCWORK_MODE_CROSSTALKTEST) {
        vcHr02StartSample(pVcHr02);
    }
}


/*
 * @brief  The processing flow under different working modes can be used as
 *         interrupt service function or as an independent task, but it is
 *         recommended to be treated as an independent task.
 *
 **/
void vcHr02_process(AlgoSportMode vcSportMode)
{
    uint8_t algoCallNum = 0;
    uint8_t ppgLength = 0;
    uint8_t vcSportFlag = 0;

    if (vcHr02IRQFlag) {
        vcHr02IRQFlag = false;

        if (vcHr02.workMode == VCWORK_MODE_HRWORK) {
            if (VCHR02RET_UNWEARTOISWEAR == vcHr02GetSampleValues(&vcHr02, &ppgLength)) {
                Algo_Init();
            }

            /* printf("ppgLength:%d\n", ppgLength); */
            if (vcHr02.vcFifoReadFlag || vcHr02.vcPsFlag) {
#if GsensorEn

                /*
                Gsensor数据处理示例:
                1.此处Gsensor采样率为36Hz，定时读取方式；
                2.800mS（心率芯片FIFO长度为20,20×40ms）中断间隔时，定时方式读取Gsensor数据
                  长度众数（800ms读一次Gsensor数据，一段时间内读到29个数据的次数最多）是29；
                3.心率芯片的FIFO长度是20，所以也只要20组Gsensor数据，心率芯片FIFO长度为多少
                  就需要多少组Gsensor数据；

                */
                /* 在下一行添加读取Gsensor数据的代码 */
                /* ReadGsensorFIFO(+-4G11bit or +-8G12bit 256/g) */

                if (GsensorLength >= 29) {
                    GsensorLength = 29;
                } else if (GsensorLength >= 1) {
                    //小于29个数据补足29个；
                    for (uint8_t i = GsensorLength; i < 29; i++) {
                        xData[i] = xData[GsensorLength - 1];
                        yData[i] = yData[GsensorLength - 1];
                        zData[i] = zData[GsensorLength - 1];
                    }
                }

                for (uint8_t i = 0; i < 20; i++) {
                    //cash_num[20]数组为抽样数组；此处将29个数据抽成20个，匹配800mS中断的20个PPG
                    xData[i] = yData[cash_num[i]] >> 5;
                    yData[i] = xData[cash_num[i]] >> 5;
                    zData[i] = zData[cash_num[i]] >> 5;
                }
#endif
                if (vcHr02.vcFifoReadFlag) {
                    vcHr02.vcFifoReadFlag = 0;
                    if (vcHr02.wearStatus == VCHR02WEARST_ISWEAR) {
#if GsensorEn
                        if (GsensorLength < ppgLength) { //此处为了防止丢中断，保证每个PPG数据参与算法运算，丢中断的情况下，Gsensor数据个数补足到ppgLength；
                            for (uint8_t i = GsensorLength; i < ppgLength; i++) {
                                xData[i] = xData[GsensorLength - 1];
                                yData[i] = yData[GsensorLength - 1];
                                zData[i] = zData[GsensorLength - 1];
                            }
                        }
#endif
                        algoInputData.envSample = vcHr02.sampleData.envValue[0];
                        for (algoCallNum = 0; algoCallNum < ppgLength; algoCallNum++) {
                            algoInputData.ppgSample = vcHr02.sampleData.ppgValue[algoCallNum];
                            algoInputData.axes.x = xData[algoCallNum];//The direction vertical with ARM.
                            algoInputData.axes.y = yData[algoCallNum];//The direction parallel with ARM.
                            algoInputData.axes.z = zData[algoCallNum];//The direction upside.
                            Algo_Input(&algoInputData, 1000 / vcHr02SampleRate, vcSportMode, 0, 0);
                        }

                        Algo_Output(&algoOutputData);
                        HeartRateValue = algoOutputData.hrData;
                        /* printf("HeartRateValue :%d\n", HeartRateValue); */

                        if (HeartRateValue == -1) {
                            Algo_Init();
                        }
                    }
                } else {
#if SportMotionEn
                    green_led_off_state_gsensor_abs_sum_diff_func(xData[0], yData[0], zData[0]);
#endif
                    HeartRateValue = -2;
                }
            }
        }
#if VCHR02_SPO2_EN
        else if (vcHr02.workMode == VCWORK_MODE_SPO2WORK) {
            if (VCHR02RET_UNWEARTOISWEAR == vcHr02GetSampleValues(&vcHr02, &ppgLength)) {
                /* vcSpo2AlgoInit(); */
                spo2AlgoInit();

#if GsensorEn
                vcSportMotionAlgoInit();//need gsensor
#endif
            }
            /* printf("spo2ppgLength:%d\n", ppgLength); */
            if (vcHr02.vcFifoReadFlag || vcHr02.vcPsFlag) {
#if GsensorEn
                /* ReadGsensorFIFO(+-4G11bit or +-8G12bit 256/g) */
                if (GsensorLength >= 29) {
                    GsensorLength = 29;
                } else if (GsensorLength >= 1) {
                    for (uint8_t i = GsensorLength; i < 29; i++) {
                        xData[i] = xData[GsensorLength - 1];
                        yData[i] = yData[GsensorLength - 1];
                        zData[i] = zData[GsensorLength - 1];
                    }
                }

                for (uint8_t i = 0; i < 20; i++) {
                    xData[i] = yData[cash_num[i]] >> 5;
                    yData[i] = xData[cash_num[i]] >> 5;
                    zData[i] = zData[cash_num[i]] >> 5;
                }
#endif
                if (vcHr02.vcFifoReadFlag) {
                    vcHr02.vcFifoReadFlag = 0;
                    if (ppgLength <= 50) {
                        for (algoCallNum = 0; algoCallNum < ppgLength / 2; algoCallNum++) {
                            vcIrPPG = vcHr02.sampleData.ppgValue[algoCallNum * 2];
                            vcRedPPG = vcHr02.sampleData.ppgValue[algoCallNum * 2 + 1];
                            /* vcSpo2Value = vcSpo2Calculate(vcRedPPG, vcIrPPG); */
                            vcSpo2Value = spo2Algo(vcRedPPG, vcIrPPG, 0);
#if GsensorEn
                            vcSportFlag = vcSportMotionCalculate(xData[algoCallNum], yData[algoCallNum], zData[algoCallNum]);
#endif
                            if ((!vcSportFlag) && (vcSpo2Value > 0)) {
                                real_spo = vcSpo2Value;
                                /* printf("real_spo:%d\n", real_spo); */
                            }
                        }
                    }
                } else {
#if SportMotionEn
                    green_led_off_state_gsensor_abs_sum_diff_func(xData[0], yData[0], zData[0]);
#endif
                }
            }
        }
#endif
        else if (vcHr02.workMode == VCWORK_MODE_LPDETECTION) {
            vcHr02GetSampleValues(&vcHr02, &ppgLength);
        } else if (vcHr02.workMode == VCWORK_MODE_CROSSTALKTEST) {
            vcHr02GetSampleValues(&vcHr02, &ppgLength);
            /* If Pass: */
            if ((vcHr02.sampleData.maxLedCur >= 100) && (vcHr02.sampleData.preValue[0] <= 2)) {
                //PASS：
                //Display the value of vcHr02.sampleData.maxLedCur and vcHr02.sampleData.preValue[0]
            }
        }
    }
}

/****************************iic*****************************/
#if TCFG_VCHR02_USE_IIC_TYPE
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
static struct _vchr02_dev_platform_data *vchr02_iic_info;

static u8 vchr02_write_reg(u8 vchr02_reg_addr, u8 *buf, u8 len)
{
    /* os_mutex_pend(&SENSOR_IIC_MUTEX,0); */
    /* printf("%s",__func__); */
    /* spin_lock(&sensor_iic); */
    iic_start(vchr02_iic_info->iic_hdl);
    if (0 == iic_tx_byte(vchr02_iic_info->iic_hdl, VCHR02_WADDR)) {
        iic_stop(vchr02_iic_info->iic_hdl);
        spin_unlock(&sensor_iic);
        log_e("vchr02 write fail1!\n");
        return false;
    }
    MDELAY(vchr02_iic_info->iic_delay);
    if (0 == iic_tx_byte(vchr02_iic_info->iic_hdl, vchr02_reg_addr)) {
        iic_stop(vchr02_iic_info->iic_hdl);
        spin_unlock(&sensor_iic);
        log_e("vchr02 write fail2!\n");
        return false;
    }
    for (u8 i = 0; i < len; i++) {
        MDELAY(vchr02_iic_info->iic_delay);
        if (0 == iic_tx_byte(vchr02_iic_info->iic_hdl, *(buf + i))) {
            iic_stop(vchr02_iic_info->iic_hdl);
            spin_unlock(&sensor_iic);
            log_e("vchr02 write fail3!\n");
            return false;
        }
    }
    iic_stop(vchr02_iic_info->iic_hdl);
    MDELAY(vchr02_iic_info->iic_delay);
    /* os_mutex_post(&SENSOR_IIC_MUTEX); */
    /* spin_unlock(&sensor_iic); */
    return true;
}
AT(.volatile_ram_code)
static u8 vchr02_read_reg(u8 vchr02_reg_addr, u8 *read_data, u8 len)
{
    /* printf("%s",__func__); */
    /* os_mutex_pend(&SENSOR_IIC_MUTEX,0); */
    /* spin_lock(&sensor_iic); */
    iic_start(vchr02_iic_info->iic_hdl);
    if (0 == iic_tx_byte(vchr02_iic_info->iic_hdl, VCHR02_WADDR)) {
        iic_stop(vchr02_iic_info->iic_hdl);
        spin_unlock(&sensor_iic);
        log_e("vchr02 read fail1!\n");
        return false;
    }
    MDELAY(vchr02_iic_info->iic_delay);
    if (0 == iic_tx_byte(vchr02_iic_info->iic_hdl, vchr02_reg_addr)) {
        iic_stop(vchr02_iic_info->iic_hdl);
        spin_unlock(&sensor_iic);
        log_e("vchr0vchr02 fail2!\n");
        return false;
    }
    MDELAY(vchr02_iic_info->iic_delay);
    iic_start(vchr02_iic_info->iic_hdl);
    if (0 == iic_tx_byte(vchr02_iic_info->iic_hdl, VCHR02_RADDR)) {
        iic_stop(vchr02_iic_info->iic_hdl);
        spin_unlock(&sensor_iic);
        log_e("vchr02 read fail3!\n");
        return false;
    }

    for (u8 i = 0; i < len - 1; i++) {
        read_data[i] = iic_rx_byte(vchr02_iic_info->iic_hdl, 1);
        MDELAY(vchr02_iic_info->iic_delay);
    }
    read_data[len - 1] = iic_rx_byte(vchr02_iic_info->iic_hdl, 0);
    iic_stop(vchr02_iic_info->iic_hdl);
    MDELAY(vchr02_iic_info->iic_delay);
    /* os_mutex_post(&SENSOR_IIC_MUTEX); */
    /* spin_unlock(&sensor_iic); */
    return true;
}

/*
 * @brief  MCU peripherals initialization.
 *
 *
 **/
void vcHr02PhyInit(void)
{
    /*****************************************************************************
    * 1.If Reset Pin of the chip is connected to MCU, initialize the corresponding
    *	pin on the MCU.
    *
    * 2.Initialize the I2C of MCU.
    *
    * 3.Initialize the external interrupt of MCU.
    *
    *
    ******************************************************************************/
    iic_init(vchr02_iic_info->iic_hdl);
    gpio_set_die(VCHR02_INT_IO, 1);
    gpio_set_direction(VCHR02_INT_IO, 1);
    gpio_set_pull_up(VCHR02_INT_IO, 1);

    /* #ifdef CONFIG_CPU_BR23 */
    /*         io_ext_interrupt_init(MPU9250_INT_IO, 1, vcHr02IRQHandler); */
    /* #elif defined(CONFIG_CPU_BR28)||defined(CONFIG_CPU_BR27) */
    /*         // br28外部中断回调函数，按照现在的外部中断注册方式 */
    /*         // io配置在板级，定义在板级头文件，这里只是注册回调函数 */
    /*         port_edge_wkup_set_callback(vcHr02IRQHandler); */
    /* #endif */
}


/*
 * @brief  Interrupt service function.
 *
 *
 **/
void vcHr02IRQHandler(void)
{
    vcHr02IRQFlag = true;
}

u8 vcHr02DevInit(void *priv)
{
    if (priv == NULL) {
        log_error("vchr02 init fail(no param)\n");
        return false;
    }
    vchr02_iic_info = (struct _vchr02_dev_platform_data *)priv;

    vcHr02PhyInit();

    vcHr02Init(&vcHr02, vcMode);

#ifdef CONFIG_CPU_BR23
    io_ext_interrupt_init(VCHR02_INT_IO, 1, vcHr02IRQHandler);
#elif defined(CONFIG_CPU_BR28)
    // br28外部中断回调函数，按照现在的外部中断注册方式
    // io配置在板级，定义在板级头文件，这里只是注册回调函数
    port_edge_wkup_set_callback_by_index(4, vcHr02IRQHandler); // 序号需要和板级配置中的wk_param对应上
#elif defined(CONFIG_CPU_BR27)
    // br28外部中断回调函数，按照现在的外部中断注册方式
    // io配置在板级，定义在板级头文件，这里只是注册回调函数
    port_edge_wkup_set_callback(vcHr02IRQHandler);
#endif
    return true;
}


/*
 * @brief  Register writing.
 *
 *
 **/
vcHr02Ret_t vcHr02WriteRegisters(uint8_t startAddress, uint8_t *pRegisters, uint8_t len)
{
    /**********************************************************************************
    * @param
    *       startAddress:Register address.
    *       pRegisters  :Point to the data to write.
    *       len         :Byte to be transmitted.
    *
    *
    *       7bit I2C own address:0x33
    *       8bit I2C own address:0x66(the least significant bit is the W/R control bit)
    ************************************************************************************/
    vchr02_write_reg(startAddress, pRegisters, len);
    return VCHR02RET_ISOK;
}


/*
 * @brief  Register reading.
 *
 *
 **/
vcHr02Ret_t vcHr02ReadRegisters(uint8_t startAddress, uint8_t *pRegisters, uint8_t len)
{
    /***********************************************************************************
    * @param
    *       startAddress:Register address.
    *       pRegisters  :Point to the location to save.
    *       len         :Byte to be transmitted.
    *
    *
    *       7bit I2C own address:0x33
    *       8bit I2C own address:0x67(the least significant bit is the W/R control bit)
    ************************************************************************************/
    if (vchr02_read_reg(startAddress, pRegisters, len)) {
        /* return VCHR02RET_ISOK; */
    }
    return VCHR02RET_ISOK;
}




/****************************timer3*****************************/
#define timer_x_vchr02 JL_TIMER4  //timer3
#define IRQ_TIMEx_IDX (4+4)       //timer3 IRQ_TIME0_IDX
volatile static u32 vchr02_time_cnt = 1;
___interrupt
static void timerx_isr()
{
    timer_x_vchr02->CON |= BIT(14);
    ++vchr02_time_cnt;
    /* if(vchr02_time_cnt==0)vchr02_time_cnt=1; */
    JL_PORTB->OUT ^= BIT(2);
    /* if (vchr02_time_cnt >= 43) { */
    /*     vchr02_time_cnt = 0; */
    /* } */
}
#define APP_TIMER_CLK           clk_get("timer")
#define MAX_TIME_CNT            0x7fff
#define MIN_TIME_CNT            0x100
#define TIMER_UNIT_MS           1 //1ms起一次中断
int timerx_init()
{
    u32 prd_cnt = 1500;
    u8 index = 2;
    u32 timer_div[] = {
        /*0000*/    1,
        /*0001*/    4,
        /*0010*/    16,
        /*0011*/    64,
        /*0100*/    2,
        /*0101*/    8,
        /*0110*/    32,
        /*0111*/    128,
        /*1000*/    256,
        /*1001*/    4 * 256,
        /*1010*/    16 * 256,
        /*1011*/    64 * 256,
        /*1100*/    2 * 256,
        /*1101*/    8 * 256,
        /*1110*/    32 * 256,
        /*1111*/    128 * 256,
    };

//    printf("%s :%d", __func__, __LINE__);
    /* for (index = 0; index < (sizeof(timer_div) / sizeof(timer_div[0])); index++) { */
    /*     prd_cnt = 40 * TIMER_UNIT_MS * (clk_get("timer") / 1000) / timer_div[index]; */
    /*     if (prd_cnt > MIN_TIME_CNT && prd_cnt < MAX_TIME_CNT) { */
    /*         break; */
    /*     } */
    /* } */
    timer_x_vchr02->CNT = 0;
    /* timer_x_vchr02->PRD = prd_cnt; //1ms*40 */
    timer_x_vchr02->PRD = 800; //24M---30K
    request_irq(IRQ_TIMEx_IDX, 7, timerx_isr, 0);
    /* timer_x_vchr02->CON = (index << 4)  | BIT(0); */
    timer_x_vchr02->CON = (0 << 4);//div=1
    timer_x_vchr02->CON |= (6 << 10);//std24M
    timer_x_vchr02->CON |= BIT(0);
//pwm
    gpio_set_direction(IO_PORTB_02, 0);
    gpio_write(IO_PORTB_02, 0);
    printf("   PRD:0x%x / lsb:%d iosc:%d,div:%d\n", timer_x_vchr02->PRD, clk_get("lsb"), clk_get("timer"), index);
    return 0;
}
void vchr02_timers_start()//定时器轮询模式：开始血氧采集（初始化成功后调用）
{
    timer_x_vchr02->CON |= BIT(0);
}
void vchr02_timers_close()//关闭定时器
{
    timer_x_vchr02->CON &= ~BIT(0);
}


/*
 * @brief  Get the count value of RTC or general timer of MCU.
 *
 *
 **/
uint32_t vcHr02GetRtcCountFromMCU(void)
{
    /* return RTC_GetCounter(); */
    return vchr02_time_cnt;
}



#if 1
/*
 * @brief  main test.
 **/
static struct _vchr02_dev_platform_data vchr02_iic_info_test = {
    .iic_hdl = 0,
    .iic_delay = 0
};

void vchr02_test()
{
    u8 temp = 0;
    /*-----------INSERT BOARD HARDWARE AND SOFTWARE INITIAL FUNCTION------------*/
    /* accInit();          	//Accelerometer initial +-8G 12bit */
    /* vcHr02PhyInit();      	//vcHr02 board initial */
    /*     vcHr02Init(&vcHr02,vcMode); */
    /* #ifdef CONFIG_CPU_BR23 */
    /*         io_ext_interrupt_init(MPU9250_INT_IO, 1, vcHr02IRQHandler); */
    /* #elif defined(CONFIG_CPU_BR28)||defined(CONFIG_CPU_BR27) */
    /*         // br28外部中断回调函数，按照现在的外部中断注册方式 */
    /*         // io配置在板级，定义在板级头文件，这里只是注册回调函数 */
    /*         port_edge_wkup_set_callback(vcHr02IRQHandler); */
    /* #endif */

    /* vcMode = VCWORK_MODE_HRWORK; */
    vcMode = VCWORK_MODE_SPO2WORK;
    if (vcHr02DevInit(&vchr02_iic_info_test)) {
        timerx_init();
        vcHr02ReadRegisters(0x00, &temp, 1);//read id
        log_info("vchr02 init ok!(id:0x%x)", temp);
        while (1) {
            vcHr02_process(vcSportMode);
            wdt_clear();
        }
    } else {
        log_info("vchr02 init fail!");
    }
}
#endif

