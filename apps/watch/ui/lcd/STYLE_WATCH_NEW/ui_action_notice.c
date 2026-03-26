#include "app_config.h"
#include "ui/ui_api.h"
#include "ui/ui.h"
#include "ui/ui_style.h"
#include "app_task.h"
#include "system/timer.h"
#include "app_main.h"
#include "init.h"
#include "key_event_deal.h"
#include "ui_vm/ui_vm.h"
#include "message_vm_cfg.h"
#include "cat1/cat1_common.h"

/* #pragma bss_seg(".ui_not_bss") */
/* #pragma data_seg(".ui_not_data") */


#define LOG_TAG_CONST       UI
#define LOG_TAG     		"[UI-ACTION]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"


#ifdef CONFIG_UI_STYLE_JL_ENABLE
#if TCFG_UI_ENABLE_NOTICE

/* #define STYLE_NAME  DIAL */
#define STYLE_NAME  JL

REGISTER_UI_STYLE(STYLE_NAME)

#define ui_grid_for_id(id) \
	({ \
		struct element *elm = ui_core_get_element_by_id(id); \
	 	elm ? container_of(elm, struct ui_grid, elm): NULL; \
	 })

#define ui_pic_for_id(id) \
		(struct ui_pic *)ui_core_get_element_by_id(id)

#define MESSAGE_DETAIL_PAGE     PAGE_65

#define PACKAGE_NAME_SYS_MESSAGE_RECEIVE        "MobileSMS_RECEIVE"
#define PACKAGE_NAME_SYS_MESSAGE_SEND           "MobileSMS_SEND"

#define IOS_PACKAGE_NAME_SYS_MESSAGE    "com.apple.MobileSMS"
#define IOS_PACKAGE_NAME_WECHAT         "com.tencent.xin"
#define IOS_PACKAGE_NAME_QQ             "com.tencent.mqq"
#define IOS_PACKAGE_NAME_DING_DING      "com.laiwang.DingTalk"


#define MESSAGE_MAX_NUM     10          /**< 支持存储的最大消息数量 */
#define MESSAGE_PACKAGENAME_LEN    32   /**< 通知消息包名数据长度 */
#define MESSAGE_APPIDENTIFIER_LEN   1   /**< 通知消息app标识符数据长度 0:通用 1:短信 2:微信 3:QQ 4:钉钉 其他:未定义 */
#define MESSAGE_TITLE_LEN   37          /**< 通知消息标题数据长度 */
#define MESSAGE_CONTENT_LEN 441         /**< 通知消息内容数据长度 */
#define MESSAGE_TIMESTAMP_LEN  4        /**< 通知消息时间戳数据长度 */

struct __UI_MESSAGE {
    u32 TimeStamp;
    u8 packagename[MESSAGE_PACKAGENAME_LEN];
    u8 AppIdentifier;
    u8 title[MESSAGE_TITLE_LEN];
    u8 content[MESSAGE_CONTENT_LEN];

    u8 packagename_len;
    u8 title_len;
    u16 content_len;

    u8 title_encode;
    u8 title_flags;
    u16 title_icon_Xcoordinate;
    u16 title_text_Xcoordinate;

};

extern u16 ui_get_text_width_and_height(u8 encode, u8 *str, u16 strlen, u16 elm_width, u16 elm_height, u8 flags, char *value_type);


static struct __UI_MESSAGE temp_message_buffer;
static u8 message_select_index = 0;
static u8 create_control_by_menu;
static u8 message_num = 0;
static u8 message_index[3];

#define MESSAGE_MALLOC_ENABLE       1

#define MESSAGE_STORE_PATH      "storage/sd1/C/DOWNLOAD/message.txt"

static struct __UI_MESSAGE *message_store_buffer[3];


static u32 message_offset_tab[MESSAGE_MAX_NUM];
static u32 timestamp_tab[MESSAGE_MAX_NUM];
static u16 message_data_len[MESSAGE_MAX_NUM];

void *notice_info_create(void)
{
    return zalloc(sizeof(struct __UI_MESSAGE));
}

void notice_info_release(void *info)
{
    if (info) {
        free(info);
    }
}

void notice_set_info_from(void *info, void *name, void *data, u16 len)
{
    u32 copy_len;
    struct __UI_MESSAGE *p_msg = info;

    if (!strcmp((char *)name, "UID")) {
        memcpy(&p_msg->TimeStamp, (u8 *)data, sizeof(p_msg->TimeStamp));
        /* log_info("TimeStamp:%d", p_msg->TimeStamp); */
    } else if (!strcmp((char *)name, "AppIdentifier")) {
        copy_len = (len + 1) > MESSAGE_PACKAGENAME_LEN ? MESSAGE_PACKAGENAME_LEN : (len + 1);
        p_msg->packagename_len = copy_len;
        memset(p_msg->packagename, 0, MESSAGE_PACKAGENAME_LEN);
        memcpy(p_msg->packagename, (u8 *)data, copy_len);
        /* log_info("packagename:%s", p_msg->packagename); */
        if (!strcmp((char *)data, PACKAGE_NAME_SYS_MESSAGE_SEND)) {
            p_msg->AppIdentifier = 0;
        } else if (!strcmp((char *)data, PACKAGE_NAME_SYS_MESSAGE_RECEIVE)) {
            p_msg->AppIdentifier = 1;
        } else if (!strcmp((char *)data, IOS_PACKAGE_NAME_SYS_MESSAGE)) {
            p_msg->AppIdentifier = 2;
        } else if (!strcmp((char *)data, IOS_PACKAGE_NAME_WECHAT)) {
            p_msg->AppIdentifier = 3;
        } else if (!strcmp((char *)data, IOS_PACKAGE_NAME_QQ)) {
            p_msg->AppIdentifier = 4;
        } else if (!strcmp((char *)data, IOS_PACKAGE_NAME_DING_DING)) {
            p_msg->AppIdentifier = 5;
        } else {
            p_msg->AppIdentifier = 6;
        }
        /* log_info("AppIdentifier:%d", p_msg->AppIdentifier); */
    } else if (!strcmp((char *)name, "IDTitle")) {
        copy_len = (len + 1) > MESSAGE_TITLE_LEN ? MESSAGE_TITLE_LEN : (len + 1);
        p_msg->title_len = copy_len;
        memset(p_msg->title, 0, MESSAGE_TITLE_LEN);
        memcpy(p_msg->title, (u8 *)data, copy_len);
        log_info("%s %d %d\n", data, len, __LINE__);
    } else if (!strcmp((char *)name, "IDMessage")) {
        copy_len = (len + 1) > MESSAGE_CONTENT_LEN ? MESSAGE_CONTENT_LEN : (len + 1);
        p_msg->content_len = copy_len;
        memset(p_msg->content, 0, MESSAGE_CONTENT_LEN);
        memcpy(p_msg->content, (u8 *)data, copy_len);
        log_info("id_msg:%s %d %d\n", data, len, __LINE__);
    }
}

u8 message_data_analysis(int index, u8 store_buf_index)
{
    int len;
    u8 *data;
    if (index >= flash_message_count()) {
        log_info("index over");
        return 1;
    }
    data = zalloc(512);
    if (!data) {
        log_info("data malloc fail");
        return 1;
    }

    len = flash_message_read_by_index(flash_message_count() - 1 - index, data, 512);
    if (!len) {
        log_info("len err is 0 < %s %d>\n", __FUNCTION__, __LINE__);
    }

    if (!message_store_buffer[store_buf_index]) {
        message_store_buffer[store_buf_index] = (struct __UI_MESSAGE *)zalloc(sizeof(struct __UI_MESSAGE));
        if (!message_store_buffer[store_buf_index]) {
            log_info("message_store_buffer malloc fail");
            free(data);
            return 1;
        }
    }

    u8 T;
    u8 *V;
    u16 L;
    u16 offset = 0;
    u32 copy_len;
    struct __UI_MESSAGE *temp_message_buf;
    temp_message_buf = message_store_buffer[store_buf_index];
    memset(temp_message_buf, 0, sizeof(struct __UI_MESSAGE));
    /* log_info("index:%d store_buf_index:%d",index,store_buf_index); */

    while (offset < len) {
        L = (data[offset] << 8) | data[offset + 1];
        T = data[offset + 2];
        V = &data[offset + 3];
        if (T == 0) {
            copy_len = (L - 1) > MESSAGE_TIMESTAMP_LEN ? MESSAGE_TIMESTAMP_LEN : (L - 1);
            memcpy(&temp_message_buf->TimeStamp, V, copy_len);
            /* log_info("TimeStamp:0x%x",temp_message_buf->TimeStamp); */
        } else if (T == 1) {
            copy_len = (L - 1) > MESSAGE_PACKAGENAME_LEN ? MESSAGE_PACKAGENAME_LEN : (L - 1);
            memcpy(temp_message_buf->packagename, V, copy_len);
            /* log_info("packagename:%s", temp_message_buf->packagename); */
        } else if (T == 2) {
            copy_len = (L - 1) > MESSAGE_APPIDENTIFIER_LEN ? MESSAGE_APPIDENTIFIER_LEN : (L - 1);
            memcpy(&temp_message_buf->AppIdentifier, V, copy_len);
            /* log_info("AppIdentifier:%d", temp_message_buf->AppIdentifier); */
        } else if (T == 3) {
            copy_len = (L - 1) > MESSAGE_TITLE_LEN ? MESSAGE_TITLE_LEN : (L - 1);
            memcpy(temp_message_buf->title, V, copy_len);
            /* log_info("title:%s", temp_message_buf->title); */
        } else if (T == 4) {
            copy_len = (L - 1) > MESSAGE_CONTENT_LEN ? MESSAGE_CONTENT_LEN : (L - 1);
            memcpy(temp_message_buf->content, V, copy_len);
            /* log_info("copy_len:%d",copy_len); */
            /* log_info("content:%s", temp_message_buf->content); */
        }
        offset += L + 2;
    }

    //图标和标题居中处理
    u16 title_width;
    u16 icon_width = 24;            //图标控件宽度
    u16 title_elm_width = 160;      //标题控件宽度
    u16 title_elm_height = 40;      //标题控件高度
    u16 total_width;

    temp_message_buf->title_encode = FONT_ENCODE_UTF8;
    temp_message_buf->title_flags = FONT_DEFAULT;
    title_width = ui_get_text_width_and_height(temp_message_buf->title_encode,
                  temp_message_buf->title,
                  strlen(temp_message_buf->title),
                  title_elm_width, title_elm_height,
                  temp_message_buf->title_flags,
                  "width");
    total_width = title_width + icon_width;
    temp_message_buf->title_icon_Xcoordinate = (240 - total_width) / 2;   //根据屏幕大小、标题宽度、图标宽度进行居中处理
    temp_message_buf->title_text_Xcoordinate = temp_message_buf->title_icon_Xcoordinate + icon_width;

    free(data);
    return 0;
}

void notice_add_info_from(void *info)
{
    u32 offset = 0;
    u8 *temp_ptr = 0;
    struct __UI_MESSAGE *p_msg = info;
    u32 mess_data_len = MESSAGE_TIMESTAMP_LEN + p_msg->packagename_len +
                        MESSAGE_APPIDENTIFIER_LEN + p_msg->title_len +
                        p_msg->content_len + 3 * 5;

    temp_ptr = zalloc(mess_data_len);
    if (!temp_ptr) {
        log_info("temp_ptr malloc fail");
        return;
    }

    temp_ptr[offset] = (MESSAGE_TIMESTAMP_LEN + 1) >> 8;
    offset++;
    temp_ptr[offset] = (MESSAGE_TIMESTAMP_LEN + 1) & 0xff;
    offset++;
    temp_ptr[offset] = 0;
    offset++;
    memcpy(&temp_ptr[offset], &p_msg->TimeStamp, MESSAGE_TIMESTAMP_LEN);
    offset += MESSAGE_TIMESTAMP_LEN;

    temp_ptr[offset] = (p_msg->packagename_len + 1) >> 8;
    offset++;
    temp_ptr[offset] = (p_msg->packagename_len + 1) & 0xff;
    offset++;
    temp_ptr[offset] = 1;
    offset++;
    memcpy(&temp_ptr[offset], p_msg->packagename, p_msg->packagename_len);
    offset += p_msg->packagename_len;

    temp_ptr[offset] = (MESSAGE_APPIDENTIFIER_LEN + 1) >> 8;
    offset++;
    temp_ptr[offset] = (MESSAGE_APPIDENTIFIER_LEN + 1) & 0xff;
    offset++;
    temp_ptr[offset] = 2;
    offset++;
    temp_ptr[offset] = p_msg->AppIdentifier;
    offset += MESSAGE_APPIDENTIFIER_LEN;

    temp_ptr[offset] = (p_msg->title_len + 1) >> 8;
    offset++;
    temp_ptr[offset] = (p_msg->title_len + 1) & 0xff;
    offset++;
    temp_ptr[offset] = 3;
    offset++;
    memcpy(&temp_ptr[offset], p_msg->title, p_msg->title_len);
    offset += p_msg->title_len;

    temp_ptr[offset] = (p_msg->content_len + 1) >> 8;
    offset++;
    temp_ptr[offset] = (p_msg->content_len + 1) & 0xff;
    offset++;
    temp_ptr[offset] = 4;
    offset++;
    memcpy(&temp_ptr[offset], p_msg->content, p_msg->content_len);

    //////////////////////////////////////////////
    flash_message_write(temp_ptr, mess_data_len);
    ///////////////////////////////////////
    free(temp_ptr);

    // UI SHOW
#if 0
    if (flash_message_count() < 10) {
        UI_MSG_POST("message_status:event=%4", 1);
    } else {
        UI_MSG_POST("message_status:event=%4", 3);
    }
#else
    u8 cur_task = app_get_curr_task();
    switch (cur_task) {
    case APP_POWERON_TASK:
    case APP_POWEROFF_TASK:
    case APP_WATCH_UPDATE_TASK:
    case APP_SMARTBOX_ACTION_TASK:
        break;
    default:
        if (UI_WINDOW_PREEMPTION_CHECK()) {
            break;
        }
        if (get_screen_saver_status()) {
            ui_screen_recover(0);
            ui_auto_shut_down_enable();
            UI_SHOW_WINDOW(ID_WINDOW_MESS);
        } else {
            ui_auto_shut_down_re_run();
            if (UI_GET_WINDOW_ID() == ID_WINDOW_MESS) {
                if (flash_message_count() < 10) {
                    UI_MSG_POST("message_status:event=%4", 1);
                } else {
                    UI_MSG_POST("message_status:event=%4", 3);
                }
            } else {
                UI_HIDE_CURR_WINDOW();
                UI_SHOW_WINDOW(ID_WINDOW_MESS);
            }
        }
        break;
    }
#endif
    // moto
    UI_MOTO_RUN(2);
}

void notice_set_info_from_ancs(void *name, void *data, u16 len)
{
    notice_set_info_from(&temp_message_buffer, name, data, len);
}

void notice_add_info_from_ancs()
{
    notice_add_info_from(&temp_message_buffer);
}

void notice_remove_info_from_ancs(u32 uid)
{
    if (flash_message_count() == 0) {
        log_info("message null!!!");
        return;
    }

    flash_message_delete_by_mask(uid);

    UI_MSG_POST("message_status:event=%4", 2);
}

void func_attr_notice_add_info(void *priv, u8 attr, u8 *data, u16 len)
{
    u8 seq = data[0];
    log_info("seq:%d", seq);
    static u8 *message_buf;
    static u16 message_offset = 0;
    if (!seq) {
        if (message_buf) {
            //分包
            ASSERT(message_offset);
            ASSERT(message_offset + len - 1 <= 512);
            memcpy(message_buf + message_offset, &data[1], len - 1);
            message_offset += len - 1;
            flash_message_write(message_buf, message_offset);
            message_offset = 0;
            free(message_buf);
            message_buf = NULL;

        } else {
            //不分包
            flash_message_write(&data[1], len - 1);
        }
    } else { //分包情况
        if (!message_buf) {
            message_buf = malloc(512);
        }
        memcpy(message_buf + message_offset, &data[1], len - 1);
        message_offset += len - 1;
    }

    if (flash_message_count() < 10) {
        UI_MSG_POST("message_status:event=%4", 1);
    } else {
        UI_MSG_POST("message_status:event=%4", 3);
    }
}

void func_attr_notice_remove_info(void *priv, u8 attr, u8 *data, u16 len)
{
    u8 packagename[MESSAGE_PACKAGENAME_LEN];
    u32 timestamp;
    u32 copy_len;
    u16 offset = 0;
    u16 L;
    u8 T;
    u8 *V;

    if (flash_message_count() == 0) {
        log_info("message null!!!");
        return;
    }

    while (offset < len) {
        L = (data[offset] << 8) | data[offset + 1];
        T = data[offset + 2];
        V = &data[offset + 3];
        offset += L + 2;
        if (T == 0) {
            copy_len = (L - 1) > MESSAGE_TIMESTAMP_LEN ? MESSAGE_TIMESTAMP_LEN : (L - 1);
            memcpy(&timestamp, V, copy_len);
        } else if (T == 1) {
            copy_len = (L - 1) > MESSAGE_PACKAGENAME_LEN ? MESSAGE_PACKAGENAME_LEN : (L - 1);
            memcpy(packagename, V, copy_len);
        }
    }
    flash_message_delete_by_mask(timestamp);
    UI_MSG_POST("message_status:event=%4", 2);
}

int notice_status_handler(const char *type, u32 arg)
{
    int row, col;
    struct element_css *css;
    struct element *elm;
    struct ui_grid *grid = ui_grid_for_id(SIDEBAR_VLIST_NOTICE);

    if (type && (!strcmp(type, "event"))) {
        switch (arg) {
        case 1: /* add */
            log_info("add message.....");
            log_info("message_num:%d", flash_message_count());
            if (flash_message_count() == 1) {
                row = 2;
                col = 1;
            } else {
                row = 1;
                col = 0;
            }
            css = ui_core_get_element_css(&grid->elm);
            if (css->invisible == true) {
                css->invisible = false;
            }
            ui_grid_add_dynamic_by_id(SIDEBAR_VLIST_NOTICE, &row, &col, create_control_by_menu);
            break;
        case 2: /* del */
            log_info("del message.....");
            log_info("message_num:%d", flash_message_count());
            if (flash_message_count() == 0) {
                row = 2;
                col = 1;
            } else {
                row = 1;
                col = 0;
            }
            if (flash_message_count() == 0) {
                css = ui_core_get_element_css(ui_core_get_element_by_id(SIDEBAR_NOTICE_LAYOUT));
                css->top = css->height;
                css = ui_core_get_element_css(&grid->elm);
                css->invisible = true;
            }
            ui_grid_del_dynamic_by_id(SIDEBAR_VLIST_NOTICE, &row, &col, create_control_by_menu);
            break;
        case 3:
            /* log_info("redraw grid:%d.....",grid->avail_item_num); */
            for (u8 index = 0; index < grid->avail_item_num; index++) {
                list_for_each_child_element(elm, &grid->item[index].elm) {
                    if (elm->handler && elm->handler->onchange) {
                        elm->handler->onchange(elm, ON_CHANGE_UPDATE_ITEM, (void *)message_index[index]);
                    }
                }
            }
            break;
        }
    }

    return 0;
}

#if 0
static struct __UI_MESSAGE test1_message = {
    .packagename = "com.tencent.mm",
    .AppIdentifier = 2,
    .title = "微信",
    .content = "微信：helloworld",
    .TimeStamp = 1,
    .title_encode = FONT_ENCODE_UTF8,
    .title_flags = FONT_DEFAULT,
    .title_icon_Xcoordinate = 92,
    .title_text_Xcoordinate = 116,
};

static struct __UI_MESSAGE test2_message = {
    .packagename = "com.tencent.mobileqq",
    .AppIdentifier = 3,
    .title = "QQ",
    .content = "QQ：helloworld",
    .TimeStamp = 2,
    .title_encode = FONT_ENCODE_UTF8,
    .title_flags = FONT_DEFAULT,
    .title_icon_Xcoordinate = 92,
    .title_text_Xcoordinate = 116,
};

static struct __UI_MESSAGE test3_message = {
    .packagename = "com.alibaba.android.rimet",
    .AppIdentifier = 4,
    .title = "钉钉",
    .content = "钉钉：helloworld",
    .TimeStamp = 3,
    .title_encode = FONT_ENCODE_UTF8,
    .title_flags = FONT_DEFAULT,
    .title_icon_Xcoordinate = 92,
    .title_text_Xcoordinate = 116,
};

void test_message_add(u8 index)
{
    s8 i;
    u8 num = flash_message_count();
    struct __UI_MESSAGE *test_message;

    if (index == 1) {
        test_message = &test1_message;
    } else if (index == 2) {
        test_message = &test2_message;
    } else if (index == 3) {
        test_message = &test3_message;
    } else {
        return;
    }
    UI_MSG_POST("message_status:event=%4", 1);
}

void test_message_del(u8 timestamp)
{
    s8 i;
    UI_MSG_POST("message_status:event=%4", 2);
}
#endif

#if TCFG_UI_ENABLE && (!TCFG_LUA_ENABLE)
static int vlist_notice_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    static struct scroll_area area = {0, 0, 10000, 10000};
    struct ui_grid *grid = (struct ui_grid *)_ctrl;
    struct element_css *css;
    int row, col;

    switch (event) {
    case ON_CHANGE_INIT:
#if 0
        if (message_num == 0) {
            memset(message_store_buffer, 0, sizeof(struct __UI_MESSAGE) * MESSAGE_MAX_NUM);
            for (int index = 0; index < 3; index++) {
                if (index == 0) {
                    memcpy(&message_store_buffer[index], &test1_message, sizeof(struct __UI_MESSAGE));
                } else if (index == 1) {
                    memcpy(&message_store_buffer[index], &test2_message, sizeof(struct __UI_MESSAGE));
                } else if (index == 2) {
                    memcpy(&message_store_buffer[index], &test3_message, sizeof(struct __UI_MESSAGE));
                }
            }
            message_num = index;
        }
#endif
        if (flash_message_count() == 0) {
            row = flash_message_count();
            col = 0;
        } else {
            row = flash_message_count() + 1;
            col = 1;
        }
        ui_grid_init_dynamic(grid, &row, &col);
        log_info("%s %d:dynamic_grid %d X %d\n", __FUNCTION__, __LINE__, row, col);
        /* for (u8 i = 0; i < grid->avail_item_num; i++) { */
        /*     css = ui_core_get_element_css(&grid->item[i].elm); */
        /*     y_log_info("css[%d]->invisible:%d", i, css->invisible); */
        /* } */

        ui_grid_set_scroll_area(grid, &area);
        ui_grid_set_slide_direction(grid, SCROLL_DIRECTION_UD);
        if (flash_message_count() == 0) {
            css = ui_core_get_element_css(&grid->elm);
            css->invisible = true;
        }
        break;
    case ON_CHANGE_RELEASE:
        for (u8 i = 0; i < 3; i++) {
            if (message_store_buffer[i]) {
                free(message_store_buffer[i]);
                message_store_buffer[i] = 0;
            }
        }
        break;
    }
    return 0;
}

static int vlist_notice_ontouch(void *ctrl, struct element_touch_event *e)
{
    struct ui_grid *grid = (struct ui_grid *)ctrl;
    struct element_css *css;
    static u8 touch_action = 0;
    static u8 hide_flag = 0;
    static int first_y_offset = 0;

    switch (e->event) {
    case ELM_EVENT_TOUCH_UP:
        if (touch_action == 1) {
            int sel_item = ui_grid_cur_item(grid);
            message_select_index = message_index[sel_item];
            log_info("sel_item is:%d, index:%d, message_num:%d", sel_item, message_select_index, flash_message_count());

            if (message_select_index < flash_message_count()) {
                ui_send_event(KEY_CHANGE_PAGE, BIT(31) | MESSAGE_DETAIL_PAGE);
            }
        }
        break;
    case ELM_EVENT_TOUCH_DOWN:
        touch_action = 1;
        first_y_offset = e->pos.y;
        css = ui_core_get_element_css(&grid->item[0].elm);
        int top_half_scr = (grid->dynamic->grid_yval - css->height) / 2;
        int top = grid->area ? grid->area->top : top_half_scr;
        if (css->top == top) {
            hide_flag = 1;
        } else {
            hide_flag = 0;
        }
        break;
    case ELM_EVENT_TOUCH_MOVE:
        touch_action = 2;
        if (e->pos.y > first_y_offset) {
            if ((hide_flag == 1) && (create_control_by_menu == 0)) {
                css = ui_core_get_element_css(ui_core_get_element_by_id(SIDEBAR_NOTICE_LAYOUT));
                css->top = css->height;
                /* ui_core_redraw(ui_core_get_element_by_id(SIDEBAR_NOTICE_LAYOUT)); */
            }
        } else {
            hide_flag = 0;
        }
        break;
    }

    return false;
}
REGISTER_UI_EVENT_HANDLER(SIDEBAR_VLIST_NOTICE)
.onchange = vlist_notice_onchange,
 .onkey = NULL,
  .ontouch = vlist_notice_ontouch,
};

static int text_title_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct ui_text *text = (struct ui_text *)_ctrl;
    struct element_css *css;
    int index;
    u8 err;

    switch (event) {
    case ON_CHANGE_INIT:
        break;
    case ON_CHANGE_RELEASE:
        break;
    case ON_CHANGE_HIGHLIGHT:
        break;
    case ON_CHANGE_UPDATE_ITEM:
        index = (u32)arg;
        if (index > flash_message_count()) {
            break;
        } else if (index == flash_message_count()) {
            ui_text_set_hide_by_id(text->elm.id, true);
        } else {
            ui_text_set_hide_by_id(text->elm.id, false);
        }

        switch (text->elm.id) {
        case SIDEBAR_TEXT_TITLE0:
            err = message_data_analysis(index, 0);
            if (err) {
                return FALSE;
            }
            index = 0;
            break;
        case SIDEBAR_TEXT_TITLE1:
            err = message_data_analysis(index, 1);
            if (err) {
                return FALSE;
            }
            index = 1;
            break;
        case SIDEBAR_TEXT_TITLE2:
            err = message_data_analysis(index, 2);
            if (err) {
                return FALSE;
            }
            index = 2;
            break;
        default:
            return FALSE;
        }

        /* log_info("title:%s\n",message_store_buffer[index]->title); */
        /* log_info("%s %d:index %d, message_num:%d", __FUNCTION__, __LINE__, index, message_num); */
        css = ui_core_get_element_css(&text->elm);
        css->left = message_store_buffer[index]->title_text_Xcoordinate * 10000 / 240;
        ui_text_set_text_attrs(text, message_store_buffer[index]->title,
                               strlen(message_store_buffer[index]->title),
                               message_store_buffer[index]->title_encode, 0,
                               message_store_buffer[index]->title_flags);
        break;
    }
    return FALSE;
}
REGISTER_UI_EVENT_HANDLER(SIDEBAR_TEXT_TITLE0)
.onchange = text_title_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};
REGISTER_UI_EVENT_HANDLER(SIDEBAR_TEXT_TITLE1)
.onchange = text_title_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};
REGISTER_UI_EVENT_HANDLER(SIDEBAR_TEXT_TITLE2)
.onchange = text_title_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

static int text_content_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct ui_text *text = (struct ui_text *)_ctrl;
    int index;

    switch (event) {
    case ON_CHANGE_INIT:
        break;
    case ON_CHANGE_RELEASE:
        break;
    case ON_CHANGE_HIGHLIGHT:
        break;
    case ON_CHANGE_UPDATE_ITEM:
        index = (u32)arg;
        if (index > flash_message_count()) {
            break;
        } else if (index == flash_message_count()) {
            ui_text_set_hide_by_id(text->elm.id, true);
        } else {
            ui_text_set_hide_by_id(text->elm.id, false);
        }

        switch (text->elm.id) {
        case SIDEBAR_TEXT_CONTENT0:
            /* log_info("b0\n"); */
            message_index[0] = index;
            index = 0;
            break;
        case SIDEBAR_TEXT_CONTENT1:
            /* log_info("b1\n"); */
            message_index[1] = index;
            index = 1;
            break;
        case SIDEBAR_TEXT_CONTENT2:
            /* log_info("b2\n"); */
            message_index[2] = index;
            index = 2;
            break;
        default:
            return FALSE;
        }

        /* log_info("content:%s\n",message_store_buffer[index]->content); */
        /* log_info("%s %d:index %d, message_num:%d", __FUNCTION__, __LINE__, index, message_num); */
        ui_text_set_text_attrs(text, message_store_buffer[index]->content,
                               strlen(message_store_buffer[index]->content),
                               FONT_ENCODE_UTF8, 0, FONT_DEFAULT | FONT_SHOW_MULTI_LINE);
        break;
    }

    return FALSE;
}
REGISTER_UI_EVENT_HANDLER(SIDEBAR_TEXT_CONTENT0)
.onchange = text_content_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};
REGISTER_UI_EVENT_HANDLER(SIDEBAR_TEXT_CONTENT1)
.onchange = text_content_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};
REGISTER_UI_EVENT_HANDLER(SIDEBAR_TEXT_CONTENT2)
.onchange = text_content_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

static int pic_icon_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct ui_pic *pic = (struct ui_pic *)_ctrl;
    struct element_css *css;
    int index;

    switch (event) {
    case ON_CHANGE_INIT:
        break;
    case ON_CHANGE_RELEASE:
        break;
    case ON_CHANGE_HIGHLIGHT:
        break;
    case ON_CHANGE_UPDATE_ITEM:
        index = (u32)arg;
        if (index > flash_message_count()) {
            break;
        } else if (index == flash_message_count()) {
            ui_pic_set_hide_by_id(pic->elm.id, true);
        } else {
            ui_pic_set_hide_by_id(pic->elm.id, false);
        }

        switch (pic->elm.id) {
        case SIDEBAR_PIC_APP_ICON0:
            index = 0;
            break;
        case SIDEBAR_PIC_APP_ICON1:
            index = 1;
            break;
        case SIDEBAR_PIC_APP_ICON2:
            index = 2;
            break;
        default:
            return FALSE;
        }

        if (index != flash_message_count()) {
            css = ui_core_get_element_css(&pic->elm);
            css->left = message_store_buffer[index]->title_icon_Xcoordinate * 10000 / 240;
        }

        if (message_store_buffer[index]->AppIdentifier >= 5) {
            ui_pic_set_image_index(pic, 2);
        } else if (message_store_buffer[index]->AppIdentifier == 0) {
            ui_pic_set_image_index(pic, 0);
        } else if (message_store_buffer[index]->AppIdentifier == 1) {
            ui_pic_set_image_index(pic, 1);
        } else if (message_store_buffer[index]->AppIdentifier == 2) {
            ui_pic_set_image_index(pic, 3);
        } else if (message_store_buffer[index]->AppIdentifier == 3) {
            ui_pic_set_image_index(pic, 4);
        } else if (message_store_buffer[index]->AppIdentifier == 4) {
            ui_pic_set_image_index(pic, 5);
        }
        break;
    }
    return FALSE;
}
REGISTER_UI_EVENT_HANDLER(SIDEBAR_PIC_APP_ICON0)
.onchange = pic_icon_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};
REGISTER_UI_EVENT_HANDLER(SIDEBAR_PIC_APP_ICON1)
.onchange = pic_icon_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};
REGISTER_UI_EVENT_HANDLER(SIDEBAR_PIC_APP_ICON2)
.onchange = pic_icon_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

static int text_clear_all_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct ui_text *text = (struct ui_text *)_ctrl;
    int index;

    switch (event) {
    case ON_CHANGE_INIT:
        break;
    case ON_CHANGE_RELEASE:
        break;
    case ON_CHANGE_HIGHLIGHT:
        break;
    case ON_CHANGE_UPDATE_ITEM:
        index = (u32)arg;
        if (index > flash_message_count()) {
            break;
        }
        switch (text->elm.id) {
        case SIDEBAR_TEXT_CLEAR_ALL1:
            if ((index == flash_message_count()) && (index > 0)) {
                if (flash_message_count() <= 1) {
                    ui_text_set_hide_by_id(SIDEBAR_TEXT_CLEAR_ALL1, false);
                }
            } else {
                ui_text_set_hide_by_id(SIDEBAR_TEXT_CLEAR_ALL1, true);
            }
            break;
        case SIDEBAR_TEXT_CLEAR_ALL2:
            if ((index == flash_message_count()) && (index > 0)) {
                if (flash_message_count() > 1) {
                    ui_text_set_hide_by_id(SIDEBAR_TEXT_CLEAR_ALL2, false);
                }
            } else {
                ui_text_set_hide_by_id(SIDEBAR_TEXT_CLEAR_ALL2, true);
            }
            break;
        default:
            return FALSE;
        }

        break;
    }

    return FALSE;
}

static int text_clear_all_ontouch(void *ctrl, struct element_touch_event *e)
{
    struct ui_grid *grid = ui_grid_for_id(SIDEBAR_VLIST_NOTICE);
    struct element_css *css;
    int row, col;

    switch (e->event) {
    case ELM_EVENT_TOUCH_UP:
        row = 1;//message_num + 1;
        col = 1;
        while (flash_message_count()) {
            flash_message_delete_by_index(0);
        }
        /* if (message_num == 0)  */
        {
            css = ui_core_get_element_css(ui_core_get_element_by_id(SIDEBAR_NOTICE_LAYOUT));
            css->top = css->height;
            css = ui_core_get_element_css(&grid->elm);
            css->invisible = true;
        }
        ui_grid_del_dynamic_by_id(SIDEBAR_VLIST_NOTICE, &row, &col, 1);
        if (UI_GET_WINDOW_ID() == ID_WINDOW_MESS) {
            UI_WINDOW_BACK_SHOW(2);
        }
        break;
    case ELM_EVENT_TOUCH_DOWN:
        return true;
    }

    return false;
}
REGISTER_UI_EVENT_HANDLER(SIDEBAR_TEXT_CLEAR_ALL1)
.onchange = text_clear_all_onchange,
 .onkey = NULL,
  .ontouch = text_clear_all_ontouch,
};
REGISTER_UI_EVENT_HANDLER(SIDEBAR_TEXT_CLEAR_ALL2)
.onchange = text_clear_all_onchange,
 .onkey = NULL,
  .ontouch = text_clear_all_ontouch,
};

//===============================================================================================================//

static int text_message_detail_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    switch (event) {
    case ON_CHANGE_INIT:
        break;
    case ON_CHANGE_RELEASE:
        break;
    }
    return FALSE;
}

static int text_message_detail_ontouch(void *ctrl, struct element_touch_event *e)
{
    struct layout *layout = (struct layout *)ctrl;
    static int last_pos_y = 0;
    int y_offset;
    struct rect rect;
    struct rect parent_rect;
    struct element_css *parent_css;

    switch (e->event) {
    case ELM_EVENT_TOUCH_UP:
        break;
    case ELM_EVENT_TOUCH_DOWN:
        last_pos_y = e->pos.y;
        break;
    case ELM_EVENT_TOUCH_MOVE:
        ui_core_get_element_abs_rect(layout->elm.parent, &parent_rect);
        ui_core_get_element_abs_rect(&layout->elm, &rect);
        if (rect.height > parent_rect.height) {
            y_offset = e->pos.y - last_pos_y;
            last_pos_y = e->pos.y;
            parent_css = ui_core_get_element_css(layout->elm.parent);
            rect.top += y_offset;
            layout->elm.css.top = rect.top * 10000 / parent_rect.height;
            if (layout->elm.css.top > 0) {
                layout->elm.css.top = 0;
            } else if (layout->elm.css.top < (parent_css->height - layout->elm.css.height)) {
                layout->elm.css.top = parent_css->height - layout->elm.css.height;
            }
            ui_core_redraw(layout->elm.parent);
        }
        break;
    case ELM_EVENT_TOUCH_R_MOVE:
        log_info(">>>>>>>>>%s %d %d\n", __FUNCTION__, __LINE__, message_select_index);
        /* message_num--; */
        /* if (message_num == 0) { */
        /* message_total_len = 0; */
        /* css = ui_core_get_element_css(ui_core_get_element_by_id(SIDEBAR_NOTICE_LAYOUT)); */
        /* css->top = css->height; */
        /* css = ui_core_get_element_css(&grid->elm); */
        /* css->invisible = true; */
        /* } */
        return false;
        /* break; */
    }

    return true;
}
__REGISTER_UI_EVENT_HANDLER(JL, LAYOUT_MESSAGE_DETAIL)
.onchange = text_message_detail_onchange,
 .onkey = NULL,
  .ontouch = text_message_detail_ontouch,
};

static int text_title_detail_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct ui_text *text = (struct ui_text *)_ctrl;
    struct element_css *css;
    struct element_css *parent_css;
    u8 index;

    switch (event) {
    case ON_CHANGE_INIT:
        //根据布局实际高度重新调整控件大小
        parent_css = ui_core_get_element_css(text->elm.parent);
        css = ui_core_get_element_css(&text->elm);
        css->top = css->top * 10000 / parent_css->height;
        css->height = css->height * 10000 / parent_css->height;
        index = 0;
        ASSERT(message_store_buffer[index]);
        ui_text_set_text_attrs(text, message_store_buffer[index]->title,
                               strlen(message_store_buffer[index]->title),
                               message_store_buffer[index]->title_encode,
                               0, message_store_buffer[index]->title_flags);
        break;
    case ON_CHANGE_RELEASE:
        break;
    }
    return FALSE;
}
__REGISTER_UI_EVENT_HANDLER(JL, TEXT_TITLE_DETAIL)
.onchange = text_title_detail_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

static int text_content_detail_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct ui_text *text = (struct ui_text *)_ctrl;
    struct element_css *css;
    struct element_css *parent_css;
    struct rect rect;
    struct rect parent_rect;
    u16 content_elm_width;
    u16 content_elm_height;
    u16 message_content_height;
    u8 index;

    switch (event) {
    case ON_CHANGE_INIT:
        ui_core_get_element_abs_rect(&text->elm, &rect);
        content_elm_width = rect.width;
        content_elm_height = 240;               //预先设置最大高度，方便下面计算文本高度

        message_data_analysis(message_select_index, 0);
        index = 0;

        message_content_height = ui_get_text_width_and_height(FONT_ENCODE_UTF8, message_store_buffer[index]->content,
                                 strlen(message_store_buffer[index]->content),
                                 content_elm_width, content_elm_height, FONT_DEFAULT | FONT_SHOW_MULTI_LINE, "height");
        /* log_info("message_content_height:%d",message_content_height); */
        if (message_content_height > rect.height) {                 //根据文本实际高度调整文本控件大小
            struct element *elm = ui_core_get_element_by_id(LAYOUT_MESSAGE_DETAIL);
            ui_core_get_element_abs_rect(elm->parent, &parent_rect);
            parent_css = ui_core_get_element_css(text->elm.parent);
            parent_css->height += (message_content_height - rect.height) * 10000 / parent_rect.height;
            ui_core_get_element_abs_rect(text->elm.parent, &parent_rect);
            css = ui_core_get_element_css(&text->elm);
            css->top = css->top * 10000 / parent_css->height;
            css->height = message_content_height * 10000 / parent_rect.height;
        }
        ui_text_set_text_attrs(text, message_store_buffer[index]->content,
                               strlen(message_store_buffer[index]->content),
                               FONT_ENCODE_UTF8, 0, FONT_DEFAULT | FONT_SHOW_MULTI_LINE);
        break;
    case ON_CHANGE_RELEASE:
        break;
    }
    return FALSE;
}
__REGISTER_UI_EVENT_HANDLER(JL, TEXT_CONTENT_DETAIL)
.onchange = text_content_detail_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

#if TCFG_APP_CAT1_EN && TCFG_CAT1_FUNC_SMS_ENABLE

static int pic_sms_reply_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    u8 index;
    switch (event) {
    case ON_CHANGE_INIT:
        index = 0;
        ASSERT(message_store_buffer[index]);
        if (message_store_buffer[index]->AppIdentifier == 1) {
            ui_text_set_hide_by_id(PIC_SMS_REPLY, false);
        } else {
            ui_text_set_hide_by_id(PIC_SMS_REPLY, true);
        }
        break;
    case ON_CHANGE_RELEASE:
        break;
    }
    return FALSE;
}

static int pic_sms_reply_ontouch(void *_ctrl, struct element_touch_event *e)
{
    u8 index;
    switch (e->event) {
    case ELM_EVENT_TOUCH_UP:
        log_info("sms reply\n");
        index = 0;
        clear_send_sms();
        cat1_set_send_sms_number((u8 *)message_store_buffer[index]->title, 0,
                                 strlen(message_store_buffer[index]->title) + 1);
        UI_HIDE_CURR_WINDOW();
        UI_SHOW_WINDOW(ID_WINDOW_CAT1_SMS_REPLY);
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

__REGISTER_UI_EVENT_HANDLER(JL, PIC_SMS_REPLY)
.onchange = pic_sms_reply_onchange,
 .onkey = NULL,
  .ontouch = pic_sms_reply_ontouch,
};

#endif /* #if TCFG_APP_CAT1_EN && TCFG_CAT1_FUNC_SMS_ENABLE */

static int pic_app_detail_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct ui_pic *pic = (struct ui_pic *)_ctrl;
    struct element_css *css;
    struct element_css *parent_css;
    u8 index;

    switch (event) {
    case ON_CHANGE_INIT:
        //根据布局实际高度重新调整控件大小
        parent_css = ui_core_get_element_css(pic->elm.parent);
        css = ui_core_get_element_css(&pic->elm);
        css->top = css->top * 10000 / parent_css->height;
        css->height = css->height * 10000 / parent_css->height;
        index = 0;
        ASSERT(message_store_buffer[index]);
        if (message_store_buffer[index]->AppIdentifier >= 5) {
            ui_pic_set_image_index(pic, 2);
        } else if (message_store_buffer[index]->AppIdentifier == 0) {
            ui_pic_set_image_index(pic, 0);
        } else if (message_store_buffer[index]->AppIdentifier == 1) {
            ui_pic_set_image_index(pic, 1);
        } else if (message_store_buffer[index]->AppIdentifier == 2) {
            ui_pic_set_image_index(pic, 3);
        } else if (message_store_buffer[index]->AppIdentifier == 3) {
            ui_pic_set_image_index(pic, 4);
        } else if (message_store_buffer[index]->AppIdentifier == 4) {
            ui_pic_set_image_index(pic, 5);
        }
        break;
    case ON_CHANGE_RELEASE:
        for (u8 i = 0; i < 3; i++) {
            if (message_store_buffer[i]) {
                free(message_store_buffer[i]);
                message_store_buffer[i] = 0;
            }
        }
        break;
    }
    return FALSE;
}
__REGISTER_UI_EVENT_HANDLER(JL, PIC_APP_ICON)
.onchange = pic_app_detail_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

//===============================================================================================================//

static const struct uimsg_handl ui_menu_notice_msg_handler[] = {
    { "message_status",   notice_status_handler     },
    { NULL, NULL},      /* 必须以此结尾！ */
};

u8 is_create_control_by_menu()
{
    return create_control_by_menu;
}

static int menu_notice_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    switch (event) {
    case ON_CHANGE_INIT:
        int root = LAYER_NOTICE;

        create_control_by_menu = 1;
        if (root && create_control_by_id(RES_PATH"sidebar/sidebar.tab", 0X40820002, SIDEBAR_NOTICE_LAYOUT, root)) {
            log_info("crteate notice control succ! %x\n", root, __LINE__);
        } else {
            log_info("crteate notice control fail! %x\n", root, __LINE__);
        }

        ui_register_msg_handler(ID_WINDOW_MESS, ui_menu_notice_msg_handler);//注册消息交互的回调
        break;
    case ON_CHANGE_RELEASE:
        if (ui_core_get_element_by_id(SIDEBAR_NOTICE_LAYOUT)) {
            delete_control_by_id(SIDEBAR_NOTICE_LAYOUT);
        }
        create_control_by_menu = 0;
        break;
    }

    return FALSE;
}

__REGISTER_UI_EVENT_HANDLER(JL, LAYOUT_NOTICE)
.onchange = menu_notice_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

#endif

//-----------------------4g短信回复-----------------------------//
#if TCFG_APP_CAT1_EN && TCFG_CAT1_FUNC_SMS_ENABLE

static struct cat1_send_sms_info_t *cat1_send_sms_info = NULL;
static int is_sms_send_fail = 0;
static void *sms_show_str = NULL; // 字符显示用

static int sms_send_page_onchange(void *ctr, enum element_change_event e, void *arg)
{
    struct window *window = (struct window *)ctr;
    switch (e) {
    case ON_CHANGE_INIT:
        log_info("\n***sms_send_onchange***\n");
        is_sms_send_fail = 0;
        cat1_send_sms_info = calloc(1, sizeof(struct cat1_send_sms_info_t));
        break;
    case ON_CHANGE_RELEASE:
        if (cat1_send_sms_info->sending_show_timer) {
            sys_timer_del(cat1_send_sms_info->sending_show_timer);
            cat1_send_sms_info->sending_show_timer = 0;
        }
        if (cat1_send_sms_info->timeout_show_timer) {
            sys_timer_del(cat1_send_sms_info->timeout_show_timer);
            cat1_send_sms_info->timeout_show_timer = 0;
        }
        if (cat1_send_sms_info->offline_show_timer) {
            sys_timer_del(cat1_send_sms_info->offline_show_timer);
            cat1_send_sms_info->offline_show_timer = 0;
        }
        if (cat1_send_sms_info != NULL) {
            free(cat1_send_sms_info);
            cat1_send_sms_info = NULL;
        }
        break;
    default:
        return false;
    }
    return false;
}
REGISTER_UI_EVENT_HANDLER(ID_WINDOW_CAT1_SMS_REPLY)
.onchange = sms_send_page_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

static int text_msg_reply_number_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct ui_text *text = (struct ui_text *)_ctrl;

    switch (event) {
    case ON_CHANGE_INIT:
        switch (text->elm.id) {
        case CAT1_TEXT_SMS_REPLY_NUMBER:
            ui_text_set_text_attrs(text, cat1_get_send_sms_number(), strlen(cat1_get_send_sms_number()), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        }
        break;
    case ON_CHANGE_RELEASE:
        break;
    }
    return 0;
}
REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_SMS_REPLY_NUMBER)
.onchange = text_msg_reply_number_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

const unsigned char sms_tab[15 + 1] = {
    0xE5, 0xA5, 0xBD, 0xE7, 0x9A, 0x84, 0xEF, 0xBC, 0x8C, 0xE6, 0x94, 0xB6, 0xE5, 0x88, 0xB0, 0x00
};    //好的，收到

const unsigned char sms_tab1[9 + 1] = {
    0xE4, 0xB8, 0x80, 0xE4, 0xBC, 0x9A, 0xE8, 0xA7, 0x81, 0x00
};    //一会见

const unsigned char sms_tab2[18 + 1] = {
    0xE7, 0xA8, 0x8D, 0xE7, 0xAD, 0x89, 0xEF, 0xBC, 0x8C, 0xE9, 0xA9, 0xAC, 0xE4, 0xB8, 0x8A, 0xE5,
    0x88, 0xB0, 0x00
};

const unsigned char sms_tab3[21 + 1] = {
    0xE5, 0x9C, 0xA8, 0xE5, 0xBF, 0x99, 0xEF, 0xBC, 0x8C, 0xE7, 0xA8, 0x8D, 0xE5, 0x90, 0x8E, 0xE8,
    0x81, 0x94, 0xE7, 0xB3, 0xBB, 0x00
};    //在忙，稍后联系

const unsigned char sms_tab4[6 + 1] = {
    0xE6, 0x98, 0xAF, 0xE7, 0x9A, 0x84, 0x00
};    //是的

const unsigned char sms_tab5[9 + 1] = {
    0xE4, 0xB8, 0x8D, 0xE6, 0x98, 0xAF, 0xE7, 0x9A, 0x84, 0x00
};     //不是的

const unsigned char sms_tab6[6 + 1] = {
    0xE8, 0xB0, 0xA2, 0xE8, 0xB0, 0xA2, 0x00
};     //谢谢

static int vlist_sms_reply_ontouch(void *ctr, struct element_touch_event *e)
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
        log_info("sel_item:%d\n", sel_item);
        if (sel_item < 0) {
            break;
        }
        switch (sel_item) {
        case 0:
            cat1_set_send_sms_content(sms_tab, 0, sizeof(sms_tab));
            ui_hide(CAT1_LAYOUT_SMS_REPLY);
            ui_show(CAT1_LAYOUT_SMS_SURE);
            break;
        case 1:
            cat1_set_send_sms_content(sms_tab1, 0, sizeof(sms_tab1));
            ui_hide(CAT1_LAYOUT_SMS_REPLY);
            ui_show(CAT1_LAYOUT_SMS_SURE);
            break;
        case 2:
            cat1_set_send_sms_content(sms_tab2, 0, sizeof(sms_tab2));
            ui_hide(CAT1_LAYOUT_SMS_REPLY);
            ui_show(CAT1_LAYOUT_SMS_SURE);
            break;
        case 3:
            cat1_set_send_sms_content(sms_tab3, 0, sizeof(sms_tab3));
            ui_hide(CAT1_LAYOUT_SMS_REPLY);
            ui_show(CAT1_LAYOUT_SMS_SURE);
            break;
        case 4:
            cat1_set_send_sms_content(sms_tab4, 0, sizeof(sms_tab4));
            ui_hide(CAT1_LAYOUT_SMS_REPLY);
            ui_show(CAT1_LAYOUT_SMS_SURE);
            break;
        case 5:
            cat1_set_send_sms_content(sms_tab5, 0, sizeof(sms_tab5));
            ui_hide(CAT1_LAYOUT_SMS_REPLY);
            ui_show(CAT1_LAYOUT_SMS_SURE);
            break;
        case 6:
            cat1_set_send_sms_content(sms_tab6, 0, sizeof(sms_tab6));
            ui_hide(CAT1_LAYOUT_SMS_REPLY);
            ui_show(CAT1_LAYOUT_SMS_SURE);
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
REGISTER_UI_EVENT_HANDLER(CAT1_VLIST_SMS_REPLY)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = vlist_sms_reply_ontouch,
};

static int sms_input_ontouch(void *ctr, struct element_touch_event *e)
{
    switch (e->event) {
    case ELM_EVENT_TOUCH_UP:
        log_info("sms input\n");
        clear_send_sms_content();
        cat1_send_sms_info->input_index = 0;
        ui_hide(CAT1_LAYOUT_SMS_REPLY);
        ui_show(CAT1_LAYOUT_SMS_INPUT_SYSTEM);

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
REGISTER_UI_EVENT_HANDLER(CAT1_BUTTON_SMS_INPUT)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = sms_input_ontouch,
};

static int sms_sure_number_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct ui_text *text = (struct ui_text *)_ctrl;

    switch (event) {
    case ON_CHANGE_INIT:
        switch (text->elm.id) {
        case CAT1_TEXT_SMS_SURE_NUMBER:
            ui_text_set_text_attrs(text, cat1_get_send_sms_number(), strlen(cat1_get_send_sms_number()),
                                   FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        }
        break;
    case ON_CHANGE_RELEASE:
        break;
    }
    return 0;
}
REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_SMS_SURE_NUMBER)
.onchange = sms_sure_number_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

static int sms_sure_content_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct ui_text *text = (struct ui_text *)_ctrl;

    switch (event) {
    case ON_CHANGE_INIT:
        switch (text->elm.id) {
        case CAT1_TEXT_SMS_SURE_CONTENT:
            ui_text_set_text_attrs(text, cat1_get_send_sms_content(), strlen(cat1_get_send_sms_content()),
                                   FONT_ENCODE_UTF8, 0, FONT_DEFAULT | FONT_SHOW_MULTI_LINE | FONT_SHOW_SCROLL);
            break;
        }
        break;
    case ON_CHANGE_RELEASE:
        break;
    }
    return 0;
}
REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_SMS_SURE_CONTENT)
.onchange = sms_sure_content_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

static int sms_send_yes_ontouch(void *ctr, struct element_touch_event *e)
{
    switch (e->event) {
    case ELM_EVENT_TOUCH_UP:
        log_info("sms send\n");
        if (cat1_check_call_enable() == true) {
            cat1_send_sms(strlen(cat1_get_send_sms_number()), cat1_get_send_sms_number(),
                          strlen(cat1_get_send_sms_content()), cat1_get_send_sms_content());    //获取电话和内容长度
            log_info("number len:%d, number:%s, content len:%d, content:%s\n",
                     strlen(cat1_get_send_sms_number()), cat1_get_send_sms_number(),
                     strlen(cat1_get_send_sms_content()), cat1_get_send_sms_content());
            ui_hide(CAT1_LAYOUT_SMS_SURE);
            ui_show(CAT1_LAYOUT_SMS_SENDING);
        } else {
            ui_hide(CAT1_LAYOUT_SMS_SURE);
            ui_show(CAT1_LAYOUT_SMS_OFFLINE);
        }

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
REGISTER_UI_EVENT_HANDLER(CAT1_BUTTON_SMS_SURE_YES)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = sms_send_yes_ontouch,
};

static int sms_send_no_ontouch(void *ctr, struct element_touch_event *e)
{
    switch (e->event) {
    case ELM_EVENT_TOUCH_UP:
        log_info("sms sure return\n");
        clear_send_sms_content();
        ui_hide(CAT1_LAYOUT_SMS_SURE);
        ui_show(CAT1_LAYOUT_SMS_REPLY);
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
REGISTER_UI_EVENT_HANDLER(CAT1_BUTTON_SMS_SURE_NO)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = sms_send_no_ontouch,
};

static void offline_show_handler(void *priv)
{
    clear_send_sms_content();
    ui_hide(CAT1_LAYOUT_SMS_OFFLINE);
    ui_show(CAT1_LAYOUT_SMS_REPLY);
}

static int layout_sms_offline_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    switch (event) {
    case ON_CHANGE_INIT:
        cat1_send_sms_info->offline_show_timer = sys_timer_add(NULL, offline_show_handler, 3000);
        break;
    case ON_CHANGE_SHOW:
        break;
    case ON_CHANGE_RELEASE:
        if (cat1_send_sms_info->offline_show_timer) {
            log_info("del offline_show_timer");
            sys_timer_del(cat1_send_sms_info->offline_show_timer);
            cat1_send_sms_info->offline_show_timer = 0;
        }
        break;
    }
    return 0;
}
REGISTER_UI_EVENT_HANDLER(CAT1_LAYOUT_SMS_OFFLINE)
.onchange = layout_sms_offline_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

static int sms_sending_number_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct ui_text *text = (struct ui_text *)_ctrl;

    switch (event) {
    case ON_CHANGE_INIT:
        switch (text->elm.id) {
        case CAT1_TEXT_MSG_SENDING_NUMBER:
            ui_text_set_text_attrs(text, cat1_get_send_sms_number(), strlen(cat1_get_send_sms_number()), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
            break;
        }
        break;
    case ON_CHANGE_RELEASE:
        break;
    }
    return 0;
}
REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_MSG_SENDING_NUMBER)
.onchange = sms_sending_number_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

static void sending_show_handler(void *priv)
{
    char count_str[3];
    snprintf(count_str, 3, "%d", cat1_send_sms_info->sending_count);
    ui_text_set_text_by_id(CAT1_TEXT_SECS_COUTING, count_str, strlen(count_str), FONT_DEFAULT);
    cat1_send_sms_info->sending_count -= 1;
    if (cat1_send_sms_info->sending_count < 0) {
        is_sms_send_fail = 1;
        cat1_send_sms_info->sending_count = 0;
        ui_hide(CAT1_LAYOUT_SMS_SENDING);
        ui_show(CAT1_LAYOUT_SMS_TIMEOUT);
    }
}

static int layout_sms_sending_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    switch (event) {
    case ON_CHANGE_INIT:
        cat1_send_sms_info->sending_count = 30; //超时，单位秒
        cat1_send_sms_info->sending_show_timer = sys_timer_add(NULL, sending_show_handler, 1000);
        break;
    case ON_CHANGE_SHOW:
        break;
    case ON_CHANGE_RELEASE:
        if (cat1_send_sms_info->sending_show_timer && is_sms_send_fail == 1) {
            log_info("del sending_show_timer");
            sys_timer_del(cat1_send_sms_info->sending_show_timer);
            cat1_send_sms_info->sending_show_timer = 0;
        }
        break;
    }
    return 0;
}
REGISTER_UI_EVENT_HANDLER(CAT1_LAYOUT_SMS_SENDING)
.onchange = layout_sms_sending_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

void timeout_show_handler(void *priv)
{
    clear_send_sms_content();
    ui_hide(CAT1_LAYOUT_SMS_TIMEOUT);
    ui_show(CAT1_LAYOUT_SMS_REPLY);
}

static int layout_sms_timeout_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    switch (event) {
    case ON_CHANGE_INIT:
        cat1_send_sms_info->timeout_show_timer = sys_timer_add(NULL, timeout_show_handler, 3000);
        break;
    case ON_CHANGE_SHOW:
        break;
    case ON_CHANGE_RELEASE:
        if (cat1_send_sms_info->timeout_show_timer) {
            log_info("del timeout_show_timer");
            sys_timer_del(cat1_send_sms_info->timeout_show_timer);
            cat1_send_sms_info->timeout_show_timer = 0;
        }
        break;
    }
    return 0;
}
REGISTER_UI_EVENT_HANDLER(CAT1_LAYOUT_SMS_TIMEOUT)
.onchange = layout_sms_timeout_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

//sms输入法
static char *get_utf8_string_end_string_sms(u8 *str, int num)
{
    char *substring = NULL;
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

static void sms_content_add(char ch)
{
    if (cat1_send_sms_info->input_content.index >= (SMS_MAX_CONTENT - 1)) {
        return;
    }

    cat1_send_sms_info->input_content.content[cat1_send_sms_info->input_content.index++] = ch;
    cat1_send_sms_info->input_content.content[cat1_send_sms_info->input_content.index] = 0x00;

    if (cat1_send_sms_info->input_content.index == 1) {
        struct element *elm = ui_core_get_element_by_id(CAT1_TEXT_INPUT_SHOW);
        ui_core_show(elm, false);
    }

    if (cat1_send_sms_info->input_content.index >= 17) {  //到17位截断，只显示后17位
        if (sms_show_str) {
            free(sms_show_str);
            sms_show_str = NULL;
        }
        sms_show_str = get_utf8_string_end_string_sms(cat1_send_sms_info->input_content.content,
                       1 + cat1_send_sms_info->input_content.index - 17);
        if (sms_show_str == NULL) {
            return ;
        }
        ui_text_set_text_by_id(CAT1_TEXT_INPUT_SHOW, sms_show_str, strlen(sms_show_str), FONT_DEFAULT);
    } else {
        ui_text_set_text_by_id(CAT1_TEXT_INPUT_SHOW,
                               cat1_send_sms_info->input_content.content, strlen(cat1_send_sms_info->input_content.content),
                               FONT_DEFAULT);
    }
}

static void sms_content_del()
{
    if (cat1_send_sms_info->input_content.index <= 0) {
        return;
    }

    cat1_send_sms_info->input_content.content[--cat1_send_sms_info->input_content.index] = 0x00;

    if (cat1_send_sms_info->input_content.index == 0) {
        log_info("hide!\n");
        struct element *elm = ui_core_get_element_by_id(CAT1_TEXT_INPUT_SHOW);
        ui_core_hide(elm);
        return;
    }

    if (cat1_send_sms_info->input_content.index >= 17) {    //到17位截断，只显示后17位
        if (sms_show_str) {
            free(sms_show_str);
            sms_show_str = NULL;
        }
        sms_show_str = get_utf8_string_end_string_sms(cat1_send_sms_info->input_content.content,
                       1 + cat1_send_sms_info->input_content.index - 17);
        if (sms_show_str == NULL) {
            return ;
        }
        ui_text_set_text_by_id(CAT1_TEXT_INPUT_SHOW, sms_show_str, strlen(sms_show_str), FONT_DEFAULT);
    } else {
        ui_text_set_text_by_id(CAT1_TEXT_INPUT_SHOW,
                               cat1_send_sms_info->input_content.content, strlen(cat1_send_sms_info->input_content.content),
                               FONT_DEFAULT);
    }
}

static int text_input_alpha_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct ui_text *text = (struct ui_text *)_ctrl;

    switch (event) {
    case ON_CHANGE_INIT:
        switch (text->elm.id) {
        case CAT1_TEXT_INPUT_ALPHA:
            struct element *elm = ui_core_get_element_by_id(CAT1_TEXT_INPUT_ALPHA);
            ui_core_highlight_element(elm, 1);
            break;
        }
        break;
    case ON_CHANGE_RELEASE:
        break;
    }
    return 0;
}

static void sms_pic_switch(int index)
{
    struct ui_pic *pic = ui_pic_for_id(CAT1_PIC_INPUT_1);
    if (pic) {
        ui_pic_set_image_index(pic, index);
    }
    pic = ui_pic_for_id(CAT1_PIC_INPUT_2);
    if (pic) {
        ui_pic_set_image_index(pic, index);
    }
    pic = ui_pic_for_id(CAT1_PIC_INPUT_3);
    if (pic) {
        ui_pic_set_image_index(pic, index);
    }
    pic = ui_pic_for_id(CAT1_PIC_INPUT_4);
    if (pic) {
        ui_pic_set_image_index(pic, index);
    }
    pic = ui_pic_for_id(CAT1_PIC_INPUT_5);
    if (pic) {
        ui_pic_set_image_index(pic, index);
    }
    pic = ui_pic_for_id(CAT1_PIC_INPUT_6);
    if (pic) {
        ui_pic_set_image_index(pic, index);
    }
    pic = ui_pic_for_id(CAT1_PIC_INPUT_7);
    if (pic) {
        ui_pic_set_image_index(pic, index);
    }
    pic = ui_pic_for_id(CAT1_PIC_INPUT_8);
    if (pic) {
        ui_pic_set_image_index(pic, index);
    }
    pic = ui_pic_for_id(CAT1_PIC_INPUT_9);
    if (pic) {
        ui_pic_set_image_index(pic, index);
    }
    pic = ui_pic_for_id(CAT1_PIC_INPUT_10);
    if (pic) {
        ui_pic_set_image_index(pic, index);
    }
    pic = ui_pic_for_id(CAT1_PIC_INPUT_11);
    if (pic) {
        ui_pic_set_image_index(pic, index);
    }
}

static int sms_input_system_ontouch(void *ctrl, struct element_touch_event *e)
{
    struct element *elm = (struct element *)ctrl;

    switch (e->event) {
    case ELM_EVENT_TOUCH_R_MOVE:
        switch (elm->id) {
        case CAT1_LAYOUT_SMS_INPUT_SYSTEM:
            clear_send_sms_content();
            memset(&(cat1_send_sms_info->input_content), 0, sizeof(cat1_send_sms_info->input_content));
            ui_hide(CAT1_LAYOUT_SMS_INPUT_SYSTEM);
            ui_show(CAT1_LAYOUT_SMS_REPLY);
            return false;
        case CAT1_PIC_INPUT_NEXT_LETTER:
            clear_send_sms_content();
            memset(&(cat1_send_sms_info->input_content), 0, sizeof(cat1_send_sms_info->input_content));
            ui_hide(CAT1_LAYOUT_SMS_INPUT_SYSTEM);
            ui_show(CAT1_LAYOUT_SMS_REPLY);
            return false;
        }
    case ELM_EVENT_TOUCH_DOWN:
        switch (elm->id) {
        case CAT1_TEXT_INPUT_NUMBER:
            log_info("CAT1_TEXT_INPUT_NUMBER DOWN\n");
            break;
        case CAT1_TEXT_INPUT_ALPHA:
            log_info("CAT1_TEXT_INPUT_ALPHA DOWN\n");
            break;
        case CAT1_TEXT_INPUT_SYMBOL:
            log_info("CAT1_TEXT_INPUT_SYMBOL DOWN\n");
            break;
        case CAT1_PIC_INPUT_SURE:
            log_info("CAT1_PIC_INPUT_SURE DOWN\n");
            break;
        case CAT1_PIC_INPUT_DEL:
            log_info("CAT1_PIC_INPUT_DEL DOWN\n");
            break;
        case CAT1_PIC_INPUT_1:
            log_info("CAT1_PIC_INPUT_1 DOWN\n");
            break;
        case CAT1_PIC_INPUT_2:
            log_info("CAT1_PIC_INPUT_2 DOWN\n");
            break;
        case CAT1_PIC_INPUT_3:
            log_info("CAT1_PIC_INPUT_3 DOWN\n");
            break;
        case CAT1_PIC_INPUT_4:
            log_info("CAT1_PIC_INPUT_4 DOWN\n");
            break;
        case CAT1_PIC_INPUT_5:
            log_info("CAT1_PIC_INPUT_5 DOWN\n");
            break;
        case CAT1_PIC_INPUT_6:
            log_info("CAT1_PIC_INPUT_6 DOWN\n");
            break;
        case CAT1_PIC_INPUT_7:
            log_info("CAT1_PIC_INPUT_7 DOWN\n");
            break;
        case CAT1_PIC_INPUT_8:
            log_info("CAT1_PIC_INPUT_8 DOWN\n");
            break;
        case CAT1_PIC_INPUT_9:
            log_info("CAT1_PIC_INPUT_9 DOWN\n");
            break;
        case CAT1_PIC_INPUT_10:
            log_info("CAT1_PIC_INPUT_10 DOWN\n");
            break;
        case CAT1_PIC_INPUT_11:
            log_info("CAT1_PIC_INPUT_11 DOWN\n");
            break;
        case CAT1_PIC_INPUT_NEXT_LETTER:
            log_info("CAT1_PIC_INPUT_NEXT_LETTER DOWN\n");
            break;
        default:
            break;
        }
        /* ui_core_highlight_element(elm, true); */
        ui_core_redraw(elm);
        return true;
    case ELM_EVENT_TOUCH_UP:
        switch (elm->id) {
        case CAT1_TEXT_INPUT_NUMBER:
            log_info("CAT1_TEXT_INPUT_NUMBER UP\n");
            cat1_send_sms_info->input_index = 3;
            ui_highlight_element_by_id(CAT1_TEXT_INPUT_NUMBER);
            ui_no_highlight_element_by_id(CAT1_TEXT_INPUT_ALPHA);
            ui_no_highlight_element_by_id(CAT1_TEXT_INPUT_SYMBOL);
            sms_pic_switch(3);
            {
                struct ui_pic *pic = ui_pic_for_id(CAT1_PIC_INPUT_NEXT_LETTER);
                ui_pic_set_image_index(pic, 1);
            }
            break;
        case CAT1_TEXT_INPUT_ALPHA:
            log_info("CAT1_TEXT_INPUT_ALPHA UP\n");
            cat1_send_sms_info->input_index = 0;
            ui_highlight_element_by_id(CAT1_TEXT_INPUT_ALPHA);
            ui_no_highlight_element_by_id(CAT1_TEXT_INPUT_NUMBER);
            ui_no_highlight_element_by_id(CAT1_TEXT_INPUT_SYMBOL);
            sms_pic_switch(0);
            {
                struct ui_pic *pic = ui_pic_for_id(CAT1_PIC_INPUT_NEXT_LETTER);
                ui_pic_set_image_index(pic, 0);
            }
            break;
        case CAT1_TEXT_INPUT_SYMBOL:
            log_info("CAT1_TEXT_INPUT_SYMBOL UP\n");
            cat1_send_sms_info->input_index = 4;
            ui_highlight_element_by_id(CAT1_TEXT_INPUT_SYMBOL);
            ui_no_highlight_element_by_id(CAT1_TEXT_INPUT_NUMBER);
            ui_no_highlight_element_by_id(CAT1_TEXT_INPUT_ALPHA);
            sms_pic_switch(4);
            {
                struct ui_pic *pic = ui_pic_for_id(CAT1_PIC_INPUT_NEXT_LETTER);
                ui_pic_set_image_index(pic, 1);
            }
            break;
        case CAT1_PIC_INPUT_SURE:
            log_info("CAT1_PIC_INPUT_SURE UP\n");
            cat1_set_send_sms_content(cat1_send_sms_info->input_content.content, 0, cat1_send_sms_info->input_content.index + 1);
            memcpy(&(cat1_send_sms_info->input_content), 0, sizeof(cat1_send_sms_info->input_content));
            cat1_send_sms_info->input_content.index = 0;
            ui_hide(CAT1_LAYOUT_SMS_INPUT_SYSTEM);
            ui_show(CAT1_LAYOUT_SMS_SURE);
            break;
        case CAT1_PIC_INPUT_DEL:
            log_info("CAT1_PIC_INPUT_DEL UP\n");
            sms_content_del();
            break;
        case CAT1_PIC_INPUT_1:
            log_info("CAT1_PIC_INPUT_1 UP\n");
            if (cat1_send_sms_info->input_index == 0) {
                sms_content_add('a');
            } else if (cat1_send_sms_info->input_index == 1) {
                sms_content_add('l');
            } else if (cat1_send_sms_info->input_index == 2) {
                sms_content_add('w');
            } else if (cat1_send_sms_info->input_index == 3) {
                sms_content_add('1');
            } else if (cat1_send_sms_info->input_index == 4) {
                sms_content_add(',');
            }
            break;
        case CAT1_PIC_INPUT_2:
            log_info("CAT1_PIC_INPUT_2 UP\n");
            if (cat1_send_sms_info->input_index == 0) {
                sms_content_add('b');
            } else if (cat1_send_sms_info->input_index == 1) {
                sms_content_add('m');
            } else if (cat1_send_sms_info->input_index == 2) {
                sms_content_add('x');
            } else if (cat1_send_sms_info->input_index == 3) {
                sms_content_add('2');
            } else if (cat1_send_sms_info->input_index == 4) {
                sms_content_add('.');
            }
            break;
        case CAT1_PIC_INPUT_3:
            log_info("CAT1_PIC_INPUT_3 UP\n");
            if (cat1_send_sms_info->input_index == 0) {
                sms_content_add('c');
            } else if (cat1_send_sms_info->input_index == 1) {
                sms_content_add('n');
            } else if (cat1_send_sms_info->input_index == 2) {
                sms_content_add('y');
            } else if (cat1_send_sms_info->input_index == 3) {
                sms_content_add('3');
            } else if (cat1_send_sms_info->input_index == 4) {
                sms_content_add('!');
            }
            break;
        case CAT1_PIC_INPUT_4:
            log_info("CAT1_PIC_INPUT_4 UP\n");
            if (cat1_send_sms_info->input_index == 0) {
                sms_content_add('d');
            } else if (cat1_send_sms_info->input_index == 1) {
                sms_content_add('o');
            } else if (cat1_send_sms_info->input_index == 2) {
                sms_content_add('z');
            } else if (cat1_send_sms_info->input_index == 3) {
                sms_content_add('4');
            } else if (cat1_send_sms_info->input_index == 4) {
                sms_content_add('?');
            }
            break;
        case CAT1_PIC_INPUT_5:
            log_info("CAT1_PIC_INPUT_5 UP\n");
            if (cat1_send_sms_info->input_index == 0) {
                sms_content_add('e');
            } else if (cat1_send_sms_info->input_index == 1) {
                sms_content_add('p');
            } else if (cat1_send_sms_info->input_index == 3) {
                sms_content_add('5');
            } else if (cat1_send_sms_info->input_index == 4) {
                sms_content_add(':');
            }
            break;
        case CAT1_PIC_INPUT_6:
            log_info("CAT1_PIC_INPUT_6 UP\n");
            if (cat1_send_sms_info->input_index == 0) {
                sms_content_add('f');
            } else if (cat1_send_sms_info->input_index == 1) {
                sms_content_add('q');
            } else if (cat1_send_sms_info->input_index == 3) {
                sms_content_add('6');
            } else if (cat1_send_sms_info->input_index == 4) {
                sms_content_add('(');
            }
            break;
        case CAT1_PIC_INPUT_7:
            log_info("CAT1_PIC_INPUT_7 UP\n");
            if (cat1_send_sms_info->input_index == 0) {
                sms_content_add('g');
            } else if (cat1_send_sms_info->input_index == 1) {
                sms_content_add('r');
            } else if (cat1_send_sms_info->input_index == 3) {
                sms_content_add('7');
            } else if (cat1_send_sms_info->input_index == 4) {
                sms_content_add(')');
            }
            break;
        case CAT1_PIC_INPUT_8:
            log_info("CAT1_PIC_INPUT_8 UP\n");
            if (cat1_send_sms_info->input_index == 0) {
                sms_content_add('h');
            } else if (cat1_send_sms_info->input_index == 1) {
                sms_content_add('s');
            } else if (cat1_send_sms_info->input_index == 3) {
                sms_content_add('8');
            } else if (cat1_send_sms_info->input_index == 4) {
                sms_content_add('#');
            }
            break;
        case CAT1_PIC_INPUT_9:
            log_info("CAT1_PIC_INPUT_9 UP\n");
            if (cat1_send_sms_info->input_index == 0) {
                sms_content_add('i');
            } else if (cat1_send_sms_info->input_index == 1) {
                sms_content_add('t');
            } else if (cat1_send_sms_info->input_index == 3) {
                sms_content_add('9');
            } else if (cat1_send_sms_info->input_index == 4) {
                sms_content_add(';');
            }
            break;
        case CAT1_PIC_INPUT_10:
            log_info("CAT1_PIC_INPUT_10 UP\n");
            if (cat1_send_sms_info->input_index == 0) {
                sms_content_add('j');
            } else if (cat1_send_sms_info->input_index == 1) {
                sms_content_add('u');
            } else if (cat1_send_sms_info->input_index == 3) {
                sms_content_add('0');
            } else if (cat1_send_sms_info->input_index == 4) {
                sms_content_add('*');
            }
            break;
        case CAT1_PIC_INPUT_11:
            log_info("CAT1_PIC_INPUT_11 UP\n");
            if (cat1_send_sms_info->input_index == 0) {
                sms_content_add('k');
            } else if (cat1_send_sms_info->input_index == 1) {
                sms_content_add('v');
            } else if (cat1_send_sms_info->input_index == 4) {
                sms_content_add('%');
            }
            break;
        case CAT1_PIC_INPUT_NEXT_LETTER:
            log_info("CAT1_PIC_INPUT_NEXT_LETTER UP\n");
            if (cat1_send_sms_info->input_index >= 0 && cat1_send_sms_info->input_index < 2) {
                cat1_send_sms_info->input_index++;
                sms_pic_switch(cat1_send_sms_info->input_index);
            } else if (cat1_send_sms_info->input_index == 2) {
                cat1_send_sms_info->input_index = 0;
                sms_pic_switch(cat1_send_sms_info->input_index);
            }
            break;
        default:
            break;
        }
        /* ui_core_redraw(elm); */
        ui_core_redraw(elm->parent);

        break;
    default:
        break;
    }
    return true;
}

static int layout_sms_input_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    switch (event) {
    case ON_CHANGE_INIT:
        break;
    case ON_CHANGE_SHOW:
        break;
    case ON_CHANGE_RELEASE:
        if (sms_show_str) {
            free(sms_show_str);
            sms_show_str = NULL;
        }
        break;
    }
    return 0;
}

REGISTER_UI_EVENT_HANDLER(CAT1_LAYOUT_SMS_INPUT_SYSTEM)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = sms_input_system_ontouch,
};
REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_INPUT_NUMBER)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = sms_input_system_ontouch,
};

REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_INPUT_ALPHA)
.onchange = text_input_alpha_onchange,
 .onkey = NULL,
  .ontouch = sms_input_system_ontouch,
};
REGISTER_UI_EVENT_HANDLER(CAT1_TEXT_INPUT_SYMBOL)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = sms_input_system_ontouch,
};
REGISTER_UI_EVENT_HANDLER(CAT1_PIC_INPUT_SURE)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = sms_input_system_ontouch,
};
REGISTER_UI_EVENT_HANDLER(CAT1_PIC_INPUT_DEL)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = sms_input_system_ontouch,
};
REGISTER_UI_EVENT_HANDLER(CAT1_PIC_INPUT_1)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = sms_input_system_ontouch,
};
REGISTER_UI_EVENT_HANDLER(CAT1_PIC_INPUT_2)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = sms_input_system_ontouch,
};
REGISTER_UI_EVENT_HANDLER(CAT1_PIC_INPUT_3)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = sms_input_system_ontouch,
};
REGISTER_UI_EVENT_HANDLER(CAT1_PIC_INPUT_4)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = sms_input_system_ontouch,
};
REGISTER_UI_EVENT_HANDLER(CAT1_PIC_INPUT_5)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = sms_input_system_ontouch,
};
REGISTER_UI_EVENT_HANDLER(CAT1_PIC_INPUT_6)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = sms_input_system_ontouch,
};
REGISTER_UI_EVENT_HANDLER(CAT1_PIC_INPUT_7)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = sms_input_system_ontouch,
};
REGISTER_UI_EVENT_HANDLER(CAT1_PIC_INPUT_8)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = sms_input_system_ontouch,
};
REGISTER_UI_EVENT_HANDLER(CAT1_PIC_INPUT_9)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = sms_input_system_ontouch,
};
REGISTER_UI_EVENT_HANDLER(CAT1_PIC_INPUT_10)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = sms_input_system_ontouch,
};
REGISTER_UI_EVENT_HANDLER(CAT1_PIC_INPUT_11)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = sms_input_system_ontouch,
};
REGISTER_UI_EVENT_HANDLER(CAT1_PIC_INPUT_NEXT_LETTER)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = sms_input_system_ontouch,
};

#endif // TCFG_CAT1_FUNC_SMS_ENABLE
#endif
#endif
