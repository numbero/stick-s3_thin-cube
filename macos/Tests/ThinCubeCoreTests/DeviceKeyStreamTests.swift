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
        _ = stream.receive(session: 42, edge: .init(seq: .max, down: false), canInject: true)
        XCTAssertTrue(stream.receive(session: 42, edge: .init(seq: 0, down: true), canInject: true).press)
    }
}
