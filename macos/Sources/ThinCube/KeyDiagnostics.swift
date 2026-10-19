import AppKit
import ApplicationServices
import Combine

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
    @Published private(set) var accessibilityAllowed = false
    @Published private(set) var postingAllowed = false

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
}
