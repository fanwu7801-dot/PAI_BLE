#ifndef _TONE_PLAYER_
#define _TONE_PLAYER_

#include "app_config.h"
#include "audio_config.h"


#define TONE_RES_ROOT_PATH		 SDFILE_RES_ROOT_PATH   	//内置flash提示音根路径


#define TONE_STOP       0
#define TONE_START      1

#define CONFIG_USE_DEFAULT_SINE     1

#define TONE_DEFAULT_VOL   SYS_MAX_VOL

enum {
    IDEX_TONE_NUM_0,
    IDEX_TONE_NUM_1,
    IDEX_TONE_NUM_2,
    IDEX_TONE_NUM_3,
    IDEX_TONE_NUM_4,
    IDEX_TONE_NUM_5,
    IDEX_TONE_NUM_6,
    IDEX_TONE_NUM_7,
    IDEX_TONE_NUM_8,
    IDEX_TONE_NUM_9,
    IDEX_TONE_BT_MODE,
    IDEX_TONE_BT_CONN,
    IDEX_TONE_BT_DISCONN,
    IDEX_TONE_TWS_CONN,
    IDEX_TONE_TWS_DISCONN,
    IDEX_TONE_LOW_POWER,
    IDEX_TONE_POWER_OFF,
    IDEX_TONE_POWER_ON,
    IDEX_TONE_RING,
    IDEX_TONE_MAX_VOL,
    IDEX_TONE_NORMAL,
    IDEX_TONE_MUSIC,
    IDEX_TONE_LINEIN,
    IDEX_TONE_FM,
    IDEX_TONE_PC,
    IDEX_TONE_RTC,
    IDEX_TONE_RECORD,
    IDEX_TONE_UDISK,
    IDEX_TONE_SD,
    // 第一套音频
    IDEX_TONE_USER_001,// 寻车
    IDEX_TONE_USER_002,// 开机
    IDEX_TONE_USER_003,// 关机
    IDEX_TONE_USER_004,// 单独解防
    IDEX_TONE_USER_005,// 单独设防
    IDEX_TONE_USER_006,// 坐垫下压
    IDEX_TONE_USER_007,// 坐垫复位
    IDEX_TONE_USER_008,// 边撑打开
    IDEX_TONE_USER_009,// 边撑收起
    IDEX_TONE_USER_010,// 启用P档
    IDEX_TONE_USER_011,// 解除P档
    IDEX_TONE_USER_012,// 3档位切换
    IDEX_TONE_USER_013,// 定速巡航开启
    IDEX_TONE_USER_014,// 定速巡航关闭
    IDEX_TONE_USER_015,// 左右转向灯
    IDEX_TONE_USER_016,// 危险警示（双闪）
    IDEX_TONE_USER_017,// 车辆移动报警
    IDEX_TONE_USER_018,// 防盗报警（震动与轮动）
    IDEX_TONE_USER_019,// 倾倒报警
    IDEX_TONE_USER_020,// 低电量报警
    IDEX_TONE_USER_021,// 倒计时提示音
    IDEX_TONE_USER_022,// 确认提示音
    IDEX_TONE_USER_023,// 取消提示音
    IDEX_TONE_USER_024,// 输入提示音
    IDEX_TONE_USER_025,// 通用警告音
    IDEX_TONE_USER_026,// 15码报警提示音
    IDEX_TONE_USER_027,// APP操作响应提示音
    IDEX_TONE_USER_028,// 坐垫锁打开提示音

    // 第二套音频
    IDEX_TONE_USER_029,// 寻车提示音
    IDEX_TONE_USER_030,// 启动车辆
    IDEX_TONE_USER_031,// 关闭车辆
    IDEX_TONE_USER_032,// 单独解防
    IDEX_TONE_USER_033,// 单独设防
    IDEX_TONE_USER_034,// 坐垫下压
    IDEX_TONE_USER_035,// 坐垫复位
    IDEX_TONE_USER_036,// 边撑打开
    IDEX_TONE_USER_037,// 边撑收起
    IDEX_TONE_USER_038,// 启用P档
    IDEX_TONE_USER_039,// 解除P档
    IDEX_TONE_USER_040,// 3档位切换
    IDEX_TONE_USER_041,// 定速巡航开启
    IDEX_TONE_USER_042,// 定速巡航关闭
    IDEX_TONE_USER_043,// 左右转向灯
    IDEX_TONE_USER_044,// 危险警示（双闪）
    IDEX_TONE_USER_045,// 车辆移动报警
    IDEX_TONE_USER_046,// 防盗报警（震动与轮动）
    IDEX_TONE_USER_047,// 倾倒报警
    IDEX_TONE_USER_048,// 低电量报警
    IDEX_TONE_USER_049,// 倒计时提示音
    IDEX_TONE_USER_050,// 确认提示音
    IDEX_TONE_USER_051,// 取消提示音
    IDEX_TONE_USER_052,// 输入提示音
    IDEX_TONE_USER_053,// 通用警告音
    IDEX_TONE_USER_054,// 15码报警提示音
    IDEX_TONE_USER_055,// APP操作响应提示音
    IDEX_TONE_USER_056,// 坐垫锁打开提示音

    // 第三套音频
    IDEX_TONE_USER_057,// 寻车提示音
    IDEX_TONE_USER_058,// 启动车辆
    IDEX_TONE_USER_059,// 关闭车辆
    IDEX_TONE_USER_060,// 单独解防
    IDEX_TONE_USER_061,// 单独设防
    IDEX_TONE_USER_062,// 坐垫下压
    IDEX_TONE_USER_063,// 坐垫复位
    IDEX_TONE_USER_064,// 边撑打开
    IDEX_TONE_USER_065,// 边撑收起
    IDEX_TONE_USER_066,// 启用P档
    IDEX_TONE_USER_067,// 解除P档
    IDEX_TONE_USER_068,// 3档位切换
    IDEX_TONE_USER_069,// 定速巡航开启
    IDEX_TONE_USER_070,// 定速巡航关闭
    IDEX_TONE_USER_071,// 左右转向灯
    IDEX_TONE_USER_072,// 危险警示（双闪）
    IDEX_TONE_USER_073,// 车辆移动报警
    IDEX_TONE_USER_074,// 防盗报警（震动与轮动）
    IDEX_TONE_USER_075,// 倾倒报警
    IDEX_TONE_USER_076,// 低电量报警
    IDEX_TONE_USER_077,// 倒计时提示音
    IDEX_TONE_USER_078,// 确认提示音
    IDEX_TONE_USER_079,// 取消提示音
    IDEX_TONE_USER_080,// 输入提示音
    IDEX_TONE_USER_081,// 通用警告音
    IDEX_TONE_USER_082,// 15码报警提示音
    IDEX_TONE_USER_083,// APP操作响应提示音
    IDEX_TONE_USER_084,// 坐垫锁打开提示音

    //复用音频
    IDEX_TONE_USER_085,// 单独解防
    IDEX_TONE_USER_086,// 单独设防
    IDEX_TONE_USER_087,//坐下
    IDEX_TONE_USER_088,
    IDEX_TONE_USER_089,
    IDEX_TONE_USER_090,
    IDEX_TONE_USER_091,
    IDEX_TONE_USER_092,
    IDEX_TONE_USER_093,
    IDEX_TONE_USER_094,
    IDEX_TONE_USER_095,
    IDEX_TONE_USER_096,
    IDEX_TONE_USER_097,
    IDEX_TONE_USER_098,
    
    //节日音效
    IDEX_TONE_USER_099,// 元旦
    IDEX_TONE_USER_100,// 春节
    IDEX_TONE_USER_101,// 劳动节
    IDEX_TONE_USER_102,// 端午节
    IDEX_TONE_USER_103,// 中秋节
    IDEX_TONE_USER_104,// 国庆节
    IDEX_TONE_USER_105,// 圣诞节
    IDEX_TONE_USER_106,// 请人节
    IDEX_TONE_USER_107,// 派DAY

    //生日音效
    IDEX_TONE_USER_108,// 生日

    // 人声模拟
    IDEX_TONE_USER_109,//48v状态
    IDEX_TONE_USER_110,//60v状态
    IDEX_TONE_USER_111,//72v状态
    IDEX_TONE_USER_112,//开始配对
    IDEX_TONE_USER_113,//设置成功
    IDEX_TONE_USER_114,//失败请重试
    IDEX_TONE_USER_115,//添加成功
    IDEX_TONE_USER_116,//无通讯状态
    IDEX_TONE_USER_117,//已恢复
    IDEX_TONE_USER_118,//已解除
    IDEX_TONE_USER_119,//已清空
    IDEX_TONE_USER_120,//已删除
    IDEX_TONE_USER_121,//已退出
    IDEX_TONE_USER_122,//有通讯状态

    // 测试音效
    IDEX_TONE_USER_123,// 测试音效
    // end of user tones 完整的音频文件定义到此结束
    IDEX_TONE_NONE = 0xFF,
};

#define TONE_NUM_0      		TONE_RES_ROOT_PATH"tone/0.*"
#define TONE_NUM_1      		TONE_RES_ROOT_PATH"tone/1.*"
#define TONE_NUM_2				TONE_RES_ROOT_PATH"tone/2.*"
#define TONE_NUM_3				TONE_RES_ROOT_PATH"tone/3.*"
#define TONE_NUM_4				TONE_RES_ROOT_PATH"tone/4.*"
#define TONE_NUM_5				TONE_RES_ROOT_PATH"tone/5.*"
#define TONE_NUM_6				TONE_RES_ROOT_PATH"tone/6.*"
#define TONE_NUM_7				TONE_RES_ROOT_PATH"tone/7.*"
#define TONE_NUM_8				TONE_RES_ROOT_PATH"tone/8.*"
#define TONE_NUM_9				TONE_RES_ROOT_PATH"tone/9.*"
#define TONE_BT_MODE			TONE_RES_ROOT_PATH"tone/bt.*"
#define TONE_BT_CONN       		TONE_RES_ROOT_PATH"tone/bt_conn.*"
#define TONE_BT_DISCONN    		TONE_RES_ROOT_PATH"tone/bt_dconn.*"
#define TONE_TWS_CONN			TONE_RES_ROOT_PATH"tone/tws_conn.*"
#define TONE_TWS_DISCONN		TONE_RES_ROOT_PATH"tone/tws_dconn.*"
#define TONE_LOW_POWER			TONE_RES_ROOT_PATH"tone/low_power.*"
#define TONE_POWER_OFF			TONE_RES_ROOT_PATH"tone/power_off.*"
#define TONE_POWER_ON			TONE_RES_ROOT_PATH"tone/power_on.*"
#define TONE_RING				TONE_RES_ROOT_PATH"tone/ring.*"
#define TONE_MAX_VOL			TONE_RES_ROOT_PATH"tone/vol_max.*"
#define TONE_MUSIC				TONE_RES_ROOT_PATH"tone/music.*"
#define TONE_LINEIN				TONE_RES_ROOT_PATH"tone/linein.*"
#define TONE_PC 				TONE_RES_ROOT_PATH"tone/pc.*"
#define TONE_FM 				TONE_RES_ROOT_PATH"tone/fm.*"
#define TONE_RTC 				TONE_RES_ROOT_PATH"tone/rtc.*"
#define TONE_RECORD 			TONE_RES_ROOT_PATH"tone/record.*"
#define TONE_SPDIF 			    TONE_RES_ROOT_PATH"tone/spdif.*"
#define TONE_TEST 				TONE_RES_ROOT_PATH"tone/tone0.*"

#ifdef CONFIG_CPU_BR18
#undef TONE_POWER_ON
#undef TONE_POWER_OFF
#undef TONE_BT_CONN
#undef TONE_BT_DISCONN

#define TONE_POWER_ON			TONE_RES_ROOT_PATH"power_on.mp3"
#define TONE_POWER_OFF			TONE_RES_ROOT_PATH"power_off.*"
#define TONE_BT_CONN       		TONE_RES_ROOT_PATH"bt_conn.mp3"
#define TONE_BT_DISCONN    		TONE_RES_ROOT_PATH"bt_dconn.mp3"
#endif


#define SINE_WTONE_NORAML           0
#define SINE_WTONE_TWS_CONNECT      1
#define SINE_WTONE_TWS_DISCONNECT   2
#define SINE_WTONE_LOW_POWER        4
#define SINE_WTONE_RING             5
#define SINE_WTONE_MAX_VOLUME       6

#define TONE_REPEAT_BEGIN(a)  (char *)((0x1 << 30) | (a & 0xffff))
#define TONE_REPEAT_END()     (char *)(0x2 << 30)

#define IS_REPEAT_BEGIN(a)    ((((u32)a >> 30) & 0x3) == 0x1 ? 1 : 0)
#define IS_REPEAT_END(a)      ((((u32)a >> 30) & 0x3) == 0x2 ? 1 : 0)
#define TONE_REPEAT_COUNT(a)  (((u32)a) & 0xffff)

#define DEFAULT_SINE_TONE(a)     (char *)(((u32)0x3 << 30) | (a))
#define IS_DEFAULT_SINE(a)       ((((u32)a >> 30) & 0x3) == 0x3 ? 1 : 0)
#define DEFAULT_SINE_ID(a)       ((u32)a & 0xffff)

#if CONFIG_USE_DEFAULT_SINE
#undef TONE_TWS_CONN
#define TONE_TWS_CONN           DEFAULT_SINE_TONE(SINE_WTONE_TWS_CONNECT)
#undef TONE_TWS_DISCONN
#define TONE_TWS_DISCONN        DEFAULT_SINE_TONE(SINE_WTONE_TWS_DISCONNECT)

#undef TONE_MAX_VOL
#define TONE_MAX_VOL            DEFAULT_SINE_TONE(SINE_WTONE_MAX_VOLUME)

#undef TONE_NORMAL
#define TONE_NORMAL            DEFAULT_SINE_TONE(SINE_WTONE_NORAML)
#if BT_PHONE_NUMBER

#else
#undef TONE_RING
#define TONE_RING               DEFAULT_SINE_TONE(SINE_WTONE_RING)
#endif

#undef TONE_LOW_POWER
#define TONE_LOW_POWER          DEFAULT_SINE_TONE(SINE_WTONE_LOW_POWER)
#endif

#define TONE_SIN_NORMAL			DEFAULT_SINE_TONE(SINE_WTONE_NORAML)

extern const char *tone_table[];
int tone_play(const char *name, u8 preemption);
int tone_play_index(u8 index, u8 preemption);
int tone_play_index_with_callback(u8 index, u8 preemption, void (*user_evt_handler)(void *priv), void *priv);
int tone_file_list_play(const char **list, u8 preemption);
int tone_play_stop(void);
int tone_play_stop_by_index(u8 index);
int tone_play_stop_by_path(char *path);
int tone_get_status();
int tone_play_by_path(const char *name, u8 preemption);

int tone_dec_wait_stop(u32 timeout_ms);

// 按序列号播放提示音
int tone_play_with_callback_by_index(u8 index, // 序列号
                                     u8 preemption, // 打断标记
                                     void (*evt_handler)(void *priv, int flag), // 事件回调接口 //flag: 0正常关闭，1被打断关闭
                                     void *evt_priv // 事件回调私有句柄
                                    );

// 按名字播放提示音
int tone_play_with_callback_by_name(char *name, // 带有路径的文件名
                                    u8 preemption, // 打断标记
                                    void (*evt_handler)(void *priv, int flag), // 事件回调接口 //flag: 0正常关闭，1被打断关闭
                                    void *evt_priv // 事件回调私有句柄
                                   );

// 按列表播放提示音
int tone_play_with_callback_by_list(const char **list, // 文件名列表
                                    u8 preemption, // 打断标记
                                    void (*evt_handler)(void *priv, int flag), // 事件回调接口 //flag: 0正常关闭，1被打断关闭
                                    void *evt_priv // 事件回调私有句柄
                                   );

#endif
