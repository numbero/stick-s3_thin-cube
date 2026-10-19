import Foundation

public enum DeviceBLEOperation {
    public static func forPath(_ path: String) -> String? {
        switch path {
        case "api/info": return "info"
        case "api/pair": return "pair"
        case "api/pair/begin": return "pair_begin"
        case "api/status": return "status"
        case "api/settings": return "settings"
        case "api/wifi": return "wifi"
        case "api/network": return "network_result"
        case "api/tap": return "tap"
        default: return nil
        }
    }
}

public struct WiFiConfiguration: Sendable {
    public let ssid: String
    public let password: String
    public init(ssid: String, password: String) { self.ssid = ssid; self.password = password }
    public var validationError: String? {
        if ssid.isEmpty || ssid.utf8.count > 32 || ssid.contains("\0") { return "Wi-Fi 名称需要为 1–32 字节。" }
        if password.contains("\0") || password.utf8.count > 63 || (!password.isEmpty && password.utf8.count < 8) {
            return "Wi-Fi 密码需要为 8–63 字节；开放网络请留空。"
        }
        return nil
    }
}
public enum WiFiConfigurationRecovery {
    public static func mode(elapsed: TimeInterval, bluetoothRediscovered: Bool) -> ConnectionMode {
        elapsed >= 10 && bluetoothRediscovered ? .ble : .wifi
    }
}
public enum PairingRecovery {
    public static func pauseAutomaticConnection(automatic: Bool, paired: Bool, windowOpen: Bool) -> Bool {
        automatic && paired && windowOpen
    }
}

public struct NetworkConfigurationStatus: Decodable, Equatable, Sendable {
    public let requestId: String
    public let phase: String
    public let error: String
    public var finished: Bool { phase == "succeeded" || phase == "failed" }
    public var failureDescription: String {
        switch error {
        case "network or password", "network_or_password": return "无法连接网络，请检查网络名称和密码"
        case "storage failed", "storage_failed", "mode storage failed": return "设备无法保存网络配置"
        default: return "设备未能完成网络验证"
        }
    }
}
