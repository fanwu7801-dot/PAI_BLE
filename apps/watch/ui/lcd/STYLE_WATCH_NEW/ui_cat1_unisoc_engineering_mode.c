#include "app_config.h"
#include "ui/ui_style.h"
#include "ui/ui.h"
#include "ui/ui_api.h"
#include "app_task.h"
#include "system/timer.h"
#include "device/device.h"
#include "key_event_deal.h"
#include "res/resfile.h"
#include "ui/res_config.h"
#include "ui/ui_resource.h"
#include "system/includes.h"
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
#include "ual_gnss_export.h"
#include "ual_wifi_export.h"
#include "ual_rf_am_export.h"
#include "ual_tele_radio_export.h"
#include "cat1/cat1_common.h"
#include "modem_ctrl_api.h"
#include "unisoc_at.h"

#if TCFG_CAT1_UNISOC_ENABLE


#define STYLE_NAME  JL

#define ui_grid_for_id(id) \
	({ \
		struct element *elm = ui_core_get_element_by_id(id); \
	 	elm ? container_of(elm, struct ui_grid, elm): NULL; \
	 })

//******************全局变量****************************//
//键盘全局变量
int is_lock_frequency = 0;
int is_set_ims_index = 0;
int is_check_ims = 0;
int is_set_ims_value = 0;
int is_supply_service_call_forward = 0;
int is_supply_service_unconditional_call_forward = 0;

//ims
uint8 ims_nv_value[UAL_TELE_RADIO_IMS_NV_PARAM_MAX_LEN];
int ims_index = 0;
int ims_set_value = 0;

//AT
char call_wait_str[30] = "AT+CCWA=";
char call_forward_str[30] = "AT+CCFC=";
char unconditional_call_forward[30] = "AT+CCFC=0,3,\"";

//cell info显示全局变量
int is_cell_info = 0;
int is_neigh_cell_info = 0;
//**************************************************************//

static void wifi_show_handler()
{
    extern int is_enter_wifi_test;
    if (is_enter_wifi_test == 0) {
        ui_hide(CAT1_LAYOUT_WIFI_FUNC_TEST_SEARCHING);
        ui_show(CAT1_LAYOUT_WIFI_FUNC_TEST);
        is_enter_wifi_test = 1;
    }
    printf("ui_show_wifi\n");
    ui_redraw(CAT1_LAYOUT_WIFI_FUNC_TEST);                   //重刷ui
}

static void gps_show_handler()
{
    extern int is_enter_gnss_test;
    if (is_enter_gnss_test == 0) {
        ui_hide(CAT1_LAYOUT_GPS_TEST_SEARCHING);
        ui_show(CAT1_LAYOUT_GPS_TEST);
        is_enter_gnss_test = 1;
    }
    printf("ui_show_gps\n");
    ui_redraw(CAT1_LAYOUT_GPS_TEST);
}

static void cell_info_show_handler()
{
    if (is_cell_info == 1) {
        printf("ui_show_cell_info\n");
        ui_hide(CAT1_LAYOUT_CELLINFO);
        ui_show(CAT1_LAYOUT_SERVING_CELL);
        is_cell_info = 0;
    }
}

static void neigh_cell_info_show_handler()
{
    if (is_neigh_cell_info == 1) {
        printf("ui_show_neigh_cell_info\n");
        ui_hide(CAT1_LAYOUT_CELLINFO);
        ui_show(CAT1_LAYOUT_THE_AJACENT_CELL);
        is_neigh_cell_info = 0;
    }
}


static const struct uimsg_handl ui_msg_show_handler[] = {              //注册消息池
    { "wifi_show",        wifi_show_handler     },
    { "gps_show",     gps_show_handler     },
    { "cell_info_show",     cell_info_show_handler     },
    { "neigh_cell_info_show",     neigh_cell_info_show_handler     },
    { NULL, NULL},      /* 必须以此结尾！ */
};


//----------------------------一级菜单----------------------------//
static int communication_page_onchange(void *ctr, enum element_change_event e, void *arg)
{
    struct window *window = (struct window *)ctr;
    switch (e) {
    case ON_CHANGE_INIT:
        puts("\n***communication_page_onchange***\n");
        ui_register_msg_handler(PAGE_110, ui_msg_show_handler);//注册消息交互的回调
        break;
    default:
        return false;
    }
    return false;
}
REGISTER_UI_EVENT_HANDLER(PAGE_110)//一级菜单
.onchange = communication_page_onchange,
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
        case 1://TELEPHONEY
            ui_hide(UOS_CAT1_LAYOUT_FIRST_MENU);
            ui_show(CAT1_LAYOUT_TELEPHONY);
            break;
        case 2://modem ctrl
            ui_hide(UOS_CAT1_LAYOUT_FIRST_MENU);
            ui_show(CAT1_LAYOUT_MODEM_CTRL);
            break;
        case 3://CFT result
            ui_hide(UOS_CAT1_LAYOUT_FIRST_MENU);
            ui_show(CAT1_LAYOUT_CFT_RESULT);
            break;
        case 4://about
            ui_hide(UOS_CAT1_LAYOUT_FIRST_MENU);
            ui_show(CAT1_LAYOUT_ABOUT_LTE);
            break;
        case 5://仲裁模块
            ui_hide(UOS_CAT1_LAYOUT_FIRST_MENU);
            ui_show(CAT1_LAYOUT_ARBITRATION);
            break;
        case 6://GNSS
            ui_hide(UOS_CAT1_LAYOUT_FIRST_MENU);
            ui_show(CAT1_LAYOUT_GNSS);
            break;
        case 7://wifi scan
            ui_hide(UOS_CAT1_LAYOUT_FIRST_MENU);
            ui_show(UOS_CAT1_LAYOUT_WIFI_SCAN);
            break;
        case 9://睡眠模式
            ui_hide(UOS_CAT1_LAYOUT_FIRST_MENU);
            ui_show(CAT1_LAYOUT_SLEEP_MODE);
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
REGISTER_UI_EVENT_HANDLER(UOS_CAT1_VLIST_FIRST_MENU)//一级菜单
.onchange = first_menu_onchange,
 .onkey = NULL,
  .ontouch = first_menu_ontouch,
};


//--------------------一级菜单-TELEPHONY-----------------//
static int layout_telephony_ontouch(void *ctr, struct element_touch_event *e)
{
    struct layout *layout = (struct layout *)ctr;

    switch (e->event) {
    case ELM_EVENT_TOUCH_R_MOVE:
        LAYOUT_SCREEN_DISP;
        ui_hide(CAT1_LAYOUT_TELEPHONY);
        ui_show(UOS_CAT1_LAYOUT_FIRST_MENU);
        return true;
        break;
    default:
        break;
    }

    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_LAYOUT_TELEPHONY)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = layout_telephony_ontouch,
};

static int telephony_ontouch(void *ctr, struct element_touch_event *e)
{
    struct ui_grid *grid = (struct ui_grid *)ctr;
    static u8 touch_action = 0;
    int sel_item;
    char responde[5];
    int responde_len;

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
        case 1://network
            ui_hide(CAT1_LAYOUT_TELEPHONY);
            ui_show(CAT1_LAYOUT_NETWORK);
            break;
        case 2://VOLTE
            ui_hide(CAT1_LAYOUT_TELEPHONY);
            ui_show(CAT1_LAYOUT_VOLTE);
            break;
        case 3://lock frequency
            ui_hide(CAT1_LAYOUT_TELEPHONY);
            ui_show(CAT1_LAYOUT_LOCK_FREQUENCY);
            break;
        case 4://band select
            ui_hide(CAT1_LAYOUT_TELEPHONY);
            ui_show(CAT1_LAYOUT_BANDSELECT);
            break;
        case 5://补充业务
            ui_hide(CAT1_LAYOUT_TELEPHONY);
            ui_show(CAT1_LAYOUT_SUPPLY_SERVICE);
            break;
        case 6://呼叫转移设置
            printf("call forwarding\n");
            is_supply_service_unconditional_call_forward = 1;
            ui_hide(CAT1_LAYOUT_TELEPHONY);
            ui_show(CAT1_LAYOUT_FACTORY_MODE_NUMBER);
            break;
        case 7:     //LTE状态
            ui_hide(CAT1_LAYOUT_TELEPHONY);
            ui_show(CAT1_LAYOUT_LTE_STATUS);
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
REGISTER_UI_EVENT_HANDLER(CAT1_VLIST_TELEPHONY)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = telephony_ontouch,
};

//--------------一级菜单-TELEPHONY-NETWORK----------------//
static int layout_network_ontouch(void *ctr, struct element_touch_event *e)
{
    struct layout *layout = (struct layout *)ctr;

    switch (e->event) {
    case ELM_EVENT_TOUCH_R_MOVE:
        LAYOUT_SCREEN_DISP;
        ui_hide(CAT1_LAYOUT_NETWORK);
        ui_show(CAT1_LAYOUT_TELEPHONY);
        return true;
        break;
    default:
        break;
    }

    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_LAYOUT_NETWORK)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = layout_network_ontouch,
};

static int network_ontouch(void *ctr, struct element_touch_event *e)
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
        case 1://cell info
            ui_hide(CAT1_LAYOUT_NETWORK);
            ui_show(CAT1_LAYOUT_CELLINFO);
            break;
        case 2://network mode
            ui_hide(CAT1_LAYOUT_NETWORK);
            ui_show(CAT1_LAYOUT_NETWORK_MODE);
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
REGISTER_UI_EVENT_HANDLER(CAT1_VLIST_NETWORK)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = network_ontouch,
};

//------------一级菜单-TELEPHONY-NETWORK-CELLINFO-------------//
void int2str(int src, char *dst)
{
    sprintf(dst, "%d", src);
}

struct show_serving_cell {
    char cell_id[20];
    char earfcn[5];
    char sinr[5];
    char rssi[5];
    char transfer_mode[5];
    char bandwidth[5];
};
struct show_serving_cell s_show_serving_cell;

ual_tele_radio_lte_cell_info_t func_serving_cell_info = {0};
ual_tele_radio_lte_neighbor_cell_info_t func_neighbor_cell_info = {0};
struct show_neighbor_cell {
    char cell_id[20];
    char earfcn[5];
    char sinr[5];
    char rssi[5];
};
struct show_neighbor_cell s_show_neighbor_info;

static int layout_cellinfo_ontouch(void *ctr, struct element_touch_event *e)
{
    struct layout *layout = (struct layout *)ctr;

    switch (e->event) {
    case ELM_EVENT_TOUCH_R_MOVE:
        memset(&s_show_serving_cell, 0, sizeof(s_show_serving_cell));
        ui_hide(CAT1_LAYOUT_CELLINFO);
        ui_show(CAT1_LAYOUT_NETWORK);
        return true;
        break;
    default:
        break;
    }

    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_LAYOUT_CELLINFO)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = layout_cellinfo_ontouch,
};

static int cellinfo_ontouch(void *ctr, struct element_touch_event *e)
{
    struct ui_grid *grid = (struct ui_grid *)ctr;
    static u8 touch_action = 0;
    int sel_item;
    int rpc_ret = 0;
    int res;

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
            is_cell_info = 1;
            res = ual_tele_radio_get_lte_cell_info(0, &func_serving_cell_info, &rpc_ret);
            printf("res:%d\n", res);
            printf("rpc_ret = %d\n", rpc_ret);
            break;
        case 2:
            is_neigh_cell_info = 1;
            res = ual_tele_radio_get_lte_neighbor_cell_info(0, &func_neighbor_cell_info, &rpc_ret);
            printf("res:%d\n", res);
            printf("rpc_ret = %d\n", rpc_ret);
            break;
        case 3:
            //ui_hide(CAT1_LAYOUT_CELLINFO);
            //ui_show(CAT1_LAYOUT_BETWEEN_AJACENT_CELL);
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
REGISTER_UI_EVENT_HANDLER(CAT1_VLIST_CELLINFO)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = cellinfo_ontouch,
};

//------------CELLINFO-SERVING CELL INFO-serving cell------------//
static int LAYOUT_serving_cell_ontouch(void *ctr, struct element_touch_event *e)
{
    struct layout *layout = (struct layout *)ctr;

    switch (e->event) {
    case ELM_EVENT_TOUCH_R_MOVE:
        ui_hide(CAT1_LAYOUT_SERVING_CELL);
        ui_show(CAT1_LAYOUT_CELLINFO);
        return true;
        break;
    default:
        break;
    }

    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_LAYOUT_SERVING_CELL)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = LAYOUT_serving_cell_ontouch,
};

static int text_sc_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct ui_text *text = (struct ui_text *)_ctrl;
    int rpc_ret = 0;
    extern ual_tele_radio_lte_cell_info_t serving_cell_info;

    switch (event) {
    case ON_CHANGE_INIT:
        switch (text->elm.id) {
        case CAT1_TEXT_SC_CELL_ID:
            printf("cell id:%d\n", serving_cell_info.cell_id.values[0]);
            //int2str(serving_cell_info.cell_id.values[0], s_show_serving_cell.cell_id);
            sprintf(s_show_serving_cell.cell_id, "%d", serving_cell_info.cell_id.values[0]);
            ui_text_set_text_attrs(text, s_show_serving_cell.cell_id, strlen(s_show_serving_cell.cell_id), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_SC_FREQUENCY:
            printf("frequency:%d\n", serving_cell_info.earfcn.values[0]);
            int2str(serving_cell_info.earfcn.values[0], s_show_serving_cell.earfcn);
            ui_text_set_text_attrs(text, s_show_serving_cell.earfcn, strlen(s_show_serving_cell.earfcn), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_SC_SINR:
            printf("sinr:%d\n", serving_cell_info.rsrq.values[0]);
            int2str(serving_cell_info.rsrq.values[0], s_show_serving_cell.sinr);
            ui_text_set_text_attrs(text, s_show_serving_cell.sinr, strlen(s_show_serving_cell.sinr), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_SC_RSSI:
            printf("rssi:%d\n", serving_cell_info.srxlev.values[0]);
            int2str(serving_cell_info.srxlev.values[0], s_show_serving_cell.rssi);
            ui_text_set_text_attrs(text, s_show_serving_cell.rssi, strlen(s_show_serving_cell.rssi), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_SC_BANDWIDTH:
            printf("bw:%d\n", serving_cell_info.bw.values[0]);
            int2str(serving_cell_info.bw.values[0], s_show_serving_cell.bandwidth);
            ui_text_set_text_attrs(text, s_show_serving_cell.bandwidth, strlen(s_show_serving_cell.bandwidth), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        }
        break;
    case ON_CHANGE_RELEASE:
        break;
    }
    return 0;
}
REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_SC_CELL_ID)
.onchange = text_sc_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_SC_FREQUENCY)
.onchange = text_sc_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_SC_SINR)
.onchange = text_sc_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_SC_RSSI)
.onchange = text_sc_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_SC_BANDWIDTH)
.onchange = text_sc_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

//------------CELLINFO-THE AJACENT CELL INFO------------//
static int LAYOUT_the_ajacent_cell_ontouch(void *ctr, struct element_touch_event *e)
{
    struct layout *layout = (struct layout *)ctr;

    switch (e->event) {
    case ELM_EVENT_TOUCH_R_MOVE:
        memset(&s_show_neighbor_info, 0, sizeof(s_show_neighbor_info));
        ui_hide(CAT1_LAYOUT_THE_AJACENT_CELL);
        ui_show(CAT1_LAYOUT_CELLINFO);
        return true;
        break;
    default:
        break;
    }

    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_LAYOUT_THE_AJACENT_CELL)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = LAYOUT_the_ajacent_cell_ontouch,
};

static int text_tac_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct ui_text *text = (struct ui_text *)_ctrl;
    extern ual_tele_radio_lte_neighbor_cell_info_t neighbor_cell_info;

    switch (event) {
    case ON_CHANGE_INIT:
        switch (text->elm.id) {
        case CAT1_TEXT_TAC_CELL_ID:
            int2str(neighbor_cell_info.cell_id, s_show_neighbor_info.cell_id);
            ui_text_set_text_attrs(text, s_show_neighbor_info.cell_id, strlen(s_show_neighbor_info.cell_id), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_TAC_FREQUENCY:
            int2str(neighbor_cell_info.earfcn, s_show_neighbor_info.earfcn);
            ui_text_set_text_attrs(text, s_show_neighbor_info.earfcn, strlen(s_show_neighbor_info.earfcn), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_TAC_SINR:
            int2str(neighbor_cell_info.rsrq, s_show_neighbor_info.sinr);
            ui_text_set_text_attrs(text, s_show_neighbor_info.sinr, strlen(s_show_neighbor_info.sinr), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_TAC_RSSI:
            int2str(neighbor_cell_info.srxlev, s_show_neighbor_info.rssi);
            ui_text_set_text_attrs(text, s_show_neighbor_info.rssi, strlen(s_show_neighbor_info.rssi), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        }
        break;
    case ON_CHANGE_RELEASE:
        break;
    }
    return 0;
}
REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_TAC_CELL_ID)
.onchange = text_tac_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_TAC_FREQUENCY)
.onchange = text_tac_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_TAC_SINR)
.onchange = text_tac_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_TAC_RSSI)
.onchange = text_tac_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

//-------------一级菜单-TELEPHONY-NETWORK-NETWORK MODE----------------//
static int layout_network_mode_ontouch(void *ctr, struct element_touch_event *e)
{
    struct layout *layout = (struct layout *)ctr;

    switch (e->event) {
    case ELM_EVENT_TOUCH_R_MOVE:
        ui_hide(CAT1_LAYOUT_NETWORK_MODE);
        ui_show(CAT1_LAYOUT_NETWORK);
        return true;
        break;
    default:
        break;
    }

    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_LAYOUT_NETWORK_MODE)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = layout_network_mode_ontouch,
};

int network_mode = 0;
char *network_mode_str[] = {"GSM", "3G", "GSM and 3G", "LTE", "GSM and LTE", "3G and LTE", "GSM and 3G and LTE"};
int network_mode_unisoc_type[] = {UAL_TELE_RADIO_PREFER_NETWORK_TYPE_GSM, UAL_TELE_RADIO_PREFER_NETWORK_TYPE_3G,
                                  UAL_TELE_RADIO_PREFER_NETWORK_TYPE_GSM_AND_3G, UAL_TELE_RADIO_PREFER_NETWORK_TYPE_LTE,
                                  UAL_TELE_RADIO_PREFER_NETWORK_TYPE_GSM_LTE, UAL_TELE_RADIO_PREFER_NETWORK_TYPE_3G_LTE,
                                  UAL_TELE_RADIO_PREFER_NETWORK_TYPE_GSM_3G_LTE
                                 };

static int network_mode_sure_ontouch(void *ctr, struct element_touch_event *e)
{
    struct ui_text *text = (struct ui_text *)ctr;
    int rpc_ret = 0;

    switch (e->event) {
    case ELM_EVENT_TOUCH_UP:
        network_mode = ui_grid_get_hindex(ui_grid_for_id(CAT1_VLIST_NETWORK_MODE));//获取相应的NETWORKMODE
        //printf("network mode:%d\n", network_mode);

        //设置网络模式
        printf("network mode:%d\n", network_mode_unisoc_type[network_mode]);
        int res = ual_tele_radio_set_prefer_network_type(0, network_mode_unisoc_type[network_mode], &rpc_ret);
        printf("res:%d\n", res);
        printf("rpc_ret = %d\n", rpc_ret);

        ui_hide(CAT1_LAYOUT_NETWORK_MODE);
        ui_show(CAT1_LAYOUT_NETWORK);
        break;
    case ELM_EVENT_TOUCH_HOLD:
        break;
    case ELM_EVENT_TOUCH_MOVE:
        break;
    case ELM_EVENT_TOUCH_DOWN:
        return true;
    case ELM_EVENT_TOUCH_U_MOVE:
        break;
    case ELM_EVENT_TOUCH_D_MOVE:
        break;
    }

    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NETWORK_MODE_SURE)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = network_mode_sure_ontouch,
};

static int text_network_mode_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct ui_text *text = (struct ui_text *)_ctrl;

    switch (event) {
    case ON_CHANGE_INIT:
        switch (text->elm.id) {
        case CAT1_TEXT_NETWORK_MODE:
            ui_text_set_text_attrs(text, network_mode_str[network_mode], strlen(network_mode_str[network_mode]), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
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
REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_NETWORK_MODE)
.onchange = text_network_mode_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

//-------------------一级菜单-TELEPHONY-VOLTE------------------//
static int LAYOUT_VOLTE_ontouch(void *ctr, struct element_touch_event *e)
{
    struct layout *layout = (struct layout *)ctr;

    switch (e->event) {
    case ELM_EVENT_TOUCH_R_MOVE:
        LAYOUT_SCREEN_DISP;
        ui_hide(CAT1_LAYOUT_VOLTE);
        ui_show(CAT1_LAYOUT_TELEPHONY);
        return true;
        break;
    default:
        break;
    }

    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_LAYOUT_VOLTE)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = LAYOUT_VOLTE_ontouch,
};

static int VOLTE_ontouch(void *ctr, struct element_touch_event *e)
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
        case 1://VOLTE开关


            break;
        case 2://IMS参数

            ui_hide(CAT1_LAYOUT_VOLTE);
            ui_show(CAT1_LAYOUT_IMS);
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
REGISTER_UI_EVENT_HANDLER(CAT1_VLIST_VOLTE)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = VOLTE_ontouch,
};

int is_volte = 1;
static int pic_volte_set_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct ui_pic *pic = (struct ui_pic *)_ctrl;

    switch (event) {
    case ON_CHANGE_INIT:
        switch (pic->elm.id) {
        case CAT1_PIC_VOLTE_SEL:
            ui_pic_set_image_index(pic, !!is_volte);
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

static int pic_volte_set_ontouch(void *ctr, struct element_touch_event *e)
{
    struct ui_pic *pic = (struct ui_pic *)ctr;
    static u8 touch_action = 0;
    int rpc_ret = 0;
    int res;

    switch (e->event) {
    case ELM_EVENT_TOUCH_UP:
        if (touch_action != 1) {
            break;
        }

        switch (pic->elm.id) {
        case CAT1_PIC_VOLTE_SEL:
            is_volte = !is_volte;
            ui_pic_show_image_by_id(pic->elm.id, !!is_volte);//通过索引显示图片

            //设置volte开关
            if (is_volte == 0) {
                res = ual_tele_radio_set_volte(0, false, &rpc_ret);
                printf("res:%d\n", res);
                printf("rpc_ret = %d\n", rpc_ret);
            } else {
                res = ual_tele_radio_set_volte(0, true, &rpc_ret);
                printf("res:%d\n", res);
                printf("rpc_ret = %d\n", rpc_ret);
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
REGISTER_UI_EVENT_HANDLER(CAT1_PIC_VOLTE_SEL)//volte开关
.onchange = pic_volte_set_onchange,
 .onkey = NULL,
  .ontouch = pic_volte_set_ontouch,
};

//-----------------一级菜单-TELEPHONY-lock frequency----------------//
uint8 lock_frequency_index = 0;
int lock_frequency_list[9] = {0};

struct factory_mode_number {
    char number[10];
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
    ui_number_update_by_id(CAT1_NUMBER_SHOW, &n);
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
    ui_number_update_by_id(CAT1_NUMBER_SHOW, &n);
}

static int LAYOUT_lock_frequency_ontouch(void *ctr, struct element_touch_event *e)
{
    struct layout *layout = (struct layout *)ctr;

    switch (e->event) {
    case ELM_EVENT_TOUCH_R_MOVE:
        memset(lock_frequency_list, 0, sizeof(lock_frequency_list));
        lock_frequency_index = 0;
        ui_hide(CAT1_LAYOUT_LOCK_FREQUENCY);
        ui_show(CAT1_LAYOUT_TELEPHONY);
        return true;
        break;
    default:
        break;
    }

    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_LAYOUT_LOCK_FREQUENCY)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = LAYOUT_lock_frequency_ontouch,
};

char lock_frequency_list_str[50] = {0};
char lock_frequency_str[10] = {0};
static int vlist_lock_frequency_ontouch(void *ctr, struct element_touch_event *e)
{
    struct ui_grid *grid = (struct ui_grid *)ctr;
    static u8 touch_action = 0;
    int sel_item;
    int rpc_ret = 0;
    int res;
    uint8 number = 0;
    uint32 lte_freq[UAL_TELE_RADIO_LTE_FREQ_MAX_COUNT] = {0};


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
            is_lock_frequency = 1;
            ui_hide(CAT1_LAYOUT_LOCK_FREQUENCY);
            ui_show(CAT1_LAYOUT_FACTORY_MODE_NUMBER);       //打开数字输入键盘
            break;
        case 2:
            //锁定频率接口
            for (int i = 0; i < lock_frequency_index; i++) {
                printf("lock frequency:%d\n", lock_frequency_list[i]);
            }
            ual_tele_radio_set_lte_lock_freq(0, lock_frequency_index, lock_frequency_list, &rpc_ret);
            //printf("res:%d\n", res);
            printf("rpc_ret = %d\n", rpc_ret);

            memset(lock_frequency_list, 0, sizeof(lock_frequency_list));
            lock_frequency_index = 0;
            break;
        case 3:
            ual_tele_radio_get_lte_lock_freq(0, &number, lte_freq, &rpc_ret);
            //printf("res:%d\n", res);
            printf("rpc_ret = %d\n", rpc_ret);
            for (int i = 0; i < number; i++) {
                printf("lte_freq:%d\n", lte_freq[i]);
            }
            //字符串拼接转换
            for (int i = 0; i < number; i++) {
                memset(lock_frequency_str, 0, sizeof(lock_frequency_str));
                sprintf(lock_frequency_str, "%d", lte_freq[i]);
                strcat(lock_frequency_list_str, lock_frequency_str);
                if (i < number - 1) {
                    strcat(lock_frequency_list_str, ",");
                }
            }
            printf("lock freq:%s\n", lock_frequency_list_str);
            ui_text_set_text_by_id(CAT1_TEXT_QUERY_LOCK_FREQUENCY, lock_frequency_list_str, strlen(lock_frequency_list_str), FONT_DEFAULT | FONT_SHOW_SCROLL);
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
REGISTER_UI_EVENT_HANDLER(CAT1_VLIST_LOCK_FREQUENCY)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = vlist_lock_frequency_ontouch,
};

static int factory_mode_number_info_ontouch(void *ctrl, struct element_touch_event *e)
{
    struct element *elm = (struct element *)ctrl;
    int rpc_ret = 0;
    char ims_value_str[4];
    char responde[100];
    int responde_len;
    int res;

    switch (e->event) {
    case ELM_EVENT_TOUCH_DOWN:
        switch (elm->id) {
        case CAT1_NUMBER_DEL:
            printf("[5]CAT1_NUMBER_DEL DOWN\n");
            break;
        case CAT1_NUMBER_0:
            printf("[5]CAT1_NUMBER_0 DOWN\n");
            break;
        case CAT1_NUMBER_1:
            printf("[5]CAT1_NUMBER_1 DOWN\n");
            break;
        case CAT1_NUMBER_2:
            printf("[5]CAT1_NUMBER_2 DOWN\n");
            break;
        case CAT1_NUMBER_3:
            printf("[5]CAT1_NUMBER_3 DOWN\n");
            break;
        case CAT1_NUMBER_4:
            printf("[5]CAT1_NUMBER_4 DOWN\n");
            break;
        case CAT1_NUMBER_5:
            printf("[5]CAT1_NUMBER_5 DOWN\n");
            break;
        case CAT1_NUMBER_6:
            printf("[5]CAT1_NUMBER_6 DOWN\n");
            break;
        case CAT1_NUMBER_7:
            printf("[5]CAT1_NUMBER_7 DOWN\n");
            break;
        case CAT1_NUMBER_8:
            printf("[5]CAT1_NUMBER_8 DOWN\n");
            break;
        case CAT1_NUMBER_9:
            printf("[5]CAT1_NUMBER_9 DOWN\n");
            break;
        case CAT1_NUMBER_SURE:
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
        case CAT1_NUMBER_DEL:
            printf("[5]CAT1_NUMBER_DEL UP\n");
            if (index == 0) {
                break;
            }
            factory_mode_remove_number();
            break;
        case CAT1_NUMBER_0:
            printf("[5]CAT1_NUMBER_0 UP\n");
            factory_mode_add_number('0');
            break;
        case CAT1_NUMBER_1:
            printf("[5]CAT1_NUMBER_1 UP\n");
            factory_mode_add_number('1');
            break;
        case CAT1_NUMBER_2:
            printf("[5]CAT1_NUMBER_2 UP\n");
            factory_mode_add_number('2');
            break;
        case CAT1_NUMBER_3:
            printf("[5]CAT1_NUMBER_3 UP\n");
            factory_mode_add_number('3');
            break;
        case CAT1_NUMBER_4:
            printf("[5]CAT1_NUMBER_4 UP\n");
            factory_mode_add_number('4');
            break;
        case CAT1_NUMBER_5:
            printf("[5]CAT1_NUMBER_5 UP\n");
            factory_mode_add_number('5');
            break;
        case CAT1_NUMBER_6:
            printf("[5]CAT1_NUMBER_6 UP\n");
            factory_mode_add_number('6');
            break;
        case CAT1_NUMBER_7:
            printf("[5]CAT1_NUMBER_7 UP\n");
            factory_mode_add_number('7');
            break;
        case CAT1_NUMBER_8:
            printf("[5]CAT1_NUMBER_8 UP\n");
            factory_mode_add_number('8');
            break;
        case CAT1_NUMBER_9:
            printf("[5]CAT1_NUMBER_9 UP\n");
            factory_mode_add_number('9');
            break;
        case CAT1_NUMBER_SURE:
            //需要用全局变量确认
            if (is_lock_frequency == 1) {
                printf("lock frequency set!\n");
                //将字符串转换成数字，添加进频率列表，将结构体清空，再返回前一及界面
                int frequency_number_int = atoi(factory_mode_number_t.number);
                printf("int of frequency number:%d\n", frequency_number_int);
                lock_frequency_list[lock_frequency_index++] = frequency_number_int;
                memset(&factory_mode_number_t, 0, sizeof(factory_mode_number_t));
                is_lock_frequency = 0;
                ui_hide(CAT1_LAYOUT_FACTORY_MODE_NUMBER);
                ui_show(CAT1_LAYOUT_LOCK_FREQUENCY);
            }
            //查询ims index
            else if (is_check_ims == 1) {
                ims_index = atoi(factory_mode_number_t.number);
                printf("check ims!!!\n");
                printf("int of ims_index number:%d\n", ims_index);
                memset(ims_nv_value, 0, sizeof(ims_nv_value));
                printf("ims value:%s\n", ims_nv_value);
                res = ual_tele_radio_get_ims_param(0, ims_index, ims_nv_value, &rpc_ret);
                printf("res:%d\n", res);
                printf("rpc_ret = %d\n", rpc_ret);

                //printf("ims value:%d\n", ims_nv_value[ims_index]);
                printf("ims value:%s\n", ims_nv_value);
                //sprintf(ims_value_str, "%d", ims_nv_value[ims_index]);
                //ui_text_set_text_by_id(CAT1_TEXT_IMS_VALUE, ims_value_str, strlen(ims_value_str), FONT_DEFAULT);

                memset(&factory_mode_number_t, 0, sizeof(factory_mode_number_t));
                is_check_ims = 0;
                ui_hide(CAT1_LAYOUT_FACTORY_MODE_NUMBER);
                ui_show(CAT1_LAYOUT_IMS);
            }
            //设置ims index
            else if (is_set_ims_index == 1) {
                ims_index = atoi(factory_mode_number_t.number);
                printf("set ims!!!\n");
                printf("int of ims_index number:%d\n", ims_index);
                memset(&factory_mode_number_t, 0, sizeof(factory_mode_number_t));
                is_set_ims_index = 0;
                ui_hide(CAT1_LAYOUT_FACTORY_MODE_NUMBER);
                ui_show(CAT1_LAYOUT_SET_IMS);
            } else if (is_set_ims_value == 1) {
                printf("[5]CAT1_NUMBER_SURE UP\n");
                //ims_set_value = atoi(factory_mode_number_t.number);
                //printf("int of ims value:%d\n", ims_set_value);
                memcpy(ims_nv_value, factory_mode_number_t.number, sizeof(factory_mode_number_t.number));
                memset(&factory_mode_number_t, 0, sizeof(factory_mode_number_t));
                is_set_ims_value = 0;
                ui_hide(CAT1_LAYOUT_FACTORY_MODE_NUMBER);
                ui_show(CAT1_LAYOUT_SET_IMS);
            } else if (is_supply_service_call_forward == 1) {
                strcat(call_forward_str, factory_mode_number_t.number);
                strcat(call_forward_str, "\",");
                memset(&factory_mode_number_t, 0, sizeof(factory_mode_number_t));
                is_supply_service_call_forward = 0;
                ui_hide(CAT1_LAYOUT_FACTORY_MODE_NUMBER);
                ui_show(CAT1_LAYOUT_CALL_FORWARD);
            } else if (is_supply_service_unconditional_call_forward == 1) {
                memset(unconditional_call_forward, '\0', sizeof(unconditional_call_forward));
                strcat(unconditional_call_forward, "AT+CCFC=0,3,\"");
                strcat(unconditional_call_forward, factory_mode_number_t.number);
                strcat(unconditional_call_forward, "\",128");
                memset(&factory_mode_number_t, 0, sizeof(factory_mode_number_t));
                is_supply_service_unconditional_call_forward = 0;
                printf("the str of at:%d\n", unconditional_call_forward);
                at_send_sync(unconditional_call_forward, &responde, &responde_len);
                ui_hide(CAT1_LAYOUT_FACTORY_MODE_NUMBER);
                ui_show(CAT1_LAYOUT_TELEPHONY);
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

REGISTER_UI_EVENT_HANDLER(CAT1_NUMBER_1)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = factory_mode_number_info_ontouch,
};

REGISTER_UI_EVENT_HANDLER(CAT1_NUMBER_2)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = factory_mode_number_info_ontouch,
};

REGISTER_UI_EVENT_HANDLER(CAT1_NUMBER_3)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = factory_mode_number_info_ontouch,
};

REGISTER_UI_EVENT_HANDLER(CAT1_NUMBER_4)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = factory_mode_number_info_ontouch,
};

REGISTER_UI_EVENT_HANDLER(CAT1_NUMBER_5)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = factory_mode_number_info_ontouch,
};

REGISTER_UI_EVENT_HANDLER(CAT1_NUMBER_6)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = factory_mode_number_info_ontouch,
};

REGISTER_UI_EVENT_HANDLER(CAT1_NUMBER_7)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = factory_mode_number_info_ontouch,
};

REGISTER_UI_EVENT_HANDLER(CAT1_NUMBER_8)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = factory_mode_number_info_ontouch,
};

REGISTER_UI_EVENT_HANDLER(CAT1_NUMBER_9)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = factory_mode_number_info_ontouch,
};

REGISTER_UI_EVENT_HANDLER(CAT1_NUMBER_0)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = factory_mode_number_info_ontouch,
};

REGISTER_UI_EVENT_HANDLER(CAT1_NUMBER_DEL)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = factory_mode_number_info_ontouch,
};

REGISTER_UI_EVENT_HANDLER(CAT1_NUMBER_SURE)
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

REGISTER_UI_EVENT_HANDLER(CAT1_NUMBER_SHOW)
.onchange = factory_mode_number_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};


//----------------一级菜单-TELEPHONY-VOLTE-IMS参数-----------------------//

static int layout_ims_ontouch(void *ctr, struct element_touch_event *e)
{
    struct layout *layout = (struct layout *)ctr;

    switch (e->event) {
    case ELM_EVENT_TOUCH_R_MOVE:
        ui_hide(CAT1_LAYOUT_IMS);
        ui_show(CAT1_LAYOUT_VOLTE);
        return true;
        break;
    default:
        break;
    }

    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_LAYOUT_IMS)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = layout_ims_ontouch,
};

static int vlist_ims_ontouch(void *ctr, struct element_touch_event *e)
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
        if (sel_item < 0) {
            break;
        }
        switch (sel_item) {
        case 0://查询ims
            is_check_ims = 1;
            ims_index = 0;       //重新初始化index
            memset(ims_nv_value, 0, sizeof(ims_nv_value));
            ui_hide(CAT1_LAYOUT_IMS);
            ui_show(CAT1_LAYOUT_FACTORY_MODE_NUMBER);
            break;
        case 1:
            //printf("ims_value_onchange:%s\n", ims_nv_value);
            for (int i = 0; i < 10; i++) {
                printf("ims_value_onchange:%d", ims_nv_value[i]);
            }
            ui_text_set_text_by_id(CAT1_TEXT_IMS_VALUE, ims_nv_value, strlen(ims_nv_value), FONT_DEFAULT);
            break;
        case 3://设置ims
            ims_index = 0;
            ims_set_value = 0;
            memset(ims_nv_value, 0, sizeof(ims_nv_value));
            ui_hide(CAT1_LAYOUT_IMS);
            ui_show(CAT1_LAYOUT_SET_IMS);
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
REGISTER_UI_EVENT_HANDLER(CAT1_VLIST_IMS)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = vlist_ims_ontouch,
};

//----------------一级菜单-TELEPHONY-VOLTE-IMS参数-查询IMS-----------------------//
static int text_ims_value_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct ui_text *text = (struct ui_text *)_ctrl;
    char ims_value_str[4];

    switch (event) {
    case ON_CHANGE_INIT:
        switch (text->elm.id) {
        case CAT1_TEXT_IMS_VALUE:
            //sprintf(ims_value_str, "%d", ims_nv_value[ims_index]);
            //ui_text_set_text_attrs(text, ims_value_str, strlen(ims_value_str), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
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
REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_IMS_VALUE)
.onchange = text_ims_value_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

//----------------一级菜单-TELEPHONY-VOLTE-IMS参数-设置IMS-----------------------//
static int layout_set_ims_ontouch(void *ctr, struct element_touch_event *e)
{
    struct layout *layout = (struct layout *)ctr;

    switch (e->event) {
    case ELM_EVENT_TOUCH_R_MOVE:
        ui_hide(CAT1_LAYOUT_SET_IMS);
        ui_show(CAT1_LAYOUT_IMS);
        return true;
        break;
    default:
        break;
    }

    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_LAYOUT_SET_IMS)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = layout_set_ims_ontouch,
};

static int vlist_set_ims_ontouch(void *ctr, struct element_touch_event *e)
{
    struct ui_grid *grid = (struct ui_grid *)ctr;
    static u8 touch_action = 0;
    int sel_item;
    int rpc_ret = 0;

    switch (e->event) {
    case ELM_EVENT_TOUCH_UP:
        if (touch_action != 1) {
            break;
        }

        sel_item = ui_grid_cur_item(grid);
        printf("sel_item:%d\n", sel_item);
        if (sel_item < 0) {
            break;
        }
        switch (sel_item) {
        case 1:
            is_set_ims_index = 1;
            ui_hide(CAT1_LAYOUT_SET_IMS);
            ui_show(CAT1_LAYOUT_FACTORY_MODE_NUMBER);
            break;
        case 2://设置ims
            is_set_ims_value = 1;
            ui_hide(CAT1_LAYOUT_SET_IMS);
            ui_show(CAT1_LAYOUT_FACTORY_MODE_NUMBER);
            break;
        case 3:
            printf("ims index:%d\n", ims_index);
            printf("ims value:%s\n", ims_nv_value);
            //printf("ims value:%d\n", ims_set_value);
            //ims_nv_value[ims_index] = ims_set_value;
            int res = ual_tele_radio_set_ims_param(0, ims_index, ims_nv_value, &rpc_ret);
            printf("res:%d\n", res);
            printf("rpc_ret = %d\n", rpc_ret);
            printf("set ims value success!\n");
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
REGISTER_UI_EVENT_HANDLER(CAT1_VLIST_SET_IMS)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = vlist_set_ims_ontouch,
};

//-----------------一级菜单-TELEPHONY-band select----------------//
int lte_band_index = 0;       //array index
uint16 lte_band[UAL_TELE_RADIO_LTE_BAND_MAX_COUNT] = {0};
char lock_band_list_str[50] = "";
char lock_band_str[3] = "";
static int LAYOUT_band_select_ontouch(void *ctr, struct element_touch_event *e)
{
    struct layout *layout = (struct layout *)ctr;

    switch (e->event) {
    case ELM_EVENT_TOUCH_R_MOVE:
        memset(lte_band, 0, sizeof(lte_band));
        lte_band_index = 0;
        ui_hide(CAT1_LAYOUT_BANDSELECT);
        ui_show(CAT1_LAYOUT_TELEPHONY);
        return true;
        break;
    default:
        break;
    }

    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_LAYOUT_BANDSELECT)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = LAYOUT_band_select_ontouch,
};

static int band_select_ontouch(void *ctr, struct element_touch_event *e)
{
    struct ui_grid *grid = (struct ui_grid *)ctr;
    static u8 touch_action = 0;
    int sel_item;
    int rpc_ret = 0;
    uint8 number;
    int res;

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
            printf("1\n");
            lte_band[lte_band_index++] = 1;
            break;
        case 2:
            printf("2\n");
            lte_band[lte_band_index++] = 2;
            break;
        case 3:
            printf("3\n");
            lte_band[lte_band_index++] = 3;
            break;
        case 4:
            printf("4\n");
            lte_band[lte_band_index++] = 4;
            break;
        case 5:
            printf("5\n");
            lte_band[lte_band_index++] = 5;
            break;
        case 6:
            printf("7\n");
            lte_band[lte_band_index++] = 7;
            break;
        case 7:
            printf("8\n");
            lte_band[lte_band_index++] = 8;
            break;
        case 8:
            printf("12\n");
            lte_band[lte_band_index++] = 12;
            break;
        case 9:
            printf("13\n");
            lte_band[lte_band_index++] = 13;
            break;
        case 10:
            printf("17\n");
            lte_band[lte_band_index++] = 17;
            break;
        case 11:
            printf("20\n");
            lte_band[lte_band_index++] = 20;
            break;
        case 12:
            printf("28\n");
            lte_band[lte_band_index++] = 28;
            break;
        case 13:
            printf("34\n");
            lte_band[lte_band_index++] = 34;
            break;
        case 14:
            printf("38\n");
            lte_band[lte_band_index++] = 38;
            break;
        case 15:
            printf("39\n");
            lte_band[lte_band_index++] = 39;
            break;
        case 16:
            printf("40\n");
            lte_band[lte_band_index++] = 40;
            break;
        case 17:
            printf("41\n");
            lte_band[lte_band_index++] = 41;
            break;
        case 18:
            printf("66\n");
            lte_band[lte_band_index++] = 66;
            break;
        case 19:
            printf("lte band sure!\n");
            for (int i = 0; i < lte_band_index; i++) {
                printf("%d\n", lte_band[i]);
            }
            ual_tele_radio_set_lte_lock_band(0, (uint8)lte_band_index, lte_band, &rpc_ret);     //锁定band
            //printf("res:%d\n", res);
            printf("rpc_ret = %d\n", rpc_ret);

            memset(lte_band, 0, sizeof(lte_band));
            lte_band_index = 0;
            break;
        case 20:
            printf("lte band check!\n");
            memset(lock_band_list_str, 0, sizeof(lock_band_list_str));
            ual_tele_radio_get_lte_lock_band(0, &number, lte_band, &rpc_ret);
            //printf("res:%d\n", res);
            printf("rpc_ret = %d\n", rpc_ret);
            for (int i = 0; i < number; i++) {
                printf("lock band:%d\n", lte_band[i]);
            }
            for (int i = 0; i < number; i++) {
                memset(lock_band_str, 0, sizeof(lock_band_str));
                sprintf(lock_band_str, "%d", lte_band[i]);
                strcat(lock_band_list_str, lock_band_str);
                if (i < number - 1) {
                    strcat(lock_band_list_str, ",");
                }
            }
            printf("lock band:%s\n", lock_band_list_str);
            ui_text_set_text_by_id(CAT1_TEXT_LOCK_BAND_CHECK, lock_band_list_str, strlen(lock_band_list_str), FONT_DEFAULT | FONT_SHOW_SCROLL);
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
REGISTER_UI_EVENT_HANDLER(CAT1_VLIST_BANDSELECT)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = band_select_ontouch,
};

static int layout_supply_service_ontouch(void *ctr, struct element_touch_event *e)
{
    struct layout *layout = (struct layout *)ctr;

    switch (e->event) {
    case ELM_EVENT_TOUCH_R_MOVE:
        ui_hide(CAT1_LAYOUT_SUPPLY_SERVICE);
        ui_show(CAT1_LAYOUT_TELEPHONY);
        return true;
        break;
    default:
        break;
    }

    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_LAYOUT_SUPPLY_SERVICE)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = layout_supply_service_ontouch,
};

//---------------------------补充业务-------------------------------//
static int vlist_supply_service_ontouch(void *ctr, struct element_touch_event *e)
{
    struct ui_grid *grid = (struct ui_grid *)ctr;
    static u8 touch_action = 0;
    int sel_item;
    int rpc_ret = 0;
    int modem_state;

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
            printf("call wait setting!!\n");
            memset(call_wait_str, '\0', sizeof(call_wait_str));
            strcat(call_wait_str, "AT+CCWA=");
            ui_hide(CAT1_VLIST_SUPPLY_SERVICE);
            ui_show(CAT1_LAYOUT_CALL_WAIT);
            break;
        case 2:
            printf("call forward setting!!\n");
            memset(call_forward_str, '\0', sizeof(call_forward_str));
            strcat(call_forward_str, "AT+CCFC=");
            ui_hide(CAT1_VLIST_SUPPLY_SERVICE);
            ui_show(CAT1_LAYOUT_CALL_FORWARD);
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
REGISTER_UI_EVENT_HANDLER(CAT1_VLIST_SUPPLY_SERVICE)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = vlist_supply_service_ontouch,
};

//-------------------补充业务-呼叫等待-------------------------//
static int layout_call_wait_ontouch(void *ctr, struct element_touch_event *e)
{
    struct layout *layout = (struct layout *)ctr;

    switch (e->event) {
    case ELM_EVENT_TOUCH_R_MOVE:
        ui_hide(CAT1_LAYOUT_CALL_WAIT);
        ui_show(CAT1_LAYOUT_SUPPLY_SERVICE);
        return true;
        break;
    default:
        break;
    }

    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_LAYOUT_CALL_WAIT)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = layout_call_wait_ontouch,
};

static int vlist_call_wait_ontouch(void *ctr, struct element_touch_event *e)
{
    struct ui_grid *grid = (struct ui_grid *)ctr;
    static u8 touch_action = 0;
    int sel_item;
    int rpc_ret = 0;
    char responde[100];
    int responde_len;

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
            printf("str add 0\n");
            strcat(call_wait_str, "0,");
            break;
        case 2:
            printf("str add 1\n");
            strcat(call_wait_str, "1,");
            break;
        case 4:
            printf("str add 0\n");
            strcat(call_wait_str, "0,");
            break;
        case 5:
            printf("str add 1\n");
            strcat(call_wait_str, "1,");
            break;
        case 7:
            printf("str add voice service\n");
            strcat(call_wait_str, "1");
            break;
        case 8:
            printf("str confirm!\n");
            printf("the at str is %s\n", call_wait_str);
            at_send_sync(call_wait_str, &responde, &responde_len);
            printf("responde:%s\n", responde);
            memset(call_wait_str, '\0', sizeof(call_wait_str));
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
REGISTER_UI_EVENT_HANDLER(CAT1_VLIST_CALL_WAIT)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = vlist_call_wait_ontouch,
};

//---------------补充业务-呼叫转移-----------------------------//
static int layout_call_forward_ontouch(void *ctr, struct element_touch_event *e)
{
    struct layout *layout = (struct layout *)ctr;

    switch (e->event) {
    case ELM_EVENT_TOUCH_R_MOVE:
        ui_hide(CAT1_LAYOUT_CALL_FORWARD);
        ui_show(CAT1_LAYOUT_SUPPLY_SERVICE);
        return true;
        break;
    default:
        break;
    }

    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_LAYOUT_CALL_FORWARD)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = layout_call_forward_ontouch,
};

static int vlist_call_forward_ontouch(void *ctr, struct element_touch_event *e)
{
    struct ui_grid *grid = (struct ui_grid *)ctr;
    static u8 touch_action = 0;
    int sel_item;
    int rpc_ret = 0;
    char responde[100];
    int responde_len;

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
            printf("str add 0\n");
            strcat(call_forward_str, "0,");
            break;
        case 2:
            printf("str add 1\n");
            strcat(call_forward_str, "1,");
            break;
        case 3:
            printf("str add 2\n");
            strcat(call_forward_str, "2,");
            break;
        case 4:
            printf("str add 3\n");
            strcat(call_forward_str, "3,");
            break;
        case 5:
            printf("str add 4\n");
            strcat(call_forward_str, "4,");
            break;
        case 6:
            printf("str add 5\n");
            strcat(call_forward_str, "5,");
        case 8:
            printf("str add 0\n");
            strcat(call_forward_str, "0,\"");
            break;
        case 9:
            printf("str add 1\n");
            strcat(call_forward_str, "1,\"");
            break;
        case 10:
            printf("str add 2\n");
            strcat(call_forward_str, "2,\"");
            break;
        case 11:
            printf("str add 3\n");
            strcat(call_forward_str, "3,\"");
            break;
        case 12:
            printf("str add 4\n");
            strcat(call_forward_str, "4,\"");
            break;
        case 14:
            //输入电话号码
            is_supply_service_call_forward = 1;
            ui_hide(CAT1_LAYOUT_CALL_FORWARD);
            ui_show(CAT1_LAYOUT_FACTORY_MODE_NUMBER);
            \
            break;
        case 16:
            printf("str add 128\n");
            strcat(call_forward_str, "128,");
            break;
        case 17:
            printf("str add 129\n");
            strcat(call_forward_str, "129,");
            break;
        case 18:
            printf("str add 145\n");
            strcat(call_forward_str, "145,");
            break;
        case 19:
            printf("str add 161\n");
            strcat(call_forward_str, "161,");
            break;
        case 21:
            printf("str add 1\n");
            strcat(call_forward_str, "1,");
            break;
        case 22:
            printf("str add 2\n");
            strcat(call_forward_str, "2,");
            break;
        case 23:
            printf("str add 4\n");
            strcat(call_forward_str, "4,");
            break;
        case 24:
            printf("str add 8\n");
            strcat(call_forward_str, "8,");
            break;
        case 25:
            printf("str add 16\n");
            strcat(call_forward_str, "16,");
            break;
        case 26:
            printf("str add 32\n");
            strcat(call_forward_str, "32,");
            break;
        case 27:
            printf("str add 64\n");
            strcat(call_forward_str, "64,");
            break;
        case 28:
            printf("str add 128\n");
            strcat(call_forward_str, "128,");
            break;
        case 30:
            printf("str add 5\n");
            strcat(call_forward_str, "5");
            break;
        case 31:
            printf("str add 15\n");
            strcat(call_forward_str, "15");
            break;
        case 32:
            printf("str add 30\n");
            strcat(call_forward_str, "128");
            break;
        case 33:
            printf("str confirm!\n");
            printf("the at str is %s\n", call_forward_str);
            at_send_sync(call_forward_str, &responde, &responde_len);
            printf("responde:%s\n", responde);
            memset(call_forward_str, '\0', sizeof(call_forward_str));
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
REGISTER_UI_EVENT_HANDLER(CAT1_VLIST_CALL_FORWARD)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = vlist_call_forward_ontouch,
};

//-----------一级菜单-TELEPHONY-LTE状态------------------//
static int layout_lte_status_ontouch(void *ctr, struct element_touch_event *e)
{
    struct layout *layout = (struct layout *)ctr;

    switch (e->event) {
    case ELM_EVENT_TOUCH_R_MOVE:
        ui_hide(CAT1_LAYOUT_LTE_STATUS);
        ui_show(CAT1_LAYOUT_TELEPHONY);
        return true;
        break;
    default:
        break;
    }

    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_LAYOUT_LTE_STATUS)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = layout_lte_status_ontouch,
};

static int text_lte_status_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct ui_text *text = (struct ui_text *)_ctrl;
    int rpc_ret = 0;
    int lte;
    char *lte_status = malloc(sizeof(char) * 2);

    switch (event) {
    case ON_CHANGE_INIT:
        switch (text->elm.id) {
        case CAT1_TEXT_LTE_STATUS:
            lte = ual_rf_am_get_current_lte_status(&rpc_ret);
            printf("rpc_ret = %d\n", rpc_ret);
            printf("lte_status:%d\n", lte);
            sprintf(lte_status, "%d", lte);
            ui_text_set_text_attrs(text, lte_status, strlen(lte_status), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        }
        break;
    case ON_CHANGE_RELEASE:
        break;
    }
    return 0;
}
REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_LTE_STATUS)
.onchange = text_lte_status_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

//-----------------一级菜单-modem ctrl----------------//
static int LAYOUT_modem_ctrl_ontouch(void *ctr, struct element_touch_event *e)
{
    struct layout *layout = (struct layout *)ctr;

    switch (e->event) {
    case ELM_EVENT_TOUCH_R_MOVE:
        ui_hide(CAT1_LAYOUT_MODEM_CTRL);
        ui_show(UOS_CAT1_LAYOUT_FIRST_MENU);
        return true;
        break;
    default:
        break;
    }

    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_LAYOUT_MODEM_CTRL)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = LAYOUT_modem_ctrl_ontouch,
};

char *modem_state_str[10] = {"offline", "alive", "assert", "reset", "block", "power on", "power off", "power on progress", "power off progress", "check err"};
static int modem_ctrl_ontouch(void *ctr, struct element_touch_event *e)
{
    struct ui_grid *grid = (struct ui_grid *)ctr;
    static u8 touch_action = 0;
    int sel_item;
    int rpc_ret = 0;
    int modem_state;

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
            printf("modem assert!!\n");
            mdctrl_enable_modem_assert();
            break;
        case 2:
            printf("modem reboot!!\n");
            send_msg_to_modem_ctrl(MSG_MODEM_RESET);
            break;
        case 3:
            printf("modem poweroff!!\n");
            cat1_set_lte_onoff(false);
            break;
        case 4:
            printf("modem poweron!!\n");
            cat1_set_lte_onoff(true);
            break;
        case 5:
            printf("modem state!!\n");
            modem_state = modem_state_check();
            ui_text_set_text_by_id(CAT1_TEXT_MODEM_STATE, modem_state_str[modem_state], strlen(modem_state_str[modem_state]), FONT_DEFAULT);
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
REGISTER_UI_EVENT_HANDLER(CAT1_VLIST_MODEM_CTRL)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = modem_ctrl_ontouch,
};

static int text_modem_state_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct ui_text *text = (struct ui_text *)_ctrl;

    switch (event) {
    case ON_CHANGE_INIT:
        switch (text->elm.id) {
        case CAT1_TEXT_MODEM_STATE:
            break;
        }
        break;
    case ON_CHANGE_RELEASE:
        break;
    }
    return 0;
}
REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_MODEM_STATE)
.onchange = text_modem_state_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

//-------------一级菜单-cft result---------------//
static int LAYOUT_cft_result_ontouch(void *ctr, struct element_touch_event *e)
{
    struct layout *layout = (struct layout *)ctr;

    switch (e->event) {
    case ELM_EVENT_TOUCH_R_MOVE:
        LAYOUT_SCREEN_DISP;
        ui_hide(CAT1_LAYOUT_CFT_RESULT);
        ui_show(UOS_CAT1_LAYOUT_FIRST_MENU);
        return true;
        break;
    default:
        break;
    }

    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_LAYOUT_CFT_RESULT)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = LAYOUT_cft_result_ontouch,
};

static int text_cali_info_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct ui_text *text = (struct ui_text *)_ctrl;
    int rpc_ret = 0;
    int cali_info;
    char *cali = malloc(sizeof(char) * 2);

    switch (event) {
    case ON_CHANGE_INIT:
        switch (text->elm.id) {
        case CAT1_TEXT_CALI_INFO:
            cali_info = ual_tele_radio_get_cali_data(&rpc_ret);
            printf("rpc_ret = %d\n", rpc_ret);
            printf("cali_info:%d\n", cali_info);
            sprintf(cali, "%d", cali_info);
            ui_text_set_text_attrs(text, cali, strlen(cali), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        }
        break;
    case ON_CHANGE_RELEASE:
        break;
    }
    return 0;
}
REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_CALI_INFO)
.onchange = text_cali_info_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

//----------------一级菜单-about-----------------//
static int LAYOUT_about_ontouch(void *ctr, struct element_touch_event *e)
{
    struct layout *layout = (struct layout *)ctr;

    switch (e->event) {
    case ELM_EVENT_TOUCH_R_MOVE:
        ui_hide(CAT1_LAYOUT_ABOUT_LTE);
        ui_show(UOS_CAT1_LAYOUT_FIRST_MENU);
        return true;
        break;
    default:
        break;
    }

    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_LAYOUT_ABOUT_LTE)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = LAYOUT_about_ontouch,
};

static int about_ontouch(void *ctr, struct element_touch_event *e)
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
        case 1://VERSION
            ui_hide(CAT1_LAYOUT_ABOUT_LTE);
            ui_show(CAT1_LAYOUT_VERSION);
            break;
        case 2://THIRDPARTY
            ui_hide(CAT1_LAYOUT_ABOUT_LTE);
            ui_show(CAT1_LAYOUT_THIRDPARTY);
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
REGISTER_UI_EVENT_HANDLER(CAT1_VLIST_ABOUT_LTE)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = about_ontouch,
};

//-----------一级菜单-about-version-------------//
static int layout_version_ontouch(void *ctr, struct element_touch_event *e)
{
    struct layout *layout = (struct layout *)ctr;

    switch (e->event) {
    case ELM_EVENT_TOUCH_R_MOVE:
        ui_hide(CAT1_LAYOUT_VERSION);
        ui_show(CAT1_LAYOUT_ABOUT_LTE);
        return true;
        break;
    default:
        break;
    }

    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_LAYOUT_VERSION)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = layout_version_ontouch,
};

static int text_version_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct ui_text *text = (struct ui_text *)_ctrl;

    switch (event) {
    case ON_CHANGE_INIT:
        switch (text->elm.id) {
        case CAT1_TEXT_VERSION:

            break;
        }
        break;
    case ON_CHANGE_RELEASE:
        break;
    }
    return 0;
}
REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_VERSION)
.onchange = text_version_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

//---------------一级菜单-about-third party------------------//
static int layout_thirdparty_ontouch(void *ctr, struct element_touch_event *e)
{
    struct layout *layout = (struct layout *)ctr;

    switch (e->event) {
    case ELM_EVENT_TOUCH_R_MOVE:
        ui_hide(CAT1_LAYOUT_THIRDPARTY);
        ui_show(CAT1_LAYOUT_ABOUT_LTE);
        return true;
        break;
    default:
        break;
    }

    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_LAYOUT_THIRDPARTY)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = layout_thirdparty_ontouch,
};

static int text_thirdparty_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct ui_text *text = (struct ui_text *)_ctrl;

    switch (event) {
    case ON_CHANGE_INIT:
        switch (text->elm.id) {
        case CAT1_TEXT_THIRDPARTY:

            break;
        }
        break;
    case ON_CHANGE_RELEASE:
        break;
    }
    return 0;
}
REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_THIRDPARTY)
.onchange = text_thirdparty_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};


//-----------------一级菜单-仲裁模块-----------------//
struct arbitration_module {
    int is_background_mode;
    int is_sport_mode;
    int is_call_mode;
    int arbitration_item_memory;
};
struct arbitration_module am = {0};

static int LAYOUT_arbitration_ontouch(void *ctr, struct element_touch_event *e)
{
    struct layout *layout = (struct layout *)ctr;

    switch (e->event) {
    case ELM_EVENT_TOUCH_R_MOVE:
        ui_hide(CAT1_LAYOUT_ARBITRATION);
        ui_show(UOS_CAT1_LAYOUT_FIRST_MENU);
        return true;
        break;
    default:
        break;
    }

    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_LAYOUT_ARBITRATION)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = LAYOUT_arbitration_ontouch,
};

static void arbitration_item_get(struct ui_grid *grid)
{
    am.arbitration_item_memory = ui_grid_get_hindex(grid);
    if (am.arbitration_item_memory >= grid->avail_item_num) {
        am.arbitration_item_memory = 0;
    }
}
static void arbitration_item_set(struct ui_grid *grid, int item)
{
    ui_grid_set_hi_index(grid, item);
}

static int arbitration_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct ui_grid *grid = (struct ui_grid *)_ctrl;

    switch (event) {
    case ON_CHANGE_INIT:
        printf("first_menu!\n");
        ui_grid_set_slide_direction(grid, SCROLL_DIRECTION_UD);
        arbitration_item_set(grid, am.arbitration_item_memory);
        //set_ui_sys_param(ConnNewPhone, 0);
        break;
    case ON_CHANGE_RELEASE:
        arbitration_item_get(grid);
        break;
    }
    return 0;
}

static int arbitration_ontouch(void *ctr, struct element_touch_event *e)
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


            break;
        case 2:


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
REGISTER_UI_EVENT_HANDLER(CAT1_VLIST_ARBITRATION)//仲裁模块显示
.onchange = arbitration_onchange,
 .onkey = NULL,
  .ontouch = arbitration_ontouch,
};


static int pic_background_mode_set_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct ui_pic *pic = (struct ui_pic *)_ctrl;

    switch (event) {
    case ON_CHANGE_INIT:
        switch (pic->elm.id) {
        case CAT1_PIC_BACKGROUND_MODE_SEL:
            ui_pic_set_image_index(pic, !!am.is_background_mode);
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

static int pic_background_mode_set_ontouch(void *ctr, struct element_touch_event *e)
{
    struct ui_pic *pic = (struct ui_pic *)ctr;
    static u8 touch_action = 0;
    int rpc_ret = 0;

    switch (e->event) {
    case ELM_EVENT_TOUCH_UP:
        if (touch_action != 1) {
            break;
        }

        switch (pic->elm.id) {
        case CAT1_PIC_BACKGROUND_MODE_SEL:
            am.is_background_mode = !am.is_background_mode;
            if (am.is_background_mode == 1) {
                am.is_sport_mode = 0;
                am.is_call_mode = 0;
            }
            ui_pic_show_image_by_id(pic->elm.id, !!am.is_background_mode);//通过索引显示图片
            ui_hide(CAT1_LAYOUT_ARBITRATION);
            ui_show(CAT1_LAYOUT_ARBITRATION);

            if (am.is_background_mode == 1) {
                int res = ual_rf_am_set_mode(UAL_RF_AM_BACKGROUND_POSITION_MODE, &rpc_ret);
                printf("res:%d\n", res);
                printf("rpc_ret = %d\n", rpc_ret);
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
REGISTER_UI_EVENT_HANDLER(CAT1_PIC_BACKGROUND_MODE_SEL)
.onchange = pic_background_mode_set_onchange,
 .onkey = NULL,
  .ontouch = pic_background_mode_set_ontouch,
};

static int pic_sport_mode_set_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct ui_pic *pic = (struct ui_pic *)_ctrl;

    switch (event) {
    case ON_CHANGE_INIT:
        switch (pic->elm.id) {
        case CAT1_PIC_SPORT_MODE_SEL:
            ui_pic_set_image_index(pic, !!am.is_sport_mode);
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

static int pic_sport_mode_set_ontouch(void *ctr, struct element_touch_event *e)
{
    struct ui_pic *pic = (struct ui_pic *)ctr;
    static u8 touch_action = 0;
    int rpc_ret = 0;

    switch (e->event) {
    case ELM_EVENT_TOUCH_UP:
        if (touch_action != 1) {
            break;
        }

        switch (pic->elm.id) {
        case CAT1_PIC_SPORT_MODE_SEL:
            am.is_sport_mode = !am.is_sport_mode;
            if (am.is_sport_mode == 1) {
                am.is_background_mode = 0;
                am.is_call_mode = 0;
            }
            ui_pic_show_image_by_id(pic->elm.id, !!am.is_sport_mode);//通过索引显示图片
            ui_hide(CAT1_LAYOUT_ARBITRATION);
            ui_show(CAT1_LAYOUT_ARBITRATION);

            if (am.is_sport_mode == 1) {
                int res = ual_rf_am_set_mode(UAL_RF_AM_SPORT_POSITION_MODE, &rpc_ret);
                printf("res:%d\n", res);
                printf("rpc_ret = %d\n", rpc_ret);
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
REGISTER_UI_EVENT_HANDLER(CAT1_PIC_SPORT_MODE_SEL)//仲裁模块开关
.onchange = pic_sport_mode_set_onchange,
 .onkey = NULL,
  .ontouch = pic_sport_mode_set_ontouch,
};

static int pic_call_mode_set_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct ui_pic *pic = (struct ui_pic *)_ctrl;

    switch (event) {
    case ON_CHANGE_INIT:
        switch (pic->elm.id) {
        case CAT1_PIC_CALL_MODE_SEL:
            ui_pic_set_image_index(pic, !!am.is_call_mode);
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

static int pic_call_mode_set_ontouch(void *ctr, struct element_touch_event *e)
{
    struct ui_pic *pic = (struct ui_pic *)ctr;
    static u8 touch_action = 0;
    int rpc_ret = 0;

    switch (e->event) {
    case ELM_EVENT_TOUCH_UP:
        if (touch_action != 1) {
            break;
        }

        switch (pic->elm.id) {
        case CAT1_PIC_CALL_MODE_SEL:
            am.is_call_mode = !am.is_call_mode;
            if (am.is_call_mode == 1) {
                am.is_background_mode = 0;
                am.is_sport_mode = 0;
            }
            ui_pic_show_image_by_id(pic->elm.id, !!am.is_call_mode);//通过索引显示图片
            ui_hide(CAT1_LAYOUT_ARBITRATION);
            ui_show(CAT1_LAYOUT_ARBITRATION);

            if (am.is_call_mode == 1) {
                int res = ual_rf_am_set_mode(UAL_RF_AM_COMMUNICATION_MODE, &rpc_ret);
                printf("res:%d\n", res);
                printf("rpc_ret = %d\n", rpc_ret);
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
REGISTER_UI_EVENT_HANDLER(CAT1_PIC_CALL_MODE_SEL)//仲裁模块开关
.onchange = pic_call_mode_set_onchange,
 .onkey = NULL,
  .ontouch = pic_call_mode_set_ontouch,
};

//---------------------一级菜单-GNSS---------------------//
ual_gnss_start_param_t gnss_param = {.start_mode = UAL_GNSS_START_MODE_COLD};
int is_enter_gnss_test = 0;
static int LAYOUT_GNSS_ontouch(void *ctr, struct element_touch_event *e)
{
    struct layout *layout = (struct layout *)ctr;

    switch (e->event) {
    case ELM_EVENT_TOUCH_R_MOVE:
        ui_hide(CAT1_LAYOUT_GNSS);
        ui_show(UOS_CAT1_LAYOUT_FIRST_MENU);
        return true;
        break;
    default:
        break;
    }

    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_LAYOUT_GNSS)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = LAYOUT_GNSS_ontouch,
};

//-------------一级菜单-GNSS-周期定位测试-定位启动方式--------------------//
static int layout_locate_start_ontouch(void *ctr, struct element_touch_event *e)
{
    struct layout *layout = (struct layout *)ctr;

    switch (e->event) {
    case ELM_EVENT_TOUCH_R_MOVE:
        ui_hide(CAT1_LAYOUT_LOCATE_START);
        ui_show(CAT1_LAYOUT_PERIOD_POSITION);
        return true;
        break;
    default:
        break;
    }

    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_LAYOUT_LOCATE_START)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = layout_locate_start_ontouch,
};

//------------------------GNSS启动方式选择----------------------------------//
int locate_start_mode = 0;
char *period_locate_mode[] = {"hot", "cold", "warm", "factory"};
static int locate_start_sure_ontouch(void *ctr, struct element_touch_event *e)
{
    struct ui_text *text = (struct ui_text *)ctr;


    switch (e->event) {
    case ELM_EVENT_TOUCH_UP:
        locate_start_mode = ui_grid_get_hindex(ui_grid_for_id(CAT1_VLIST_LOCATE_START));//获取定位启动模式
        printf("network mode:%d\n", locate_start_mode);
        gnss_param.start_mode = locate_start_mode;
        ui_hide(CAT1_LAYOUT_LOCATE_START);
        ui_show(CAT1_LAYOUT_PERIOD_POSITION);
        break;
    case ELM_EVENT_TOUCH_HOLD:
        break;
    case ELM_EVENT_TOUCH_MOVE:
        break;
    case ELM_EVENT_TOUCH_DOWN:
        return true;
    case ELM_EVENT_TOUCH_U_MOVE:
        break;
    case ELM_EVENT_TOUCH_D_MOVE:
        break;
    }

    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_LOCATE_START_SURE)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = locate_start_sure_ontouch,
};

static int text_locate_start_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct ui_text *text = (struct ui_text *)_ctrl;

    switch (event) {
    case ON_CHANGE_INIT:
        switch (text->elm.id) {
        case CAT1_TEXT_LOCATE_START:
            ui_text_set_text_attrs(text, period_locate_mode[locate_start_mode], strlen(period_locate_mode[locate_start_mode]), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
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
REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_LOCATE_START)
.onchange = text_locate_start_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

static int gnss_ontouch(void *ctr, struct element_touch_event *e)
{
    struct ui_grid *grid = (struct ui_grid *)ctr;
    static u8 touch_action = 0;
    int sel_item;
    int rpc_ret = 0;

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
            ui_hide(CAT1_LAYOUT_GNSS);
            ui_show(CAT1_LAYOUT_GNSS_MODE);
            break;
        case 2:


            break;
        case 3:
            ui_hide(CAT1_LAYOUT_GNSS);
            ui_show(CAT1_LAYOUT_PERIOD_POSITION);
            break;
        case 4:
            ual_gnss_start(&gnss_param, &rpc_ret);                  //开始定位
            ui_hide(CAT1_LAYOUT_GNSS);
            ui_show(CAT1_LAYOUT_GPS_TEST_SEARCHING);
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
REGISTER_UI_EVENT_HANDLER(CAT1_VLIST_GNSS)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = gnss_ontouch,
};

//nmealog开关
int is_nmealog = 0;
ual_gnss_nmea_output_param_t gnss_nmea_param_start = {.nmea_type = UAL_GNSS_NMEA_ALL_TYPE, .time_interval = 10};
ual_gnss_nmea_output_param_t gnss_nmea_param_stop = {.nmea_type = UAL_GNSS_NMEA_NONE_TYPE, .time_interval = 10};
static int pic_nmealog_set_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct ui_pic *pic = (struct ui_pic *)_ctrl;

    switch (event) {
    case ON_CHANGE_INIT:
        switch (pic->elm.id) {
        case CAT1_PIC_NMEALOG_SEL:
            ui_pic_set_image_index(pic, !!is_nmealog);
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

static int pic_nmealog_set_ontouch(void *ctr, struct element_touch_event *e)
{
    struct ui_pic *pic = (struct ui_pic *)ctr;
    static u8 touch_action = 0;
    int rpc_ret = 0;
    int res;

    switch (e->event) {
    case ELM_EVENT_TOUCH_UP:
        if (touch_action != 1) {
            break;
        }

        switch (pic->elm.id) {
        case CAT1_PIC_NMEALOG_SEL:
            is_nmealog = !is_nmealog;
            ui_pic_show_image_by_id(pic->elm.id, !!is_nmealog);//通过索引显示图片

            if (is_nmealog == 0) {               //nmea开关接口
                printf("nmea stop!\n");
                res = ual_gnss_output_nmea(&gnss_nmea_param_stop, &rpc_ret);
                printf("res:%d\n", res);
                printf("rpc_ret = %d\n", rpc_ret);
            } else {
                printf("nmea start!\n");
                res = ual_gnss_output_nmea(&gnss_nmea_param_start, &rpc_ret);
                printf("res:%d\n", res);
                printf("rpc_ret = %d\n", rpc_ret);
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
REGISTER_UI_EVENT_HANDLER(CAT1_PIC_NMEALOG_SEL)//NMEALOG开关
.onchange = pic_nmealog_set_onchange,
 .onkey = NULL,
  .ontouch = pic_nmealog_set_ontouch,
};

//-------------一级菜单-GNSS-GPS TEST----------------------//
struct s_show_satellite_info {
    char prn[3];
    char cn0[3];
    char elevation[3];
    char azimuth[3];
    char is_used[2];
};
struct s_show_gps_info {
    char longitude[10];
    char latitude[10];
    char height[10];
    char ttff_time[10];
    char satellite_num[3];
    struct s_show_satellite_info show_satellite_info[UAL_GNSS_SATELLITE_MAX_NUM];
};

struct s_show_gps_info show_gps_info = {0};

void gps_info_float2str(float src, char *dst)
{
    sprintf(dst, "%f", src);
}

static int layout_gps_test_ontouch(void *ctr, struct element_touch_event *e)
{
    struct layout *layout = (struct layout *)ctr;
    int rpc_ret = 0;

    switch (e->event) {
    case ELM_EVENT_TOUCH_R_MOVE:
        ual_gnss_stop(&rpc_ret);
        is_enter_gnss_test = 0;
        ui_hide(CAT1_LAYOUT_GPS_TEST);
        ui_show(CAT1_LAYOUT_GNSS);
        return true;
        break;
    default:
        break;
    }

    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_LAYOUT_GPS_TEST)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = layout_gps_test_ontouch,
};

static int text_gps_test_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct ui_text *text = (struct ui_text *)_ctrl;
    extern struct s_gps_info gps_info;

    switch (event) {
    case ON_CHANGE_INIT:
        break;
    case ON_CHANGE_SHOW:
        switch (text->elm.id) {
        case CAT1_TEXT_GPS_LONGITUDE:
            gps_info_float2str(gps_info.longitude, show_gps_info.longitude);
            ui_text_set_text_attrs(text, show_gps_info.longitude, strlen(show_gps_info.longitude), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_GPS_LATITUDE:
            gps_info_float2str(gps_info.latitude, show_gps_info.latitude);
            ui_text_set_text_attrs(text, show_gps_info.latitude, strlen(show_gps_info.latitude), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_GPS_HEIGHT:
            gps_info_float2str(gps_info.height, show_gps_info.height);
            ui_text_set_text_attrs(text, show_gps_info.height, strlen(show_gps_info.height), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_GPS_TTFF:
            gps_info_float2str(gps_info.ttff_time, show_gps_info.ttff_time);
            ui_text_set_text_attrs(text, show_gps_info.ttff_time, strlen(show_gps_info.ttff_time), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_GPS_SATELLITE_NUM:
            gps_info_float2str(gps_info.satellite_num, show_gps_info.satellite_num);
            ui_text_set_text_attrs(text, show_gps_info.satellite_num, strlen(show_gps_info.satellite_num), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_GPS_PRN:
            gps_info_float2str(gps_info.satellite_info[0].satellite_identifier, show_gps_info.show_satellite_info[0].prn);
            ui_text_set_text_attrs(text, show_gps_info.show_satellite_info[0].prn, strlen(show_gps_info.show_satellite_info[0].prn), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_GPS_SNR:
            gps_info_float2str(gps_info.satellite_info[0].cn0, show_gps_info.show_satellite_info[0].cn0);
            ui_text_set_text_attrs(text, show_gps_info.show_satellite_info[0].cn0, strlen(show_gps_info.show_satellite_info[0].cn0), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_GPS_ELEVATION:
            gps_info_float2str(gps_info.satellite_info[0].elevation, show_gps_info.show_satellite_info[0].elevation);
            ui_text_set_text_attrs(text, show_gps_info.show_satellite_info[0].elevation, strlen(show_gps_info.show_satellite_info[0].elevation), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_GPS_AZIMUTH:
            gps_info_float2str(gps_info.satellite_info[0].azimuth, show_gps_info.show_satellite_info[0].azimuth);
            ui_text_set_text_attrs(text, show_gps_info.show_satellite_info[0].azimuth, strlen(show_gps_info.show_satellite_info[0].azimuth), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        case CAT1_TEXT_GPS_USED:
            gps_info_float2str(gps_info.satellite_info[0].is_used, show_gps_info.show_satellite_info[0].is_used);
            ui_text_set_text_attrs(text, show_gps_info.show_satellite_info[0].is_used, strlen(show_gps_info.show_satellite_info[0].is_used), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        }
        break;
    case ON_CHANGE_RELEASE:
        extern int gnss_timer;
        sys_timer_del(gnss_timer);
        break;
    }
    return 0;
}
REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_GPS_LONGITUDE)
.onchange = text_gps_test_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_GPS_LATITUDE)
.onchange = text_gps_test_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_GPS_HEIGHT)
.onchange = text_gps_test_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_GPS_TTFF)
.onchange = text_gps_test_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_GPS_SATELLITE_NUM)
.onchange = text_gps_test_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_GPS_PRN)
.onchange = text_gps_test_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_GPS_SNR)
.onchange = text_gps_test_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_GPS_ELEVATION)
.onchange = text_gps_test_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_GPS_AZIMUTH)
.onchange = text_gps_test_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_GPS_USED)
.onchange = text_gps_test_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};
//---------------一级菜单-GNSS-GNSS MODE设置------------------//
static int layout_gnss_mode_ontouch(void *ctr, struct element_touch_event *e)
{
    struct layout *layout = (struct layout *)ctr;

    switch (e->event) {
    case ELM_EVENT_TOUCH_R_MOVE:
        ui_hide(CAT1_LAYOUT_GNSS_MODE);
        ui_show(CAT1_LAYOUT_GNSS);
        return true;
        break;
    default:
        break;
    }

    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_LAYOUT_GNSS_MODE)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = layout_gnss_mode_ontouch,
};

int gnss_mode = 0;
char *gnss_mode_str[7] = {"GPS", "BDS", "GPS+BDS", "GLONASS", "GPS+GLONASS", "GPS+BDS+GALILEO", "GPS+BDS+GLONASS+GALILEO"};
int gnss_mode_unisoc[7] = {UAL_GNSS_MODE_GPS, UAL_GNSS_MODE_BDS, UAL_GNSS_MODE_GPS_BDS, UAL_GNSS_MODE_GLONASS,
                           UAL_GNSS_MODE_GPS_GLONASS, UAL_GNSS_MODE_GPS_BDS_GALILEO, UAL_GNSS_MODE_GPS_B1C_GLONASS_GALILEO
                          };

static int gnss_mode_sure_ontouch(void *ctr, struct element_touch_event *e)
{
    struct ui_text *text = (struct ui_text *)ctr;
    int rpc_ret = 0;

    switch (e->event) {
    case ELM_EVENT_TOUCH_UP:
        gnss_mode = ui_grid_get_hindex(ui_grid_for_id(CAT1_VLIST_GNSS_MODE));//获取相应的gnss mode
        printf("gnss mode:%d\n", gnss_mode);
        printf("gnss mode index:%d\n", gnss_mode_unisoc[gnss_mode]);
        int res = ual_gnss_set_gnss_mode(gnss_mode_unisoc[gnss_mode], &rpc_ret);
        printf("res:%d\n", res);
        printf("rpc_ret = %d\n", rpc_ret);
        ui_hide(CAT1_LAYOUT_GNSS_MODE);
        ui_show(CAT1_LAYOUT_GNSS);
        break;
    case ELM_EVENT_TOUCH_HOLD:
        break;
    case ELM_EVENT_TOUCH_MOVE:
        break;
    case ELM_EVENT_TOUCH_DOWN:
        return true;
    case ELM_EVENT_TOUCH_U_MOVE:
        break;
    case ELM_EVENT_TOUCH_D_MOVE:
        break;
    }

    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_GNSS_MODE_SURE)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = gnss_mode_sure_ontouch,
};

static int text_gnss_mode_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct ui_text *text = (struct ui_text *)_ctrl;

    switch (event) {
    case ON_CHANGE_INIT:
        switch (text->elm.id) {
        case CAT1_TEXT_GNSS_MODE:
            ui_text_set_text_attrs(text, gnss_mode_str[gnss_mode], strlen(gnss_mode_str[gnss_mode]), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
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
REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_GNSS_MODE)
.onchange = text_gnss_mode_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

//-----------------一级菜单-GNSS-周期定位测试-----------------//
static int layout_period_position_ontouch(void *ctr, struct element_touch_event *e)
{
    struct layout *layout = (struct layout *)ctr;

    switch (e->event) {
    case ELM_EVENT_TOUCH_R_MOVE:
        ui_hide(CAT1_LAYOUT_PERIOD_POSITION);
        ui_show(CAT1_LAYOUT_GNSS);
        return true;
        break;
    default:
        break;
    }

    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_LAYOUT_PERIOD_POSITION)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = layout_period_position_ontouch,
};

static int period_position_ontouch(void *ctr, struct element_touch_event *e)
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
            ui_hide(CAT1_LAYOUT_PERIOD_POSITION);
            ui_show(CAT1_LAYOUT_LOCATE_START);
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
REGISTER_UI_EVENT_HANDLER(CAT1_VLIST_PERIOD_POSITION)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = period_position_ontouch,
};


//------------------一级菜单-wifi scan--------------------//
int is_enter_wifi_test = 0;
void bssid2mac(int *bssid, char *mac)
{
    int len = 0;
    for (int i = 5; i >= 0; i--) {
        if ((bssid[i] / 16) >= 10) {
            mac[len] = 'A' + bssid[i] / 16 - 10;
        } else {
            mac[len] = '0' + bssid[i] / 16;
        }
        if ((bssid[i] % 16) >= 10) {
            mac[len + 1] = 'A' + bssid[i] % 16 - 10;
        } else {
            mac[len + 1] = '0' + bssid[i] % 16;
        }
        len += 2;
        mac[len] = ':';
        len += 1;
    }
    mac[len - 1] = '\0';
}

struct s_show_wifi_info {
    char rssi[5];
    char wifi_mac[20];
};

struct s_show_wifi_info show_wifi_info = {
    .rssi = "0",
    .wifi_mac = "00:00:00:00:00:00",
};

static int LAYOUT_WIFI_SCAN_ontouch(void *ctr, struct element_touch_event *e)
{
    struct layout *layout = (struct layout *)ctr;

    switch (e->event) {
    case ELM_EVENT_TOUCH_R_MOVE:
        ui_hide(UOS_CAT1_LAYOUT_WIFI_SCAN);
        ui_show(UOS_CAT1_LAYOUT_FIRST_MENU);
        return true;
        break;
    default:
        break;
    }

    return false;
}
REGISTER_UI_EVENT_HANDLER(UOS_CAT1_LAYOUT_WIFI_SCAN)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = LAYOUT_WIFI_SCAN_ontouch,
};

static int wifi_scan_ontouch(void *ctr, struct element_touch_event *e)
{
    struct ui_grid *grid = (struct ui_grid *)ctr;
    static u8 touch_action = 0;
    int sel_item;
    int rpc_ret = 0;

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
        case 1://wifi扫描
            int rf_state = ual_rf_am_get_current_status(&rpc_ret);
            printf("rf_state is %d, rpc_ret is %d\n", rf_state, rpc_ret);
            if (rf_state == RF_AM_STATE_IDLE || rf_state == RF_AM_STATE_WIFISCAN_WORK) {
                printf("ual_wifi_open!!!!\n");
                int res = ual_wifi_open(&rpc_ret);               //wifi_scan功能打开
                printf("res:%d\n", res);
            }
            ui_hide(UOS_CAT1_LAYOUT_WIFI_SCAN);
            ui_show(CAT1_LAYOUT_WIFI_FUNC_TEST_SEARCHING);

            break;
        case 2:


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
REGISTER_UI_EVENT_HANDLER(UOS_CAT1_VLIST_WIFI_SCAN)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = wifi_scan_ontouch,
};

//-----------------wifi scan 功能测试-------------------//
static int layout_wifi_scan_func_test_ontouch(void *ctr, struct element_touch_event *e)
{
    struct layout *layout = (struct layout *)ctr;

    switch (e->event) {
    case ELM_EVENT_TOUCH_R_MOVE:
        is_enter_wifi_test = 0;
        ui_hide(CAT1_LAYOUT_WIFI_FUNC_TEST);
        ui_show(UOS_CAT1_LAYOUT_WIFI_SCAN);
        return true;
        break;
    default:
        break;
    }

    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_LAYOUT_WIFI_FUNC_TEST)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = layout_wifi_scan_func_test_ontouch,
};

static int text_wifi_scan_func_test_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct ui_text *text = (struct ui_text *)_ctrl;
    extern struct s_wifi_info wifi_info;
    switch (event) {
    case ON_CHANGE_INIT:
//        switch (text->elm.id) {
//        case CAT1_TEXT_WIFI_FUNC_TEST_MAC:
//            bssid2mac(wifi_info.mac_addr, show_wifi_info.wifi_mac);     //转换为mac格式
//            ui_text_set_text_attrs(text, show_wifi_info.wifi_mac, strlen(show_wifi_info.wifi_mac), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
//            break;
//
//        case CAT1_TEXT_WIFI_FUNC_TEST_RSSI:
//            sprintf(show_wifi_info.rssi, "%d", wifi_info.rssi);
//            ui_text_set_text_attrs(text, show_wifi_info.rssi, strlen(show_wifi_info.rssi), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
//            break;
//        }
        break;
    case ON_CHANGE_SHOW:
        switch (text->elm.id) {
        case CAT1_TEXT_WIFI_FUNC_TEST_MAC:
            //bssid2mac(wifi_info.mac_addr, show_wifi_info.wifi_mac);     //转换为mac格式
            ui_text_set_text_attrs(text, wifi_info.mac_addr, strlen(wifi_info.mac_addr), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;

        case CAT1_TEXT_WIFI_FUNC_TEST_RSSI:
            sprintf(show_wifi_info.rssi, "%d", wifi_info.rssi);
            ui_text_set_text_attrs(text, show_wifi_info.rssi, strlen(show_wifi_info.rssi), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        }
        break;
    case ON_CHANGE_RELEASE:
        break;
    }
    return 0;
}
REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_WIFI_FUNC_TEST_MAC)
.onchange = text_wifi_scan_func_test_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_WIFI_FUNC_TEST_RSSI)
.onchange = text_wifi_scan_func_test_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};


int is_wifi_test = 0;
static int pic_wifi_test_set_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct ui_pic *pic = (struct ui_pic *)_ctrl;

    switch (event) {
    case ON_CHANGE_INIT:
        switch (pic->elm.id) {
        case CAT1_PIC_WIFI_TEST_SEL:
            ui_pic_set_image_index(pic, !!is_wifi_test);
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

static int pic_wifi_test_set_ontouch(void *ctr, struct element_touch_event *e)
{
    struct ui_pic *pic = (struct ui_pic *)ctr;
    static u8 touch_action = 0;
    int rpc_ret = 0;
    int res;

    switch (e->event) {
    case ELM_EVENT_TOUCH_UP:
        if (touch_action != 1) {
            break;
        }

        switch (pic->elm.id) {
        case CAT1_PIC_WIFI_TEST_SEL:
            is_wifi_test = !is_wifi_test;
            ui_pic_show_image_by_id(pic->elm.id, !!is_wifi_test);//通过索引显示图片

            if (is_wifi_test == 1) {
                res = ual_tele_radio_set_fly_mode(true, &rpc_ret);
                printf("res:%d\n", res);
                printf("rpc_ret = %d\n", rpc_ret);
            } else {
                res = ual_tele_radio_set_fly_mode(false, &rpc_ret);
                printf("res:%d\n", res);
                printf("rpc_ret = %d\n", rpc_ret);
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
REGISTER_UI_EVENT_HANDLER(CAT1_PIC_WIFI_TEST_SEL)
.onchange = pic_wifi_test_set_onchange,
 .onkey = NULL,
  .ontouch = pic_wifi_test_set_ontouch,
};


//----------------------一级菜单-数据业务开关---------------------//
int is_data_service = 0;
static int pic_data_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct ui_pic *pic = (struct ui_pic *)_ctrl;

    switch (event) {
    case ON_CHANGE_INIT:
        switch (pic->elm.id) {
        case CAT1_PIC_DATA_SEL:
            ui_pic_set_image_index(pic, !!is_data_service);
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

static int pic_data_ontouch(void *ctr, struct element_touch_event *e)
{
    struct ui_pic *pic = (struct ui_pic *)ctr;
    static u8 touch_action = 0;
    int rpc_ret = 0;
    int res;

    switch (e->event) {
    case ELM_EVENT_TOUCH_UP:
        if (touch_action != 1) {
            break;
        }

        switch (pic->elm.id) {
        case CAT1_PIC_DATA_SEL:
            is_data_service = !is_data_service;
            ui_pic_show_image_by_id(pic->elm.id, !!is_data_service);//通过索引显示图片

            if (is_data_service == 0) {
                printf("data deactivate!\n");
                res = ual_tele_data_deactive(0, &rpc_ret);
                printf("res:%d\n", res);
                printf("rpc_ret = %d\n", rpc_ret);
            } else {
                printf("data activate!\n");
                res = ual_tele_data_active(0, &rpc_ret);
                printf("res:%d\n", res);
                printf("rpc_ret = %d\n", rpc_ret);
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
REGISTER_UI_EVENT_HANDLER(CAT1_PIC_DATA_SEL)
.onchange = pic_data_onchange,
 .onkey = NULL,
  .ontouch = pic_data_ontouch,
};

//------------------------一级菜单-睡眠设置-------------------------//
static int layout_sleep_mode_ontouch(void *ctr, struct element_touch_event *e)
{
    struct layout *layout = (struct layout *)ctr;

    switch (e->event) {
    case ELM_EVENT_TOUCH_R_MOVE:
        is_enter_wifi_test = 0;
        ui_hide(CAT1_LAYOUT_SLEEP_MODE);
        ui_show(UOS_CAT1_LAYOUT_FIRST_MENU);
        return true;
        break;
    default:
        break;
    }

    return false;
}
REGISTER_UI_EVENT_HANDLER(CAT1_LAYOUT_SLEEP_MODE)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = layout_sleep_mode_ontouch,
};

static int sleep_mode_ontouch(void *ctr, struct element_touch_event *e)
{
    struct ui_grid *grid = (struct ui_grid *)ctr;
    static u8 touch_action = 0;
    int sel_item;
    int rpc_ret = 0;
    char responde[100];
    int responde_len;

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
            printf("sleep diasble!!\n");
            at_send_sync("AT+PMMODE=0", &responde, &responde_len);
            break;
        case 2:
            printf("sleep pm0!!\n");
            at_send_sync("AT+PMMODE=1", &responde, &responde_len);
            break;
        case 3:
            printf("sleep pm1!!\n");
            at_send_sync("AT+PMMODE=2", &responde, &responde_len);
            break;
        case 4:
            printf("sleep pm2!!\n");
            at_send_sync("AT+PMMODE=3", &responde, &responde_len);
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
REGISTER_UI_EVENT_HANDLER(CAT1_VLIST_SLEEP_MODE)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = sleep_mode_ontouch,
};

#endif








