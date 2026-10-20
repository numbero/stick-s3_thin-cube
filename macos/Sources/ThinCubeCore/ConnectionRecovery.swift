import Foundation

/// Connection intent survives transport failures; deliberate disconnect is a
/// separate event. Delays are bounded so an offline device can return later.
public struct ConnectionRecovery: Sendable {
    public enum Suspension: Hashable, Sendable { case sleep, lock, inactiveSession, network, update, termination }
    public private(set) var enabled: Bool
    public private(set) var needsUserAction = false
    public private(set) var failures = 0
    private var suspensions: Set<Suspension> = []
    private var lastAvailabilityAttempt: TimeInterval?
    public init(enabled: Bool) { self.enabled = enabled }
    public var canAttempt: Bool { enabled && !needsUserAction && suspensions.isEmpty }
    public mutating func request() { enabled = true; needsUserAction = false; failures = 0; lastAvailabilityAttempt = nil }
    public mutating func disconnect() { enabled = false; needsUserAction = false; failures = 0; lastAvailabilityAttempt = nil }
    public mutating func suspend(_ reason: Suspension) { suspensions.insert(reason) }
    public mutating func resume(_ reason: Suspension) { suspensions.remove(reason) }
    public mutating func requireUserAction() { needsUserAction = true }
    public mutating func didConnect() { failures = 0; needsUserAction = false; lastAvailabilityAttempt = nil }
    /// Fresh discovery can bypass an offline backoff even at the same address.
    /// Repeated advertisements must not turn a transport failure into a retry loop.
    public mutating func deviceAvailable(now: TimeInterval) -> Bool {
        guard canAttempt, lastAvailabilityAttempt.map({ now - $0 >= 3 }) ?? true else { return false }
        lastAvailabilityAttempt = now; failures = 0
        return true
    }
    public mutating func nextDelay() -> TimeInterval? {
        guard canAttempt else { return nil }
        let delays: [TimeInterval] = [1, 2, 4, 8, 15, 30]
        let delay = delays[min(failures, delays.count - 1)]
        failures = min(failures + 1, delays.count)
        return delay
    }
}

/// A discovered IP can replace a stale address only for the selected identity.
public struct ConnectionTarget: Equatable, Sendable {
    public var host: String
    public var deviceID: String?
    public init(host: String, deviceID: String?) { self.host = host; self.deviceID = deviceID }
    public func accepts(_ foundID: String) -> Bool { deviceID == nil || deviceID == foundID }
    @discardableResult public mutating func discovered(id: String, host: String) -> Bool {
        guard id == deviceID, self.host != host else { return false }
        self.host = host
        return true
    }
}
