# 普通模式 B 单击静音：作用、场景与频率判断

日期：2026-10-18。依据当前工作区固件、macOS 源码和仓库既有验收记录。工作区含其他未提交改动，本文引用当前文件行号；源码行为不等于本轮重新实机验收。本次仅新增本文，没有改功能代码、构建、安装、烧录或访问设备。

## 结论

**B 单击是 ThinCube 设备麦克风的持续静音开关。它最成立的场景是临时隐私保护、暂时关闭常态声音互动；确实偏向低频状态切换。** 当前没有真实使用次数统计，低频是从入口、模式边界和恢复成本推导的产品判断，不能写成用户行为测量结果。

这不是 Mac 媒体音量静音，也不是会议软件的静音状态切换。对讲机内 B 已被自定义快捷键接管；“说话中频繁切换静音”不是普通模式 B 的主要用途。[设备切换实现:545](../firmware/src/device.cpp#L545)、[双键上下文:550](../firmware/src/device.cpp#L550)、[按键路由:15](../firmware/src/button_gestures.h#L15)

## 当前真实行为

| 项目 | 当前实现与一手依据 |
| --- | --- |
| 按键动作 | 普通 B 在松开且被识别为 click 时返回 `Mute`；单独持有约 3 秒后松开是连接方式切换，A+B 是独立组合操作。对讲机拥有的整次 B 按压不会在退出松手时泄漏成静音。[button_gestures.h:10–33](../firmware/src/button_gestures.h#L10) |
| 静音对象 | `voice.muted` 取反；变为 true 时 `stopVoice()` 清除持续/手动会话、待启动动作及快捷键保持。设备和 Mac 的同名“麦克风静音”入口写同一个 `muted` 字段。[device.cpp:93](../firmware/src/device.cpp#L93)、[device.cpp:545](../firmware/src/device.cpp#L545)、[voice_state.h:22](../firmware/src/voice_state.h#L22)、[ThinCubeApp.swift:392](../macos/Sources/ThinCube/ThinCubeApp.swift#L392)、[device.cpp:608–615](../firmware/src/device.cpp#L608) |
| 停止真实采集 | `captureWanted()` 首先要求未静音；主循环调用 `audioTick(false)`，执行 `M5.Mic.end()`、清采集队列和电平。是软件控制设备停止采集，不能宣称有独立硬件断路开关。[voice_state.h:18–20](../firmware/src/voice_state.h#L18)、[device.cpp:685–686](../firmware/src/device.cpp#L685)、[audio_input.cpp:37–44](../firmware/src/audio_input.cpp#L37) |
| 停止环境声互动 | `deviceLocalSound()` 返回 `localSound && !muted`；声纹、随音量注视/动作及突发声反射没有新采样后停止响应。用户保存的 `localSound` 设置不被改写，取消静音后按原设置恢复。[device.cpp:552](../firmware/src/device.cpp#L552)、[ui.cpp:807–816](../firmware/src/ui.cpp#L807)、[clawd_sound_meter.h:62–75](../firmware/src/clawd_sound_meter.h#L62) |
| 停止网络音频 | `voice.canStream()` 要求未静音，传音停止；Mac 收到 `muted`/非传音状态后清空音频 session，释放快捷键，不保留静音前会话。[voice_state.h:15–22](../firmware/src/voice_state.h#L15)、[device.cpp:712–714](../firmware/src/device.cpp#L712)、[DeviceModel.swift:669–707](../macos/Sources/ThinCube/DeviceModel.swift#L669) |
| 离线也可用 | 普通模式 B 静音路由不要求控制连接，直接修改设备本地状态。本地环境采音同样不要求 Mac 在线；“允许语音输入”关闭也不等于环境麦克风关闭。[ui.cpp:628–639](../firmware/src/ui.cpp#L628)、[voice_state.h:6–20](../firmware/src/voice_state.h#L6) |
| 持久保存 | 每次切换调用 `persist()`，将 `muted` 写入 NVS；开机读取，默认 false。没有连接或重启自动取消静音的路径；按键路径没有向用户呈现 NVS 写失败。[device.cpp:134–141](../firmware/src/device.cpp#L134)、[device.cpp:479](../firmware/src/device.cpp#L479)、[device.cpp:545](../firmware/src/device.cpp#L545) |
| 可见反馈 | 设备优先显示红色斜杠麦克风、`mic muted`，主页底部写 `B:MUTE`；Mac 菜单栏状态及操作也显示麦克风静音。当前页脚固定文案与对讲机 B 已转为快捷键的行为存在不一致。[ui_status.h:14–18](../firmware/src/ui_status.h#L14)、[ui.cpp:315–331](../firmware/src/ui.cpp#L315)、[ui.cpp:1333–1336](../firmware/src/ui.cpp#L1333)、[ThinCubeApp.swift:31–36](../macos/Sources/ThinCube/ThinCubeApp.swift#L31) |

普通模式不等于麦克风一直关闭：`localSound` 默认 true，正常竖屏主页可为环境声纹和动作采音；`allowed` 只约束传音分支。本地功能只消费音量 RMS，网络 PCM 只有传音时发送；不能将常态声纹描述为持续上传语音。[voice_state.h:6–20](../firmware/src/voice_state.h#L6)、[ui.cpp:791–816](../firmware/src/ui.cpp#L791)、[device.cpp:712–714](../firmware/src/device.cpp#L712)、[clawd_sound_meter.h:12–14](../firmware/src/clawd_sound_meter.h#L12)

系统页面优先接管 B：配网、OTA、连接设置、配对等场景各有取消、返回或切换含义，不存在所有页面都生效的 B 静音。[ui.cpp:611–642](../firmware/src/ui.cpp#L611)

## 有哪些成立的应用场景

以下是由已实现能力推导的场景，不代表已经观察到用户高频使用。

| 场景 | 为什么有用 | 频率与限制判断 |
| --- | --- | --- |
| 桌边谈私事、接私人电话，暂时不希望设备采集 | 直接在设备上停麦，本地声音互动和远端传音一起停，离线也可操作 | **最明确的保留价值。** 是状态式隐私控制；不涉及 Mac 自带或其他外接麦克风 |
| 嘈杂环境中想让 Clawd 暂时不随声反应，但继续看角色、按 A 玩 | B 可暂停麦克风驱动的声纹和反射，恢复时不必改长期声音互动偏好 | 有效但低频；喇叭短音、A 按键与姿态反馈仍可继续，不是完整勿扰模式 |
| 搬动、拿起设备前，临时禁止误倒置开麦 | `muted` 阻止姿态与 Mac 手动请求建立新传音会话，状态跨重启保存 | 可作额外保险；长期不用语音已有“允许语音输入”开关，单纯停止当前传音已有翻回退出 |
| Mac 已开始手动持续传音，设备处于普通竖持，突然需要截断输入 | 此时双键对讲机上下文未接管 B，普通 B 可停采并结束 manual 会话 | 有应急价值；再次 B 仅取消静音，**不会恢复原手动会话**，需 Mac“开始持续语音”或新倒置动作，不适合反复轮流发言 |
| 只想减少正常主页环境采集与音频芯片功耗 | 静音消除采集需求，音频闲置后既有策略可关闭 codec | 是附带效果，缺少本轮电流/续航测量，不应单独包装成省电功能；长期停环境声用“声音互动”更精准 |

场景依据：[设备麦克风总门槛:15–20](../firmware/src/voice_state.h#L15)、[声音互动与系统按键分路:628–639](../firmware/src/ui.cpp#L628)、[A 互动仍由按键处理:707](../firmware/src/ui.cpp#L707)、[手动开始:646–652](../firmware/src/device.cpp#L646)、[双键只接管倒置上下文:550](../firmware/src/device.cpp#L550)、[codec 闲置关闭:915–923](../firmware/src/ui.cpp#L915)。

## 哪些看似合理的场景，实际上不覆盖

- **让 Clawd 不发声。** B 不改 `soundMuted` 或音量；静音后 A 互动、唤醒等仍可能有短电子音。喇叭音效在独立设置和菜单栏操作中控制。[device.cpp:545](../firmware/src/device.cpp#L545)、[sound_policy.h:11–25](../firmware/src/sound_policy.h#L11)、[ThinCubeApp.swift:35–36](../macos/Sources/ThinCube/ThinCubeApp.swift#L35)
- **让 Mac 音乐、视频或会议对方的声音静音。** ThinCube 这里处理设备麦克风到 BlackHole 的输入链路，没有修改系统输出音量。[AudioBridge.swift:19–50](../macos/Sources/ThinCube/AudioBridge.swift#L19)、[DeviceModel.swift:694–696](../macos/Sources/ThinCube/DeviceModel.swift#L694)
- **所有软件、所有麦克风的一键隐私。** 仅停止 ThinCube 来源；Mac 内建麦克风、其他输入设备及应用自己的静音状态不由该字段控制。音频桥显式选择 BlackHole 为系统输入是另外一个操作。[AudioBridge.swift:58–61](../macos/Sources/ThinCube/AudioBridge.swift#L58)
- **对讲机中按 B 立刻静音/恢复。** 协商双键后，已传音、倒置、竖屏且采集就绪时 B 是快捷键，即使 B 未绑定也不落回静音。若将 B 绑定为目标应用的静音快捷键，是另一项可配置能力，不能算普通模式静音本身的场景。[device.cpp:550](../firmware/src/device.cpp#L550)、[button_gestures.h:15–18](../firmware/src/button_gestures.h#L15)、[DeviceModel.swift:697–707](../macos/Sources/ThinCube/DeviceModel.swift#L697)
- **每次暂停说话必须使用 B。** 姿态会话本来可以翻回正常竖持结束；Mac 也有“停止语音”和菜单栏麦克风静音。停止传音与关闭全部采集的区别，在于本地声音互动是否仍被允许。[voice_state.h:37–46](../firmware/src/voice_state.h#L37)、[ThinCubeApp.swift:35–38](../macos/Sources/ThinCube/ThinCubeApp.swift#L35)

## 为什么会觉得低频

1. 普通模式没有正在向 Mac 传音时，静音的主要即时变化只是常态声纹与声音动作停止；核心效果是隐私状态，视觉收益有限。[voice_state.h:18–20](../firmware/src/voice_state.h#L18)、[ui.cpp:807–816](../firmware/src/ui.cpp#L807)
2. 最需要频繁说话/暂停的对讲机模式已经给 B 分配快捷键；普通 B 无法承担这个高频场景。[device.cpp:550](../firmware/src/device.cpp#L550)、[button_gestures.h:15–18](../firmware/src/button_gestures.h#L15)
3. 静音会结束会话；取消后不续传，必须新倒置或显式 start。它更适合“暂时关麦一段时间”，不适合通话内反复切换。[voice_state.h:22](../firmware/src/voice_state.h#L22)、[voice_state.h:37–55](../firmware/src/voice_state.h#L37)、[pose_voice.h:10–14](../firmware/src/pose_voice.h#L10)
4. 普通 B 还是可触达的实体侧键，误触造成的持久静音会影响下次倒置语音，因此若未来更换单击用途，应同时保留明确可恢复的设备端隐私入口；Mac 菜单栏已有同一字段入口可沿用。这是后续设计建议，不是当前变更。[device.cpp:545](../firmware/src/device.cpp#L545)、[ThinCubeApp.swift:35](../macos/Sources/ThinCube/ThinCubeApp.swift#L35)

**建议定位为“设备麦克风隐私开关”并保留能力；它是否值得占据普通模式 B 的唯一单击，需要与候选高频动作比较。** 当前证据支持“功能有价值、使用频率可能较低”，不能只凭历史保留就认定单击入口最合适，也不能因低频就推导隐私能力可以删除。

若未来希望覆盖会议中反复发言/闭麦，更合适的设计对象是目标应用的静音状态，或保留会话的可恢复采音暂停。例如 Zoom 在 macOS 提供 `⌘⇧A` 静音/取消静音，以及可启用的按住空格暂时取消静音；这证明存在独立的应用层控制入口，不代表当前普通 B 已对接，也不保证任意软件的快捷键都全局有效。[Zoom 官方快捷键说明](https://support.zoom.com/hc/en/article?id=zm_kb&sysparm_article=KB0067050)、[Zoom 官方按住说话说明](https://support.zoom.com/hc/en/article?id=zm_kb&sysparm_article=KB0063245)

## 历史与验证范围

初始仓库基线 `6d3bb24` 已有 `deviceToggleMute()` 与持久化，后来的 Wi-Fi/BLE 切换继续保留 B 短按静音；v0.4.41 才将对讲机内 B 分配为第二快捷键。既有记录显示用户实测普通 B 两次短按静音并恢复，通过 BLE 按键验收；这证明基本动作可用，不证明需求频率。[Wi-Fi/BLE 记录:11](wifi-ble-implementation.md#L11)、[Wi-Fi/BLE 实体验收:93](wifi-ble-implementation.md#L93)、[双键行为](walkie-b-shortcut-v0.4.41.md#使用行为)、[双键实体验收](walkie-b-shortcut-v0.4.41.md#实体按键验收)

本轮做了静态源码与相关 Git 历史核对，没有重新运行测试或测量功耗、使用次数、端到端静音时延。若要决定替换单击，下一步应围绕候选动作做产品取舍和小样本使用观察。
