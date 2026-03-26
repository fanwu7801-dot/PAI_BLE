#include "user_info_file.h"

#include "asm/sfc_norflash_api.h"
#include "device/ioctl_cmds.h"
#include "fs/sdfile.h"

#include "generic/printf.h"
#include "tone_player.h"
#include "user_cfg_id.h"

#include <string.h>

// 前置声明
static void user_info_ensure_dir_exists_for_path(const char *file_path);

// 播放回调函数
static void tone_play_callback(void *priv, int flag) {
  printf("[CUSTOM_TONE][PLAY_CALLBACK] flag=%d\n", flag);
  if (flag == TONE_STOP) {
    extern void bt_tone_pa_ctrl_set(u8 on);
    bt_tone_pa_ctrl_set(0);
  }
}

static void custom_tone_print_head(const char *tag, const char *path)
{
  if (!tag) {
    tag = "CUSTOM_TONE";
  }
  if (!path || !path[0]) {
    printf("[%s] head: invalid path\n", tag);
    return;
  }

  FILE *fp = fopen(path, "rb");
  if (!fp) {
    printf("[%s] head: open failed: %s\n", tag, path);
    return;
  }

  u8 head[16];
  memset(head, 0, sizeof(head));
  int r = fread(fp, head, sizeof(head));
  fclose(fp);

  if (r <= 0) {
    printf("[%s] head: read failed: %s\n", tag, path);
    return;
  }

  printf("[%s] head(%d): %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
         tag, r,
         head[0], head[1], head[2], head[3], head[4], head[5], head[6], head[7],
         head[8], head[9], head[10], head[11], head[12], head[13], head[14], head[15]);

  if (r >= 4) {
    if (head[0] == 'R' && head[1] == 'I' && head[2] == 'F' && head[3] == 'F') {
      printf("[%s] hint: looks like WAV (RIFF).\n", tag);
    } else if (head[0] == 'I' && head[1] == 'D' && head[2] == '3') {
      printf("[%s] hint: looks like MP3 (ID3).\n", tag);
    } else if (head[0] == 'O' && head[1] == 'g' && head[2] == 'g' && head[3] == 'S') {
      printf("[%s] hint: looks like OGG/Opus (OggS).\n", tag);
    }
  }
}

// 查看传输报告
void custom_tone_print_transfer_report(void)
{
  const char *report_path = "mnt/sdfile/app/uwav/transfer_report.txt";
  FILE *fp = fopen(report_path, "r");
  if (!fp) {
    printf("\n无法打开传输报告文件: %s\n", report_path);
    printf("可能还没有进行过个性音效传输\n");
    return;
  }
  
  printf("\n========== 个性音效传输报告 ==========\n");
  // 使用 static 缓冲区避免栈溢出
  static char line[128];
  while (fgets(line, sizeof(line), fp)) {
    printf("%s", line);
  }
  fclose(fp);
  printf("\n报告路径: %s\n", report_path);
  printf("==========================================\n\n");
}

static int custom_tone_copy_exact_bytes(const char *src_path, const char *dst_path, u32 bytes)
{
  if (!src_path || !dst_path || bytes == 0) {
    printf("[CUSTOM_TONE] copy_exact_bytes: invalid params\n");
    return -1;
  }

  printf("[CUSTOM_TONE] copy_exact_bytes: src=%s, dst=%s, bytes=%u\n", src_path, dst_path, bytes);

  FILE *src = fopen(src_path, "rb");
  if (!src) {
    printf("[CUSTOM_TONE] copy_exact_bytes: failed to open src: %s\n", src_path);
    return -1;
  }
  
  FILE *dst = fopen(dst_path, "wb");
  if (!dst) {
    printf("[CUSTOM_TONE] copy_exact_bytes: failed to open dst: %s\n", dst_path);
    fclose(src);
    return -1;
  }

  // 使用 static 缓冲区避免栈溢出
  static u8 buf[256];
  u32 left = bytes;
  u32 copied = 0;
  
  while (left) {
    u32 chunk = left > sizeof(buf) ? sizeof(buf) : left;
    int r = fread(src, buf, chunk);
    if (r <= 0) {
      printf("[CUSTOM_TONE] copy_exact_bytes: read failed at offset %u\n", copied);
      fclose(src);
      fclose(dst);
      return -1;
    }
    int w = fwrite(dst, buf, r);
    if (w != r) {
      printf("[CUSTOM_TONE] copy_exact_bytes: write failed, wrote %d/%d bytes\n", w, r);
      fclose(src);
      fclose(dst);
      return -1;
    }
    left -= (u32)r;
    copied += (u32)r;
  }

  fclose(src);
  fclose(dst);
  printf("[CUSTOM_TONE] copy_exact_bytes: success, copied %u bytes\n", copied);
  return 0;
}

// 定义已移至 user_info_file.h，此处保留注释以便理解
// #define CUSTOM_TONE_META_MAGIC       0x544F4E45u
// #define CUSTOM_TONE_MAX_TYPES        4
// #define CUSTOM_TONE_SLOT_MAX_SIZE    0x180000u
// typedef struct custom_tone_meta_t ... (see user_info_file.h)

const char *custom_tone_slot_path(uint8_t tone_type)
{
  // 路径不带后缀，用于读写 reserved 区域
  static const char *paths[CUSTOM_TONE_MAX_TYPES] = {
      "mnt/sdfile/app/uwtg0",
      "mnt/sdfile/EXT_RESERVED/UWTG1",
      "mnt/sdfile/EXT_RESERVED/UWTG2",
      "mnt/sdfile/EXT_RESERVED/UWTG3",
  };
  if (tone_type >= CUSTOM_TONE_MAX_TYPES) {
    return paths[0];
  }
  return paths[tone_type];
}

int custom_tone_meta_cfg_id(uint8_t tone_type)
{
  switch (tone_type) {
  case 0:
    return CFG_CUSTOM_TONE_META_0;
  case 1:
    return CFG_CUSTOM_TONE_META_1;
  case 2:
    return CFG_CUSTOM_TONE_META_2;
  case 3:
    return CFG_CUSTOM_TONE_META_3;
  default:
    return CFG_CUSTOM_TONE_META_0;
  }
}

// 根据文件头检测音频格式并返回相应后缀
static const char *detect_audio_format_suffix(const char *path)
{
  if (!path || !path[0]) {
    return ".wtg";
  }

  u8 head[16] = {0};
  FILE *fp = fopen(path, "rb");
  if (!fp) {
    printf("[CUSTOM_TONE] detect_format: open failed\n");
    return ".wtg";
  }

  int rlen = fread(fp, head, sizeof(head));
  fclose(fp);

  if (rlen < 4) {
    printf("[CUSTOM_TONE] detect_format: read failed, use default .wtg\n");
    return ".wtg";
  }

  // 检测 WAV 格式: RIFF xxxx WAVE
  if (rlen >= 12 && !memcmp(head, "RIFF", 4) && !memcmp(head + 8, "WAVE", 4)) {
    printf("[CUSTOM_TONE] detected format: WAV\n");
    return ".wav";
  }
  // 检测 MP3 格式: ID3 标签
  else if (rlen >= 3 && !memcmp(head, "ID3", 3)) {
    printf("[CUSTOM_TONE] detected format: MP3 (ID3)\n");
    return ".mp3";
  }
  // 检测 MP3 格式: 帧同步标记 (0xFF 0xEx)
  else if (rlen >= 2 && head[0] == 0xFF && ((head[1] & 0xE0) == 0xE0)) {
    printf("[CUSTOM_TONE] detected format: MP3 (frame sync)\n");
    return ".mp3";
  }
  // 检测 Ogg/Opus 格式: OggS
  else if (rlen >= 4 && !memcmp(head, "OggS", 4)) {
    printf("[CUSTOM_TONE] detected format: Opus/Ogg\n");
    return ".opus";
  }
  // 检测 AAC 格式: 0xFF 0xFx
  else if (rlen >= 2 && head[0] == 0xFF && ((head[1] & 0xF0) == 0xF0)) {
    printf("[CUSTOM_TONE] detected format: AAC\n");
    return ".aac";
  }

  // 默认使用 .wtg (G729)
  printf("[CUSTOM_TONE] detected format: unknown, use default .wtg\n");
  return ".wtg";
}

bool user_custom_tone_play_if_exist(uint8_t tone_type, u8 preemption)
{
  if (tone_type >= CUSTOM_TONE_MAX_TYPES) {
    printf("[CUSTOM_TONE] ERROR: invalid tone_type=%u (max=%u)\n", tone_type, CUSTOM_TONE_MAX_TYPES);
    return false;
  }

  // tone_play_with_callback_by_name 需要可写字符串，避免在栈上分配大缓冲
  static char play_name_buf[64];

  custom_tone_meta_t meta;
  memset(&meta, 0, sizeof(meta));

  int cfg_id = custom_tone_meta_cfg_id(tone_type);
  int rlen = syscfg_read(cfg_id, &meta, sizeof(meta));
  if (rlen != (int)sizeof(meta)) {
    printf("[CUSTOM_TONE] meta not found for slot %u\n", tone_type);
    return false;
  }
  if (meta.magic != CUSTOM_TONE_META_MAGIC) {
    printf("[CUSTOM_TONE] invalid meta magic for slot %u: 0x%08X\n", tone_type, (unsigned)meta.magic);
    return false;
  }
  if (meta.tone_type != tone_type) {
    printf("[CUSTOM_TONE] meta tone_type mismatch: %u != %u\n", meta.tone_type, tone_type);
    return false;
  }
  if (meta.file_size == 0 || meta.file_size > CUSTOM_TONE_SLOT_MAX_SIZE) {
    printf("[CUSTOM_TONE] invalid file_size=%u for slot %u\n", (unsigned)meta.file_size, tone_type);
    return false;
  }

  const char *path = custom_tone_slot_path(tone_type);
  if (!path || !path[0]) {
    printf("[CUSTOM_TONE] no path for slot %u\n", tone_type);
    return false;
  }

  // 诊断：打印 meta 与底层节点属性，帮助定位“杂音/时长不对”
  struct vfs_attr src_attr;
  memset(&src_attr, 0, sizeof(src_attr));
  FILE *src_fp = fopen(path, "rb");
  if (!src_fp) {
    printf("[CUSTOM_TONE] ERROR: cannot open slot %u file: %s\n", tone_type, path);
    return false;
  }
  (void)fget_attrs(src_fp, &src_attr);
  fclose(src_fp);

  // 1) 非常明显的 logo，便于确认是否走了自定义音效
  printf("\n================== CUSTOM_TONE PLAY ==================\n");
  printf("[CUSTOM_TONE] tone_type=%u preemption=%u meta_size=%u meta_crc=0x%08X\n",
         (unsigned)tone_type, (unsigned)preemption,
         (unsigned)meta.file_size, (unsigned)meta.crc32);
  printf("[CUSTOM_TONE] src=%s (vfs_fsize=%u)\n", path, (unsigned)src_attr.fsize);
  custom_tone_print_head("CUSTOM_TONE_SRC", path);
  
  // 2) 直接使用原始路径播放
  //    注意：reserved 区域不支持通配符 .*，解码器会通过文件头自动探测格式
  printf("[CUSTOM_TONE] play_path: %s\n", path);
  
  memset(play_name_buf, 0, sizeof(play_name_buf));
  strncpy(play_name_buf, path, sizeof(play_name_buf) - 1);

  int ret = tone_play_with_callback_by_name(play_name_buf, preemption, tone_play_callback, NULL);
  printf("[CUSTOM_TONE] play ret=%d\n", ret);
  printf("================================================\n\n");
  
  if (ret < 0) {
    return false;
  }
  return true;
}

// 说明：
// - USER 自定义区域在 isd_config.ini 的 [RESERVED_CONFIG] 中配置（名称 UWTG），VFS 路径固定为 mnt/sdfile/app/<name>
// - 由于 norflash 特性，写入前需要先擦除（本文件提供 user_info_zone_erase）

static bool g_file_system_initialized = false;
static char g_user_info_path[64] = USER_INFO_DEFAULT_PATH;

static int user_info_get_flash_range(FILE *fp, const struct vfs_attr *attr_in,
                                     struct vfs_attr *attr_out,
                                     u32 *flash_addr_out,
                                     u32 *size_out)
{
  struct vfs_attr attr = {0};

  if (attr_in) {
    attr = *attr_in;
  } else {
    if (!fp) {
      return -1;
    }
    if (fget_attrs(fp, &attr)) {
      return -1;
    }
  }

  if (attr_out) {
    *attr_out = attr;
  }
  if (flash_addr_out) {
    *flash_addr_out = sdfile_cpu_addr2flash_addr(attr.sclust);
  }
  if (size_out) {
    *size_out = attr.fsize;
  }
  return 0;
}

static bool user_info_is_default_path(const char *path)
{
  if (!path || !path[0]) {
    return false;
  }
  return (0 == strcmp(path, g_user_info_path));
}

static void user_info_ensure_dir_exists_for_path(const char *file_path)
{
  if (!file_path || !file_path[0]) {
    return;
  }

  // 使用 static 缓冲区避免栈溢出
  static char parent_dir[64];
  static char dir_path[64];
  static char tmp_path[64];
  static char mid_folder[20];
  static char folder_slash[40];
  
  // 取 parent_dir（去掉最后一个 '/' 及之后）
  memset(parent_dir, 0, sizeof(parent_dir));
  strncpy(parent_dir, file_path, sizeof(parent_dir) - 1);

  char *last_slash = strrchr(parent_dir, '/');
  if (!last_slash) {
    return;
  }
  *last_slash = '\0';

  // parent_dir 形如：mnt/sdfile/app/uwtg
  // fmk_dir 需要“父路径 + 文件夹名”形式；参考工程内用法 folder 常带前导 '/'
  memset(dir_path, 0, sizeof(dir_path));
  strncpy(dir_path, parent_dir, sizeof(dir_path) - 1);

  char *dir_last = strrchr(dir_path, '/');
  if (!dir_last) {
    return;
  }
  *dir_last = '\0';
  char *folder = dir_last + 1;
  if (!folder[0]) {
    return;
  }

  // 1) 先确保中间层目录存在：mnt/sdfile/app
  //    如果 dir_path 以 /app 结尾，则尝试在其父路径下创建 /app
  {
    memset(tmp_path, 0, sizeof(tmp_path));
    strncpy(tmp_path, dir_path, sizeof(tmp_path) - 1);
    char *p = strrchr(tmp_path, '/');
    if (p && p[1]) {
      // tmp_path = "mnt/sdfile" ; p+1 = "app"
      char *mid = p + 1;
      *p = '\0';
      memset(mid_folder, 0, sizeof(mid_folder));
      snprintf(mid_folder, sizeof(mid_folder), "/%s", mid);
      int r_mid = fmk_dir(tmp_path, mid_folder, 0);
      printf(">>>[user_info]:mkdir %s%s ret=%d\n", tmp_path, mid_folder, r_mid);
    }
  }

  // 2) 创建最终目录：dir_path + /folder
  memset(folder_slash, 0, sizeof(folder_slash));
  snprintf(folder_slash, sizeof(folder_slash), "/%s", folder);
  int r1 = fmk_dir(dir_path, folder_slash, 0);
  printf(">>>[user_info]:mkdir %s%s ret=%d\n", dir_path, folder_slash, r1);

  // 兼容：有的实现可能不需要前导 '/'
  int r2 = fmk_dir(dir_path, folder, 0);
  printf(">>>[user_info]:mkdir %s/%s ret=%d\n", dir_path, folder, r2);
}

/**
 * @brief 初始化文件系统
 * @param base_path 基础路径
 * @return 操作结果
 */
file_op_result_t file_system_init(const char *base_path) {
  if (g_file_system_initialized) {
    return FILE_OP_SUCCESS;
  }

  // 兼容：允许传入自定义 USER 区域路径（默认 USER_INFO_DEFAULT_PATH）
  if (base_path && base_path[0]) {
    strncpy(g_user_info_path, base_path, sizeof(g_user_info_path) - 1);
    g_user_info_path[sizeof(g_user_info_path) - 1] = '\0';
  }

  g_file_system_initialized = true;
  printf(">>>[user_info]:ready, path=%s\n", g_user_info_path);
  return FILE_OP_SUCCESS;
}

/**
 * @brief 反初始化文件系统
 */
void file_system_uninit(void) {
  if (g_file_system_initialized) {
    g_file_system_initialized = false;
    printf(">>>[user_info]:uninit\n");
  }
}

FILE *user_info_zone_open(const char *mode, struct vfs_attr *attr)
{
  const char *open_mode = (mode && mode[0]) ? mode : "r";
  FILE *fp = fopen(g_user_info_path, open_mode);
  if (!fp) {
    return NULL;
  }

  if (attr) {
    memset(attr, 0, sizeof(*attr));
    if (fget_attrs(fp, attr)) {
      memset(attr, 0, sizeof(*attr));
    }
  }
  return fp;
}

int user_info_zone_erase(FILE *user_fp, const struct vfs_attr *attr)
{
  // 按 4K sector 擦除，避免 page erase 在 4K 对齐 flash 上引发不可预知问题
  const u32 sector_size = 4096;
  u32 flash_addr = 0;
  u32 size = 0;

  if (user_info_get_flash_range(user_fp, attr, NULL, &flash_addr, &size)) {
    return -1;
  }
  if (!flash_addr || !size) {
    return -1;
  }

  u32 start = flash_addr & ~(sector_size - 1);
  u32 end = (flash_addr + size + sector_size - 1) & ~(sector_size - 1);

  for (u32 addr = start; addr < end; addr += sector_size) {
    norflash_ioctl(NULL, IOCTL_ERASE_SECTOR, addr);
  }
  return 0;
}

/**
 * @brief 创建文件
 * @param config 文件配置
 * @param file_handle 返回的文件句柄
 * @return 操作结果
 */
file_op_result_t file_create(const file_config_t *config,
                             FILE **file_handle)
{
  if (!g_file_system_initialized) {
    file_system_init(NULL);
  }

  if (config == NULL || file_handle == NULL) {
    return FILE_OP_ERROR_INVALID_PARAM;
  }

  const char *path = (config->file_path && config->file_path[0]) ? config->file_path
                                                                 : g_user_info_path;
  const char *mode = (config->mode && config->mode[0]) ? config->mode : "w+";

  printf(">>>[user_info]:open %s mode=%s\n", path, mode);

  // 仅当目标是“目录/子文件路径”时才 mkdir；USER reserved 节点本身不是目录
  if (!user_info_is_default_path(path) && strchr(path, '/')) {
    user_info_ensure_dir_exists_for_path(path);
  }

  FILE *fp = fopen(path, mode);
  if (!fp) {
    printf(">>>[user_info]:open failed: %s\n", path);
    return FILE_OP_ERROR_CREATE_FAILED;
  }

  // 如果操作的是 USER 默认文件，且为写入模式，则擦除整个 USER 区域
  if (user_info_is_default_path(path) && (strchr(mode, 'w') || strchr(mode, 'a') || strchr(mode, '+'))) {
    struct vfs_attr attr = {0};
    if (0 == fget_attrs(fp, &attr)) {
      user_info_zone_erase(fp, &attr);
      fseek(fp, 0, SEEK_SET);
    }
  }

  *file_handle = fp;
  return FILE_OP_SUCCESS;
}

/**
 * @brief 写入文件数据
 * @param file_handle 文件句柄
 * @param data 数据指针
 * @param data_size 数据大小
 * @param write_count 写入次数
 * @return 操作结果
 */
int file_write_data(FILE *file_handle, const void *data, int data_size, int offset)
{
  if (file_handle == NULL || data == NULL || data_size <= 0) {
    return -1;
  }

  // offset >=0 时按指定偏移写入；用于分包传输写入
  if (offset >= 0) {
    if (fseek(file_handle, offset, SEEK_SET)) {
      return -1;
    }
  }

  int wlen = fwrite(file_handle, (void *)data, (u32)data_size);
  if (wlen != data_size) {
    return -1;
  }
  return 0;
}

/**
 * @brief 读取文件数据
 * @param config 文件配置
 * @param buffer 读取缓冲区
 * @param buffer_size 缓冲区大小
 * @param read_count 读取次数
 * @return 操作结果
 */
file_op_result_t file_read_data(const file_config_t *config, void *buffer,
                                int buffer_size, int read_count) {
  if (!g_file_system_initialized || config == NULL || buffer == NULL ||
      buffer_size <= 0 || read_count <= 0) {
    printf(">>>[file_api]:错误 - 无效参数\n");
    return FILE_OP_ERROR_INVALID_PARAM;
  }

  printf(">>>[file_api]:读取文件: %s\n", config->file_path);

  FILE *file_handle = fopen(config->file_path, "r");
  if (file_handle == NULL) {
    printf(">>>[file_api]:文件打开失败: %s\n", config->file_path);
    return FILE_OP_ERROR_FILE_NOT_EXIST;
  }

  printf(">>>[file_api]:读取数据，缓冲区大小: %d, 次数: %d\n", buffer_size,
         read_count);

  for (int i = 0; i < read_count; i++) {
    int rlen = fread(file_handle, buffer, buffer_size);
    if (rlen > 0) {
      put_buf((const u8 *)buffer, rlen);
    }
  }

  fclose(file_handle);
  printf(">>>[file_api]:数据读取完成\n");
  return FILE_OP_SUCCESS;
}

/**
 * @brief 关闭文件
 * @param file_handle 文件句柄
 */
void file_close(FILE *file_handle) {
  if (file_handle != NULL) {
    fclose(file_handle);
    printf(">>>[file_api]:文件已关闭\n");
  }
}

int user_copy_file(const char *src_file_name, const char *dest_file_name)
{
  FILE *src_file = NULL;
  FILE *dest_file = NULL;
  char buff[100];
  int ret;

  if (!src_file_name || !dest_file_name) {
    return -1;
  }

  src_file = fopen(src_file_name, "rb");
  if (!src_file) {
    return -1;
  }

  // 判断目标是否为自定义区域（USER 区域或 uwtg0/UWTG1/UWTG2/UWTG3）
  // 这些区域都是 Flash 预留区，写入前需要先擦除
  bool is_custom_zone = false;
  if (0 == strcmp(dest_file_name, g_user_info_path)) {
    is_custom_zone = true; // 默认 USER 区域
  } else if (strstr(dest_file_name, "uwtg") || strstr(dest_file_name, "UWTG")) {
    is_custom_zone = true; // uwtg0/UWTG1 等自定义音效区域
  }

  // 如果是自定义区域，先擦除
  if (is_custom_zone) {
    struct vfs_attr attr = {0};
    dest_file = fopen(dest_file_name, "w+");
    if (!dest_file) {
      fclose(src_file);
      return -1;
    }
    if (0 == fget_attrs(dest_file, &attr)) {
      printf("[CUSTOM_TONE][COPY] Erasing custom zone: %s (size=%u)\n", dest_file_name, attr.fsize);
      user_info_zone_erase(dest_file, &attr);
      fseek(dest_file, 0, SEEK_SET);
    }
  } else {
    dest_file = fopen(dest_file_name, "wb");
    if (!dest_file) {
      fclose(src_file);
      return -1;
    }
  }

  int total_bytes = 0;
  do {
    ret = fread(src_file, buff, sizeof(buff));
    if (ret > 0) {
      fwrite(dest_file, buff, ret);
      total_bytes += ret;
    }
  } while (ret > 0);

  printf("[CUSTOM_TONE][COPY] total copied bytes=%d\n", total_bytes);

  fclose(src_file);
  fclose(dest_file);
  return 0;
}

int user_copy_file_and_play(const char *src_file_name, const char *dest_file_name, u8 preemption)
{
  if (!src_file_name || !dest_file_name) {
    return -1;
  }

  if (!g_file_system_initialized) {
    file_system_init(NULL);
  }

  printf("[CUSTOM_TONE][COPY_PLAY] src=%s, dst=%s\n", src_file_name, dest_file_name);

  int ret = user_copy_file(src_file_name, dest_file_name);
  printf("[CUSTOM_TONE][COPY_PLAY] copy ret=%d\n", ret);
  if (ret) {
    return ret;
  }

  // 检查目标文件是否存在
  FILE *check_fp = fopen(dest_file_name, "r");
  if (!check_fp) {
    printf("[CUSTOM_TONE][COPY_PLAY] ERROR: dst file not found after copy: %s\n", dest_file_name);
    return -1;
  }
  struct vfs_attr attr;
  if (fget_attrs(check_fp, &attr)) {
    printf("[CUSTOM_TONE][COPY_PLAY] ERROR: failed to get dst file attrs: %s\n", dest_file_name);
    fclose(check_fp);
    return -1;
  }
  printf("[CUSTOM_TONE][COPY_PLAY] dst file size=%d\n", attr.fsize);
  fclose(check_fp);

  printf("[CUSTOM_TONE][COPY_PLAY] calling tone_play_by_path: %s\n", dest_file_name);
  // 使用回调版本来记录播放事件
  int pret = tone_play_with_callback_by_name((char *)dest_file_name, preemption, tone_play_callback, NULL);
  printf("[CUSTOM_TONE][COPY_PLAY] play ret=%d, preemption=%u\n", pret, preemption);

  // 检查播放状态
  int status = tone_get_status();
  printf("[CUSTOM_TONE][COPY_PLAY] tone status after play=%d\n", status);

  return pret;
}

int user_custom_tone_copy_res0_to_user_and_play(void)
{
  // 备注：该用例用于验证“资源区拷贝->文件->播放”链路。
  // 修复：原始示例里 src 以 'tone0.' 结尾导致打开失败，这里改为带后缀的实际文件名。
  // 如果需要按实际串口下发的文件名来拷贝，请在上层（uart 处理）构造正确路径并调用
  const char *src = "mnt/sdfile/res/tone/tone0.wtg"; // 常见资源后缀
  const char *dst = "mnt/sdfile/app/uwav/tone0.wtg";
  return user_copy_file_and_play(src, dst, 0);
}

/**
 * @brief 生成测试数据
 * @param buffer 缓冲区
 * @param size 缓冲区大小
 * @param reverse 是否反向填充
 */
void generate_test_data(char *buffer, int size, bool reverse) {
  for (int i = 0; i < size; i++) {
    if (reverse) {
      buffer[i] = (size - 1 - i) & 0xff;
    } else {
      buffer[i] = i & 0xff;
    }
  }
  printf(">>>[file_api]:测试数据生成完成，大小: %d, 模式: %s\n", size,
         reverse ? "反向" : "正向");
}

// 示例使用函数
void user_set_music_file(void) {
  printf("\n>>>[file_api]:开始文件操作测试\n");

  // 1. 初始化文件系统
  file_op_result_t result = file_system_init(NULL);
  if (result != FILE_OP_SUCCESS) {
    printf(">>>[file_api]:文件系统初始化失败\n");
    return;
  }

  // 2. 定义文件配置
  file_config_t file1_config = {.file_path = "flash/APP/ufile/eq_cfg_hw.bin",
                                .max_size = 6 * 1024 * 400,
                                .mode = "wb"};

  file_config_t file2_config = {.file_path = "flash/APP/ufile/cfg_tool.bin",
                                .max_size = 1024,
                                .mode = "w+"};

  // 3. 测试数据
  char test_buffer[16] = {0};
  char read_buffer[512] = {0};

  // 4. 测试第一个文件（大文件）
  printf("\n--- 测试第一个文件 ---\n");
  FILE *file_handle;

  // 创建文件
  result = file_create(&file1_config, &file_handle);
  if (result == FILE_OP_SUCCESS) {
    // 生成并写入测试数据
    generate_test_data(test_buffer, sizeof(test_buffer), false);
    file_write_data(file_handle, test_buffer, sizeof(test_buffer), 20);
    file_close(file_handle);

    // 读取验证
    file_read_data(&file1_config, read_buffer, sizeof(test_buffer), 20);
  }

  // 5. 测试第二个文件（小文件）
  printf("\n--- 测试第二个文件 ---\n");

  result = file_create(&file2_config, &file_handle);
  if (result == FILE_OP_SUCCESS) {
    // 生成反向测试数据并写入
    generate_test_data(test_buffer, sizeof(test_buffer), true);
    file_write_data(file_handle, test_buffer, sizeof(test_buffer), 1);
    file_close(file_handle);

    // 读取验证
    file_read_data(&file2_config, read_buffer, sizeof(read_buffer), 1);
  }

  // 6. 清理资源
  file_system_uninit();
  printf(">>>[file_api]:文件操作测试完成\n");
}

//=====================================================================
// 音效选择相关函数实现（默认音效 vs 个性音效）
//=====================================================================

// 获取槽位对应的用户选择 syscfg ID
static int user_tone_select_cfg_id(uint8_t tone_type)
{
  switch (tone_type) {
  case 0:
    return CFG_TONE_USER_SELECT_0;
  case 1:
    return CFG_TONE_USER_SELECT_1;
  case 2:
    return CFG_TONE_USER_SELECT_2;
  default:
    return CFG_TONE_USER_SELECT_0;
  }
}

/**
 * @brief 保存用户音效选择
 * @param tone_type 音效类型 (0=开机, 1=关机, 2=报警, 3=Hello)
 * @param select 选择类型 (0=默认音效, 1=个性音效)
 * @return 0=成功, -1=失败
 */
int user_tone_select_save(uint8_t tone_type, uint8_t select)
{
  if (tone_type >= CUSTOM_TONE_MAX_TYPES) {
    printf("[TONE_SELECT] save: invalid tone_type=%u\n", tone_type);
    return -1;
  }
  
  int cfg_id = user_tone_select_cfg_id(tone_type);
  uint8_t val = (select == TONE_SELECT_CUSTOM) ? TONE_SELECT_CUSTOM : TONE_SELECT_DEFAULT;
  
  int ret = syscfg_write(cfg_id, &val, 1);
  if (ret != 1) {
    printf("[TONE_SELECT] save failed: tone_type=%u, ret=%d\n", tone_type, ret);
    return -1;
  }
  
  printf("[TONE_SELECT] saved: tone_type=%u, select=%u (%s)\n", 
         tone_type, val, val ? "个性音效" : "默认音效");
  return 0;
}

/**
 * @brief 读取用户音效选择
 * @param tone_type 音效类型 (0=开机, 1=关机, 2=报警, 3=Hello)
 * @return 0=默认音效, 1=个性音效, -1=读取失败（默认返回默认音效）
 */
int user_tone_select_get(uint8_t tone_type)
{
  if (tone_type >= CUSTOM_TONE_MAX_TYPES) {
    printf("[TONE_SELECT] get: invalid tone_type=%u, use default\n", tone_type);
    return TONE_SELECT_DEFAULT;
  }
  
  int cfg_id = user_tone_select_cfg_id(tone_type);
  uint8_t val = TONE_SELECT_DEFAULT;
  
  int ret = syscfg_read(cfg_id, &val, 1);
  if (ret != 1) {
    // 读取失败，默认使用默认音效
    printf("[TONE_SELECT] read failed: tone_type=%u, ret=%d, use default\n", tone_type, ret);
    return TONE_SELECT_DEFAULT;
  }
  
  // 确保返回合法值
  if (val != TONE_SELECT_DEFAULT && val != TONE_SELECT_CUSTOM) {
    val = TONE_SELECT_DEFAULT;
  }
  
  printf("[TONE_SELECT] get: tone_type=%u, select=%u (%s)\n", 
         tone_type, val, val ? "个性音效" : "默认音效");
  return val;
}

// 前向声明：串口发送函数
extern int uart1_send_toMCU(uint16_t cmd, const uint8_t *data, uint16_t data_len);

/**
 * @brief 根据用户选择播放音效
 * @param tone_type 音效类型 (0=开机, 1=关机, 2=报警, 3=Hello)
 * @param default_tone_id 默认音效的 tone_id（用于 0x00F4 串口播放）
 * @param preemption 是否抢占播放
 * @return 0=成功, -1=失败
 */
int user_tone_play_by_selection(uint8_t tone_type, uint16_t default_tone_id, u8 preemption)
{
  if (tone_type >= CUSTOM_TONE_MAX_TYPES) {
    printf("[TONE_PLAY] invalid tone_type=%u\n", tone_type);
    return -1;
  }
  
  int select = user_tone_select_get(tone_type);
  
  if (select == TONE_SELECT_CUSTOM) {
    // 用户选择了个性音效
    printf("[TONE_PLAY] tone_type=%u: playing CUSTOM tone\n", tone_type);
    
    // 检查个性音效是否存在
    if (user_custom_tone_play_if_exist(tone_type, preemption)) {
      printf("[TONE_PLAY] custom tone played successfully\n");
      return 0;
    } else {
      // 个性音效不存在，回退到默认音效
      printf("[TONE_PLAY] custom tone not exist, fallback to default\n");
    }
  }
  
  // 使用默认音效：通过 0x00F4 串口发送给 MCU 播放
  printf("[TONE_PLAY] tone_type=%u: playing DEFAULT tone (id=%u) via UART 0x00F4\n", 
         tone_type, default_tone_id);
  
  // 构建 BCD 编码的 tone_id
  uint8_t data[2];
  // 将 tone_id 转换为 BCD 编码
  // 例如: 28 -> 0x00, 0x28; 111 -> 0x01, 0x11
  uint16_t bcd = 0;
  uint16_t val = default_tone_id;
  int shift = 0;
  while (val > 0 && shift < 16) {
    bcd |= ((val % 10) << shift);
    val /= 10;
    shift += 4;
  }
  data[0] = (bcd >> 8) & 0xFF;
  data[1] = bcd & 0xFF;
  
  int ret = uart1_send_toMCU(0x00F4, data, 2);
  if (ret >= 0) {
    printf("[TONE_PLAY] default tone sent via UART: ret=%d\n", ret);
    return 0;
  } else {
    printf("[TONE_PLAY] UART send failed: ret=%d\n", ret);
    return -1;
  }
}
