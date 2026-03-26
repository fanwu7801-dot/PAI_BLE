/*****************************************************************
>file name : iflytek_asr.c
>create time : Sun 10 Mar 2023 10:00:58 AM CST
>科大讯飞离线语音识别平台
*****************************************************************/
#include "system/includes.h"
#include "../smart_voice.h"
#include "../voice_mic_data.h"
#include "../vad_mic.h"
#include "../kws_event.h"
#include "app_config.h"
#include "clock_cfg.h"
#include "w_ivw.h"


#define LOG_TAG_CONST       IFLY_ASR
#define LOG_TAG     		"[IFLY_ASR]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
// #define LOG_DUMP_ENABLE
#define LOG_CLI_ENABLE
#include "debug.h"


#if ((TCFG_SMART_VOICE_ENABLE) && (TCFG_AUDIO_ASR_DEVELOP == ASR_CFG_IFLYTEK))


#define SMART_VOICE_SAMPLE_RATE             16000
#define VOICE_DATA_BUFFER_SIZE              (2 * 1024)
#define ASR_FRAME_SAMPLES                   160 //语音识别帧长（采样点） 科大讯飞官方demo参考大小

#define INTERN_PATH                         "mnt/sdfile/res/"
#define MLP_OUTPUT_PATH                     INTERN_PATH"mlp_output.bin"
#define FILLER_KEYWORD_PATH                 INTERN_PATH"filler_key.bin"

struct iflytek_platform_asr_context {
    void *mic;
    s16 data[ASR_FRAME_SAMPLES];
    u8 data_enable;
};

static struct iflytek_platform_asr_context *__this = NULL;
static ivChar *callback_param = "ivw engine from iflytek";
static WIVW_INST ivw_inst = NULL;


static const int iflytek_asr_events[] = {
    IFLYTEK_EVENT_XIOWEI,
    IFLYTEK_EVENT_OPEN_NOTIFICATION,
    IFLYTEK_EVENT_OPEN_STEP,
    IFLYTEK_EVENT_BLOOD_PRESSURE_MEASUREMENT,
    IFLYTEK_EVENT_OPEN_GAME,
    IFLYTEK_EVENT_POWER_SAVING_MODE,
    IFLYTEK_EVENT_STOP_SPORT,
    IFLYTEK_EVENT_KEEP_MOVING,
    IFLYTEK_EVENT_PAUSE_SPORT,
    IFLYTEK_EVENT_BACK_TO_DIAL,
    IFLYTEK_EVENT_OPEN_RIDE_CODE,
    IFLYTEK_EVENT_OPEN_COLLECTION_CODE,
    IFLYTEK_EVENT_OPEN_PAYMENT_CODE,
    IFLYTEK_EVENT_OPEN_4G_NETWORK,
    IFLYTEK_EVENT_CLOSE_4G_NETWORK,
};

/*
 * param的格式为 {"rlt":[{"istart":200,"iresid":0,"iduration":211,"nfillerscore":0,"nkeywordscore":282731,
 *                "ncm":2651,"ncmKeyword":1443,"ncmFiller":-1208,"ncmThreshold":1200,"keyword":"xiao3 wei1 tong2 xue2"}]}
 */
int smart_voice_iflytek_asr_event_handler(const char *param)
{
    if (!param) {
        return false;
    }

    int event = KWS_EVENT_NULL;
    struct sys_event e;
    int index = 0;
    char *id_str = "iresid";
    char *found =  strstr(param, id_str);

    if (found) {
        found = found + strlen(id_str) + 2;
        for (int i = 0; found[i] != ','; i++) {
            index = index * 10 + found[i] - '0';
        }

        if (index >= ARRAY_SIZE(iflytek_asr_events)) {
            return false;
        }
    } else {
        return false;
    }

    event = iflytek_asr_events[index];
    if (event == KWS_EVENT_NULL) {
        return false;
    }

    e.type = SYS_KEY_EVENT;
    e.u.key.event = event;
    e.u.key.value = 'V';
    e.u.key.type = KEY_DRIVER_TYPE_VOICE;//区分按键类型
    sys_event_notify(&e);

    return true;
}

//IFLYTEK唤醒回调
static ivInt IvwCallBackWakeup1(const ivChar *param, void *pUserParam)
{
    log_info("IvwCallBackWakeup1 = %s \n\n", param);
    smart_voice_iflytek_asr_event_handler(param);
    return 0;
}

//算法引擎打开函数
static int iflytek_asr_core_open()
{
    int ret;
    FILE *fp_mlp_res;
    FILE *fp_filler_res;
    struct vfs_attr attr_mlp_res;
    struct vfs_attr attr_filler_res;
    u32 *flash_addr_mlp_res;
    u32 *flash_addr_filler_res;

    log_info("iflytek asr open");
    mem_stats();
    ivw_inst = malloc(wIvwGetInstSize());
    log_debug("ivw_inst:%x,  wIvwGetInstSize():%d", ivw_inst,  wIvwGetInstSize());
    if (!ivw_inst) {
        goto __err;
    }

    //read mlp_res
    fp_mlp_res = fopen(MLP_OUTPUT_PATH, "r");
    if (!fp_mlp_res) {
        log_debug("MLP open %s error", MLP_OUTPUT_PATH);
        goto __err;
    }

    fget_attrs(fp_mlp_res, &attr_mlp_res);
    flash_addr_mlp_res = attr_mlp_res.sclust;
    fclose(fp_mlp_res);

    //read filler_keyword res
    fp_filler_res = fopen(FILLER_KEYWORD_PATH, "r");
    if (!fp_filler_res) {
        log_debug("FILLER open %s error", FILLER_KEYWORD_PATH);
        goto __err;
    }
    fget_attrs(fp_filler_res, &attr_filler_res);
    flash_addr_filler_res = attr_filler_res.sclust;
    fclose(fp_filler_res);

    //直接访问科大讯飞资源文件内部flash地址来运行
    ret = wIvwCreate(ivw_inst, flash_addr_mlp_res, flash_addr_filler_res);
    if (ret) {
        log_debug("wIvwCreate error:%d", ret);
        goto __err;
    }

    ret = wIvwRegisterCallBacks(ivw_inst, CallBackFuncNameWakeUp, IvwCallBackWakeup1, callback_param);
    if (ret) {
        log_debug("wIvwRegisterCallBacks error:%d", ret);
        goto __err;
    }

    ret = wIvwStart(ivw_inst);
    if (ret) {
        log_debug("wIvwStart error:%d", ret);
        goto __err;
    }

    mem_stats();
    log_info("iflytek asr open succ!");
    return true;

__err:
    log_info("iflytek asr open fail!");
    if (ivw_inst) {
        free(ivw_inst);
        ivw_inst = NULL;
    }
    return false;
}

//算法引擎关闭函数
static int iflytek_asr_core_close()
{
    int ret;
    log_info("iflytek asr close");
    if (ivw_inst) {
        ret = wIvwStop(ivw_inst);
        log_debug("wIvwStop error:%d", ret);
        ret = wIvwDestroy(ivw_inst);
        log_debug("wIvwDestroy error:%d", ret);
        free(ivw_inst);
        ivw_inst = NULL;
    }
    mem_stats();
    log_info("iflytek asr close succ!");
    return true;
}

/*
 * 算法引擎数据处理
 */
static int iflytek_asr_core_data_handler(void *data, int len)
{
    int err = ASR_CORE_STANDBY;

    if (ivw_inst) {
        err = wIvwWrite(ivw_inst, data, len);
        if (err != 0) {
            printf("wIvwWrite:%d", err);
            err =  ASR_CORE_STANDBY;
        } else {
            err = ASR_CORE_WAKEUP;
        }
    }
    return err;
}

/*
*********************************************************************
*       iflytek asr data handler
* Description: 科大讯飞平台平台语音识别数据处理
* Arguments  : asr - 语音识别数据管理结构
* Return	 : 0 - 处理成功， 非0 - 处理失败.
* Note(s)    : 该函数通过读取mic数据送入算法引擎完成语音帧.
*********************************************************************
*/
static int iflytek_asr_data_handler(struct iflytek_platform_asr_context *asr)
{
    int err = ASR_CORE_STANDBY;

    if (!asr->mic) {
        err = ASR_CORE_STANDBY;
        goto __err;
    }


    int len = voice_mic_data_read(asr->mic, asr->data, sizeof(asr->data));
    if (len < sizeof(asr->data)) {
        err = ASR_CORE_STANDBY;
        goto __err;
    }

    err = iflytek_asr_core_data_handler(asr->data, sizeof(asr->data));

    return err;

__err:
    return err;
}

/*
*********************************************************************
*       user_asr core handler
* Description: 用户自定义语音识别处理
* Arguments  : priv - 语音识别私有数据
*              taskq_type - TASK消息类型
*              msg  - 消息存储指针(对应自身模块post的消息)
* Return	 : None.
* Note(s)    : 音频平台资源控制以及ASR主要识别算法引擎.
*********************************************************************
*/
int audio_smart_voice_core_handler(void *priv, int taskq_type, int *msg)
{
    struct iflytek_platform_asr_context *asr = (struct iflytek_platform_asr_context *)priv;
    int err = ASR_CORE_STANDBY;

    if (taskq_type != OS_TASKQ) {
        return err;
    }
    log_debug("iflytek task msg:%d", msg[0]);

    switch (msg[0]) {
    case SMART_VOICE_MSG_MIC_OPEN: //语音识别打开 - MIC打开
        if (asr->data_enable == 0) {
            asr->mic = voice_mic_data_open(msg[1], msg[2], msg[3]);
            if (!asr->mic) {
                log_info("smart voice  mic open failed.\n");
                err = ASR_CORE_STANDBY;
                break;
            }
        }
        break;
    case SMART_VOICE_MSG_SWITCH_SOURCE://这里进行MIC的数据源切换 （主系统MIC或VAD MIC）
        if (asr->mic) {
            voice_mic_data_clear(asr->mic);
            voice_mic_data_switch_source(asr->mic, msg[1], msg[2], msg[3]);
        }
        break;
    case SMART_VOICE_MSG_MIC_CLOSE: //语音识别关闭 - MIC关闭，算法引擎关闭
        if (asr->data_enable == 1) {
            voice_mic_data_close(asr->mic);
            asr->mic = NULL;
            asr->data_enable = 0;
            iflytek_asr_core_close();
            err = ASR_CORE_STANDBY;
        }
        os_sem_post((OS_SEM *)msg[1]);
        break;
    case SMART_VOICE_MSG_WAKE:
        err = ASR_CORE_WAKEUP;
        asr->data_enable = 1;
        break;
    case SMART_VOICE_MSG_STANDBY:
        asr->data_enable = 0;
        err = ASR_CORE_STANDBY;
        if (asr->mic) {
            voice_mic_data_clear(asr->mic);
        }
        break;
    case SMART_VOICE_MSG_DMA: //MIC通路数据DMA消息
        err = ASR_CORE_WAKEUP;
        break;
    default:
        break;
    }

    if (asr->data_enable) {
        err = iflytek_asr_data_handler(asr);
        err = err ? ASR_CORE_STANDBY : ASR_CORE_WAKEUP;
    }
    return err;
}

int __iflytek_platform_asr_open(u8 mic, int buffer_size)
{
    int err;

    if (__this) {
        smart_voice_core_post_msg(4, SMART_VOICE_MSG_SWITCH_SOURCE, mic, buffer_size, SMART_VOICE_SAMPLE_RATE);
        err = true;
        return err;
    }

    struct iflytek_platform_asr_context *asr = (struct iflytek_platform_asr_context *)zalloc(sizeof(struct iflytek_platform_asr_context));

    if (!asr) {
        err = false;
        goto __err;
    }

    err = iflytek_asr_core_open();
    if (err == false) {
        goto __err;
    }

    err = smart_voice_core_create(asr);
    if (err == -EINVAL) {
        err = false;
        goto __err;
    } else {
        err = true;
    }

    clock_add_set(SMART_VOICE_CLK);
    smart_voice_core_post_msg(4, SMART_VOICE_MSG_MIC_OPEN, mic, buffer_size, SMART_VOICE_SAMPLE_RATE);
    __this = asr;

    return err;

__err:
    iflytek_asr_core_close();
    if (asr) {
        free(asr);
    }
    return err;
}

int audio_smart_voice_detect_open(u8 value)
{
    //return __iflytek_platform_asr_open(VOICE_VAD_MIC, VOICE_DATA_BUFFER_SIZE);
    return __iflytek_platform_asr_open(VOICE_MCU_MIC, VOICE_DATA_BUFFER_SIZE);
}

void audio_smart_voice_detect_close(void)
{
    if (__this) {
        OS_SEM *sem = (OS_SEM *)malloc(sizeof(OS_SEM));
        os_sem_create(sem, 0);
        smart_voice_core_post_msg(2, SMART_VOICE_MSG_MIC_CLOSE, (int)sem);
        os_sem_pend(sem, 0);
        free(sem);
        clock_remove_set(SMART_VOICE_CLK);
        smart_voice_core_free();
        free(__this);
        __this = NULL;
    }
}

int audio_smart_voice_detect_init(struct vad_mic_platform_data *mic_data)
{
    //lp_vad_mic_data_init(mic_data);
    audio_smart_voice_detect_open(NULL);
    return 0;
}

int audio_smart_voice_phone_call_start(void)
{
    if (__this) {
        //通话语音识别，由LP VAD的MIC切到系统主MIC进行识别
        smart_voice_core_post_msg(4, SMART_VOICE_MSG_SWITCH_SOURCE, VOICE_MCU_MIC, VOICE_DATA_BUFFER_SIZE, SMART_VOICE_SAMPLE_RATE);
        return 0;
    }

    __iflytek_platform_asr_open(VOICE_MCU_MIC, VOICE_DATA_BUFFER_SIZE);
    return 0;
}

int audio_smart_voice_phone_call_close(void)
{
    if (!__this) {
        return 0;
    }

    smart_voice_core_post_msg(4, SMART_VOICE_MSG_SWITCH_SOURCE, VOICE_MCU_MIC, VOICE_DATA_BUFFER_SIZE, SMART_VOICE_SAMPLE_RATE);
    return 0;
}

int audio_smart_voice_check_status(void)
{
    return (__this ? true : false);
}


#if !TCFG_VAD_LP_CLOSE
static u8 iflytek_asr_core_idle_query()
{
    if (__this) {
        return !(__this->data_enable);
    } else {
        return 1;
    }
}

REGISTER_LP_TARGET(iflytek_asr_core_lp_target) = {
    .name = "iflytek_asr_core",
    .is_idle = iflytek_asr_core_idle_query,
};
#endif

#endif


