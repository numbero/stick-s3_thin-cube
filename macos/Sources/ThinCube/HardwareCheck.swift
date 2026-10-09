import Foundation

// Opt-in hardware runner uses the production connection, authentication and audio
// bridge. It does not synthesize physical gestures or collect microphone content.
@MainActor enum HardwareCheck {
    static func startIfRequested(model: DeviceModel) {
        let args = ProcessInfo.processInfo.arguments
        guard let index = args.firstIndex(of: "--hardware-check"), args.indices.contains(index + 1),
              args[index + 1].hasPrefix("/") else { return }
        func number(_ flag: String, maximum: Int) -> Int {
            guard let i = args.firstIndex(of: flag), args.indices.contains(i + 1),
                  let value = Int(args[i + 1]), (0...maximum).contains(value) else { return 0 }
            return value
        }
        let path = args[index + 1], seconds = number("--audio-seconds", maximum: 3600)
        let switches = number("--switch-count", maximum: 200)
        Task {
            await run(model: model, path: path, seconds: seconds, switches: switches)
            // A new explicit request can start another check in this same
            // process, preserving its already approved Keychain access.
            let request = path + ".request"
            while !Task.isCancelled {
                try? await Task.sleep(nanoseconds: 1_000_000_000)
                guard let bytes = FileManager.default.contents(atPath: request),
                      let job = try? JSONSerialization.jsonObject(with: bytes) as? [String: Any],
                      let log = job["log"] as? String, log.hasPrefix("/"),
                      let audio = job["audioSeconds"] as? Int, (0...3600).contains(audio),
                      let count = job["switchCount"] as? Int, (0...200).contains(count) else { continue }
                try? FileManager.default.removeItem(atPath: request)
                await run(model: model, path: log, seconds: audio, switches: count)
            }
        }
    }
    private static func run(model: DeviceModel, path: String, seconds: Int, switches: Int) async {
        guard FileManager.default.createFile(atPath: path, contents: nil),
              let output = FileHandle(forWritingAtPath: path) else { return }
        defer { try? output.close() }
        func record(_ event: String, _ extra: [String: Any] = [:]) {
            var value = model.hardwareSnapshot(); value["event"] = event
            value["time"] = ISO8601DateFormatter().string(from: Date())
            for (key, item) in extra { value[key] = item }
            if var data = try? JSONSerialization.data(withJSONObject: value, options: [.sortedKeys]) {
                data.append(10); try? output.write(contentsOf: data)
            }
        }
        func wait(_ condition: () -> Bool, timeout: TimeInterval) async -> Bool {
            let deadline = Date().addingTimeInterval(timeout)
            while Date() < deadline {
                if condition() { return true }
                do { try await Task.sleep(nanoseconds: 200_000_000) } catch { return false }
            }
            return false
        }
        record("begin", ["audioSeconds": seconds, "switchCount": switches])
        guard await wait({ model.connected && !model.connecting && !model.switching }, timeout: 70) else { record("failed", ["reason": "initial_connection"]); return }
        if seconds > 0 {
            await model.command("start")
            guard await wait({ model.status?.streaming == true }, timeout: 5) else { record("failed", ["reason": "audio_start"]); return }
            record("audio_begin")
            for second in 1...seconds {
                try? await Task.sleep(nanoseconds: 1_000_000_000)
                record("audio_sample", ["second": second])
                if !model.connected || model.status?.streaming != true {
                    record("failed", ["reason": "audio_interrupted", "second": second]); await model.command("stop"); return
                }
            }
            await model.command("stop"); record("audio_end")
        }
        for index in 0..<switches {
            guard await wait({ model.canSwitchTransport }, timeout: 70) else { record("failed", ["reason": "switch_unavailable", "index": index]); return }
            let desired = model.currentTransport == "ble" ? "wifi" : "ble"
            let start = Date(); record("switch_begin", ["index": index, "target": desired])
            await model.switchTransport()
            guard await wait({ model.connected && !model.connecting && !model.switching && model.currentTransport == desired && model.status?.transportTransaction?.phase == "committed" }, timeout: 75) else {
                record("failed", ["reason": "switch_not_committed", "index": index, "target": desired]); return
            }
            record("switch_end", ["index": index, "target": desired, "seconds": Date().timeIntervalSince(start)])
            try? await Task.sleep(nanoseconds: 1_000_000_000)
        }
        record("complete")
    }
}
