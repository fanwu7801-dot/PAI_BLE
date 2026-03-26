JL701n_watch_release_v3.0.2_multi_ble_关闭UI - sdk_ota_testing
=====================================================

项目概述
--
本仓库为 JL701N 系列手表固件（release v3.0.2）相关的 SDK 与 OTA 测试工程集合，面向嵌入式固件开发、调试、串口 OTA 下发与验证。仓库包含：

- 上层示例应用与功能模块（音频、蓝牙、运动等）。
- 芯片相关底层驱动与平台适配层。
- PC 端 OTA 工具与脚本，用于通过串口/桥接设备下发固件。

本文档为开发者与维护者的快速参考，涵盖项目结构、构建流程、OTA 使用、调试要点与贡献规范。

主要目录说明（高层）
--
- `apps/`：上层应用及示例，包含 `watch/` 主应用（例如 [apps/watch/app_main.c](apps/watch/app_main.c)），以及通用模块 `common/`。
- `cpu/`：平台与芯片相关文件（驱动、时钟、电源管理、I/O 接口实现等）。
- `driver/`：外设驱动集合，用于对接具体硬件。
- `OTA_up_computer/`：PC 端 OTA 工具与脚本，包含 `OTA.py`、`OTA_gui.py`（用于串口/桥接器下发固件）。
- `include_lib/`、`btstack/`：第三方或本地蓝牙协议栈、头文件与接口实现。
- `tools/`：构建与辅助脚本（如 `make_prompt.bat`、`.vscode` 相关任务等）。
- `doc/`：协议与流程文档（例如串口 OTA 流程、协议格式、使用说明）。
- `debug_log/`：运行或测试时产生日志的历史记录，供问题定位使用。
- `objs/`：构建产物目录（由构建系统生成，通常不提交修改）。

常见文件链接：
- 构建入口： [Makefile](Makefile)
- 主应用入口示例： [apps/watch/app_main.c](apps/watch/app_main.c)
- OTA 文档： [doc/串口OTA流程说明.md](doc/串口OTA流程说明.md)
- Agent 开发约束： [agents.md](agents.md)

快速开始
--
先决条件（建议）
- Windows 10/11 或 WSL/Ubuntu（用于类 Unix 构建环境）。
- Python 3.7+（用于运行 PC 端 OTA 脚本）。
- 交叉编译工具链（例如 `arm-none-eabi-gcc` 或厂商提供的工具链）；确保将工具链加入 `PATH`。

在 Windows（推荐）
1. 打开命令行（或 PowerShell），切换到仓库根目录。
2. 运行构建脚本：

```
.vscode/winmk.bat all      # 构建所有目标
.vscode/winmk.bat clean    # 清理构建产物
```

在 Linux / WSL

```
make all
make clean
```

（说明：具体构建目标可能依赖项目内的 `.vscode/winmk.bat`，Makefile 里可能有针对平台的调用，若遇到缺少交叉编译器或工具，请参考内部文档或联系维护者。）

OTA 与调试
--
PC 端 OTA 工具
- 工具位于 `OTA_up_computer/`，包含 `OTA.py`（命令行）与 `OTA_gui.py`（图形界面）。通常流程为：选择串口 → 选择固件文件（bin）→ 下发并校验。
- 运行示例：

```
python OTA_up_computer/OTA.py --help
python OTA_up_computer/OTA_gui.py
```

（请根据本地 Python 环境安装必要依赖，例如 `pyserial`，若仓库中存在 `requirements.txt` 可优先使用。）

串口 OTA 协议
- 详细协议与流程见 [doc/串口OTA流程说明.md](doc/串口OTA流程说明.md) 与 [doc/MCU串口OTA协议格式.md](doc/MCU串口OTA协议格式.md)。请严格按文档操作，避免在下发过程中断电或中断串口连接。

运行时日志与问题定位
- 运行或调试过程中产生的日志保存在 `debug_log/`（历史日志）以及运行时串口输出。定位问题时：先检查串口参数 → 查看 OTA 工具输出 → 对照 `debug_log/`。

开发规范与贡献指南
--
工程约束
- 请务必阅读并遵守仓库根目录的 [agents.md](agents.md) 要求，关键点包括：
	- 所有说明与沟通使用简体中文。
	- 优先保证稳定性，最小化改动范围。
	- 禁止在 ISR 中阻塞或大量打印日志；避免 busy-wait。
	- 错误必须显式处理或记录，避免 silent-failure。

提交与分支策略（建议）
- 新特性或大改动：基于 `main` 创建 feature 分支（例如 `feature/xxxxx`），完成后发起 PR 并关联 issue。
- 修复类提交：使用 `fix/xxx` 分支。
- 提交信息建议使用如下前缀格式：`feat(module): 描述`、`fix(module): 描述`。

代码风格与审查
- 遵守仓库当前命名与风格；不要在不必要的文件或高频路径增加日志输出。
- 变动涉及底层驱动或时序关键代码时，请在 PR 描述中说明风险与回退方案。

常见问题（FAQ）
--
Q: 构建时报错找不到交叉编译器怎么办？
A: 请安装或配置正确的交叉编译工具链（如 `arm-none-eabi-gcc`），或在构建脚本中调整工具链路径；也可在 WSL 下使用系统包管理器安装。

Q: OTA 下发失败且设备无响应。
A: 首先确认串口参数（波特率、数据位、校验位等）是否正确；检查供电与线路稳定性；查看 `debug_log/` 与 OTA 工具输出获取错误码并对照协议文档。

下一步建议
--
- 若需要更细粒度文档，我可以：
	- 为 `apps/` 下每个子模块生成模块级 README（包含依赖与编译示例）；
	- 添加一个 `CONTRIBUTING.md`，明确 PR 流程与代码审核要点；
	- 扫描仓库并生成一份依赖/工具链清单（可用于 CI 配置）。

变更记录
--
- 2026-03-26：扩展并重写 `README.md`，补充构建、OTA、调试、贡献等内容。

文件
--
- 本文件已更新： [README.md](README.md)

