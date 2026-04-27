#include "includes.h"
#include "app_config.h"
#include "norflash_sfc.h"
#include "asm/sfc_norflash_api.h"

#undef LOG_TAG_CONST
#define LOG_TAG     "[FLASH_INDIDE]"
#define LOG_ERROR_ENABLE
#define LOG_INFO_ENABLE
#include "debug.h"

extern void sfc_protect_lock(u8 index);
extern void sfc_protect_release(u8 index);

#define norflash_mutex_init()   //
#define norflash_mutex_enter()  sfc_protect_lock(0)
#define norflash_mutex_exit()   sfc_protect_release(0)

static struct list_head head = LIST_HEAD_INIT(head);

struct norflash_partition {
    struct list_head entry;
    const char *name;
    u32 start_addr;
    u32 size;
    u8  igore_overlaps;     //忽略分区检查
    struct device device;
};

static struct norflash_partition *norflash_find_part(const char *name)
{
    struct norflash_partition *p, *n;
    local_irq_disable();
    list_for_each_entry_safe(p, n, &head, entry) {
        if (!strcmp(p->name, name)) {
            local_irq_enable();
            return p;
        }
    }
    local_irq_enable();
    return NULL;
}

static struct norflash_partition *norflash_new_part(const char *name, u32 addr, u32 size, u8 igore_overlap)
{
    struct norflash_partition *part = zalloc(sizeof(struct norflash_partition));
    ASSERT(part, "norflash_partition malloc err");
    part->name = name;
    part->start_addr = addr;
    part->size = size;
    part->igore_overlaps = igore_overlap;
    local_irq_disable();
    list_add_tail(&part->entry, &head);
    local_irq_enable();
    return part;
}

static void norflash_delete_part(const char *name)
{
    struct norflash_partition *p, *n;
    local_irq_disable();
    list_for_each_entry_safe(p, n, &head, entry) {
        if (!strcmp(p->name, name)) {
            list_del(&p->entry);
            free(p);
            local_irq_enable();
            return ;
        }
    }
    local_irq_enable();
    return ;
}

static int norflash_verify_part(struct norflash_partition *p)
{

    if (p->igore_overlaps) {
        return 0;
    }
    struct norflash_partition *part, *n;
    local_irq_disable();
    list_for_each_entry_safe(part, n, &head, entry) {
        if (part->igore_overlaps) {
            continue;
        }
        if ((p->start_addr >= part->start_addr) && (p->start_addr < part->start_addr + part->size)) {
            if (strcmp(p->name, part->name) != 0) {
                return -1;
            }
        }
    }
    local_irq_enable();
    return 0;
}




static int _norflash_init(const char *name, struct norflash_sfc_dev_platform_data *pdata)
{
    log_info("norflash_sfc_init >>>>");
    if (pdata->path) {
        FILE *fp = fopen(pdata->path, "r");
        if (!fp) {
            log_error("fopen err :%s \n", pdata->path);
            ASSERT(0);
            return false;
        }
        struct vfs_attr attr = {0};
        fget_attrs(fp, &attr);
        u32 part_addr = attr.sclust;
        u32 part_len = attr.fsize;
        part_addr = sdfile_cpu_addr2flash_addr(part_addr);
        ASSERT(pdata->start_addr == part_addr, "%x %x\n", pdata->start_addr, part_addr);
        ASSERT(part_len <= pdata->size, "%x ,%x\n", pdata->start_addr, part_addr);
        fclose(fp);
    }


    struct norflash_partition *part;
    part = norflash_find_part(name);

    if (!part) {
        part = norflash_new_part(name, pdata->start_addr, pdata->size, pdata->igore_overlaps);
        ASSERT(part, "not enough norflash partition memory in array\n");
        ASSERT(norflash_verify_part(part) == 0, "norflash partition %s overlaps\n", name);
        log_info("norflash new partition %s\n", part->name);
    } else {
        ASSERT(0, "norflash partition name already exists\n");
    }
    return 0;
}


static int norflash_sfc_fs_dev_init(const struct dev_node *node, void *arg)
{
    struct norflash_sfc_dev_platform_data *pdata = arg;

    return _norflash_init(node->name, pdata);
}

static int norflash_sfc_fs_dev_open(const char *name, struct device **device, void *arg)
{
    struct norflash_partition *part;
    part = norflash_find_part(name);
    if (!part) {
        log_error("no norflash partition is found\n");
        return -ENODEV;
    }
    *device = &part->device;
    (*device)->private_data = part;
    if (atomic_read(&part->device.ref)) {
        return 0;
    }
    return 0;//_norflash_open(arg);
}
static int norflash_sfc_fs_dev_close(struct device *device)
{
    return 0;//_norflash_close();
}



static int norflash_sfc_fs_dev_read(struct device *device, void *buf, u32 len, u32 offset)
{
    u32 rets;//, reti;
    __asm__ volatile("%0 = rets":"=r"(rets));
    int reg;
    struct norflash_partition *part;
    part = (struct norflash_partition *)device->private_data;
    if (!part) {
        log_error("norflash partition invalid\n");
        return -EFAULT;
    }
    if (offset + len > part->size) {
        log_error("__func__ %s %x %x %x\n", __func__, rets, offset, len);
        ASSERT(0);
    }
    offset += part->start_addr;
    norflash_mutex_enter();
    u32 cpu_addr = sdfile_flash_addr2cpu_addr(offset);
    memcpy((u8 *)buf, (u8 *)cpu_addr, len);
    norflash_mutex_exit();
    return len;
}

static int norflash_sfc_fs_dev_write(struct device *device, void *buf, u32 len, u32 offset)
{
    u32 rets;//, reti;
    __asm__ volatile("%0 = rets":"=r"(rets));
    int reg = 0;
    struct norflash_partition *part = device->private_data;
    if (!part) {
        log_error("norflash partition invalid\n");
        return -EFAULT;
    }
    if (offset + len > part->size) {
        log_error("__func__ %s %x %x %x\n", __func__, rets, offset, len);
        ASSERT(0);
    }
    offset += part->start_addr;
    /* norflash_mutex_enter(); */
    ASSERT(!cpu_in_irq());
    ASSERT(!cpu_irq_disabled());
    reg =  norflash_write(NULL, buf, len, offset);//写操作里面已经集合的互斥,这里仅仅检查中断
    /* norflash_mutex_exit(); */
    return reg;
}

static bool norflash_sfc_fs_dev_online(const struct dev_node *node)
{
    return 1;
}

static int norflash_sfc_fs_dev_ioctl(struct device *device, u32 cmd, u32 arg)
{
    u32 rets;//, reti;
    __asm__ volatile("%0 = rets":"=r"(rets));
    struct norflash_partition *part = device->private_data;
    struct sfc_no_enc_wr   *no_enc_wr;
    int reg;
    if (!part) {
        log_error("norflash partition invalid\n");
        return -EFAULT;
    }

    switch (cmd) {
    case IOCTL_GET_PART_INFO:
        u32 *info = (u32 *)arg;
        u32 *start_addr = &info[0];
        u32 *part_size = &info[1];
        *start_addr = part->start_addr;
        *part_size = part->size;
        break;
    case IOCTL_GET_CAPACITY:
        *(u32 *)arg = part->size;
        break;
    case IOCTL_ERASE_PAGE:
        ASSERT(!cpu_in_irq());
        ASSERT(!cpu_irq_disabled());
        //写操作里面已经集合的互斥,这里仅仅检查中断
        return  norflash_ioctl(NULL, cmd, arg + part->start_addr);
        break;
    case IOCTL_ERASE_SECTOR:
        if ((arg * 1) + 4096 > part->size) {
            log_error("__func__ %s %x %x %x\n", __func__, rets, arg, 1);
            ASSERT(0);
        }

        ASSERT(!cpu_in_irq());
        ASSERT(!cpu_irq_disabled());
        //写操作里面已经集合的互斥,这里仅仅检查中断
        return  norflash_ioctl(NULL, cmd, arg + part->start_addr);
        break;
    case IOCTL_ERASE_BLOCK:
        if ((arg * 1) + (64 * 1024) > part->size) {
            log_error("__func__ %s %x %x %x\n", __func__, rets, arg, 1);
            ASSERT(0);
        }

        ASSERT(!cpu_in_irq());
        ASSERT(!cpu_irq_disabled());
        //写操作里面已经集合的互斥,这里仅仅检查中断
        return  norflash_ioctl(NULL, cmd, arg + part->start_addr);
        break;
    case IOCTL_SFC_NORFLASH_READ_NO_ENC:
        no_enc_wr = (struct sfc_no_enc_wr *)arg;
        norflash_mutex_enter();
        u32 cpu_addr = sdfile_flash_addr2cpu_addr(no_enc_wr->addr);
        memcpy((u8 *)no_enc_wr->buf, (u8 *)cpu_addr, no_enc_wr->len);
        norflash_mutex_exit();
        reg = 0;
        break;
    case IOCTL_SFC_NORFLASH_WRITE_NO_ENC:
        no_enc_wr = (struct sfc_no_enc_wr *)arg;
        /* norflash_mutex_enter(); */
        ASSERT(!cpu_in_irq());
        ASSERT(!cpu_irq_disabled());
        //写操作里面已经集合的互斥,这里仅仅检查中断
        reg =  norflash_write(NULL, no_enc_wr->buf, no_enc_wr->len, no_enc_wr->addr);
        /* norflash_mutex_exit(); */
        break;

    case IOCTL_FLUSH:
        break;
    default:
        return  norflash_ioctl(NULL, cmd, arg);

    }

    return  reg;
}

//按1字节单位读写
const struct device_operations inside_norflash_fs_dev_ops = {
    .init   = norflash_sfc_fs_dev_init,
    .online = norflash_sfc_fs_dev_online,
    .open   = norflash_sfc_fs_dev_open,
    .read   = norflash_sfc_fs_dev_read,
    .write  = norflash_sfc_fs_dev_write,
    .ioctl  = norflash_sfc_fs_dev_ioctl,
    .close  = norflash_sfc_fs_dev_close,
};

#if TCFG_NOR_TONE_FAT

#define FLASH_CACHE_ENABLE              1
#define FLASH_CACHE_SYNC_T_INTERVAL     60

#if FLASH_CACHE_ENABLE
static u32 flash_cache_addr = 4096;
static u8 flash_cache_buf[4096];
static u8 flash_cache_is_dirty;
static u16 flash_cache_timer;
#endif

static int norflash_fat_fs_dev_init(const struct dev_node *node, void *arg)
{
    struct norflash_sfc_dev_platform_data *pdata = arg;

    return _norflash_init(node->name, pdata);
}

static int norflash_fat_fs_dev_open(const char *name, struct device **device, void *arg)
{
    struct norflash_partition *part = norflash_find_part(name);

    if (!part) {
        log_error("no norflash partition is found\n");
        return -ENODEV;
    }
    *device = &part->device;
    (*device)->private_data = part;
    if (atomic_read(&part->device.ref)) {
        return 0;
    }

#if FLASH_CACHE_ENABLE
    norflash_sfc_fs_dev_read(*device, flash_cache_buf, sizeof(flash_cache_buf), 0);
    flash_cache_addr = 0;
    flash_cache_is_dirty = 0;
#endif

    return 0;
}

static int norflash_fat_fs_dev_close(struct device *device)
{
#if FLASH_CACHE_ENABLE
    norflash_mutex_enter();
    if (flash_cache_is_dirty) {
        flash_cache_is_dirty = 0;
        norflash_sfc_fs_dev_ioctl(device, IOCTL_ERASE_SECTOR, flash_cache_addr);
        norflash_sfc_fs_dev_write(device, flash_cache_buf, sizeof(flash_cache_buf), flash_cache_addr);
    }
    norflash_mutex_exit();
#endif

    return 0;
}

static int norflash_fat_fs_dev_read(struct device *device, void *buf, u32 len, u32 addr)
{
    int org_len = len;
    u8 *r_buf = (u8 *)buf;

#if FLASH_CACHE_ENABLE
    addr *= 512;
    len *= 512;

    norflash_mutex_enter();
    u32 r_len = sizeof(flash_cache_buf) - (addr % sizeof(flash_cache_buf));
    if ((addr >= flash_cache_addr) && (addr < (flash_cache_addr + sizeof(flash_cache_buf)))) {
        if (len <= r_len) {
            memcpy(r_buf, flash_cache_buf + (addr - flash_cache_addr), len);
            norflash_mutex_exit();
            return org_len;
        }
        memcpy(r_buf, flash_cache_buf + (addr - flash_cache_addr), r_len);
        addr += r_len;
        r_buf += r_len;
        len -= r_len;
    }
    norflash_mutex_exit();
#else
    addr *= 512;
    len *= 512;
#endif

    norflash_sfc_fs_dev_read(device, r_buf, len, addr);

    return org_len;
}

#if FLASH_CACHE_ENABLE
static void _norflash_cache_sync_timer(struct device *device)
{
    int reg = 0;

    norflash_mutex_enter();
    if (flash_cache_is_dirty) {
        flash_cache_is_dirty = 0;
        reg = norflash_sfc_fs_dev_ioctl(device, IOCTL_ERASE_SECTOR, flash_cache_addr);
        if (reg == -EFAULT) {
            goto __exit;
        }
        norflash_sfc_fs_dev_write(device, flash_cache_buf, sizeof(flash_cache_buf), flash_cache_addr);
    }
    if (flash_cache_timer) {
        sys_timeout_del(flash_cache_timer);
        flash_cache_timer = 0;
    }
__exit:
    norflash_mutex_exit();
}
#endif

static int norflash_fat_fs_dev_write(struct device *device, void *buf, u32 len, u32 addr)
{
    int org_len = len;
    int reg = 0;

    norflash_mutex_enter();
#if FLASH_CACHE_ENABLE
    u8 *w_buf = (u8 *)buf;
    u32 w_len = len * 512;
    addr *= 512;

    while (w_len) {
        u32 align_addr = addr / sizeof(flash_cache_buf) * sizeof(flash_cache_buf);
        u32 cnt = sizeof(flash_cache_buf) - (addr - align_addr);
        cnt = w_len > cnt ? cnt : w_len;

        if (align_addr != flash_cache_addr) {
            if (flash_cache_is_dirty) {
                flash_cache_is_dirty = 0;
                reg = norflash_sfc_fs_dev_ioctl(device, IOCTL_ERASE_SECTOR, flash_cache_addr);
                if (reg == -EFAULT) {
                    goto __exit;
                }
                reg = norflash_sfc_fs_dev_write(device, flash_cache_buf, sizeof(flash_cache_buf), flash_cache_addr);
                if (reg != sizeof(flash_cache_buf)) {
                    goto __exit;
                }
            }
            norflash_sfc_fs_dev_read(device, flash_cache_buf, sizeof(flash_cache_buf), align_addr);
            flash_cache_addr = align_addr;
        }

        memcpy(flash_cache_buf + (addr - align_addr), w_buf, cnt);
        if ((addr + cnt) % sizeof(flash_cache_buf)) {
            flash_cache_is_dirty = 1;
            if (flash_cache_timer) {
                sys_timer_re_run(flash_cache_timer);
            } else {
                flash_cache_timer = sys_timeout_add(0, _norflash_cache_sync_timer, FLASH_CACHE_SYNC_T_INTERVAL);
            }
        } else {
            flash_cache_is_dirty = 0;
            reg = norflash_sfc_fs_dev_ioctl(device, IOCTL_ERASE_SECTOR, align_addr);
            if (reg == -EFAULT) {
                goto __exit;
            }
            reg = norflash_sfc_fs_dev_write(device, flash_cache_buf, sizeof(flash_cache_buf), align_addr);
            if (reg != sizeof(flash_cache_buf)) {
                goto __exit;
            }
        }

        addr += cnt;
        w_buf += cnt;
        w_len -= cnt;
    }
#else
    reg = norflash_sfc_fs_dev_write(device, buf, len * 512, addr * 512);
#endif

__exit:
    norflash_mutex_exit();
    return reg < 0 ? reg : org_len;
}

static bool norflash_fat_fs_dev_online(const struct dev_node *node)
{
    return 1;
}

static int norflash_fat_fs_dev_ioctl(struct device *device, u32 cmd, u32 arg)
{
    struct norflash_partition *part = device->private_data;

    switch (cmd) {
    case IOCTL_ERASE_SECTOR:
        return norflash_sfc_fs_dev_ioctl(device, IOCTL_ERASE_SECTOR, arg * 512);
    case IOCTL_FLUSH:
        norflash_mutex_enter();
        if (flash_cache_is_dirty) {
            flash_cache_is_dirty = 0;
            norflash_sfc_fs_dev_ioctl(device, IOCTL_ERASE_SECTOR, flash_cache_addr);
            norflash_sfc_fs_dev_write(device, flash_cache_buf, sizeof(flash_cache_buf), flash_cache_addr);
        }
        norflash_mutex_exit();
        return 0;
    case IOCTL_GET_CAPACITY:
        *(u32 *)arg = part->size / 512;
        break;
    case IOCTL_GET_BLOCK_SIZE:
        *(u32 *)arg = 512;
        break;
    case IOCTL_CMD_RESUME:
    case IOCTL_CMD_SUSPEND:
        break;
    default:
        log_error("unsupport ioctl cmd %d\n", cmd);
        return -EINVAL;
    }

    return 0;
}

const struct device_operations inside_norflash_fat_fs_dev_ops = {
    .init   = norflash_fat_fs_dev_init,
    .online = norflash_fat_fs_dev_online,
    .open   = norflash_fat_fs_dev_open,
    .read   = norflash_fat_fs_dev_read,
    .write  = norflash_fat_fs_dev_write,
    .ioctl  = norflash_fat_fs_dev_ioctl,
    .close  = norflash_fat_fs_dev_close,
};
#endif  /* TCFG_NOR_TONE_FAT */
