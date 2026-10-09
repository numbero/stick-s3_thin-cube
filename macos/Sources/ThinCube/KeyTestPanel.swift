import AppKit
import SwiftUI

struct KeyTestPanel: View {
    @ObservedObject var model: DeviceModel
    @ObservedObject private var diagnostics: KeyDiagnostics

    init(model: DeviceModel) {
        self.model = model
        diagnostics = model.keyDiagnostics
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack {
                Text("按键测试").font(.system(size: 15, weight: .semibold))
                Spacer()
                Button("复制日志") { diagnostics.copy() }.disabled(diagnostics.entries.isEmpty)
                Button("清空") { diagnostics.clear() }.disabled(diagnostics.entries.isEmpty)
            }

            HStack(alignment: .top, spacing: 10) {
                Image(systemName: diagnostics.postingAllowed ? "checkmark.circle.fill" : "exclamationmark.triangle.fill")
                    .foregroundStyle(diagnostics.postingAllowed ? .green : .orange)
                VStack(alignment: .leading, spacing: 4) {
                    Text(diagnostics.postingAllowed ? "系统键盘控制权限已生效" : "系统键盘控制权限未生效，快捷键会被拦截")
                        .font(.system(size: 12, weight: .medium))
                    Text("辅助功能检查：\(diagnostics.accessibilityAllowed ? "允许" : "拒绝") · 事件发送检查：\(diagnostics.postingAllowed ? "允许" : "拒绝")")
                        .font(.system(size: 11)).foregroundStyle(.secondary)
                    if !diagnostics.postingAllowed {
                        Text("若系统开关已开启，请关闭再开启 ThinCube，并重启上位机；更新应用后旧授权可能未生效。")
                            .font(.system(size: 11)).foregroundStyle(.secondary)
                            .fixedSize(horizontal: false, vertical: true)
                    }
                }
                Spacer(minLength: 0)
                Button("打开辅助功能设置…") {
                    NSWorkspace.shared.open(URL(string: "x-apple.systempreferences:com.apple.preference.security?Privacy_Accessibility")!)
                }
            }

            Text(context)
                .font(.system(size: 11.5)).foregroundStyle(.secondary)
                .fixedSize(horizontal: false, vertical: true)

            HStack(spacing: 10) {
                Button(diagnostics.monitoring ? "停止本窗口测试" : "开始本窗口测试") {
                    if diagnostics.monitoring { model.stopKeyboardTest() }
                    else { diagnostics.start() }
                }
                Button("测试当前快捷键") { model.testCurrentShortcut() }
                    .disabled(model.shortcutTestRunning || model.shortcutRecording || model.updating || model.connecting)
                if diagnostics.monitoring { Text("测试中").foregroundStyle(.orange) }
            }
            Text(diagnostics.monitoring
                 ? "可按设备 A 或 Mac 键盘查看事件；本窗口测试会接收并拦住键盘操作，停止后恢复。"
                 : "设备事件会自动记录。测试当前快捷键会在本窗口发送一次并自动松开，无需连接设备。")
                .font(.system(size: 11)).foregroundStyle(.secondary)
                .fixedSize(horizontal: false, vertical: true)

            ScrollView {
                LazyVStack(alignment: .leading, spacing: 6) {
                    if diagnostics.entries.isEmpty {
                        Text("尚无按键事件。请在倒置传音时按下并松开设备 A，或开始本窗口测试。")
                            .foregroundStyle(.secondary)
                    }
                    ForEach(diagnostics.entries) { entry in
                        Text(entry.line)
                            .foregroundStyle(entry.source == "系统回读" ? Color.green : Color.primary)
                            .frame(maxWidth: .infinity, alignment: .leading)
                    }
                }
                .font(.system(size: 10.5, design: .monospaced))
                .textSelection(.enabled)
                .padding(10)
                .frame(maxWidth: .infinity, alignment: .leading)
            }
            .frame(height: 180)
            .background(.black.opacity(0.12), in: RoundedRectangle(cornerRadius: 6))
            Text("最近 100 条，新事件在上方。“已提交”表示发往系统；“系统回读”表示本窗口实际收到。切换窗口会停止本窗口测试。")
                .font(.system(size: 10.5)).foregroundStyle(.secondary)
                .fixedSize(horizontal: false, vertical: true)
        }
        .buttonStyle(.bordered)
        .controlSize(.small)
        .padding(14)
        .background(.white.opacity(0.05), in: RoundedRectangle(cornerRadius: 10))
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
        return "设备：\(connection) · \(audio) · A 键：\(held)\(editing)\n姿态：\(status?.poseDescription ?? "等待设备状态")"
    }
}
