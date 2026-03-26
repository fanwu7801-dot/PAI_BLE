# MCU 串口 OTA 协议格式

## 1. 文档目的

本文档描述 MCU 端如何通过串口发送升级文件给设备端完成 OTA。

当前对应的设备端入口：

- `apps/watch/smartbox/bt_manage/bt_trans_data/usart0_to_mcu.c`
- `cpu/periph_demo/dual_update_demo.c`

当前升级文件：

- `cpu/br28/tools/download/watch/db_update_data.bin`

## 2. 总体流程

MCU 端发送顺序如下：

1. 发送 START 包
2. 等待设备回 `CONTINUE(0x00)`
3. 分包发送升级文件数据
4. 每发一包后等待设备回 `CONTINUE(0x00)`
5. 全部数据发完后发送 END 包
6. 等待设备回 `SUCCESS(0x01)` 或 `FAIL(0x02)`

## 3. 外层串口帧格式

MCU 发给设备的每一帧都使用以下格式：

```text
FE BA + protocol_id(2B) + data_len(2B) + payload(NB) + crc16(2B) + 0A 0D
```

字段说明：

- `FE BA`
  - 帧头
- `protocol_id`
  - 2 字节，大端
- `data_len`
  - 2 字节，大端
- `payload`
  - 业务数据
- `crc16`
  - 对 `protocol_id + data_len + payload` 计算 CRC16
- `0A 0D`
  - 帧尾

OTA 固定使用：

```text
protocol_id = 0x55AA
```

## 4. OTA 业务 payload 格式

### 4.1 START 包

格式：

```text
55 AA + file_size(4B, little-endian) + file_crc(4B, little-endian)
```

总长度固定为 10 字节。

字段说明：

- `55 AA`
  - OTA 开始标志
- `file_size`
  - 升级文件总大小，单位字节
- `file_crc`
  - 升级文件校验字段

### 4.2 DATA 包

格式：

```text
升级文件原始二进制数据
```

说明：

- 直接从 `db_update_data.bin` 中读取数据发送
- 不要转成 HEX 字符串

### 4.3 END 包

格式：

```text
AA 55
```

总长度固定为 2 字节。

说明：

- END 包不带其他字段
- 设备端收到后才进入最终校验流程

## 5. `file_crc` 

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



## 6. MCU 发送时序

### 步骤 1：发送 START 包

payload：

```text
55 AA + file_size(LE4) + file_crc(LE4)
```

data_len：

```text
0x000A
```

设备期望回复：

```text
payload = 00
```

### 步骤 2：循环发送 DATA 包

MCU 从 `db_update_data.bin` 中按块读出数据，逐包发送。

每一包：

1. `protocol_id = 0x55AA`
2. `payload = 文件数据块`
3. `data_len = 当前数据块长度`
4. 发送后等待设备回复

设备期望回复：

```text
payload = 00
```

如果收到：

```text
payload = 02
```

表示升级失败，MCU 端必须停止发送。

### 步骤 3：发送 END 包

payload：

```text
AA 55
```

data_len：

```text
0x0002
```

设备可能回复：

- `00`
  - 正在处理尾包或准备校验
- `01`
  - 升级成功
- `02`
  - 升级失败

## 7. 设备回包格式

设备回 MCU 时，外层格式相同：

```text
FE BA + protocol_id + data_len + payload + crc16 + 0A 0D
```

其中：

- `protocol_id = 0x55AA`
- `data_len = 0x0001`
- `payload` 为 1 字节状态码

状态码定义：

- `0x00`
  - CONTINUE
- `0x01`
  - SUCCESS
- `0x02`
  - FAIL

## 8. MCU 端必须遵守的规则

### 8.1 必须先发 START

未发送 START 就直接发 DATA，设备端会判定失败。

### 8.2 必须发送 END

即使文件数据已经全部发完，如果没有发 END 包，设备端也不会进入最终校验成功流程。

### 8.3 发送的数据总量必须严格等于 `file_size`

如果 MCU 实际发送的数据量：

- 少于 `file_size`
- 多于 `file_size`

设备端都会失败退出。

### 8.4 DATA 必须是原始二进制


正确做法：

- 从 `db_update_data.bin` 直接读取原始字节
- 原样作为 payload 发送

### 8.5 每包发送后必须等待设备应答

推荐策略：

1. 发一包
2. 等待设备回 `0x00`
3. 收到后再发下一包

## 9. 推荐的 MCU 伪代码

```c
open_file("db_update_data.bin");
file_size = get_file_size();
file_crc = 0;   // 先传 0，按当前设备端逻辑走 without_crc

send_start_packet(file_size, file_crc);
wait_rsp();
if (rsp != 0x00) {
    fail();
}

while ((len = read_file(buf, CHUNK_SIZE)) > 0) {
    send_data_packet(buf, len);
    wait_rsp();
    if (rsp != 0x00) {
        fail();
    }
}

send_end_packet();

while (1) {
    wait_rsp();
    if (rsp == 0x01) {
        success();
        break;
    }
    if (rsp == 0x02) {
        fail();
        break;
    }
}
```

## 10. 示例

### 10.1 START 包示例

假设：

- `protocol_id = 0x55AA`
- `file_size = 0x00001000`
- `file_crc = 0x00000000`

则业务 payload：

```text
55 AA 00 10 00 00 00 00 00 00
```

### 10.2 END 包示例

业务 payload：

```text
AA 55
```

