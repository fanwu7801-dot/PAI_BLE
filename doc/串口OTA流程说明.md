# 设备端串口 OTA 流程与 CRC 说明

## 1. 设备端流程



### 1.1 收包入口

1. 串口收到 FE 帧
2. `uart1_parse_packet()` 解析外层协议
3. 当 `protocol_id == 0x55AA` 时
4. 调用 `dual_ota_app_data_deal(u32 msg, u8 *buf, u32 len)`

### 1.2 START 阶段

设备端识别 START 包的条件：

- 当前状态为 `DUAL_OTA_STEP_START`
- `len == 10`
- `buf[0] == 0x55`
- `buf[1] == 0xAA`

START payload 格式：

```text
55 AA + file_size(4B, little-endian) + file_crc(4B, little-endian)
```

设备端在 START 阶段执行：

1. 解析 `file_size`
2. 解析 `file_crc`
3. 申请 `MAX_PACKET_LEN` 缓冲区
4. 调用 `dual_bank_passive_update_init(file_crc, file_size, pkt_len, NULL)`
5. 调用 `dual_bank_update_allow_check(file_size)`
6. 关闭 flash 写保护
7. 清零 OTA 上下文
8. 回包 `0x00`

当前聚包长度：

```c
#define MAX_PACKET_LEN (512 * 4)
```

即 2048 字节。

### 1.3 DATA 阶段

普通 DATA 包不带业务头，直接是升级文件原始字节流。

设备端在 DATA 阶段执行：

1. 把收到的数据拷贝到 `bin_buf`
2. 累积到 `dual_ota_cache_size`
3. 当缓存达到 `MAX_PACKET_LEN` 时
4. 调用 `dual_bank_update_write(buf, len, dual_ota_write_cb)`
5. 写成功后回包 `0x00`

设备端还会检查：

- `written_size + cache_size + len` 不能超过 `file_size`

超出则直接失败退出。

### 1.4 END 阶段

设备端识别 END 包的条件：

- 当前状态为 `DUAL_OTA_STEP_WRITE`
- `len == 2`
- `buf[0] == 0xAA`
- `buf[1] == 0x55`

END 包 payload：

```text
AA 55
```

设备端在 END 阶段执行：

1. 检查 `written_size + cache_size == file_size`
2. 若不相等，直接失败退出
3. 如果还有尾包缓存，先写最后一包
4. 最后一包写成功后进入 VERIFY
5. 如果没有尾包，直接进入 VERIFY

### 1.5 VERIFY 阶段

VERIFY 阶段由 `dual_ota_update_verify()` 执行。

分两种情况：

- `file_crc != 0`
  - 调用 `dual_bank_update_verify(...)`
- `file_crc == 0`
  - 调用 `dual_bank_update_verify_without_crc()`

校验成功后：

1. 调用 `dual_bank_update_burn_boot_info(...)`
2. 回包 `0x01`
3. 延时复位

校验失败后：

1. 调用 `dual_ota_fail_exit()`
2. 回包 `0x02`
3. 退出 OTA 态

### 1.6 回包定义

SOC端通过 `dual_ota_notify_msg_to_app()` 回包。

回包 payload 为 1 字节：

- `0x00`
  - 继续发送
- `0x01`
  - 升级成功
- `0x02`
  - 升级失败

## 2. CRC 校验算法

当前设备端代码中，OTA 自定义 CRC 入口为：

- `u16 dual_ota_crc_calc(u8 *buf, u32 len, u16 init)`

默认算法是：

- `CRC16-XMODEM`

参数如下：

- 多项式：`0x1021`
- 初值：`init`
- 不异或输出
- 按高位在前方式迭代

设备端默认实现如下：

```c
u16 dual_ota_crc_calc(u8 *buf, u32 len, u16 init)
{
    u16 poly = 0x1021;
    u16 crc = init;
    while (len--) {
        u16 data = *buf++;
        crc ^= data << 8;
        for (u8 i = 0; i < 8; ++i) {
            crc = crc & 0x8000 ? (crc << 1) ^ poly : crc << 1;
        }
    }
    return crc;
}
```

