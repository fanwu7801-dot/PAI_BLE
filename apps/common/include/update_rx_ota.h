#ifndef __UPDATE_RX_OTA_H__
#define __UPDATE_RX_OTA_H__

#include "generic/typedef.h"
typedef struct {
    int (*ota_init)(void);
    int (*ota_start)(void);
    int (*ota_program)(u8 *buf, u16 length);
} ota_rx_ops;
extern const ota_rx_ops ota_rx_op;

#endif//__ADAPTER_OTA_H__

