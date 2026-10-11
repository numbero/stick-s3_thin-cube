import XCTest
@testable import ThinCubeCore

final class ConnectionPresentationTests: XCTestCase {
    func testSavedAuthorizationSkipsCodeAndWaitsForSession() {
        var progress = ConnectionProgress(); let id = UUID(); progress.begin(id)
        XCTAssertFalse(progress.stage.channelReady)
        XCTAssertFalse(progress.advance(.connected, attempt: id))
        XCTAssertTrue(progress.advance(.authorization, attempt: id))
        XCTAssertTrue(progress.stage.channelReady)
        XCTAssertTrue(progress.advance(.session, attempt: id))
        XCTAssertNotEqual(progress.stage, .connected)
        XCTAssertTrue(progress.advance(.connected, attempt: id))
    }
    func testCodeSubmissionAndRetryRetainSameAttempt() {
        var progress = ConnectionProgress(); let id = UUID(); progress.begin(id)
        progress.advance(.authorization, attempt: id); progress.advance(.pairing, attempt: id)
        XCTAssertTrue(progress.advance(.verifying, attempt: id))
        XCTAssertFalse(progress.advance(.connected, attempt: id))
        XCTAssertTrue(progress.advance(.pairing, attempt: id))
        XCTAssertEqual(progress.attempt, id)
        progress.advance(.verifying, attempt: id); progress.advance(.session, attempt: id)
        XCTAssertTrue(progress.advance(.connected, attempt: id))
    }
    func testCancelRejectsLateAuthorizationAndConnectionCallbacks() {
        var progress = ConnectionProgress(); let old = UUID(); progress.begin(old)
        progress.advance(.authorization, attempt: old); progress.advance(.pairing, attempt: old)
        progress.reset()
        XCTAssertFalse(progress.advance(.verifying, attempt: old))
        let fresh = UUID(); progress.begin(fresh)
        XCTAssertFalse(progress.advance(.authorization, attempt: old))
        XCTAssertEqual(progress.stage, .channel)
        XCTAssertTrue(progress.advance(.authorization, attempt: fresh))
    }
    func testDiscoveryMergesModesAndDistinguishesHistoricalSelection() {
        let choices = ConnectionDeviceOption.make(endpoints: [
            ConnectionEndpoint(deviceID: "A", host: "ble://new"),
            ConnectionEndpoint(deviceID: "A", host: "ble://old"),
            ConnectionEndpoint(deviceID: "A", host: "192.168.1.10")
        ], selectedID: "B", connectedID: nil, connectedTransport: "wifi")
        XCTAssertEqual(choices.count, 2)
        XCTAssertEqual(choices[0].presence, .discovered)
        XCTAssertEqual(choices[0].transports, ["ble", "wifi"])
        XCTAssertEqual(choices[1].presence, .remembered)
        XCTAssertTrue(choices[1].transports.isEmpty)
    }
    func testConnectedDeviceRemainsCurrentWithoutAdvertising() {
        let choices = ConnectionDeviceOption.make(endpoints: [], selectedID: "A", connectedID: "A", connectedTransport: "ble")
        XCTAssertEqual(choices.count, 1)
        XCTAssertEqual(choices[0].presence, .connected)
        XCTAssertEqual(choices[0].transports, ["ble"])
    }
    func testExpiredDiscoveryBecomesHistoryAndNewBroadcastRestoresPresence() {
        let offline = ConnectionDeviceOption.make(endpoints: [], selectedID: "A", connectedID: nil, connectedTransport: "ble")
        XCTAssertEqual(offline[0].presence, .remembered)
        let nearby = ConnectionDeviceOption.make(endpoints: [ConnectionEndpoint(deviceID: "A", host: "ble://fresh")], selectedID: "A", connectedID: nil, connectedTransport: "ble")
        XCTAssertEqual(nearby[0].presence, .discovered)
        XCTAssertEqual(nearby[0].id, offline[0].id)
    }
}
