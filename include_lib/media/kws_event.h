/*****************************************************************
>file name : kws_event.h
>author : lichao
>create time : Fri 05 Nov 2021 10:11:40 AM CST
*****************************************************************/
#ifndef _AUDIO_KWS_EVENT_
#define _AUDIO_KWS_EVENT_
#include "app_config.h"

enum audio_kws_event {
    KWS_EVENT_NULL = 0,

    /*Hey xxx系列关键词*/
    KWS_EVENT_HEY_KEYWORD,

    /*杰理唤醒词*/
    KWS_EVENT_XIAOJIE,

    /*百度 -- 小度小度等系列命令词消息*/
    KWS_EVENT_XIAODU,       /*小度小度*/

    /*音乐关键词*/
    KWS_EVENT_PLAY_MUSIC,   /*播放音乐*/
    KWS_EVENT_STOP_MUSIC,   /*停止播放*/
    KWS_EVENT_PAUSE_MUSIC,  /*暂停播放*/
    KWS_EVENT_VOLUME_UP,    /*增大音量*/
    KWS_EVENT_VOLUME_DOWN,  /*减小音量*/
    KWS_EVENT_PREV_SONG,    /*上一首*/
    KWS_EVENT_NEXT_SONG,    /*下一首*/

    /*通话关键词*/
    KWS_EVENT_CALL_ACTIVE,  /*接听电话*/
    KWS_EVENT_CALL_HANGUP,  /*挂断电话*/

    /*ANC系列关键词词*/
    KWS_EVENT_ANC_ON,       /*打开降噪*/
    KWS_EVENT_ANC_OFF,      /*关闭降噪*/
    KWS_EVENT_TRANSARENT_ON,/*打开通透*/

    /*音量补充关键词*/
    KWS_EVENT_VOLUME_MUTE,		/*静音模式*/
    KWS_EVENT_VOLUME_UNMUTE, 	/*取消静音*/
    KWS_EVENT_VOLUME_MAX, 		/*最大音量*/

    /*亮度调整关键词*/
    KWS_EVENT_BRIGHTNESS_ALWAYS,/*屏幕常亮*/
    KWS_EVENT_BRIGHTNESS_UP,	/*增大亮度*/
    KWS_EVENT_BRIGHTNESS_DOWN,	/*减小亮度*/
    KWS_EVENT_BRIGHTNESS_AUTO,	/*自动调整亮度*/

    /*健康测量关键词*/
    KWS_EVENT_DETECTION_HEART,	/*测量心率*/
    KWS_EVENT_DETECTION_OXYGEN,	/*测量血氧*/

    /*蓝牙应用关键词*/
    KWS_EVENT_OPEN_EDR,			/*打开蓝牙*/
    KWS_EVENT_FIND_PHONE,		/*找手机*/
    KWS_EVENT_START_PHOTOS,		/*开始拍照*/

    /*表盘应用关键词*/
    KWS_EVENT_SWITCH_DIAL,		/*更换表盘*/
    KWS_EVENT_SWITCH_STYLE,		/*更换界面风格*/

    /*记录查看关键词*/
    KWS_EVENT_SEE_SPORT_RECORD,	/*查看锻炼记录*/
    KWS_EVENT_SEE_ACTION_RECORD,/*查看活动记录*/
    KWS_EVENT_SEE_SLEEP_RECORD,	/*查看睡眠数据*/
    KWS_EVENT_SEE_CALL_REDORD,	/*查看通话记录*/
    KWS_EVENT_SEE_TRAIN_RECORD,	/*查看训练记录*/
    KWS_EVENT_SEE_HEAT,			/*查看压力指标*/

    /*打开功能页面关键词*/
    KWS_EVENT_OPEN_SPORT,		/*打开运动*/
    KWS_EVENT_OPEN_TRAIN,		/*打开锻炼*/
    KWS_EVENT_OPEN_CALCULAGRAPH,/*打开计时器*/
    KWS_EVENT_OPEN_CALL_DIAL,	/*打开电话*/
    KWS_EVENT_OPEN_PHONEBOOK,	/*打开联系人*/
    KWS_EVENT_OPEN_ALARM,		/*打开闹钟*/
    KWS_EVENT_OPEN_STOPWATCH,	/*打开秒表*/
    KWS_EVENT_OPEN_WEATHER,		/*打开天气*/
    KWS_EVENT_OPEN_MESS,		/*打开消息*/
    KWS_EVENT_OPEN_SET,			/*打开设置*/
    KWS_EVENT_OPEN_APP_LIST,	/*打开应用列表*/
    KWS_EVENT_OPEN_BREATH_TRAIN,/*打开呼吸训练*/
    KWS_EVENT_OPEN_BARO,		/*打开海拔气压计*/
    KWS_EVENT_OPEN_COMPASS,		/*打开指南针*/
    KWS_EVENT_OPEN_CARD_BAG,	/*打开卡包*/
    KWS_EVENT_OPEN_ALIPAY,		/*打开支付宝*/
    KWS_EVENT_OPEN_FLASHLIGHT,	/*打开手电筒*/
    KWS_EVENT_OPEN_CALENDAR,	/*打开日历*/
    KWS_EVENT_OPEN_CALCULATOR,	/*打开计算器*/

#if TCFG_AUDIO_ASR_DEVELOP == ASR_CFG_IFLYTEK
    /*科大讯飞关键词*/
    IFLYTEK_EVENT_XIOWEI,                    /*小微同学*/
    IFLYTEK_EVENT_OPEN_NOTIFICATION,         /*打开通知*/
    IFLYTEK_EVENT_OPEN_STEP,                 /*打开步数*/
    IFLYTEK_EVENT_BLOOD_PRESSURE_MEASUREMENT,/*测量血压*/
    IFLYTEK_EVENT_OPEN_GAME,                 /*打开游戏*/
    IFLYTEK_EVENT_POWER_SAVING_MODE,         /*省电模式*/
    IFLYTEK_EVENT_STOP_SPORT,                /*停止运动*/
    IFLYTEK_EVENT_KEEP_MOVING,               /*继续运动*/
    IFLYTEK_EVENT_PAUSE_SPORT,               /*暂停运动*/
    IFLYTEK_EVENT_BACK_TO_DIAL,              /*回到表盘*/
    IFLYTEK_EVENT_OPEN_RIDE_CODE,            /*打开乘车码*/
    IFLYTEK_EVENT_OPEN_COLLECTION_CODE,      /*打开收款码*/
    IFLYTEK_EVENT_OPEN_PAYMENT_CODE,         /*打开付款码*/
    IFLYTEK_EVENT_OPEN_4G_NETWORK,           /*打开4G网络*/
    IFLYTEK_EVENT_CLOSE_4G_NETWORK,          /*关闭4G网络*/
#endif

    /*TODO*/

    KWS_EVENT_MAX,
};

#endif
