import SwiftUI
import AppKit
import UniformTypeIdentifiers
import ThinCubeCore

@main struct ThinCubeApp: App {
    @StateObject private var model = DeviceModel()
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
        WindowGroup("ThinCube") { MainView(model: model, discovery: model.discovery).frame(minWidth: 780, minHeight: 500) }
        MenuBarExtra {
            Text(model.name)
            Text(model.status?.streaming == true ? "LIVE · 正在传音" : "语音未传输")
            Button(model.status?.muted == true ? "取消静音" : "停止麦克风并静音") { Task { await model.apply(["muted": model.status?.muted != true]) } }.disabled(!model.connected)
            Button(model.status?.soundMuted == true ? "开启 Clawd 音效" : "静音 Clawd 音效") { Task { await model.apply(["soundMuted": model.status?.soundMuted != true]) } }.disabled(!model.connected || model.status?.soundVolume == nil)
            Button("停止语音输入") { Task { await model.command("stop") } }.disabled(!model.connected)
            Divider()
            Button("按键与快捷键…") { model.pane = .keys; NSApp.activate(ignoringOtherApps: true); NSApp.windows.first?.makeKeyAndOrderFront(nil) }
            Button("显示 ThinCube") { NSApp.activate(ignoringOtherApps: true); NSApp.windows.first?.makeKeyAndOrderFront(nil) }
            Button("退出") { model.shutdown(); NSApp.terminate(nil) }
        } label: {
            Image(nsImage: Self.menuBarIcon).accessibilityLabel("ThinCube")
        }
    }
}

enum MainPane: String, CaseIterable, Identifiable {
    case device, keys, voice, firmware, settings
    var id: String { rawValue }
    var title: String {
        switch self {
        case .device: return "设备"
        case .keys: return "按键"
        case .voice: return "语音"
        case .firmware: return "固件"
        case .settings: return "设置"
        }
    }
    var icon: String {
        switch self {
        case .device: return "rectangle.portrait"
        case .keys: return "keyboard"
        case .voice: return "mic"
        case .firmware: return "arrow.down.circle"
        case .settings: return "gearshape"
        }
    }
}

struct MainView: View {
    @ObservedObject var model: DeviceModel
    @ObservedObject var discovery: Discovery
    @State private var hoveredPane: MainPane?
    @State private var accessibilityTrusted = false
    @State private var manualAddress = false
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
                    Label(model.connected ? "在线 · " + model.name : (model.connecting ? "连接中" : (model.reconnecting ? "等待自动重连" : "未连接")),
                          systemImage: model.connected ? (model.currentTransport == "ble" ? "antenna.radiowaves.left.and.right" : "wifi") : "wifi.slash")
                        .font(.system(size: 11.5)).foregroundStyle(model.connected ? .green : .secondary)
                }
            }
        }
        .tint(amber)
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
                paneHeader("按键", subtitle: "倒置语音模式中的正面 A 键会被注入为下面的快捷键。支持录制或直接选择，设置完成即生效。")
                ShortcutEditor(model: model).disabled(model.updating || model.connecting)
                KeyTestPanel(model: model)
            }
            .padding(.horizontal, 28).padding(.top, 24).padding(.bottom, 30)
            .frame(maxWidth: .infinity, alignment: .leading)
        }
    }

    private var settings: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 20) {
                paneHeader("设置", subtitle: "权限状态。")
                HStack(spacing: 12) {
                    VStack(alignment: .leading, spacing: 3) {
                        Text("辅助功能").font(.system(size: 13))
                        Text("用于把设备按键注入到当前应用").font(.system(size: 11.5)).foregroundStyle(.secondary)
                    }
                    Spacer()
                    Text(accessibilityTrusted ? "已授权" : "未授权").font(.system(size: 13))
                        .foregroundStyle(accessibilityTrusted ? .green : .orange)
                    if !accessibilityTrusted {
                        Button("打开系统设置…") {
                            NSWorkspace.shared.open(URL(string: "x-apple.systempreferences:com.apple.preference.security?Privacy_Accessibility")!)
                        }
                    }
                }
                .padding(14).background(Color.white.opacity(0.05), in: RoundedRectangle(cornerRadius: 10))
            }
            .padding(.horizontal, 28).padding(.top, 24).padding(.bottom, 30)
            .frame(maxWidth: .infinity, alignment: .leading)
        }
        .onAppear { accessibilityTrusted = model.keys.trusted }
        .onReceive(NotificationCenter.default.publisher(for: NSApplication.didBecomeActiveNotification)) { _ in
            accessibilityTrusted = model.keys.trusted
        }
    }

    private func paneHeader(_ title: String, subtitle: String) -> some View {
        VStack(alignment: .leading, spacing: 5) {
            Text(title).font(.system(size: 21, weight: .bold))
            Text(subtitle).font(.system(size: 12.5)).foregroundStyle(.secondary)
                .fixedSize(horizontal: false, vertical: true)
        }
    }
    private var device: some View {
        Form {
            Section("设备连接") {
                Picker("连接模式", selection: Binding(get: { model.connectionMode }, set: { mode in Task { await model.selectConnectionMode(mode) } })) {
                    ForEach(ConnectionMode.allCases, id: \.self) { Text($0.label).tag($0) }
                }.pickerStyle(.menu).disabled(!model.canSelectConnectionMode)
                Text(model.connectionMode == .automatic
                     ? "跟随设备当前的连接方式；设备切换 Wi-Fi 或蓝牙后会自动重新连接。"
                     : "已连接时选择其他模式，会同步切换设备的连接方式。")
                    .font(.callout).foregroundStyle(.secondary)
                if !model.discoveredDeviceIDs.isEmpty {
                    Picker("设备", selection: Binding(get: { model.selectedDeviceID }, set: { model.selectDevice($0) })) {
                        if model.selectedDeviceID.isEmpty { Text("请选择设备").tag("") }
                        ForEach(model.discoveredDeviceIDs, id: \.self) { Text($0).tag($0) }
                    }.pickerStyle(.menu).disabled(model.connected || model.connecting || model.switching || model.updating)
                } else {
                    Text("正在查找 ThinCube…").foregroundStyle(.secondary)
                }
                if model.connected {
                    Label("已连接 · " + model.transportLabel, systemImage: "checkmark.circle.fill").foregroundStyle(.green)
                }
                if model.switching { ProgressView(model.transportPhase) }
                else if !model.transportPhase.isEmpty { Text(model.transportPhase).font(.callout).foregroundStyle(.secondary) }
                else if model.connecting { ProgressView(model.pairingRequired ? "等待设备授权" : "正在连接…") }
                else if model.reconnecting { ProgressView("正在等待设备重新连接…") }
                HStack {
                    Button(model.connected || model.connecting || model.reconnecting ? "断开" : "连接") {
                        if model.connected || model.connecting || model.reconnecting { model.disconnect() }
                        else { Task { await model.connect() } }
                    }.disabled(model.updating)
                }
                if model.pairingRequired {
                    VStack(alignment: .leading, spacing: 8) {
                        Text("设备授权").fontWeight(.semibold)
                        Text(model.pairingOverBluetooth
                             ? "系统蓝牙配对已完成。现在请输入设备 PAIR PIN 页面上的授权码。"
                             : "请输入设备 PAIR PIN 页面上的授权码；授权保存后，下次直接连接。")
                            .font(.callout).foregroundStyle(.secondary)
                        HStack {
                            SecureField("六位设备授权码", text: $model.pin).frame(width: 160)
                                .onSubmit { model.submitPairingCode() }
                            Button("授权并连接") { model.submitPairingCode() }.disabled(!model.canSubmitPairingCode)
                        }
                        Text("未显示 PAIR PIN 时，请在设备空闲时同时按住 A+B 3 秒并松开，再重新连接。")
                            .font(.callout).foregroundStyle(.secondary)
                    }.padding(.vertical, 4)
                }
                if model.connectionMode != .wifi || model.currentTransport == "ble" {
                    Text("蓝牙首次连接：在 macOS 系统弹窗中输入设备 BLUETOOTH PIN 页面上的六位码。已保存的 ThinCube 授权会自动复用。")
                        .font(.callout).foregroundStyle(.secondary)
                }
                if model.connectionMode != .ble {
                    DisclosureGroup("手动输入 Wi-Fi 地址", isExpanded: $manualAddress) {
                        TextField("设备 IP / .local", text: $model.host)
                            .disabled(model.connected || model.connecting || model.switching || model.updating)
                    }
                }
                DisclosureGroup("首次配网与设备按键") {
                    Text("Wi-Fi 首次使用：连接 ThinCube-Setup 热点，按设备屏幕上的密码登录，在 192.168.4.1 配置 Wi-Fi。让 Mac 与设备加入同一局域网。")
                    Text("短按 B 静音；单独长按 B 3 秒后松开切换 Wi-Fi / 蓝牙。自动模式会跟随设备；切换过程中请保持设备供电。")
                }.font(.callout).foregroundStyle(.secondary)
            }
            Section("姿态校准 · 首次使用一次") {
                HStack {
                    Button("竖向倒立，A 键在上 → 校准") { Task { await model.calibrate("calibrate_inverted") } }.disabled(model.calibrationBusy)
                    Image(systemName: model.status?.calibrated == true ? "checkmark.circle.fill" : "circle")
                }
                HStack {
                    Button("屏幕朝上平放 → 校准") { Task { await model.calibrate("calibrate_faceup") } }.disabled(model.calibrationBusy)
                    Image(systemName: model.status?.faceCalibrated == true ? "checkmark.circle.fill" : "circle")
                }
                Text("竖向校准时让屏幕面向你、A 键在上、显示屏在下，USB 线从顶部引出；平放校准时屏幕朝上。校准后先屏幕朝上平放，再倒立并稍作停稳即可进入语音。屏幕朝下平放会进入休息。")
                    .font(.callout).foregroundStyle(.secondary)
                Text("当前姿态：" + (model.status?.poseDescription ?? "等待设备连接"))
            }.disabled(!model.connected || model.updating || model.switching)
            Section("设备设置") {
                HStack { Text("亮度"); Slider(value: $model.brightness, in: 8...255, step: 1); Button("保存") { Task { await model.apply(["brightness": Int(model.brightness)]) } } }
                TextField("新的 Wi-Fi 名称", text: $model.wifiSSID)
                SecureField("新的 Wi-Fi 密码（开放网络留空）", text: $model.wifiPassword)
                Button("验证并切换 Wi-Fi") { Task { await model.changeWifi() } }.disabled(model.wifiSSID.isEmpty || model.currentTransport == "ble")
            }.disabled(!model.connected || model.updating || model.switching)
        }.formStyle(.grouped)
    }
    private var audio: some View {
        Form {
            Section("声音与语音") {
                Toggle("允许设备向 Mac 输入语音", isOn: Binding(get: { model.status?.allowed ?? false }, set: { value in Task { await model.apply(["allowed": value]) } }))
                Toggle("静音 · 真正停止麦克风采集", isOn: Binding(get: { model.status?.muted ?? false }, set: { value in Task { await model.apply(["muted": value]) } }))
                Toggle("本地声音互动", isOn: Binding(get: { model.status?.localSound ?? true }, set: { value in Task { await model.apply(["localSound": value]) } }))
                HStack { Text("输入增益"); Slider(value: $model.gain, in: 0.1...4, step: 0.1); Text(String(format: "%.1f×", model.gain)); Button("保存") { Task { await model.apply(["gain": model.gain]) } } }
                ProgressView(value: model.status?.level ?? 0).accessibilityLabel("麦克风音量")
                HStack {
                    Button("开始持续语音") { Task { await model.command("start") } }.disabled(!model.audioPrepared || model.status?.allowed != true || model.status?.muted == true)
                    Button("停止语音") { Task { await model.command("stop") } }
                    Spacer(); Text(model.status?.streaming == true ? "LIVE" : "待命").foregroundStyle(model.status?.streaming == true ? .green : .secondary)
                }
                Text("当前姿态：" + (model.status?.poseDescription ?? "等待设备连接"))
                Text(model.status?.voiceHint ?? "请先连接设备").font(.callout).foregroundStyle(.secondary)
            }.disabled(!model.connected || model.updating || model.switching)
            Section("Clawd 音效") {
                if model.status?.soundVolume != nil {
                    Toggle("音效静音", isOn: Binding(get: { model.status?.soundMuted ?? false }, set: { value in Task { await model.apply(["soundMuted": value]) } }))
                    HStack {
                        Text("音效音量")
                        Slider(value: $model.soundVolume, in: 0...100, step: 1)
                            .accessibilityLabel("Clawd 音效音量")
                            .onChange(of: model.soundVolume) { value in
                                model.scheduleSoundVolume(Int(value))
                            }
                        Text("\(Int(model.soundVolume))%").monospacedDigit().frame(width: 40)
                    }
                    Button("试听音效") { Task { await model.command("sound_preview") } }
                        .disabled(model.status?.soundMuted == true || model.soundVolume == 0 || model.status?.streaming == true)
                    Text(model.status?.streaming == true ? "正在传音，音效暂停。" : "短电子音会配合唤醒、互动、扫描和发现。设置保存在设备上，断开 Mac 后仍生效。")
                        .font(.callout).foregroundStyle(.secondary)
                    Text("音效静音只控制喇叭；语音输入和本地声音互动分别由上方开关控制。音量 0% 也会停止音效。")
                        .font(.callout).foregroundStyle(.secondary)
                } else {
                    Text(model.connected ? "升级设备至 v0.3.9 后可控制音效。" : "连接设备后可调整音效音量与静音。")
                        .foregroundStyle(.secondary)
                }
            }.disabled(!model.connected || model.updating || model.switching)
            Section("Mac 音频输入") {
                Text(model.audioPrepared ? "BlackHole 2ch · 48 kHz · 已就绪" : "需要 BlackHole 2ch 虚拟音频驱动")
                HStack {
                    Link("安装说明", destination: URL(string: "https://github.com/ExistentialAudio/BlackHole")!)
                    Button("准备音频") { model.prepareAudio() }.disabled(!model.connected)
                    Button("设为系统输入") { model.selectSystemInput() }.disabled(!model.audioPrepared)
                }
                Text("也可在语音、会议或录音软件中直接选择 BlackHole 2ch。A 键控制快捷键，持续音频不受 A 键松开影响。")
                    .font(.callout).foregroundStyle(.secondary)
                DisclosureGroup("音频调试") { Text(model.audioDiagnostics).font(.caption).textSelection(.enabled) }
            }
        }.formStyle(.grouped)
    }
    private var firmware: some View {
        Form {
            Section("Wi-Fi OTA") {
                if model.connected { Text(model.status?.bootVerified == true ? "当前固件：启动健康检查已通过" : "当前固件：启动健康检查中（至少 60 秒）") }
                Text("选择 ThinCube 的应用固件 .bin。设备按 A 确认后，无需插线即可升级；电量须至少 50%。")
                Button("选择固件并升级") { model.chooseFirmware() }.disabled(!model.connected || model.updating || model.switching || model.currentTransport == "ble")
                if model.currentTransport == "ble" { Text("升级前请在设备连接页切换至 Wi-Fi。").font(.callout).foregroundStyle(.secondary) }
                if model.updating { ProgressView(value: model.updateProgress); Text("请保持设备供电与网络连接。") }
                Text("升级前会停止语音并释放快捷键。固件经过设备型号、尺寸、SHA-256 与 ESP 镜像校验后才切换 A/B 槽；新版本稳定运行 60 秒后通过健康确认。")
                    .font(.callout).foregroundStyle(.secondary)
            }
            Section("Clawd") {
                Text("活泼、好奇、勇于探索的极客。没有嘴巴和腮红，用念力控制对讲机。")
                Text("待命红灯 · A 按住传话绿灯 · 松开恢复红灯")
            }
        }.formStyle(.grouped)
    }
}
