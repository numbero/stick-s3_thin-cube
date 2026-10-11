import Foundation

public enum ConnectionStage: String, Sendable {
    case idle, channel, authorization, pairing, verifying, session, connected
    public var channelReady: Bool { ![.idle, .channel].contains(self) }
    public var label: String {
        switch self {
        case .idle: return "未连接"
        case .channel: return "正在连接信道"
        case .authorization: return "正在检查已保存配对"
        case .pairing: return "等待输入配对码"
        case .verifying: return "正在验证配对码"
        case .session: return "正在确认设备会话"
        case .connected: return "已连接"
        }
    }
}

/// Presentation follows the same generation as the connection task. A late
/// response cannot reopen a cancelled prompt or mark a newer attempt ready.
public struct ConnectionProgress: Equatable, Sendable {
    public private(set) var attempt: UUID?
    public private(set) var stage: ConnectionStage = .idle
    public init() {}
    public mutating func begin(_ id: UUID) { attempt = id; stage = .channel }
    @discardableResult public mutating func advance(_ next: ConnectionStage, attempt id: UUID) -> Bool {
        guard attempt == id else { return false }
        let allowed: [ConnectionStage: Set<ConnectionStage>] = [
            .channel: [.authorization], .authorization: [.pairing, .session],
            .pairing: [.verifying], .verifying: [.pairing, .session], .session: [.connected]
        ]
        guard allowed[stage]?.contains(next) == true else { return false }
        stage = next; return true
    }
    public mutating func reset() { attempt = nil; stage = .idle }
}

public struct ConnectionDeviceOption: Identifiable, Equatable, Sendable {
    public enum Presence: Int, Sendable { case connected, discovered, remembered }
    public let id: String
    public let presence: Presence
    public let transports: [String]
    public var label: String {
        let modes = transports.map { $0 == "ble" ? "蓝牙" : "Wi-Fi" }.joined(separator: " / ")
        switch presence {
        case .connected: return id + " · 当前连接 · " + modes
        case .discovered: return id + " · 已发现 · " + modes
        case .remembered: return id + " · 上次选择 · 暂未发现"
        }
    }
    public static func make(endpoints: [ConnectionEndpoint], selectedID: String?,
                            connectedID: String?, connectedTransport: String) -> [Self] {
        var modes: [String: Set<String>] = [:]
        for endpoint in endpoints { modes[endpoint.deviceID, default: []].insert(endpoint.host.hasPrefix("ble://") ? "ble" : "wifi") }
        if let selectedID, !selectedID.isEmpty, modes[selectedID] == nil { modes[selectedID] = [] }
        if let connectedID { modes[connectedID] = [connectedTransport] }
        return modes.map { id, transports in
            Self(id: id, presence: id == connectedID ? .connected : transports.isEmpty ? .remembered : .discovered,
                 transports: transports.sorted())
        }.sorted { $0.presence.rawValue == $1.presence.rawValue ? $0.id < $1.id : $0.presence.rawValue < $1.presence.rawValue }
    }
}
