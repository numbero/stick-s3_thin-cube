import Foundation
import ThinCubeCore

@MainActor final class ConnectionDiagnostics {
    private var timeline = ConnectionTimeline()
    private let queue = DispatchQueue(label: "ThinCube.connection-log", qos: .utility)
    let fileURL = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
        .appendingPathComponent("ThinCube", isDirectory: true).appendingPathComponent("connection.jsonl")
    func begin(attempt: UUID, transport: String) {
        write(timeline.begin(attempt: attempt, transport: transport, now: ProcessInfo.processInfo.systemUptime))
    }
    func mark(_ stage: ConnectionTimeline.Stage, attempt: UUID) {
        if let row = timeline.mark(stage, attempt: attempt, now: ProcessInfo.processInfo.systemUptime) { write(row) }
    }
    var snapshot: [[String: Any]] {
        timeline.records.map { ["attempt": $0.attempt.uuidString, "transport": $0.transport,
                               "stage": $0.stage.rawValue, "elapsedMs": $0.elapsedMs, "stepMs": $0.stepMs] }
    }
    func http(_ record: ConnectionRequestTiming) {
        guard timeline.attempt == record.attempt else { return }
        write(record)
    }
    private func write<T: Encodable>(_ record: T) {
        guard var data = try? JSONEncoder().encode(record) else { return }
        data.append(10)
        let url = fileURL, payload = data
        queue.async {
            do {
                let manager = FileManager.default
                try manager.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
                if let size = try? manager.attributesOfItem(atPath: url.path)[.size] as? NSNumber,
                   size.intValue + payload.count > 2 * 1024 * 1024 {
                    let previous = url.deletingLastPathComponent().appendingPathComponent("connection.previous.jsonl")
                    if manager.fileExists(atPath: previous.path) { try manager.removeItem(at: previous) }
                    try manager.moveItem(at: url, to: previous)
                }
                if !manager.fileExists(atPath: url.path) {
                    guard manager.createFile(atPath: url.path, contents: nil, attributes: [.posixPermissions: 0o600]) else { return }
                }
                let file = try FileHandle(forWritingTo: url)
                defer { try? file.close() }
                try file.seekToEnd(); try file.write(contentsOf: payload)
            } catch { NSLog("ThinCube connection diagnostics: %@", error.localizedDescription) }
        }
    }
}
