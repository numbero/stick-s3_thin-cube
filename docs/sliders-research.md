# ThinCube macOS 滑动条外观与交互调研

调研日期：2026-10-10。目标：修复亮度滑动条全灰的反馈，并统一亮度、输入增益、音效音量三个滑动条。调查依据为 Apple 官方设计/API 文档、WWDC25 讲解及本机 macOS 26.2 SDK；不以第三方教程作为事实来源。本文记录调查及实施建议，不代替实机验收。

## 结论

颜色应该从最小值一端填充到当前滑块位置，随当前值变化。不是固定把一半轨道染色；只有值处于范围中点时才接近一半。Apple HIG 明确描述这一填充关系，并建议滑动过程中给予反馈。ThinCube 的亮度范围为 `8...255`，因此轨道比例应为 `(brightness - 8) / 247`。[Apple HIG：Sliders](https://developer.apple.com/design/human-interface-guidelines/sliders)

截图中的“刻度密集、整条灰色”与 macOS 26 的原生规则一致：SwiftUI 的 `step` 初始化自动产生刻度；AppKit 的默认 tint prominence 对有刻度的滑动条不着色。这不是缺少某个固定半宽装饰条。前一条规则由 SwiftUI WWDC25 讲解直接说明，后一条规则由 `NSSlider.tintProminence` 官方说明及 SDK 注释直接说明；将两者联系到截图是基于本地代码的归因。[Apple WWDC25：Build a SwiftUI app with the new design，15:46](https://developer.apple.com/videos/play/wwdc2025/323/?time=946)、[Apple：NSSlider.trackFillColor / tintProminence](https://developer.apple.com/documentation/appkit/nsslider/trackfillcolor)、[本机 NSSlider.h](/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/System/Library/Frameworks/AppKit.framework/Headers/NSSlider.h:30)

推荐使用一个共享组件封装原生 `NSSlider`：去掉刻度、显式设置统一琥珀色 `trackFillColor`，在 macOS 26 及以后设 `tintProminence = .primary`，数值更新时按照各自步长量化。它可以兼容本项目 macOS 13 最低部署目标，并保留 AppKit 的原生轨道、滑块及鼠标追踪。此次目的不需要自行绘制轨道或替换拖拽手势；这是根据兼容范围和 API 能力作出的实施判断。[Apple：NSSlider](https://developer.apple.com/documentation/appkit/nsslider)、[Apple：trackFillColor](https://developer.apple.com/documentation/appkit/nsslider/trackfillcolor)、[Apple WWDC25：Build an AppKit app with the new design，16:12 起](https://developer.apple.com/videos/play/wwdc2025/310/?time=972)

## 调研时的项目状态

来源：本地 `macos/Sources/ThinCube/ThinCubeApp.swift`、`macos/Package.swift`，2026-10-10 调研开始时只读检查。

| 控件 | 数值范围 | 原步长 | 调研时的实现 |
| --- | --- | --- | --- |
| 亮度 | 8...255 | 1 | `Slider(..., step: 1)` |
| 输入增益 | 0.1...4 | 0.1 | `Slider(..., step: 0.1)` |
| 音效音量 | 0...100 | 1 | `Slider(..., step: 1)` |

三个控件都使用 `step`，页面根节点已有 `.tint(amber)`。因此仅给现有 SwiftUI Slider 重复添加 `.tint(amber)`，不能有依据地保证改变有刻度滑动条的默认填充规则。项目最低部署目标为 `.macOS(.v13)`。[ThinCubeApp.swift](/Users/numbero/Documents/Project/personal-project/stick-s3_thin-cube/macos/Sources/ThinCube/ThinCubeApp.swift)、[Package.swift](/Users/numbero/Documents/Project/personal-project/stick-s3_thin-cube/macos/Package.swift)

## SDK API 可用性核对

本机 `xcrun --show-sdk-path` 指向 `/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk`；`SDKSettings.plist` 的 `Version` 为 `26.2`。下表最低版本以本机 Apple SDK 的 `API_AVAILABLE` / `@available` 声明为准。

| API | 本机 SDK 结论 | 对本项目的意义 |
| --- | --- | --- |
| `NSSlider.trackFillColor` | macOS 10.12.2+ | macOS 13 全范围可直接使用 |
| `NSSlider.tintProminence` | macOS 26.0+ | 必须有 `if #available(macOS 26.0, *)` 守卫 |
| `NSSlider.neutralValue` | macOS 26.0+；未显式设置时取最小值 | 本次无需设置；普通亮度/音量按最小值填充 |
| SwiftUI `SliderTick`、`SliderTickBuilder` | macOS 26.0+ | 不能覆盖 macOS 13...15 |
| SwiftUI 新版 `Slider(..., ticks: ...)` | macOS 26.0+；该重载无 `step` 参数 | 即使空 ticks 可用，仍要自行处理步长且增加版本分支 |
| SwiftUI 新版 `Slider(..., step: ..., tick: ...)` | macOS 26.0+；`tick` 默认 `{ _ in nil }` | `nil` 本身是默认行为，不能据此宣称隐藏自动刻度 |
| SwiftUI `sliderTickBehavior` | 本机 SwiftUI / SwiftUICore 的公开 interface 查无声明，官方文档搜索亦未找到 | 不作为当前工具链可编译的修复方案 |

AppKit 属性声明见 [NSSlider.h](/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/System/Library/Frameworks/AppKit.framework/Headers/NSSlider.h:23)。SwiftUI 新重载及可用性声明见 [SwiftUI.swiftinterface](/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/System/Library/Frameworks/SwiftUI.framework/Modules/SwiftUI.swiftmodule/arm64e-apple-macos.swiftinterface:23850)。新版 step 重载的官方参数说明也将 `tick` 描述为各 step 的刻度，并注明默认 `nil`，没有说明“返回 nil 就关闭刻度”。[Apple：Slider step/tick initializer](https://developer.apple.com/documentation/swiftui/slider/init(value:in:step:neutralvalue:enabledbounds:label:currentvaluelabel:minimumvaluelabel:maximumvaluelabel:tick:oneditingchanged:))

## 方案判断

1. **共享原生 AppKit 封装，推荐。** `numberOfTickMarks = 0`，`allowsTickMarkValuesOnly = false`，设置统一 `trackFillColor`，26+ 使用 `.primary`。官方说明 primary / secondary 都会填充，而 none 不填充；不用改变全应用的控件表现。[Apple：NSSlider](https://developer.apple.com/documentation/appkit/nsslider)、[Apple WWDC25：AppKit controls](https://developer.apple.com/videos/play/wwdc2025/310/)
2. **继续使用无 step 的 SwiftUI 原生 Slider，同时在 Binding setter 量化，代码改动最少。** 去掉 step 后避免自动刻度，但必须特别检查键盘和 VoiceOver 的增减能否按原步长移动，避免系统默认微小增量被舍入成原值、表现为“按键无效”。SwiftUI 官方说明 step 同时影响 VoiceOver 的增减，因此移除它会丢失这一明确的步长契约；这是需要额外补充并验证的行为。[Apple：SwiftUI Slider](https://developer.apple.com/documentation/swiftui/slider)
3. **SwiftUI 26 新 ticks 初始化，暂不选。** 有部署版本分支、无 tick behavior API 的工具链问题，且文档不足以证明 `tick: nil` 会关闭自动刻度。仅为本次颜色问题引入这些分支没有必要。这是基于上表 SDK 检查的判断。

## 统一交互的实施与验收要求

以下是项目层面的实施建议，而非把全部行为都视为系统自动保证：

- 三个滑动条共用同一组件、填充色、尺寸、数值格式策略；剩余轨道采用原生灰色，颜色只覆盖最小值到当前值这一段。数值显示与保存/提交读取同一个量化后的值。
- 拖动保留原生 NSSlider 追踪，量化公式使用以最小值为起点的步长格：`min + round((value - min) / step) * step`，然后夹在范围内；浮点误差要规整。亮度和音量为整数，增益为一位小数。
- 保留键盘聚焦和原生绘制。若自定义步长，方向键和辅助功能增减均需调整完整一个 step；Home/End 或等效首尾动作应到达范围端点，并保证 disabled 时不能改值。只量化鼠标回调不足以验证这些行为。
- 原生 NSSlider 已具备辅助功能 slider 身份；给它有意义的中文标签、当前值与人类可读的 value description，测试 VoiceOver 增减。Apple 的 slider 协议要求 label、value、increment/decrement；对应 API 在本机 SDK 中可用于 macOS 13。[Apple：NSSlider 的协议关系](https://developer.apple.com/documentation/appkit/nsslider)、[NSAccessibilityProtocols.h](/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/System/Library/Frameworks/AppKit.framework/Headers/NSAccessibilityProtocols.h:113)
- SwiftUI 的 disabled 环境应传给原生 `NSControl.isEnabled`，保留系统禁用态。该属性与 continuous 事件选项见 [NSControl.h](/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/System/Library/Frameworks/AppKit.framework/Headers/NSControl.h:28)。不能仅以灰色视觉作为 disabled 判断。
- 在实际应用中检查亮度/增益/音量的低、中、高值，浅色/深色外观、焦点和窗口活动状态。检查颜色端点随值移动、数值按步长更新、禁用态无操作、既有保存时机保持一致。截图能证明外观；精确键盘与 AX 调整需要交互检查，不能由“编译成功”代替。


## 实施与本机验证

本机运行系统实际为 macOS 15.7.9，SDK 为 26.2；SDK 版本不能当成运行系统版本。上文 macOS 26 的默认 tint 规则用于说明新版系统行为；本机确认的是原有 step 滑动条存在密集刻度、界面 tint 没有产生可见填充。共享 NSSlider 去掉刻度并设置 trackFillColor 后，用户已确认应用前台显示琥珀色填充。非活动窗口由原生 AppKit 显示灰色，这与控件是否禁用是两个状态。

实现文件为 `macos/Sources/ThinCube/SettingsSlider.swift`。亮度、输入增益、音效音量共用尺寸、填充色、读数列和量化规则；保留亮度/增益的保存按钮，音量继续自动保存并显示对应提示。原生鼠标追踪、方向键、Option+方向键及 Home/End 共用范围/步长，SwiftUI 的禁用环境传给 NSControl。辅助功能采用 Apple 官方建议的 accessibilityRepresentation + 系统 Slider，给它同一 Binding、范围和步长；鼠标、键盘、辅助功能的输入均作相同的量化与浮点规整。[Apple：accessibilityRepresentation](https://developer.apple.com/documentation/swiftui/view/accessibilityrepresentation(representation:))

应用版本升级到 0.4.2（build 23），固定签名并安装到 `/Applications/ThinCube.app`。通过真实 UI 检查亮度 134→135→134、增益 1.14→量化 1.1→恢复 1.0、音量 36.2→量化 36→恢复 35；测试后原值保留。最终验收记录见 `artifacts/sliders-ui-acceptance-20261010.json`，构建与安装日志见 `artifacts/sliders-macos-build-20261010.log`。

最终版另验证断开后的禁用保护：辅助功能写入不会改变禁用滑动条的值，重连恢复设备亮度 134、增益 1.0 与音量 35。自动化只读和 AX 数值写入可用；原生鼠标/键盘接口返回 noWindowsAvailable，因此没有把它们或真实 VoiceOver 作为已完成实测，详见上述验收 JSON。浅色外观及 macOS 13 / 26 的运行验收未执行。
