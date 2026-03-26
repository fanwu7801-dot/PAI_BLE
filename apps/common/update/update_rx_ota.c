#include "update.h"
#include "update_rx_ota.h"
#include "update_loader_download.h"
#include "system/fs/fs.h"
#include "broadcast_upgrade_api.h"
#include "system/includes.h"
#include "app_config.h"
#include "config/config_interface.h"
#include "math.h"
#include "bt_common.h"
#include "btstack/avctp_user.h"
#include "btstack/third_party/wireless_mic/wl_mic_api.h"
#include "ui/ui_api.h"

#if (defined(TCFG_NANDFLASH_DEV_ENABLE) && (TCFG_NANDFLASH_DEV_ENABLE))
#define BROADCAST_OTA_TYPE	(BROADCAST_NAND_DATA_UPDATA)
#else
#define BROADCAST_OTA_TYPE	(BROADCAST_FILL_DATA_UPDATA)
#endif
#define EXIF_CFG_PATH	SDFILE_APP_ROOT_PATH"exif"
#define SECOTR_SIZE	(4*1024L)

#define LOG_TAG "[BC-UPDATE]"
#define LOG_INFO_ENABLE
#define LOG_ERROR_ENABLE
#include "system/debug.h"

typedef struct _exif_info_t {
    u32 addr;
    u32 len;
} exif_info_t;

typedef struct b_ota_update_param_t {
    int (*sleep_hdl)(void *priv);
    void (*resume_hdl)(void *priv);
    exif_info_t exif_info;
    broadcast_upgrade_loader_info loader_info;
} b_ota_update_param;

static int g_result = 0;
static u8 g_broadcast_update_idle = 1;
static u16 g_trigger_timer = 0;
static b_ota_update_param *g_update_param;
#define __this (g_update_param)

#define OTA_NUM 4
#define OTA_LEN 1
#define OTA_HEAD  OTA_NUM + OTA_LEN
#define OTA_BLOCK_SIZE 240
#define OTA_BUFFER_SIZE OTA_BLOCK_SIZE + OTA_HEAD

#define BUFF 4*1024
#define BLOCK 5
#define OTA_BIN_HEAD 2
/* #define OTA_PERIOD 4 */
#define OTA_PERIOD 10

#define BOTA_START_TIMEOUT    (30 *1000L)    //unit:ms
#define BROADCAST_UPDATE_MODULE_CONTROL 0

static u32 app_size;
static u32 block_size;
static u32 nblocks;
static u32 program_blocks;
static bool program;
static u32 lastblock_bytes;
static u32 ota_timer_handle = 0;
static u8 file_buff[BUFF];
static u8 r_buff[(OTA_BUFFER_SIZE) * 50];
static u32 table_bits;
static u32 table_last_bits;

typedef struct {
    u8 *base;
    u8 *head;
    u8 *tail;
    u32 size;
    bool full;
} ring_buffer;

static ring_buffer g_rb;

typedef enum _FLASH_ERASER {
    CHIP_ERASER,
    BLOCK_ERASER,
    SECTOR_ERASER,
} FLASH_ERASER;

extern u32 sdfile_cpu_addr2flash_addr(u32 offset);
extern bool sfc_erase(FLASH_ERASER cmd, u32 addr);
extern u32 sfc_write(const u8 *buf, u32 addr, u32 len);
extern u32 sfc_read(u8 *buf, u32 addr, u32 len);
extern int norflash_erase(u32 cmd, u32 addr);
extern int norflash_write(struct device *device, void *buf, u32 len, u32 offset);

static void ring_buffer_init(ring_buffer *rb, u8 *buf, u32 size)
{
    rb->base = buf;
    rb->head = buf;
    rb->tail = buf;
    rb->size = size;
    rb->full = false;
}

static bool ring_buffer_is_empty(const ring_buffer *rb)
{
    return (rb->head == rb->tail && !rb->full);
}


static bool ring_buffer_if_full(const ring_buffer *rb)
{
    return rb->full;
}

static u32 ring_buffer_get_size(const ring_buffer *rb)
{
    return rb->size;
}

static u32 ring_buffer_get_count(const ring_buffer *rb)
{
    u32 count = 0;
    if (rb->full) {
        count = rb->size;
    } else if (rb->head >= rb->tail) {
        count = (u32)(rb->head - rb->tail);
    } else {
        count = (u32)(rb->head + rb->size - rb->tail);
    }
    return count;
}

static u32 ring_buffer_write(ring_buffer *rb, const u8 *data, u32 count)
{
    u32 i;
    for (i = 0; i < count; i++) {
        *(rb->head++) = *(data + i);
        if (rb->head >= (rb->base + rb->size)) {
            rb->head = rb->base;
        }
        if (rb->full) {
            rb->tail++;
            if (rb->tail >= (rb->base + rb->size)) {
                rb->tail = rb->base;
            }
        } else if (rb->head == rb->tail) {
            rb->full = true;
        }
    }
    return i;
}

static u32 ring_buffer_read(ring_buffer *rb, u8 *data, u32 count)
{
    u32 i;
    for (i = 0; i < count; i++) {
        if (ring_buffer_is_empty(rb)) {
            break;
        }
        *(data + i) = *(rb->tail++);
        if (rb->tail >= (rb->base + rb->size)) {
            rb->tail = rb->base;
        }
        rb->full = false;
    }
    return i;
}

static int update_otarx_init(void)
{
    program = true;

    ring_buffer_init(&g_rb, r_buff, sizeof(r_buff));

    return 0;
}

static int update_otarx_start(void)
{
    return 0;
}

static void update_otarx_deinit(void)
{

}


static int update_otarx_program(u8 *buf, u16 length)
{
    printf("[%d]\n", length);
    if (program) {
        if ((OTA_BUFFER_SIZE + 2) == length && program) {
            ring_buffer_write(&g_rb, buf + 2, OTA_BUFFER_SIZE);
            if (__this->resume_hdl) {
                __this->resume_hdl(NULL);
            }
        }
    }
    return 0;
}

static int task_program(void)
{
    int ret = 0;
    if (program) {
        static u8 buf[OTA_BUFFER_SIZE];
        if (__this->sleep_hdl) {
            if (OS_TIMEOUT == __this->sleep_hdl(NULL)) {
                // err
                return BC_UPDATEING_TIMEOUT;
            }
        }
        if (ring_buffer_read(&g_rb, buf, OTA_BUFFER_SIZE) > 0) {
            put_buf(buf, 11);
            u32 count = ring_buffer_get_count(&g_rb);
            ret = broadcast_upgrade_write_data(buf + OTA_HEAD, OTA_BLOCK_SIZE, NULL);
            if (0 == ret) {
                printf("update success\n");
                program = false;
            }
            if (count > 1000) {
                printf("over %d!!!\n", count);
            }
        }
    }
    return ret;
}

const ota_rx_ops ota_rx_op = {
    .ota_init = update_otarx_init,
    .ota_start = update_otarx_start,
    .ota_program = update_otarx_program,
};

static void boradcast_upgrade_exit(void)
{
    if (__this) {
        free(__this);
        __this = NULL;
    }
    broadcast_upgrade_release();
    g_broadcast_update_idle = 1;
    ui_auto_shut_down_enable();
    ble_module_enable(1);
}

static void broadcast_trigger_fail(void *priv)
{
    if (g_trigger_timer) {
        if (wlm_1t1_rx_op.wlm_close) {
            wlm_1t1_rx_op.wlm_close();
        }
        boradcast_upgrade_exit();
        g_trigger_timer = sys_timeout_add(NULL, broadcast_trigger_fail, BOTA_START_TIMEOUT);
    }
}

int broadcast_upgrade_exit_sniff(void)
{
    ui_auto_shut_down_disable();
    user_send_cmd_prepare(USER_CTRL_ALL_SNIFF_EXIT, 0, NULL);
    g_broadcast_update_idle = 0;
    // 添加定时器 - 1分钟触发
    if (0 == g_trigger_timer) {
        g_trigger_timer = sys_timeout_add(NULL, broadcast_trigger_fail, BOTA_START_TIMEOUT);
    }
    return 0;
}

static int broadcast_upgrade_prepare_handle(void)
{
    extern  void wlm_1t1_padv_create_sync(void);
    wlm_1t1_padv_create_sync();
    return 0;
}

static int broadcast_upgrade_handle(void)
{
    int ret = 0;
    u8 retry_cnt = 5;
    broadcast_upgrade_prepare_handle();
    // 接收loader数据并写入vm中
    while (program && retry_cnt) {
        // 防止二级广播校验失败导致卡在升级流程中的情况
        ret = task_program();
        if (ret < 0) {
            if (BC_UPDATEING_VERIFY_FAIL == ret) {
                break;
            }
            printf("program err\n");
            retry_cnt--;
        }
    }

    if (wlm_1t1_rx_op.wlm_close) {
        wlm_1t1_rx_op.wlm_close();
    }

    return ret;
}

static int broadcast_upgrade_param_write_func(void *buf, u32 offset, u32 len)
{
    if (0 == __this->exif_info.addr || __this->exif_info.len < SECOTR_SIZE) {
        ASSERT(0, "exif area illegal\n");
    }
    printf("exif info:\n");
    put_buf(buf, len);
    int ret = 0;
    norflash_erase(IOCTL_ERASE_SECTOR, __this->exif_info.addr);
    norflash_write(NULL, buf, len, __this->exif_info.addr + offset);

    return ret;
}

static int broadcast_upgrade_bt_info_fill(u8 *fill_buf, u32 area_size)
{
    u16 offset = 0;
    int ret = 0;

    // 填充蓝牙名
    u8 *host_name = (u8 *)bt_get_local_name();
    u16 host_name_len = strlen(bt_get_local_name());
    if ((offset + host_name_len + 4) > area_size) {
        ret = -1;
        goto _ERR_RET;
    }
    offset += b_update_cfg_area_data_fill_with_vm_format(fill_buf + offset, host_name, host_name_len, B_SLAVE_OTA_OTP_BT_NAME);

    // 填充蓝牙地址
    if ((offset + 6 + 4) > area_size) {
        ret = -1;
        goto _ERR_RET;
    }
    offset += b_update_cfg_area_data_fill_with_vm_format(fill_buf + offset, bt_get_mac_addr(), 6, B_SLAVE_OTA_OTP_BT_ADDR);

    ret = offset;
_ERR_RET:
    return ret;
}

static void broadcast_upgrade_private_param_fill(UPDATA_PARM *p)
{
    if (NULL == __this) {
        return;
    }
    u8 *fill_buf = NULL;
    // 获取升级区域信息，填入exif中
    FILE *g_fp = fopen(EXIF_CFG_PATH, "r");
    if (g_fp) {
        struct vfs_attr attr = {0};
        fget_attrs(g_fp, &attr);
        __this->exif_info.addr = sdfile_cpu_addr2flash_addr(attr.sclust);
        __this->exif_info.len = attr.fsize;
        fclose(g_fp);
        g_fp = NULL;
    }

    // 处理exif区域
    if (broadcast_upgrade_exif_check(&__this->exif_info.addr, &__this->exif_info.len)) {
        printf("err: remote exif is not exist\n");
        ASSERT(0);
    } else {
        printf("exif_addr: %x, exif_len: %x\n", __this->exif_info.addr, __this->exif_info.len);
    }

    // 获取填充exif区域的数据
    u16 offset = 0;
    if (__this->exif_info.len) {
        fill_buf = zalloc(__this->exif_info.len);
        if (NULL == file_buff) {
            printf("%s, alloc fail\n", __func__);
            ASSERT(0);
        }
        int ret = broadcast_upgrade_param_fill(fill_buf + offset, __this->exif_info.len - offset);
        if (ret < 0) {
            printf("err:exif is not enough\n");
            ASSERT(0);
        }
        offset += (u16)ret;

        // 填充蓝牙名、mac地址和广播包等信息(后续添加)
        ret = broadcast_upgrade_bt_info_fill(fill_buf + offset, __this->exif_info.len - offset);
        if (ret < -1) {
            printf("err:exif is not enough\n");
            ASSERT(0);
        }
        offset += (u16)ret;

        if (offset > 4 && offset < __this->exif_info.len) {
            ret = broadcast_upgrade_dual_bank_loader_info_fill(fill_buf, __this->exif_info.len, offset);
            if (ret > 0) {
                offset += (u16)ret;
            }
            broadcast_upgrade_param_write_func(fill_buf, 0, offset);
        } else {
            printf("err:%s fail\n", __func__);
            ASSERT(0);
        }
    }

    // 填充exif区域
    if (__this->exif_info.addr) {
        memcpy(p->parm_priv, (u8 *)&__this->exif_info.addr, sizeof(__this->exif_info.addr));
    }

    if (fill_buf) {
        free(fill_buf);
    }
}

static void broadcast_upgrade_before_jump_handle(int type)
{
#if CONFIG_UPDATE_JUMP_TO_MASK
    y_printf(">>>[test]:latch reset update\n");
    latch_reset();
#endif
    cpu_reset();
}

static int broadcast_db_burn_boot_info_result_hdl(void *priv, int type, int err)
{
    extern void update_result_type_set(u16 result, u16 type);
    if (err) {
        update_result_type_set(UPDATA_DEV_ERR, type);
    } else {
        update_result_type_set(UPDATA_SUCC, type);
    }
    cpu_reset();
    return 0;
}

extern void wlm_1t1_ext_scan_en(u8 en);
static void broadcast_upgrade_state_cbk(int type, u32 state, void *priv)
{
    update_ret_code_t *ret_code = (update_ret_code_t *)priv;
    if (NULL == __this) {
        ret_code->stu = -1;
        return;
    }
    switch (state) {
    case UPDATE_CH_INIT:
        wlm_1t1_ext_scan_en(0);
        ret_code->stu = broadcast_upgrade_target_info(type, &__this->loader_info, ret_code->priv);
        break;
    case EX_UPDATE_CH_LOAD_OTA_START:
        wlm_1t1_ext_scan_en(1);
        printf("EX_UPDATE_CH_LOAD_OTA_START\n");
        ret_code->stu = broadcast_upgrade_handle();
        break;
    case UPDATE_CH_EXIT:
        if ((0 == ret_code->stu) && (0 == ret_code->err_code)) {
            printf("BROADCAST_UPDATE\n");
            bt_ble_exit();
            if (broadcast_upgrade_dual_bank_result_dual(NULL, type, broadcast_db_burn_boot_info_result_hdl)) { // 正常执行会重启
                // 升级失败
            }
            update_mode_api_v2(BROADCAST_OTA_TYPE,
                               broadcast_upgrade_private_param_fill,
                               broadcast_upgrade_before_jump_handle);
        }
        boradcast_upgrade_exit();
        break;
    }
}

static void broadcast_upgrade_update_init(void (*resume_hdl)(void *priv), int (*sleep_hdl)(void *priv))
{
    __this->resume_hdl = resume_hdl;
    __this->sleep_hdl = sleep_hdl;
}

// 解析并校验扩展包中的校验数据
int broadcast_upgrade_prepare(u8 *data, u8 data_len)
{
    g_result = broadcast_upgrade_verify_data_analys(data, data_len);
    if (g_result) {
        broadcast_upgrade_release();
    } else {
        // 如果成功删除定时器
        if (g_trigger_timer) {
            sys_timeout_del(g_trigger_timer);
            g_trigger_timer = 0;
            log_info("Del trigger timer\n");
        }
    }
    return g_result;
}

static const update_op_api_t b_ota_op = {
    .ch_init = broadcast_upgrade_update_init,
};

int broadcast_upgrade_loop(void)
{
    int ret = 0;
    if (g_result) {
        ret = -1;
        goto _ERR_RET;
    }
    if (NULL == __this) {
        __this = zalloc(sizeof(b_ota_update_param));
        if (NULL == __this) {
            ret = -1;
            goto _ERR_RET;
        }
    }
    update_mode_info_t info = {
        .type = BROADCAST_OTA_TYPE,
        .state_cbk = broadcast_upgrade_state_cbk,
        .p_op_api = &b_ota_op,
        .task_en = 1,
    };
    ret = broadcast_ota_task_init(&info);

_ERR_RET:
    return ret;
}

static u8 bc_update_idle_query(void)
{
    return g_broadcast_update_idle;
}

REGISTER_LP_TARGET(bc_update_lp_target) = {
    .name = "bc_update",
    .is_idle = bc_update_idle_query,
};

