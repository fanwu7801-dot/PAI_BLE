#include "app_config.h"
#include "typedef.h"
#include "fs.h"
#include "update_loader_download.h"
#include "rec_nor/nor_interface.h"

#if (USER_FILE_UPDATE_V2_EN && TCFG_NANDFLASH_DEV_ENABLE && TCFG_NANDFLASH_FAT_ENABLE)

#define LOG_TAG "[UP_NAND_F_DNLD]"
#define LOG_INFO_ENABLE
#define LOG_ERROR_ENABLE
#include "system/debug.h"

typedef enum _FLASH_ERASER {
    CHIP_ERASER,
    BLOCK_ERASER,
    SECTOR_ERASER,
} FLASH_ERASER;

#define ERASE_BLOCK_SIZE	(128 * 1024)
#define FILE_TYPE_FW_NAND_ZONE_FILE		0x41
#define NAND_FLASH_FILE_DOWNLOAD_DEBUG	0
#define NAND_FLASH_UI_ZONE_DEV_NAME		"virfat_flash"
#define NAND_FLASH_FAT_ZONE_DEV_NAME	"fat_nand"
typedef struct _user_file_base_info_t {
    struct list_head 	entry;
    user_chip_update_info_t *(*get_res_base_info)(void *priv,
            u32(**local_read_hdl)(void *buf, u32 addr, u32 len),
            u32(**local_write_hdl)(void *buf, u32 addr, u32 len),
            u32(**local_erase_hdl)(u32 cmd, u32 addr));
    int (*user_chip_release_opt)(user_chip_update_info_t *update_info);
    void *priv;
} user_file_base_info_t;

typedef struct _nand_flash_file_update_info_t {
    user_chip_update_info_t info;
    u32 start_addr;
    u32 len;
    u8 en;
} nand_flash_file_update_info_t;

static nand_flash_file_update_info_t *g_nand_flash_info = NULL;
#define __this (g_nand_flash_info)

#ifndef TCFG_NANDFLASH_FAT_BASE
#define TCFG_NANDFLASH_FAT_BASE 0
#endif

#ifndef TCFG_NANDFLASH_FAT_SIZE
#define TCFG_NANDFLASH_FAT_SIZE 0
#endif

#include "ftl_api.h"
#define NAND_FLASH_UI_ZONE_ADDR		0
#define NAND_FLASH_UI_ZONE_SIZE		(CONFIG_EXTERN_FLASH_SIZE - CONFIG_EXTERN_USER_VM_FLASH_SIZE)
#define NAND_FLASH_FAT_ZONE_ADDR	TCFG_NANDFLASH_FAT_BASE
#define NAND_FLASH_FAT_ZONE_SIZE	TCFG_NANDFLASH_FAT_SIZE

static int nand_zone_f_open(u32 addr, u32 len)
{
    int ret = -1;
    struct dev_info_t {
        u32 start_addr;
        u32 part_size;
    } dev_info;
    if (__this) {
        if (((NAND_FLASH_UI_ZONE_ADDR >= addr) && ((addr + len) <= (NAND_FLASH_UI_ZONE_ADDR + NAND_FLASH_UI_ZONE_SIZE))) ||
            ((NAND_FLASH_FAT_ZONE_ADDR >= addr) && ((addr + len) <= (NAND_FLASH_FAT_ZONE_ADDR + NAND_FLASH_FAT_ZONE_SIZE)))) {
            __this->en = 1;
            ret = 0;
        }
    }
_ERR_RET:
    return ret;
}

static u32 nand_zone_file_write(const u8 *buf, u32 addr, u32 len)
{
    int rlen = 0;
    enum ftl_error_t error;
#if NAND_FLASH_FILE_DOWNLOAD_DEBUG
    log_info("%s, %d, %x, %x, %x\n", __func__, __LINE__, addr, len, CRC16(buf, len));
#endif
    if (__this && __this->en) {
        rlen = ftl_write_bytes(addr, buf, len, &error);
    }
    return rlen;
}

static u32 nand_zone_file_read(u8 *buf, u32 addr, u32 len)
{
    int rlen = 0;
    enum ftl_error_t error;
    if (__this && __this->en) {
        wdt_clear();
        rlen = ftl_read_bytes(addr, buf, len, &error);
    }
    return rlen;
}

static bool nand_zone_file_erase(FLASH_ERASER cmd, u32 addr)
{
    return false;
}

// 填充当前操作的方式的特殊的读、写和擦函数，并返回对应的info信息
static user_chip_update_info_t *nand_get_res_base_info(void *priv,
        u32(**local_read_hdl)(void *buf, u32 addr, u32 len),
        u32(**local_write_hdl)(void *buf, u32 addr, u32 len),
        u32(**local_erase_hdl)(u32 cmd, u32 addr))
{
    nand_flash_file_update_info_t *reserve_info = (nand_flash_file_update_info_t *)priv;
    if (local_read_hdl && local_write_hdl && local_erase_hdl) {
        log_info("nand flash zone start_addr %x and len %x update ...\n", reserve_info->info.dev_addr, reserve_info->info.len);
        __this = (nand_flash_file_update_info_t *)priv;
        *local_read_hdl = nand_zone_file_read;
        *local_write_hdl = nand_zone_file_write;
        *local_erase_hdl = nand_zone_file_erase;
    }
    return &reserve_info->info;
}

static int nand_update_release(user_chip_update_info_t *update_info_entry)
{
    nand_flash_file_update_info_t *file_update_info = (nand_flash_file_update_info_t *)update_info_entry->priv;
    if (update_info_entry) {
        if (update_info_entry->priv) {
            free(update_info_entry->priv);
            update_info_entry->priv = NULL;
        }
        free(update_info_entry);
        update_info_entry = NULL;
    }
    return 0;
}

struct list_head *nand_zone_file_update_init(void *priv, u8 type, u8 *file_name, int (*update_info_get)(u8 type, user_chip_update_info_t *info, void *priv))
{
    user_chip_update_info_t *info = (user_chip_update_info_t *)priv;
    user_file_base_info_t *base_info = NULL;
    __this = NULL;

    if (NULL == priv || FILE_TYPE_FW_NAND_ZONE_FILE != type) {
        goto _ERR_RET;
    }

    __this = zalloc(sizeof(nand_flash_file_update_info_t));
    if (NULL == __this) {
        log_error("%s, alloc fail\n", __func__);
        goto _ERR_RET;
    }
    memcpy(&__this->info, info, sizeof(user_chip_update_info_t));

    if (update_info_get(type, &__this->info, NULL)) {
        log_error("%s, get info err\n", __func__);
        goto _ERR_RET;
    }
    // 尝试open设备，并记录相关信息
    if (nand_zone_f_open(__this->info.dev_addr, __this->info.len)) {
        goto _ERR_RET;
    }

    // 分配当前链表信息，记录上面的数据，返回当前链表
    base_info = zalloc(sizeof(user_file_base_info_t));
    if (NULL == base_info) {
        log_error("%s, alloc base_info fail\n", __func__);
        goto _ERR_RET;
    }

    base_info->get_res_base_info = nand_get_res_base_info;
    base_info->user_chip_release_opt = nand_update_release;
    base_info->priv = (void *)__this;

    return &base_info->entry;
_ERR_RET:
    if (__this) {
        // 注意，如果info中的priv成员有值，最后是需要free掉
        if (__this->info.priv) {
            free(__this->info.priv);
            __this->info.priv = NULL;
        }
        free(__this);
        __this = NULL;
    }
    if (base_info) {
        free(base_info);
    }
    return NULL;
}

#endif

