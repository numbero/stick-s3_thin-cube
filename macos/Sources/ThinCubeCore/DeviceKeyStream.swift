public struct DeviceKeyEdge: Decodable, Equatable {
    public let seq: UInt32
    public let down: Bool
    public init(seq: UInt32, down: Bool) { self.seq = seq; self.down = down }
}

/// Deduplicate reliable device edges independently of the current held snapshot.
/// Even a blocked edge must be acknowledged, and must not replay when enabled.
public struct DeviceKeyStream {
    public struct Update {
        public let sessionChanged: Bool
        public let edge: DeviceKeyEdge?
        public let release: Bool
        public let press: Bool
    }
    public private(set) var acknowledgement: UInt32 = 0
    private var session: UInt32?
    private var lastSequence: UInt32?
    public init() {}

    public mutating func receive(session: UInt32, edge: DeviceKeyEdge?, canInject: Bool) -> Update {
        let changed = self.session != session
        if changed {
            self.session = session; lastSequence = nil; acknowledgement = 0
        }
        let fresh = edge.flatMap { $0.seq == lastSequence ? nil : $0 }
        if let fresh { lastSequence = fresh.seq; acknowledgement = fresh.seq }
        return Update(sessionChanged: changed, edge: fresh,
                      release: changed || !canInject || fresh?.down == false,
                      press: canInject && fresh?.down == true)
    }
}
