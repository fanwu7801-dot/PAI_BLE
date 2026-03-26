
#include "app_config.h"
#include "system/includes.h"
#include "ui/ui_sys_param.h"
#include "ui/ui_style.h"
#include "ui/ui.h"
#include "ui/ui_api.h"
#include "app_task.h"
#include "system/timer.h"
#include "key_event_deal.h"
#include "res/resfile.h"
#include "ui/res_config.h"
#include "asm/psram_api.h"
#include "asm/imb.h"
#include "res/jpeg_decoder.h"
#include "demo/photos/load_jpeg_from_sd.h"
#include "btstack/avctp_user.h"

#if TCFG_UI_ENABLE && (!TCFG_LUA_ENABLE)
#if TCFG_UI_AVRCP_MUSIC_BG_ENABLE
/*************************************************************************/
//			avrcp传输音乐图片,模块功能部分
/*流程：
  蓝牙数据
  ===> bip_rx_data_handle
  ===> 数据拷贝到链表
  ===> 在bip_deal线程写入临时文件，避免与ui同时读写文件
  ===> 传输结束，重命名文件
  ***注意跨线程操作时，需要做好链表、资源文件、流程同步
 */
/*************************************************************************/
extern void bip_file_set_callback(void (*callback)(void));
extern u8 *bip_file_path_get();
extern u8 bip_file_status_get();
extern u8 *bip_file_name_get();
extern u8 *bip_file_tmp_path_get();
#if 1
//配置
/* #define BIP_FILE_PATH  	"storage/sd0/C/musicbgp.jpg"//存储到sd卡 */
#define BIP_FILE_PATH  			"storage/fat_nand/C/musicbgp.jpg"//存储到flash的资源区
#define BIP_FILE_PATH_TMP		"storage/fat_nand/C/mbgtmp.jpg"
#define BIP_FILE_NAME			"musicbgp.jpg"
#define BIP_MSG_UPDATE			1
#define BIP_GET_IMAGE_DELAY		1000	//ms，延时一秒取图片，可以适当加大，避免快速切换时获取图片不对
enum {
    BIP_DATA_STATUS_START = 0X01,	//开始包
    BIP_DATA_STATUS_CONTINUE,		//继续包(中间包)
    BIP_DATA_STATUS_STOP,			//结束包
    BIP_DATA_STATUS_ERR,			//错误包，可能是不支持，或者音乐软件未打开
};
enum {
    BIP_FILE_STATUS_ERR,			//文件不存在
    BIP_FILE_STATUS_OK,				//文件存在
    BIP_FILE_STATUS_UPDATE,			//文件更新中
};
struct bip_block {
    struct list_head head;			//list
    u8 *buf;						//待写入数据包
    u32 type: 8;						//数据包类型
    u32 buf_len: 16;					//数据包长度
    u32 rev: 8;						//保留位
};									//数据块
struct bip_file_info {
    FILE *fp; 						//传输文件句柄
    void(*callback)(void);			//传输完成回调
    struct bip_block bip_block_head;//block头
    spinlock_t bip_block_lock;		//block锁
    u32 file_status: 3;				//文件状态
    u32 bip_deal_task_init: 1;		//线程初始化
    u32 en: 1;						//使能
    u16 tid;
};
volatile struct bip_file_info bip_file;
#define __bip_info (&bip_file)

u32 bip_block_cnt_len_max;			//最大ram统计
volatile u32 bip_block_cnt_len; 	//ram统计

u8 *bip_file_path_get()
{
    return 	BIP_FILE_PATH;
}
u8 *bip_file_tmp_path_get()
{
    return 	BIP_FILE_PATH_TMP;
}
u8 *bip_file_name_get()
{
    return BIP_FILE_NAME;
}
u8 bip_file_status_get()
{
    return __bip_info->file_status;
}
u8 bip_file_is_update()
{
    printf("%s %d", __func__, __bip_info->file_status);
    if (__bip_info->file_status == BIP_FILE_STATUS_UPDATE) {
        return true;
    }
    return false;
}
void bip_file_set_callback(void (*callback)(void))
{
    __bip_info->callback = callback;
}
void bip_file_enable()
{
    __bip_info->en = 1;
}
void bip_file_disable()
{
    __bip_info->en = 0;
}
/* ------------------------------------------------------------------------------------*/
/**
 * @brief bip_block_init 数据块链表初始化
 */
/* ------------------------------------------------------------------------------------*/
void bip_block_init()
{
    if (__bip_info->bip_block_head.type) {
        return;
    }
    INIT_LIST_HEAD(&__bip_info->bip_block_head.head);
    __bip_info->bip_block_head.buf_len = 0;
    __bip_info->bip_block_head.type = 1;
    spin_lock_init(&__bip_info->bip_block_lock);
}
int bip_block_alloc(u8 *in_buf, int in_len, int type)
{
    struct bip_block *new_block = zalloc(sizeof(struct bip_block));
    new_block->buf_len = in_len;
    new_block->buf = malloc(in_len);
    memcpy(new_block->buf, in_buf, in_len);
    new_block->type = type;
    /* printf("%s buf:%x len:%d type:%d\n",__func__,new_block->buf,new_block->buf_len,new_block->type); */
    spin_lock(&__bip_info->bip_block_lock);
    list_add_tail(&new_block->head, &__bip_info->bip_block_head.head);
    __bip_info->bip_block_head.buf_len++;
    bip_block_cnt_len += in_len;
    if (bip_block_cnt_len > bip_block_cnt_len_max) {
        bip_block_cnt_len_max = bip_block_cnt_len;
        printf("%s len_max%d", __func__, bip_block_cnt_len_max);
    }
    spin_unlock(&__bip_info->bip_block_lock);
    return 0;
}
int bip_block_free(struct bip_block *block)
{
    bip_block_cnt_len -= block->buf_len;
    free(block->buf);
    free(block);
    return 0;
}
struct bip_block *bip_block_get()
{
    struct bip_block *p = NULL;
    struct bip_block *n = NULL;
    struct bip_block *r = NULL;
    spin_lock(&__bip_info->bip_block_lock);
    list_for_each_entry_safe(p, n, &__bip_info->bip_block_head.head, head) {
        if (p != &__bip_info->bip_block_head) {
            /* printf("%s buf:%x len:%d type:%d\n",__func__,p->buf,p->buf_len,p->type); */
            list_del(p);
            __bip_info->bip_block_head.buf_len--;
            r = p;
            break;
        }
    }
    spin_unlock(&__bip_info->bip_block_lock);

    return r;
}
void bip_deal_cb(void *p)
{
    int msg[32];
    while (1) {
        memset(msg, 0, sizeof(msg));
        int ret = os_taskq_pend(NULL, msg, ARRAY_SIZE(msg)); //500ms_reflash
        if (ret != OS_TASKQ) {
            continue;
        }
        if (msg[0] == BIP_MSG_UPDATE) {
            struct bip_block *p = bip_block_get();
            if (!p) {
                break;
            }
            u8 bip_data_status = p->type;
            u8 *packet  = p->buf;
            u16 body_len = p->buf_len;

            printf("%s type:%d\n", __func__, bip_data_status);
            switch (bip_data_status) {
            case BIP_DATA_STATUS_START: //收到第一包数据
                u8 *bit_file_path = bip_file_tmp_path_get();
                /* printf("bip_file_path: %s\n", bit_file_path); */
                //删除旧文件
                __bip_info->fp = fopen(bit_file_path, "r");
                if (__bip_info->fp) {
                    __bip_info->file_status = BIP_FILE_STATUS_ERR;
                    fdelete(__bip_info->fp);
                    __bip_info->fp = NULL;
                }
                //新增文件
                __bip_info->fp = fopen(bit_file_path, "w+");
                if (__bip_info->fp) {
                    __bip_info->file_status = BIP_FILE_STATUS_UPDATE,
                                fwrite(__bip_info->fp, packet, body_len);
                } else {
                    printf("bip file open err\n");
                }
                break;
            case BIP_DATA_STATUS_CONTINUE: //收到文件数据
                if (__bip_info->fp) {
                    fwrite(__bip_info->fp, packet, body_len);
                } else {
                    printf("bip file open err\n");
                }
                break;
            case BIP_DATA_STATUS_STOP: //收到结束命令
                if (__bip_info->fp) {
                    fwrite(__bip_info->fp, packet, body_len);
                    fclose(__bip_info->fp);
                    __bip_info->fp = NULL;
                } else {
                    printf("bip file open err\n");
                }
                if (__bip_info->callback) {
                    __bip_info->callback();
                }
                __bip_info->file_status = BIP_FILE_STATUS_OK;
                break;
            case BIP_DATA_STATUS_ERR:
                printf("BIP_DATA_STATUS_ERR");
                break;
            }
            bip_block_free(p);

        }
    }
}
/* ------------------------------------------------------------------------------------*/
/**
 * @brief bip_deal_task_create 创建线程
 */
/* ------------------------------------------------------------------------------------*/
void bip_deal_task_create()
{
    if (!__bip_info->bip_deal_task_init) {
        task_create(bip_deal_cb, NULL, "bip_deal");
        __bip_info->bip_deal_task_init = 1;
    }
}
/* ------------------------------------------------------------------------------------*/
/**
 * @brief bip_block_to_flash_sync 通知bip_deal线程写文件
 */
/* ------------------------------------------------------------------------------------*/
void bip_block_to_flash_sync()
{
    if (!__bip_info->bip_deal_task_init) {
        bip_deal_task_create();
        /* return;	 */
    }
    int msg[3] = {0};
    msg[0] = (int)BIP_MSG_UPDATE;
    msg[1] = 0;
    do {
        int os_err = os_taskq_post_type("bip_deal", msg[0], 1, &msg[1]);
        if (os_err == OS_ERR_NONE) {
            break;
        }
        if (os_err != OS_Q_FULL) {
            return ;
        }
        os_time_dly(1);
    } while (1);

}
/* ------------------------------------------------------------------------------------*/
/**
 * @brief bip_update_pic_file 获取文件
 */
/* ------------------------------------------------------------------------------------*/
void bip_get_pic_file_cb(void *p)
{
    if (__bip_info->tid) {
        sys_timer_del(__bip_info->tid);
        __bip_info->tid = 0;
    }
    user_send_cmd_prepare(USER_CTRL_BIP_GET_IMAGE, 0, NULL);
}
void bip_update_pic_file()
{
    if (__bip_info->tid) {
        sys_timer_modify(__bip_info->tid, BIP_GET_IMAGE_DELAY);
    } else {
        __bip_info->tid = sys_timer_add(NULL, bip_get_pic_file_cb, BIP_GET_IMAGE_DELAY);
    }
}
/* ------------------------------------------------------------------------------------*/
/**
 * @brief bip_rx_data_handle	音乐图片数据回调
 *
 * @param packet	数据包内容
 * @param body_len	数据包长度
 * @param length	整个图片大小,只有ios支持在第一包返回
 * @param bip_data_status	数据状态
 */
/* ------------------------------------------------------------------------------------*/
void bip_rx_data_handle(u8 *packet, u16 body_len, u32 length, u8 bip_data_status)
{
    printf("<%s>status:%d task:%s len:%d\n", __func__, bip_data_status, os_current_task(), body_len);
    switch (bip_data_status) {
    case BIP_DATA_STATUS_START: //收到第一包数据
        bip_block_init();
        bip_block_alloc(packet, body_len, bip_data_status);
        bip_block_to_flash_sync();
        break;
    case BIP_DATA_STATUS_CONTINUE: //收到文件数据
        bip_block_alloc(packet, body_len, bip_data_status);
        bip_block_to_flash_sync();
        break;
    case BIP_DATA_STATUS_STOP: //收到结束命令
        bip_block_alloc(packet, body_len, bip_data_status);
        bip_block_to_flash_sync();
        break;
    case BIP_DATA_STATUS_ERR:
        printf("BIP_DATA_STATUS_ERR");
        break;
    }

}
#endif



/******************************************************************/
//			AVRCP音乐背景DEMO---UI
/******************************************************************/
#define STYLE_NAME  JL
#define OUTPUT_FORMAT_RGB565 1

#define MUSIC_BG_CENTER_X	    (get_lcd_width_from_imd()/2)	/*图片中心x*/
#define MUSIC_BG_CENTER_Y		(get_lcd_height_from_imd()/2)	/*图片中心y*/
#define MUSIC_BG_MAX_X		    300								/*不能超过布局宽高*/
#define MUSIC_BG_MAX_Y			300								/*不能超过布局宽高*/
#define MUSIC_DEC_FORMAT    OUTPUT_FORMAT_RGB565        		/*输出解码格式*/
#if (TCFG_PSRAM_DEV_ENABLE)							    		/*使用psram缓存*/
#define MALLOC	malloc_psram
#define FREE	free_psram
#else
#define MALLOC	malloc
#define FREE	free
#endif

#define USELESS_RAM				1								//省ram


struct music_bg_info {
    u8 init;			//初始化
    u8 *buf;			//缓存buffer
    u32 buf_len;		//缓存buffer长度
    s16 x_start;		//绘图起始坐标
    s16 y_start;		//绘图起始坐标
    u16 x_width;		//绘图宽度
    u16 y_height;		//绘图宽度
    u16 file_width;		//jpg背景文件实际宽度
    u16 file_height;	//jpg背景文件实际高度
    u16 file_dec_width;	//文件解码宽度
    u16 file_dec_height;//文件解码高度
    u16 timer_id;
};
struct music_bg_info music_bg_info;	//模块句柄
#define __this (&music_bg_info)		//模块指针

/* ------------------------------------------------------------------------------------*/
/**
 * @brief music_bg_draw_cb 绘图回调
 *
 * @param id				默认为0
 * @param dst_buf			绘图区域buffer
 * @param dst_r				绘图区域rect
 * @param src_r				源数据区域rect
 * @param bytes_per_pixel	2 rgb565
 * @param priv				私有句柄
 */
/* ------------------------------------------------------------------------------------*/
static void music_bg_draw_cb(int id, u8 *dst_buf, struct rect *dst_r, struct rect *src_r, u8 bytes_per_pixel, void *priv)
{
    struct rect r;
    struct music_bg_info *pinfo = (struct music_bg_info *)priv;

    int src_stride = (pinfo->file_dec_width * bytes_per_pixel + 3) / 4 * 4;
    int dst_stride = (dst_r->width * bytes_per_pixel + 3) / 4 * 4;
    int x_offset = (pinfo->file_width - pinfo->x_width) / 2;
    int y_offset =	(pinfo->file_height - pinfo->y_height) / 2;
    /* printf("stride %d %d offset %d %d",src_stride,dst_stride,x_offset,y_offset);  */

    int w, h;
    if (get_rect_cover(src_r, dst_r, &r)) {
        for (h = 0; h < r.height; h++) {
            int dst_start = (r.top + h - dst_r->top) * dst_stride + (r.left - dst_r->left) * 2;
            int src_start = (r.top + h - src_r->top + y_offset) * src_stride + (r.left - src_r->left + x_offset) * 2;
            /* printf("%s h:%d dst%d src:%d len:%d",__func__,h,dst_start,src_start,r.width*2); */
            memcpy(&dst_buf[dst_start],
                   &pinfo->buf[src_start],
                   r.width * 2);
        }
    }
}
static void music_bg_draw_cb_useless_ram(int id, u8 *dst_buf, struct rect *dst_r, struct rect *src_r, u8 bytes_per_pixel, void *priv)
{
    /* printf("%s [%d %d %d %d]",__func__,src_r->left,src_r->top,src_r->width,src_r->height); */
    if (dst_r->top == 0) {
        load_jpg_from_sd_reset();
    }
    load_jpg_from_sd_start_with_rect(bip_file_path_get(), MUSIC_DEC_FORMAT, dst_buf, dst_r, src_r);
}
/* ------------------------------------------------------------------------------------*/
/**
 * @brief music_bg_draw 音乐背景绘图
 *
 * @param _dc			dc指针 在show_post时获取
 * @param x				绘制区域的起始水平坐标
 * @param y				绘制区域的起始垂直坐标
 * @param width			绘制区域宽度
 * @param height		绘制区域高度
 * @param info			音乐背景句柄
 */
/* ------------------------------------------------------------------------------------*/
static void __music_bg_draw(void *_dc, int x, int y, int width, int height, struct music_bg_info *info)
{
#if USELESS_RAM
    ui_draw(_dc, NULL, x, y, width, height, music_bg_draw_cb_useless_ram, info, sizeof(struct music_bg_info), 0);
#else
    ui_draw(_dc, NULL, x, y, width, height, music_bg_draw_cb, info, sizeof(struct music_bg_info), 0);
#endif
}

/* ------------------------------------------------------------------------------------*/
/**
 * @brief music_bg_draw_image 绘制音乐背景
 *
 * @param dc dc指针
 */
/* ------------------------------------------------------------------------------------*/
static void music_bg_draw_image(void *dc)
{
    /* if(!__this->init){ */
    /* return ; */
    /* } */
    /* __this->x_width = (__this->file_width > MUSIC_BG_MAX_X)? MUSIC_BG_MAX_X:__this->file_width; */
    /* __this->x_start = MUSIC_BG_CENTER_X - __this->x_width/2; */
    /* __this->y_height = (__this->file_height > MUSIC_BG_MAX_Y)? MUSIC_BG_MAX_Y:__this->file_height; */
    /* __this->y_start = MUSIC_BG_CENTER_Y - __this->y_height/2; */
    __this->x_start = 77;
    __this->y_start = 77;
    __this->x_width = 300;
    __this->y_height = 300;
    __music_bg_draw(dc, __this->x_start, __this->y_start, __this->x_width, __this->y_height, __this);
}

/* ------------------------------------------------------------------------------------*/
/**
 * @brief bip_jpeg_buf_malloc 音乐背景申请buffer
 *
 * @param r	图像信息
 *
 * @return buffer地址
 */
/* ------------------------------------------------------------------------------------*/
u8 *bip_jpeg_buf_malloc(struct rect *r)
{
    int buf_len = 2 * r->width * r->height;
    u8 *buf_addr = NULL;
    if (__this->buf) {
        if (__this->buf_len == buf_len) {
            buf_addr = __this->buf;
        } else {
            free(__this->buf);
            __this->buf = NULL;
            buf_addr = MALLOC(buf_len);
        }
    } else {
        buf_addr = MALLOC(buf_len);
    }
    /* printf("%s %d %d %d %x",__func__,r->width,r->height,buf_len,buf_addr); */
    memset(buf_addr, 0, buf_len);
    return buf_addr;
}
/* ------------------------------------------------------------------------------------*/
/**
 * @brief music_bg_load_jpg	加载音乐背景图
 *
 * @return 0 正常 -1 异常
 */
/* ------------------------------------------------------------------------------------*/
static int music_bg_load_jpg()
{
    int file_status  = bip_file_status_get();
    if (file_status != 1) {
        printf("file_status error:%d", file_status);
        return -1;
    }
    __this->buf = load_jpg_from_sd_start_with_buf_malloc(bip_file_path_get(), MUSIC_DEC_FORMAT, bip_jpeg_buf_malloc);
    struct jpeg_decoder_fd *jpg_fd = jpeg_dec_get_fd();
    __this->file_width  = jpg_fd->info.x;
    __this->file_height = jpg_fd->info.y;
    __this->file_dec_width = jpg_fd->info.x;
    __this->file_dec_height = jpg_fd->info.y_dec;
    __this->buf_len = 2 * jpg_fd->info.x_dec * jpg_fd->info.y_dec;
    printf("<%s>:path:%s\n [%d %d]buf(%d):%x", __func__, bip_file_path_get(), __this->file_width, __this->file_height, __this->buf_len, __this->buf);
    if (__this->buf) {
        return 0;
    } else {
        return -1;
    }
}

/* ------------------------------------------------------------------------------------*/
/**
 * @brief music_bg_layout_init 初始化音乐背景
 */
/* ------------------------------------------------------------------------------------*/
static void music_bg_layout_init()
{
    printf("%s", __func__);
    if (load_jpg_from_sd_init()) {
        printf("load_jpg_from_sd_init error\n");
        return;
    }
    printf("%s %x", __func__, __this->buf);
    __this->init = 1;
    /* music_bg_load_jpg(); */
}
static void __music_bg_page_update()
{
    u8 *path_tmp =  bip_file_tmp_path_get();
    u8 *path = bip_file_path_get();
    int ret = -1;
    FILE *fp_tmp = fopen(path_tmp, "r");
    if (fp_tmp) {
        printf("%s file_len:%d", __func__, flen(fp_tmp));
        FILE *fp = fopen(path, "r");
        if (fp) {
            fdelete(fp);
        }
        ret = frename(fp_tmp, bip_file_name_get());
        fclose(fp_tmp);
    }
    if (!ret) {
        u32 page_id = ui_get_current_window_id();
        struct element *elm = ui_core_get_element_by_id(page_id);
        if (elm) {
            ui_core_redraw(elm);
        }
    }
}
static void music_bg_page_redraw()
{
    int argu[3];
    argu[0] = (int) __music_bg_page_update;
    argu[1] = 0x0;
    argu[2] = 0x0;
    int ret = os_taskq_post_type("ui", Q_CALLBACK, 2, argu);
}
static void music_bg_update()
{
    printf("%s", __func__);
    /* music_bg_load_jpg(); */
    music_bg_page_redraw();
}

/* ------------------------------------------------------------------------------------*/
/**
 * @brief music_bg_layout_free 释放音乐背景
 */
/* ------------------------------------------------------------------------------------*/
static void music_bg_layout_free()
{
    printf("%s %d", __func__, __LINE__);
    load_jpg_from_sd_free();
    if (__this->buf) {
        FREE(__this->buf);
        __this->buf = NULL;
    }
    __this->init = 0;
}

static int music_bg_layout_ontouch(void *ctr, struct element_touch_event *e)
{
    switch (e->event) {
    case ELM_EVENT_TOUCH_DOWN:	// 按下
        break;
    case ELM_EVENT_TOUCH_MOVE:
        break;
    case ELM_EVENT_TOUCH_UP:
        break;
    case ELM_EVENT_TOUCH_HOLD:
        break;
    case ELM_EVENT_TOUCH_ENERGY:
        break;
    case ELM_EVENT_TOUCH_R_MOVE:
        return true;
    default:
        break;
    }
    return true;
}


static int music_bg_layout_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct layout *layout = (struct layout *)_ctrl;

    switch (event) {
    case ON_CHANGE_INIT:
        bip_file_enable();
        bip_deal_task_create();
        ui_auto_shut_down_disable();
        bip_file_set_callback(music_bg_update);
        music_bg_layout_init();
        break;
    case ON_CHANGE_FIRST_SHOW:
        break;
    case ON_CHANGE_SHOW:
        break;
    case ON_CHANGE_SHOW_POST:
        ui_custom_draw_clear((struct draw_context *)arg);
        if (!bip_file_is_update()) {
            music_bg_draw_image(arg);
        }
        break;
    case ON_CHANGE_RELEASE:
        ui_auto_shut_down_enable();
        bip_file_set_callback(NULL);
        if (__this->timer_id) {
            sys_timer_del(__this->timer_id);
            __this->timer_id = 0;
        }
        music_bg_layout_free();
        break;
    }
    return false;
}
REGISTER_UI_EVENT_HANDLER(MUSIC_BIP_BG)
.onchange = music_bg_layout_onchange,
 .onkey = NULL,
  .ontouch = music_bg_layout_ontouch,
};


#endif
#else
//
void bip_update_pic_file()
{

}
#endif


