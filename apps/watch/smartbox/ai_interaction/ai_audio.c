
#include "ai_interaction/ai_audio.h"
#include "media/includes.h"
#include "audio_config.h"
#include "JL_rcsp_protocol.h"

#if TCFG_AI_INTERACTION_ENABLE

#define AI_AUDIO_SAVE_TEST				0

#pragma pack(1)
struct _AI_AUDIO_REC_START_PAYLOAD {
    u8 type;	// 编码类型
    u8 sr;		// 编码采样率
    u8 way;		// 断句端：0-设备，1-APP
};
struct _AI_AUDIO_REC_STOP_PAYLOAD {
    u8 stop_reason; //断句原因：0-正常，1-取消
    u8 config;		//参数
};
#pragma pack()

#define APP_REC_STOP_REASON_OK			(0) //断句原因，正常
#define APP_REC_STOP_REASON_CANCEL		(1) //断句原因，取消

#define APP_REC_STOP_CFG_LOCAL_TEXT		BIT(0) //是否下发识别文本
#define APP_REC_STOP_CFG_AI_TEXT		BIT(1) //是否下发AI文本
#define APP_REC_STOP_CFG_AI_TTS			BIT(2) //是否下发AI语音

#define APP_RECORDING_PCM 				0
#define APP_RECORDING_SPEEX				1
#define APP_RECORDING_OPUS				2

#define APP_RECORDING_8K 				0x08
#define APP_RECORDING_16K 				0x10

#define APP_RECORDING_DEVICE 			0
#define APP_RECORDING_APP				1

#if TCFG_ENC_OPUS_ENABLE
#define AI_AUDIO_CODING_TYPE			AUDIO_CODING_OPUS // 编码格式
#else
#error "ONLY SUPPORT OPUS"
#endif

#define AI_AUDIO_CODING_SR				16000 // 采样率。和audio_mic_enc_open()函数中的对应

#define AI_AUDIO_STOP_CFG				(APP_REC_STOP_CFG_LOCAL_TEXT | APP_REC_STOP_CFG_AI_TEXT | APP_REC_STOP_CFG_AI_TTS)

#define AI_AUDIO_SEND_TIMEOUT_MS		1000 // 发送超时

struct _AI_AUDIO {
    void (* evt_cb)(int event);
};
static struct _AI_AUDIO ai_audio;

#if AI_AUDIO_SAVE_TEST
static FILE *save_file = NULL;
#endif

extern int ai_mic_is_busy(void); //mic正在被使用
extern int ai_mic_rec_start(void); //启动mic和编码模块
extern int ai_mic_rec_close(void); //停止mic和编码模块

extern int mic_rec_pram_init(/* const char **name,  */u32 enc_type, u8 opus_type, u16(*speech_send)(u8 *buf, u16 len), u16 frame_num, u16 cbuf_size);


static u16 ai_audio_APP_rec_send_data(u8 *voice_buf, u16 voice_len)
{
    log_c('A');
#if AI_AUDIO_SAVE_TEST
    if (save_file) {
        int wlen = fwrite(save_file, voice_buf, voice_len);
        if (wlen != voice_len) {
            log_e("save file err: %d, %d\n", wlen, voice_len);
        }
    }
#endif
    int ret = 0;
    ret = JL_DATA_send(JL_OPCODE_DATA, JL_OPCODE_APP_RECORDING, voice_buf, voice_len, JL_NOT_NEED_RESPOND);
    if (ret) {
        log_e("send data err: %d, %d\n", ret, voice_len);
        if (ai_audio.evt_cb) {
            ai_audio.evt_cb(AI_AUDIO_EVENT_ERR_REC_DATA);
        }
    }
    return 0;
}

static int ai_audio_APP_opcode_rec_start(void)
{
    int to = 0;
    int result;
    struct _AI_AUDIO_REC_START_PAYLOAD rec_start = {0};
    rec_start.way = APP_RECORDING_DEVICE;

#if (AI_AUDIO_CODING_TYPE == AUDIO_CODING_OPUS)
    rec_start.type = APP_RECORDING_OPUS;
#else
#error "AI_AUDIO_CODING_TYPE error"
#endif

#if (AI_AUDIO_CODING_SR == 16000)
    rec_start.sr = APP_RECORDING_16K;
#elif (AI_AUDIO_CODING_SR == 8000)
    rec_start.sr = APP_RECORDING_8K;
#else
#error "AI_AUDIO_CODING_SR error"
#endif

__send:
    result = JL_CMD_send(JL_OPCODE_APP_RECORDING, (u8 *)&rec_start, sizeof(struct _AI_AUDIO_REC_START_PAYLOAD), JL_NEED_RESPOND);
    if (result) {
        if (result == JL_ERR_SEND_BUSY) {
            if (to > AI_AUDIO_SEND_TIMEOUT_MS) {
                log_e("send timeout\n");
                return false;
            }
            log_i("send busy \n");
            os_time_dly(1);
            to += 10;
            goto __send;
        }
        log_e("send err:%d \n", result);
        return false;
    }

    return true;
}

static int ai_audio_APP_opcode_rec_stop(int cancel)
{
    int to = 0;
    int result;
    struct _AI_AUDIO_REC_STOP_PAYLOAD rec_stop = {0};

    if (cancel) {
        rec_stop.stop_reason = APP_REC_STOP_REASON_CANCEL;
        rec_stop.config = 0;
    } else {
        rec_stop.stop_reason = APP_REC_STOP_REASON_OK;
        rec_stop.config = AI_AUDIO_STOP_CFG;
    }

__send:
    result = JL_CMD_send(JL_OPCODE_APP_RECORD_END, (u8 *)&rec_stop, sizeof(struct _AI_AUDIO_REC_STOP_PAYLOAD), JL_NEED_RESPOND);
    if (result) {
        if (result == JL_ERR_SEND_BUSY) {
            if (to > AI_AUDIO_SEND_TIMEOUT_MS) {
                log_e("send timeout\n");
                return false;
            }
            log_i("send busy \n");
            os_time_dly(1);
            to += 10;
            goto __send;
        }
        log_e("send err:%d \n", result);
        return false;
    }
    return true;
}

int ai_audio_stop(int cancel)
{
    if (!ai_mic_is_busy()) {
        log_w("ai_mic_is_null \n\n");
        return true;
    }
    ai_mic_rec_close();
#if AI_AUDIO_SAVE_TEST
    if (save_file) {
        fclose(save_file);
        save_file = NULL;
    }
#endif
    if (!ai_audio_APP_opcode_rec_stop(cancel)) {
        log_e("ai_audio_APP_opcode_rec_stop err \n\n");
        if (ai_audio.evt_cb) {
            ai_audio.evt_cb(AI_AUDIO_EVENT_ERR_REC_STOP);
        }
        return false;
    }
    return true;
}

int ai_audio_start(void)
{
    if (ai_mic_is_busy()) {
        log_w("ai_mic_is_busy \n\n");
        return false;
    }
    if (!ai_audio_APP_opcode_rec_start()) {
        log_e("ai_audio_APP_opcode_rec_start err \n\n");
        if (ai_audio.evt_cb) {
            ai_audio.evt_cb(AI_AUDIO_EVENT_ERR_REC_START);
        }
        return false;
    }
#if AI_AUDIO_SAVE_TEST
    if (save_file) {
        fclose(save_file);
        save_file = NULL;
    }
    save_file = fopen("storage/sd0/C/sf.bin", "w+");
    if (!save_file) {
        log_e("fopen err \n\n");
    }
#endif
#if (AI_AUDIO_CODING_TYPE == AUDIO_CODING_OPUS) && (AI_AUDIO_CODING_SR == 16000)
    mic_rec_pram_init(AI_AUDIO_CODING_TYPE, 0, ai_audio_APP_rec_send_data, 4, 1024 * 4);
#else
    mic_rec_pram_init(AI_AUDIO_CODING_TYPE, 0, ai_audio_APP_rec_send_data, 4, 1024);
#endif
    ai_mic_rec_start();
    return true;
}

int ai_audio_init(void (* evt_cb)(int event))
{
    memset(&ai_audio, 0, sizeof(struct _AI_AUDIO));
    ai_audio.evt_cb = evt_cb;
    return true;
}

#endif
