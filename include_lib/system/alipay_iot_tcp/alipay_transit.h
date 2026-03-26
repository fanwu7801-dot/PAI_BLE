#ifndef _ALIPAY_TRANSIT_H_
#define _ALIPAY_TRANSIT_H_

#include "alipay_common.h"
#if defined(__cplusplus)||defined(c_plusplus)
extern "C" {
#endif

/**
 * @brief 网络初始化
 * @return retval_e
 */
retval_e alipay_pre_init(void);

typedef struct alipay_tansit_CardBaseVO {
    char  cardNo[40];               //卡号
    char  cardType[40];             //卡类型
    char  title[40];                //卡标题
} alipay_tansit_CardBaseVO_t;

/***
 * 获取本地的开通状态(成人设备默认开通，无需关心该接口)
 * @return  true  已经开通
 *          false 未开通
 **/
extern bool alipay_transit_get_activity_status_local(void);

/***
 * 更新开通状态(成人设备默认开通，无需关心该接口)
 * @param  [out]status   开通状态，true-已开通；false-未开通
 * @return  RV_OK 更新成功
 *          RV_JS_ERROR             CJSON库或接口错误
 *          RV_NETWORK_ERROR        网络错误
 *          RV_UNKNOWN              未知错误
 *          RV_SERVER_FAIL_ERROR    服务端返回错误
 *          RV_WRITE_ERROR          KV写错误，检查KV写接口的实现
 **/
extern retval_e alipay_transit_update_activity_status(PARAM_OUT uint8_t *status);

/***
 * 获取卡列表
 * @param   [out]card_list          存放获取的卡列表
 *          [inout]len_card_list    输入时为允许获取的最大长度，输出时为实际获取的长度
 *          [out]card_num           获取的卡片数量
 *          [in]onlineFlag          1:优先从服务端获取；0:只获取本地保存的
 * @return  RV_OK 更新成功
 *          RV_JS_ERROR             CJSON库或接口错误
 *          RV_NETWORK_ERROR        网络错误
 *          RV_UNKNOWN              未知错误
 *          RV_SERVER_FAIL_ERROR    服务端返回错误
 *          RV_WRITE_ERROR          KV写错误，检查KV写接口的实现
 *          RV_BUF_TOO_SHORT        允许的最大长度小于实际获取的长度
 **/
extern retval_e alipay_transit_get_card_list(PARAM_OUT alipay_tansit_CardBaseVO_t *card_list, PARAM_INOUT uint32_t *len_card_list, PARAM_OUT uint32_t *card_num, PARAM_IN bool onlineFlag);

/***
 * 查询功能是否可用
 * @param   [out]precheck_status    true:可用，false:不可用
 * @return  RV_OK 更新成功
 *          RV_JS_ERROR             CJSON库或接口错误
 *          RV_NETWORK_ERROR        网络错误
 *          RV_UNKNOWN              未知错误
 *          RV_SERVER_FAIL_ERROR    服务端返回错误
 **/
extern retval_e alipay_transit_get_degrade_status(PARAM_OUT uint32_t *precheck_status);

/***
 * 获取指定卡的具体数据
 * @param   [in]cardNo              卡序号
 *          [in]cardType            卡类型
 *          [out] error_message     错误原因，仅在返回值为RV_SERVER_FAIL_ERROR的时候使用
 *          [in] len_error_message  error_message的buffer大小，建议为256
 * @return  RV_OK 更新成功
 *          RV_JS_ERROR             CJSON库或接口错误
 *          RV_NETWORK_ERROR        网络错误
 *          RV_UNKNOWN              未知错误
 *          RV_SERVER_FAIL_ERROR    服务端返回错误，屏幕显示error_message参数输出的错误原因
 *          RV_WRITE_ERROR          KV写错误，检查KV写接口的实现
 **/
extern retval_e alipay_transit_update_card_data(PARAM_IN char *cardNo, PARAM_IN char *cardType, PARAM_OUT char *error_message, PARAM_IN uint32_t len_error_message);


typedef struct {
    bool    is_exists;              //本地是否有缓存；
    int32_t expire_timestamp;       //过期时间
    int32_t remain_use_count;       //未来12小时剩余使用次数
} alipay_transit_card_status_t;

extern retval_e alipay_transit_check_card_status(PARAM_IN char *cardNo, PARAM_IN char *cardType, PARAM_OUT alipay_transit_card_status_t *status);

/***
 * 获取指定卡的乘车码码值
 * @param   [in]cardNo              卡序号
 *          [in]cardType            卡类型
 *          [out]transitcode        储存码值的缓存，buffer大小建议为512，字节码，不是字符串
 *          [inout]len_transitcode  输入时为允许获取的最大长度，输出时为实际获取的长度
 *          [out] error_message     错误原因，仅在返回值为RV_SERVER_FAIL_ERROR的时候使用
 *          [in] len_error_message  error_message的buffer大小，建议为256
 * @return  RV_OK 更新成功
 *          RV_CARD_DATA_LIMITED    由于风控原因生码受限
 *          RV_UNKNOWN              未知错误
 *          RV_BUF_TOO_SHORT        允许的最大长度小于实际获取的长度
 *          RV_CODE_GEN_ERROR       生码异常
 *          RV_NETWORK_ERROR        网络错误
 *          RV_SERVER_FAIL_ERROR    服务端返回错误，屏幕显示error_message参数输出的错误原因
 *          RV_COMMON_ERROR         显示“暂不支持，请重试或切换到手机侧使用”
 *          RV_UNSUPPORTED_CARD     不支持该乘车码，显示“暂不支持，请重试或切换到手机侧使用”
 **/
extern retval_e alipay_transit_get_TransitCode(PARAM_IN char *cardNo, PARAM_IN char *cardType, PARAM_OUT uint8_t *transitcode, PARAM_INOUT uint32_t *len_transitcode, char *error_message, uint32_t len_error_message);

/***
 * 清除存储中乘车码相关的内容，如果开启了乘车码功能，请在调用alipay_unbinding接口前调用此接口
 * 若清除失败，请检查文件存储相关接口
 * @return  RV_OK  清除成功
 *          RV_DEL_ERROR 清除失败
 **/
extern retval_e alipay_transit_unbind(void);

/***
 * 获取默认卡信息
 * @param  [out]卡信息结构体
 * @return  RV_OK                   获取成功
 *          RV_IO_ERROR             读取IO错误
 **/
retval_e alipay_transit_get_default_card_info(PARAM_OUT alipay_tansit_CardBaseVO_t *default_card_info);

/***
 * 保存默认卡信息
 * @param  [in]卡信息结构体
 * @return  RV_OK                   保存成功
 *          RV_IO_ERROR             保存IO错误
 **/
retval_e alipay_transit_set_default_card_info(PARAM_IN alipay_tansit_CardBaseVO_t *default_card_info);

#if defined(__cplusplus)||defined(c_plusplus)
}
#endif
#endif
