import Foundation

public enum ConnectionMode: String, CaseIterable, Sendable {
    case automatic, wifi, ble

    public var label: String {
        switch self {
        case .automatic: return "自动"
        case .wifi: return "Wi-Fi"
        case .ble: return "蓝牙"
        }
    }

    public func accepts(host: String) -> Bool {
        self == .automatic || host.hasPrefix("ble://") == (self == .ble)
    }

    /// Live discovery takes precedence over saved addresses, but never over identity.
    public func endpoint(for deviceID: String, in endpoints: [ConnectionEndpoint],
                         preferredTransport: String, wifiAvailable: Bool,
                         bluetoothAvailable: Bool) -> ConnectionEndpoint? {
        let candidates = endpoints.filter {
            $0.deviceID == deviceID && accepts(host: $0.host) &&
            ($0.host.hasPrefix("ble://") ? bluetoothAvailable : wifiAvailable)
        }
        return candidates.first { $0.host.hasPrefix("ble://") == (preferredTransport == "ble") }
            ?? candidates.first
    }
}

public struct ConnectionEndpoint: Equatable, Sendable {
    public let deviceID: String
    public let host: String
    public init(deviceID: String, host: String) { self.deviceID = deviceID; self.host = host }
}

public enum PairingCode {
    public static func isValid(_ value: String) -> Bool {
        value.utf8.count == 6 && value.utf8.allSatisfy { (48...57).contains($0) }
    }
}
