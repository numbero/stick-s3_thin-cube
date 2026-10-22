import XCTest
@testable import ThinCubeCore

final class LocalVoiceGateTests: XCTestCase {
    func testRejectedSwitchUnblocksWithoutAStandbySnapshot() {
        var gate = LocalVoiceGate()
        XCTAssertTrue(gate.blocksOutput(switching: true, updating: false))
        gate.observe(streaming: true)
        // The device rejects the switch while its current voice session lives.
        XCTAssertFalse(gate.blocksOutput(switching: false, updating: false))
        XCTAssertFalse(gate.waitingForStandby)
    }

    func testRejectedSwitchCannotCancelAnExplicitStop() {
        var gate = LocalVoiceGate(); gate.waitForStandby()
        XCTAssertTrue(gate.blocksOutput(switching: true, updating: false))
        gate.observe(streaming: true)
        XCTAssertTrue(gate.blocksOutput(switching: false, updating: false))
        gate.observe(streaming: false)
        XCTAssertFalse(gate.blocksOutput(switching: false, updating: false))
    }

    func testConnectionBaselineWaitsForStandbyAndKeepsOtherHolds() {
        var gate = LocalVoiceGate(); gate.waitForStandby()
        for _ in 0..<3 { gate.observe(streaming: true) }
        XCTAssertTrue(gate.waitingForStandby)
        gate.observe(streaming: false)
        XCTAssertTrue(gate.blocksOutput(switching: true, updating: false))
        XCTAssertTrue(gate.blocksOutput(switching: false, updating: true))
        XCTAssertFalse(gate.blocksOutput(switching: false, updating: false))
    }

    func testInterruptedKeysAreAcknowledgedAndNotReplayedAfterSwitchRejection() {
        let gate = LocalVoiceGate()
        var stream = DeviceKeyStream()
        let down = DeviceKeyEdge(seq: 10, down: true, key: .b)
        let blocked = stream.receive(session: 42, edge: down,
            canInject: !gate.blocksOutput(switching: true, updating: false), voiceSession: "current", mode: 2)
        XCTAssertFalse(blocked.press); XCTAssertTrue(blocked.releaseAll)
        XCTAssertEqual(stream.acknowledgement, 10)
        let resumed = stream.receive(session: 42, edge: down,
            canInject: !gate.blocksOutput(switching: false, updating: false), voiceSession: "current", mode: 2)
        XCTAssertNil(resumed.edge); XCTAssertFalse(resumed.press)
        _ = stream.receive(session: 42, edge: .init(seq: 11, down: false, key: .b), canInject: true, voiceSession: "current", mode: 2)
        XCTAssertTrue(stream.receive(session: 42, edge: .init(seq: 12, down: true, key: .b), canInject: true, voiceSession: "current", mode: 2).press)
    }

    func testResetAndIndependentTemporaryHolds() {
        var gate = LocalVoiceGate(); gate.waitForStandby(); gate.reset()
        XCTAssertFalse(gate.waitingForStandby)
        XCTAssertTrue(gate.blocksOutput(switching: true, updating: true))
        XCTAssertTrue(gate.blocksOutput(switching: false, updating: true))
        XCTAssertFalse(gate.blocksOutput(switching: false, updating: false))
    }
}
