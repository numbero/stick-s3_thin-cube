import Foundation
import ThinCubeCore

/// Bounded local voltage log survives USB unplugging and sudden device power loss.
/// It deliberately includes no PCM, pairing keys, PINs or session cookies.
@MainActor final class PowerDiagnostics {
    private var cursor = PowerSampleCursor()
    private var lastCalibrationFile: String?
    private var terminalCalibrationFiles = Set<String>()
    private let queue = DispatchQueue(label: "ThinCube.power-log", qos: .utility)
    let fileURL = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
        .appendingPathComponent("ThinCube", isDirectory: true).appendingPathComponent("power.jsonl")

    func sample(_ power: PowerTelemetry, device: String, transport: String,
                brightness: Int, streaming: Bool, soundPlaying: Bool) {
        guard cursor.accept(power, device: device) else { return }
        let calibrationFile = power.calibrationLogName
        let recordCalibration = calibrationFile.map { !terminalCalibrationFiles.contains($0) } ?? false
        lastCalibrationFile = recordCalibration ? calibrationFile : nil
        enqueue(["event": "sample", "device": device, "transport": transport,
                 "brightness": brightness, "streaming": streaming,
                 "soundPlaying": soundPlaying, "power": power.dictionary],
                calibrationFile: recordCalibration ? calibrationFile : nil)
        if let calibrationFile, let cal = power.cal, cal.state >= 5 {
            terminalCalibrationFiles.insert(calibrationFile)
        }
    }
    func disconnected(device: String) {
        // A disconnect alone does not identify idle shutdown, low voltage or RF loss.
        enqueue(["event": "disconnected", "device": device], calibrationFile: lastCalibrationFile)
        lastCalibrationFile = nil
    }
    private func enqueue(_ event: [String: Any], calibrationFile: String? = nil) {
        var row = event; row["time"] = Date().timeIntervalSince1970
        guard var data = try? JSONSerialization.data(withJSONObject: row, options: [.sortedKeys]) else { return }
        data.append(10)
        let url = fileURL
        queue.async {
            Self.append(data, to: url)
            if let calibrationFile {
                let calibrationURL = url.deletingLastPathComponent().appendingPathComponent("calibration", isDirectory: true)
                    .appendingPathComponent(calibrationFile)
                Self.appendCalibration(data, to: calibrationURL)
            }
        }
    }
    nonisolated private static func appendCalibration(_ data: Data, to url: URL) {
        let manager = FileManager.default
        do {
            try manager.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
            if let size = try? manager.attributesOfItem(atPath: url.path)[.size] as? NSNumber,
               size.intValue + data.count > 32 * 1024 * 1024 {
                NSLog("ThinCube calibration log exceeded 32 MiB; this run is incomplete")
                return
            }
            if !manager.fileExists(atPath: url.path) {
                guard manager.createFile(atPath: url.path, contents: nil, attributes: [.posixPermissions: 0o600]) else { return }
            }
            let output = try FileHandle(forWritingTo: url)
            defer { try? output.close() }
            try output.seekToEnd(); try output.write(contentsOf: data)
        } catch {
            NSLog("ThinCube calibration logging: %@", error.localizedDescription)
        }
    }
    nonisolated private static func append(_ data: Data, to url: URL) {
        // At most two 2 MiB files. Disk work never runs on the UI/audio thread.
        let manager = FileManager.default
        do {
            try manager.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
            if let size = try? manager.attributesOfItem(atPath: url.path)[.size] as? NSNumber,
               size.intValue + data.count > 2 * 1024 * 1024 {
                let previous = url.deletingLastPathComponent().appendingPathComponent("power.previous.jsonl")
                if manager.fileExists(atPath: previous.path) { try manager.removeItem(at: previous) }
                try manager.moveItem(at: url, to: previous)
            }
            if !manager.fileExists(atPath: url.path) {
                guard manager.createFile(atPath: url.path, contents: nil, attributes: [.posixPermissions: 0o600]) else { return }
            }
            let output = try FileHandle(forWritingTo: url)
            defer { try? output.close() }
            try output.seekToEnd(); try output.write(contentsOf: data)
        } catch {
            // Diagnostic I/O must not interrupt control, audio, or reconnection.
            NSLog("ThinCube power diagnostics: %@", error.localizedDescription)
        }
    }
}
