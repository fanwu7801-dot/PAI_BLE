#include "includes.h"
#include "app_config.h"
#include "asm/power_interface.h"
#include "jl_kws.h"
#include "smart_voice/smart_voice.h"
#include "app_task.h"

#if TCFG_SMART_VOICE_ENABLE && TCFG_VAD_LP_CLOSE

static char kws_model = -1;

#if TCFG_KWS_HOLD_TIME
static u16 hold_tm = 0;
#endif /* #if TCFG_KWS_HOLD_TIME */

#if TCFG_KWS_HOLD_TIME
static void kws_time_callback(void *priv)
{
    hold_tm = 0;
    printf("kws tmr cb \n");
}
void kws_hold_time_enable(void)
{
    local_irq_disable();
    if (hold_tm) {
        sys_hi_timeout_del(hold_tm);
        hold_tm = 0;
    }
    hold_tm = sys_hi_timeout_add(NULL, kws_time_callback, TCFG_KWS_HOLD_TIME * 1000);
    local_irq_enable();
}
#else /* #if TCFG_KWS_HOLD_TIME */
void kws_hold_time_enable(void)
{
}
#endif /* #if TCFG_KWS_HOLD_TIME */

static void kws_poweroff(int priv)
{
#if TCFG_AUDIO_ASR_DEVELOP == ASR_CFG_JL_KWS
    int model = audio_smart_voice_kws_get_model();
    if (model >= 0) {
        printf("smart_voice close \n");
        audio_smart_voice_detect_close();
        kws_model = model;
    }
#else
    int status = audio_smart_voice_check_status();
    if (status) {
        audio_smart_voice_detect_close();
    }
#endif
}

AT(.volatile_ram_code)
static u8 extern_kws_handler(u32 timeout)
{
    int msg[3];
#if TCFG_AUDIO_ASR_DEVELOP == ASR_CFG_JL_KWS
    int model = audio_smart_voice_kws_get_model();
    if (model < 0) {
        return 0;
    }
#else
    int status = audio_smart_voice_check_status();
    if (!status) {
        return 0;
    }
#endif
    msg[0] = (int)kws_poweroff;
    msg[1] = 1;
    msg[2] = (int)NULL;
    os_taskq_post_type("app_core", Q_CALLBACK, 3, msg);
    return 1;
}

static u8 exit_kws_handler(u32 timeout)
{
    return 0;
}

//低功耗线程请求所有模块关闭，由对应线程处理
REGISTER_LP_REQUEST(kws_target) = {
    .name = "extern_kws",
    .request_enter = extern_kws_handler,
    .request_exit = exit_kws_handler,
};


int smart_voice_reset(void)
{
#if TCFG_AUDIO_ASR_DEVELOP == ASR_CFG_JL_KWS
    int model = audio_smart_voice_kws_get_model();
    if (model >= 0) {
        printf("smart_voice is open \n");
    } else {
        if (kws_model >= 0) {
            printf("smart_voice open \n");
            audio_smart_voice_detect_open(kws_model);
            kws_model = -1;
            return true;
        }
    }
#else
    int status = audio_smart_voice_check_status();
    if (status) {
        printf("smart_voice is open \n");
    } else {
        printf("smart_voice open \n");
        audio_smart_voice_detect_open(NULL);
    }
#endif
    return false;
}

#if TCFG_UI_ENABLE
#include "ui/ui_api.h"

static void val_lcd_sleep_enter(void)
{
    kws_hold_time_enable();
}

static void val_lcd_sleep_exit(void)
{
    u8 curr_task =  app_get_curr_task();
    if ((curr_task != APP_WATCH_UPDATE_TASK) && (curr_task != APP_SMARTBOX_ACTION_TASK)) {
        smart_voice_reset();
    }
}

REGISTER_LCD_SLEEP_HEADLER(vad_lcd_sleep) = {
    .name = "vad",
    .enter = val_lcd_sleep_enter,
    .exit = val_lcd_sleep_exit,
};

#endif

#endif /* #if TCFG_VAD_LP_CLOSE */

