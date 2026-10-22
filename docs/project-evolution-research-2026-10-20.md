# ThinCube 后续演进方向调研

调研日期：2026-10-20。源码基线：`a67e927`；固件 **v0.4.61**，Mac **0.4.35（56）**，协议 **v1**。本次只读源码、已有验收记录和官方资料，没有重新构建、安装、刷机或执行实机测试。下文的优先级、产品方向与验收门槛均为建议，不能视为已实现或已获用户确认的需求。

## 1. 建议的主线

**让 ThinCube 成为一个能感知工作场景、提供实体操作和轻量反馈的 Mac 桌面伙伴。** 设备负责姿态、采音、按键与角色表现；Mac 负责应用场景、工作流、识别与长期状态。

这条路线可以把现有的无线语音、A/B 与双敲、Clawd 连接起来。优先完成“拿起可用 → 操作目标明确 → 知道结果”的使用流程，再依据真实使用选择语音转写或专注陪伴分支。

| 方向 | 用户能得到什么 | 与现有能力的关系 | 建议顺序 |
|---|---|---|---|
| 日常可靠性与首次使用 | 唤醒后能用、声音能到目标应用、续航有实测依据 | 补齐已交付功能的体验验收 | 第一阶段 |
| 应用场景配置与动作反馈 | 同一组按键随应用使用，能看懂当前操作与执行状态 | 扩展现有快捷键和双向控制 | 第二阶段，主要产品增量 |
| Mac 本地语音转文字 | 说完得到可复制的文字或笔记 | 从无线麦克风扩展到完整输入流程 | 第三阶段的可选原型 |
| Clawd 工作状态与专注陪伴 | 把计时、工作进度、完成等状态变成角色反应 | 复用动画调度与场景动作 | 可在第二阶段做一个小场景 |
| 更新与分发能力 | 新设备易安装，升级有可信来源并能恢复 | 加强现有配对、A/B OTA 和签名安装 | 向其他用户分发前优先 |

排序假设：项目继续服务个人日常使用，暂未确定商业化、跨平台或多设备管理目标。若下一步是交给其他人试用，应将首次使用、更新与分发提前。

## 2. 当前项目的真实起点

### 已经具备，不应重新列作待做

- Clawd 日常表现、小剧场、环境音量互动和两种横屏玩法。
- Wi-Fi / BLE 互斥工作模式、屏幕 PIN、已保存授权、自动重连、模式切换事务及恢复出厂。
- 倒置语音、手动语音、Mac 音频桥；Wi-Fi 48 kHz PCM，BLE 16 kHz 采集与 ADPCM 传输。
- 对讲状态下 A/B 的按下保持与松开释放、双敲一次触发，以及设备端敲击参数持久保存。
- 五分钟在线息屏、纯电池十分钟深睡、二十分钟关机；睡前通知、Mac 释放按键/音频和恢复识别。
- A/B OTA、应用型号/长度/SHA-256 检查、设备上确认及启动健康确认/回滚代码。

依据：[使用说明](../README.md)、[音频采集](../firmware/src/audio_input.cpp)、[BLE 音频协议](../macos/Sources/ThinCubeCore/BLEProtocol.swift)、[敲击持久化验收](tap-persistence-v0.4.57.md)、[休眠策略](unified-sleep-v0.4.60.md)、[OTA 实现](../firmware/src/ota.cpp)。

### 最新证据改变了哪些判断

1. **自动建联的主要旧瓶颈已经优化。** Mac 0.4.32 的同机对照，自动模式中位数由 6.78 秒降至 1.90 秒，各十次成功；这是重启上位机后的特定样本，不能代表设备冷启动、深睡唤醒或第一帧声音。[实机对照与方法](automatic-connection-v0.4.32.md)
2. **省电策略已实现，续航仍缺完整测量。** v0.4.61 本地验收记录确认非朝下放置五分钟后息屏、麦克风与 codec 关闭、CPU 160 MHz、BLE 间隔 90 ms；同一记录明确 `pureBattery20MinuteCycleVerified=false`。因此下一步应测真实电池流程，而非继续增加休眠档位。[本地原始验收记录](../artifacts/online-standby-v0.4.61/final-verification.json)、[公开策略与验证边界](unified-sleep-v0.4.60.md)
3. **快捷操作仍与语音状态绑定。** Mac 的 `shortcutBlock` 要求允许语音、未静音、正在传音并处于倒置姿态；当前前台应用仅用于注入日志，没有按应用选择操作配置的机制。普通模式 B 静音、A 互动也已占有明确语义。[事件门槛与配置保存](../macos/Sources/ThinCube/DeviceModel.swift)、[注入实现](../macos/Sources/ThinCube/KeyInjector.swift)
4. **Flash 总量与应用可用空间不同。** 两个应用槽各为 2,097,152 字节；v0.4.61 镜像 1,803,744 字节，按镜像长度计算占约 86.01%，剩余 293,408 字节。最终可构建镜像还受段布局、对齐和签名开销影响。大功能必须先评估槽位预算；更改分区表按现有流程需要 USB，无法随普通 app OTA 一起完成。[分区表](../firmware/partitions.csv)、[当前镜像清单](../artifacts/online-standby-v0.4.61/firmware/firmware-manifest.json)

README 当前版本文字仍为 0.4.60 / 0.4.34；`implementation-status.md` 顶部更早停留在 0.4.14 / 0.4.7。本文以当前源码版本和最新对应验收记录为准。上述 `artifacts/` 文件只在本机保留，不是公开仓库的可复现证据；关键验证边界应逐步摘要到版本文档中。

## 3. 第一阶段：把日常可靠性变成可验证的体验

目前最缺的是完整使用流程的数据。已有测试与诊断基础可以继续复用，无需再建设一套独立监控系统。[本地回归脚本](../scripts/test-local.sh)、[连接计时](../macos/Sources/ThinCube/ConnectionDiagnostics.swift)、[电源诊断](../macos/Sources/ThinCube/PowerDiagnostics.swift)、[连接基准脚本](../scripts/benchmark-connections.py)

建议先建立三类实机验收：

| 验收 | 需要记录 | 为什么影响后续决策 |
|---|---|---|
| 纯电池完整周期 | 五分钟息屏、十分钟深睡、二十分钟关机；提前拿起；各阶段整板电流和典型续航 | 判断现策略是否已经够用，以及是否值得升级底层 SDK 做保连浅睡 |
| 音频端到端流程 | 动作确认、链路性能就绪、首个音频包、本地渲染、目标应用实际输入；丢帧和音质 | 区分“连上了”和“说话没有漏开头”；决定优化建联、缓冲还是音频准备 |
| 恢复与误操作 | Mac 睡眠/锁屏、离开范围、Wi-Fi 变化、权限关闭、双敲自然活动负样本 | 避免错误触发、卡键、旧事件补放，确认失败后的提示可理解 |

建议门槛：每条关键恢复路径连续做一组固定次数的测试并保留全部结果；音频至少覆盖长会话；双敲同时报告漏识别与自然活动下的误触发，不能只展示成功样本。具体延迟和续航目标在测得基线后确定，此处不承诺尚未验证的毫秒数或小时数。

同时，把首次使用收拢成可见的准备状态：设备已授权、所需姿态已校准、音频输出已准备、快捷操作权限已生效。让用户能直接定位“哪一步未就绪”，复用现有检查并提供对应恢复入口。[硬件检查](../macos/Sources/ThinCube/HardwareCheck.swift)、[现有帮助](../macos/Sources/ThinCube/HelpViews.swift)

## 4. 第二阶段：应用场景配置 + 有依据的反馈

这是建议优先投入的新功能。先选两个真实高频应用，为 A/B/双敲配置不同动作；保留全局默认和手动固定配置。示例可以是语音应用中的快捷操作，以及笔记场景的一键工作流，具体绑定由实际使用决定。

Apple 提供前台应用激活通知与 `NSRunningApplication`，可据应用身份选择配置；Shortcuts 支持命令行列出和执行快捷指令，并通过退出状态报告运行结果。技术入口已存在，但本项目尚未实现这层路由。[NSWorkspace 激活通知](https://developer.apple.com/documentation/appkit/nsworkspace/didactivateapplicationnotification)、[Shortcuts 命令行接口](https://support.apple.com/en-nz/guide/shortcuts-mac/apd455c82f02/mac)

建议首版范围：

- Mac 侧按稳定设备身份和应用 bundle ID 保存配置，迁移当前按设备名称保存的绑定时保留原值。
- 动作先支持现有键盘组合与用户选择的 Shortcuts；URL 动作可后续加入。无需首版建立任意脚本平台或插件市场。
- 第一轮继续沿用现有对讲姿态的触发条件，先验证配置路由本身。若实际需要“无需开麦也能控制”，再单独设计明确的操作模式，处理普通 A/B、静音、游戏和系统手势冲突。
- 设备只接收简短场景名与状态事件，复用现有角色区域显示，不向设备推送整个桌面画面。

关键边界：键盘组合保留按下/松开；Shortcuts 属于一次动作，不能把按住重复解释为多次启动。切应用或切配置时，已按下的键需要按原上下文正确释放，再接纳新操作。

反馈也应分级：设备事件已接收、系统键盘事件已提交、工作流执行结束，分别表示不同事实。现有 `CGEvent.post` 无法证明目标应用完成了工作；只有拿到可核实的应用结果时，才显示业务“成功”。[现有注入与日志边界](../macos/Sources/ThinCube/KeyInjector.swift)、[Apple Shortcuts 运行结果](https://support.apple.com/en-nz/guide/shortcuts-mac/apd455c82f02/mac)

首版验收重点：两种场景切换正确；未知应用回全局配置；切换时无卡键；重连不重放过期动作；执行中的一次动作不因重复事件被再次启动；失败反馈与真实结果一致。

## 5. 语音分支：Mac 本地转写值得做一个独立原型

当前语音流程是设备采音，经 Mac 与 BlackHole 送给其他应用识别。可新增另一条流程：设备采音 → Mac 解码 → 本地识别 → 文字预览/复制。它直接解决“说完得到什么”，也有机会让纯文字用途省去 BlackHole 准备步骤；通用麦克风输入仍继续使用现有音频桥。[当前 AudioBridge](../macos/Sources/ThinCube/AudioBridge.swift)

Apple 的 SpeechAnalyzer 可以接收音频流，SpeechTranscriber 进行设备端转写并管理所需语言模型。SpeechTranscriber 从 **macOS 26.0** 引入，而项目最低版本是 macOS 13；因此应作为带版本判断的可选后端，检测 `isAvailable`、支持语言和已安装模型，不能承诺所有现有用户都可使用。[Apple WWDC25 技术说明](https://developer.apple.com/videos/play/wwdc2025/277/)、[官方 API 平台元数据](https://developer.apple.com/tutorials/data/documentation/speech/speechtranscriber.json)、[项目最低系统版本](../macos/Package.swift)

若评估旧 `SFSpeechRecognizer`，要同时检查 `supportsOnDeviceRecognition` 并设置 `requiresOnDeviceRecognition`，不满足时不能声称离线；是否选择其他本地引擎，留到对真实中文、英文和混说样本的比较之后。[支持条件](https://developer.apple.com/documentation/speech/sfspeechrecognizer/supportsondevicerecognition)、[设备端识别请求](https://developer.apple.com/documentation/speech/sfspeechrecognitionrequest/requiresondevicerecognition)

原型只需验证：两条无线链路的真实音频是否可接受、首个文字和最终文字延迟、漏首字、中文/英文表现、断线取消与下一次会话隔离，以及离线时的实际行为。当前音频“就绪”与 BlackHole 路径有关，需要将音频接收能力和输出/识别后端拆开，才能真正实现不依赖 BlackHole 的文字模式。

不把“自研虚拟麦克风驱动”当作这个原型的前置。Apple 对虚拟音频设备指向 Audio Server Plug-in 路线；替代 BlackHole 是另一个需要持续维护与分发的工程，详见[一手资料补充](evolution-primary-sources-2026-10-20.md)。

## 6. Clawd 分支：先让角色回应一件真实工作

建议从一个专注计时或一项工作流状态开始：开始时进入专注表现、完成时短暂庆祝、失败时给出可理解的轻反馈。这样可以验证“角色表现是否让实体操作更有价值”，再决定是否增加历史、收藏或更长期的陪伴机制。

这是产品假设，尚无项目用户数据证明它能提高使用频率。首版只需记录本地场景使用次数与用户是否愿意继续使用；不必建设成长数值或复杂养成系统。

现有小剧场已经有低频预算、冷却和打断规则，应复用这些规则。工作状态与动画分开：状态可以持续，演出仍是有限时长。Mac 保存计时和任务真相；设备深睡时不承诺远程提醒，醒来后同步最新状态，避免补播一串过期通知。计时不能依赖让屏幕或麦克风一直工作。[小剧场策略](../firmware/src/clawd_story_policy.h)、[角色节奏](clawd-rhythm-v0.4.33.md)、[深睡恢复边界](unified-sleep-v0.4.60.md)

先接项目自己发起的任务，后续再按实际需求接一个外部工具。天气、日历、消息提醒和智能家居都可另作场景，但暂时没有证据表明它们比现有语音与按键主线更值得投入。

## 7. 架构和交付应怎样配合

### 围绕新能力逐步拆分

Mac 的 `DeviceModel.swift` 当前约 1,194 行，集中处理发现、配对、连接、音频就绪、事件注入、设置和升级。`ThinCubeCore` 已经承载可测试的纯规则，是可复用的分层基础。[当前 DeviceModel](../macos/Sources/ThinCube/DeviceModel.swift)、[Core 目录与目标](../macos/Package.swift)

加入场景配置时抽出动作路由与执行；加入本地转写时抽出音频接收/输出后端和语音会话；设备连接仍由统一会话管理。保留既有 generation、epoch、ack、睡前释放等约束，不为每种新功能另开一套连接。固件中 `device.cpp` 与 `ui.cpp` 也适合在新增具体能力时沿策略/渲染边界拆分，无需先做大规模重写。

协议继续使用可选能力与版本协商。场景反馈可定义带请求身份、会话身份及有效期的小事件；旧 Mac/旧固件忽略不认识的能力。不要通过一串越来越复杂的键盘模拟来假装取得了外部应用的状态。

### 交给其他用户之前补齐发布能力

当前 OTA 已有完整性检查和回滚调用，但 SHA-256 与型号检查不能独立证明发行者身份；Wi-Fi 控制与音频仍是局域网明文。现有本地 SDK 配置启用应用回滚，未启用 Secure Boot 或无 Secure Boot 的应用签名校验。这是当前构建配置，不表示以后必须烧写不可逆的硬件配置。[OTA 源码](../firmware/src/ota.cpp)、[局域网协议边界](../README.md)、[本机 SDK 配置](/Users/numbero/.platformio/packages/framework-arduinoespressif32/tools/sdk/esp32s3/sdkconfig)

建议独立交付更新包来源验证、版本兼容清单，以及升级中断/新固件不健康时的真实回滚测试；面向更广网络时再把控制与音频的机密性、重放防护纳入同一版本设计。Espressif 已提供 OTA 签名与回滚机制，具体与当前预编译 Arduino 栈的集成仍需原型验证，不能将官方能力直接等同于项目已启用。[ESP-IDF 4.4.7 OTA](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-reference/system/ota.html)、[无硬件 Secure Boot 的 OTA 签名检查](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/security/secure-boot-v2.html#signed-app-verification-without-hardware-secure-boot)

Mac 已有固定签名、安装和权限诊断基础；正式向外分发时再补 Developer ID、公证和更新策略。每次本机重新构建仍按仓库要求替换 `/Applications/ThinCube.app`，不只更新 artifacts。[构建安装要求](../AGENTS.md)、[签名与授权调查](keychain-update-research.md)、[Apple 公证说明](https://developer.apple.com/documentation/security/notarizing-macos-software-before-distribution)

## 8. 暂不优先投入的方向

| 候选 | 暂缓理由 | 什么证据出现后再做 |
|---|---|---|
| 持续堆新手势 | 已有按键、姿态和双敲；新动作增加学习、误触和状态冲突成本 | 有高频任务无法由现有操作自然完成 |
| 直接做脱离 Mac 的通用 AI 对话终端 | 需要新增识别、推理、网络、音频回传与会话体验；现有应用槽空间也有限 | Mac 侧已经证明一个语音场景有持续使用价值 |
| Wi-Fi/BLE 同时运行及无缝切换 | 当前互斥模式已有可靠事务；并行会增加共存、功耗和会话管理成本 | 实测表明模式切换频繁且等待是主要痛点 |
| 为浅睡立即升级整套 SDK | 预编译栈配置是限制之一，但整板收益仍未测；升级还影响音频、BLE、IMU和OTA | 电池数据表明保连待机已成为首要耗电项 |
| 多设备、多 Mac 争用、跨平台 | 当前模型主要管理单一活动会话，需要重做所有者与配置同步 | 有实际多主机或多设备需求 |
| 大量扩展游戏、硬件模块或自研音频驱动 | 会分散场景主线，也增加资源与维护成本 | 验证出明确独立用途，且愿意为它承担维护 |

## 9. 推荐的下一轮工作包

按依赖安排，不给未经估算的日历承诺：

1. **形成体验基线。** 整理当前版本和未验收范围，复用现有诊断完成电池周期、唤醒与首音频、目标应用输入和自然活动误触测试。成果是可复测的数据与问题排序。
2. **实现两个应用的场景配置原型。** 先保留当前触发语义，在 Mac 增加配置路由；再加一个可查询结束状态的 Shortcuts 动作和设备短反馈。成果是两条用户愿意每天使用的操作流程。
3. **根据使用结果选择一个扩展。** 文字输入是主要价值时做本地转写；工作状态陪伴更有价值时做一个专注/完成反馈场景。两条分支分别验收，避免同时扩成大型助手。

如果只选一个新增能力，推荐 **应用场景配置 + 执行反馈**。如果下一轮只解决一个现存体验问题，推荐先实测 **拿起设备到目标应用收到声音** 的完整过程。

外部技术可行性、版本边界与一手链接另见：[演进方向一手资料补充](evolution-primary-sources-2026-10-20.md)。
