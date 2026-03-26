/********************************************************************************
** Copyright <2022>[-<2023>]                                                    *
** Licensed under the Unisoc General Software License                           *
** version 1.0 (the License);                                                   *
** you may not use this file except in compliance with the License              *
** You may obtain a copy of the License at                                      *
** https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US       *
** Software distributed under the License is distributed on an "AS IS" BASIS,   *
** WITHOUT WARRANTIES OF ANY KIND, either express or implied.                   *
** See the Unisoc General Software License, version 1.0 for more details.       *
********************************************************************************/
/********************************************************************************
** File Name:       uos_deval.h
** Author: Qiang He
** Copyright (C) 2002 Unisoc Communications Inc.
** Description:
********************************************************************************/
#ifndef _UOS_DEVAL_H_
#define _UOS_DEVAL_H_
#include "app_config.h"
#include "uos_type.h"
//#include <unistd.h>
#include "asm/gpio.h"
//#include <stdio.h>
#include <debug.h>
//#include <sys/statfs.h>
#include <fcntl.h>
//#include <syslog.h>
//#include <sys/stat.h>
//#include <stdbool.h>
//#include <termios.h>
#ifdef CONFIG_ARCH_SIM
#else
//#include <nuttx/arch.h>
//#include <nuttx/irq.h>
//#include <nuttx/ioexpander/gpio.h>
//#include <nuttx/ioexpander/ioexpander.h>
//#include <arch/chip/bes_gpio.h>
//#include <uv.h>
//#include "uos_vendor_cfg.h"
#endif

#ifdef __cplusplus
extern "C"
{
#endif

#define PIN_LOW                         0x00
#define PIN_HIGH                        0x01

#define PIN_MODE_OUTPUT                 0x00
#define PIN_MODE_INPUT                  0x01
#define PIN_MODE_INPUT_PULLUP           0x02
#define PIN_MODE_INPUT_PULLDOWN         0x03
#define PIN_MODE_OUTPUT_OD              0x04

#define PIN_IRQ_MODE_RISING             0x00
#define PIN_IRQ_MODE_FALLING            0x01
#define PIN_IRQ_MODE_RISING_FALLING     0x02
#define PIN_IRQ_MODE_HIGH_LEVEL         0x03
#define PIN_IRQ_MODE_LOW_LEVEL          0x04

#define PIN_IRQ_DISABLE                 0x00
#define PIN_IRQ_ENABLE                  0x01

#define PIN_IRQ_PIN_NONE                -1

#ifdef SOC_BES2700iBP
#define MODEM_UART_NAME "uart0"
#define MODEM_DL_SPI_INDEX  (0)
#else
#define MODEM_UART_NAME "uart1"
#define MODEM_DL_SPI_INDEX  (2)
#endif

/* SIO Device type defination */
#define SIO_DEV_TYPE_I2C        0x00
#define SIO_DEV_TYPE_SPI        0x01
#define SIO_DEV_TYPE_UART       0x02
#define SIO_DEV_TYPE_USB        0x03
#define SIO_DEV_TYPE_SOC        0x04
#define SIO_DEV_TYPE_SDIO       0x05
#define SIO_DEV_TYPE_INVALID    0x06

/* SIO Device index defination */
#define SIO_DEV_INDEX_0         0x00
#define SIO_DEV_INDEX_1         0x01
#define SIO_DEV_INDEX_2         0x02
#define SIO_DEV_INDEX_3         0x03
#define SIO_DEV_INDEX_4         0x04
#define SIO_DEV_INDEX_5         0x05
#define SIO_DEV_INDEX_6         0x06
#define SIO_DEV_INDEX_7         0x07
#define SIO_DEV_INDEX_8         0x08
#define SIO_DEV_INDEX_9         0x09
#define SIO_DEV_INDEX_10        0x0A
#define SIO_DEV_INDEX_11        0x0B
#define SIO_DEV_INDEX_12        0x0C
#define SIO_DEV_INDEX_13        0x0D
#define SIO_DEV_INDEX_14        0x0E
#define SIO_DEV_INDEX_15        0x0F
#define SIO_DEV_INDEX_INVALID   0x10

#define SIO_DEV(type, index)        (((uint16_t)(type) << 8) | ((uint16_t)(index) & 0xFF))
#define SIO_DEV_GET_TYPE(sio_dev)   (uint8_t)((uint16_t)(sio_dev) >> 8)
#define SIO_DEV_GET_INDEX(sio_dev)  (uint8_t)((uint16_t)(sio_dev) & 0x00FF)

typedef enum {
    UOS_DEVICE_OPEN = 0x01,
    UOS_DEVICE_CLOSED = 0x02
} DEVICE_STATUS;

typedef enum {
    UOS_GPIO_MODE_OUTPUT = 0x01,
    UOS_GPIO_MODE_INPUT = 0x02,
    UOS_GPIO_MODE_INT = 0x04,
    UOS_GPIO_MODE_ANALOG = 0x08
} uos_gpio_mode;

typedef enum {
    UOS_MODULE_UART0 = SIO_DEV(SIO_DEV_TYPE_UART, SIO_DEV_INDEX_0),
    UOS_MODULE_UART1 = SIO_DEV(SIO_DEV_TYPE_UART, SIO_DEV_INDEX_1),
    UOS_MODULE_UART2 = SIO_DEV(SIO_DEV_TYPE_UART, SIO_DEV_INDEX_2),
    UOS_MODULE_UART3 = SIO_DEV(SIO_DEV_TYPE_UART, SIO_DEV_INDEX_3),
    UOS_MODULE_UART4 = SIO_DEV(SIO_DEV_TYPE_UART, SIO_DEV_INDEX_4),
    UOS_MODULE_UART5 = SIO_DEV(SIO_DEV_TYPE_UART, SIO_DEV_INDEX_5)
} uos_uart_module;

typedef enum {
    UOS_UART_MODE_POLL = 0x01,
    UOS_UART_MODE_INT = 0x02,
    UOS_UART_MODE_DMA = 0x04
} uos_uart_mode;

typedef struct uos_dev_uart_struct {
    uos_uart_module uart_module;
    DEVICE_STATUS dev_status;
} uos_dev_uart;

typedef enum {
    UOS_MODULE_SPI0 = SIO_DEV(SIO_DEV_TYPE_SPI, SIO_DEV_INDEX_0),
    UOS_MODULE_SPI1 = SIO_DEV(SIO_DEV_TYPE_SPI, SIO_DEV_INDEX_1),
    UOS_MODULE_SPI2 = SIO_DEV(SIO_DEV_TYPE_SPI, SIO_DEV_INDEX_2),
    UOS_MODULE_SPI3 = SIO_DEV(SIO_DEV_TYPE_SPI, SIO_DEV_INDEX_3),
    UOS_MODULE_SPI4 = SIO_DEV(SIO_DEV_TYPE_SPI, SIO_DEV_INDEX_4),
    UOS_MODULE_SPI5 = SIO_DEV(SIO_DEV_TYPE_SPI, SIO_DEV_INDEX_5)
} uos_spi_module;

typedef enum {
    UOS_SPI_MODE_POLL = 0x01,
    UOS_SPI_MODE_INT = 0x02,
    UOS_SPI_MODE_DMA = 0x04
} uos_spi_mode;

typedef struct uos_dev_spi_struct {
    uos_spi_module spi_module;
    DEVICE_STATUS dev_status;
} uos_dev_spi;

typedef enum {
    SPI_STATUS_OK = 0,
    SPI_STATUS_BUSY,
    SPI_STATUS_FAIL,
    SPI_STATUS_INV,
} SpiStatusEnum;

typedef enum {
    UART_STATUS_OK = 0,
    UART_STATUS_BUSY,
    UART_STATUS_READY,
    UART_STATUS_DENY,
    UART_STATUS_FAIL,
    UART_STATUS_INV,
} UartStatusEnum;

typedef void (*uos_gpio_isr_handler)(void *args);
typedef void (*spi_callback)(uos_uint16_t spi_module, SpiStatusEnum status, uos_uint8_t *data_buff, uos_uint16_t data_size);

typedef void (*uart_rx_callback)(uos_uint16_t uart_module, UartStatusEnum status, uos_uint8_t *rx_buff, uos_uint16_t rx_size);
typedef void (*uart_tx_callback)(uos_uint16_t uart_module, UartStatusEnum status, uos_uint8_t *tx_buff);

#define TO_PIN(PORT, MASK) ((PORT*8) + (MASK))

#ifdef CONFIG_ARCH_SIM
#define MODEM_REMOTE_REQ    TO_PIN(5, 2)
//#define SPI_AP_REQ_PIN      TO_PIN(5, 2)
#define SPI_CP_REQ_PIN      66
#define SPI_AP_ACK_PIN      73
#define SPI_CP_ACK_PIN      67
#define MODEM_ALIVE_PIN     71
#define MODEM_RST_PIN       36
#define ST_UART2_TX_PIN     21
#define ST_UART2_RX_PIN     20
#define MODEM_SPI_MOSI_PIN  40
#define MODEM_SPI_MISO_PIN  37
#define MODEM_SPI_CLK_PIN   39
#define MODEM_SPI_CS_PIN    30
#define MODEM_PBINT         TO_PIN(5, 4)
#define MODEM_EN            0
#else
typedef struct uv_async_data_uart {
    uv_poll_t handle;
    uart_rx_callback cb;
    uos_uint8_t *buffer;
    uos_uint16_t len;
    uos_uint16_t offset;
} uv_async_data_t_uart;

typedef struct uv_async_data_spi {
    uv_poll_t handle;
    spi_callback cb_write;
    uos_uint8_t *buffer_write;
    uos_uint16_t len_write;
    spi_callback cb_read;
    uos_uint8_t *buffer_read;
    uos_uint16_t len_read;
} uv_async_data_t_spi;

typedef enum uos_uart_baudrate {
    UOS_BAUD_9600,
    UOS_BAUD_115200,
    UOS_BAUD_921600,
    UOS_BAUD_1000000,
    UOS_BAUD_2000000,
} uos_uart_baud_enum;

enum gpio_setting {
    OLD_GPIO_SETTING,
    NEW_GPIO_SETTING
};

#define SPI_AP_REQ_PIN    IO_PORTG_00
//extern uos_uint32_t SPI_AP_REQ_PIN;
extern uos_uint32_t SPI_CP_REQ_PIN;
extern uos_uint32_t SPI_AP_ACK_PIN;
extern uos_uint32_t SPI_CP_ACK_PIN;
extern uos_uint32_t MODEM_ALIVE_PIN;
extern uos_uint32_t MODEM_RST_PIN;
extern uos_uint32_t MODEM_RST_PMIC_PIN;

/*
#define SPI_AP_REQ_PIN      GPIO_PIN8_2//PIN_PMU_P0_7
#define SPI_CP_REQ_PIN      GPIO_PIN6_0//PIN_PMU_P1_2
#define SPI_AP_ACK_PIN      GPIO_PIN8_4//PIN_PMU_P0_1
#define SPI_CP_ACK_PIN      GPIO_PIN8_5//PIN_PMU_P0_0
#define MODEM_ALIVE_PIN     GPIO_PIN6_2//PIN_PMU_P0_3
#define MODEM_RST_PIN       GPIO_PIN4_5//PIN_PMU_P0_2
*/
#define ST_UART2_TX_PIN     11
#define ST_UART2_RX_PIN     11
#define MODEM_SPI_MOSI_PIN  11
#define MODEM_SPI_MISO_PIN  11
#define MODEM_SPI_CLK_PIN   GPIO_PIN72
#define MODEM_SPI_CS_PIN    GPIO_PIN73
#define MODEM_PBINT_PIN     PIN_PMU_P4_1
#define MODEM_EN_PIN        PIN_PMU_P3_3

#define USB_SW_CTRL_PIN     PIN_PMU_P6_3  /* Drive CB low to connect COM_ to NC_. Drive CB high to connect COM_ to NO_. */
#endif

#define USE_UNISOC_GPIO_FRAMEWORK   (1) /* code add by tao.shi@2023.11.17 */
#if USE_UNISOC_GPIO_FRAMEWORK
typedef enum {
    GPIO_UNISOC_AP_REQ = 0,
    GPIO_UNISOC_AP_ACK,
    GPIO_UNISOC_CP_REQ,
    GPIO_UNISOC_CP_ACK,
    GPIO_UNISOC_MODEM_EN,
    GPIO_UNISOC_PBINT,
    GPIO_UNISOC_MAX,
} unisoc_gpio_e;

uos_err_t uos_gpio_init_ex(unisoc_gpio_e id, uos_gpio_mode pin_mode, void(*isr_handler)(void *args), void *args);
uos_err_t uos_gpio_write_ex(unisoc_gpio_e id, uos_int32_t value);
uos_int32_t uos_gpio_read_ex(unisoc_gpio_e id);
uos_err_t uos_gpio_disable_ex(unisoc_gpio_e id);
int uos_gpio_get_pin_ex(unisoc_gpio_e id);
uos_err_t uos_gpio_int_trigger_set_ex(unisoc_gpio_e id, uos_uint32_t trigger_style);
#endif

uos_err_t uos_board_gpio_config(void);
uos_err_t uos_gpio_init(uos_uint32_t pin, uos_gpio_mode pin_mode, void (*isr_handler)(void *args), void *args);
uos_err_t uos_gpio_write(uos_uint32_t pin, uos_int32_t value);
uos_int32_t uos_gpio_read(uos_uint32_t pin);
uos_err_t uos_gpio_disable(uos_uint32_t pin);
uos_err_t uos_gpio_int_trigger_set(uos_uint32_t pin, uos_uint32_t trigger_style);

uos_err_t uos_uart_open(void);
uos_uint32_t uos_uart_write(uos_uint8_t *write_buff, uos_uint32_t write_size);
uos_uint32_t uos_uart_read(uos_uint8_t *read_buff, uos_uint32_t read_size);
uos_err_t uos_uart_write_async(uos_uint8_t *write_buff, uos_uint32_t write_size, uart_tx_callback tx_callback);
uos_err_t uos_uart_read_async(uos_uint8_t *read_buff, uos_uint32_t read_size, uart_rx_callback rx_callback);
uos_err_t uos_uart_baud_config(uos_uart_baud_enum baud);
uos_err_t uos_uart_close(void);

uos_err_t uos_spi_open(void);
uos_uint32_t uos_spi_write(uos_uint8_t *write_buff, uos_uint32_t write_size);
uos_uint32_t uos_spi_read(uos_uint8_t *read_buff, uos_uint32_t read_size);
uos_uint32_t uos_spi_write_read(uos_uint8_t *write_buff, uos_uint8_t *read_buff, uos_uint32_t size);
uos_err_t uos_spi_write_async(uos_uint8_t *write_buff, uos_uint32_t write_size, spi_callback tx_callback);
uos_err_t uos_spi_read_async(uos_uint8_t *read_buff, uos_uint32_t read_size, spi_callback rx_callback);

uos_err_t uos_spi_abort(void);
uos_err_t uos_spi_deinit(void);
uos_int8_t uos_spi_get_cs_state(void);
uos_err_t uos_spi_close(void);


extern void syslog(int  level, const char *format, int a, ...);
#define UOS_LOG_DEBUG(fmt, para_num, ...)   do { syslog(1, fmt,(int)para_num, ##__VA_ARGS__); } while(0)
#define UOS_LOG_WARNING(fmt, para_num, ...) do { syslog(2, fmt,(int)para_num, ##__VA_ARGS__); } while(0)
#define UOS_LOG_INFO(fmt, para_num, ...)    do { syslog(3, fmt,(int)para_num, ##__VA_ARGS__); } while(0)
#define UOS_LOG_ERROR(fmt, para_num, ...)   do { syslog(4, fmt,(int)para_num, ##__VA_ARGS__); } while(0)
#define UOS_LOG_PRINTF(fmt, para_num, ...)  do { syslog(5, fmt,(int)para_num, ##__VA_ARGS__); } while(0)

#define UOS_ASSERT(X)    ASSERT(X)
/********************************************************************************************/

/* file I/O, POSIX.1 standard, unbuffered I/O */
/* flags: O_RDONLY: 只读打开
          O_WRONLY: 只写打开
          O_RDWR:   读写打开
          O_APPEND: 每次写追加到文件尾端
          O_CREAT:  若文件不存在则创建
          O_SYNC:   每次write需等待物理I/O操作完成
          O_TRUNC:  若文件存在，且为只写或读写成功打开，则将其长度截断为0
 */
uos_int32_t uos_open(const char *file, int flags, uos_int32_t mode);
uos_int32_t uos_close(uos_int32_t fd);
uos_int32_t uos_read(uos_int32_t fd, void *buf, uos_uint32_t len);
uos_int32_t uos_write(uos_int32_t fd, const void *buf, uos_uint32_t len);
/* whence: SEEK_SET: 将该文件的偏移量设置为距文件开始处offset个字节
           SEEK_CUR: 将该文件的偏移量设置为当前值加offset，offset可为正或负
           SEEK_END: 将该文件的偏移量设置为文件长度加offset,offset可为正或负
 */
uos_int32_t uos_lseek(uos_int32_t fd, uos_uint32_t offset, uos_int32_t whence);
uos_int32_t uos_unlink(const char *path);
uos_int32_t uos_stat(const char *path, struct stat *buf);
uos_int32_t uos_fsync(uos_int32_t fd);
/********************************************************************************************/
uos_uint32_t uos_get_hw_version(uos_uint8_t board);
#if 0
struct uos_ringbuffer {
    uos_uint8_t *uos_buffer_ptr;
    uos_uint16_t uos_read_mirror : 1;
    uos_uint16_t uos_read_index : 15;
    uos_uint16_t uos_write_mirror : 1;
    uos_uint16_t uos_write_index : 15;
    uos_int16_t uos_buffer_size;
};

enum uos_ringbuffer_state {
    UOS_RINGBUFFER_EMPTY,
    UOS_RINGBUFFER_FULL,
    UOS_RINGBUFFER_NOT_EMPTY,
};
#endif

/* ring buffer */
struct uos_ringbuffer {
    uos_uint8_t *buffer_ptr;
    /* use the msb of the {read,write}_index as mirror bit. You can see this as
     * if the buffer adds a virtual mirror and the pointers point either to the
     * normal or to the mirrored buffer. If the write_index has the same value
     * with the read_index, but in a different mirror, the buffer is full.
     * While if the write_index and the read_index are the same and within the
     * same mirror, the buffer is empty. The ASCII art of the ringbuffer is:
     *
     *          mirror = 0                    mirror = 1
     * +---+---+---+---+---+---+---+|+~~~+~~~+~~~+~~~+~~~+~~~+~~~+
     * | 0 | 1 | 2 | 3 | 4 | 5 | 6 ||| 0 | 1 | 2 | 3 | 4 | 5 | 6 | Full
     * +---+---+---+---+---+---+---+|+~~~+~~~+~~~+~~~+~~~+~~~+~~~+
     *  read_idx-^                   write_idx-^
     *
     * +---+---+---+---+---+---+---+|+~~~+~~~+~~~+~~~+~~~+~~~+~~~+
     * | 0 | 1 | 2 | 3 | 4 | 5 | 6 ||| 0 | 1 | 2 | 3 | 4 | 5 | 6 | Empty
     * +---+---+---+---+---+---+---+|+~~~+~~~+~~~+~~~+~~~+~~~+~~~+
     * read_idx-^ ^-write_idx
     *
     * The tradeoff is we could only use 32KiB of buffer for 16 bit of index.
     * But it should be enough for most of the cases.
     *
     * Ref: http://en.wikipedia.org/wiki/Circular_buffer#Mirroring */
    uos_uint16_t read_mirror : 1;
    uos_uint16_t read_index : 15;
    uos_uint16_t write_mirror : 1;
    uos_uint16_t write_index : 15;
    /* as we use msb of index as mirror bit, the size should be signed and
     * could only be positive. */
    uos_int16_t buffer_size;
};

enum uos_ringbuffer_state {
    UOS_RINGBUFFER_EMPTY,
    UOS_RINGBUFFER_FULL,
    UOS_RINGBUFFER_NOT_EMPTY,
};


void uos_ringbuffer_init(struct uos_ringbuffer *rb, uos_uint8_t *pool, uos_int16_t size);
void uos_ringbuffer_reset(struct uos_ringbuffer *rb);
uos_uint32_t uos_ringbuffer_put(struct uos_ringbuffer *rb, const uos_uint8_t *ptr, uos_uint16_t length);
uos_uint32_t uos_ringbuffer_put_force(struct uos_ringbuffer *rb, const uos_uint8_t *ptr, uos_uint16_t length);
uos_uint32_t uos_ringbuffer_putchar(struct uos_ringbuffer *rb, const uos_uint8_t ch);
uos_uint32_t uos_ringbuffer_putchar_force(struct uos_ringbuffer *rb, const uos_uint8_t ch);
uos_uint32_t uos_ringbuffer_get(struct uos_ringbuffer *rb, uos_uint8_t *ptr, uos_uint16_t length);
uos_uint32_t uos_ringbuffer_getchar(struct uos_ringbuffer *rb, uos_uint8_t *ch);
uos_uint32_t uos_ringbuffer_data_len(struct uos_ringbuffer *rb);
struct uos_ringbuffer *uos_ringbuffer_create(uos_uint16_t length);
void uos_ringbuffer_destroy(struct uos_ringbuffer *rb);

/** return the size of empty space in rb */
#if 0
#define uos_ringbuffer_space_len(rb) ((rb)->uos_buffer_size - uos_ringbuffer_data_len(rb))
#endif
#define uos_ringbuffer_space_len(rb) ((rb)->buffer_size - uos_ringbuffer_data_len(rb))
/********************************************************************************************/
extern uos_err_t uos_memset_s(void *dest, uos_uint32_t destMax, int c, uos_uint32_t count);
extern uos_err_t uos_memcpy_s(void *dest, uos_uint32_t destMax, const void *src, uos_uint32_t count);
extern uos_err_t uos_memmove_s(void *dest, uos_uint32_t destMax, const void *src, uos_uint32_t count);

#define uos_memset                   memset
#define uos_memcpy(dst, src, count)  memcpy(dst, src, count)
#define uos_memmove(dst, src, count) uos_memmove_s(dst, count, src, count)
#define uos_memcmp(cs, ct, count)    memcmp(cs, ct, count)

#define uos_strstr(s1, s2)           strstr(s1, s2)
#define uos_strcasecmp(a, b)         strcasecmp(a, b)
#define uos_strcpy(dst, src)         strcpy(dst, src)
#define uos_strncpy(dst, src, n)     strncpy(dst, src, n)
#define uos_strncmp(cs, ct, count)   strncmp(cs, ct, count)
#define uos_strcmp(cs, ct)           strcmp(cs, ct)
#define uos_strlen(s)                strlen(s)
#define uos_strcat(dst,src)          strcat(dst,src)
#define uos_snprintf(buf,size,format, ...) snprintf(buf,size,format,##__VA_ARGS__)
#define uos_sprintf(buf,format, ...) sprintf(buf,format,##__VA_ARGS__)
#define uos_atoi(s)                  atoi(s)

#define UOS_SECUREC_MEMORY_NO_OVERLAP(uos_dest, uos_src, uos_count) \
    (((uos_src) < (uos_dest) && ((const char*)(uos_src) + (uos_count)) <= (char*)(uos_dest)) || \
     ((uos_dest) < (uos_src) && ((char*)(uos_dest) + (uos_count)) <= (const char*)(uos_src)))

#define UOS_SECUREC_MEMORY_IS_OVERLAP(uos_dest, uos_src, uos_count) \
    (((uos_src) < (uos_dest) && ((const char*)(uos_src) + (uos_count)) > (char*)(uos_dest)) || \
     ((uos_dest) < (uos_src) && ((char*)(uos_dest) + (uos_count)) > (const char *)(uos_src)))

#define UOS_SECUREC_STRING_NO_OVERLAP(uos_dest, uos_src, len) \
    (((uos_src) < (uos_dest) && ((uos_src) + (len)) < (uos_dest)) || \
     ((uos_dest) < (uos_src) && ((uos_dest) + (len)) < (uos_src)))

#define UOS_SECUREC_STRING_IS_OVERLAP(uos_dest, uos_src, len) \
    (((uos_src) < (uos_dest) && ((uos_src) + (len)) >= (uos_dest)) || \
     ((uos_dest) < (uos_src) && ((uos_dest) + (len)) >= (uos_src)))

#define UOS_SECUREC_CAT_STRING_IS_OVERLAP(uos_dest, destLen, uos_src, srcLen) \
    (((uos_dest) < (uos_src) && ((uos_dest) + (destLen) + (srcLen)) >= (uos_src)) || \
     ((uos_src) < (uos_dest) && ((uos_src) + (srcLen)) >= (uos_dest)))

#define UOS_SECUREC_MEMSET_PARAM_CHECK(uos_dest, destMax, uos_count) ((destMax) <= 0x7FFFFFFF && \
                                                                      (uos_dest) != NULL && (uos_count) <= (destMax))

#define UOS_SECUREC_MEMCPY_PARAM_CHECK(uos_dest, destMax, uos_src, uos_count) ((uos_count) <= (destMax) && \
                                                                               (uos_dest) != NULL && (uos_src) != NULL && (uos_count) > 0 && \
                                                                               UOS_SECUREC_MEMORY_NO_OVERLAP((uos_dest), (uos_src), (uos_count)))

#ifdef __cplusplus
}
#endif

#endif
