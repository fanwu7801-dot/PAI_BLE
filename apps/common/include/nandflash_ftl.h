#ifndef __NANDFLASH_FTL_H__
#define __NANDFLASH_FTL_H__

#include "system/includes.h"

#define FTL_NAND_PAGE_SIZE 					2048
#define FTL_NAND_BLOCK_SIZE    			0x20000
enum {
    FTL_OK = 0,
    FTL_DISK_ERR,
    FTL_FILE_ERR,
    FTL_BLOCK_ERR,
    FTL_FLASH_ERR,
    FTL_MOUNT_FAT_ERR,
    FTL_UPDATE_SPBK_ERR,
    FTL_SEARCH_ERR,
    FTL_MAPPING_ERR,
    FTL_WRITE_DIRTY,
};

extern const struct device_operations nandflash_ftl_ops;
extern const struct device_operations nandflash_fs_ftl_ops;
extern const struct device_operations nandflash_sfc_ftl_ops;

#define FTL_BLOCK_FREE   0xFFFFFFFF
#define FTL_BLOCK_BREAK  0xFFFFFFFE

#define FTL_TMP_CACHE_SIZE  8192 //使用4K 倍数
#define FTL_SERCTOR_SIZE  512

#define FTL_BACKUPS_RATIO  32
// #define FTL_ROTATE_RATIO  32


#define FTL_SPBK_BLOCK_INFO_NUM  6 //SPBK + BLOCK 使用6个block

typedef struct _spbk_info {
    u8 spbk_logo[4];  //spbk 标志
    u32 spbk_active_start_addr; //活跃区起始地址
    u32 spbk_active_end_addr; //活跃区结束地址
    u32 spbk_rsv_start_addr; //备份区起始地址
    u32 spbk_rsv_end_addr; //备份区结束地址
    u32 spbk_break_block_num;
    u8 spbk_rsv[6];
    u16 spbk_crc;  //CRC校验
} SPBK_INFO;

//记录逻辑地址映射的物理地址
typedef struct _phy_block_info {
    u32 phy_block;
    u32 phy_offset;
} PHY_BLOCK_INFO;

#if 0
typedef struct _ftl_wbuf {
    u8 *page_cache;
    u32 last_page_addr; //byte 为单位
    u32 last_page_len;//byte 为单位
} FTL_WBUF;
#endif

typedef struct _ftl_hdl {
    SPBK_INFO spbk;
    PHY_BLOCK_INFO phy_block_info;
    void *hdev;
    u8 re_devname[16];
    u8 erase_all;
    u8 *tmp_cache;
    u16 cache_len;
#if 0
    FTL_WBUF wbuf;
#endif
    u16 page_size;
    u32 block_size;
    u16 err_block_num;
    u16 all_block_num;
    u16 cur_block_info_addr; //当前spbk 和 block_info起始block号
    // int new_cold_sblock;
    // int old_cold_exaddr;
} FTL_HDL;

#endif
