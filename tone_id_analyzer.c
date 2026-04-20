// 音效 ID BCD 解码分析工具
// 编译：gcc tone_id_analyzer.c -o tone_id_analyzer.exe
// 运行：./tone_id_analyzer.exe

#include <stdio.h>
#include <stdint.h>

#define BCD_TO_DEC_4(bcd) (((((bcd) >> 12) & 0x0F) * 1000) + ((((bcd) >> 8) & 0x0F) * 100) + ((((bcd) >> 4) & 0x0F) * 10) + (((bcd) >> 0) & 0x0F))

// 边撑打开枚举值
#define Side_support_open_01  0x0008
#define Side_support_open_02  0x0037
#define Side_support_open_03  0x0065
#define Side_support_open_04  0x0081

// 所有报警相关枚举值
#define Displacement_alarm_01   0x0017
#define Displacement_alarm_02   0x0046
#define Displacement_alarm_03   0x006E
#define Displacement_alarm_04   0x008A

#define burglar_alarm_01        0x0018
#define burglar_alarm_02        0x0047
#define burglar_alarm_03        0x006F
#define burglar_alarm_04        0x008B

#define Overfilling_alarm_01    0x0019
#define Overfilling_alarm_02    0x0048
#define Overfilling_alarm_03    0x0070
#define Overfilling_alarm_04    0x008C

#define General_Warning_01      0x0025
#define General_Warning_02      0x0054
#define General_Warning_03      0x0076
#define General_Warning_04      0x0092

#define fifteen_size_warning_01 0x0026
#define fifteen_size_warning_02 0x0055
#define fifteen_size_warning_03 0x0077
#define fifteen_size_warning_04 0x0093

int main() {
    printf("========================================\n");
    printf("音效 ID BCD 解码分析\n");
    printf("========================================\n\n");

    printf("边撑打开音效 BCD 解码值：\n");
    printf("  Side_support_open_01 (0x%04X) → case_id = %d\n", 
           Side_support_open_01, BCD_TO_DEC_4(Side_support_open_01));
    printf("  Side_support_open_02 (0x%04X) → case_id = %d\n", 
           Side_support_open_02, BCD_TO_DEC_4(Side_support_open_02));
    printf("  Side_support_open_03 (0x%04X) → case_id = %d\n", 
           Side_support_open_03, BCD_TO_DEC_4(Side_support_open_03));
    printf("  Side_support_open_04 (0x%04X) → case_id = %d\n\n", 
           Side_support_open_04, BCD_TO_DEC_4(Side_support_open_04));

    printf("报警音效 BCD 解码值（会映射到自定义音效类型2）：\n");
    printf("  Displacement_alarm_01 (0x%04X) → case_id = %d\n", 
           Displacement_alarm_01, BCD_TO_DEC_4(Displacement_alarm_01));
    printf("  Displacement_alarm_02 (0x%04X) → case_id = %d (已被特殊排除)\n", 
           Displacement_alarm_02, BCD_TO_DEC_4(Displacement_alarm_02));
    printf("  Displacement_alarm_03 (0x%04X) → case_id = %d ⚠️ BCD非法（E=14）\n", 
           Displacement_alarm_03, BCD_TO_DEC_4(Displacement_alarm_03));
    printf("  Displacement_alarm_04 (0x%04X) → case_id = %d ⚠️ BCD非法（A=10）\n\n", 
           Displacement_alarm_04, BCD_TO_DEC_4(Displacement_alarm_04));

    printf("  burglar_alarm_01 (0x%04X) → case_id = %d\n", 
           burglar_alarm_01, BCD_TO_DEC_4(burglar_alarm_01));
    printf("  burglar_alarm_02 (0x%04X) → case_id = %d\n", 
           burglar_alarm_02, BCD_TO_DEC_4(burglar_alarm_02));
    printf("  burglar_alarm_03 (0x%04X) → case_id = %d ⚠️ BCD非法（F=15）\n", 
           burglar_alarm_03, BCD_TO_DEC_4(burglar_alarm_03));
    printf("  burglar_alarm_04 (0x%04X) → case_id = %d ⚠️ BCD非法（B=11）\n\n", 
           burglar_alarm_04, BCD_TO_DEC_4(burglar_alarm_04));

    printf("  Overfilling_alarm_01 (0x%04X) → case_id = %d\n", 
           Overfilling_alarm_01, BCD_TO_DEC_4(Overfilling_alarm_01));
    printf("  Overfilling_alarm_02 (0x%04X) → case_id = %d\n", 
           Overfilling_alarm_02, BCD_TO_DEC_4(Overfilling_alarm_02));
    printf("  Overfilling_alarm_03 (0x%04X) → case_id = %d\n", 
           Overfilling_alarm_03, BCD_TO_DEC_4(Overfilling_alarm_03));
    printf("  Overfilling_alarm_04 (0x%04X) → case_id = %d ⚠️ BCD非法（C=12）\n\n", 
           Overfilling_alarm_04, BCD_TO_DEC_4(Overfilling_alarm_04));

    printf("  General_Warning_01 (0x%04X) → case_id = %d\n", 
           General_Warning_01, BCD_TO_DEC_4(General_Warning_01));
    printf("  General_Warning_02 (0x%04X) → case_id = %d\n", 
           General_Warning_02, BCD_TO_DEC_4(General_Warning_02));
    printf("  General_Warning_03 (0x%04X) → case_id = %d\n", 
           General_Warning_03, BCD_TO_DEC_4(General_Warning_03));
    printf("  General_Warning_04 (0x%04X) → case_id = %d ⚠️ BCD非法（2后面是9，但十六进制连续）\n\n", 
           General_Warning_04, BCD_TO_DEC_4(General_Warning_04));

    printf("  fifteen_size_warning_01 (0x%04X) → case_id = %d\n", 
           fifteen_size_warning_01, BCD_TO_DEC_4(fifteen_size_warning_01));
    printf("  fifteen_size_warning_02 (0x%04X) → case_id = %d\n", 
           fifteen_size_warning_02, BCD_TO_DEC_4(fifteen_size_warning_02));
    printf("  fifteen_size_warning_03 (0x%04X) → case_id = %d\n", 
           fifteen_size_warning_03, BCD_TO_DEC_4(fifteen_size_warning_03));
    printf("  fifteen_size_warning_04 (0x%04X) → case_id = %d ⚠️ BCD非法（3后面是9，但十六进制连续）\n\n", 
           fifteen_size_warning_04, BCD_TO_DEC_4(fifteen_size_warning_04));

    printf("========================================\n");
    printf("冲突检查：\n");
    printf("========================================\n");
    
    int side_ids[4] = {
        BCD_TO_DEC_4(Side_support_open_01),
        BCD_TO_DEC_4(Side_support_open_02),
        BCD_TO_DEC_4(Side_support_open_03),
        BCD_TO_DEC_4(Side_support_open_04)
    };
    
    int alarm_ids[] = {
        BCD_TO_DEC_4(Displacement_alarm_01),
        BCD_TO_DEC_4(Displacement_alarm_03),
        BCD_TO_DEC_4(Displacement_alarm_04),
        BCD_TO_DEC_4(burglar_alarm_01),
        BCD_TO_DEC_4(burglar_alarm_02),
        BCD_TO_DEC_4(burglar_alarm_03),
        BCD_TO_DEC_4(burglar_alarm_04),
        BCD_TO_DEC_4(Overfilling_alarm_01),
        BCD_TO_DEC_4(Overfilling_alarm_02),
        BCD_TO_DEC_4(Overfilling_alarm_03),
        BCD_TO_DEC_4(Overfilling_alarm_04),
        BCD_TO_DEC_4(General_Warning_01),
        BCD_TO_DEC_4(General_Warning_02),
        BCD_TO_DEC_4(General_Warning_03),
        BCD_TO_DEC_4(General_Warning_04),
        BCD_TO_DEC_4(fifteen_size_warning_01),
        BCD_TO_DEC_4(fifteen_size_warning_02),
        BCD_TO_DEC_4(fifteen_size_warning_03),
        BCD_TO_DEC_4(fifteen_size_warning_04)
    };
    
    int found_conflict = 0;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < sizeof(alarm_ids)/sizeof(alarm_ids[0]); j++) {
            if (side_ids[i] == alarm_ids[j]) {
                printf("❌ 冲突！边撑打开 case_id=%d 和某个报警音效的 case_id 相同！\n", side_ids[i]);
                found_conflict = 1;
            }
        }
    }
    
    if (!found_conflict) {
        printf("✅ 未发现边撑和报警的 case_id 直接冲突\n");
    }
    
    printf("\n建议：\n");
    printf("1. 在实际运行时添加日志，打印接收到的 case_id\n");
    printf("2. 打印 bt_tone_case_to_custom_type() 的返回值和 tone_type\n");
    printf("3. 检查 MCU 发送的 case_id 是否和预期一致\n");

    return 0;
}
