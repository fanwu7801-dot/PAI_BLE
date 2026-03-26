#include "audio_iis.h"
#include "includes.h"
#include "audio_link.h"
#include "debug.h"
#include "sound/sound.h"
#include "circular_buf.h"
#include "gpio.h"
#include "asm/iis.h"
#include "uos_iis.h"
#include "audio_splicing.h"


#if TCFG_CAT1_UNISOC_ENABLE

#define IIS_OUTPUT_CBUF_LEN (4 * 1024)
#define MIC_SR 16000
#define DEBUG_DATA 0

#if DEBUG_DATA
short tsin_441k2[512];
#endif
cbuffer_t *adc_cbuffer = NULL; // adc解码数据的发送和解码buf
static void *adc_buf;
static u16 read_buf[2 * 1024]; // 复制的中转数据
static u8 first_flag = 0;
static u8 mute_mic = 0;
static u8 pcm_dec_maigc = 0;
static struct new_iis_hdl *iis = NULL;
static usr_mic2dac_hd *mci2dac_hdl = NULL;
static volatile u8 slave_dir = 0;


void IIS_close()
{
    slave_dir = 0;
    if (first_flag == 1) {
        first_flag = 0;
    } else {
        return;
    }
    if (iis->start == 1) {
        ALINK_EN(hw_alink_parm->module, 0);
        alink_uninit(&iis->alink_parm);
        iis->init = 1;
        iis->start = 0;
    }
    mic_stop();
    free(iis->iis_buf);
    free(adc_buf);
    free(adc_cbuffer);
    iis->iis_buf = NULL;
    first_flag = 0;
}

static void alink_iis_init1(int dma_size, int sr)
{
    if (iis) {
        printf("IIS have init!!!");
        return;
    }

    struct new_iis_hdl *iis_hdl = zalloc(sizeof(struct new_iis_hdl));
    if (iis_hdl) {
        iis = iis_hdl;
        iis->module_idx = ALINK0;

#if DEBUG_DATA
        iis->iis_buf = malloc(IIS_OUTPUT_CBUF_LEN);
        cbuf_init(&iis->iis_cbuffer, iis->iis_buf, IIS_OUTPUT_CBUF_LEN);
#endif
#if master_iis
        iis->alink_tx_ch = 1;
        iis->alink_rx_ch = 0;
#else
        iis->alink_tx_ch = 1;
        iis->alink_rx_ch = 0;
#endif
        iis->alink_parm.mode = ALINK_MD_IIS;
        iis->alink_parm.clk_mode = ALINK_CLK_FALL_UPDATE_RAISE_SAMPLE;
        iis->alink_parm.bitwide = ALINK_LEN_16BIT;
        iis->alink_parm.sclk_per_frame = ALINK_FRAME_32SCLK;
        iis->alink_parm.buf_mode = ALINK_BUF_CIRCLE;
        iis->alink_parm.dma_len = dma_size;
        iis->alink_parm.sample_rate = sr;
        iis->alink_parm.module = iis->module_idx;
        iis->alink_parm.mclk_io = IO_PORTA_15;
        iis->alink_parm.lrclk_io = IO_PORTB_00;
        iis->alink_parm.sclk_io = IO_PORTB_01;

#if master_iis
        /* iis->alink_parm.ch_cfg[iis->alink_tx_ch].data_io = IO_PORTA_12; */
        iis->alink_parm.ch_cfg[iis->alink_tx_ch].data_io = IO_PORTB_02;
        iis->alink_parm.ch_cfg[iis->alink_rx_ch].data_io = IO_PORTB_03;
#else
        iis->alink_parm.ch_cfg[iis->alink_tx_ch].data_io = IO_PORTB_02;
        iis->alink_parm.ch_cfg[iis->alink_rx_ch].data_io = IO_PORTB_03;

#endif
#if master_iis
        iis->alink_parm.role = ALINK_ROLE_MASTER; // 主机
#else
        iis->alink_parm.role = ALINK_ROLE_SLAVE; // 从机
#endif
    }
    iis->init = 1;
    iis->start = 0;
}

static void alink_iis_start1(void *rx_priv, void *tx_priv, void (*rx_handle)(void *priv, void *addr, int len), void (*tx_handle)(void *priv, void *addr, int len))
{
    static u8 num = 0x1;
    if (iis) {
        if (iis->start == 0 && iis->init == 1) {
            void *hw_alink1 = alink_init(&iis->alink_parm);
            if (rx_handle != NULL) {
                alink_channel_init(hw_alink1, iis->alink_rx_ch, ALINK_DIR_RX, rx_priv, rx_handle);
            }
            if (tx_handle != NULL) {
                alink_channel_init(hw_alink1, iis->alink_tx_ch, ALINK_DIR_TX, tx_priv, tx_handle);
                ALINK_OPNS_SET(0, 256);
            }
            alink_start(hw_alink1);
            iis->start = 1;
        }
    }
}

static int audio_iis_output_write1(s16 *data, u32 len)
{
    int wlen = 0;
    u8 *u8_data = (u8 *)data;
    if (iis) {
        wlen = cbuf_write(&(iis->iis_cbuffer), u8_data, len);
    }
    return wlen;
}

static void iis_rx_handle_demo(void *priv, void *buf, int len)
{
    static i = 0;
    //  putchar('s');
#if DEBUG_DATA
    cbuf_write(&(iis->iis_cbuffer), (buf), len);
#endif
    int rlen = cbuf_write(&mci2dac_hdl->pcm_cbuf, (buf), len);
    alink_set_shn(&iis->alink_parm.ch_cfg[iis->alink_rx_ch], rlen / 4);
#if !master_iis
    audio_decoder_resume(&mci2dac_hdl->pcm_dec.decoder);
#else
    // audio_decoder_resume(&mci2dac_hdl->pcm_dec.decoder);
#endif
}

static void handle_tx_demo(void *priv, void *buf, int len)
{
    int wlen = cbuf_get_data_size(adc_cbuffer);
    if (wlen > 1536) {
        cbuf_read_alloc_len_updata(adc_cbuffer, (wlen - 1024));
        wlen = 1024;
    }
    if (!mute_mic) {
        int rlen = cbuf_read(adc_cbuffer, buf, wlen);
        if (rlen != 0) {
            alink_set_shn(&iis->alink_parm.ch_cfg[iis->alink_tx_ch], rlen / 4);
        } else {
            alink_set_shn(&iis->alink_parm.ch_cfg[iis->alink_tx_ch], 256);
        }
    } else {
        cbuf_read_alloc_len_updata(adc_cbuffer, wlen);
        //memccpy(buf)
        memset(buf, 0, 1024);
        alink_set_shn(&iis->alink_parm.ch_cfg[iis->alink_tx_ch], 256);
    }

    // printf("alink_get_swptr=%x",alink_get_swptr(&iis->alink_parm.ch_cfg[iis->alink_tx_ch]));

    return;
}



static void task_data_output()
{
    u16 rlen;
    while (1) {
        rlen = cbuf_get_data_size(&(iis->iis_cbuffer));
        rlen = cbuf_read(&(iis->iis_cbuffer), read_buf, rlen);
        if (rlen > 0) {
            put_buf(read_buf, rlen);
            printf("[msg]>>>>>>>>>>>rlen=%d", rlen);
            // rlen = cbuf_get_data_size(&mci2dac_hdl->pcm_cbuf);
        }
        // printf("\n [ERROR] %s -[yuyu] %d\n", __FUNCTION__, __LINE__);
        // os_taskq_pend(NULL, msg, 4);
        os_time_dly(1);
    }
}

void iis_open(void)
{

    if (first_flag == 1) {
        return;
    } else {
        first_flag = 1;
    }
    slave_dir = 1;
    adc_cbuffer = zalloc(sizeof(cbuffer_t));
    adc_buf = malloc(IIS_OUTPUT_CBUF_LEN);
    if (adc_buf != NULL) {
        cbuf_init(adc_cbuffer, adc_buf, IIS_OUTPUT_CBUF_LEN);
    } else {
        printf("err!@!!!");
    }
    mic_start();
    u16 vol = app_audio_get_max_volume();
    app_audio_volume_set(vol);
#if master_iis
    alink_iis_init1(1024 * 4, MIC_SR);
#else
    alink_iis_init1(1024 * 4, MIC_SR);
#endif
#if master_iis
    alink_iis_start1(NULL, NULL, iis_rx_handle_demo, handle_tx_demo);
    printf("\n [ERROR]master %s -[yuyu] %d\n", __FUNCTION__, __LINE__);
#else
    alink_iis_start1(NULL, NULL, iis_rx_handle_demo, handle_tx_demo);
    printf("\n [ERROR]slave %s -[yuyu] %d\n", __FUNCTION__, __LINE__);
#endif
#if DEBUG_DATA
    os_task_create(task_data_output,
                   NULL,
                   1,
                   1024,
                   128,
                   "iis_TEXT");
#endif
}

static int pcm_dec_data_handler(struct audio_stream_entry *entry,
                                struct audio_data_frame *in,
                                struct audio_data_frame *out)
{
    struct audio_decoder *decoder = container_of(entry, struct audio_decoder, entry);
    audio_stream_run(&decoder->entry, in);
    return decoder->process_len;
}

/*----------------------------------------------------------------------------*/
/**@brief    pcm解码数据流激活
   @param    *p: 私有句柄
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
static void dec_out_stream_resume(void *p)
{
    usr_mic2dac_hd *dec = (usr_mic2dac_hd *)p;

    audio_decoder_resume(&dec->pcm_dec.decoder);
}

/*******************************************************************************************

*******************************************************************************************/
static int pcm_fread(void *hdl, void *buf, int len)
{
    usr_mic2dac_hd *dec = (usr_mic2dac_hd *)hdl;
    int rlen = cbuf_read(&dec->pcm_cbuf, buf, len);
    return rlen;
}

/*******************************************************************************************

*******************************************************************************************/
static void pcm_dec_event_handler(struct audio_decoder *decoder, int argc, int *argv)
{
    printf("\n [ERROR] %s -[yuyu] %d\n", __FUNCTION__, __LINE__);
    switch (argv[0]) {
    case AUDIO_DEC_EVENT_END:
        if ((u8)argv[1] != (u8)(pcm_dec_maigc - 1)) {
            printf("maigc err, %s\n", __FUNCTION__);
            break;
        }
        /* pcm_dec_close(); */
        break;
    }
}
static int pcm_dec_start(void)
{
    int err;
    usr_mic2dac_hd *dec = mci2dac_hdl;

    printf("[%s]", __FUNCTION__);
    err = pcm_decoder_open(&dec->pcm_dec, &decode_task);
    if (err) {
        goto __err1;
    }

    pcm_decoder_set_event_handler(&dec->pcm_dec, pcm_dec_event_handler, 0);
    pcm_decoder_set_read_data(&dec->pcm_dec, pcm_fread, dec);
    pcm_decoder_set_data_handler(&dec->pcm_dec, pcm_dec_data_handler);

    audio_mixer_ch_open(&dec->mix_ch, &mixer);
    audio_mixer_ch_set_src(&dec->mix_ch, 1, 1);
    /* audio_mixer_ch_set_no_wait(&dec->mix_ch, 1, 10); // 超时自动丢数 */

    // 数据流串联
    struct audio_stream_entry *entries[4] = {NULL};
    u8 entry_cnt = 0;
    entries[entry_cnt++] = &dec->pcm_dec.decoder.entry;
    entries[entry_cnt++] = &dec->mix_ch.entry;
    dec->stream = audio_stream_open(dec, dec_out_stream_resume);
    audio_stream_add_list(dec->stream, entries, entry_cnt);

    audio_output_set_start_volume(APP_AUDIO_STATE_CALL);

    /* audio_adc_mic_start(&dec->mic_ch); */
    //    mic_debug("\n\n audio decoder start \n");
    dec->status = MIC2DAC_STATUS_START;
    err = audio_decoder_start(&dec->pcm_dec.decoder);
    if (err) {
        goto __err3;
    }
    //    mic_debug("\n\n audio mic start  1 \n");
    return 0;
__err3:

    dec->status = 0;
    audio_mic_close(&dec->mic_ch, &dec->adc_output);
    if (dec->stream) {
        audio_stream_close(dec->stream);
        dec->stream = NULL;
    }
    pcm_decoder_close(&dec->pcm_dec);
__err1:
    audio_decoder_task_del_wait(&decode_task, &dec->wait);
    return err;
}
/*******************************************************************************************
    pcm解码资源回调
*******************************************************************************************/
static int usr_pcmdec_wait_res_handler(struct audio_res_wait *wait, int event)
{
    int err = 0;
    printf("[%s]", __FUNCTION__);
    if (!mci2dac_hdl) {
        printf("struct mci2dac_hd1 err !");
        return -1;
    }
    if (event == AUDIO_RES_GET) {
        if (mci2dac_hdl->status == MIC2DAC_STATUS_STOP) {
            err = pcm_dec_start();
        } else if (mci2dac_hdl->status == MIC2DAC_STATUS_PAUSE) {
            mci2dac_hdl->status = MIC2DAC_STATUS_START;
        }
    } else if (event == AUDIO_RES_PUT) {
        if (mci2dac_hdl->status == MIC2DAC_STATUS_START) {
            /* mci2dac_hdl->status = MIC2DAC_STATUS_PAUSE; */
        }
    }
    return err;
}
/*******************************************************************************************
acd 数据输出回调函数
*******************************************************************************************/
static void usr_mic2dac_output_handler(void *priv, void *data, int len)
{

    volatile u32 wlen;
    void *double_date = cbuf_write_alloc(adc_cbuffer, &wlen);
    if (wlen < len * 2) {
        ASSERT(0, "CBUF NO ENOUGH MEMORY!!!");
    }
#if !DEBUG_DATA

    pcm_single_to_dual(double_date, data, len);
    cbuf_write_updata(adc_cbuffer, len * 2);
#else
    pcm_single_to_dual(double_date, tsin_441k2, len);
    cbuf_write_updata(adc_cbuffer, len * 2);
#endif
    if (!wlen) {

        putchar('W');
    }
}
#if DEBUG_DATA
void demo_num()
{
    //static  int num=0x0100;
    int num = 0;
    for (int i = 0; i < 512; i++) {
        tsin_441k2[i] = num++;
        if (num >= 256) {
            num = 0;
        }
    }
}
#endif
int mic_start()
{
#if DEBUG_DATA
    demo_num();
#endif
    u16 ladc_sr = MIC_SR;
    u8 ladc_gain = 13;
    if (!(mci2dac_hdl)) {
        mci2dac_hdl = zalloc(sizeof(usr_mic2dac_hd));
    }

    if (mci2dac_hdl) {
        cbuf_init(&mci2dac_hdl->pcm_cbuf, mci2dac_hdl->pcm_buf, sizeof(mci2dac_hdl->pcm_buf));
#if master_iis

        // open mic
        if (audio_mic_open(&mci2dac_hdl->mic_ch, ladc_sr, ladc_gain) == 0) {
            mci2dac_hdl->adc_output.handler = usr_mic2dac_output_handler;
            audio_mic_add_output(&mci2dac_hdl->adc_output);
            audio_mic_start(&mci2dac_hdl->mic_ch);
            // mci2dac_hdl->status = MIC2DAC_STATUS_START;
        }
#else
        if (audio_mic_open(&mci2dac_hdl->mic_ch, ladc_sr, ladc_gain) == 0) {
            mci2dac_hdl->adc_output.handler = usr_mic2dac_output_handler;
            audio_mic_add_output(&mci2dac_hdl->adc_output);
            audio_mic_start(&mci2dac_hdl->mic_ch);
            // mci2dac_hdl->status = MIC2DAC_STATUS_START;
        }

        mci2dac_hdl->pcm_dec.ch_num = 2;
        mci2dac_hdl->pcm_dec.output_ch_num = audio_output_channel_num();
        mci2dac_hdl->pcm_dec.output_ch_type = audio_output_channel_type();
        mci2dac_hdl->pcm_dec.sample_rate = ladc_sr;

        mci2dac_hdl->wait.priority = 0;
        mci2dac_hdl->wait.preemption = 0;
        mci2dac_hdl->wait.protect = 1;
        mci2dac_hdl->wait.handler = usr_pcmdec_wait_res_handler;

        int ret = audio_decoder_task_add_wait(&decode_task, &mci2dac_hdl->wait);
        if (ret == 0) {
            mci2dac_hdl->status = MIC2DAC_STATUS_START;
            return 0;
        }
        printf("[%s] decoder task add wait err !", __FUNCTION__);
        audio_mic_close(&mci2dac_hdl->mic_ch, &mci2dac_hdl->adc_output);
        mci2dac_hdl->status = MIC2DAC_STATUS_STOP;
        return -1;
#endif
    } else {
        printf("[%s] zalloc err !", __FUNCTION__);
        return -1;
    }

    return 0;
}

/*******************************************************************************************
    pcm 解码停止
*******************************************************************************************/
static int pcm_dec_stop(void)
{
    audio_decoder_close(&mci2dac_hdl->pcm_dec.decoder);
    audio_mixer_ch_close(&mci2dac_hdl->mix_ch);
    if (mci2dac_hdl->stream) {
        audio_stream_close(mci2dac_hdl->stream);
        mci2dac_hdl->stream = NULL;
    }
    // pcm_decoder_close(&mci2dac_hdl->pcm_dec);
    return 0;
}

int mic_stop(void)
{
    printf("[%s]!", __FUNCTION__);
    if (mci2dac_hdl) {
        mci2dac_hdl->status == MIC2DAC_STATUS_STOP;
        // 关闭mic
        audio_mic_close(&mci2dac_hdl->mic_ch, &mci2dac_hdl->adc_output);

        // 关闭解码
        pcm_dec_stop();

        // 删除解码资源
        audio_decoder_task_del_wait(&decode_task, &mci2dac_hdl->wait);

        free(mci2dac_hdl);
        mci2dac_hdl = NULL;
    }
    return 0;
}
void set_mic_mute()
{
    mute_mic = 1;
}
void clear_mic_mute()
{
    mute_mic = 0;
}
static u8 iis_idle_query(void)
{
    return !slave_dir;
}

REGISTER_LP_TARGET(uos_iis_lp_target) = {
    .name = "uos_iis_slave",
    .is_idle = iis_idle_query,
};
#endif
