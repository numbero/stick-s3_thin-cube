# ThinCube v0.4.0 Wi-Fi / BLE 实现与验收

日期：2026-10-09。实现、构建、烧录及核心功能短测已完成；上位机与实体 B 键均已实测双向切换，短按静音保留。三分钟语音暴露的丢帧问题修复后，最终一分钟复测达到本轮质量目标。未执行长时间压力或端到端时延测试，具体范围见下文。本文覆盖早期调研中的候选方案。

## 使用方式

- 同一固件每次启动只启用 Wi-Fi 或 BLE。两种模式完整提供语音、A 键快捷键、设置、心跳和姿态校准。
- Mac 的“设备连接”页显示当前方式，点击“切换至 BLE / Wi-Fi”即可切换。旧固件继续走 Wi-Fi v1 协议，不显示切换按钮。
- B 短按保留静音功能；单独按住 B 至少 3 秒，屏幕提示后松手切换。
- 空闲时同时按住 A+B 至少 3 秒，松开两键后重新配对，撤销应用密钥与 BLE 绑定，不改变当前模式。组合手势优先，并消费 A/B 的单键动作；重叠不足 3 秒不执行动作。
- 配网、OTA 或切换期间禁用上述运行时手势。开机按住 B 始终恢复 Wi-Fi 配网。
- BLE 第一次连接需要在 macOS 系统蓝牙提示中输入设备屏幕上的六位码。应用首次配对另外在两分钟配对窗口内验证 PIN。普通模式切换保留已有应用配对。
- OTA 和修改 Wi-Fi 配置继续使用 Wi-Fi；BLE 模式先切回 Wi-Fi。

## 切换事务

主循环的 `requestTransportSwitch(target, source, requestId)` 是实体按键和上位机的共同入口。受理前检查网络配置、NVS、OTA、其他切换及 OTA 启动健康确认；OTA 与切换使用共同受理锁。

`transport_switch` 控制请求带 `epoch`、`target`（`wifi` / `ble`）、`requestId`。设备将原模式、目标、请求 ID、来源和阶段作为一个 NVS blob 写入，停止采集、音效与旧会话，公布 `pending`，700 ms 后重启。切换请求重复发送保持幂等。

目标启动先持久化 `trial`，只启动目标服务。60 秒内必须完成应用身份认证、有效状态交换，并由 Mac 发送匹配的 `transport_commit`；设备确认后持久化 `committed`。未收到确认或试连接期间重新启动，恢复原模式并记录 `rolled_back`。正常模式中的普通断线只触发当前模式重连，不改变设备模式。

Mac 通过原有稳定名称 `ThinCube-XXXXXX`、相同钥匙串密钥和认证后的 `deviceId` 统一两种发现结果及快捷键设置。切换会释放快捷键、清空音频与会话；恢复后需要新姿态动作或手动开始。主动断开、锁定、休眠、退出会停止连接尝试；解锁/唤醒只在连接意图仍有效时恢复。Mac 可在原连接丢失后查找另一种通道，认证后再读取切换状态。

## BLE 协议

使用当前 Arduino 2.0.17 / IDF 4.4.7 的原生 Bluedroid GATT，没有 SDK 升级或外部 BLE 依赖。

| 项目 | 值 |
|---|---|
| Service | `8D7A1000-6D43-4F3B-9AC8-4C7E62A8B101` |
| Control | `8D7A1001-6D43-4F3B-9AC8-4C7E62A8B101`：有响应写入、通知 |
| Audio | `8D7A1002-6D43-4F3B-9AC8-4C7E62A8B101`：通知 |
| 安全 | LE Secure Connections、MITM、绑定；业务特征与 CCC 写入要求加密 |
| 应用认证 | 一次性随机 nonce，十秒有效；HMAC-SHA256（二进制 128 位应用 key，UTF-8 `ThinCube-BLE-v1\|name\|nonce`），64 位小写 hex proof |
| 控制 | JSON；`info`、`pair`、`challenge`、`auth`、`status`、`settings`、`heartbeat`、原有 start/stop/校准/音效操作及 transport_switch/commit |
| 请求响应 | 请求 `rid` 原样返回；错误为 `error`；异步状态不带 rid |
| 音频 | 16 kHz / mono / IMA ADPCM；每帧 160 样本（10 ms），解码为 signed 16-bit PCM |

两个特征使用同一分片包头：ASCII `TC`（2 字节）、大端 uint16 消息 ID、偏移、总长，共 8 字节。每片总长不超过 ATT MTU−3；Mac 的控制写入按消息串行、有响应流控，不交错分片。控制请求最多 768 字节，响应最多 2048 字节；重组有长度、超时和数量上限。

音频帧 `TCB3` + 16 字节随机会话 + 大端 uint32 sequence + 大端 uint32 sampleClock + 两个 84 字节 IMA 块，共 196 字节（20 ms）。sequence 是第一个 10 ms 块的序号，第二块为 sequence+1（按 uint32 回绕）；Mac 解码为两帧 480 样本。IMA 块含第一个样本作为 little-endian int16 predictor、uint8 step index（0…88）、保留字节 0，以及 159 个低 nibble 优先的 4-bit 码；末字节未使用的高 nibble 必须为 0。每块独立解码，丢帧不影响后续块；这是有损压缩。Mac 仍兼容早期 112 字节 `TCB2` 单块 ADPCM 和 348 字节 `TCB1` PCM；sampleClock = sequence × 160（按 uint32 回绕）。MTU 至少 185 才启用语音，较小 MTU 保留控制并提示切回 Wi-Fi。设备控制响应优先、周期状态合并，音频最多排队六个 20 ms 包，120 ms 过期丢弃，以覆盖 Mac 实测 30 ms 连接间隔中的短暂停顿。大块 BLE 队列位于 PSRAM；BT 回调只更新连接状态或入队，业务与 NVS 写入留在主循环。

Mac 检查会话、长度和样本时钟，使用 AVAudioConverter 转为 480 样本 / 48 kHz 的标准帧，与 Wi-Fi 共用现有 JitterBuffer 和 BlackHole。重复或旧序号不进入有状态转换器；断片或序号缺失由播放缓冲补零。Wi-Fi 的 988 字节 `TCM1` 格式保持兼容。

BLE 每 200 ms 发送精简状态（关键标志或按键边沿变化即时发送） `{"s":[epoch,flags,session,level,pose,voiceBlock,bleAudioDropped],"edge":对象或null}`。flags 的 bit 0…7 依次为 allowed、muted、localSound、streaming、manual、held、micReady、inverted。Mac 仅接受与本次完整快照 epoch 相同的精简状态，重新连接清除基准；edge=null 明确清除旧边沿。空闲时完整快照每五秒同步，设置/校准/切换阶段变化即时同步；传音时不周期重复完整快照，请求响应优先、精简状态与完整快照分别合并。无 rid 的成功 heartbeat 使用 ATT 写入响应，不再重复发送应用确认。安全连接后协商 251 字节链路层数据长度，ATT 通知负载上限 244 字节；待 BTC 完成的通知最多两条，控制器拥塞时暂停提交。

状态新增 `deviceId`、`transport`、`transportSwitch`、`audioRate`、`audioCodec`、`transportAudioReady`、`transportTransaction {requestId,source,target,previous,phase}`、`transportError`、`rejectedRequest`。BLE 另有 `bleMtu`、`bleAudioDropped`。五秒串口诊断报告模式、阶段、内部堆/最大块/最小余量、DMA 和 loop 栈水位，不打印应用 key 或认证 proof。

## 本机验证与交付

- `bash scripts/test-local.sh`：所有原有固件策略测试、新切换/手势/分片测试，以及 Swift 全套测试。
- `pio run -d firmware`：只构建；应用槽仍为 2 MiB。
- `bash scripts/build-macos.sh`：release 构建、签名、安装到 `/Applications/ThinCube.app`，保留上次安装备份。
- `python3 scripts/package-firmware.py`：生成 `artifacts/thincube-v0.4.0.bin` 和 SHA-256 manifest；不会烧录。

源码修改前快照已归档：`artifacts/pre-wifi-ble-v0.4.0-source-20261009.tar.gz`。当前目录没有 Git 元数据，未创建提交。现有历史固件与备份保留。

### 本次验证结果

2026-10-09：10 个 C++ 测试程序通过，Swift 58 项检查通过。最终全套运行 57 项、真实镜像项因未传文件先跳过；随后单独传入最终 v0.4.1 文件执行镜像项，通过且无跳过，记录见 `artifacts/wifi-ble-final-tests-20261009.log` 和 `wifi-ble-final-firmware-test-20261009.log`。当前修复固件 v0.4.1 文件 1,668,592 B，重新构建和镜像标识检查通过，保留 2 MiB A/B 应用槽。这些是构建数值，运行时验证范围见实机记录。

Mac release 构建、项目固定 Apple 签名、系统安装与逐文件比较通过；`/Applications/ThinCube.app` 和分发产物的可执行文件 SHA-256 相同，版本 0.4.0。应用启动界面显示新手势说明，已在用户授权下进行 USB 和上位机实机操作。

当前日志：`artifacts/wifi-ble-paced-tests-20261009.log`、`wifi-ble-buffer-2m-build-20261009.log`、`wifi-ble-buffer-2m-flash-20261009.log`、`wifi-ble-paced-macos-build-20261009.log`；早期构建日志保留用于追溯。Mac 发布包：`artifacts/ThinCube-v0.4.0-mac-arm64.zip`；当前固件修复候选：`artifacts/thincube-v0.4.1.bin`，SHA-256 见 `firmware-manifest.json`。应用镜像不能替代整片 Flash 备份。

已进行源码复核，修正 OTA / 切换受理竞争、未确认连接的按键注入、预期 BLE 断开回调、BLE 初始化失败保护、系统配对码显示优先级及 USB 截图泄露配对码的路径。当前无 Git 仓库，未提交。USB 烧录与实机结果见下节。

## 实机调试记录

2026-10-09：烧录前已读取并校验完整 8 MiB Flash 备份，位于 `firmware-backup/pre-wifi-ble-v0.4.0-20261009-8MB.bin`；其中包含配对与 Wi-Fi 私有信息，不作为公开分发物。USB 写入 v0.4.0 及镜像校验通过，NVS 未擦除；Wi-Fi 恢复、原应用配对、两项姿态校准和亮度 134 均确认保留。

Mac 首次切换至 BLE、系统六位码配对、Secure Connections / MITM 应用认证与切换提交已成功，MTU 517。BLE 开始持续语音后曾出现大量补零和连接中断，不能据此判定音频可用。正在对照测试分片、发送流控及 Mac 蓝牙耳机环境；出现系统绑定不一致时由用户重建 BLE 绑定，应用密钥与设置保留。

调试新增只记录元数据的可选 `--hardware-check` 入口，实际调用生产上位机的认证、音频桥和切换流程；不会合成实体按键或记录 PCM、配对码、密钥与会话 cookie。USB 诊断日志存于 `artifacts/wifi-ble-*-device-20261009.log`；逐秒统计存于 `artifacts/wifi-ble-*.jsonl`。标记 `failed` 的运行属于失败或被系统确认中断的测试，不计入验收成功次数。

主循环中仅由主线程使用的大块 JSON 临时缓冲改为静态存储，栈水位从不足 1 KiB 恢复至约 4 KiB；BLE 主机提交中的通知限制为最多两条。早期仅调整数据长度或控制器可发送计数没有通过检查，撤回了计数门控。当前候选同时精简高频状态、移除重复 heartbeat 应用确认并压缩音频，重新启用链路数据长度协商，并按数据包直接保留控制器余量，等待实机结果。

固件 IMA 编码测试帧与 Mac、FFmpeg 独立解码结果逐样本一致（160 样本，最大差值 0）；合成正弦测试信噪比约 25.21 dB。这只验证编解码兼容性，不代表真实语音听感或 BLE 稳定性。记录见 `artifacts/wifi-ble-adpcm-codec-check-20261009.json`。

最新候选：Mac 0.4.0 build 21 已按固定 Apple 签名安装并刷新分发包，固件减小周期控制通知负载。普通 PHY 偏好请求不会保证改变实际速率，必须核对完成事件；加入默认 1M 偏好后曾实际协商到 1M，但一分钟语音吞吐明显下降，已撤回，当前请求默认及连接 2M 并保留有界缓冲。不同实验的控制负载、队列与 PHY 组合不同，不能把早期单项结果归因于 PHY 一项。USB 复位曾造成 I²C 外设识别失败，启动时加入有界总线恢复并设置正确板型，最新启动已观察到 LCD 135×240、IMU 与麦克风健康。

加密失败时，当前 Bluedroid SDK 可能删除设备端绑定而 Mac 保留旧绑定，导致需要用户忽略设备并重新进行系统配对；这是 [Espressif 已记录的问题](https://github.com/espressif/esp-idf/issues/6766)。调试烧录前先退出上位机，完成烧录及 USB 监视启动后再连接，以免复位中断加密过程。最新实机曾在目标 BLE 未认证时按 60 秒期限回退 Wi-Fi，原应用密钥、校准和设置保留。

按用户最新要求，本轮验收缩短为三分钟连续语音和两个方向各两次上位机切换，取消三十分钟语音及一百次切换压力测试。用户自行保持 Mac 唤醒，未修改系统锁屏设置。

三分钟短测已完成：BLE 保持连接，收到 17,158 个十毫秒音频帧，播放补零 1,752 帧（按接收帧加补零帧计算约 9.26%），音频质量未通过。随后四次上位机切换全部提交成功：BLE→Wi-Fi 3.24 / 3.46 秒，Wi-Fi→BLE 5.66 / 6.59 秒；每次两项校准与亮度 134 保留，未要求重新配对。记录见 `artifacts/wifi-ble-short-acceptance-20261009.jsonl` 和 `wifi-ble-short-results-20261009.json`。`complete` 只表示诊断流程完成，不代表音频质量达标，需另行核对统计。

最终修复复测：实际 PHY tx/rx 均为 2M，数据长度 251，MTU 517。连续语音请求时长一分钟，按本次 `audio_begin` / `audio_end` 累计统计差值收到 6,407 帧，播放补零 1 帧，占比约 0.0156%，设备发送队列丢帧 0，全程保持连接与传音，达到本轮 <0.5% 目标。通知提交失败与拥塞计数均为 0，内部堆最小余量约 34 KiB，loop 栈水位约 4 KiB。记录见 `artifacts/wifi-ble-buffer-2m-audio-20261009.jsonl`、`wifi-ble-buffer-2m-results-20261009.json` 和 `wifi-ble-buffer-2m-device-20261009.log`。最终修复没有再跑三分钟或压力测试，不能据此声称长时间稳定性或端到端时延已验证。

用户现场依次执行 B 短按开启静音、再次短按恢复、长按三秒后松手切 Wi-Fi，再长按切回 BLE，并确认“一切正常”。USB 记录显示两次模式启动均健康，BLE 再次安全认证通过；最终认证状态快照为 `transport=ble`、`phase=committed`、按键来源 `button`、`muted=false`、两项校准有效、亮度 134、无活动快捷键。见 `artifacts/wifi-ble-final-state-20261009.jsonl`。A+B 撤销配对、OTA、全部断电与异常组合未在本轮重复实测。

## 实机验收清单（按完成情况逐项更新）

1. 记录初始配对、校准、静音、亮度和快捷键；烧录后确认这些设置保留。先检查 Wi-Fi 的音频、按键、音效、姿态、电池显示和 OTA 启动健康确认。
2. 首次 Wi-Fi → BLE 验证系统 PIN、应用认证、MTU、语音、快捷键和设置。再由 Mac 切回 Wi-Fi。
3. 上位机两个方向各切换两次，并验证单独长按 B。确认只启用一种无线服务、无重启循环，没有旧音频和卡住的快捷键；不执行一百次压力测试。
4. B 的短按、2999/3000 ms 松手、长按不松手；A/B 两种按下顺序、不同松开顺序、重叠不足 3 秒、组合手势不额外静音/切换/扫描。重新配对保留模式并撤销旧密钥和 BLE 绑定；Wi-Fi 模式的 BLE 解绑在下次 BLE 启动执行。
5. 关闭 Mac 蓝牙、拒绝授权、移走 Mac、错误 Wi-Fi、目标试连接期间重启/断电、确认丢失：60 秒未确认自动恢复原模式，无循环。正常连接断开仅重连当前模式。
6. Mac 锁定/睡眠/主动断开/退出时释放按键和音频，停止提交试连接；恢复后不自动开始语音。
7. BLE 连续语音三分钟：检查断线、重启、音频补零及队列增长，记录 MTU、内存与栈水位。补零占比 <0.5% 为质量目标；P95 端到端时延尚未测量，不能以传输统计替代。
8. Wi-Fi OTA、旧 v1 固件连接兼容与升级失败回滚回归。未通过语音/稳定性门槛前，不将 BLE 标记为实机验证通过。
