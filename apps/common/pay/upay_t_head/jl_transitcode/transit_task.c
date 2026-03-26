
#include "typedef.h"
#include "alipay_iot_kal.h"
#include "alipay_common.h"
#include "alipay_transit.h"
#include "os/os_api.h"
#include "system/task.h"
#include "app_config.h"


#if TCFG_PAY_TRANSITCODE_ENABLE
#if (TCFG_PAY_ALIOS_WAY_SEL==TCFG_PAY_ALIOS_WAY_T_HEAD)

extern int net_get_dhcp_flag();
extern int ui_show_transitcodes_list(u8);

enum {
    TRANSITCODE_GET_DEFAUL_CARD = 1,//获取默认参数
    TRANSITCODE_SET_DEFAUL_CARD = 2,//设置默认参数
    TRANSITCODE_LOCAL_GET_CARD_LIST  = 3,//获取本地卡片
    TRANSITCODE_SERVER_GET_CARD_LIST = 4,//获取服务器卡片
    TRANSITCODE_GET_QRCODED = 5,//获取二维码
};



//乘车码对内存消耗极大 因此这里专门任务处理

#define MAX_CODE_CITY 40
static alipay_tansit_CardBaseVO_t CardBaseVO;//全局信息
static alipay_tansit_CardBaseVO_t *CardBasebuf;
static int CardBasenum = 0; //卡片数
static int CardBaseindex = 0;//设置的索引
static char *CardBase_Qr_Data;//乘车码数字
static u16 CardBase_Qr_len;//乘车码数字

#define TASK_INIT_OK     1
#define TASK_DELECT_ING  0xff
static u8 task_init = 0;//
static u8 mutex_init = 0;
static OS_MUTEX mutex;



u8 Card_no_need_release;

void set_cardcode_no_release()
{
    Card_no_need_release = 1;
}

u8 cardcode_is_need_release()
{
    return !Card_no_need_release;
}

void clear_cardcode_no_release()
{
    Card_no_need_release = 0;
}








extern int transit_code_data_callback(int event, int err, int card_num, alipay_tansit_CardBaseVO_t *card_list, int len_card_list);

static int __alipay_tansit_get_card_data(int event, int (*callback)(int event, int err, int card_num, alipay_tansit_CardBaseVO_t *card_list, int len_card_list));

static int __alipay_tansit_get_qr_data(int event, alipay_tansit_CardBaseVO_t *CardBasebuf, int (*callback)(int event, int err, int card_num, u8 *card_list, int len_card_list));

u8 is_task_active;
u8 transitcode_task_idle_query(void)
{
    return !is_task_active;
}
REGISTER_LP_TARGET(transitcode_lp_target) = {
    .name = "transit",
    .is_idle = transitcode_task_idle_query,
};


static void transitcode_task_free();
static OS_SEM start_sem;

static void transitcode_task(void *p)
{

    int msg[32];
    int ret;
    alipay_tansit_CardBaseVO_t *Cardsetdefaul;
    os_sem_post(&start_sem);
    while (1) {
        is_task_active = 0;
        ret = os_taskq_pend(NULL, msg, ARRAY_SIZE(msg)); //500ms_reflash
        if (os_task_del_req(OS_TASK_SELF) == OS_TASK_DEL_REQ) {
            os_task_del_res(OS_TASK_SELF);
        }

        if (ret != OS_TASKQ) {
            continue;
        }

        if (!(msg[0] & Q_EVENT)) {
            continue;
        }

        if (task_init  == TASK_DELECT_ING) {
            printf("%s kill ing\n", __FUNCTION__);
            continue;
        }




        int (*callback)(int event, int err, int card_num, alipay_tansit_CardBaseVO_t *card_list, int len_card_list);
        callback  = (int (*)())msg[1];

        int event = msg[0] & 0xffff;
        is_task_active = 1;

        printf("%s msg :%d\n", __FUNCTION__, msg[0] & 0xffff);

        switch (msg[0] & 0xffff) {
        case TRANSITCODE_GET_DEFAUL_CARD:
            /* printf("%s %d\n",__FUNCTION__,__LINE__); */

            if (net_get_dhcp_flag()) {
                alipay_pre_init();
            }

            ret = alipay_transit_get_default_card_info(&CardBaseVO);
            if (callback) {
                callback(event, ret, 1, &CardBaseVO, sizeof(alipay_tansit_CardBaseVO_t));
            }
            break;
        case TRANSITCODE_SET_DEFAUL_CARD:
            /* printf("%s %d\n",__FUNCTION__,__LINE__); */
            Cardsetdefaul = (alipay_tansit_CardBaseVO_t *)msg[2];
            ret = alipay_transit_set_default_card_info(Cardsetdefaul);
            if (callback) {
                callback(event, ret, 1, Cardsetdefaul, sizeof(alipay_tansit_CardBaseVO_t));
            }
            break;
        case TRANSITCODE_LOCAL_GET_CARD_LIST:
            /* printf("%s %d\n",__FUNCTION__,__LINE__); */
            __alipay_tansit_get_card_data(event, callback);
            break;
        case TRANSITCODE_SERVER_GET_CARD_LIST:
            /* printf("%s %d\n",__FUNCTION__,__LINE__); */
            __alipay_tansit_get_card_data(event, callback);
            break;
        case TRANSITCODE_GET_QRCODED:
            /* printf("%s %d\n",__FUNCTION__,__LINE__); */
            Cardsetdefaul = (alipay_tansit_CardBaseVO_t *)msg[2];
            __alipay_tansit_get_qr_data(event, Cardsetdefaul, callback);
            break;
        default:
            break;
        }


    }
}




static int __alipay_tansit_get_cardcode(alipay_tansit_CardBaseVO_t *card_list, int (*callback)(int err, u8 *transitcode, int len_card_list))
{

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
    u8 transitcode[512] = {0};
    int len_transitcode  = sizeof(transitcode);
    u8 error_message[256] = {0};
    memset(error_message, 0, sizeof(error_message));

    int ret =  alipay_transit_get_TransitCode(card_list[0].cardNo, card_list[0].cardType, transitcode, &len_transitcode, error_message, sizeof(error_message));
    if (callback) {
        callback(ret, transitcode, len_transitcode);
    }
    return 0;

}


static int __alipay_tansit_get_card_data(int event, int (*callback)(int event, int err, int card_num, alipay_tansit_CardBaseVO_t *card_list, int len_card_list))
{

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


    int onlineFlag = 0;
    if (event == TRANSITCODE_SERVER_GET_CARD_LIST) { //获取服务器卡片
        onlineFlag = 1;
    }



    alipay_tansit_CardBaseVO_t *card_list = (alipay_tansit_CardBaseVO_t *)zalloc(sizeof(alipay_tansit_CardBaseVO_t) * MAX_CODE_CITY);
    u32 len_card_list = sizeof(alipay_tansit_CardBaseVO_t) * MAX_CODE_CITY;
    u32 card_num = 0;
    char error_message[256];
    error_message[0] = 0;
    int ret = alipay_transit_get_card_list(card_list, &len_card_list, &card_num, onlineFlag);
    printf("alipay_transit_get_card_list ret %d,card_num %d,onlineFlag %d ,len %d\n", ret, card_num, onlineFlag, len_card_list);
    if (onlineFlag) {
        for (int i = 0; i < card_num; i++) {
            error_message[0] = 0;
            ret = alipay_transit_update_card_data(card_list[i].cardNo, card_list[i].cardType, error_message, sizeof(error_message));
            if (ret) {
                printf("ret %x %s \n", ret, error_message);
                break;
            }
        }
    }
    callback(event, ret, card_num, card_list, len_card_list);
    free(card_list);
    return ret;

}



static int __alipay_tansit_get_qr_data(int event, alipay_tansit_CardBaseVO_t *CardBasebuf, int (*callback)(int event, int err, int card_num, u8 *card_list, int len_card_list))
{
    CardBase_Qr_len = 512;
    if (CardBase_Qr_Data) {
        CardBase_Qr_Data  = realloc(CardBase_Qr_Data, CardBase_Qr_len);
    } else {
        CardBase_Qr_Data  = zalloc(CardBase_Qr_len);
    }
    char error_message[256];
    memset(error_message, 0, sizeof(error_message));

    /* typedef struct { */
    /*     bool    is_exists;              //本地是否有缓存； */
    /*     int32_t expire_timestamp;       //过期时间 */
    /*     int32_t remain_use_count;       //未来12小时剩余使用次数 */
    /* } alipay_transit_card_status_t; */
    alipay_transit_card_status_t card_status_t = {0};
    alipay_transit_check_card_status(CardBasebuf[0].cardNo, CardBasebuf[0].cardType, &card_status_t);
    /* printf("\n card_status_t,is_exists :%d, expire_timestamp :%d,remain_use_count :%d\n",card_status_t.is_exists,card_status_t.expire_timestamp,card_status_t.remain_use_count); */
    extern time_t time(time_t *timer);
    int32_t timestamp =  time(NULL);
    //最后10分钟也不给用
    if ((!card_status_t.is_exists) || !card_status_t.remain_use_count || (timestamp + 60 * 10 > card_status_t.expire_timestamp)) {
        if (callback(event, RV_CARD_DATA_OVERDUE, 0, CardBase_Qr_Data, 0)) {
            return 0;
        }//卡数据过期
    }

    int ret =  alipay_transit_get_TransitCode(CardBasebuf[0].cardNo, CardBasebuf[0].cardType, CardBase_Qr_Data, &CardBase_Qr_len, error_message, sizeof(error_message));
    callback(event, ret, 1, CardBase_Qr_Data, CardBase_Qr_len);
    return 0;
}

static void transitcode_task_free()
{
    if (CardBasebuf) {
        free(CardBasebuf);
        CardBasebuf = NULL;
    }
    CardBasenum = 0;
    CardBaseindex = 0;
    if (CardBase_Qr_Data) {
        free(CardBase_Qr_Data);
        CardBase_Qr_Data = NULL;
    }
    CardBase_Qr_len = 0;
    task_init  = 0;
}


void transitcode_kill()
{
    int count = 0;
    if (!mutex_init) {
        return ;
    }
    os_mutex_pend(&mutex, 0);

    if (task_init == TASK_INIT_OK) {
        task_init  = TASK_DELECT_ING;
        printf("%s\n", __FUNCTION__);

        //阿里提供
        extern void iot_rpc_cancel(long long rpc_id);
        extern long long g_my_rpc_id;
        extern u8 g_alipay_rpc_stop;
        g_alipay_rpc_stop = 1;
        iot_rpc_cancel(g_my_rpc_id);
        //阿里提供

        while (OS_TASK_NOT_EXIST != os_task_del_req("transitcode")) {
            os_taskq_post("transitcode", 1, 0);
            os_time_dly(1);
        }
        transitcode_task_free();
    }
    os_mutex_post(&mutex);
}


void transitcode_create()
{
    int err;
    if (!mutex_init) {
        os_mutex_create(&mutex);
        mutex_init = 1;
    }
    os_mutex_pend(&mutex, 0);
    if (task_init == TASK_INIT_OK) {
        os_mutex_post(&mutex);
        return;
    } else if (task_init  == TASK_DELECT_ING) {
        while (task_init) {
            os_time_dly(1);
        }
    }
    os_sem_create(&start_sem, 0);
    err = task_create(transitcode_task, NULL, "transitcode");
    os_sem_pend(&start_sem, 0);
    os_sem_del(&start_sem, OS_DEL_NO_PEND);
    task_init = TASK_INIT_OK;
    os_mutex_post(&mutex);
}


//获取
int local_transitcode_cardlist_get()
{
    int msg[3];

    if (task_init  == TASK_DELECT_ING) {
        printf("task is delect ing %d\n", __LINE__);
        return -1;
    }
    transitcode_create();
    os_mutex_pend(&mutex, 0);
    msg[0] = TRANSITCODE_LOCAL_GET_CARD_LIST;
    msg[1] = transit_code_data_callback;
    os_taskq_del_type("transitcode", msg[0] | Q_EVENT);//删消息池命令
    int err = os_taskq_post_type("transitcode", msg[0] | Q_EVENT, 1, &msg[1]);
    os_mutex_post(&mutex);
    return err;
}


//获取网上
int net_transitcode_cardlist_get()
{
    int msg[3];
    if (task_init  == TASK_DELECT_ING) {
        printf("task is delect ing %d\n", __LINE__);
        return -1;
    }
    transitcode_create();
    os_mutex_pend(&mutex, 0);
    msg[0] = TRANSITCODE_SERVER_GET_CARD_LIST;
    msg[1] = transit_code_data_callback;
    os_taskq_del_type("transitcode", msg[0] | Q_EVENT);//删消息池命令
    int err = os_taskq_post_type("transitcode", msg[0] | Q_EVENT, 1, &msg[1]);
    os_mutex_post(&mutex);
    return err;
}

//获取本地
int local_transitcode_get_default_card_info()
{
    int msg[3];
    if (task_init  == TASK_DELECT_ING) {
        printf("task is delect ing %d\n", __LINE__);
        return -1;
    }

    transitcode_create();
    os_mutex_pend(&mutex, 0);
    msg[0] = TRANSITCODE_GET_DEFAUL_CARD;
    msg[1] = transit_code_data_callback;
    os_taskq_del_type("transitcode", msg[0] | Q_EVENT);//删消息池命令
    int err = os_taskq_post_type("transitcode", msg[0] | Q_EVENT, 1, &msg[1]);
    os_mutex_post(&mutex);
    return err;
}


//设置默认卡片
int local_transitcode_set_default_card_info_by_index(int index)
{
    int msg[3];
    ASSERT(CardBasebuf);

    if (task_init  == TASK_DELECT_ING) {
        printf("task is delect ing %d\n", __LINE__);
        return -1;
    }

    os_mutex_pend(&mutex, 0);
    memcpy(&CardBaseVO, &CardBasebuf[index], sizeof(CardBaseVO));
    msg[0] = TRANSITCODE_SET_DEFAUL_CARD;
    msg[1] = transit_code_data_callback;
    msg[2] = &CardBaseVO;
    CardBaseindex = index;
    os_taskq_del_type("transitcode", msg[0] | Q_EVENT);//删消息池命令
    int err = os_taskq_post_type("transitcode", msg[0] | Q_EVENT, 2, &msg[1]);
    os_mutex_post(&mutex);
    return err;
}



//获取qr
int local_transitcode_get_qr_data(int (*callback)(int, int, int, u8 *, int))
{
    int msg[3];

    if (task_init  == TASK_DELECT_ING) {
        printf("task is delect ing %d\n", __LINE__);
        return -1;
    }

    transitcode_create();
    os_mutex_pend(&mutex, 0);
    msg[0] = TRANSITCODE_GET_QRCODED;
    msg[1] = (int)callback;
    msg[2] = (int)&CardBaseVO;
    os_taskq_del_type("transitcode", msg[0] | Q_EVENT);//删消息池命令
    int err = os_taskq_post_type("transitcode", msg[0] | Q_EVENT, 2, &msg[1]);
    os_mutex_post(&mutex);
    printf("ret %d\n", err);
    return err;
}




int transit_code_data_callback(int event, int err, int card_num, alipay_tansit_CardBaseVO_t *card_list, int len_card_list)
{
    /*
     * @return  RV_OK 更新成功
     *          RV_JS_ERROR             CJSON库或接口错误
     *          RV_NETWORK_ERROR        网络错误
     *          RV_UNKNOWN              未知错误
     *          RV_SERVER_FAIL_ERROR    服务端返回错误
     *          RV_WRITE_ERROR          KV写错误，检查KV写接口的实现
     *          RV_BUF_TOO_SHORT        允许的最大长度小于实际获取的长度
     *          RV_IO_ERROR             读取IO错误
     **/


    if (err) {
        switch (err) {
        case RV_JS_ERROR:
            puts("RV_JS_ERROR\n");
            break;
        case RV_NETWORK_ERROR:
            puts("RV_NETWORK_ERROR\n");
            break;
        case RV_UNKNOWN:
            puts("RV_UNKNOWN\n");
            break;
        case RV_SERVER_FAIL_ERROR:
            puts("RV_SERVER_FAIL_ERROR\n");
            break;
        case RV_WRITE_ERROR:
            puts("RV_WRITE_ERROR\n");
            break;
        case RV_BUF_TOO_SHORT:
            puts("RV_BUF_TOO_SHORT\n");
            break;
        case RV_IO_ERROR:
            puts("RV_IO_ERROR\n");
            break;
        default:
            printf("RV ERROR %x\n", err);
            break;
        }

    }


    if (task_init  == TASK_DELECT_ING) {
        printf("task is delect ing %d\n", __LINE__);
        return 0;
    }


    switch (event) {
    case TRANSITCODE_GET_DEFAUL_CARD:
        puts("TRANSITCODE_GET_DEFAUL_CARD \n");
        if (strlen(card_list[0].cardNo) == 0) {
            /* int net_get_dhcp_flag(); */
            /* if(net_get_dhcp_flag()){ */
            /*  */
            /* } */
            local_transitcode_cardlist_get();//获取本地缓存
        } else {
            printf("cardNo %s\n", card_list[0].cardNo);
            printf("cardType %s\n", card_list[0].cardType);
            printf("cardtitle %s\n", card_list[0].title);
            alipay_transit_card_status_t card_status_t = {0};
            alipay_transit_check_card_status(card_list[0].cardNo, card_list[0].cardType, &card_status_t);
            extern time_t time(time_t *timer);
            int32_t timestamp =  time(NULL);
            //最后10分钟也不给用
            if ((!card_status_t.is_exists) || !card_status_t.remain_use_count || (timestamp + 60 * 10 > card_status_t.expire_timestamp)) {
                //卡数据过期
                //1显示卡片列表
                printf("\n card_status_t,is_exists :%d, expire_timestamp :%d,remain_use_count :%d %d\n", card_status_t.is_exists, card_status_t.expire_timestamp, card_status_t.remain_use_count, timestamp);

                local_transitcode_cardlist_get();//获取本地缓存
            } else {
                //0直接显示二维码
                ui_show_transitcodes_list(0);
            }
        }
        break;
    case TRANSITCODE_SET_DEFAUL_CARD:
        puts("TRANSITCODE_SET_DEFAUL_CARD \n");
        break;
    case TRANSITCODE_SERVER_GET_CARD_LIST:
    case TRANSITCODE_LOCAL_GET_CARD_LIST:
        puts("TRANSITCODE_LOCAL_GET_CARD_LIST \n");
        //card_list 需要拷贝使用
        CardBasenum = card_num;
        if (card_num) {
            if (CardBasebuf) {
                CardBasebuf  = realloc(CardBasebuf, len_card_list);
            } else {
                CardBasebuf  = zalloc(len_card_list);
            }
            memcpy(CardBasebuf, card_list, len_card_list);

            //0直接显示二维码
            //1显示卡片列表
            //2显示进度
            ui_show_transitcodes_list(1);
            for (int i = 0; i < card_num; i++) {
                printf("cardNo %s\n", CardBasebuf[i].cardNo);
                printf("cardType %s\n", CardBasebuf[i].cardType);
                printf("cardtitle %s\n", CardBasebuf[i].title);
            }

        } else {
            if ((event == TRANSITCODE_LOCAL_GET_CARD_LIST) && net_get_dhcp_flag()) {
                //本地获取失败 带dhcp 情况
                ui_show_transitcodes_list(2);
                net_transitcode_cardlist_get();
            } else {
                if ((event == TRANSITCODE_SERVER_GET_CARD_LIST) && (!err)) {
                    //联网情况获取失败，显示没有开通
                    ui_show_transitcodes_list(6);
                } else {
                    //这里直接显示 城市卡片获取失败
                    ui_show_transitcodes_list(4);
                }
            }
        }
        break;
    case TRANSITCODE_GET_QRCODED:
        puts("TRANSITCODE_GET_QRCODED \n");
        break;
    default:
        break;
    }
    return 0;
}


void *alipay_tansit_buf_get()
{
    return (void *)CardBasebuf;
}

int alipay_tansit_num_get()
{
    return CardBasenum;
}


char *alipay_transit_name_get_by_index(int index)
{
    ASSERT(index < CardBasenum);
    ASSERT(CardBasebuf);
    return CardBasebuf[index].title;
}


int alipay_transit_get_default_card_index()
{
    ASSERT(CardBasebuf);
    return CardBaseindex;
}

char *alipay_transit_get_global_default_card_name()
{
    return  CardBaseVO.title;
}



#endif
#endif





