/* Copyright(C)
 * not free
 * All right reserved
 *
 * @file load_jpeg_from_sd.c
 * @brief 从SD卡加载jpeg图片显示（注意：jpeg的宽高必须16对齐）
 *
 * 注意：这部分代码从imb_demo.c的JPEG_SD_TEST部分代码copy而来，
 * 用于杰理UI框架搭配从SD卡读取jpeg图片显示功能，具体逻辑可参考
 * imb_demo.c文件的JPEG_SD_TEST宏包住部分。
 *
 * @author
 * @version V1.0.0
 * @date 2023-08-01
 */


/* ------------------------------------------------------------------------------------*/
/**
 * @brief load_jpg_from_sd_init 初始化从sd卡加载jpeg图片模块(如果调用free，再次解码时需再次调用init)
 *
 * @return 0 正常，-1 申请缓存空间失败(可能是内存不足)
 */
/* ------------------------------------------------------------------------------------*/
int load_jpg_from_sd_init();

/* ------------------------------------------------------------------------------------*/
/**
 * @brief load_jpg_from_sd_free 释放从sd卡加载jpeg图片模块
 */
/* ------------------------------------------------------------------------------------*/
void load_jpg_from_sd_free(void);

/* ------------------------------------------------------------------------------------*/
/**
 * @brief load_jpg_from_sd_start 启动从sd卡加载指定图片，并解码成指定格式到缓存buf
 *
 * @Params path 图片路径(jpeg在SD卡的文件路径)
 * @Params out_format jpeg解码器的输出格式(OUTPUT_FORMAT_RGB888/OUTPUT_FORMAT_RGB565)
 * @Params addr 缓存地址(内部ram或psram的buf地址)
 *
 * @return 0 正常，-1 异常
 */
/* ------------------------------------------------------------------------------------*/
int load_jpg_from_sd_start(char *path, int out_format, u8 *addr);

u8 *load_jpg_from_sd_start_with_buf_malloc(char *path, int out_format, u8 * (*buffer_malloc)(struct rect *r));


int load_jpg_from_sd_start_with_rect(char *path, int out_format, u8 *dst_buf, struct rect *disp_r, struct rect *elm_rect);


/* ------------------------------------------------------------------------------------*/
/**
 * @brief load_jpg_from_sd_reset
 *
 * @return
 */
/* ------------------------------------------------------------------------------------*/
int load_jpg_from_sd_reset();
