import XCTest
@testable import ThinCubeCore
final class BLEProtocolTests: XCTestCase {
    func testFragmentsAtSmallAndLargeMTU() {
        let bytes = Data((0..<1600).map { UInt8(truncatingIfNeeded: $0) })
        for size in [20, 182, 244, 512] {
            var decoder = BLEFragments(); let fragments = BLEFragments.split(bytes, id: 65535, valueLength: size)
            var result: Data?
            for (i, piece) in fragments.enumerated() { result = decoder.accept(piece, now: Double(i)/100) ?? result }
            XCTAssertEqual(result, bytes)
        }
    }
    func testDuplicatesReorderMissingExpiryAndMalformed() {
        let data = Data(repeating: 9, count: 100), fragments = BLEFragments.split(data, id: 1, valueLength: 28)
        var d = BLEFragments(timeout: 0.2)
        XCTAssertNil(d.accept(fragments[0], now: 0)); XCTAssertNil(d.accept(fragments[0], now: 0.01))
        for i in [2, 1, 3] { XCTAssertNil(d.accept(fragments[i], now: 0.02)) }
        XCTAssertEqual(d.accept(fragments[4], now: 0.03), data)
        XCTAssertNil(d.accept(fragments[0], now: 1)); XCTAssertNil(d.accept(fragments[1], now: 2))
        var bad = fragments[0]; bad[6] = 255; XCTAssertNil(d.accept(bad, now: 3))
        XCTAssertTrue(BLEFragments.split(data, id: 0, valueLength: 8).isEmpty)
        XCTAssertTrue(BLEFragments.split(Data(repeating: 0, count: 2049), id: 0, valueLength: 20).isEmpty)
    }
    private func packet(_ seq: UInt32, cookie: Data, value: Int16 = 10000) -> Data {
        func be(_ v: UInt32) -> [UInt8] { (0..<4).reversed().map { UInt8(truncatingIfNeeded: v >> ($0*8)) } }
        var bytes = Data("TCB1".utf8) + cookie + Data(be(seq) + be(seq &* 160))
        for _ in 0..<160 { bytes.append(UInt8(truncatingIfNeeded: value)); bytes.append(UInt8(truncatingIfNeeded: value >> 8)) }
        return bytes
    }
    func testAudioResamplingSessionsLossAndWrap() {
        let decoder = BLEAudioDecoder(), cookie = Data(repeating: 3, count: 16)
        let first = decoder.decode(packet(UInt32.max, cookie: cookie), session: cookie)
        XCTAssertEqual(first?.samples.count, 480)
        XCTAssertNotNil(decoder.decode(packet(0, cookie: cookie), session: cookie))
        XCTAssertNil(decoder.decode(packet(UInt32.max, cookie: cookie), session: cookie))
        let afterGap = decoder.decode(packet(3, cookie: cookie), session: cookie)
        XCTAssertEqual(afterGap?.samples.count, 480)
        XCTAssertNil(decoder.decode(packet(4, cookie: cookie), session: Data(repeating: 4, count: 16)))
        decoder.reset(); XCTAssertNotNil(decoder.decode(packet(0, cookie: cookie, value: -32768), session: cookie))
    }
    func testIndependentADPCMBlocksAndValidation() {
        let zero = Data(repeating: 0, count: 84)
        XCTAssertEqual(IMAADPCM.decode160(zero), [Int16](repeating: 0, count: 160))
        XCTAssertNil(IMAADPCM.decode160(zero.dropLast()))
        for (at, value) in [(2, UInt8(89)), (3, UInt8(1)), (83, UInt8(0xf0))] {
            var bad = zero; bad[at] = value; XCTAssertNil(IMAADPCM.decode160(bad))
        }
        let cookie = Data(repeating: 3, count: 16), decoder = BLEAudioDecoder()
        for seq in [UInt32(0), 1, 9] {
            var envelope = packet(seq, cookie: cookie).prefix(28); envelope.replaceSubrange(0..<4, with: Data("TCB2".utf8))
            let frame = decoder.decode(Data(envelope) + zero, session: cookie)
            XCTAssertEqual(frame?.samples.count, 480)
            XCTAssertEqual(frame?.samples.max(), 0)
        }
    }
    func testTwoBlockPacketsLossWrapAndAtomicValidation() {
        let cookie = Data(repeating: 3, count: 16), decoder = BLEAudioDecoder()
        func pair(_ seq: UInt32) -> Data {
            var envelope = packet(seq, cookie: cookie).prefix(28)
            envelope.replaceSubrange(0..<4, with: Data("TCB3".utf8))
            return Data(envelope) + Data(repeating: 0, count: 168)
        }
        XCTAssertEqual(decoder.decodeFrames(pair(UInt32.max), session: cookie).map(\.sequence), [UInt32.max, 0])
        XCTAssertTrue(decoder.decodeFrames(pair(UInt32.max), session: cookie).isEmpty)
        XCTAssertEqual(decoder.decodeFrames(pair(3), session: cookie).map(\.sequence), [3, 4])
        var bad = pair(5); bad[114] = 89
        XCTAssertTrue(decoder.decodeFrames(bad, session: cookie).isEmpty)
        XCTAssertEqual(decoder.decodeFrames(pair(5), session: cookie).map(\.samples.count), [480, 480])
        XCTAssertTrue(decoder.decodeFrames(pair(7), session: Data(repeating: 4, count: 16)).isEmpty)
    }
}
