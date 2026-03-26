#ifndef __GIF_H__
#define __GIF_H__

#include "typedef.h"
#include "asm/imb.h"
#include "res/resfile.h"

enum {
    COMPRESS_LOW,       //低压缩，省SRAM，NorFlash不需要PSRAM
    COMPRESS_MIDDLE,    //中压缩，是否需要PSRAM视GIF图片而定
    COMPRESS_HIGH,      //高压缩，省Flash, 需要PSRAM
};



struct gif_file_info {
    u16 width;
    u16 height;
    u16 frame_num;
    u16 delay;
    u16 version;
};

struct gif_frame_info {
    u32 addr;
    u32 size;
    int type;
    int compress;
    u16 delay;
    u8 *buf;
    u8 *lut;
};

struct gif_file {
    void *fp;                           //资源文件句柄
    int offset;                         //数据在资源文件里的偏移
    int len;                            //数据长度
    void *file_info;                    //norflash地址映射表信息
    int extern_file_info;               //外部file_info
    int (*read)(void *, int, u8 *, int);//自定义读接口
};

struct gif_info {
    u16 gif_timer;
    u16 gif_frame_num;
    u16 gif_frame_index;
    u16 gif_frame_last_index;
    u16 gif_frame_main_index;
    u16 gif_delay;
    u8 *gif_palette1;
    u8 *gif_dec_buf;
    u8 *gif_palette2;
    struct gif_file gif_file;
    u32 gif_addr;
    u32 gif_size;
    struct imb_task *task;
    u8 gif_type;
    u8 gif_compress;
    u16 gif_version;
    u32 task_id;
    u8 *gif_buf;
    u8 task_prior;
};

int gif_read_head_info(struct gif_file *file, struct gif_file_info *file_info, struct gif_frame_info *frame_info);
int gif_decode_init(struct gif_file *file, struct gif_file_info *file_info, struct gif_frame_info *frame_info, struct imb_task *p, void (*gif_timer)(void *));
int gif_decode(struct imb_task *task);
int gif_decode_uninit(struct imb_task *task, const char *task_name, int (*gif_msg_type)());
struct gif_info *gif_info_get(struct imb_task *task);

#endif



