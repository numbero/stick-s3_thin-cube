import Foundation

public struct AudioPacket {
    public let sequence: UInt32
    public let samples: [Float]
    public init?(data: Data, session: Data) {
        guard data.count == 988, session.count == 16,
              data.prefix(4) == Data("TCM1".utf8), data[4..<20] == session else { return nil }
        func be(_ i: Int) -> UInt32 {
            (UInt32(data[i]) << 24) | (UInt32(data[i+1]) << 16) | (UInt32(data[i+2]) << 8) | UInt32(data[i+3])
        }
        sequence = be(20)
        guard be(24) == sequence &* 480 else { return nil }
        samples = stride(from: 28, to: 988, by: 2).map {
            Float(Int16(bitPattern: UInt16(data[$0]) | UInt16(data[$0+1]) << 8)) / 32768
        }
    }
}
public extension Data {
    init?(hex: String) {
        guard hex.count == 32 else { return nil }
        var bytes = [UInt8](); bytes.reserveCapacity(16)
        var index = hex.startIndex
        for _ in 0..<16 {
            let end = hex.index(index, offsetBy: 2)
            guard let value = UInt8(hex[index..<end], radix: 16) else { return nil }
            bytes.append(value); index = end
        }
        self.init(bytes)
    }
}
