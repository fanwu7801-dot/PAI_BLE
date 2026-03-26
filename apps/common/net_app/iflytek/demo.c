#include "ifly_socket.h"
#include "authentication.h"
#include "sparkdesk_main.h"
#include "tts_main.h"
#include "vad_main.h"
#include "system/timer.h"
#include "app_config.h"

#if TCFG_IFLYTEK_ENABLE

#define IFLY_DEMO_TEST    0

#if IFLY_DEMO_TEST

struct ifly_net_struct {
    // vad
    ifly_vad_param vad_param;

    // ai
    ifly_ai_param ai_param;

    // tts
    ifly_tts_param tts_param;

    // other
    char local_text[MAX_VAD_LEN];		// 对话文本。近端
    char ai_text[MAX_SPARKDESK_LEN];	// 对话文本。远端
    int vad_timer;
};
static struct ifly_net_struct *p_ifly_net = NULL;


static int ifly_tts_event_cb(ifly_tts_event_enum evt, void *param)  //tts任务状态callback
{
    switch (evt) {
    case IFLY_TTS_EVT_PLAY_START:
        break;
    case IFLY_TTS_EVT_PLAY_STOP:
        break;
    case IFLY_TTS_EVT_PLAY_FAIL_STOP:
        break;
    case IFLY_TTS_EVT_EXIT:
        if (p_ifly_net) {
            free(p_ifly_net);
            printf("free!!\n");
            p_ifly_net = NULL;
        }
        break;
    default:
        break;
    }
    return 0;
}

static int ifly_ai_event_cb(ifly_ai_event_enum evt, void *param)  //ai任务状态callback
{
    switch (evt) {
    case IFLY_AI_EVT_RECV_OK:
        ifly_sparkdesk_stop(10);

        p_ifly_net->tts_param.event_cb = ifly_tts_event_cb;
        p_ifly_net->tts_param.text_res = p_ifly_net->ai_text;
        ifly_tts_start(&p_ifly_net->tts_param);                   //创建tts任务

        break;
    case IFLY_AI_EVT_EXIT:
        break;
    default:
        break;
    }
    return 0;
}

static int ifly_vad_event_cb(ifly_vad_event_enum evt, void *param)  //vad任务状态callback
{
    switch (evt) {
    case IFLY_VAD_EVT_AUDIO_START:
        break;
    case IFLY_VAD_EVT_RECV_OK:
        break;
    case IFLY_VAD_EVT_EXIT:   //vad结束，开启ai
        p_ifly_net->ai_param.ai_res = p_ifly_net->ai_text;
        p_ifly_net->ai_param.ai_res_len = MAX_SPARKDESK_LEN;
        p_ifly_net->ai_param.event_cb = ifly_ai_event_cb;
        p_ifly_net->ai_param.content = p_ifly_net->local_text;
        p_ifly_net->ai_param.ai_res[0] = 0;
        ifly_sparkdesk_start(&p_ifly_net->ai_param);               //创建ai任务
        break;
    default:
        break;
    }
    return 0;
}


static void stop_rec(void *priv)   //8s结束录音,并关闭vad任务
{
    ifly_vad_stop(10);
    if (p_ifly_net->vad_timer) {
        sys_timer_del(p_ifly_net->vad_timer);
        p_ifly_net->vad_timer = 0;
    }
}

void ifly_net_demo(void)
{
    if (p_ifly_net == NULL) {
        p_ifly_net = zalloc(sizeof(struct ifly_net_struct));
        ASSERT(p_ifly_net);
    }
    p_ifly_net->vad_param.vad_res = p_ifly_net->local_text;
    p_ifly_net->vad_param.vad_res_len = MAX_VAD_LEN;
    p_ifly_net->vad_param.event_cb = ifly_vad_event_cb;
    p_ifly_net->vad_param.vad_res[0] = 0;

    if (!p_ifly_net->vad_timer) {
        p_ifly_net->vad_timer = sys_timer_add(NULL, stop_rec, 8000);
    }
    ifly_vad_start(&p_ifly_net->vad_param);  //创建vad任务
}

#endif
#endif
