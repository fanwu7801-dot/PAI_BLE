
// #include "type.h"
#include "hs_private.h"
#include "iotsec.h"
#define ALIPAY_SE_FW_V2_0     1
//--------------------------------------------------------------------------
//
//									版本V1.0.01
//
//--------------------------------------------------------------------------





/*○ 功能描述
    ■ 获取随机数
  ○ 接口参数
    ■ random-随机数地址
    ■ size-获取随机数数据长度
  ○ 返回值
    ■ 0表示成功，非0表示失败*/
csi_error_t csi_rng_get_data(void *random, uint32_t size)
{
    unsigned int dwOff;
#if ALIPAY_SE_FW_V2_0
    unsigned int i;
#endif
    dwOff = 0;
    while (size - dwOff > 256) {
        g_HSI2CBuf[0] = 0x00;
        g_HSI2CBuf[1] = 0x05;
        g_HSI2CBuf[2] = 0x00;
        g_HSI2CBuf[3] = 0x84;
        g_HSI2CBuf[4] = 0x00;
        g_HSI2CBuf[5] = 0x00;
        g_HSI2CBuf[6] = 0x00;
#if ALIPAY_SE_FW_V2_0
        if (gHSEncTransSign == 0x01) {
            for (i = 0; i < 5; ++i) {
                g_HSI2CBuf[2 + i] ^= g_TransTmpKey[i];
            }
        }
#endif
        ComputeCRC(g_HSI2CBuf, 7, g_HSI2CBuf + 7);
        HS_IIC_Send_Cmd(g_HSI2CBuf, 9);
        if (0 == HS_IIC_Recv_Resp(random + dwOff, 256)) {
            dwOff += 256;
        } else {
            return CSI_ERROR;
        }
    }

    g_HSI2CBuf[0] = 0x00;
    g_HSI2CBuf[1] = 0x05;
    g_HSI2CBuf[2] = 0x00;
    g_HSI2CBuf[3] = 0x84;
    g_HSI2CBuf[4] = 0x00;
    g_HSI2CBuf[5] = 0x00;
    g_HSI2CBuf[6] = size - dwOff;
#if ALIPAY_SE_FW_V2_0
    if (gHSEncTransSign == 0x01) {
        for (i = 0; i < 5; ++i) {
            g_HSI2CBuf[2 + i] ^= g_TransTmpKey[i];
        }
    }
#endif
    ComputeCRC(g_HSI2CBuf, 7, g_HSI2CBuf + 7);
    HS_IIC_Send_Cmd(g_HSI2CBuf, 9);
    if (0 != HS_IIC_Recv_Resp(random + dwOff, size - dwOff)) {
        return CSI_ERROR;
    }

    return CSI_OK;
}

/*○ 功能描述
    ■ 获取设备ID
  ○ 接口参数
    ■ uid-设备ID地址
    ■ len-返回设备ID长度
  ○ 返回值
    ■ 0表示成功，非0表示失败*/
csi_error_t csi_get_huk(uint8_t *uid, uint32_t *len)
{
#if ALIPAY_SE_FW_V2_0
    unsigned char i;
#endif
    g_HSI2CBuf[0] = 0x00;
    g_HSI2CBuf[1] = 0x05;
    g_HSI2CBuf[2] = 0x80;
    g_HSI2CBuf[3] = 0xF6;
    g_HSI2CBuf[4] = 0x00;
    g_HSI2CBuf[5] = 0x00;
    g_HSI2CBuf[6] = 0x10;
#if ALIPAY_SE_FW_V2_0
    if (gHSEncTransSign == 0x01) {
        for (i = 0; i < 5; ++i) {
            g_HSI2CBuf[2 + i] ^= g_TransTmpKey[i];
        }
    }
#endif
    ComputeCRC(g_HSI2CBuf, 7, g_HSI2CBuf + 7);
    HS_IIC_Send_Cmd(g_HSI2CBuf, 9);
    if (0 != HS_IIC_Recv_Resp(uid, 16)) {
        return CSI_ERROR;
    }

    *len = 16;
    return CSI_OK;
}

csi_error_t csi_get_ssk(uint8_t *seed, uint8_t *salt, uint8_t *derived_key, uint8_t derived_key_len)
{
    return CSI_OK;
}
