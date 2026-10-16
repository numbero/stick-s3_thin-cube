import SwiftUI
import ThinCubeCore

struct ConnectionSteps: View {
    let stage: ConnectionStage
    let transport: String
    var body: some View {
        VStack(alignment: .leading, spacing: 9) {
            Label("① 信道连接 · " + transport, systemImage: stage.channelReady ? "checkmark.circle.fill" : "circle")
                .foregroundStyle(stage.channelReady ? .green : .secondary)
            Label("② 设备配对 · " + stage.label,
                  systemImage: stage == .connected ? "checkmark.circle.fill" : "lock.circle")
                .foregroundStyle(stage == .connected ? .green : .primary)
        }.font(.callout)
    }
}

struct PairingSheet: View {
    @ObservedObject var model: DeviceModel
    @FocusState private var codeFocused: Bool
    var body: some View {
        VStack(alignment: .leading, spacing: 18) {
            Text("完成设备配对").font(.title2.bold())
            Text(model.pairingSession?.deviceName ?? model.name).font(.headline)
            ConnectionSteps(stage: model.connectionProgress.stage, transport: model.pairingSession?.transport ?? model.transportLabel)
            Text("信道已连接。请输入设备屏幕上 PAIR PIN 的六位配对码。")
                .fixedSize(horizontal: false, vertical: true)
            VStack(alignment: .leading, spacing: 7) {
                Text("六位配对码").fontWeight(.medium)
                SecureField("例如 001234", text: $model.pin)
                    .textFieldStyle(.roundedBorder).focused($codeFocused)
                    .accessibilityLabel("设备六位配对码")
                    .onSubmit { model.submitPairingCode() }
                    .disabled(model.connectionProgress.stage != .pairing)
                if !model.pairingError.isEmpty {
                    Text(model.pairingError).font(.callout).foregroundStyle(.red)
                        .fixedSize(horizontal: false, vertical: true)
                }
            }
            Text("开始配对后有两分钟输入配对码。设备短按 A 可刷新码；更换上位机时，同时按住设备 A+B 三秒并松开，再点击连接。短按 B 可取消重新配对，原 Wi-Fi 与设置保留。")
                .font(.callout).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
            Text("配对保存后，Wi-Fi 和蓝牙共用授权，后续连接自动完成。")
                .font(.callout).foregroundStyle(.secondary)
            HStack {
                if model.connectionProgress.stage != .pairing {
                    ProgressView(model.connectionProgress.stage.label).controlSize(.small)
                }
                Spacer()
                Button("取消连接") { model.disconnect(note: "配对已取消。点击“连接”可重新开始。") }
                    .keyboardShortcut(.cancelAction)
                Button("配对并连接") { model.submitPairingCode() }
                    .keyboardShortcut(.defaultAction).disabled(!model.canSubmitPairingCode)
            }
        }.padding(24).frame(width: 460)
            .onAppear { codeFocused = true }
            .onChange(of: model.connectionProgress.stage) { if $0 == .pairing { codeFocused = true } }
    }
}
