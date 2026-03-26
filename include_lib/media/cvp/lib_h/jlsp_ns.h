#ifndef __NS_H__
#define __NS_H__

//出现单个麦掩蔽参数
typedef struct {
    int cur_state;   //当前状态
    int t_top_cnt_limit; //计数上限
    int t_bot_cnt_limit; //计数下限

    int s0_cnt;   //计数器0
    int s1_cnt;   //计数器1

    float mic_diff_db;  //两个mic相差db阈值
    float basic_noise_bot_db;  //mic的底噪阈值
    float basic_noise_top_db;  //mic的上限阈值
} JLSP_dms_micsel_cfg;


//beamforming参数设置

typedef struct {
    int UseExWeight;    //挂空
    int *ENC_WeightA;   //挂空
    int *ENC_WeightB;   //挂空
    int ENC_Weight_Q;   //挂空

    int ENC_Process_MaxFrequency;       //设定enc处理的最大频率，设定范围（3000~8000），默认8000
    int ENC_Process_MinFrequency;       //设定enc处理的最小频率，设定范围（0~1000），默认为0
    int SIR_MaxFrequency;               //设定enc信噪比处理最大频率，设定范围（1000~8000），默认3000
    float Mic_Distance;                 //设定主副mic的距离，单位（m），范围（0.015~35），默认0.015
    float Target_Signal_Degradation;    //根据主副麦能量的diff值，设定范围（0~1），默认1.0
    float AggressFactor;                //设置enc压制强度，设定范围（0~4），默认4
    float minSuppress;                  //设定enc抑制的最小值，设定范围（0~0.1），默认0.09

    float compen_db;                    //补偿增益控制，单位（db），具体调节配合麦的增益协同调节，一般设定范围为（0~20）
    int bf_enhance;                     //该值默认设置为0，设为1时，高频成分会丰富些，但是低信噪比下高频噪声也会偏大
} JLSP_dms_bf_cfg;

//回声消除设置参数
typedef struct {
    int AEC_Process_MaxFrequency;    //设定回声消除处理的最大频率，设定范围（3000~8000），默认值为8000
    int AEC_Process_MinFrequency;    //设定回声消除处理的最小频率，设定范围（0~1000），默认值为0
    int AF_Length; //挂空
    float muf;                      //设定aec滤波器的学习速率，默认0.02，设定范围（0.0001~0.5）
} JLSP_dms_aec_cfg;


//非线性回声压制设置参数
typedef struct {
    int NLP_Process_MaxFrequency;    //设定回声抑制的最大频率，设定范围（3000~8000），默认值为8000
    int NLP_Process_MinFrequency;    //设定回声抑制的最小频率，设定范围（0~1000），默认值为0
    float OverDrive;                 //定设压制强度，越大压制越强，设定范围（0~5）默认为1.0
} JLSP_dms_nlp_cfg;


//dns参数设置
typedef struct {
    float AggressFactor;    //降噪强度，设置范围（0.3~6.0），默认值：1.0
    float minSuppress;      //增益最小值控制，越小降噪越强，设置范围（0.0~1.0），默认为0.1
    float init_noise_lvl;   //初始噪声水平
    float high_gain;        //单麦高频增强，默认设置为1.0，设置范围为（1.0~3.0）
    float rb_rate;          //单麦混响增强，默认设置为0.1，设置范围为（0~0.9）
    int enhance_flag;       //是否开启双麦高频增强，默认为1
} JLSP_dms_dns_cfg;

//风噪检测参数设置
typedef struct {
    float wn_msc_th;    //双麦非相关性阈值
    float ms_th;        //麦增益能量阈值
    int wn_inty1;
    int wn_inty2;
    float wn_gain1;
    float wn_gain2;

    int t1_bot;         //低风噪等级能量下限阈值
    int t1_top;         //低风噪等级能量上限阈值
    int t2_bot;         //强风噪等级能量下限阈值
    int t2_top;         //强风噪等级能量上限阈值
    int offset;         //风噪能量增益偏移阈值

    int t1_bot_cnt_limit;   //风强变到弱风等级连续帧数计数,位宽16bit
    int t1_top_cnt_limit;   //风强由弱风变到中风等级连续帧数计数，位宽16bit
    int t2_bot_cnt_limit;   //风强由强风变到中风连续帧数计数，位宽16bit
    int t2_top_cnt_limit;   //风强变到强风连续帧数计数，位宽16bit
} JLSP_dms_wind_cfg;

/*
gain_floor: 增益的最小值控制,范围0~1,建议值(0~0.2)之间
over_drive: 控制降噪强度:
0 < over_drive < 1，越小降噪强度越轻，太小噪声会很大；
over_drive = 1,正常降噪
over_drive > 1,降噪强度加强，越大降噪强度越强，太大会吃音
建议调节范围0.3~3之间来控制降噪强度的强弱
high_gain: 控制声音的宏亮度,范围(1.0f~3.5f),越大声音越宏亮,太大可能会使噪声增加, 为1.0f表示不做增强, 建议设置2.0f左右
rb_rate: 混响强度,设置范围（0.0f~0.9f）,越大混响越强, 为0.0f表示不增强, 建议默认设置0.5f
*/

void *JLSP_ns_init(char *private_buffer, char *shared_buffer, float gain_floor, float over_drive, float high_gain, float rb_rate, int samplerate, const int is_dual);
int JLSP_ns_get_heap_size(int *private_size, int *shared_size, int samplerate, int is_dual);

int JLSP_ns_reset(void *m);
void JLSP_ns_update_shared_buffer(void *m, char *shared_buffer);
int JLSP_ns_process(void *m, void *input, void *output, int *outsize);
int JLSP_ns_free(void *m);

void JLSP_ns_set_noiselevel(void *m, float noise_level_init);


/*功能：初始化snr相关参数
* stop_freq: 高通滤波器的截止频率
* snr_th: 人声阈值设定,即大于该值才会进行snr压制处理, 采用db进行设置, 默认-25db
* commpress_params: 压制系数
*	0db一下分为了5个区间进行压缩,压缩系数范围(0~3),越大压制越强,建议范围(0~1.0f)
*	snr < -10, 压制系数compress_params[4]
*	snr < -8, 压制系数compress_params[3]
*	snr < -5, 压制系数compress_params[2]
*	snr < -3, 压制系数compress_params[1]
*	snr < 0, 压制系数compress_params[0]
*/
void JLSP_init_snr(void *m, int stop_freq, int snr_th,  float *compress_params);

/*功能： 获取当前帧snr
* 返回：当前帧snr值
*/
float JLSP_get_snr(void *m);

void JLSP_wd_single(unsigned char wd_act);
/*
 *wd_flag: 0: no wind 1: wind
 *wd_val:  wind strong bit(16)
 *wd_lev:  wind level 0:弱风，1:中风, 2:强风
 */
int JLSP_get_wd_info(void *handle, int *wd_flag, int *wd_val, int *wd_lev);

void JLSP_enable_module(int Enablebit);

void *JLSP_DualMicSystem_Init(char *private_buf,
                              char *share_buf,
                              void *aec_cfg,
                              void *nlp_cfg,
                              void *bf_cfg,
                              void *dns_cfg,
                              void *wn_cfg,
                              int samplerate,
                              int EnableBit);

int JLSP_DualMicSystem_Process(void *handle,
                               short *near,
                               short *near_ref,
                               short *far,
                               short *out,
                               int points);

#endif
