import Foundation

public struct BatteryCalibrationTelemetry: Codable, Equatable {
    public let run, elapsed, phaseMs, stableMs: UInt32
    public let state, phase, reason: Int
    public var dictionary: [String: Any] {
        ["run": run, "state": state, "elapsed": elapsed, "phase": phase,
         "phaseMs": phaseMs, "stableMs": stableMs, "reason": reason]
    }
}

/// Voltage estimates and diagnostic metadata; never an audio or control level.
public struct PowerTelemetry: Codable, Equatable {
    public let boot, ms: UInt32
    public let rawMv, medianMv, filteredMv, percent, otaPercent: Int
    public let source, vinMv, cutoffMv: Int
    public let charging, chargeKnown, low: Bool
    public let failures: UInt32
    public let chip, model: String?
    public let actualBrightness: Int?
    public let idleMs: UInt32?
    public let cal: BatteryCalibrationTelemetry?

    public var dictionary: [String: Any] {
        var value: [String: Any] = ["boot": boot, "ms": ms, "rawMv": rawMv, "medianMv": medianMv,
         "filteredMv": filteredMv, "percent": percent, "otaPercent": otaPercent,
         "source": source, "vinMv": vinMv, "cutoffMv": cutoffMv,
         "charging": charging, "chargeKnown": chargeKnown, "low": low, "failures": failures]
        if let chip { value["chip"] = chip }
        if let model { value["model"] = model }
        if let actualBrightness { value["actualBrightness"] = actualBrightness }
        if let idleMs { value["idleMs"] = idleMs }
        if let cal { value["cal"] = cal.dictionary }
        return value
    }
    public var calibrationLogName: String? {
        guard let cal, cal.run != 0, (1...6).contains(cal.state), let chip, chip.count == 12,
              chip.unicodeScalars.allSatisfy({ CharacterSet(charactersIn: "0123456789abcdefABCDEF").contains($0) }) else { return nil }
        return "battery-\(chip.lowercased())-\(boot)-\(cal.run).jsonl"
    }
}

/// Cached BLE deltas repeat samples; a delayed full snapshot may be older.
/// Accept only new samples, including MCU clock wrap and a newly booted device.
public struct PowerSampleCursor {
    private var device: String?
    private var boot: UInt32 = 0
    private var ms: UInt32 = 0
    public init() {}
    public mutating func accept(_ sample: PowerTelemetry, device id: String) -> Bool {
        if device == id && boot == sample.boot {
            let elapsed = sample.ms &- ms
            guard elapsed > 0, elapsed < 0x80000000 else { return false }
        }
        device = id; boot = sample.boot; ms = sample.ms
        return true
    }
}
