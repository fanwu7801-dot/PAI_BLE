#include "string.h"
#include "vad_main.h"
#include "ifly_dec_file.h"
#include "sparkdesk_main.h"
#include "authentication.h"
#include "websocket_define.h"
#include "ifly_socket.h"
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
#include "audio_config.h"
#include "third_party_profile/interface/app_protocol_common.h"
#include "circular_buf.h"
#include "ui/ui_api.h"
#include "ifly_common.h"

#if TCFG_IFLYTEK_ENABLE

#define LOG_TAG_CONST       NET_IFLY
#define LOG_TAG             "[IFLY_VAD]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
#define LOG_CLI_ENABLE
#include "debug.h"


#ifdef TCFG_IFLYTEK_APP_ID
#define APP_ID       				TCFG_IFLYTEK_APP_ID
#else
#define APP_ID       				"123"
#endif

#if TCFG_ENC_SPEEX_ENABLE
#define AI_AUDIO_CODING_TYPE    	AUDIO_CODING_SPEEX // 编码格式
#else
#error "ONLY SUPPORT SPEEX"
#endif

#define AI_AUDIO_CODING_SR          16000 // 采样率。和audio_mic_enc_open()函数中的对应

#define PCM_OUT_BUF_LEN				(AI_AUDIO_CODING_SR*2/1000 * 30)

#define SPEEX_SIZE   				42

#define AUDIO_LEN    				168
#define BASE63_AUDIO_LEN    		256

#define STATUS_FIRST_FRAME      	0
#define STATUS_CONTINUE_FRAME   	1
#define STATUS_LAST_FRAME       	2
#define STATUS_NED_FRAME       		3

#define HEART_BEAT_REQ              "client ping"    // 服务器下发的心跳保持请求

typedef enum {
    IFLY_VAD_STATUS_NULL = 0,
    IFLY_VAD_STATUS_START,	// 启动
    IFLY_VAD_STATUS_PCM_START,	// 音频启动发数
    IFLY_VAD_STATUS_SENDING,	// 音频数据发数中
    IFLY_VAD_STATUS_SEND_END,	// 音频数据发数完毕
    IFLY_VAD_STATUS_RECV,		// 有接受到数据
    IFLY_VAD_STATUS_RECV_END,	// 接受完成
    IFLY_VAD_STATUS_RECV_ERROR,	// 接受错误
    IFLY_VAD_STATUS_EXIT,		// 已经退出
} ifly_vad_status;

struct vad_info_t {
    u8 force_stop; 			// 强制结束
    u8 recv_finish;         // 接收结束
    u8 frame_status;
    char *pcm_out_buf;
    cbuffer_t pcm_cbuf;
    ifly_vad_status status;
    ifly_vad_param *param;
};

static struct vad_info_t vad_info;
static struct ifly_websocket_struct vad_socket;


#define AI_AUDIO_SAVE_TEST          0
#if AI_AUDIO_SAVE_TEST
static FILE *save_file = NULL;
#endif

extern int mbedtls_base64_encode(unsigned char *dst, size_t dlen, size_t *olen, const unsigned char *src, size_t slen);

//录音编码模块
static u16 vad_audio_send_data(u8 *voice_buf, u16 voice_len)
{
#if AI_AUDIO_SAVE_TEST
    if (save_file) {
        int wlen = fwrite(save_file, voice_buf, voice_len);
        if (wlen != voice_len) {
            log_error("save file err: %d, %d\n", wlen, voice_len);
        }
    }
#endif
    //上传数据到服务器
    int wlen = cbuf_write(&vad_info.pcm_cbuf, voice_buf, voice_len);
    if (wlen != voice_len) {
        log_error("pcm out err: %d, %d\n", wlen, voice_len);
    }
    return 0;
}

static int vad_audio_stop(int cancel)
{
    if (!ai_mic_is_busy()) {
        log_error("ai_mic_is_null \n\n");
        return true;
    }
    ai_mic_rec_close();

#if AI_AUDIO_SAVE_TEST
    if (save_file) {
        fclose(save_file);
        save_file = NULL;
    }
#endif
    return true;
}

static int vad_audio_start(void)
{
    if (ai_mic_is_busy()) {
        log_error("ai_mic_is_busy \n\n");
        return false;
    }
#if AI_AUDIO_SAVE_TEST
    if (save_file) {
        fclose(save_file);
        save_file = NULL;
    }
    save_file = fopen("storage/sd0/C/sf.bin", "w+");
    if (!save_file) {
        log_error("fopen err \n\n");
    }
#endif

    mic_rec_pram_init(AI_AUDIO_CODING_TYPE, 0, vad_audio_send_data, 4, 1024);

    ai_mic_rec_start();

    return true;
}

static void ifly_vad_recv_cb(u8 *j_str, u32 len, u8 type)
{
    log_info("recv:%s\n", j_str);

    if (vad_info.force_stop) {
        return;
    }
    if (vad_info.status >= IFLY_VAD_STATUS_RECV_END) {
        return;
    }
    if (!strcmp(j_str, HEART_BEAT_REQ)) {  // 保持心跳请求，不解析
        return;
    }
    cJSON *cjson_root = cJSON_Parse(j_str);   //json解析错误
    if (cjson_root == NULL) {
        log_error("cjson error...\r\n");
        if (vad_info.status <= IFLY_VAD_STATUS_RECV_END) {
            vad_info.status = IFLY_VAD_STATUS_RECV_ERROR;
        }
        vad_info.param->event_cb(IFLY_VAD_EVT_NETWORK_RECV_ERROR, vad_info.param);
        return;
    }
    vad_info.status = IFLY_VAD_STATUS_RECV;

    cJSON *cjson_data = cJSON_GetObjectItem(cjson_root, "data");
    cJSON *cjson_status = cJSON_GetObjectItem(cjson_data, "status");
    cJSON *cjson_result = cJSON_GetObjectItem(cjson_data, "result");
    cJSON *cjson_ws = cJSON_GetObjectItem(cjson_result, "ws");

    int arr_size = cJSON_GetArraySize(cjson_ws);
    cJSON *arr_item = cjson_ws->child;
    u32 vad_res_len = strlen(vad_info.param->vad_res);
    for (int i = 0; i < arr_size; i++) {
        cJSON *cjson_cw = cJSON_GetObjectItem(arr_item, "cw");
        int arr_size_cw = cJSON_GetArraySize(cjson_cw);
        cJSON *arr_item_cw = cjson_cw->child;
        for (int j = 0; j < arr_size_cw; j++) {
            cJSON *cjson_w = cJSON_GetObjectItem(arr_item_cw, "w");
            char *cjson_str = cJSON_Print(cjson_w);
            u32 json_len = strlen(cjson_str);
            if ((vad_res_len + json_len + 1) > vad_info.param->vad_res_len) {
                log_error("len error\n");
            } else {
                strcpy(&vad_info.param->vad_res[vad_res_len], cjson_str);
                vad_res_len += json_len;
            }
            arr_item_cw = arr_item_cw->next;
            cJSON_free(cjson_str);
        }

        arr_item = arr_item->next;
    }

    int res_len = strlen(vad_info.param->vad_res);

    str_remove_quote(vad_info.param->vad_res, res_len);

    log_info("final res:%s\n", vad_info.param->vad_res);

    if (cjson_status->valueint == STATUS_LAST_FRAME) {
        vad_info.status = IFLY_VAD_STATUS_RECV_END;
        vad_info.recv_finish = 1;
        vad_info.param->event_cb(IFLY_VAD_EVT_RECV_OK, vad_info.param);
    }

    cJSON_Delete(cjson_root);
}


//语音听写数据模块
char *ifly_vad_format_audio_data(void)
{
    char *data_str = NULL;
    cJSON *cjson_test = NULL;
    cJSON *cjson_common = NULL;
    cJSON *cjson_business = NULL;
    cJSON *cjson_data = NULL;

    int out_len = 0;
    char *buf = malloc(BASE63_AUDIO_LEN);
    char *audio_data = malloc(AUDIO_LEN);
    ASSERT(buf);
    ASSERT(audio_data);

    int rlen = cbuf_read(&vad_info.pcm_cbuf, audio_data, AUDIO_LEN);
    if (rlen != AUDIO_LEN) {
        free(buf);
        free(audio_data);
        return NULL;
    }

    if (vad_info.frame_status == STATUS_FIRST_FRAME) {

        mbedtls_base64_encode(buf, BASE63_AUDIO_LEN, &out_len, audio_data, AUDIO_LEN);

        //编码第一帧
        cjson_test = cJSON_CreateObject();
        cjson_common = cJSON_CreateObject();
        cjson_business = cJSON_CreateObject();
        cjson_data = cJSON_CreateObject();

        cJSON_AddStringToObject(cjson_common, "app_id", APP_ID);
        cJSON_AddItemToObject(cjson_test, "common", cjson_common);

        cJSON_AddStringToObject(cjson_business, "language", "zh_cn");
        cJSON_AddStringToObject(cjson_business, "domain", "iat");
        cJSON_AddStringToObject(cjson_business, "accent", "mandarin");
        cJSON_AddNumberToObject(cjson_business, "speex_size", SPEEX_SIZE);
        cJSON_AddItemToObject(cjson_test, "business", cjson_business);

        cJSON_AddNumberToObject(cjson_data, "status", 0);
        cJSON_AddStringToObject(cjson_data, "format", "audio/L16;rate=16000");
        cJSON_AddStringToObject(cjson_data, "encoding", "speex-wb");
        cJSON_AddStringToObject(cjson_data, "audio", buf);
        cJSON_AddItemToObject(cjson_test, "data", cjson_data);

        data_str = cJSON_Print(cjson_test);

        cJSON_Delete(cjson_test);

        vad_info.frame_status = STATUS_CONTINUE_FRAME;

    } else if (vad_info.frame_status == STATUS_CONTINUE_FRAME) {

        mbedtls_base64_encode(buf, BASE63_AUDIO_LEN, &out_len, audio_data, AUDIO_LEN);

        //编码
        cjson_test = cJSON_CreateObject();
        cjson_data = cJSON_CreateObject();

        cJSON_AddNumberToObject(cjson_data, "status", 1);
        cJSON_AddStringToObject(cjson_data, "format", "audio/L16;rate=16000");
        cJSON_AddStringToObject(cjson_data, "encoding", "speex-wb");
        cJSON_AddStringToObject(cjson_data, "audio", buf);
        cJSON_AddItemToObject(cjson_test, "data", cjson_data);

        data_str = cJSON_Print(cjson_test);

        cJSON_Delete(cjson_test);

    } else {
        //编码最后一帧
        mbedtls_base64_encode(buf, BASE63_AUDIO_LEN, &out_len, audio_data, AUDIO_LEN);

        cjson_test = cJSON_CreateObject();
        cjson_data = cJSON_CreateObject();

        cJSON_AddNumberToObject(cjson_data, "status", 2);
        cJSON_AddStringToObject(cjson_data, "format", "audio/L16;rate=16000");
        cJSON_AddStringToObject(cjson_data, "encoding", "speex-wb");
        cJSON_AddStringToObject(cjson_data, "audio", buf);
        cJSON_AddItemToObject(cjson_test, "data", cjson_data);

        data_str = cJSON_Print(cjson_test);

        cJSON_Delete(cjson_test);

        if (vad_info.status < IFLY_VAD_STATUS_SEND_END) {
            vad_info.status = IFLY_VAD_STATUS_SEND_END;
            vad_audio_stop(1);
            cbuf_clear(&vad_info.pcm_cbuf);
        }
    }

    free(buf);
    free(audio_data);

    return data_str;
}

static bool ifly_vad_get_send(u8 **buf, u32 *len)
{
    if (vad_info.force_stop) {
        log_info("vad task kill!\n");
        return false;
    }
    if (vad_info.status >= IFLY_VAD_STATUS_RECV_END) {
        log_info("vad task kill!\n");
        return false;
    }
    if (vad_info.status < IFLY_VAD_STATUS_PCM_START) {
        vad_info.status = IFLY_VAD_STATUS_PCM_START;
        vad_audio_start();
        vad_info.param->event_cb(IFLY_VAD_EVT_AUDIO_START, vad_info.param);
    }
    if (cbuf_get_data_len(&vad_info.pcm_cbuf) >= AUDIO_LEN) {
        char *input_src_json = ifly_vad_format_audio_data();
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

static int ifly_vad_event_cb(ifly_socket_event_enum evt, void *param)
{
    switch (evt) {
    case IFLY_SOCKET_EVT_SEND_OK:
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
        vad_audio_stop(1);
        if ((evt != IFLY_SOCKET_EVT_END) && (evt != IFLY_SOCKET_EVT_FORCE_END)) {
            if (!vad_info.recv_finish) {
                vad_info.param->event_cb(IFLY_VAD_EVT_NETWORK_FAIL, vad_info.param);
            }
        }
        break;
    case IFLY_SOCKET_EVT_EXIT:
        vad_info.status = IFLY_VAD_STATUS_EXIT;
        vad_info.param->event_cb(IFLY_VAD_EVT_EXIT, vad_info.param);
        if (vad_socket.auth) {
            free(vad_socket.auth);
            vad_socket.auth = NULL;
        }
        if (vad_info.pcm_out_buf) {
            free(vad_info.pcm_out_buf);
            vad_info.pcm_out_buf = NULL;
        }
        break;
    default:
        break;
    }
    return 0;
}

bool ifly_vad_start(ifly_vad_param *param)
{
    memset(&vad_info, 0, sizeof(struct vad_info_t));
    memset(&vad_socket, 0, sizeof(struct ifly_websocket_struct));

    vad_info.pcm_out_buf = malloc(PCM_OUT_BUF_LEN);
    ASSERT(vad_info.pcm_out_buf);
    cbuf_init(&vad_info.pcm_cbuf, vad_info.pcm_out_buf, PCM_OUT_BUF_LEN);

    vad_info.param = param;
    vad_socket.auth = ifly_authentication("wss://iat-api.xfyun.cn/v2/iat",
                                          "iat-api.xfyun.cn",
                                          "GET /v2/iat HTTP/1.1");
    if (!vad_socket.auth) {
        free(vad_info.pcm_out_buf);
        vad_info.pcm_out_buf = NULL;
        vad_info.param->event_cb(IFLY_VAD_EVT_NETWORK_FAIL, vad_info.param);
        return false;
    }
    vad_socket.task_name = "ifly_vad";
    vad_socket.socket_mode = WEBSOCKET_MODE;
    vad_socket.recv_cb = ifly_vad_recv_cb;
    vad_socket.get_send = ifly_vad_get_send;
    vad_socket.event_cb = ifly_vad_event_cb;

    vad_info.status = IFLY_VAD_STATUS_START;

    //创建链接
    bool ret = ifly_websocket_client_create(&vad_socket);
    if (ret == false) {
        vad_info.status = IFLY_VAD_STATUS_NULL;
        free(vad_socket.auth);
        vad_socket.auth = NULL;
        free(vad_info.pcm_out_buf);
        vad_info.pcm_out_buf = NULL;
    }
    return ret;
}

void ifly_vad_stop(u8 force_stop, u32 to_ms)
{
    log_info("vad stop!\n");
    ifly_vad_audio_stop();
    vad_info.force_stop = force_stop;
    while (vad_socket.auth) { // 结束时auth会自动释放
        os_time_dly(1);
        if (to_ms <= 10) {
            break;
        }
        to_ms -= 10;
    }
    if (to_ms < 1000) {
        to_ms = 1000;
    }
    vad_info.force_stop = 1;
    ifly_websocket_client_release(&vad_socket, to_ms);
}

void ifly_vad_audio_stop(void)
{
    vad_info.frame_status = STATUS_LAST_FRAME; // 停止语音发送
}

bool ifly_vad_is_work()
{
    if ((vad_info.status != IFLY_VAD_STATUS_NULL) && (vad_info.status != IFLY_VAD_STATUS_EXIT)) {
        return true;
    }
    return false;
}
#endif


