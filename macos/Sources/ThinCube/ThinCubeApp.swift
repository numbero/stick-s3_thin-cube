import SwiftUI
import AppKit
import UniformTypeIdentifiers
import ThinCubeCore

@main struct ThinCubeApp: App {
    @NSApplicationDelegateAdaptor(ThinCubeAppDelegate.self) private var delegate
    private static let menuBarIcon: NSImage = {
        let image = Bundle.main.url(forResource: "MenuBarIconTemplate", withExtension: "pdf")
            .flatMap { NSImage(contentsOf: $0) }
            ?? NSImage(systemSymbolName: "waveform", accessibilityDescription: "ThinCube")
            ?? NSImage(size: NSSize(width: 18, height: 18))
        image.isTemplate = true
        image.size = NSSize(width: 18, height: 18)
        return image
    }()
    var body: some Scene {
        MenuBarExtra {
            ThinCubeMenu(model: delegate.model, presentation: delegate.presentation)
        } label: {
            Image(nsImage: Self.menuBarIcon).accessibilityLabel("ThinCube")
        }
    }
}

private struct ThinCubeMenu: View {
    @ObservedObject var model: DeviceModel
    @ObservedObject var presentation: AppPresentation
    var body: some View {
        Text(model.name)
        Text(model.pairingRequired ? "等待设备配对" : !model.connected ? "设备未连接" : (model.status?.streaming == true ? "正在传音" : (model.status?.muted == true ? "麦克风已静音" : "语音待命")))
        if model.pairingRequired {
            Button("完成配对…") { presentation.showMainWindow(pane: .device) }
        }
        Toggle("麦克风静音", isOn: Binding(
            get: { model.status?.muted ?? false },
            set: { value in Task { await model.apply(["muted": value]) } }
        )).disabled(!model.connected)
        Toggle("扬声器静音", isOn: Binding(
            get: { model.status?.soundMuted ?? false },
            set: { value in Task { await model.apply(["soundMuted": value]) } }
        )).disabled(!model.connected || model.status?.soundVolume == nil)
        Toggle("仅在菜单栏显示", isOn: Binding(get: { presentation.menuBarOnly }, set: { presentation.setMenuBarOnly($0) }))
        Divider()
        Button("详细设置…") { presentation.showMainWindow() }
        Divider()
        Button("退出") { model.shutdown(); NSApp.terminate(nil) }
    }
}

enum MainPane: String, CaseIterable, Identifiable {
    case device, keys, voice, firmware, settings
    var id: String { rawValue }
    var title: String {
        switch self {
        case .device: return "设备"
        case .keys: return "按键"
        case .voice: return "声音"
        case .firmware: return "固件"
        case .settings: return "设置"
        }
    }
    var icon: String {
        switch self {
        case .device: return "rectangle.portrait"
        case .keys: return "keyboard"
        case .voice: return "speaker.wave.2"
        case .firmware: return "arrow.down.circle"
        case .settings: return "gearshape"
        }
    }
}

private enum MainSheet: Equatable, Identifiable {
    case pairing(UUID), manualAddress, changingNetwork, keyboardDiagnostics, audioDiagnostics, tapDiagnostics
    var id: String {
        switch self {
        case .pairing(let id): return "pairing-" + id.uuidString
        case .manualAddress: return "address"
        case .changingNetwork: return "network"
        case .keyboardDiagnostics: return "keyboard"
        case .audioDiagnostics: return "audio"
        case .tapDiagnostics: return "tap"
        }
    }
}

struct MainView: View {
    @ObservedObject var model: DeviceModel
    @ObservedObject var discovery: Discovery
    @ObservedObject var presentation: AppPresentation
    @State private var hoveredPane: MainPane?
    @State private var accessibilityTrusted = false
    @State private var postingAllowed = false
    @State private var activeSheet: MainSheet?
    @State private var presentedPairing: UUID?
    private let amber = Color(red: 0.973, green: 0.690, blue: 0.000)
    private let amberHi = Color(red: 1.000, green: 0.788, blue: 0.302)

    var body: some View {
        NavigationSplitView {
            VStack(alignment: .leading, spacing: 1) {
                ForEach(MainPane.allCases) { pane in sidebarRow(pane) }
                Spacer(minLength: 0)
                VStack(alignment: .leading, spacing: 2) {
                    Text("ThinCube " + (Bundle.main.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String ?? ""))
                    Text("实时注入")
                }
                .font(.system(size: 10.5))
                .foregroundStyle(.tertiary)
                .padding(.horizontal, 9)
                .padding(.top, 10)
            }
            .padding(.horizontal, 8)
            .padding(.top, 6)
            .padding(.bottom, 10)
            .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
            .navigationSplitViewColumnWidth(min: 180, ideal: 196, max: 220)
        } detail: {
            VStack(alignment: .leading, spacing: 0) {
                switch model.pane {
                case .device: device
                case .keys: keys
                case .voice: audio
                case .firmware: firmware
                case .settings: settings
                }
                Text(model.note).font(.callout).foregroundStyle(.secondary).textSelection(.enabled)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .padding(.horizontal, 28).padding(.vertical, 12)
                    .accessibilityLabel("状态：" + model.note)
            }
            .toolbar {
                ToolbarItem(placement: .status) {
                    Label(model.pairingRequired ? "等待配对" : model.connected ? "在线 · " + model.name : (model.connecting ? "连接中" : (model.reconnecting ? "等待自动重连" : "未连接")),
                          systemImage: model.connected ? (model.currentTransport == "ble" ? "antenna.radiowaves.left.and.right" : "wifi") : "wifi.slash")
                        .font(.system(size: 11.5)).foregroundStyle(model.connected ? .green : .secondary)
                }
            }
        }
        .tint(amber)
        .sheet(item: $activeSheet, onDismiss: sheetDismissed) { sheet in
            switch sheet {
            case .pairing(let id): PairingSheet(model: model).onAppear { presentedPairing = id }
            case .manualAddress: ManualWiFiConnectionSheet(model: model)
            case .changingNetwork: ChangeWiFiSheet(model: model)
            case .keyboardDiagnostics: KeyboardDiagnosticsSheet(model: model)
            case .audioDiagnostics: AudioDiagnosticsSheet(model: model)
            case .tapDiagnostics: TapSettingsPanel(model: model)
            }
        }
        .onAppear { presentPairingIfNeeded() }
        .onChange(of: model.pairingSession?.id) { _ in presentPairingIfNeeded() }
        .onReceive(NotificationCenter.default.publisher(for: NSApplication.didBecomeActiveNotification)) { _ in presentPairingIfNeeded() }
        .onReceive(NotificationCenter.default.publisher(for: AppPresentation.windowShown)) { _ in presentPairingIfNeeded() }
        .onReceive(NotificationCenter.default.publisher(for: NSWindow.didBecomeKeyNotification)) { _ in presentPairingIfNeeded() }
    }

    private func presentPairingIfNeeded() {
        guard let session = model.pairingSession else {
            if case .pairing = activeSheet { activeSheet = nil }
            return
        }
        guard NSApp.isActive, presentation.mainWindowVisible else { return }
        let desired = MainSheet.pairing(session.id)
        if activeSheet == desired { return }
        // Complete dismissal of an existing sheet before presenting the next.
        if activeSheet != nil { activeSheet = nil }
        else { activeSheet = desired }
    }
    private func sheetDismissed() {
        if let id = presentedPairing, model.pairingSession?.id == id {
            model.disconnect(note: "配对已取消。点击“连接”可重新开始。")
        }
        presentedPairing = nil
        presentPairingIfNeeded()
    }

    private func sidebarRow(_ pane: MainPane) -> some View {
        let selected = model.pane == pane
        let hovered = hoveredPane == pane
        return Button { model.pane = pane } label: {
            HStack(spacing: 9) {
                Image(systemName: pane.icon).font(.system(size: 12)).frame(width: 16)
                Text(pane.title).font(.system(size: 13, weight: selected ? .semibold : .regular))
                Spacer(minLength: 0)
            }
            .padding(.horizontal, 9)
            .frame(height: 30)
            .background(selected ? amber.opacity(0.16) : (hovered ? Color.white.opacity(0.06) : .clear),
                        in: RoundedRectangle(cornerRadius: 6))
            .foregroundStyle(selected ? amberHi : Color.primary.opacity(0.88))
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .onHover { inside in
            if inside { hoveredPane = pane } else if hoveredPane == pane { hoveredPane = nil }
        }
        .accessibilityAddTraits(selected ? [.isSelected] : [])
    }

    private var keys: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 20) {
                HStack {
                    Text("按键").font(.system(size: 21, weight: .bold))
                    Spacer()
                    Button("按键诊断…") { activeSheet = .keyboardDiagnostics }
                        .disabled(model.shortcutRecording || model.connecting || model.updating)
                    HelpPopover(title: "快捷键帮助") {
                        HelpParagraph("设备 A/B 键", "倒置对讲机模式中，A/B 分别触发设置的快捷键，按下保持、松开释放。B 未设置时不执行操作；其他模式 B 保持静音和系统功能。设置为当前设备保存。")
                        HelpParagraph("独立双敲", "倒置对讲时双敲正面面板，执行独立设置的快捷键。绑定按设备独立保存。敲击设置窗口打开或手持实验模式开启时，双敲仅记录，不发送快捷键。")
                        HelpParagraph("录制或选择", "点击录制后按下组合键，松开全部按键即保存，Esc 取消。也可直接选择按键；仅修饰键组合无需主键。")
                        HelpParagraph("按键诊断", "自动记录 A/B 与双敲的设备事件、接受或拦截原因，以及按键提交的时间线。保留权限与设备状态，支持复制、清空日志。")
                    }
                }
                ShortcutEditor(model: model).disabled(model.updating || model.connecting)
                HStack(spacing: 12) {
                    VStack(alignment: .leading, spacing: 4) {
                        Text("敲击识别").font(.system(size: 13))
                        Text("按习惯调整敲击力度、节奏与防误触。")
                            .font(.system(size: 11.5)).foregroundStyle(.secondary)
                            .fixedSize(horizontal: false, vertical: true)
                    }
                    Spacer(minLength: 12)
                    Button("敲击设置…") { activeSheet = .tapDiagnostics }
                        .buttonStyle(.bordered).controlSize(.small)
                        .disabled(!model.connected || model.updating || model.switching)
                }
                .padding(.horizontal, 14).padding(.vertical, 12)
                .background(ShortcutStyle.groupFill, in: RoundedRectangle(cornerRadius: ShortcutStyle.groupCornerRadius))
            }
            .padding(.horizontal, 28).padding(.top, 24).padding(.bottom, 30)
            .frame(maxWidth: .infinity, alignment: .leading)
        }
    }

    private var settings: some View {
        Form {
            Section {
                Toggle("仅在菜单栏显示", isOn: Binding(get: { presentation.menuBarOnly }, set: { presentation.setMenuBarOnly($0) }))
                Text("隐藏 Dock 图标并收起主窗口，连接与语音继续运行。可从菜单栏打开窗口或恢复 Dock 显示；重启后保留此选择。")
                    .font(.callout).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
            } header: { Text("应用显示") }
            Section {
                HStack(spacing: 12) {
                    Text("键盘控制")
                    Spacer()
                    Label(accessibilityTrusted && postingAllowed ? "已授权" : "未生效",
                          systemImage: accessibilityTrusted && postingAllowed ? "checkmark.circle.fill" : "exclamationmark.circle")
                        .foregroundStyle(accessibilityTrusted && postingAllowed ? .green : .orange)
                    if !accessibilityTrusted || !postingAllowed {
                        Button("打开系统设置…") {
                            NSWorkspace.shared.open(URL(string: "x-apple.systempreferences:com.apple.preference.security?Privacy_Accessibility")!)
                        }
                    }
                }
            } header: {
                HelpSectionHeader("权限", helpTitle: "权限帮助") {
                    HelpParagraph("键盘控制", "ThinCube 需要辅助功能权限，才能把设备 A/B 键发送到当前应用。语音连接不依赖此权限。")
                    HelpParagraph("授权未生效", "在系统设置的辅助功能中开启 ThinCube。若开关已开启但仍未生效，请关闭再开启该项，并退出重启 ThinCube。")
                }
            }
            Section {
                LabeledContent("ThinCube", value: Bundle.main.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String ?? "")
                if model.connected { LabeledContent("设备固件", value: model.version) }
            } header: {
                HelpSectionHeader("关于", helpTitle: "关于 ThinCube") {
                    HelpParagraph("ThinCube", "连接 StickS3 设备，在 Mac 上使用语音输入和设备快捷键。Wi-Fi 与蓝牙共用一次应用配对。")
                    HelpParagraph("Clawd", "设备上的电子伙伴，活泼、好奇、勇于探索，用念力控制对讲机。")
                }
            }
        }
        .formStyle(.grouped)
        .onAppear { refreshPermissions() }
        .onReceive(NotificationCenter.default.publisher(for: NSApplication.didBecomeActiveNotification)) { _ in
            refreshPermissions()
        }
    }
    private func refreshPermissions() {
        model.keyDiagnostics.refreshPermission()
        accessibilityTrusted = model.keyDiagnostics.accessibilityAllowed
        postingAllowed = model.keyDiagnostics.postingAllowed
    }
    private var device: some View {
        Form {
            Section {
                Picker("连接模式", selection: Binding(get: { model.connectionMode }, set: { mode in Task { await model.selectConnectionMode(mode) } })) {
                    ForEach(ConnectionMode.allCases, id: \.self) { Text($0.label).tag($0) }
                }.pickerStyle(.menu).disabled(!model.canSelectConnectionMode)
                if !model.deviceOptions.isEmpty {
                    Picker("设备", selection: Binding(get: { model.selectedDeviceID }, set: { model.selectDevice($0) })) {
                        if model.selectedDeviceID.isEmpty { Text("请选择设备").tag("") }
                        ForEach(model.deviceOptions) { Text($0.label).tag($0.id) }
                    }.pickerStyle(.menu).disabled(model.connected || model.connecting || model.switching || model.updating)
                } else {
                    Text("正在查找 ThinCube…").foregroundStyle(.secondary)
                }
                if model.connected {
                    Label("已连接 · " + model.transportLabel, systemImage: "checkmark.circle.fill").foregroundStyle(.green)
                }
                if model.switching { ProgressView(model.transportPhase) }
                else if !model.transportPhase.isEmpty { Text(model.transportPhase).font(.callout).foregroundStyle(.secondary) }
                else if model.connecting { ProgressView(model.connectionProgress.stage.label) }
                else if model.reconnecting { ProgressView("正在等待设备重新连接…") }
                if model.connecting || model.connected {
                    ConnectionSteps(stage: model.connectionProgress.stage, transport: model.transportLabel)
                }
                HStack {
                    Button(model.connected || model.connecting || model.reconnecting ? "断开" : "连接") {
                        if model.connected || model.connecting || model.reconnecting { model.disconnect() }
                        else { Task { await model.connect() } }
                    }.disabled(model.updating)
                    if model.connectionMode == .wifi && !model.connected {
                        Spacer()
                        Button("通过 IP 连接…") { activeSheet = .manualAddress }
                            .disabled(model.connecting || model.switching || model.updating)
                    }
                }
                if model.pairingRequired {
                    Label("信道已连接，等待设备配对", systemImage: "exclamationmark.circle.fill").foregroundStyle(.orange)
                    Button("完成配对…") { presentPairingIfNeeded() }
                }
            } header: {
                HStack {
                    Text("设备连接")
                    Spacer()
                    HelpPopover(title: "连接帮助") {
                        HelpParagraph("连接模式", "自动模式会跟随设备当前的 Wi-Fi 或蓝牙方式。已连接时选择 Wi-Fi 或蓝牙，会同步切换设备。")
                        HelpParagraph("首次配对", "先连接信道，再由弹窗引导输入设备 PAIR PIN。蓝牙无需系统验证码；若 macOS 显示配对确认，请点击配对。有效配对会自动复用。")
                        HelpParagraph("设备列表", "已发现表示当前扫描或局域网发现的设备；上次选择但暂未发现属于历史记录。系统忽略蓝牙设备后，仍在广播的设备仍可被发现，ThinCube 的应用授权也会保留。")
                        HelpParagraph("首次连接", "设备首次开机可选 A 蓝牙或 B Wi-Fi 热点。蓝牙无需先配网；选择热点时，按屏幕密码连接 ThinCube-Setup，在 192.168.4.1 配置网络，再在同一局域网配对。")
                        HelpParagraph("恢复出厂设置", "设备固件 v0.4.38 起，按住 B，再单击电源/复位键，松开 B 后进入恢复出厂确认页。A 确认清空全部设备配置，包括 Wi-Fi、蓝牙绑定、应用配对、校准、亮度和声音，重启后重新选择连接方式；B 取消并保留原配置。较早固件的此入口仅重设网络和应用授权。")
                        HelpParagraph("设备按键", "对讲机内 B 为自定义快捷键，长按不切换连接。其他模式短按 B 静音，长按 B 3 秒并松开切换模式。所选模式会保存，未连接也不会回退。空闲时同时按住 A+B 3 秒并松开可重新配对。")
                        HelpParagraph("电量与休眠", "红色 LOW 表示电压持续偏低，请及时充电。插拔电源后短暂显示 --% 是重新采样。默认静置五分钟息屏，电池供电下十分钟关机。开启深睡后，电池供电下五分钟进入深睡，拿起或按机身电源键唤醒后自动重连。")
                    }
                }
            }
            Section {
                Group {
                    HStack {
                        Button("竖向倒立，A 键在上 → 校准") { Task { await model.calibrate("calibrate_inverted") } }.disabled(model.calibrationBusy)
                        Image(systemName: model.status?.calibrated == true ? "checkmark.circle.fill" : "circle")
                    }
                    HStack {
                        Button("屏幕朝上平放 → 校准") { Task { await model.calibrate("calibrate_faceup") } }.disabled(model.calibrationBusy)
                        Image(systemName: model.status?.faceCalibrated == true ? "checkmark.circle.fill" : "circle")
                    }
                    Text("当前姿态：" + (model.status?.poseDescription ?? "等待设备连接"))
                }.disabled(!model.connected || model.updating || model.switching)
            } header: {
                HStack {
                    Text("姿态校准")
                    Spacer()
                    HelpPopover(title: "校准帮助") {
                        HelpParagraph("首次使用校准一次", "竖向倒立时让屏幕面向你、A 键在上、显示屏在下，USB 线从顶部引出；平放校准时屏幕朝上。")
                        HelpParagraph("开始语音", "校准后先屏幕朝上平放，再倒立并稍作停稳即可进入语音。屏幕朝下平放会进入休息。")
                    }
                }
            }
            Section("设备设置") {
                SettingsSliderRow(title: "亮度", value: $model.brightness, range: 8...255, step: 1,
                                  valueLabel: "\(Int(model.brightness))") {
                    Task { await model.apply(["brightness": Int(model.brightness)]) }
                }
                Toggle("运行时常亮（调试）", isOn: Binding(get: { model.status?.powerLedDebug ?? false }, set: { model.setPowerLedDebug($0) }))
                    .disabled(model.powerLedSettingBusy || model.status?.powerLedDebug == nil || model.status?.powerLedAvailable != true)
                Text(model.status?.powerLedDebug == nil ? "此设置需要升级设备固件。" : "默认自动：启动期间亮灯，本机就绪并稳定运行约 2 秒后熄灭，无需等待联网或连接 Mac。开启此项会在运行期间保持亮灯，设备会记住选择。")
                    .font(.callout).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
                Toggle("深睡待机", isOn: Binding(get: { model.deepSleepEnabled }, set: { model.setDeepSleep($0) }))
                    .disabled(model.deepSleepSettingBusy || model.status?.deepSleepAvailable != true)
                Text("开启后，使用电池静置五分钟会断开连接并进入深睡；拿起或按机身电源键唤醒后自动重连。USB供电时保持在线。深睡期间A/B键和Mac指令不能唤醒。需要完全关机时，先唤醒并关闭此开关。")
                    .font(.callout).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
                if model.deviceSleeping {
                    Label("设备正在深睡，等待拿起或电源键唤醒", systemImage: "moon.zzz.fill")
                }
            }.disabled(!model.connected || model.updating || model.switching)
            if model.connected || !model.networkConfigurationNote.isEmpty {
                Section {
                    if model.connected {
                        Button("配置 Wi-Fi…") { activeSheet = .changingNetwork }
                            .disabled(!model.canConfigureWiFi)
                        if model.currentTransport == "ble" && model.status?.wifiProvisioning != true {
                            Text("蓝牙配网需要升级设备固件。").font(.callout).foregroundStyle(.secondary)
                        }
                    }
                    if !model.networkConfigurationNote.isEmpty { Text(model.networkConfigurationNote).font(.callout).foregroundStyle(.secondary) }
                } header: {
                    HelpSectionHeader("Wi-Fi 网络", helpTitle: "网络帮助") {
                        HelpParagraph("配置网络", "通过已配对的蓝牙或 Wi-Fi 连接提交新网络。验证成功后使用新 Wi-Fi，Mac 接入同一局域网后自动恢复，无需重新配对。")
                        HelpParagraph("验证失败", "新版设备会恢复原连接和原网络。配置期间停止语音输入；暂时断开属于正常验证过程。")
                    }
                }
            }
        }.formStyle(.grouped)
            .onChange(of: model.connectionMode) { _ in if activeSheet == .manualAddress { activeSheet = nil } }
            .onChange(of: model.connected) { connected in
                if connected && activeSheet == .manualAddress { activeSheet = nil }
                if !connected && activeSheet == .changingNetwork { activeSheet = nil }
            }
            .onChange(of: model.switching) { if $0 && activeSheet == .changingNetwork { activeSheet = nil } }
            .onChange(of: model.updating) { if $0 && activeSheet == .changingNetwork { activeSheet = nil } }
    }
    private var audio: some View {
        Form {
            Section {
                Group {
                    Toggle("允许语音输入", isOn: Binding(get: { model.status?.allowed ?? false }, set: { value in Task { await model.apply(["allowed": value]) } }))
                    Toggle("麦克风静音", isOn: Binding(get: { model.status?.muted ?? false }, set: { value in Task { await model.apply(["muted": value]) } }))
                    Toggle("声音互动", isOn: Binding(get: { model.status?.localSound ?? true }, set: { value in Task { await model.apply(["localSound": value]) } }))
                    SettingsSliderRow(title: "输入增益", value: $model.gain, range: 0.1...4, step: 0.1,
                                      valueLabel: String(format: "%.1f×", model.gain)) {
                        Task { await model.apply(["gain": model.gain]) }
                    }
                    HStack {
                        Text("输入电平")
                        ProgressView(value: model.status?.level ?? 0).accessibilityLabel("麦克风音量")
                    }
                    HStack {
                        if model.status?.streaming == true {
                            Button("停止语音") { Task { await model.command("stop") } }
                        } else {
                            Button("开始持续语音") { Task { await model.command("start") } }
                                .disabled(!model.audioPrepared || model.status?.allowed != true || model.status?.muted == true)
                        }
                        Spacer()
                        Label(model.status?.streaming == true ? "正在传音" : "待命", systemImage: "mic")
                            .foregroundStyle(model.status?.streaming == true ? .green : .secondary)
                    }
                    Text(model.connected ? (model.status?.voiceHint ?? "等待设备状态") : "请先连接设备")
                        .font(.callout).foregroundStyle(.secondary)
                    if model.connected && !model.audioPrepared {
                        Text("Mac 音频输入尚未就绪，请在下方准备音频。").font(.callout).foregroundStyle(.orange)
                    }
                }.disabled(!model.connected || model.updating || model.switching)
            } header: {
                HelpSectionHeader("语音输入", helpTitle: "语音帮助") {
                    HelpParagraph("语音与静音", "允许语音输入后，设备可向 Mac 传音。麦克风静音会真正停止采集；开始持续语音会保持传音，直到点击停止。")
                    HelpParagraph("姿态与按键", "完成校准后，屏幕朝上平放，再倒立并稍作停稳可开始传音。对讲机内 A/B 只控制各自快捷键，松开不会停止持续语音。")
                    HelpParagraph("声音互动", "设备在空闲时可根据环境声音作出反应，此开关与向 Mac 传音、扬声器播放分别控制。")
                    HelpParagraph("输入增益", "调整麦克风输入幅度后点击保存。")
                }
            }
            Section {
                if model.status?.soundVolume != nil {
                    Group {
                        Toggle("扬声器静音", isOn: Binding(get: { model.status?.soundMuted ?? false }, set: { value in Task { await model.apply(["soundMuted": value]) } }))
                        SettingsSliderRow(title: "扬声器音量", value: $model.soundVolume, range: 0...100, step: 1,
                                          valueLabel: "\(Int(model.soundVolume))%")
                            .onChange(of: model.soundVolume) { value in model.scheduleSoundVolume(Int(value)) }
                        Button("试听声音") { Task { await model.command("sound_preview") } }
                            .disabled(model.status?.soundMuted == true || model.soundVolume == 0 || model.status?.streaming == true)
                        if model.status?.streaming == true {
                            Text("正在传音，扬声器播放已暂停。").font(.callout).foregroundStyle(.secondary)
                        }
                    }.disabled(!model.connected || model.updating || model.switching)
                } else {
                    Text(model.connected ? "此设备固件尚不支持声音设置。" : "连接设备后可调整声音设置。")
                        .foregroundStyle(.secondary)
                }
            } header: {
                HelpSectionHeader("设备声音", helpTitle: "设备声音帮助") {
                    HelpParagraph("设备扬声器", "设备扬声器播放唤醒、互动、扫描和发现等声音。扬声器静音控制设备外放；音量设为 0% 也会停止播放。")
                    HelpParagraph("保存与传音", "扬声器音量自动保存到设备，断开 Mac 后仍生效。传音时暂停扬声器播放。")
                }
            }
            Section {
                Label(!model.connected ? "连接设备后准备音频输入" : (model.audioPrepared ? "BlackHole 2ch 已就绪" : "Mac 音频输入尚未就绪"),
                      systemImage: model.audioPrepared ? "checkmark.circle.fill" : "exclamationmark.circle")
                    .foregroundStyle(model.audioPrepared ? .green : .secondary)
                HStack {
                    if model.connected && !model.audioPrepared {
                        Link("安装 BlackHole…", destination: URL(string: "https://github.com/ExistentialAudio/BlackHole")!)
                        Button("准备音频") { model.prepareAudio() }
                            .disabled(!model.connected || model.updating || model.switching)
                    } else if model.connected && model.audioPrepared {
                        Button("设为系统输入") { model.selectSystemInput() }
                            .disabled(!model.connected || model.updating || model.switching)
                    }
                    Spacer()
                    Button("音频诊断…") { activeSheet = .audioDiagnostics }
                }
            } header: {
                HelpSectionHeader("Mac 音频输入", helpTitle: "音频输入帮助") {
                    HelpParagraph("BlackHole 2ch", "ThinCube 通过 BlackHole 2ch 虚拟音频驱动向其他 Mac 应用提供音频，采样率为 48 kHz。安装驱动后连接设备，点击准备音频。")
                    HelpParagraph("选择输入", "可在语音、会议或录音软件中直接选择 BlackHole 2ch；设为系统输入会修改 Mac 的默认输入设备。")
                    HelpParagraph("诊断", "音频诊断显示接收、缓冲与丢帧统计，不会开始录音。")
                }
            }
        }.formStyle(.grouped)
    }
    private var firmware: some View {
        Form {
            Section {
                if model.connected {
                    LabeledContent("设备固件", value: model.version)
                    if model.status?.bootVerified != true {
                        ProgressView("正在检查设备启动状态…")
                    }
                }
                if model.updating {
                    ProgressView(value: model.updateProgress)
                    Text("正在升级，请保持设备供电与网络连接。").font(.callout).foregroundStyle(.secondary)
                } else if model.switching {
                    ProgressView(model.transportPhase)
                } else if model.connecting || model.reconnecting {
                    ProgressView("等待设备连接…")
                } else if !model.connected {
                    Text("连接设备后可升级固件。").foregroundStyle(.secondary)
                    Button("连接设备") { model.pane = .device }
                } else if model.currentTransport == "ble" {
                    Text("固件升级需要 Wi-Fi 连接。").foregroundStyle(.secondary)
                    Button("切换至 Wi-Fi") { Task { await model.selectConnectionMode(.wifi) } }
                        .disabled(!model.canSwitchTransport)
                } else {
                    Button("选择固件并升级…") { model.chooseFirmware() }
                        .disabled(model.switching || model.connecting || model.status?.bootVerified != true)
                }
            } header: {
                HelpSectionHeader("固件升级", helpTitle: "固件升级帮助") {
                    HelpParagraph("升级条件", "通过 Wi-Fi 选择 ThinCube 应用固件 .bin，无需 USB。设备电量须至少 50%，且当前固件的启动健康检查已通过。")
                    HelpParagraph("设备确认", "开始前会停止语音并释放快捷键。在设备上按 A 确认升级，按 B 取消；升级期间保持供电与网络连接。")
                    HelpParagraph("校验与恢复", "升级会校验型号、尺寸及文件完整性，写入备用应用槽。新版本稳定运行 60 秒后确认健康；设备重启后自动恢复连接。")
                }
            }
        }.formStyle(.grouped)
    }

}

private struct ManualWiFiConnectionSheet: View {
    @ObservedObject var model: DeviceModel
    @Environment(\.dismiss) private var dismiss
    @State private var address: String
    @State private var error: String?
    @FocusState private var addressFocused: Bool
    init(model: DeviceModel) {
        self.model = model
        _address = State(initialValue: model.host.hasPrefix("ble://") ? "" : model.host)
    }
    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            Text("通过 IP 连接").font(.title2.bold())
            Text("自动发现不到设备时，输入设备的局域网 IP 或 .local 名称。Mac 与设备需在同一局域网。")
                .foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
            VStack(alignment: .leading, spacing: 6) {
                Text("设备地址").fontWeight(.medium)
                TextField("例如 192.168.0.202 或 ThinCube-xxxxxx.local", text: $address)
                    .textFieldStyle(.roundedBorder).focused($addressFocused)
                    .onSubmit { connect() }
                    .onChange(of: address) { _ in error = nil }
                if let error { Text(error).font(.callout).foregroundStyle(.red) }
            }
            HStack {
                Spacer()
                Button("取消") { dismiss() }.keyboardShortcut(.cancelAction)
                Button("连接") { connect() }.keyboardShortcut(.defaultAction)
                    .disabled(address.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty || model.connectionMode != .wifi || model.connected || model.connecting || model.switching || model.updating)
            }
        }.padding(24).frame(width: 420).onAppear { addressFocused = true }
    }
    private func connect() {
        guard model.connectionMode == .wifi, !model.connected, !model.connecting, !model.switching, !model.updating else { return }
        let value = address.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !value.isEmpty else { return }
        if let message = model.wifiAddressError(value) { error = message; return }
        dismiss()
        Task { await model.connectWiFi(at: value) }
    }
}
