import XCTest
@testable import ThinCubeCore

final class AudioTests: XCTestCase {
    private let session = "11111111111111111111111111111111"
    private func packet(_ seq: UInt32, value: Int16 = 8192) -> Data {
        var data = Data("TCM1".utf8); data.append(Data(hex: session)!)
        for value in [seq, seq &* 480] { data.append(contentsOf: [UInt8(truncatingIfNeeded: value >> 24), UInt8(truncatingIfNeeded: value >> 16), UInt8(truncatingIfNeeded: value >> 8), UInt8(truncatingIfNeeded: value)]) }
        for _ in 0..<480 { let value = UInt16(bitPattern: value); data.append(UInt8(truncatingIfNeeded: value)); data.append(UInt8(truncatingIfNeeded: value >> 8)) }
        return data
    }
    private func render(_ buffer: JitterBuffer, _ count: Int = 480) -> [Float] {
        var samples = [Float](repeating: -1, count: count)
        samples.withUnsafeMutableBufferPointer { buffer.render(into: $0.baseAddress!, count: count) }
        return samples
    }
    func testWireFormatSignedSamplesAndSessionIsolation() {
        let data = packet(0, value: -32768)
        XCTAssertEqual(AudioPacket(data: data, session: Data(hex: session)!)?.samples.first, -1)
        XCTAssertNil(AudioPacket(data: data, session: Data(repeating: 0, count: 16)))
        XCTAssertNil(AudioPacket(data: data.dropLast(), session: Data(hex: session)!))
        var badClock = data; badClock[27] = 1
        XCTAssertNil(AudioPacket(data: badClock, session: Data(hex: session)!))
    }
    func testReordersPacketsAndIgnoresDuplicates() {
        let buffer = JitterBuffer(); buffer.setSession(session)
        for seq: UInt32 in [2,0,3,1,1] { buffer.accept(packet(seq, value: Int16((seq+1)*4096))) }
        XCTAssertEqual(render(buffer).first!, 0.125, accuracy: 0.00001)
        XCTAssertEqual(render(buffer).first!, 0.25, accuracy: 0.001)
    }
    func testLossProducesSilenceAndRecoversAfterLongGap() {
        let buffer = JitterBuffer(); buffer.setSession(session)
        for seq: UInt32 in [0,1,3,4] { buffer.accept(packet(seq)) }
        let samples = render(buffer, 6 * 480)
        XCTAssertTrue(samples[1000..<1300].allSatisfy { $0 == 0 })
        XCTAssertTrue(render(buffer).allSatisfy { $0 == 0 })
        for seq: UInt32 in [50,51,52,53] { buffer.accept(packet(seq)) }
        XCTAssertEqual(render(buffer).first!, 0.25, accuracy: 0.001)
    }
    func testOldSessionCannotReplayAfterMuteOrNewSession() {
        let buffer = JitterBuffer(); buffer.setSession(session)
        for seq: UInt32 in 0..<4 { buffer.accept(packet(seq)) }
        buffer.setSession(nil)
        for seq: UInt32 in 4..<8 { buffer.accept(packet(seq)) }
        XCTAssertTrue(render(buffer).allSatisfy { $0 == 0 })
        buffer.setSession("22222222222222222222222222222222")
        for seq: UInt32 in 8..<12 { buffer.accept(packet(seq)) }
        XCTAssertTrue(render(buffer).allSatisfy { $0 == 0 })
    }
    func testSequenceWrapAndBoundedClockDrift() {
        let buffer = JitterBuffer(); buffer.setSession(session)
        let first = UInt32.max - 2
        for i: UInt32 in 0..<4 { buffer.accept(packet(first &+ i)) }
        XCTAssertEqual(render(buffer).first!, 0.25, accuracy: 0.001)
        let sustained = JitterBuffer(); sustained.setSession(session)
        for seq: UInt32 in 0..<4 { sustained.accept(packet(seq)) }
        for seq: UInt32 in 4..<2004 {
            sustained.accept(packet(seq)); let samples = render(sustained, seq % 10 == 0 ? 479 : 480)
            XCTAssertGreaterThan(samples.reduce(0,+)/Float(samples.count), 0.24)
        }
    }
    func testFirmwarePreflightRejectsFullFlashAndWrongBoard() throws {
        var image = Data(repeating: 0, count: 1024); image[0] = 0xe9; image[12] = 9
        image.replaceSubrange(32..<36, with: [0x32,0x54,0xcd,0xab])
        let id = Data("ThinCube-StickS3-v1".utf8); image.replaceSubrange(288..<288+id.count, with: id)
        let version = Data("v0.3.0".utf8); image.replaceSubrange(320..<320+version.count, with: version)
        XCTAssertEqual(try FirmwareImage(data: image).version, "v0.3.0")
        image[12] = 0; XCTAssertThrowsError(try FirmwareImage(data: image))
        XCTAssertThrowsError(try FirmwareImage(data: Data(repeating: 0xff, count: 8*1024*1024)))
    }
    func testBuiltFirmwareIdentityWhenProvided() throws {
        guard let path = ProcessInfo.processInfo.environment["THINCUBE_TEST_FIRMWARE"] else { throw XCTSkip("Set THINCUBE_TEST_FIRMWARE to validate a real build") }
        let image = try FirmwareImage(data: Data(contentsOf: URL(fileURLWithPath: path)))
        if let expected = ProcessInfo.processInfo.environment["THINCUBE_TEST_VERSION"] { XCTAssertEqual(image.version, expected) }
        else { XCTAssertFalse(image.version.isEmpty) }
        XCTAssertEqual(image.sha256.count, 64)
    }
}
