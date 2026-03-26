#ifndef _UOS_VENDOR_CFG_H_
#define _UOS_VENDOR_CFG_H_

#define _LITTLE_ENDIAN
#define MD_WRITE_MIDST_USE_OFFSET

/* support INT FLIP FOR MODEM ALIVE PIN */
#define INTFLIP_SUPPORT
#define PLATFORM_ALIAS_UMS9117L
#define BB_DRAM_TYPE_48MB
#define NANDLESS_DRV_CUT_SUPPORT

#define MAX_PKT_SIZE        (0x1000)
#define PACKET_HEADER_SIZE  (4)
#define PACKET_MAX_NUM      (3)
#define DL_ROMCODE_PACKET_SIZE (0x400 - 80)
#define DL_FDL1_PACKET_SIZE (0x840)
#define DL_FDL2_PACKET_SIZE (MAX_PKT_SIZE - 40)

#define DL_FILENAME_BUF_SIZE    (128)
#define DL_CMD_RETRY_TIMES      (3)

#define DL_CMD_CHECKBAUD_RETRY_TIMES (300)

#define DL_CMD_CHECKBAUD_TIMEOUT    (5)
#define DL_FILENAME_BUF_SIZE        (128)
#define DL_CMD_FDL1_HDLC_FLAG_CNT   (3)
#define DL_CMD_FDL2_HDLC_FLAG_CNT   (1)
#define DL_CMD_TIMEOUT              (5000)
#define DL_CMD_MDLOAD_UPGRADE_TIMEOUT      (100000)

#define DL_CHANNEL_SPI_RETRY_TIMES  (1)
#define DL_CHANNEL_SPI_TIMEOUT (DL_CMD_TIMEOUT - 50)
#define DL_CHANNEL_SPI_MDLOAD_UPGRADE_TIMEOUT (DL_CMD_MDLOAD_UPGRADE_TIMEOUT - 50)

#define DL_CHAN_SPI_DEV     "spi2"
#define DL_CHAN_FIFO_SIZE   (2048)
#define DL_SPI_CS_CHECK_CNT (2000)

#define MD_FDL1_ADDR (0x6200)
#define MD_FDL1_SIZE (0x8000)

#define MD_KERNEL_HEADER_SIZE       (0x200)
#define MD_TGDSP_START_ADDR_OFFSET  (0x20000)
#define MD_LTEDSP_START_ADDR_OFFSET (0x48000)
#define MD_USE_LZ4 (1)

#define FDL_PACKETPARSE_THREAD_STACK_SIZE   (1024) //debug Tianyu.Yang
#define FDL_PACKETPARSE_THREAD_TICK         (20)

#define FDL_CHAN_SPI_THREAD_STACK_SIZE      (1024) //debug Tianyu.Yang
#define FDL_CHAN_SPI_THREAD_TICK            (10)

#define FDL_CHAN_UART_BAUD_DEFAULT          (115200)
#define FDL_CHAN_SPI_BAUD_DEFAULT           (48000000)
#define FDL_THREAD_SCHEDULE_MAX_TIMES       (5000)

#define MD_VERSION_STR  "MOCOR_20A_WATCH"

#define FDL_MODEM_SPI_USE_32BIT /* dload spi use 32bit */
#define MD_FW_FILE      "/vendor/modem/modem.bin"
#define MD_DELTANV_PATH "/vendor/modem/"
#define MD_NV_PATH      "/nv/"
#define MD_FIXNV_FILE   "/nv/fixnv.bin"
#define MD_FIXNV_NEW_TEST_FILE "/nv/fixnv_new.bin"
#define MD_FIXNV2_FILE  "/nv/fixnv2.bin"
#define MD_RUNNV1_FILE  "/nv/runnv1.bin"
#define MD_RUNNV2_FILE  "/nv/runnv2.bin"
#define KERNEL_SECTION  1

#define BACKUP_ID_CFG_FILE 1
#ifdef  BACKUP_ID_CFG_FILE /*nv_merge BACKUP_ID_CFG_FILE feature*/
#define NV_CFG_FILE_PATH "/vendor/modem/nvmerge.cfg"
#endif

#define uos_custom_sleep()   uos_thread_sleep(uos_ms_to_tick(50))
#define uos_spec_sleep()
#define UOS_CHECK_SPI_VALID()
//#define DEBUG_TICK_COUNT_SUPPORT
//#define TEST_POINT_SUPPORT
#define OS_VELA_SUPPORT
#define MODEM_LOADER_TEST_THREAD_PRIORITY       (9)

/* dload module customization config */
#define FDL_PACKETPARSE_THREAD_PRIORITY         (6)
#define FDL_CHAN_SPI_THREAD_PRIORITY            (11)
#define FDL_CALL_DLOAD_THREAD_PRIORITY          (8)
#define FDL_CALL_DLOAD_HIGH_THREAD_PRIORITY     (7)
#define FDL_LIBUV_THREAD_PRIORITY               (151)

#define DL_FDL_WRITE_MALLOC_SUPPORT         1  /* 0: adapter dbgspi(without dma) 1:adapter spi dma */
#define DL_SPI_THREAD_CHEAK_CS_SUPPORT      1  /* 0: adapter dbgspi(without dma) 1:adapter spi dma */

#endif //_UNISOC_VENDOR_H_
