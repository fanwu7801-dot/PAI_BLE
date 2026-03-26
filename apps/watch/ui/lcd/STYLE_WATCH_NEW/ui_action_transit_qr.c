
#if TCFG_PAY_ALIOS_ENABLE

#if TCFG_PAY_TRANSITCODE_ENABLE

extern void set_cardcode_no_release();
extern u8 cardcode_is_need_release();
extern void clear_cardcode_no_release();
extern void transitcode_kill();
extern int ui_show_transitcodes_list(u8 event);



static u8 transit_qr_ok;//更新二维码时候不能刷新

struct _QR_CODE alipay_transit_qr_code = {
    .code128_mode = 60,
    .qr_version = 3,
    .qr_max_version = 16,
    .qr_ecc_level = 2,
    .qr_code_max_input_len = 512,
    .qr_buf_size = 4096 + 2048 + 1024,
    .img_w = 320,
};



#include "alipay_common.h"

static int qr_get_callback(int event, int err, int num, u8 *data, int len)
{
    printf("%s,%d err %d\n", __func__, __LINE__, err);
    if (!err) {
        printf("%s %d\n", data, len);
        put_buf(data, len);
        //释放的原因是里面会重新申请
        if (row_img) {
            br28_free(row_img);
            row_img = 0;
        }
        if (copy_row_img) {
            br28_free(copy_row_img);
            copy_row_img = 0;
        }
        jl_qr_code_process(data, len, &alipay_transit_qr_code);
        transit_qr_ok = 1;//在这之前ui不会操作二维码buf
        UI_MSG_POST("transitcode_finsh:err=%4", err);
    } else {
        //过期情况
        if (err == RV_CARD_DATA_OVERDUE) {
            extern int net_get_dhcp_flag();
            if (net_get_dhcp_flag()) {
                return 0;//带网络情况下可以更新
            }
        }
        //0直接显示二维码
        //1显示卡片列表
        //2显示进度
        //3显示选择界面
        //4城市卡片获取失败
        //5二维码卡片获取失败
        //6乘车码未开通
        ui_show_transitcodes_list(5);
        return true;//不带带网络情况退界面
    }
    return 0;
}

extern int local_transitcode_get_qr_data(int (*callback)(int, int, int, u8 *, int));




int ui_transitcode_flag;

//0直接显示二维码
//1显示卡片列表
//2显示进度
//3显示选择界面
//4城市卡片获取失败
//5二维码卡片获取失败
//6乘车码未开通
int ui_show_transitcodes_list(u8 event)
{
    u32 rets;
    __asm__ volatile("%0 = rets":"=r"(rets));
    printf("__func__ %s %x %d\n", __func__, rets, event);
    extern void set_cardcode_no_release();
    set_cardcode_no_release();//主动行为告诉ui不用释放线程
    ui_transitcode_flag = event;
    switch (event) {
    case 0:
        UI_WINDOW_BACK_SPEC_SHOW(PAGE_101);
        break;
    case 1:
        UI_WINDOW_BACK_SPEC_SHOW(PAGE_100);
        break;
    case 2:
        UI_WINDOW_BACK_SPEC_SHOW(PAGE_102);
        break;
    case 3:
        UI_WINDOW_BACK_SPEC_SHOW(PAGE_92);
        break;
    case 4:
    case 5:
    case 6:
        UI_WINDOW_BACK_SPEC_SHOW(PAGE_102);
        break;


    }
    return 0;
}


static int layout_sel_transitcode_onchange(void *_ctrl, enum element_change_event event, void *arg)
{

    struct element *elm = (struct element *)_ctrl;
    switch (event) {
    case ON_CHANGE_INIT:
        clear_cardcode_no_release();//进去前必须要清
        break;
    case ON_CHANGE_RELEASE:
        if (cardcode_is_need_release()) { //不是自主行为 需要释放任务
            transitcode_kill();
            ui_auto_shut_down_enable();//开启自动灭屏
        }
        clear_cardcode_no_release();//出去前也清
        break;
    }
    return FALSE;
}

static int layout_sel_transitcode_ontouch(void *ctr, struct element_touch_event *e)
{
    switch (e->event) {
    case ELM_EVENT_TOUCH_DOWN:
        return true;
        break;
    case ELM_EVENT_TOUCH_UP:
        set_cardcode_no_release();//主动行为
        UI_HIDE_CURR_WINDOW();
        ui_auto_shut_down_disable();
        extern int local_transitcode_get_default_card_info();
        local_transitcode_get_default_card_info();
        return true;
        break;
    default:
        return false;
        break;
    }
    return false;//不接管消息
}
REGISTER_UI_EVENT_HANDLER(UPAY_TRANSIT_SEL_LAYOUT)
.onchange = layout_sel_transitcode_onchange,
 .onkey = NULL,
  .ontouch = layout_sel_transitcode_ontouch,
};




static int layout_alipay_transitcode_qrcode_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct element *elm = (struct element *)_ctrl;
    struct draw_context *dc = (struct draw_context *)arg;
    struct rect rect = {0};

    switch (event) {
    case ON_CHANGE_INIT:
        printf("%s,%d \n", __func__, __LINE__);
        transit_qr_ok = 0;
        local_transitcode_get_qr_data(qr_get_callback);
        printf("%s,%d \n", __func__, __LINE__);
        break;
    case ON_CHANGE_SHOW:
        /*根据需要调用以下三个接口:ui_remove_backcolor,ui_remove_backimage,ui_remove_border*/
        /* ui_remove_backcolor(elm);//移除控件背景颜色 */
        /* ui_remove_backimage(elm);//移除控件背景图像 */
        /* ui_remove_border(elm);//移除控件边界 */
        break;
    case ON_CHANGE_SHOW_POST:
        ui_core_get_element_abs_rect(elm, &rect); //跟随控件移动,注释掉这句则不跟随控件移动
        if (transit_qr_ok) {
            int draw_width = (rect.width > rect.height) ? rect.height : rect.width;

            if (draw_width <= alipay_transit_qr_code.img_w) {
                printf("%d\n", draw_width);
                ASSERT(0);
            }
            ui_draw(dc,
                    NULL,
                    rect.left + (draw_width - alipay_transit_qr_code.img_w) / 2,
                    rect.top + (draw_width - alipay_transit_qr_code.img_w) / 2,
                    alipay_transit_qr_code.img_w,
                    alipay_transit_qr_code.img_w,
                    ui_draw_qrcode,
                    (void *)&alipay_transit_qr_code,
                    sizeof(struct _QR_CODE),
                    0);
        }
        break;

    case ON_CHANGE_RELEASE:
        if (row_img) {
            br28_free(row_img);
            row_img = 0;
        }
        if (copy_row_img) {
            br28_free(copy_row_img);
            copy_row_img = 0;
        }
        log_info("qr_code release:");
        transit_qr_ok = 0;
        break;
    }
    return false;
}
REGISTER_UI_EVENT_HANDLER(UPAY_TRANSIT_QR_DRAW_LAYOUT)
.onchange = layout_alipay_transitcode_qrcode_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};





static int transitcode_text_title_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct ui_text *text = (struct ui_text *)_ctrl;

    switch (event) {
    case ON_CHANGE_INIT:
        //根据布局实际高度重新调整控件大小
        extern char *alipay_transit_get_global_default_card_name();
        char *name = alipay_transit_get_global_default_card_name();
        ui_text_set_text_attrs(text, name, strlen(name), FONT_ENCODE_UTF8, 0, FONT_DEFAULT);
        break;
    case ON_CHANGE_RELEASE:
        break;
    }
    return FALSE;
}
REGISTER_UI_EVENT_HANDLER(UPAY_TRANSIT_QR_TEXT)
.onchange = transitcode_text_title_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

static int qr_msg_info_handler(const char *type, u32 arg)
{
    printf("_func_  == %s %d err %d\n", __func__, __LINE__, arg);
    struct element *elm = ui_core_get_element_by_id(UPAY_TRANSIT_QR_DRAW_LAYOUT);
    if (!elm) {
        return -1;
    }
    return ui_core_redraw(elm);
}



static const struct uimsg_handl ui_msg_handler[] = {
    { "transitcode_finsh",  qr_msg_info_handler     }, /* qr 信息 */
    { NULL, NULL},      /* 必须以此结尾！ */
};






static int transitcode_qr_layout_ontouch(void *ctr, struct element_touch_event *e)
{
    switch (e->event) {
    case ELM_EVENT_TOUCH_DOWN:
        return true;
        break;

    case ELM_EVENT_TOUCH_UP:
        transit_qr_ok = 0;
        local_transitcode_get_qr_data(qr_get_callback);//更新二维码
        return true;
        break;

    case ELM_EVENT_TOUCH_R_MOVE:
        extern int alipay_tansit_num_get();
        set_cardcode_no_release();//主动行为不用释放
        if (alipay_tansit_num_get()) { //有数量
            //0直接显示二维码
            //1显示卡片列表
            //2显示进度
            extern int ui_show_transitcodes_list(u8 envent);
            ui_show_transitcodes_list(1);
        } else {
            extern int local_transitcode_cardlist_get();
            local_transitcode_cardlist_get();//获取卡片再显示
        }

        return true;

        break;
    default:
        return false;
        break;
    }
    return false;//不接管消息
}







static int transitcode_qr_layout_onchange(void *_ctrl, enum element_change_event event, void *arg)
{

    struct element *elm = (struct element *)_ctrl;
    switch (event) {
    case ON_CHANGE_INIT:
        ui_register_msg_handler(PAGE_101, ui_msg_handler);//注册消息交互的回调
        clear_cardcode_no_release();//进去前必须要清
        break;
    case ON_CHANGE_RELEASE:
        if (cardcode_is_need_release()) {
            transitcode_kill();
            ui_auto_shut_down_enable();//开启自动灭屏
        }
        clear_cardcode_no_release();//出去前也清
        break;
    }
    return FALSE;
}

REGISTER_UI_EVENT_HANDLER(UPAY_TRANSIT_QR_LAYOUT)
.onchange = transitcode_qr_layout_onchange,
 .onkey = NULL,
  .ontouch = transitcode_qr_layout_ontouch,
};


#endif


#endif /* #if TCFG_PAY_ALIOS_ENABLE */


