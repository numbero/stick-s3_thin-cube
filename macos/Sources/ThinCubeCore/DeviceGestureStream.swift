public enum ShortcutTrigger: String, CaseIterable {
    case a, b, doubleFlick = "double_flick"
    public init(_ button: DeviceButton) { self = button == .a ? .a : .b }
    public var label: String { switch self { case .a: return "A 键"; case .b: return "B 键"; case .doubleFlick: return "连续双甩" } }
}

public struct DeviceGestureEvent: Decodable, Equatable {
    public let seq: UInt32
    public let type: String
    public let ageMs: UInt32
    public init(seq: UInt32, type: String = "double_flick", ageMs: UInt32 = 0) { self.seq = seq; self.type = type; self.ageMs = ageMs }
}

/// Consume each event once, including events discarded while editing, blocked
/// or entering a new context. Acknowledgement never implies key injection.
public struct DeviceGestureStream {
    public struct Update { public let event: DeviceGestureEvent?; public let fire: Bool }
    public private(set) var acknowledgement: UInt32 = 0
    private var epoch: UInt32?, voiceSession: String?, mode = 0
    private var sequence: UInt32?
    public init() {}
    public mutating func receive(epoch: UInt32, voiceSession: String?, mode: Int,
                                 event: DeviceGestureEvent?, canInject: Bool) -> Update {
        let newEpoch = self.epoch != epoch
        let changed = newEpoch || self.voiceSession != voiceSession || self.mode != mode
        if newEpoch { sequence = nil; acknowledgement = 0 }
        self.epoch = epoch; self.voiceSession = voiceSession; self.mode = mode
        guard let event, sequence.map({ Int32(bitPattern: event.seq &- $0) > 0 }) ?? true else {
            return Update(event: nil, fire: false)
        }
        sequence = event.seq; acknowledgement = event.seq
        return Update(event: event, fire: canInject && !changed && voiceSession != nil && mode == 1 &&
                      event.type == "double_flick" && event.ageMs <= 750)
    }
}
