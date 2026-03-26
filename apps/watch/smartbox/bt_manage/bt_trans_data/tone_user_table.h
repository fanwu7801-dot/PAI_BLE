#ifndef _TONE_USER_TABLE_H
#define _TONE_USER_TABLE_H
#include <stdbool.h>
#include <stdint.h>
#define TONE_USER_TABLE_PROTOCOL_ID 0x00F4 // 音频播放
typedef enum {
  // 第一套提示音
  Car_search_01 = 0x0001,                    // 寻车
  Power_on_01 = 0x0002,                      // 上电
  Power_off_01 = 0x0003,                     // 下电
  Lift_the_defense_01 = 0x0004,              // 单独解防
  Set_up_defenses_01 = 0x0005,               // 单独设防
  Sit_down_01 = 0x0006,                      // 坐下
  Stand_up_01 = 0x0007,                      // 站起
  Side_support_open_01 = 0x0008,             // 侧支撑开
  Side_support_close_01 = 0x0009,            // 侧支撑关
  tone_10_01 = 0x0010,                       // 未知提示音
  tone_11_01 = 0x0011,                       // 未知提示音
  Gearshift_Level_01 = 0x0012,               // 档位切换
  tone_13_01 = 0x0013,                       // 未知提示音
  tone_14_01 = 0x0014,                       // 未知提示音
  diversion_01 = 0x0015,                     // 转向
  tone_16_01 = 0x0016,                       // 未知提示音
  Displacement_alarm_01 = 0x0017,            // 位移报警
  burglar_alarm_01 = 0x0018,                 //  防盗报警
  Overfilling_alarm_01 = 0x0019,             // 倾倒报警
  low_battery_01 = 0x0020,                   // 低电报警
  count_down_01 = 0x0021,                    // 倒计时
  tone_22_01 = 0x0022,                       // 未知提示音
  tone_23_01 = 0x0023,                       // 未知提示音
  Input_prompt_tone_01 = 0x0024,             // 输入提示音
  General_Warning_01 = 0x0025,               // 一般警告
  fifteen_size_warning_01 = 0x0026,          // 15码警告
  tone_27_01 = 0x0027,                       // 未知提示音
  Seat_cushion_lock_is_unlocked_01 = 0x0028, // 座位 cushion 锁已解锁

  // 第二套提示音
  Car_search_02 = 0x0029,                    // 寻车
  Power_on_02 = 0x0030,                      // 上电
  Power_off_02 = 0x0031,                     // 下电
  Lift_the_defense_02 = 0x0032,              // 单独解防
  Set_up_defenses_02 = 0x0033,               // 单独设防
  Sit_down_02 = 0x0034,                      // 坐下
  Stand_up_02 = 0x0036,                      // 站起
  Side_support_open_02 = 0x0037,             // 侧支撑开
  Side_support_close_02 = 0x0038,            // 侧支撑关
  tone_10_02 = 0x0039,                       // 未知提示音
  tone_11_02 = 0x0040,                       // 未知提示音
  Gearshift_Level_02 = 0x0041,               // 档位切换
  tone_13_02 = 0x0042,                       // 未知提示音
  tone_14_02 = 0x0043,                       // 未知提示音
  diversion_02 = 0x0044,                     // 转向
  tone_16_02 = 0x0045,                       // 未知提示音
  Displacement_alarm_02 = 0x0046,            // 位移报警
  burglar_alarm_02 = 0x0047,                 //  防盗报警
  Overfilling_alarm_02 = 0x0048,             // 倾倒报警
  low_battery_02 = 0x0049,                   // 低电报警
  count_down_02 = 0x0050,                    // 倒计时
  tone_22_02 = 0x0051,                       // 未知提示音
  tone_23_02 = 0x0052,                       // 未知提示音
  Input_prompt_tone_02 = 0x0053,             // 输入提示音
  General_Warning_02 = 0x0054,               // 一般警告
  fifteen_size_warning_02 = 0x0055,          // 15码警告
  tone_27_02 = 0x0056,                       // 未知提示音
  Seat_cushion_lock_is_unlocked_02 = 0x0057, // 座位 cushion 锁已解锁
  // 第三套提示音
  Car_search_03 = 0x0058,                    // 寻车
  Power_on_03 = 0x0059,                      // 上电
  Power_off_03 = 0x0060,                     // 下电
  Lift_the_defense_03 = 0x0061,              // 单独解防
  Set_up_defenses_03 = 0x0062,               // 单独设防
  Sit_down_03 = 0x0063,                      // 坐下
  Stand_up_03 = 0x0064,                      // 站起
  Side_support_open_03 = 0x0065,             // 侧支撑开
  Side_support_close_03 = 0x0066,            // 侧支撑关
  tone_10_03 = 0x0067,                       // 未知提示音
  tone_11_03 = 0x0068,                       // 未知提示音
  Gearshift_Level_03 = 0x0069,               // 档位切换
  tone_13_03 = 0x006A,                       // 未知提示音
  tone_14_03 = 0x006B,                       // 未知提示音
  diversion_03 = 0x006C,                     // 转向
  tone_16_03 = 0x006D,                       // 未知提示音
  Displacement_alarm_03 = 0x006E,            // 位移报警
  burglar_alarm_03 = 0x006F,                 //  防盗报警
  Overfilling_alarm_03 = 0x0070,             // 倾倒报警
  low_battery_03 = 0x0071,                   // 低电报警
  count_down_03 = 0x0072,                    // 倒计时
  tone_22_03 = 0x0073,                       // 未知提示音
  tone_23_03 = 0x0074,                       // 未知提示音
  Input_prompt_tone_03 = 0x0075,             // 输入提示音
  General_Warning_03 = 0x0076,               // 一般警告
  fifteen_size_warning_03 = 0x0077,          // 15码警告
  tone_27_03 = 0x0078,                       // 未知提示音
  Seat_cushion_lock_is_unlocked_03 = 0x0079, // 座位 cushion 锁已解锁

  // 第四套提示音
  Car_search_04 = 0x007A,                    // 寻车
  Power_on_04 = 0x007B,                      // 上电
  Power_off_04 = 0x007C,                     // 下电
  Lift_the_defense_04 = 0x007D,              // 单独解防
  Set_up_defenses_04 = 0x007E,               // 单独设防
  Sit_down_04 = 0x007F,                      // 坐下
  Stand_up_04 = 0x0080,                      // 站起
  Side_support_open_04 = 0x0081,             // 侧支撑开
  Side_support_close_04 = 0x0082,            // 侧支撑关
  tone_10_04 = 0x0083,                       // 未知提示音
  tone_11_04 = 0x0084,                       // 未知提示音
  Gearshift_Level_04 = 0x0085,               // 档位切换
  tone_13_04 = 0x0086,                       // 未知提示音
  tone_14_04 = 0x0087,                       // 未知提示音
  diversion_04 = 0x0088,                     // 转向
  tone_16_04 = 0x0089,                       // 未知提示音
  Displacement_alarm_04 = 0x008A,            // 位移报警
  burglar_alarm_04 = 0x008B,                 //  防盗报警
  Overfilling_alarm_04 = 0x008C,             // 倾倒报警
  low_battery_04 = 0x008D,                   // 低电报警
  count_down_04 = 0x008E,                    // 倒计时
  tone_22_04 = 0x008F,                       // 未知提示音
  tone_23_04 = 0x0090,                       // 未知提示音
  Input_prompt_tone_04 = 0x0091,             // 输入提示音
  General_Warning_04 = 0x0092,               // 一般警告
  fifteen_size_warning_04 = 0x0093,          // 15码警告
  tone_27_04 = 0x0094,                       // 未知提示音
  Seat_cushion_lock_is_unlocked_04 = 0x0095, // 座位 cushion 锁已解锁

  // 节日音效
  Festival_60 = 0x0096, // 60 节日音效
  Festival_61 = 0x0097, // 61 节日音效
  Festival_62 = 0x0098, // 62 节日音效
  Festival_63 = 0x0099, // 63 节日音效
  Festival_64 = 0x0100, // 64 节日音效
  Festival_65 = 0x0101, // 65 节日音效
  Festival_66 = 0x0102, // 66 节日音效
  Festival_67 = 0x0103, // 67 节日音效
  Festival_68 = 0x0104, // 68 节日音效

  // 生日祝福
  Birthday_Wish_01 = 0x0105, // 1 生日祝福
  // 人声提示
  status_48v = 0x0106,         // 48v 状态
  status_60v = 0x0107,         // 60v 状态
  status_72v = 0x0108,         // 72v 状态
  pairing_01 = 0x0109,         // 1 蓝牙配对
  set_success_01 = 0x0110,     // 1 设置成功
  failed_try_01 = 0x0111,      // 1 失败尝试
  add_success_01 = 0x0112,     // 1 添加成功
  no_information_01 = 0x0113,  // 1 无通讯
  recovered_01 = 0x0114,       // 1 恢复成功
  relieve_success_01 = 0x0115, // 1 解除成功
  empty_01 = 0x0116,           // 1 已清空
  delete_success_01 = 0x0117,  // 1 删除成功
  return_success_01 = 0x0118,  // 1 退出成功
  yes_information_01 = 0x0119, // 1 有通讯
} Tone_player;
bool tone_player_cmp(int tone_id);
bool tone_user_table_has_pending_messages(void);
void Usertone_player_init(void);
void tone_user_table_process_queue(void *p);
#endif