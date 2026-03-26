#ifndef __CMUX_API_H__
#define __CMUX_API_H__

#include "uos_type.h"

/*---------------------------------------------------------------------------*/
/*                         TYPE AND CONSTANT                                 */
/*---------------------------------------------------------------------------*/
// priority from hight to low
typedef enum {
    CMUX_CHANNEL_CTRL,
    CMUX_CHANNEL_RPC,
    CMUX_CHANNEL_DIAG, //uart
    CMUX_CHANNEL_NV,
    CMUX_CHANNEL_AT,
    CMUX_CHANNEL_AT_VSIM_TX,
    CMUX_CHANNEL_AT_VSIM_RX,
    CMUX_CHANNEL_AT_LOG, //AT for log(SPI)
    CMUX_CHANNEL_DEBUG_MODEM_STATUS, //modem status(SPI)
    CMUX_CHANNEL_DEBUG_DUMP, //modem dump/minidump(UART)
    CMUX_CHANNEL_IP1, //IP network
    CMUX_CHANNEL_IP2, //IP network
    CMUX_CHANNEL_IP3, //IP network
    CMUX_CHANNEL_IP4, //IP network
    CMUX_CHANNEL_IP5, //IP network
    CMUX_CHANNEL_LOG, //modem log(SPI)
    //following is used for cmux unit testing
    CMUX_CHANNEL_TEST1,
    CMUX_CHANNEL_TEST2,
    CMUX_CHANNEL_LOOPBACK_TEST1,
    CMUX_CHANNEL_LOOPBACK_TEST2,
    CMUX_CHANNEL_LOOPBACK_TEST3,
    CMUX_CHANNEL_LOOPBACK_TEST4,
    CMUX_CHANNEL_LOOPBACK_TEST5,
    CMUX_CHANNEL_AUDIO,
    CMUX_CHANNEL_BOOT_LOG,
#ifdef HIGH_SPEED_HTTP_DOWNLOAD
    CMUX_CHANNEL_AUDIO_CTL,
#endif /*HIGH_SPEED_HTTP_DOWNLOAD*/
    CMUX_CHANNEL_MAX,
} cmux_channel_e;

typedef enum {
    CMUX_DEVICE_SPI,
    CMUX_DEVICE_UART,
    CMUX_DEVICE_MAX
} cmux_device_e;

typedef enum {
    CMUX_RET_OK = 0,
    CMUX_RET_BUSY  = -1,
    CMUX_RET_INVALID_PARAMETER = -2,
    CMUX_RET_ERROR = -3,
    CMUX_RET_NOT_READY = -4,
} cmux_ret_error_e;

typedef struct {
    uint32  send_bytes;
    uint32  send_packets;
    //uint32  send_lost_bytes;
    //uint32  send_lost_packets;
    uint32  send_seq;
    uint32  recv_bytes;
    uint32  recv_packets;
    uint32  recv_lost_bytes;
    uint32  recv_lost_packets;
    uint32  recv_seq;
} cmux_stat_t;

#define MAX_PACKET_DATA_SIZE 2000  //tao.shi  2000

//for spi transfer layer use driver used
void spi_data_coming(uint8 *p, uint32 len);

typedef uint8 *(*cmux_alloc_func)(uint32 size, cmux_channel_e channel);
typedef void (*cmux_free_func)(uint8 *p);
//for modem abnomal callback
typedef void (*CMUX_ERROR_CB_T)(void);

typedef void (*CMUX_READY_CB_T)(uint16 cmux_status);

/**
 * param[in]:
          data: data address in packets, not include cmux header
          data_len: data length
          channel:  channel
*/
typedef void (*cmux_data_come_callback_func)(uint8 *data, uint32 data_len, uint32 channel);

/** cmux_open
 * @brief :               cmux_open()
                          open a spi channel, enable channel read/write, not support multi user to open at the same time
 * @param[in] :
      channel:
                          channel number enum
      callback:
                          if callback is not NULL, it will be called when data is comming, cmux_read() will aways return NULL, and
                          should free memory by user, see example;
      callback_alloc/
      callback_free:
                          channel memory alloc/free function, if NULL, cmux_buffer_alloc/cmux_buffer_free will be used
                          alloc/free must be matched, use channel private alloc/free fucntion or set to NULL.
                          on modem side, IP channel should use daps's alloc/free
                          channel private alloc/free must pre-reserve at least 24 bytes memory space for cmux packet head.
                          alloc() return value and free() paramter is the address of data(not include pre-reserved memory)
 * @return :
        TRUE:             sucess
        FALSE:            failed because has been opened, or invalid channel
 *exmaple:
                     static void at_recieve_handle(uint8 *data, uint32 data_len, uint32 callback_userdata)
                     {
                        assert(0x11223344==callback_userdata)
                        my_free(data); //must use selfdefined free function to match cmux_open() parameter
                     }
                     static void at_recieve_handle2(uint8 *data, uint32 data_len, uint32 callback_userdata)
                     {
                        assert(0x12345678==callback_userdata)
                        cmux_buffer_free(data); //must default memory free funciuotn to match cmux_open() parameter
                     }

                     void main()
                     {
                        cmux_open(CMUX_CHANNEL_AT_SIM1_REPORT, at_recieve_handle, 0x11223344, my_malloc, my_free); //selfdefined mem alloc/free
                        cmux_open(CMUX_CHANNEL_AT_SIM0_REPORT, at_recieve_handle2, 0x12345678, NULL, NULL); //default mem alloc/free
                     }
*/
BOOLEAN cmux_open(cmux_device_e device,
                  cmux_channel_e channel,
                  cmux_data_come_callback_func callback,
                  cmux_alloc_func              callback_alloc,
                  cmux_free_func               callback_free);

/** cmux_close
 * @brief :               cmux_close()
                          close a spi channel
 * @param[in] :
      channel:
                          channel number enum
 * @return :
        TRUE:             sucess
        FALSE:            failed because has been opened, or invalid channel
 *exmaple:
                     void main()
                     {
                        cmux_close(CMUX_CHANNEL_IP1);
                     }
*/
BOOLEAN cmux_close(cmux_device_e device, cmux_channel_e channel);

/**
 * @brief :               cmux_write()
                          write buffer asynchronously
 * @param[in] :
      channel:
                          channel number enum
      data:
                          data is the return value of callback_alloc/cmux_buffer_alloc specified at cmux_open().
                          data can be used by caller after cmux_write() .
                          data will be free by cmux internal after send to spi.
                          cmux free data by callback_free/cmux_buffer_free specified at cmux_open().
      len:
                          length of data
      channel_userdata:
                          channel user data, will be send in cmux head on source side.
                          on sink side, callback or cmux_read() will return it, see cmux_open()'s callback
 * @return :              cmux_ret_error_e
        --------------------------------------
        when return CMUX_RET_BUSY, failed because send queue is full.
                          default channel is limit to 16 in send queue.
                          IP channel limit to 64, LOG channel limit to 128
                          the data is not be freed in cmux, user can use data to write again

                          while(1) {
                            data = cmux_buffer_alloc/callback_alloc()
                            if (data) {
                                break;
                            }
                            msleep(10);
                            log("wait buffer");
                          }
                          while(1) {
                            int ret = cmux_write(channel, data, len);
                            if (CMUX_RET_BUSY==ret) {
                                log("channel busy, can modify data[]")
                                msleep(10);
                            } else if(CMUX_RET_OK==ret) {
                                log("write ok, can't used data[] any more")
                                break;
                            } else {
                               log("write failed, other error")
                            }
                          }
*/
int cmux_write(cmux_device_e device, cmux_channel_e channel, uint8 *data, uint32 len);

/**
 * @brief :               cmux_read()
                          read data from channel, if  callback is NULL
 * @param :
      channel:
                          channel number enum
      readed_len:
                          return length of data
      len:
                          length of data
      timeout_ms:
                          wait timeout_ms millsenconds untill data is coming,
                          if millsenconds is -1, wait for ever until data was received.
      channel_userdata:
                          if not NULL, return channel user data, see cmux_write().

 * @return :
                          address of data, the buffer is alloced by cmux internal,
                          data is alloced using callback_alloc() or cmux_buffer_alloc() specified at cmux_open().
                          after use the return value, must call callback_alloc() or cmux_buffer_alloc() to free the memory
                          if NULL calback is specified at cmux_open(), use the default callback_free function cmux_buffer_free()
                          to free the data memory.
                          return NULL if timeout,
                          return NULL if  a non-NULL cmux_data_come_callback_func specified at cmux_open().

  example
                          uint32 len;
                          uint8 *data;
                          cmux_open(MY_CHANNEL, NULL, 0, NULL, NULL);
                          data = cmux_read(MY_CHANNEL, &len, -1);
                          cmux_buffer_free(data);

                          uint32 len;
                          uint8 *data;
                          cmux_open(IP_CHANNEL, NULL, 0, ip_malloc, ip_freee);
                          data = cmux_read(MY_CHANNEL, &len, -1);
                          ip_free(data);
*/
uint8 *cmux_read(cmux_device_e device, cmux_channel_e channel, uint32 *readed_len, uint32 timeout_ms);

/**
 * @brief :               cmux_buffer_alloc()
                          alloc a buffer with cmux head space pre-reserved
 * @param :
      size:
                          buffer data size, not include cmux head.
                          max size is MAX_PACKET_DATA_SIZE
      channel;
                          channel enum, internal use this value to give some channel enough buffer number
 * @return :
                          return the buffer address from data part, not include pre-reserved cmux head
                          return NULL is no free buffer.
                          return NULL is size big than MAX_PACKET_DATA_SIZE

  example

                          cmux_buffer_alloc(32,MY_CHANNEL);
                          data = cmux_read(MY_CHANNEL, &len, -1);
                          cmux_buffer_free(data);

                          uint32 len;
                          uint8 *data;
                          cmux_open(IP_CHANNEL, NULL, 0, ip_malloc, ip_freee);
                          data = cmux_read(MY_CHANNEL, &len, -1);
                          ip_free(data);
*/
uint8 *cmux_buffer_alloc(uint32 size, cmux_channel_e channel);

/**
 * @brief : cmux_buffer_free()
           free buffer alloced by cmux_buffer_alloc
 * @param :
           p:
             non-NULL value return at cmux_buffer_alloc()
*/
void cmux_buffer_free(uint8 *p);

/**
 * @brief :               cmux_write_user_memory()
                          write user buffer synchronously, data will be copied before send to spi,
                          because of data-copy this fuction can't be used frequently. exmaple log & ip channel
                          should not use this fucntion and should use cmux_write() instead.
 * @param[in] :
      channel:
                          channel number enum
      data:
                          data is the caller's self memory
                          data will not be modify or free by cmux internal.
                          cmux will copy the data to send, if data is too big, cmux split and copy to many small packet then send out
      len:
                          length of data, must <= 2048
 * @return :
        TRUE:             put in aray sucess
        FALSE:            put in aray fail

 * exmaple:
                           cmux_write_user_memory(channel, "AT\r\n", 4);

                           p = malloc(1024);
                           cmux_write_user_memory(channel, p, 1024);
                           os_free(p);

*/
BOOLEAN cmux_write_user_memory(cmux_device_e device, cmux_channel_e channel, uint8 *data, uint32 len);

/**
 * @brief :               cmux_write_user_memory_sync()
                          write user buffer synchronously, data will be copied before send to spi,
                          because of data-copy this fuction can't be used frequently. exmaple log & ip channel
                          should not use this fucntion and should use cmux_write() instead.
 * @param[in] :
      channel:
                          channel number enum
      data:
                          data is the caller's self memory
                          data will not be modify or free by cmux internal.
                          cmux will copy the data to send, if data is too big, cmux split and copy to many small packet then send out
      len:
                          length of data, must <= 2048
 * @return :
        TRUE:             write spi sucess
        FALSE:            write spi fail

 * exmaple:
                           cmux_write_user_memory_sync(channel, "AT\r\n", 4);

                           p = malloc(1024);
                           cmux_write_user_memory_sync(channel, p, 1024);
                           os_free(p);

*/
BOOLEAN cmux_write_user_memory_sync(cmux_device_e device, cmux_channel_e channel, uint8 *data, uint32 len);

/**
 * @brief :               cmux_stat()
                          stat channel info
                          because of data copy the fuction can't be used frequently. exmaple log & ip channel
                          should not use this fucntion and should use cmux_write() instead.
 * @param[in/out] :
      stat:               return channel info
*/
BOOLEAN cmux_stat(cmux_device_e device, cmux_channel_e channel, cmux_stat_t *stat);
BOOLEAN cmux_stat_peer(cmux_device_e device, cmux_channel_e channel, cmux_stat_t *stat);

/**
 * @brief :               cmux_build_head_data()
                          build mux header data
                          build raw mux protocol head data
                          should not use this fucntion and should use cmux_write() instead.
 * @param[in/out] :
      channel:
                          channel number enum
      head_data:          head data buffer
      head_data_len:      head data len
      userdata_len:      user data len
 * @return :
        :             mux head data length

*/
uint32 cmux_build_head_data(cmux_channel_e channel, uint8 *head_data, uint32 head_data_len, uint32 userdata_len);

/*
*@brief unblock cmux_read immediately
*/
void cmux_unblock_read(cmux_device_e device, cmux_channel_e channel);

void cmux_error_reg_callback(CMUX_ERROR_CB_T callback);

void cmux_ready_reg_callback(CMUX_READY_CB_T callback);
#endif