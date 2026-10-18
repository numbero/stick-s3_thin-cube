# Mac v0.4.7 仅菜单栏模式

日期：2026-10-11。Mac v0.4.7（build 28）新增“仅在菜单栏显示”，入口位于设置页的“应用显示”以及 ThinCube 菜单栏菜单。默认关闭；首次启用立即隐藏 Dock 图标并收起主窗口，连接、语音和菜单栏快捷操作继续运行。偏好保存到 `UserDefaults` 的 `ui.menuBarOnly.v1`，后续启动按保存状态运行。

开启后，点击菜单栏的“显示 ThinCube”“设置…”或“按键与快捷键…”可以打开同一个主窗口，Dock 图标继续隐藏。关闭主窗口仅收起窗口，应用继续驻留；菜单栏可以再次打开窗口或明确退出。关闭该开关会恢复 Dock 图标并显示主窗口。应用已在运行时从访达再次打开，也能恢复主窗口，不改变菜单栏偏好。[Apple：应用重新打开回调](https://developer.apple.com/documentation/appkit/nsapplicationdelegate/applicationshouldhandlereopen(_:hasvisiblewindows:))。

需要设备配对时，菜单栏显示等待配对及“完成配对…”入口；显式打开窗口后沿用现有自动配对弹窗。窗口被收起或最小化时，配对需求不会主动拉出窗口抢焦点。

## 参考与实现

对照本机 `folotoy-vibecard/macapp/Sources/VibeCard/main.swift` 的 `applyActivationPolicy` / `setMenuBarOnly`、`Preferences.swift` 的菜单栏偏好和 `MainWindow.swift` 的窗口持有方式。ThinCube 保留原 SwiftUI 页面与 `MenuBarExtra`，由 AppKit 管理一个可重复打开的主窗口，避免菜单栏模式启动时创建或闪现窗口，也修复原来关闭窗口后只查找 `NSApp.windows.first` 无法可靠重开的路径。

Apple 的公开 `.accessory` 激活策略支持不显示 Dock 图标，同时仍可程序激活或点击窗口；普通模式使用 `.regular`。`LSUIElement=true` 使启动阶段先作为菜单栏工具运行，再按保存偏好设置激活策略。没有修改系统 Dock 全局设置或请求额外权限。[Apple：accessory](https://developer.apple.com/documentation/appkit/nsapplication/activationpolicy-swift.enum/accessory)、[设置激活策略](https://developer.apple.com/documentation/appkit/nsapplication/setactivationpolicy(_:))、[MenuBarExtra](https://developer.apple.com/documentation/swiftui/menubarextra)。

窗口使用 `NSHostingController` 容纳原 SwiftUI 页面，macOS 14+ 显式启用工具栏桥接；最低部署目标仍为 macOS 13。[Apple：NSHostingController](https://developer.apple.com/documentation/swiftui/nshostingcontroller)、[工具栏桥接](https://developer.apple.com/documentation/swiftui/nshostingcontroller/scenebridgingoptions)。

## 验证

- release 构建、固定签名与 `/Applications/ThinCube.app` 安装比对通过；设置页实际显示 0.4.7，已授权状态保留，设备 v0.4.13 自动恢复 Wi-Fi 在线，亮度 134 与校准保留。
- 70 项 Swift 测试通过、无失败及跳过。没有改动或烧录设备固件。
- 原生界面检查“应用显示”开关和说明，开启、关闭、再开启均通过；关闭窗口后应用继续运行，可从系统窗口菜单恢复同一个窗口，连接仍在线。
- 用户现场确认：“Dock 已隐藏，菜单栏能打开设置”；菜单栏打开设置后 Dock 仍保持隐藏。
- 最终构建安装后自动重启，原生设置页确认开关仍为 on，既有授权保留并恢复 Wi-Fi 在线。重新打开设置的自动化操作会激活应用，未将此时的窗口当作冷启动不显示窗口的独立现场证明。
- 原有“更换 Wi-Fi”弹窗打开与取消通过，未提交新的网络凭据。最终保持菜单栏模式开启并收起主窗口。
- 收尾时设备暂不可达，上位机在收起窗口后继续按原自动模式重试 Wi-Fi / BLE。独立请求设备公开信息也连接超时，本机未枚举 USB 串口；本轮没有修改设备固件或连接偏好。

构建与测试日志保存在本机 Git 忽略目录 `artifacts/menu-bar-v0.4.7/`。macOS 13 已通过最低目标编译，实机检查使用 macOS 15.7.9。

## 2026-10-19 菜单精简

菜单上半部分统一使用原生对勾开关：“麦克风静音”“扬声器静音”“仅在菜单栏显示”。名称固定，勾选表示已启用；前两项与设备返回的静音状态同步，“扬声器静音”沿用设备独立的 `soundMuted` 设置，不改变已保存音量。

移除菜单中的“停止语音输入”“按键与快捷键…”和“设置…”入口，将“显示 ThinCube”改为“详细设置…”，统一打开现有主窗口并保留当前页面。完整语音控制、按键配置和其他设置仍可从主窗口访问；需要配对时继续提供“完成配对…”入口。“仅在菜单栏显示”的窗口及 Dock 行为沿用原实现。

主窗口侧栏的“语音”改为“声音”，图标改为扬声器，页内保留“语音输入”“设备声音”和“Mac 音频输入”分组。设备声音区域统一使用“扬声器静音”“扬声器音量”和“试听声音”，连接、固件兼容、播放暂停及帮助提示同步使用声音与扬声器相关表述。

本轮只修改 macOS 上位机界面和文案，设备固件及通信接口沿用原实现。release 构建、固定签名和 `/Applications/ThinCube.app` 安装一致性校验通过；已在实际页面核验声音区域文案、帮助说明及侧栏扬声器图标。构建安装日志保存在本机 Git 忽略目录 `artifacts/menu-2026-10-19/`。
