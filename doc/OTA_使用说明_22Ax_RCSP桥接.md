# OTA 使用说明（22Ax -> RCSP 桥接版）

## 1. 适用范围
- 本文档对应当前工程 OTA 首版实现：`22Ax` 仅做控制层，真实固件下载走 `RCSP` 原生链路。
- 升级目标：主固件 App 区。

## 2. 当前架构
- `fill_protocol` 的 `0x22A0~0x22A5` 已接入 OTA 门面。
- `ota_test` 负责统一状态机与错误码。
- `smartbox_update + update_loader_download` 负责真实下载、校验、写入与升级流程。

## 3. 状态与错误码
- 状态码：
1. `0` `OTA_IDLE`
2. `1` `OTA_READY`
3. `2` `OTA_DOWNLOADING`
4. `3` `OTA_VERIFYING`
5. `4` `OTA_DONE`
6. `5` `OTA_FAILED`

- 错误码：
1. `0` `OTA_ERR_NONE`
2. `1` `OTA_ERR_STATE`
3. `2` `OTA_ERR_PARAM`
4. `3` `OTA_ERR_TIMEOUT`
5. `4` `OTA_ERR_CRC`
6. `5` `OTA_ERR_FLASH`
7. `6` `OTA_ERR_LOW_BAT`

## 4. 手机侧调用顺序（推荐）
1. 发送 `0x22A0 (ota_query_cap)` 查询能力。
2. 发送 `0x22A1 (ota_enter)` 开始 OTA 会话。
3. 切换到 RCSP 升级流程（`0xE3~0xE7`）进行真实固件传输。
4. 轮询 `0x22A4 (ota_status_query)` 直到 `state=OTA_DONE` 或 `OTA_FAILED`。
5. 若失败或主动取消，发送 `0x22A5 (ota_exit)` 清理会话。
6. OTA 成功后执行设备重启（通常由 RCSP 流程中的 `0xE7` 触发）。

## 5. 22Ax 各指令行为说明
- `ota_query_cap (0x22A0)`：
1. 返回支持标记与 `max_pkt_len`。
2. 返回当前 OTA 状态。

- `ota_enter (0x22A1)`：
1. 做低电量检查（极低电量直接失败）。
2. 触发 `ota_test_start_rcsp()`，进入 RCSP 升级通道。
3. `payload` 支持携带总大小（用于状态回包展示）。

- `ota_block_req (0x22A2)`：
1. 当前版本不走 22Ax 直传。
2. 返回 `ret_code=3`（`OTA_BRIDGE_RET_NOT_DIRECT`），表示“请走 RCSP 数据面”。

- `ota_block_data (0x22A3)`：
1. 当前版本不做直刷写入。
2. 返回 `ret_code=3`。

- `ota_status_query (0x22A4)`：
1. 回包结构包含：`state`、`last_error`、`received_size`、`total_size`。
2. 数据来自 `ota_test` 统一状态。

- `ota_exit (0x22A5)`：
1. 调用 `ota_test_abort()` 清理会话。
2. 状态回到 `IDLE`。

## 6. 设备侧真实升级链路
1. `22Ax` 进入后调用 `ota_test_start_rcsp()`。
2. `ota_test` 调用 `smartbox_rcsp_ota_bridge_start()`。
3. `smartbox_update` 启动 `MSG_JL_LOADER_DOWNLOAD_START` 对应流程。
4. `update_loader_download` 通过 RCSP `0xE3~0xE7` 完成下载与校验。
5. 回调 `ota_test_on_rcsp_result()` 更新统一状态。
6. 成功后执行设备重启，切换到新固件。

## 7. 异常与断链处理
- BLE/RCSP 断链时：调用 `ota_test_on_disconnect()`，状态置 `FAILED`，错误码 `TIMEOUT`。
- 低电量：`ota_enter` 阶段返回失败，错误码 `LOW_BAT`。
- 校验或写入异常：由 RCSP 升级回调映射为 `FLASH/FAILED`。

## 8. 联调建议
1. 先验证 `ota_enter` 后 `ota_status_query` 能从 `IDLE/READY` 进入 `DOWNLOADING`。
2. 验证 `ota_block_req/ota_block_data` 必须返回 `ret=3`（符合桥接设计）。
3. 验证升级成功后状态到 `DONE`，并可正常重启生效。
4. 验证断链后状态为 `FAILED`，再发 `ota_exit` 可回 `IDLE` 并可再次发起。

## 9. 关键代码位置
- `apps/watch/smartbox/bt_manage/bt_trans_data/fill_protocol.c`
- `apps/watch/smartbox/ota_test/ota_test.c`
- `apps/watch/smartbox/smartbox_update/smartbox_update.c`
- `apps/watch/smartbox/smartbox_update/rcsp_ch_loader_download.c`
- `apps/watch/smartbox/smartbox_update/smartbox_update.h`

## 10. 刷写实操（手机侧）

### 10.1 前置条件检查
1. 连接状态：BLE 已连接且链路稳定；升级期间避免主动断链。
2. 电量条件：设备不在极低电量（`ota_enter` 会拦截低电量）。
3. 镜像合法性：使用与当前机型匹配的 OTA 包，避免跨产品包。
4. 版本策略：确认是否允许降级；若不允许，APP 侧提前拦截。
5. 协议能力：APP 侧必须支持 RCSP 升级流程（`0xE3~0xE7`）。

### 10.2 标准时序（逐步）
1. `0x22A0` 查询能力：确认设备支持桥接 OTA，获取 `max_pkt_len` 与当前状态。
2. `0x22A1` 进入会话：触发设备进入 RCSP 升级准备态。
3. 启动 RCSP 升级：按 RCSP 流程执行进入升级、按需回包固件块、查询刷新状态、重启。
4. `0x22A4` 轮询状态：观察 `DOWNLOADING -> VERIFYING -> DONE` 或 `FAILED`。
5. 失败清理：`0x22A5` 退出，回到 `IDLE` 后可重试。

### 10.3 请求/响应字段说明（payload 级）

#### A. `0x22A0 (ota_query_cap)`
- 用途：查询桥接 OTA 能力与当前状态。
- 请求 payload：空。
- 响应 payload（固定 6 字节）：
1. `byte0`：`ret`，`0` 表示成功。
2. `byte1`：能力标记，当前实现返回 `1`（支持桥接 OTA）。
3. `byte2`：保留位（当前为 `0`）。
4. `byte3~4`：`max_pkt_len`（大端），当前默认 `180`。
5. `byte5`：当前 OTA 状态（见状态码定义）。
- 成功判定：`ret=0` 且 `byte1=1`。
- 失败处理：链路检查后重发；若连续失败，先断开重连。

#### B. `0x22A1 (ota_enter)`
- 用途：创建 OTA 会话并触发 RCSP 数据面升级。
- 请求 payload（建议）：
1. 仅传 4 字节 `total_size`（大端）。
2. 不建议首版传 channel+size 复合字段，避免字段重叠歧义。
- 响应 payload：`ret + 10字节状态块`。
1. `ret`：`0` 成功，`1` 失败。
2. 状态块 `10` 字节：
   - `state(1)`
   - `last_error(1)`
   - `received_size(4, BE)`
   - `total_size(4, BE)`
- 成功判定：`ret=0` 且状态进入 `READY` 或 `DOWNLOADING`。
- 失败处理：
1. 若 `last_error=LOW_BAT`，提示充电后重试。
2. 其他失败执行 `ota_exit` 后重进。

#### C. `0x22A4 (ota_status_query)`
- 用途：查询实时升级状态。
- 请求 payload：空。
- 响应 payload：`ret + 10字节状态块`（同 `ota_enter`）。
- 成功判定：`ret=0`。
- 失败处理：
1. 若 `state=FAILED`，读取 `last_error` 后走失败恢复流程。
2. 若长时间 `DOWNLOADING` 不变化，检查 RCSP 数据面是否正常传输。

#### D. `0x22A2 (ota_block_req)` / `0x22A3 (ota_block_data)`
- 用途：兼容协议保留。
- 当前行为：返回 `ret=3`（`OTA_BRIDGE_RET_NOT_DIRECT`）。
- 含义：当前版本不支持 22Ax 直传固件数据，必须走 RCSP 数据面。
- 失败处理：APP 不应重试该数据面，应切换/继续 RCSP 传输逻辑。

### 10.4 关键注意事项（必须）
1. `22Ax` 不是固件数据面，只是控制与状态面。
2. 真实固件下载必须由 RCSP 升级链路完成。
3. `ota_enter` payload 建议只传 4 字节 `total_size`，避免 channel/size 字段重叠歧义。
4. 升级中禁止主动断链、切后台释放连接或切换设备。
5. 不要以 `ota_block_*` 是否成功来判定刷写成功，判定依据是 `ota_status_query` + RCSP 升级结果。

### 10.5 不要做什么
1. 不要把 `0x22A2/0x22A3` 当成 bin 传输接口。
2. 不要在 `FAILED` 状态直接重新推数据，必须先 `ota_exit` 清理。
3. 不要跳过 RCSP 的 `0xE3~0xE7` 正规流程。

### 10.6 最小可跑流程（示例）
1. `query_cap(0x22A0)` -> 期望 `ret=0`、能力=1、状态=`IDLE`。
2. `enter(0x22A1, total_size)` -> 期望 `ret=0`、状态=`READY/DOWNLOADING`。
3. 开始 RCSP 升级（`0xE3~0xE7`）。
4. 每 500ms~1s 轮询 `status_query(0x22A4)`。
5. 状态到 `DONE` 后，确认设备重启并上报新版本。

### 10.7 失败后恢复流程（示例）
1. 发现 `state=FAILED`，记录 `last_error`。
2. 发 `ota_exit(0x22A5)`，确认回到 `IDLE`。
3. 重发 `ota_query_cap(0x22A0)` 验证会话已清理。
4. 重新 `ota_enter(0x22A1)` 并再次走 RCSP 升级。

## 11. 失败处理与重试策略

### 11.1 `LOW_BAT`
- 现象：`ota_enter` 返回失败，`last_error=OTA_ERR_LOW_BAT`。
- 动作：提示用户充电或提高电量后再执行。

### 11.2 `TIMEOUT`
- 现象：升级中断链或超时，状态变 `FAILED`。
- 动作：
1. 保持连接稳定后执行 `ota_exit`。
2. 重新 `query_cap -> enter`。
3. 重新走 RCSP 升级。

### 11.3 `FLASH`
- 现象：RCSP 回调映射失败，状态 `FAILED` 且 `last_error=FLASH`。
- 动作：
1. 核对 OTA 包合法性与机型匹配。
2. 重新发起完整升级流程。
3. 若连续失败，保留日志并停止自动重试。

### 11.4 标准重试流程
1. `ota_exit(0x22A5)`
2. `ota_query_cap(0x22A0)`
3. `ota_enter(0x22A1)`
4. 重新执行 RCSP `0xE3~0xE7`

## 12. 联调抓包检查清单

### 12.1 成功路径检查点
1. `0x22A0` 返回能力=1。
2. `0x22A1` 返回 `ret=0`。
3. RCSP 升级阶段存在 `0xE3~0xE7` 正常交互。
4. `0x22A4` 状态最终到 `DONE`。
5. 设备重启并进入新版本。

### 12.2 失败路径检查点
1. 低电量时 `ota_enter` 直接失败并上报 `LOW_BAT`。
2. 断链后状态变 `FAILED/TIMEOUT`。
3. `ota_block_*` 恒为 `ret=3`（桥接模式正确行为）。
4. `ota_exit` 后状态可回到 `IDLE`。

### 12.3 建议日志关键字与代码定位
1. `ota_test_start_rcsp`：会话启动入口。
2. `smartbox_rcsp_ota_bridge_start`：桥接到 RCSP 的关键函数。
3. `ota_test_on_rcsp_result`：成功/失败落状态。
4. `rcsp_update_state_cbk`：RCSP 升级结束回调。
5. `handle_ota_cmd`：22Ax 指令分发。

## FAQ

### Q1：为什么 `ota_block_req/ota_block_data` 总是返回 3？
- 因为当前版本是桥接模式，22Ax 不承担固件数据面，`ret=3` 是明确提示“请走 RCSP 传输”。

### Q2：为什么 `ota_status_query` 一直是 `DOWNLOADING`？
- 常见原因是 RCSP 数据面没有实际推进（APP 没有完成 `0xE3~0xE7` 交互，或链路卡住）。

### Q3：何时发送 `0xE7` 重启？
- 在 RCSP 升级流程确认下载/校验完成后发送；通常由既有 RCSP 升级逻辑控制。

## 13. AB（双备份）模式说明（701N 当前工程）

### 13.1 当前配置
1. 目标板：`CONFIG_BOARD_701N_DEMO`。
2. 双备份开关：`CONFIG_DOUBLE_BANK_ENABLE = 1`。
3. 运行时能力：`support_dual_bank_update_en` 将随编译配置变为 `1`。

### 13.2 AB 模式下的升级行为
1. 升级数据写入非当前运行分区（目标分区）。
2. 下载结束后执行校验与升级结果提交。
3. 仅当校验/提交成功，才允许重启切换到新固件。
4. 校验失败或流程中断时，保持旧分区可启动，不做生效切换。

### 13.3 手机侧时序（保持不变）
1. `0x22A1` 建立 OTA 会话（22Ax 控制面）。
2. `0xE3~0xE7` 执行真实下载、校验、重启（RCSP 数据面）。
3. `0x22A4` 查询状态用于观测过程与失败码。

### 13.4 联调必查项（AB）
1. 构建后检查生成的 `isd_config.ini` 包含 `BR22_TWS_DB = YES`。
2. 构建后确认不再走单备份分支 `NEW_FLASH_FS = YES`。
3. 成功场景：`E6` 成功后发 `E7`，重启后版本号变化为新版本。
4. 失败场景：`E6` 失败或中断时，重启后仍是旧版本且可再次升级。

### 13.5 判定规则（AB）
1. `E5` 传完不等于升级成功，必须以 `E6` 结果为准。
2. 没有 `E6` 成功结论时，不应触发 `E7` 生效重启。
3. 若状态长期停在 `DOWNLOADING`，优先排查 RCSP 数据面是否持续推进。
