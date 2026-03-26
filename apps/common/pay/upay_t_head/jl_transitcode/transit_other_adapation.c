//使用非杰理框架的用户需要重新适配的接口如下：
//
#include "le_smartbox_adv.h"
#include "system/fs/fs.h"
#include "app_config.h"


#if (TCFG_PAY_ALIOS_WAY_SEL==TCFG_PAY_ALIOS_WAY_T_HEAD)

#ifndef PARAM_IN
#define PARAM_IN
#endif

#ifndef PARAM_OUT
#define PARAM_OUT
#endif

#ifndef PARAM_INOUT
#define PARAM_INOUT
#endif

#define USE_USER_FS    0 //不是杰理ui框架或者没有文件系统 需要适配
#define USE_USER_TIME  0 //不是使用杰理rtc 的需要额外适配






void upay_recv_data_handle(uint8_t *data, uint16_t len)
{
    extern int upay2ali_ibuf_to_cbuf(u8 * buf, u32 len);
    upay2ali_ibuf_to_cbuf(data, len);
    extern void upay2ali_send_event(void);
    upay2ali_send_event();
}



void alipay_upay_init()
{
    // upay
#ifdef CONFIG_APP_BT_ENABLE
    upay_ble_regiest_recv_handle(upay_recv_data_handle);
#endif
}


#if TCFG_PAY_TRANSITCODE_ENABLE

//使用了文件系统方案的更新以下路径即可，不需要重新适配文件系统接口
char *alipay_get_rsvd_root()
{
    return "storage/virfat_flash/C/";
}



#if USE_USER_FS

/*
 * 打开名称为filename的文件，并且要求权限为可读写，若不存在该文件则重新创建
 *
 * parametr: in: filename:   文件名
 *
 * return: 文件描述符: 表示成功
 *         NULL: 表示失败
*/

void *alipay_open_rsvd_part(PARAM_IN char filename[36])
{
    void *file = NULL;
    return file;

}

/*
 * 写文件
 *
 * parametr: in: fd: 文件描述符
 *               offset: 地址偏移
 *               data: 要写的数据
 *               data_len: 要写的数据长度
 *
 * return: 0: 表示成功
 *         -1: 表示失败
*/

int alipay_write_rsvd_part(PARAM_IN void *fd, PARAM_IN uint32_t offset, PARAM_IN void *data, PARAM_IN uint32_t data_len)
{
    return -1;
}


/*
 * 读文件
 *
 * parametr: in:  fd: 文件描述符
 *                offset: 地址偏移
 *                read_len: 要读取的长度
 *           out: buffer: 读取的数据所存放的区域

 * return: 0: 表示成功
           -1: 表示失败
 */

int alipay_read_rsvd_part(PARAM_IN void *fd, PARAM_IN uint32_t offset, PARAM_OUT void *buffer, PARAM_INOUT uint32_t *read_len)
{
    return -1;
}

/*
 * 关闭文件
 *
 * parametr: in: fd: 文件描述符

 * return: 0: 表示成功
           -1: 表示失败
 */

int alipay_close_rsvd_part(PARAM_IN void *fd)
{
    return 0;
}

/*
 * 查看文件是否存在
 *
 * parametr: in: filename: 文件名(字符串)

 * return: 0: 文件存在
           -1: 文件不存在
 */

int alipay_access_rsvd_part(PARAM_IN char filename[36])
{
    return -1;

}

/*
 * 删除文件
 *
 * parametr: in: filename: 文件名(字符串)

 * return: 0: 删除成功
           -1: 删除失败
 */

int alipay_remove_rsvd_part(PARAM_IN char filename[36])
{
    return 0;
}

/*
 * 清除所有通过alipay_write_rsvd_part写的alipay文件
 *
 * parametr:

 * return: 0: 清除成功
           -1: 清除失败
           */

int alipay_clear_rsvd_part(void)
{
    return 0;
}


#endif



#if USE_USER_TIME
int transit_time_get_support(struct sys_time *time)
{
    return true;
}
#endif


#endif
#endif


