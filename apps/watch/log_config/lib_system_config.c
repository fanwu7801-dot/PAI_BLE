/*********************************************************************************************
    *   Filename        : lib_system_config.c

    *   Description     : Optimized Code & RAM (编译优化配置)

    *   Author          : Bingquan

    *   Email           : caibingquan@zh-jieli.com

    *   Last modifiled  : 2019-03-18 15:22

    *   Copyright:(c)JIELI  2011-2019  @ , All Rights Reserved.
*********************************************************************************************/

#include "app_config.h"
#include "system/includes.h"


///打印是否时间打印信息
const int config_printf_time         = 1;

///异常中断，asser打印开启
#ifdef CONFIG_RELEASE_ENABLE
const int config_asser         = 1;
#else
const int config_asser         = 1;
#endif

const int config_system_info   = 0;

//================================================//
//                 异常信息记录使能               //
//================================================//
//注意: 当config_asser变量为0时才有效.
#if	(defined CONFIG_DEBUG_RECORD_ENABLE && CONFIG_DEBUG_RECORD_ENABLE)
const int config_debug_exception_record = (!config_asser) && 1; 	//异常记录功能总开关
const int config_debug_exception_record_dump_info = (!config_asser) && 1; 		//小机上电输出异常信息使能
const int config_debug_exception_record_p11 = (!config_asser) && 1; 			//P11异常信息使能
const int config_debug_exception_record_stack = (!config_asser) && 1; 			//堆栈异常信息使能
const int config_debug_exception_record_ret_instrcion = (!config_asser) && 1; 	//指令数据异常信息使能
#else /* #if	(define CONFIG_DEBUG_RECORD_ENABLE && CONFIG_DEBUG_RECORD_ENABLE) */
const int config_debug_exception_record = 0; 				//异常记录功能总开关
const int config_debug_exception_record_dump_info = 0; 		//小机上电输出异常信息使能
const int config_debug_exception_record_p11 = 0; 			//P11异常信息使能
const int config_debug_exception_record_stack = 0; 			//堆栈异常信息使能
const int config_debug_exception_record_ret_instrcion = 0; 	//指令数据异常信息使能
#endif /* #if (define CONFIG_DEBUG_RECORD_ENABLE && CONFIG_DEBUG_RECORD_ENABLE) */

//================================================//
//                  SDFILE 精简使能               //
//================================================//
#ifdef TCFG_SDFILE_VFS_REDUCE_ENABLE
const int SDFILE_VFS_REDUCE_ENABLE = TCFG_SDFILE_VFS_REDUCE_ENABLE;
#else
const int SDFILE_VFS_REDUCE_ENABLE = 0;
#endif

//================================================//
//                  FLASH FAT管理使能             //
//================================================//
#ifdef  TCFG_VIRFAT_FLASH_SIMPLE
const int VIRFAT_FLASH_ENABLE = 1;
#else
const int VIRFAT_FLASH_ENABLE = 0;
#endif

const int FATFS_TIMESORT_TURN_ENABLE = 1; //按时排序翻转，由默认从小到大变成从大到小
const int FATFS_TIMESORT_NUM = 3010; //按时间排序,记录文件数量, 每个占用14 byte

const int FILE_TIME_HIDDEN_ENABLE = 0; //创建文件是否隐藏时间
const int FILE_TIME_USER_DEFINE_ENABLE = 1;//用户自定义时间，每次创建文件前设置，如果置0 需要确定芯片是否有RTC功能。


//================================================//
//                  dev使用异步读使能             //
//================================================//
#ifdef TCFG_DEVICE_BULK_READ_ASYNC_ENABLE
const int device_bulk_read_async_enable = 0;
#else
const int device_bulk_read_async_enable = 0;
#endif

//================================================//
//                  UI 							  //
//================================================//
#if (TCFG_LUA_ENABLE)
const int ENABLE_LUA_VIRTUAL_MACHINE = 1;
#else /* #if (TCFG_LUA_ENABLE) */
const int ENABLE_LUA_VIRTUAL_MACHINE = 0;
#endif /* #if (TCFG_LUA_ENABLE) */

const int INDIC_MODE_SWITCH       = 1;
const int TIBETAN_MODE_SWITCH     = 0;
const int FONT_UNIC_SWITCH        = 0;
const int JPEG_DECODE_TIMEOUT	  = 20;	//20-50ms
const int const_JPEG_DEC_EXPAND = 1;
//================================================//
//              不带PSRAM的UI缩放效果 	          //
//================================================//
const int SCALE_EFFECT_WITHOUT_PSRAM_ENABLE     = 0;

//================================================//
//              IMD是否使用JL UI  				  //
//================================================//
#if (TCFG_UI_ENABLE)
const int ENABLE_JL_UI_FRAME = 1;
#else
const int ENABLE_JL_UI_FRAME = 0;
#endif

//================================================//
//              是否使用PSRAM UI  				  //
//================================================//
#if (TCFG_PSRAM_DEV_ENABLE)
const int ENABLE_PSRAM_UI_FRAME = 1;
#else
const int ENABLE_PSRAM_UI_FRAME = 0;
#endif


#if (defined CONFIG_CPU_BR27)
const int ENABLE_BR27_UI = 1;
#else
const int ENABLE_BR27_UI = 0;
#endif


//================================================//
//          不可屏蔽中断使能配置(UNMASK_IRQ)      //
//================================================//
#if TCFG_LCD_QSPI_ST77903_ENABLE || TCFG_LCD_QSPI_ST77903_V1_ENABLE || TCFG_LCD_QSPI_ST77903_V2_ENABLE || TCFG_LCD_QSPI_ST77903_V3_ENABLE || TCFG_LCD_QSPI_ST77903_V4_ENABLE
const int CONFIG_CPU_UNMASK_IRQ_ENABLE = 1;
#else
const int CONFIG_CPU_UNMASK_IRQ_ENABLE = 0;
#endif

//================================================//
//phy_malloc碎片整理使能:            			  //
//配置为0: phy_malloc申请不到不整理碎片           //
//配置为1: phy_malloc申请不到会整理碎片           //
//配置为2: phy_malloc申请总会启动一次碎片整理     //
//================================================//
const int PHYSIC_MALLOC_DEFRAG_ENABLE = 1;

//================================================//
//低功耗流程添加内存碎片整理使能:    			  //
//配置为0: 低功耗流程不整理碎片                   //
//配置为1: 低功耗流程会整理碎片                   //
//================================================//
const int MALLOC_MEMORY_DEFRAG_ENABLE = 1;

//================================================//
//ui资源是否存储在NORFLASH:    			  //
//配置为0: 存储在非NORFLASH                   //
//配置为1: 存储在NORFLASH                   //
//================================================//
#if (TCFG_NORFLASH_SFC_DEV_ENABLE || TCFG_VIRFAT_INSERT_FLASH_ENABLE)
const int UI_DATA_STORE_IN_NORFLASH = 1;
const int UI_VM_STORE_IN_NORFLASH = 1;
#else
const int UI_DATA_STORE_IN_NORFLASH = 0;
const int UI_VM_STORE_IN_NORFLASH = 0;
#endif
//================================================//
//ui资源是否通过packres打包    			  		  //
//================================================//
#if TCFG_UI_RES_USE_FAT_FTL_ENABLE
const int UI_NANDFLASH_RES_BY_PACKRES = 1;
#else
const int UI_NANDFLASH_RES_BY_PACKRES = 0;
#endif
//================================================//
//配置cpu的核心数量:    		             	  //
//可以配置为1~2                                   //
//================================================//
#if (CPU_CORE_NUM == 2)
const int CONFIG_CPU_CORE_NUM = 2;
#else
const int CONFIG_CPU_CORE_NUM = 1;
#endif


/**
 * @brief Log (Verbose/Info/Debug/Warn/Error)
 */
/*-----------------------------------------------------------*/
const char log_tag_const_v_SYS_TMR AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_i_SYS_TMR AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_d_SYS_TMR AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_w_SYS_TMR AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(TRUE);
const char log_tag_const_e_SYS_TMR AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(TRUE);

const char log_tag_const_v_JLFS AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_i_JLFS AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_d_JLFS AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_w_JLFS AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(TRUE);
const char log_tag_const_e_JLFS AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(TRUE);

//FreeRTOS
const char log_tag_const_v_PORT AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_i_PORT AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_d_PORT AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_w_PORT AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(TRUE);
const char log_tag_const_e_PORT AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(TRUE);

const char log_tag_const_v_KTASK AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_i_KTASK AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_d_KTASK AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_w_KTASK AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(TRUE);
const char log_tag_const_e_KTASK AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(TRUE);

const char log_tag_const_v_uECC AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_i_uECC AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_d_uECC AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_w_uECC AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(TRUE);
const char log_tag_const_e_uECC AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(TRUE);

const char log_tag_const_v_HEAP_MEM AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_i_HEAP_MEM AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_d_HEAP_MEM AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(TRUE);
const char log_tag_const_w_HEAP_MEM AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(TRUE);
const char log_tag_const_e_HEAP_MEM AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(TRUE);

const char log_tag_const_v_V_MEM AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_i_V_MEM AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_d_V_MEM AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_w_V_MEM AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(TRUE);
const char log_tag_const_e_V_MEM AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(TRUE);

const char log_tag_const_v_P_MEM AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_i_P_MEM AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_d_P_MEM AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_w_P_MEM AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(TRUE);
const char log_tag_const_e_P_MEM AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(TRUE);

const char log_tag_const_v_P_MEM_C AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_i_P_MEM_C AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_d_P_MEM_C AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_w_P_MEM_C AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(TRUE);
const char log_tag_const_e_P_MEM_C AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(TRUE);

const char log_tag_const_v_PSRAM_HEAP AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_i_PSRAM_HEAP AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(TRUE);
const char log_tag_const_d_PSRAM_HEAP AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_w_PSRAM_HEAP AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(TRUE);
const char log_tag_const_e_PSRAM_HEAP AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(TRUE);

const char log_tag_const_v_DEBUG_RECORD AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_i_DEBUG_RECORD AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(TRUE);
const char log_tag_const_d_DEBUG_RECORD AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_w_DEBUG_RECORD AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(TRUE);
const char log_tag_const_e_DEBUG_RECORD AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(TRUE);


const char log_tag_const_v_UI_RES AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_i_UI_RES AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(TRUE);
const char log_tag_const_d_UI_RES AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_w_UI_RES AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(TRUE);
const char log_tag_const_e_UI_RES AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(TRUE);

const char log_tag_const_v_UI_VM AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_i_UI_VM AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_d_UI_VM AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_w_UI_VM AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(TRUE);
const char log_tag_const_e_UI_VM AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(TRUE);

const char log_tag_const_v_JPEG AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_i_JPEG AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_d_JPEG AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(FALSE);
const char log_tag_const_w_JPEG AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(TRUE);
const char log_tag_const_e_JPEG AT(.LOG_TAG_CONST) = CONFIG_DEBUG_LIB(TRUE);

