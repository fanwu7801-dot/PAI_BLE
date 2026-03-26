#include "tts_main.h"
#include "cJSON.h"
#include "circular_buf.h"
#include "stdlib.h"
#include "app_config.h"
#include "authentication.h"
#include "ifly_dec_file.h"
#include "sparkdesk_main.h"
#include "websocket_define.h"
#include "ifly_socket.h"
#include "authentication.h"
#include "stdlib.h"
#include "app_config.h"
#include "app_task.h"
#include "system/timer.h"
#include "app_main.h"
#include "init.h"
#include "key_event_deal.h"
#include "device/device.h"
#include "app_power_manage.h"
#include "btstack/avctp_user.h"
#include "asm/charge.h"
#include "cJSON.h"
#include "media/includes.h"
#include "audio_config.h"
#include "circular_buf.h"
#include "ui/ui_api.h"
#include "ifly_common.h"

#if TCFG_IFLYTEK_ENABLE

#define LOG_TAG_CONST       NET_IFLY
#define LOG_TAG             "[IFLY_TTS]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
#define LOG_CLI_ENABLE
#include "debug.h"

#define TTS_AUDIO_SAVE_TEST     0
#if TTS_AUDIO_SAVE_TEST
static FILE *save_file = NULL;
#endif

#ifdef TCFG_IFLYTEK_APP_ID
#define APP_ID       			TCFG_IFLYTEK_APP_ID
#else
#define APP_ID       			"123"
#endif

#define RECV_LAST_FRAME  		2

#define IFLY_TTS_PKG_MAX        (1024 * 10)   //经测试，最长一包下发数据的长度为8500bytes左右
#define IFLY_TTS_CBUF_LEN       (IFLY_TTS_PKG_MAX + 4096)
#define TTS_TIMEOUT_TIME  		8  //防止tts在播放过程中收不到消息而卡住，8s没接收消息，就关闭任务


typedef enum {
    IFLY_TTS_STATUS_NULL = 0,
    IFLY_TTS_STATUS_START,	// 启动
    IFLY_TTS_STATUS_SEND,	// 数据已经发给socket
    IFLY_TTS_STATUS_RECV,	// 有接受到数据
    IFLY_TTS_STATUS_RECV_END,// 接受完成
    IFLY_TTS_STATUS_PLAY_END,// 播放完
    IFLY_TTS_STATUS_RECV_ERROR,// 接受错误
    IFLY_TTS_STATUS_EXIT,	// 已经退出
} ifly_tts_status;

struct tts_info_t {
    u8 force_stop; 			// 强制结束
    u8 recv_finish;         // 接受完毕
    u8  tts_to_cnt;			// 超时计数
    u16 tts_timer;			// 超时用的timer ID
    cbuffer_t dec_cbuf;     // 用于tts音频播放的cbuf
    char *dec_out_buf;
    ifly_tts_status status;
    ifly_tts_param *param;
};

static struct tts_info_t tts_info;
static struct ifly_websocket_struct tts_socket;
static void *ifly_tts_usr_task = "app_core";

extern int mbedtls_base64_encode(unsigned char *dst, size_t dlen, size_t *olen, const unsigned char *src, size_t slen);
extern int mbedtls_base64_decode(unsigned char *dst, size_t dlen, size_t *olen, const unsigned char *src, size_t slen);

static int test_read(void *priv, void *buf, u32 len, u8 tmp_flag, int tmp_offset)
{
    int rlen = 0;

    if (tts_info.status > IFLY_TTS_STATUS_RECV_END) {
        return 0;
    }
    if (tmp_flag) {
        // 格式检查时读数，不保留读取记录
        void *rbuf = cbuf_read_alloc(&tts_info.dec_cbuf, &rlen);
        if (rlen) {
            if (rlen > len) {
                rlen = len;
            }
            memcpy(buf, rbuf, rlen);
        }
    } else {
        // 正常读数
        rlen = cbuf_read(&tts_info.dec_cbuf, buf, len);
        if (rlen != len) {
            if (tts_info.status >= IFLY_TTS_STATUS_RECV_END) {
                rlen = cbuf_get_data_len(&tts_info.dec_cbuf);
                rlen = cbuf_read(&tts_info.dec_cbuf, buf, rlen);
            }
        }
    }
    if (tts_info.status < IFLY_TTS_STATUS_RECV_END) {
        if (rlen == 0) {
            rlen = -1; // 没结束时返回-1挂起解码，不结束解码
        }
    }
    return rlen;
}

static void ifly_net_tts_stop(u32 dat)
{
    int to = dat;
    while (tts_info.dec_out_buf && (tts_info.status < IFLY_TTS_STATUS_PLAY_END)) {
        if (cbuf_get_data_len(&tts_info.dec_cbuf) == 0) {
            break;
        }
        os_time_dly(1);
        if (to < 10) {
            break;
        }
        to -= 10;
    }
    if (tts_info.status < IFLY_TTS_STATUS_PLAY_END) {
        tts_info.status = IFLY_TTS_STATUS_PLAY_END;
    }
    ifly_net_tts_dec_close();
}

static void ifly_net_tts_start(u32 dat)
{
    log_info("ifly_net_tts_start");
    ifly_net_tts_dec_open(NULL, test_read);
}

static void ifly_tts_dec_func(void(*func)(u32 dat), int to)
{
    int argv[3];
    argv[0] = (int)func;
    argv[1] = 1;
    argv[2] = to;
    int ret = os_taskq_post_type(ifly_tts_usr_task, Q_CALLBACK, 3, argv);
    if ((ret == OS_ERR_POST_NULL_PTR) || (ret == OS_TASK_NOT_EXIST)) {
        os_taskq_post_type("app_core", Q_CALLBACK, 3, argv);
    }
}

//tts数据模块
char *ifly_tts_format_text_data(void)
{
    char *data_str = NULL;
    int out_len = 0;
    cJSON *cjson_test = NULL;
    cJSON *cjson_common = NULL;
    cJSON *cjson_business = NULL;
    cJSON *cjson_data = NULL;

    char *buf = malloc(MAX_SPARKDESK_LEN);
    mbedtls_base64_encode(buf, MAX_SPARKDESK_LEN, &out_len, tts_info.param->text_res, strlen(tts_info.param->text_res) + 1);
//定义最长回答

    cjson_test = cJSON_CreateObject();
    cjson_common = cJSON_CreateObject();
    cjson_business = cJSON_CreateObject();
    cjson_data = cJSON_CreateObject();

    cJSON_AddStringToObject(cjson_common, "app_id", APP_ID);
    cJSON_AddItemToObject(cjson_test, "common", cjson_common);

    cJSON_AddStringToObject(cjson_business, "aue", "lame");
    cJSON_AddNumberToObject(cjson_business, "sfl", 1);
    cJSON_AddStringToObject(cjson_business, "auf", "audio/L16;rate=16000");
    cJSON_AddStringToObject(cjson_business, "vcn", "xiaoyan");
    cJSON_AddStringToObject(cjson_business, "tte", "UTF8");
    cJSON_AddItemToObject(cjson_test, "business", cjson_business);

    cJSON_AddNumberToObject(cjson_data, "status", 2);
    cJSON_AddStringToObject(cjson_data, "text", buf);
    cJSON_AddItemToObject(cjson_test, "data", cjson_data);

    data_str = cJSON_Print(cjson_test);
    free(buf);

    cJSON_Delete(cjson_test);

    log_info("tts content:%s\n", data_str);

    return data_str;
}

static void tts_recv_timeout_timer(void)
{
    if (tts_info.tts_to_cnt < TTS_TIMEOUT_TIME) {
        tts_info.tts_to_cnt++;
        if (tts_info.tts_to_cnt == TTS_TIMEOUT_TIME) {
            log_info("tts timeout!");
            if (tts_info.status < IFLY_TTS_STATUS_RECV_ERROR) {
                tts_info.status = IFLY_TTS_STATUS_RECV_ERROR;
            }
        }
    }
}

//tts解析播放模块
static void ifly_tts_recv_cb(u8 *j_str, u32 len, u8 type)
{
    tts_info.tts_to_cnt = 0;   //收到数据重置定时器

    if (tts_info.force_stop) {
        return;
    }
    if (tts_info.status >= IFLY_TTS_STATUS_RECV_END) {
        return;
    }

    cJSON *cjson_root = cJSON_Parse(j_str);
    char *audio = NULL;
    char *res_tts = NULL;
    if (cjson_root == NULL) {
        log_error("cjson error...\r\n");
        if (tts_info.status <= IFLY_TTS_STATUS_RECV_END) {
            tts_info.status = IFLY_TTS_STATUS_RECV_ERROR;
        }
        return;
    }

    cJSON *cjson_data = cJSON_GetObjectItem(cjson_root, "data");
    cJSON *cjson_status = cJSON_GetObjectItem(cjson_data, "status");
    cJSON *cjson_audio = cJSON_GetObjectItem(cjson_data, "audio");

    audio = cJSON_Print(cjson_audio);
    if (!audio) {
        log_error("audio is null!!\n");
        tts_info.status = IFLY_TTS_STATUS_RECV_ERROR;
        return;
    }

    str_remove_quote(audio, strlen(audio));

    res_tts = malloc(IFLY_TTS_PKG_MAX);
    if (!res_tts) {
        log_error("malloc fail!!\n");
        tts_info.status = IFLY_TTS_STATUS_RECV_ERROR;
        return;
    }
    int olen = 0;
    int ret = mbedtls_base64_decode(res_tts, IFLY_TTS_PKG_MAX, &olen, audio, strlen(audio));
    if (ret || (olen > IFLY_TTS_PKG_MAX)) {
        log_error("mbedtls_base64_decode fail!!\n");
        tts_info.status = IFLY_TTS_STATUS_RECV_ERROR;
        return;
    }


#if TTS_AUDIO_SAVE_TEST
    if (save_file) {
        int wlen = fwrite(save_file, res_tts, olen);
        if (wlen != olen) {
            log_error("save file err: %d, %d\n", wlen, olen);
        }
    }
#endif


    //若cbuf满了，需要先while住，等待播放
    int to = 1500;
    while (!cbuf_write(&tts_info.dec_cbuf, res_tts, olen)) {
        if (tts_info.force_stop) {
            break;
        }
        os_time_dly(1);
        if (to < 10) {
            log_info(">>>>>>>>>>>>>>>>>>>>>>>cbuf full");
            break;
        }
        to -= 10;
    }

    if (tts_info.status < IFLY_TTS_STATUS_RECV) {
        tts_info.status = IFLY_TTS_STATUS_RECV;
        log_info("tts start play!\n");

        ifly_tts_dec_func(ifly_net_tts_start, 0);
        tts_info.param->event_cb(IFLY_TTS_EVT_PLAY_START, tts_info.param);
    }
    if (cjson_status->valueint == RECV_LAST_FRAME) {
        log_info("tts last frame!\n");
        tts_info.status = IFLY_TTS_STATUS_RECV_END;
        tts_info.recv_finish = 1;

        ifly_tts_dec_func(ifly_net_tts_stop, 3000);
        tts_info.param->event_cb(IFLY_TTS_EVT_PLAY_STOP, tts_info.param);
    }

    cJSON_free(audio);
    cJSON_Delete(cjson_root);
    free(res_tts);

    ifly_net_tts_dec_resume(NULL);

}

static bool ifly_tts_get_send(u8 **buf, u32 *len)
{
    if (tts_info.force_stop) {
        log_info("tts task kill!\n");
        return false;
    }
    if (tts_info.status >= IFLY_TTS_STATUS_PLAY_END) {
        log_info("tts task kill!\n");
        return false;
    }
    if (tts_info.status < IFLY_TTS_STATUS_SEND) {
        tts_info.status = IFLY_TTS_STATUS_SEND;
        char *input_src_json = ifly_tts_format_text_data();
        if (input_src_json == NULL) {
            log_error("get json err \n");
            return false;
        }
        *buf = input_src_json;
        *len = strlen(input_src_json);
        return true;
    }
    os_time_dly(2);
    return true;
}

static int ifly_tts_event_cb(ifly_socket_event_enum evt, void *param)
{
    switch (evt) {
    case IFLY_SOCKET_EVT_SEND_OK:
        if (!tts_info.tts_timer) {
            tts_info.tts_timer = sys_timer_add(NULL, tts_recv_timeout_timer, 1000);
        }
        cJSON_free(param);
        break;
    case IFLY_SOCKET_EVT_SEND_ERROR:
        cJSON_free(param);
        break;
    case IFLY_SOCKET_EVT_INIT_OK:
        break;
    case IFLY_SOCKET_EVT_INIT_ERROR:
    case IFLY_SOCKET_EVT_HANSHACK_ERROR:
    case IFLY_SOCKET_EVT_ACCIDENT_END:
    case IFLY_SOCKET_EVT_END:
    case IFLY_SOCKET_EVT_FORCE_END:
        if (ifly_net_tts_dec_check_run()) { // 正常结束会自动关闭，这里为异常或者强制关闭处理
            int to = 10;
            if (evt == IFLY_SOCKET_EVT_ACCIDENT_END) {     //如果是意外退出，先播放完cbuf的内容
                to = 300;
            } else {
                if (tts_info.status < IFLY_TTS_STATUS_RECV_ERROR) {
                    tts_info.status = IFLY_TTS_STATUS_RECV_ERROR;
                }
            }
            ifly_tts_dec_func(ifly_net_tts_stop, to * 10);
            while (ifly_net_tts_dec_check_run()) {
                os_time_dly(1);
                to -= 1;
                if (to <= 0) {
                    break;
                }
                if (tts_info.force_stop) {
                    tts_info.status = IFLY_TTS_STATUS_RECV_ERROR;
                    break;
                }
            }
        }
        if ((evt != IFLY_SOCKET_EVT_END) && (evt != IFLY_SOCKET_EVT_FORCE_END)) {
            if (!tts_info.recv_finish) {
                tts_info.param->event_cb(IFLY_TTS_EVT_PLAY_FAIL_STOP, tts_info.param);
            }
        }
        break;
    case IFLY_SOCKET_EVT_EXIT:
        tts_info.status = IFLY_TTS_STATUS_EXIT;
        tts_info.param->event_cb(IFLY_TTS_EVT_EXIT, tts_info.param);
        if (tts_socket.auth) {
            free(tts_socket.auth);
            tts_socket.auth = NULL;
        }
        if (tts_info.tts_timer) {
            sys_timer_del(tts_info.tts_timer);
            tts_info.tts_timer = 0;
        }
        if (tts_info.dec_out_buf) {
            free(tts_info.dec_out_buf);
            tts_info.dec_out_buf = NULL;;
        }
        break;
    default:
        break;
    }
    return 0;
}

bool ifly_tts_start(ifly_tts_param *param)
{
    memset(&tts_info, 0, sizeof(tts_info));
    memset(&tts_socket, 0, sizeof(struct ifly_websocket_struct));

    tts_info.param = param;

    tts_info.dec_out_buf = malloc(sizeof(char) * IFLY_TTS_CBUF_LEN);
    cbuf_init(&tts_info.dec_cbuf, tts_info.dec_out_buf, IFLY_TTS_CBUF_LEN);

    tts_socket.auth = ifly_authentication("wss://tts-api.xfyun.cn/v2/tts",
                                          "tts-api.xfyun.cn",
                                          "GET /v2/tts HTTP/1.1");
    if (!tts_socket.auth) {
        free(tts_info.dec_out_buf);
        tts_info.dec_out_buf = NULL;;
        tts_info.param->event_cb(IFLY_TTS_EVT_PLAY_FAIL_STOP, tts_info.param);
        return false;
    }
    tts_socket.task_name = "ifly_tts";
    tts_socket.socket_mode = WEBSOCKET_MODE;
    tts_socket.recv_cb = ifly_tts_recv_cb;
    tts_socket.get_send = ifly_tts_get_send;
    tts_socket.event_cb = ifly_tts_event_cb;

    tts_info.status = IFLY_TTS_STATUS_START;

    //创建链接
    bool ret = ifly_websocket_client_create(&tts_socket);
    if (ret == false) {
        tts_info.status = IFLY_TTS_STATUS_NULL;
        free(tts_socket.auth);
        tts_socket.auth = NULL;
        free(tts_info.dec_out_buf);
        tts_info.dec_out_buf = NULL;;
    }

#if TTS_AUDIO_SAVE_TEST
    if (save_file) {
        fclose(save_file);
        save_file = NULL;
    }
    save_file = fopen("storage/sd0/C/sf.mp3", "w+");
    if (!save_file) {
        log_error("fopen err \n\n");
    }
#endif

    return ret;
}


void ifly_tts_stop(u8 force_stop, u32 to_ms)
{
    log_info("tts close!\n");
    tts_info.force_stop = force_stop;
    while (tts_socket.auth) { // 结束时auth会自动释放
        os_time_dly(1);
        if (to_ms <= 10) {
            break;
        }
        to_ms -= 10;
    }
    if (to_ms < 1000) {
        to_ms = 1000;
    }
    tts_info.force_stop = 1;
    ifly_websocket_client_release(&tts_socket, to_ms);


#if TTS_AUDIO_SAVE_TEST
    if (save_file) {
        fclose(save_file);
        save_file = NULL;
    }
#endif
}

bool ifly_tts_is_work()
{
    if ((tts_info.status != IFLY_TTS_STATUS_NULL) && (tts_info.status != IFLY_TTS_STATUS_EXIT)) {
        return true;
    }
    return false;
}
#endif

