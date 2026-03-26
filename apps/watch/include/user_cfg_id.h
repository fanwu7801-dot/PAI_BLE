#ifndef _USER_CFG_ID_H_
#define _USER_CFG_ID_H_

//=================================================================================//
//                            与APP CASE相关配置项[1 ~ 60]                         //
//=================================================================================//
#define     PT_TEST_RESULT              2
// #define 	VM_FM_EMITTER_FREQ				 1
// #define 	VM_FM_EMITTER_DIG_VOL		 	 2
// #define 	VM_MUSIC_LAST_DEV	    		 3
// #define 	VM_WAKEUP_PND					 4
#define     VM_USB_MIC_GAIN             	 4
#define     VM_ALARM_0                  	 5
#define     VM_ALARM_1                  	 6
#define     VM_ALARM_2                  	 7
#define     VM_ALARM_3                  	 8
#define     VM_ALARM_4                  	 9
#define     VM_ALARM_SNOOZE                	 10
#define     VM_ALARM_MASK               	 11
#define     VM_ALARM_NAME_0             	 12
#define     VM_ALARM_NAME_1             	 13
#define     VM_ALARM_NAME_2             	 14
#define     VM_ALARM_NAME_3             	 15
#define     VM_ALARM_NAME_4             	 16
#define     VM_FM_INFO				    	 17
// #define		VM_RTC_TRIM				    	 18

#define		CFG_RCSP_ADV_HIGH_LOW_VOL		 19
#define     CFG_RCSP_ADV_EQ_MODE_SETTING     20
#define     CFG_RCSP_ADV_EQ_DATA_SETTING     21
#define     ADV_SEQ_RAND                     22
#define     CFG_RCSP_ADV_TIME_STAMP          23
#define     CFG_RCSP_ADV_WORK_SETTING        24
#define     CFG_RCSP_ADV_MIC_SETTING         25
#define     CFG_RCSP_ADV_LED_SETTING         26
#define     CFG_RCSP_ADV_KEY_SETTING         27
#define     CFG_RCSP_MISC_DRC_SETTING        28
#define     CFG_RCSP_MISC_REVERB_ON_OFF      29
// #define     VM_ALARM_RING_NAME_0             30
// #define     VM_ALARM_RING_NAME_1             31
// #define     VM_ALARM_RING_NAME_2             32
// #define     VM_ALARM_RING_NAME_3             33
// #define     VM_ALARM_RING_NAME_4             34
// #define     VM_COLOR_LED_SETTING			 35
// #define     VM_EXTRA_FLASH_UPDATE_FLAG		 36
// #define     VM_EXTRA_FLASH_ALL_UPDATE_FLAG	 37

// // #define 	CFG_FLASH_BREAKPOINT			 29
// // #define 	CFG_USB_BREAKPOINT				 30
// // #define 	CFG_SD0_BREAKPOINT				 31
// // #define 	CFG_SD1_BREAKPOINT				 32
// // #define 	CFG_MUSIC_DEVICE				 33
// // #define 	CFG_FM_RECEIVER_INFO			 34
// // #define 	CFG_FM_TRANSMIT_INFO			 35
// // #define 	CFG_AAP_MODE_INFO				 36
// #define    VM_TWS_ROLE                       38
// #define    VM_WATCH_SELECT                   39


// #define    VM_WATCH_DEFAULT_NFC_ID           40
#define    VM_RESET_EX_FLASH_FLAG            51

// #define    VM_SPORT_INFO_SWITCH_FLAG         42
// #define    VM_SPORT_INFO_MODE_FLAG           43
#define    VM_SPORT_INFO_EXERCISE_HEART_RATE 44
// #define    VM_SPORT_INFO_PERSONAL_INFO_FLAG  45
// #define    VM_SPORT_INFO_SEDENTARY           46
// #define    VM_SPORT_INFO_SLEEP_DETECTION	 47
// #define    VM_SPORT_INFO_FALL_DETECTION      48
// #define    VM_SPORT_INFO_RAISE_WRIST         49
#define 	VM_SPORT_INFO_DAILY_DATA		 50
#define tone_effect 27                   // 空间音频提示音效果
#define CFG_VEHICLE_PASSWORD_UNLOCK 30   // 保存车辆密码解锁配置参数id
#define CFG_BLE_KEY_PERIPHERAL_SEND_1 31 // 保存蓝牙外设钥匙配置1参数id
#define CFG_BLE_KEY_PERIPHERAL_SEND_2 32 // 保存蓝牙外设钥匙配置2参数id
#define CFG_BLE_KEY_PERIPHERAL_SEND_3 33 // 保存蓝牙外设钥匙配置3参数id
#define CFG_BLE_KEY_PERIPHERAL_SEND_4 34 // 保存蓝牙外设钥匙配置4参数id
#define CFG_BLE_KEY_PERIPHERAL_USABLE 50 // 保存蓝牙外设钥匙配置可用数量参数id
#define CFG_NFC_KEY_PERIPHERAL_SEND_1 36 // 保存NFC外设钥匙配置1参数id
#define CFG_NFC_KEY_PERIPHERAL_SEND_2 37 // 保存NFC外设钥匙配置2参数id
#define CFG_NFC_KEY_PERIPHERAL_SEND_3 38 // 保存NFC外设钥匙配置3参数id
#define CFG_NFC_KEY_PERIPHERAL_SEND_4 39 // 保存NFC外设钥匙配置4参数id
#define CFG_NFC_KEY_PERIPHERAL_USABLE_NUM                                      \
  40                                     // 保存NFC外设钥匙配置可用数量参数id
#define RSSI_STATUS_ID 41                // 保存RSSI状态参数id
#define RSSI_UNLOCK_DISTANCE_ID 42       // 保存RSSI解锁距离参数id
#define CFG_PHONE_BLE_KEY_USABLE_NUM 45  // 保存手机蓝牙钥匙配置可用数量参数id
#define CFG_PHONE_BLE_KEY_DELETE_ID_1 46 // 保存删除手机蓝牙钥匙配置参数id
#define CFG_PHONE_BLE_KEY_DELETE_ID_2 47 // 保存删除手机蓝牙钥匙配置参数id
#define CFG_PHONE_BLE_KEY_DELETE_ID_3 48 // 保存删除手机蓝牙钥匙配置参数id

// 自定义音效 meta（用于 0x0075 查询/开机恢复）
// 存储结构由 fill_protocol.c 内的 custom_tone_meta_t 定义
// 4 个槽位分别存一份 meta：0=开机，1=关机，2=报警，3=hello
#define CFG_CUSTOM_TONE_META_0 54
#define CFG_CUSTOM_TONE_META_1 55
#define CFG_CUSTOM_TONE_META_2 56
#define CFG_CUSTOM_TONE_META_3 57
#define CFG_CUSTOM_TONE_META   CFG_CUSTOM_TONE_META_0

// 用户音效选择（4个槽位，每个1字节：0=默认音效，1=个性音效）
#define CFG_TONE_USER_SELECT_0 58  // 开机音效选择
#define CFG_TONE_USER_SELECT_1 59  // 关机音效选择
#define CFG_TONE_USER_SELECT_2 60  // 报警音效选择

// 设备SN/AES Key（由MCU通过UART下发，SOC保存后用于广播/加解密）
#define CFG_DEVICE_SN 52      // 8字节SN（UINT8[8]）
#define CFG_DEVICE_AES_KEY 53 // 16字节AES KEY（UINT8[16]）

// #define     VM_VIR_RTC_TIME                  51
// #define     VM_VIR_ALM_TIME                  52
// #define     VM_VIR_SUM_NSEC                  53
// #define     CFG_DMS_MALFUNC_STATE_ID         54//dms故障麦克风检测默认使用哪个mic的参数id

//=================================================================================//
//                            ！！！最大到 60 ！！！                         	   //
//=================================================================================//


#if (VM_ITEM_MAX_NUM > 128)
//=================================================================================//
//                            扩展项[128 ~ 253]                         		   //
//=================================================================================//
#define		CFG_FLASH_BREAKPOINT0		128
#define		CFG_FLASH_BREAKPOINT1		129
#define		CFG_FLASH_BREAKPOINT2		130
#define		CFG_FLASH_BREAKPOINT3		131
#define		CFG_FLASH_BREAKPOINT4		132
#define		CFG_FLASH_BREAKPOINT5		133
#define		CFG_FLASH_BREAKPOINT6		134
#define		CFG_FLASH_BREAKPOINT7		135
#define		CFG_FLASH_BREAKPOINT8		136
#define		CFG_FLASH_BREAKPOINT9		137
#define 	CFG_USB_BREAKPOINT0			138
#define 	CFG_USB_BREAKPOINT1			139
#define 	CFG_USB_BREAKPOINT2			140
#define 	CFG_USB_BREAKPOINT3			141
#define 	CFG_USB_BREAKPOINT4			142
#define 	CFG_USB_BREAKPOINT5			143
#define 	CFG_USB_BREAKPOINT6			144
#define 	CFG_USB_BREAKPOINT7			145
#define 	CFG_USB_BREAKPOINT8			146
#define 	CFG_USB_BREAKPOINT9			147
#define 	CFG_SD0_BREAKPOINT0		    148
#define 	CFG_SD0_BREAKPOINT1		    149
#define 	CFG_SD0_BREAKPOINT2		    150
#define 	CFG_SD0_BREAKPOINT3		    151
#define 	CFG_SD0_BREAKPOINT4		    152
#define 	CFG_SD0_BREAKPOINT5		    153
#define 	CFG_SD0_BREAKPOINT6		    154
#define 	CFG_SD0_BREAKPOINT7		    155
#define 	CFG_SD0_BREAKPOINT8		    156
#define 	CFG_SD0_BREAKPOINT9		    157
#define 	CFG_SD1_BREAKPOINT0		    158
#define 	CFG_SD1_BREAKPOINT1		    159
#define 	CFG_SD1_BREAKPOINT2		    160
#define 	CFG_SD1_BREAKPOINT3		    161
#define 	CFG_SD1_BREAKPOINT4		    162
#define 	CFG_SD1_BREAKPOINT5		    163
#define 	CFG_SD1_BREAKPOINT6		    164
#define 	CFG_SD1_BREAKPOINT7		    165
#define 	CFG_SD1_BREAKPOINT8		    166
#define 	CFG_SD1_BREAKPOINT9		    167
#define     CFG_CHGBOX_ADDR             168
#define 	CFG_REMOTE_DN_00		    169
#define 	CFG_REMOTE_DN_01		    170
#define 	CFG_REMOTE_DN_02		    171
#define 	CFG_REMOTE_DN_03		    172
#define 	CFG_REMOTE_DN_04		    173
#define 	CFG_REMOTE_DN_05		    174
#define 	CFG_REMOTE_DN_06		    175
#define 	CFG_REMOTE_DN_07		    176
#define 	CFG_REMOTE_DN_08		    177
#define 	CFG_REMOTE_DN_09		    178
#define 	CFG_REMOTE_DN_10		    179
#define 	CFG_REMOTE_DN_11		    180
#define 	CFG_REMOTE_DN_12		    181
#define 	CFG_REMOTE_DN_13		    182
#define 	CFG_REMOTE_DN_14		    183
#define 	CFG_REMOTE_DN_15		    184
#define 	CFG_REMOTE_DN_16		    185
#define 	CFG_REMOTE_DN_17		    186
#define 	CFG_REMOTE_DN_18		    187
#define 	CFG_REMOTE_DN_19		    188
#define 	CFG_BT_PAGE_LIST		    189
#define		CFG_MUSIC_MODE				190

#define     VM_WATCH_EX_BEGIN            191//拓展表盘信息
#define     VM_WATCH_EX0                      VM_WATCH_EX_BEGIN//拓展表盘信息

#define     VM_WATCH_EX1                 192
#define     VM_WATCH_EX2                 193
#define     VM_WATCH_EX3                 194
#define     VM_WATCH_EX4                 195
#define     VM_WATCH_EX5                 196
#define     VM_WATCH_EX_END              VM_WATCH_EX5//拓展表盘信息

#define     VM_UI_SYS_INFO				 197
#define     VM_UI_FILE_LIST				 198
#endif//(VM_ITEM_MAX_NUM > 128)

// findmy
#define     CFG_FMNA_BLE_ADDRESS_INFO        198
#define     CFG_FMNA_SOFTWARE_AUTH_START     199
#define     CFG_FMNA_SOFTWARE_AUTH_END       (CFG_FMNA_SOFTWARE_AUTH_START + 4)
#define     CFG_FMNA_SOFTWARE_AUTH_FLAG      204
#define     CFG_FMY_INFO                     205


#endif /* #ifndef _USER_CFG_ID_H_ */
