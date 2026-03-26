# 修复v5_1_function_control中音频播放导致SOC崩溃问题

## 问题根源
v5_1_function_control在持有veh_ctrl_mutex锁时调用uart1_send_toMCU()，而UART通信可能触发音频播放（0x00A2/0x00A3指令），导致死锁。

## 修复方案
采用"释放锁后再发送UART"的策略，避免在持有锁时进行可能触发音频播放的操作。

## 修改内容

### 文件：fill_protocol.c
修改v5_1_function_control函数中的7个case分支：

1. **APP_FUNC_CODE_EBIKE_UNLOCK**（车辆解锁）
2. **APP_FUNC_CODE_EBIKE_LOCK**（车辆加锁）
3. **APP_FUNC_CODE_OPEN_SEAT**（打开座桶）
4. **APP_FUNC_CODE_FIND_EBIKE**（查找车辆）
5. **APP_FUNC_CODE_REBOOT_EBIKE**（重启中控）
6. **APP_FUNC_CODE_PAIR_NFC**（配对NFC）
7. **APP_FUNC_CODE_PAIR_REMOTE_KEY**（配对遥控器）

### 修改模式
每个case分支：
1. 保存必要的局部变量（protocol_id, content_data, content_length等）
2. 调用veh_ctrl_unlock()释放锁
3. 执行uart1_send_toMCU()和send_data_to_ble_post()
4. 调用reply_power_state_change_to_app()（如果需要）
5. 调用veh_ctrl_lock()重新获取锁

## 预期效果
- 消除死锁风险
- 保持业务逻辑不变
- 减少锁持有时间，提升并发性能