#include "system/includes.h"
#include "app_config.h"
#include "asm/pwm_led.h"
#include "tone_player.h"
#include "ui_manage.h"
#include "app_main.h"
#include "app_task.h"
#include "asm/charge.h"
#include "app_power_manage.h"
#include "app_charge.h"
#include "user_cfg.h"
#include "power_on.h"
#include "bt.h"
#include "vm.h"
#include "ui/ui_effect.h"
#include "usb/device/usb_stack.h"
#include "usart0_to_mcu.h"
#include "asm/power/power_reset.h"


#include "app_config.h"
#include "system/includes.h"
#include "asm/charge.h"
#include "app_power_manage.h"
#include "update.h"
#include "app_main.h"
#include "app_charge.h"
#include "update_loader_download.h"
#include "audio_config.h"


extern u8 power_reset_src;
#define LOG_TAG_CONST       APP
#define LOG_TAG             "[APP]"
#define LOG_ERROR_ENABLE    1
#define LOG_DEBUG_ENABLE    1
#define LOG_INFO_ENABLE     1
/* #define LOG_DUMP_ENABLE */   
#define LOG_CLI_ENABLE      1
#include "debug.h"



APP_VAR  app_var;


void app_entry_idle()
{
    app_task_switch_to(APP_IDLE_TASK);
}



void app_task_loop()
{
    while (1) {
        switch (app_curr_task) {
        case APP_POWERON_TASK:
            log_info("APP_POWERON_TASK \n");
            app_poweron_task();
            break;
        case APP_POWEROFF_TASK:
            log_info("APP_POWEROFF_TASK \n");
            app_poweroff_task();
            break;
        case APP_BT_TASK:
            log_info("APP_BT_TASK \n");
            app_bt_task();
            break;
        case APP_MUSIC_TASK:
            log_info("APP_MUSIC_TASK \n");
            app_music_task();
            break;
        case APP_RTC_TASK:
            log_info("APP_RTC_TASK \n");
            app_rtc_task();
            break;
        case APP_PC_TASK:
            log_info("APP_PC_TASK \n");
            app_pc_task();
            break;
        case APP_WATCH_UPDATE_TASK:
            log_info(" APP_WATCH_UPDATE_TASK \n");
            app_watch_ui_updata_task();
            break;
        case APP_SMARTBOX_ACTION_TASK:
            log_info("APP_SMARTBOX_ACTION_TASK \n");
#ifdef CONFIG_APP_BT_ENABLE

//SMART_BOX_EN为0时不开启
#if (SMART_BOX_EN)
            app_smartbox_task();
#endif

#endif
            break;
        case APP_IDLE_TASK:
            log_info("APP_IDLE_TASK \n");
            app_idle_task();
            break;
#if TCFG_APP_RECORD_EN
        case APP_RECORD_TASK:
            log_info("APP_RECORD_TASK \n");
            app_record_task();
            break;
#endif
#if TCFG_APP_CAT1_EN
        case APP_CAT1_TASK:
            log_info("APP_CAT1_TASK \n");
            app_cat1_task();
            break;
#endif /* #if TCFG_APP_CAT1_EN */
        }
        app_task_clear_key_msg();//清理按键消息
        //检查整理VM
        vm_check_all(0);
    }
}

void mem_printf(void *priv)
{
    mem_stats();
}

// task堆栈统计
extern const int config_system_info;
extern void os_system_info_output(void);
static void task_info(void *priv)
{
    if (config_system_info) {
        os_system_info_output();
    }
}

extern const int support_user_file_update_v2_en;
extern void ex_flash_file_download_init(void);
extern void user_file_flash_file_download_init(void);

void app_main()
{
#if TCFG_PSRAM_UI_EFFECT
    static struct effect_sca_alpha sal = {0};
#endif
    log_info("app_main \n");
    // u32 reset_src = get_reset_source_value();
    // log_info("reset_src: 0x%x\n", reset_src);
    if (is_reset_source(P33_PPINR_RST)) {
        log_info("reset_src: P33_PPINR_RST\n");
    }
    if (is_reset_source(P11_P33_RST)) {
        log_info("reset_src: P11_P33_RST\n");
    }
    if (is_reset_source(MSYS_P11_RST)) {
        log_info("reset_src: MSYS_P11_RST\n");
    }

    if (config_system_info) {
        sys_timer_add(NULL, task_info, 10 * 1000);
    }

#if TCFG_CHARGE_ENABLE
    set_charge_event_flag(1);
#endif

    app_var.start_time = timer_get_ms();

    if (get_charge_online_flag()) {

        app_var.poweron_charge = 1;

#if (TCFG_SYS_LVD_EN == 1)
        vbat_check_init();
#endif  
        log_info("+++++++++++++++++++++++++++++++++++++++++++++++++++++++/charge online enter bt task++++++++++++++++++++++++++++++++++++++++++\n");
        /* 充电开机也需要广播/串口/音频，直接进入 BT 任务 */
        app_curr_task = APP_BT_TASK;
    } else {

#if SOUNDCARD_ENABLE
        soundcard_peripheral_init();
#endif

        /* endless_loop_debug_int(); */
        ui_manage_init();
        ui_update_status(STATUS_POWERON);

        if (0) {
            /* app_curr_task = APP_WATCH_UPDATE_TASK; */
            app_curr_task = APP_SMARTBOX_ACTION_TASK;
        } else {
            app_curr_task = APP_POWERON_TASK;
        }
    }

#if TCFG_PSRAM_UI_EFFECT
    //sal.sca = 0.85;
    //sal.alpha = 67;
    sal.sca = 0;
    sal.alpha = 0xff;
    sal.flip_all_en = 1;
    /* sal.cnt = 120; */
    /* sal.wait_te = 1; */
    ui_window_effect(-1, EFFECT_MODE_FLIP_SCA_ALPHA, NULL, &sal);

    ui_page_set_auto_sw_effect(1);
#endif

#if (TCFG_USB_CDC_BACKGROUND_RUN && !TCFG_PC_ENABLE)
    usb_cdc_background_run();
#endif

#if (TCFG_VIRFAT_INSERT_FLASH_ENABLE || TCFG_NORFLASH_DEV_ENABLE || TCFG_NANDFLASH_DEV_ENABLE || (TCFG_NORFLASH_SFC_DEV_ENABLE && TCFG_NOR_FS))
    if (support_user_file_update_v2_en) {
        user_file_flash_file_download_init();
    } else {
        ex_flash_file_download_init();
    }
#endif

#if defined(SMART_BOX_EN) && (SMART_BOX_EN)
    // UART 初始化放到 app_main 靠后位置：确保能看到日志，同时避免过早初始化引发卡死
    log_info("串口初始化开始(app_main)\n");
    uart1_init();
    log_info("串口初始化结束(app_main)\n");
#endif

    // 初始化看门狗管理器，统一由 wdg_mgr 负责喂硬件 WDT
    log_info("wdg_mgr init start\n");
    wdg_mgr_init(16000, 2000); // hw timeout = 16s, check period = 2s
    log_info("wdg_mgr init done\n");
    /* sys_timer_add(NULL, mem_printf, 1000); */
    app_task_loop();

}
