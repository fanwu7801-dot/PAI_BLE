#ifndef __USER_INFO_FILE__
#define __USER_INFO_FILE__

#include "typedef.h"
#include "fs/fs.h"

/**
 * @file User_music_set.h
 * @brief 配置文件操作API接口头文件
 *
 * 提供文件创建、写入、读取等操作的封装接口
 */

/**
 * @brief 文件操作结果枚举
 */
typedef enum {
  FILE_OP_SUCCESS = 0,         ///< 操作成功
  FILE_OP_ERROR_INIT_FAILED,   ///< 文件系统初始化失败
  FILE_OP_ERROR_CREATE_FAILED, ///< 文件创建失败
  FILE_OP_ERROR_WRITE_FAILED,  ///< 文件写入失败
  FILE_OP_ERROR_READ_FAILED,   ///< 文件读取失败
  FILE_OP_ERROR_INVALID_PARAM, ///< 无效参数
  FILE_OP_ERROR_FILE_NOT_EXIST ///< 文件不存在
} file_op_result_t;

/**
 * @brief 文件配置结构体
 */
typedef struct {
  const char *file_path; ///< 文件路径
  int max_size;          ///< 最大文件大小（字节）
  const char *mode;      ///< 打开模式（"r", "w", "w+", "a"等）
} file_config_t;

// USER 自定义区域默认文件路径（对应 isd_config_rule.c/isd_config.ini 里 RESERVED_CONFIG 的名称 UWTG0）
// 说明：VFS 路径固定为 mnt/sdfile/app/<reserved_name_lowercase>
#ifndef USER_INFO_DEFAULT_PATH
#define USER_INFO_DEFAULT_PATH  "mnt/sdfile/app/uwtg0"
#endif

/**
 * @brief 初始化文件系统
 * @param base_path 基础路径，如果为NULL则使用默认路径
 * @return 操作结果
 */
file_op_result_t file_system_init(const char *base_path);

/**
 * @brief 反初始化文件系统
 */
void file_system_uninit(void);

/**
 * @brief 创建文件
 * @param config 文件配置参数
 * @param file_handle 返回的文件句柄指针
 * @return 操作结果
 */
file_op_result_t file_create(const file_config_t *config,
                             FILE **file_handle);

/**
 * @brief 写入文件数据
 * @param file_handle 文件句柄
 * @param data 要写入的数据指针
 * @param data_size 单次写入数据大小
 * @param write_count 写入次数
 * @return 操作结果
 */
// 按 offset 写入（offset<0 表示从当前文件指针处写）
int file_write_data(FILE *file_handle, const void *data, int data_size,
                    int offset);

/**
 * @brief 读取文件数据
 * @param config 文件配置参数
 * @param buffer 读取数据缓冲区
 * @param buffer_size 缓冲区大小
 * @param read_count 读取次数
 * @return 操作结果
 */
file_op_result_t file_read_data(const file_config_t *config, void *buffer,
                                int buffer_size, int read_count);

/**
 * @brief 关闭文件
 * @param file_handle 文件句柄
 */
void file_close(FILE *file_handle);

// 获取 USER 区域句柄和属性（用于擦除/范围校验）
FILE *user_info_zone_open(const char *mode, struct vfs_attr *attr);

// 擦除 USER 区域（按 4K sector 擦除；写入前需要调用）
int user_info_zone_erase(FILE *user_fp, const struct vfs_attr *attr);

// 资源区 -> USER 区域拷贝（用于提示音写入 USER 区域演示）
int user_copy_file(const char *src_file_name, const char *dest_file_name);

// UART/调试：按路径拷贝并立即播放（用于验证“拷贝到文件系统后播放”的链路是否正确）
int user_copy_file_and_play(const char *src_file_name, const char *dest_file_name, u8 preemption);

// UART/调试：固定用例（res/tone/0.wtg -> mnt/sdfile/app/user），并播放
int user_custom_tone_copy_res0_to_user_and_play(void);

/**
 * @brief 生成测试数据
 * @param buffer 数据缓冲区
 * @param size 缓冲区大小
 * @param reverse 是否反向填充数据
 */
void generate_test_data(char *buffer, int size, bool reverse);

/**
 * @brief 示例使用函数
 */
void user_set_music_file(void);

// 业务：优先播放自定义音效（0=开机，1=关机，2=报警，3=hello）。
// 返回 true 表示已成功触发播放；false 表示不存在或播放失败。
bool user_custom_tone_play_if_exist(uint8_t tone_type, u8 preemption);

//=====================================================================
// 自定义音效 meta 相关定义（共享给其他模块使用）
//=====================================================================
#define CUSTOM_TONE_META_MAGIC       0x544F4E45u /* 'TONE' */
#define CUSTOM_TONE_MAX_TYPES        4
#define CUSTOM_TONE_SLOT_MAX_SIZE    0x180000u /* 1.5MB */

typedef struct {
  uint32_t magic;
  uint8_t tone_type;
  uint8_t name_len;
  uint16_t reserved;
  uint64_t tone_id;
  uint32_t file_size;
  uint32_t crc32;
  char file_name[32];
} custom_tone_meta_t;

// 获取槽位对应的文件路径
const char *custom_tone_slot_path(uint8_t tone_type);

// 获取槽位对应的 syscfg ID
int custom_tone_meta_cfg_id(uint8_t tone_type);

// 查看个性音效传输报告（从文件中读取）
void custom_tone_print_transfer_report(void);

//=====================================================================
// 音效选择相关（默认音效 vs 个性音效）
//=====================================================================
// 音效选择类型
#define TONE_SELECT_DEFAULT   0  // 使用默认音效（走 0x00F4 串口播放）
#define TONE_SELECT_CUSTOM    1  // 使用个性音效（播放用户上传的音效）

// 保存用户音效选择（tone_type: 0~3, select: 0=默认, 1=个性）
int user_tone_select_save(uint8_t tone_type, uint8_t select);

// 读取用户音效选择（返回 0=默认, 1=个性, -1=读取失败）
int user_tone_select_get(uint8_t tone_type);

// 根据用户选择播放音效（内部自动判断走默认还是个性）
// tone_type: 0=开机, 1=关机, 2=报警, 3=Hello
// default_tone_id: 默认音效的 tone_id（用于 0x00F4 串口播放）
// preemption: 是否抢占
// 返回: 0=成功, -1=失败
int user_tone_play_by_selection(uint8_t tone_type, uint16_t default_tone_id, u8 preemption);

#endif // !__USER_INFO_FILE__
