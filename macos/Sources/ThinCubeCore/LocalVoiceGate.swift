/// Explicit stop and connection baselining wait for a standby snapshot.
/// Temporary work blocks output only for its lifetime; rejecting that work
/// cannot leave a stop latched or clear a separate explicit stop.
public struct LocalVoiceGate: Sendable {
    public private(set) var waitingForStandby = false
    public init() {}
    public mutating func waitForStandby() { waitingForStandby = true }
    public mutating func observe(streaming: Bool) {
        if !streaming { waitingForStandby = false }
    }
    public mutating func reset() { waitingForStandby = false }
    public func blocksOutput(switching: Bool, updating: Bool) -> Bool {
        waitingForStandby || switching || updating
    }
}
