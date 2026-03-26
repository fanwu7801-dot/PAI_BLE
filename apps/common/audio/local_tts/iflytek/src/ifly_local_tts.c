#include "ifly_local_tts.h"
#include "app_config.h"
#include "audio_dec.h"
#include "media/pcm_decoder.h"

#if TCFG_AI_IFLY_LOCAL_TTS_ENABLE

#define LOG_TAG_CONST       APP
#define LOG_TAG             "[IFLY_TTS]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
#define LOG_CLI_ENABLE
#include "debug.h"

#define LOCAL_TTS_TASK_NAME 		"local_tts"

#define IFLY_LOCAL_TTS_APPID		"123"
#define IFLY_LOCAL_TTS_RES_PATH		SDFILE_RES_ROOT_PATH"ivTTS.irf"

#define DEC_SOURCE_DATA_CH_NUM		1
#define DEC_SOURCE_DATA_SR			16000
#define DEC_SOURCE_BUF_LEN			(1024*2)

#define DEC_MUTE_LEN				(DEC_SOURCE_DATA_SR * DEC_SOURCE_DATA_CH_NUM * 2 * 200 / 1000)

enum {
    IFLY_LOCAL_TTS_STATUS_IDLE = 0,
    IFLY_LOCAL_TTS_STATUS_REDAY,
    IFLY_LOCAL_TTS_STATUS_PLAY,
    IFLY_LOCAL_TTS_STATUS_END,
};

struct ifly_local_tts_dec {
    struct audio_stream *stream;	// 音频流
    struct pcm_decoder pcm_dec;		// pcm解码句柄
    struct audio_res_wait wait;		// 资源等待句柄
    struct audio_mixer_ch mix_ch;	// 叠加句柄
    u32 id;				// 唯一标识符，随机值
    volatile u8  status;
};

struct ifly_local_tts {
    OS_MUTEX 	mutex;
    volatile u8 status;
    volatile u8 stop;

    u8    dat_malloc;
    void *dat;
    int   dat_len;
    int   code_page;
    void *event_cb_priv;
    void (*event_cb)(void *priv, int event);

    int mute_len;
    void *out_buf;
    cbuffer_t dec_cbuf;
    struct ifly_local_tts_dec *dec;
};
static struct ifly_local_tts local_tts;

void local_tts_dec_close(void);

static int get_res_addr(void *res_path, int *out_addr, int *out_size)
{
    FILE *file = fopen(res_path, "r");
    if (file == NULL) {
        log_error("ifly_local_tts_init err \n");
        return false;
    }
    struct vfs_attr attr = {0};
    fget_attrs(file, &attr);

    if (out_addr) {
        *out_addr = (int)(attr.sclust);
    }
    if (out_size) {
        *out_size = (int)(attr.fsize);
    }

    log_info("res,addr:%d, 0x%x, size:%d \n", attr.sclust, attr.sclust, attr.fsize);

#if 0
    u8 buf[16];
    memcpy(buf, (void *)attr.sclust, sizeof(16));
    log_info("read res data \n");
    put_buf(buf, sizeof(buf));
#endif

    fclose(file);

    return true;
}

static void local_tts_dec_relaese()
{
    if (local_tts.dec) {
        audio_decoder_task_del_wait(&decode_task, &local_tts.dec->wait);
        local_irq_disable();
        free(local_tts.dec);
        local_tts.dec = NULL;
        local_irq_enable();
    }
}

static void local_tts_dec_event_handler(struct audio_decoder *decoder, int argc, int *argv)
{
    switch (argv[0]) {
    case AUDIO_DEC_EVENT_END:
        if (!local_tts.dec) {
            log_error("dec_hdl handle err ");
            break;
        }

        if (local_tts.dec->id != argv[1]) {
            log_info("dec_hdl id err : 0x%x, 0x%x \n", local_tts.dec->id, argv[1]);
            break;
        }

        local_tts_dec_close();
        //audio_decoder_resume_all(&decode_task);
        break;
    default :
        break;
    }
}

static void local_tts_dec_out_stream_resume(void *p)
{
    struct ifly_local_tts_dec *dec = p;
    audio_decoder_resume(&dec->pcm_dec.decoder);
}


static int local_tts_dec_read(void *hdl, void *data, int len)
{
    int rlen = 0;
    if (local_tts.mute_len) {
        if (local_tts.mute_len > len) {
            local_tts.mute_len -= len;
        } else {
            local_tts.mute_len = 0;
        }
        memset(data, 0, len);
        return len;
    }
    if (local_tts.status) {
        rlen = cbuf_read(&local_tts.dec_cbuf, data, len);
    }
    /* log_info("read:%d,%d \n", len, rlen); */
    return rlen;
}

static int local_tts_dec_start(void)
{
    int err;
    struct ifly_local_tts_dec *dec = local_tts.dec;
    struct audio_mixer *p_mixer = &mixer;

    if (!local_tts.dec) {
        return -EINVAL;
    }

    err = pcm_decoder_open(&dec->pcm_dec, &decode_task);
    if (err) {
        goto __err1;
    }

    pcm_decoder_set_event_handler(&dec->pcm_dec, local_tts_dec_event_handler, dec->id);
    pcm_decoder_set_read_data(&dec->pcm_dec, local_tts_dec_read, NULL);

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
    dec->stream = audio_stream_open(dec, local_tts_dec_out_stream_resume);
    audio_stream_add_list(dec->stream, entries, entry_cnt);

    // 设置音频输出音量
    audio_output_set_start_volume(APP_AUDIO_STATE_MUSIC);

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
    local_tts_dec_relaese();
    return err;
}

static void __local_tts_dec_close(void)
{
    if (local_tts.dec && local_tts.dec->status) {
        local_tts.dec->status = 0;

        pcm_decoder_close(&local_tts.dec->pcm_dec);

        audio_mixer_ch_close(&local_tts.dec->mix_ch);
#if SYS_DIGVOL_GROUP_EN
        sys_digvol_group_ch_close("music_aic");
#endif // SYS_DIGVOL_GROUP_EN

        // 先关闭各个节点，最后才close数据流
        if (local_tts.dec->stream) {
            audio_stream_close(local_tts.dec->stream);
            local_tts.dec->stream = NULL;
        }
    }
}

static int aic_wait_res_handler(struct audio_res_wait *wait, int event)
{
    int err = 0;
    log_info("aic_wait_res_handler, event:%d\n", event);
    if (event == AUDIO_RES_GET) {
        // 启动解码
        err = local_tts_dec_start();
    } else if (event == AUDIO_RES_PUT) {
        // 被打断
        __local_tts_dec_close();
    }

    return err;
}


void local_tts_dec_resume(void)
{
    if (local_tts.dec && local_tts.dec->status) {
        audio_decoder_resume(&local_tts.dec->pcm_dec.decoder);
    }
}

int local_tts_dec_write_pcm(void *data, int len)
{
    int wlen = 0;
    if (local_tts.status) {
        wlen = cbuf_write(&local_tts.dec_cbuf, data, len);
        local_tts_dec_resume();
    }
    /* log_info("write:%d,%d \n", len, wlen); */
    return wlen;
}

void local_tts_dec_close(void)
{
    log_info("%s \n", __func__);
    if (local_tts.dec) {
        __local_tts_dec_close();
        local_tts_dec_relaese();
        log_info("aic dec close \n\n ");
    }
}

int local_tts_dec_open(void)
{
    if (local_tts.dec) {
        local_tts_dec_close();
    }
    log_info("%s \n", __func__);

    int err;
    struct ifly_local_tts_dec *dec;
    dec = zalloc(sizeof(*dec));
    if (!dec) {
        return -ENOMEM;
    }

    dec->pcm_dec.ch_num = DEC_SOURCE_DATA_CH_NUM;
    dec->pcm_dec.sample_rate = DEC_SOURCE_DATA_SR;
    dec->pcm_dec.output_ch_num = audio_output_channel_num();
    dec->pcm_dec.output_ch_type = audio_output_channel_type();
    log_info("in ch:%d, sr:%d, out ch:%d, type:0x%x \n", dec->pcm_dec.ch_num, dec->pcm_dec.sample_rate,
             dec->pcm_dec.output_ch_num, dec->pcm_dec.output_ch_type);

    dec->wait.priority = 2;
    dec->wait.preemption = 0;
    dec->wait.snatch_same_prio = 1;
    dec->wait.handler = aic_wait_res_handler;

    dec->id = rand32();
    local_tts.dec = dec;

    err = audio_decoder_task_add_wait(&decode_task, &dec->wait);
    return err;
}


static int userCallback(void *context, int msg, int param1, int param2, int param3, const void *param4)
{
    char *ptr = (char *)param4;
    int len = param3;
    int wlen = 0;
    /* log_info("userCallback:%d \n", len); */
    while (len) {
        wlen = local_tts_dec_write_pcm(ptr, len);
        if (wlen != len) {
            /* putchar('W'); */
            os_time_dly(1);
        }
        ptr += wlen;
        len -= wlen;

        if (local_tts.stop) {
            return -1;
        }
    }

    //返回非0时，将终止合成。ttsTextPut接口返回2
    return 0;
}

static void local_tts_task_kill(void *priv)
{
    os_mutex_pend(&local_tts.mutex, 0);
    if (local_tts.status == IFLY_LOCAL_TTS_STATUS_END) {
        log_info("local_tts_task_kill \n");
        task_kill(LOCAL_TTS_TASK_NAME);
    }
    os_mutex_post(&local_tts.mutex);
}

static void local_tts_main(void *priv)
{
    void *tts_res = priv;

    local_tts.out_buf = malloc(DEC_SOURCE_BUF_LEN);
    if (!local_tts.out_buf) {
        goto __exit;
    }
    cbuf_init(&local_tts.dec_cbuf, local_tts.out_buf, DEC_SOURCE_BUF_LEN);
    local_tts.mute_len = DEC_MUTE_LEN;

    int val = ttsSessionBegin(IFLY_LOCAL_TTS_APPID, userCallback, tts_res);
    log_info("ttsSessionBegin val is %d \n", val);

    val = ttsSetParam(ifly_TTS_PARAM_INPUT_CODEPAGE, local_tts.code_page);
    log_info("ttsSetParam val is %d \n", val);

    local_tts_dec_open();

    local_tts.status = IFLY_LOCAL_TTS_STATUS_PLAY;

    val = ttsTextPut(local_tts.dat, local_tts.dat_len);
    log_info("ttsTextPut val is %d \n", val);

    val = ttsSessionEnd();
    log_info("ttsSessionEnd val is %d \n", val);

__exit:

    if (local_tts.dec) {
        local_tts_dec_close();
        local_tts.dec = NULL;
    }
    if (local_tts.out_buf) {
        free(local_tts.out_buf);
        local_tts.out_buf = NULL;
    }
    if (local_tts.dat_malloc && local_tts.dat) {
        free(local_tts.dat);
        local_tts.dat = NULL;
    }

    local_tts.status = IFLY_LOCAL_TTS_STATUS_END;

    int event = IFLY_LOCAL_TTS_EVENT_ERR_DEC_END;
    if (local_tts.stop) {
        event = IFLY_LOCAL_TTS_EVENT_ERR_DEC_STOP;
    } else {
        sys_timeout_add(NULL, local_tts_task_kill, 1);
    }
    if (local_tts.event_cb) {
        local_tts.event_cb(local_tts.event_cb_priv, event);
    }

    while (1) {
        os_time_dly(1000);
    }
}

static void local_tts_release(void)
{
    if (local_tts.status == 0) {
        return ;
    }
    local_tts.stop = 1;
    while (local_tts.status != IFLY_LOCAL_TTS_STATUS_END) {
        os_time_dly(1);
    }
    task_kill(LOCAL_TTS_TASK_NAME);
    /* memset(&local_tts, 0, sizeof(struct ifly_local_tts)); */
    local_tts.status = 0;
    local_tts.stop = 0;
}


void ifly_local_tts_stop(void)
{
    os_mutex_pend(&local_tts.mutex, 0);
    local_tts_release();
    os_mutex_post(&local_tts.mutex);
}


int ifly_local_tts_play(char *dat, int dat_len, int code_page, u8 dat_malloc, void *cb_priv, void (*cb)(void *priv, int event))
{
    int ret = IFLY_LOCAL_TTS_EVENT_OK;
    os_mutex_pend(&local_tts.mutex, 0);
    local_tts_release();

    int out_addr = 0;
    if (false == get_res_addr(IFLY_LOCAL_TTS_RES_PATH, &out_addr, NULL)) {
        ret = IFLY_LOCAL_TTS_EVENT_ERR_OPEN_RES;
        goto __exit;
    }

    if (dat_malloc) {
        local_tts.dat = malloc(dat_len);
        if (local_tts.dat == NULL) {
            ret = IFLY_LOCAL_TTS_EVENT_ERR_MALLOC;
            goto __exit;
        }
        memcpy(local_tts.dat, dat, dat_len);
    } else {
        local_tts.dat = dat;
    }
    local_tts.dat_len = dat_len;
    local_tts.dat_malloc = dat_malloc;
    local_tts.code_page = code_page;
    local_tts.event_cb_priv = cb_priv;
    local_tts.event_cb = cb;

    local_tts.status = IFLY_LOCAL_TTS_STATUS_REDAY;

    int err = task_create(local_tts_main, (void *)out_addr, LOCAL_TTS_TASK_NAME);
    if (err) {
        ret = IFLY_LOCAL_TTS_EVENT_ERR_TASK_CREATE;
        goto __exit;
    }

__exit:
    os_mutex_post(&local_tts.mutex);

    if (ret) {
        if (cb) {
            cb(cb_priv, ret);
        }
    }

    return ret;
}

static u8 local_tts_idle_query(void)
{
    return !local_tts.status;
}
REGISTER_LP_TARGET(local_tts_lp_target) = {
    .name = "local_tts",
    .is_idle = local_tts_idle_query,
};


int ifly_local_tts_init(void)
{
    os_mutex_create(&local_tts.mutex);

    return true;
}
module_initcall(ifly_local_tts_init);


#if 0 // test
void ifly_local_tts_test(void)
{
    static u8 flag = 0;
    flag = !flag;

    mem_stats();

    int out_addr = 0;
    int out_size = 0;

    if (false == get_res_addr(SDFILE_RES_ROOT_PATH"test.txt", &out_addr, &out_size)) {
        return ;
    }

    if (flag) {
        // 内存不用释放
        ifly_local_tts_play((void *)out_addr, out_size, ifly_TTS_CODEPAGE_UTF8, 0, NULL, NULL);
    } else {
        // 内存要释放
        void *dat = malloc(out_size);
        ASSERT(dat);
        memcpy(dat, out_addr, out_size);
        ifly_local_tts_play(dat, out_size, ifly_TTS_CODEPAGE_UTF8, 1, NULL, NULL);
        free(dat);
    }
}
#endif

#endif /* #if TCFG_AI_IFLY_LOCAL_TTS_ENABLE */

