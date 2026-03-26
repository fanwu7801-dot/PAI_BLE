/*
 ***************************************************************************
 *							AUDIO AEC DEMO
 * File  : audio_aec_demo.c
 * By    : GZR
 * Notes : 1.可用内存
 *		   (1)aec_hdl_mem:	静态变量
 *		   (2)free_ram:	动态内存(mem_stats:physics memory size xxxx bytes)
 *		   2.demo默认将输入数据copy到输出，相关处理只需在运算函数
 *		    audio_aec_run()实现即可
 *		   3.双mic ENC开发，只需打开对应板级以下配置即可：
 *			#define TCFG_AUDIO_DUAL_MIC_ENABLE		ENABLE_THIS_MOUDLE
 *		   4.建议算法开发者使用宏定义将自己的代码模块包起来
 *		   5.算法处理完的数据，如有需要，可以增加EQ处理：AEC_UL_EQ_EN
 *		   6.开发阶段，默认使用芯片最高主频160MHz，可以通过修改AEC_CLK来修改
 			运行频率。
 ***************************************************************************
 */
#include "aec_user.h"
#include "system/includes.h"
#include "media/includes.h"
#include "media/effects_adj.h"
#include "circular_buf.h"
#include "overlay_code.h"
#include "audio_config.h"
#include "debug.h"

#if defined(TCFG_CVP_DEVELOP_ENABLE) && (TCFG_CVP_DEVELOP_ENABLE == 1)

#define AEC_CLK				(160 * 1000000L)	/*模块运行时钟(MaxFre:160MHz)*/
#define AEC_FRAME_POINTS	256					/*AEC处理帧长，跟mic采样长度关联*/
#define AEC_FRAME_SIZE		(AEC_FRAME_POINTS << 1)
#if TCFG_AUDIO_DUAL_MIC_ENABLE
#define AEC_MIC_NUM         2   /*双mic*/
#else
#define AEC_MIC_NUM         1   /*单mic*/
#endif/*TCFG_AUDIO_DUAL_MIC_ENABLE*/

/*上行数据eq*/
#define AEC_UL_EQ_EN		0

#define AEC_USER_MALLOC_ENABLE	1 /*是否使用动态内存*/

#ifdef AUDIO_PCM_DEBUG
/*AEC串口数据导出*/
const u8 CONST_AEC_EXPORT = 1;
#else
const u8 CONST_AEC_EXPORT = 0;
#endif/*AUDIO_PCM_DEBUG*/

/*参考数据变采样处理*/
#if TCFG_USB_MIC_CVP_ENABLE
const u8 CONST_REF_SRC = 1;
#else
const u8 CONST_REF_SRC = 0;
#endif/*TCFG_USB_MIC_CVP_ENABLE*/

extern int audio_dac_read_reset(void);
extern int audio_dac_read(s16 points_offset, void *data, int len, u8 ref_channel);
extern void esco_enc_resume(void);
extern int aec_uart_init();
extern int aec_uart_open(u8 nch, u16 single_size);
extern int aec_uart_fill(u8 ch, void *buf, u16 size);
extern void aec_uart_write(void);
extern int aec_uart_close(void);

/*AEC输入buf复用mic_adc采样buf*/
#define MIC_BULK_MAX		3
struct mic_bulk {
    struct list_head entry;
    s16 *addr;
    u16 len;
    u16 used;
};

struct audio_aec_hdl {
    volatile u8 start;				//aec模块状态
    u8 mic_num;				//MIC的数量
    volatile u8 busy;
    volatile u8 ref_busy;
    volatile u8 ref_ok;
    u8 output_buf[1000];	//aec数据输出缓存
    cbuffer_t output_cbuf;
    s16 *mic;				/*主mic数据地址*/
    s16 *mic_ref;			/*参考mic数据地址*/
    s16 *mic_ref_1;         /*参考mic数据地址*/
    s16 *pFar;			/*参考数据地址*/
    s16 *free_ram;			/*当前可用内存*/
    /* s16 spk_ref[AEC_FRAME_POINTS];	#<{(|扬声器参考数据|)}># */
    s16 *spk_ref;	/*扬声器参考数据*/
    s16 out[AEC_FRAME_POINTS];		/*运算输出地址*/
    OS_SEM sem;
    /*数据复用相关数据结构*/
    struct mic_bulk in_bulk[MIC_BULK_MAX];
    struct mic_bulk inref_bulk[MIC_BULK_MAX];
    struct mic_bulk inref_1_bulk[MIC_BULK_MAX];
    struct mic_bulk ref0_bulk[MIC_BULK_MAX];
    struct list_head in_head;
    struct list_head inref_head;
    struct list_head inref_1_head;
    struct list_head ref0_head;
#if AEC_UL_EQ_EN
    struct audio_eq *ul_eq;
#endif/*AEC_UL_EQ_EN*/
    int (*output_handle)(u8 *dat, u16 len);
    u8 output_way;
    u8 fm_tx_start;
    u8 ref_channel;
    u8 adc_ref_en;
    u16 ref_size;
    u32 ref_sr;
};
#if AEC_USER_MALLOC_ENABLE
struct audio_aec_hdl *aec_hdl = NULL;
#else
struct audio_aec_hdl  aec_handle;
struct audio_aec_hdl *aec_hdl = &aec_handle;
#endif /*AEC_USER_MALLOC_ENABLE*/

#define AEC_REF_CBUF_SIZE         (AEC_FRAME_POINTS * 6)
#define AEC_REF_CBUF_DOOR_SIZE    (AEC_REF_CBUF_SIZE / 2)
static cbuffer_t *aec_ref_cbuf = NULL;

__attribute__((weak))u32 usb_mic_is_running()
{
    return 0;
}

extern int esco_adc_mic_en();
void audio_aec_ref_start(u8 en)
{
    if (aec_hdl) {
        if (en != aec_hdl->fm_tx_start) {
            if (esco_adc_mic_en() == 0) {
                aec_hdl->fm_tx_start = en;
                y_printf("fm_tx_start:%d\n", en);
            }
        }
    }
}

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
int audio_aec_output_read(s16 *buf, u16 len)
{
    //printf("rlen:%d-%d\n",len,aec_hdl.output_cbuf.data_len);
    local_irq_disable();
    if (!aec_hdl || !aec_hdl->start) {
        printf("audio_aec close now");
        local_irq_enable();
        return -EINVAL;
    }
    u16 rlen = cbuf_read(&aec_hdl->output_cbuf, buf, len);
    if (rlen == 0) {
        //putchar('N');
    }
    local_irq_enable();
    return rlen;
}

/*
*********************************************************************
*                  Audio AEC Output Handle
* Description: AEC模块数据输出回调
* Arguments  : data 输出数据地址
*			   len	输出数据长度
* Return	 : 数据输出消耗长度
* Note(s)    : None.
*********************************************************************
*/
static int audio_aec_output(s16 *data, u16 len)
{
    u16 wlen = 0;
    if (aec_hdl && aec_hdl->start) {
        wlen = cbuf_write(&aec_hdl->output_cbuf, data, len);
        //printf("wlen:%d-%d\n",len,aec_hdl.output_cbuf.data_len);
        if (wlen != len) {
            printf("aec_out_full:%d,%d\n", len, wlen);
        }
        esco_enc_resume();
    }
    return wlen;
}

/*
 *跟踪系统内存使用情况:physics memory size xxxx bytes
 *正常的系统运行过程，应该至少有3k bytes的剩余空间给到系统调度开销
 */
static void sys_memory_trace(void)
{
    static int cnt = 0;
    if (cnt++ > 200) {
        cnt = 0;
        mem_stats();
    }
}
#include "Resample_api.h"
static RS_STUCT_API *sw_src_api = NULL;
static unsigned int *sw_src_buf = NULL;
static int sw_src_init(u8 nch, u16 insample, u16 outsample)
{
    if (CONST_REF_SRC == 1) {
        if (insample != outsample) {
            sw_src_api = get_rs16_context();
            /* sw_src_api = get_rsfast_context(); */
            g_printf("sw_src_api:0x%x\n", sw_src_api);
            ASSERT(sw_src_api);
            u32 sw_src_need_buf = sw_src_api->need_buf();
            g_printf("sw_src_buf:%d\n", sw_src_need_buf);
            sw_src_buf = malloc(sw_src_need_buf);
            ASSERT(sw_src_buf);
            RS_PARA_STRUCT rs_para_obj;
            rs_para_obj.nch = nch;

            rs_para_obj.new_insample = insample;//48000;
            rs_para_obj.new_outsample = outsample;//16000;
            printf("sw src,ch = %d, in = %d,out = %d\n", rs_para_obj.nch, rs_para_obj.new_insample, rs_para_obj.new_outsample);
            sw_src_api->open(sw_src_buf, &rs_para_obj);
        }
    }

    return 0;
}

static int sw_src_run(s16 *indata, s16 *outdata, u16 len)
{
    int outlen = len;
    if (CONST_REF_SRC == 1) {
        if (sw_src_api && sw_src_buf) {
            outlen = sw_src_api->run(sw_src_buf, indata, len >> 1, outdata);
            /* ASSERT(outlen <= (sizeof(outdata) >> 1));  */
            outlen = outlen << 1;
            /* printf("%d\n",outlen); */
        }
    }
    return outlen;
}

static void sw_src_exit(void)
{
    if (CONST_REF_SRC == 1) {
        if (sw_src_buf) {
            free(sw_src_buf);
            sw_src_buf = NULL;
            sw_src_api = NULL;
        }
    }
}

/*
*********************************************************************
*                  Audio AEC RUN
* Description: AEC数据处理核心
* Arguments  : in 		主mic数据
*			   inref	参考mic数据(双mic降噪有用)
*			   ref		speaker参考数据
*			   out		数据输出
*			   points   数据点数，单位short
* Return	 : 数据运算输出长度
* Note(s)    : 在这里实现AEC_core
*********************************************************************
*/
static int audio_aec_run(s16 *in, s16 *inref, s16 *inref1, s16 *ref, s16 *out, u16 points)
{
    int out_size = 0;
    putchar('.');
    memcpy(out, in, (points << 1));
    /* memcpy(out, inref, (points << 1)); */
    /* memcpy(out, ref, (points << 1)); */
    out_size = points << 1;
#if AEC_UL_EQ_EN
    if (aec_hdl->ul_eq) {
        audio_eq_run(aec_hdl->ul_eq, out, out_size);
    }
#endif/*AEC_UL_EQ_EN*/

    sys_memory_trace();
    return out_size;
}

/*
*********************************************************************
*                  Audio AEC Task
* Description: AEC任务
* Arguments  : priv	私用参数
* Return	 : None.
* Note(s)    : None.
*********************************************************************
*/
static void audio_aec_task(void *priv)
{
    printf("==Audio AEC Task==\n");
    struct mic_bulk *bulk = NULL;
    struct mic_bulk *bulk_ref = NULL;
    struct mic_bulk *bulk_ref_1 = NULL;
    struct mic_bulk *ref0_bulk = NULL;
    u8 pend = 1;
    while (1) {
        if (aec_hdl->output_way == 1) {
            if (!list_empty(&aec_hdl->in_head) && (aec_ref_cbuf->data_len >= aec_hdl->ref_size)) {
                cbuf_read(aec_ref_cbuf, aec_hdl->spk_ref, aec_hdl->ref_size);
            } else {
                os_sem_pend(&aec_hdl->sem, 0);
                continue;
            }
        } else {
            if (pend) {
                os_sem_pend(&aec_hdl->sem, 0);
            }
        }
        pend = 1;
        if (aec_hdl->start) {
            if ((!list_empty(&aec_hdl->in_head)) &&
                (!list_empty(&aec_hdl->ref0_head))) {
                aec_hdl->busy = 1;
                local_irq_disable();
                /*1.获取主mic数据*/
                bulk = list_first_entry(&aec_hdl->in_head, struct mic_bulk, entry);
                list_del(&bulk->entry);
                aec_hdl->mic = bulk->addr;
                if (aec_hdl->mic_num >= 2) {
                    /*获取参考mic数据*/
                    bulk_ref = list_first_entry(&aec_hdl->inref_head, struct mic_bulk, entry);
                    list_del(&bulk_ref->entry);
                    aec_hdl->mic_ref = bulk_ref->addr;
                }
                if (aec_hdl->mic_num >= 3) {
                    /*获取参考mic数据*/
                    bulk_ref_1 = list_first_entry(&aec_hdl->inref_1_head, struct mic_bulk, entry);
                    list_del(&bulk_ref_1->entry);
                    aec_hdl->mic_ref_1 = bulk_ref_1->addr;
                }
                /*获取参考数据*/
                aec_hdl->pFar = aec_hdl->spk_ref;
                if (aec_hdl->output_way == 0) {
                    ref0_bulk = list_first_entry(&aec_hdl->ref0_head, struct mic_bulk, entry);
                    list_del(&ref0_bulk->entry);
                    aec_hdl->pFar = (s16 *)ref0_bulk->addr;
                }
                local_irq_enable();

                int rlen = aec_hdl->ref_size;
                /*dac参考数据是立体声数据时，合成一个声道*/
                if (aec_hdl->ref_channel == 2) {
                    for (int i = 0; i < rlen / 4; i++) {
                        aec_hdl->pFar[i] = (short)(((int)aec_hdl->pFar[i * 2] + (int)aec_hdl->pFar[i * 2 + 1]) >> 1);
                    }
                    rlen = rlen >> 1;
                }

                /*参考数据变采样*/
                if (CONST_REF_SRC) {
                    rlen = sw_src_run(aec_hdl->pFar, aec_hdl->pFar, rlen);
                }

                /*4.算法处理*/
                int out_len = audio_aec_run(aec_hdl->mic, aec_hdl->mic_ref, aec_hdl->mic_ref_1, aec_hdl->pFar, aec_hdl->out, AEC_FRAME_POINTS);

                /*5.结果输出*/
                aec_hdl->output_handle(aec_hdl->out, out_len);

                /*6.数据导出*/
                if (CONST_AEC_EXPORT) {
                    aec_uart_fill(0, aec_hdl->mic, 512);		//主mic数据
                    /* aec_uart_fill(1, aec_hdl->mic_ref, 512);	//副mic数据 */
                    aec_uart_fill(1, aec_hdl->pFar, 512);	//扬声器数据
                    aec_uart_fill(2, aec_hdl->out, out_len); //算法运算结果
                    if (aec_hdl->mic_num == 3) {
                        aec_uart_fill(2, aec_hdl->mic_ref_1, 512);  //扬声器数据
                        aec_uart_fill(3, aec_hdl->pFar, 512);    //扬声器数据
                        aec_uart_fill(4, aec_hdl->out, out_len); //算法运算结果
                    }
                    aec_uart_write();
                }
                bulk->used = 0;
                if (aec_hdl->mic_num >= 2) {
                    bulk_ref->used = 0;
                }
                if (aec_hdl->mic_num >= 3) {
                    bulk_ref_1->used = 0;
                }
                if (aec_hdl->output_way == 0) {
                    ref0_bulk->used = 0;
                }
                aec_hdl->busy = 0;
                pend = 0;
            }
        }
    }
}

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
int audio_aec_open(u16 sample_rate, s16 enablebit, int (*out_hdl)(s16 *data, u16 len))
{
    printf("audio_aec_init,sr = %d\n", sample_rate);
    mem_stats();
    if (aec_hdl) {
        printf("audio aec is already open!\n");
        return -1;
    }
    overlay_load_code(OVERLAY_AEC);
#if AEC_USER_MALLOC_ENABLE
    aec_hdl = zalloc(sizeof(struct audio_aec_hdl));
    if (aec_hdl == NULL) {
        printf("aec_hdl malloc failed");
        return -ENOMEM;
    }
#endif /*AEC_USER_MALLOC_ENABLE*/

    aec_hdl->mic_num = AEC_MIC_NUM;
    aec_hdl->output_way = 0;

#if (TCFG_CVP_REF_IN_ADC_ENABLE)
    aec_hdl->adc_ref_en = 1;
#else
    aec_hdl->adc_ref_en = 0;
#endif /*TCFG_CVP_REF_IN_DAC_ENABLE*/
    aec_hdl->fm_tx_start = 0;
    aec_hdl->ref_channel = 1;

    aec_hdl->ref_sr  = usb_mic_is_running();
    if (aec_hdl->ref_sr == 0) {
        aec_hdl->ref_sr = sample_rate;
    }
    if (aec_hdl->adc_ref_en) {
        aec_hdl->ref_sr = sample_rate;
    }

    /*不支持参考数据和输入数据不成倍数关系的运算*/
    if (aec_hdl->ref_sr % sample_rate) {
        printf("AEC error: ref_sr:%d,in_sr:%d\n", aec_hdl->ref_sr, sample_rate);
        return -ENOEXEC;
    }

    if (aec_hdl->ref_channel != 2) {
        aec_hdl->ref_channel = 1;
    }

    if (out_hdl) {
        aec_hdl->output_handle = out_hdl;
    } else {
        aec_hdl->output_handle = audio_aec_output;
    }


    INIT_LIST_HEAD(&aec_hdl->in_head);
    INIT_LIST_HEAD(&aec_hdl->inref_head);
    INIT_LIST_HEAD(&aec_hdl->inref_1_head);
    if (aec_hdl->output_way == 0) {
        INIT_LIST_HEAD(&aec_hdl->ref0_head);
    }
    cbuf_init(&aec_hdl->output_cbuf, aec_hdl->output_buf, sizeof(aec_hdl->output_buf));

    /*adc回采参考数据时，复用adc的buf*/
    aec_hdl->ref_size = AEC_FRAME_POINTS * sizeof(short) * (aec_hdl->ref_sr / sample_rate) * aec_hdl->ref_channel;
    printf("aec ref_size:%d\n", aec_hdl->ref_size);

    if (aec_hdl->adc_ref_en == 0) {
        if (aec_hdl->output_way == 0) {
            aec_hdl->spk_ref = zalloc(aec_hdl->ref_size * MIC_BULK_MAX);
        } else {
            aec_hdl->spk_ref = zalloc(aec_hdl->ref_size);
        }
    }

    if (aec_hdl->output_way == 1) {
        u16 aec_ref_buf_size = AEC_REF_CBUF_SIZE * (aec_hdl->ref_sr / sample_rate) * aec_hdl->ref_channel;
        u8 *aec_ref_buf = zalloc(aec_ref_buf_size + sizeof(cbuffer_t));
        aec_ref_cbuf = (cbuffer_t *)aec_ref_buf;
        printf("aec ref cbuf:%d\n", aec_ref_buf_size);
        if (aec_ref_cbuf) {
            cbuf_init(aec_ref_cbuf, aec_ref_buf + sizeof(cbuffer_t), aec_ref_buf_size);
            /* cbuf_write(aec_ref_cbuf, aec->output, 128); */
        } else {
            printf("aec cbuf zalloc failed\n");
        }
    }
    if (CONST_AEC_EXPORT) {
        if (aec_hdl->mic_num == 3) {
            aec_uart_open(5, 512);
        } else {
            aec_uart_open(3, 512);
        }
    }

    audio_dac_read_reset();
    if (CONST_REF_SRC) {
        sw_src_init(1, aec_hdl->ref_sr, sample_rate);
    }

#if AEC_UL_EQ_EN
    u8 mode = 2;
    if (sample_rate == 8000) {
        mode = 3;
    }
    struct audio_eq_param ul_eq_param = {0};
    ul_eq_param.sr = sample_rate;
    ul_eq_param.channels = 1;
    ul_eq_param.max_nsection = phone_mode[mode].eq_parm.seg_num;
    ul_eq_param.nsection = phone_mode[mode].eq_parm.seg_num;
    ul_eq_param.seg = phone_mode[mode].eq_parm.seg;
    ul_eq_param.global_gain = phone_mode[mode].eq_parm.global_gain;
    ul_eq_param.cb = eq_get_filter_info;
    ul_eq_param.eq_name = AEID_ESCO_UL_EQ;
    aec_hdl->ul_eq = audio_dec_eq_open(&ul_eq_param);
#endif/*AEC_UL_EQ_EN*/
    os_sem_create(&aec_hdl->sem, 0);
    task_create(audio_aec_task, NULL, "aec");
    audio_dac_read_reset();
    aec_hdl->start = 1;

    mem_stats();
#ifdef MUX_RX_BULK_TEST_DEMO
    mux_rx_bulk_test = zalloc_mux_rx_bulk(MUX_RX_BULK_MAX);
    if (mux_rx_bulk_test) {
        printf("mux_rx_bulk_test:0x%x\n", mux_rx_bulk_test);
        free_mux_rx_bulk(mux_rx_bulk_test);
        mux_rx_bulk_test = NULL;
    }
#endif/*MUX_RX_BULK_TEST_DEMO*/

    printf("audio_aec_open succ\n");
    return 0;
}

/*
*********************************************************************
*                  Audio AEC Open
* Description: 初始化AEC模块
* Arguments  : sr 			采样率(8000/16000)
* Return	 : 0 成功 其他 失败
* Note(s)    : None.
*********************************************************************
*/
int audio_aec_init(u16 sample_rate)
{
    return audio_aec_open(sample_rate, -1, NULL);
}

/*
*********************************************************************
*                  Audio AEC Close
* Description: 关闭AEC模块
* Arguments  : None.
* Return	 : None.
* Note(s)    : None.
*********************************************************************
*/
void audio_aec_close(void)
{
    printf("audio_aec_close:%x", (u32)aec_hdl);
    if (aec_hdl) {
        aec_hdl->start = 0;
        while (aec_hdl->busy || aec_hdl->ref_busy) {
            os_time_dly(2);
        }
        task_kill("aec");
        if (CONST_AEC_EXPORT) {
            aec_uart_close();
        }
        if (CONST_REF_SRC) {
            sw_src_exit();
        }

        if (aec_ref_cbuf) {
            free(aec_ref_cbuf);
            aec_ref_cbuf = NULL;
        }

        if (aec_hdl->spk_ref) {
            free(aec_hdl->spk_ref);
            aec_hdl->spk_ref = NULL;
        }

        if (aec_hdl->free_ram) {
            free(aec_hdl->free_ram);
        }

#if AEC_UL_EQ_EN
        if (aec_hdl->ul_eq) {
            audio_dec_eq_close(aec_hdl->ul_eq);
            aec_hdl->ul_eq = NULL;
        }
#endif/*AEC_UL_EQ_EN*/

        local_irq_disable();
#if AEC_USER_MALLOC_ENABLE
        free(aec_hdl);
#endif /*AEC_USER_MALLOC_ENABLE*/
        aec_hdl = NULL;
        local_irq_enable();
        printf("audio_aec_close succ\n");
    }
}

/*
*********************************************************************
*                  Audio AEC Status
* Description: AEC模块当前状态
* Arguments  : None.
* Return	 : 0 关闭 其他 打开
* Note(s)    : None.
*********************************************************************
*/
u8 audio_aec_status(void)
{
    if (aec_hdl) {
        return aec_hdl->start;
    }
    return 0;
}

int cvp_develop_read_ref_data(void)
{
    u16 rlen = -1;
    u8 i;
    int err = 0;
    if (aec_hdl == NULL) {
        return 0;
    }
    if ((aec_hdl->output_way) || (aec_hdl->adc_ref_en)) {
        return 0;
    }
    aec_hdl->ref_busy = 1;
    for (i = 0; i < MIC_BULK_MAX; i++) {
        if (aec_hdl->ref0_bulk[i].used == 0) {
            rlen = audio_dac_read(60,
                                  aec_hdl->spk_ref + ((aec_hdl->ref_size >> 1) * i),
                                  aec_hdl->ref_size / aec_hdl->ref_channel,
                                  aec_hdl->ref_channel);
            break;
        }
    }
    if (i < MIC_BULK_MAX) {
        //AEC_D("bulk:%d-%d\n",i,len);
        aec_hdl->ref0_bulk[i].addr = (aec_hdl->spk_ref + ((aec_hdl->ref_size >> 1) * i));
        aec_hdl->ref0_bulk[i].used = 0x55;
        aec_hdl->ref0_bulk[i].len = rlen;
        list_add_tail(&aec_hdl->ref0_bulk[i].entry, &aec_hdl->ref0_head);
    } else {
        printf(">>>far_in_full\n");
        /*align reset*/
        struct mic_bulk *bulk;
        list_for_each_entry(bulk, &aec_hdl->ref0_head, entry) {
            bulk->used = 0;
            __list_del_entry(&bulk->entry);
        }
        aec_hdl->ref_busy = 0;
        return -1;
    }
    aec_hdl->ref_busy = 0;
    return rlen;
}

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
void audio_aec_inbuf(s16 *buf, u16 len)
{
    if (aec_hdl && aec_hdl->start) {

        if (aec_hdl->output_way == 1) {
            if ((aec_hdl->ref_ok == 0) || (aec_hdl->fm_tx_start == 0)) {
                if (aec_hdl->ref_ok && !aec_hdl->fm_tx_start) {
                    printf("[aec]fm_tx_start == 0\n");
                }
                return;
            }
        }

        int i = 0;
        for (i = 0; i < MIC_BULK_MAX; i++) {
            if (aec_hdl->in_bulk[i].used == 0) {
                break;
            }
        }
        if (i < MIC_BULK_MAX) {
            aec_hdl->in_bulk[i].addr = buf;
            aec_hdl->in_bulk[i].used = 0x55;
            aec_hdl->in_bulk[i].len = len;
            list_add_tail(&aec_hdl->in_bulk[i].entry, &aec_hdl->in_head);
        } else {
            printf(">>>aec_in_full\n");
            /*align reset*/
            if (aec_hdl->output_way == 0) {
                audio_dac_read_reset();
            }
            struct mic_bulk *bulk;
            list_for_each_entry(bulk, &aec_hdl->in_head, entry) {
                bulk->used = 0;
                __list_del_entry(&bulk->entry);
            }
            return;
        }

        int rlen = cvp_develop_read_ref_data();

        os_sem_set(&aec_hdl->sem, 0);
        os_sem_post(&aec_hdl->sem);
    }
}

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
void audio_aec_inbuf_ref(s16 *buf, u16 len)
{
    if (aec_hdl && aec_hdl->start) {
        if (aec_hdl->output_way == 1) {
            if ((aec_hdl->ref_ok == 0) || (aec_hdl->fm_tx_start == 0)) {
                if (aec_hdl->ref_ok && !aec_hdl->fm_tx_start) {
                    printf("[aec]fm_tx_start == 0\n");
                }
                return;
            }
        }

        int i = 0;
        for (i = 0; i < MIC_BULK_MAX; i++) {
            if (aec_hdl->inref_bulk[i].used == 0) {
                break;
            }
        }
        if (i < MIC_BULK_MAX) {
            aec_hdl->inref_bulk[i].addr = buf;
            aec_hdl->inref_bulk[i].used = 0x55;
            aec_hdl->inref_bulk[i].len = len;
            list_add_tail(&aec_hdl->inref_bulk[i].entry, &aec_hdl->inref_head);
        } else {
            printf(">>>aec_inref_full\n");
            /*align reset*/
            if (aec_hdl->output_way == 0) {
                audio_dac_read_reset();
            }
            struct mic_bulk *bulk;
            list_for_each_entry(bulk, &aec_hdl->inref_head, entry) {
                bulk->used = 0;
                __list_del_entry(&bulk->entry);
            }
            return;
        }
    }
}

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
void audio_aec_inbuf_ref_1(s16 *buf, u16 len)
{
    if (aec_hdl && aec_hdl->start) {
        if (aec_hdl->output_way == 1) {
            if ((aec_hdl->ref_ok == 0) || (aec_hdl->fm_tx_start == 0)) {
                if (aec_hdl->ref_ok && !aec_hdl->fm_tx_start) {
                    printf("[aec]fm_tx_start == 0\n");
                }
                return;
            }
        }

        int i = 0;
        for (i = 0; i < MIC_BULK_MAX; i++) {
            if (aec_hdl->inref_1_bulk[i].used == 0) {
                break;
            }
        }
        if (i < MIC_BULK_MAX) {
            aec_hdl->inref_1_bulk[i].addr = buf;
            aec_hdl->inref_1_bulk[i].used = 0x55;
            aec_hdl->inref_1_bulk[i].len = len;
            list_add_tail(&aec_hdl->inref_1_bulk[i].entry, &aec_hdl->inref_1_head);
        } else {
            printf(">>>aec_inref_1_full\n");
            /*align reset*/
            struct mic_bulk *bulk;
            list_for_each_entry(bulk, &aec_hdl->inref_1_head, entry) {
                bulk->used = 0;
                __list_del_entry(&bulk->entry);
            }
        }
    }
}

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
void audio_aec_refbuf(s16 *buf, u16 len)
{
    if (aec_hdl && aec_hdl->start) {
        if (aec_hdl->output_way != 1) {
            return;
        }
        aec_hdl->ref_busy = 1;

        aec_hdl->ref_ok = 1;
        if (0 == cbuf_write(aec_ref_cbuf, buf, len)) {
            printf("aec wfail:%d\n", len);
        }
#if 1
        static u16 aec_ref_max = 0;
        if (aec_ref_max < aec_ref_cbuf->data_len) {
            aec_ref_max = aec_ref_cbuf->data_len;
            printf("aec_ref_max:%d\n", aec_ref_max);
        }
#endif

        if (!list_empty(&aec_hdl->in_head) && (aec_ref_cbuf->data_len >= AEC_FRAME_SIZE)) {
            os_sem_set(&aec_hdl->sem, 0);
            os_sem_post(&aec_hdl->sem);
        }
        aec_hdl->ref_busy = 0;
    }
}

/*adc ref0*/
void audio_aec_in_refbuf(s16 *buf, u16 len)
{
    u16 wlen;
    u8 i;
    int err = 0;

    if ((aec_hdl == NULL) || (aec_hdl->adc_ref_en == 0) || (aec_hdl->output_way == 1)) {
        return;
    }
    aec_hdl->ref_busy = 1;

    //AEC_D("A:%d\n", len);
    for (i = 0; i < MIC_BULK_MAX; i++) {
        if (aec_hdl->ref0_bulk[i].used == 0) {
            break;
        }
    }
    if (i < MIC_BULK_MAX) {
        /* printf("bulk:%d-%d\n",i,len); */
        aec_hdl->ref0_bulk[i].addr = buf;
        aec_hdl->ref0_bulk[i].used = 0x55;
        aec_hdl->ref0_bulk[i].len = len;
        list_add_tail(&aec_hdl->ref0_bulk[i].entry, &aec_hdl->ref0_head);
    } else {
        printf(">>>aec_in_ref0_full\n");
        /*align reset*/
        struct mic_bulk *bulk;
        list_for_each_entry(bulk, &aec_hdl->ref0_head, entry) {
            bulk->used = 0;
            __list_del_entry(&bulk->entry);
        }
    }
    aec_hdl->ref_busy = 0;
}
#endif /*TCFG_CVP_DEVELOP_ENABLE*/
