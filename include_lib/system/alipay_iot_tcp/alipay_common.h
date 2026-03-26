#ifndef _ALIPAY_COMMOM2_H_
#define _ALIPAY_COMMOM2_H_
#include "stdint.h"
#include "stdbool.h"

#define PARAM_IN
#define PARAM_OUT
#define PARAM_INOUT

#define retval_e retval_t

#define __readonly
#define __readwrite
typedef enum {
    RV_OK = 0,
    RV_WRITE_ERROR,
    RV_READ_ERROR,
    RV_DEL_ERROR,
    RV_NOT_FOUND,
    RV_IO_ERROR,
    RV_NOT_INITIALISED,
    RV_NETWORK_ERROR,
    RV_ECC_GENERATE_ERROR,
    RV_ECC_SHARE_ERROR,
    RV_SE_INFO_ERROR,//10
    RV_SE_RESET_ERROR,
    RV_SE_GET_STATUS_ERROR,
    RV_SE_SAVE_ERROR,
    RV_SE_GENCODE_ERROR,
    RV_BUF_TOO_SHORT,
    RV_ENCRYPTION_ERRO,
    RV_DECRYPTION_ERRO,
    RV_WRONG_TIMESTAMP,
    RV_WRONG_PARAM,
    RV_PRODECT_MODEL_ERROR,//20
    RV_NOT_INPROCESSING,
    RV_SEMAPHORE_CREATE_ERROR,
    RV_SERVER_TIMEOUT,
    RV_BINDING_DATA_FORMAT_ERROR,
    RV_PROCESSING,//25
    RV_SERVER_FAIL_ERROR,
    RV_PB_PACK_ERROR,
    RV_ID2_ERROR,
    RV_JS_ERROR,
    RV_MEM_ERROR,//30
    RV_NO_QRCODE_DETECTED, //图片中未识别出二维码
    RV_NOT_SUPPORT_QRCODE, //不支持的码类型
    RV_BASE64_ERROR,
    RV_ECC_SIGN_ERROR,
    RV_RESP_TOO_LONG,             //服务端返回的数据量太大
    RV_SERVER_DATA_ERROR,         //服务端返回的数据格式有误
    RV_DATA_ITEM_EXCEED,          //数据项数量超出限制（公交卡个数太多，超出限制）
    RV_CARD_DATA_INVALID,         //卡数据无效或卡未支持
    RV_CARD_DATA_OVERDUE,         //卡数据过期
    RV_CARD_DATA_LIMITED,         //卡生码受限  40
    RV_CARD_DATA_NEED_TO_REFRESH, //卡数据即将过期
    RV_CODE_GEN_ERROR,            //生码异常
    RV_UNSUPPORTED_CARD,
    RV_JS_ENGINE_ERROR,
    RV_FILE_OPEN_ERROR,
    RV_FILE_WRITE_ERROR,
    RV_FILE_READ_ERROR,
    RV_FILE_CLOSE_ERROR,
    RV_COMMON_ERROR,
    RV_UNKNOWN = 99
} retval_e;

typedef enum {
    ALIPAY_PAYMENT_STATUS_SUCCESS          = 0,
    ALIPAY_PAYMENT_STATUS_FAIL             = 1,
    ALIPAY_PAYMENT_STATUS_NOTHING          = 2,
    ALIPAY_PAYMENT_STATUS_UNBIND_BY_USER   = 3,
    ALIPAY_PAYMENT_STATUS_DISABLED_BY_USER = 4,
    ALIPAY_PAYMENT_STATUS_UNKNOWN          = 5,
    ALIPAY_PAYMENT_STATUS_PROCESSING       = 6,
} alipay_payment_status_e;
typedef enum {
    ALIPAY_FAIL_RES_AMOUNT_LIMIT       = 0,  //超出设置的支付额度
    ALIPAY_FAIL_RES_BALANCE_NOT_ENOUGH = 1,  //零花钱不足，请存入
    ALIPAY_FAIL_RES_COMMON_FAIL        = 2,  //支付失败
    ALIPAY_FAIL_RES_ACCOUNT_NOT_EXIST  = 3,  //零花钱不存在
    ALIPAY_FAIL_RES_CASE_NOT_SUPPORT   = 4,  //该场景不支持使用智能设备支付,显示“不支持使用该设备进行支付，请使用手机支付宝支付”
} alipay_scan_pay_fail_res_e;

typedef enum {
    prd_type_watch = 0, //成人手表
    prd_type_b6t,   //成人手环
    prd_type_card,  //成人卡片
    prd_type_qzwt,  //儿童手表
    prd_type_qzb6t, //儿童手环
    prd_type_qzcrd, //儿童卡片
    prd_type_general, //其他类型，比如功能机
    prd_type_invalid
} prd_type_e;

#define ALIPAY_ITEM_PRIVATE_KEY     (0x01 << 0)
#define ALIPAY_ITEM_SHARE_KEY       (0x01 << 1)
#define ALIPAY_ITEM_SEED            (0x01 << 2)
#define ALIPAY_ITEM_TIMEDIFF        (0x01 << 3)
#define ALIPAY_ITEM_NICKNAME        (0x01 << 4)
#define ALIPAY_ITEM_LOGONID         (0x01 << 5)
#define ALIPAY_ITEM_BINDFLAG        (0x01 << 6)
#define ALIPAY_ITEM_TID             65
#define ALIPAY_ITEM_UID             66
#define ALIPAY_ITEM_DEGRADE_STATUS  67
#define ALIPAY_ITEM_TRANSPORT_ACTIVITY_STATUS   68
#define MAX_KV_NUM  11

typedef enum {
    STATUS_BINDING_FAIL = 0x0,
    STATUS_UNBINDED,
    STATUS_START_BINDING,       //准备环境，进度0
    STATUS_GETTING_PROFILE,     //进度10%，进度>0时说明BLE连接已经OK，可以关闭绑定码，显示进度
    STATUS_SAVING_DATA,         //进度30%
    STATUS_SAVING_DATA_OK,      //进度70%
    STATUS_FINISH_BINDING,      //进度90%
    STATUS_FINISH_BINDING_OK,
    STATUS_BINDING_OK = 0xA5,
    STATUS_UNKNOWN = 0xFF
} binding_status_e;


#define INPUT_PARAM
#define OUTPUT_PARAM
#define INOUT_PARAM

#define __readonly
#define __readwrite

#ifdef __cplusplus
#define EXTERNC extern "C"
#else
#define EXTERNC
#endif

#endif
