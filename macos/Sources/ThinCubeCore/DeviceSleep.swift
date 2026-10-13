import Foundation

public struct DeviceSleepNotice: Codable, Equatable, Sendable {
    public let id: UInt32
    public let phase: String
    public init(id: UInt32, phase: String) { self.id = id; self.phase = phase }
}

/// Only a fresh, acknowledged notice from the current authenticated epoch can
/// turn the following disconnect into an expected sleep. Ordinary outages keep
/// their normal error/recovery behavior; cancelled and stale notices do not count.
public struct DeviceSleepTracker: Sendable {
    private var notice: DeviceSleepNotice?
    private var epoch: UInt32 = 0
    private var announcedAt: TimeInterval = 0
    private var acknowledged = false
    public private(set) var sleeping = false
    public init() {}
    public mutating func reset() { self = Self() }
    public mutating func observe(_ value: DeviceSleepNotice?, epoch: UInt32, now: TimeInterval) {
        sleeping = false
        guard epoch != 0, let value, value.id != 0, value.phase == "preparing" else {
            notice = nil; acknowledged = false; return
        }
        if self.epoch != epoch || notice != value {
            self.epoch = epoch; notice = value; announcedAt = now; acknowledged = false
        }
    }
    public var acknowledgementNeeded: UInt32? { acknowledged ? nil : notice?.id }
    @discardableResult public mutating func didAcknowledge(id: UInt32, epoch: UInt32, now: TimeInterval) -> Bool {
        guard self.epoch == epoch, notice?.id == id, now >= announcedAt, now - announcedAt <= 5 else { return false }
        acknowledged = true; return true
    }
    @discardableResult public mutating func didDisconnect(now: TimeInterval) -> Bool {
        if sleeping { return true }
        sleeping = acknowledged && notice != nil && now >= announcedAt && now - announcedAt <= 5
        notice = nil; acknowledged = false; return sleeping
    }
}
