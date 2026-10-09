import AppKit
import ApplicationServices
import ThinCubeCore

struct KeyTraceEntry: Identifiable {
    let id = UUID()
    let time = Date()
    let source: String
    let detail: String
    var line: String { "\(Self.clock.string(from: time))  [\(source)] \(detail)" }
    private static let clock: DateFormatter = {
        let formatter = DateFormatter(); formatter.dateFormat = "HH:mm:ss.SSS"; return formatter
    }()
}

@MainActor final class KeyDiagnostics: ObservableObject {
    @Published private(set) var entries: [KeyTraceEntry] = []
    @Published private(set) var monitoring = false
    @Published private(set) var accessibilityAllowed = false
    @Published private(set) var postingAllowed = false
    var onStop: (() -> Void)?
    private var monitor: Any?
    private var observers: [NSObjectProtocol] = []
    private weak var window: NSWindow?

    init() { refreshPermission() }

    func refreshPermission() {
        let accessibility = AXIsProcessTrusted()
        let posting = CGPreflightPostEventAccess()
        if accessibilityAllowed != accessibility { accessibilityAllowed = accessibility }
        if postingAllowed != posting { postingAllowed = posting }
    }

    func record(_ source: String, _ detail: String) {
        entries.insert(KeyTraceEntry(source: source, detail: detail), at: 0)
        if entries.count > 100 { entries.removeLast(entries.count - 100) }
    }

    func clear() { entries.removeAll() }
    func copy() {
        NSPasteboard.general.clearContents()
        NSPasteboard.general.setString(entries.reversed().map(\.line).joined(separator: "\n"), forType: .string)
    }

    func start() {
        guard !monitoring else { return }
        NSApp.activate(ignoringOtherApps: true)
        guard let window = NSApp.keyWindow ?? NSApp.mainWindow ?? NSApp.windows.first(where: {
            $0.isVisible && $0.canBecomeKey && $0.title == "ThinCube"
        }) else { return }
        self.window = window
        window.makeKeyAndOrderFront(nil)
        monitor = NSEvent.addLocalMonitorForEvents(matching: [.keyDown, .keyUp, .flagsChanged]) { [weak self] event in
            guard let self, self.monitoring,
                  event.window === self.window || NSApp.keyWindow === self.window else { return event }
            let code = event.keyCode
            let flags = event.cgEvent?.flags ?? CGEventFlags(rawValue: UInt64(event.modifierFlags.rawValue))
            let tagged = event.cgEvent?.getIntegerValueField(.eventSourceUserData) == KeyEventStep.sourceTag
            let type: String
            if event.type == .flagsChanged {
                let modifier = ShortcutCatalog.modifier(code)
                let down = modifier.map { flags.rawValue & $0.side != 0 } ?? false
                type = "flagsChanged / \(down ? "按下" : "松开")"
            } else { type = event.type == .keyDown ? "keyDown / 按下" : "keyUp / 松开" }
            let key = ShortcutCatalog.label(ShortcutCatalog.modifier(code)?.token ?? "keycode_\(code)")
            let repeating = event.type == .keyDown && event.isARepeat
            self.record(tagged ? "系统回读" : "窗口键盘", "\(type) · \(key) · code=\(code) · flags=0x\(String(flags.rawValue, radix: 16))\(repeating ? " · 重复" : "")")
            // Only this explicit test window consumes keys. No global listener,
            // input-monitoring permission, or capture in other applications.
            return nil
        }
        guard monitor != nil else { self.window = nil; return }
        monitoring = true
        for (name, object) in [(NSWindow.didResignKeyNotification, window as AnyObject?),
                               (NSApplication.didResignActiveNotification, nil)] {
            observers.append(NotificationCenter.default.addObserver(forName: name, object: object, queue: .main) { [weak self] _ in
                MainActor.assumeIsolated { self?.stop() }
            })
        }
        record("测试", "已开始本窗口测试；键盘事件只显示在此处，不修改快捷键映射。")
    }

    func stop() {
        guard monitoring else { return }
        if let monitor { NSEvent.removeMonitor(monitor) }
        monitor = nil; observers.forEach(NotificationCenter.default.removeObserver); observers.removeAll()
        window = nil; monitoring = false
        onStop?()
        record("测试", "已停止本窗口测试。")
    }

    deinit {
        if let monitor { NSEvent.removeMonitor(monitor) }
        observers.forEach(NotificationCenter.default.removeObserver)
    }
}
