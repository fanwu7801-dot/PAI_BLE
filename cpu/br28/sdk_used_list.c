#include "app_config.h"

#if TCFG_USER_TWS_ENABLE
tws_local_media_sync
#if TCFG_EQ_ONLINE_ENABLE
tws_ci_data
#endif /* #if TCFG_EQ_ONLINE_ENABLE */
tws_acl_data_sync
tws_event_sync
tws_conn_sync
tws_lmp_slot_sync
tws_low_latency
tws_media_sync
tws_power_balance
tws_sync_call
tws_tx_sync
tws_link_sync
tws_afh_sync
#endif /* #if TCFG_USER_TWS_ENABLE */

#if CONFIG_FATFS_ENABLE
fat_vfs_ops
#endif

#if VFS_ENABLE
sdfile_vfs_ops

#if TCFG_NORFLASH_SFC_DEV_ENABLE || TCFG_NORFLASH_DEV_ENABLE || TCFG_VIRFAT_INSERT_FLASH_ENABLE || TCFG_NANDFLASH_DEV_ENABLE
nor_fs_vfs_ops
nor_sdfile_vfs_ops
#endif

#if TCFG_NOR_REC
nor_fs_vfs_ops
nor_rec_fs_vfs_ops
#endif

#if TCFG_VIRFAT_FLASH_ENABLE || TCFG_VIRFAT_INSERT_FLASH_ENABLE
fat_sdfile_fat_ops
#endif

#endif /*VFS_ENABLE*/

#if FLASH_INSIDE_REC_ENABLE
inside_nor_fs_vfs_ops
#endif

#ifndef TCFG_DEC_SBC_CLOSE
sbc_decoder
#endif

#ifndef TCFG_DEC_MSBC_CLOSE
msbc_decoder
#endif

#ifndef TCFG_DEC_SBC_HWACCEL_CLOSE
sbc_hwaccel
#endif

#ifndef TCFG_DEC_CVSD_CLOSE
cvsd_decoder
#endif

#ifndef TCFG_DEC_PCM_CLOSE
pcm_decoder
#endif

#if TCFG_DEC_MTY_ENABLE
mty_decoder
#endif

#if TCFG_DEC_MP3_ENABLE
mp3_decoder
#if TCFG_DEC2TWS_ENABLE
mp3pick_decoder
#endif
#endif

#if TCFG_DEC_WMA_ENABLE
wma_decoder
#if TCFG_DEC2TWS_ENABLE
wmapick_decoder
#endif
#endif

#if TCFG_DEC_FLAC_ENABLE
flac_decoder
#endif

#if TCFG_DEC_APE_ENABLE
ape_decoder
#endif

#if TCFG_DEC_M4A_ENABLE
m4a_decoder
#if TCFG_DEC2TWS_ENABLE
m4apick_decoder
#endif
#endif

#if TCFG_DEC_ALAC_ENABLE
alac_decoder
#endif

#if TCFG_DEC_AMR_ENABLE
amr_decoder
#endif

#if TCFG_DEC_DTS_ENABLE
dts_decoder
#endif

#if TCFG_DEC_G729_ENABLE || TCFG_BT_SUPPORT_G729
g729_decoder
#endif

#if TCFG_DEC_WTGV2_ENABLE
wtgv2_decoder
#endif

#if TCFG_DEC_AAC_ENABLE || TCFG_BT_SUPPORT_AAC
aac_decoder
#endif

#if TCFG_DEC_G726_ENABLE
g726_decoder
#endif

#if TCFG_DEC_MIDI_ENABLE
midi_decoder
#endif

#if TCFG_DEC_WAV_ENABLE
wav_decoder
#endif

#if AUDIO_MIDI_CTRL_CONFIG
midi_ctrl_decoder
#endif

#if TCFG_DEC_SPEEX_ENABLE
speex_decoder
#endif

#if TCFG_DEC_OPUS_ENABLE
opus_decoder
#endif

#if TCFG_DEC_LC3_ENABLE
lc3_decoder
#endif





#if TCFG_ENC_CVSD_ENABLE
cvsd_encoder
#endif

#if TCFG_ENC_MSBC_ENABLE
msbc_encoder
#endif

#if TCFG_ENC_MP3_ENABLE
mp3_encoder
#endif

#if TCFG_ENC_G726_ENABLE
g726_encoder
#endif

#if TCFG_ENC_ADPCM_ENABLE
adpcm_encoder
#endif

#if TCFG_ENC_PCM_ENABLE
pcm_encoder
#endif

#if TCFG_ENC_OPUS_ENABLE
opus_encoder
#endif

#if TCFG_ENC_SPEEX_ENABLE
speex_encoder
#endif

#if TCFG_ENC_LC3_ENABLE
lc3_encoder
#endif

#if TCFG_ENC_SBC_ENABLE
sbc_encoder
#endif


#ifdef AUDIO_OUT_WAY_TYPE
#if (AUDIO_OUT_WAY_TYPE & AUDIO_WAY_TYPE_DAC)
audio_dac_driver
#endif /*(AUDIO_OUT_WAY_TYPE & AUDIO_WAY_TYPE_DAC)*/
#endif /*AUDIO_OUT_WAY_TYPE*/

#if TCFG_CAT1_AICXTEK_ENABLE
__initcall_pthread_init
#endif/* #if TCFG_CAT1_AICXTEK_ENABLE */

/* smartbox 串口/协议相关：在 LTO + used-symbol-file 模式下强制保留这些符号 */
g_uart_sn_valid
uart_sn_buffer
uart1_init
uart1_send_toMCU

rssi_check_init
rssi_on_connect
rssi_on_disconnect
rssi_timer_stop
rssi_timer_start_by_cid

wdg_mgr_init
wdg_mgr_register
wdg_mgr_heartbeat
wdg_mgr_unregister

per_key_scan_init
per_key_scan_enable
per_key_scan_mac
per_key_scan_on_adv_report_ex

protocol_id
send_protocl
content_data
content_length
fill_send_protocl
convert_protocol_to_buffer
fill_SOC_Phone_protocl
fill_protocol_ble_on_can_send_now
fill_protocol_ble_tx_reset

smbox_pairing_init
smbox_pairing_on_pair_process

ble_proto_ble_pair_req_Proc
send_ble_key_list
vehicle_control_timer_start
vehicle_control_timer_stop
user_custom_tone_play_if_exist
