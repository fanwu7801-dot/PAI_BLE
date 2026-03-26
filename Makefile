
# make 编译并下载
# make VERBOSE=1 显示编译详细过程
# make clean 清除编译临时文件
#
# 注意： Linux 下编译方式：
#     1. 从 http://pkgman.jieliapp.com/doc/all 处找到下载链接
#     2. 下载后，解压到 /opt/jieli 目录下，保证
#       /opt/jieli/common/bin/clang 存在（注意目录层次）
#     3. 确认 ulimit -n 的结果足够大（建议大于8096），否则链接可能会因为打开文件太多而失败
#       可以通过 ulimit -n 8096 来设置一个较大的值
#

# 工具路径设置
ifeq ($(OS), Windows_NT)
# Windows 下工具链位置
TOOL_DIR := C:/JL/pi32/bin
CC    := clang.exe
CXX   := clang.exe
LD    := pi32v2-lto-wrapper.exe
AR    := llvm-ar.exe
MKDIR := mkdir_win -p
RM    := rm -rf

SYS_LIB_DIR := C:/JL/pi32/pi32v2-lib/r3-large
SYS_INC_DIR := C:/JL/pi32/pi32v2-include
EXT_CFLAGS  := # Windows 下不需要 -D__SHELL__
export PATH:=$(TOOL_DIR);$(PATH)

## 后处理脚本
FIXBAT          := tools/utils/fixbat.exe # 用于处理 utf8->gbk 编码问题
POST_SCRIPT     := cpu/br28/tools/download.bat
RUN_POST_SCRIPT := cpu\br28\tools\download.bat
else
# Linux 下工具链位置
TOOL_DIR := /opt/jieli/pi32v2/bin
CC    := clang
CXX   := clang
LD    := lto-wrapper
AR    := lto-ar
MKDIR := mkdir -p
RM    := rm -rf
export OBJDUMP := $(TOOL_DIR)/objdump
export OBJCOPY := $(TOOL_DIR)/objcopy
export OBJSIZEDUMP := $(TOOL_DIR)/objsizedump

SYS_LIB_DIR := $(TOOL_DIR)/../lib/r3-large
SYS_INC_DIR := $(TOOL_DIR)/../include
EXT_CFLAGS  := -D__SHELL__ # Linux 下需要这个保证正确处理 download.c
export PATH:=$(TOOL_DIR):$(PATH)

## 后处理脚本
FIXBAT          := touch # Linux下不需要处理 bat 编码问题
POST_SCRIPT     := cpu/br28/tools/download.sh
RUN_POST_SCRIPT := bash $(POST_SCRIPT)
endif

CC  := $(TOOL_DIR)/$(CC)
CXX := $(TOOL_DIR)/$(CXX)
LD  := $(TOOL_DIR)/$(LD)
AR  := $(TOOL_DIR)/$(AR)
# 输出文件设置
OUT_ELF   := cpu/br28/tools/sdk.elf
OBJ_FILE  := $(OUT_ELF).objs.txt
# 编译路径设置
BUILD_DIR := objs

# 编译参数设置
CFLAGS := \
	-target pi32v2 \
	-mcpu=r3 \
	-integrated-as \
	-flto \
	-Wuninitialized \
	-Wno-invalid-noreturn \
	-fno-common \
	-integrated-as \
	-Oz \
	-g \
	-flto \
	-fallow-pointer-null \
	-fprefer-gnu-section \
	-Wno-shift-negative-value \
	-Wundef \
	-Wframe-larger-than=256 \
	-Wincompatible-pointer-types \
	-Wreturn-type \
	-Wimplicit-function-declaration \
	-mllvm -pi32v2-large-program=true \
	-fms-extensions \
	-fdiscrete-bitfield-abi \
	-w \


# C++额外的编译参数
CXXFLAGS :=


# 宏定义
DEFINES := \
	-DSUPPORT_MS_EXTENSIONS \
	-DCONFIG_RELEASE_ENABLE \
	-DLWIP_USE_BT \
	-DCONFIG_CPU_BR28 \
	-DCONFIG_PRINT_FLOAT_ENABLE \
	-DCONFIG_NEW_BREDR_ENABLE \
	-DCONFIG_NEW_MODEM_ENABLE \
	-DCONFIG_NEW_TWS_FORWARD_ENABLE \
	-DCONFIG_UCOS_ENABLE \
	-DCONFIG_EQ_SUPPORT_ASYNC \
	-DEQ_CORE_V1 \
	-DCONFIG_FAT_MAX_DEEPTH=3 \
	-DCONFIG_AAC_CODEC_FFT_USE_MUTEX \
	-DCONFIG_DNS_ENABLE \
	-DCONFIG_DMS_MALLOC \
	-DCONFIG_MMU_ENABLE \
	-DCONFIG_SBC_CODEC_HW \
	-DCONFIG_MSBC_CODEC_HW \
	-DCONFIG_AEC_M=256 \
	-DCONFIG_AUDIO_ONCHIP \
	-DCONFIG_MEDIA_DEVELOP_ENABLE \
	-DCONFIG_MIXER_CYCLIC \
	-DCONFIG_SOUND_PLATFORM_ENABLE=1 \
	-DCONFIG_SUPPORT_EX_TWS_ADJUST \
	-D__GCC_PI32V2__ \
	-DCONFIG_NEW_ECC_ENABLE \
	-DCONFIG_MAC_ADDR_GET_USED_SFC \
	-DSUPPORT_BLUETOOTH_PROFILE_RELEASE \
	-DCONFIG_SOUNDBOX \
	-DAPP_USE_SOFT_SPI_ACCESS_VM \
	-DEVENT_HANDLER_NUM_CONFIG=2 \
	-DEVENT_TOUCH_ENABLE_CONFIG=0 \
	-DEVENT_POOL_SIZE_CONFIG=256 \
	-DCONFIG_EVENT_KEY_MAP_ENABLE=0 \
	-DTIMER_POOL_NUM_CONFIG=15 \
	-DAPP_ASYNC_POOL_NUM_CONFIG=0 \
	-DVFS_ENABLE=1 \
	-DUSE_SDFILE_NEW=1 \
	-DSDFILE_STORAGE=1 \
	-DVFS_FILE_POOL_NUM_CONFIG=1 \
	-DFS_VERSION=0x020001 \
	-DFATFS_VERSION=0x020101 \
	-DSDFILE_VERSION=0x020000 \
	-DVM_MAX_PAGE_ALIGN_SIZE_CONFIG=64*1024 \
	-DVM_MAX_SECTOR_ALIGN_SIZE_CONFIG=64*1024 \
	-DVM_ITEM_MAX_NUM=256 \
	-DCONFIG_TWS_ENABLE \
	-DCONFIG_WATCH_CASE_ENABLE=1 \
	-DCONFIG_OS_AFFINITY_ENABLE \
	-DCONFIG_LITE_AEC_ENABLE=0 \
	-DAUDIO_REC_POOL_NUM=1 \
	-DAUDIO_DEC_POOL_NUM=3 \
	-DAUDIO_LINEIN_ENABLE \
	-DCONFIG_LMP_CONN_SUSPEND_ENABLE \
	-DCONFIG_BTCTRLER_TASK_DEL_ENABLE \
	-DCONFIG_LINK_DISTURB_SCAN_ENABLE=0 \
	-DCONFIG_UPDATA_ENABLE \
	-DCONFIG_OTA_UPDATA_ENABLE \
	-DCONFIG_EFFECT_CORE_V2_ENABLE \
	-DCONFIG_ITEM_FORMAT_VM \
	-DLV_LVGL_H_INCLUDE_SIMPLE \
	-DLV_EX_CONF_INCLUDE_SIMPLE \
	-DLV_DEMO_CONF_INCLUDE_SIMPLE \


DEFINES += $(EXT_CFLAGS) # 额外的一些定义

# 头文件搜索路径
INCLUDES := \
	-Iinclude_lib \
	-Iinclude_lib/driver \
	-Iinclude_lib/driver/device \
	-Iinclude_lib/driver/cpu/br28 \
	-Iinclude_lib/system \
	-Iinclude_lib/system/alipay_iot_tcp \
	-Iinclude_lib/system/quickjs \
	-Iinclude_lib/net/mbedtls_2_26_0 \
	-Iinclude_lib/system/generic \
	-Iinclude_lib/system/device \
	-Iinclude_lib/system/fs \
	-Iinclude_lib/system/ui_new \
	-Iinclude_lib/btctrler \
	-Iinclude_lib/btctrler/port/br28 \
	-Iinclude_lib/update \
	-Iinclude_lib/agreement \
	-Iinclude_lib/btstack/third_party/common \
	-Iinclude_lib/btstack/third_party/rcsp \
	-Iinclude_lib/media/media_develop \
	-Iinclude_lib/media/media_develop/media \
	-Iinclude_lib/media/media_develop/media/cpu/br28 \
	-Iinclude_lib/media/media_develop/media/cpu/br28/asm \
	-Iinclude_lib/net/lwip_2_1_3/lwip/src/include \
	-Iinclude_lib/net/lwip_2_1_3/lwip/port \
	-Iinclude_lib/net/lwip_2_1_3/lwip/app \
	-Iinclude_lib/net/web_socket \
	-Iapps/watch/include \
	-Iapps/watch/include/common \
	-Iapps/watch/include/task_manager \
	-Iapps/watch/include/task_manager/bt \
	-Iapps/watch/include/user_api \
	-Iapps/watch/include/sport_data \
	-Iapps/common \
	-Iapps/common/device \
	-Iapps/common/audio \
	-Iapps/common/audio/decode \
	-Iapps/common/audio/encode \
	-Iapps/common/audio/stream \
	-Iapps/common/power_manage \
	-Iapps/common/device/key \
	-Iapps/common/third_party_profile/common \
	-Iapps/common/third_party_profile/jieli \
	-Iapps/common/third_party_profile/jieli/trans_data_demo \
	-Iapps/common/third_party_profile/jieli/JL_rcsp \
	-Iapps/common/third_party_profile/jieli/JL_rcsp/bt_trans_data \
	-Iapps/common/third_party_profile/dma_deal \
	-Iapps/common/third_party_profile/jieli/online_db \
	-Iapps/common/third_party_profile/interface \
	-Iapps/common/dev_manager \
	-Iapps/common/file_operate \
	-Iapps/common/music \
	-Iapps/common/cJSON \
	-Iapps/common/include \
	-Iapps/common/config/include \
	-Iapps/common/device/hr_sensor \
	-Iapps/common/device/hr_sensor/vchr02 \
	-Iapps/common/device/hr_sensor/hx3602 \
	-Iapps/common/device/imu_sensor \
	-Iapps/common/device/bmp280 \
	-Iapps/common/device/nfc \
	-Iapps/watch/board/br28 \
	-Iapps/watch/ui \
	-Iapps/watch/product_test \
	-Icpu/br28 \
	-Icpu/br28/audio_common \
	-Icpu/br28/audio_dec \
	-Icpu/br28/audio_enc \
	-Icpu/br28/audio_effect \
	-Icpu/br28/audio_mic \
	-Icpu/br28/localtws \
	-Iapps/common/debug \
	-Iapps/common/debug/data_export \
	-Iinclude_lib/media \
	-Iinclude_lib/system/math/cpu/br28 \
	-Iinclude_lib/system/flash_translation_layer \
	-Iapps/common/third_party_profile/gfps_protocol \
	-Iapps/common/device/usb \
	-Iapps/common/device/usb/device \
	-Iapps/common/device/usb/host \
	-Iapps/watch/include/smartbox \
	-Iapps/watch/smartbox \
	-Iapps/watch/smartbox/bt_manage/bt_trans_data \
	-Iapps/watch/smartbox/bt_manage \
	-Iapps/watch/smartbox/bt_manage/bt_fmy \
	-Iapps/watch/smartbox/smartbox_setting \
	-Iapps/watch/smartbox/smartbox_setting/smartbox_misc_setting \
	-Iapps/watch/smartbox/smartbox_setting_opt \
	-Iapps/watch/smartbox/smartbox_update \
	-Iapps/watch/smartbox/ota_test \
	-Iapps/watch/smartbox/external_flash \
	-Iapps/watch/smartbox/file_transfer \
	-Iapps/watch/smartbox/sensors_data_opt \
	-Iapps/watch/smartbox/sensors_data_opt/sports_data_opt \
	-Iapps/watch/smartbox/sensors_data_opt/sensors_info_opt \
	-Iapps/watch/smartbox/sensors_data_opt/sport_data_info_opt \
	-Iinclude_lib/system/ui_new/ui/cpu/br28 \
	-Iinclude_lib/net \
	-Iinclude_lib/net/lwip_2_1_3/lwip/src/include/netif \
	-Iinclude_lib/net/lwip_2_1_3/lwip/port/arch \
	-Iinclude_lib/net/lwip_2_1_3/lwip/app/ctp \
	-Iinclude_lib/net/lwip_2_1_3/lwip/app/cdp \
	-Iinclude_lib/net/lwip_2_1_3/lwip/app/dhcp_srv \
	-Iinclude_lib/net/lwip_2_1_3/lwip/app/dtp \
	-Iinclude_lib/net/lwip_2_1_3/lwip/app/sock_api \
	-Iinclude_lib/net/lwip_2_1_3/lwip/app/udtp \
	-Iinclude_lib/net/lwip_2_1_3/lwip/src/include/lwip \
	-Iinclude_lib/net/lwip_2_1_3/lwip/app/mssdp \
	-Iinclude_lib/net/lwip_2_1_3/lwip/app/ddns \
	-Iinclude_lib/net/lte_module \
	-Icpu/br28/ui_driver \
	-Icpu/br28/audio_way \
	-Ilib/media \
	-Ilib/media/cpu/br28 \
	-Icpu/br28/ui_driver/lvgl \
	-Iapps/common/ui/lvgl_v810 \
	-Iapps/common/ui/lvgl_v810/src/extra \
	-Iapps/common/ui/lvgl_v810/src/core \
	-Iapps/common/ui/lvgl_v810/src/draw \
	-Iapps/common/ui/lvgl_v810/src/font \
	-Iapps/common/ui/lvgl_v810/src/gpu \
	-Iapps/common/ui/lvgl_v810/src/hal \
	-Iapps/common/ui/lvgl_v810/src/misc \
	-Iapps/common/ui/lvgl_v810/src/widgets \
	-Iapps/common/ui/lvgl_app/lv_demos \
	-Iinclude_lib/media/aispeech/asr/include \
	-Iinclude_lib/media/aispeech/enc/include \
	-Iapps/common/pay/upay_t_head \
	-Iapps/common/pay/upay_t_head/hal \
	-Iapps/common/pay/upay_t_head/include \
	-Iapps/common/pay/upay_t_head/soc/port \
	-Iapps/common/pay/upay_t_head/soc/se/JL/hsc32i1 \
	-Iapps/common/pay/upay_t_head/soc/softse \
	-Iapps/common/timestamp \
	-Iapps/common/device/gSensor/silan \
	-Iapps/common/audio/local_tts/iflytek/inc \
	-Iapps/common/audio/local_tts/iflytek/src \
	-Iapps/common/cat1/aicxtek/aic_srv/inc \
	-Iapps/common/cat1/aicxtek/aic_srv/portable/include \
	-Iapps/common/cat1/unisoc/audio \
	-Iapps/common/cat1/unisoc/export \
	-Iapps/common/cat1/unisoc/include \
	-Iapps/common/cat1/unisoc/mcu_at_process \
	-Iapps/common/cat1/unisoc/mcu_diag_process \
	-Iapps/common/cat1/unisoc/ping \
	-Iapps/common/cat1/unisoc/port \
	-Iapps/common/cat1/unisoc/port/OSAL_DEVAL_H \
	-Iapps/common/cat1/unisoc/port/uos_osal \
	-Iapps/common/net_app/iflytek/SparkDesk \
	-Iapps/common/net_app/iflytek/tts \
	-Iapps/common/net_app/iflytek/vad \
	-Iapps/common/net_app/iflytek \
	-Iinclude_lib/media/cvp \
	-Iapps/common/uC-Shell/Cfg/Template \
	-Iapps/common/uC-Shell/Cmd/General \
	-Iapps/common/uC-Shell/Source \
	-Iapps/common/uC-Shell/Terminal/Cfg/Template \
	-Iapps/common/uC-Shell/Terminal/Source \
	-Iapps/watch/smartbox/bt_manage/bt_trans_data/usart0_to_mcu \
	-Iapps/watch/smartbox/bt_manage/bt_trans_data/aes_pkcs7 \
	-Iapps/watch/smartbox/bt_manage/bt_trans_data/crc16 \
	-Iapps/watch/smartbox/bt_manage/bt_trans_data/rssi_check \
	-Iapps/watch/smartbox/bt_manage/bt_trans_data/per_key_scan \
	-Iapps/watch/smartbox/bt_manage/bt_trans_data/tone_user_data \
	-Iapps/watch/smartbox/bt_manage/bt_trans_data/wdg_mgr \
	-I$(SYS_INC_DIR) \


# 需要编译的 .c 文件
c_SRC_FILES := \
	apps/common/audio/audio_digital_vol.c \
	apps/common/audio/decode/decode.c \
	apps/common/audio/encode/encode_write_file.c \
	apps/common/audio/local_tts/iflytek/src/ifly_local_tts.c \
	apps/common/audio/sine_make.c \
	apps/common/audio/uartPcmSender.c \
	apps/common/bt_common/bt_test_api.c \
	apps/common/cJSON/cJSON.c \
	apps/common/cat1/aicxtek/aic_srv/portable/drivers/aic_ctrl_drv.c \
	apps/common/cat1/aicxtek/aic_srv/portable/drivers/audio.c \
	apps/common/cat1/aicxtek/aic_srv/portable/drivers/config.c \
	apps/common/cat1/aicxtek/aic_srv/portable/drivers/gpio.c \
	apps/common/cat1/aicxtek/aic_srv/portable/drivers/gpio_wakeup.c \
	apps/common/cat1/aicxtek/aic_srv/portable/drivers/serial.c \
	apps/common/cat1/aicxtek/aic_srv/portable/drivers/time.c \
	apps/common/cat1/aicxtek/aic_srv/portable/drivers/timer.c \
	apps/common/cat1/aicxtek/aic_srv/portable/log/aic_log.c \
	apps/common/cat1/aicxtek/aic_srv/portable/posix/pthread.c \
	apps/common/cat1/aicxtek/aic_srv/portable/tele_cfg/tele_cfg.c \
	apps/common/cat1/aicxtek/aic_srv/portable/tmr/aic_srv_tmr.c \
	apps/common/cat1/unisoc/audio/uos_iis.c \
	apps/common/cat1/unisoc/mcu_at_process/mcu_at_process.c \
	apps/common/cat1/unisoc/mcu_diag_process/mcu_diag_process.c \
	apps/common/cat1/unisoc/ping/ping.c \
	apps/common/cat1/unisoc/port/uos_fs.c \
	apps/common/cat1/unisoc/port/uos_gpio.c \
	apps/common/cat1/unisoc/port/uos_osal/uos_api.c \
	apps/common/cat1/unisoc/port/uos_ringbuffer.c \
	apps/common/cat1/unisoc/port/uos_spi.c \
	apps/common/cat1/unisoc/port/uos_uart.c \
	apps/common/cat1/unisoc/raw.c \
	apps/common/config/app_config.c \
	apps/common/config/bt_profile_config.c \
	apps/common/config/ci_transport_uart.c \
	apps/common/config/new_cfg_tool.c \
	apps/common/debug/data_export/data_export.c \
	apps/common/debug/data_export/data_export_to_file.c \
	apps/common/debug/debug.c \
	apps/common/debug/debug_lite.c \
	apps/common/dev_manager/dev_manager.c \
	apps/common/dev_manager/dev_reg.c \
	apps/common/dev_manager/dev_update.c \
	apps/common/device/bmp280/bmp280.c \
	apps/common/device/detection.c \
	apps/common/device/gSensor/SC7A20.c \
	apps/common/device/gSensor/gSensor_manage.c \
	apps/common/device/gSensor/mc3433.c \
	apps/common/device/gSensor/mpu6050.c \
	apps/common/device/gSensor/msa310.c \
	apps/common/device/gSensor/msa310_function.c \
	apps/common/device/gSensor/p11_gsensor.c \
	apps/common/device/gSensor/silan/silan_motion_module.c \
	apps/common/device/hr_sensor/hrSensor_manage.c \
	apps/common/device/hr_sensor/hrs3300.c \
	apps/common/device/hr_sensor/hrs3603.c \
	apps/common/device/hr_sensor/hx3602/hxHr_module.c \
	apps/common/device/hr_sensor/p11_hrsensor.c \
	apps/common/device/hr_sensor/vcHr_module.c \
	apps/common/device/hr_sensor/vchr02/module_heart_vc_int.c \
	apps/common/device/hr_sensor/vchr02/vcHr02Hci.c \
	apps/common/device/imu_sensor/imuSensor_manage.c \
	apps/common/device/imu_sensor/mpu6887/mpu6887p.c \
	apps/common/device/imu_sensor/mpu9250/mpu9250.c \
	apps/common/device/imu_sensor/sh3001/sh3001.c \
	apps/common/device/key/adkey.c \
	apps/common/device/key/iokey.c \
	apps/common/device/key/irkey.c \
	apps/common/device/key/key_driver.c \
	apps/common/device/key/rdec_key.c \
	apps/common/device/key/touch_key.c \
	apps/common/device/nandflash/ftl_device.c \
	apps/common/device/nandflash/nandflash.c \
	apps/common/device/nandflash/nandflash_vm.c \
	apps/common/device/nfc/nfc_fm11nt081d.c \
	apps/common/device/norflash/inside_norflash.c \
	apps/common/device/norflash/norflash.c \
	apps/common/device/norflash/norflash_sfc.c \
	apps/common/device/touch_panel/IT7259E.c \
	apps/common/device/touch_panel/bl6133.c \
	apps/common/device/touch_panel/bl_fw_update.c \
	apps/common/device/touch_panel/cst816d/cst816d.c \
	apps/common/device/touch_panel/cst816s/cst816s.c \
	apps/common/device/touch_panel/ft3x68/ft3x68.c \
	apps/common/device/touch_panel/ft3x68/ft3x68_upgrade.c \
	apps/common/device/touch_panel/ft6336g.c \
	apps/common/device/touch_panel/spt511x/spt511x_firmware_customer.c \
	apps/common/device/usb/device/cdc.c \
	apps/common/device/usb/device/custom_hid.c \
	apps/common/device/usb/device/descriptor.c \
	apps/common/device/usb/device/hid.c \
	apps/common/device/usb/device/msd.c \
	apps/common/device/usb/device/msd_upgrade.c \
	apps/common/device/usb/device/rcsp_hid_inter.c \
	apps/common/device/usb/device/task_pc.c \
	apps/common/device/usb/device/uac1.c \
	apps/common/device/usb/device/uac_stream.c \
	apps/common/device/usb/device/usb_device.c \
	apps/common/device/usb/device/user_setup.c \
	apps/common/device/usb/host/adb.c \
	apps/common/device/usb/host/aoa.c \
	apps/common/device/usb/host/audio.c \
	apps/common/device/usb/host/audio_demo.c \
	apps/common/device/usb/host/hid.c \
	apps/common/device/usb/host/usb_bulk_transfer.c \
	apps/common/device/usb/host/usb_ctrl_transfer.c \
	apps/common/device/usb/host/usb_host.c \
	apps/common/device/usb/host/usb_storage.c \
	apps/common/device/usb/usb_config.c \
	apps/common/device/usb/usb_host_config.c \
	apps/common/fat_nor/nor_fs.c \
	apps/common/fat_nor/phone_rec_fs.c \
	apps/common/fat_nor/virfat_flash.c \
	apps/common/file_operate/file_api.c \
	apps/common/file_operate/file_bs_deal.c \
	apps/common/file_operate/file_manager.c \
	apps/common/music/breakpoint.c \
	apps/common/music/general_player.c \
	apps/common/music/music_decrypt.c \
	apps/common/music/music_id3.c \
	apps/common/music/music_player.c \
	apps/common/net/lwip_2_1_3/port/LwIP.c \
	apps/common/net/lwip_2_1_3/port/bt_ethernetif.c \
	apps/common/net/lwip_2_1_3/port/ext_wireless_ethernetif.c \
	apps/common/net/lwip_2_1_3/port/lte_ethernetif_aicxtek.c \
	apps/common/net/lwip_2_1_3/port/lte_ethernetif_unisoc.c \
	apps/common/net/lwip_2_1_3/port/sys_arch.c \
	apps/common/net/lwip_2_1_3/port/wireless_ethernetif.c \
	apps/common/net_app/iflytek/SparkDesk/sparkdesk_main.c \
	apps/common/net_app/iflytek/authentication.c \
	apps/common/net_app/iflytek/demo.c \
	apps/common/net_app/iflytek/ifly_common.c \
	apps/common/net_app/iflytek/ifly_socket.c \
	apps/common/net_app/iflytek/tts/ifly_dec_file.c \
	apps/common/net_app/iflytek/tts/tts_main.c \
	apps/common/net_app/iflytek/vad/vad_main.c \
	apps/common/pay/upay_t_head/jl_transitcode/transit_other_adapation.c \
	apps/common/pay/upay_t_head/jl_transitcode/transit_task.c \
	apps/common/pay/upay_t_head/soc/se/JL/hsc32i1/hs_private.c \
	apps/common/pay/upay_t_head/soc/se/JL/vendor_cache.c \
	apps/common/pay/upay_t_head/soc/se/JL/vendor_crypto.c \
	apps/common/pay/upay_t_head/soc/se/JL/vendor_product.c \
	apps/common/pay/upay_t_head/soc/se/JL/vendor_rot.c \
	apps/common/pay/upay_t_head/soc/se/JL/vendor_ssp.c \
	apps/common/pay/upay_t_head/soc/softse/aes.c \
	apps/common/pay/upay_t_head/soc/softse/kdf.c \
	apps/common/pay/upay_t_head/soc/softse/sha256.c \
	apps/common/pay/upay_t_head/soc/softse/utils.c \
	apps/common/temp_trim/dtemp_pll_trim.c \
	apps/common/third_party_profile/common/3th_profile_api.c \
	apps/common/third_party_profile/common/custom_cfg.c \
	apps/common/third_party_profile/common/mic_rec.c \
	apps/common/third_party_profile/gfps_protocol/gfps_protocol.c \
	apps/common/third_party_profile/jieli/le_client_demo.c \
	apps/common/third_party_profile/jieli/online_db/online_db_deal.c \
	apps/common/third_party_profile/jieli/online_db/spp_online_db.c \
	apps/common/third_party_profile/jieli/trans_data_demo/le_trans_data.c \
	apps/common/third_party_profile/jieli/trans_data_demo/spp_trans_data.c \
	apps/common/third_party_profile/multi_protocol_common.c \
	apps/common/third_party_profile/multi_protocol_event.c \
	apps/common/third_party_profile/multi_protocol_main.c \
	apps/common/timestamp/timestamp.c \
	apps/common/uC-Shell/Cmd/General/sh_shell.c \
	apps/common/uC-Shell/Source/shell.c \
	apps/common/uC-Shell/Terminal/Mode/VT100/terminal_mode.c \
	apps/common/uC-Shell/Terminal/OS/uCOS-II/terminal_os.c \
	apps/common/uC-Shell/Terminal/Serial/Template/terminal_serial.c \
	apps/common/uC-Shell/Terminal/Source/terminal.c \
	apps/common/uC-Shell/shell_main.c \
	apps/common/ui/lcd/lcd_ui_api.c \
	apps/common/ui/led7/led7_ui_api.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_benchmark/assets/img_benchmark_cogwheel_alpha16.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_benchmark/assets/img_benchmark_cogwheel_argb.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_benchmark/assets/img_benchmark_cogwheel_chroma_keyed.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_benchmark/assets/img_benchmark_cogwheel_indexed16.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_benchmark/assets/img_benchmark_cogwheel_rgb.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_benchmark/assets/lv_font_montserrat_12_compr_az.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_benchmark/assets/lv_font_montserrat_16_compr_az.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_benchmark/assets/lv_font_montserrat_28_compr_az.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_benchmark/lv_demo_benchmark.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_keypad_encoder/lv_demo_keypad_encoder.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_music/assets/img_lv_demo_music_btn_corner_large.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_music/assets/img_lv_demo_music_btn_list_pause.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_music/assets/img_lv_demo_music_btn_list_pause_large.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_music/assets/img_lv_demo_music_btn_list_play.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_music/assets/img_lv_demo_music_btn_list_play_large.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_music/assets/img_lv_demo_music_btn_loop.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_music/assets/img_lv_demo_music_btn_loop_large.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_music/assets/img_lv_demo_music_btn_next.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_music/assets/img_lv_demo_music_btn_next_large.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_music/assets/img_lv_demo_music_btn_pause.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_music/assets/img_lv_demo_music_btn_pause_large.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_music/assets/img_lv_demo_music_btn_play.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_music/assets/img_lv_demo_music_btn_play_large.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_music/assets/img_lv_demo_music_btn_prev.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_music/assets/img_lv_demo_music_btn_prev_large.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_music/assets/img_lv_demo_music_btn_rnd.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_music/assets/img_lv_demo_music_btn_rnd_large.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_music/assets/img_lv_demo_music_corner_left.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_music/assets/img_lv_demo_music_corner_left_large.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_music/assets/img_lv_demo_music_corner_right.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_music/assets/img_lv_demo_music_corner_right_large.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_music/assets/img_lv_demo_music_cover_1.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_music/assets/img_lv_demo_music_cover_1_large.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_music/assets/img_lv_demo_music_cover_2.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_music/assets/img_lv_demo_music_cover_2_large.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_music/assets/img_lv_demo_music_cover_3.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_music/assets/img_lv_demo_music_cover_3_large.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_music/assets/img_lv_demo_music_icon_1.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_music/assets/img_lv_demo_music_icon_1_large.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_music/assets/img_lv_demo_music_icon_2.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_music/assets/img_lv_demo_music_icon_2_large.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_music/assets/img_lv_demo_music_icon_3.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_music/assets/img_lv_demo_music_icon_3_large.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_music/assets/img_lv_demo_music_icon_4.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_music/assets/img_lv_demo_music_icon_4_large.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_music/assets/img_lv_demo_music_list_border.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_music/assets/img_lv_demo_music_list_border_large.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_music/assets/img_lv_demo_music_logo.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_music/assets/img_lv_demo_music_slider_knob.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_music/assets/img_lv_demo_music_slider_knob_large.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_music/assets/img_lv_demo_music_wave_bottom.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_music/assets/img_lv_demo_music_wave_bottom_large.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_music/assets/img_lv_demo_music_wave_top.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_music/assets/img_lv_demo_music_wave_top_large.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_music/lv_demo_music.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_music/lv_demo_music_list.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_music/lv_demo_music_main.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_stress/assets/lv_font_montserrat_12_compr_az.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_stress/assets/lv_font_montserrat_16_compr_az.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_stress/assets/lv_font_montserrat_28_compr_az.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_stress/lv_demo_stress.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_widgets/assets/img_clothes.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_widgets/assets/img_demo_widgets_avatar.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_widgets/assets/img_lvgl_logo.c \
	apps/common/ui/lvgl_app/lv_demos/src/lv_demo_widgets/lv_demo_widgets.c \
	apps/common/ui/lvgl_v810/src/core/lv_disp.c \
	apps/common/ui/lvgl_v810/src/core/lv_event.c \
	apps/common/ui/lvgl_v810/src/core/lv_group.c \
	apps/common/ui/lvgl_v810/src/core/lv_indev.c \
	apps/common/ui/lvgl_v810/src/core/lv_indev_scroll.c \
	apps/common/ui/lvgl_v810/src/core/lv_obj.c \
	apps/common/ui/lvgl_v810/src/core/lv_obj_class.c \
	apps/common/ui/lvgl_v810/src/core/lv_obj_draw.c \
	apps/common/ui/lvgl_v810/src/core/lv_obj_pos.c \
	apps/common/ui/lvgl_v810/src/core/lv_obj_scroll.c \
	apps/common/ui/lvgl_v810/src/core/lv_obj_style.c \
	apps/common/ui/lvgl_v810/src/core/lv_obj_style_gen.c \
	apps/common/ui/lvgl_v810/src/core/lv_obj_tree.c \
	apps/common/ui/lvgl_v810/src/core/lv_refr.c \
	apps/common/ui/lvgl_v810/src/core/lv_theme.c \
	apps/common/ui/lvgl_v810/src/draw/lv_draw_arc.c \
	apps/common/ui/lvgl_v810/src/draw/lv_draw_blend.c \
	apps/common/ui/lvgl_v810/src/draw/lv_draw_img.c \
	apps/common/ui/lvgl_v810/src/draw/lv_draw_label.c \
	apps/common/ui/lvgl_v810/src/draw/lv_draw_line.c \
	apps/common/ui/lvgl_v810/src/draw/lv_draw_mask.c \
	apps/common/ui/lvgl_v810/src/draw/lv_draw_rect.c \
	apps/common/ui/lvgl_v810/src/draw/lv_draw_triangle.c \
	apps/common/ui/lvgl_v810/src/draw/lv_img_buf.c \
	apps/common/ui/lvgl_v810/src/draw/lv_img_cache.c \
	apps/common/ui/lvgl_v810/src/draw/lv_img_decoder.c \
	apps/common/ui/lvgl_v810/src/extra/layouts/flex/lv_flex.c \
	apps/common/ui/lvgl_v810/src/extra/layouts/grid/lv_grid.c \
	apps/common/ui/lvgl_v810/src/extra/lv_extra.c \
	apps/common/ui/lvgl_v810/src/extra/others/snapshot/lv_snapshot.c \
	apps/common/ui/lvgl_v810/src/extra/themes/basic/lv_theme_basic.c \
	apps/common/ui/lvgl_v810/src/extra/themes/default/lv_theme_default.c \
	apps/common/ui/lvgl_v810/src/extra/themes/mono/lv_theme_mono.c \
	apps/common/ui/lvgl_v810/src/extra/widgets/animimg/lv_animimg.c \
	apps/common/ui/lvgl_v810/src/extra/widgets/calendar/lv_calendar.c \
	apps/common/ui/lvgl_v810/src/extra/widgets/calendar/lv_calendar_header_arrow.c \
	apps/common/ui/lvgl_v810/src/extra/widgets/calendar/lv_calendar_header_dropdown.c \
	apps/common/ui/lvgl_v810/src/extra/widgets/chart/lv_chart.c \
	apps/common/ui/lvgl_v810/src/extra/widgets/colorwheel/lv_colorwheel.c \
	apps/common/ui/lvgl_v810/src/extra/widgets/imgbtn/lv_imgbtn.c \
	apps/common/ui/lvgl_v810/src/extra/widgets/keyboard/lv_keyboard.c \
	apps/common/ui/lvgl_v810/src/extra/widgets/led/lv_led.c \
	apps/common/ui/lvgl_v810/src/extra/widgets/list/lv_list.c \
	apps/common/ui/lvgl_v810/src/extra/widgets/meter/lv_meter.c \
	apps/common/ui/lvgl_v810/src/extra/widgets/msgbox/lv_msgbox.c \
	apps/common/ui/lvgl_v810/src/extra/widgets/span/lv_span.c \
	apps/common/ui/lvgl_v810/src/extra/widgets/spinbox/lv_spinbox.c \
	apps/common/ui/lvgl_v810/src/extra/widgets/spinner/lv_spinner.c \
	apps/common/ui/lvgl_v810/src/extra/widgets/tabview/lv_tabview.c \
	apps/common/ui/lvgl_v810/src/extra/widgets/tileview/lv_tileview.c \
	apps/common/ui/lvgl_v810/src/extra/widgets/win/lv_win.c \
	apps/common/ui/lvgl_v810/src/font/lv_font.c \
	apps/common/ui/lvgl_v810/src/font/lv_font_dejavu_16_persian_hebrew.c \
	apps/common/ui/lvgl_v810/src/font/lv_font_fmt_txt.c \
	apps/common/ui/lvgl_v810/src/font/lv_font_loader.c \
	apps/common/ui/lvgl_v810/src/font/lv_font_montserrat_10.c \
	apps/common/ui/lvgl_v810/src/font/lv_font_montserrat_12.c \
	apps/common/ui/lvgl_v810/src/font/lv_font_montserrat_12_subpx.c \
	apps/common/ui/lvgl_v810/src/font/lv_font_montserrat_14.c \
	apps/common/ui/lvgl_v810/src/font/lv_font_montserrat_16.c \
	apps/common/ui/lvgl_v810/src/font/lv_font_montserrat_18.c \
	apps/common/ui/lvgl_v810/src/font/lv_font_montserrat_20.c \
	apps/common/ui/lvgl_v810/src/font/lv_font_montserrat_22.c \
	apps/common/ui/lvgl_v810/src/font/lv_font_montserrat_24.c \
	apps/common/ui/lvgl_v810/src/font/lv_font_montserrat_26.c \
	apps/common/ui/lvgl_v810/src/font/lv_font_montserrat_28.c \
	apps/common/ui/lvgl_v810/src/font/lv_font_montserrat_28_compressed.c \
	apps/common/ui/lvgl_v810/src/font/lv_font_montserrat_30.c \
	apps/common/ui/lvgl_v810/src/font/lv_font_montserrat_32.c \
	apps/common/ui/lvgl_v810/src/font/lv_font_montserrat_34.c \
	apps/common/ui/lvgl_v810/src/font/lv_font_montserrat_36.c \
	apps/common/ui/lvgl_v810/src/font/lv_font_montserrat_38.c \
	apps/common/ui/lvgl_v810/src/font/lv_font_montserrat_40.c \
	apps/common/ui/lvgl_v810/src/font/lv_font_montserrat_42.c \
	apps/common/ui/lvgl_v810/src/font/lv_font_montserrat_44.c \
	apps/common/ui/lvgl_v810/src/font/lv_font_montserrat_46.c \
	apps/common/ui/lvgl_v810/src/font/lv_font_montserrat_48.c \
	apps/common/ui/lvgl_v810/src/font/lv_font_montserrat_8.c \
	apps/common/ui/lvgl_v810/src/font/lv_font_simsun_16_cjk.c \
	apps/common/ui/lvgl_v810/src/font/lv_font_unscii_16.c \
	apps/common/ui/lvgl_v810/src/font/lv_font_unscii_8.c \
	apps/common/ui/lvgl_v810/src/gpu/lv_gpu_nxp_pxp.c \
	apps/common/ui/lvgl_v810/src/gpu/lv_gpu_nxp_pxp_osa.c \
	apps/common/ui/lvgl_v810/src/gpu/lv_gpu_nxp_vglite.c \
	apps/common/ui/lvgl_v810/src/gpu/lv_gpu_sdl.c \
	apps/common/ui/lvgl_v810/src/gpu/lv_gpu_stm32_dma2d.c \
	apps/common/ui/lvgl_v810/src/hal/lv_hal_disp.c \
	apps/common/ui/lvgl_v810/src/hal/lv_hal_indev.c \
	apps/common/ui/lvgl_v810/src/hal/lv_hal_tick.c \
	apps/common/ui/lvgl_v810/src/misc/lv_anim.c \
	apps/common/ui/lvgl_v810/src/misc/lv_anim_timeline.c \
	apps/common/ui/lvgl_v810/src/misc/lv_area.c \
	apps/common/ui/lvgl_v810/src/misc/lv_async.c \
	apps/common/ui/lvgl_v810/src/misc/lv_bidi.c \
	apps/common/ui/lvgl_v810/src/misc/lv_color.c \
	apps/common/ui/lvgl_v810/src/misc/lv_fs.c \
	apps/common/ui/lvgl_v810/src/misc/lv_gc.c \
	apps/common/ui/lvgl_v810/src/misc/lv_ll.c \
	apps/common/ui/lvgl_v810/src/misc/lv_log.c \
	apps/common/ui/lvgl_v810/src/misc/lv_math.c \
	apps/common/ui/lvgl_v810/src/misc/lv_mem.c \
	apps/common/ui/lvgl_v810/src/misc/lv_printf.c \
	apps/common/ui/lvgl_v810/src/misc/lv_style.c \
	apps/common/ui/lvgl_v810/src/misc/lv_style_gen.c \
	apps/common/ui/lvgl_v810/src/misc/lv_timer.c \
	apps/common/ui/lvgl_v810/src/misc/lv_tlsf.c \
	apps/common/ui/lvgl_v810/src/misc/lv_txt.c \
	apps/common/ui/lvgl_v810/src/misc/lv_txt_ap.c \
	apps/common/ui/lvgl_v810/src/misc/lv_utils.c \
	apps/common/ui/lvgl_v810/src/widgets/lv_arc.c \
	apps/common/ui/lvgl_v810/src/widgets/lv_bar.c \
	apps/common/ui/lvgl_v810/src/widgets/lv_btn.c \
	apps/common/ui/lvgl_v810/src/widgets/lv_btnmatrix.c \
	apps/common/ui/lvgl_v810/src/widgets/lv_canvas.c \
	apps/common/ui/lvgl_v810/src/widgets/lv_checkbox.c \
	apps/common/ui/lvgl_v810/src/widgets/lv_dropdown.c \
	apps/common/ui/lvgl_v810/src/widgets/lv_img.c \
	apps/common/ui/lvgl_v810/src/widgets/lv_label.c \
	apps/common/ui/lvgl_v810/src/widgets/lv_line.c \
	apps/common/ui/lvgl_v810/src/widgets/lv_roller.c \
	apps/common/ui/lvgl_v810/src/widgets/lv_slider.c \
	apps/common/ui/lvgl_v810/src/widgets/lv_switch.c \
	apps/common/ui/lvgl_v810/src/widgets/lv_table.c \
	apps/common/ui/lvgl_v810/src/widgets/lv_textarea.c \
	apps/common/update/cat1_update.c \
	apps/common/update/ex_flash_file_download.c \
	apps/common/update/lc_flash_ufw_update.c \
	apps/common/update/nandflash_ufw_update.c \
	apps/common/update/norflash_ufw_update.c \
	apps/common/update/norflash_update.c \
	apps/common/update/testbox_update.c \
	apps/common/update/uart_update.c \
	apps/common/update/uart_update_master.c \
	apps/common/update/update.c \
	apps/common/update/update_rx_ota.c \
	apps/common/update/user_file_download/ext_flash_file_download.c \
	apps/common/update/user_file_download/nand_zone_file_download.c \
	apps/common/update/user_file_download/reserve_file_download.c \
	apps/common/update/user_file_download/user_file_download.c \
	apps/watch/aec/br28/audio_aec.c \
	apps/watch/aec/br28/audio_aec_demo.c \
	apps/watch/aec/br28/audio_aec_dms.c \
	apps/watch/aec/br28/audio_aec_online.c \
	apps/watch/app_main.c \
	apps/watch/board/br28/board_7012a7_demo/board_7012a7_demo.c \
	apps/watch/board/br28/board_7012a7_demo/key_table/adkey_table.c \
	apps/watch/board/br28/board_7012a7_demo/key_table/iokey_table.c \
	apps/watch/board/br28/board_7012a7_demo/key_table/irkey_table.c \
	apps/watch/board/br28/board_7012a7_demo/key_table/rdec_key_table.c \
	apps/watch/board/br28/board_7012a7_demo/key_table/touch_key_table.c \
	apps/watch/board/br28/board_7012a_demo/board_7012a_demo.c \
	apps/watch/board/br28/board_7012a_demo/key_table/adkey_table.c \
	apps/watch/board/br28/board_7012a_demo/key_table/iokey_table.c \
	apps/watch/board/br28/board_7012a_demo/key_table/irkey_table.c \
	apps/watch/board/br28/board_7012a_demo/key_table/rdec_key_table.c \
	apps/watch/board/br28/board_7012a_demo/key_table/touch_key_table.c \
	apps/watch/board/br28/board_7014f5_demo/board_7014f5_demo.c \
	apps/watch/board/br28/board_7014f5_demo/key_table/adkey_table.c \
	apps/watch/board/br28/board_7014f5_demo/key_table/iokey_table.c \
	apps/watch/board/br28/board_7014f5_demo/key_table/irkey_table.c \
	apps/watch/board/br28/board_7014f5_demo/key_table/rdec_key_table.c \
	apps/watch/board/br28/board_7014f5_demo/key_table/touch_key_table.c \
	apps/watch/board/br28/board_701n_demo/board_701n_demo.c \
	apps/watch/board/br28/board_701n_demo/key_table/adkey_table.c \
	apps/watch/board/br28/board_701n_demo/key_table/iokey_table.c \
	apps/watch/board/br28/board_701n_demo/key_table/irkey_table.c \
	apps/watch/board/br28/board_701n_demo/key_table/rdec_key_table.c \
	apps/watch/board/br28/board_701n_demo/key_table/touch_key_table.c \
	apps/watch/board/br28/board_701n_lvgl_demo/board_701n_lvgl_demo.c \
	apps/watch/board/br28/board_701n_lvgl_demo/key_table/adkey_table.c \
	apps/watch/board/br28/board_701n_lvgl_demo/key_table/iokey_table.c \
	apps/watch/board/br28/board_701n_lvgl_demo/key_table/irkey_table.c \
	apps/watch/board/br28/board_701n_lvgl_demo/key_table/rdec_key_table.c \
	apps/watch/board/br28/board_701n_lvgl_demo/key_table/touch_key_table.c \
	apps/watch/board/br28/board_701n_nandflash_demo/board_701n_nandflash_demo.c \
	apps/watch/board/br28/board_701n_nandflash_demo/key_table/adkey_table.c \
	apps/watch/board/br28/board_701n_nandflash_demo/key_table/iokey_table.c \
	apps/watch/board/br28/board_701n_nandflash_demo/key_table/irkey_table.c \
	apps/watch/board/br28/board_701n_nandflash_demo/key_table/rdec_key_table.c \
	apps/watch/board/br28/board_701n_nandflash_demo/key_table/touch_key_table.c \
	apps/watch/board/br28/board_ad2802e_demo/board_ad2802e_demo.c \
	apps/watch/board/br28/board_ad2802e_demo/key_table/adkey_table.c \
	apps/watch/board/br28/board_ad2802e_demo/key_table/iokey_table.c \
	apps/watch/board/br28/board_ad2802e_demo/key_table/irkey_table.c \
	apps/watch/board/br28/board_ad2802e_demo/key_table/rdec_key_table.c \
	apps/watch/board/br28/board_ad2802e_demo/key_table/touch_key_table.c \
	apps/watch/common/call_common.c \
	apps/watch/common/dev_status.c \
	apps/watch/common/init.c \
	apps/watch/common/smart_voice_user.c \
	apps/watch/common/sport_data/altitude_presure.c \
	apps/watch/common/sport_data/blood_oxygen.c \
	apps/watch/common/sport_data/detection_response.c \
	apps/watch/common/sport_data/heart_rate.c \
	apps/watch/common/sport_data/motion_module.c \
	apps/watch/common/sport_data/sleep.c \
	apps/watch/common/sport_data/sport.c \
	apps/watch/common/sport_data/watch_common.c \
	apps/watch/common/sport_data/watch_time_common.c \
	apps/watch/common/step_calcul.c \
	apps/watch/common/task_table.c \
	apps/watch/common/tone_table.c \
	apps/watch/common/user_cfg.c \
	apps/watch/font/fontinit.c \
	apps/watch/kws_voice_event_deal.c \
	apps/watch/log_config/app_config.c \
	apps/watch/log_config/lib_btctrler_config.c \
	apps/watch/log_config/lib_btstack_config.c \
	apps/watch/log_config/lib_driver_config.c \
	apps/watch/log_config/lib_media_config.c \
	apps/watch/log_config/lib_net_config.c \
	apps/watch/log_config/lib_system_config.c \
	apps/watch/log_config/lib_update_config.c \
	apps/watch/lte/lte_network_test.c \
	apps/watch/lte/tcp_test.c \
	apps/watch/power_manage/app_charge.c \
	apps/watch/power_manage/app_chargestore.c \
	apps/watch/power_manage/app_power_manage.c \
	apps/watch/product_test/product_test.c \
	apps/watch/product_test/pt_gpio.c \
	apps/watch/product_test/pt_gsensor.c \
	apps/watch/product_test/pt_hr.c \
	apps/watch/product_test/pt_lcd_tp.c \
	apps/watch/product_test/pt_motor.c \
	apps/watch/product_test/pt_sd.c \
	apps/watch/product_test/pt_speaker_mic.c \
	apps/watch/smartbox/ai_interaction/ai_audio.c \
	apps/watch/smartbox/ai_interaction/ai_interaction.c \
	apps/watch/smartbox/ai_interaction/ai_text.c \
	apps/watch/smartbox/browser/browser.c \
	apps/watch/smartbox/bt_manage/bt_fmy/ble_fmy.c \
	apps/watch/smartbox/bt_manage/bt_fmy/ble_fmy_fmna.c \
	apps/watch/smartbox/bt_manage/bt_fmy/ble_fmy_modet.c \
	apps/watch/smartbox/bt_manage/bt_trans_data/le_smartbox_adv.c \
	apps/watch/smartbox/bt_manage/bt_trans_data/le_smartbox_client.c \
	apps/watch/smartbox/bt_manage/bt_trans_data/le_smartbox_module.c \
	apps/watch/smartbox/bt_manage/bt_trans_data/le_multi_smartbox_module.c \
	apps/watch/smartbox/bt_manage/bt_trans_data/le_smartbox_multi_client.c \
	apps/watch/smartbox/bt_manage/bt_trans_data/le_smartbox_multi_common.c \
	apps/watch/smartbox/bt_manage/bt_trans_data/le_smartbox_multi_trans.c \
	apps/watch/smartbox/bt_manage/bt_trans_data/fill_protocol.c \
	apps/watch/smartbox/bt_manage/bt_trans_data/BLE_OTA.c \
	apps/watch/smartbox/bt_manage/bt_trans_data/crc16.c \
	apps/watch/smartbox/bt_manage/bt_trans_data/aes_pkcs7.c \
	apps/watch/smartbox/bt_manage/bt_trans_data/usart0_to_mcu.c \
	apps/watch/smartbox/bt_manage/bt_trans_data/rssi_check.c \
	apps/watch/smartbox/bt_manage/bt_trans_data/per_key_scan.c \
	apps/watch/smartbox/bt_manage/bt_trans_data/wdg_mgr.c \
	apps/watch/smartbox/bt_manage/bt_trans_data/user_info_file.c \
	apps/watch/smartbox/bt_manage/smartbox_bt_manage.c \
	apps/watch/smartbox/cmd_data_deal/cmd_recieve.c \
	apps/watch/smartbox/cmd_data_deal/cmd_recieve_no_respone.c \
	apps/watch/smartbox/cmd_data_deal/cmd_respone.c \
	apps/watch/smartbox/cmd_data_deal/cmd_user.c \
	apps/watch/smartbox/cmd_data_deal/command.c \
	apps/watch/smartbox/cmd_data_deal/data_recieve.c \
	apps/watch/smartbox/cmd_data_deal/data_recieve_no_respone.c \
	apps/watch/smartbox/cmd_data_deal/data_respone.c \
	apps/watch/smartbox/config.c \
	apps/watch/smartbox/event.c \
	apps/watch/smartbox/external_flash/smartbox_extra_flash_cmd.c \
	apps/watch/smartbox/external_flash/smartbox_extra_flash_opt.c \
	apps/watch/smartbox/feature.c \
	apps/watch/smartbox/file_transfer/dev_format.c \
	apps/watch/smartbox/file_transfer/file_bluk_trans_prepare.c \
	apps/watch/smartbox/file_transfer/file_delete.c \
	apps/watch/smartbox/file_transfer/file_env_prepare.c \
	apps/watch/smartbox/file_transfer/file_simple_transfer.c \
	apps/watch/smartbox/file_transfer/file_trans_back.c \
	apps/watch/smartbox/file_transfer/file_transfer.c \
	apps/watch/smartbox/func_cmd/bt_func.c \
	apps/watch/smartbox/func_cmd/fm_func.c \
	apps/watch/smartbox/func_cmd/linein_func.c \
	apps/watch/smartbox/func_cmd/music_func.c \
	apps/watch/smartbox/func_cmd/rtc_func.c \
	apps/watch/smartbox/function.c \
	apps/watch/smartbox/mass_data/mass_data.c \
	apps/watch/smartbox/sensors_data_opt/nfc_data_opt.c \
	apps/watch/smartbox/sensors_data_opt/sensor_log_notify.c \
	apps/watch/smartbox/sensors_data_opt/sensors_data_opt.c \
	apps/watch/smartbox/sensors_data_opt/sport_data_func.c \
	apps/watch/smartbox/sensors_data_opt/sport_data_info_opt/sport_info_bt_disconn.c \
	apps/watch/smartbox/sensors_data_opt/sport_data_info_opt/sport_info_continuous_heart_rate.c \
	apps/watch/smartbox/sensors_data_opt/sport_data_info_opt/sport_info_exercise_heart_rate.c \
	apps/watch/smartbox/sensors_data_opt/sport_data_info_opt/sport_info_fall_detection.c \
	apps/watch/smartbox/sensors_data_opt/sport_data_info_opt/sport_info_personal_info.c \
	apps/watch/smartbox/sensors_data_opt/sport_data_info_opt/sport_info_pressure_detection.c \
	apps/watch/smartbox/sensors_data_opt/sport_data_info_opt/sport_info_raise_wrist.c \
	apps/watch/smartbox/sensors_data_opt/sport_data_info_opt/sport_info_sedentary.c \
	apps/watch/smartbox/sensors_data_opt/sport_data_info_opt/sport_info_sensor_opt.c \
	apps/watch/smartbox/sensors_data_opt/sport_data_info_opt/sport_info_sleep_detection.c \
	apps/watch/smartbox/sensors_data_opt/sport_info_opt.c \
	apps/watch/smartbox/sensors_data_opt/sport_info_sync.c \
	apps/watch/smartbox/sensors_data_opt/sport_info_vm.c \
	apps/watch/smartbox/sensors_data_opt/sports_data_opt/sport_data_air_pressure.c \
	apps/watch/smartbox/sensors_data_opt/sports_data_opt/sport_data_altitude.c \
	apps/watch/smartbox/sensors_data_opt/sports_data_opt/sport_data_blood_oxygen.c \
	apps/watch/smartbox/sensors_data_opt/sports_data_opt/sport_data_exercise_recovery_time.c \
	apps/watch/smartbox/sensors_data_opt/sports_data_opt/sport_data_exercise_steps.c \
	apps/watch/smartbox/sensors_data_opt/sports_data_opt/sport_data_heart_rate.c \
	apps/watch/smartbox/sensors_data_opt/sports_data_opt/sport_data_max_oxygen_uptake.c \
	apps/watch/smartbox/sensors_data_opt/sports_data_opt/sport_data_pressure_detection.c \
	apps/watch/smartbox/sensors_data_opt/sports_data_opt/sport_data_sports_information.c \
	apps/watch/smartbox/sensors_data_opt/sports_data_opt/sport_data_training_load.c \
	apps/watch/smartbox/smartbox.c \
	apps/watch/smartbox/smartbox_common_info.c \
	apps/watch/smartbox/smartbox_device_config.c \
	apps/watch/smartbox/smartbox_rcsp_manage.c \
	apps/watch/smartbox/smartbox_setting/adv_bt_name_setting.c \
	apps/watch/smartbox/smartbox_setting/adv_key_setting.c \
	apps/watch/smartbox/smartbox_setting/adv_led_setting.c \
	apps/watch/smartbox/smartbox_setting/adv_mic_setting.c \
	apps/watch/smartbox/smartbox_setting/adv_time_stamp_setting.c \
	apps/watch/smartbox/smartbox_setting/adv_work_setting.c \
	apps/watch/smartbox/smartbox_setting/smartbox_color_led_setting.c \
	apps/watch/smartbox/smartbox_setting/smartbox_eq_setting.c \
	apps/watch/smartbox/smartbox_setting/smartbox_high_low_vol_setting.c \
	apps/watch/smartbox/smartbox_setting/smartbox_karaoke_eq_setting.c \
	apps/watch/smartbox/smartbox_setting/smartbox_karaoke_setting.c \
	apps/watch/smartbox/smartbox_setting/smartbox_misc_setting/smartbox_misc_drc_setting.c \
	apps/watch/smartbox/smartbox_setting/smartbox_misc_setting/smartbox_misc_reverbration_setting.c \
	apps/watch/smartbox/smartbox_setting/smartbox_misc_setting/smartbox_misc_setting.c \
	apps/watch/smartbox/smartbox_setting/smartbox_music_info_setting.c \
	apps/watch/smartbox/smartbox_setting/smartbox_vol_setting.c \
	apps/watch/smartbox/smartbox_setting_opt/smartbox_adv_bluetooth.c \
	apps/watch/smartbox/smartbox_setting_opt/smartbox_setting_opt.c \
	apps/watch/smartbox/smartbox_setting_opt/smartbox_setting_sync.c \
	apps/watch/smartbox/smartbox_task.c \
	apps/watch/smartbox/ota_test/ota_test.c \
	apps/watch/smartbox/smartbox_update/rcsp_ch_loader_download.c \
	apps/watch/smartbox/smartbox_update/smartbox_update.c \
	apps/watch/smartbox/smartbox_update/smartbox_update_tws.c \
	apps/watch/smartbox/smartbox_update_cat1_module.c \
	apps/watch/smartbox/switch_device.c \
	apps/watch/sport/manage_source.c \
	apps/watch/sport/message_vm_cfg.c \
	apps/watch/sport/sport_api.c \
	apps/watch/task_manager/app_common.c \
	apps/watch/task_manager/app_net_event.c \
	apps/watch/task_manager/app_task_switch.c \
	apps/watch/task_manager/bt/att_over_bredr.c \
	apps/watch/task_manager/bt/bt.c \
	apps/watch/task_manager/bt/bt_ble.c \
	apps/watch/task_manager/bt/bt_emitter.c \
	apps/watch/task_manager/bt/bt_event_fun.c \
	apps/watch/task_manager/bt/bt_key_fun.c \
	apps/watch/task_manager/bt/bt_product_test.c \
	apps/watch/task_manager/bt/bt_switch_fun.c \
	apps/watch/task_manager/bt/bt_tws.c \
	apps/watch/task_manager/bt/vol_sync.c \
	apps/watch/task_manager/cat1/cat1.c \
	apps/watch/task_manager/cat1/cat1_aicxtek.c \
	apps/watch/task_manager/cat1/cat1_unisoc.c \
	apps/watch/task_manager/idle/idle.c \
	apps/watch/task_manager/music/music.c \
	apps/watch/task_manager/pc/pc.c \
	apps/watch/task_manager/power_off/power_off.c \
	apps/watch/task_manager/power_on/power_on.c \
	apps/watch/task_manager/record/record.c \
	apps/watch/task_manager/rtc/alarm_api.c \
	apps/watch/task_manager/rtc/alarm_user.c \
	apps/watch/task_manager/rtc/rtc.c \
	apps/watch/task_manager/rtc/virtual_rtc.c \
	apps/watch/task_manager/task_key.c \
	apps/watch/third_party_profile/ancs_client_demo/ancs_client_demo.c \
	apps/watch/third_party_profile/trans_data_demo/trans_data_demo.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/demo/photos/load_jpeg_from_sd.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/demo/photos/ui_demo_photo_details.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/demo/photos/ui_demo_photo_preview.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/demo/ui_demo_app_dock.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/demo/ui_demo_app_dock_v2.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/demo/ui_demo_bezier_anim.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/demo/ui_demo_inertial.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/demo/ui_demo_waterfall_menu.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/gfps_user_api.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/ui_action_aidial.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/ui_action_alarm.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/ui_action_altimeter.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/ui_action_breath_train.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/ui_action_calculator.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/ui_action_calendar.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/ui_action_detection.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/ui_action_dial.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/ui_action_drawer.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/ui_action_file.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/ui_action_findmy_setting.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/ui_action_findphone.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/ui_action_flashlight.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/ui_action_heat.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/ui_action_iflytek.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/ui_action_menu.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/ui_action_menu_star.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/ui_action_music.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/ui_action_music_bg.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/ui_action_music_menu.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/ui_action_netapp_iflytek.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/ui_action_notice.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/ui_action_oxygen.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/ui_action_pc.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/ui_action_pressure.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/ui_action_pulldown_menu.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/ui_action_qrcode.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/ui_action_record.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/ui_action_set.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/ui_action_sport_info.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/ui_action_sportrecord.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/ui_action_standby_dial.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/ui_action_stopwatch.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/ui_action_train.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/ui_action_train_status.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/ui_action_transitcode.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/ui_action_voice.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/ui_action_watch.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/ui_action_watchex.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/ui_action_weather.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/ui_batcharge_action.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/ui_bt_page_action.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/ui_bt_page_api.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/ui_bt_scan.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/ui_cat1_aicxtek_engineering_mode.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/ui_cat1_unisoc_engineering_mode.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/ui_page_touch.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/ui_phone_action.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/ui_phone_sel_action.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/ui_phonebook_action.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/ui_scan_touch.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/ui_sms_call_sel_action.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/ui_sportrecord_browse.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/ui_timer_action.c \
	apps/watch/ui/lcd/STYLE_WATCH_NEW/ui_upgrade_action.c \
	apps/watch/ui/lcd/lyrics_api.c \
	apps/watch/ui/lcd/ui_screen_saver.c \
	apps/watch/ui/lcd/ui_sys_param_api.c \
	apps/watch/ui/lcd/watch_syscfg_manage.c \
	apps/watch/ui/led/pwm_led_api.c \
	apps/watch/ui/led/pwm_led_para_table.c \
	apps/watch/ui/lua_ui/lua_bsp.c \
	apps/watch/ui/lua_ui/lua_bt.c \
	apps/watch/ui/lua_ui/lua_device.c \
	apps/watch/ui/lua_ui/lua_gui.c \
	apps/watch/ui/lua_ui/lua_module.c \
	apps/watch/ui/lua_ui/lua_music.c \
	apps/watch/ui/lua_ui/lua_obj.c \
	apps/watch/ui/lua_ui/lua_obj_battery.c \
	apps/watch/ui/lua_ui/lua_obj_compass.c \
	apps/watch/ui/lua_ui/lua_obj_image.c \
	apps/watch/ui/lua_ui/lua_obj_layout.c \
	apps/watch/ui/lua_ui/lua_obj_list.c \
	apps/watch/ui/lua_ui/lua_obj_multiprogress.c \
	apps/watch/ui/lua_ui/lua_obj_number.c \
	apps/watch/ui/lua_ui/lua_obj_progress.c \
	apps/watch/ui/lua_ui/lua_obj_slider.c \
	apps/watch/ui/lua_ui/lua_obj_text.c \
	apps/watch/ui/lua_ui/lua_obj_time.c \
	apps/watch/ui/lua_ui/lua_obj_watch.c \
	apps/watch/ui/lua_ui/lua_sys.c \
	apps/watch/ui/lua_ui/lua_utils.c \
	apps/watch/ui/lua_ui/lua_weather.c \
	apps/watch/user_api/dev_multiplex_api.c \
	apps/watch/user_api/product_info_api.c \
	apps/watch/user_api/vm_api.c \
	apps/watch/version.c \
	cpu/br28/adc_api.c \
	cpu/br28/audio_codec_clock.c \
	cpu/br28/audio_common/app_audio.c \
	cpu/br28/audio_common/sound_device_driver.c \
	cpu/br28/audio_common/sound_pcm_demo.c \
	cpu/br28/audio_cvp_dut.c \
	cpu/br28/audio_dec/audio_dec.c \
	cpu/br28/audio_dec/audio_dec_bt.c \
	cpu/br28/audio_dec/audio_dec_file.c \
	cpu/br28/audio_dec/audio_dec_fm.c \
	cpu/br28/audio_dec/audio_dec_linein.c \
	cpu/br28/audio_dec/audio_dec_midi_ctrl.c \
	cpu/br28/audio_dec/audio_dec_midi_file.c \
	cpu/br28/audio_dec/audio_dec_pc.c \
	cpu/br28/audio_dec/audio_dec_record.c \
	cpu/br28/audio_dec/audio_dec_spdif.c \
	cpu/br28/audio_dec/audio_dec_tone.c \
	cpu/br28/audio_dec/audio_spectrum.c \
	cpu/br28/audio_dec/audio_usb_mic.c \
	cpu/br28/audio_dec/lfwordana_enc_api.c \
	cpu/br28/audio_dec/tone_player.c \
	cpu/br28/audio_effect/audio_autotune_demo.c \
	cpu/br28/audio_effect/audio_ch_swap_demo.c \
	cpu/br28/audio_effect/audio_dynamic_eq_demo.c \
	cpu/br28/audio_effect/audio_eff_default_parm.c \
	cpu/br28/audio_effect/audio_eq_drc_demo.c \
	cpu/br28/audio_effect/audio_gain_process_demo.c \
	cpu/br28/audio_effect/audio_noise_gate_demo.c \
	cpu/br28/audio_effect/audio_sound_track_2_p_x.c \
	cpu/br28/audio_effect/audio_surround_demo.c \
	cpu/br28/audio_effect/audio_vbass_demo.c \
	cpu/br28/audio_effect/audio_voice_changer_demo.c \
	cpu/br28/audio_effect/effects_adj.c \
	cpu/br28/audio_effect/eq_config.c \
	cpu/br28/audio_enc/audio_adc_demo.c \
	cpu/br28/audio_enc/audio_enc.c \
	cpu/br28/audio_enc/audio_enc_file.c \
	cpu/br28/audio_enc/audio_enc_recoder.c \
	cpu/br28/audio_enc/audio_mic_codec.c \
	cpu/br28/audio_enc/audio_sbc_codec.c \
	cpu/br28/audio_iis.c \
	cpu/br28/audio_link.c \
	cpu/br28/audio_mic_capless.c \
	cpu/br28/audio_way/audio_app_stream.c \
	cpu/br28/audio_way/audio_bt_emitter/audio_bt_emitter.c \
	cpu/br28/audio_way/audio_bt_emitter/audio_bt_emitter_controller.c \
	cpu/br28/audio_way/audio_bt_emitter/audio_bt_emitter_dma.c \
	cpu/br28/audio_way/audio_bt_emitter/audio_bt_emitter_hw.c \
	cpu/br28/audio_way/audio_way.c \
	cpu/br28/audio_way/audio_way_bt_emitter.c \
	cpu/br28/audio_way/audio_way_dac.c \
	cpu/br28/charge.c \
	cpu/br28/chargestore.c \
	cpu/br28/clock_manager.c \
	cpu/br28/exti.c \
	cpu/br28/hw_fft.c \
	cpu/br28/iic_hw.c \
	cpu/br28/iic_soft.c \
	cpu/br28/irflt.c \
	cpu/br28/localtws/localtws.c \
	cpu/br28/localtws/localtws_dec.c \
	cpu/br28/lp_touch_key.c \
	cpu/br28/lp_touch_key_tool.c \
	cpu/br28/lua_port_api.c \
	cpu/br28/mcpwm.c \
	cpu/br28/overlay_code.c \
	cpu/br28/plcnt.c \
	cpu/br28/power/power_app.c \
	cpu/br28/power/power_check.c \
	cpu/br28/power/power_port.c \
	cpu/br28/power/power_trim.c \
	cpu/br28/psram.c \
	cpu/br28/pwm_led.c \
	cpu/br28/rdec_soft.c \
	cpu/br28/setup.c \
	cpu/br28/smart_voice/aispeech_asr.c \
	cpu/br28/smart_voice/iflytek_asr/iflytek_asr.c \
	cpu/br28/smart_voice/jl_kws_platform.c \
	cpu/br28/smart_voice/kws_event.c \
	cpu/br28/smart_voice/nn_vad.c \
	cpu/br28/smart_voice/smart_voice_config.c \
	cpu/br28/smart_voice/smart_voice_core.c \
	cpu/br28/smart_voice/user_asr.c \
	cpu/br28/smart_voice/vad_clock_trim.c \
	cpu/br28/smart_voice/vad_mic.c \
	cpu/br28/smart_voice/voice_mic_data.c \
	cpu/br28/sound_iis.c \
	cpu/br28/spi.c \
	cpu/br28/uart_v1.c \
	cpu/br28/ui_driver/LED_1888/LED1888.c \
	cpu/br28/ui_driver/imb_demo.c \
	cpu/br28/ui_driver/interface/ui_platform.c \
	cpu/br28/ui_driver/jpeg_demo.c \
	cpu/br28/ui_driver/lcd_drive/lcd_drive.c \
	cpu/br28/ui_driver/lcd_drive/lcd_mcu/lcd_mcu_JD5858_320x385.c \
	cpu/br28/ui_driver/lcd_drive/lcd_mcu/lcd_mcu_JD5858_360x360.c \
	cpu/br28/ui_driver/lcd_drive/lcd_rgb/lcd_rgb_ILI6485_480x272.c \
	cpu/br28/ui_driver/lcd_drive/lcd_rgb/lcd_rgb_st7789v_240x240.c \
	cpu/br28/ui_driver/lcd_drive/lcd_spi/lcd_qspi_ICNA3310B_466x466.c \
	cpu/br28/ui_driver/lcd_drive/lcd_spi/lcd_qspi_ST77903_400x400.c \
	cpu/br28/ui_driver/lcd_drive/lcd_spi/lcd_qspi_ST77903_400x400_v1.c \
	cpu/br28/ui_driver/lcd_drive/lcd_spi/lcd_qspi_ST77903_400x400_v2.c \
	cpu/br28/ui_driver/lcd_drive/lcd_spi/lcd_qspi_ST77903_400x400_v3.c \
	cpu/br28/ui_driver/lcd_drive/lcd_spi/lcd_qspi_ST77903_400x400_v4.c \
	cpu/br28/ui_driver/lcd_drive/lcd_spi/lcd_spi_L_WEA2012_356x400.c \
	cpu/br28/ui_driver/lcd_drive/lcd_spi/lcd_spi_co5300_466x466_v1.c \
	cpu/br28/ui_driver/lcd_drive/lcd_spi/lcd_spi_gc9307_RQ_BOE_240x283.c \
	cpu/br28/ui_driver/lcd_drive/lcd_spi/lcd_spi_rm69330_454x454.c \
	cpu/br28/ui_driver/lcd_drive/lcd_spi/lcd_spi_sh8501a_240x296.c \
	cpu/br28/ui_driver/lcd_drive/lcd_spi/lcd_spi_sh8601a_454x454.c \
	cpu/br28/ui_driver/lcd_drive/lcd_spi/lcd_spi_st7789_BOE1.54_update_240x240.c \
	cpu/br28/ui_driver/lcd_drive/lcd_spi/lcd_spi_st7789v_240x240.c \
	cpu/br28/ui_driver/lcd_drive/lcd_spi/lcd_spi_st7789v_360x360.c \
	cpu/br28/ui_driver/lcd_seg/lcd_seg3x9_driver.c \
	cpu/br28/ui_driver/led7/led7_driver.c \
	cpu/br28/ui_driver/lvgl/lv_port_disp.c \
	cpu/br28/ui_driver/lvgl/lv_port_fs.c \
	cpu/br28/ui_driver/lvgl/lv_port_indev.c \
	cpu/br28/ui_driver/lvgl/lvgl_main.c \
	cpu/br28/ui_driver/ui_common.c \
	cpu/br28/umidigi_chargestore.c \
	cpu/br28/user_p11_cmd.c \
	cpu/periph_demo/dual_update_demo.c \
	cpu/periph_demo/uart_test.c \


# 需要编译的 .S 文件
S_SRC_FILES := \
	apps/watch/sdk_version.z.S \


# 需要编译的 .s 文件
s_SRC_FILES :=


# 需要编译的 .cpp 文件
cpp_SRC_FILES :=


# 需要编译的 .cc 文件
cc_SRC_FILES :=


# 需要编译的 .cxx 文件
cxx_SRC_FILES :=


# 链接参数
LFLAGS := \
	--plugin-opt=-pi32v2-always-use-itblock=false \
	--plugin-opt=-enable-ipra=true \
	--plugin-opt=-pi32v2-merge-max-offset=4096 \
	--plugin-opt=-pi32v2-enable-simd=true \
	--plugin-opt=mcpu=r3 \
	--plugin-opt=-global-merge-on-const \
	--plugin-opt=-inline-threshold=5 \
	--plugin-opt=-inline-max-allocated-size=32 \
	--plugin-opt=-inline-normal-into-special-section=true \
	--plugin-opt=-dont-used-symbol-list=malloc,free,sprintf,printf,puts,putchar \
	--plugin-opt=save-temps \
	--plugin-opt=-pi32v2-enable-rep-memop \
	--plugin-opt=-warn-stack-size=256 \
	--sort-common \
	--plugin-opt=-used-symbol-file=cpu/br28/sdk_used_list.used \
	--plugin-opt=-pi32v2-large-program=true \
	--gc-sections \
	--start-group \
	cpu/br28/liba/cpu.a \
	cpu/br28/liba/system.a \
	cpu/br28/liba/btstack.a \
	cpu/br28/liba/rcsp_stack.a \
	cpu/br28/liba/tme_stack.a \
	cpu/br28/liba/fmna_stack.a \
	cpu/br28/liba/google_fps.a \
	cpu/br28/liba/lib_fmy_auth_br28.a \
	cpu/br28/liba/fmy_MotionDetection.a \
	cpu/br28/liba/btctrler.a \
	cpu/br28/liba/aec.a \
	cpu/br28/liba/media.a \
	cpu/br28/liba/libepmotion.a \
	cpu/br28/liba/libAptFilt_pi32v2_OnChip.a \
	cpu/br28/liba/libEchoSuppress_pi32v2_OnChip.a \
	cpu/br28/liba/libNoiseSuppress_pi32v2_OnChip.a \
	cpu/br28/liba/libSplittingFilter_pi32v2_OnChip.a \
	cpu/br28/liba/libDelayEstimate_pi32v2_OnChip.a \
	cpu/br28/liba/libSMS_TDE_pi32v2_OnChip.a \
	cpu/br28/liba/libDualMicSystem_pi32v2_OnChip.a \
	cpu/br28/liba/libDualMicSystem_flexible_pi32v2_OnChip.a \
	cpu/br28/liba/libAdaptiveEchoSuppress_pi32v2_OnChip.a \
	cpu/br28/liba/libOpcore_maskrom_pi32v2_OnChip.a \
	cpu/br28/liba/lib_resample_cal.a \
	cpu/br28/liba/lib_resample_fast_cal.a \
	cpu/br28/liba/sbc_eng_lib.a \
	cpu/br28/liba/mp3_dec_lib.a \
	cpu/br28/liba/mp3_decstream_lib.a \
	cpu/br28/liba/mp3_enc_lib.a \
	cpu/br28/liba/wav_dec_lib.a \
	cpu/br28/liba/wma_dec_lib.a \
	cpu/br28/liba/flac_dec_lib.a \
	cpu/br28/liba/ape_dec_lib.a \
	cpu/br28/liba/dts_dec_lib.a \
	cpu/br28/liba/amr_dec_lib.a \
	cpu/br28/liba/opus_dec_lib.a \
	cpu/br28/liba/lib_midi_dec.a \
	cpu/br28/liba/lib_mp2_code.a \
	cpu/br28/liba/lib_adpcm_code.a \
	cpu/br28/liba/lib_adpcm_ima_code.a \
	cpu/br28/liba/speex_enc_lib.a \
	cpu/br28/liba/amr_enc_lib.a \
	cpu/br28/liba/lc3_codec_lib.a \
	cpu/br28/liba/agreement.a \
	cpu/br28/liba/media_app.a \
	cpu/br28/liba/lc3_dec_lib.a \
	cpu/br28/liba/opus_enc_lib.a \
	cpu/br28/liba/SpectrumShow.a \
	cpu/br28/liba/qr_code.a \
	cpu/br28/liba/libjlsp.a \
	cpu/br28/liba/drc.a \
	cpu/br28/liba/lfaudio_plc_lib.a \
	cpu/br28/liba/lib_esco_repair.a \
	cpu/br28/liba/pedometer.a \
	cpu/br28/liba/sleep.a \
	cpu/br28/liba/wrist_bright.a \
	cpu/br28/liba/Preprocess.a \
	cpu/br28/liba/fall_detection.a \
	cpu/br28/liba/motion_detection.a \
	cpu/br28/liba/HeartRate_Calc.a \
	cpu/br28/liba/sedentary.a \
	cpu/br28/liba/SPO2.a \
	cpu/br28/liba/p33_algo_motion.a \
	cpu/br28/liba/hx3602_algo.a \
	cpu/br28/liba/vc_lib_hr_v637_jieli_NA_01.a \
	cpu/br28/liba/vc_lib_spo2_lc11s_int_JieLi_NA_01.a \
	cpu/br28/liba/vc_lib_sportMotion_std_int_jieli_NA_01.a \
	cpu/br28/liba/libjlsp_kws_smart_watch.a \
	cpu/br28/liba/libkwscommon.a \
	cpu/br28/liba/lib_pitch_speed.a \
	cpu/br28/liba/quickjs.a \
	cpu/br28/liba/alpay_iot_vendor.a \
	cpu/br28/liba/noisegate.a \
	cpu/br28/liba/libnlfaultinjection.a \
	cpu/br28/liba/libFFT_pi32v2_OnChip.a \
	cpu/br28/liba/wtg_dec_lib.a \
	cpu/br28/liba/bfilterfun_lib.a \
	cpu/br28/liba/crypto_toolbox_Osize.a \
	cpu/br28/liba/lib_dns.a \
	cpu/br28/liba/compressor.a \
	cpu/br28/liba/crossover_coff.a \
	cpu/br28/liba/limiter.a \
	cpu/br28/liba/update.a \
	cpu/br28/liba/ui_new.a \
	cpu/br28/liba/ui_draw_new.a \
	cpu/br28/liba/font_new.a \
	cpu/br28/liba/res_new.a \
	cpu/br28/liba/ui_flash_vm.a \
	cpu/br28/liba/ui_expand.a \
	cpu/br28/liba/gif.a \
	cpu/br28/liba/lwip_2_1_3.a \
	cpu/br28/liba/mbedtls_2_26_0.a \
	cpu/br28/liba/http_cli.a \
	cpu/br28/liba/websocket.a \
	apps/common/pay/upay_t_head/lib/JL/libadapt_for_se.a \
	apps/common/pay/upay_t_head/lib/JL/libalipay_base.a \
	apps/common/pay/upay_t_head/lib/JL/libalipay_crypto.a \
	apps/common/pay/upay_t_head/lib/JL/libalipay_framework.a \
	apps/common/pay/upay_t_head/lib/JL/libalipay_pay.a \
	apps/common/pay/upay_t_head/lib/JL/libalipay_mbedtls.a \
	apps/common/pay/upay_t_head/lib/JL/libalipay_secure_cert.a \
	apps/common/pay/upay_t_head/lib/JL/libalipay_iot_device_name.a \
	apps/common/pay/upay_t_head/lib/JL/libalipay_iot_netcore.a \
	apps/common/pay/upay_t_head/lib/JL/libALIPAY_IoTPay_network.a \
	apps/common/pay/upay_t_head/lib/JL/libalipay_iot_rpc.a \
	apps/common/pay/upay_t_head/lib/JL/libalipay_transit.a \
	apps/common/pay/upay_t_head/lib/JL/libprotobuf-c.a \
	apps/common/pay/upay_t_head/lib/JL/libalipay_sm2.a \
	apps/common/device/gSensor/silan/silan_data.a \
	apps/common/device/gSensor/silan/silan_motion_algo.a \
	apps/common/audio/local_tts/iflytek/lib/lib_ifly_tts.a \
	apps/common/cat1/aicxtek/aic_srv/libs/libaic_srv.a \
	apps/common/cat1/unisoc/libs/cit.a \
	apps/common/cat1/unisoc/libs/cms.a \
	apps/common/cat1/unisoc/libs/cmux.a \
	apps/common/cat1/unisoc/libs/data_parse.a \
	apps/common/cat1/unisoc/libs/diag_at.a \
	apps/common/cat1/unisoc/libs/liblpa.a \
	apps/common/cat1/unisoc/libs/mdload.a \
	apps/common/cat1/unisoc/libs/modem_control.a \
	apps/common/cat1/unisoc/libs/netif.a \
	apps/common/cat1/unisoc/libs/rpc_api.a \
	apps/common/cat1/unisoc/libs/rpc_core.a \
	apps/common/cat1/unisoc/libs/thinmodem.a \
	--end-group \
	-Tcpu/br28/sdk.ld \
	-M=cpu/br28/tools/sdk.map \
	--plugin-opt=mcpu=r3 \
	--plugin-opt=-mattr=+fprev1 \


LIBPATHS := \
	-L$(SYS_LIB_DIR) \


LIBS := \
	$(SYS_LIB_DIR)/libm.a \
	$(SYS_LIB_DIR)/libc.a \
	$(SYS_LIB_DIR)/libm.a \
	$(SYS_LIB_DIR)/libcompiler-rt.a \



c_OBJS    := $(c_SRC_FILES:%.c=%.c.o)
S_OBJS    := $(S_SRC_FILES:%.S=%.S.o)
s_OBJS    := $(s_SRC_FILES:%.s=%.s.o)
cpp_OBJS  := $(cpp_SRC_FILES:%.cpp=%.cpp.o)
cxx_OBJS  := $(cxx_SRC_FILES:%.cxx=%.cxx.o)
cc_OBJS   := $(cc_SRC_FILES:%.cc=%.cc.o)

OBJS      := $(c_OBJS) $(S_OBJS) $(s_OBJS) $(cpp_OBJS) $(cxx_OBJS) $(cc_OBJS)
DEP_FILES := $(OBJS:%.o=%.d)


OBJS      := $(addprefix $(BUILD_DIR)/, $(OBJS))
DEP_FILES := $(addprefix $(BUILD_DIR)/, $(DEP_FILES))


VERBOSE ?= 0
ifeq ($(VERBOSE), 1)
QUITE :=
else
QUITE := @
endif

# 一些旧的 make 不支持 file 函数，需要 make 的时候指定 LINK_AT=0 make
LINK_AT ?= 1

# 表示下面的不是一个文件的名字，无论是否存在 all, clean, pre_build 这样的文件
# 还是要执行命令
# see: https://www.gnu.org/software/make/manual/html_node/Phony-Targets.html
.PHONY: all clean pre_build

# 不要使用 make 预设置的规则
# see: https://www.gnu.org/software/make/manual/html_node/Suffix-Rules.html
.SUFFIXES:

all: pre_build $(OUT_ELF)
	$(info +POST-BUILD)
	$(QUITE) $(RUN_POST_SCRIPT) sdk

pre_build:
	$(info +PRE-BUILD)
	$(QUITE) $(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -D__LD__ -E -P cpu/br28/sdk_used_list.c -o cpu/br28/sdk_used_list.used
	$(QUITE) $(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -D__LD__ -E -P cpu/br28/sdk_ld.c -o cpu/br28/sdk.ld
	$(QUITE) $(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -D__LD__ -E -P cpu/br28/tools/download.c -o $(POST_SCRIPT)
	$(QUITE) $(FIXBAT) $(POST_SCRIPT)
	$(QUITE) $(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -D__LD__ -E -P cpu/br28/tools/isd_config_rule.c -o cpu/br28/tools/isd_config.ini

clean:
	$(QUITE) $(RM) $(OUT_ELF)
	$(QUITE) $(RM) $(BUILD_DIR)



ifeq ($(LINK_AT), 1)
$(OUT_ELF): $(OBJS)
	$(info +LINK $@)
	$(shell $(MKDIR) $(@D))
	$(file >$(OBJ_FILE), $(OBJS))
	$(QUITE) $(LD) -o $(OUT_ELF) @$(OBJ_FILE) $(LFLAGS) $(LIBPATHS) $(LIBS)
else
$(OUT_ELF): $(OBJS)
	$(info +LINK $@)
	$(shell $(MKDIR) $(@D))
	$(QUITE) $(LD) -o $(OUT_ELF) $(OBJS) $(LFLAGS) $(LIBPATHS) $(LIBS)
endif


$(BUILD_DIR)/%.c.o : %.c
	$(info +CC $<)
	$(QUITE) $(MKDIR) $(@D)
	$(QUITE) $(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -MMD -MF $(@:.o=.d) -c $< -o $@

$(BUILD_DIR)/%.S.o : %.S
	$(info +AS $<)
	$(QUITE) $(MKDIR) $(@D)
	$(QUITE) $(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -MMD -MF $(@:.o=.d) -c $< -o $@

$(BUILD_DIR)/%.s.o : %.s
	$(info +AS $<)
	$(QUITE) $(MKDIR) $(@D)
	$(QUITE) $(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -MMD -MF $(@:.o=.d) -c $< -o $@

$(BUILD_DIR)/%.cpp.o : %.cpp
	$(info +CXX $<)
	$(QUITE) $(MKDIR) $(@D)
	$(QUITE) $(CXX) $(CXXFLAGS) $(CFLAGS) $(DEFINES) $(INCLUDES) -MMD -MF $(@:.o=.d) -c $< -o $@

$(BUILD_DIR)/%.cxx.o : %.cxx
	$(info +CXX $<)
	$(QUITE) $(MKDIR) $(@D)
	$(QUITE) $(CXX) $(CXXFLAGS) $(CFLAGS) $(DEFINES) $(INCLUDES) -MMD -MF $(@:.o=.d) -c $< -o $@

$(BUILD_DIR)/%.cc.o : %.cc
	$(info +CXX $<)
	$(QUITE) $(MKDIR) $(@D)
	$(QUITE) $(CXX) $(CXXFLAGS) $(CFLAGS) $(DEFINES) $(INCLUDES) -MMD -MF $(@:.o=.d) -c $< -o $@

-include $(DEP_FILES)
