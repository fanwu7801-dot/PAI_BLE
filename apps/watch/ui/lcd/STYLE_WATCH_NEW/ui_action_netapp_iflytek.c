#include "app_config.h"
#include "media/includes.h"
#include "audio_config.h"
#include "ui/ui_api.h"
#include "ui/ui.h"
#include "ui/ui_style.h"
#include "app_task.h"
#include "system/timer.h"
#include "app_main.h"
#include "init.h"
#include "key_event_deal.h"
#include "btstack/avctp_user.h"
#include "ui/ui_number.h"
#include "ui/ui_text.h"
#include "sparkdesk_main.h"
#include "tts_main.h"
#include "vad_main.h"
#include "ifly_socket.h"
#include "cat1/cat1_common.h"

#if TCFG_IFLYTEK_ENABLE  //科大讯飞网络版

#define LOG_TAG_CONST       NET_IFLY
#define LOG_TAG             "[IFLY_UI]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
#define LOG_CLI_ENABLE
#include "debug.h"

#define STYLE_NAME JL

#define IFLY_TIME_MS				(200)	// 定时刷新
#define IFLY_TIME_HZ				(1000 / IFLY_TIME_MS)

#define IFLY_VAD_TIMEOUT_CNT_MAX	((20*1000) / IFLY_TIME_MS) // vad超时
#define IFLY_VAD_NO_CONN_CNT_MAX	(( 3*1000) / IFLY_TIME_MS) // vad没有网络超时
#define IFLY_VAD_NO_DATA_CNT_MAX	(( 3*1000) / IFLY_TIME_MS) // vad没有数据超时

#define IFLY_AI_TIMEOUT_CNT_MAX		((20*1000) / IFLY_TIME_MS) // ai超时
#define IFLY_AI_NO_CONN_CNT_MAX		(( 3*1000) / IFLY_TIME_MS) // ai没有网络超时
#define IFLY_AI_NO_DATA_CNT_MAX		(( 3*1000) / IFLY_TIME_MS) // ai没有数据超时

#define IFLY_TTS_NO_CONN_CNT_MAX	(( 3*1000) / IFLY_TIME_MS) // tts没有网络超时

// ui参数
struct ifly_ui_t {
    u16 time_id;		// 定时器
    u16 switch_pic_cnt; // 动图循环
    u16 to_cnt;			// 超时计数
    int cur_layout;    //当前布局
    int show_layout;    //需要显示的布局
};

// ifly功能参数
struct ifly_net_struct {
    ifly_vad_param 	vad_param;	// vad
    ifly_ai_param 	ai_param;	// ai
    ifly_tts_param 	tts_param;	// tts
    char local_text[MAX_VAD_LEN];		// 对话文本。近端
    char ai_text[MAX_SPARKDESK_LEN];	// 对话文本。远端
};

static struct ifly_net_struct *p_ifly_net = NULL;
static struct ifly_ui_t *ifly_ui = NULL;

extern int net_get_dhcp_flag();

static bool ifly_check_net_connect(void)
{
#if TCFG_APP_CAT1_EN
    if (cat1_check_call_enable() && cat1_get_lte_net_onoff()) {
        return true;
    }
#endif
    if (net_get_dhcp_flag()) {
        return true;
    }
    return false;
}

static int ui_ifly_ai_event_cb(ifly_ai_event_enum evt, void *param)
{
    switch (evt) {
    case IFLY_AI_EVT_NETWORK_FAIL:
        UI_MSG_POST("ai_network_fail");
        break;
    case IFLY_AI_EVT_RECV_OK:
        UI_MSG_POST("ai_recv_ok");
        break;
    case IFLY_AI_EVT_EXIT:
        break;
    default:
        break;
    }
    return 0;
}

static int ui_ifly_vad_event_cb(ifly_vad_event_enum evt, void *param)
{
    switch (evt) {
    case IFLY_VAD_EVT_AUDIO_START:
        UI_MSG_POST("vad_audio_start");
        break;
    case IFLY_VAD_EVT_RECV_OK:
        ifly_vad_stop(0, 2000);
        log_info("stop recording\n");
        if (strlen(p_ifly_net->local_text) == 0) {
            UI_MSG_POST("vad_no_content");
        } else {
            UI_MSG_POST("ai_think");
        }
        break;
    case IFLY_VAD_EVT_NETWORK_RECV_ERROR:
        UI_MSG_POST("vad_network_recv_error");
        break;
    case IFLY_VAD_EVT_NETWORK_FAIL:
        UI_MSG_POST("vad_network_fail");
        break;
    case IFLY_VAD_EVT_EXIT:
        break;
    default:
        break;
    }
    return 0;
}

static int ui_ifly_tts_event_cb(ifly_tts_event_enum evt, void *param)
{
    switch (evt) {
    case IFLY_TTS_EVT_PLAY_START:
        break;
    case IFLY_TTS_EVT_PLAY_STOP:
        UI_MSG_POST("tts_play_end");
        break;
    case IFLY_TTS_EVT_PLAY_FAIL_STOP:
        UI_MSG_POST("tts_network_fail");
        break;
    case IFLY_TTS_EVT_EXIT:
        break;
    default:
        break;
    }
    return 0;
}


static int ai_play_voice_ontouch(void *ctr, struct element_touch_event *e)
{
    switch (e->event) {
    case ELM_EVENT_TOUCH_DOWN:
        if (strlen(p_ifly_net->ai_text) == 0) {
            log_info("no ai content!!");
            return false;
        }
        if (!ifly_tts_is_work()) {
            log_info("tts start!\n");
            p_ifly_net->tts_param.event_cb = ui_ifly_tts_event_cb;
            p_ifly_net->tts_param.text_res = p_ifly_net->ai_text;
            ifly_tts_start(&p_ifly_net->tts_param);
        }
        break;
    default:
        break;
    }
    return false;
}
REGISTER_UI_EVENT_HANDLER(IFLY_BUTTON_TTS_PLAY)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = ai_play_voice_ontouch,
};


static void reflash_gettxt_handler(void)
{
    if (ifly_check_net_connect() == false) {
        UI_MSG_POST("tts_network_no_connect");
    }

    ui_auto_shut_down_re_run();

    ui_text_set_textu_by_id(IFLY_TEXT_TTS_TXT,
                            p_ifly_net->ai_text, strlen(p_ifly_net->ai_text),
                            FONT_DEFAULT | FONT_SHOW_MULTI_LINE | FONT_SHOW_SCROLL);
}


static int showtxt_onchange(void *ctr, enum element_change_event e, void *arg)
{
    switch (e) {
    case ON_CHANGE_INIT:
        if (ifly_ui->time_id) {
            sys_timer_del(ifly_ui->time_id);
            ifly_ui->time_id = 0;
        }
        ifly_ui->time_id = sys_timer_add(NULL, reflash_gettxt_handler, 1000);
        break;
    case ON_CHANGE_FIRST_SHOW:
        break;
    case ON_CHANGE_SHOW_POST:
        break;
    case ON_CHANGE_RELEASE:
        if (ifly_ui && ifly_ui->time_id) {
            sys_timer_del(ifly_ui->time_id);
            ifly_ui->time_id = 0;
        }
        if (p_ifly_net) {
            ifly_tts_stop(1, 10);
        }
        break;
    }
    return 0;
}

static int showtxt_ontouch(void *ctr, struct element_touch_event *e)
{
    switch (e->event) {
    case ELM_EVENT_TOUCH_R_MOVE:
        ui_hide(IFLY_LAYOUT_TTS);
        ui_show(IFLY_LAYOUT_VAD);
        return true;
    case ELM_EVENT_TOUCH_L_MOVE:
        break;
    default:
        break;
    }
    return false;
}

REGISTER_UI_EVENT_HANDLER(IFLY_LAYOUT_TTS)
.onchange = showtxt_onchange,
 .onkey = NULL,
  .ontouch = showtxt_ontouch,
};



static void ai_thinking_handler(void *priv)
{
    if (ifly_check_net_connect() == false) {
        UI_MSG_POST("ai_network_no_connect");
    }

    ui_auto_shut_down_re_run();

    // 动图处理
    ifly_ui->switch_pic_cnt++;
    if (ifly_ui->switch_pic_cnt > 8) {
        ifly_ui->switch_pic_cnt = 1;
    }
    ui_pic_show_image_by_id(IFLY_PIC_AI_THINKING, ifly_ui->switch_pic_cnt);

    // 超时处理
    if (ifly_ui->to_cnt) {
        ifly_ui->to_cnt--;
        if (ifly_ui->to_cnt == 0) {
            UI_MSG_POST("ai_no_content");
        }
    }
}


static int think_layout_onchange(void *ctr, enum element_change_event e, void *arg)
{
    struct element *elm = (struct element *)ctr;
    switch (e) {
    case ON_CHANGE_INIT:
        p_ifly_net->ai_param.ai_res = p_ifly_net->ai_text;
        p_ifly_net->ai_param.ai_res_len = MAX_SPARKDESK_LEN;
        p_ifly_net->ai_param.event_cb = ui_ifly_ai_event_cb;
        p_ifly_net->ai_param.content = p_ifly_net->local_text;
        p_ifly_net->ai_param.ai_res[0] = 0;
        ifly_sparkdesk_start(&p_ifly_net->ai_param);

        if (ifly_ui->time_id) {
            sys_timer_del(ifly_ui->time_id);
            ifly_ui->time_id = 0;
        }
        ifly_ui->switch_pic_cnt = 0;
        ifly_ui->to_cnt = IFLY_AI_TIMEOUT_CNT_MAX;
        ifly_ui->time_id = sys_timer_add(NULL, ai_thinking_handler, IFLY_TIME_MS);
        break;
    case ON_CHANGE_FIRST_SHOW:
        break;
    case ON_CHANGE_SHOW:
        break;
    case ON_CHANGE_SHOW_POST:
        break;
    case ON_CHANGE_RELEASE:
        if (ifly_ui && ifly_ui->time_id) {
            sys_timer_del(ifly_ui->time_id);
            ifly_ui->time_id = 0;
        }
        if (p_ifly_net) {
            ifly_sparkdesk_stop(1, 10);
        }
        break;
    }
    return 0;
}
REGISTER_UI_EVENT_HANDLER(IFLY_LAYOUT_AI)
.onchange = think_layout_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};


static int ifly_vad_text_onchange(void *ctr, enum element_change_event e, void *arg)
{
    struct ui_text *text = (struct ui_text *)ctr;
    switch (e) {
    case ON_CHANGE_INIT:
        if (ifly_vad_is_work()) {
            ui_text_set_index(text, 1);
        } else {
            ui_text_set_index(text, 0);
        }
        break;
    case ON_CHANGE_FIRST_SHOW:
        break;
    case ON_CHANGE_SHOW:
        break;
    case ON_CHANGE_RELEASE:
        break;
    }
    return 0;
}
REGISTER_UI_EVENT_HANDLER(IFLY_TEXT_VAD)
.onchange = ifly_vad_text_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

static int ifly_vad_button_ontouch(void *ctr, struct element_touch_event *e)
{
    switch (e->event) {
    case ELM_EVENT_TOUCH_DOWN:
        if (!ifly_vad_is_work()) {
            // 开始
            if (ifly_check_net_connect() == false) {
                UI_MSG_POST("vad_network_no_connect");
                break;
            }
            ui_hide(IFLY_LAYOUT_VAD);
            ui_show(IFLY_LAYOUT_CONNECTTING);

            ifly_vad_stop(1, 10);
            p_ifly_net->vad_param.vad_res = p_ifly_net->local_text;
            p_ifly_net->vad_param.vad_res_len = MAX_VAD_LEN;
            p_ifly_net->vad_param.event_cb = ui_ifly_vad_event_cb;
            p_ifly_net->vad_param.vad_res[0] = 0;
            ifly_vad_start(&p_ifly_net->vad_param);
        } else {
            // 停止
            ifly_vad_stop(0, 2000);
            log_info("stop recording\n");
            if (strlen(p_ifly_net->local_text) == 0) {
                UI_MSG_POST("vad_no_content");
            } else {
                UI_MSG_POST("ai_think");
            }
        }
        break;
    default:
        break;
    }
    return false;
}

REGISTER_UI_EVENT_HANDLER(IFLY_BUTTON_VAD)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = ifly_vad_button_ontouch,
};


static int ifly_vad_time_onchange(void *ctr, enum element_change_event e, void *arg)
{
    struct ui_number *number = (struct ui_text *)ctr;
    switch (e) {
    case ON_CHANGE_INIT:
        struct unumber num;
        num.type = TYPE_NUM;
        num.numbs = 1;
        num.number[0] = (IFLY_VAD_TIMEOUT_CNT_MAX * IFLY_TIME_MS) / 1000;
        ui_number_update(number, &num);
        break;
    case ON_CHANGE_FIRST_SHOW:
        break;
    case ON_CHANGE_SHOW:
        break;
    case ON_CHANGE_RELEASE:
        break;
    }
    return 0;
}
REGISTER_UI_EVENT_HANDLER(IFLY_TIME_VAD)
.onchange = ifly_vad_time_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};


static int ifly_vad_layout_onchange(void *ctr, enum element_change_event e, void *arg)
{
    struct element *elm = (struct element *)ctr;
    switch (e) {
    case ON_CHANGE_INIT:

        break;
    case ON_CHANGE_FIRST_SHOW:
        break;
    case ON_CHANGE_SHOW:
        break;
    case ON_CHANGE_RELEASE:
        if (ifly_ui && ifly_ui->time_id) {
            sys_timer_del(ifly_ui->time_id);
            ifly_ui->time_id = 0;
        }
        break;
    }
    return 0;
}

REGISTER_UI_EVENT_HANDLER(IFLY_LAYOUT_VAD)
.onchange = ifly_vad_layout_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};


static void ifly_no_content_time_deal(void *priv)
{
    if (ifly_ui->to_cnt) {
        ifly_ui->to_cnt--;
        if (ifly_ui->to_cnt == 0) {
            if (ifly_ui->time_id) {
                sys_timer_del(ifly_ui->time_id);
                ifly_ui->time_id = 0;
            }
            int layout = (int)priv;
            ui_hide(ifly_ui->show_layout);
            if (ifly_ui->cur_layout == IFLY_LAYOUT_TTS) {
                ui_show(IFLY_LAYOUT_TTS);
            } else {
                ui_show(IFLY_LAYOUT_VAD);
            }
        }
    }
}

static void ifly_no_content(u16 to_cnt_max)
{
    ui_hide(ifly_ui->cur_layout);
    ui_show(ifly_ui->show_layout);

    if (ifly_ui->time_id) {
        sys_timer_del(ifly_ui->time_id);
        ifly_ui->time_id = 0;
    }
    ifly_ui->to_cnt = to_cnt_max;
    ifly_ui->time_id = sys_timer_add(NULL, ifly_no_content_time_deal, IFLY_TIME_MS);
}


static void vad_rec_timer_deal(void *priv)
{
    if (ifly_check_net_connect() == false) {
        UI_MSG_POST("vad_network_no_connect");
    }

    // 动图处理
    ifly_ui->switch_pic_cnt++;
    if (ifly_ui->switch_pic_cnt > 10) {
        ifly_ui->switch_pic_cnt = 1;
    }
    ui_pic_show_image_by_id(IFLY_PIC_VAD, ifly_ui->switch_pic_cnt);

    // 超时处理
    if (ifly_ui->to_cnt) {
        ifly_ui->to_cnt--;

        if ((ifly_ui->to_cnt % IFLY_TIME_HZ) == 0) { // 按秒刷新
            struct unumber num;
            num.type = TYPE_NUM;
            num.numbs = 1;
            num.number[0] = (ifly_ui->to_cnt * IFLY_TIME_MS) / 1000;
            ui_number_update_by_id(IFLY_TIME_VAD, &num);
            ui_auto_shut_down_re_run();
        }

        if (ifly_ui->to_cnt == 0) {
            if (ifly_ui->time_id) {
                sys_timer_del(ifly_ui->time_id);
                ifly_ui->time_id = NULL;
            }
            ifly_vad_stop(1, 2000);
            if (strlen(p_ifly_net->local_text) == 0) {
                UI_MSG_POST("vad_no_content");
            } else {
                UI_MSG_POST("ai_think");
            }
        }
    }
}

static void vad_audio_start()
{
    ui_hide(IFLY_LAYOUT_CONNECTTING);
    ui_show(IFLY_LAYOUT_VAD);

    struct unumber num;
    num.type = TYPE_NUM;
    num.numbs = 1;
    num.number[0] = (IFLY_VAD_TIMEOUT_CNT_MAX * IFLY_TIME_MS) / 1000;
    ui_number_update_by_id(IFLY_TIME_VAD, &num);

    if (ifly_ui->time_id) {
        sys_timer_del(ifly_ui->time_id);
        ifly_ui->time_id = 0;
    }
    ifly_ui->switch_pic_cnt = 0;
    ifly_ui->to_cnt = IFLY_VAD_TIMEOUT_CNT_MAX;
    ifly_ui->time_id = sys_timer_add(NULL, vad_rec_timer_deal, IFLY_TIME_MS);
}

static void vad_no_content(void)
{
    ifly_ui->cur_layout = IFLY_LAYOUT_VAD;
    ifly_ui->show_layout = IFLY_LAYOUT_NO_CONTENT;
    ifly_no_content(IFLY_VAD_NO_DATA_CNT_MAX);
}

static void vad_network_fail(void)
{
    ifly_ui->cur_layout = IFLY_LAYOUT_CONNECTTING;
    ifly_ui->show_layout = IFLY_LAYOUT_CONNECT_FAIL;
    ifly_no_content(IFLY_VAD_NO_CONN_CNT_MAX);
}

static void vad_network_recv_error(void)
{
    ifly_ui->cur_layout = IFLY_LAYOUT_VAD;
    ifly_ui->show_layout = IFLY_LAYOUT_CONNECT_FAIL;
    ifly_no_content(IFLY_VAD_NO_CONN_CNT_MAX);
}

static void vad_network_no_connect(void)
{
    ifly_ui->cur_layout = IFLY_LAYOUT_CONNECTTING;
    ifly_ui->show_layout = IFLY_LAYOUT_NO_CONNECT;
    ifly_no_content(IFLY_VAD_NO_CONN_CNT_MAX);
}

static void ai_think()
{
    ui_hide(IFLY_LAYOUT_VAD);
    ui_show(IFLY_LAYOUT_AI);
}

static void ai_recv_ok(void)
{
    log_info("page_showtxt\n");
    if (ifly_ui->time_id) {
        sys_timer_del(ifly_ui->time_id);
        ifly_ui->time_id = 0;
    }
    ui_hide(IFLY_LAYOUT_AI);
    ui_show(IFLY_LAYOUT_TTS);
}

static void ai_no_content(void)
{
    ifly_ui->cur_layout = IFLY_LAYOUT_AI;
    ifly_ui->show_layout = IFLY_LAYOUT_NO_CONTENT;
    ifly_no_content(IFLY_AI_NO_DATA_CNT_MAX);
}

static void ai_network_fail(void)
{
    ifly_ui->cur_layout = IFLY_LAYOUT_AI;
    ifly_ui->show_layout = IFLY_LAYOUT_CONNECT_FAIL;
    ifly_no_content(IFLY_AI_NO_CONN_CNT_MAX);
}

static void ai_network_no_connect(void)
{
    ifly_ui->cur_layout = IFLY_LAYOUT_AI;
    ifly_ui->show_layout = IFLY_LAYOUT_NO_CONNECT;
    ifly_no_content(IFLY_AI_NO_CONN_CNT_MAX);
}

static void tts_play_end(void)
{
    ifly_tts_stop(0, 3000);
}

static void tts_network_fail(void)
{
    ifly_ui->cur_layout = IFLY_LAYOUT_TTS;
    ifly_ui->show_layout = IFLY_LAYOUT_CONNECT_FAIL;
    ifly_no_content(IFLY_TTS_NO_CONN_CNT_MAX);
}

static void tts_network_no_connect(void)
{
    ifly_ui->cur_layout = IFLY_LAYOUT_TTS;
    ifly_ui->show_layout = IFLY_LAYOUT_NO_CONNECT;
    ifly_no_content(IFLY_TTS_NO_CONN_CNT_MAX);
}

static const struct uimsg_handl ui_msg_cmd_handler[] = {
    {"vad_audio_start", 		vad_audio_start},		// vad联网成功，录音开始
    {"vad_no_content", 			vad_no_content},		// vad没有收到数据
    {"vad_network_fail", 		vad_network_fail},		// vad网络错误
    {"vad_network_recv_error", 		vad_network_recv_error},		// vad网络接收错误
    {"vad_network_no_connect",	vad_network_no_connect},// vad没有网络

    {"ai_think", 				ai_think},				// ai思考
    {"ai_recv_ok", 				ai_recv_ok},			// ai接受完成
    {"ai_no_content", 			ai_no_content},			// ai没有收到数据
    {"ai_network_fail", 		ai_network_fail},		// ai网络错误
    {"ai_network_no_connect",	ai_network_no_connect},	// ai没有网络

    {"tts_play_end", 			tts_play_end},			// tts播放完毕
    {"tts_network_fail", 		tts_network_fail},		// ts网络错误
    {"tts_network_no_connect",	tts_network_no_connect},// tts没有网络

    {NULL, NULL},
};

static int ifly_ui_switch_init(int id)
{
    if (ifly_check_net_connect() == false) {
        ui_show(IFLY_LAYOUT_NO_CONNECT);
    } else {
        ui_show(IFLY_LAYOUT_VAD);
    }
    return 0;
}

static int ifly_window_onchange(void *ctr, enum element_change_event e, void *arg)
{
    switch (e) {
    case ON_CHANGE_INIT:
        if (p_ifly_net == NULL) {
            p_ifly_net = zalloc(sizeof(struct ifly_net_struct));
            ASSERT(p_ifly_net);
        }
        if (ifly_ui == NULL) {
            ifly_ui = zalloc(sizeof(struct ifly_ui_t));
            ASSERT(ifly_ui);
        }
        ifly_socket_kill_task_create();

        ui_set_call(ifly_ui_switch_init, 0);
        ui_register_msg_handler(ID_WINDOW_NET_IFLY, ui_msg_cmd_handler);//注册消息交互的回调
        break;
    case ON_CHANGE_FIRST_SHOW:
        break;
    case ON_CHANGE_RELEASE:
        ui_register_msg_handler(ID_WINDOW_NET_IFLY, NULL);//注册消息交互的回调

        ifly_vad_stop(1, (u32) - 1);
        ifly_sparkdesk_stop(1, (u32) - 1);
        ifly_tts_stop(1, (u32) - 1);

        ifly_socket_kill_task_release();

        if (ifly_ui) {
            if (ifly_ui->time_id) {
                sys_timer_del(ifly_ui->time_id);
                ifly_ui->time_id = 0;
            }
            free(ifly_ui);
            ifly_ui = NULL;
        }
        if (p_ifly_net) {
            free(p_ifly_net);
            p_ifly_net = NULL;
        }
        break;
    }
    return 0;
}
REGISTER_UI_EVENT_HANDLER(ID_WINDOW_NET_IFLY)
.onchange = ifly_window_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};





#endif

