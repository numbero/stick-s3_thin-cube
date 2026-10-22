# ThinCube 后续演进：外部技术可行性核查

调研日期：2026-10-20。源码基线：`a67e927`。本文只调研，不改变代码、设备配置或已安装应用。文中“已实现”来自仓库与既有验收记录，“官方能力”来自一手资料，“建议”是结合两者作出的判断，不能当作已完成承诺。

## 结论

建议沿着“可信赖的桌面物理入口”继续演进：先补齐续航、音频和异常恢复的完整证据，再做应用场景配置及动作反馈。Mac 本地转写适合单独做原型；固件签名、通信保护和正式分发在开始给更多人使用时进入主线。当前不需要重做 BLE、自动重连、分级休眠或普通 OTA，这些已经存在。

| 方向 | 当前已有 | 有价值的下一步 | 技术判断 |
|---|---|---|---|
| 日常可靠性与续航 | 双通道、自动连接、5/10/20 分钟休眠、音频统计 | 真实电池与端到端音频基准、异常恢复验收 | 最应先收尾，减少后续功能建立在未知行为上 |
| 应用场景与动作反馈 | A/B 保持与双敲脉冲、系统按键注入 | 按应用切换配置、运行 Shortcuts、清楚反馈执行状态 | 主要落在 Mac，硬件与既有输入可以复用 |
| 可选本地转写 | Wi-Fi/BLE 收音并输出 BlackHole | 从 Mac 已解码 PCM 直接转写，先复制确认结果 | 有官方 API；系统、硬件、语言与模型可用性必须运行时判断 |
| 设备生命周期与发布信任 | A/B OTA、物理确认、SHA-256、健康确认 | 故障回滚实测、签名发布、设备验签、通信保护和正式 Mac 分发 | SDK 有基础能力，但配置、迁移和恢复不能省略 |

## 1. 先确认当前基线与资源上限

- 源码固件为 **v0.4.61**，Mac 为 **0.4.35（56）**，协议为 v1。README 的 v0.4.60 / 0.4.34 与 `implementation-status.md` 的 v0.4.14 / 0.4.7 均落后；`design-v1.md` 是早期设计，不能直接当作现状。来源：[固件配置](../firmware/platformio.ini)、[Mac 元数据](../macos/Info.plist)、[早期实施状态](implementation-status.md)。
- BLE 已经是项目能力。当前源码使用 ESP-IDF 4.4.7 / Arduino 2.0.17 的 Bluedroid GATT，而非 M5Unified 自带接口。当前配对是 LE Secure Connections / Just Works 自动加密，再叠加应用 PIN 与 nonce/HMAC 授权；不能沿用早期记录中的系统 passkey/MITM 说法。来源：[当前配对说明](pairing-v0.4.4.md)、[`ble_link.cpp`](../firmware/src/ble_link.cpp)。本地 StickS3 skill 中“不要把蓝牙算进设计”的提醒对应厂商产品页及旧开发基线，不能覆盖本项目已经接入并实测的能力。
- 固件分区为两个 **2 MiB** OTA 槽；本地 v0.4.61 manifest 的镜像为 **1,803,744 字节**，占单槽约 **86.01%**，剩余 **293,408 字节**。全板 8 MB Flash 不等于应用槽有 8 MB 空间。当前只 OTA 应用，修改分区表需要设计 USB 迁移。来源：[分区表](../firmware/partitions.csv)、本地产物 `artifacts/online-standby-v0.4.61/firmware/firmware-manifest.json`，后者不纳入 Git。
- Mac 最低系统仍是 macOS 13；设备端仍应承担低延迟输入、状态显示、采集和省电，较重的模型与应用集成优先放 Mac。这是基于现有分工和槽位余量的工程建议，并非硬件无法运行任何模型。来源：[Mac Package](../macos/Package.swift)、[设备音频实现](../firmware/src/device.cpp)。

## 2. 日常可靠性与续航：从“具备策略”走到“有实测承诺”

**已实现：** 自动模式已能在新 BLE 广播到来时提前结束无效 Wi-Fi 探测；相同节能状态下，单 Mac、单设备各十次样本的建联中位数由 6.78 秒降为 1.90 秒。该指标包含本地音频准备，但不是首帧声音或目标应用录音延迟。统一策略已为五分钟在线息屏、纯电池十分钟 PM1 L1 深睡、二十分钟 L0 关机；纯电池完整周期、电流与续航仍待完整实测。来源：[自动建联记录](automatic-connection-v0.4.32.md)、[统一休眠记录](unified-sleep-v0.4.60.md)。

**官方能力与约束：** M5Stack 明确支持保留 IMU 供电，通过 IMU 中断唤醒 PM1、重新为 ESP32-S3 上电并重新初始化。厂商低功耗模式支持当前方向，但官方样机电流不代表 ThinCube 镜像的实测电流。[M5Stack 低功耗配置](https://docs.m5stack.com/en/arduino/m5sticks3/m5pm1)

ESP-IDF 4.4.7 明确区分主动进入 Light/Deep-sleep 与维持连接的 modem-sleep/自动 Light-sleep：前者不保留 Wi-Fi/BT 连接。当前项目 PM1 L1 更是切断主控供电，唤醒后的重新连接属于预期路径。[ESP-IDF 4.4.7 Sleep Modes](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-reference/system/sleep_modes.html#wifi-bt-and-sleep-modes)

**建议增量：** 用同一测试记录串起“拿起 → 显示 → 连接可控 → 首帧音频 → 目标应用获得声音”，记录中位数、尾部延迟及失败；分别覆盖 Wi-Fi 与 BLE、普通息屏与 PM1 唤醒、Mac 锁屏/睡眠与网络改变。音频方面补长时间漂移、突发丢包和目标应用输入质量；电源方面测在线、息屏、L1、L0 电流，并完成纯电池二十分钟周期、提前拿起及 USB 插拔。上述是建议验收范围，不是本次新增测试或通过结论。

## 3. 应用场景与动作反馈：最适合下一轮的小产品迭代

**已实现：** `KeyInjector` 可按下、保持、释放与脉冲注入；它读取前台应用名称用于日志。当前没有据此前台应用切换配置的逻辑，也没有 Shortcuts 动作类型。来源：[`KeyInjector.swift`](../macos/Sources/ThinCube/KeyInjector.swift)、[`DeviceModel.swift`](../macos/Sources/ThinCube/DeviceModel.swift)。

**官方能力：** `NSWorkspace.didActivateApplicationNotification` 提供应用切换通知，其 `userInfo` 包含对应的 `NSRunningApplication`；应在 workspace 的 notification center 订阅。这足以支持基于应用身份的场景判断，不要求读取整个屏幕。[Apple NSWorkspace 通知](https://developer.apple.com/documentation/appkit/nsworkspace/didactivateapplicationnotification)

Apple 支持用 `shortcuts run` 启动快捷指令并传入内容；若快捷指令要求交互输入，命令行进程会停下来等待。因此它可作为低耦合的工作流入口，但需要显示等待、取消与失败状态。[Apple Shortcuts 命令行说明](https://support.apple.com/guide/shortcuts-mac/run-shortcuts-from-the-command-line-apd455c82f02/mac)

**建议增量：** 第一版只做既有快捷键与运行用户选定的 Shortcut；配置按应用保存，并保留默认项，打开应用或 URL 可作为后续扩展。先沿用现有 A/B/双敲及对讲场景，不同时扩张手势数量。用户可在不同应用得到不同的按键含义，Mac 显示当前配置，设备只显示简短模式与结果。

**实现边界：** 按住期间切换应用时，必须用按下时的配置释放原组合键，不能按新配置生成 key-up。按键事件成功提交只能说明“已提交”；Shortcut 退出成功也不必然证明目标业务完成。只有集成能观察到最终结果时，才让 Clawd 显示“完成”，否则显示“已发送”或“等待”。这部分属于协议与交互设计建议。Mac 上的场景选择可先实现；向设备反馈则需要小范围、可版本协商的协议增量。

## 4. 本地转写：有价值，但先独立验证使用场景

**已实现：** 当前语音流送往 Mac，再经 BlackHole 提供系统音频输入，识别由目标应用负责。Wi-Fi 使用 48 kHz PCM，BLE 使用 16 kHz 有损 IMA ADPCM，再在 Mac 转成共用音频格式。不能把两条链路的转写准确率视作天然相同。来源：[Wi-Fi/BLE 实现](wifi-ble-implementation.md)、[`device.cpp`](../firmware/src/device.cpp)、[`BLEConnection.swift`](../macos/Sources/ThinCube/BLEConnection.swift)。

**官方能力：** Apple SpeechAnalyzer 可接收应用提供的音频流；SpeechTranscriber 在设备端转写，语言模型通过 AssetInventory 管理和下载。模型首次下载仍需要网络。支持情况应检查 `isAvailable`、`supportedLocales` 与 `installedLocales`，不应静态承诺某一机器或全部语言都可用。[Apple WWDC25 SpeechAnalyzer](https://developer.apple.com/videos/play/wwdc2025/277/)、[SpeechTranscriber 文档](https://developer.apple.com/documentation/speech/speechtranscriber)

SpeechTranscriber 的官方 API 元数据标注 **macOS 26.0 起可用**，因此无法直接覆盖项目 macOS 13 的最低系统要求；需要编译工具链更新和系统可用性分支。官方材料没有给出足够完整的 Mac 型号白名单，本报告不把“所有 Apple Silicon 均支持”当作已核实结论。[Apple 官方 API 元数据](https://developer.apple.com/tutorials/data/documentation/speech/speechtranscriber.json)

旧系统可另行评估 SFSpeechRecognizer。若产品承诺本地识别，必须确认 `supportsOnDeviceRecognition` 为 true，并要求 `requiresOnDeviceRecognition`；前者为 false 时识别器需要网络。[Apple 本地识别支持检查](https://developer.apple.com/documentation/speech/sfspeechrecognizer/supportsondevicerecognition)、[Apple 本地识别请求](https://developer.apple.com/documentation/speech/sfspeechrecognitionrequest/requiresondevicerecognition)

**建议原型：** 直接从已解码 PCM 分支进入识别器，以“开始 → 说话 → 停止 → 预览/复制确认文本”验证真实使用价值，分别测中文、英文术语、短句和丢包场景。该路径可不经过 BlackHole，但输出的是文本，不能代替向会议或其他语音应用提供的通用麦克风输入。识别失败、模型未就绪和取消都要有明确反馈，先不要加入自动执行自然语言指令。

**为什么不优先自研虚拟麦克风：** Apple 将虚拟音频设备指向 Audio Server Driver Plug-in，并明确 AudioDriverKit 仅支持物理音频设备。前者示例需要安装 `.driver` 到 HAL 目录并重启。这说明技术可行，同时说明它是需要安装与生命周期维护的独立工程，而非给普通 Swift 应用补一个 AudioDriverKit API 即可替换 BlackHole。[Apple Audio Server Plug-in 示例](https://developer.apple.com/documentation/coreaudio/creating-an-audio-server-driver-plug-in)、[Apple AudioDriverKit 示例及限制](https://developer.apple.com/documentation/audiodriverkit/creating-an-audio-device-driver)

## 5. 生命周期与发布信任：已有 OTA 之后还缺什么

**已实现：** 设备验证镜像长度、型号标识、SHA-256，要求应用鉴权、设备物理确认以及电量已知且至少 50%；新镜像运行至少六十秒并满足健康条件后确认，超过九十秒仍未通过时走失败回滚。正常 OTA 已有多次真机成功记录，不能再表述为“OTA 从未验收”；故障注入与真实异常回滚仍应单列。来源：[`ota.cpp`](../firmware/src/ota.cpp)、[实施状态中 v0.4.5–v0.4.7 正常 OTA 实测记录](implementation-status.md)。

**官方能力：** A/B 应用分区、OTA 状态及回滚由 IDF 支持，但实际行为依赖 bootloader 开启 rollback 配置。IDF 4.4.7 还支持不启用硬件 Secure Boot 时验证签名 OTA。[ESP-IDF 4.4.7 OTA](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-reference/system/ota.html)

这条软验签路径要求初始运行应用也已签名，只使用运行镜像中的第一个签名块公钥验证后续应用；它不抵御有 Flash 写权限的攻击者。启用硬件 Secure Boot 则涉及 eFuse、密钥及调试/恢复路径变化，应与可逆的软件验证原型分开决策。当前 Arduino 预编译 SDK 配置是否能直接启用软验签仍需专门构建验证，不能把官方 IDF 选项等同于当前 PlatformIO 配置已可用。[ESP-IDF 4.4.7 Secure Boot v2](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/security/secure-boot-v2.html#signed-app-verification-without-hardware-secure-boot)

**建议顺序：** 先给版本包建立统一清单、兼容范围、签名与离线来源校验；随后实现设备端签名验证及初始可信镜像迁移，再提供自动检查版本。包的 SHA-256 只能检查与给定摘要是否一致，无法单独证明发布者身份。故障验收至少覆盖上传中断、错误签名、启动崩溃、健康失败以及配对/校准保留。不要将软件版本号的普通增长直接映射成每次都烧 eFuse 的防降级策略。

**通信保护是另一项工作：** 当前 Wi-Fi HTTP 与 UDP 音频仍为明文，BLE Just Works 的链路加密也不应宣传成强 MITM 认证。IDF 4.4.7 有 HTTPS server，但官方文档提示初次 TLS 握手可能约两秒，单个 SSL socket 约 40 KB RAM；这些是官方示例量级，不是本设备实测。应验证长连接复用、内存与唤醒延迟，并单独设计设备身份信任；把控制/OTA 改为 HTTPS 不会自动加密现有 UDP 音频。[ESP-IDF 4.4.7 HTTPS server](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-reference/protocols/esp_https_server.html)

**Mac 分发：** 项目已有固定 Apple 签名身份和系统应用安装脚本；旧状态文档中“ad-hoc 构建”不代表当前脚本。若要公开提供下载，需要正式梳理 Developer ID、Hardened Runtime、公证与 stapling，而不是把“固定签名”视作“已公证”。Apple 要求面向公证的适当签名身份与 Hardened Runtime，并提供 notarytool/stapler 工作流。[项目签名脚本](../scripts/setup-macos-signing.sh)、[Apple 公证要求](https://developer.apple.com/documentation/security/notarizing-macos-software-before-distribution)、[Apple Developer ID 说明](https://developer.apple.com/developer-id/)

## 6. 暂缓的扩张及理由

- **设备端通用语音识别/自由对话：** ESP-SR 官方 MultiNet 是有限命令词识别，要求 16 kHz、16-bit、单声道 AFE 输入，并不等于通用听写。官方基准中的 MultiNet7 还需约 2.9 MB PSRAM，模型有单独分区方案；当前槽位余量与缺少模型分区使集成成为工具链/分区专项工作。先把设备当高质量采集入口更符合现状。[Espressif MultiNet](https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/speech_command_recognition/README.html)、[ESP-SR 基准](https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/benchmark/README.html)
- **常开唤醒词：** 可研究，但与当前电池十分钟切断主控的策略有直接冲突；需要先定义 USB 桌面模式与电池模式的行为，再实测持续采集和推理耗电。不能同时承诺深睡断电和设备端持续听音。
- **多设备同时工作、跨 Mac 抢占或云端接管：** 当前是选择一个目标设备的会话模型。扩展前先解决控制权、按键释放、音频路由和配对身份，而不是只在列表允许多选。没有具体多人/多工位需求证据时，优先级低于单设备的确定性。来源：[`DeviceModel.swift`](../macos/Sources/ThinCube/DeviceModel.swift)。这是产品与工程优先级判断，不是系统 API 不支持多设备的断言。
- **不断增加动画和小游戏：** 可以保持小规模内容更新，但核心输入、录音、睡眠与升级的保护边界继续优先。下一步若给 Clawd 增加“等待/已发送/完成/失败”的真实工作反馈，比继续扩张系统外设更直接连接现有产品价值；这是本次调研的设计建议。

## 调研边界

本次未重新构建、烧录、操作设备、采集音频或测量电流；未验证外部分发账号、证书资格和 Speech API 在当前 Mac 上的运行结果。外部资料只使用 Apple、Espressif 和 M5Stack 官方文档/源码入口。官方 API 可用性与官方开发板基准只证明路线具备研究基础，不替代 ThinCube 的构建、迁移和真机验收。
