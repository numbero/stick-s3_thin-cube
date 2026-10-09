import Foundation
import AVFAudio

/// A bounded ATT message envelope shared by control and audio characteristics.
public struct BLEFragments {
    private struct Partial { var total: Int; var bytes: [UInt8]; var seen: [Bool]; var received = 0; let started: TimeInterval }
    private var partials: [UInt16: Partial] = [:]
    public let maximum: Int, timeout: TimeInterval
    public init(maximum: Int = 2048, timeout: TimeInterval = 2) { self.maximum = maximum; self.timeout = timeout }
    public mutating func reset() { partials.removeAll() }
    public static func split(_ data: Data, id: UInt16, valueLength: Int) -> [Data] {
        guard !data.isEmpty, data.count <= 2048, valueLength > 8 else { return [] }
        func be(_ v: Int) -> [UInt8] { [UInt8(truncatingIfNeeded: v >> 8), UInt8(truncatingIfNeeded: v)] }
        return stride(from: 0, to: data.count, by: valueLength - 8).map { offset in
            Data([84, 67] + be(Int(id)) + be(offset) + be(data.count)) + data[offset..<min(data.count, offset + valueLength - 8)]
        }
    }
    public mutating func accept(_ fragment: Data, now: TimeInterval) -> Data? {
        partials = partials.filter { now - $0.value.started <= timeout }
        let data = [UInt8](fragment)
        guard data.count > 8, data.count <= 514, data[0] == 84, data[1] == 67 else { return nil }
        func be(_ i: Int) -> Int { Int(data[i]) << 8 | Int(data[i+1]) }
        let id = UInt16(be(2)), offset = be(4), total = be(6), count = data.count - 8
        guard total > 0, total <= maximum, offset + count <= total else { partials.removeValue(forKey: id); return nil }
        if offset == 0, partials[id] == nil {
            if partials.count == 4, let oldest = partials.min(by: { $0.value.started < $1.value.started }) { partials.removeValue(forKey: oldest.key) }
            partials[id] = Partial(total: total, bytes: [UInt8](repeating: 0, count: total), seen: [Bool](repeating: false, count: total), started: now)
        }
        guard var p = partials[id], p.total == total else { partials.removeValue(forKey: id); return nil }
        for i in 0..<count {
            let index = offset + i
            if p.seen[index], p.bytes[index] != data[8+i] { partials.removeValue(forKey: id); return nil }
            if !p.seen[index] { p.received += 1; p.seen[index] = true; p.bytes[index] = data[8+i] }
        }
        if p.received == total { partials.removeValue(forKey: id); return Data(p.bytes) }
        partials[id] = p; return nil
    }
}
public struct AudioFrame {
    public let session: Data, sequence: UInt32, samples: [Float]
    public init(session: Data, sequence: UInt32, samples: [Float]) { self.session = session; self.sequence = sequence; self.samples = samples }
}
public final class BLEAudioDecoder {
    private let inputFormat = AVAudioFormat(commonFormat: .pcmFormatInt16, sampleRate: 16000, channels: 1, interleaved: false)!
    private let outputFormat = AVAudioFormat(commonFormat: .pcmFormatFloat32, sampleRate: 48000, channels: 1, interleaved: false)!
    private var converter: AVAudioConverter?
    private var cookie = Data(), expected: UInt32?
    public init() {}
    public func reset() { cookie = Data(); expected = nil; converter = nil }
    public func decodeFrames(_ data: Data, session: Data) -> [AudioFrame] {
        guard data.count == 196, data.prefix(4) == Data("TCB3".utf8) else {
            return decode(data, session: session).map { [$0] } ?? []
        }
        let bytes = [UInt8](data)
        func be(_ i: Int) -> UInt32 { UInt32(bytes[i]) << 24 | UInt32(bytes[i+1]) << 16 | UInt32(bytes[i+2]) << 8 | UInt32(bytes[i+3]) }
        let first = be(20)
        guard session.count == 16, data[4..<20] == session, be(24) == first &* 160,
              IMAADPCM.decode160(Data(data[28..<112])) != nil,
              IMAADPCM.decode160(Data(data[112..<196])) != nil else { return [] }
        if cookie == session, let expected, Int32(bitPattern: first &- expected) < 0 { return [] }
        var frames: [AudioFrame] = []
        for index in 0..<2 {
            let sequence = first &+ UInt32(index)
            func bigEndian(_ value: UInt32) -> Data { Data((0..<4).reversed().map { UInt8(truncatingIfNeeded: value >> ($0*8)) }) }
            let packet = Data("TCB2".utf8) + session + bigEndian(sequence) + bigEndian(sequence &* 160) + data[(28+index*84)..<(112+index*84)]
            guard let frame = decode(packet, session: session) else { return [] }
            frames.append(frame)
        }
        return frames
    }
    public func decode(_ data: Data, session: Data) -> AudioFrame? {
        let b = [UInt8](data)
        guard b.count >= 28, session.count == 16, data[4..<20] == session else { return nil }
        let samples: [Int16]
        if b.count == 112, data.prefix(4) == Data("TCB2".utf8), let decoded = IMAADPCM.decode160(Data(data.dropFirst(28))) { samples = decoded }
        else if b.count == 348, data.prefix(4) == Data("TCB1".utf8) {
            samples = (0..<160).map { Int16(bitPattern: UInt16(b[28+$0*2]) | UInt16(b[29+$0*2]) << 8) }
        } else { return nil }
        func be(_ i: Int) -> UInt32 { UInt32(b[i]) << 24 | UInt32(b[i+1]) << 16 | UInt32(b[i+2]) << 8 | UInt32(b[i+3]) }
        let seq = be(20)
        guard be(24) == seq &* 160 else { return nil }
        // Stale/reordered input cannot feed the stateful sample-rate converter.
        if cookie == session, let expected, Int32(bitPattern: seq &- expected) < 0 { return nil }
        if cookie != session || expected != seq {
            converter = AVAudioConverter(from: inputFormat, to: outputFormat); converter?.primeMethod = .none
        }
        guard let converter, let input = AVAudioPCMBuffer(pcmFormat: inputFormat, frameCapacity: 160),
              let output = AVAudioPCMBuffer(pcmFormat: outputFormat, frameCapacity: 480) else { return nil }
        input.frameLength = 160
        for i in 0..<160 { input.int16ChannelData![0][i] = samples[i] }
        var supplied = false, error: NSError?
        let status = converter.convert(to: output, error: &error) { _, state in
            if supplied { state.pointee = .noDataNow; return nil }
            supplied = true; state.pointee = .haveData; return input
        }
        guard error == nil, status != .error, output.frameLength == 480 else { return nil }
        cookie = session; expected = seq &+ 1
        return AudioFrame(session: session, sequence: seq, samples: Array(UnsafeBufferPointer(start: output.floatChannelData![0], count: 480)))
    }
}
