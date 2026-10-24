import AppKit
import ServiceManagement
import SwiftUI

/// The system owns this preference, including changes made in System Settings.
/// Reading it never registers the app or overrides the user's choice.
@MainActor final class LoginItemSettings: ObservableObject {
    @Published private(set) var status = SMAppService.mainApp.status
    @Published private(set) var errorMessage: String?

    var enabled: Bool { status == .enabled }
    var needsApproval: Bool { status == .requiresApproval }
    var detail: String {
        switch status {
        case .enabled: return "已开启，登录 Mac 时自动启动 ThinCube。"
        case .notRegistered, .notFound: return "开启后，登录 Mac 时自动启动 ThinCube。"
        case .requiresApproval: return "开机自启尚未生效，请在系统的“登录项”中允许 ThinCube。"
        @unknown default: return "暂时无法确认开机自启状态，请稍后重试。"
        }
    }

    func refresh() {
        let current = SMAppService.mainApp.status
        if current != status {
            status = current
            errorMessage = nil
        }
    }

    func setEnabled(_ enabled: Bool) {
        refresh()
        errorMessage = nil
        if enabled && needsApproval { return }
        guard enabled != self.enabled else { return }
        do {
            if enabled { try SMAppService.mainApp.register() }
            else { try SMAppService.mainApp.unregister() }
            refresh()
            if self.enabled != enabled && !needsApproval {
                errorMessage = "系统尚未确认开机自启变更，请重试。"
            }
        } catch {
            refresh()
            // A registration can need approval even when register() throws.
            if self.enabled != enabled && !needsApproval {
                errorMessage = "无法\(enabled ? "开启" : "关闭")开机自启：\(error.localizedDescription)"
            }
        }
    }

    func openSystemSettings() {
        SMAppService.openSystemSettingsLoginItems()
    }
}

struct LaunchAtLoginToggle: View {
    @ObservedObject var settings: LoginItemSettings
    var showSettings: (() -> Void)? = nil

    var body: some View {
        Toggle("开机自启", isOn: Binding(get: { settings.enabled }, set: {
            settings.setEnabled($0)
            if settings.needsApproval || settings.errorMessage != nil { showSettings?() }
        }))
        .help("登录 Mac 时自动启动 ThinCube")
        .onAppear { settings.refresh() }
        .onReceive(NotificationCenter.default.publisher(for: NSApplication.didBecomeActiveNotification)) { _ in
            settings.refresh()
        }
    }
}

struct LoginItemSettingsSection: View {
    @ObservedObject var settings: LoginItemSettings

    var body: some View {
        Section {
            LaunchAtLoginToggle(settings: settings)
            Text(settings.detail)
                .font(.callout).foregroundStyle(settings.needsApproval ? .orange : .secondary)
                .fixedSize(horizontal: false, vertical: true)
            if settings.needsApproval {
                Button("打开系统登录项…") { settings.openSystemSettings() }
            }
            if let error = settings.errorMessage {
                Text(error).font(.callout).foregroundStyle(.red)
                    .fixedSize(horizontal: false, vertical: true).textSelection(.enabled)
            }
        } header: { Text("启动") }
    }
}
