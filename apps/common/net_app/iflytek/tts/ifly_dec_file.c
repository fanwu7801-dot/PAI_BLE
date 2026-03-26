#include "circular_buf.h"
#include "app_config.h"
#include "audio_dec.h"
#include "media/pcm_decoder.h"
#include "tts_main.h"

#if TCFG_IFLYTEK_ENABLE

#define LOG_TAG_CONST       NET_IFLY
#define LOG_TAG             "[IFLY_SOCKET]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
#define LOG_CLI_ENABLE
#include "debug.h"

struct ifly_net_tts_dec {
    struct audio_stream *stream;    // 音频流
    struct audio_decoder decoder;   // 解码句柄
    struct audio_res_wait wait;     // 资源等待句柄
    struct audio_mixer_ch mix_ch;   // 叠加句柄
    u32 id;             // 唯一标识符，随机值
    volatile u8  start;
    int read_offset;
    void *read_priv;
    int (*read)(void *priv, void *buf, u32 len, u8 tmp_flag, int tmp_offset);
};

static struct ifly_net_tts_dec *net_tts = NULL;

static int ifly_net_tts_dec_file_fread(struct audio_decoder *decoder, void *buf, u32 len)
{
    struct ifly_net_tts_dec *dec = container_of(decoder, struct ifly_net_tts_dec, decoder);
    int rlen = 0;

    rlen = dec->read(dec->read_priv, buf, len, !dec->start, dec->read_offset);
    if (rlen != len) {
    }
    if (rlen == -1) {
        audio_decoder_suspend(&dec->decoder);
    }
    return rlen;
}

static int ifly_net_tts_dec_file_fseek(struct audio_decoder *decoder, u32 offset, int seek_mode)
{
    struct ifly_net_tts_dec *dec = container_of(decoder, struct ifly_net_tts_dec, decoder);
    /* printf("seek:%d \n", offset); */
    dec->read_offset = offset;
    return 0;
}

static int ifly_net_tts_dec_file_flen(struct audio_decoder *decoder)
{
    return 0x7fffffff;
}

static const struct audio_dec_input ifly_net_tts_dec_file_input = {
    .coding_type = AUDIO_CODING_MP3,
    .data_type   = AUDIO_INPUT_FILE,
    .ops = {
        .file = {
            .fread = ifly_net_tts_dec_file_fread,
            .fseek = ifly_net_tts_dec_file_fseek,
            .flen  = ifly_net_tts_dec_file_flen,
        }
    }
};


// 解码预处理
static int ifly_net_tts_dec_probe_handler(struct audio_decoder *decoder)
{
    return 0;
}

// 解码后处理
static int ifly_net_tts_dec_post_handler(struct audio_decoder *decoder)
{
    return 0;
}

static const struct audio_dec_handler ifly_net_tts_dec_handler = {
    .dec_probe  = ifly_net_tts_dec_probe_handler,
    .dec_post   = ifly_net_tts_dec_post_handler,
};

// 解码释放
static void ifly_net_tts_dec_release(void)
{
    // 删除解码资源等待
    audio_decoder_task_del_wait(&decode_task, &net_tts->wait);
    // 释放空间
    local_irq_disable();
    free(net_tts);
    net_tts = NULL;
    local_irq_enable();
}

// 解码关闭
static void ifly_net_tts_dec_audio_res_close(void)
{
    if (net_tts->start == 0) {
        log_info("net_tts->start == 0");
        return ;
    }

    // 关闭数据流节点
    net_tts->start = 0;
    audio_decoder_close(&net_tts->decoder);
    audio_mixer_ch_close(&net_tts->mix_ch);

    // 先关闭各个节点，最后才close数据流
    if (net_tts->stream) {
        audio_stream_close(net_tts->stream);
        net_tts->stream = NULL;
    }
    app_audio_state_exit(APP_AUDIO_STATE_MUSIC);
}

// 解码事件处理
static void ifly_net_tts_dec_event_handler(struct audio_decoder *decoder, int argc, int *argv)
{
    switch (argv[0]) {
    case AUDIO_DEC_EVENT_END:
        log_info("AUDIO_DEC_EVENT_END\n");
        if (!net_tts) {
            log_error("net_tts handle err ");
            break;
        }

        if (net_tts->id != argv[1]) {
            log_error("net_tts id err : 0x%x, 0x%x \n", net_tts->id, argv[1]);
            break;
        }

        void ifly_net_tts_dec_close(void);
        ifly_net_tts_dec_close();
        break;
    }
}

static void ifly_net_tts_dec_out_stream_resume(void *p)
{
    struct ifly_net_tts_dec *dec = (struct ifly_net_tts_dec *)p;

    audio_decoder_resume(&dec->decoder);
}


static int ifly_net_tts_dec_dec_start(void)
{
    int err;
    struct audio_fmt *fmt;
    struct ifly_net_tts_dec *dec = net_tts;

    if (!net_tts) {
        return -EINVAL;
    }

    log_info("ifly_net_tts_dec_start: in\n");

    // 打开ifly_net_tts_dec解码
    err = audio_decoder_open(&dec->decoder, &ifly_net_tts_dec_file_input, &decode_task);
    if (err) {
        goto __err1;
    }

    // 设置运行句柄
    audio_decoder_set_handler(&dec->decoder, &ifly_net_tts_dec_handler);

    err = audio_decoder_get_fmt(&dec->decoder, &fmt);
    if (err) {
        goto __err2;
    }

    // 使能事件回调
    audio_decoder_set_event_handler(&dec->decoder, ifly_net_tts_dec_event_handler, dec->id);

    // 设置输出声道类型
    audio_decoder_set_output_channel(&dec->decoder, audio_output_channel_type());

    // 配置mixer通道参数
    audio_mixer_ch_open_head(&dec->mix_ch, &mixer); // 挂载到mixer最前面
    audio_mixer_ch_set_src(&dec->mix_ch, 1, 0);

    // 数据流串联
    struct audio_stream_entry *entries[8] = {NULL};
    u8 entry_cnt = 0;
    entries[entry_cnt++] = &dec->decoder.entry;
    // 添加自定义数据流节点等
    // 最后输出到mix数据流节点
    entries[entry_cnt++] = &dec->mix_ch.entry;
    // 创建数据流，把所有节点连接起来
    dec->stream = audio_stream_open(dec, ifly_net_tts_dec_out_stream_resume);
    audio_stream_add_list(dec->stream, entries, entry_cnt);

    // 设置音频输出类型
    audio_output_set_start_volume(APP_AUDIO_STATE_MUSIC);

    // 开始解码
    dec->start = 1;
    err = audio_decoder_start(&dec->decoder);
    if (err) {
        goto __err3;
    }

    return 0;

__err3:
    dec->start = 0;

    audio_mixer_ch_close(&dec->mix_ch);

    // 先关闭各个节点，最后才close数据流
    if (dec->stream) {
        audio_stream_close(dec->stream);
        dec->stream = NULL;
    }

__err2:
    audio_decoder_close(&dec->decoder);
__err1:
    ifly_net_tts_dec_release();

    return err;
}

static int ifly_net_tts_dec_wait_res_handler(struct audio_res_wait *wait, int event)
{
    int err = 0;

    log_info("ifly_net_tts_dec_wait_res_handler: %d\n", event);

    if (event == AUDIO_RES_GET) {
        // 可以开始解码
        err = ifly_net_tts_dec_dec_start();
    } else if (event == AUDIO_RES_PUT) {
        // 被打断
        if (net_tts->start) {
            ifly_net_tts_dec_audio_res_close();
        }
    }
    return err;
}


void ifly_net_tts_dec_close(void)
{
    if (!net_tts) {
        return ;
    }
    if (net_tts->start) {
        os_time_dly(10);  //等待最后的解码语音播放完成
        ifly_net_tts_dec_audio_res_close();
    }
    ifly_net_tts_dec_release();
}

int ifly_net_tts_dec_open(void *read_priv, int (*read)(void *priv, void *buf, u32 len, u8 tmp_flag, int tmp_offset))
{
    int err;
    ifly_net_tts_dec_close();

    struct ifly_net_tts_dec *dec = zalloc(sizeof(struct ifly_net_tts_dec));
    ASSERT(dec);

    net_tts = dec;

    dec->read_priv = read_priv;
    dec->read = read;
    dec->id = rand32();
    dec->wait.priority = 2;
    dec->wait.preemption = 0;
    dec->wait.snatch_same_prio = 1;
    dec->wait.handler = ifly_net_tts_dec_wait_res_handler;
    err = audio_decoder_task_add_wait(&decode_task, &dec->wait);

    return err;
}

void ifly_net_tts_dec_resume(void *priv)
{
    if (net_tts) {
        audio_decoder_resume(&net_tts->decoder);
    }
}

bool ifly_net_tts_dec_check_run(void)
{
    if (net_tts) {
        return true;
    }
    return false;
}

#endif

