import Foundation
import CoreFoundation

/// Fast BLE updates reuse an authenticated full snapshot from the same epoch.
/// A new connection must obtain a full snapshot before deltas are accepted.
public struct BLEStatusSnapshots {
    private var baseline: [String: Any]?
    public init() {}
    public mutating func reset() { baseline = nil }
    public mutating func accept(_ data: Data) -> Data? {
        guard let value = try? JSONSerialization.jsonObject(with: data) as? [String: Any] else { return nil }
        func unsigned(_ value: Any) -> UInt32? {
            guard let number = value as? NSNumber, CFGetTypeID(number) != CFBooleanGetTypeID() else { return nil }
            return number as? UInt32
        }
        guard let live = value["s"] as? [Any] else {
            if value["epoch"] != nil { baseline = value }
            return data
        }
        guard live.count == 7, let epoch = unsigned(live[0]), epoch != 0,
              let flags = unsigned(live[1]), flags <= 255,
              let session = live[2] as? String, session.count == 32, Data(hex: session)?.count == 16,
              let levelNumber = live[3] as? NSNumber, CFGetTypeID(levelNumber) != CFBooleanGetTypeID(),
              let level = levelNumber as? Double, level.isFinite, (0...1).contains(level),
              let pose = live[4] as? String, let block = live[5] as? String,
              let dropped = unsigned(live[6]), var snapshot = baseline,
              snapshot["epoch"] as? UInt32 == epoch, value["edge"] != nil else { return nil }
        if !(value["edge"] is NSNull) {
            guard let edge = value["edge"] as? [String: Any], let sequence = edge["seq"], unsigned(sequence) != nil,
                  let down = edge["down"] as? NSNumber, CFGetTypeID(down) == CFBooleanGetTypeID() else { return nil }
            if let key = edge["key"] {
                guard let key = key as? String, DeviceButton(rawValue: key) != nil else { return nil }
            }
        }
        if let mode = value["keyMode"], unsigned(mode) != 1 && unsigned(mode) != 2 { return nil }
        if let held = value["heldB"] {
            guard let held = held as? NSNumber, CFGetTypeID(held) == CFBooleanGetTypeID() else { return nil }
        }
        if snapshot["gestureProtocol"] as? Int == 1, value["gesture"] == nil { return nil }
        if let mode = value["gestureMode"], unsigned(mode) != 0 && unsigned(mode) != 1 { return nil }
        if let gesture = value["gesture"], !(gesture is NSNull) {
            guard let event = gesture as? [String: Any], let seq = event["seq"], unsigned(seq) != nil,
                  let age = event["ageMs"], unsigned(age) != nil,
                  let type = event["type"] as? String, !type.isEmpty, type.count <= 32 else { return nil }
        }
        for (bit, key) in ["allowed", "muted", "localSound", "streaming", "manual", "held", "micReady", "inverted"].enumerated() {
            snapshot[key] = flags & (1 << bit) != 0
        }
        snapshot["session"] = session; snapshot["level"] = level
        snapshot["pose"] = pose; snapshot["voiceBlock"] = block
        snapshot["bleAudioDropped"] = dropped; snapshot["edge"] = value["edge"]
        if let mode = value["keyMode"] { snapshot["keyMode"] = mode }
        if let held = value["heldB"] { snapshot["heldB"] = held }
        if let gesture = value["gesture"] { snapshot["gesture"] = gesture }
        if let mode = value["gestureMode"] { snapshot["gestureMode"] = mode }
        if let power = value["power"] { snapshot["power"] = power }
        if let sleep = value["sleep"] { snapshot["sleep"] = sleep }
        baseline = snapshot
        return try? JSONSerialization.data(withJSONObject: snapshot)
    }
}
