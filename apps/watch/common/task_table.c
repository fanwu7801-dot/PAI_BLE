#include "system/includes.h"
#include "app_config.h"

#if (TCFG_APP_CAT1_EN && TCFG_CAT1_AICXTEK_ENABLE)
// aicxtek task: uart-[27,28],ctrl-[25,26],ota-[20,21],tele-[18,20],sms-[14,15]
// ui任务不能挡住aicxtek的uart
#define UI_TASK_PRIO        26
#else
#define UI_TASK_PRIO        2
#endif

/*任务列表 */
const struct task_info task_info_table[] = {

#if TCFG_PAY_ALIOS_ENABLE && (TCFG_PAY_ALIOS_WAY_SEL==TCFG_PAY_ALIOS_WAY_T_HEAD)
    {"app_core",            1,      0,  2048,  1024  },
#else
    {"app_core",            1,      0,  2048,  1024  },
    // usart0com: 增加栈大小到 1536 字节，防止串口协议解析和音频播放时栈溢出
    {"usart0com",           3,      0,  1536,   128  },
#endif
    //乘车码应用使用完要kill 任务 对栈占用厉害
    {"transitcode",         1,      0,  2048 + 2048 + 2048 + 1024,  512 },

    {"sys_event",           6,      0,  256,   0    },
    {"btctrler",            4,      0,  512,   512  },
    {"btencry",             1,      0,  512,   128  },
    {"tws",                 5,      0,  512,   128  },
#if TCFG_PAY_ALIOS_ENABLE && (TCFG_PAY_ALIOS_WAY_SEL==TCFG_PAY_ALIOS_WAY_ALIYUN)
    /* btstack:
     * BLE write 回调里会走 AES/CRC/printf 等深调用链，原栈容易溢出；适当加大栈做兜底
     */
    {"btstack",             3,      0,  2048 + 512,   512  },
#else
    /* btstack:
     * BLE write 回调里会走 AES/CRC/printf 等深调用链，原栈容易溢出；适当加大栈做兜底
     */
    {"btstack",             3,      0,  2048,   512  },
#endif
    {"spo2read",			1,	    0,  256	, 	0},
    {"heartrate",			1,		0,  256,	0},
    {"gsensor",				1,	    0,  768,     0  },

#if (TCFG_USER_TWS_ENABLE && TCFG_REVERB_ENABLE)
    {"audio_dec",           3,      0,  768 + 128,   128  },
#else
    {"audio_dec",           3,      0,  768 + 32,   128  },
#endif
    {"dev_mg",           	3,      0,  512,   512  },
    {"audio_enc",           UI_TASK_PRIO + 1,      1,  512,   128  },
    {"usb_msd",           	1,      0,  512,   128  },
    {"aec",					2,	    0,  768,   128	},
    {"aec_dbg",				3,	    0,  256,   128	},
    {"update",				1,	    0,  512,   0    },
    {"dw_update",		 	2,	    0,  256,   128  },
#ifdef USER_UART_UPDATE_ENABLE
    {"uart_update",	        1,	    0,  256,   128	},
#endif
    {"systimer",		    6,	    0,  128,   0    },
    {"usb_audio",           5,      0,  256,   256  },
    {"plnk_dbg",            5,      0,  256,   256  },
    {"adc_linein",          2,      0,  768,   128  },
    {"enc_write",           1,      0,  768,   0 	},
    /* {"src_write",           1,      0,  768,   256 	}, */
    {"fm_task",             3,      0,  512,   128  },
#if (RCSP_BTMATE_EN || RCSP_ADV_EN)
    {"rcsp_task",			4,	    0,  768,   128	},
#endif
#if TCFG_SPI_LCD_ENABLE
#if TCFG_LUA_ENABLE
    {"ui",           	    2,      1,  1024 * 2,   1024  },
    {"imb",           	    2,      1,  1024,   1024  },
#else
    {"ui",           	    UI_TASK_PRIO,      1,  1024,   1024  },
    {"lcd_init",           	2,      0,  256,    0  },
    {"tp_init",           	2,      0,  256,    0  },
    {"imb",           	    UI_TASK_PRIO,      1,  1024,   1024  },
    {"jpeg_demo",           UI_TASK_PRIO,      1,  1024,   1024  },
    {"lcd",           	    5,      0,  1024,   1024  },
    {"effect",           	2,      0,  1024,   1024  },
    {"jpg_sd",           	5,      1,  1024,   0},
    {"jpg_dec",           	4,      1,  1024,   128},
#endif
#else
    {"ui",                  3,      1,  384,   128  },
#endif

#if(TCFG_CHARGE_BOX_ENABLE)
    {"chgbox_n",            6,      0,  512,   128  },
#endif
#if (SMART_BOX_EN)
    {"smartbox",            4,      0,  512,   128  },
#if (TCFG_DEV_MANAGER_ENABLE)
    {"ftran_back",		    1,	    0,  512,	  0  },
#endif
#if (JL_SMART_BOX_SENSORS_DATA_OPT)
    {"w_nfc_back",		    1,	    0,  512,	  0  },
#endif
#if (RCSP_UPDATE_EN)
    {"ex_f_update",			1,	    1,  512,   0    },
#endif
#endif//SMART_BOX_EN
#if RCSP_FILE_OPT
    {"file_bs",				1,	    0,  768,	  0  },
#endif
    {"data_export",				1,	    0,  768,	  0  },
    {"mic_stream",          5,      0,  768,   128  },
#if TCFG_SMART_VOICE_ENABLE
#if TCFG_AUDIO_ASR_DEVELOP == ASR_CFG_IFLYTEK
    {"audio_vad",           3,     0,   768,   128 },
#else
    {"audio_vad",           3,     0,   512,   128 },
#endif
#endif
#if PRODUCT_TEST_ENABLE
    {"pt",					1,	    0,  512,	  128  },
#endif
#if TCFG_AI_IFLY_LOCAL_TTS_ENABLE
    {"local_tts",			1,	    0,  1024,	  0	},
#endif
#if TCFG_CAT1_AICXTEK_ENABLE
    {"aic_audio",			2,	    0,  512,	  128  },
#endif
#if TCFG_CAT1_MODULE_UPDATE_ENABLE
#if TCFG_CAT1_UNISOC_ENABLE
    {"cat1_update",			11,	    0,  256,	  128  },
#else
    {"cat1_update",			1,	    0,  256,	  128  },
#endif
#endif
#if TCFG_UI_AVRCP_MUSIC_BG_ENABLE
    {"bip_deal",           		2,      0,  512,   512},
#endif
    {0, 0},
};

