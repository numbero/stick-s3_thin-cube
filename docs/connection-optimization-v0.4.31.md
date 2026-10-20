# 首批建联优化 · macOS 0.4.31（52）

2026-10-20，在 `a4f7442` 及工作区原有待机/深睡改动之上增量实施。未改变 Wi-Fi/BLE 互斥、配对方式、传输超时或重试退避。

## 实现

- **耗时埋点**：Mac 每次尝试以 UUID 和单调时钟记录发现、connect、GATT、订阅、凭据/用户配对、challenge/auth、有效基线、控制连接、音频桥和首帧；HTTP 另记录 DNS、TCP、请求、首字节等待和响应耗时。日志仅含枚举阶段、时长和固定 API 路径，不包含密钥、PIN、nonce/proof、地址、会话 cookie 或音频内容。后台写入 `~/Library/Application Support/ThinCube/connection.jsonl`，最多保留两个 2 MiB 文件；硬件检查快照包含当前尝试的里程碑。
- **初始 BLE 间隔**：固件从 CONNECT 事件读取实际 interval，仅接受 6…3200 单位的有效值。若实际已经不超过 30ms，就不再从未知状态额外请求快连接；慢间隔、无效值和空闲降速后的恢复仍要经过原来的请求/确认。每次新连接重置观测状态。
- **一次有效状态读取**：抽出可测试的 BLE 握手，`challenge → auth → status` 返回首个有效完整基线，直接用于建立上下文；不再在认证探针中读完丢弃、重置后再次请求。仍保留十次有界状态确认、首次配对、401/403 恢复、新 epoch/设备身份验证，以及取消后的代次检查。固件主循环在发送 auth 响应之前处理已入队的 connect 并发布新 epoch；没有更改协议或要求新固件能力。
- **音频与握手并行**：连接开始时异步准备音频。Core Audio 的 start/stop/输入设备操作全部由同一后台串行队列拥有，API 入队在 MainActor 上按序完成；取消/断线后排队清理，旧完成回调不能绑定 UDP、发布 ready 或清掉新任务。控制连接可以先完成，音频桥准备后由后续心跳通告可用状态。

## 里程碑口径

Mac 的 `control_connected` 表示收到并验证首个控制基线，`audio_bridge_ready` 仅表示本地 AudioUnit/必要 UDP 接收端就绪，`heartbeat_sent` 表示本机发送完成，不能冒充设备确认租约。固件的 `control_ready` 才是有效 epoch 心跳建立控制租约，`audio_ready` 同时检查 ready 和 BLE 性能条件。`first_audio` 为 Mac 解码/接收首帧；固件的 `first_audio_queued` 是发送入队/UDP 提交，均不代表目标应用已经录到声音。

`elapsedMs` 从本次连接尝试开始计时；`stepMs` 是相邻里程碑间隔，不是某个并行阶段的独占耗时。自动重试前的退避不计入本次尝试；配对提示和用户提交分别记点。两端时钟未对齐，不能直接相减；以场景/事件序列关联。

固件输出形如 `[connect-timing] ble #1 secured t=... dt=... v=0 lost=0`，毫秒计时支持回绕。BLE/GAP/Wi-Fi 回调只写入有界队列，主循环在串口缓冲有空间时输出完整行；USB 未连接时允许丢弃日志并累计 `lost`，不无限堆积。开机、Wi-Fi 无线重连和 BLE 连接有独立阶段；同一次 Wi-Fi 关联中的后续应用重连主要看 Mac 的新 attempt 日志。

## 验证与交付

- 新增 9 项 Swift 测试，覆盖单次状态请求、独立 HMAC 协议向量、非法 nonce、状态重试边界、取消中的握手、慢音频与控制并行、旧音频完成不影响新任务、失败后重试、里程碑去重/代次隔离。
- 固件测试覆盖实际初始快/慢间隔、非法间隔、慢转快确认、新连接重置，以及计时去重与时钟回绕。
- `scripts/test-local.sh`：44 个 C++ 测试程序、Python 5+3 项检查、旧版 IMU/SDK 检查、120 项 Swift 测试均通过。提供本次固件镜像，因此固件身份检查未跳过。既有 `face_down_rest_test.cpp` 有一处未使用常量警告，本次未改它。
- 最终 Swift 回归 120 项通过、零失败；PlatformIO `m5stack-sticks3` 目标构建成功。日志、实施前文件副本和固件镜像保存在 `artifacts/connection-optimization-20261020/`。
- 上位机使用 `scripts/build-macos.sh` 构建签名并安装到 `/Applications/ThinCube.app`，安装脚本备份旧应用并从系统应用目录重启；安装后的版本、签名、进程路径及二进制一致性另做校验。

本轮没有刷写实体设备。上位机优化兼容现有固件并在安装后生效；初始 BLE interval 复用和固件侧阶段日志需要部署新固件后才会在设备生效。本轮测试证明行为与构建正确，尚无足够实机 A/B 样本支持中位数/P95 改善幅度的结论。

安装后校验确认：`0.4.31 / 52`，运行进程路径为 `/Applications/ThinCube.app/Contents/MacOS/ThinCube`，固定签名验证通过，与 `artifacts/ThinCube.app` 完全一致。观察期间新日志已记录音频桥在连接等待期间独立就绪；设备控制连接尚未建立，不能把这次观察作为实机建联提速验收。校验摘要与脱敏里程碑见上述产物目录的 `verification.json` 和 `runtime-timing.json`。

## 后续实机验收

同日随后已将新固件以 `v0.4.59` 刷入设备，完成三组各十次固定 BLE 对照、短时传音和双向切换，再恢复自动模式验证。固定 BLE 建联中位数从 2.34 秒降到 1.27 秒；自动模式仍存在约五秒 Wi-Fi 超时后回退的等待。刷写保护校验、原始数据、适用范围与最终安装状态见[固件 v0.4.59 实机报告](connection-optimization-hardware-v0.4.59.md)。上文“未刷写”描述的是实施完成当时的状态。
