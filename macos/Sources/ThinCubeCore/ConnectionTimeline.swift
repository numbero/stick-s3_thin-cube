import Foundation

/// Typed, once-per-attempt milestones: callers cannot attach credentials or PCM.
public struct ConnectionTimeline: Sendable {
    public enum Stage: String, Codable, Sendable {
        case started, discovered, channel, services, characteristics
        case connectRequested = "connect_requested", credentialStarted = "credential_started"
        case pairingRequired = "pairing_required", pairingSubmitted = "pairing_submitted"
        case controlSubscribed = "control_subscribed", audioSubscribed = "audio_subscribed"
        case info, credential, challenge, authenticated, status
        case controlReady = "control_connected", audioStarted = "audio_started", audioReady = "audio_bridge_ready"
        case heartbeatSent = "heartbeat_sent", firstAudio = "first_audio"
        case redirected, failed, cancelled
    }
    public struct Record: Codable, Sendable {
        public let attempt: UUID, transport: String, stage: Stage
        public let elapsedMs: Double, stepMs: Double
    }
    public private(set) var attempt: UUID?
    public private(set) var records: [Record] = []
    private var started = 0.0, previous = 0.0
    private var transport = "", ended = false
    public init() {}
    public mutating func begin(attempt: UUID, transport: String, now: TimeInterval) -> Record {
        self.attempt = attempt; self.transport = transport; started = now; previous = now
        records = []; ended = false
        return mark(.started, attempt: attempt, now: now)!
    }
    public mutating func mark(_ stage: Stage, attempt: UUID, now: TimeInterval) -> Record? {
        guard self.attempt == attempt, !ended, now.isFinite, now >= previous,
              !records.contains(where: { $0.stage == stage }) else { return nil }
        let row = Record(attempt: attempt, transport: transport, stage: stage,
                         elapsedMs: (now-started)*1000, stepMs: (now-previous)*1000)
        records.append(row); previous = now
        if stage == .failed || stage == .cancelled { ended = true }
        return row
    }
}
