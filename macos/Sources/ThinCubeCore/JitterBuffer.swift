import Foundation

// Bounded reorder storage, 40ms pre-roll, silence on loss, and occupancy-based clock correction.
// The render callback uses try(), so it never waits on the UDP writer.
public final class JitterBuffer: @unchecked Sendable {
    public struct Statistics: Sendable {
        public let receivedFrames: UInt64, renderedSamples: UInt64, concealedFrames: UInt64, waitingSamples: UInt64
        public let peak: Float
    }
    private let lock = NSLock()
    private var pcm = [Float](repeating: 0, count: 64 * 480)
    private var slots = [UInt32?](repeating: nil, count: 64)
    private var expected: UInt32?
    private var offset = 0.0, playing = false, started = false
    private var cookie = Data()
    private var occupancy = 0
    private var floor: UInt32 = 0
    public private(set) var lostFrames: UInt64 = 0
    private var receivedFrames: UInt64 = 0, renderedSamples: UInt64 = 0
    private var waitingSamples: UInt64 = 0
    private var peak: Float = 0

    public init() {}
    public func statistics() -> Statistics {
        lock.lock(); defer { lock.unlock() }
        return Statistics(receivedFrames: receivedFrames, renderedSamples: renderedSamples, concealedFrames: lostFrames, waitingSamples: waitingSamples, peak: peak)
    }
    public func setSession(_ hex: String?) {
        let next = hex.flatMap(Data.init(hex:)) ?? Data()
        lock.lock(); defer { lock.unlock() }
        guard next != cookie else { return }
        cookie = next; reset()
    }
    public func flush() { lock.lock(); reset(); lock.unlock() }
    private func reset() {
        slots = [UInt32?](repeating: nil, count: 64); expected = nil
        offset = 0; playing = false; started = false; occupancy = 0; floor = 0
    }
    public func accept(_ data: Data) {
        lock.lock(); defer { lock.unlock() }
        guard let packet = AudioPacket(data: data, session: cookie) else { return }
        store(sequence: packet.sequence, samples: packet.samples)
    }
    public func accept(_ frame: AudioFrame) {
        lock.lock(); defer { lock.unlock() }
        guard cookie.count == 16, frame.session == cookie, frame.samples.count == 480 else { return }
        store(sequence: frame.sequence, samples: frame.samples)
    }
    private func store(sequence: UInt32, samples: [Float]) {
        if let base = expected {
            let distance = Int32(bitPattern: sequence &- base)
            if distance < 0 {
                guard !playing, !started, distance > -64 else { return }
                expected = sequence
            } else if distance >= 64 {
                reset(); expected = sequence
            }
        } else {
            if started && Int32(bitPattern: sequence &- floor) < 0 { return }
            expected = sequence
        }
        let slot = Int(sequence % 64)
        if slots[slot] == sequence { return }
        receivedFrames &+= 1
        peak = samples.reduce(0) { max($0, abs($1)) }
        if slots[slot] == nil { occupancy += 1 }
        slots[slot] = sequence
        let base = slot * 480
        for i in 0..<480 { pcm[base+i] = samples[i] }
    }
    public func render(into output: UnsafeMutablePointer<Float>, count: Int) {
        output.initialize(repeating: 0, count: count)
        guard lock.try() else { return }
        defer { lock.unlock() }
        guard cookie.count == 16 else { return }
        guard let first = expected else { waitingSamples &+= UInt64(count); return }
        if !playing {
            guard occupancy >= 4 else { waitingSamples &+= UInt64(count); return }
            playing = true; started = true
        }
        var seq = first
        // +/-0.2% accommodates independent device clocks without periodic buffer resets.
        let adjustment = max(-0.002, min(0.002, Double(occupancy - 4) * 0.0002))
        let step = 1 + adjustment
        for i in 0..<count {
            let slot = Int(seq % 64), sample = Int(offset)
            if slots[slot] == seq {
                renderedSamples &+= 1
                let a = pcm[slot*480+sample]
                let nextSample: Float
                if sample < 479 { nextSample = pcm[slot*480+sample+1] }
                else { let next = seq &+ 1; let index = Int(next % 64); nextSample = slots[index] == next ? pcm[index*480] : a }
                output[i] = a + (nextSample-a) * Float(offset-Double(sample))
            }
            offset += step
            if offset >= 480 {
                offset -= 480
                if slots[slot] == seq { slots[slot] = nil; occupancy -= 1 }
                else { lostFrames &+= 1 }
                seq &+= 1
            }
        }
        expected = seq
        if occupancy == 0 { playing = false; offset = 0; floor = seq; expected = nil }
    }
}
