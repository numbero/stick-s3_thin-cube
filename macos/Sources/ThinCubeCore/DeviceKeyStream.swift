public enum DeviceButton: String, Codable, CaseIterable { case a, b }

public struct DeviceKeyEdge: Decodable, Equatable {
    public let seq: UInt32
    public let down: Bool
    public let key: DeviceButton
    public init(seq: UInt32, down: Bool, key: DeviceButton = .a) { self.seq = seq; self.down = down; self.key = key }
    private enum CodingKeys: String, CodingKey { case seq, down, key }
    public init(from decoder: Decoder) throws {
        let values = try decoder.container(keyedBy: CodingKeys.self)
        seq = try values.decode(UInt32.self, forKey: .seq)
        down = try values.decode(Bool.self, forKey: .down)
        key = values.contains(.key) ? try values.decode(DeviceButton.self, forKey: .key) : .a
    }
}

/// Deduplicate reliable device edges independently of the current held snapshot.
/// Even a blocked edge must be acknowledged, and must not replay when enabled.
public struct DeviceKeyStream {
    public struct Update {
        public let sessionChanged: Bool
        public let edge: DeviceKeyEdge?
        public let release: Bool
        public let releaseAll: Bool
        public let press: Bool
    }
    public private(set) var acknowledgement: UInt32 = 0
    private var session: UInt32?
    private var lastSequence: UInt32?
    private var voiceSession: String?
    private var mode = 1
    public init() {}

    public mutating func receive(session: UInt32, edge: DeviceKeyEdge?, canInject: Bool,
                                 voiceSession: String? = nil, mode: Int = 1) -> Update {
        let newEpoch = self.session != session
        let changed = newEpoch || self.voiceSession != voiceSession || self.mode != mode
        if newEpoch { lastSequence = nil; acknowledgement = 0 }
        if changed {
            self.session = session; self.voiceSession = voiceSession; self.mode = mode
        }
        let fresh = edge.flatMap { edge -> DeviceKeyEdge? in
            if let lastSequence, Int32(bitPattern: edge.seq &- lastSequence) <= 0 { return nil }
            return edge
        }
        if let fresh { lastSequence = fresh.seq; acknowledgement = fresh.seq }
        return Update(sessionChanged: changed, edge: fresh,
                      release: changed || !canInject || fresh?.down == false,
                      releaseAll: changed || !canInject,
                      press: canInject && fresh?.down == true && (fresh?.key == .a || mode == 2))
    }
}
