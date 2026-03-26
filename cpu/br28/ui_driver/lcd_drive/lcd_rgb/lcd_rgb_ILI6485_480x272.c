/*
** 包含board的头文件，确定baord里面开关的屏驱宏
*/
#include "app_config.h"


/*
** 驱动代码的宏开关
** 注意：rgb屏需要一直推才能正常显示
*/
#if TCFG_LCD_RGB_ILI6485_ENABLE

#define LCD_DRIVE_CONFIG                    RGB_SPI_3WIRE_RGB888

/*
** 包含imd头文件，屏驱相关的变量和结构体都定义在imd.h
*/
#include "asm/imd.h"
#include "asm/imb.h"
#include "includes.h"
#include "ui/ui_api.h"

#define SCR_X 0
#define SCR_Y 0
#define SCR_W 480
#define SCR_H 272
#define LCD_W 480
#define LCD_H 272
#define LCD_BLOCK_W 480
#define LCD_BLOCK_H 40
#define BUF_NUM 2

#define FORMAT_RGB888 0
#define FORMAT_RGB565 1
#define IN_FORMAT     FORMAT_RGB565

/*
** lcd背光控制
** 考虑到手表应用lcd背光控制需要更灵活自由，可能需要pwm调光，随时亮灭等
** 因此内部不操作lcd背光，全部由外部自行控制
*/
static int lcd_rgb_ILI6485_backlight_ctrl(u8 onoff)
{
    lcd_bl_ctrl(onoff);
    return 0;
}


/*
** 设置lcd进入睡眠
*/
static void lcd_rgb_ILI6485_entersleep(void)
{
    //TODO
}

/*
** 设置lcd退出睡眠
*/
static void lcd_rgb_ILI6485_exitsleep(void)
{
    //TODO
}

struct imd_param lcd_rgb_ILI6485_param = {
    .scr_x    = SCR_X,
    .scr_y	  = SCR_Y,
    .scr_w	  = SCR_W,
    .scr_h	  = SCR_H,

    .in_width  = SCR_W,
    .in_height = SCR_H,
#if IN_FORMAT == FORMAT_RGB888
    .in_format = OUTPUT_FORMAT_RGB888,
#else
    .in_format = OUTPUT_FORMAT_RGB565,
#endif

    .lcd_width  = LCD_W,
    .lcd_height = LCD_H,

    .lcd_type = LCD_TYPE_RGB,

    .buffer_num	= BUF_NUM,
#if IN_FORMAT == FORMAT_RGB888
    .buffer_size = LCD_BLOCK_W * LCD_BLOCK_H * 3,
    .fps = 50,
#else
    .buffer_size = LCD_BLOCK_W * LCD_BLOCK_H * 2,
    .fps = 60,
#endif
    //rgb屏需要初始化，带spi接口的需要配置，否则无须配置spi接口参数
    /* .spi = { */
    /* .spi_mode = SPI_IF_MODE(LCD_DRIVE_CONFIG), */
    /* .port = SPI_PORTB, */
    /* .spi_dat_mode = SPI_MODE_UNIDIR, */
    /* }, */
    .rgb = {
        .out_format = OUT_FORMAT(LCD_DRIVE_CONFIG),
        .continue_frames = 1,//连续帧模式
        .dat_l2h = 0,//0:输出格式由高位到低位 1:输出格式由低位到高位
        .dat_sf = 1,//0:rgb888取高6位输出  1:rgb888取低6位输出，需与dat_l2h配合使用
        .hv_mode = 0,//0:de_mode，该模式需要DE信号线, 1:hv_mode，该模式下不需要DE信号线
        .xor_edge = 1,//0:clk信号的下降沿更新数据 ,1:clk信号的上升沿更新数据

        .hpw_prd = 4,   //行同步脉冲宽度周期，以clk为单位，最少得配置成1
        .hbw_prd = 43,  //行后沿宽度周期，以clk为单位，最少得配置成0
        .hfw_prd = 8,   //行前沿宽度周期，以clk为单位，最少得配置成0

        .vpw_prd = 4,   //列同步脉冲宽度周期，以line为单位，最少得配置成1
        .vbw_prd = 12,  //列后沿宽度周期，以line为单位，最少得配置成0
        .vfw_prd = 8,   //列前沿宽度周期，以line为单位，最少得配置成0
    },

    .debug_mode_en = false,
    .debug_mode_color = 0xff0000,
};

REGISTER_LCD_DEVICE(ILI6485_rgb) = {
    .logo = "ILI6485_rgb",
    .row_addr_align		= 1,
    .column_addr_align	= 1,

    .lcd_cmd = NULL,
    .cmd_cnt = 0,
    .param	 = (void *) &lcd_rgb_ILI6485_param,

    .reset			= NULL,// 没有特殊的复位操作，用内部普通复位函数即可
    .backlight_ctrl = lcd_rgb_ILI6485_backlight_ctrl,
    .entersleep		= lcd_rgb_ILI6485_entersleep,
    .exitsleep		= lcd_rgb_ILI6485_exitsleep,
};


#endif
