/*****************************************************************
>file name : kws_event.c
>author : lichao
>create time : Mon 01 Nov 2021 11:34:00 AM CST
*****************************************************************/
#include "system/includes.h"
#include "kws_event.h"
#include "jl_kws.h"

extern int config_audio_kws_event_enable;

static const int kws_wake_word_event[] = {
    KWS_EVENT_NULL,
    KWS_EVENT_HEY_KEYWORD,
};

static const int kws_multi_command_event[] = {
    [ 0] = KWS_EVENT_NULL,
    [ 1] = KWS_EVENT_NULL,
    [ 2] = KWS_EVENT_PLAY_MUSIC,
    [ 3] = KWS_EVENT_PAUSE_MUSIC,
    [ 4] = KWS_EVENT_STOP_MUSIC,
    [ 5] = KWS_EVENT_VOLUME_UP,
    [ 6] = KWS_EVENT_VOLUME_DOWN,
    [ 7] = KWS_EVENT_PREV_SONG,
    [ 8] = KWS_EVENT_NEXT_SONG,
    [ 9] = KWS_EVENT_DETECTION_HEART,
    [10] = KWS_EVENT_DETECTION_OXYGEN,
    [11] = KWS_EVENT_SEE_SPORT_RECORD,
    [12] = KWS_EVENT_SEE_ACTION_RECORD,
    [13] = KWS_EVENT_SEE_SLEEP_RECORD,
    [14] = KWS_EVENT_SEE_CALL_REDORD,
    [15] = KWS_EVENT_SEE_TRAIN_RECORD,
    [16] = KWS_EVENT_SEE_HEAT,
    [17] = KWS_EVENT_START_PHOTOS,
    [18] = KWS_EVENT_SWITCH_DIAL,
    [19] = KWS_EVENT_SWITCH_STYLE,
    [20] = KWS_EVENT_FIND_PHONE,
    [21] = KWS_EVENT_VOLUME_MUTE,
    [22] = KWS_EVENT_VOLUME_UNMUTE,
    [23] = KWS_EVENT_VOLUME_MAX,
    [24] = KWS_EVENT_BRIGHTNESS_ALWAYS,
    [25] = KWS_EVENT_BRIGHTNESS_UP,
    [26] = KWS_EVENT_BRIGHTNESS_DOWN,
    [27] = KWS_EVENT_BRIGHTNESS_AUTO,
    [28] = KWS_EVENT_OPEN_SPORT,
    [29] = KWS_EVENT_OPEN_TRAIN,
    [30] = KWS_EVENT_OPEN_CALCULAGRAPH,
    [31] = KWS_EVENT_OPEN_CALL_DIAL,
    [32] = KWS_EVENT_OPEN_PHONEBOOK,
    [33] = KWS_EVENT_OPEN_ALARM,
    [34] = KWS_EVENT_OPEN_STOPWATCH,
    [35] = KWS_EVENT_OPEN_WEATHER,
    [36] = KWS_EVENT_OPEN_MESS,
    [37] = KWS_EVENT_OPEN_SET,
    [38] = KWS_EVENT_OPEN_APP_LIST,
    [39] = KWS_EVENT_OPEN_BREATH_TRAIN,
    [40] = KWS_EVENT_OPEN_BARO,
    [41] = KWS_EVENT_OPEN_COMPASS,
    [42] = KWS_EVENT_OPEN_CARD_BAG,
    [43] = KWS_EVENT_OPEN_ALIPAY,
    [44] = KWS_EVENT_OPEN_FLASHLIGHT,
    [45] = KWS_EVENT_OPEN_CALENDAR,
    [46] = KWS_EVENT_OPEN_CALCULATOR,
    [47] = KWS_EVENT_OPEN_EDR,
    [48] = KWS_EVENT_CALL_ACTIVE,
    [49] = KWS_EVENT_CALL_HANGUP,
};

static const int kws_call_command_event[] = {
    KWS_EVENT_NULL,
    KWS_EVENT_NULL,
    KWS_EVENT_CALL_ACTIVE,
    KWS_EVENT_CALL_HANGUP,
};

static const int *kws_model_events[3] = {
    kws_wake_word_event,
    kws_multi_command_event,
    kws_call_command_event,
};

int smart_voice_kws_event_handler(u8 model, int kws)
{
    if (!config_audio_kws_event_enable || kws < 0) {
        return 0;
    }
    int event = KWS_EVENT_NULL;
    struct sys_event e;

    event = kws_model_events[model][kws];

    if (event == KWS_EVENT_NULL) {
        return -EINVAL;
    }

    e.type = SYS_KEY_EVENT;
    e.u.key.event = event;
    e.u.key.value = 'V';
    e.u.key.type = KEY_DRIVER_TYPE_VOICE;//区分按键类型

    sys_event_notify(&e);

    return 0;
}

static const char *kws_dump_words[] = {
    [ 0] = "no words",
    [ 1] = "hey siri",
    [ 2] = "bo fang",
    [ 3] = "zan ting",
    [ 4] = "ting zhi",
    [ 5] = "zeng da",
    [ 6] = "jian xiao",
    [ 7] = "shangyishou",
    [ 8] = "xiayishou",
    [ 9] = "xin lv",
    [10] = "xue yang",
    [11] = "duan lian ji lu",
    [12] = "huo dong ji lu",
    [13] = "shui mian shuo ju",
    [14] = "tong hua ji lu",
    [15] = "xun lian ji lu",
    [16] = "ya li zhi biao",
    [17] = "pai zhao",
    [18] = "geng huan biao pan",
    [19] = "jie mian feng ge",
    [20] = "zhao shou ji",
    [21] = "jing yin mo shi",
    [22] = "qu xiao jing yin",
    [23] = "zui da yin liang",
    [24] = "ping mu chang liang",
    [25] = "zeng da liang du",
    [26] = "jain xiao liang du",
    [27] = "tiao zheng liang du",
    [28] = "yun dong",
    [29] = "duan lian",
    [30] = "ji shi qi",
    [31] = "dian hua",
    [32] = "lian xi ren",
    [33] = "nao zhong",
    [34] = "miao biao",
    [35] = "tian qi",
    [36] = "xiao xi",
    [37] = "she zhi",
    [38] = "ying yong lie biao",
    [39] = "hu xi xun lian",
    [40] = "hai bo qi ya ji",
    [41] = "zhi nan zhen",
    [42] = "ka bao",
    [43] = "zhi fu bao",
    [44] = "shou dian tong",
    [45] = "ri li",
    [46] = "ji suan qi",
    [47] = "lan ya",
    [48] = "jie ting",
    [49] = "gua duan",
};

struct kws_result_context {
    u16 timer;
    u32 result[0];
};

static void smart_voice_kws_dump_timer(void *arg)
{
    struct kws_result_context *ctx = (struct kws_result_context *)arg;
    int i = 0;

    int kws_num = ARRAY_SIZE(kws_dump_words);
    printf("\n===============================================\nResults:\n");
    for (i = 1; i < kws_num; i++) {
        printf("%s : %u\n", kws_dump_words[i], ctx->result[i]);
    }
    printf("\n===============================================\n");
}

void *smart_voice_kws_dump_open(int period_time)
{
    if (!config_audio_kws_event_enable) {
        return NULL;
    }

    struct kws_result_context *ctx = NULL;
    ctx = zalloc(sizeof(struct kws_result_context) + (sizeof(u32) * ARRAY_SIZE(kws_dump_words)));

    if (ctx) {
        ctx->timer = sys_timer_add(ctx, smart_voice_kws_dump_timer, period_time);
    }
    return ctx;
}

void smart_voice_kws_dump_result_add(void *_ctx, u8 model, int kws)
{
    if (!config_audio_kws_event_enable || kws < 0) {
        return;
    }

    struct kws_result_context *ctx = (struct kws_result_context *)_ctx;
    int event = kws_model_events[model][kws];
    ctx->result[event]++;
}

void smart_voice_kws_dump_close(void *_ctx)
{
    struct kws_result_context *ctx = (struct kws_result_context *)_ctx;
    if (config_audio_kws_event_enable) {
        if (ctx->timer) {
            sys_timer_del(ctx->timer);
            free(ctx);
        }
    }
}
