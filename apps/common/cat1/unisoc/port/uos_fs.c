#include "uos_type.h"
#include "uos_deval.h"
#include "fs/fs.h"

#if TCFG_CAT1_UNISOC_ENABLE

#define UOS_FS_DEBUG_EN  0
uos_int32_t uos_open(const char *file, int flags, uos_int32_t mode)
{
    FILE *fp = NULL;

    if ((flags & O_WRONLY) || (flags & O_APPEND) || (flags & O_SYNC)) {
        printf("%s, line = %d, no support O_WRONLY | O_APPEND | O_SYNC\n", __FUNCTION__, __LINE__);
        return 0;
    }

    if (flags == O_RDONLY) {
        fp = fopen(file, "r");
    } else if ((flags & O_RDWR) && (flags & O_TRUNC) && (fp = fopen(file, "w"))) {
        fdelete(fp);
        fp = fopen(file, "w+");
    } else if ((flags & O_RDWR) && !(flags & O_CREAT)) {
        fp = fopen(file, "w");
    } else if ((flags & O_RDWR) && (flags & O_CREAT)) {
        fp = fopen(file, "w+");
    } else {
        printf("%s, line = %d, no support params\n", __FUNCTION__, __LINE__);
        return 0;
    }

    return (uos_int32_t)(fp);
}

uos_int32_t uos_close(uos_int32_t fd)
{
    return (fclose((FILE *)fd) ? UOS_ERROR : UOS_OK);
}

uos_int32_t uos_read(uos_int32_t fd, void *buf, uos_uint32_t len)
{
    return fread((FILE *)fd, buf, len);
}

uos_int32_t uos_write(uos_int32_t fd, const void *buf, uos_uint32_t len)
{
    return fwrite((FILE *)fd, buf, len);
}

uos_int32_t uos_lseek(uos_int32_t fd, uos_uint32_t offset, uos_int32_t whence)
{
    return fseek((FILE *)fd, offset, whence);
}

uos_int32_t uos_unlink(const char *path)
{
    return (fdelete_by_name(path) ? UOS_ERROR : UOS_OK);
}

uos_int32_t uos_stat(const char *path, struct stat *buf)
{
    FILE *fp;
    struct vfs_attr attr;

    if (fp = fopen(path, "r")) {
        fget_attrs(fp, &attr);
        fclose(fp);
        buf->st_size = attr.fsize;
        return UOS_OK;
    }

    return UOS_ERROR;
}

uos_int32_t uos_fsync(uos_int32_t fd)
{
    return UOS_OK;
}
#if UOS_FS_DEBUG_EN
void uos_fs_demo(void)
{
#define SD_ROOT_PATH "storage/sd0/C/"               //SD卡访问路径
#define EXT_FLASH_PATH "storage/virfat_flash/C/"    //外挂Flash访问路径,最多只能在此路径下加多一层文件夹
    //#define FILE_NAME SD_ROOT_PATH"sdabc.txt"
#define FILE_NAME EXT_FLASH_PATH"flashabc.txt"

    uos_int32_t fp;
    uos_int32_t ret;
    u8 rbuf[64] = {0};
    u8 str[] = "1234567890abcdef";
    struct stat stat = {0};

    //fp = uos_open(FILE_NAME, O_RDONLY, 0);
    //fp = uos_open(FILE_NAME, O_RDWR, 0);
    //fp = uos_open(FILE_NAME, O_RDWR | O_CREAT, 0);
    //fp = uos_open(FILE_NAME, O_RDWR | O_TRUNC, 0);
    fp = uos_open(FILE_NAME, O_RDWR | O_TRUNC | O_CREAT, 0);
    if (!fp) {
        printf("%s, line = %d, uos_open fail\n", __FUNCTION__, __LINE__);
    } else {
        printf("%s, line = %d, uos_open succ\n", __FUNCTION__, __LINE__);

        ret = uos_write(fp, str, sizeof(str));
        printf("%s, line = %d, uos_write_ret = %d\n", __FUNCTION__, __LINE__, ret);

        ret = uos_lseek(fp, sizeof(str), SEEK_SET);
        printf("%s, line = %d, uos_lseek_ret = %d\n", __FUNCTION__, __LINE__, ret);

        ret = uos_write(fp, str, sizeof(str));
        printf("%s, line = %d, uos_write_ret = %d\n", __FUNCTION__, __LINE__, ret);

        ret = uos_lseek(fp, 0, SEEK_SET);
        printf("%s, line = %d, uos_lseek_ret = %d\n", __FUNCTION__, __LINE__, ret);

        memset(rbuf, 0, sizeof(rbuf));
        ret = uos_read(fp, rbuf, sizeof(rbuf));
        printf("%s, line = %d, uos_read_ret = %d, %s\n", __FUNCTION__, __LINE__, ret, rbuf);
        ret = uos_close(fp);
        printf("%s, line = %d, uos_close_ret = %d\n", __FUNCTION__, __LINE__, ret);

        ret = uos_stat(FILE_NAME, &stat);
        printf("%s, line = %d, uos_stat_ret = %d\n", __FUNCTION__, __LINE__, ret);

        ret = uos_unlink(FILE_NAME);
        printf("%s, line = %d, uos_unlink_ret = %d\n", __FUNCTION__, __LINE__, ret);
    }
}
#endif
#endif
