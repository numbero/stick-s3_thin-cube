import Foundation

/// A lost reply is distinct from acceptance: the device may already be restarting.
/// Only an explicit reconnect ends either kind of maintenance pause.
public struct PhoneUpgradeSession: Equatable, Sendable {
    public enum Phase: String, Sendable { case idle, requesting, waitingForPhone, unconfirmed }
    public private(set) var phase: Phase
    public private(set) var requestID: UUID?
    public init(restoring phase: Phase = .idle) {
        self.phase = phase == .requesting ? .unconfirmed : phase
    }
    public var pausesConnection: Bool { phase != .idle }
    public mutating func begin() -> UUID? {
        guard phase == .idle else { return nil }
        let id = UUID(); requestID = id; phase = .requesting; return id
    }
    @discardableResult public mutating func accepted(_ id: UUID) -> Bool { complete(id, phase: .waitingForPhone) }
    @discardableResult public mutating func unconfirmed(_ id: UUID) -> Bool { complete(id, phase: .unconfirmed) }
    @discardableResult public mutating func rejected(_ id: UUID) -> Bool { complete(id, phase: .idle) }
    private mutating func complete(_ id: UUID, phase: Phase) -> Bool {
        guard self.phase == .requesting, requestID == id else { return false }
        self.phase = phase; requestID = nil; return true
    }
    public mutating func finish() { phase = .idle; requestID = nil }
    public static func rejectionDescription(_ code: String) -> String {
        switch code {
        case "busy": return "设备正在配对、配网、校准或处理其他操作，请结束后再试。"
        case "boot_unverified": return "设备启动健康检查尚未通过，请等待后再进入手机升级。"
        case "storage_failed": return "设备无法保存临时升级请求，请检查后重试。"
        case "request_conflict": return "设备已有另一条手机升级请求，请按设备提示操作。"
        case "invalid_request": return "设备未接受手机升级请求，请重新连接后重试。"
        default: return "设备未接受手机升级请求：" + code
        }
    }
}

public struct PhoneUpgradeAcceptance: Decodable, Sendable {
    public let accepted: Bool
}
