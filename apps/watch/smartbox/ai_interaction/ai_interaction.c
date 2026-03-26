#include "ai_interaction/ai_interaction.h"
#include "ai_interaction/ai_audio.h"
#include "ai_interaction/ai_text.h"
#include "smartbox_rcsp_manage.h"
#include "btstack/avctp_user.h"
#include "app_task.h"
#include "bt.h"
#include "ui/ui_api.h"
#include "string.h"
#if TCFG_AI_INTERACTION_ENABLE
volatile u8 *tts_txt = NULL;
volatile u8 *ai_txt1 = NULL;
volatile u8 *ai_dial_txt1 = NULL;
volatile u8 *ai_txt2 = '\0';
volatile u8 *ai_txttempa = '\0';
volatile u8 *ai_txttempb = '\0';
u8 aidial_flag;
#define AI_INTERACTION_REC_LIMIT_MIN_MS		(1000) //录音时间最低限制
//截取UTF8编码第num位到末尾字符串
static char *getutf8stringendstring(u8 *str, int num)
{
    char *substring = NULL;
    free(substring);
    int length = strlen(str);
    int i = 0;
    const char *starstr = str;
    while (*starstr != '\0' && i < num) {
        if ((*starstr & 0XC0) != 0X80) {
            i++;
        }
        starstr++;
    }
    substring = strdup(starstr);
    return substring;
}
//截取UTF8编码后num位字符
static char *getutf8stringend(u8 *str, int num)
{
    int length = strlen(str);
    int i = length - 1;
    int count = 0;
    while (i >= 0 && count < num) {
        if ((str[i] & 0XC0) != 0X80) {
            count++;
        }
        i--;
    }
    int substringlength = length - i - 1;
    char *substring = malloc(substringlength + 1);
    strncpy(substring, &str[i + 1], substringlength);
    substring[substringlength] = '\0';
    return substring;
}
//截取UTF8字符串开始到指定位置
static u8 *getutf8stringfrontstring(u8 *str, int num)
{
    char *substring = NULL;
    free(substring);
    int i = 0;
    const u8 *starstr = str;
    while (*starstr != '\0' && i < num) {
        if ((*starstr & 0XC0) != 0X80) {
            i++;
        }
        starstr++;
    }
    int substringlength = starstr - str + 1;
    substring = malloc(substringlength + 1);
    strncpy(substring, str, substringlength);
    substring[substringlength] = '\0';
    return substring;
}

struct _AI_INTERACTION {
    u8  rec_busy;
    u8  err;
    u32 lose_cnt;
    u32 rec_min_time;
};
static struct _AI_INTERACTION ai_interaction;


static void ai_audio_evt_cb(int event)
{
    switch (event) {
    case AI_AUDIO_EVENT_ERR_REC_START:
        ai_interaction.err = 1;
        break;
    case AI_AUDIO_EVENT_ERR_REC_DATA:
        if (ai_interaction.lose_cnt < (u32) - 1) {
            ai_interaction.lose_cnt ++;
        }
        break;
    default :
        break;
    }
}
static int ai_text_rx_cb(u8 ai_text, u8 *data, int len)
{
    aidial_flag = ai_text;
    printf("aidial_flag_txt:%d\n", aidial_flag);
    if (ai_dial_txt1) {
        free(ai_dial_txt1);
        ai_dial_txt1 = NULL;
    }
    ai_dial_txt1 = data;
    ai_dial_txt1 = malloc(len + 1);
    memset(ai_dial_txt1, '\0', len + 1);
    memcpy(ai_dial_txt1, data + '\0', len);
    char *str = getutf8stringfrontstring(data, 145);
    char *str1 = getutf8stringendstring(data, 144);
    if (ai_text && data != NULL) {//1----ai文本
        tts_txt = str;
        if (len > 430) {
            free(ai_txt2);
            ai_txt2 = strdup(str1);
        } else {
            ai_txt2 = NULL;
        }
    }
    if (!ai_text) {
        free(ai_txt1);
        ai_txt1 = data;
        ai_txt1 = malloc(len + 1);
        memset(ai_txt1, '\0', len + 1);
        memcpy(ai_txt1, data + '\0', len);
    }
    if (ai_text == 2) {
        UI_MSG_POST("AI_ERROR");
    }
// UTF8
    ai_txttempa = tts_txt;
    ai_txttempb = ai_txt2;
    UI_MSG_POST("SEND_TXT");
    log_i("rx text:%d, %s \n", ai_text, data);
    put_buf(data, len);
    return false;
}

void ai_interaction_release(void)
{
    ai_audio_stop(1);
    ai_text_release();
    memset(&ai_interaction, 0, sizeof(struct _AI_INTERACTION));
}


int ai_interaction_rec_stop(void)
{
    if (!ai_interaction.rec_busy) {
        log_e("rec no run \n");
        return false;
    }
    ai_interaction.rec_busy = 0;

    u8 cur_task = app_get_curr_task();
    if (APP_BT_TASK != cur_task) {
        // 为啥不是蓝牙模式？
        log_e("cur task:%d no support ai rec stop \n", cur_task);
        /* ai_interaction_release(); */
        ai_audio_stop(1);
        return false;
    }
    if (!time_after(jiffies, ai_interaction.rec_min_time)) {
        log_e("limit time:%d,%d \n", jiffies, ai_interaction.rec_min_time);
        ai_audio_stop(1);
        return false;
    } else if (ai_interaction.err) {
        log_e("rec err \n");
        ai_audio_stop(1);
        return false;
    } else {
        log_i("stop lose:%d \n", ai_interaction.lose_cnt);
        ai_audio_stop(0);
    }
    return true;
}

int ai_interaction_rec_start(void)
{
    if (!get_rcsp_connect_status()) {
        log_e("rcsp disconnect \n");
        return false;
    }

    u8 cur_task = app_get_curr_task();
    switch (cur_task) {
    case APP_POWERON_TASK:
    case APP_POWEROFF_TASK:
    case APP_WATCH_UPDATE_TASK:
    case APP_SMARTBOX_ACTION_TASK:
    case APP_CAT1_TASK:
        // 这些模式不支持跳转到录音
        log_e("cur task:%d no support ai rec start \n", cur_task);
        return false;
    /* break; */
    default:
        break;
    }

    if (bt_must_work()) {
        log_e("bt busy \n");
        return false;
    }

    // 跳转到蓝牙模式
    app_task_switch_to(APP_BT_TASK);

    if (a2dp_get_status() == BT_MUSIC_STATUS_STARTING) {
        log_i("switch a2dp \n");
        user_send_cmd_prepare(USER_CTRL_AVCTP_OPID_PLAY, 0, NULL);
    }

    // 释放旧的
    ai_interaction_release();

    // 状态初始化
    ai_interaction.rec_min_time = jiffies + msecs_to_jiffies(AI_INTERACTION_REC_LIMIT_MIN_MS);
    ai_interaction.rec_busy = 1;

    // 启动录音
    int ret = ai_audio_start();
    if (ret == false) {
        ai_interaction.rec_busy = 0;
    }
    return ret;
}

void ai_interaction_init(void)
{
    memset(&ai_interaction, 0, sizeof(struct _AI_INTERACTION));
    ai_audio_init(ai_audio_evt_cb);
    ai_text_init(ai_text_rx_cb);
}

static u8 ai_interaction_idle_query(void)
{
    return !ai_interaction.rec_busy;
}
REGISTER_LP_TARGET(ai_lp_target) = {
    .name = "ai",
    .is_idle = ai_interaction_idle_query,
};

#endif

