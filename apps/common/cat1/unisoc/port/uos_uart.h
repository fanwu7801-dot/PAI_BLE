#ifndef ZHANRUI_UART_H
#define ZHANRUI_UART_H
#include "uos_type.h"
#include "uos_deval.h"
#include "uos_vendor_cfg.h"
/*************************************************
* 功能： 打开和modem通信的uart并配置完成，内部逻辑处理使用哪一个UART
* 输入参数：
* 无
* 返回：
* 错误码
************************************************/
uos_err_t uos_uart_open(void);

/*************************************************
* 功能： 阻塞式写UART，必须实现
* 输入参数：
* write_buff 输入。写数据指针
* write_size 输入。写数据大小
* 返回：
* 实际写入数据大小
************************************************/
uos_uint32_t uos_uart_write(uos_uint8_t *write_buff, uos_uint32_t write_size);
/*************************************************
* 功能： 阻塞式读UART， 可选实现
* 输入参数：
* read_buff 输入。读数据指针
* read_size 输入。读数据大小
* 返回：
* 实际读出数据大小
************************************************/
uos_uint32_t uos_uart_read(uos_uint8_t *read_buff, uos_uint32_t read_size);
/*************************************************
* 功能： 非阻塞式写UART， 可选实现
* * 输入参数：
* write_buff 输入。写数据指针
* write_size 输入。写数据大小
* tx_callback 输入。写完成回调函数
* 返回：
* 错误码
************************************************/
uos_err_t uos_uart_write_async(uos_uint8_t *write_buff, uos_uint32_t write_size,
                               uart_tx_callback tx_callback);
/*************************************************
* 功能： 非阻塞式读UART，必须实现
* 输入参数：
* read_buff 输入。读数据指针
* read_size 输入。读数据大小
* rx_callback 输入。读完成回调函数
* 返回：
* 错误码
************************************************/
uos_err_t uos_uart_read_async(uos_uint8_t *read_buff, uos_uint32_t
                              read_size, uart_rx_callback rx_callback);
/*************************************************
* 功能： 改变UART波特率
* 输入参数：
* baud 输入。波特率指定
* 返回：
* 错误码
************************************************/
uos_err_t uos_uart_baud_config(uos_uart_baud_enum baud);
/*************************************************
* 功能： 关闭UART
* 输入参数：
* 无
* 返回：
* 错误码
************************************************/
uos_err_t uos_uart_close(void);
/*************************************************
* 功能： 更改串口发送口
* * 输入参数：
* gpio      把串口变更的目标IO

************************************************/
void UART_TX_PIN_CHANGE(u32 gpio);
/*************************************************
* 功能： 更改串口接收口
* * 输入参数：
* gpio      把串口变更的目标IO

************************************************/
void UART_RX_PIN_CHANGE(u32 gpio);
#endif
