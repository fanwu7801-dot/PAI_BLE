/*******************************************************
 * @file change_mac.c 
 * @author fanzx (fanzx@1456925916.com)
 * @brief sn号修改mac地址，形成新的mac地址 用于updata
 *        防止固定的mac地址
 * @version 0.1
 * @date 2026-03-16
 * 
 * @copyright Copyright (c) 2026
 * 
*******************************************************/
#include "change_mac.h"
#include <stdint.h>

#define MAC_HEADER_BYTE 0xC0 //MAC地址的静态地址规范

uint8_t get_sn_data ()
{   // TODO: 获取sn号数据的逻辑
    return sn_data;
}


uint8_t* sn_change_mac(uint8_t *mac_addr, uint8_t sn_data)
{
    // 1. 将10位字符串转为 64位无符号整型
    // 注意：不能用 atoi，要用 atoll，因为 10位数超过了 32位整型的上限
    uint64_t sn_num = atoll(sn_str);
    
    // 2. 填充最高字节 (大端/小端序视你的蓝牙芯片API要求而定，这里按从左到右常规视觉习惯)
    mac_out[0] = MAC_HEADER_BYTE;
    
    // 3. 将其余数据拆分到后5个字节 (5 bytes = 40 bits，足够容纳 34 bits的 SN)
    mac_out[1] = (sn_num >> 32) & 0xFF;
    mac_out[2] = (sn_num >> 24) & 0xFF;
    mac_out[3] = (sn_num >> 16) & 0xFF;
    mac_out[4] = (sn_num >> 8)  & 0xFF;
    mac_out[5] = sn_num & 0xFF;
    return mac_out;
}

void updata_mac_addr()
{
    uint8_t mac_addr[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55}; // 原始MAC地址
    uint8_t sn_data = get_sn_data(); // 获取SN号数据
    uint8_t *new_mac_addr = sn_change_mac(mac_addr, sn_data); // 修改MAC地址
    // TODO: 开始使用新的MAC地址进行更新的逻辑
}

