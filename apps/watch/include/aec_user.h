#ifndef _AEC_USER_H_
#define _AEC_USER_H_

#include "generic/typedef.h"
#include "user_cfg.h"

#define AEC_DEBUG_ONLINE	0
#define AEC_READ_CONFIG		1

/*清晰语音处理模块定义*/
#define AEC_EN				BIT(0)	//回音消除
#define NLP_EN				BIT(1)	//回音压制
#define	ANS_EN				BIT(2)	//语音降噪
#define	ENC_EN				BIT(3)
#define AGC_EN				BIT(4)	//AGC自动增益控制
/*仅双麦使用*/
#define WNC_EN              BIT(5)
#define MFDT_EN             BIT(6)
/*仅单麦使用，TDE_EN和TDEYE_EN只有在TCFG_AUDIO_SMS_ENABLE = SMS_TDE有效*/
#define TDE_EN				BIT(5)	//延时估计模块使能
#define TDEYE_EN			BIT(6)	//延时估计模块结果使用(前提是模块使能)

/*清晰语音处理模式定义*/
#define AEC_MODE_ADVANCE	(AEC_EN | NLP_EN | ANS_EN)	//高级模式
#define AEC_MODE_REDUCE		(NLP_EN | ANS_EN)			//精简模式
#define AEC_MODE_SIMPLEX	(ANS_EN)					//单工模式

/*
 *SMS模式选择
 *(1)SMS模式性能更好，同时也需要更多的ram和mips
 *(2)SMS_NORMAL和SMS_TDE功能一样，只是SMS_TDE内置了延时估计和延时补偿
 *可以更好的兼容延时不固定的场景
 */
#define SMS_DISABLE		0
#define SMS_NORMAL		1
#define SMS_TDE			2
#define TCFG_AUDIO_SMS_ENABLE	SMS_DISABLE

/*
 *DMS版本配置
 *DMS_GLOBAL_V100:第一版双麦算法
 *DMS_GLOBAL_V200:第二版双麦算法，更少的ram和mips
 */
#if (TCFG_AUDIO_CVP_NS_MODE == CVP_ANS_MODE)
#define TCFG_AUDIO_DMS_GLOBAL_VERSION		DMS_GLOBAL_V100
#else
#define TCFG_AUDIO_DMS_GLOBAL_VERSION		DMS_GLOBAL_V200
#endif

/*兼容SMS和DMS*/
#if TCFG_AUDIO_DUAL_MIC_ENABLE
#include "commproc_dms.h"
#if (TCFG_AUDIO_DMS_SEL == DMS_NORMAL)
#define aec_open		aec_dms_init
#define aec_close		aec_dms_exit
#define aec_in_data		aec_dms_fill_in_data
#define aec_in_data_ref	aec_dms_fill_in_ref_data
#define aec_ref_data	aec_dms_fill_ref_data
#else /*TCFG_AUDIO_DMS_SEL == DMS_FLEXIBLE*/
#define aec_open		aec_dms_flexible_init
#define aec_close		aec_dms_flexible_exit
#define aec_in_data		aec_dms_flexible_fill_in_data
#define aec_in_data_ref	aec_dms_flexible_fill_in_ref_data
#define aec_ref_data	aec_dms_flexible_fill_ref_data
#endif/*TCFG_AUDIO_DMS_SEL*/

void aec_cfg_fill(AEC_DMS_CONFIG *cfg);
/*
*********************************************************************
*                  Audio AEC Output Select
* Description: 输出选择
* Arguments  : sel  = Default	默认输出算法处理结果
*					= Master	输出主mic(通话mic)的原始数据
*					= Slave		输出副mic(降噪mic)的原始数据
*			   agc 输出数据要不要经过agc自动增益控制模块
* Return	 : None.
* Note(s)    : 可以通过选择不同的输出，来测试mic的频响和ENC指标
*********************************************************************
*/
void audio_aec_output_sel(CVP_OUTPUT_ENUM sel, u8 agc);

/*sms单麦的时候*/
#elif (TCFG_AUDIO_SMS_ENABLE == SMS_NORMAL)
#include "commproc.h"
#define aec_open		sms_init
#define aec_close		sms_exit
#define aec_in_data		sms_fill_in_data
#define aec_ref_data	sms_fill_ref_data
#define aec_in_ref_data	sms_fill_in_ref_data
#elif (TCFG_AUDIO_SMS_ENABLE == SMS_TDE)
#include "commproc.h"
#define aec_open		sms_tde_init
#define aec_close		sms_tde_exit
#define aec_in_data		sms_tde_fill_in_data
#define aec_ref_data	sms_tde_fill_ref_data
#define aec_in_ref_data	sms_tde_fill_in_ref_data
#else /*TCFG_AUDIO_SMS_ENABLE == SMS_DISABLE*/
#include "commproc.h"
#define aec_open		aec_init
#define aec_close		aec_exit
#define aec_in_data		aec_fill_in_data
#define aec_ref_data	aec_fill_ref_data
#define aec_in_ref_data	aec_fill_in_ref_data
#endif/*TCFG_AUDIO_SMS_ENABLE*/

/*
*********************************************************************
*                  Audio AEC Init
* Description: 初始化AEC模块
* Arguments  : sr 采样率(8000/16000)
* Return	 : 0 成功 其他 失败
* Note(s)    : None.
*********************************************************************
*/
int audio_aec_init(u16 sample_rate);

/*
*********************************************************************
*                  Audio AEC Open
* Description: 初始化AEC模块
* Arguments  : sr 			采样率(8000/16000)
*			   enablebit	使能模块(AEC/NLP/AGC/ANS...)
*			   out_hdl		自定义回调函数，NULL则用默认的回调
* Return	 : 0 成功 其他 失败
* Note(s)    : 该接口是对audio_aec_init的扩展，支持自定义使能模块以及
*			   数据输出回调函数
*********************************************************************
*/
int audio_aec_open(u16 sample_rate, s16 enablebit, int (*out_hdl)(s16 *data, u16 len));

/*
*********************************************************************
*                  Audio AEC Close
* Description: 关闭AEC模块
* Arguments  : None.
* Return	 : None.
* Note(s)    : None.
*********************************************************************
*/
void audio_aec_close(void);

/*
*********************************************************************
*                  Audio AEC Input
* Description: AEC源数据输入
* Arguments  : buf	输入源数据地址
*			   len	输入源数据长度
* Return	 : None.
* Note(s)    : 输入一帧数据，唤醒一次运行任务处理数据，默认帧长256点
*********************************************************************
*/
void audio_aec_inbuf(s16 *buf, u16 len);

/*
*********************************************************************
*                  Audio AEC Input Reference
* Description: AEC源参考数据输入
* Arguments  : buf	输入源数据地址
*			   len	输入源数据长度
* Return	 : None.
* Note(s)    : 双mic ENC的参考mic数据输入,单mic的无须调用该接口
*********************************************************************
*/
void audio_aec_inbuf_ref(s16 *buf, u16 len);

/*
*********************************************************************
*                  Audio AEC Reference
* Description: AEC模块参考数据输入
* Arguments  : buf	输入参考数据地址
*			   len	输入参考数据长度
* Return	 : None.
* Note(s)    : 声卡设备是DAC，默认不用外部提供参考数据
*********************************************************************
*/
void audio_aec_refbuf(s16 *buf, u16 len);

/*
*********************************************************************
*                  Audio AEC Reference
* Description: AEC模块回采DAC作为参考数据输入
* Arguments  : buf	输入参考数据地址
*			   len	输入参考数据长度
* Return	 : None.
* Note(s)    : 声卡设备是DAC，可以使用ADC采集dac的数据作为参数数据
*********************************************************************
*/
void audio_aec_in_refbuf(s16 *buf, u16 len);

/*
*********************************************************************
*                  Audio AEC Output Query
* Description: 查询aec模块的输出数据缓存大小
* Arguments  : None.
* Return	 : 数据缓存大小
* Note(s)    : None.
*********************************************************************
*/
int audio_aec_output_data_size(void);

/*
*********************************************************************
*                  Audio AEC Output Read
* Description: 读取aec模块的输出数据
* Arguments  : buf  读取数据存放地址
*			   len	读取数据长度
* Return	 : 数据读取长度
* Note(s)    : None.
*********************************************************************
*/
int audio_aec_output_read(s16 *buf, u16 len);
int audio_aec_open(u16 sample_rate, s16 enablebit, int (*out_hdl)(s16 *data, u16 len));

/*
*********************************************************************
*                  			Audio CVP IOCTL
* Description: CVP功能配置
* Arguments  : cmd 		操作命令
*		       value 	操作数
*		       priv 	操作内存地址
* Return	 : 0 成功 其他 失败
* Note(s)    : (1)比如动态开关降噪NS模块:
*				audio_cvp_ioctl(CVP_NS_SWITCH,1,NULL);	//降噪关
*				audio_cvp_ioctl(CVP_NS_SWITCH,0,NULL);  //降噪开
*********************************************************************
*/
int audio_cvp_ioctl(int cmd, int value, void *priv);

// 清mic数据
void aec_input_clear_enable(u8 enable);
int aec_get_input_clear_status(void);

//aec 上行音效
enum  {
    SOUND_ORIGINAL, //原声
    SOUND_UNCLE,    //大叔
    SOUND_GODDESS,  //女神
};

#if defined(AEC_PRESETS_UL_EQ_CONFIG) && AEC_PRESETS_UL_EQ_CONFIG
#ifndef UL_PRESETS_EQ_NSECTION
#define UL_PRESETS_EQ_NSECTION 5
#endif
#if  (UL_PRESETS_EQ_NSECTION > EQ_SECTION_MAX )
#error "EQ_SETCTION_MAX too samll"
#endif
void aec_ul_eq_update();
void ul_presets_eq_set(u8 tab_num);
#endif/*AEC_PRESETS_UL_EQ_CONFIG*/

#endif/*_AEC_USER_H_*/
