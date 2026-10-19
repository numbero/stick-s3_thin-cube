import AppKit
import SwiftUI
import ThinCubeCore

struct KeyDiagnosticsPanel: View {
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
                    HelpParagraph("实时日志", "设备事件自动记录，无需开始测试。日志展示收到的 A/B、双敲事件、接受或拦截原因，以及向系统提交的按下与松开。")
                    HelpParagraph("时间线", "保留最近 100 条事件，新事件在上方。复制日志按时间先后排列，方便排查接收、处理和发键的顺序。")
                    HelpParagraph("已提交", "表示按键事件已提交给系统，不能据此确认目标应用是否响应。此页只观察日志，不模拟发键或捕获键盘输入。")
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
            Text(doubleTapContext).font(.callout).foregroundStyle(.secondary)
                .fixedSize(horizontal: false, vertical: true)
            HStack {
                Text("最近 \(diagnostics.entries.count) 条事件").font(.caption).foregroundStyle(.secondary)
                Spacer()
                Button("复制日志") { diagnostics.copy() }.disabled(diagnostics.entries.isEmpty)
                Button("清空") { diagnostics.clear() }.disabled(diagnostics.entries.isEmpty)
            }
            ScrollView {
                LazyVStack(alignment: .leading, spacing: 6) {
                    if diagnostics.entries.isEmpty { Text("暂无事件，设备事件将自动显示在此处。").foregroundStyle(.secondary) }
                    ForEach(diagnostics.entries) { entry in
                        Text(entry.line).frame(maxWidth: .infinity, alignment: .leading)
                    }
                }
                .font(.system(size: 10.5, design: .monospaced)).textSelection(.enabled)
                .padding(10).frame(maxWidth: .infinity, alignment: .leading)
            }
            .frame(height: 240)
            .background(.black.opacity(0.12), in: RoundedRectangle(cornerRadius: 6))
        }
        .buttonStyle(.bordered).controlSize(.small)
        .onAppear { diagnostics.refreshPermission() }
        .onReceive(Timer.publish(every: 1, on: .main, in: .common).autoconnect()) { _ in diagnostics.refreshPermission() }
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
    private var doubleTapContext: String {
        let binding = model.shortcut(for: .doubleTap)?.label ?? "未设置"
        let state: String
        if !model.connected { state = "设备未连接" }
        else if !model.supportsDoubleTap { state = "当前固件不支持双敲" }
        else { state = model.status?.tapMode == 1 ? "设备协议已协商" : "等待设备协议协商" }
        return "双敲：\(binding) · \(state)"
    }
}
