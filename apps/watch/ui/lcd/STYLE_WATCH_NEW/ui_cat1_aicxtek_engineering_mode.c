#include "app_config.h"
#include "ui/ui_style.h"
#include "ui/ui.h"
#include "ui/ui_api.h"
#include "app_task.h"
//#include "system/timer.h"
#include "device/device.h"
#include "key_event_deal.h"
#include "res/resfile.h"
#include "ui/res_config.h"
#include "ui/ui_resource.h"
//#include "system/includes.h"
#include "audio_config.h"
#include "asm/mcpwm.h"
#include "ui/ui_sys_param.h"
#include "ui/watch_syscfg_manage.h"
#include "font/language_list.h"
#include "bt_common.h"
#include "btstack/btstack_task.h"
#include "btstack/avctp_user.h"
#include "custom_cfg.h"
#include "product_info_api.h"
#include "cat1/cat1_common.h"
#include "aic_srv_tele_radio.h"
#include "aic_srv_tele_sim.h"
#include "aic_srv_tele_sms.h"
#include "aic_srv_tele_data.h"
#include "aic_srv_tele_call.h"
#include "aic_srv_voice.h"
#include "aic_ctrl.h"
#include "http/http_cli.h"
#include "aic_net.h"
#include "aic_srv_version.h"

/* AIC_MODIFY start */
/* TODO: del this after resolve compile error problem */
#define lwip_read         read
#undef lwip_close
#define lwip_close        close
/* AIC_MODIFY end */

/* AIC_TEST: used for call voice volume set test */
static bool g_is_from_engineering_mode = false;
bool engineering_poweromoff_can_touch = true;

#if TCFG_CAT1_AICXTEK_ENABLE

#define STYLE_NAME  JL

#define ui_grid_for_id(id) \
	({ \
		struct element *elm = ui_core_get_element_by_id(id); \
	 	elm ? container_of(elm, struct ui_grid, elm): NULL; \
	 })

extern u16 sys_timer_add(void *priv, void (*func)(void *priv), u32 msec);
extern void sys_timer_del(u16 t);

int is_sms_layout = 0;
static void show_sms_center_number_handler()
{
    ui_text_set_text_by_id(CAT1_TEXT_SMS_CENTER_NUMBER, cat1_get_smsc(), strlen(cat1_get_smsc()), FONT_DEFAULT | FONT_SHOW_SCROLL);
}

static void network_search_handler()
{

    struct element *p;
    struct element *elm = ui_core_get_element_by_id(CAT1_LAYER_AICXTEK);
    int id;
    list_for_each_child_element(p, (struct element *)elm) {
        if (p->id == CAT1_LAYOUT_FIRST_MENU) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_FIRST_MENU;
            }
        }
        if (p->id == CAT1_LAYOUT_DEVICE_INFORMATION) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_DEVICE_INFORMATION;
            }
        }
        if (p->id == CAT1_LAYOUT_TELE) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_TELE;
            }
        }
        if (p->id == CAT1_LAYOUT_POWER_CTRL) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_POWER_CTRL;
            }
        }
        if (p->id == CAT1_LAYOUT_WIFI_SCAN) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_WIFI_SCAN;
            }
        }
        if (p->id == CAT1_LAYOUT_SOFTWARE_VERSION) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_SOFTWARE_VERSION;
            }
        }
        if (p->id == CAT1_LAYOUT_SN_NUMBER) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_SN_NUMBER;
            }
        }
        if (p->id == CAT1_LAYOUT_IMEI) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_IMEI;
            }
        }
        if (p->id == CAT1_LAYOUT_CALIBRATION) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_CALIBRATION;
            }
        }
        if (p->id == CAT1_LAYOUT_SIM_INFORMATION) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_SIM_INFORMATION;
            }
        }
        if (p->id == CAT1_LAYOUT_SIGNAL_INFO) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_SIGNAL_INFO;
            }
        }
        if (p->id == CAT1_LAYOUT_CELL_INFO) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_CELL_INFO;
            }
        }
        if (p->id == CAT1_LAYOUT_REG_STATUS) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_REG_STATUS;
            }
        }
        if (p->id == CAT1_LAYOUT_NETWORK_RATE) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_NETWORK_RATE;
            }
        }
        if (p->id == CAT1_LAYOUT_CALL_SETTING) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_CALL_SETTING;
            }
        }
        if (p->id == CAT1_LAYOUT_SMS_SETTING) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_SMS_SETTING;
            }
        }
        if (p->id == CAT1_LAYOUT_WIFI_SCAN_INFO) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_WIFI_SCAN_INFO;
            }
        }
        if (p->id == CAT1_LAYOUT_SINGLE_CHANNEL_SEARCH) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_SINGLE_CHANNEL_SEARCH;
            }
        }
        if (p->id == CAT1_LAYOUT_THIS_CELL) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_THIS_CELL;
            }
        }
        if (p->id == CAT1_LAYOUT_NEIGHBOR) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_NEIGHBOR;
            }
        }
        if (p->id == CAT1_LAYOUT_DIAL_NUMBER) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_DIAL_NUMBER;
            }
        }
        if (p->id == CAT1_LAYOUT_SMS_SHOW) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_SMS_SHOW;
            }
        }
        if (p->id == CAT1_LAYOUT_LOG) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_LOG;
            }
        }
    }

    ui_hide(id);
    ui_show(CAT1_LAYOUT_NETWORK_SEARCH);
}

static void cell_info_show_handler()
{

    struct element *p;
    struct element *elm = ui_core_get_element_by_id(CAT1_LAYER_AICXTEK);
    int id;
    list_for_each_child_element(p, (struct element *)elm) {
        if (p->id == CAT1_LAYOUT_FIRST_MENU) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_FIRST_MENU;
            }
        }
        if (p->id == CAT1_LAYOUT_DEVICE_INFORMATION) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_DEVICE_INFORMATION;
            }
        }
        if (p->id == CAT1_LAYOUT_TELE) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_TELE;
            }
        }
        if (p->id == CAT1_LAYOUT_POWER_CTRL) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_POWER_CTRL;
            }
        }
        if (p->id == CAT1_LAYOUT_WIFI_SCAN) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_WIFI_SCAN;
            }
        }
        if (p->id == CAT1_LAYOUT_SOFTWARE_VERSION) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_SOFTWARE_VERSION;
            }
        }
        if (p->id == CAT1_LAYOUT_SN_NUMBER) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_SN_NUMBER;
            }
        }
        if (p->id == CAT1_LAYOUT_IMEI) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_IMEI;
            }
        }
        if (p->id == CAT1_LAYOUT_CALIBRATION) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_CALIBRATION;
            }
        }
        if (p->id == CAT1_LAYOUT_SIM_INFORMATION) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_SIM_INFORMATION;
            }
        }
        if (p->id == CAT1_LAYOUT_SIGNAL_INFO) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_SIGNAL_INFO;
            }
        }
        if (p->id == CAT1_LAYOUT_CELL_INFO) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_CELL_INFO;
            }
        }
        if (p->id == CAT1_LAYOUT_REG_STATUS) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_REG_STATUS;
            }
        }
        if (p->id == CAT1_LAYOUT_NETWORK_RATE) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_NETWORK_RATE;
            }
        }
        if (p->id == CAT1_LAYOUT_CALL_SETTING) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_CALL_SETTING;
            }
        }
        if (p->id == CAT1_LAYOUT_SMS_SETTING) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_SMS_SETTING;
            }
        }
        if (p->id == CAT1_LAYOUT_WIFI_SCAN_INFO) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_WIFI_SCAN_INFO;
            }
        }
        if (p->id == CAT1_LAYOUT_SINGLE_CHANNEL_SEARCH) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_SINGLE_CHANNEL_SEARCH;
            }
        }
        if (p->id == CAT1_LAYOUT_NETWORK_SEARCH) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_NETWORK_SEARCH;
            }
        }
        if (p->id == CAT1_LAYOUT_NEIGHBOR) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_NEIGHBOR;
            }
        }
        if (p->id == CAT1_LAYOUT_DIAL_NUMBER) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_DIAL_NUMBER;
            }
        }
        if (p->id == CAT1_LAYOUT_SMS_SHOW) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_SMS_SHOW;
            }
        }
        if (p->id == CAT1_LAYOUT_LOG) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_LOG;
            }
        }
    }
    ui_hide(id);
    ui_show(CAT1_LAYOUT_THIS_CELL);
}

static void neigh_cell_info_show_handler()
{
    struct element *p;
    struct element *elm = ui_core_get_element_by_id(CAT1_LAYER_AICXTEK);
    int id;
    list_for_each_child_element(p, (struct element *)elm) {
        if (p->id == CAT1_LAYOUT_FIRST_MENU) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_FIRST_MENU;
            }
        }
        if (p->id == CAT1_LAYOUT_DEVICE_INFORMATION) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_DEVICE_INFORMATION;
            }
        }
        if (p->id == CAT1_LAYOUT_TELE) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_TELE;
            }
        }
        if (p->id == CAT1_LAYOUT_POWER_CTRL) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_POWER_CTRL;
            }
        }
        if (p->id == CAT1_LAYOUT_WIFI_SCAN) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_WIFI_SCAN;
            }
        }
        if (p->id == CAT1_LAYOUT_SOFTWARE_VERSION) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_SOFTWARE_VERSION;
            }
        }
        if (p->id == CAT1_LAYOUT_SN_NUMBER) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_SN_NUMBER;
            }
        }
        if (p->id == CAT1_LAYOUT_IMEI) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_IMEI;
            }
        }
        if (p->id == CAT1_LAYOUT_CALIBRATION) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_CALIBRATION;
            }
        }
        if (p->id == CAT1_LAYOUT_SIM_INFORMATION) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_SIM_INFORMATION;
            }
        }
        if (p->id == CAT1_LAYOUT_SIGNAL_INFO) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_SIGNAL_INFO;
            }
        }
        if (p->id == CAT1_LAYOUT_CELL_INFO) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_CELL_INFO;
            }
        }
        if (p->id == CAT1_LAYOUT_REG_STATUS) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_REG_STATUS;
            }
        }
        if (p->id == CAT1_LAYOUT_NETWORK_RATE) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_NETWORK_RATE;
            }
        }
        if (p->id == CAT1_LAYOUT_CALL_SETTING) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_CALL_SETTING;
            }
        }
        if (p->id == CAT1_LAYOUT_SMS_SETTING) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_SMS_SETTING;
            }
        }
        if (p->id == CAT1_LAYOUT_WIFI_SCAN_INFO) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_WIFI_SCAN_INFO;
            }
        }
        if (p->id == CAT1_LAYOUT_SINGLE_CHANNEL_SEARCH) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_SINGLE_CHANNEL_SEARCH;
            }
        }
        if (p->id == CAT1_LAYOUT_THIS_CELL) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_THIS_CELL;
            }
        }
        if (p->id == CAT1_LAYOUT_NETWORK_SEARCH) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_NETWORK_SEARCH;
            }
        }
        if (p->id == CAT1_LAYOUT_DIAL_NUMBER) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_DIAL_NUMBER;
            }
        }
        if (p->id == CAT1_LAYOUT_SMS_SHOW) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_SMS_SHOW;
            }
        }
        if (p->id == CAT1_LAYOUT_LOG) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_LOG;
            }
        }
    }
    ui_hide(id);
    ui_show(CAT1_LAYOUT_NEIGHBOR);
}

static void power_onoff_show_handler()
{
    if (cat1_ctrl_is_poweron() == true) {
        ui_pic_show_image_by_id(CAT1_PIC_POWERON_SEL, 1);
    } else if (cat1_ctrl_is_poweron() == false) {
        ui_pic_show_image_by_id(CAT1_PIC_POWERON_SEL, 0);
    }
}

static void sms_show_handler()
{
    if (is_sms_layout == 1) {
        ui_hide(CAT1_LAYOUT_SMS_SETTING);
        ui_show(CAT1_LAYOUT_SMS_SHOW);
    }
}

static void network_rate_show_handler()
{
    struct element *p;
    struct element *elm = ui_core_get_element_by_id(CAT1_LAYER_AICXTEK);
    int id;
    list_for_each_child_element(p, (struct element *)elm) {
        if (p->id == CAT1_LAYOUT_FIRST_MENU) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_FIRST_MENU;
            }
        }
        if (p->id == CAT1_LAYOUT_DEVICE_INFORMATION) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_DEVICE_INFORMATION;
            }
        }
        if (p->id == CAT1_LAYOUT_TELE) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_TELE;
            }
        }
        if (p->id == CAT1_LAYOUT_POWER_CTRL) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_POWER_CTRL;
            }
        }
        if (p->id == CAT1_LAYOUT_WIFI_SCAN) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_WIFI_SCAN;
            }
        }
        if (p->id == CAT1_LAYOUT_SOFTWARE_VERSION) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_SOFTWARE_VERSION;
            }
        }
        if (p->id == CAT1_LAYOUT_SN_NUMBER) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_SN_NUMBER;
            }
        }
        if (p->id == CAT1_LAYOUT_IMEI) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_IMEI;
            }
        }
        if (p->id == CAT1_LAYOUT_CALIBRATION) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_CALIBRATION;
            }
        }
        if (p->id == CAT1_LAYOUT_SIM_INFORMATION) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_SIM_INFORMATION;
            }
        }
        if (p->id == CAT1_LAYOUT_SIGNAL_INFO) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_SIGNAL_INFO;
            }
        }
        if (p->id == CAT1_LAYOUT_CELL_INFO) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_CELL_INFO;
            }
        }
        if (p->id == CAT1_LAYOUT_REG_STATUS) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_REG_STATUS;
            }
        }
        if (p->id == CAT1_LAYOUT_NETWORK_SEARCH) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_NETWORK_SEARCH;
            }
        }
        if (p->id == CAT1_LAYOUT_CALL_SETTING) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_CALL_SETTING;
            }
        }
        if (p->id == CAT1_LAYOUT_SMS_SETTING) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_SMS_SETTING;
            }
        }
        if (p->id == CAT1_LAYOUT_WIFI_SCAN_INFO) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_WIFI_SCAN_INFO;
            }
        }
        if (p->id == CAT1_LAYOUT_SINGLE_CHANNEL_SEARCH) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_SINGLE_CHANNEL_SEARCH;
            }
        }
        if (p->id == CAT1_LAYOUT_THIS_CELL) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_THIS_CELL;
            }
        }
        if (p->id == CAT1_LAYOUT_NEIGHBOR) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_NEIGHBOR;
            }
        }
        if (p->id == CAT1_LAYOUT_DIAL_NUMBER) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_DIAL_NUMBER;
            }
        }
        if (p->id == CAT1_LAYOUT_SMS_SHOW) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_SMS_SHOW;
            }
        }
        if (p->id == CAT1_LAYOUT_LOG) {
            if (p->css.invisible == 0) {
                id = CAT1_LAYOUT_LOG;
            }
        }
    }
    ui_hide(id);
    ui_show(CAT1_LAYOUT_NETWORK_RATE);
}


static const struct uimsg_handl ui_msg_show_handler[] = {              //注册消息池
    { "show_sms_center_number",        show_sms_center_number_handler     },
    { "network_search",        network_search_handler     },
    { "cell_info_show",        cell_info_show_handler     },
    { "neigh_cell_info_show",        neigh_cell_info_show_handler     },
    { "power_onoff_show",        power_onoff_show_handler     },
    { "sms_show",        sms_show_handler     },
    { "network_rate_show",        network_rate_show_handler     },
    { NULL, NULL},      /* 必须以此结尾！ */
};

#if TCFG_CAT1_FUNC_NET_ENABLE
static int is_network_rate_run = 0;
//下行速率测试
#include "http/http_cli.h"
#define DOWNLOAD_BUF_SIZE   (512)
struct network_byte {
    u32 ul_bytes;
    u32 ul_packets;
    u32 dl_bytes;
    u32 dl_packets;
};

struct network_rate {
    double dl_rate;
    double ul_rate;
};
struct network_rate network_rate_res;

struct aic_network_stats network_rate_before_t = {0};
struct aic_network_stats network_rate_after_t = {0};
struct network_rate network_rate_t = {0};

static int __httpcli_cb(void *ctx, void *buf, unsigned int size, void *priv, httpin_status status)
{
    return 0;
}

static s32 http_download(const char *url)
{
    /* extern struct aic_network_stats net_stats; */
    /* memset(&net_stats, 0, sizeof(struct aic_network_stats)); */
    /* aic_get_network_stats(&network_rate_before_t); */

    s32 ret = 0;
    u8 *buf = NULL;
    u32 total_len = 0, offset = 0, remain = 0;
    const struct net_download_ops *ops = &http_ops;

    buf = (u8 *)malloc(DOWNLOAD_BUF_SIZE);
    if (!buf) {
        printf("buf malloc err\n");
        goto _download_exit_;
    }

    httpcli_ctx *ctx = (httpcli_ctx *)calloc(1, sizeof(httpcli_ctx));
    if (NULL == ctx) {
        printf("calloc err\n");
        goto _download_exit_;
    }

    ctx->url = url;
    ctx->connection = "close";
    ctx->timeout_millsec = 10000;
    ctx->cb = __httpcli_cb;

    printf("URL : %s\n", url);

    if (ops->init(ctx) != HERROR_OK) {
        printf("http init err\n");
        goto _download_exit_;
    }

    total_len = ctx->content_length;
    printf("total_len = %d\n", total_len);

    /* int time_before_dl = jiffies_msec(); */
    /* printf("time_before_dl:%d\n", time_before_dl); */

    while (total_len > 0) {
        if (total_len >= DOWNLOAD_BUF_SIZE) {
            remain = DOWNLOAD_BUF_SIZE;
            total_len -= DOWNLOAD_BUF_SIZE;
        } else {
            remain = total_len;
            total_len = 0;
        }

        do {
            ret = ops->read(ctx, buf + offset, remain - offset);
            if (ret < 0) {
                printf("http download err\n");
                goto _download_exit_;
            }
            offset += ret;
        } while (remain != offset);
        offset = 0;
        /* log_info("total_len = %d\n", total_len); */
        putchar('.');
    }

    /* int time_after_dl = jiffies_msec(); */
    /* printf("time_after_dl:%d\n", time_after_dl); */

    ops->close(ctx);
    printf("%s, succ\n", __FUNCTION__);


_download_exit_:
    if (buf) {
        free(buf);
    }

    if (ctx) {
        free(ctx);
    }
    return 0;
}

static void network_rate_task(void *priv)
{
    extern struct aic_network_stats net_stats;
    memset(&net_stats, 0, sizeof(struct aic_network_stats));
    aic_get_network_stats(&network_rate_before_t);

    int time_before_dl = jiffies_msec();
    printf("time_before_dl:%d\n", time_before_dl);

    //http_download("https://profile.jieliapp.com/license/v1/fileupdate/download/92bb2c13-4360-4cc4-b7c2-75a57c82e8a9");
    http_download("https://profile.jieliapp.com/license/v1/fileupdate/download/d07f3202-4da0-4c96-8ba3-bb80960097dd");

    int time_after_dl = jiffies_msec();
    printf("time_after_dl:%d\n", time_after_dl);
    aic_get_network_stats(&network_rate_after_t);

    double interval_time = time_after_dl - time_before_dl;
    printf("interval time:%d\n", interval_time);
    printf("ul_bytes_before:%d, ul_packets_before:%d, dl_bytes_before:%d, dl_packets_before:%d\n", network_rate_before_t.ul_bytes,
           network_rate_before_t.ul_packets, network_rate_before_t.dl_bytes,
           network_rate_before_t.dl_packets);

    printf("ul_bytes_after:%d, ul_packets_after:%d, dl_bytes_after:%d, dl_packets_after:%d\n", network_rate_after_t.ul_bytes,
           network_rate_after_t.ul_packets, network_rate_after_t.dl_bytes,
           network_rate_after_t.dl_packets);

    double rate_dl_bytes = ((network_rate_after_t.dl_bytes - network_rate_before_t.dl_bytes) / interval_time) * 1000;
    double rate_ul_bytes = ((network_rate_after_t.ul_bytes - network_rate_before_t.ul_bytes) / interval_time) * 1000;

    network_rate_t.dl_rate = rate_dl_bytes / 1024;

    void tcp_client_main(void *priv);
    tcp_client_main(NULL);

    for (;;) {
        os_time_dly(100 * 60 * 60);
    }
}

void engineering_mode_network_rate_test(void)
{
    os_task_create(network_rate_task, NULL, 6, 0x600, 0, "network_rate_task");
}

//上行速率测试
#include "sock_api/sock_api.h"
#include "app_config.h"
#include "lwip.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"

#define SERVER_PORT 3300
#define SERVER_ADDR "120.78.8.157"
#define BUF_SIZE 4*1024
#define LOG_TAG "[TCP Client]"
static char send_data[BUF_SIZE];
static int ul_timer = 0;
static int ul_del = 0;

void tcp_client(void)
{
    void *sockfd;
    int bytes;
    struct sockaddr_in server_addr;

    if ((sockfd = sock_reg(AF_INET, SOCK_STREAM, 0, NULL, NULL)) == NULL) {
        printf(LOG_TAG"malloc err\n");
        goto __loop;
    }

    memset(&server_addr, 0, sizeof(struct sockaddr_in));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(SERVER_ADDR);
    server_addr.sin_port = htons(SERVER_PORT);

    if (0 != sock_connect(sockfd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_in))) {
        printf(LOG_TAG"sock_connect err\n");
        goto __loop;
    }

    memset(send_data, 0xee, BUF_SIZE);

    int count = 0;
    while (1) {
        count++;
        if (count == 100) {
            sock_unreg(sockfd);
            break;
        }
        bytes = sock_send(sockfd, send_data, BUF_SIZE, 0);
        if (bytes <= 0 || ul_del == 1) {
            sock_unreg(sockfd);
            break;
        }
    }

__loop:
    for (;;) {
        os_time_dly(10000);
    }
}

void count_ul_rate(void)
{
    aic_get_network_stats(&network_rate_after_t);
    int ul_data = network_rate_after_t.ul_bytes - network_rate_before_t.ul_bytes;
    printf("ul total:%d\n", network_rate_after_t.ul_bytes);
    printf("ul:%d\n", ul_data);

    network_rate_t.ul_rate = ul_data / 1024;

    memset(&network_rate_before_t, 0, sizeof(network_rate_before_t));
    memcpy(&network_rate_before_t, &network_rate_after_t, sizeof(network_rate_after_t));

    ul_del = 1;
    UI_MSG_POST("network_rate_show");
    if (ul_timer) {
        sys_timer_del(ul_timer);
        ul_timer = 0;
    }
}

void tcp_client_main(void *priv)
{
    ul_del = 0;
    os_task_create(tcp_client, NULL, 30, 1024, 0, "tcp_client");
    ul_timer = sys_timer_add(NULL, count_ul_rate, 1000);
}



#endif
//----------------------一级菜单-------------------------//
static int engineering_mode_page_onchange(void *ctr, enum element_change_event e, void *arg)
{
    struct window *window = (struct window *)ctr;
    switch (e) {
    case ON_CHANGE_INIT:
        printf("\n***engineering_mode_page_onchange***\n");
        cat1_set_engineering_mode(1);
        printf("engineering mode enter!\n");
        ui_register_msg_handler(PAGE_107, ui_msg_show_handler);//注册消息交互的回调
        break;
    case ON_CHANGE_RELEASE:
#if TCFG_CAT1_FUNC_NET_ENABLE
        is_network_rate_run = 0;
        cat1_set_engineering_mode(0);
        printf("engineering mode exit!\n");
#endif
        break;
    default:
        return false;
    }
    return false;
}
REGISTER_UI_EVENT_HANDLER(PAGE_107)//一级菜单
.onchange = engineering_mode_page_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

static u8 first_menu_item_memory = 0;
static void first_menu_item_get(struct ui_grid *grid)
{
    first_menu_item_memory = ui_grid_get_hindex(grid);
    if (first_menu_item_memory >= grid->avail_item_num) {
        first_menu_item_memory = 0;
    }
}
static void first_menu_item_set(struct ui_grid *grid, int item)
{
    ui_grid_set_hi_index(grid, item);
}

static int first_menu_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct ui_grid *grid = (struct ui_grid *)_ctrl;

    switch (event) {
    case ON_CHANGE_INIT:
        printf("first_menu!\n");
        ui_grid_set_slide_direction(grid, SCROLL_DIRECTION_UD);
        first_menu_item_set(grid, first_menu_item_memory);
        //set_ui_sys_param(ConnNewPhone, 0);
        break;
    case ON_CHANGE_RELEASE:
        first_menu_item_get(grid);
        break;
    }
    return 0;
}

static int first_menu_ontouch(void *ctr, struct element_touch_event *e)
{
    struct ui_grid *grid = (struct ui_grid *)ctr;
    static u8 touch_action = 0;
    int sel_item;

    switch (e->event) {
    case ELM_EVENT_TOUCH_UP:
        if (touch_action != 1) {
            break;
        }

        sel_item = ui_grid_cur_item(grid);
        printf("sel_item:%d\n", sel_item);
        if (sel_item <= 0) {
            break;
        }
        switch (sel_item) {
        case 1://设备信息
            ui_hide(CAT1_LAYOUT_FIRST_MENU);
            ui_show(CAT1_LAYOUT_DEVICE_INFORMATION);
            break;
        case 2://tele
            ui_hide(CAT1_LAYOUT_FIRST_MENU);
            ui_show(CAT1_LAYOUT_TELE);
            break;
        case 3://电源控制
            ui_hide(CAT1_LAYOUT_FIRST_MENU);
            ui_show(CAT1_LAYOUT_POWER_CTRL);
            break;
        case 4://wifi_scan
            ui_hide(CAT1_LAYOUT_FIRST_MENU);
            ui_show(CAT1_LAYOUT_WIFI_SCAN);
            break;
        case 5://log
            ui_hide(CAT1_LAYOUT_FIRST_MENU);
            ui_show(CAT1_LAYOUT_LOG);
            break;
        }
        break;
    case ELM_EVENT_TOUCH_HOLD:
        break;
    case ELM_EVENT_TOUCH_MOVE:
        touch_action = 2;
        break;
    case ELM_EVENT_TOUCH_DOWN:
        touch_action = 1;
        break;
    case ELM_EVENT_TOUCH_U_MOVE:
        break;
    case ELM_EVENT_TOUCH_D_MOVE:
        break;
    }
    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_VLIST_FIRST_MENU)//一级菜单
.onchange = first_menu_onchange,
 .onkey = NULL,
  .ontouch = first_menu_ontouch,
};

//--------------------------设备信息-----------------------------//
static int layout_device_information_ontouch(void *ctr, struct element_touch_event *e)
{
    struct layout *layout = (struct layout *)ctr;

    switch (e->event) {
    case ELM_EVENT_TOUCH_R_MOVE:
        ui_hide(CAT1_LAYOUT_DEVICE_INFORMATION);
        ui_show(CAT1_LAYOUT_FIRST_MENU);
        return true;
        break;
    default:
        break;
    }

    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_LAYOUT_DEVICE_INFORMATION)//一级菜单
.onchange = NULL,
 .onkey = NULL,
  .ontouch = layout_device_information_ontouch,
};

static int vlist_device_information_ontouch(void *ctr, struct element_touch_event *e)
{
    struct ui_grid *grid = (struct ui_grid *)ctr;
    static u8 touch_action = 0;
    int sel_item;

    switch (e->event) {
    case ELM_EVENT_TOUCH_UP:
        if (touch_action != 1) {
            break;
        }

        sel_item = ui_grid_cur_item(grid);
        printf("sel_item:%d\n", sel_item);
        if (sel_item <= 0) {
            break;
        }
        switch (sel_item) {
        case 1://软件版本号
            ui_hide(CAT1_LAYOUT_DEVICE_INFORMATION);
            ui_show(CAT1_LAYOUT_SOFTWARE_VERSION);
            break;
        case 2://SN
            ui_hide(CAT1_LAYOUT_DEVICE_INFORMATION);
            ui_show(CAT1_LAYOUT_SN_NUMBER);
            break;
        case 3://IMEI
            ui_hide(CAT1_LAYOUT_DEVICE_INFORMATION);
            ui_show(CAT1_LAYOUT_IMEI);
            break;
        case 4://校准
            ui_hide(CAT1_LAYOUT_DEVICE_INFORMATION);
            ui_show(CAT1_LAYOUT_CALIBRATION);
            break;
        }
        break;
    case ELM_EVENT_TOUCH_HOLD:
        break;
    case ELM_EVENT_TOUCH_MOVE:
        touch_action = 2;
        break;
    case ELM_EVENT_TOUCH_DOWN:
        touch_action = 1;
        break;
    case ELM_EVENT_TOUCH_U_MOVE:
        break;
    case ELM_EVENT_TOUCH_D_MOVE:
        break;
    }
    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_VLIST_DEVICE_INFORMATION)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = vlist_device_information_ontouch,
};

//----------------------设备信息-软件版本号-------------------------//
struct dev_info {
    char sw_version[AIC_SRV_VERSION_TOTAL_LEN];
    char sn_device_serial_number[10];
    char imei_number[18];
};

struct dev_info s_dev_info;

static int layout_software_version_ontouch(void *ctr, struct element_touch_event *e)
{
    struct layout *layout = (struct layout *)ctr;

    switch (e->event) {
    case ELM_EVENT_TOUCH_R_MOVE:
        ui_hide(CAT1_LAYOUT_SOFTWARE_VERSION);
        ui_show(CAT1_LAYOUT_DEVICE_INFORMATION);
        return true;
        break;
    default:
        break;
    }

    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_LAYOUT_SOFTWARE_VERSION)//一级菜单
.onchange = NULL,
 .onkey = NULL,
  .ontouch = layout_software_version_ontouch,
};

static int text_software_version_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct ui_text *text = (struct ui_text *)_ctrl;

    switch (event) {
    case ON_CHANGE_INIT:
        switch (text->elm.id) {
        case CAT1_TEXT_SOFTWARE_VERSION:
            aic_get_version(s_dev_info.sw_version, AIC_SRV_VERSION_TOTAL_LEN);
            ui_text_set_text_attrs(text, s_dev_info.sw_version, strlen(s_dev_info.sw_version), FONT_ENCODE_UTF8, 0, FONT_DEFAULT | FONT_SHOW_MULTI_LINE);
            break;
        }
        break;
    case ON_CHANGE_RELEASE:
        break;
    }
    return 0;
}
REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_SOFTWARE_VERSION)
.onchange = text_software_version_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

//--------------------------设备信息-SN设备序列号-----------------------//
static int layout_sn_number_ontouch(void *ctr, struct element_touch_event *e)
{
    struct layout *layout = (struct layout *)ctr;

    switch (e->event) {
    case ELM_EVENT_TOUCH_R_MOVE:
        ui_hide(CAT1_LAYOUT_SN_NUMBER);
        ui_show(CAT1_LAYOUT_DEVICE_INFORMATION);
        return true;
        break;
    default:
        break;
    }

    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_LAYOUT_SN_NUMBER)//一级菜单
.onchange = NULL,
 .onkey = NULL,
  .ontouch = layout_sn_number_ontouch,
};

static int text_sn_number_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct ui_text *text = (struct ui_text *)_ctrl;

    switch (event) {
    case ON_CHANGE_INIT:
        switch (text->elm.id) {
        case CAT1_TEXT_SN_NUMBER:
            //aic_srv_get_sn(0, s_dev_info.sn_device_serial_number);
            //ui_text_set_text_attrs(text,s_dev_info.sn_device_serial_number, strlen(s_dev_info.sn_device_serial_number),FONT_ENCODE_UTF8, 0, FONT_DEFAULT|FONT_SHOW_SCROLL);
            break;
        }
        break;
    case ON_CHANGE_RELEASE:
        break;
    }
    return 0;
}
REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_SN_NUMBER)
.onchange = text_sn_number_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

//--------------------------设备信息-IMEI-----------------------//
static int layout_imei_ontouch(void *ctr, struct element_touch_event *e)
{
    struct layout *layout = (struct layout *)ctr;

    switch (e->event) {
    case ELM_EVENT_TOUCH_R_MOVE:
        ui_hide(CAT1_LAYOUT_IMEI);
        ui_show(CAT1_LAYOUT_DEVICE_INFORMATION);
        return true;
        break;
    default:
        break;
    }

    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_LAYOUT_IMEI)//一级菜单
.onchange = NULL,
 .onkey = NULL,
  .ontouch = layout_imei_ontouch,
};

static int text_imei_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct ui_text *text = (struct ui_text *)_ctrl;

    switch (event) {
    case ON_CHANGE_INIT:
        switch (text->elm.id) {
        case CAT1_TEXT_IMEI:
            aic_srv_tele_radio_get_imei(0, s_dev_info.imei_number);
            ui_text_set_text_attrs(text, s_dev_info.imei_number, strlen(s_dev_info.imei_number), FONT_ENCODE_UTF8, 0, FONT_DEFAULT | FONT_SHOW_SCROLL);
            break;
        }
        break;
    case ON_CHANGE_RELEASE:
        break;
    }
    return 0;
}
REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_IMEI)
.onchange = text_imei_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

//---------------------------设备信息-校准状态------------------------//
static int layout_calibration_ontouch(void *ctr, struct element_touch_event *e)
{
    struct layout *layout = (struct layout *)ctr;

    switch (e->event) {
    case ELM_EVENT_TOUCH_R_MOVE:
        ui_hide(CAT1_LAYOUT_CALIBRATION);
        ui_show(CAT1_LAYOUT_DEVICE_INFORMATION);
        return true;
        break;
    default:
        break;
    }

    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_LAYOUT_CALIBRATION)//一级菜单
.onchange = NULL,
 .onkey = NULL,
  .ontouch = layout_calibration_ontouch,
};

static int text_calibration_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct ui_text *text = (struct ui_text *)_ctrl;
    char *is_cali_str = malloc(sizeof(char) * 2);

    switch (event) {
    case ON_CHANGE_INIT:
        switch (text->elm.id) {
        case CAT1_TEXT_CALIBRATION:
            int is_cali = aic_srv_tele_radio_have_cali();
            sprintf(is_cali_str, "%d", is_cali);
            ui_text_set_text_attrs(text, is_cali_str, strlen(is_cali_str), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        }
        break;
    case ON_CHANGE_RELEASE:
        break;
    }
    return 0;
}
REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_CALIBRATION)
.onchange = text_calibration_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

//--------------------------------tele-----------------------------------------//
int network_rate_timer = 0;    //网络速率定时器
static int layout_tele_ontouch(void *ctr, struct element_touch_event *e)
{
    struct layout *layout = (struct layout *)ctr;

    switch (e->event) {
    case ELM_EVENT_TOUCH_R_MOVE:
        ui_hide(CAT1_LAYOUT_TELE);
        ui_show(CAT1_LAYOUT_FIRST_MENU);
        return true;
        break;
    default:
        break;
    }

    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_LAYOUT_TELE)//一级菜单
.onchange = NULL,
 .onkey = NULL,
  .ontouch = layout_tele_ontouch,
};

ts_radio_signal_info_t radio_signal_info = {0};
static int vlist_tele_ontouch(void *ctr, struct element_touch_event *e)
{
    struct ui_grid *grid = (struct ui_grid *)ctr;
    static u8 touch_action = 0;
    int sel_item;

    switch (e->event) {
    case ELM_EVENT_TOUCH_UP:
        if (touch_action != 1) {
            break;
        }

        sel_item = ui_grid_cur_item(grid);
        printf("sel_item:%d\n", sel_item);
        if (sel_item <= 0) {
            break;
        }
        switch (sel_item) {
        case 1:
            ui_hide(CAT1_LAYOUT_TELE);
            ui_show(CAT1_LAYOUT_SIM_INFORMATION);
            break;
        case 2:
            aic_srv_tele_radio_get_signal_info(0, &radio_signal_info);
            ui_hide(CAT1_LAYOUT_TELE);
            ui_show(CAT1_LAYOUT_SIGNAL_INFO);
            break;
        case 3:
            ui_hide(CAT1_LAYOUT_TELE);
            ui_show(CAT1_LAYOUT_CELL_INFO);
            break;
        case 4:
            ui_hide(CAT1_LAYOUT_TELE);
            ui_show(CAT1_LAYOUT_REG_STATUS);
            break;
        case 5:
            printf("lte_test!\n");
#if TCFG_CAT1_FUNC_NET_ENABLE
            if (is_network_rate_run == 0) {
                printf("network rate start!\n");
                engineering_mode_network_rate_test();
                is_network_rate_run = 1;
            }
#endif
            break;
        case 6:
            ui_hide(CAT1_LAYOUT_TELE);
            ui_show(CAT1_LAYOUT_CALL_SETTING);
            break;
        case 7:
            ui_hide(CAT1_LAYOUT_TELE);
            ui_show(CAT1_LAYOUT_SMS_SETTING);
            break;
        }
        break;
    case ELM_EVENT_TOUCH_HOLD:
        break;
    case ELM_EVENT_TOUCH_MOVE:
        touch_action = 2;
        break;
    case ELM_EVENT_TOUCH_DOWN:
        touch_action = 1;
        break;
    case ELM_EVENT_TOUCH_U_MOVE:
        break;
    case ELM_EVENT_TOUCH_D_MOVE:
        break;
    }
    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_VLIST_TELE)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = vlist_tele_ontouch,
};

//---------------------------tele-sim卡信息--------------------------------//
struct sim_info_t {
    char imsi[20];
    char phone_number[15];
};
struct sim_info_t sim_info;
static int layout_sim_information_ontouch(void *ctr, struct element_touch_event *e)
{
    struct layout *layout = (struct layout *)ctr;

    switch (e->event) {
    case ELM_EVENT_TOUCH_R_MOVE:
        ui_hide(CAT1_LAYOUT_SIM_INFORMATION);
        ui_show(CAT1_LAYOUT_TELE);
        return true;
        break;
    default:
        break;
    }

    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_LAYOUT_SIM_INFORMATION)//一级菜单
.onchange = NULL,
 .onkey = NULL,
  .ontouch = layout_sim_information_ontouch,
};

static int text_sim_information_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct ui_text *text = (struct ui_text *)_ctrl;
    ts_sim_status_e sim_status;
    char *sim_status_str = malloc(sizeof(char) * 3);

    switch (event) {
    case ON_CHANGE_INIT:
        switch (text->elm.id) {
        case CAT1_TEXT_SIM_ISPLACE:
            aic_srv_tele_sim_get_status(0, &sim_status);
            sprintf(sim_status_str, "%d", sim_status);
            ui_text_set_text_attrs(text, sim_status_str, strlen(sim_status_str), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_SIM_ISMI:
            aic_srv_tele_sim_get_imsi(0, sim_info.imsi);
            ui_text_set_text_attrs(text, sim_info.imsi, strlen(sim_info.imsi), FONT_ENCODE_UTF8, 0, FONT_DEFAULT | FONT_SHOW_SCROLL);
            break;
        case CAT1_TEXT_PHONE_NUMBER:
            aic_srv_tele_sim_get_msisdn(0, sim_info.phone_number);
            printf("phone number:%s\n", sim_info.phone_number);
            ui_text_set_text_attrs(text, sim_info.phone_number, strlen(sim_info.phone_number), FONT_ENCODE_UTF8, 0, FONT_DEFAULT | FONT_SHOW_SCROLL);
            break;
        }
        break;
    case ON_CHANGE_RELEASE:
        break;
    }
    return 0;
}
REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_SIM_ISPLACE)
.onchange = text_sim_information_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_SIM_ISMI)
.onchange = text_sim_information_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_PHONE_NUMBER)
.onchange = text_sim_information_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

//-----------------------------tele-信号状态----------------------------//
struct s_radio_signal_info {
    char signal_level[4];
    char signal_strength[4];
    char signal_level_val[4];
};
struct s_radio_signal_info show_radio_signal_info = {0};
int signal_info_timer = 0;
void signal_info_timer_handler(void)
{
    printf("signal info timer!\n");

    memset(&radio_signal_info, 0, sizeof(ts_radio_signal_info_t));
    aic_srv_tele_radio_get_signal_info(0, &radio_signal_info);

    sprintf(show_radio_signal_info.signal_strength, "%d", radio_signal_info.signal_strength);
    sprintf(show_radio_signal_info.signal_level, "%d", radio_signal_info.signal_level);
    sprintf(show_radio_signal_info.signal_level_val, "%d", radio_signal_info.signal_level_val);

    ui_text_set_text_by_id(CAT1_TEXT_SIGNAL_STRENGTH, show_radio_signal_info.signal_strength,
                           strlen(show_radio_signal_info.signal_strength), FONT_DEFAULT);
    ui_text_set_text_by_id(CAT1_TEXT_SIGNAL_LEVEL, show_radio_signal_info.signal_level,
                           strlen(show_radio_signal_info.signal_level), FONT_DEFAULT);
    ui_text_set_text_by_id(CAT1_TEXT_SIGNAL_STRENGTH_LEVEL, show_radio_signal_info.signal_level_val,
                           strlen(show_radio_signal_info.signal_level_val), FONT_DEFAULT);
}

static int layout_signal_info_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct layout *layout = (struct layout *)_ctrl;

    switch (event) {
    case ON_CHANGE_INIT:
        signal_info_timer = sys_timer_add(NULL, signal_info_timer_handler, 3000);
        break;
    case ON_CHANGE_RELEASE:
        if (signal_info_timer != 0) {
            sys_timer_del(signal_info_timer);
        }
        signal_info_timer = 0;
        break;
    }
    return 0;
}

static int layout_signal_info_ontouch(void *ctr, struct element_touch_event *e)
{
    struct layout *layout = (struct layout *)ctr;

    switch (e->event) {
    case ELM_EVENT_TOUCH_R_MOVE:
        ui_hide(CAT1_LAYOUT_SIGNAL_INFO);
        ui_show(CAT1_LAYOUT_TELE);
        return true;
        break;
    default:
        break;
    }

    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_LAYOUT_SIGNAL_INFO)//一级菜单
.onchange = layout_signal_info_onchange,
 .onkey = NULL,
  .ontouch = layout_signal_info_ontouch,
};

static int text_signal_info_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct ui_text *text = (struct ui_text *)_ctrl;

    switch (event) {
    case ON_CHANGE_INIT:
        switch (text->elm.id) {
        case CAT1_TEXT_SIGNAL_STRENGTH:
            //aic_srv_tele_radio_get_signal_info(0, &radio_signal_info);
            printf("signal strength:%d\n", radio_signal_info.signal_strength);
            sprintf(show_radio_signal_info.signal_strength, "%d", radio_signal_info.signal_strength);
            ui_text_set_text_attrs(text, show_radio_signal_info.signal_strength, strlen(show_radio_signal_info.signal_strength), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_SIGNAL_LEVEL:
            //aic_srv_tele_radio_get_signal_info(0, &radio_signal_info);
            printf("signal level:%d\n", radio_signal_info.signal_level);
            sprintf(show_radio_signal_info.signal_level, "%d", radio_signal_info.signal_level);
            ui_text_set_text_attrs(text, show_radio_signal_info.signal_level, strlen(show_radio_signal_info.signal_level), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_SIGNAL_STRENGTH_LEVEL:
            //aic_srv_tele_radio_get_signal_info(0, &radio_signal_info);
            printf("signal strength level:%d\n", radio_signal_info.signal_level_val);
            sprintf(show_radio_signal_info.signal_level_val, "%d", radio_signal_info.signal_level_val);
            ui_text_set_text_attrs(text, show_radio_signal_info.signal_level_val, strlen(show_radio_signal_info.signal_level_val), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        }
        break;
    case ON_CHANGE_RELEASE:
        break;
    }
    return 0;
}
REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_SIGNAL_STRENGTH)
.onchange = text_signal_info_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_SIGNAL_LEVEL)
.onchange = text_signal_info_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_SIGNAL_STRENGTH_LEVEL)
.onchange = text_signal_info_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

//-----------------------------tele-cell info------------------------------//
struct s_this_cell_info {
    int network_mode;
    char plmn_mcc[4];
    char plmn_mnc[4];
    char tac[10];
    char earfcn[10];
    char cellid[10];
    char rsrp[5];
    char rsrq[5];
    char pcid[4];
    char is_roaming[2];
    char band[5];
    char ul_bandwidth[5];
    char dl_bandwidth[5];
    char sinr[5];
    char srxlev[5];
    char rssi[5];
};

struct s_neighbor_cell_info {
    int network_mode;
    char earfcn[10];
    char rsrp[4];
    char rsrq[4];
    char pcid[4];
    char sinr[5];
    char srxlev[5];
    char rssi[4];
    char plmn_mcc[4];
    char plmn_mnc[4];
    char tac[10];
    char cellid[10];
};

struct s_this_cell_info show_cell_info = {0};
struct s_neighbor_cell_info show_neigh_cell_info[6] = {0};

static int layout_cell_info_ontouch(void *ctr, struct element_touch_event *e)
{
    struct layout *layout = (struct layout *)ctr;

    switch (e->event) {
    case ELM_EVENT_TOUCH_R_MOVE:
        ui_hide(CAT1_LAYOUT_CELL_INFO);
        ui_show(CAT1_LAYOUT_TELE);
        return true;
        break;
    default:
        break;
    }

    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_LAYOUT_CELL_INFO)//一级菜单
.onchange = NULL,
 .onkey = NULL,
  .ontouch = layout_cell_info_ontouch,
};

static int vlist_cell_info_ontouch(void *ctr, struct element_touch_event *e)
{
    struct ui_grid *grid = (struct ui_grid *)ctr;
    static u8 touch_action = 0;
    int sel_item;
    extern int32_t handleRadioCallback(void *p_param, uint32_t size);

    switch (e->event) {
    case ELM_EVENT_TOUCH_UP:
        if (touch_action != 1) {
            break;
        }

        sel_item = ui_grid_cur_item(grid);
        printf("sel_item:%d\n", sel_item);
        if (sel_item <= 0) {
            break;
        }
        switch (sel_item) {
        case 1:
            aic_srv_tele_radio_get_cell_info(0, handleRadioCallback);
            break;
        case 2:
            aic_srv_tele_radio_get_neighbor_cell_info_list(0, handleRadioCallback);
            break;
        }
        break;
    case ELM_EVENT_TOUCH_HOLD:
        break;
    case ELM_EVENT_TOUCH_MOVE:
        touch_action = 2;
        break;
    case ELM_EVENT_TOUCH_DOWN:
        touch_action = 1;
        break;
    case ELM_EVENT_TOUCH_U_MOVE:
        break;
    case ELM_EVENT_TOUCH_D_MOVE:
        break;
    }
    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_VLIST_CELL_INFO)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = vlist_cell_info_ontouch,
};

//-----------------------TELE-cell info-当区信息-------------------------//
static int layout_this_cell_ontouch(void *ctr, struct element_touch_event *e)
{
    struct layout *layout = (struct layout *)ctr;

    switch (e->event) {
    case ELM_EVENT_TOUCH_R_MOVE:
        ui_hide(CAT1_LAYOUT_THIS_CELL);
        ui_show(CAT1_LAYOUT_CELL_INFO);
        return true;
        break;
    default:
        break;
    }

    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_LAYOUT_THIS_CELL)//一级菜单
.onchange = NULL,
 .onkey = NULL,
  .ontouch = layout_this_cell_ontouch,
};

char *network_type[7] = {"unknown", "GSM", "CDMA", "WCDMA", "TDSCDMA", "LTE", "NR"};
static int text_this_cell_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct ui_text *text = (struct ui_text *)_ctrl;
    extern ts_radio_get_cell_info_resp_t cell_info_t;

    switch (event) {
    case ON_CHANGE_INIT:
        switch (text->elm.id) {
        case CAT1_TEXT_CELL_NETWORK:
            ui_text_set_text_attrs(text, network_type[cell_info_t.cell_info.rat_type], strlen(network_type[cell_info_t.cell_info.rat_type]), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_CELL_MCC:
            ui_text_set_text_attrs(text, cell_info_t.cell_info.plmn_mcc, strlen(cell_info_t.cell_info.plmn_mcc), FONT_ENCODE_UTF8, 0, FONT_DEFAULT | FONT_SHOW_SCROLL);
            break;
        case CAT1_TEXT_CELL_MNC:
            ui_text_set_text_attrs(text, cell_info_t.cell_info.plmn_mnc, strlen(cell_info_t.cell_info.plmn_mnc), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_CELL_TAC:
            sprintf(show_cell_info.tac, "%d", cell_info_t.cell_info.tac);
            ui_text_set_text_attrs(text, show_cell_info.tac, strlen(show_cell_info.tac), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_CELL_EARFCN:
            sprintf(show_cell_info.earfcn, "%d", cell_info_t.cell_info.earfcn);
            ui_text_set_text_attrs(text, show_cell_info.earfcn, strlen(show_cell_info.earfcn), FONT_ENCODE_UTF8, 0, FONT_DEFAULT | FONT_SHOW_SCROLL);
            break;
        case CAT1_TEXT_CELL_CELLID:
            sprintf(show_cell_info.cellid, "%d", cell_info_t.cell_info.cellid);
            ui_text_set_text_attrs(text, show_cell_info.cellid, strlen(show_cell_info.cellid), FONT_ENCODE_UTF8, 0, FONT_DEFAULT | FONT_SHOW_SCROLL);
            break;
        case CAT1_TEXT_CELL_RSRP:
            sprintf(show_cell_info.rsrp, "%d", cell_info_t.cell_info.rsrp);
            ui_text_set_text_attrs(text, show_cell_info.rsrp, strlen(show_cell_info.rsrp), FONT_ENCODE_UTF8, 0, FONT_DEFAULT | FONT_SHOW_SCROLL);
            break;
        case CAT1_TEXT_CELL_RSRQ:
            sprintf(show_cell_info.rsrq, "%d", cell_info_t.cell_info.rsrq);
            ui_text_set_text_attrs(text, show_cell_info.rsrq, strlen(show_cell_info.rsrq), FONT_ENCODE_UTF8, 0, FONT_DEFAULT | FONT_SHOW_SCROLL);
            break;
        case CAT1_TEXT_CELL_PCID:
            sprintf(show_cell_info.pcid, "%d", cell_info_t.cell_info.pcid);
            ui_text_set_text_attrs(text, show_cell_info.pcid, strlen(show_cell_info.pcid), FONT_ENCODE_UTF8, 0, FONT_DEFAULT | FONT_SHOW_SCROLL);
            break;
        case CAT1_TEXT_CELL_IS_ROAMING:
            sprintf(show_cell_info.is_roaming, "%d", cell_info_t.cell_info.is_roaming);
            ui_text_set_text_attrs(text, show_cell_info.is_roaming, strlen(show_cell_info.is_roaming), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_CELL_BAND:
            sprintf(show_cell_info.band, "%d", cell_info_t.cell_info.band);
            ui_text_set_text_attrs(text, show_cell_info.band, strlen(show_cell_info.band), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_CELL_UL_BANDWIDTH:
            sprintf(show_cell_info.ul_bandwidth, "%d", cell_info_t.cell_info.ul_bandwidth);
            ui_text_set_text_attrs(text, show_cell_info.ul_bandwidth, strlen(show_cell_info.ul_bandwidth), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_CELL_DL_BANDWIDTH:
            sprintf(show_cell_info.dl_bandwidth, "%d", cell_info_t.cell_info.dl_bandwidth);
            ui_text_set_text_attrs(text, show_cell_info.dl_bandwidth, strlen(show_cell_info.dl_bandwidth), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_CELL_SINR:
            sprintf(show_cell_info.sinr, "%d", cell_info_t.cell_info.sinr);
            ui_text_set_text_attrs(text, show_cell_info.sinr, strlen(show_cell_info.sinr), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_CELL_SRXLEV:
            sprintf(show_cell_info.srxlev, "%d", cell_info_t.cell_info.srxlev);
            ui_text_set_text_attrs(text, show_cell_info.srxlev, strlen(show_cell_info.srxlev), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_CELL_RSSI:
            sprintf(show_cell_info.rssi, "%d", cell_info_t.cell_info.rssi);
            ui_text_set_text_attrs(text, show_cell_info.rssi, strlen(show_cell_info.rssi), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        }
        break;
    case ON_CHANGE_RELEASE:
        break;
    }
    return 0;
}
REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_CELL_NETWORK)
.onchange = text_this_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_CELL_MCC)
.onchange = text_this_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_CELL_MNC)
.onchange = text_this_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_CELL_TAC)
.onchange = text_this_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_CELL_EARFCN)
.onchange = text_this_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_CELL_CELLID)
.onchange = text_this_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_CELL_RSRP)
.onchange = text_this_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_CELL_RSRQ)
.onchange = text_this_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_CELL_PCID)
.onchange = text_this_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_CELL_IS_ROAMING)
.onchange = text_this_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_CELL_BAND)
.onchange = text_this_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_CELL_UL_BANDWIDTH)
.onchange = text_this_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_CELL_DL_BANDWIDTH)
.onchange = text_this_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_CELL_SINR)
.onchange = text_this_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_CELL_SRXLEV)
.onchange = text_this_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_CELL_RSSI)
.onchange = text_this_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

//------------------tele-小区信息-邻区信息----------------------------//
static int layout_neighbor_cell_ontouch(void *ctr, struct element_touch_event *e)
{
    struct layout *layout = (struct layout *)ctr;

    switch (e->event) {
    case ELM_EVENT_TOUCH_R_MOVE:
        ui_hide(CAT1_LAYOUT_NEIGHBOR);
        ui_show(CAT1_LAYOUT_CELL_INFO);
        return true;
        break;
    default:
        break;
    }

    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_LAYOUT_NEIGHBOR)//一级菜单
.onchange = NULL,
 .onkey = NULL,
  .ontouch = layout_neighbor_cell_ontouch,
};

static int text_neighbor_cell_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct ui_text *text = (struct ui_text *)_ctrl;
    extern ts_radio_get_neighbor_cell_info_list_resp_t neighbor_cell_info_list_t;

    switch (event) {
    case ON_CHANGE_INIT:
        switch (text->elm.id) {
        case CAT1_TEXT_NEIGHBOR_NETWORK:
            ui_text_set_text_attrs(text, network_type[neighbor_cell_info_list_t.ncell_info_list.ncell_info[0].rat_type], strlen(network_type[neighbor_cell_info_list_t.ncell_info_list.ncell_info[0].rat_type]), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_NEIGHBOR_EARFCN:
            sprintf(show_neigh_cell_info[0].earfcn, "%d", neighbor_cell_info_list_t.ncell_info_list.ncell_info[0].earfcn);
            ui_text_set_text_attrs(text, show_neigh_cell_info[0].earfcn, strlen(show_neigh_cell_info[0].earfcn), FONT_ENCODE_UTF8, 0, FONT_DEFAULT | FONT_SHOW_SCROLL);
            break;
        case CAT1_TEXT_NEIGHBOR_RSRP:
            sprintf(show_neigh_cell_info[0].rsrp, "%d", neighbor_cell_info_list_t.ncell_info_list.ncell_info[0].rsrp);
            ui_text_set_text_attrs(text, show_neigh_cell_info[0].rsrp, strlen(show_neigh_cell_info[0].rsrp), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_NEIGHBOR_RSRQ:
            sprintf(show_neigh_cell_info[0].rsrq, "%d", neighbor_cell_info_list_t.ncell_info_list.ncell_info[0].rsrq);
            ui_text_set_text_attrs(text, show_neigh_cell_info[0].rsrq, strlen(show_neigh_cell_info[0].rsrq), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_NEIGHBOR_PCID:
            sprintf(show_neigh_cell_info[0].pcid, "%d", neighbor_cell_info_list_t.ncell_info_list.ncell_info[0].pcid);
            ui_text_set_text_attrs(text, show_neigh_cell_info[0].pcid, strlen(show_neigh_cell_info[0].pcid), FONT_ENCODE_UTF8, 0, FONT_DEFAULT | FONT_SHOW_SCROLL);
            break;
        case CAT1_TEXT_NEIGHBOR_SINR:
            sprintf(show_neigh_cell_info[0].sinr, "%d", neighbor_cell_info_list_t.ncell_info_list.ncell_info[0].sinr);
            ui_text_set_text_attrs(text, show_neigh_cell_info[0].sinr, strlen(show_neigh_cell_info[0].sinr), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_NEIGHBOR_SRXLEV:
            sprintf(show_neigh_cell_info[0].srxlev, "%d", neighbor_cell_info_list_t.ncell_info_list.ncell_info[0].srxlev);
            ui_text_set_text_attrs(text, show_neigh_cell_info[0].srxlev, strlen(show_neigh_cell_info[0].srxlev), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_NEIGHBOR_RSSI:
            sprintf(show_neigh_cell_info[0].rssi, "%d", neighbor_cell_info_list_t.ncell_info_list.ncell_info[0].rssi);
            ui_text_set_text_attrs(text, show_neigh_cell_info[0].rssi, strlen(show_neigh_cell_info[0].rssi), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_NEIGHBOR_MCC:
            ui_text_set_text_attrs(text, neighbor_cell_info_list_t.ncell_info_list.ncell_info[0].plmn_mcc, strlen(neighbor_cell_info_list_t.ncell_info_list.ncell_info[0].plmn_mcc), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_NEIGHBOR_MNC:
            ui_text_set_text_attrs(text, neighbor_cell_info_list_t.ncell_info_list.ncell_info[0].plmn_mnc, strlen(neighbor_cell_info_list_t.ncell_info_list.ncell_info[0].plmn_mnc), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_NEIGHBOR_TAC:
            sprintf(show_neigh_cell_info[0].tac, "%d", neighbor_cell_info_list_t.ncell_info_list.ncell_info[0].tac);
            ui_text_set_text_attrs(text, show_neigh_cell_info[0].tac, strlen(show_neigh_cell_info[0].tac), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_NEIGHBOR_CELLID:
            sprintf(show_neigh_cell_info[0].cellid, "%d", neighbor_cell_info_list_t.ncell_info_list.ncell_info[0].cellid);
            ui_text_set_text_attrs(text, show_neigh_cell_info[0].cellid, strlen(show_neigh_cell_info[0].cellid), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_NEIGHBOR_NETWORK1:
            ui_text_set_text_attrs(text, network_type[neighbor_cell_info_list_t.ncell_info_list.ncell_info[1].rat_type], strlen(network_type[neighbor_cell_info_list_t.ncell_info_list.ncell_info[1].rat_type]), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_NEIGHBOR_EARFCN1:
            sprintf(show_neigh_cell_info[1].earfcn, "%d", neighbor_cell_info_list_t.ncell_info_list.ncell_info[1].earfcn);
            ui_text_set_text_attrs(text, show_neigh_cell_info[1].earfcn, strlen(show_neigh_cell_info[1].earfcn), FONT_ENCODE_UTF8, 0, FONT_DEFAULT | FONT_SHOW_SCROLL);
            break;
        case CAT1_TEXT_NEIGHBOR_RSRP1:
            sprintf(show_neigh_cell_info[1].rsrp, "%d", neighbor_cell_info_list_t.ncell_info_list.ncell_info[1].rsrp);
            ui_text_set_text_attrs(text, show_neigh_cell_info[1].rsrp, strlen(show_neigh_cell_info[1].rsrp), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_NEIGHBOR_RSRQ1:
            sprintf(show_neigh_cell_info[1].rsrq, "%d", neighbor_cell_info_list_t.ncell_info_list.ncell_info[1].rsrq);
            ui_text_set_text_attrs(text, show_neigh_cell_info[1].rsrq, strlen(show_neigh_cell_info[1].rsrq), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_NEIGHBOR_PCID1:
            sprintf(show_neigh_cell_info[1].pcid, "%d", neighbor_cell_info_list_t.ncell_info_list.ncell_info[1].pcid);
            ui_text_set_text_attrs(text, show_neigh_cell_info[1].pcid, strlen(show_neigh_cell_info[1].pcid), FONT_ENCODE_UTF8, 0, FONT_DEFAULT | FONT_SHOW_SCROLL);
            break;
        case CAT1_TEXT_NEIGHBOR_SINR1:
            sprintf(show_neigh_cell_info[1].sinr, "%d", neighbor_cell_info_list_t.ncell_info_list.ncell_info[1].sinr);
            ui_text_set_text_attrs(text, show_neigh_cell_info[1].sinr, strlen(show_neigh_cell_info[1].sinr), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_NEIGHBOR_SRXLEV1:
            sprintf(show_neigh_cell_info[1].srxlev, "%d", neighbor_cell_info_list_t.ncell_info_list.ncell_info[1].srxlev);
            ui_text_set_text_attrs(text, show_neigh_cell_info[1].srxlev, strlen(show_neigh_cell_info[1].srxlev), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_NEIGHBOR_RSSI1:
            sprintf(show_neigh_cell_info[1].rssi, "%d", neighbor_cell_info_list_t.ncell_info_list.ncell_info[1].rssi);
            ui_text_set_text_attrs(text, show_neigh_cell_info[1].rssi, strlen(show_neigh_cell_info[1].rssi), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_NEIGHBOR_MCC1:
            ui_text_set_text_attrs(text, neighbor_cell_info_list_t.ncell_info_list.ncell_info[1].plmn_mcc, strlen(neighbor_cell_info_list_t.ncell_info_list.ncell_info[1].plmn_mcc), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_NEIGHBOR_MNC1:
            ui_text_set_text_attrs(text, neighbor_cell_info_list_t.ncell_info_list.ncell_info[1].plmn_mnc, strlen(neighbor_cell_info_list_t.ncell_info_list.ncell_info[1].plmn_mnc), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_NEIGHBOR_TAC1:
            sprintf(show_neigh_cell_info[1].tac, "%d", neighbor_cell_info_list_t.ncell_info_list.ncell_info[1].tac);
            ui_text_set_text_attrs(text, show_neigh_cell_info[1].tac, strlen(show_neigh_cell_info[1].tac), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_NEIGHBOR_CELLID1:
            sprintf(show_neigh_cell_info[1].cellid, "%d", neighbor_cell_info_list_t.ncell_info_list.ncell_info[1].cellid);
            ui_text_set_text_attrs(text, show_neigh_cell_info[1].cellid, strlen(show_neigh_cell_info[1].cellid), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_NEIGHBOR_NETWORK2:
            ui_text_set_text_attrs(text, network_type[neighbor_cell_info_list_t.ncell_info_list.ncell_info[2].rat_type], strlen(network_type[neighbor_cell_info_list_t.ncell_info_list.ncell_info[2].rat_type]), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_NEIGHBOR_EARFCN2:
            sprintf(show_neigh_cell_info[2].earfcn, "%d", neighbor_cell_info_list_t.ncell_info_list.ncell_info[2].earfcn);
            ui_text_set_text_attrs(text, show_neigh_cell_info[2].earfcn, strlen(show_neigh_cell_info[2].earfcn), FONT_ENCODE_UTF8, 0, FONT_DEFAULT | FONT_SHOW_SCROLL);
            break;
        case CAT1_TEXT_NEIGHBOR_RSRP2:
            sprintf(show_neigh_cell_info[2].rsrp, "%d", neighbor_cell_info_list_t.ncell_info_list.ncell_info[2].rsrp);
            ui_text_set_text_attrs(text, show_neigh_cell_info[2].rsrp, strlen(show_neigh_cell_info[2].rsrp), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_NEIGHBOR_RSRQ2:
            sprintf(show_neigh_cell_info[2].rsrq, "%d", neighbor_cell_info_list_t.ncell_info_list.ncell_info[2].rsrq);
            ui_text_set_text_attrs(text, show_neigh_cell_info[2].rsrq, strlen(show_neigh_cell_info[2].rsrq), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_NEIGHBOR_PCID2:
            sprintf(show_neigh_cell_info[2].pcid, "%d", neighbor_cell_info_list_t.ncell_info_list.ncell_info[2].pcid);
            ui_text_set_text_attrs(text, show_neigh_cell_info[2].pcid, strlen(show_neigh_cell_info[2].pcid), FONT_ENCODE_UTF8, 0, FONT_DEFAULT | FONT_SHOW_SCROLL);
            break;
        case CAT1_TEXT_NEIGHBOR_SINR2:
            sprintf(show_neigh_cell_info[2].sinr, "%d", neighbor_cell_info_list_t.ncell_info_list.ncell_info[2].sinr);
            ui_text_set_text_attrs(text, show_neigh_cell_info[2].sinr, strlen(show_neigh_cell_info[2].sinr), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_NEIGHBOR_SRXLEV2:
            sprintf(show_neigh_cell_info[2].srxlev, "%d", neighbor_cell_info_list_t.ncell_info_list.ncell_info[2].srxlev);
            ui_text_set_text_attrs(text, show_neigh_cell_info[2].srxlev, strlen(show_neigh_cell_info[2].srxlev), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_NEIGHBOR_RSSI2:
            sprintf(show_neigh_cell_info[2].rssi, "%d", neighbor_cell_info_list_t.ncell_info_list.ncell_info[2].rssi);
            ui_text_set_text_attrs(text, show_neigh_cell_info[2].rssi, strlen(show_neigh_cell_info[2].rssi), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_NEIGHBOR_MCC2:
            ui_text_set_text_attrs(text, neighbor_cell_info_list_t.ncell_info_list.ncell_info[2].plmn_mcc, strlen(neighbor_cell_info_list_t.ncell_info_list.ncell_info[2].plmn_mcc), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_NEIGHBOR_MNC2:
            ui_text_set_text_attrs(text, neighbor_cell_info_list_t.ncell_info_list.ncell_info[2].plmn_mnc, strlen(neighbor_cell_info_list_t.ncell_info_list.ncell_info[2].plmn_mnc), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_NEIGHBOR_TAC2:
            sprintf(show_neigh_cell_info[2].tac, "%d", neighbor_cell_info_list_t.ncell_info_list.ncell_info[2].tac);
            ui_text_set_text_attrs(text, show_neigh_cell_info[2].tac, strlen(show_neigh_cell_info[2].tac), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_NEIGHBOR_CELLID2:
            sprintf(show_neigh_cell_info[2].cellid, "%d", neighbor_cell_info_list_t.ncell_info_list.ncell_info[2].cellid);
            ui_text_set_text_attrs(text, show_neigh_cell_info[2].cellid, strlen(show_neigh_cell_info[2].cellid), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_NEIGHBOR_NETWORK3:
            ui_text_set_text_attrs(text, network_type[neighbor_cell_info_list_t.ncell_info_list.ncell_info[3].rat_type], strlen(network_type[neighbor_cell_info_list_t.ncell_info_list.ncell_info[3].rat_type]), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_NEIGHBOR_EARFCN3:
            sprintf(show_neigh_cell_info[3].earfcn, "%d", neighbor_cell_info_list_t.ncell_info_list.ncell_info[3].earfcn);
            ui_text_set_text_attrs(text, show_neigh_cell_info[3].earfcn, strlen(show_neigh_cell_info[3].earfcn), FONT_ENCODE_UTF8, 0, FONT_DEFAULT | FONT_SHOW_SCROLL);
            break;
        case CAT1_TEXT_NEIGHBOR_RSRP3:
            sprintf(show_neigh_cell_info[3].rsrp, "%d", neighbor_cell_info_list_t.ncell_info_list.ncell_info[3].rsrp);
            ui_text_set_text_attrs(text, show_neigh_cell_info[3].rsrp, strlen(show_neigh_cell_info[3].rsrp), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_NEIGHBOR_RSRQ3:
            sprintf(show_neigh_cell_info[3].rsrq, "%d", neighbor_cell_info_list_t.ncell_info_list.ncell_info[3].rsrq);
            ui_text_set_text_attrs(text, show_neigh_cell_info[3].rsrq, strlen(show_neigh_cell_info[3].rsrq), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_NEIGHBOR_PCID3:
            sprintf(show_neigh_cell_info[3].pcid, "%d", neighbor_cell_info_list_t.ncell_info_list.ncell_info[3].pcid);
            ui_text_set_text_attrs(text, show_neigh_cell_info[3].pcid, strlen(show_neigh_cell_info[3].pcid), FONT_ENCODE_UTF8, 0, FONT_DEFAULT | FONT_SHOW_SCROLL);
            break;
        case CAT1_TEXT_NEIGHBOR_SINR3:
            sprintf(show_neigh_cell_info[3].sinr, "%d", neighbor_cell_info_list_t.ncell_info_list.ncell_info[3].sinr);
            ui_text_set_text_attrs(text, show_neigh_cell_info[3].sinr, strlen(show_neigh_cell_info[3].sinr), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_NEIGHBOR_SRXLEV3:
            sprintf(show_neigh_cell_info[3].srxlev, "%d", neighbor_cell_info_list_t.ncell_info_list.ncell_info[3].srxlev);
            ui_text_set_text_attrs(text, show_neigh_cell_info[3].srxlev, strlen(show_neigh_cell_info[3].srxlev), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_NEIGHBOR_RSSI3:
            sprintf(show_neigh_cell_info[3].rssi, "%d", neighbor_cell_info_list_t.ncell_info_list.ncell_info[3].rssi);
            ui_text_set_text_attrs(text, show_neigh_cell_info[3].rssi, strlen(show_neigh_cell_info[3].rssi), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_NEIGHBOR_MCC3:
            ui_text_set_text_attrs(text, neighbor_cell_info_list_t.ncell_info_list.ncell_info[3].plmn_mcc, strlen(neighbor_cell_info_list_t.ncell_info_list.ncell_info[3].plmn_mcc), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_NEIGHBOR_MNC3:
            ui_text_set_text_attrs(text, neighbor_cell_info_list_t.ncell_info_list.ncell_info[3].plmn_mnc, strlen(neighbor_cell_info_list_t.ncell_info_list.ncell_info[3].plmn_mnc), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_NEIGHBOR_TAC3:
            sprintf(show_neigh_cell_info[3].tac, "%d", neighbor_cell_info_list_t.ncell_info_list.ncell_info[3].tac);
            ui_text_set_text_attrs(text, show_neigh_cell_info[3].tac, strlen(show_neigh_cell_info[3].tac), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_NEIGHBOR_CELLID3:
            sprintf(show_neigh_cell_info[3].cellid, "%d", neighbor_cell_info_list_t.ncell_info_list.ncell_info[3].cellid);
            ui_text_set_text_attrs(text, show_neigh_cell_info[3].cellid, strlen(show_neigh_cell_info[3].cellid), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_NEIGHBOR_NETWORK4:
            ui_text_set_text_attrs(text, network_type[neighbor_cell_info_list_t.ncell_info_list.ncell_info[4].rat_type], strlen(network_type[neighbor_cell_info_list_t.ncell_info_list.ncell_info[4].rat_type]), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_NEIGHBOR_EARFCN4:
            sprintf(show_neigh_cell_info[4].earfcn, "%d", neighbor_cell_info_list_t.ncell_info_list.ncell_info[4].earfcn);
            ui_text_set_text_attrs(text, show_neigh_cell_info[4].earfcn, strlen(show_neigh_cell_info[4].earfcn), FONT_ENCODE_UTF8, 0, FONT_DEFAULT | FONT_SHOW_SCROLL);
            break;
        case CAT1_TEXT_NEIGHBOR_RSRP4:
            sprintf(show_neigh_cell_info[4].rsrp, "%d", neighbor_cell_info_list_t.ncell_info_list.ncell_info[4].rsrp);
            ui_text_set_text_attrs(text, show_neigh_cell_info[4].rsrp, strlen(show_neigh_cell_info[4].rsrp), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_NEIGHBOR_RSRQ4:
            sprintf(show_neigh_cell_info[0].rsrq, "%d", neighbor_cell_info_list_t.ncell_info_list.ncell_info[0].rsrq);
            ui_text_set_text_attrs(text, show_neigh_cell_info[0].rsrq, strlen(show_neigh_cell_info[0].rsrq), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_NEIGHBOR_PCID4:
            sprintf(show_neigh_cell_info[4].pcid, "%d", neighbor_cell_info_list_t.ncell_info_list.ncell_info[4].pcid);
            ui_text_set_text_attrs(text, show_neigh_cell_info[4].pcid, strlen(show_neigh_cell_info[4].pcid), FONT_ENCODE_UTF8, 0, FONT_DEFAULT | FONT_SHOW_SCROLL);
            break;
        case CAT1_TEXT_NEIGHBOR_SINR4:
            sprintf(show_neigh_cell_info[4].sinr, "%d", neighbor_cell_info_list_t.ncell_info_list.ncell_info[4].sinr);
            ui_text_set_text_attrs(text, show_neigh_cell_info[4].sinr, strlen(show_neigh_cell_info[4].sinr), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_NEIGHBOR_SRXLEV4:
            sprintf(show_neigh_cell_info[4].srxlev, "%d", neighbor_cell_info_list_t.ncell_info_list.ncell_info[4].srxlev);
            ui_text_set_text_attrs(text, show_neigh_cell_info[4].srxlev, strlen(show_neigh_cell_info[4].srxlev), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_NEIGHBOR_RSSI4:
            sprintf(show_neigh_cell_info[4].rssi, "%d", neighbor_cell_info_list_t.ncell_info_list.ncell_info[4].rssi);
            ui_text_set_text_attrs(text, show_neigh_cell_info[4].rssi, strlen(show_neigh_cell_info[4].rssi), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_NEIGHBOR_MCC4:
            ui_text_set_text_attrs(text, neighbor_cell_info_list_t.ncell_info_list.ncell_info[4].plmn_mcc, strlen(neighbor_cell_info_list_t.ncell_info_list.ncell_info[4].plmn_mcc), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_NEIGHBOR_MNC4:
            ui_text_set_text_attrs(text, neighbor_cell_info_list_t.ncell_info_list.ncell_info[4].plmn_mnc, strlen(neighbor_cell_info_list_t.ncell_info_list.ncell_info[4].plmn_mnc), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_NEIGHBOR_TAC4:
            sprintf(show_neigh_cell_info[4].tac, "%d", neighbor_cell_info_list_t.ncell_info_list.ncell_info[4].tac);
            ui_text_set_text_attrs(text, show_neigh_cell_info[4].tac, strlen(show_neigh_cell_info[4].tac), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_NEIGHBOR_CELLID4:
            sprintf(show_neigh_cell_info[4].cellid, "%d", neighbor_cell_info_list_t.ncell_info_list.ncell_info[4].cellid);
            ui_text_set_text_attrs(text, show_neigh_cell_info[4].cellid, strlen(show_neigh_cell_info[4].cellid), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_NEIGHBOR_NETWORK5:
            ui_text_set_text_attrs(text, network_type[neighbor_cell_info_list_t.ncell_info_list.ncell_info[5].rat_type], strlen(network_type[neighbor_cell_info_list_t.ncell_info_list.ncell_info[5].rat_type]), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_NEIGHBOR_EARFCN5:
            sprintf(show_neigh_cell_info[5].earfcn, "%d", neighbor_cell_info_list_t.ncell_info_list.ncell_info[5].earfcn);
            ui_text_set_text_attrs(text, show_neigh_cell_info[5].earfcn, strlen(show_neigh_cell_info[5].earfcn), FONT_ENCODE_UTF8, 0, FONT_DEFAULT | FONT_SHOW_SCROLL);
            break;
        case CAT1_TEXT_NEIGHBOR_RSRP5:
            sprintf(show_neigh_cell_info[5].rsrp, "%d", neighbor_cell_info_list_t.ncell_info_list.ncell_info[5].rsrp);
            ui_text_set_text_attrs(text, show_neigh_cell_info[5].rsrp, strlen(show_neigh_cell_info[5].rsrp), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_NEIGHBOR_RSRQ5:
            sprintf(show_neigh_cell_info[5].rsrq, "%d", neighbor_cell_info_list_t.ncell_info_list.ncell_info[5].rsrq);
            ui_text_set_text_attrs(text, show_neigh_cell_info[5].rsrq, strlen(show_neigh_cell_info[5].rsrq), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_NEIGHBOR_PCID5:
            sprintf(show_neigh_cell_info[5].pcid, "%d", neighbor_cell_info_list_t.ncell_info_list.ncell_info[5].pcid);
            ui_text_set_text_attrs(text, show_neigh_cell_info[5].pcid, strlen(show_neigh_cell_info[5].pcid), FONT_ENCODE_UTF8, 0, FONT_DEFAULT | FONT_SHOW_SCROLL);
            break;
        case CAT1_TEXT_NEIGHBOR_SINR5:
            sprintf(show_neigh_cell_info[5].sinr, "%d", neighbor_cell_info_list_t.ncell_info_list.ncell_info[5].sinr);
            ui_text_set_text_attrs(text, show_neigh_cell_info[5].sinr, strlen(show_neigh_cell_info[5].sinr), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_NEIGHBOR_SRXLEV5:
            sprintf(show_neigh_cell_info[5].srxlev, "%d", neighbor_cell_info_list_t.ncell_info_list.ncell_info[5].srxlev);
            ui_text_set_text_attrs(text, show_neigh_cell_info[5].srxlev, strlen(show_neigh_cell_info[5].srxlev), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_NEIGHBOR_RSSI5:
            sprintf(show_neigh_cell_info[5].rssi, "%d", neighbor_cell_info_list_t.ncell_info_list.ncell_info[5].rssi);
            ui_text_set_text_attrs(text, show_neigh_cell_info[5].rssi, strlen(show_neigh_cell_info[5].rssi), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_NEIGHBOR_MCC5:
            ui_text_set_text_attrs(text, neighbor_cell_info_list_t.ncell_info_list.ncell_info[5].plmn_mcc, strlen(neighbor_cell_info_list_t.ncell_info_list.ncell_info[5].plmn_mcc), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_NEIGHBOR_MNC5:
            ui_text_set_text_attrs(text, neighbor_cell_info_list_t.ncell_info_list.ncell_info[5].plmn_mnc, strlen(neighbor_cell_info_list_t.ncell_info_list.ncell_info[5].plmn_mnc), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_NEIGHBOR_TAC5:
            sprintf(show_neigh_cell_info[5].tac, "%d", neighbor_cell_info_list_t.ncell_info_list.ncell_info[5].tac);
            ui_text_set_text_attrs(text, show_neigh_cell_info[5].tac, strlen(show_neigh_cell_info[5].tac), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_NEIGHBOR_CELLID5:
            sprintf(show_neigh_cell_info[5].cellid, "%d", neighbor_cell_info_list_t.ncell_info_list.ncell_info[5].cellid);
            ui_text_set_text_attrs(text, show_neigh_cell_info[5].cellid, strlen(show_neigh_cell_info[5].cellid), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        }
        break;
    case ON_CHANGE_RELEASE:
        break;
    }
    return 0;
}
REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_NETWORK)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_EARFCN)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_RSRP)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_RSRQ)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_PCID)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_SINR)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_SRXLEV)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_RSSI)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_MCC)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_MNC)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_TAC)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_CELLID)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_NETWORK1)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_EARFCN1)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_RSRP1)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_RSRQ1)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_PCID1)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_SINR1)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_SRXLEV1)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_RSSI1)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_MCC1)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_MNC1)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_TAC1)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_CELLID1)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_NETWORK2)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_EARFCN2)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_RSRP2)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_RSRQ2)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_PCID2)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_SINR2)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_SRXLEV2)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_RSSI2)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_MCC2)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_MNC2)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_TAC2)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_CELLID2)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_NETWORK3)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_EARFCN3)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_RSRP3)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_RSRQ3)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_PCID3)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_SINR3)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_SRXLEV3)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_RSSI3)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_MCC3)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_MNC3)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_TAC3)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_CELLID3)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_NETWORK4)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_EARFCN4)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_RSRP4)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_RSRQ4)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_PCID4)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_SINR4)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_SRXLEV4)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_RSSI4)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_MCC4)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_MNC4)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_TAC4)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_CELLID4)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_NETWORK5)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_EARFCN5)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_RSRP5)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_RSRQ5)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_PCID5)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_SINR5)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_SRXLEV5)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_RSSI5)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_MCC5)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_MNC5)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_TAC5)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NEIGHBOR_CELLID5)
.onchange = text_neighbor_cell_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

//------------------------------tele-驻网状态---------------------------//
ts_radio_operator_info_t radio_operator_info = {0};
ts_data_pdp_info_t data_pdp_info = {0};

struct reg_status {
    char network_reg[2];
    char is_ims[2];
    char is_pdp[2];
    char ip[20];
};

struct reg_status s_reg_status;

char *status[2] = {"是", "否"};

static int layout_reg_status_ontouch(void *ctr, struct element_touch_event *e)
{
    struct layout *layout = (struct layout *)ctr;

    switch (e->event) {
    case ELM_EVENT_TOUCH_R_MOVE:
        ui_hide(CAT1_LAYOUT_REG_STATUS);
        ui_show(CAT1_LAYOUT_TELE);
        return true;
        break;
    default:
        break;
    }

    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_LAYOUT_REG_STATUS)//一级菜单
.onchange = NULL,
 .onkey = NULL,
  .ontouch = layout_reg_status_ontouch,
};

static int text_reg_status_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct ui_text *text = (struct ui_text *)_ctrl;
    ts_radio_reg_status_e radio_reg_status;

    switch (event) {
    case ON_CHANGE_INIT:
        switch (text->elm.id) {
        case CAT1_TEXT_IS_CONNECTED_NETWORK:
            aic_srv_tele_radio_get_reg_status(0, &radio_reg_status);
            sprintf(s_reg_status.network_reg, "%d", radio_reg_status);
            ui_text_set_text_attrs(text, s_reg_status.network_reg, strlen(s_reg_status.network_reg), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_OPERATOR:
            ui_text_set_text_attrs(text, cat1_get_operator(), strlen(cat1_get_operator()), FONT_ENCODE_UTF8, 0, FONT_DEFAULT | FONT_SHOW_SCROLL);
            break;
        case CAT1_TEXT_IS_IMS:
            int is_ims =  aic_srv_tele_radio_have_ims_reg(0);
            sprintf(s_reg_status.is_ims, "%d", is_ims);
            ui_text_set_text_attrs(text, s_reg_status.is_ims, strlen(s_reg_status.is_ims), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_IS_PDP:
            aic_srv_tele_data_get_internet_pdp_info(0, &data_pdp_info);
            sprintf(s_reg_status.is_pdp, "%d", data_pdp_info.pdp_status);
            memcpy(s_reg_status.ip, data_pdp_info.link_info.ipv4_addr, strlen(data_pdp_info.link_info.ipv4_addr));
            ui_text_set_text_attrs(text, s_reg_status.is_pdp, strlen(s_reg_status.is_pdp), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_IP:
            ui_text_set_text_attrs(text, s_reg_status.ip, strlen(s_reg_status.ip), FONT_ENCODE_UTF8, 0, FONT_DEFAULT | FONT_SHOW_SCROLL);
            break;
        }
        break;
    case ON_CHANGE_RELEASE:
        break;
    }
    return 0;
}
REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_IS_CONNECTED_NETWORK)
.onchange = text_reg_status_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_OPERATOR)
.onchange = text_reg_status_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_IS_IMS)
.onchange = text_reg_status_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_IS_PDP)
.onchange = text_reg_status_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_IP)
.onchange = text_reg_status_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

//手动搜网
static int is_network_search_start = 0;
static int vlist_reg_status_ontouch(void *ctr, struct element_touch_event *e)
{
    struct ui_grid *grid = (struct ui_grid *)ctr;
    static u8 touch_action = 0;
    int sel_item;
    extern int32_t handleRadioCallback(void *p_param, uint32_t size);

    switch (e->event) {
    case ELM_EVENT_TOUCH_UP:
        if (touch_action != 1) {
            break;
        }

        sel_item = ui_grid_cur_item(grid);
        printf("sel_item:%d\n", sel_item);
        if (sel_item <= 0) {
            break;
        }
        switch (sel_item) {
        case 4:
            if (is_network_search_start == 0) {
                printf("network search!");
                aic_srv_tele_radio_start_search(0, handleRadioCallback);   //í¨1yUI_MSG_POST??è?
                is_network_search_start = 1;
            }
            break;
        }
        break;
    case ELM_EVENT_TOUCH_HOLD:
        break;
    case ELM_EVENT_TOUCH_MOVE:
        touch_action = 2;
        break;
    case ELM_EVENT_TOUCH_DOWN:
        touch_action = 1;
        break;
    case ELM_EVENT_TOUCH_U_MOVE:
        break;
    case ELM_EVENT_TOUCH_D_MOVE:
        break;
    }
    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_VLIST_REG_STATUS)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = vlist_reg_status_ontouch,
};

static int layout_network_search_ontouch(void *ctr, struct element_touch_event *e)
{
    struct layout *layout = (struct layout *)ctr;

    switch (e->event) {
    case ELM_EVENT_TOUCH_R_MOVE:
        ui_hide(CAT1_LAYOUT_NETWORK_SEARCH);
        ui_show(CAT1_LAYOUT_REG_STATUS);
        return true;
        break;
    default:
        break;
    }

    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_LAYOUT_NETWORK_SEARCH)//一级菜单
.onchange = NULL,
 .onkey = NULL,
  .ontouch = layout_network_search_ontouch,
};

char *network_status[4] = {"UNKNOW", "AVAILABLE", "CURRENT", "FORBIDDEN"};
static int text_network_search_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct ui_text *text = (struct ui_text *)_ctrl;
    extern ts_radio_operator_info_t operator_info[4];

    switch (event) {
    case ON_CHANGE_INIT:
        switch (text->elm.id) {
        case CAT1_NETWORK_NAME:
            ui_text_set_text_attrs(text, operator_info[0].operator_long_name, strlen(operator_info[0].operator_long_name), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;

        case CAT1_NETWORK_MCC:
            ui_text_set_text_attrs(text, operator_info[0].plmn_mcc, strlen(operator_info[0].plmn_mcc), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;

        case CAT1_NETWORK_MNC:
            ui_text_set_text_attrs(text, operator_info[0].plmn_mnc, strlen(operator_info[0].plmn_mnc), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;

        case CAT1_NETWORK_TYPE:
            ui_text_set_text_attrs(text, network_type[operator_info[0].network_type], strlen(network_type[operator_info[0].network_type]), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;

        case CAT1_NETWORK_STATUS:
            ui_text_set_text_attrs(text, network_status[operator_info[0].status], strlen(network_status[operator_info[0].status]), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_NETWORK_NAME1:
            ui_text_set_text_attrs(text, operator_info[1].operator_long_name, strlen(operator_info[1].operator_long_name), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;

        case CAT1_NETWORK_MCC1:
            ui_text_set_text_attrs(text, operator_info[1].plmn_mcc, strlen(operator_info[1].plmn_mcc), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;

        case CAT1_NETWORK_MNC1:
            ui_text_set_text_attrs(text, operator_info[1].plmn_mnc, strlen(operator_info[1].plmn_mnc), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;

        case CAT1_NETWORK_TYPE1:
            ui_text_set_text_attrs(text, network_type[operator_info[1].network_type], strlen(network_type[operator_info[1].network_type]), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;

        case CAT1_NETWORK_STATUS1:
            ui_text_set_text_attrs(text, network_status[operator_info[1].status], strlen(network_status[operator_info[1].status]), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_NETWORK_NAME2:
            ui_text_set_text_attrs(text, operator_info[2].operator_long_name, strlen(operator_info[2].operator_long_name), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;

        case CAT1_NETWORK_MCC2:
            ui_text_set_text_attrs(text, operator_info[2].plmn_mcc, strlen(operator_info[2].plmn_mcc), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;

        case CAT1_NETWORK_MNC2:
            ui_text_set_text_attrs(text, operator_info[2].plmn_mnc, strlen(operator_info[2].plmn_mnc), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;

        case CAT1_NETWORK_TYPE2:
            ui_text_set_text_attrs(text, network_type[operator_info[2].network_type], strlen(network_type[operator_info[2].network_type]), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;

        case CAT1_NETWORK_STATUS2:
            ui_text_set_text_attrs(text, network_status[operator_info[2].status], strlen(network_status[operator_info[2].status]), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_NETWORK_NAME3:
            ui_text_set_text_attrs(text, operator_info[3].operator_long_name, strlen(operator_info[3].operator_long_name), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;

        case CAT1_NETWORK_MCC3:
            ui_text_set_text_attrs(text, operator_info[3].plmn_mcc, strlen(operator_info[3].plmn_mcc), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;

        case CAT1_NETWORK_MNC3:
            ui_text_set_text_attrs(text, operator_info[3].plmn_mnc, strlen(operator_info[3].plmn_mnc), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;

        case CAT1_NETWORK_TYPE3:
            ui_text_set_text_attrs(text, network_type[operator_info[3].network_type], strlen(network_type[operator_info[3].network_type]), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;

        case CAT1_NETWORK_STATUS3:
            ui_text_set_text_attrs(text, network_status[operator_info[3].status], strlen(network_status[operator_info[3].status]), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        }
        break;
    case ON_CHANGE_SHOW:

        break;
    case ON_CHANGE_RELEASE:
        is_network_search_start = 0;
        break;
    }
    return 0;
}
REGISTER_UI_EVENT_HANDLER(CAT1_NETWORK_NAME)
.onchange = text_network_search_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_NETWORK_MCC)
.onchange = text_network_search_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_NETWORK_MNC)
.onchange = text_network_search_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_NETWORK_TYPE)
.onchange = text_network_search_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_NETWORK_STATUS)
.onchange = text_network_search_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};
REGISTER_UI_EVENT_HANDLER(CAT1_NETWORK_NAME1)
.onchange = text_network_search_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_NETWORK_MCC1)
.onchange = text_network_search_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_NETWORK_MNC1)
.onchange = text_network_search_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_NETWORK_TYPE1)
.onchange = text_network_search_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_NETWORK_STATUS1)
.onchange = text_network_search_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};
REGISTER_UI_EVENT_HANDLER(CAT1_NETWORK_NAME2)
.onchange = text_network_search_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_NETWORK_MCC2)
.onchange = text_network_search_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_NETWORK_MNC2)
.onchange = text_network_search_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_NETWORK_TYPE2)
.onchange = text_network_search_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_NETWORK_STATUS2)
.onchange = text_network_search_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};
REGISTER_UI_EVENT_HANDLER(CAT1_NETWORK_NAME3)
.onchange = text_network_search_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_NETWORK_MCC3)
.onchange = text_network_search_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_NETWORK_MNC3)
.onchange = text_network_search_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_NETWORK_TYPE3)
.onchange = text_network_search_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_NETWORK_STATUS3)
.onchange = text_network_search_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

//-----------------------------tele-网络上下行速率------------------------//
static int layout_network_rate_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct layout *layout = (struct layout *)_ctrl;

    switch (event) {
    case ON_CHANGE_INIT:

        break;
    case ON_CHANGE_RELEASE:
#if TCFG_CAT1_FUNC_NET_ENABLE
        os_task_del("network_rate_task");
        os_task_del("tcp_client");
        is_network_rate_run = 0;
#endif
        break;
    }
    return 0;
}

static int layout_network_rate_ontouch(void *ctr, struct element_touch_event *e)
{
    struct layout *layout = (struct layout *)ctr;

    switch (e->event) {
    case ELM_EVENT_TOUCH_R_MOVE:
        ui_hide(CAT1_LAYOUT_NETWORK_RATE);
        ui_show(CAT1_LAYOUT_TELE);
        return true;
        break;
    default:
        break;
    }

    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_LAYOUT_NETWORK_RATE)//一级菜单
.onchange = layout_network_rate_onchange,
 .onkey = NULL,
  .ontouch = layout_network_rate_ontouch,
};

#if TCFG_CAT1_FUNC_NET_ENABLE
char dl_rate[7] = {0};
char ul_rate[7] = {0};
static int text_network_rate_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct ui_text *text = (struct ui_text *)_ctrl;

    switch (event) {
    case ON_CHANGE_INIT:
        switch (text->elm.id) {
        case CAT1_TEXT_NETWORK_UPSTREAM_RATE:
            sprintf(ul_rate, "%.2f", network_rate_t.ul_rate);
            ui_text_set_text_attrs(text, ul_rate, strlen(ul_rate), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_NETWORK_DOWNLINK_RATE:
            sprintf(dl_rate, "%.2f", network_rate_t.dl_rate);
            ui_text_set_text_attrs(text, dl_rate, strlen(dl_rate), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        }
        break;
    case ON_CHANGE_RELEASE:
        break;
    }
    return 0;
}
REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NETWORK_UPSTREAM_RATE)
.onchange = text_network_rate_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NETWORK_DOWNLINK_RATE)
.onchange = text_network_rate_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};
#endif
//-----------------------------tele-call设置-----------------------------//
static int layout_call_setting_ontouch(void *ctr, struct element_touch_event *e)
{
    struct layout *layout = (struct layout *)ctr;

    switch (e->event) {
    case ELM_EVENT_TOUCH_R_MOVE:
        ui_hide(CAT1_LAYOUT_CALL_SETTING);
        ui_show(CAT1_LAYOUT_TELE);
        return true;
        break;
    default:
        break;
    }

    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_LAYOUT_CALL_SETTING)//一级菜单
.onchange = NULL,
 .onkey = NULL,
  .ontouch = layout_call_setting_ontouch,
};

int is_auto_answer = 0;
static int pic_auto_answer_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct ui_pic *pic = (struct ui_pic *)_ctrl;

    switch (event) {
    case ON_CHANGE_INIT:
        switch (pic->elm.id) {
        case CAT1_PIC_AUTO_ANSWER_SEL:
            ui_pic_set_image_index(pic, !!is_auto_answer);
            break;
        default:
            return false;
        }

        break;
    case ON_CHANGE_HIGHLIGHT:
        break;
    }
    return FALSE;
}

static int pic_auto_answer_ontouch(void *ctr, struct element_touch_event *e)
{
    struct ui_pic *pic = (struct ui_pic *)ctr;
    static u8 touch_action = 0;

    switch (e->event) {
    case ELM_EVENT_TOUCH_UP:
        if (touch_action != 1) {
            break;
        }

        switch (pic->elm.id) {
        case CAT1_PIC_AUTO_ANSWER_SEL:
            is_auto_answer = !is_auto_answer;
            ui_pic_show_image_by_id(pic->elm.id, !!is_auto_answer);//通过索引显示图片

            //接口调用
            if (is_auto_answer == 0) {
                printf("auto answer close!\n");
                aic_srv_tele_call_set_auto_answer(false);
            } else {
                printf("auto answer open!\n");
                aic_srv_tele_call_set_auto_answer(true);
            }

            break;
        default:
            return false;
        }

        break;
    case ELM_EVENT_TOUCH_HOLD:
        break;
    case ELM_EVENT_TOUCH_MOVE:
        touch_action = 2;
        break;
    case ELM_EVENT_TOUCH_DOWN:
        touch_action = 1;
        return true;
    case ELM_EVENT_TOUCH_U_MOVE:
        break;
    case ELM_EVENT_TOUCH_D_MOVE:
        break;
    }

    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_PIC_AUTO_ANSWER_SEL)
.onchange = pic_auto_answer_onchange,
 .onkey = NULL,
  .ontouch = pic_auto_answer_ontouch,
};


static int text_call_ims_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct ui_text *text = (struct ui_text *)_ctrl;
    char *is_call_ims_str = malloc(sizeof(char) * 2);

    switch (event) {
    case ON_CHANGE_INIT:
        switch (text->elm.id) {
        case CAT1_TEXT_CALL_IMS:
            int is_call_ims = aic_srv_tele_call_have_volte_reg(0);
            sprintf(is_call_ims_str, "%d", is_call_ims);
            ui_text_set_text_attrs(text, is_call_ims_str, strlen(is_call_ims_str), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        }
        break;
    case ON_CHANGE_RELEASE:
        break;
    }
    return 0;
}
REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_CALL_IMS)
.onchange = text_call_ims_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

static int pic_voice_loopback_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct ui_pic *pic = (struct ui_pic *)_ctrl;
    int is_voice_loopback = aic_srv_voice_is_enable_loopback();

    switch (event) {
    case ON_CHANGE_INIT:
        switch (pic->elm.id) {
        case CAT1_PIC_VOICE_LOOPBACK_SEL:
            ui_pic_set_image_index(pic, !!is_voice_loopback);
            break;
        default:
            return false;
        }

        break;
    case ON_CHANGE_HIGHLIGHT:
        break;
    }
    return FALSE;
}

static int pic_voice_loopback_ontouch(void *ctr, struct element_touch_event *e)
{
    struct ui_pic *pic = (struct ui_pic *)ctr;
    static u8 touch_action = 0;
    int is_voice_loopback = aic_srv_voice_is_enable_loopback();

    switch (e->event) {
    case ELM_EVENT_TOUCH_UP:
        if (touch_action != 1) {
            break;
        }

        switch (pic->elm.id) {
        case CAT1_PIC_VOICE_LOOPBACK_SEL:
            is_voice_loopback = !is_voice_loopback;
            ui_pic_show_image_by_id(pic->elm.id, !!is_voice_loopback);//通过索引显示图片

            //接口调用
            if (is_voice_loopback == 0) {
                printf("voice loopback disable!\n");
                aic_srv_voice_enable_loopback(false);
            } else {
                printf("voice loopback enable!\n");
                aic_srv_voice_enable_loopback(true);
            }

            break;
        default:
            return false;
        }

        break;
    case ELM_EVENT_TOUCH_HOLD:
        break;
    case ELM_EVENT_TOUCH_MOVE:
        touch_action = 2;
        break;
    case ELM_EVENT_TOUCH_DOWN:
        touch_action = 1;
        return true;
    case ELM_EVENT_TOUCH_U_MOVE:
        break;
    case ELM_EVENT_TOUCH_D_MOVE:
        break;
    }

    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_PIC_VOICE_LOOPBACK_SEL)//volte开关
.onchange = pic_voice_loopback_onchange,
 .onkey = NULL,
  .ontouch = pic_voice_loopback_ontouch,
};

int is_call_setting = 0;
ts_call_dial_info_t engineering_dial_info = {0};
static int vlist_call_setting_ontouch(void *ctr, struct element_touch_event *e)
{
    struct ui_grid *grid = (struct ui_grid *)ctr;
    static u8 touch_action = 0;
    int sel_item;

    switch (e->event) {
    case ELM_EVENT_TOUCH_UP:
        if (touch_action != 1) {
            break;
        }

        sel_item = ui_grid_cur_item(grid);
        printf("sel_item:%d\n", sel_item);
        if (sel_item <= 0) {
            break;
        }
        switch (sel_item) {
        case 4:
            printf("dial!!\n");
            ui_hide(CAT1_LAYOUT_CALL_SETTING);
            ui_show(CAT1_LAYOUT_DIAL_NUMBER);
            is_call_setting = 1;
            break;
        }
        break;
    case ELM_EVENT_TOUCH_HOLD:
        break;
    case ELM_EVENT_TOUCH_MOVE:
        touch_action = 2;
        break;
    case ELM_EVENT_TOUCH_DOWN:
        touch_action = 1;
        break;
    case ELM_EVENT_TOUCH_U_MOVE:
        break;
    case ELM_EVENT_TOUCH_D_MOVE:
        break;
    }
    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_VLIST_CALL_SETTING)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = vlist_call_setting_ontouch,
};

//---------------------------tele-sms设置------------------------------//
int is_sms_setting = 0;
static int layout_sms_setting_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct ui_grid *grid = (struct ui_grid *)_ctrl;

    switch (event) {
    case ON_CHANGE_INIT:
        is_sms_layout = 1;
        break;
    case ON_CHANGE_RELEASE:
        is_sms_layout = 0;
        break;
    }
    return 0;
}

static int layout_sms_setting_ontouch(void *ctr, struct element_touch_event *e)
{
    struct layout *layout = (struct layout *)ctr;

    switch (e->event) {
    case ELM_EVENT_TOUCH_R_MOVE:
        ui_hide(CAT1_LAYOUT_SMS_SETTING);
        ui_show(CAT1_LAYOUT_TELE);
        return true;
        break;
    default:
        break;
    }

    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_LAYOUT_SMS_SETTING)//一级菜单
.onchange = layout_sms_setting_onchange,
 .onkey = NULL,
  .ontouch = layout_sms_setting_ontouch,
};

static int vlist_sms_ontouch(void *ctr, struct element_touch_event *e)
{
    struct ui_grid *grid = (struct ui_grid *)ctr;
    static u8 touch_action = 0;
    int sel_item;
    extern int32_t handleSmsCallback(void *p_param, uint32_t size);

    switch (e->event) {
    case ELM_EVENT_TOUCH_UP:
        if (touch_action != 1) {
            break;
        }

        sel_item = ui_grid_cur_item(grid);
        printf("sel_item:%d\n", sel_item);
        if (sel_item <= 0) {
            break;
        }
        switch (sel_item) {
        case 1:
            aic_srv_tele_sms_register(NULL, handleSmsCallback);                //收单条短信
            break;
        case 2:
            printf("send message!\n");
            is_sms_setting = 1;
            ui_hide(CAT1_LAYOUT_SMS_SETTING);
            ui_show(CAT1_LAYOUT_DIAL_NUMBER);
            break;
        case 3:
            printf("get smsc!\n");
            aic_srv_tele_sms_get_smsc(0, handleSmsCallback);        //获取短信中心号码
            break;
        case 5:
            ui_hide(CAT1_LAYOUT_SMS_SETTING);
            ui_show(CAT1_LAYOUT_SMS_SHOW);
            break;
        }
        break;
    case ELM_EVENT_TOUCH_HOLD:
        break;
    case ELM_EVENT_TOUCH_MOVE:
        touch_action = 2;
        break;
    case ELM_EVENT_TOUCH_DOWN:
        touch_action = 1;
        break;
    case ELM_EVENT_TOUCH_U_MOVE:
        break;
    case ELM_EVENT_TOUCH_D_MOVE:
        break;
    }
    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_VLIST_SMS_SETTING)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = vlist_sms_ontouch,
};

static int text_engineering_sms_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct ui_text *text = (struct ui_text *)_ctrl;
    extern struct s_cat1_sms_info cat1_sms_info;

    switch (event) {
    case ON_CHANGE_INIT:
        switch (text->elm.id) {
        case CAT1_ENGINEERING_SMS_NUMBER:
            ui_text_set_text_attrs(text, cat1_get_send_sms_number(), strlen(cat1_get_send_sms_number()), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_ENGINEERING_SMS_CONTENT:
            ui_text_set_text_attrs(text, cat1_get_send_sms_content(), strlen(cat1_get_send_sms_content()), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        }
        break;
    case ON_CHANGE_RELEASE:
        break;
    }
    return 0;
}
REGISTER_UI_EVENT_HANDLER(CAT1_ENGINEERING_SMS_NUMBER)
.onchange = text_engineering_sms_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_ENGINEERING_SMS_CONTENT)
.onchange = text_engineering_sms_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

static int layout_sms_show_ontouch(void *ctr, struct element_touch_event *e)
{
    struct layout *layout = (struct layout *)ctr;

    switch (e->event) {
    case ELM_EVENT_TOUCH_R_MOVE:
        ui_hide(CAT1_LAYOUT_SMS_SHOW);
        ui_show(CAT1_LAYOUT_SMS_SETTING);
        return true;
        break;
    default:
        break;
    }

    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_LAYOUT_SMS_SHOW)//一级菜单
.onchange = NULL,
 .onkey = NULL,
  .ontouch = layout_sms_show_ontouch,
};

//----------------------工模数字输入--------------------------------//
struct factory_mode_number {
    char number[20];
    int index;
};
struct factory_mode_number factory_mode_number_t = {0};

static void factory_mode_add_number(char ch)
{
    static struct unumber n;

    factory_mode_number_t.number[factory_mode_number_t.index++] = ch;
    factory_mode_number_t.number[factory_mode_number_t.index] = '\0';
    n.type = TYPE_STRING;
    n.num_str = (u8 *)factory_mode_number_t.number;
    /* g_printf("%s",n.num_str); */
    ui_number_update_by_id(CAT1_TEXT_SHOW_NUMBER, &n);
}

static void factory_mode_remove_number()
{
    static struct unumber n;

    if (factory_mode_number_t.index <= 0) {
        return;
    }
    factory_mode_number_t.number[--factory_mode_number_t.index] = '\0';
    n.type = TYPE_STRING;
    n.num_str = (u8 *)factory_mode_number_t.number;
    /* r_printf("%s",n.num_str); */
    ui_number_update_by_id(CAT1_TEXT_SHOW_NUMBER, &n);
}

bool aic_test_get_is_from_engineering_mode(void)
{
    return g_is_from_engineering_mode;
}

void aic_test_set_is_from_engineering_mode(bool flag)
{
    printf("[%s]flag = %d.", __func__, flag);
    g_is_from_engineering_mode = flag;
}

static int factory_mode_number_info_ontouch(void *ctrl, struct element_touch_event *e)
{
    struct element *elm = (struct element *)ctrl;
    extern int32_t handleCallCallback(void *p_param, uint32_t size);
    extern int32_t handleSmsCallback(void *p_param, uint32_t size);

    switch (e->event) {
    case ELM_EVENT_TOUCH_DOWN:
        switch (elm->id) {
        case CAT1_NUM_DEL:
            printf("[5]CAT1_NUMBER_DEL DOWN\n");
            break;
        case CAT1_NUM_0:
            printf("[5]CAT1_NUMBER_0 DOWN\n");
            break;
        case CAT1_NUM_1:
            printf("[5]CAT1_NUMBER_1 DOWN\n");
            break;
        case CAT1_NUM_2:
            printf("[5]CAT1_NUMBER_2 DOWN\n");
            break;
        case CAT1_NUM_3:
            printf("[5]CAT1_NUMBER_3 DOWN\n");
            break;
        case CAT1_NUM_4:
            printf("[5]CAT1_NUMBER_4 DOWN\n");
            break;
        case CAT1_NUM_5:
            printf("[5]CAT1_NUMBER_5 DOWN\n");
            break;
        case CAT1_NUM_6:
            printf("[5]CAT1_NUMBER_6 DOWN\n");
            break;
        case CAT1_NUM_7:
            printf("[5]CAT1_NUMBER_7 DOWN\n");
            break;
        case CAT1_NUM_8:
            printf("[5]CAT1_NUMBER_8 DOWN\n");
            break;
        case CAT1_NUM_9:
            printf("[5]CAT1_NUMBER_9 DOWN\n");
            break;
        case CAT1_NUM_SURE:
            printf("[5]CAT1_NUMBER_SURE DOWN\n");
            break;
        default:
            break;
        }
        /* ui_core_highlight_element(elm, true); */
        ui_core_redraw(elm);
        return true;
    case ELM_EVENT_TOUCH_UP:
        switch (elm->id) {
        case CAT1_NUM_DEL:
            printf("[5]CAT1_NUMBER_DEL UP\n");
            if (index == 0) {
                break;
            }
            factory_mode_remove_number();
            break;
        case CAT1_NUM_0:
            printf("[5]CAT1_NUMBER_0 UP\n");
            factory_mode_add_number('0');
            break;
        case CAT1_NUM_1:
            printf("[5]CAT1_NUMBER_1 UP\n");
            factory_mode_add_number('1');
            break;
        case CAT1_NUM_2:
            printf("[5]CAT1_NUMBER_2 UP\n");
            factory_mode_add_number('2');
            break;
        case CAT1_NUM_3:
            printf("[5]CAT1_NUMBER_3 UP\n");
            factory_mode_add_number('3');
            break;
        case CAT1_NUM_4:
            printf("[5]CAT1_NUMBER_4 UP\n");
            factory_mode_add_number('4');
            break;
        case CAT1_NUM_5:
            printf("[5]CAT1_NUMBER_5 UP\n");
            factory_mode_add_number('5');
            break;
        case CAT1_NUM_6:
            printf("[5]CAT1_NUMBER_6 UP\n");
            factory_mode_add_number('6');
            break;
        case CAT1_NUM_7:
            printf("[5]CAT1_NUMBER_7 UP\n");
            factory_mode_add_number('7');
            break;
        case CAT1_NUM_8:
            printf("[5]CAT1_NUMBER_8 UP\n");
            factory_mode_add_number('8');
            break;
        case CAT1_NUM_9:
            printf("[5]CAT1_NUMBER_9 UP\n");
            factory_mode_add_number('9');
            break;
        case CAT1_NUM_SURE:
            //需要用全局变量确认
            if (is_call_setting == 1) {
                printf("dial number!\n");
                printf("number:%s\n", factory_mode_number_t.number);
                memcpy(engineering_dial_info.phone_number, factory_mode_number_t.number, sizeof(factory_mode_number_t.number));
                memset(&factory_mode_number_t, 0, sizeof(factory_mode_number_t));
                is_call_setting = 0;
                aic_srv_tele_call_dial(&engineering_dial_info, handleCallCallback);
                /* AIC_TEST */
                g_is_from_engineering_mode = true;
                printf("[%s]g_is_from_engineering_mode = %d.", __func__, g_is_from_engineering_mode);
                ui_hide(CAT1_LAYOUT_DIAL_NUMBER);
                ui_show(CAT1_LAYOUT_CALL_SETTING);
            } else if (is_sms_setting == 1) {
                printf("send msg!\n");
                printf("number:%s\n", factory_mode_number_t.number);
                ts_sms_send_info_t sms_send_info;
                sms_send_info.p_number = malloc(sizeof(char) * 20);
                sms_send_info.p_smsc = NULL;
                sms_send_info.p_text = "hello";
                sms_send_info.is_write_to_sim = false;
                memcpy(sms_send_info.p_number, factory_mode_number_t.number, sizeof(factory_mode_number_t.number));
                aic_srv_tele_sms_send(0, &sms_send_info, handleSmsCallback);            //发单条信息
                printf("send success!\n");
                memset(&factory_mode_number_t, 0, sizeof(factory_mode_number_t));
                is_sms_setting = 0;
                ui_hide(CAT1_LAYOUT_DIAL_NUMBER);
                ui_show(CAT1_LAYOUT_SMS_SETTING);
            }
            break;
        default:
            break;
        }
        /* ui_core_highlight_element(elm, false); */
        ui_core_redraw(elm);

        break;
    default:
        break;
    }
    return true;
}

REGISTER_UI_EVENT_HANDLER(CAT1_NUM_1)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = factory_mode_number_info_ontouch,
};

REGISTER_UI_EVENT_HANDLER(CAT1_NUM_2)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = factory_mode_number_info_ontouch,
};

REGISTER_UI_EVENT_HANDLER(CAT1_NUM_3)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = factory_mode_number_info_ontouch,
};

REGISTER_UI_EVENT_HANDLER(CAT1_NUM_4)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = factory_mode_number_info_ontouch,
};

REGISTER_UI_EVENT_HANDLER(CAT1_NUM_5)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = factory_mode_number_info_ontouch,
};

REGISTER_UI_EVENT_HANDLER(CAT1_NUM_6)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = factory_mode_number_info_ontouch,
};

REGISTER_UI_EVENT_HANDLER(CAT1_NUM_7)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = factory_mode_number_info_ontouch,
};

REGISTER_UI_EVENT_HANDLER(CAT1_NUM_8)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = factory_mode_number_info_ontouch,
};

REGISTER_UI_EVENT_HANDLER(CAT1_NUM_9)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = factory_mode_number_info_ontouch,
};

REGISTER_UI_EVENT_HANDLER(CAT1_NUM_0)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = factory_mode_number_info_ontouch,
};

REGISTER_UI_EVENT_HANDLER(CAT1_NUM_DEL)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = factory_mode_number_info_ontouch,
};

REGISTER_UI_EVENT_HANDLER(CAT1_NUM_SURE)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = factory_mode_number_info_ontouch,
};

static int factory_mode_number_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct ui_number *number = (struct ui_number *)_ctrl;

    switch (event) {
    case ON_CHANGE_INIT:
        factory_mode_number_t.index = 0;
        factory_mode_number_t.number[factory_mode_number_t.index] = '\0';
        /* ui_number_update(number, dial_info_t.number); */
        number->type = TYPE_STRING;
        number->num_str = (u8 *)factory_mode_number_t.number;
        break;
    case ON_CHANGE_RELEASE:
        break;
    default:
        return FALSE;
    }
    return FALSE;
}

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_SHOW_NUMBER)
.onchange = factory_mode_number_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

//------------------------------电源控制------------------------------//
static int layout_power_ctrl_ontouch(void *ctr, struct element_touch_event *e)
{
    struct layout *layout = (struct layout *)ctr;

    switch (e->event) {
    case ELM_EVENT_TOUCH_R_MOVE:
        ui_hide(CAT1_LAYOUT_POWER_CTRL);
        ui_show(CAT1_LAYOUT_FIRST_MENU);
        return true;
        break;
    default:
        break;
    }

    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_LAYOUT_POWER_CTRL)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = layout_power_ctrl_ontouch,
};

//power on
static int pic_poweron_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct ui_pic *pic = (struct ui_pic *)_ctrl;
    int is_power_on = cat1_ctrl_is_poweron();

    switch (event) {
    case ON_CHANGE_INIT:
        switch (pic->elm.id) {
        case CAT1_PIC_POWERON_SEL:
            ui_pic_set_image_index(pic, !!is_power_on);
            break;
        default:
            return false;
        }

        break;
    case ON_CHANGE_HIGHLIGHT:
        break;
    }
    return FALSE;
}

bool get_engineering_poweronoff_can_touch(void)
{
    return engineering_poweromoff_can_touch;
}

void set_engineering_poweronoff_can_touch(bool flag)
{
    engineering_poweromoff_can_touch = flag;
}

static int pic_poweron_ontouch(void *ctr, struct element_touch_event *e)
{
    if (get_engineering_poweronoff_can_touch() == false) {
        return 0;
    }

    struct ui_pic *pic = (struct ui_pic *)ctr;
    static u8 touch_action = 0;
    int is_power_on = cat1_ctrl_is_poweron();

    switch (e->event) {
    case ELM_EVENT_TOUCH_UP:
        if (touch_action != 1) {
            break;
        }

        switch (pic->elm.id) {
        case CAT1_PIC_POWERON_SEL:
            is_power_on = !is_power_on;
            ui_pic_show_image_by_id(pic->elm.id, !!is_power_on);//通过索引显示图片

            //接口调用
            if (is_power_on == 0) {
                printf("power off!\n");
                cat1_set_lte_onoff(false);
                set_engineering_poweronoff_can_touch(false);
            } else {
                printf("power on!\n");
                cat1_set_lte_onoff(true);
                set_engineering_poweronoff_can_touch(false);
            }

            break;
        default:
            return false;
        }

        break;
    case ELM_EVENT_TOUCH_HOLD:
        break;
    case ELM_EVENT_TOUCH_MOVE:
        touch_action = 2;
        break;
    case ELM_EVENT_TOUCH_DOWN:
        touch_action = 1;
        return true;
    case ELM_EVENT_TOUCH_U_MOVE:
        break;
    case ELM_EVENT_TOUCH_D_MOVE:
        break;
    }

    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_PIC_POWERON_SEL)
.onchange = pic_poweron_onchange,
 .onkey = NULL,
  .ontouch = pic_poweron_ontouch,
};

static int vlist_power_ctrl_ontouch(void *ctr, struct element_touch_event *e)
{
    struct ui_grid *grid = (struct ui_grid *)ctr;
    static u8 touch_action = 0;
    int sel_item;

    switch (e->event) {
    case ELM_EVENT_TOUCH_UP:
        if (touch_action != 1) {
            break;
        }

        sel_item = ui_grid_cur_item(grid);
        printf("sel_item:%d\n", sel_item);
        if (sel_item <= 0) {
            break;
        }
        switch (sel_item) {
        case 2:
            printf("reset!!\n");
            aic_ctrl_reset();
            break;
        case 3:
            printf("assert!!\n");
            aic_ctrl_assert();
            break;
        }
        break;
    case ELM_EVENT_TOUCH_HOLD:
        break;
    case ELM_EVENT_TOUCH_MOVE:
        touch_action = 2;
        break;
    case ELM_EVENT_TOUCH_DOWN:
        touch_action = 1;
        break;
    case ELM_EVENT_TOUCH_U_MOVE:
        break;
    case ELM_EVENT_TOUCH_D_MOVE:
        break;
    }
    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_VLIST_POWER_CTRL)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = vlist_power_ctrl_ontouch,
};


//------------------------------wifi scan----------------------------//
static int layout_wifi_scan_ontouch(void *ctr, struct element_touch_event *e)
{
    struct layout *layout = (struct layout *)ctr;

    switch (e->event) {
    case ELM_EVENT_TOUCH_R_MOVE:
        ui_hide(CAT1_LAYOUT_WIFI_SCAN);
        ui_show(CAT1_LAYOUT_FIRST_MENU);
        return true;
        break;
    default:
        break;
    }

    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_LAYOUT_WIFI_SCAN)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = layout_wifi_scan_ontouch,
};

static int vlist_wifi_scan_ontouch(void *ctr, struct element_touch_event *e)
{
    struct ui_grid *grid = (struct ui_grid *)ctr;
    static u8 touch_action = 0;
    int sel_item;

    switch (e->event) {
    case ELM_EVENT_TOUCH_UP:
        if (touch_action != 1) {
            break;
        }

        sel_item = ui_grid_cur_item(grid);
        printf("sel_item:%d\n", sel_item);
        if (sel_item <= 0) {
            break;
        }
        switch (sel_item) {
        case 0:
            ui_hide(CAT1_LAYOUT_WIFI_SCAN);
            ui_show(CAT1_LAYOUT_WIFI_SCAN_INFO);
            break;
        case 1:
            ui_hide(CAT1_LAYOUT_WIFI_SCAN);
            ui_show(CAT1_LAYOUT_SINGLE_CHANNEL_SEARCH);
            break;
        }
        break;
    case ELM_EVENT_TOUCH_HOLD:
        break;
    case ELM_EVENT_TOUCH_MOVE:
        touch_action = 2;
        break;
    case ELM_EVENT_TOUCH_DOWN:
        touch_action = 1;
        break;
    case ELM_EVENT_TOUCH_U_MOVE:
        break;
    case ELM_EVENT_TOUCH_D_MOVE:
        break;
    }
    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_VLIST_WIFI_SCAN)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = vlist_wifi_scan_ontouch,
};


//-----------------------log设置--------------------------//
static int layout_log_ontouch(void *ctr, struct element_touch_event *e)
{
    struct layout *layout = (struct layout *)ctr;

    switch (e->event) {
    case ELM_EVENT_TOUCH_R_MOVE:
        ui_hide(CAT1_LAYOUT_LOG);
        ui_show(CAT1_LAYOUT_FIRST_MENU);
        return true;
        break;
    default:
        break;
    }

    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_LAYOUT_LOG)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = layout_log_ontouch,
};

static int vlist_log_ontouch(void *ctr, struct element_touch_event *e)
{
    struct ui_grid *grid = (struct ui_grid *)ctr;
    static u8 touch_action = 0;
    int sel_item;

    switch (e->event) {
    case ELM_EVENT_TOUCH_UP:
        if (touch_action != 1) {
            break;
        }

        sel_item = ui_grid_cur_item(grid);
        printf("sel_item:%d\n", sel_item);
        if (sel_item <= 0) {
            break;
        }
        switch (sel_item) {
        case 1://log none
            printf("log none\n");
            alog_set_level(ALOG_LEVEL_NONE);
            break;
        case 2://log error
            printf("log error\n");
            alog_set_level(ALOG_LEVEL_ERROR);
            break;
        case 3://log warning
            printf("log warning\n");
            alog_set_level(ALOG_LEVEL_WARNING);
            break;
        case 4://log info
            printf("log info\n");
            alog_set_level(ALOG_LEVEL_INFO);
            break;
        case 5://log
            printf("log debug\n");
            alog_set_level(ALOG_LEVEL_DEBUG);
            break;
        }
        break;
    case ELM_EVENT_TOUCH_HOLD:
        break;
    case ELM_EVENT_TOUCH_MOVE:
        touch_action = 2;
        break;
    case ELM_EVENT_TOUCH_DOWN:
        touch_action = 1;
        break;
    case ELM_EVENT_TOUCH_U_MOVE:
        break;
    case ELM_EVENT_TOUCH_D_MOVE:
        break;
    }
    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_VLIST_LOG)//一?恫说?
.onchange = NULL,
 .onkey = NULL,
  .ontouch = vlist_log_ontouch,
};


#endif // TCFG_CAT1_AICXTEK_ENABLE

