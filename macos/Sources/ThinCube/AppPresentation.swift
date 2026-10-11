import AppKit
import SwiftUI

@MainActor final class ThinCubeAppDelegate: NSObject, NSApplicationDelegate {
    let model = DeviceModel()
    lazy var presentation = AppPresentation(model: model)

    func applicationWillFinishLaunching(_ notification: Notification) {
        presentation.applyActivationPolicy()
    }
    func applicationDidFinishLaunching(_ notification: Notification) {
        if !presentation.menuBarOnly { presentation.showMainWindow() }
    }
    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool { false }
    func applicationShouldHandleReopen(_ sender: NSApplication, hasVisibleWindows flag: Bool) -> Bool {
        presentation.showMainWindow()
        return false
    }
}

/// One retained window keeps menu actions usable after closing the window and
/// lets menu-only launches avoid creating or flashing an unwanted window.
@MainActor final class AppPresentation: NSObject, ObservableObject, NSWindowDelegate {
    private static let preferenceKey = "ui.menuBarOnly.v1"
    static let windowShown = Notification.Name("ThinCubeMainWindowShown")
    @Published private(set) var menuBarOnly: Bool
    private let defaults: UserDefaults
    private let model: DeviceModel
    private var mainWindow: NSWindow?
    var mainWindowVisible: Bool { mainWindow?.isVisible == true && mainWindow?.isMiniaturized == false }

    init(model: DeviceModel, defaults: UserDefaults = .standard) {
        self.model = model; self.defaults = defaults
        menuBarOnly = defaults.bool(forKey: Self.preferenceKey)
        super.init()
    }
    @discardableResult func applyActivationPolicy() -> Bool {
        let policy: NSApplication.ActivationPolicy = menuBarOnly ? .accessory : .regular
        return NSApp.activationPolicy() == policy || NSApp.setActivationPolicy(policy)
    }
    func setMenuBarOnly(_ enabled: Bool) {
        guard enabled != menuBarOnly else { return }
        let policy: NSApplication.ActivationPolicy = enabled ? .accessory : .regular
        guard NSApp.setActivationPolicy(policy) else {
            model.note = "无法更新 Dock 显示方式，请重试。"; return
        }
        menuBarOnly = enabled; defaults.set(enabled, forKey: Self.preferenceKey)
        if enabled { mainWindow?.orderOut(nil) }
        else { showMainWindow() }
    }
    func showMainWindow(pane: MainPane? = nil) {
        if let pane { model.pane = pane }
        if mainWindow == nil {
            let window = NSWindow(contentRect: NSRect(x: 0, y: 0, width: 900, height: 600),
                                  styleMask: [.titled, .closable, .miniaturizable, .resizable, .fullSizeContentView],
                                  backing: .buffered, defer: false)
            window.title = "ThinCube"; window.isReleasedWhenClosed = false
            window.minSize = NSSize(width: 780, height: 500)
            window.delegate = self
            let host = NSHostingController(rootView: MainView(model: model, discovery: model.discovery, presentation: self)
                .frame(minWidth: 780, minHeight: 500))
            host.sizingOptions = [.minSize]
            if #available(macOS 14, *) { host.sceneBridgingOptions = [.toolbars] }
            window.contentViewController = host
            let restored = window.setFrameUsingName("ThinCubeMainWindow")
            window.setFrameAutosaveName("ThinCubeMainWindow")
            let onScreen = NSScreen.screens.contains {
                let overlap = $0.visibleFrame.intersection(window.frame)
                return overlap.width >= 200 && overlap.height >= 120
            }
            if !restored || !onScreen { window.center() }
            mainWindow = window
        }
        mainWindow?.deminiaturize(nil)
        NSApp.activate(ignoringOtherApps: true)
        mainWindow?.makeKeyAndOrderFront(nil)
        NotificationCenter.default.post(name: Self.windowShown, object: mainWindow)
    }
    func windowShouldClose(_ sender: NSWindow) -> Bool {
        sender.orderOut(nil)
        return false
    }
}
