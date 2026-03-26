
/*************************************************************
   此文件函数主要是蓝牙模式各种状态处理

	void app_bt_task()
   bt模式主函数

	static int bt_sys_event_handler(struct sys_event *event)
   bt模式系统事件所有处理入口

	static void bt_task_close(void)
	bt模式退出


	void bt_function_select_init()
    蓝牙功能配置

	void bredr_handle_register()
 	蓝牙协议栈回调函数

	static int bt_connction_status_event_handler(struct bt_event *bt)
	蓝牙状态事件处理函数

	static int bt_hci_event_handler(struct bt_event *bt)
	蓝牙协议栈事件回调，app处理函数

	static int bt_ai_event_handler(struct bt_event *bt)
	蓝牙ai事件处理函数

	int bt_background_event_handler_filter(struct sys_event *event)
	蓝牙后台事件过滤处理
	int bt_background_event_handler(struct sys_event *event)
	蓝牙后台事件处理函数

	int bt_key_event_handler(struct sys_event *event)
	蓝牙按键处理函数

	avctp_user.h 有蓝牙支持的命令介绍

**************************************************************/



#include "system/includes.h"
#include "media/includes.h"

#include "app_config.h"
#include "app_task.h"

#include "btstack/avctp_user.h"
#include "btstack/btstack_task.h"
#include "btstack/bluetooth.h"
#include "btstack/btstack_error.h"
#include "btctrler/btctrler_task.h"
#include "classic/hci_lmp.h"

#include "bt/bt_tws.h"
#include "bt/bt_ble.h"
#include "bt/bt.h"
#include "bt/vol_sync.h"
#include "bt/bt_emitter.h"
#include "bt_common.h"
#include "aec_user.h"

#include "math.h"
#include "spp_user.h"


#include "app_chargestore.h"
#include "app_charge.h"
#include "app_main.h"
#include "app_power_manage.h"
#include "user_cfg.h"

#include "asm/pwm_led.h"
#include "asm/timer.h"
#include "asm/hwi.h"
#include "cpu.h"

#include "ui/ui_api.h"
#include "ui_manage.h"
#include "ui/ui_style.h"
#include "os/os_api.h"

#include "key_event_deal.h"
#include "clock_cfg.h"
#include "gSensor/gSensor_manage.h"
/* #include "soundcard/soundcard.h" */

#include "audio_config.h"
#include "audio_dec.h"
/* #include "audio_reverb.h" */
#include "tone_player.h"
#include "tone_user_table.h"
#include "dac.h"
#include "asm/gpio.h"

#include "message_vm_cfg.h"
#include "watch_common.h"
#include "fill_protocol.h"
#include "user_info_file.h"
#if TCFG_UI_ENABLE_MOTO
#include "ui/ui_sys_param.h"
#endif /* #if TCFG_UI_ENABLE_MOTO */
#include "os/os_api.h"

/* 播放提示音加互斥，避免并发触发崩溃 */
static OS_MUTEX tone_play_mutex;
static u8 tone_play_mutex_init = 0;
void bt_tone_pa_ctrl_set(u8 on);
static void tone_play_lock(void)
{
    if (!tone_play_mutex_init) {
        os_mutex_create(&tone_play_mutex);
        tone_play_mutex_init = 1;
    }
    os_mutex_pend(&tone_play_mutex, 0);
}
static void tone_play_unlock(void)
{
    if (tone_play_mutex_init) {
        os_mutex_post(&tone_play_mutex);
    }
}

/* 打断式播放：进入新音效前先停止当前音效，避免叠播/漏切换 */
static void bt_tone_interrupt_current_locked(void)
{
    tone_play_stop();
    bt_tone_pa_ctrl_set(0);
}

/* 播放提示音前后控制功放使能脚 */
void bt_tone_pa_ctrl_set(u8 on)
{
#if defined(TCFG_AUDIO_DAC_PA_PORT) && (TCFG_AUDIO_DAC_PA_PORT != NO_CONFIG_PORT)
    gpio_direction_output(TCFG_AUDIO_DAC_PA_PORT, on ? 1 : 0);
#else
    (void)on;
#endif
}

static void bt_tone_play_end_cb(void *priv, int flag)
{
    (void)priv;
    if (flag == TONE_STOP) {
        bt_tone_pa_ctrl_set(0);
    }
}

#define LOG_TAG_CONST        BT
#define LOG_TAG             "[BT]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
#define LOG_DUMP_ENABLE
#define LOG_CLI_ENABLE
#include "debug.h"


#if TCFG_APP_BT_EN

#define WATCH_CALL_CHOOSE 1   //通话拦截   bqb profile需要关掉

struct app_bt_opr app_bt_hdl = {
    .exit_flag = 1,
    .replay_tone_flag = 1,
    .esco_dump_packet = ESCO_DUMP_PACKET_CALL,
    .hid_mode = 0,
    .a2dp_en = 0,
    .music_dev = SET_MUSIC_IN_WATCH,
};

#define __this 	(&app_bt_hdl)

BT_USER_PRIV_VAR bt_user_priv_var;

/*******************************************************
 * @brief   bt_tone_case_to_custom_type: 蓝牙提示音 case_id 映射到自定义音效类型
 * 
 * @param case_id {placeholder}
 * @param tone_type_out {placeholder}
 * @retval {placeholder}  
 * @retval {placeholder}  
*******************************************************/
static bool bt_tone_case_to_custom_type(int case_id, uint8_t *tone_type_out)
{
    if (!tone_type_out) {
        return false;
    }

#define BCD_TO_DEC_4(bcd) (((((bcd) >> 12) & 0x0F) * 1000) + ((((bcd) >> 8) & 0x0F) * 100) + ((((bcd) >> 4) & 0x0F) * 10) + (((bcd) >> 0) & 0x0F))

    // 特殊处理：case_id 46 不应该映射到任何自定义音效类型 
    // if (case_id == 46) {
    //     log_info("bt_tone_case_to_custom_type: case_id 46 不参与自定义音效映射\n");
    //     return false;
    // }

    // 用 if/else 代替 switch-case：部分 tone_user_table 的 BCD 值可能重复，switch 会编译失败
    if (case_id == BCD_TO_DEC_4(Power_on_01) ||
        case_id == BCD_TO_DEC_4(Power_on_02) ||
        case_id == BCD_TO_DEC_4(Power_on_03) ||
        case_id == BCD_TO_DEC_4(Power_on_04)) {
        *tone_type_out = 0;
        log_info("bt_tone_case_to_custom_type: case_id %d 映射到自定义音效类型0（开机）\n", case_id);
        return true;
    }

    if (case_id == BCD_TO_DEC_4(Power_off_01) ||
        case_id == BCD_TO_DEC_4(Power_off_02) ||
        case_id == BCD_TO_DEC_4(Power_off_03) ||
        case_id == BCD_TO_DEC_4(Power_off_04)) {
        *tone_type_out = 1;
        log_info("bt_tone_case_to_custom_type: case_id %d 映射到自定义音效类型1（关机）\n", case_id);
        return true;
    }

    if (case_id == BCD_TO_DEC_4(Displacement_alarm_01) ||
        case_id == BCD_TO_DEC_4(Displacement_alarm_02) ||
        case_id == BCD_TO_DEC_4(Displacement_alarm_03) ||
        case_id == BCD_TO_DEC_4(Displacement_alarm_04) ||
        case_id == BCD_TO_DEC_4(burglar_alarm_01) ||
        case_id == BCD_TO_DEC_4(burglar_alarm_02) ||
        case_id == BCD_TO_DEC_4(burglar_alarm_03) ||
        case_id == BCD_TO_DEC_4(burglar_alarm_04) ||
        case_id == BCD_TO_DEC_4(Overfilling_alarm_01) ||
        case_id == BCD_TO_DEC_4(Overfilling_alarm_02) ||
        case_id == BCD_TO_DEC_4(Overfilling_alarm_03) ||
        case_id == BCD_TO_DEC_4(Overfilling_alarm_04) ||
        // case_id == BCD_TO_DEC_4(low_battery_01) ||
        // case_id == BCD_TO_DEC_4(low_battery_02) ||
        // case_id == BCD_TO_DEC_4(low_battery_03) ||
        // case_id == BCD_TO_DEC_4(low_battery_04) ||
        case_id == BCD_TO_DEC_4(General_Warning_01) ||
        case_id == BCD_TO_DEC_4(General_Warning_02) ||
        case_id == BCD_TO_DEC_4(General_Warning_03) ||
        case_id == BCD_TO_DEC_4(General_Warning_04) ||
        case_id == BCD_TO_DEC_4(fifteen_size_warning_01) ||
        case_id == BCD_TO_DEC_4(fifteen_size_warning_02) ||
        case_id == BCD_TO_DEC_4(fifteen_size_warning_03) ||
        case_id == BCD_TO_DEC_4(fifteen_size_warning_04)
    ) {
        *tone_type_out = 2;
        log_info("bt_tone_case_to_custom_type: case_id %d 映射到自定义音效类型2（告警）\n", case_id);
        return true;
    }

    if (case_id == BCD_TO_DEC_4(pairing_01)) {
        *tone_type_out = 3;
        return true;
    }

    return false;
#undef BCD_TO_DEC_4
}


u8 lmp_get_conn_num(void);
void set_call_log_number(u8 *number);
void set_call_log_date(u8 *date);
void update_call_log_message();

extern void __set_support_3M_size(u8 en);
extern void __set_sbc_cap_bitpool(u8 sbc_cap_bitpoola);
extern void __set_aac_bitrate(u32 bitrate);

/*----------------------------------------------------------------------------*/
/**@brief    蓝牙模式变量初始化
   @param    无
   @return   无
   @note
*/
/*----------------------------------------------------------------------------*/
void bt_var_init()
{
    memset((u8 *)&bt_user_priv_var, 0, sizeof(BT_USER_PRIV_VAR));
}

void set_smartbox_watch_upgrade_flag(u8 flag)
{
    __this->smartbox_watch_upgrade_flag = flag;
}

/*----------------------------------------------------------------------------*/
/**@brief    蓝牙模式协议栈回调函数
   @param    无
   @return   无
   @note
*/
/*----------------------------------------------------------------------------*/
void bredr_handle_register()
{
#if (APP_ONLINE_DEBUG)
    extern void online_spp_init(void);
    spp_data_deal_handle_register(user_spp_data_handler);
    online_spp_init();

#else
#if TRANS_DATA_EN
    spp_data_deal_handle_register(user_spp_data_handler);
#else
    spp_data_deal_handle_register(spp_data_handler);
#endif
#endif /*((APP_ONLINE_DEBUG) && (TCFG_COMM_TYPE == TCFG_SPP_COMM))*/
#if BT_SUPPORT_MUSIC_VOL_SYNC
    ///蓝牙音乐和通话音量同步
    music_vol_change_handle_register(bt_set_music_device_volume, phone_get_device_vol);
#endif

#if BT_SUPPORT_DISPLAY_BAT
    ///电量显示获取电量的接口
    get_battery_value_register(bt_get_battery_value);
#endif

    ///被测试盒链接上进入快速测试回调
    bt_fast_test_handle_register(bt_fast_test_api);

    ///样机进入dut被测试仪器链接上回调
    bt_dut_test_handle_register(bt_dut_api);

    ///获取远端设备蓝牙名字回调
    read_remote_name_handle_register(bt_read_remote_name);

    ////获取歌曲信息回调
#if USER_SUPPORT_PROFILE_BIP == 1
    bt_music_info_handle_register(user_get_bt_music_info);
#endif

}


/*----------------------------------------------------------------------------*/
/**@brief    蓝牙模式协议栈功能配置
   @param    无
   @return   无
   @note
*/
/*----------------------------------------------------------------------------*/
void bt_function_select_init()
{
#if defined(CONFIG_CPU_BR28)
    // 3M包
    set_bt_data_rate_acl_3mbs_mode(1);
    __set_support_3M_size(1);
#endif /* #if defined(CONFIG_CPU_BR28) */
    __set_sbc_cap_bitpool(38);
    __set_aac_bitrate(131 * 1000);
    set_idle_period_slot(1600);
    ////设置协议栈支持设备数
    __set_user_ctrl_conn_num(TCFG_BD_NUM);
    ////msbc功能使能
    __set_support_msbc_flag(1);
#if TCFG_BT_SUPPORT_AAC
    ////AAC功能使能
    __set_support_aac_flag(1);
#else
    __set_support_aac_flag(0);
#endif

#if BT_SUPPORT_DISPLAY_BAT
    ////设置更新电池电量的时间间隔
    __bt_set_update_battery_time(60);
#else
    __bt_set_update_battery_time(0);
#endif

    ////回连搜索时间长度设置,可使用该函数注册使用，ms单位,u16
    __set_page_timeout_value(8000);

    ////回连时超时参数设置。ms单位。做主机有效
    __set_super_timeout_value(8000);

    ///禁止发射器远端发起通话
    __set_disable_hfp_ag_flag(1);
#if (TCFG_BD_NUM == 2)
    ////设置开机回链的设备个数
    /* __set_auto_conn_device_num(2); */
#endif

#if (TCFG_SPI_LCD_ENABLE)
#if TCFG_USER_EMITTER_ENABLE//带有屏幕的方案根据UI选项连接
    ////设置开机回链的设备个数
    __set_auto_conn_device_num(10);//彩屏方案支持10个设备的连接
#endif
#endif

#if BT_SUPPORT_MUSIC_VOL_SYNC
    ////设置音乐音量同步的表
    vol_sys_tab_init();
#endif

    ////设置蓝牙是否跑后台
    __set_user_background_goback(BACKGROUND_GOBACK); // 后台链接是否跳回蓝牙 1:跳回

    ////设置蓝牙加密的level
    //io_capabilities ; /*0: Display only 1: Display YesNo 2: KeyboardOnly 3: NoInputNoOutput*/
    //authentication_requirements: 0:not protect  1 :protect
    __set_simple_pair_param(3, 0, 2);

#if AUDIO_OUTPUT_INCLUDE_FM
    bt_set_ldos(2);
#endif

#if (USER_SUPPORT_PROFILE_PBAP==1)
    ////设置蓝牙设备类型
    __change_hci_class_type(BD_CLASS_CAR_AUDIO);
#endif

#if (TCFG_BT_SNIFF_ENABLE == 0)
    void lmp_set_sniff_disable(void);
    lmp_set_sniff_disable();
#endif


    /*
                TX     RX
       AI800x   PA13   PA12
       AC692x   PA13   PA12
       AC693x   PA8    PA9
       AC695x   PA9    PA10
       AC696x   PA9    PA10
       AC694x   PB1    PB2
       AC697x   PC2    PC3
       AC631x   PA7    PA8

    */
    ////设置蓝牙接收状态io输出，可以外接pa
    /* bt_set_rxtx_status_enable(1); */

#if TCFG_USER_BLE_ENABLE
    {
        u8 tmp_ble_addr[6];
#if DOUBLE_BT_SAME_MAC || (TCFG_BLE_DEMO_SELECT == DEF_BLE_DEMO_ADV)
        memcpy(tmp_ble_addr, (void *)bt_get_mac_addr(), 6);
#else
        lib_make_ble_address(tmp_ble_addr, (void *)bt_get_mac_addr());
#endif //
        le_controller_set_mac((void *)tmp_ble_addr);
        printf("\n-----edr + ble 's address-----");

#if TCFG_UI_ENABLE_QR_CODE
        //extern void qr_code_message_init(u8 * ble_addr, u8 * edr_addr, u8 * name);
        // qr_code_message_init(tmp_ble_addr, bt_get_mac_addr(), (u8 *)bt_get_local_name());
        extern int ui_qrcode_init();
        ui_qrcode_init();
        extern  void qr_code_message_init(void);
        qr_code_message_init();
#endif /* #if TCFG_UI_ENABLE_QR_CODE */

        printf_buf((void *)bt_get_mac_addr(), 6);
        printf_buf((void *)tmp_ble_addr, 6);
    }
#endif // TCFG_USER_BLE_ENABLE

#if (CONFIG_BT_MODE != BT_NORMAL)
    set_bt_enhanced_power_control(1);
#endif
}

static u16 reset_idle_period_timeout = 0;
static void reset_idle_period_slot(void *p)
{
    reset_idle_period_timeout = 0;
    set_idle_period_slot(1600);
}
extern void link_task_idle_run(int slot);
void ble_ctkd_pair_complete(hci_con_handle_t con_handle)
{
    if (reset_idle_period_timeout) {
        sys_timeout_del(reset_idle_period_timeout);
        reset_idle_period_timeout = 0;
    }
    printf("ble_ctkd_pair_complete\n");
    set_idle_period_slot(400);
    u8 is_bredr_close(void);
    if (is_bredr_close() == 0) {
        link_task_idle_run(4);
    }
    reset_idle_period_timeout = sys_timeout_add(NULL, reset_idle_period_slot, 20 * 1000);
}

u16 bt_esco_by_watch_time = 0;
extern void __set_bt_esco_by_watch(bool flag);
extern bool get_bt_esco_by_watch();
extern void lmp_esco_rejust_establish(u8 value);
/* ***************************************************************************/
/**
 * @brief   ：手机接听电话后定时器函数
   @param
   @return
   @note    用于判断手机接听后一秒内禁止esco连接的计时函数
            bt_esco_by_watch_time = sys_timeout_add (NULL,bt_esco_conn_time,1000);
 */
/* ***************************************************************************/
void bt_esco_conn_time()
{
    lmp_esco_rejust_establish(0);
    if (bt_esco_by_watch_time) {
        sys_timeout_del(bt_esco_by_watch_time);
        bt_esco_by_watch_time = 0;
    }
}

#define WATCH_SCO_DLY_MS				(300) // 延时断开sco链路

static u16 disable_sco_timer = 0;

static void watch_sco_dly_time_del(void)
{
    if (disable_sco_timer) {
        sys_timeout_del(disable_sco_timer);
        disable_sco_timer = 0;
    }
}

static void send_sco_disconn(void *priv)
{
    disable_sco_timer = 0;
    user_send_cmd_prepare(USER_CTRL_DISCONN_SCO, 0, NULL);
}

void watch_sco_dly_disconnection()
{
    watch_sco_dly_time_del();
    disable_sco_timer = sys_timeout_add(NULL, send_sco_disconn, WATCH_SCO_DLY_MS);
}

/* ***************************************************************************/
/**
 * @brief   ：手机接听电话后定时时间内禁止esco连接
   @param
   @return
   @note    用于判断手机接听后一秒内禁止esco连接的函数
            放于esco发生连接状态中调用
 */
/* ***************************************************************************/
bool bt_esco_ban_conn()
{
    if (bt_esco_by_watch_time) {
        watch_sco_dly_disconnection();
        return true;
    }
    return false;
}

/*-------------------定频测试添加接口----------------------------------------*/
void bt_fix_txrx_api(u8 mode, u8 *mac_addr, u8 fre, u8 packet_type, u16 payload)
{
    bt_dut_api(1);
    local_irq_disable();
    link_fix_txrx_disable();
    if (mode) {
        link_fix_rx_enable(mac_addr, fre, packet_type, 0xffff);
    } else {
        link_fix_tx_enable(mac_addr, fre, packet_type, 0xffff);
    }
    local_irq_enable();
}


void bt_updata_fix_rx_result()
{
    struct link_fix_rx_result fix_rx_result;
    link_fix_rx_update_result(&fix_rx_result);
    printf("logerr_b:%d sum_b:%d perr:%d herr_b:%d invaile:%d  \n",
           fix_rx_result.rx_err_b,
           fix_rx_result.rx_sum_b,
           fix_rx_result.rx_perr_p,
           fix_rx_result.rx_herr_p,
           fix_rx_result.rx_invail_p
          );
}
/*定频测试的时候tx的mac_addr设置成rx的地址 */
u8 fix_rx_mac_addr[6] = {0xC0, 0xFF, 0x68, 0x7D, 0x16, 0x00};

/*----------------------------------------------------------------------------*/
/**@brief    蓝牙模式协议栈对应状态处理函数
   @param    bt:事件
   @return
   @note     蓝牙初始化完成、链接、通话播歌等状态
*/
/*----------------------------------------------------------------------------*/
static int bt_connction_status_event_handler(struct bt_event *bt)
{

    log_debug("-----------------------bt_connction_status_event_handler %d", bt->event);
    log_info("TS:%d bt_connction_status_event_handler event %d", timer_get_ms(), bt->event);

    if (bt_status_event_filter(bt) == false) {
        return false;
    }

#if GFPS_EN
    extern int gfps_bt_status_event_handler(int *msg);
    gfps_bt_status_event_handler((int *)bt);
#endif

    //运动相关的
    static u8 sport_phone_memory = 0;
    struct watch_execise __execise_hd;
    watch_execise_handle_get(&__execise_hd);
    u8 sport_status = __execise_hd.execise_ctrl_status_get();
    switch (bt->event) {
    case BT_STATUS_EXIT_OK:
        log_info("BT_STATUS_EXIT_OK\n");
        break;
    case BT_STATUS_INIT_OK:
        log_info("BT_STATUS_INIT_OK\n");
#if WECHAT_SPORT_ENABLE
        extern void att_profile_init();
        att_profile_init();
#endif
        bt_status_init_ok(bt);
        break;
    case BT_STATUS_START_CONNECTED:
        log_info(" BT_STATUS_START_CONNECTED\n");
#if TCFG_USER_EMITTER_ENABLE
        user_send_cmd_prepare(USER_CTRL_INQUIRY_CANCEL, 0, NULL);
#endif
        break;
    case BT_STATUS_ENCRY_COMPLETE:
        log_info(" BT_STATUS_ENCRY_COMPLETE\n");
        break;
    case BT_STATUS_SECOND_CONNECTED:
        log_info(" BT_STATUS_SECOND_CONNECTED\n");
        clear_current_poweron_memory_search_index(0);
    case BT_STATUS_FIRST_CONNECTED:
        log_info("BT_STATUS_CONNECTED\n");
        bt_status_connect(bt);
        break;
    case BT_STATUS_FIRST_DISCONNECT:
    case BT_STATUS_SECOND_DISCONNECT:
        log_info(" BT_STATUS_SECOND_DISCONNECT\n");
        bt_status_disconnect(bt);
        break;
    case BT_STATUS_PHONE_INCOME:
        log_info("BT_STATUS_PHONE_INCOME\n");
        //当手机app与手表传输过程中不能响应通话事件
        if (__this->smartbox_watch_upgrade_flag) {
            break;
        }
        UI_WINDOW_PREEMPTION_POSH(ID_WINDOW_PHONE, NULL, NULL, UI_WINDOW_PREEMPTION_TYPE_PHONE);
        log_info(" ui_moto_run 1");
#if TCFG_UI_ENABLE_MOTO
        UI_MOTO_RUN(1);
#endif
        bt_status_phone_income(bt);
#if TCFG_UI_ENABLE_PHONEBOOK
        set_call_log_date(NULL);
        set_call_log_type(2);
#endif /* #if TCFG_UI_ENABLE_PHONEBOOK */
        break;
    case BT_STATUS_PHONE_OUT:
        log_info("BT_STATUS_PHONE_OUT\n");
        //当手机app与手表传输过程中不能响应通话事件
        if (__this->smartbox_watch_upgrade_flag) {
            break;
        }
        log_info(" ui_moto_run 0");
#if TCFG_UI_ENABLE_MOTO
        UI_MOTO_RUN(0);
#endif
        bt_status_phone_out(bt);
#if WATCH_CALL_CHOOSE
        if (get_bt_esco_by_watch() == FALSE) { // 增加手机去电mic使用手机端，同时会禁掉微信来电走手表这个功能
            /* 延时200ms发送断开esco的命令，测试发现立马发送断开命令，手机会再次发送esco请求 */
            /* 此处定时1s，在1s内发起的esco链路都会被断开 */
            bt_esco_by_watch_time = sys_timeout_add(NULL, bt_esco_conn_time, 1000);
            break;
        }
#endif
        UI_WINDOW_PREEMPTION_POSH(ID_WINDOW_PHONE, NULL, NULL, UI_WINDOW_PREEMPTION_TYPE_PHONE);
#if TCFG_UI_ENABLE_PHONEBOOK
        set_call_log_date(NULL);
#endif /* #if TCFG_UI_ENABLE_PHONEBOOK */
        break;
    case BT_STATUS_PHONE_ACTIVE:
        log_info("BT_STATUS_PHONE_ACTIVE\n");
        //当手机app与手表传输过程中不能响应通话事件
        if (__this->smartbox_watch_upgrade_flag) {
            break;
        }
#if TCFG_APP_CAT1_EN
        if (APP_CAT1_TASK == app_get_curr_task()) {// cat1通话中，不响应
            break;
        }
#endif /* #if TCFG_APP_CAT1_EN */
#if TCFG_UI_ENABLE_MOTO
        UI_MOTO_RUN(0);
#endif
        bt_status_phone_active(bt);
#if WATCH_CALL_CHOOSE
        if (get_bt_esco_by_watch() == FALSE) {
            /* 延时200ms发送断开esco的命令，测试发现立马发送断开命令，手机会再次发送esco请求 */
            /* 此处定时1.5s，在1.5s内发起的esco链路都会被断开 */ /*  针对iphone se修改增加定时 */
            watch_sco_dly_disconnection();
            bt_esco_by_watch_time = sys_timeout_add(NULL, bt_esco_conn_time, 1500);
#if TCFG_UI_ENABLE
            if (UI_GET_WINDOW_ID() == ID_WINDOW_PHONE) {//判断手表在来电界面，退出界面
                UI_WINDOW_PREEMPTION_POP(ID_WINDOW_PHONE);
            }
#endif /* #if TCFG_UI_ENABLE */
            break;
        }
#endif
        UI_MSG_POST("phone_active:status=%4", 1);
        break;
    case BT_STATUS_PHONE_HANGUP:
        log_info(" BT_STATUS_PHONE_HANGUP\n");
        //当手机app与手表传输过程中不能响应通话事件
        if (__this->smartbox_watch_upgrade_flag) {
            // 添加这个判断是防止升级过程中出现挂断多次电话的情况
            if (app_get_curr_task() != APP_SMARTBOX_ACTION_TASK) {
                __this->smartbox_watch_upgrade_flag = 0;
            }
            break;
        }
#if TCFG_UI_ENABLE_MOTO
        UI_MOTO_RUN(0);
#endif
        bt_status_phone_hangup(bt);
        __set_bt_esco_by_watch(0);
#if TCFG_UI_ENABLE
        if (UI_GET_WINDOW_ID() != ID_WINDOW_PHONE) {//判断手表在来电界面，退出界面
            break;
        }
#if TCFG_APP_CAT1_EN
        if (APP_CAT1_TASK == app_get_curr_task()) {// cat1通话中，不响应
            break;
        }
#endif /* #if TCFG_APP_CAT1_EN */
        ui_hide_main(ID_WINDOW_PHONE);
#endif /* #if TCFG_UI_ENABLE */
#if TCFG_UI_ENABLE_PHONEBOOK
        update_call_log_message();
#endif /* #if TCFG_UI_ENABLE_PHONEBOOK */
        UI_WINDOW_PREEMPTION_POP(ID_WINDOW_PHONE);
        break;
    case BT_STATUS_PHONE_NUMBER:
        log_info("BT_STATUS_PHONE_NUMBER\n");
#if USER_SUPPORT_PROFILE_PBAP_LIST
        user_send_cmd_prepare(USER_CTRL_PBAP_READ_LIST, 0, NULL);
#endif
        bt_status_phone_number(bt);
#if TCFG_UI_ENABLE_MOTO
        if (get_call_status() == BT_CALL_INCOMING) {
            log_info(" ui_moto_run 1");
            UI_MOTO_RUN(1);
        }
#endif
#if TCFG_UI_ENABLE_PHONEBOOK
        u8 call_name[20] = {0};
        if (phonebook_get_name_by_number(bt->value, call_name)) {
            set_call_log_name(call_name);
        }
        set_call_log_number((u8 *)bt->value);
#endif /* #if TCFG_UI_ENABLE_PHONEBOOK */
        break;
    case BT_STATUS_PHONE_NANE:
        log_info("BT_STATUS_PHONE_NANE\n");
        bt_status_phone_name(bt);
        break;
    case BT_STATUS_INBAND_RINGTONE:
        log_info("BT_STATUS_INBAND_RINGTONE\n");
        bt_status_inband_ringtone(bt);
        break;
    case BT_STATUS_BEGIN_AUTO_CON:
        log_info("BT_STATUS_BEGIN_AUTO_CON\n");
        break;
    case BT_STATUS_A2DP_MEDIA_START:
        log_info(" BT_STATUS_A2DP_MEDIA_START\n");
        bt_status_a2dp_media_start(bt);
        break;
    case BT_STATUS_A2DP_MEDIA_STOP:
        log_info(" BT_STATUS_A2DP_MEDIA_STOP");
        bt_status_a2dp_media_stop(bt);
        break;
    case BT_STATUS_SCO_STATUS_CHANGE:
        log_info(" BT_STATUS_SCO_STATUS_CHANGE");
        log_info(" ui_moto_run 0");
#if TCFG_UI_ENABLE_MOTO
        UI_MOTO_RUN(0);
#endif
#if WATCH_CALL_CHOOSE
        if (bt->value != 0xff) {
            if (bt_esco_ban_conn()) {
                break;
            } else if (get_bt_esco_by_watch() == FALSE) {
                /* 用于判断部分手机来电铃声会发起esco请求 */
#if !BT_INBAND_RINGTONE
                if (get_call_status() != BT_CALL_ACTIVE) {
                    watch_sco_dly_disconnection();
                    break;
                }
#endif
            }
        } else {
            watch_sco_dly_time_del();
        }
#endif
        bt_status_sco_change(bt);
        break;
    case BT_STATUS_CALL_VOL_CHANGE:
        log_info(" BT_STATUS_CALL_VOL_CHANGE ");
        bt_status_call_vol_change(bt);
        break;
    case BT_STATUS_SNIFF_STATE_UPDATE:
        log_info(" BT_STATUS_SNIFF_STATE_UPDATE \n");    //0退出SNIFF
        bt_status_sniff_state_update(bt);
        break;
    case BT_STATUS_LAST_CALL_TYPE_CHANGE:
        log_info("BT_STATUS_LAST_CALL_TYPE_CHANGE\n");
        bt_status_last_call_type_change(bt);
        break;
    case BT_STATUS_CONN_A2DP_CH:
        bt_status_conn_a2dp_ch(bt);
        break;
    case BT_STATUS_CONN_HFP_CH:
        bt_status_conn_hfp_ch(bt);
        break;
    case BT_STATUS_PHONE_MANUFACTURER:
        log_info("BT_STATUS_PHONE_MANUFACTURER\n");
        bt_status_phone_menufactuer(bt);
        break;
    case BT_STATUS_VOICE_RECOGNITION:
        log_info(" BT_STATUS_VOICE_RECOGNITION \n");
        bt_status_voice_recognition(bt);
        break;
    case BT_STATUS_AVRCP_INCOME_OPID:
        log_info("  BT_STATUS_AVRCP_INCOME_OPID \n");
        bt_status_avrcp_income_opid(bt);
        break;
    case  BT_STATUS_RECONN_OR_CONN:
        log_info("  BT_STATUS_RECONN_OR_CONN \n");
#if USER_SUPPORT_PROFILE_PBAP_LIST
        user_send_cmd_prepare(USER_CTRL_PBAP_CONNECT, 0, NULL);
#endif
#if defined USER_SUPPORT_PROFILE_PAN && USER_SUPPORT_PROFILE_PAN
        user_pan_send_cmd(NULL, 2, 0, NULL);
#endif
#if USER_SUPPORT_PROFILE_MAP
        log_info("USER_CTRL_MAP_READ_TIME");
        user_send_cmd_prepare(USER_CTRL_MAP_READ_TIME, 0, NULL);
#endif
        break;
    default:
        log_info(" BT STATUS DEFAULT\n");
        break;
    }
    return 0;
}




static int bt_update_hci_event_handler(struct bt_event *bt)
{
    //对应原来的蓝牙连接上断开处理函数  ,bt->value=reason
    log_debug("------------bt_update_hci_event_handler reason %x %x", bt->event, bt->value);
    log_info("TS:%d bt_update_hci_event_handler event %x value %x", timer_get_ms(), bt->event, bt->value);
    if (bt_hci_event_filter(bt) == 0) {
        return 0;
    }

#if GFPS_EN
    extern int gfps_hci_event_handler(int *event);
    gfps_hci_event_handler((int *)bt);
#endif

    switch (bt->event) {
    case HCI_EVENT_INQUIRY_COMPLETE:
        log_info(" HCI_EVENT_INQUIRY_COMPLETE \n");
        break;
    case HCI_EVENT_IO_CAPABILITY_REQUEST:
        log_info(" HCI_EVENT_IO_CAPABILITY_REQUEST \n");
        clock_add_set(BT_CONN_CLK);
        break;
    case HCI_EVENT_USER_CONFIRMATION_REQUEST:
        log_info(" HCI_EVENT_USER_CONFIRMATION_REQUEST \n");
        ///<可通过按键来确认是否配对 1：配对   0：取消
        bt_send_pair(1);
        clock_remove_set(BT_CONN_CLK);
        break;
    case HCI_EVENT_USER_PASSKEY_REQUEST:
        log_info(" HCI_EVENT_USER_PASSKEY_REQUEST \n");
        ///<可以开始输入6位passkey
        break;
    case HCI_EVENT_REMOTE_KEYPRESS_NOTIFICATION:
        log_info(" HCI_EVENT_REMOTE_KEYPRESS_NOTIFICATION %x\n", bt->value);
        ///<可用于显示输入passkey位置 value 0:start  1:enrer  2:earse   3:clear  4:complete
        break;
    case HCI_EVENT_USER_PRESSKEY_NOTIFICATION:
        log_info(" HCI_EVENT_USER_PRESSKEY_NOTIFICATION %x\n", bt->value);
        break;
    case HCI_EVENT_PIN_CODE_REQUEST :
        log_info("HCI_EVENT_PIN_CODE_REQUEST  \n");
        bt_send_pair(1);
        break;
    case HCI_EVENT_VENDOR_NO_RECONN_ADDR :
        log_info("HCI_EVENT_VENDOR_NO_RECONN_ADDR \n");
        /* user_send_cmd_prepare(USER_CTRL_WRITE_SCAN_ENABLE, 0, NULL); */
        user_send_cmd_prepare(USER_CTRL_WRITE_CONN_ENABLE, 0, NULL);
        break;
    case HCI_EVENT_DISCONNECTION_COMPLETE :
        log_info("HCI_EVENT_DISCONNECTION_COMPLETE \n");
        /* user_send_cmd_prepare(USER_CTRL_WRITE_SCAN_ENABLE, 0, NULL); */
        user_send_cmd_prepare(USER_CTRL_WRITE_CONN_ENABLE, 0, NULL);
        clock_remove_set(BT_CONN_CLK);
        break;
    case BTSTACK_EVENT_HCI_CONNECTIONS_DELETE:
    case HCI_EVENT_CONNECTION_COMPLETE:
        log_info(" HCI_EVENT_CONNECTION_COMPLETE \n");
        switch (bt->value) {
        case ERROR_CODE_SUCCESS :
            log_info("ERROR_CODE_SUCCESS  \n");
            break;
        case ERROR_CODE_PIN_OR_KEY_MISSING:
            log_info(" ERROR_CODE_PIN_OR_KEY_MISSING \n");
            bt_hci_event_linkkey_missing(bt);
        case ERROR_CODE_SYNCHRONOUS_CONNECTION_LIMIT_TO_A_DEVICE_EXCEEDED :
        case ERROR_CODE_CONNECTION_REJECTED_DUE_TO_LIMITED_RESOURCES:
        case ERROR_CODE_CONNECTION_REJECTED_DUE_TO_UNACCEPTABLE_BD_ADDR:
        case ERROR_CODE_CONNECTION_ACCEPT_TIMEOUT_EXCEEDED  :
        case ERROR_CODE_REMOTE_USER_TERMINATED_CONNECTION   :
        case ERROR_CODE_CONNECTION_TERMINATED_BY_LOCAL_HOST :
        case ERROR_CODE_AUTHENTICATION_FAILURE :
        case CUSTOM_BB_AUTO_CANCEL_PAGE:
        case ERROR_CODE_PAGE_TIMEOUT:
        case ERROR_CODE_CONNECTION_TIMEOUT:
        case ERROR_CODE_ACL_CONNECTION_ALREADY_EXISTS  :
            /* user_send_cmd_prepare(USER_CTRL_WRITE_SCAN_ENABLE, 0, NULL); */
            user_send_cmd_prepare(USER_CTRL_WRITE_CONN_ENABLE, 0, NULL);
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }
    return 0;
}


static int bt_update_connction_status_event_handler(struct bt_event *bt)
{
    log_debug("------------bt_update_connction_status_event_handler %d", bt->event);

    switch (bt->event) {
    case BT_STATUS_INIT_OK:
        log_info("BT_STATUS_INIT_OK\n");

#if TCFG_USER_EMITTER_ENABLE
        ////发射器设置回调等
        void bt_emitter_init() ;
        bt_emitter_init();
        extern u8 emitter_search_result(char *name, u8 name_len, u8 * addr, u32 dev_class, char rssi);
        inquiry_result_handle_register(emitter_search_result);
        extern void emitter_or_receiver_switch(u8 flag);

        emitter_or_receiver_switch(BT_EMITTER_EN);
#endif
        /* user_send_cmd_prepare(USER_CTRL_WRITE_SCAN_ENABLE, 0, NULL);
        user_send_cmd_prepare(USER_CTRL_WRITE_CONN_ENABLE, 0, NULL); */
        break;
    }
    return 0;
}

int bt_update_event_handler(struct sys_event *event)
{
    int ret = false;
    switch (event->type) {
    case SYS_BT_EVENT:
        if ((u32)event->arg == SYS_BT_EVENT_TYPE_CON_STATUS) {
            bt_update_connction_status_event_handler(&event->u.bt);
        } else if ((u32)event->arg == SYS_BT_EVENT_TYPE_HCI_STATUS) {
            bt_update_hci_event_handler(&event->u.bt);
        }
        return true;
    default:
        return false;
    }
    return ret;
}



/*----------------------------------------------------------------------------*/
/**@brief    蓝牙模式协议栈事件
   @param    bt:事件
   @return
   @note     蓝牙底层事件,通过app层处理
*/
/*----------------------------------------------------------------------------*/
static int bt_hci_event_handler(struct bt_event *bt)
{
    //对应原来的蓝牙连接上断开处理函数  ,bt->value=reason
    log_debug("------------------------bt_hci_event_handler reason %x %x", bt->event, bt->value);
    log_info("TS:%d bt_hci_event_handler event %x value %x", timer_get_ms(), bt->event, bt->value);

    UI_MSG_POST("bt_status:hci_event=%4", bt->event);
    if (bt_hci_event_filter(bt) == 0) {
        return 0;
    }

    switch (bt->event) {
    case HCI_EVENT_INQUIRY_COMPLETE:
        log_info(" HCI_EVENT_INQUIRY_COMPLETE \n");
        bt_hci_event_inquiry(bt);
        break;
    case HCI_EVENT_IO_CAPABILITY_REQUEST:
        log_info(" HCI_EVENT_IO_CAPABILITY_REQUEST \n");
        clock_add_set(BT_CONN_CLK);
        break;
    case HCI_EVENT_USER_CONFIRMATION_REQUEST:
        log_info(" HCI_EVENT_USER_CONFIRMATION_REQUEST \n");
        ///<可通过按键来确认是否配对 1：配对   0：取消
        bt_send_pair(1);
        clock_remove_set(BT_CONN_CLK);
        break;
    case HCI_EVENT_USER_PASSKEY_REQUEST:
        log_info(" HCI_EVENT_USER_PASSKEY_REQUEST \n");
        ///<可以开始输入6位passkey
        break;
    case HCI_EVENT_USER_PRESSKEY_NOTIFICATION:
        log_info(" HCI_EVENT_USER_PRESSKEY_NOTIFICATION %x\n", bt->value);
        ///<可用于显示输入passkey位置 value 0:start  1:enrer  2:earse   3:clear  4:complete
        break;
    case HCI_EVENT_PIN_CODE_REQUEST :
        log_info("HCI_EVENT_PIN_CODE_REQUEST  \n");
        bt_send_pair(1);
        break;
    case HCI_EVENT_VENDOR_NO_RECONN_ADDR :
        log_info("HCI_EVENT_VENDOR_NO_RECONN_ADDR \n");
        bt_hci_event_disconnect(bt) ;
        break;
    case HCI_EVENT_DISCONNECTION_COMPLETE :
        log_info("HCI_EVENT_DISCONNECTION_COMPLETE \n");
        if (bt->value == ERROR_CODE_PAIRING_NOT_ALLOWED) { //用于判断华为手表的断开情况
            UI_MSG_POST("bt_status:hci_value=%4", bt->value);
        }
        bt_hci_event_disconnect(bt) ;
        clock_remove_set(BT_CONN_CLK);
        break;
    case BTSTACK_EVENT_HCI_CONNECTIONS_DELETE:
    case HCI_EVENT_CONNECTION_COMPLETE:
        log_info(" HCI_EVENT_CONNECTION_COMPLETE \n");
        UI_MSG_POST("bt_status:hci_value=%4", bt->value);
        switch (bt->value) {
        case ERROR_CODE_SUCCESS :
            log_info("ERROR_CODE_SUCCESS  \n");
            bt_hci_event_connection(bt);
            break;
        case ERROR_CODE_PIN_OR_KEY_MISSING:
            log_info(" ERROR_CODE_PIN_OR_KEY_MISSING \n");
            bt_hci_event_linkkey_missing(bt);
        case ERROR_CODE_SYNCHRONOUS_CONNECTION_LIMIT_TO_A_DEVICE_EXCEEDED :
        case ERROR_CODE_CONNECTION_REJECTED_DUE_TO_LIMITED_RESOURCES:
        case ERROR_CODE_CONNECTION_REJECTED_DUE_TO_UNACCEPTABLE_BD_ADDR:
        case ERROR_CODE_CONNECTION_ACCEPT_TIMEOUT_EXCEEDED  :
        case ERROR_CODE_REMOTE_USER_TERMINATED_CONNECTION   :
        case ERROR_CODE_CONNECTION_TERMINATED_BY_LOCAL_HOST :
        case ERROR_CODE_AUTHENTICATION_FAILURE :
            bt_hci_event_disconnect(bt) ;
            break;
        case CUSTOM_BB_AUTO_CANCEL_PAGE:
            bt_wait_phone_connect_control(1);
            break;
        case ERROR_CODE_PAGE_TIMEOUT:
            log_info(" ERROR_CODE_PAGE_TIMEOUT \n");
            bt_hci_event_page_timeout(bt);
            break;
        case ERROR_CODE_CONNECTION_TIMEOUT:
            log_info(" ERROR_CODE_CONNECTION_TIMEOUT \n");
            bt_hci_event_connection_timeout(bt);
            break;
        case ERROR_CODE_ACL_CONNECTION_ALREADY_EXISTS  :
            log_info("ERROR_CODE_ACL_CONNECTION_ALREADY_EXISTS   \n");
            bt_hci_event_connection_exist(bt);
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }
    return 0;
}



/*----------------------------------------------------------------------------*/
/**@brief    蓝牙后台事件过滤处理
   @param    event: 事件
   @return   0:不需要切换模式， 1:需要切换模式，2:通话导致需要切换
   @note
*/
/*----------------------------------------------------------------------------*/
int bt_background_event_handler_filter(struct sys_event *event)
{
    u8 ret = 0;
#if TCFG_BLUETOOTH_BACK_MODE
    u8 send = 0;
    if ((u32)event->arg == SYS_BT_EVENT_TYPE_CON_STATUS) {
        log_info("bt con event: %d \n", event->u.bt.event);
        switch (event->u.bt.event) {
        // 需要切换蓝牙的命令
        /* case BT_STATUS_A2DP_MEDIA_START: */
        /*     if (__this->sbc_packet_step != 0) { */
        /*         #<{(| log_info("sbc_packet_step : %d \n", __this->sbc_packet_step); |)}># */
        /*         break; */
        /*     } */
        /*  */
        case BT_STATUS_FIRST_DISCONNECT:
        case BT_STATUS_SECOND_DISCONNECT:
            //关机导致的断开不可以回去蓝牙，否则后台关机会有问题
            if (app_var.goto_poweroff_flag) {
                break;
            }
#if BACKGROUND_GOBACK
            ret = 1;
#else
            //判断断开的是sink设备，默认切换蓝牙
            if (event->u.bt.value) {
                /* ret = 1; */
                /* bt_status_disconnect_background(&event->u.bt); */
            }
            if (ret == 0) {
                bt_status_disconnect_background(&event->u.bt);
            }
#endif
            break;

        case BT_STATUS_SECOND_CONNECTED:
        case BT_STATUS_FIRST_CONNECTED:
#if BACKGROUND_GOBACK
            ret = 1;
#else
            bt_status_connect_background(&event->u.bt);
#endif

            if (__this->smartbox_watch_upgrade_flag) {
                lmp_esco_rejust_establish(1);
            }
            break;

        case BT_STATUS_START_CONNECTED:
#if BACKGROUND_GOBACK
            ret = 1;
#endif
            break;

        case  BT_STATUS_ENCRY_COMPLETE:
            break;
        case BT_STATUS_SCO_STATUS_CHANGE:
            ret = 1;
            break;
        case BT_STATUS_LAST_CALL_TYPE_CHANGE:
            bt_status_last_call_type_change(&event->u.bt);
        case BT_STATUS_VOICE_RECOGNITION:
        case BT_STATUS_PHONE_INCOME:
        case BT_STATUS_PHONE_NUMBER:
        /* case BT_STATUS_PHONE_MANUFACTURER: */
        case BT_STATUS_PHONE_OUT:
        case BT_STATUS_PHONE_ACTIVE:
            /* case BT_STATUS_PHONE_HANGUP: */
            //当手机app与手表传输过程中不能响应通话事件
            if (__this->smartbox_watch_upgrade_flag) {
                break;
            }
#if TCFG_APP_CAT1_EN
            if (APP_CAT1_TASK == app_get_curr_task()) {// cat1通话中，不响应
                break;
            }
#endif /* #if TCFG_APP_CAT1_EN */
            ret = 2;
            break;
        // 不需要处理的命令
        case BT_STATUS_INIT_OK:
            bt_status_init_ok_background(&event->u.bt);
            break;
        case BT_STATUS_A2DP_MEDIA_STOP:
            bt_drop_a2dp_frame_stop();
        case BT_STATUS_CALL_VOL_CHANGE:
            break;
        // 按原方式处理的命令
        default:
            send = 1;
            u8 temp_flag = (u8) - 1;
            if (app_get_curr_task() == APP_SMARTBOX_ACTION_TASK) {
                //后台不清升级变量
                temp_flag = __this->smartbox_watch_upgrade_flag;
            }
            bt_connction_status_event_handler(&event->u.bt);
            if (temp_flag != (u8) - 1) {
                __this->smartbox_watch_upgrade_flag = temp_flag;
            }
            break;

        }
        if (!send) {
            UI_MSG_POST("bt_status:event=%4", event->u.bt.event);
        }
    } else if ((u32)event->arg == SYS_BT_EVENT_TYPE_HCI_STATUS) {
        /* log_info("bt hci event: %d \n", event->u.bt.event); */
        switch (event->u.bt.event) {
        case HCI_EVENT_IO_CAPABILITY_REQUEST:
            clock_add_set(BT_CONN_CLK);
#if BACKGROUND_GOBACK
            ret = 1;
#endif
            break;
        default:
            bt_hci_event_handler(&event->u.bt);
            break;
        }
    }
#if TCFG_USER_TWS_ENABLE
    else if (((u32)event->arg == SYS_BT_EVENT_FROM_TWS)) {
        /* log_info("bt tws event: %d \n", event->u.bt.event); */
        switch (event->u.bt.event) {
        /* case TWS_EVENT_CONNECTED: */
        /* ret = 1; */
        /* break; */
        default:
            bt_tws_connction_status_event_handler(&event->u.bt);
#if (TCFG_DEC2TWS_ENABLE)
            localtws_bt_event_deal(&event->u.bt);
#endif
            break;
        }
    }
#endif
    else if ((u32)event->arg == SYS_EVENT_FROM_CTRLER) {
        switch (event->u.bt.event) {
        case  BTCTRLER_EVENT_RESUME_REQ:
            log_info("-------   BTCTRLER_EVENT_RESUME_REQ \n");

            if (__this->smartbox_watch_upgrade_flag) {
                break;
            }

            ret = 2;
            break;
        }
    }
#endif
    return ret;
}

/*----------------------------------------------------------------------------*/
/**@brief    蓝牙后台事件处理
   @param    event: 事件
   @return   无
   @note
*/
/*----------------------------------------------------------------------------*/
int bt_background_event_handler(struct sys_event *event)
{
    int ret = bt_background_event_handler_filter(event);
    if (ret) {
        if (false == app_check_curr_task(APP_BT_TASK)) {
            __this->cmd_flag = 1;
            if (ret == 2) {
                __this->call_flag = 1;
            } else {
                __this->call_flag = 0;
            }
            app_task_switch_to(APP_BT_TASK);
            sys_event_notify(event);
        }
    }
    return 0;
}

void bt_background_flag_clean(void)
{
    __this->cmd_flag = 0;
    __this->call_flag = 0;
}
void bt_charge_flag_set(void)
{
    __this->cmd_flag = 1;
}

#if FINDMY_EN
void fmy_other_device_bt_name_set(u8 *name)
{
    log_info("fmy change eir local name");
    bt_modify_name(name);
    // just set when in scan state
    if (!lmp_get_conn_num()) {
        log_info("bt scan disable");
        user_send_cmd_prepare(USER_CTRL_WRITE_SCAN_DISABLE, 0, NULL);
        user_send_cmd_prepare(USER_CTRL_WRITE_CONN_DISABLE, 0, NULL);
        log_info("bt scan enable");
        user_send_cmd_prepare(USER_CTRL_WRITE_SCAN_ENABLE, 0, NULL);
        user_send_cmd_prepare(USER_CTRL_WRITE_CONN_ENABLE, 0, NULL);
    }
}
#endif

/*----------------------------------------------------------------------------*/
/**@brief    蓝牙模式按键响应处理
   @param    event: 按键
   @return   1:事件处理  0:事件无处理
   @note
*/
/*----------------------------------------------------------------------------*/
int bt_key_event_handler(struct sys_event *event)
{
    int ret = true;
    struct key_event *key = &event->u.key;
    int key_event = event->u.key.event;
    int key_value = event->u.key.value;


    if (bt_key_event_filter_before() == false) {
        return false;
    }

    if (bt_ui_key_event_filter(key_event) == true) { //判断为ui 按键直接到ui
        return true;
    }

    log_debug("bt key_event:%d %d %d %d\n", key_event, key->value, key->event, key->init);

#if FINDMY_EN
    extern void fmy_key_process(u8 key_event);
    if (key_event == 64) {  // short
        fmy_key_process(key_event);
        return true;
    } else if (key_event == 66) {   // long
        fmy_key_process(key_event);
        return true;
    }
#endif

    if (bt_key_event_filter_after(key_event) == true) {
        return true;
    }

    static int curr_page = 0;

    switch (key_event) {
    case  KEY_MUSIC_PP:
        log_info("   KEY_MUSIC_PP  \n");
        bt_key_music_pp();
        break;
    case  KEY_MUSIC_PREV:
        log_info("    KEY_MUSIC_PREV \n");
        bt_key_music_prev();
        break;
    case  KEY_MUSIC_NEXT:
        log_info("    KEY_MUSIC_NEXT \n");
        bt_key_music_next();
        break;
    case  KEY_VOL_UP:
        log_info("    KEY_VOL_UP \n");
        bt_key_vol_up();
        break;
    case  KEY_VOL_DOWN:
        log_info("    KEY_VOL_DOWN \n");
        bt_key_vol_down();
        break;
    case  KEY_CALL_LAST_NO:
        log_info("    KEY_CALL_LAST_NO \n");
        bt_key_call_last_on();
        break;
    case  KEY_CALL_HANG_UP:
        log_info("    KEY_CALL_HANG_UP \n");
        bt_key_call_hand_up();
        break;
    case  KEY_CALL_ANSWER:
        log_info("    KEY_CALL_ANSWER \n");
        bt_key_call_answer();
        break;
    case  KEY_OPEN_SIRI:
        log_info("    KEY_OPEN_SIRI \n");
        bt_key_call_siri();
        break;
    case  KEY_HID_CONTROL:
        log_info("    KEY_HID_CONTROL \n");
        bt_key_hid_control();
        break;
    case KEY_THIRD_CLICK:
        log_info("    KEY_THIRD_CLICK \n");
        bt_key_third_click(event);
        break;
    case KEY_LOW_LANTECY:
        log_info("    KEY_LOW_LANTECY \n");
        bt_key_low_lantecy();
        break;
    case  KEY_NULL:
        log_info("    KEY_NULL \n");
        ret = false;
#if TCFG_USER_TWS_ENABLE
        if ((u32)event->arg == KEY_EVENT_FROM_TWS) {
            break;
        }
#endif
        break;

    case KEY_HID_MODE_SWITCH:
        __this->hid_mode = !__this->hid_mode;
        printf("KEY_HID_TAKE_PICTURE, %d\n", __this->hid_mode);
        user_change_profile_mode(__this->hid_mode);///HID设备名称， 默认蓝牙名称追加_HID
        break;
    case KEY_HID_TAKE_PICTURE:
        if (__this->hid_mode) {
            printf("KEY_HID_TAKE_PICTURE\n");
            user_send_cmd_prepare(USER_CTRL_HID_BOTH, 0, NULL);
        }
        break;

    default:
        ret = false;
        break;
    }
#if (SMART_BOX_EN)
    extern int smartbox_bt_key_event_deal(u8 key_event, int ret);
    ret = smartbox_bt_key_event_deal(key_event, ret);
#endif
    return ret;
}


/*----------------------------------------------------------------------------*/
/**@brief    蓝牙模式协议栈状态事件、hci事件、对箱事件
   @param    event: 事件
   @return   无
   @note
*/
/*----------------------------------------------------------------------------*/
int bt_sys_event_office(struct sys_event *event)
{
    if ((u32)event->arg == SYS_BT_EVENT_TYPE_CON_STATUS) {
        bt_connction_status_event_handler(&event->u.bt);
    } else if ((u32)event->arg == SYS_BT_EVENT_TYPE_HCI_STATUS) {
        bt_hci_event_handler(&event->u.bt);
    } else if ((u32)event->arg == SYS_BT_EVENT_FORM_SELF) {
        bt_reverb_status_change(&event->u.bt);
    }
#if TCFG_USER_TWS_ENABLE
    else if (((u32)event->arg == SYS_BT_EVENT_FROM_TWS)) {
        bt_tws_connction_status_event_handler(&event->u.bt);
    }
#endif
    else if (((u32)event->arg == SYS_BT_EVENT_FROM_KEY)) {
        switch (event->u.key.event) {
        case KEY_CHANGE_MODE:
            app_task_switch_next();
            break;
        }
    }
    return false;
}


/*----------------------------------------------------------------------------*/
/**@brief    蓝牙模式事件消息处理
   @param    event: 事件
   @return   1:处理完成,消息不需要进入common处理  0:common defult处理
   @note
*/
/*----------------------------------------------------------------------------*/
extern int jl_kws_voice_event_handle(struct key_event *key);
static int bt_sys_event_handler(struct sys_event *event)
{
    int ret = false;
    struct key_event *key;
    switch (event->type) {
    case SYS_KEY_EVENT:
        key = &event->u.key;
        if (key->type == KEY_DRIVER_TYPE_VOICE) {
            /* 语音消息 */
            ret = jl_kws_voice_event_handle(event);
            break;
        }
        ret = bt_key_event_handler(event);
        break;
    case SYS_BT_EVENT:
        ret = bt_sys_event_office(event);
        break;
    case SYS_DEVICE_EVENT:
#if APP_ONLINE_DEBUG
        if ((u32)event->arg == DEVICE_EVENT_ONLINE_DATA) {
            putchar('%');
            extern void app_online_event_handle(int evt_value);
            app_online_event_handle(event->u.dev.value);
        }
#endif /* APP_ONLINE_DEBUG */
        break;
    default:
        return false;
    }
    // SYS_EVENT_HANDLER_SPECIFIC(event);
    return ret;
}



/*----------------------------------------------------------------------------*/
/**@brief    蓝牙模式
   @param
   @return   无
   @note
*/
/*----------------------------------------------------------------------------*/
u16 test_timer;
u32  printf_ble_test();
u32 printf_bredr_test();
static void bt_test_timer(void *p)
{
    u32 *tmp;
    u8 i;
    printf("<<<<<<<<<<<<<<<<");
    tmp = printf_bredr_test();
    for (i = 0; i < 3; i++) {
        printf("%d=%x ", i, tmp[i]);
    }
    for (i = 0; i < 3; i++) {
        printf("%d=%x ", i, tmp[i + 3]);
    }
    printf("------------");
    tmp = printf_ble_test();
    for (i = 0; i < 3; i++) {
        printf("%d=%x ", i, tmp[i]);
    }
    for (i = 0; i < 3; i++) {
        printf("%d=%x ", i, tmp[i + 3]);
    }
}

void app_bt_task()
{
    int msg[32];
    ui_update_status(STATUS_EXIT_LOWPOWER);
    extern void set_poweron_idle(u8 idle);
    extern int get_poweron_idle();
    if (!get_poweron_idle()) {
        set_poweron_idle(1);//防止直接切bt模式poweron卡住不进低功耗
        extern void ui_auto_shut_down_enable(void);
        ui_auto_shut_down_enable();
    }

    if (!__this->cmd_flag) { //蓝牙后台拉回蓝牙模式不播放提示音
        tone_play_by_path(tone_table[IDEX_TONE_BT_MODE], 1);
    }
#define WDT_IS_ENABLE() 		(P11_WDT->CON & BIT(5))
    printf("%s %d", __func__, WDT_IS_ENABLE());
    bt_task_start();

    if (CONFIG_VISUAL_BT_RF_POWER_LEVEL) {
        test_timer = sys_timer_add(NULL, bt_test_timer, 10000);
    }


    while (1) {
        app_task_get_msg(msg, ARRAY_SIZE(msg), 1);

        switch (msg[0]) {
        case APP_MSG_SYS_EVENT:
            if (bt_sys_event_handler((struct sys_event *)(msg + 1)) == false) {
                app_default_event_deal((struct sys_event *)(&msg[1]));
            }
            break;
        case APP_MSG_USER:
            // 处理用户自定义消息
            switch (msg[1]) {
            case 1: // APP_MSG_USER_SN
                log_info("处理UART SN消息\n");
                // SN已在UART线程写入CFG_DEVICE_SN，这里仅触发广播刷新
                extern void le_smartbox_ble_adv_restart(void);
                le_smartbox_ble_adv_restart();
                break;
            case 2: // APP_MSG_USER_AES_KEY
                log_info("处理UART AES_KEY消息\n");
                // AES已在UART线程写入CFG_DEVICE_AES_KEY，这里仅触发广播刷新
                extern void le_smartbox_ble_adv_restart(void);
                le_smartbox_ble_adv_restart();
                break;
            case 3: // APP_MSG_USER_PARING_INFORMATION
                log_info("处理UART配对信息消息\n");
                // 处理配对信息逻辑：
                // - 兼容旧逻辑：msg[2]=0(0x0037)/1(0x0040)
                // - 新增 UART 指定 PIN/passkey：msg[2]=FLAG|passkey(0~999999)
                {
                    const int arg0 = msg[2];
                    const uint32_t UART_PAIR_PASSKEY_FLAG = 0x40000000;
                    uint16_t protocol_id = 0x0037;

                    if (arg0 == 1) {
                        protocol_id = 0x0040;
                    }

                    if (((uint32_t)arg0 & UART_PAIR_PASSKEY_FLAG) != 0) {
                        uint32_t passkey = ((uint32_t)arg0) & 0x000FFFFF;
                        // 通知 BLE 配对模块：下次配对优先使用该 passkey（一次性）
                        extern void ble_pairing_set_uart_passkey(uint32_t passkey);
                        ble_pairing_set_uart_passkey(passkey);
                        extern void fill_protocol_set_uart_passkey(uint32_t passkey);
                        fill_protocol_set_uart_passkey(passkey);
                        protocol_id = 0x0037; // passkey 仅对 0x0037 配对流程生效
                    }

                    extern uint8_t ble_proto_ble_pair_req_Proc(uint16_t protocol_id);
                    ble_proto_ble_pair_req_Proc(protocol_id);
                }
                break;
            case 4: // APP_MSG_USER_READ_KEY_LIST
                log_info("处理UART读取密钥列表消息\n");
                // 处理读取密钥列表逻辑
                extern void send_ble_key_list(uint16_t protocol_id);
                send_ble_key_list(0x0038);      
                break;
            case 5: // APP_MSG_USER_TONPLAY
                log_info("音乐播放的逻辑\n");
                log_info("=======================================音乐播放=========================================");
                #define TONPLAY_VOL_THRESHOLD_LOW    14  // 低音量阈值百分比
                #define TONPLAY_VOL_THRESHOLD_MID    17  // 中音量阈值百分比
                #define TONPLAY_VOL_THRESHOLD_HIGH   20  // 高音量阈值百分比

                /* 添加当前的音量log，便于确认播放前的音量状态 */
                /* 仅用于调试，不改变实际播放流程 */
                extern u8 get_max_sys_vol(void);
                int current_vol = (int)app_audio_get_volume(APP_AUDIO_CURRENT_STATE);
                log_info("音效播放: 当前音量 %d / %d\n",
                         (int)app_audio_get_volume(APP_AUDIO_CURRENT_STATE),
                         (int)get_max_sys_vol());
                // 根据阈值判断音量档位（低/中/高）
                switch (current_vol)
                {
                case TONPLAY_VOL_THRESHOLD_HIGH:
                    log_info("音效播放: 当前为高音量档位\n");
                    break;
                case TONPLAY_VOL_THRESHOLD_MID:
                    log_info("音效播放: 当前为中音量档位\n");
                    break;
                case TONPLAY_VOL_THRESHOLD_LOW:
                    log_info("音效播放: 当前为低音量档位\n");
                    break;
                default:
                    break;
                }
                /* MCU->SOC 0x00F4 转过来的 case_id：先尝试自定义音效，再回落默认，并加互斥保护 */
                do {
                    int case_id = msg[2];
                    log_info("音效播放: 收到 case_id=%d\n", case_id);
                    if (case_id <= 0) {
                        log_info("音效播放: case_id 无效，范围错误: %d\n", case_id);
                        break;
                    }

                    extern const int tone_table_size;

                    /* 优先自定义音效映射：开机/关机/报警/hello 等 */
                    u8 tone_type = 0;
                    bool custom_mapped = bt_tone_case_to_custom_type(case_id, &tone_type);
                    if (custom_mapped) {
                        log_info("音效播放: case_id %d 映射到自定义音效类型 %d\n", case_id, tone_type);
                        // 自定义音效播放加互斥，避免并发冲突
                        tone_play_lock();
                        bt_tone_interrupt_current_locked();
                        bt_tone_pa_ctrl_set(1);
                        bool played = user_custom_tone_play_if_exist(tone_type, 1);
                        tone_play_unlock();
                        if (played) {
                            log_info("音效播放: 自定义音效播放成功\n");
                            break;
                        } else {
                            bt_tone_pa_ctrl_set(0);
                            log_info("音效播放: 未找到自定义音效，回退默认音效\n");
                        }
                    } else {
                        log_info("音效播放: case_id %d 未映射自定义音效，使用默认音效\n", case_id);
                    }

                    // 协议里的 case_id 直接表示音频编号：
                    // 0 -> tone0.*, 19 -> tone19.*
                    int user_idx = IDEX_TONE_USER_001 + case_id;
                    if (user_idx >= 0 && user_idx < tone_table_size && tone_table[user_idx]) {
                        log_info("音效播放: 播放默认音效，索引=%d\n", user_idx);
                        // 默认音效播放同样加互斥保护
                        tone_play_lock();
                        bt_tone_interrupt_current_locked();
                        bt_tone_pa_ctrl_set(1);
                        int ret = tone_play_with_callback_by_name((char *)tone_table[user_idx], 1, bt_tone_play_end_cb, NULL);
                        tone_play_unlock();
                        if (ret < 0) {
                            log_info("音效播放: 默认音效播放失败 ret=%d\n", ret);
                            bt_tone_pa_ctrl_set(0);
                        }
                    } else {
                        log_info("音效播放: 默认音效索引无效或为空，跳过。user_idx=%d size=%d\n",
                                 user_idx, tone_table_size);
                    }
                } while (0);
                log_info("=======================================音乐播放_end=========================================");
                break;
            case 6: // APP_MSG_USER_CUSTOM_TONEPLAY
                // packed: 高16位=tone_type(0=开机,1=关机,2=报警,3=hello), 低16位=原 case_id
                {
                    int packed = msg[2];
                    int case_id = packed & 0xFFFF;
                    u8 tone_type = (u8)((packed >> 16) & 0xFF);
                    tone_play_lock();
                    bt_tone_interrupt_current_locked();
                    bt_tone_pa_ctrl_set(1);
                    bool played = user_custom_tone_play_if_exist(tone_type, 1);
                    if (!played) {
                        bt_tone_pa_ctrl_set(0);
                        // 回退原来的内置提示音
                        if (case_id >= 0 && case_id < 122) {
                            int user_idx = IDEX_TONE_USER_001 + case_id;
                            bt_tone_pa_ctrl_set(1);
                            int ret = tone_play_with_callback_by_name((char *)tone_table[user_idx], 1, bt_tone_play_end_cb, NULL);
                            if (ret < 0) {
                                bt_tone_pa_ctrl_set(0);
                            }
                        } else {
                            log_info("CUSTOM_TONE fallback case_id out of range: %d\n", case_id);
                        }
                    }
                    tone_play_unlock();
                }
                break;
            default:
                log_info("未知用户消息: %d\n", msg[1]);
                break;
            }
            break;
        default:
            break;
        }

        if (app_task_exitting()) {
            bt_task_close();
            __this->wait_exit = 1;
        }

        if (__this->wait_exit) { //开始退出
            if (!__this->exiting) { ///等待蓝牙退出处理完成
                return;
            }
        }
    }
}

// 耳机/音箱按键处理
void forward_esco_key_handler(u8 cmd, u8 *packet, u16 size)
{
    static u32 esco_volume;
    u32 temp = 0;

    switch (cmd) {
    case ATCMD_CMD_CHUP:
        log_info("KEY_CALL_HANG_UP \n");
        bt_key_call_hand_up();
        break;
        /* case ATCMD_CMD_VGS: */
        /*     if(packet[size - 3] != 0x3d) { */
        /*         temp = packet[size - 3]*0x100 | packet[size - 2]; */
        /*     }else { */
        /*      */
        /*         temp = packet[size - 2]; */
        /*     } */
        /*     if(temp > esco_volume) { //调大音量 */
        /*         bt_key_vol_up(); */
        /*      */
        /*     } */
        /*     if(temp < esco_volume) { //调小音量 */
        /*         bt_key_vol_down(); */
        /*     } */
        /*     esco_volume = temp; */
        /*     break; */

    }

}

u8 bt_app_exit_check()
{
    return	bt_app_switch_exit_check();
}

#else

void app_bt_task()
{
}

u8 bt_app_exit_check()
{
    return 1;
}

int bt_background_event_handler_filter(struct sys_event *event)
{
    return 0;
}
#endif
