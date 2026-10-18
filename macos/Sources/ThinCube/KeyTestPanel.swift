import AppKit
import SwiftUI
import ThinCubeCore

struct KeyTestPanel: View {
    @ObservedObject var model: DeviceModel
    @ObservedObject private var diagnostics: KeyDiagnostics

    init(model: DeviceModel) {
        self.model = model
        diagnostics = model.keyDiagnostics
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 14) {
            HStack {
                Text("按键诊断").font(.system(size: 18, weight: .semibold))
                Spacer()
                HelpPopover(title: "按键诊断帮助") {
                    HelpParagraph("测试方式", "设备事件会自动记录。测试当前快捷键会向 ThinCube 窗口发送一次按下与松开，无需连接设备。")
                    HelpParagraph("本窗口测试", "开始后可按设备 A/B 或 Mac 键盘查看事件；此时键盘操作会被窗口接收，Esc 停止测试。关闭诊断或切换窗口也会停止。")
                    HelpParagraph("日志", "保留最近 100 条事件，新事件在上方。已提交表示发往系统，系统回读表示本窗口实际收到。")
                    HelpParagraph("权限", "若辅助功能开关已开启而键盘控制仍未生效，请关闭再开启 ThinCube 授权，并退出重启应用。")
                }
            }
            HStack {
                Label(diagnostics.postingAllowed ? "键盘控制权限已生效" : "键盘控制权限未生效",
                      systemImage: diagnostics.postingAllowed ? "checkmark.circle.fill" : "exclamationmark.circle")
                    .foregroundStyle(diagnostics.postingAllowed ? .green : .orange)
                Spacer()
                if !diagnostics.postingAllowed {
                    Button("打开系统设置…") {
                        NSWorkspace.shared.open(URL(string: "x-apple.systempreferences:com.apple.preference.security?Privacy_Accessibility")!)
                    }
                }
            }
            Text("辅助功能：\(diagnostics.accessibilityAllowed ? "允许" : "拒绝") · 事件发送：\(diagnostics.postingAllowed ? "允许" : "拒绝")")
                .font(.callout).foregroundStyle(.secondary)
            Text(context).font(.callout).foregroundStyle(.secondary)
                .fixedSize(horizontal: false, vertical: true)
            HStack(spacing: 10) {
                Button(diagnostics.monitoring ? "停止本窗口测试" : "开始本窗口测试") {
                    if diagnostics.monitoring { model.stopKeyboardTest() }
                    else { diagnostics.start() }
                }.disabled(model.shortcutRecording || model.updating || model.connecting)
                Button("测试 A 快捷键") { model.testCurrentShortcut() }
                    .disabled(!diagnostics.postingAllowed || model.shortcutTestRunning || model.shortcutRecording || model.updating || model.connecting)
                Button("测试 B 快捷键") { model.testCurrentShortcut(button: .b) }
                    .disabled(model.shortcutB == nil || !diagnostics.postingAllowed || model.shortcutTestRunning || model.shortcutRecording || model.updating || model.connecting)
                if diagnostics.monitoring { Text("测试中 · Esc 停止").foregroundStyle(.orange) }
            }
            HStack {
                Text("最近 \(diagnostics.entries.count) 条事件").font(.caption).foregroundStyle(.secondary)
                Spacer()
                Button("复制日志") { diagnostics.copy() }.disabled(diagnostics.entries.isEmpty)
                Button("清空") { diagnostics.clear() }.disabled(diagnostics.entries.isEmpty)
            }
            ScrollView {
                LazyVStack(alignment: .leading, spacing: 6) {
                    if diagnostics.entries.isEmpty { Text("暂无事件").foregroundStyle(.secondary) }
                    ForEach(diagnostics.entries) { entry in
                        Text(entry.line)
                            .foregroundStyle(entry.source == "系统回读" ? Color.green : Color.primary)
                            .frame(maxWidth: .infinity, alignment: .leading)
                    }
                }
                .font(.system(size: 10.5, design: .monospaced)).textSelection(.enabled)
                .padding(10).frame(maxWidth: .infinity, alignment: .leading)
            }
            .frame(height: 180)
            .background(.black.opacity(0.12), in: RoundedRectangle(cornerRadius: 6))
        }
        .buttonStyle(.bordered).controlSize(.small)
        .onAppear { diagnostics.refreshPermission() }
        .onReceive(Timer.publish(every: 1, on: .main, in: .common).autoconnect()) { _ in diagnostics.refreshPermission() }
        .onDisappear { model.stopKeyboardTest() }
    }

    private var context: String {
        let status = model.status
        let connection = model.connected ? "在线" : "未连接"
        let audio = status?.streaming == true ? "传音中" : "未传音"
        let held = status?.held == true ? "按住" : "松开"
        let editing = model.shortcutRecording ? " · 快捷键编辑中，暂停注入" : ""
        let heldB = status?.heldB == true ? "按住" : "松开"
        return "设备：\(connection) · \(audio) · A：\(held) · B：\(heldB)\(editing)\n姿态：\(status?.poseDescription ?? "等待设备状态")"
    }
}
