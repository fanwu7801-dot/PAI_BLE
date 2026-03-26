#include "device/ioctl_cmds.h"
#include "device/device.h"
#include "nandflash.h"
#include "ftl_api.h"
#include "app_config.h"


#if (defined TCFG_NANDFLASH_DEV_ENABLE && TCFG_NANDFLASH_DEV_ENABLE)

/* 这里引用 norflash_sfc.h 头文件，是因为需要使用 norflash_sfc_dev_platform_data 结构体，同时保证板级配置一致 */
#include "norflash_sfc.h"

/* 这里容量直接 extern ftl_device.c 中的定义，因为这里必须通过ftl才能访问nandflash，因此 ftl_device 应该先运行 */
extern u32 nandflash_g_capacity;

/* 以下内容参考 Norflash_sfc.c 实现，保证 vm 区操作与norflash一致 */
#define MAX_NORFLASH_PART_NUM       4

struct norflash_partition {
    const char *name;
    u32 start_addr;
    u32 size;
    struct device device;
};
static struct norflash_partition nor_part[MAX_NORFLASH_PART_NUM];

struct norflash_info {
    u32 flash_id;
    u32 flash_capacity;
    int spi_num;
    int spi_err;
    u8 spi_cs_io;
    u8 spi_r_width;
    u8 part_num;
    u8 open_cnt;
    struct norflash_partition *const part_list;
    OS_MUTEX mutex;
    u32 max_end_addr;
};

static struct norflash_info _norflash = {
    .spi_num = (int) - 1,
    .part_list = nor_part,
};

static struct norflash_partition *norflash_find_part(const char *name)
{
    struct norflash_partition *part = NULL;
    u32 idx;
    for (idx = 0; idx < MAX_NORFLASH_PART_NUM; idx++) {
        part = &_norflash.part_list[idx];
        if (part->name == NULL) {
            continue;
        }
        if (!strcmp(part->name, name)) {
            return part;
        }
    }
    return NULL;
}

static struct norflash_partition *norflash_new_part(const char *name, u32 addr, u32 size)
{
    struct norflash_partition *part;
    u32 idx;
    for (idx = 0; idx < MAX_NORFLASH_PART_NUM; idx++) {
        part = &_norflash.part_list[idx];
        if (part->name == NULL) {
            break;
        }
    }
    if (part->name != NULL) {
        printf("create norflash part fail\n");
        return NULL;
    }
    memset(part, 0, sizeof(*part));
    part->name = name;
    part->start_addr = addr;
    part->size = size;
    if (part->start_addr + part->size > _norflash.max_end_addr) {
        _norflash.max_end_addr = part->start_addr + part->size;
    }
    _norflash.part_num++;
    return part;
}

static int norflash_verify_part(struct norflash_partition *p)
{
    struct norflash_partition *part = NULL;
    u32 idx;
    for (idx = 0; idx < MAX_NORFLASH_PART_NUM; idx++) {
        part = &_norflash.part_list[idx];
        if (part->name == NULL ||
            0 == strcmp(p->name, "update_noenc") ||
            0 == strcmp(part->name, "update_noenc")) {
            continue;
        }
        if ((p->start_addr >= part->start_addr) && (p->start_addr < part->start_addr + part->size)) {
            if (strcmp(p->name, part->name) != 0) {
                return -1;
            }
        }
    }
    return 0;
}


#if (defined TCFG_NOR_VM && TCFG_NOR_VM)
static int ftl_dev_init(const struct dev_node *node, void *arg)
{
    struct norflash_partition *part = NULL;
    struct norflash_sfc_dev_platform_data *pdata = arg;
    part =  norflash_find_part(node->name);

    if (!part) {
        part = norflash_new_part(node->name, pdata->start_addr, pdata->size);
        ASSERT(part, "not enough norflash partition memory in array\n");
        ASSERT(norflash_verify_part(part) == 0, "norflash partition %s overlaps\n", node->name);
        printf("norflash new partition %s\n", part->name);
    } else {
        ASSERT(0, "norflash partition name already exists\n");
    }

    return 0;
}

static int ftl_dev_open(const char *name, struct device **device, void *arg)
{
    /* 这里使用 ftl 设备进行操作，使用时ftl设备应该已经挂载成功。 */
    struct norflash_partition *part = NULL;
    part = norflash_find_part(name);
    if (!part) {
        printf("Error: no nandflash partition is found!\n");
        return -ENODEV;
    }
    *device = &part->device;
    (*device)->private_data = part;
    return 0;
}

static int ftl_dev_close(struct device *device)
{
    return 0;
}

static int ftl_dev_read(struct device *device, void *buf, u32 len, u32 offset)
{
    enum ftl_error_t error;
    struct norflash_partition *part = device->private_data;

    /* 防止访问的地址超过vm区配置 */
    ASSERT((offset + len) <= part->size, "read nandflash addr over limit: 0x%x\n", (offset + len));

    /* 偏移到vm区起始地址 */
    offset += part->start_addr;
    /* printf("@@@ [%s] %s: start: 0x%x, offset: 0x%x\n", __FILE__, __func__, part->start_addr, offset); */

    int rlen = ftl_read_bytes(offset, buf, len, &error);
    if (rlen < 0) {
        return 0;
    }
    return rlen;
}

static int ftl_dev_write(struct device *device, void *buf, u32 len, u32 offset)
{
    enum ftl_error_t error;
    struct norflash_partition *part = device->private_data;

    /* 防止访问的地址超过vm区配置 */
    ASSERT((offset + len) <= part->size, "write nandflash addr over limit: 0x%x\n", (offset + len));

    /* 偏移到vm区起始地址 */
    offset += part->start_addr;
    /* printf("@@@ [%s] %s: start: 0x%x, offset: 0x%x\n", __FILE__, __func__, part->start_addr, offset); */

    int wlen = ftl_write_bytes(offset, buf, len, &error);
    if (wlen < 0) {
        return 0;
    }
    return wlen;
}

static bool ftl_dev_online(const struct dev_node *node)
{
    return 1;
}

u32 ftl_get_nand_id();
static int ftl_dev_ioctl(struct device *device, u32 cmd, u32 arg)
{
    int reg = 0;
    switch (cmd) {
    case IOCTL_GET_STATUS:
        *(u32 *)arg = 1;
        break;
    case IOCTL_GET_ID:
        *((u32 *)arg) = ftl_get_nand_id();
        break;
    case IOCTL_GET_BLOCK_SIZE:
        *(u32 *)arg = 512;//usb fat
        break;
    case IOCTL_ERASE_BLOCK:
        break;
    case IOCTL_GET_CAPACITY:
        *(u32 *)arg = nandflash_g_capacity;
        break;
    case IOCTL_FLUSH:
        break;
    case IOCTL_CMD_RESUME:
        break;
    case IOCTL_CMD_SUSPEND:
        break;
    case IOCTL_CHECK_WRITE_PROTECT:
        *(u32 *)arg = 0;
        break;
    default:
        reg = -EINVAL;
        break;
    }
    return reg;
}

const struct device_operations nandflash_fs_ftl_ops = {
    .init   = ftl_dev_init,
    .online = ftl_dev_online,
    .open   = ftl_dev_open,
    .read   = ftl_dev_read,
    .write  = ftl_dev_write,
    .ioctl  = ftl_dev_ioctl,
    .close  = ftl_dev_close,
};

#endif // TCFG_NOR_VM


/* 普通fat系统512对齐访问 */

static int ftl_sfc_dev_init(const struct dev_node *node, void *arg)
{
    struct norflash_partition *part = NULL;
    struct norflash_sfc_dev_platform_data *pdata = arg;
    part =  norflash_find_part(node->name);

    if (!part) {
        part = norflash_new_part(node->name, pdata->start_addr, pdata->size);
        ASSERT(part, "not enough norflash partition memory in array\n");
        ASSERT(norflash_verify_part(part) == 0, "norflash partition %s overlaps\n", node->name);
        printf("norflash new partition %s\n", part->name);
    } else {
        ASSERT(0, "norflash partition name already exists\n");
    }

    return 0;
}

static int ftl_sfc_dev_open(const char *name, struct device **device, void *arg)
{
    /* 这里使用 ftl 设备进行操作，使用时ftl设备应该已经挂载成功。 */
    struct norflash_partition *part = NULL;
    part = norflash_find_part(name);
    if (!part) {
        printf("Error: no nandflash partition is found!\n");
        return -ENODEV;
    }

    ASSERT((part->size + part->start_addr) <= (nandflash_g_capacity * 512), "part->size %x need <= %x\n", part->size, part->size - ((part->size + part->start_addr) - nandflash_g_capacity * 512));
    *device = &part->device;
    (*device)->private_data = part;
    return 0;
}

static int ftl_sfc_dev_close(struct device *device)
{
    return 0;
}

static int ftl_sfc_dev_read(struct device *device, void *buf, u32 len, u32 offset)
{
    enum ftl_error_t error;
    struct norflash_partition *part = device->private_data;

    offset = offset * 512;
    len = len * 512;

    /* 防止访问的地址超过vm区配置 */
    ASSERT((offset + len) <= part->size, "read nandflash addr over limit: 0x%x\n", (offset + len));

    /* 偏移到vm区起始地址 */
    offset += part->start_addr;
    /* printf("@@@ [%s] %s: start: 0x%x, offset: 0x%x\n", __FILE__, __func__, part->start_addr, offset); */
    /* printf("@@@ [] %s: start: 0x%x, offset: 0x%x, len: %d\n", __func__, part->start_addr, offset, len); */

    int rlen = ftl_read_bytes(offset, buf, len, &error);
    if (rlen < 0) {
        return 0;
    }
    return rlen / 512;
}

static int ftl_sfc_dev_write(struct device *device, void *buf, u32 len, u32 offset)
{
    enum ftl_error_t error;
    struct norflash_partition *part = device->private_data;

    offset = offset * 512;
    len = len * 512;

    /* 防止访问的地址超过vm区配置 */
    ASSERT((offset + len) <= part->size, "write nandflash addr over limit: 0x%x\n", (offset + len));

    /* 偏移到vm区起始地址 */
    offset += part->start_addr;
    /* printf("@@@ [%s] %s: start: 0x%x, offset: 0x%x\n", __FILE__, __func__, part->start_addr, offset); */

    int wlen = ftl_write_bytes(offset, buf, len, &error);
    if (wlen < 0) {
        return 0;
    }
    return wlen / 512;
}

static bool ftl_sfc_dev_online(const struct dev_node *node)
{
    return 1;
}

u32 ftl_get_nand_id();
static int ftl_sfc_dev_ioctl(struct device *device, u32 cmd, u32 arg)
{
    int reg = 0;
    struct norflash_partition *part = device->private_data;
    switch (cmd) {
    case IOCTL_GET_STATUS:
        *(u32 *)arg = 1;
        break;
    case IOCTL_GET_ID:
        *((u32 *)arg) = ftl_get_nand_id();
        break;
    case IOCTL_GET_BLOCK_SIZE:
        *(u32 *)arg = 512;//usb fat
        break;
    case IOCTL_ERASE_BLOCK:
        break;
    case IOCTL_GET_CAPACITY:
        *(u32 *)arg = part->size / 512;
        break;
    case IOCTL_FLUSH:
        break;
    case IOCTL_CMD_RESUME:
        break;
    case IOCTL_CMD_SUSPEND:
        break;
    case IOCTL_CHECK_WRITE_PROTECT:
        *(u32 *)arg = 0;
        break;
    default:
        reg = -EINVAL;
        break;
    }
    return reg;
}

const struct device_operations nandflash_sfc_ftl_ops = {
    .init   = ftl_sfc_dev_init,
    .online = ftl_sfc_dev_online,
    .open   = ftl_sfc_dev_open,
    .read   = ftl_sfc_dev_read,
    .write  = ftl_sfc_dev_write,
    .ioctl  = ftl_sfc_dev_ioctl,
    .close  = ftl_sfc_dev_close,
};



#endif

