/* Copyright(C)
 * not free
 * All right reserved
 *
 * @file ui_demo_photo_preview.c
 * @brief 电子相册预览DEMO
 * 注意：
 * 1. 本DEMO主要展示电子相册预览功能，使用自定义绘图方式显示图片，因此UI设计仅需页面内一个布局即可；
 * 2. 直接从SD卡加载jpg并显示速度很慢，因此最佳方案为设计一个照片缓存buf；
 * 3. 本DEMO设计优先使用psram作为缓存buf，划屏时通过切换buf来减少从sd卡加载jpg次数，减少卡顿；
 * 4. 从SD卡加载图片速度与图片大小有关；
 * 5. 缓存buf由交互设计决定，如本demo每次屏幕显示4张图片，因此需实现划屏连续，则需6张图片的缓存buf；
 * 6. 缓存buf可以使用内部ram或psram，根据实际情况决定；
 * 7. 加载jpg方法参考load_jpeg_from_sd.c文件，具体可参考imb_demo.c；
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

#define TCFG_PHOTO_PREVIEW_ENABLE		0	// demo使能

#if TCFG_PHOTO_PREVIEW_ENABLE
#if TCFG_UI_ENABLE && (!TCFG_LUA_ENABLE)

#define STYLE_NAME  JL

/* 扩展功能使用的头文件 */
#include "math.h"
#include "ui_expand/ui_extend.h"
#include "ui_expand/ui_bezier.h"
#include "ui_expand/ui_scrollview.h"

/* 需要手动修改imb任务信息 */
#include "asm/imb.h"

/* 从SD卡加载jpeg图片 */
#include "res/jpeg_decoder.h"
#include "load_jpeg_from_sd.h"


/* 父布局ID */
#define PHOTO_PREVIEW_LAYOUT	UI_DEMO_PHOTO_PREVIEW

/* 照片详情页 */
#define PHOTO_DETAILS_PAGE		PAGE_105

/* 父布局宽高 */
#define PREVIEW_LAYOUT_W		454
#define PREVIEW_LAYOUT_H		454

/* jpeg缩略图宽、高 */
#define PREVIEW_IMAGE_W			224
#define PREVIEW_IMAGE_H			224

/* 预览图时起始X、Y坐标 */
#define PREVIEW_START_X			2
#define PREVIEW_START_Y			2

/* 预览图之间的X、Y间隔 */
#define PREVIEW_SPACE_X			2
#define PREVIEW_SPACE_Y			2

/* 预览图的起始索引 */
#define PREVIEW_INDEX_BEGIN		1

/* 预览图和文件目录是否同一个 */
#define PREVIEW_PATH_TYPE		PHOTO_AND_PREVIEW_TOGETHER

/* 如果开了PSRAM功能，默认使用PSRAM空间 */
#if (TCFG_PSRAM_DEV_ENABLE)
#define MALLOC	malloc_psram
#define FREE	free_psram
#else
#define MALLOC	malloc
#define FREE	free
#endif

/* 计算行、列预览图数量(这里demo使用上下滑动，行数要比屏幕内多一行) */
#define PREVIEW_COL_NUM			((PREVIEW_LAYOUT_W - PREVIEW_START_X) / (PREVIEW_IMAGE_W + PREVIEW_SPACE_X))
#define PREVIEW_ROW_NUM			((PREVIEW_LAYOUT_H - PREVIEW_START_Y) / (PREVIEW_IMAGE_H + PREVIEW_SPACE_Y) + 1)

/* 预览图总数量 */
#define PREVIEW_PIC_NUM			(PREVIEW_COL_NUM * PREVIEW_ROW_NUM)

/* Jpeg转成RGB565时的数据长度 */
#define PREVIEW_IMAGE_LEN		(PREVIEW_IMAGE_W * PREVIEW_IMAGE_H * 3)

/* 照片和预览图在不同个目录下 */
#define PHOTO_AND_PREVIEW_SEPARATE	1

/* 照片和预览图在同一个目录下 */
#define PHOTO_AND_PREVIEW_TOGETHER	2


/* 预览图路径配置 */
#define PHOTO_PREVIEW_FOLDER	"storage/sd0/C/photos/"		// 目录
#define PHOTO_PREVIEW_PREFIX	PHOTO_PREVIEW_FOLDER"pre"	// 前缀
#define PHOTO_PREVIEW_SUFFIX	"jpg"	// 后缀(格式名)
#define PHOTO_PREVIEW_LENGTH	3		// 数字转字符串的字符个数

/* 文件路径 */
#define PHOTO_FOLDER	PHOTO_PREVIEW_FOLDER
#define PHOTO_PREFIX	PHOTO_PREVIEW_PREFIX
#define PHOTO_SUFFIX	PHOTO_PREVIEW_SUFFIX

/* 照片名中数字个数：如定义为3，则名称为 xxx001.jpg */
#define PHOTO_NAME_NUMBER_LEN	PHOTO_PREVIEW_LENGTH
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


struct preview_icon {
    int x;
    int y;
    int show;
    u8 *addr;
};

/* 相册预览私有变量 */
struct photo_preview {
    /* 当前的索引，用来指向左上角第一张图片对应的缩略图索引 */
    u16 icon_index;

    /* 上一次索引，用来判断加载新的缩略图时应该往前还是往后加载 */
    u16 last_index;

    u16 pic_number;	// 照片数量
    u16 select_pic;	// 点击时被选中的照片索引

    u8 move_dir;	// 移动方向
    u8 has_init;	// 初始化标志

    struct position pos;
    struct preview_icon icon[PREVIEW_PIC_NUM];

    ScrollView_t *view;	// 滚动视图
};
static struct photo_preview preview = {0};
#define __this (&preview)


/* 获取被点击的图片索引 */
int photo_preview_get_touch_index()
{
    return __this->select_pic;
}


/* ------------------------------------------------------------------------------------*/
/**
 * @brief photo_preview_reload_jpg 更新jpeg图片
 * 注意：这里为了节省从SD卡加载jpeg的时间消耗，采用尽可能减少jpeg图片数量加载的方式
 * 只加载必须更新的内容，其他还可以继续使用的jpeg内容采用缓存buf指针切换的方式，替换
 * 到不同的位置显示，实现划屏时更新尽可能少的jpeg，降低卡顿感
 */
/* ------------------------------------------------------------------------------------*/
static void photo_preview_reload_jpg()
{
    int i;
    char *path;
    int index = 0;
    int d_index;

    for (i = 0; i < PREVIEW_PIC_NUM; i++) {
        __this->icon[i].show = true;
    }

    /* printf("%s: icon_index=%d, last_index=%d\n", __func__, __this->icon_index, __this->last_index); */
    if (__this->icon_index > __this->last_index) {
        /* 1、把前几个buf拿出来，2、把数组上后几个buf往前移，3、把前几个buf放后面去，4、更新后面几个buf的数据内容 */
        d_index = __this->icon_index - __this->last_index;
        u32 addr[d_index];
        for (i = 0; i < d_index; i++) {
            addr[i] = __this->icon[i].addr;
            /* printf("addr[%d] = 0x%x\n", i, addr[i]); */
        }
        for (i = 0; i < (PREVIEW_PIC_NUM - d_index); i++) {
            __this->icon[i].addr = __this->icon[i + d_index].addr;
            /* printf("icon[%d] = 0x%x\n", i, __this->icon[i].addr); */
        }
        for (i = (PREVIEW_PIC_NUM - d_index); i < PREVIEW_PIC_NUM; i++) {
            __this->icon[i].addr = addr[index++];
            /* printf("@@@ icon[%d] = 0x%x, index = %d\n", i, __this->icon[i].addr, (__this->icon_index + i)); */
            path = photo_get_path_by_index(__this->icon_index + i);
            /* printf("new path= %s\n", path); */
            if (load_jpg_from_sd_start(path, OUTPUT_FORMAT_RGB565, __this->icon[i].addr)) {
                printf("Error: load %s faild!!\n", path);
                __this->icon[i].show = false;	// 加载失败的路径
            } else {
                __this->icon[i].show = true;
            }
        }
        __this->last_index = __this->icon_index;
    } else if (__this->icon_index < __this->last_index) {
        /* 1、把后几个buf拿出来，2、把数组上前几个buf往后移，3、把后几个buf放前面去，4、更新前面几个buf的数据内容 */
        d_index = __this->last_index - __this->icon_index;
        u32 addr[d_index];
        for (i = (PREVIEW_PIC_NUM - d_index); i < PREVIEW_PIC_NUM; i++) {
            addr[index++] = __this->icon[i].addr;
            /* printf("addr[%d] = 0x%x\n", index, addr[index]); */
        }
        for (i = (PREVIEW_PIC_NUM - 1); i >= d_index; i--) {
            __this->icon[i].addr = __this->icon[i - d_index].addr;
            /* printf("icon[%d] = 0x%x\n", i, __this->icon[i].addr); */
        }
        for (i = 0; i < d_index; i++) {
            __this->icon[i].addr = addr[i];
            /* printf("@@@ icon[%d] = 0x%x, index = %d\n", i, __this->icon[i].addr, (__this->icon_index + i)); */
            path = photo_get_path_by_index(__this->icon_index + i);
            /* printf("new path= %s\n", path); */
            if (load_jpg_from_sd_start(path, OUTPUT_FORMAT_RGB565, __this->icon[i].addr)) {
                printf("Error: load %s faild!!\n", path);
                __this->icon[i].show = false;	// 加载失败的路径
            } else {
                __this->icon[i].show = true;
            }
        }
        __this->last_index = __this->icon_index;
    } else {
        for (i = 0; i < PREVIEW_PIC_NUM; i++) {
            if (!__this->icon[i].addr) {
                __this->icon[i].addr = MALLOC(PREVIEW_IMAGE_LEN);
                /* printf("icon[%d] = 0x%x\n", i, __this->icon[i].addr); */
                ASSERT(__this->icon[i].addr, "malloc buf icon[%d] faild\n", i);
            }
            path = photo_get_path_by_index(__this->icon_index + i);
            if (load_jpg_from_sd_start(path, OUTPUT_FORMAT_RGB565, __this->icon[i].addr)) {
                printf("Error: load %s faild!!\n", path);
                __this->icon[i].show = false;	// 加载失败的路径
            } else {
                __this->icon[i].show = true;
            }
        }
    }
}


/* 设置自定义绘图的图片起始坐标位置 */
static void photo_preview_pic_set_pos(int x, int y)
{
    int row = 0;
    int col = 0;

    for (int i = 0; i < PREVIEW_PIC_NUM; i++) {
        __this->icon[i].x = col * (PREVIEW_IMAGE_W + PREVIEW_SPACE_X) + PREVIEW_START_X + x;
        __this->icon[i].y = row * (PREVIEW_IMAGE_H + PREVIEW_SPACE_Y) + PREVIEW_START_Y + y;

        if ((++col) >= PREVIEW_COL_NUM) {
            col = 0;
            if ((++row) >= PREVIEW_ROW_NUM) {
                break;
            }
        }
    }
}


/* 自定义绘图更新页面显示的图片，参考 ui_action_watch.c */
extern void ui_draw_custom_rgb565(void *_dc, int x, int y, int width, int height, u8 *buf, u32 len);
static void photo_preview_draw_image(void *dc)
{
    for (int i = 0; i < PREVIEW_PIC_NUM; i++) {
        printf("icon[%d]: x=%d, y=%d, addr=0x%x\n", i, __this->icon[i].x, __this->icon[i].y, __this->icon[i].addr);
        if (__this->icon[i].show) {
            ui_draw_custom_rgb565(dc, __this->icon[i].x, __this->icon[i].y, \
                                  PREVIEW_IMAGE_W, PREVIEW_IMAGE_H, __this->icon[i].addr, PREVIEW_IMAGE_LEN);
        }
    }
}

/* 滚动视图的回调 */
static void scrollview_callback(ScrollView_t *view)
{
    int cur_x = ui_scroll_view_get_cur_x(view);
    int cur_y = ui_scroll_view_get_cur_y(view);
    /* printf("cur_x = %d, cur_y = %d\n", cur_x, cur_y); */
    /* 计算最大的图标索引，照片数量超过图片控件数量才需要最大值限制 */
    int max_index = 0;
    if (__this->pic_number > PREVIEW_PIC_NUM) {
        max_index = __this->pic_number - PREVIEW_PIC_NUM + PREVIEW_INDEX_BEGIN;
    } else {
        max_index = PREVIEW_INDEX_BEGIN;
    }

    /* 计算当前的缩略图索引 */
    if (cur_y < 0) {
        __this->icon_index = ((-cur_y) / PREVIEW_IMAGE_H) * PREVIEW_COL_NUM + PREVIEW_INDEX_BEGIN;
        if (__this->icon_index > max_index) {
            __this->icon_index = max_index;
        }
    } else  {
        __this->icon_index = PREVIEW_INDEX_BEGIN;
    }
    /* printf("icon_index = %d\n", __this->icon_index); */
    if (__this->icon_index != __this->last_index) {
        photo_preview_reload_jpg();
    }

    if (__this->icon_index == max_index) {
        // 考虑到回弹功能，图片索引到最后是活动范围加大
        photo_preview_pic_set_pos(cur_x, cur_y % (PREVIEW_IMAGE_H * 2));
    } else {
        photo_preview_pic_set_pos(cur_x, cur_y % PREVIEW_IMAGE_H);
    }

    if (__this->has_init) {
        ui_redraw(PHOTO_PREVIEW_LAYOUT);
    }
}


static void photo_preview_layout_init()
{
    __this->has_init = false;
    __this->icon_index = PREVIEW_INDEX_BEGIN;
    __this->last_index = __this->icon_index;

    __this->pic_number = photo_get_number_by_path(PHOTO_FOLDER) / PREVIEW_PATH_TYPE;

    if (load_jpg_from_sd_init()) {
        return;
    }

    /* extern void psram_heap_dump(void); */
    /* psram_heap_dump(); */

    photo_preview_reload_jpg();
    photo_preview_pic_set_pos(0, 0);

    int min_y = (__this->pic_number / PREVIEW_COL_NUM - PREVIEW_ROW_NUM + 1) * PREVIEW_IMAGE_H;
    __this->view = ui_scroll_view_init(0, -min_y, 0, 0);
    ui_scroll_view_set_callback(__this->view, scrollview_callback);
    ui_scroll_view_set_bounce(__this->view, 0, PREVIEW_IMAGE_H / 2);
    ui_scroll_view_move_to_pos(__this->view, 0, 0);

    __this->has_init = true;
}

static void photo_preview_layout_free()
{
    load_jpg_from_sd_free();

    for (int i = 0; i < PREVIEW_PIC_NUM; i++) {
        if (__this->icon[i].addr) {
            FREE(__this->icon[i].addr);
            __this->icon[i].addr = NULL;
        }
    }

    if (__this->view) {
        ui_scroll_view_free(__this->view);
        __this->view = NULL;
    }
}


static void photo_preview_select_pic(int x, int y)
{
    for (int i = 0; i < PREVIEW_PIC_NUM; i++) {
        if ((x > __this->icon[i].x) && (x < (__this->icon[i].x + PREVIEW_IMAGE_W)) && \
            (y > __this->icon[i].y) && (y < (__this->icon[i].y + PREVIEW_IMAGE_H))) {
            __this->select_pic = __this->icon_index + i;
            printf("select_pic = %d\n", __this->select_pic);
#if (defined PHOTO_DETAILS_PAGE && PHOTO_DETAILS_PAGE)
            UI_HIDE_CURR_WINDOW();
            UI_SHOW_WINDOW(PHOTO_DETAILS_PAGE);
#endif
            return;
        }
    }
}


static int photo_preview_layout_ontouch(void *ctr, struct element_touch_event *e)
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
        if (__this->move_dir == TOUCH_MOVE_DIR_E) {
            photo_preview_select_pic(e->pos.x, e->pos.y);
        } else {
            ui_scroll_view_move_bounce(__this->view);
        }
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


static int photo_preview_layout_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct layout *layout = (struct layout *)_ctrl;

    switch (event) {
    case ON_CHANGE_INIT:
        photo_preview_layout_init();
        break;
    case ON_CHANGE_FIRST_SHOW:
        break;
    case ON_CHANGE_SHOW:
        break;
    case ON_CHANGE_SHOW_POST:
        photo_preview_draw_image(arg);
        break;
    case ON_CHANGE_RELEASE:
        photo_preview_layout_free();
        break;
    }
    return false;
}
REGISTER_UI_EVENT_HANDLER(PHOTO_PREVIEW_LAYOUT)
.onchange = photo_preview_layout_onchange,
 .onkey = NULL,
  .ontouch = photo_preview_layout_ontouch,
};




#if 0
static u8 *buf = NULL;
static u8 *buf1 = NULL;

#define PHOTO_PREVIEW_PATH_TEST		"storage/sd0/C/photos/pre009.jpg"
#define PHOTO_PREVIEW_PATH_TEST1	"storage/sd0/C/photos/pre002.jpg"

/* 图片控件ID */
#define PHOTO_PREVIEW_IMAGE0	BASEFORM_1287

static int photo_preview_image_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct ui_pic *pic = (struct ui_pic *)_ctrl;
    struct draw_context *dc = (struct draw_context *)arg;

    switch (event) {
    case ON_CHANGE_INIT:
        buf = malloc_psram(224 * 224 * 3);	// 申请psram缓存
        memset(buf, 0, 224 * 224 * 3);
        load_jpg_from_sd_init();
        load_jpg_from_sd_start(PHOTO_PREVIEW_PATH_TEST, OUTPUT_FORMAT_RGB565, buf);

        buf1 = malloc(224 * 224 * 3);		// 申请ram缓存
        memset(buf1, 0, 224 * 224 * 3);
        load_jpg_from_sd_start(PHOTO_PREVIEW_PATH_TEST1, OUTPUT_FORMAT_RGB565, buf1);
        break;
    case ON_CHANGE_FIRST_SHOW:
        break;
    case ON_CHANGE_SHOW:
        break;
    case ON_CHANGE_SHOW_POST:
        struct imb_task_info imb_tsk_info = {0};
        struct imb_task *p = NULL;
        struct rect rect = {0};
        ui_core_get_element_abs_rect(&pic->elm, &rect);

        extern void ui_draw_custom_rgb565(void *_dc, int x, int y, int width, int height, u8 * buf, u32 len);
        ui_draw_custom_rgb565(dc, rect.left, rect.top, 224, 224, buf, 224 * 224 * 3);
        ui_draw_custom_rgb565(dc, rect.left + 224, rect.top, 224, 224, buf1, 224 * 224 * 3);

        break;
    case ON_CHANGE_RELEASE:
        load_jpg_from_sd_free();
        break;
    }
    return false;
}
REGISTER_UI_EVENT_HANDLER(PHOTO_PREVIEW_IMAGE0)
.onchange = photo_preview_image_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};
#endif


#endif
#endif


