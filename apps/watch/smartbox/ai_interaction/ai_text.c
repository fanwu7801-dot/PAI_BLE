#include "ai_interaction/ai_text.h"
#include "mass_data/mass_data.h"
#include "system/includes.h"
#include "JL_rcsp_protocol.h"

#if TCFG_AI_INTERACTION_ENABLE

#pragma pack(1)
struct _AI_TEXT_PAYLOAD {
    u8 version;	// [0,3]:version // [4,7]:0-localText,1-aiText
    u8 vendor;	// 0-杰理,1-科大讯飞
    u16 len;
    u8 dat[0];
};
struct _AI_TEXT_PAYLOAD_TTS {
    u8 version;
    u16 len;
    u8 dat[0];
};
#pragma pack()

struct _AI_TEXT {
    struct _AI_TEXT_PAYLOAD_TTS *ai;
    struct _AI_TEXT_PAYLOAD_TTS *local;
    int (* rx_text)(u8 ai_text, u8 *data, int len);
    OS_MUTEX mutex;
};


#define AI_TEXT_RULE(x)			((x)->version >> 4)
#define AI_TEXT_RULED(x)			((x)->version >>3)
static struct _AI_TEXT ai_text;

void ai_text_enter_critical(void)
{
    os_mutex_pend(&ai_text.mutex, 0);
}

void ai_text_exit_critical(void)
{
    os_mutex_post(&ai_text.mutex);
}

static void ai_text_rx_cb(void *priv, u8 *data, int len)
{
    u8 is_ai_text = 0;
    struct _AI_TEXT_PAYLOAD ai;
    struct _AI_TEXT_PAYLOAD_TTS *tts;
    memcpy(&ai, data, sizeof(struct _AI_TEXT_PAYLOAD));
    ai.len = READ_BIG_U16(&ai.len);
    if (AI_TEXT_RULE(&ai)) {
        is_ai_text = 1;
    }
    if (AI_TEXT_RULED(&ai)) {
        is_ai_text = 2;
    }
    if (ai_text.rx_text) {
        if (true == ai_text.rx_text(is_ai_text, &data[sizeof(struct _AI_TEXT_PAYLOAD)], ai.len)) {
            // 外面已经处理，内部不再保存数据
            return ;
        }
    }

    tts = malloc(ai.len + sizeof(struct _AI_TEXT_PAYLOAD_TTS));
    if (!tts) {
        log_e("malloc err \n");
        return ;
    }
    tts->version = 0;
    WRITE_BIG_U16(&tts->len, ai.len);
    memcpy(tts->dat, &data[sizeof(struct _AI_TEXT_PAYLOAD)], ai.len);

    ai_text_enter_critical();
    if (is_ai_text) {
        if (ai_text.ai) {
            free(ai_text.ai);
            ai_text.ai = NULL;
        }
        ai_text.ai = tts;
    } else {
        if (ai_text.local) {
            free(ai_text.local);
            ai_text.local = NULL;
        }
        ai_text.local = tts;
    }
    ai_text_exit_critical();
}

int ai_text_get_local_text(u8 **buf)
{
    if (ai_text.local) {
        *buf = ai_text.local->dat;
        return READ_BIG_U16(&ai_text.local->len);
    }
    return 0;
}

int ai_text_get_ai_text(u8 **buf)
{
    if (ai_text.ai) {
        *buf = ai_text.ai->dat;
        return READ_BIG_U16(&ai_text.ai->len);
    }
    return 0;
}

int ai_text_local_to_tts(void)
{
    ai_text_enter_critical();
    if (ai_text.local) {
        mass_data_asyn_send(MASS_DATA_TYPE_TTS, (u8 *)ai_text.local, READ_BIG_U16(&ai_text.local->len) + sizeof(struct _AI_TEXT_PAYLOAD_TTS), NULL, NULL);
        ai_text_exit_critical();
        return true;
    }
    ai_text_exit_critical();
    return false;
}

int ai_text_ai_to_tts(void)
{
    ai_text_enter_critical();
    if (ai_text.ai) {
        mass_data_asyn_send(MASS_DATA_TYPE_TTS, (u8 *)ai_text.ai, READ_BIG_U16(&ai_text.ai->len) + sizeof(struct _AI_TEXT_PAYLOAD_TTS), NULL, NULL);
        ai_text_exit_critical();
        return true;
    }
    ai_text_exit_critical();
    return false;
}

void ai_text_release(void)
{
    ai_text_enter_critical();
    if (ai_text.ai) {
        free(ai_text.ai);
        ai_text.ai = NULL;
    }
    if (ai_text.local) {
        free(ai_text.local);
        ai_text.local = NULL;
    }
    ai_text_exit_critical();
}

int ai_text_init(int (* rx_text)(u8 ai_text, u8 *data, int len))
{
    memset(&ai_text, 0, sizeof(struct _AI_TEXT));
    ai_text.rx_text = rx_text;
    os_mutex_create(&ai_text.mutex);
    mass_data_recv_register(MASS_DATA_TYPE_AI_TEXT, NULL, ai_text_rx_cb);
    return true;
}


#endif

