import AppKit
import SwiftUI

struct KeyboardDiagnosticsSheet: View {
    @ObservedObject var model: DeviceModel
    @Environment(\.dismiss) private var dismiss
    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            KeyTestPanel(model: model)
            HStack {
                Spacer()
                Button("完成") { dismiss() }.keyboardShortcut(.cancelAction)
            }
        }.padding(20).frame(width: 640)
    }
}

struct AudioDiagnosticsSheet: View {
    @ObservedObject var model: DeviceModel
    @Environment(\.dismiss) private var dismiss
    var body: some View {
        VStack(alignment: .leading, spacing: 18) {
            HelpSectionHeader("音频诊断", helpTitle: "音频诊断帮助") {
                HelpParagraph("统计", "显示当前连接接收的音频帧、输出样本、丢失补零、缓冲等待和输入峰值。")
                HelpParagraph("查看诊断", "只查看运行统计，不会开始传音或录音。")
            }.font(.title2.bold())
            LabeledContent("设备", value: model.connected ? model.name : "未连接")
            if model.connected { LabeledContent("连接方式", value: model.transportLabel) }
            Text(model.audioDiagnostics).font(.system(.callout, design: .monospaced))
                .textSelection(.enabled).fixedSize(horizontal: false, vertical: true)
                .frame(maxWidth: .infinity, alignment: .leading).padding(14)
                .background(.white.opacity(0.05), in: RoundedRectangle(cornerRadius: 8))
            HStack {
                Button("复制统计") {
                    NSPasteboard.general.clearContents()
                    NSPasteboard.general.setString(model.audioDiagnostics, forType: .string)
                }
                Spacer()
                Button("完成") { dismiss() }.keyboardShortcut(.cancelAction)
            }
        }.padding(24).frame(width: 500)
    }
}

struct ChangeWiFiSheet: View {
    @ObservedObject var model: DeviceModel
    @Environment(\.dismiss) private var dismiss
    @State private var ssid = ""
    @State private var password = ""
    @State private var submitting = false
    @State private var error: String?
    @FocusState private var ssidFocused: Bool
    var body: some View {
        VStack(alignment: .leading, spacing: 18) {
            HelpSectionHeader("更换 Wi-Fi", helpTitle: "更换网络帮助") {
                HelpParagraph("验证与保存", "设备会先验证新网络，成功后保存并重启；失败时保留旧凭据。更换网络会停止语音，Mac 接入同一局域网后可恢复连接。")
                HelpParagraph("开放网络", "网络无需密码时，将密码留空。")
            }.font(.title2.bold())
            VStack(alignment: .leading, spacing: 6) {
                Text("网络名称")
                TextField("Wi-Fi 名称", text: $ssid).focused($ssidFocused)
                    .textFieldStyle(.roundedBorder)
            }
            .disabled(submitting)
            VStack(alignment: .leading, spacing: 6) {
                Text("密码")
                SecureField("开放网络留空", text: $password).textFieldStyle(.roundedBorder)
                    .onSubmit { submit() }
            }
            .disabled(submitting)
            if let error { Text(error).font(.callout).foregroundStyle(.red) }
            HStack {
                if submitting { ProgressView("正在提交…").controlSize(.small) }
                Spacer()
                Button("取消") { dismiss() }.keyboardShortcut(.cancelAction).disabled(submitting)
                Button("验证并更换") { submit() }.keyboardShortcut(.defaultAction)
                    .disabled(submitting || ssid.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty || !model.connected || model.currentTransport != "wifi" || model.updating || model.switching)
            }
        }.padding(24).frame(width: 420)
            .onAppear { ssidFocused = true }
            .onChange(of: ssid) { _ in error = nil }
            .onChange(of: password) { _ in error = nil }
    }
    private func submit() {
        guard !submitting, !ssid.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty,
              model.connected, model.currentTransport == "wifi", !model.updating, !model.switching else { return }
        submitting = true; error = nil
        model.wifiSSID = ssid; model.wifiPassword = password
        Task {
            let accepted = await model.changeWifi()
            submitting = false
            if accepted { dismiss() } else { error = model.note }
        }
    }
}
