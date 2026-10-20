# 统一休眠策略：固件 v0.4.60 / Mac v0.4.33

2026-10-20。此版本将先前可选深睡与普通待机合并为唯一的自动策略，不增加浅睡。

| 从最后一次真实活动起 | 行为 | 恢复方式 |
|---|---|---|
| 5 分钟 | 在线息屏，保持当前连接 | 按键、动作或 Mac 语音命令恢复 |
| 10 分钟 | 纯电池供电下进入 PM1 L1 深睡，主控断电，保留运动唤醒 | 拿起或按电源键，重新启动并重连 |
| 20 分钟 | 解除运动唤醒与 IMU 保电，转入 L0 关机 | 电源键重新开机 |

三个期限共用最后活动时刻，不是逐段额外等待。用户活动或提前唤醒后重新计时；心跳、睡眠确认、角色动画和环境声音不延长期限。朝下放置的快捷息屏继续保留，但不会把十分钟深睡或二十分钟关机提前。USB 供电可息屏，但不自动深睡或关机。

语音、音效、游戏、配对、配网、OTA、连接切换、旋转、诊断及组合手势沿用原有保护。正在受保护的任务或失效运动输入会阻止自动休眠，任务结束且输入恢复后重新获得完整窗口；供电未知时不切断电源。深睡准备不可用、失败或未收到 Mac 确认时，不再恢复旧十分钟关机分支：检查条件允许时维持在线待机，到二十分钟走正常关机路径。

## 统一入口与兼容

- 固件移除启用/禁用状态与不同关机期限，`IdlePowerPolicy` 只保留 5、10、20 分钟三个门限。
- 不读取或写入旧 NVS `deep`；旧值无论 true/false 均不影响新策略。保留旧数据不需要清空配对、校准或其他配置。
- HTTP / BLE `settings` 的旧 `deepSleep` 字段作为已退役字段忽略。完整状态固定提供 `deepSleep:true`、`deepSleepStaged:true`，新增 `deepSleepUnified:true`；`deepSleepAvailable` 继续表示硬件准备结果。
- Mac 移除深睡 Toggle、写设置的方法及异步确认任务，只显示“自动休眠”的说明。旧固件按实际能力和旧开关状态显示说明并提示升级，避免在设备尚未升级时承诺新策略。
- 睡前通知、释放快捷键与音频、同 epoch 确认、重连与失败回退沿用现有实现。硬件检查入口改为只读 `checkSleepPolicy`，不再切换睡眠设置。

## 主控断电后的二十分钟关机

延用 [v0.4.58 的 PM1 定时路径](idle-stages-v0.4.58.md)：睡前按原二十分钟期限的剩余时间配置 PM1 独立定时器。到期短暂启动主控，在 UI / 无线服务初始化前解除 IMU 保电与运动唤醒，再断电。因此实际关机含秒级取整与启动开销，不是毫秒精确。拿起、电源键、USB 或混合醒因优先继续正常启动，并取消旧关机期限。

## 验证

验证记录与交付产物位于本机 `artifacts/unified-sleep-v0.4.60/`。

- `scripts/test-local.sh` 通过：44 个 C++ 回归程序、8 项 Python 测试、Legacy IMU / Bosch SDK 检查；Swift 共 131 项，初跑仅跳过可选真实镜像项，随后对最终 v0.4.60 镜像单独补跑该项通过。
- 回归覆盖唯一的 5/10/20 分钟门限、活动/受保护任务重计时、USB、朝下息屏与动画不延长期限、时钟回绕、PM1 延迟入睡后的原期限、提前唤醒与 I2C 故障。Mac 验证统一策略字段随同 epoch 快速状态保留，切换到旧固件基线后不继承。
- 默认 StickS3 固件构建与实际镜像身份预检通过，产物为 `firmware/thincube-v0.4.60.bin`。
- Mac v0.4.33（54）已通过 `scripts/build-macos.sh` 构建、签名并替换 `/Applications/ThinCube.app`，安装脚本确认与产物一致，并从系统应用位置重启。
- 更新设备前的只读检查确认旧固件 `deepSleep:false`、`deepSleepAvailable:true`，用于检查旧关闭设置不再影响自动策略。
- 使用项目固定 esptool 4.9 完成设备完整 8 MiB Flash 备份，仅向原活动 app0（`0x10000`）写入 v0.4.60。写后确认应用镜像一致，bootloader、分区表、NVS、OTA 元数据逐字节未变，备用槽及活动应用分区外摘要未变。原配对和两个姿态校准均继续有效。
- USB 日志确认运行 `clawd v0.4.60`，初始化 `deep-sleep available 1`，统一门限为 `screen 300s / deep-sleep 600s / power-off 1200s`。持续运行至 65 秒，未见 panic/assert，音频控制健康，电源采样失败为零，USB 下 `auto_off 0 source 1`。
- Mac 更新后两次只读策略检查均通过：`deepSleep:true`、`deepSleepStaged:true`、`deepSleepUnified:true`、`deepSleepAvailable:true`，BLE 连接及校准状态正常；证明旧关闭设置已不影响新固件。最终已安装可执行文件再次与构建产物核对一致。串口监视器已退出并释放端口。

关键记录：`final-verification.json`、`flash-verification.json`、`before-firmware-mac.jsonl`、`after-firmware-mac.jsonl`、`final-health-mac.jsonl`、`after-firmware-serial.log`。完整 Flash 备份仅保留在本地产物目录。

| 产物 | 大小 | SHA-256 |
|---|---:|---|
| 更新前完整 Flash | 8388608 字节 | `5bcebe49efd689caa4a4db9a70482b95bed602d039d371da2f4389cf7629910b` |
| v0.4.60 应用镜像 | 1803712 字节 | `c8bebb632aac0782e9553fa9a551a7b9019a65af32a5cb820f81cb0ed64c259a` |

纯电池下完整二十分钟流程、实际拿起唤醒、各阶段电流和续航仍需实机验收；CPU 时间模拟与 USB 运行检查不能代替这些测量。

## 上位机 v0.4.34 界面收敛

设备设置区移除“运行时常亮（调试）”开关及说明、“自动休眠”说明区，仅保留亮度；同时删除已无入口的指示灯设置方法和异步确认任务。休眠帮助、预期断连识别与自动重连继续保留，设备固件仍为 v0.4.60。

v0.4.34（55）已构建、签名并安装到 `/Applications/ThinCube.app`，可执行文件与构建产物一致。实机界面确认设备设置仅含亮度，两个区域均已移除，设备仍通过蓝牙连接。记录位于本机 `artifacts/device-settings-v0.4.34/`。
