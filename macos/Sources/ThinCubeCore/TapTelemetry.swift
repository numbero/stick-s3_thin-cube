import Foundation

public struct TapConfiguration: Codable, Equatable {
    public var enabled = true, single = false, doubleTap = true, triple = false
    public var filtered = true, wait = true, manual = false, software = true
    public var axis = 2, sensitivity = 9, windowMs = 400, quietMs = 400
    public var settleMs = 500, buttonGuardMs = 500, cooldownMs = 600
    public var softThresholdMg = 550, softReboundMs = 180, softWindowMs = 450
    public var softGyroDps = 200, softMotionGuardMs = 300, softBaselineMs = 200, softRatioPercent = 65
    public var softGyroFilterMs = 100
    public init() {}
    private enum CodingKeys: String, CodingKey { case enabled, single, doubleTap, triple, filtered, wait, manual, software, axis, sensitivity, windowMs, quietMs, settleMs, buttonGuardMs, cooldownMs, softThresholdMg, softReboundMs, softWindowMs, softGyroDps, softMotionGuardMs, softBaselineMs, softRatioPercent, softGyroFilterMs }
    public init(from decoder: Decoder) throws {
        self.init()
        let container = try decoder.container(keyedBy: CodingKeys.self)
        enabled = try container.decode(Bool.self, forKey: .enabled)
        single = try container.decode(Bool.self, forKey: .single)
        doubleTap = try container.decode(Bool.self, forKey: .doubleTap)
        triple = try container.decode(Bool.self, forKey: .triple)
        filtered = try container.decode(Bool.self, forKey: .filtered)
        wait = try container.decode(Bool.self, forKey: .wait)
        manual = try container.decode(Bool.self, forKey: .manual)
        software = try container.decodeIfPresent(Bool.self, forKey: .software) ?? false
        axis = try container.decode(Int.self, forKey: .axis)
        sensitivity = try container.decode(Int.self, forKey: .sensitivity)
        windowMs = try container.decode(Int.self, forKey: .windowMs)
        quietMs = try container.decode(Int.self, forKey: .quietMs)
        settleMs = try container.decode(Int.self, forKey: .settleMs)
        buttonGuardMs = try container.decode(Int.self, forKey: .buttonGuardMs)
        cooldownMs = try container.decode(Int.self, forKey: .cooldownMs)
        softThresholdMg = try container.decodeIfPresent(Int.self, forKey: .softThresholdMg) ?? softThresholdMg
        softReboundMs = try container.decodeIfPresent(Int.self, forKey: .softReboundMs) ?? softReboundMs
        softWindowMs = try container.decodeIfPresent(Int.self, forKey: .softWindowMs) ?? softWindowMs
        softGyroDps = try container.decodeIfPresent(Int.self, forKey: .softGyroDps) ?? softGyroDps
        softMotionGuardMs = try container.decodeIfPresent(Int.self, forKey: .softMotionGuardMs) ?? softMotionGuardMs
        softBaselineMs = try container.decodeIfPresent(Int.self, forKey: .softBaselineMs) ?? softBaselineMs
        softRatioPercent = try container.decodeIfPresent(Int.self, forKey: .softRatioPercent) ?? softRatioPercent
        softGyroFilterMs = try container.decodeIfPresent(Int.self, forKey: .softGyroFilterMs) ?? 0
    }
    public var isValid: Bool {
        (!enabled || single || doubleTap || triple) && (0...2).contains(axis) && (0...15).contains(sensitivity) &&
        (250...1000).contains(windowMs) && windowMs % 5 == 0 &&
        (250...500).contains(quietMs) && quietMs % 5 == 0 &&
        (0...5000).contains(settleMs) && (0...5000).contains(buttonGuardMs) && (0...10000).contains(cooldownMs) &&
        (100...2000).contains(softThresholdMg) && (80...300).contains(softReboundMs) &&
        (250...1000).contains(softWindowMs) && softWindowMs > softReboundMs &&
        (30...600).contains(softGyroDps) && (100...2000).contains(softMotionGuardMs) &&
        (50...1000).contains(softBaselineMs) && (40...95).contains(softRatioPercent) && (0...1000).contains(softGyroFilterMs)
    }
}

public struct TapTelemetry: Codable {
    public let supported: Bool
    public let persistence: Int?, saved: Bool?, storage: String?
    public let diagnosticOnly, verified, enabled, healthy, context: Bool?
    public let revision: UInt32?, requestId, result: String?
    public let config: TapConfiguration?
    public let raw, accepted, blocked, single, triple, unknown: UInt32?
    public let softwareRaw, softwareAccepted, softwareBlocked, softwareCandidates, softwareRejected, softwareSamples: UInt32?
    public let softwareFifo: Bool?
    public let softwareMotionRejected, softwareSingleRejected, softwareMultipleRejected: UInt32?
    public let softwareMaxGyro: Double?
    public let inactive, buttons, settling, cooldown: UInt32?
    public let eventSequence, eventAt, output, errors, maxPollMs, ms: UInt32?
    public let reason: String?
    public var supportsPersistence: Bool { persistence == 1 }
    public func confirms(_ id: String, configuration: TapConfiguration? = nil, requireSaved: Bool = false) -> Bool {
        supported && requestId == id && result == "ok" && verified == true &&
        (!requireSaved || (supportsPersistence && saved == true)) &&
        (configuration == nil || (config == configuration && enabled == configuration?.enabled))
    }
}
