#include "asm/includes.h"
#include "media/includes.h"
#include "system/includes.h"
#include "app_config.h"
#include "app_main.h"
#include "app_task.h"
#include "audio_config.h"
#include "audio_enc/audio_enc.h"
#include "audio_dec.h"
#include "media/pcm_decoder.h"
#include "aec_user.h"
#include "aic_audio.h"
#include "aic_portable.h"

#if TCFG_CAT1_AICXTEK_ENABLE

#define AIXCTEK_MIC_AEC_EN					0 // MIC接入降噪

// mic输出配置
#define LADC_MIC_OUT_CH_NUM					1 // 单声道输出
#define LADC_MIC_OUT_SR						16000 // 输出采样率
#define LADC_MIC_IRQ_MS						20 // 多久一次中断
#define LADC_MIC_RESUME_SIZE				(LADC_MIC_IRQ_MS * LADC_MIC_OUT_SR * LADC_MIC_OUT_CH_NUM * 2 / 1000) // 一次中断所需的buf
#define LADC_MIC_OUT_BUF_MS					(LADC_MIC_IRQ_MS * 5 / 2) // 输出缓存多少时间数据
#define LADC_MIC_OUT_BUF_LEN				(LADC_MIC_OUT_BUF_MS * LADC_MIC_OUT_SR * LADC_MIC_OUT_CH_NUM * 2 / 1000) // mic输出缓存buf


// dec输入配置
#define DEC_SOURCE_DATA_CH_NUM				1 // 单声道输入
#define DEC_SOURCE_DATA_SR					16000 // 输入采样率
#define DEC_SOURCE_BUF_LEN					(LADC_MIC_OUT_BUF_MS * DEC_SOURCE_DATA_SR * DEC_SOURCE_DATA_CH_NUM * 2 / 1000) // mic输出缓存buf


// mic hw配置
#define LADC_MIC_GAIN						19 // MIC增益
#define LADC_MIC_CH_SR						LADC_MIC_OUT_SR //mic采样率
#if TCFG_AUDIO_DUAL_MIC_ENABLE
#define LADC_MIC_CH_NUM         			2	//双mic
#else /*TCFG_AUDIO_DUAL_MIC_ENABLE == 0*/
#define LADC_MIC_CH_NUM         			1	//单mic
#endif /*TCFG_AUDIO_DUAL_MIC_ENABLE*/
#define LADC_MIC_BUF_NUM        			2
#if AIXCTEK_MIC_AEC_EN
#define LADC_MIC_IRQ_POINTS     			256 // 降噪必须256个点一次中断
#else /* #if AIXCTEK_MIC_AEC_EN */
#define LADC_MIC_IRQ_POINTS     			(LADC_MIC_IRQ_MS * LADC_MIC_CH_SR / 1000)
#endif /* #if AIXCTEK_MIC_AEC_EN */
#define LADC_MIC_BUFS_SIZE      			(LADC_MIC_CH_NUM * LADC_MIC_BUF_NUM * LADC_MIC_IRQ_POINTS)


#define DATA_ALIN(var,al)     				((((var)+(al)-1)/(al))*(al))


#define LADC_MIC_SINE_DBG					0 // mic数据替换成正弦波

#if 1
#define DBG_LOG								printf
#else
#define DBG_LOG(...)
#endif


// mic
struct aic_mic_hdl {
    struct audio_adc_output_hdl adc_output;
    struct adc_mic_ch    mic_ch;
    s16 *adc_buf;
    volatile u8  status;
};

// dec
struct aic_dec_hdl {
    struct audio_stream *stream;	// 音频流
    struct pcm_decoder pcm_dec;		// pcm解码句柄
    struct audio_res_wait wait;		// 资源等待句柄
    struct audio_mixer_ch mix_ch;	// 叠加句柄
    u32 id;				// 唯一标识符，随机值
    volatile u8  status;
};

// audio
struct aic_mic_dec_hdl {
    cbuffer_t mic_cbuf;
    cbuffer_t dec_cbuf;
    u32 mic_lose_r;
    u32 mic_lose_w;
    u32 dec_lose_r;
    u32 dec_lose_w;
    volatile u8  dec_w_flag;
    volatile u8  status;
    volatile u8  input_clear;
    void (*resume)(void *);
    void *resume_priv;
};



struct aic_mic_hdl *mic_hdl = NULL;
struct aic_dec_hdl *dec_hdl = NULL;
struct aic_mic_dec_hdl *audio_hdl = NULL;


extern void audio_adc_set_irq_point_unit(u16 point_unit);

void aic_dec_close(void);


#if LADC_MIC_SINE_DBG
static short const tsin_16k[16] = {
    0x0000, 0x30fd, 0x5a83, 0x7641, 0x7fff, 0x7642, 0x5a82, 0x30fc,
    0x0000, 0xcf04, 0xa57d, 0x89be, 0x8000, 0x89be, 0xa57e, 0xcf05,
};
static u16 tx_s_cnt = 0;

static int get_sine_16k_data(u16 *s_cnt, s16 *data, u16 points, u8 ch)
{
    while (points--) {
        if (*s_cnt >= 16) {
            *s_cnt = 0;
        }
        *data++ = tsin_16k[*s_cnt];
        if (ch == 2) {
            *data++ = tsin_16k[*s_cnt];
        }
        (*s_cnt)++;
    }
    return 0;
}
#endif /* #if LADC_MIC_SINE_DBG */

static void adc_mic_write_pcm(s16 *data, u16 len)
{
    if (audio_hdl && audio_hdl->status) {
        if (audio_hdl->input_clear) {
            memset(data, 0, len);
        }
#if LADC_MIC_SINE_DBG
        /* if ((audio_hdl->mic_cbuf.total_len - audio_hdl->mic_cbuf.data_len) < len) { */
        /* return ; */
        /* } */
        get_sine_16k_data(&tx_s_cnt, data, len / 2, 1);
#endif /* #if LADC_MIC_SINE_DBG */

        int wlen = cbuf_write(&audio_hdl->mic_cbuf, data, len);
        if (wlen != len) {
            audio_hdl->mic_lose_w++;
        }
#if AIXCTEK_MIC_AEC_EN
        if (cbuf_get_data_len(&audio_hdl->mic_cbuf) < LADC_MIC_RESUME_SIZE) {
            return ;
        }
#endif /* #if AIXCTEK_MIC_AEC_EN */
        if (audio_hdl->resume) {
            audio_hdl->resume(audio_hdl->resume_priv);
        }
    }
}

#if AIXCTEK_MIC_AEC_EN
static int adc_aec_output_cb(s16 *data, u16 len)
{
    if (mic_hdl && mic_hdl->status) {
        adc_mic_write_pcm(data, len);
    }
    return len;
}
#endif/* #if AIXCTEK_MIC_AEC_EN*/

static void adc_output_to_enc(void *priv, s16 *data, int len)
{
    /* putchar('o'); */
    if ((!mic_hdl) || (!mic_hdl->status)) {
        return ;
    }

#if AIXCTEK_MIC_AEC_EN

#if (LADC_MIC_CH_NUM == 2)/*DualMic*/
    s16 *mic0_data = data;
    s16 *mic1_data = mic_hdl->adc_buf;
    s16 *mic1_data_pos = data + (len / 2);
    //DBG_LOG("mic_data:%x,%x,%d\n",data,mic1_data_pos,len);
    for (u16 i = 0; i < (len >> 1); i++) {
        mic0_data[i] = data[i * 2];
        mic1_data[i] = data[i * 2 + 1];
    }
    memcpy(mic1_data_pos, mic1_data, len);

#if 0 /*debug*/
    static u16 mic_cnt = 0;
    if (mic_cnt++ > 300) {
        putchar('1');
        audio_aec_inbuf(mic1_data_pos, len);
        if (mic_cnt > 600) {
            mic_cnt = 0;
        }
    } else {
        putchar('0');
        audio_aec_inbuf(mic0_data, len);
    }
#else
#if (TCFG_AUDIO_DMS_MIC_MANAGE == DMS_MASTER_MIC0)
    audio_aec_inbuf_ref(mic1_data_pos, len);
    audio_aec_inbuf(data, len);
#else
    audio_aec_inbuf_ref(data, len);
    audio_aec_inbuf(mic1_data_pos, len);
#endif/*TCFG_AUDIO_DMS_MIC_MANAGE*/
#endif/*debug end*/
#else/*SingleMic*/
    audio_aec_inbuf(data, len);
#endif/*LADC_MIC_CH_NUM */

#else /* #if AIXCTEK_MIC_AEC_EN*/

#if (LADC_MIC_CH_NUM == 2)
    if (LADC_MIC_OUT_CH_NUM == 2) { // 双声道输出
        adc_mic_write_pcm(data, len * 2);
    } else { // 单声道输出
        pcm_dual_to_single(data, data, len * 2);
        adc_mic_write_pcm(data, len * 1);
    }
#else /* #if (LADC_MIC_CH_NUM == 2)*/
    adc_mic_write_pcm(data, len * 1);
#endif /* #if (LADC_MIC_CH_NUM == 2)*/

#endif /* #if AIXCTEK_MIC_AEC_EN*/
}

int aic_mic_read_pcm(void *data, u16 len)
{
    int rlen = 0;
    if (audio_hdl && audio_hdl->status) {
        rlen = cbuf_read(&audio_hdl->mic_cbuf, data, len);
        if (rlen != len) {
            audio_hdl->mic_lose_r++;
        }
    }
    return rlen;
}

void aic_mic_close(void)
{
    DBG_LOG("%s \n", __func__);
    if (mic_hdl) {
        mic_hdl->status = 0;

#if AIXCTEK_MIC_AEC_EN
        audio_aec_close();
#endif/* #if AIXCTEK_MIC_AEC_EN*/
        audio_mic_close(&mic_hdl->mic_ch, &mic_hdl->adc_output);
        audio_adc_set_irq_point_unit(0);

        local_irq_disable();
        free(mic_hdl);
        mic_hdl = NULL;
        local_irq_enable();
    }
}

int aic_mic_open(void)
{
    struct aic_mic_hdl *pmic = NULL;

    if (mic_hdl) {
        aic_mic_close();
    }
    DBG_LOG("%s \n", __func__);

    u32 offset = 0;
    u32 size = DATA_ALIN(sizeof(struct aic_mic_hdl), 4);
#if (LADC_MIC_CH_NUM == 2)
    size += DATA_ALIN((sizeof(s16) * LADC_MIC_BUFS_SIZE), 4);
#endif /* #if (LADC_MIC_CH_NUM == 2)*/

    u8 *buf = zalloc(size);
    if (!buf) {
        return -ENOMEM;
    }
    pmic = (struct aic_mic_hdl *)(buf + offset);
    offset += DATA_ALIN(sizeof(struct aic_mic_hdl), 4);

#if AIXCTEK_MIC_AEC_EN
    if ((LADC_MIC_OUT_CH_NUM != 1) || (LADC_MIC_CH_SR > 16000)) {
        DBG_LOG("aec only support 8k,16k,1ch \n");
        free(pmic);
        return -EIO;
    }
#if (LADC_MIC_CH_NUM == 2)
    int err = audio_aec_open(LADC_MIC_CH_SR, ANS_EN | ENC_EN | AGC_EN, adc_aec_output_cb);
#else
    int err = audio_aec_open(LADC_MIC_CH_SR, ANS_EN | AGC_EN, adc_aec_output_cb);
#endif
    if (err) {
        DBG_LOG("audio_aec_init failed:%d", err);
    }
#endif /* #if AIXCTEK_MIC_AEC_EN*/


#if (LADC_MIC_CH_NUM == 2)
    pmic->adc_buf = (s16 *)(buf + offset);
#endif /* #if (LADC_MIC_CH_NUM == 2) */
    pmic->adc_output.handler = adc_output_to_enc;
    pmic->adc_output.priv    = pmic;
    audio_adc_set_irq_point_unit(LADC_MIC_IRQ_POINTS);
    if (audio_mic_open(&pmic->mic_ch, LADC_MIC_CH_SR, LADC_MIC_GAIN) == 0) {
        audio_mic_add_output(&pmic->adc_output);
        audio_mic_start(&pmic->mic_ch);
    }

    pmic->status = 1;

    mic_hdl = pmic;

    return 0;
}


static void aic_dec_relaese()
{
    if (dec_hdl) {
        audio_decoder_task_del_wait(&decode_task, &dec_hdl->wait);
        local_irq_disable();
        free(dec_hdl);
        dec_hdl = NULL;
        local_irq_enable();
    }
}

static void aic_dec_event_handler(struct audio_decoder *decoder, int argc, int *argv)
{
    switch (argv[0]) {
    case AUDIO_DEC_EVENT_END:
        if (!dec_hdl) {
            DBG_LOG("dec_hdl handle err ");
            break;
        }

        if (dec_hdl->id != argv[1]) {
            DBG_LOG("dec_hdl id err : 0x%x, 0x%x \n", dec_hdl->id, argv[1]);
            break;
        }

        aic_dec_close();
        //audio_decoder_resume_all(&decode_task);
        break;
    }
}

static void aic_dec_out_stream_resume(void *p)
{
    struct aic_dec_hdl *dec = p;
    audio_decoder_resume(&dec->pcm_dec.decoder);
}


static int aic_dec_read(void *hdl, void *data, int len)
{
    int rlen = 0;
    if (audio_hdl && audio_hdl->status) {
        rlen = cbuf_read(&audio_hdl->dec_cbuf, data, len);
        if (rlen != len) {
            if (audio_hdl->dec_w_flag) {
                audio_hdl->dec_lose_r++;
            } else {
                memset(data, 0, len);
                rlen = len; // 还没开始写数据前先填空
            }
        }
    }
    return rlen;
}

static int aic_dec_start(void)
{
    int err;
    struct aic_dec_hdl *dec = dec_hdl;
    struct audio_mixer *p_mixer = &mixer;

    if (!dec_hdl) {
        return -EINVAL;
    }

    err = pcm_decoder_open(&dec->pcm_dec, &decode_task);
    if (err) {
        goto __err1;
    }

    pcm_decoder_set_event_handler(&dec->pcm_dec, aic_dec_event_handler, dec->id);
    pcm_decoder_set_read_data(&dec->pcm_dec, aic_dec_read, NULL);

    audio_mode_main_dec_open(AUDIO_MODE_MAIN_STATE_DEC_ESCO);

    // 设置叠加功能
    audio_mixer_ch_open_head(&dec->mix_ch, p_mixer);

    audio_mixer_ch_set_sample_rate(&dec->mix_ch, DEC_SOURCE_DATA_SR);

    // 数据流串联
    struct audio_stream_entry *entries[32] = {NULL};
    u8 entry_cnt = 0;
    entries[entry_cnt++] = &dec->pcm_dec.decoder.entry;

#if SYS_DIGVOL_GROUP_EN
    void *dvol_entry = sys_digvol_group_ch_open("music_aic", -1, NULL);
    entries[entry_cnt++] = dvol_entry;
#endif // SYS_DIGVOL_GROUP_EN

    entries[entry_cnt++] = &dec->mix_ch.entry;

    // 创建数据流，把所有节点连接起来
    dec->stream = audio_stream_open(dec, aic_dec_out_stream_resume);
    audio_stream_add_list(dec->stream, entries, entry_cnt);

    // 设置音频输出音量
    audio_output_set_start_volume(APP_AUDIO_STATE_CALL);

    // 开始解码
    dec->status = 1;
    err = audio_decoder_start(&dec->pcm_dec.decoder);
    if (err) {
        goto __err3;
    }
    return 0;

__err3:
    dec->status = 0;

    audio_mixer_ch_close(&dec->mix_ch);
#if SYS_DIGVOL_GROUP_EN
    sys_digvol_group_ch_close("music_aic");
#endif // SYS_DIGVOL_GROUP_EN

    if (dec->stream) {
        audio_stream_close(dec->stream);
        dec->stream = NULL;
    }

    pcm_decoder_close(&dec->pcm_dec);
__err1:
    aic_dec_relaese();
    return err;
}

static void __aic_dec_close(void)
{
    if (dec_hdl && dec_hdl->status) {
        dec_hdl->status = 0;

        pcm_decoder_close(&dec_hdl->pcm_dec);

        audio_mixer_ch_close(&dec_hdl->mix_ch);
#if SYS_DIGVOL_GROUP_EN
        sys_digvol_group_ch_close("music_aic");
#endif // SYS_DIGVOL_GROUP_EN

        // 先关闭各个节点，最后才close数据流
        if (dec_hdl->stream) {
            audio_stream_close(dec_hdl->stream);
            dec_hdl->stream = NULL;
        }
    }
}

static int aic_wait_res_handler(struct audio_res_wait *wait, int event)
{
    int err = 0;
    DBG_LOG("aic_wait_res_handler, event:%d\n", event);
    if (event == AUDIO_RES_GET) {
        // 启动解码
        err = aic_dec_start();
    } else if (event == AUDIO_RES_PUT) {
        // 被打断
        __aic_dec_close();
    }

    return err;
}


void aic_dec_resume(void)
{
    if (dec_hdl && dec_hdl->status) {
        audio_decoder_resume(&dec_hdl->pcm_dec.decoder);
    }
}

int aic_dec_write_pcm(void *data, int len)
{
    int wlen = 0;
    if (audio_hdl && audio_hdl->status) {
        wlen = cbuf_write(&audio_hdl->dec_cbuf, data, len);
        if (wlen != len) {
            audio_hdl->dec_lose_w++;
        }
        audio_hdl->dec_w_flag = 1;
        aic_dec_resume();
    }
    return wlen;
}

void aic_dec_close(void)
{
    DBG_LOG("%s \n", __func__);
    if (dec_hdl) {
        __aic_dec_close();
        aic_dec_relaese();
        DBG_LOG("aic dec close \n\n ");
    }
}

int aic_dec_open(void)
{
    if (dec_hdl) {
        aic_dec_close();
    }
    DBG_LOG("%s \n", __func__);

    int err;
    struct aic_dec_hdl *dec;
    dec = zalloc(sizeof(*dec));
    if (!dec) {
        return -ENOMEM;
    }

    dec->pcm_dec.ch_num = DEC_SOURCE_DATA_CH_NUM;
    dec->pcm_dec.sample_rate = DEC_SOURCE_DATA_SR;
    dec->pcm_dec.output_ch_num = audio_output_channel_num();
    dec->pcm_dec.output_ch_type = audio_output_channel_type();
    DBG_LOG("in ch:%d, sr:%d, out ch:%d, type:0x%x \n", dec->pcm_dec.ch_num, dec->pcm_dec.sample_rate,
            dec->pcm_dec.output_ch_num, dec->pcm_dec.output_ch_type);

    dec->wait.priority = 2;
    dec->wait.preemption = 0;
    dec->wait.snatch_same_prio = 1;
    dec->wait.handler = aic_wait_res_handler;

    dec->id = rand32();
    dec_hdl = dec;

    err = audio_decoder_task_add_wait(&decode_task, &dec->wait);
    return err;
}


void aic_mic_dec_close(void)
{
    DBG_LOG("%s \n", __func__);

    if (audio_hdl) {
        audio_hdl->status = 0;
        aic_mic_close();
        aic_dec_close();

        DBG_LOG("mic lose:%d,%d \n", audio_hdl->mic_lose_r, audio_hdl->mic_lose_w);
        DBG_LOG("dec lose:%d,%d \n", audio_hdl->dec_lose_r, audio_hdl->dec_lose_w);

        local_irq_disable();
        free(audio_hdl);
        audio_hdl = NULL;
        local_irq_enable();
    }
}

int aic_mic_dec_open(void (*resume)(void *), void *resume_priv)
{
    if (audio_hdl) {
        aic_mic_dec_close();
    }
    DBG_LOG("%s \n", __func__);

    int err;
    struct aic_mic_dec_hdl *audio = NULL;
    audio = zalloc(sizeof(struct aic_mic_dec_hdl) + LADC_MIC_OUT_BUF_LEN + DEC_SOURCE_BUF_LEN);
    if (!audio) {
        return -ENOMEM;
    }
    void *mic_buf = ((u8 *)audio) + sizeof(struct aic_mic_dec_hdl);
    void *dec_buf = ((u8 *)mic_buf) + LADC_MIC_OUT_BUF_LEN;
    cbuf_init(&audio->mic_cbuf, mic_buf, LADC_MIC_OUT_BUF_LEN);
    cbuf_init(&audio->dec_cbuf, dec_buf, DEC_SOURCE_BUF_LEN);
    audio->resume = resume;
    audio->resume_priv = resume_priv;

    err = aic_mic_open();
    if (err) {
        free(audio);
        DBG_LOG("mic open err\n");
        return err;
    }
    err = aic_dec_open();
    if (err) {
        aic_mic_close();
        free(audio);
        DBG_LOG("dec open err\n");
        return err;
    }
    audio->status = 1;
    audio_hdl = audio;
    DBG_LOG("aic_mic_dec_open SUCCESS\n");
    return 0;
}


///////////////////////////////////////////////////////////////////////////////////
// api
#include "aic_srv_voice.h"

#define AIC_AUDIO_TASK_NAME 			"aic_audio"

#define AICXTEK_AUDIO_BUF_LEN			640

enum {
    AIC_AUDIO_TASK_MSG_RUN,
    AIC_AUDIO_TASK_MSG_KILL,
};


struct aicxtek_audio {
    u8 ibuf[AICXTEK_AUDIO_BUF_LEN];
    u8 obuf[AICXTEK_AUDIO_BUF_LEN];
    volatile u8 need_exit;
};
static struct aicxtek_audio *aic_audio = NULL;

static OS_MUTEX	aic_audio_mutex = {0};

// test
static int aic_audio_test_deal(int param)
{
    // 数据处理
    // 此处把mic的数据拷贝到dac输出
    int len;
    os_mutex_pend(&aic_audio_mutex, 0);
    do {
        if (!aic_audio) {
            break;
        }
        len = aic_mic_read_pcm(aic_audio->ibuf, AICXTEK_AUDIO_BUF_LEN);
        if (!len) {
            printf("mic read NULL \n");
            break;
        }
        len = aic_dec_write_pcm(aic_audio->ibuf, AICXTEK_AUDIO_BUF_LEN);
        if (!len) {
            printf("dec write NULL \n");
            break;
        }
        putchar('.');
    } while (0);
    os_mutex_post(&aic_audio_mutex);

    return 0;
}

static void aic_audio_resume(void *priv)
{
    // mic获取数据后激活task处理。
#if 0
    // 此处推送到app_core任务处理
    int argv[3];
    argv[0] = (int)aic_audio_test_deal;
    argv[1] = ARRAY_SIZE(argv) - 2;
    os_taskq_post_type("app_core", Q_CALLBACK, ARRAY_SIZE(argv), argv);
#else
    if (aic_audio && (aic_audio->need_exit == 0)) {
        os_taskq_post_msg(AIC_AUDIO_TASK_NAME, 1, AIC_AUDIO_TASK_MSG_RUN);
    }
#endif
}

static void aic_audio_task_main(void *priv)
{
    int msg[32];
    int ret;

    while (1) {
        ret = os_taskq_pend(NULL, msg, ARRAY_SIZE(msg));
        if (ret != OS_TASKQ) {
            continue;
        }
        if (msg[1] == AIC_AUDIO_TASK_MSG_RUN) {
            putchar('.');
            u32 m_size = AICXTEK_AUDIO_BUF_LEN;
            u32 d_size = AICXTEK_AUDIO_BUF_LEN;
            m_size = aic_mic_read_pcm(aic_audio->ibuf, m_size);
            if (!m_size) {
                DBG_LOG("mic read NULL \n");
                memset(aic_audio->ibuf, 0, AICXTEK_AUDIO_BUF_LEN);
                m_size = AICXTEK_AUDIO_BUF_LEN;
            }
            ret = aic_srv_voice_exchange_data(aic_audio->ibuf, &m_size, aic_audio->obuf, &d_size, 20);
            if (d_size < AICXTEK_AUDIO_BUF_LEN) {
                DBG_LOG("aic read NULL \n");
            }
            if (d_size) {
                d_size = aic_dec_write_pcm(aic_audio->obuf, d_size);
                if (!d_size) {
                    DBG_LOG("dec write NULL \n");
                }
            }
        } else {
            DBG_LOG("aic task exit \n");
            aic_audio->need_exit = 2;
            break;
        }
    }
    while (1) {
        os_time_dly(100000);
    }
}

void aic_audio_init(void)
{
    os_mutex_create(&aic_audio_mutex);
}

void aic_audio_close(void)
{
    DBG_LOG("%s \n", __func__);

    os_mutex_pend(&aic_audio_mutex, 0);
    if (aic_audio) {
        aic_audio->need_exit = 1;
        do {
            os_taskq_post_msg(AIC_AUDIO_TASK_NAME, 1, AIC_AUDIO_TASK_MSG_KILL);
            if (aic_audio->need_exit != 1) {
                break;
            }
            os_time_dly(1);
        } while (1);

        aic_mic_dec_close();

        task_kill(AIC_AUDIO_TASK_NAME);
        free(aic_audio);
        aic_audio = NULL;
    }
    os_mutex_post(&aic_audio_mutex);
}

int aic_audio_open(void)
{
    DBG_LOG("%s \n", __func__);

    os_mutex_pend(&aic_audio_mutex, 0);

    if (aic_audio) {
        DBG_LOG("%s aic_audio is already opened \n", __func__);
        os_mutex_post(&aic_audio_mutex);
        return 0;
    }

    aic_audio = zalloc(sizeof(struct aicxtek_audio));
    ASSERT(aic_audio);

    int err = task_create(aic_audio_task_main, NULL, AIC_AUDIO_TASK_NAME);
    if (err) {
        DBG_LOG("task_create err:%d \n", err);
        free(aic_audio);
        aic_audio = NULL;
    } else {
        aic_mic_dec_open(aic_audio_resume, NULL);
    }

    os_mutex_post(&aic_audio_mutex);

    return err;
}

bool aic_audio_is_run(void)
{
    return aic_audio ? true : false;
}

void aic_audio_input_clear_enable(u8 enable)
{
    if (audio_hdl) {
        audio_hdl->input_clear = enable;
        DBG_LOG("%s,%d, clear:%d \n", __func__, __LINE__, enable);
    }
}

int aic_audio_get_input_clear_status(void)
{
    if (audio_hdl) {
        return audio_hdl->input_clear;
    }
    return 0;
}

static u8 aic_idle_query(void)
{
    return !aic_audio;
}
REGISTER_LP_TARGET(aic_lp_target) = {
    .name = "aic",
    .is_idle = aic_idle_query,
};

#endif /* #if TCFG_CAT1_AICXTEK_ENABLE */

