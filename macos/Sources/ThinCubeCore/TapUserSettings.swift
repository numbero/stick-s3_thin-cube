import Foundation

/// Bounded everyday choices around the calibrated default configuration.
/// Selecting one choice only changes the parameters belonging to that control.
public enum TapStrength: Int, CaseIterable {
    case light = 450, normal = 550, firm = 700
}

public enum TapRhythm: Int, CaseIterable {
    case quick = 350, natural = 450, relaxed = 550
}

public enum TapProtection: CaseIterable {
    case standard, stronger

    private var values: [Int] {
        switch self {
        case .standard: return [200, 300, 500, 500, 600]
        case .stronger: return [150, 450, 700, 700, 800]
        }
    }

    fileprivate func matches(_ config: TapConfiguration) -> Bool {
        values == [config.softGyroDps, config.softMotionGuardMs, config.settleMs,
                   config.buttonGuardMs, config.cooldownMs]
    }

    fileprivate func apply(to config: inout TapConfiguration) {
        let v = values
        config.softGyroDps = v[0]; config.softMotionGuardMs = v[1]
        config.settleMs = v[2]; config.buttonGuardMs = v[3]; config.cooldownMs = v[4]
    }
}

public extension TapConfiguration {
    var strength: TapStrength? { TapStrength(rawValue: softThresholdMg) }
    var rhythm: TapRhythm? { TapRhythm(rawValue: softWindowMs) }
    var protection: TapProtection? { TapProtection.allCases.first { $0.matches(self) } }
    var usesEverydayDetector: Bool { software && !manual }

    mutating func setStrength(_ value: TapStrength) { softThresholdMg = value.rawValue }
    mutating func setRhythm(_ value: TapRhythm) { softWindowMs = value.rawValue }
    mutating func setProtection(_ value: TapProtection) { value.apply(to: &self) }
}
