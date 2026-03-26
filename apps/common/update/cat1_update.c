#include "task_manager/cat1/cat1_common.h"
#include "system/includes.h"
#include "app_action.h"
#include "smartbox/mass_data/mass_data.h"
#include "update/update.h"
#include "update/update_loader_download.h"
#include "btstack/avctp_user.h"

#if (TCFG_CAT1_MODULE_UPDATE_ENABLE == 1)

#define LOG_TAG_CONST       CAT1_UPDATE
#define LOG_TAG             "[CAT1-UPDATE]"
#define LOG_INFO_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_ERROR_ENABLE
#define LOG_DUMP_ENABLE

#define LOG_ASSERT_ENABLE
#include "debug.h"

#define THIS_TASK_NAME                      "cat1_update"
#define CAT1_MODULE_START_UPDATE_APP_WAIT_TIMEOUT    (60)  // APP等待启动模块升级时的超时等待, 1min
#define CAT1_MODULE_START_UPDATE_TIMEOUT    (10*1000)  // 启动模块升级时的超时等待, 10s
#define CAT1_DATA_TRANS_TIMEOUT             (5 * 1000) // 数据传输超时时间, 5s

#define CAT1_UPDATE_TRANS_BUF_LEN           (4*1024)   // 对固件数据进行组包加快传输速度

#define CFG_CAT1_UPDATE_CAT1_VER            (42)   // 保存版本(vm id)
#define CFG_CAT1_UPDATE_FLAG_ID             (43)   // 保存升级是否完成标志(vm id)

#define CAT1_UPDATE_BEGIN_FLAG              (0x5A55A5AA)  // 标识启动升级
#define CAT1_UPDATE_END_FLAG                (0x46574F4B)  // 标识升级完成

#define READ_BIG_U16(a) ((*((u8*)(a)) <<8) + *((u8*)(a)+1))

enum : u8 {
    // 升级状态信息命令的state
    CAT1_UPDATE_STATE_END = 0,
    CAT1_UPDATE_STATE_START,
    CAT1_UPDATE_STATE_DATA_TRANS,
};

enum : u8 {
    // 升级状态信息命令的结果码
    CAT1_UPDATE_RESULT_SUCC = 0,
    CAT1_UPDATE_RESULT_FAIL,
    CAT1_UPDATE_RESULT_TIMEOUT,
};

typedef enum cat1_update_state_e : u32 {
    // 不可随意添加、修改顺序
    // 下面三种状态互斥
    MODULE_UPDATE_END = BIT(0),
    MODULE_UPDATE_STARTING = BIT(1),
    MODULE_UPDATE_START = BIT(2),
    MODULE_UPDATE_DATA_TRANS = BIT(3),
    MODULE_UPDATE_STATE_MAX = MODULE_UPDATE_END | MODULE_UPDATE_STARTING | MODULE_UPDATE_START | MODULE_UPDATE_DATA_TRANS,
    // 下面三种状态互斥
    MODULE_UPDATE_SUCC = BIT(6),
    MODULE_UPDATE_FAIL = BIT(7),
    MODULE_UPDATE_TIMEOUT = BIT(8),
    MODULE_UPDATE_RESULT_MAX = MODULE_UPDATE_SUCC | MODULE_UPDATE_FAIL | MODULE_UPDATE_TIMEOUT,
    // 下面两种状态互斥
    GET_MODULE_VER = BIT(11),
    GET_MODULE_VER_OK = BIT(12),
    GET_MODULE_VER_MAX = GET_MODULE_VER | GET_MODULE_VER_OK,
    // 蓝牙断连
    MODULE_UPDATE_BT_DISCONNECT = BIT(16),
} cat1_update_state_t;

typedef struct cat1_update_s {
    u32 fw_pack_size;
    u8 cb_priv[1];
    int w_offset;
    u32 update_flag;
    u16 timer;
    char *cat1_version;
    cat1_update_state_t cat1_module_state;
    u8 *cat1_update_trans_buf;
    u32 packet_offset;
    u8 cat1_module_exception;
    u8 cat1_ver_need_upload;
} cat1_update_t;

static cat1_update_t cat1_update;

extern u8 smartbox_command_set_cmd_notify_cat1_update_result(u8 state, u8 result);
extern u8 smartbox_command_set_cmd_notify_cat1_update_basic_info(void);
extern int watch_sensor_close(void);
extern void ble_module_enable(u8 en);

static void cat1_module_get_version_timer(void *p);
static void cat1_module_save_version(void);
static void cat1_module_update_close(void);
static void cat1_module_state_clr(cat1_update_state_t state);
static void cat1_module_update_data_recv_callback(void *priv, u8 *data, int len);
static void cat1_module_check_update_exception(void);
static void cat1_module_update_reboot(void);

static void cat1_module_recv_evet_cb(void *priv, int event, void *evt_param)
{
    int result = (int)evt_param;
    if (event == MASS_DATA_RECV_EVENT_OK) {
        cat1_module_post_update_result(CAT1_UPDATE_MSG_DATA_TRANS_END, result);
    }
}

static void cat1_module_update_bt_recv_data_register(void *cb_priv, void (*cb)(void *priv, u8 *data, int len))
{
    /* mass_data_recv_register(MASS_DATA_TYPE_CAT1_UPDATE, cb_priv, cb); */
    struct _MASSDATA_RECV_CB recv_cb = {0};
    recv_cb.part_cb = 1;
    recv_cb.cb = cb;
    recv_cb.cb_priv = cb_priv;
    recv_cb.evt_cb = cat1_module_recv_evet_cb;
    mass_data_recv_register_ext(MASS_DATA_TYPE_CAT1_UPDATE, &recv_cb);
}

static void cat1_module_state_set(cat1_update_state_t state)
{
    if (state & MODULE_UPDATE_STATE_MAX) {
        cat1_module_state_clr(MODULE_UPDATE_STATE_MAX);
    }
    if (state & MODULE_UPDATE_RESULT_MAX) {
        cat1_module_state_clr(MODULE_UPDATE_RESULT_MAX);
    }
    if (state & GET_MODULE_VER_MAX) {
        cat1_module_state_clr(GET_MODULE_VER_MAX);
    }
    cat1_update.cat1_module_state |= state;
}

static void cat1_module_state_clr(cat1_update_state_t state)
{
    cat1_update.cat1_module_state &= ~state;
}

static u8 cat1_module_state_match(cat1_update_state_t state)
{
    return ((cat1_update.cat1_module_state & state) != 0 ? 1 : 0);
}

static u8 cat1_module_get_update_state(void)
{
    u8 state = (u8)(cat1_update.cat1_module_state & MODULE_UPDATE_STATE_MAX);
    switch (state) {
    case MODULE_UPDATE_END:
        return CAT1_UPDATE_STATE_END;
    case MODULE_UPDATE_START:
        return CAT1_UPDATE_STATE_START;
    case MODULE_UPDATE_DATA_TRANS:
        return CAT1_UPDATE_STATE_DATA_TRANS;
    default:
        log_debug("maybe not in update 0x%x\n", state);
        return CAT1_UPDATE_STATE_END;
    }
    return CAT1_UPDATE_STATE_END;
}

static u8 cat1_module_get_update_result(void)
{
    u8 result = (u8)(cat1_update.cat1_module_state & MODULE_UPDATE_RESULT_MAX);
    switch (result) {
    case MODULE_UPDATE_SUCC:
        return CAT1_UPDATE_RESULT_SUCC;
    case MODULE_UPDATE_FAIL:
        return CAT1_UPDATE_RESULT_FAIL;
    case MODULE_UPDATE_TIMEOUT:
        return CAT1_UPDATE_RESULT_TIMEOUT;
    default:
        log_debug("maybe not in update 0x%x\n", result);
        return CAT1_UPDATE_RESULT_FAIL;
    }
    return CAT1_UPDATE_RESULT_FAIL;
}

u8 cat1_module_is_updating(void)
{
    if ((cat1_module_get_update_state() == CAT1_UPDATE_STATE_END) && \
        !cat1_module_state_match(MODULE_UPDATE_STARTING)) {
        return 0;
    }
    return 1;
}

void cat1_module_update_init(void)
{
    cat1_module_state_set(GET_MODULE_VER);

    // 检查是否存在升级异常
    cat1_module_check_update_exception();

    if (cat1_module_get_power_state() == CAT1_POWER_STATE_POWEROFF) {
        // 启动电源
        cat1_ctrl_poweron();
        // 创建定时器去检测是否可以获取版本号
        if (cat1_module_state_match(GET_MODULE_VER)) {
            cat1_module_get_version_timer(NULL);
        }
    } else if (cat1_module_get_power_state() == CAT1_POWER_STATE_POWERON) {
        // 可以直接获取版本号
        cat1_module_save_version();
    } else {
        cat1_module_state_clr(GET_MODULE_VER);
        log_error("cat1 module state not support get version\n");
    }
}

static void cat1_module_save_version(void)
{
    u16 ver_len = cat1_module_get_version_total_len();
    char *tmp_buf = (char *)zalloc(ver_len);

    cat1_module_state_clr(GET_MODULE_VER_MAX);

    if (cat1_update.cat1_version == NULL) {
        cat1_update.cat1_version = (char *)zalloc(ver_len); // 不释放
        if (cat1_update.cat1_version == NULL) {
            log_error("cat1_version zalloc err !!!\n");
            free(tmp_buf);
            return;
        }
    }

    if (cat1_module_pasre_version(cat1_update.cat1_version) == 0) {
        cat1_module_state_set(GET_MODULE_VER_OK);

        syscfg_read(CFG_CAT1_UPDATE_CAT1_VER, tmp_buf, ver_len);
        if ((strlen(cat1_update.cat1_version) != 0) && strncmp(tmp_buf, cat1_update.cat1_version, ver_len)) { // 新旧版本不同, 需要更新上传
            if (syscfg_write(CFG_CAT1_UPDATE_CAT1_VER, cat1_update.cat1_version, ver_len) != ver_len) {
                log_error("%s version write err\n", __func__);
            }
        }
    }

    free(tmp_buf);
    cat1_ctrl_poweroff();
    os_time_dly(1);
}

char *cat1_module_get_version(void)
{
    u8 *ret = NULL;
    u16 ver_len = cat1_module_get_version_total_len();

    if (cat1_module_state_match(GET_MODULE_VER_OK)) {
        ret = cat1_update.cat1_version;
    } else { // 版本号还没读取成功, 去vm获取保存的版本号
        u8 *tmp_ver_p = (u8 *)zalloc(ver_len);

        if (syscfg_read(CFG_CAT1_UPDATE_CAT1_VER, tmp_ver_p, ver_len) != ver_len) {
            log_error("%s version read err\n", __func__);
            ret = NULL;
        } else {
            if (cat1_update.cat1_version == NULL) {
                cat1_update.cat1_version = zalloc(ver_len);  // 不释放
                if (cat1_update.cat1_version == NULL) {
                    log_error("cat1_version zalloc err !!!\n");
                    free(tmp_ver_p);
                    return NULL;
                }
            }
            memcpy(cat1_update.cat1_version, tmp_ver_p, ver_len);
            ret = cat1_update.cat1_version;
        }
        free(tmp_ver_p);
    }

    return ret;
}

static void cat1_module_get_version_timer(void *p)
{
    static s8 timeout = 10; // 10s超时

    if (cat1_update.timer == 0) {
        timeout = 10;
        cat1_update.timer = sys_timer_add(NULL, cat1_module_get_version_timer, 1000);
        return;
    }

    if (cat1_module_get_power_state() != CAT1_POWER_STATE_POWERON) {
        return;
    }

    if (timeout-- <= 0) {
        timeout = 0;
        sys_timer_del(cat1_update.timer);
        cat1_update.timer = 0;
        log_error("cat1 module get version timeout !!!\n");
    } else if (cat1_module_can_get_version()) {
        timeout = 0;
        sys_timer_del(cat1_update.timer);
        cat1_update.timer = 0;
        cat1_module_save_version();
        ;
        if (cat1_update.cat1_ver_need_upload && (cat1_update.cat1_module_exception == 0) &&
            (cat1_module_state_match(MODULE_UPDATE_STARTING) == 0)) { // cat1异常的情况不需要更新版本号
            cat1_update.cat1_ver_need_upload = 0;
            if (smartbox_command_set_cmd_notify_cat1_update_basic_info()) {
                log_error("notify cat1 version err\n");
            }
        }
    }

    if ((cat1_update.timer == 0) && (cat1_update.cat1_module_exception)) { // 强制升级+版本号获取失败, 需要复位重新启动4G模块电源
        cat1_module_update_reboot();
    }
}

static void cat1_module_get_version_err_deal(void *p)
{
    log_debug("%s", cat1_module_get_version_err_deal);
    if (cat1_update.cat1_module_exception) {
        cat1_module_update_reboot();
    }
}

// return : 0: err , other: ok
u16 cat1_module_update_basic_info_get(u8 *buf, u16 len)
{
    u16 wlen = 0;
    u8 force_update_offset = 0;

    char *ver = cat1_module_get_version();
    if (ver) {
        if (len < (7 + strlen(ver))) { // 常数是4G模块信息的除版本号以外的数据长度,具体看协议文档
            log_error("%d len err %d\n", __LINE__, len);
            return 0;   // 错误
        }
    } else {
        log_error("get the version err\n");
        // 获取版本号失败不返回, 避免升级失败后无法识别到需要强制升级的问题
        // return wlen;
    }

    buf[wlen++] = cat1_module_get_vender_id();  // vender
    force_update_offset = wlen;
    if (cat1_update.update_flag == CAT1_UPDATE_BEGIN_FLAG) {
        buf[wlen++] = 1;  // 需要强制升级
    } else {
        buf[wlen++] = 0;  // 不需要强制升级
    }
    if (ver) {
        buf[wlen++] = strlen(ver);   // 版本号长度
        memcpy(&buf[wlen], ver, strlen(ver)); // 版本号
        wlen += strlen(ver);
        if (strlen(ver) == 0) {
            if (buf[force_update_offset]) {
                cat1_update.cat1_module_exception = 1;  // 需求强制升级且版本号为空的情况, 默认认为是4G模块异常
                if (cat1_update.timer == 0) {
                    sys_timeout_add(NULL, cat1_module_get_version_err_deal, 1000);
                }
            }
            cat1_update.cat1_ver_need_upload = 1;   // 版本号为空, 需要更新版本号
            /* buf[force_update_offset] = 0;  // 版本号为空不需要强制升级 */
        }
    } else {
        buf[wlen++] = 0;   // 版本号长度
    }

    u16 timeout = CAT1_MODULE_START_UPDATE_APP_WAIT_TIMEOUT;
    timeout = READ_BIG_U16(&timeout);
    memcpy(&buf[wlen], &timeout, 2);
    wlen += 2;
    timeout = cat1_module_get_update_end_timeout();
    timeout = READ_BIG_U16(&timeout);
    memcpy(&buf[wlen], &timeout, 2);
    wlen += 2;

    return wlen;
}

u16 cat1_module_update_get_update_result_info(u8 *buf, u16 len)
{
    if (len < 2) {
        log_error("%d len err %d\n", __LINE__, len);
        return 0;
    }

    u8 state = cat1_module_get_update_state();
    u8 result = cat1_module_get_update_result();
    u16 wlen = 0;

    buf[wlen++] = state;   // 升级状态
    if (state == 0x00) {
        buf[wlen++] = result;   // 升级结果码
    }

    return wlen;
}

void cat1_module_post_update_result(u8 type, int result)
{
    int ret = os_taskq_post_msg(THIS_TASK_NAME, 2, type, result);
    if (ret != OS_NO_ERR) {
        log_error("cat1 update post err %d\n", ret);
    }
}

static void cat1_module_update_event_to_user(u8 event, u8 size)
{
    struct sys_event e;
    e.type = SYS_DEVICE_EVENT;
    e.arg  = (void *)DEVICE_EVENT_FROM_CAT1_UPDATE;
    e.u.cfg_tool.event = event;
    e.u.cfg_tool.size = size;
    sys_event_notify(&e);
}

static void cat1_module_check_update_exception(void)
{
    if (syscfg_read(CFG_CAT1_UPDATE_FLAG_ID, &cat1_update.update_flag, sizeof(cat1_update.update_flag)) != sizeof(cat1_update.update_flag)) {
        log_error("%s read update flag err\n", __func__);
    } else {
        log_debug("%s read update flag 0x%x\n", __func__, cat1_update.update_flag);
    }
}

static void cat1_module_update_data_trans_timeout(void *p)
{
    log_debug("%s", __func__);
    if (cat1_module_state_match(MODULE_UPDATE_START) || cat1_module_state_match(MODULE_UPDATE_DATA_TRANS)) {
        cat1_module_post_update_result(CAT1_UPDATE_MSG_BT_DISCONNECT, CAT1_UPDATE_RESULT_FAIL);
    }
}

// return : 0: err , other: ok
u8 cat1_module_update_call_disable(void)
{
    u8 call_state = 0;
    u32 prev_time = 0;
    // 挂断电话
    if ((cat1_get_call_status(&call_state) == true) && (call_state != BT_CALL_HANGUP)) {
        log_debug("%s: in calling\n", __func__);
        cat1_call_hangup();
        prev_time = jiffies_msec();
        while (1) {
            if ((cat1_get_call_status(&call_state) == false) || (call_state == BT_CALL_HANGUP)) {
                break;
            }
            if ((jiffies_msec() - prev_time) > 5000) {  // 5s超时
                return 0;
            }
            os_time_dly(30);
        }
    }
    // 关闭通话功能
    if (cat1_get_lte_call_onoff()) {
        log_debug("%s: disable lte call\n", __func__);
        cat1_set_lte_call_onoff(0);
        prev_time = jiffies_msec();
        while (1) {
            if (!cat1_get_lte_call_onoff()) {
                break;
            }
            if ((jiffies_msec() - prev_time) > 3000) { // 3s超时
                return 0;
            }
            os_time_dly(30);
        }
    }
    return 1;
}

void cat1_module_update_task(void *p)
{
    int msg[3];
    int ret = 0;

    os_time_dly(2);//让出cpu去回复ble消息
    log_info("cat1_module_update_task\n");
    while (1) {
        if (os_taskq_pend(NULL, msg, sizeof(msg)) != OS_TASKQ) {
            log_error("%s msg err\n", __func__, __LINE__);
            continue;
        }
        if (msg[0] != Q_MSG) {
            log_error("%s msg err %d\n", __func__, __LINE__);
            continue;
        }
        log_debug("cat1 update task msg 0x%x\n", msg[1]);
        switch (msg[1]) {
        case CAT1_UPDATE_MSG_SUCC:
            break;
        case CAT1_UPDATE_MSG_FAIL:
            break;
        case CAT1_UPDATE_MSG_TIMEOUT:
            break;
        case CAT1_UPDATE_MSG_DATA_TRANS_END:
            puts("CAT1_UPDATE_MSG_DATA_TRANS_END\n");
            if (msg[2] == 0) { // msg[2] == 0 表示大数据传输成功
                ret = cat1_module_fota_end();
                if (ret == 0) {
                    log_info("aic fota update succ!\n");
                    smartbox_command_set_cmd_notify_cat1_update_result(CAT1_UPDATE_STATE_END, CAT1_UPDATE_RESULT_SUCC);
                } else {
                    log_debug("aic fota update failed(ret=%d), retry later!  %d\n", ret, msg[2]);
                    smartbox_command_set_cmd_notify_cat1_update_result(CAT1_UPDATE_STATE_END, CAT1_UPDATE_RESULT_FAIL);
                }
                cat1_module_state_set(MODULE_UPDATE_END);
            } else {
                log_debug("aic fota update failed(ret=%d), retry later!  %d\n", ret, msg[2]);
                smartbox_command_set_cmd_notify_cat1_update_result(CAT1_UPDATE_STATE_END, CAT1_UPDATE_RESULT_FAIL);
                ret = 1;
            }
            os_time_dly(20);  // 让出cpu给ble发送数据, 经验值
            goto __cat1_update_end;
        case CAT1_UPDATE_MSG_START:
            puts("CAT1_UPDATE_MSG_START\n");
            if (cat1_module_state_match(MODULE_UPDATE_START) && \
                (msg[2] == CAT1_UPDATE_RESULT_SUCC)) {
                // 通知升级开始
                smartbox_command_set_cmd_notify_cat1_update_result(CAT1_UPDATE_STATE_START, CAT1_UPDATE_RESULT_SUCC);
                if (cat1_update.timer == 0) {
                    cat1_update.timer = sys_timeout_add(NULL, cat1_module_update_data_trans_timeout, CAT1_DATA_TRANS_TIMEOUT);
                }
            } else {
                // 升级失败
                smartbox_command_set_cmd_notify_cat1_update_result(CAT1_UPDATE_STATE_END, CAT1_UPDATE_RESULT_FAIL);
                ret = 1;
                goto __cat1_update_end;
            }
            break;
        case CAT1_UPDATE_MSG_BT_DISCONNECT:
            puts("CAT1_UPDATE_MSG_BT_DISCONNECT\n");
            cat1_module_state_set(MODULE_UPDATE_BT_DISCONNECT);
            ret = 1;
            goto __cat1_update_end;
        case CAT1_UPDATE_MSG_STARTING:
            puts("CAT1_UPDATE_MSG_STARTING\n");
            if (cat1_update.update_flag != CAT1_UPDATE_BEGIN_FLAG) {
                cat1_update.update_flag = CAT1_UPDATE_BEGIN_FLAG;
                if (syscfg_write(CFG_CAT1_UPDATE_FLAG_ID, &cat1_update.update_flag, sizeof(cat1_update.update_flag)) != sizeof(cat1_update.update_flag)) {
                    log_error("%s update flag write err\n", __func__);
                }
            }

            if (cat1_update.timer) {  // 删除获取版本号的timer
                sys_timeout_del(cat1_update.timer);
                cat1_update.timer = 0;
            }

            if (cat1_module_update_call_disable() == 0) {
                log_error("cat1 disable call err\n");
                ret = 1;
                cat1_module_post_update_result(CAT1_UPDATE_MSG_START, CAT1_UPDATE_RESULT_FAIL);
                break;
            }

#if SMART_BOX_EN
            extern void app_smartbox_prepare_update_ex_flash(void);
            app_smartbox_prepare_update_ex_flash();
#endif
            // 注册大数据传输回调函数
            cat1_module_update_bt_recv_data_register(cat1_update.cb_priv, cat1_module_update_data_recv_callback);
            // 阻塞式等待电源启动完毕, 会先产生AIC_CTRL_STATE_FOTA状态回调, 再执行完该函数
            ret = cat1_module_fota_start(cat1_update.fw_pack_size, CAT1_MODULE_START_UPDATE_TIMEOUT);
            if (ret) {
                log_error("aic start update err %d, or start timeout\n", ret);
                cat1_module_post_update_result(CAT1_UPDATE_MSG_START, CAT1_UPDATE_RESULT_FAIL);
            } else {
                // 对于升级时断开蓝牙连接的情况，可能出现不会产生AIC_CTRL_STATE_FOTA回调的问题
                if (cat1_module_state_match(MODULE_UPDATE_STARTING)) {
                    cat1_module_state_set(MODULE_UPDATE_START);
                    cat1_module_post_update_result(CAT1_UPDATE_MSG_START, CAT1_UPDATE_RESULT_SUCC);
                }
            }
            break;
        default:
            puts("msg undefined!\n");
            break;
        }
    }

__cat1_update_end:
    if (ret) {
        cat1_module_state_set(MODULE_UPDATE_FAIL);
    } else {
        cat1_module_state_set(MODULE_UPDATE_SUCC);
    }
    if (cat1_update.timer) {
        sys_timeout_del(cat1_update.timer);
        cat1_update.timer = 0;
    }
    cat1_module_update_close();
    while (1) {
        os_time_dly(1000);
    }
}

u8 cat1_module_update_start(u32 size)
{
    u8 ret = 0;
    log_info("%s\n", __func__);

    // 前面的升级还未结束
    if (cat1_module_state_match(MODULE_UPDATE_DATA_TRANS) \
        || cat1_module_state_match(MODULE_UPDATE_START) \
        || cat1_module_state_match(MODULE_UPDATE_STARTING)) {
        log_error("err : prev update process in progress!\n");
        ret = 1;
        return ret;
    }

    cat1_module_state_set(MODULE_UPDATE_STARTING);
    cat1_update.fw_pack_size = size;

    // 创建升级任务, 用于处理升级结果, 升级失败等消息
    ret = task_create(cat1_module_update_task, NULL, THIS_TASK_NAME);
    if (ret != OS_NO_ERR) {
        log_error("cat1_module_update_task create err\n");
        cat1_module_state_clr(MODULE_UPDATE_STARTING);
        ret = 1;
        return ret;
    }
    cat1_module_post_update_result(CAT1_UPDATE_MSG_STARTING, CAT1_UPDATE_RESULT_SUCC);

    return ret;
}

static u32 cat1_module_update_data_packet(u8 *buf, u16 len)
{
    if (cat1_update.cat1_update_trans_buf == NULL) {
        cat1_update.cat1_update_trans_buf = (u8 *)zalloc(CAT1_UPDATE_TRANS_BUF_LEN);  // 在升级close时释放
    }
    memcpy(cat1_update.cat1_update_trans_buf + cat1_update.packet_offset, buf, len);
    cat1_update.packet_offset += len;
    if ((cat1_update.packet_offset >= (CAT1_UPDATE_TRANS_BUF_LEN - len)) || \
        ((cat1_update.w_offset + cat1_update.packet_offset) >= cat1_update.fw_pack_size)) {
        if (cat1_update.packet_offset > CAT1_UPDATE_TRANS_BUF_LEN) {
            log_debug(" %s, %d data overflow\n", __func__, __LINE__);
        }
        u32 tmp = cat1_update.packet_offset | BIT(31);
        cat1_update.packet_offset = 0;
        return tmp;
    }

    return cat1_update.packet_offset;
}


// len: 是总长度
// 本包数据的长度是data的前2byte
// data的前2byte之外的数据是本包数据
// return : priv[0] = 0: succ , priv[0] = 1: err
static void cat1_module_update_data_recv_callback(void *priv, u8 *data, int len)
{
    int ret = 0;
    u16 recv_len;

    if (!(cat1_module_state_match(MODULE_UPDATE_START) || cat1_module_state_match(MODULE_UPDATE_DATA_TRANS))) {
        // 还未准备好接受数据
        log_debug("not ready to recv update data !\n");
        ret = 1;
        goto __data_err;
    }

    if (cat1_module_state_match(MODULE_UPDATE_START)) {
        cat1_update.w_offset = 0;
        cat1_update.packet_offset = 0;
        if (len != cat1_update.fw_pack_size) {
            log_error("fw pack size err %d, %d\n", len, cat1_update.fw_pack_size);
            ret = 1;
            goto __data_err;
        }
        cat1_module_state_set(MODULE_UPDATE_DATA_TRANS);
    }

    memcpy(&recv_len, data, sizeof(recv_len));
    if (cat1_update.timer) {
        sys_timer_re_run(cat1_update.timer);
    }

    u32 packet_len = cat1_module_update_data_packet(data + sizeof(recv_len), recv_len);
    if (packet_len & BIT(31)) { // BIT(31) 表示组包完成
        packet_len = packet_len & (~BIT(31));
        ret = cat1_module_fota_trans_data(cat1_update.cat1_update_trans_buf, packet_len);
        if (ret != 0) {
            log_error("aic fota trans data err !\n");
            // 本包数据传输出错,需要出错处理
            ret = 1;
            goto __data_err;
        }
        cat1_update.w_offset += packet_len;
    }

__data_err:
    ((u8 *)priv)[0] = ret;
}

static void cat1_module_update_reboot(void)
{
    // 关闭蓝牙等其它外设, 直接复位会导致APP没有识别到蓝牙断开
    user_send_cmd_prepare(USER_CTRL_POWER_OFF, 0, NULL);
    ble_module_enable(0);

    watch_sensor_close();
    cat1_close();

    cpu_reset();
}

static void cat1_module_update_reset(u8 result)
{
    if (!UPDATE_SUPPORT_DEV_IS_NULL()) {
        UPDATA_PARM *p = UPDATA_FLAG_ADDR;

        memset(p, 0x00, sizeof(UPDATA_PARM));
        if (result && (cat1_update.update_flag == CAT1_UPDATE_END_FLAG)) {
            p->parm_result = UPDATA_SUCC;
        } else {
            p->parm_result = UPDATA_DEV_ERR;
        }
        p->magic = BT_UPDATA;
        p->parm_crc = CRC16(((u8 *)p) + 2, sizeof(UPDATA_PARM) - 2);

        cat1_module_update_reboot();
    }
}

static void cat1_module_update_close(void)
{
    u8 result = cat1_module_state_match(MODULE_UPDATE_SUCC);
    cat1_update.fw_pack_size = 0;
    cat1_update.w_offset = 0;
    cat1_update.packet_offset = 0;

    log_info("%s\n", __func__);

    if (cat1_module_fota_close()) {
        log_info("cat1_module_fota_close err");
    }

    // 升级成功
    if (result) {
        // 清除版本号
        u16 ver_len = cat1_module_get_version_total_len();
        u8 *tmp_ver_p = (u8 *)zalloc(ver_len);
        if (tmp_ver_p) {
            if (syscfg_write(CFG_CAT1_UPDATE_CAT1_VER, tmp_ver_p, ver_len) != ver_len) {
                log_error("%s version write err\n", __func__);
            }
        } else {
            log_error("%s version write err\n", __func__);
        }
        free(tmp_ver_p);

        // 清除升级中标记
        cat1_update.update_flag = CAT1_UPDATE_END_FLAG;
        if (syscfg_write(CFG_CAT1_UPDATE_FLAG_ID, &cat1_update.update_flag, sizeof(cat1_update.update_flag)) != sizeof(cat1_update.update_flag)) {
            log_error("%s  %d update flag write err\n", __LINE__, __func__);
        }
    }

    // 1, 取消注册大数据传输回调函数
    cat1_module_update_bt_recv_data_register(NULL, NULL);

    // 2, 4G模块进入poweroff
    if (cat1_module_get_power_state() != CAT1_POWER_STATE_POWEROFF) {
        cat1_ctrl_poweroff();
    }

    // 3, 清除状态
    cat1_module_state_clr(MODULE_UPDATE_STATE_MAX);
    cat1_module_state_clr(MODULE_UPDATE_RESULT_MAX);
    cat1_module_state_clr(MODULE_UPDATE_BT_DISCONNECT);

    extern void smartbox_update_flag_clear(void);
    smartbox_update_flag_clear();

    // // 4, 删除升级任务
    if (os_task_get_handle(THIS_TASK_NAME)) {
        cat1_module_update_event_to_user(result, 0);
    }
}

void cat1_module_update_task_del(u8 result)
{
    log_info("%s\n", __func__);
    int ret = task_kill(THIS_TASK_NAME);
    if (ret != OS_NO_ERR) {
        log_debug("task del err %s, %d\n", THIS_TASK_NAME, ret);
    } else {
        log_debug("task del ok %s, %d\n", THIS_TASK_NAME, ret);
    }
    // 升级完成复位
    cat1_module_update_reset(result);
}
#endif

