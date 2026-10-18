import XCTest
@testable import ThinCubeCore

final class DeviceKeyStreamTests: XCTestCase {
    func testShortTapUsesBothEdgesRatherThanCurrentHeldSnapshot() {
        var stream = DeviceKeyStream()
        let down = stream.receive(session: 42, edge: .init(seq: 1, down: true), canInject: true)
        XCTAssertTrue(down.release) // Release the previous session before a new down.
        XCTAssertTrue(down.press)
        XCTAssertEqual(stream.acknowledgement, 1)
        let repeated = stream.receive(session: 42, edge: .init(seq: 1, down: true), canInject: true)
        XCTAssertNil(repeated.edge)
        XCTAssertFalse(repeated.press)
        XCTAssertFalse(repeated.release)
        let up = stream.receive(session: 42, edge: .init(seq: 2, down: false), canInject: true)
        XCTAssertTrue(up.release)
        XCTAssertFalse(up.press)
        XCTAssertEqual(stream.acknowledgement, 2)
    }

    func testBlockedDownIsAcknowledgedButNeverReplayedAfterPermissionOrModeChanges() {
        var stream = DeviceKeyStream()
        let edge = DeviceKeyEdge(seq: 10, down: true)
        let blocked = stream.receive(session: 42, edge: edge, canInject: false)
        XCTAssertEqual(blocked.edge, edge)
        XCTAssertFalse(blocked.press)
        XCTAssertTrue(blocked.release)
        XCTAssertEqual(stream.acknowledgement, 10)
        XCTAssertFalse(stream.receive(session: 42, edge: edge, canInject: true).press)
        XCTAssertTrue(stream.receive(session: 42, edge: .init(seq: 11, down: false), canInject: true).release)
        XCTAssertTrue(stream.receive(session: 42, edge: .init(seq: 12, down: true), canInject: true).press)
    }

    func testModeExitReleasesWithoutAnEdgeAndNewSessionAcceptsReusedSequence() {
        var stream = DeviceKeyStream()
        _ = stream.receive(session: 42, edge: .init(seq: 1, down: true), canInject: true)
        XCTAssertTrue(stream.receive(session: 42, edge: nil, canInject: false).release)
        let next = stream.receive(session: 43, edge: .init(seq: 1, down: true), canInject: true)
        XCTAssertTrue(next.sessionChanged)
        XCTAssertTrue(next.release)
        XCTAssertTrue(next.press)
    }

    func testSequenceZeroIsValidAtStartupAndAfterWrap() {
        var stream = DeviceKeyStream()
        XCTAssertTrue(stream.receive(session: 42, edge: .init(seq: 0, down: true), canInject: true).press)
        XCTAssertEqual(stream.acknowledgement, 0)
        _ = stream.receive(session: 43, edge: .init(seq: .max, down: false), canInject: true)
        XCTAssertTrue(stream.receive(session: 43, edge: .init(seq: 0, down: true), canInject: true).press)
    }

    func testLegacySourceAndStrictBSourceDecoding() throws {
        let decoder = JSONDecoder()
        XCTAssertEqual(try decoder.decode(DeviceKeyEdge.self, from: Data(#"{"seq":1,"down":true}"#.utf8)).key, .a)
        XCTAssertEqual(try decoder.decode(DeviceKeyEdge.self, from: Data(#"{"seq":1,"down":true,"key":"b"}"#.utf8)).key, .b)
        for key in [#""c""#, "null", "1"] {
            XCTAssertThrowsError(try decoder.decode(DeviceKeyEdge.self, from: Data("{\"seq\":1,\"down\":true,\"key\":\(key)}".utf8)))
        }
    }

    func testBRequiresNegotiationAndOneReleaseDoesNotReleaseAll() {
        var stream = DeviceKeyStream()
        XCTAssertFalse(stream.receive(session: 1, edge: .init(seq: 1, down: true, key: .b), canInject: true).press)
        XCTAssertTrue(stream.receive(session: 1, edge: .init(seq: 2, down: true, key: .a), canInject: true, mode: 2).releaseAll)
        let b = stream.receive(session: 1, edge: .init(seq: 3, down: true, key: .b), canInject: true, mode: 2)
        XCTAssertTrue(b.press); XCTAssertFalse(b.releaseAll)
        let aUp = stream.receive(session: 1, edge: .init(seq: 4, down: false), canInject: true, mode: 2)
        XCTAssertTrue(aUp.release); XCTAssertFalse(aUp.releaseAll)
        XCTAssertEqual(aUp.edge?.key, .a)
        XCTAssertNil(stream.receive(session: 1, edge: .init(seq: 3, down: true, key: .b), canInject: true, mode: 2).edge)
        XCTAssertEqual(stream.acknowledgement, 4)
    }

    func testVoiceContextChangeReleasesBothAndDoesNotReplayOldSequence() {
        var stream = DeviceKeyStream()
        _ = stream.receive(session: 1, edge: .init(seq: 10, down: true, key: .b), canInject: true, voiceSession: "old", mode: 2)
        let next = stream.receive(session: 1, edge: .init(seq: 10, down: true, key: .b), canInject: true, voiceSession: "new", mode: 2)
        XCTAssertTrue(next.releaseAll); XCTAssertNil(next.edge); XCTAssertFalse(next.press)
        XCTAssertTrue(stream.receive(session: 1, edge: nil, canInject: false, voiceSession: "new", mode: 2).releaseAll)
    }
}
