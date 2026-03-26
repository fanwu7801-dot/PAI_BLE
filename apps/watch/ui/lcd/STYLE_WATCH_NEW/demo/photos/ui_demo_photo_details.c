/* Copyright(C)
 * not free
 * All right reserved
 *
 * @file ui_demo_photo_details.c
 * @brief 电子相册详情 DEMO
 * 注意：
 * 1. 本DEMO主要展示电子相册照片查看功能，使用自定义绘图方式显示图片，因此UI设计仅需页面内一个布局即可；
 * 2. 直接从SD卡加载jpg并显示速度很慢，因此最佳方案为设计一个照片缓存buf；
 * 3. 本DEMO设计优先使用psram作为缓存buf，划屏时通过切换buf来减少从sd卡加载jpg次数，减少卡顿；
 * 4. 从SD卡加载图片速度与图片大小有关；
 * 5. 缓存buf由交互设计决定，如本demo每次屏幕显示1张图片，因此需实现划屏连续，则需2张图片的缓存buf；
 * 6. 缓存buf可以使用内部ram或psram，根据实际情况决定。
 * 7. 加载jpg方法参考load_jpeg_from_sd.c文件，具体可参考imb_demo.c。
 * 8. 自定义绘图接口参考ui_action_watch.c，本demo直接使用其中的示例接口。
 * @author
 * @version V1.0.0
 * @date 2023-08-01
 */
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

#define TCFG_PHOTO_DETAILS_ENABLE		0	// demo使能

#if TCFG_PHOTO_DETAILS_ENABLE
#if TCFG_UI_ENABLE && (!TCFG_LUA_ENABLE)

#define STYLE_NAME  JL

/* 扩展功能使用的头文件 */
#include "math.h"
#include "ui_expand/ui_extend.h"
#include "ui_expand/ui_bezier.h"
#include "ui_expand/ui_scrollview.h"

/* 从SD卡加载jpeg图片 */
#include "asm/imb.h"
#include "res/jpeg_decoder.h"
#include "load_jpeg_from_sd.h"


/* 父布局 */
#define PHOTO_DETAILS_LAYOUT	UI_DEMO_PHOTO_DETAILS_LAYOUT

/* 父布局宽高 */
#define PHOTO_DETAILS_LAYOUT_W	454
#define PHOTO_DETAILS_LAYOUT_H	454

/* 照片宽高（如果是jpeg，宽高都必须16对齐） */
#define PHOTO_DETAILS_IMAGE_W	448
#define PHOTO_DETAILS_IMAGE_H	448

/* 预览图和文件目录是否同一个 */
#define DETAILS_PATH_TYPE		PHOTO_AND_PREVIEW_TOGETHER

/* 缓存的图片数量 */
#define PHOTO_DETAILS_BUF_NUM	2

/* 照片起始索引 */
#define PHOTO_INDEX_START		1

/* 如果使能PSRAM，优先使用PSRAM空间 */
#if (TCFG_PSRAM_DEV_ENABLE)
#define MALLOC	malloc_psram
#define FREE	free_psram
#else
#define MALLOC	malloc
#define FREE	free
#endif

/* 自动滚动阈值，当滑动超过这个阈值时自动滚动到下一张照片 */
#define PHOTO_DETAILS_MOVE_THR	(PHOTO_DETAILS_LAYOUT_W / 4)

/* 照片起始坐标偏移 */
#define PHOTO_DETAILS_START_X	((PHOTO_DETAILS_LAYOUT_W - PHOTO_DETAILS_IMAGE_W) / 2)
#define PHOTO_DETAILS_START_Y	((PHOTO_DETAILS_LAYOUT_H - PHOTO_DETAILS_IMAGE_H) / 2)

/* 照片之间的间隔 */
#define PHOTO_DETAILS_SPACE_X	(PHOTO_DETAILS_START_X * 2)
#define PHOTO_DETAILS_SPACE_Y	(PHOTO_DETAILS_START_Y * 2)

/* 缓存buf的空间大小 */
#define PHOTO_DETAILS_IMAGE_LEN	(PHOTO_DETAILS_IMAGE_W * PHOTO_DETAILS_IMAGE_H * 3)

/* 照片和预览图在不同个目录下 */
#define PHOTO_AND_PREVIEW_SEPARATE	1

/* 照片和预览图在同一个目录下 */
#define PHOTO_AND_PREVIEW_TOGETHER	2

/* 照片路径配置 */
#define PHOTO_DETAILS_FOLDER	"storage/sd0/C/photos/"		// 目录
#define PHOTO_DETAILS_PREFIX	PHOTO_DETAILS_FOLDER"pht"	// 前缀
#define PHOTO_DETAILS_SUFFIX	"jpg"	// 后缀(格式名)
#define PHOTO_DETAILS_LENGTH	3		// 数字转字符串的字符个数

/* 文件路径 */
#define PHOTO_FOLDER	PHOTO_DETAILS_FOLDER
#define PHOTO_PREFIX	PHOTO_DETAILS_PREFIX
#define PHOTO_SUFFIX	PHOTO_DETAILS_SUFFIX

/* 照片名中数字个数：如定义为3，则名称为 xxx001.jpg */
#define PHOTO_NAME_NUMBER_LEN	PHOTO_DETAILS_LENGTH
/* 照片名中字符个数：包括文件夹路径、前缀、后缀和“.”，注意sizeof计算字符串长度时包含结束符 */
#define PHOTO_NAME_STRING_LEN	(sizeof(PHOTO_FOLDER)+sizeof(PHOTO_PREFIX)+sizeof(PHOTO_SUFFIX)+1)
/* 照片名称缓存buf长度，包括结束符 */
#define PHOTO_NAME_CACHE_LEN	(PHOTO_NAME_STRING_LEN + PHOTO_NAME_NUMBER_LEN + 1)
/* 照片名称缓存buf */
static char photo_name_cache[PHOTO_NAME_CACHE_LEN] = {'\0'};

/* 数字转字符串，并自动在前面补0 */
static inline void number_to_string(int num, int padZeros, char *result)
{
    sprintf(result, "%d", num);
    if (padZeros > 0) {
        int resultLen = strlen(result);
        if (padZeros < resultLen) {
            printf("Error: The length of the number to string exceeds the limit!\n");
            return;
        }

        char temp[padZeros + 1];
        int padZeroCount = padZeros - resultLen;
        for (int i = 0; i < padZeroCount; i++) {
            temp[i] = '0';
        }
        strcpy(&temp[padZeroCount], result);
        strcpy(result, temp);
    }
}

/* 根据索引号，拼接预览图文件路径 */
static inline char *photo_get_path_by_index(int pic_index)
{
    const char *prefix = PHOTO_PREFIX;
    const char *suffix = "."PHOTO_SUFFIX;

    int prefixLen = strlen(prefix);
    char cache[PHOTO_NAME_NUMBER_LEN + 1] = {'\0'};
    number_to_string(pic_index, PHOTO_NAME_NUMBER_LEN, &cache);

    /* 分配足够的内存来存储新的字符串 */
    char *result = photo_name_cache;

    /* 将前缀、数字和后缀按顺序拼接到新的字符串中 */
    strcpy(result, prefix);
    sprintf(result + prefixLen, "%s", cache);
    strcat(result, suffix);

    return result;
}


static const u8 FILE_SCAN_PARAM[] = "-t"
                                    "JPG"
                                    " -sn -r"
                                    ;
/* 扫描预览图目录下文件数量 */
static int photo_get_number_by_path(char *path)
{
    int file_number = 0;
    struct vfscan *fs = fscan(path, FILE_SCAN_PARAM, 9);
    if (fs) {
        file_number = fs->file_number;
        fscan_release(fs);
    }
    return file_number;
}

/* 获取预览图中点击的是哪一张图片，在ui_demo_photo_preview.c中实现 */
extern int photo_preview_get_touch_index();

struct photo_info {
    int x;
    int y;
    int index;
    int show;
    u8 *addr;
};

/* 私有变量 */
struct photo_details {
    u16 cur_index;	// 当前照片索引
    u16 last_index;	// 上一个照片索引
    u16 pic_number;	// 照片数量
    u8 has_init;	// scrollview_t初始化标志(用于区分是否需要刷新)
    u8 move_dir;	// 移动方向

    struct photo_info icon[PHOTO_DETAILS_BUF_NUM];	// 照片解码缓存buf
    struct position pos; 	// 触摸位置记录

    ScrollView_t *view;	// scrollview_t 模型句柄
};
static struct photo_details details = {0};
#define __this	(&details)


static void photo_details_reload_jpeg()
{
    int i;
    char *path;
    u8 *addr;
    /* printf("cur index=%d, last_index=%d\n", __this->cur_index, __this->last_index); */
    for (i = 0; i < PHOTO_DETAILS_BUF_NUM; i++) {
        __this->icon[i].show = true;
    }
    if (__this->cur_index > __this->last_index) {
        addr = __this->icon[0].addr;
        __this->icon[0].addr = __this->icon[1].addr;
        __this->icon[1].addr = addr;
        path = photo_get_path_by_index(__this->cur_index + 1);
        if (load_jpg_from_sd_start(path, OUTPUT_FORMAT_RGB565, __this->icon[1].addr)) {
            __this->icon[1].show = false;
        } else {
            __this->icon[1].show = true;
        }
        __this->last_index = __this->cur_index;
    } else if (__this->cur_index < __this->last_index) {
        addr = __this->icon[1].addr;
        __this->icon[1].addr = __this->icon[0].addr;
        __this->icon[0].addr = addr;
        path = photo_get_path_by_index(__this->cur_index);
        if (load_jpg_from_sd_start(path, OUTPUT_FORMAT_RGB565, __this->icon[0].addr)) {
            __this->icon[0].show = false;
        } else {
            __this->icon[0].show = true;
        }
        __this->last_index = __this->cur_index;
    } else {
        for (i = 0; i < PHOTO_DETAILS_BUF_NUM; i++) {
            if (!__this->icon[i].addr) {
                /* 初始化申请缓存 */
                __this->icon[i].addr = MALLOC(PHOTO_DETAILS_IMAGE_LEN);
                /* __this->icon[i].index = __this->cur_index + i; */
                ASSERT(__this->icon[i].addr, "malloc buf icon[%d] faild\n", i);
            }
            if (((__this->cur_index + i) > __this->pic_number) || (__this->cur_index < PHOTO_INDEX_START)) {
                __this->icon[i].show = false;
                continue;
            }
            path = photo_get_path_by_index(__this->cur_index + i);
            /* printf("icon[%d], addr = 0x%x, index = %d, path = %s\n", i, __this->icon[i].addr, __this->cur_index + i, path); */
            if (load_jpg_from_sd_start(path, OUTPUT_FORMAT_RGB565, __this->icon[i].addr)) {
                printf("Error: load %s faild!!!\n", path);
                __this->icon[i].show = false;
                continue;
            }
            __this->icon[i].show = true;
        }
    }
}

static void photo_details_set_pos(int x, int y)
{
    int row = 0;
    int col = 0;

    for (int i = 0; i < PHOTO_DETAILS_BUF_NUM; i++) {
        __this->icon[i].x = i * (PHOTO_DETAILS_IMAGE_W + PHOTO_DETAILS_SPACE_X) + PHOTO_DETAILS_START_X + x;
        __this->icon[i].y = PHOTO_DETAILS_START_Y;
    }
}


/* 自定义绘图更新页面显示的图片，参考 ui_action_watch.c */
extern void ui_draw_custom_rgb565(void *_dc, int x, int y, int width, int height, u8 *buf, u32 len);
static void photo_details_draw_image(void *dc)
{
    ui_custom_draw_clear(dc);
    for (int i = 0; i < PHOTO_DETAILS_BUF_NUM; i++) {
        if (__this->icon[i].show) {
            ui_draw_custom_rgb565(dc, __this->icon[i].x, __this->icon[i].y, \
                                  PHOTO_DETAILS_IMAGE_W, PHOTO_DETAILS_IMAGE_H, __this->icon[i].addr, PHOTO_DETAILS_IMAGE_LEN);
        }
    }
}


static void scrollview_callback(ScrollView_t *view)
{
    int cur_x = ui_scroll_view_get_cur_x(view);
    int cur_y = ui_scroll_view_get_cur_y(view);
    /* printf("cur_x = %d, cur_y = %d, status = %d\n", cur_x, cur_y, view->status); */

    int max_index = __this->pic_number - PHOTO_DETAILS_BUF_NUM + PHOTO_INDEX_START;
    if (cur_x < 0) {
        __this->cur_index = ((-cur_x) / PHOTO_DETAILS_LAYOUT_W) + PHOTO_INDEX_START;
        if (__this->cur_index > max_index) {
            __this->cur_index = max_index;
        }
    } else {
        __this->cur_index = PHOTO_INDEX_START;
    }

    if (__this->cur_index != __this->last_index) {
        photo_details_reload_jpeg();
    }

    if (__this->cur_index == max_index) {
        photo_details_set_pos(cur_x % (PHOTO_DETAILS_LAYOUT_W * 2), cur_y);
    } else {
        photo_details_set_pos(cur_x % PHOTO_DETAILS_LAYOUT_W, cur_y);
    }

    if (__this->has_init) {
        ui_redraw(PHOTO_DETAILS_LAYOUT);
    }
}


static void scrollview_auto_align()
{
    int target_x = 0;
    int cur_x = ui_scroll_view_get_cur_x(__this->view);
    int xoffset = abs(cur_x) % PHOTO_DETAILS_LAYOUT_W;
    int duration = 5;
    /* printf("@@@ %s, cur_x = %d, offset = %d\n", __func__, cur_x, xoffset); */

    if ((cur_x < __this->view->min_x) || (cur_x > __this->view->max_x)) {
        ui_scroll_view_move_bounce(__this->view);
        return;
    }

    switch (__this->move_dir) {
    case TOUCH_MOVE_DIR_L:	// 向左划动
        if (xoffset < PHOTO_DETAILS_MOVE_THR) {
            target_x = cur_x + xoffset;
        } else {
            target_x = cur_x - (PHOTO_DETAILS_LAYOUT_W - xoffset);
        }
        ui_scroll_view_auto_move(__this->view, target_x, 0, duration);
        break;
    case TOUCH_MOVE_DIR_R:	// 向右划动
        if (xoffset < (PHOTO_DETAILS_LAYOUT_W - PHOTO_DETAILS_MOVE_THR)) {
            target_x = cur_x + xoffset;
        } else {
            target_x = cur_x - (PHOTO_DETAILS_LAYOUT_W - xoffset);
        }
        ui_scroll_view_auto_move(__this->view, target_x, 0, duration);
        break;
    default:
        break;
    }
}


static void photo_details_layout_init()
{
    __this->has_init = false;
    __this->cur_index = photo_preview_get_touch_index();
    __this->pic_number = photo_get_number_by_path(PHOTO_FOLDER) / DETAILS_PATH_TYPE;
    __this->last_index = __this->cur_index;

    if (load_jpg_from_sd_init()) {
        return;
    }

    photo_details_reload_jpeg();
    photo_details_set_pos(0, 0);

    int min_x = (__this->pic_number - 1) * PHOTO_DETAILS_LAYOUT_W;
    int cur_x = (__this->cur_index - PHOTO_INDEX_START) * PHOTO_DETAILS_LAYOUT_W;
    /* printf("@@@ min_x = %d, cur_x = %d\n", min_x, cur_x); */

    __this->view = ui_scroll_view_init(-min_x, 0, 0, 0);
    ui_scroll_view_set_callback(__this->view, scrollview_callback);
    ui_scroll_view_set_bounce(__this->view, (PHOTO_DETAILS_IMAGE_W / 2), 0);
    ui_scroll_view_move_to_pos(__this->view, -cur_x, 0);
    __this->has_init = true;
}

static void photo_details_layout_free()
{
    int i;

    if (__this->view) {
        ui_scroll_view_free(__this->view);
        __this->view = NULL;
    }

    for (i = 0; i < PHOTO_DETAILS_BUF_NUM; i++) {
        if (__this->icon[i].addr) {
            /* printf("free item[i].addr = 0x%x\n", i, __this->icon[i].addr); */
            FREE(__this->icon[i].addr);
            __this->icon[i].addr = NULL;
        }
    }
}



static int photo_detail_layout_ontouch(void *ctr, struct element_touch_event *e)
{
    switch (e->event) {
    case ELM_EVENT_TOUCH_DOWN:	// 按下
        __this->pos.x = e->pos.x;
        __this->pos.y = e->pos.y;
        __this->move_dir = TOUCH_MOVE_DIR_E;
        break;
    case ELM_EVENT_TOUCH_MOVE:
        if (__this->move_dir == TOUCH_MOVE_DIR_E) {
            __this->move_dir = get_touch_move_dir_by_point(__this->pos.x, __this->pos.y, e->pos.x, e->pos.y);
        }
        /* printf("offset x=%d, y=%d\n", (e->pos.x - __this->pos.x), (e->pos.y - __this->pos.y)); */
        if ((__this->move_dir == TOUCH_MOVE_DIR_L) || (__this->move_dir == TOUCH_MOVE_DIR_R)) {
            int xoffset = e->pos.x - __this->pos.x;
            if (xoffset > 0) {
                __this->move_dir = TOUCH_MOVE_DIR_R;
            } else {
                __this->move_dir = TOUCH_MOVE_DIR_L;
            }
            ui_scroll_view_move_offset(__this->view, xoffset, 0);
        }
        __this->pos.x = e->pos.x;
        __this->pos.y = e->pos.y;
        break;
    case ELM_EVENT_TOUCH_UP:
        scrollview_auto_align();
        break;
    case ELM_EVENT_TOUCH_R_MOVE:
        return true;
    default:
        break;
    }
    return true;
}


static int photo_detail_layout_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct layout *layout = (struct layout *)_ctrl;

    switch (event) {
    case ON_CHANGE_INIT:
        photo_details_layout_init();
        break;
    case ON_CHANGE_SHOW_POST:
        photo_details_draw_image(arg);
        break;
    case ON_CHANGE_RELEASE:
        photo_details_layout_free();
        break;
    }
    return false;
}
REGISTER_UI_EVENT_HANDLER(PHOTO_DETAILS_LAYOUT)
.onchange = photo_detail_layout_onchange,
 .onkey = NULL,
  .ontouch = photo_detail_layout_ontouch,
};


#if 0
/* 通过照片路径直接加载并显示图片DEMO */

/* 主布局 */
#define PHOTO_DETAILS_LAYOUT	UI_DEMO_PHOTO_DETAILS_LAYOUT

/* 照片显示图片控件 */
#define PHOTO_DETAILS_IMAGE0	PHOTO_IMAGE0
#define PHOTO_DETAILS_IMAGE1	PHOTO_IMAGE1

/* 编辑按钮 */
#define PHOTO_DETAILS_EDIT		PHOTO_EDIT

/* 删除按钮 */
#define PHOTO_EDTAILS_DELE		PHOTO_DELE


#define PHOTO_DETAILS_LAYOUT_W	454
#define PHOTO_DETAILS_LAYOUT_H	454

/* 照片显示图片控件的数量 */
#define PHOTO_DETAILS_NUM		2


#define DEMO_TEST_PHOTO_NUM		10	// 测试照片数量


/* 文件路径 */
#define PHOTO_FOLDER	"storage/virfat_flash/C/photos/"
#define PHOTO_PREFIX	"pht"
#define PHOTO_SUFFIX	"jpg"

/* 照片名中数字个数：如定义为3，则名称为 pht001.jpg */
#define PHOTO_NAME_NUMBER_LEN	3
/* 照片名中字符个数：包括文件夹路径、前缀、后缀和“.”，注意sizeof计算字符串长度时包含结束符 */
#define PHOTO_NAME_STRING_LEN	(sizeof(PHOTO_FOLDER)+sizeof(PHOTO_PREFIX)+sizeof(PHOTO_SUFFIX)-3+1)
/* 照片名称缓存buf长度，包括结束符 */
#define PHOTO_NAME_CACHE_LEN	(PHOTO_NAME_STRING_LEN + PHOTO_NAME_NUMBER_LEN + 1)
/* 照片名称缓存buf */
static char photo_name_cache[PHOTO_NAME_CACHE_LEN] = {'\0'};


static inline void number_to_string(int num, int padZeros, char *result)
{
    sprintf(result, "%d", num);
    if (padZeros > 0) {
        int resultLen = strlen(result);
        if (padZeros < resultLen) {
            printf("Error: The length of the number to string exceeds the limit!\n");
            return;
        }

        char temp[padZeros + 1];
        int padZeroCount = padZeros - resultLen;
        for (int i = 0; i < padZeroCount; i++) {
            temp[i] = '0';
        }
        strcpy(&temp[padZeroCount], result);
        strcpy(result, temp);
    }
}

static inline char *photo_get_path_by_index(int pic_index)
{
    int index = 0;
    char name_cache[PHOTO_NAME_NUMBER_LEN];
    memset(&photo_name_cache, '\0', PHOTO_NAME_CACHE_LEN);
    number_to_string(pic_index, PHOTO_NAME_NUMBER_LEN, &name_cache);

    strcpy(&photo_name_cache[index], PHOTO_FOLDER);
    index = strlen(photo_name_cache);
    strcpy(&photo_name_cache[index], PHOTO_PREFIX);
    index = strlen(photo_name_cache);
    strcpy(&photo_name_cache[index], name_cache);
    index = strlen(photo_name_cache);
    photo_name_cache[index] = '.';
    index = strlen(photo_name_cache);
    strcpy(&photo_name_cache[index], PHOTO_SUFFIX);
    /* printf("name_cache=%s, path = %s\n", name_cache, photo_name_cache); */
    return &photo_name_cache;
}


#define PHOTO_TEST		"storage/virfat_flash/C/photos/photos.res"


struct photo_info {
    int id;
    RESFILE *file;
    struct flash_file_info file_info;
};

struct photo_details {
    int photo_index;
    int move_dir;
    int has_init;
    struct position pos;
    struct photo_info photo[PHOTO_DETAILS_NUM];
    ScrollView_t *view;
};
static struct photo_details photo_details = {0};
#define __this (&photo_details)


static struct photo_info photo = {0};

static int photo_details_get_number()
{
    return DEMO_TEST_PHOTO_NUM;
}

static void photo_details_pic_set_pos(int x, int y)
{
    // 上下滑动，X坐标永远为0
    int x_pos = 0;//x * 10000 / PHOTO_DETAILS_LAYOUT_W;
    int y_pos = y * 10000 / PHOTO_DETAILS_LAYOUT_H;

    struct element *img0 = ui_core_get_element_by_id(PHOTO_DETAILS_IMAGE0);
    if (img0) {
        img0->css.left = x_pos;
        img0->css.top  = y_pos;

        struct element *img1 = ui_core_get_element_by_id(PHOTO_DETAILS_IMAGE0);
        if (img1) {
            img1->css.left = img0->css.left;
            img1->css.top  = img0->css.top;
        }
    }
}

static void scrollview_callback(ScrollView_t *view)
{
    int x = ui_scroll_view_get_cur_x(view);
    int y = ui_scroll_view_get_cur_y(view);

    photo_details_pic_set_pos(x, y);

    if (__this->has_init) {
        ui_redraw(PHOTO_DETAILS_LAYOUT);
    }
}

static void photo_detail_layout_init()
{
    __this->has_init = false;
    photo_details_pic_set_pos(0, 0);

    photo.file = res_fopen(PHOTO_TEST, "r");
    ui_res_flash_info_get(&photo.file_info, PHOTO_TEST, "res");

    for (int i = 0; i < PHOTO_DETAILS_NUM; i++) {
        __this->photo[i].file = res_fopen(PHOTO_TEST, "r");
        ui_res_flash_info_get(&__this->photo[i].file_info, PHOTO_TEST, "res");
    }
    __this->photo_index = 1;

    int photo_number = photo_details_get_number();
    __this->view = ui_scroll_view_init(0, -(photo_number * 454), 0, 0);
    ui_scroll_view_set_callback(__this->view, scrollview_callback);
    ui_scroll_view_set_bounce(__this->view, 0, 227);
    ui_scroll_view_move_to_pos(__this->view, 0, 0);

    __this->has_init = true;
}

static void photo_detail_layout_free()
{
    if (__this->view) {
        ui_scroll_view_free(__this->view);
        __this->view = NULL;
    }

    for (int i = 0; i < PHOTO_DETAILS_NUM; i++) {
        ui_res_flash_info_free(&__this->photo[i].file_info, "res");
    }
}

static int photo_detail_layout_ontouch(void *ctr, struct element_touch_event *e)
{
    switch (e->event) {
    case ELM_EVENT_TOUCH_DOWN:	// 按下
        __this->pos.x = e->pos.x;
        __this->pos.y = e->pos.y;
        __this->move_dir = TOUCH_MOVE_DIR_E;
        break;
    case ELM_EVENT_TOUCH_MOVE:
        if (__this->move_dir == TOUCH_MOVE_DIR_E) {
            __this->move_dir = get_touch_move_dir_by_point(__this->pos.x, __this->pos.y, e->pos.x, e->pos.y);
        }
        /* printf("offset x=%d, y=%d\n", (e->pos.x - __this->pos.x), (e->pos.y - __this->pos.y)); */
        ui_scroll_view_move_offset(__this->view, (e->pos.x - __this->pos.x), (e->pos.y - __this->pos.y));
        __this->pos.x = e->pos.x;
        __this->pos.y = e->pos.y;
        break;
    case ELM_EVENT_TOUCH_UP:
        if (__this->move_dir == TOUCH_MOVE_DIR_L || __this->move_dir == TOUCH_MOVE_DIR_R) {
            __this->photo_index ++;

            if (__this->photo_index > photo_details_get_number()) {
                __this->photo_index = 1;
            }
            char *path = photo_get_path_by_index(__this->photo_index);
            printf("@@@@@ path= %s\n", path);
        }
        ui_scroll_view_move_bounce(__this->view);
        break;
    case ELM_EVENT_TOUCH_HOLD:
        break;
    case ELM_EVENT_TOUCH_ENERGY:
        int x_energy = e->pos.x >> 16;
        int y_energy = e->pos.y >> 16;
        ui_scroll_view_inertia_start(__this->view, x_energy, y_energy);
        break;
    case ELM_EVENT_TOUCH_R_MOVE:
        return true;
    default:
        break;
    }
    return true;
}


static int photo_detail_layout_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct layout *layout = (struct layout *)_ctrl;

    switch (event) {
    case ON_CHANGE_INIT:
        photo_detail_layout_init();
        break;
    case ON_CHANGE_FIRST_SHOW:
        break;
    case ON_CHANGE_SHOW:
        break;
    case ON_CHANGE_RELEASE:
        photo_detail_layout_free();
        break;
    }
    return false;
}
REGISTER_UI_EVENT_HANDLER(PHOTO_DETAILS_LAYOUT)
.onchange = photo_detail_layout_onchange,
 .onkey = NULL,
  .ontouch = photo_detail_layout_ontouch,
};



static int photo_details_image_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct ui_pic *pic = (struct ui_pic *)_ctrl;
    struct draw_context *dc = (struct draw_context *)arg;

    switch (event) {
    case ON_CHANGE_INIT:
        /* photo.file = res_fopen(PHOTO_TEST, "r"); */
        /* ui_res_flash_info_get(&photo.file_info, PHOTO_TEST, "res"); */
        /* printf("@@@@@@@@@@@ file=0x%x, info: tab_size=%d, offset=%d\n", photo.file, photo.file_info.tab_size, photo.file_info.offset); */
        break;
    case ON_CHANGE_FIRST_SHOW:
        break;
    case ON_CHANGE_SHOW:
        if (photo.file) {
            pic->elm.css.background_image = 1;
            switch (pic->elm.id) {
            case PHOTO_DETAILS_IMAGE0:
                dc->preview.file = photo.file;
                dc->preview.file_info = &photo.file_info;
                dc->preview.id = __this->photo_index;
                dc->preview.page = 0;
                break;
            case PHOTO_DETAILS_IMAGE1:
                /* dc->preview.file = photo.file; */
                /* dc->preview.file_info = &photo.file_info; */
                /* dc->preview.id = 2; */
                /* dc->preview.page = 0; */
                break;
            default:
                break;
            }
        }
        break;
    case ON_CHANGE_SHOW_POST:
        break;
    case ON_CHANGE_RELEASE:
        break;
    }
    return false;
}
REGISTER_UI_EVENT_HANDLER(PHOTO_DETAILS_IMAGE0)
.onchange = photo_details_image_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(PHOTO_DETAILS_IMAGE1)
.onchange = photo_details_image_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};
#endif




#endif
#endif



