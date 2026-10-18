# 对讲机 A / B 动画：当前源码证据与实施边界

日期：2026-10-18。依据当前工作区源码及已锁定角色规范；工作区含已实施的 v0.4.41 双键改动，本笔记按文件当前内容和行号记录。仅新增本文，没有改产品代码、构建、烧录或访问设备。

**后续用户决定（2026-10-18）：继续使用 A/B 统一动作，不实施本文提出的输入拆分和 B 独立反馈。** 本文与历史角色规范的对比仅保留为调研证据；本次统一动作与灯色以用户最新决定为准。

## 决策结论

可在现有几何渲染上给 A / B 增加不同的本地反馈，无需新图片、完整动作帧资源或通信协议。**动画应表示“设备本地接受了这枚对讲机按键”**；设备没有 Mac 绑定信息及注入结果，不能把它表述为“快捷键已执行”或“语音软件成功”。

当前渲染已经存在一个来源丢失：`deviceHeld()` 将 A 与 B 合并，`petDraw()` 用该值控制抬起对讲机、绿灯和音量标记。因此 B 单独按住也会触发原 A 的演出。新增区分时应为动画分别读取 A/B；`deviceHeld()` 仍保留给现有故事、文字门控及状态文案。[device.cpp:529](../firmware/src/device.cpp#L529)、[pet.cpp:50](../firmware/src/pet.cpp#L50)、[pet.cpp:95](../firmware/src/pet.cpp#L95)、[ui.cpp:748](../firmware/src/ui.cpp#L748)、[ui.cpp:793](../firmware/src/ui.cpp#L793)

## 已锁定内容：本次不能顺带改变

| 约束 | 一手依据 |
| --- | --- |
| Clawd 无嘴、无腮红、无手臂/手掌；触角不握物；保留 terracotta 身体、琥珀界面、直角像素轮廓和原几何比例 | [角色规范:9–16](clawd-character-v2.md#L9) |
| 物件使用念力悬浮，与身体留清晰间隙；不画实体连接 | [角色规范:14–15](clawd-character-v2.md#L14) |
| A 按住：对讲机浮到脸侧、看向道具、触角微竖；A 松开：降回身侧，音频会话继续 | [角色规范:22–24](clawd-character-v2.md#L22) |
| 待命红灯 `#FF4D4F`；**A 按住绿灯 `#3DDC97`**；灯色与真实音频 LIVE 分开 | [角色规范:28](clawd-character-v2.md#L28) |
| 静音/退出收道具；静音停止麦克风与声音驱动动作；动画不宣称识别、提交或转写成功 | [角色规范:25–26](clawd-character-v2.md#L25) |
| 声音动作是轻量叠加；常态约 1–2 px、事件跳跃上限约 4 px；语音时让出音频通路 | [角色规范:34](clawd-character-v2.md#L34)、[角色规范:63–67](clawd-character-v2.md#L63)、[角色规范:72](clawd-character-v2.md#L72) |

可给 B 设计短侧目、触角一侧利落回应、少量离散念力粒子等独立叠加；不让 B 抬起原 A 对讲机或把灯变绿。若 A+B 同时按住，保留 A 的抬机/绿灯并叠加 B 的小动作。这是与现有约束兼容的实施方向，具体动作仍待用户选择和预览确认。

## 渲染链与坐标证据

`petDraw()` 是程序化矩形绘制，不是精灵帧播放器。原单位空间为 **66×52**：身体54×39，两侧触角各6×13，四腿x=6/18/42/54，眼睛为两颗方块；实际几何由 `rect(Context,...)` 按比例换算到 sprite。[pet.h:5–8](../firmware/src/pet.h#L5)、[pet.cpp:15–16](../firmware/src/pet.cpp#L15)、[pet.cpp:88–90](../firmware/src/pet.cpp#L88)

| 当前量/分支 | 源码位置与含义 |
| --- | --- |
| `scale, visible, lift` | [pet.cpp:8](../firmware/src/pet.cpp#L8)，命名空间全局状态。源码没有名为 `appear` 的变量；当前出现/收回进度叫 `visible` |
| 出现与抬升 | [pet.cpp:50–54](../firmware/src/pet.cpp#L50)：`live=deviceStreaming()`，`held=deviceHeld()&&live`；`dt` 上限80ms；visible 按130ms尺度追随live，lift按85ms尺度追随held；这些是渐近缓动，不是固定完成时长 |
| 传音造型覆盖 | [pet.cpp:63](../firmware/src/pet.cpp#L63)：live时覆写触角、视线和身体位移；普通情绪仍为底层，但部分效果被覆盖 |
| 出现时缩放/位置 | [pet.cpp:65](../firmware/src/pet.cpp#L65)、[pet.cpp:80–82](../firmware/src/pet.cpp#L80)：live缩放上限1.24；默认从居中移动到x=16附近 |
| 对讲机本体 | [pet.cpp:94–102](../firmware/src/pet.cpp#L94)：位于原单位x≈74，held抬升约22单位；灯色、屏幕刻度、音量标记均依赖合并held |
| 边界不止66单位 | 道具本体延伸到x≈91；竖屏稳定live按x=16、k=1.24估算最右约129px。新B粒子不能只按66宽身体估算占位 |
| 普通动作/声音 | [pet.cpp:55–79](../firmware/src/pet.cpp#L55)：传音期间不走本地声音表情叠加；音量身体微动来自deviceLevel，而deviceLevel在静音时为0，见[device.cpp:545](../firmware/src/device.cpp#L545) |

原 A 动作保持与 B 独立叠加适合由两个连续量处理，例如保留 `aLift`，新增幅度受限的 `bResponse`；灯色只查 A 的逻辑保持状态。不要将 `visible` 改成 A/B 的 OR：无键时仍在持续传音，对讲机本就应留在身侧。

### 调用次数与场景

当前全项目有 **4 个 `petDraw()` 调用点**；由 `uiDraw()` 的 return/if分支互斥，正常每个绘制帧最多调用一次，不是一次绘制4只角色。

| 场景 | 调用及当前尺寸 | 新动作注意点 |
| --- | --- | --- |
| 配网 | [ui.cpp:1008](../firmware/src/ui.cpp#L1008)，scale=.8，y=124 | 操作流程优先；不能在这个调用无条件读物理A/B来播放对讲机动作 |
| 横屏探索 | [ui.cpp:1112](../firmware/src/ui.cpp#L1112)，scale=1.35，y=38；live时originX=2 | Mac手动会话可保留道具，横屏A给游戏，不应触发快捷键反馈 |
| 横屏接星 | [ui.cpp:1179–1181](../firmware/src/ui.cpp#L1179)，clip为73×63，live scale=.74 | 道具/粒子受裁剪；新增外延不能只验证竖屏 |
| 竖屏主页 | [ui.cpp:1284–1294](../firmware/src/ui.cpp#L1284)，scale=1.55，y=77；故事播放时不用petDraw | 对讲机动画主要落点；实际live缩放仍受pet.cpp的1.24限制 |

路由优先级见 [ui.cpp:1237–1271](../firmware/src/ui.cpp#L1237)：连接设置、配网、OTA、切换、配对、诊断、横屏依次接管。故事及文字门控排除 `deviceStreaming()` / `deviceHeld()`，见 [ui.cpp:747–756](../firmware/src/ui.cpp#L747)、[ui.cpp:793](../firmware/src/ui.cpp#L793)。

主循环先 `M5.update()`，再uiTick，再deviceTick，随后故事/文字更新，最后到绘制限频，见 [main.cpp:138–157](../firmware/src/main.cpp#L138)、[main.cpp:186–193](../firmware/src/main.cpp#L186)。当前全局缓动仅在petDraw推进；隐藏操作页、故事或息屏时不会推进，回场可能继承旧缓动。若新增“按下只播一次”的B动作，宜在deviceTick后统一更新小动作策略，绘制只消费结果，或显式处理离开场景重置；避免各调用点各自保存上一键状态导致复播。

## 输入来源：三种语义不能混用

| 语义层 | 当前来源 | 能证明什么 | 动画适配 |
| --- | --- | --- | --- |
| 物理按下 | `M5.BtnA/B.isPressed()` | 机械键当前按着 | 不适合作对讲机正式反馈；会越过预按、页面接管与武装规则 |
| 设备本地接受 | `voice.held`（A）、`walkieKeys.heldB`（B） | 当前上下文已武装、允许并正在保持 | **推荐**；动画描述本地A/B动作，不能描述Mac执行结果 |
| Mac已提交/目标软件成功 | Mac过滤、KeyInjector以及外部应用处理 | 目前没有回传设备的结果字段 | 不能仅凭held、ack、LIVE推断；真要显示需独立设计结果协议，超出最小动画改动 |

`WalkieKeys` 已分别存储active/heldA/heldB/armedA/armedB：进入时已按住的键不激活，物理松开后才武装；A还受 `allowA` 限制，B独立保持。[walkie_keys.h:4–14](../firmware/src/walkie_keys.h#L4)。注意 `armedA/B` 表示“允许下一次按下”，**并不等于正在按住**；动画使用held而不是armed。

双键模式中 [device.cpp:657–660](../firmware/src/device.cpp#L657) 采样WalkieKeys，并把heldA写回voice.held；未协商双键时使用既有 `voice.button()`，所以只暴露walkieKeys.heldA会漏掉旧Mac的正常A反馈。旧A逻辑与防预按规则见 [voice_state.h:37–59](../firmware/src/voice_state.h#L37)。

双键上下文还要求 `dualKeys && streaming && inverted && canStream && 竖屏 && 非游戏`，见 [device.cpp:530](../firmware/src/device.cpp#L530)。A的UI过滤排除唤醒消费、系统手势、游戏等，见 [ui.cpp:568](../firmware/src/ui.cpp#L568)。

Mac绑定只在 [DeviceModel.swift:112–119](../macos/Sources/ThinCube/DeviceModel.swift#L112) 管理；B未设置时绑定为nil。Mac收到边沿后可因未绑定、编辑、停止、切换等原因拦截，之后仍更新ack；ack不是成功确认。[DeviceModel.swift:688–715](../macos/Sources/ThinCube/DeviceModel.swift#L688)、[DeviceKeyStream.swift:17–50](../macos/Sources/ThinCubeCore/DeviceKeyStream.swift#L17)。注入还可被权限、无效组合、物理键冲突或事件构造失败拦截，见 [KeyInjector.swift:24–40](../macos/Sources/ThinCube/KeyInjector.swift#L24)；即使post成功，也没有目标语音软件业务结果，符合角色规范“不得宣称完成”的限制。

## 必须保留的边界行为

| 场景 | 当前源码真实行为 | A/B动画应如何取值 |
| --- | --- | --- |
| B未设置 | 新Mac仍协商双键；固件可接受heldB；Mac收到后不注入但确认边沿 | 可播放“B被本地接受”的中性动作；不能绿灯、庆祝成功或提示已执行。若要求未绑定完全无反馈，必须新增绑定同步，非当前零协议方案 |
| 旧Mac/尚未协商 | dualKeys=false；A走旧voice.held；B保留短按静音/长按系统动作 | A仍播原动作；B不播对讲机动作。协商变化会清除held，需先松开再按 |
| A+B同时按下 | WalkieKeys两路独立；对讲机上下文不触发系统组合键 | 允许两个独立动作共存；A抬机/绿灯优先占位，B只轻量叠加 |
| 分别释放 | A释放只清heldA，B释放只清heldB；音频继续streaming | A松后降机/红灯；B仍可保持自己的弱反馈，反之亦然 |
| 按住进入/退出 | 预按不触发；上下文关闭清空WalkieKeys；B整次按压被消费，不在退出松手重新解释成静音/切换 | 不读raw来绕过门槛；没有延迟重放；新上下文从松开重新武装 |
| 静音/禁止/睡眠/操作忙 | voice不能继续stream；stop清A；主tick清B | 两种活动反馈退出，道具按live收起；静音停止音量驱动 |
| 断线/控制租约到期 | 物理链路丢失停voice；设备peer租约超过1000ms停voice；B随后清除 | 使用本地接受状态退出，不承诺与Mac的清键同一毫秒 |
| Mac状态超过400ms未更新 | Mac先释放所有快捷键及音频会话；超过2s进入connectionLost | 设备可能仍收到反向心跳而短暂保持held动画；不能把本地动作当成Mac仍保持按键的精确镜像 |
| 横屏/手动会议/系统页 | 手动音频可继续，快捷键上下文关闭；系统页优先显示 | live道具可沿用现有行为，A/B快捷键动作均停；不抢占操作提示 |

边界证据：[walkie_keys.h:7–13](../firmware/src/walkie_keys.h#L7)、[button_gestures.h:13–18](../firmware/src/button_gestures.h#L13)、[device.cpp:525–530](../firmware/src/device.cpp#L525)、[device.cpp:595–598](../firmware/src/device.cpp#L595)、[device.cpp:642–673](../firmware/src/device.cpp#L642)、[voice_state.h:15–35](../firmware/src/voice_state.h#L15)、[DeviceModel.swift:769–782](../macos/Sources/ThinCube/DeviceModel.swift#L769)。双键既有策略测试见 [walkie_keys_test.cpp:7–42](../firmware/test/walkie_keys_test.cpp#L7)，本调研没有重新运行测试。

## 最小可实施接口与范围

建议新增动画专用的只读快照，例如：

```cpp
struct WalkieVisualInput { bool live, heldA, heldB; };
WalkieVisualInput deviceWalkieVisualInput();
// live = voice.streaming
// heldA = voice.streaming && voice.held     // 同时兼容旧Mac与双键
// heldB = deviceWalkieKeysActive() && walkieKeys.heldB
```

快照在device内部读取，不暴露可变WalkieKeys、不重新读物理键、不读取Mac队列作为动画时钟、不改变现有deviceHeld语义。无需向协议增加heldA：现有线上 `held` 就是A，`heldB` 是B；动画只需本地getter。[device.cpp:316–319](../firmware/src/device.cpp#L316)

渲染侧保留live的visible逻辑，将原lift、绿灯、对讲机屏幕状态只交给heldA；B用独立连续量与边沿时间表达。可新增一个不依赖硬件的小策略，在主循环deviceTick之后更新一次，处理进入、保持、释放、上下文取消；`petDraw`继续画到已有canvas，不增加第二轮角色绘制，也不重置全局PetState。仅调用petPoke并不等价于B专属反馈：它会重设底层情绪时间，live覆盖部分造型，原EXCITED火花又可能混入，见 [pet.cpp:44–46](../firmware/src/pet.cpp#L44)、[pet.cpp:57](../firmware/src/pet.cpp#L57)、[pet.cpp:63](../firmware/src/pet.cpp#L63)、[pet.cpp:93](../firmware/src/pet.cpp#L93)。

UI另有一处需随最终方案对齐：竖屏底部目前固定显示 `B:MUTE`，双键对讲机时已不是实际行为；应按双键上下文显示B快捷键提示，普通/旧Mac模式保留原提示。[ui.cpp:1324–1325](../firmware/src/ui.cpp#L1324)

本地动画范围可限于device getter、pet绘制/小策略、主循环单次更新与页脚提示。**不需要修改Mac映射、键盘注入、可靠边沿协议、传音协议、IMU参数或角色图像资产。** 若新增“B已绑定”“Mac允许注入”“系统事件已提交”反馈，则另立状态协议、版本协商和超时语义，不能拿现有ack充当结果。

后续验证应覆盖A单独、B单独、两键同时及不同释放顺序；预按进入、持有退出、旧Mac、B未绑定、静音/断线/400ms状态超时；特殊页面、横屏裁剪、反复切场和低帧率。先核对本地语义与预览，再验证实机观感。本笔记没有把上述场景视为新增动画已通过验收。
