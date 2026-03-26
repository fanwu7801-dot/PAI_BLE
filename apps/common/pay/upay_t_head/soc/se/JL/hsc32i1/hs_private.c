
// #include "IIC_Master_Driver.h"
#include "asm/iic_hw.h"
#include "asm/iic_soft.h"
#include "app_config.h"
#include "system/includes.h"

const char log_tag_const_v_ALIPAY_HSP AT(.LOG_TAG_CONST) = 1;//LIB_DEBUG & FALSE;
const char log_tag_const_i_ALIPAY_HSP AT(.LOG_TAG_CONST) = 1;//LIB_DEBUG & TRUE;
const char log_tag_const_d_ALIPAY_HSP AT(.LOG_TAG_CONST) = 1;//LIB_DEBUG & TRUE;
const char log_tag_const_w_ALIPAY_HSP AT(.LOG_TAG_CONST) = 1;//LIB_DEBUG & TRUE;
const char log_tag_const_e_ALIPAY_HSP AT(.LOG_TAG_CONST) = 1;//LIB_DEBUG & TRUE;

#define LOG_TAG_CONST       ALIPAY_HSP
#define LOG_TAG             "[ALIPAY_HSP]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"
#include "gpio.h"
#if TCFG_TOUCH_USER_IIC_TYPE
#define iic_init(iic)                       hw_iic_init(iic)
#define iic_uninit(iic)                     hw_iic_uninit(iic)
#define iic_start(iic)                      hw_iic_start(iic)
#define iic_stop(iic)                       hw_iic_stop(iic)
#define iic_tx_byte(iic, byte)              hw_iic_tx_byte(iic, byte)
#define iic_rx_byte(iic, ack)               hw_iic_rx_byte(iic, ack)
#define iic_read_buf(iic, buf, len)         hw_iic_read_buf(iic, buf, len)
#define iic_write_buf(iic, buf, len)        hw_iic_write_buf(iic, buf, len)
#define iic_suspend(iic)                    hw_iic_suspend(iic)
#define iic_resume(iic)                     hw_iic_resume(iic)
#else
#define iic_init(iic)                       soft_iic_init(iic)
#define iic_uninit(iic)                     soft_iic_uninit(iic)
#define iic_start(iic)                      soft_iic_start(iic)
#define iic_stop(iic)                       soft_iic_stop(iic)
#define iic_tx_byte(iic, byte)              soft_iic_tx_byte(iic, byte)
#define iic_rx_byte(iic, ack)               soft_iic_rx_byte(iic, ack)
#define iic_read_buf(iic, buf, len)         soft_iic_read_buf(iic, buf, len)
#define iic_write_buf(iic, buf, len)        soft_iic_write_buf(iic, buf, len)
#define iic_suspend(iic)                    soft_iic_suspend(iic)
#define iic_resume(iic)                     soft_iic_resume(iic)
#endif

typedef struct {
    u8 init;
    hw_iic_dev iic_hdl;
} ft_param;

static ft_param module_param = {
    .init = 0,
    .iic_hdl = 1,
};
#define  SE_GPIO_RESET_PIN  IO_PORTG_01 // SE芯片的reset管脚配置,默认用IO_PORTG_01

#define __this   (&module_param)

#define HOST_ID        1
#define ALIPAY_SE_FW_V2_0  1
extern const struct soft_iic_config soft_iic_cfg[];


//unsigned char g_HSI2CBuf[2048];
unsigned char *g_HSI2CBuf;
#define HS_BUF_LEN 4096//2048
#if ALIPAY_SE_FW_V2_0

unsigned char gHSEncTransSign;	//通信是否加密，0 = 明文通信，1 = 加密通信
unsigned char g_DefTransKey[16] = {0x54, 0x8E, 0x8D, 0x7A, 0x26, 0x35, 0x85, 0x11, 0x25, 0x2E, 0xF8, 0xD6, 0xB7, 0x17, 0xA4, 0xE4};
unsigned char g_TransTmpKey[16];

static void delay_ms(int cnt)
{
    int delay = (cnt + 9) / 10;
    os_time_dly(delay);
}

void Delay_Ms(unsigned char byMilliSec)
{
    delay_ms(byMilliSec);
}
void Set_GPIO_RESET_DLE(void)
{
    u32 gpio = SE_GPIO_RESET_PIN;//指定IO给加密芯片reste
    gpio_set_pull_down(gpio, 0);
    gpio_set_pull_up(gpio, 0);
    gpio_direction_input(gpio);
    gpio_set_die(gpio, 0);
}
void Set_GPIO_State(unsigned char byState)
{
    u32 gpio = SE_GPIO_RESET_PIN;//指定IO给加密芯片reste
    gpio_set_pull_down(gpio, 0);
    gpio_set_pull_up(gpio, 0);
    gpio_set_hd(gpio, 0);//看需求是否需要开启强推,会导致芯片功耗大
    gpio_set_hd0(gpio, 0);
    gpio_set_direction(gpio, 0);
    gpio_set_output_value(gpio, byState); //1高0低*/

    return ;
}
void se_ic_reset(void)
{
    printf("func =%s", __func__);
    Set_GPIO_State(0);
    Delay_Ms(5);
    Set_GPIO_State(1);
    Delay_Ms(30);
    Set_GPIO_RESET_DLE();
    return ;
}
#endif

//extern unsigned char * IIC_Master_Init(void);

//extern void IIC_Master_Send(unsigned char byAddr, unsigned char *pData, unsigned short wLen);

//extern void IIC_Master_Receive(unsigned char byAddr, unsigned char *pData, unsigned short wLen);

//CRC单字节计算——循环左移，先移最高bit
unsigned short LeftShift_CRC(unsigned char byValue, unsigned short *lpwCrc)
{
    // log_info("%s\n", __func__);
    byValue ^= (unsigned char)(*lpwCrc >> 8);
    byValue ^= (byValue >> 4);

    *lpwCrc = (*lpwCrc << 8) ^ (unsigned short)(byValue);
    *lpwCrc ^= (unsigned short)byValue << 12;
    *lpwCrc ^= (unsigned short)byValue << 5;

    return (*lpwCrc);
}

void ComputeCRC(unsigned char *Data, unsigned short Length, unsigned char *pbyOut)
{
    unsigned char chBlock;
    unsigned short wCrc;
    log_info("%s\n", __func__);

    wCrc = 0x0000; // ITU-V.41

    while (Length--) {
        chBlock = *Data++;
        LeftShift_CRC(chBlock, &wCrc);
    }

    pbyOut[0] = (unsigned char)((wCrc >> 8) & 0xFF);
    pbyOut[1] = (unsigned char)(wCrc & 0xFF);

    return;
}

void lmemset(unsigned char *pBuf, unsigned char byVal, unsigned short wLen)
{
    unsigned short i;
    log_info("%s\n", __func__);

    for (i = 0; i < wLen; ++i) {
        pBuf[i] = byVal;
    }
    return ;
}

void lmemcpy(unsigned char *pDst, unsigned char *pSrc, unsigned short wLen)
{
    unsigned short i;
    // log_info("%s\n", __func__);

    if ((pSrc == pDst) || (wLen == 0)) {
        return;
    }

    if (pSrc < pDst) {
        for (i = wLen - 1; i != 0; --i) {
            pDst[i] = pSrc[i];
        }
        pDst[0] = pSrc[0];
    } else {
        for (i = 0; i < wLen; ++i) {
            pDst[i] = pSrc[i];
        }
    }

    return ;
}

//相同返回0，不同返回1
unsigned char lmemcmp(unsigned char *pBuf1, unsigned char *pBuf2, unsigned short wLen)
{
    unsigned short i;
    log_info("%s\n", __func__);

    for (i = 0; i < wLen; ++i) {
        if (pBuf1[i] != pBuf2[i]) {
            return 1;
        }
    }

    return 0;
}

#define TEST_1		0

#define FT6336G_IIC_ADDR  0xC8
#define FT6336G_IIC_DELAY   1

#define SLAVE_ADDR      0x50

#define WRITE_ADDR      SLAVE_ADDR
#define READ_ADDR       (WRITE_ADDR|BIT(0))
#if  TCFG_ALIPAY_SE_IIC_MUL
//extern  spinlock_t se_iic_lock;
DEFINE_SPINLOCK(se_iic_lock);
#endif
static int hs_i2c_write(u8 *buf, int len)
{

#if TEST_1
    int ret = 0;
    log_info("%s[len:%d]", __func__, len);
    soft_iic_start(__this->iic_hdl);
    ret = soft_iic_tx_byte(__this->iic_hdl, WRITE_ADDR);
    if (0 == ret) {
        ret = -1;
        goto __end;
    }
    delay(soft_iic_cfg[HOST_ID].delay);
    ret = soft_iic_tx_byte(__this->iic_hdl, 0x55);
    if (0 == ret) {
        ret = -2;
        goto __end;
    }
    delay(soft_iic_cfg[__this->iic_hdl].delay);
    ret = soft_iic_write_buf(__this->iic_hdl, buf, len);
    if (0 == ret) {
        ret = -3;
        goto __end;
    }
__end:
    soft_iic_stop(__this->iic_hdl);
    delay(soft_iic_cfg[__this->iic_hdl].delay);
    log_info("%s[ret:%d]", __func__, ret);
    return ret;
#else

    int ret = 0;
    log_info("%s[len:%d]", __func__, len);
#if  TCFG_ALIPAY_SE_IIC_MUL
    spin_lock(&se_iic_lock);
#endif
    soft_iic_start(__this->iic_hdl);
    ret = soft_iic_tx_byte(__this->iic_hdl, FT6336G_IIC_ADDR);
    if (0 == ret) {
        ret = -1;
        goto __end;
    }
    delay(soft_iic_cfg[__this->iic_hdl].delay);
    ret = soft_iic_write_buf(__this->iic_hdl, buf, len);
    if (0 == ret) {
        ret = -3;
        goto __end;
    }
__end:
    soft_iic_stop(__this->iic_hdl);
    delay(soft_iic_cfg[__this->iic_hdl].delay);
    log_info("%s[ret:%d]", __func__, ret);
#if  TCFG_ALIPAY_SE_IIC_MUL
    spin_unlock(&se_iic_lock);
#endif
    return ret;
#endif
}

u8 hs_i2c_read(u8 addr, u8 *buf, unsigned short len)
{
#if TEST_1
    int ret = 0;
    log_info("%s[len:%d]", __func__, len);
    soft_iic_start(__this->iic_hdl);
    ret = soft_iic_tx_byte(__this->iic_hdl, WRITE_ADDR);
    if (0 == ret) {
        ret = -1;
        goto __end;
    }
    delay(soft_iic_cfg[__this->iic_hdl].delay);
    ret = soft_iic_tx_byte(__this->iic_hdl, 0xAA);
    if (0 == ret) {
        ret = -2;
        goto __end;
    }
    delay(soft_iic_cfg[__this->iic_hdl].delay);
    soft_iic_start(__this->iic_hdl);
    ret = soft_iic_tx_byte(__this->iic_hdl, READ_ADDR);
    if (0 == ret) {
        ret = -3;
        goto __end;
    }
    delay(soft_iic_cfg[HOST_ID].delay);
    ret = soft_iic_read_buf(__this->iic_hdl, buf, len);
    if (0 == ret) {
        ret = -4;
        goto __end;
    }
__end:
    soft_iic_stop(__this->iic_hdl);
    delay(soft_iic_cfg[HOST_ID].delay);
    log_info("%s[ret:0x%x]", __func__, ret);
    return ret;
#else
    int ret = 0;
#if  TCFG_ALIPAY_SE_IIC_MUL
    spin_lock(&se_iic_lock);
#endif
    log_info("%s[len:%d]", __func__, len);

    soft_iic_start(__this->iic_hdl);
    ret = soft_iic_tx_byte(__this->iic_hdl, addr);
    if (0 == ret) {
        ret = -3;
        goto __end;
    }
    delay(soft_iic_cfg[__this->iic_hdl].delay);
    ret = soft_iic_read_buf(__this->iic_hdl, buf, len);
    if (0 == ret) {
        ret = -4;
        goto __end;
    }
__end:
    soft_iic_stop(__this->iic_hdl);
    delay(soft_iic_cfg[__this->iic_hdl].delay);
    put_buf(buf, len);
    log_info("%s[ret:0x%x]", __func__, ret);
#if  TCFG_ALIPAY_SE_IIC_MUL
    spin_unlock(&se_iic_lock);
#endif
    return ret;
#endif
}

void HS_IIC_Init(void)
{
#if ALIPAY_SE_FW_V2_0
    unsigned char ver[29];
    unsigned char i;

#if ALIPAY_SE_USE_RESET_PIN
    extern void se_ic_reset(void);
    se_ic_reset();
    os_time_dly(3);
#endif
    gHSEncTransSign = 0x00;
    lmemset(g_TransTmpKey, 0x00, 16);
#endif

    log_info("%s[hd:%d]\n", __func__, __this->iic_hdl);
    // g_HSI2CBuf = IIC_Master_Init();

    if (!g_HSI2CBuf) {
        g_HSI2CBuf = malloc(HS_BUF_LEN);
    }
    iic_init(__this->iic_hdl);
    printf("delay:%d\n", soft_iic_cfg[__this->iic_hdl].delay);

#if ALIPAY_SE_FW_V2_0
    g_HSI2CBuf[0] = 0x00;
    g_HSI2CBuf[1] = 0x05;
    g_HSI2CBuf[2] = 0x00;
    g_HSI2CBuf[3] = 0xCA;
    g_HSI2CBuf[4] = 0x00;
    g_HSI2CBuf[5] = 0x00;
    g_HSI2CBuf[6] = 0x00;
    ComputeCRC(g_HSI2CBuf, 7, g_HSI2CBuf + 7);
    unsigned char HS_IIC_Recv_Resp(unsigned char *pData, unsigned short wLen);
    void HS_IIC_Send_Cmd(unsigned char *pData, unsigned short wLen);
    HS_IIC_Send_Cmd(g_HSI2CBuf, 9);
    if (0 != HS_IIC_Recv_Resp(ver, 29)) { //如果获取初值密钥指令执行异常，就用16字节FF进行加密通信
        gHSEncTransSign = 0x01;
        lmemset(g_TransTmpKey, 0xFF, 16);
        return ;
    }

    if ((ver[0] == 0x80) && (ver[1] == 0x10) && (ver[18] == 0x81) && (ver[19] == 0x02)) { //简单的检查一下数据格式内容
        if ((ver[20] != 0x10) || (ver[21] != 0x03)) {
            gHSEncTransSign = 0x01;
            for (i = 0; i < 16; ++i) {
                g_TransTmpKey[i] = ver[2 + i] ^ g_DefTransKey[i];
            }
        }
    } else {
        gHSEncTransSign = 0x01;
        lmemset(g_TransTmpKey, 0xFF, 16);
    }

#endif  //ALIPAY_SE_FW_V2_0
}

void HS_IIC_Uninit(void)
{
    log_info("%s[hd:%d]\n", __func__, __this->iic_hdl);
#if TCFG_ALIPAY_SE_IIC_MUL
    log_info("%s is NULL\n", __func__);
#else
    iic_uninit(__this->iic_hdl);
#endif
    /* if (g_HSI2CBuf != NULL) { */
    /* free(g_HSI2CBuf); */
    /* printf("g_HSI2CBuf is free 123----"); */
    /* g_HSI2CBuf = NULL; */
    /* } */
    printf("delay:%d\n", soft_iic_cfg[__this->iic_hdl].delay);
}

void HS_IIC_Send_Cmd(unsigned char *pData, unsigned short wLen)
{
    // IIC_Master_Send(0xC8, pData, wLen);
    log_info("%s[len:%d]\n", __func__, wLen);
    put_buf(pData, wLen);
    int ret = hs_i2c_write(pData, wLen);
    log_debug("%s[ret:%d]\n", __func__, ret);
}

//功能:	接收wLen长度的数据，到pData缓冲区中
//返回:
//			0 表示正确接收
//			1 表示接收数据CRC错误
//			2 表示实际接收数据长度域不是wLen
//			3 表示指令返回状态字不是9000
//			4 表示指令超时未响应
unsigned char HS_IIC_Recv_Resp(unsigned char *pData, unsigned short wLen)
{
    unsigned short wRspLen;
    unsigned char crc[2];
    unsigned int dwCnt;
    unsigned short i;
    log_info("%s[len:%d]\n", __func__, wLen);
    int ret = 0;

    g_HSI2CBuf[0] = 0x00;
    dwCnt = 0;
    while (g_HSI2CBuf[0] != 0x55) {
        ++dwCnt;
        if (dwCnt > 0x100) {

            return 4;
        }
        ret = hs_i2c_read(0xC9, g_HSI2CBuf, 1);
        // r_printf("%s[ret1:%d]\n", __func__, ret);
        // put_buf(g_HSI2CBuf,1);
        // g_printf("g_HSI2CBuf:0x%x\n", g_HSI2CBuf[0]);

    }

    int rlen = wLen + 6;

    hs_i2c_read(0xC9, g_HSI2CBuf, rlen);

    wRspLen = g_HSI2CBuf[0];
    wRspLen <<= 8;
    wRspLen |= g_HSI2CBuf[1];


    if (wRspLen != wLen + 2) {


        return 2;
    }

#if  ALIPAY_SE_FW_V2_0
    ComputeCRC(g_HSI2CBuf + 2, wLen + 2, crc);
    if ((g_HSI2CBuf[wLen + 4] != crc[0]) || (g_HSI2CBuf[wLen + 5] != crc[1])) {
        return 1;
    }

    if (gHSEncTransSign == 0x01) {
        for (i = 0; i < wLen + 2; ++i) {
            g_HSI2CBuf[2 + i] ^= g_TransTmpKey[i % 16];
        }
    }
#endif

    if ((g_HSI2CBuf[2 + wLen] != 0x90) || (g_HSI2CBuf[3 + wLen] != 0x00)) { //状态字


        pData[0] = g_HSI2CBuf[2 + wLen];
        pData[1] = g_HSI2CBuf[3 + wLen];

        return 3;
    }

#if  ALIPAY_SE_FW_V2_0
#else
    ComputeCRC(g_HSI2CBuf + 2, wLen + 2, crc);


    if ((g_HSI2CBuf[wLen + 4] != crc[0]) || (g_HSI2CBuf[wLen + 5] != crc[1])) {


        return 1;
    }
#endif

    lmemcpy(pData, g_HSI2CBuf + 2, wLen);



    return 0;

}

static u8 write[16];
static u8 read[16];
static u8 receive[16];
void iic_master_test(void)
{

    int ret = 0;
    int ret1 = 0;
    // clk_set("sys", (48 * 1000000L));
    memset(read, 0x00, sizeof(read));
    write[0]++;
    for (u8 i = 0; i < sizeof(write); i++) {
        write[i] = i;
    }
    write[0] = 1;
    write[sizeof(write) - 1] = 0xff; //包结束符 //视情况修改
    HS_IIC_Send_Cmd(write, sizeof(write));
    /* delay(soft_iic_cfg[HOST_ID].delay); */
    ret1 = hs_i2c_read(0xAA, read, sizeof(read));

    log_info("%s[send 0x%x  0x%x]", __func__, write[0], ret);
    put_buf(receive, sizeof(receive));
    log_info("%s[read 0x%x]", __func__, ret1);
    if (ret1 > 0) {
        put_buf(read, ret1);
    }
}


