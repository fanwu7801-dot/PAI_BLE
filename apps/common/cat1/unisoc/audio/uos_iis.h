#ifndef UOS_IIS_H
#define UOS_IIS_H
#define master_iis 0
#if master_iis
#define PCM_BUF_LEN  		(4*1024)
#else
#define PCM_BUF_LEN  		(20*1024)
#endif
#include "app_config.h"
#include "asm/audio_adc.h"
#include "generic/circular_buf.h"
#include "media/audio_decoder.h"
#include "media/pcm_decoder.h"
#include "media/mixer.h"
#include "audio_dec.h"
#include "generic/log.h"
#include "audio_enc/audio_enc.h"
#include "Resample_api.h"
#include "aec_user.h"
#include "audio_link.h"

typedef struct {
    struct adc_mic_ch mic_ch;
    struct audio_stream *stream;		// 音频流
    struct audio_adc_output_hdl adc_output;
    cbuffer_t pcm_cbuf;
    u8 pcm_buf[PCM_BUF_LEN ];
    u8 status;
    struct audio_res_wait wait;
    struct audio_mixer_ch mix_ch;
    struct pcm_decoder pcm_dec;
} usr_mic2dac_hd;
struct esco_iis_mic_hdl {
    cbuffer_t *iis_mic_cbuf;
    s16 iis_rbuf[3][256];
    u8 iis_inbuf_idx;
    u32 in_sample_rate;
    u32 out_sample_rate;
    u32 sr_cal_timer;
    u32 points_total;
    RS_STUCT_API *iis_sw_src_api;
    u8 *iis_sw_src_buf;
};
struct new_iis_hdl {
    ALINK_PARM alink_parm;
    cbuffer_t iis_cbuffer;
    u8 *iis_buf;
    u8 init;
    u8 start;
    u8 module_idx;
    u8 alink_tx_ch; // iis 有四个通道，ch_idx的值可以为0,1,2,3, 设置为0代表iis有1个通道
    u8 alink_rx_ch;
};
enum {
    MIC2DAC_STATUS_STOP = 0,
    MIC2DAC_STATUS_START,
    MIC2DAC_STATUS_PAUSE,   //暂时未添加这个状态
};
int mic_start();
int mic_stop(void);
void iis_open(void);//IIS
void IIS_close(void);
void set_mic_mute(void);
void clear_mic_mute(void);
#endif
