
#include "ui/ui.h"
#include "app_config.h"
#include "ui/ui_api.h"
#include "system/timer.h"
#include "key_event_deal.h"
#include "jiffies.h"
#include "app_task.h"
#define LOG_TAG_CONST       UI
#define LOG_TAG     		"[UI-ACTION]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"



#if (!TCFG_LUA_ENABLE) && (TCFG_PAY_ALIOS_ENABLE)

#if TCFG_UI_ENABLE_FILE
#if TCFG_SPI_LCD_ENABLE
#define STYLE_NAME  JL//必须要


#if TCFG_PAY_TRANSITCODE_ENABLE


extern void set_cardcode_no_release();
extern u8 cardcode_is_need_release();
extern void clear_cardcode_no_release();
extern void transitcode_kill();




extern char *alipay_transit_name_get_by_index(int index);

struct grid_set_info {
    int flist_index;  //文件列表首项所指的索引
    int cur_total;
    FILE *file;
    struct vfscan *fs;
    FS_DIR_INFO *dir_buf;
    int    show_temp;
#if (TCFG_LFN_EN)
    u8  lfn_buf[512];
#endif//TCFG_LFN_EN
};

static struct grid_set_info *ghandler = NULL;
#define __this 	(ghandler)
#define sizeof_this     (sizeof(struct grid_set_info))

static int close_file_handler();
static int brow_children_redraw(int id);


static int open_file_handler(int show_temp)
{
    extern int alipay_tansit_num_get();
    __this = zalloc(sizeof_this);
    __this->cur_total = alipay_tansit_num_get();
    if (!__this->cur_total) {
        return -1;
    }
    return 0;

}

static int close_file_handler()
{

    if (!__this) {
        return -1;
    }

    if (__this->fs) {
        fscan_release(__this->fs);
        __this->fs = NULL;
    }

    if (__this->file) {
        fclose(__this->file);
        __this->file = NULL;
    }

    if (__this->dir_buf) {
        free(__this->dir_buf);
        __this->dir_buf = NULL;
    }

    free(__this);
    __this = NULL;
    return 0;
}

static int file_select_enter(u32 index)
{
    char *info = alipay_transit_name_get_by_index(index);
    if (!info) {
        return -1;
    }
    if (!__this || !__this->cur_total) {
        return -1;
    }
    extern int local_transitcode_set_default_card_info_by_index(int index);
    local_transitcode_set_default_card_info_by_index(index);

    //0直接显示二维码
    //1显示卡片列表
    //2显示进度
    extern int ui_show_transitcodes_list(u8);
    ui_show_transitcodes_list(0);
    return 0;
}


static int grid_child_cb(void *_ctrl, int id, int type, int index)
{
    char *info = alipay_transit_name_get_by_index(index);
    switch (type) {
    case CTRL_TYPE_PROGRESS:
        break;
    case CTRL_TYPE_MULTIPROGRESS:
        break;
    case CTRL_TYPE_TEXT:
        struct ui_text *text = (struct ui_text *)_ctrl;
        if (!strcmp(text->source, "title")) {
            text->elm.css.invisible = !!index;
            break;
        }
        if (!info) {
            return 0;
        }
        if (!strcmp(text->source, "name")) {
            text->attrs.offset = 0;
            text->attrs.format = "text";
            text->attrs.flags  = FONT_DEFAULT;
            if (info) {
                text->attrs.encode = FONT_ENCODE_UTF8;
                text->attrs.str    = info;
                text->attrs.strlen = strlen(info);
            }
            text->elm.css.invisible = 0;
        }
        break;
    case CTRL_TYPE_NUMBER:
        struct ui_number *number = (struct ui_number *)_ctrl;
        break;
    case CTRL_TYPE_PIC:
        struct ui_pic *pic = (struct ui_pic *)_ctrl;
        break;
    case CTRL_TYPE_TIME:
        break;
    }
    return 0;
}


static int transitcode_file_switch_ontouch(void *ctr, struct element_touch_event *e)
{
    struct ui_grid *grid = (struct ui_grid *)ctr;
    int sel_item;
    static u8 move_flag = 0;
    switch (e->event) {
    case ELM_EVENT_TOUCH_R_MOVE:
        extern int alipay_tansit_num_get();
        set_cardcode_no_release();//主动行为不用释放
        extern int ui_show_transitcodes_list(u8 event);
        ui_show_transitcodes_list(3);
        return TRUE;

    case ELM_EVENT_TOUCH_L_MOVE:
        log_info("line:%d", __LINE__);
        break;
    case ELM_EVENT_TOUCH_MOVE:
        if (!__this) {
            return TRUE;
        }
        move_flag = 1;
        return false;
        break;
    case ELM_EVENT_TOUCH_DOWN:
        move_flag = 0;
        return false;//不接管消息
        break;
    case ELM_EVENT_TOUCH_UP:
        if (move_flag) {
            move_flag = 0;
            return false;//不接管消息
        }
        sel_item = ui_grid_cur_item_dynamic(grid);
        file_select_enter(sel_item);
        return false;//不接管消息

        break;
    default:
        return false;
        break;
    }
    return false ;//不接管
    //return true;//接管消息
}



static int brows_children_init(struct ui_grid *grid)
{
    struct element *k;
    int count =  0;//
    if (!grid) {
        return 0;
    }

    if (__this) {
        count += __this->cur_total;
    }

    for (int i = 0; i < grid->avail_item_num; i++) {
        if (i < count) {
            /* list_for_each_child_element(k, &grid->item[i].elm) { */
            /* grid_child_cb(k, k->id, ui_id2type(k->id), i); */
            /* } */
            grid->item[i].elm.css.invisible = 0;
        } else {
            grid->item[i].elm.css.invisible = 1;
        }
    }
    return 0;
}






static int browse_enter_child_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct element *k ;
    struct element *elm = (struct element *)_ctrl;
    if (event == ON_CHANGE_UPDATE_ITEM) {
        int index = (u32)arg;
        grid_child_cb(elm, elm->id, ui_id2type(elm->id), index);

    }
    return 0;
}

static int transitcode_browse_enter_onchane(void *ctr, enum element_change_event e, void *arg)
{
    struct ui_grid *grid = (struct ui_grid *)ctr;
    int fnum = 0;

    switch (e) {
    case ON_CHANGE_INIT:
        clear_cardcode_no_release();//进去时候清标
        open_file_handler(grid->avail_item_num);
        {
            int row = __this->cur_total;
            int col = 1;
            ui_set_default_handler(grid, NULL, NULL, browse_enter_child_onchange);
            ui_grid_init_dynamic(grid, &row, &col);

        }
        break;
    case ON_CHANGE_RELEASE:
        close_file_handler();
        ui_set_default_handler(grid, NULL, NULL, NULL);
        //判断是否需要释放任务
        if (cardcode_is_need_release()) {
            transitcode_kill();
            ui_auto_shut_down_enable();//开启自动灭屏
        }
        clear_cardcode_no_release();
        //清标
        break;
    case ON_CHANGE_FIRST_SHOW:
        break;
    default:
        return false;
    }
    return false;
}


REGISTER_UI_EVENT_HANDLER(TRANSITCODE_LIST)
.onchange = transitcode_browse_enter_onchane,
 .onkey = NULL,
  .ontouch = transitcode_file_switch_ontouch,
};








static int transitcode_load_ing_onchane(void *ctr, enum element_change_event e, void *arg)
{
    struct element *elm = (struct element *)ctr;
    switch (e) {
    case ON_CHANGE_INIT:
        printf("%s %d\n", __FUNCTION__, __LINE__);
        extern int ui_transitcode_flag;
        //0直接显示二维码
        //1显示卡片列表
        //2显示进度
        //3显示选择界面
        //4城市卡片获取失败
        //5二维码卡片获取失败
        //6乘车码未开通
        //7未知错误
        if (ui_transitcode_flag != 2) {
            elm->css.invisible = 1;
        }

        break;
    case ON_CHANGE_RELEASE:
        break;
    case ON_CHANGE_FIRST_SHOW:
        break;
    default:
        return false;
    }
    return false;
}


REGISTER_UI_EVENT_HANDLER(TRANSITCODE_ING_LAYOUT)
.onchange = transitcode_load_ing_onchane,
 .onkey = NULL,
  .ontouch = NULL,
};









static int transitcode_err_onchane(void *ctr, enum element_change_event e, void *arg)
{
    struct element *elm = (struct element *)ctr;
    switch (e) {
    case ON_CHANGE_INIT:
        printf("%s %d\n", __FUNCTION__, __LINE__);
        clear_cardcode_no_release();//进去前必须要清
        extern int ui_transitcode_flag;
        //0直接显示二维码
        //1显示卡片列表
        //2显示进度
        //3显示选择界面
        //4城市卡片获取失败
        //5二维码卡片获取失败
        //6乘车码未开通
        //7未知错误

        if (ui_transitcode_flag == 2) {
            elm->css.invisible = 1;
        }
        break;
    case ON_CHANGE_RELEASE:
        //判断是否需要释放任务
        if (cardcode_is_need_release()) {
            transitcode_kill();
            ui_auto_shut_down_enable();//开启自动灭屏
        }
        clear_cardcode_no_release();
        //清标
        break;
    case ON_CHANGE_FIRST_SHOW:
        printf("%s %d\n", __FUNCTION__, __LINE__);
        struct ui_text *text = ui_core_get_element_by_id(TRANSITCODE_ERR_TEXT);
        if (text) {
            if (ui_transitcode_flag == 4) {
                ui_text_set_index(text, 0);
            }
            if (ui_transitcode_flag == 6) {
                ui_text_set_index(text, 2);
            }
            if (ui_transitcode_flag == 5) {
                ui_text_set_index(text, 1);
            }
            if (ui_transitcode_flag == 7) {
                ui_text_set_index(text, 3);
            }
        }
        break;
    default:
        return false;
    }
    return false;
}

REGISTER_UI_EVENT_HANDLER(TRANSITCODE_ERR_LAYOUT)
.onchange = transitcode_err_onchane,
 .onkey = NULL,
  .ontouch = NULL,
};


static int transitcode_text_err_button(void *ctr, struct element_touch_event *e)
{
    printf("__FUNCTION__ = %s %d\n", __FUNCTION__, e->event);
    switch (e->event) {
    case ELM_EVENT_TOUCH_DOWN:
        return true;
    case ELM_EVENT_TOUCH_UP:
        //3显示选择界面
        ui_show_transitcodes_list(3);
        return true;
        break;
    default:
        break;
    }
    return false;

}

REGISTER_UI_EVENT_HANDLER(TRANSITCODE_ERR_TEXT_BUTTON)
.onchange = NULL,
 .onkey = NULL,
  .ontouch = transitcode_text_err_button,
};





#endif
#endif
#endif /* #if TCFG_APP_MUSIC_EN */
#endif /* #if (!TCFG_LUA_ENABLE) */

