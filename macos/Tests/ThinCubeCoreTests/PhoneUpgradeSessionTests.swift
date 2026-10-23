import XCTest
@testable import ThinCubeCore

final class PhoneUpgradeSessionTests: XCTestCase {
    func testPhoneEntryRoutesOverBLEWhileFirmwareUploadStillRequiresWiFi() {
        XCTAssertEqual(DeviceBLEOperation.forPath("api/maintenance"), "maintenance")
        XCTAssertNil(DeviceBLEOperation.forPath("ota/begin"))
        XCTAssertNil(DeviceBLEOperation.forPath("ota"))
    }

    func testOnlyExplicitBooleanAcceptanceCanConfirmEntry() throws {
        let accepted = try JSONDecoder().decode(PhoneUpgradeAcceptance.self, from: Data(#"{"accepted":true}"#.utf8))
        XCTAssertTrue(accepted.accepted)
        let rejected = try JSONDecoder().decode(PhoneUpgradeAcceptance.self, from: Data(#"{"accepted":false}"#.utf8))
        XCTAssertFalse(rejected.accepted)
        for response in [#"{"ok":true}"#, #"{"accepted":1}"#, #"{"accepted":"true"}"#] {
            XCTAssertThrowsError(try JSONDecoder().decode(PhoneUpgradeAcceptance.self, from: Data(response.utf8)))
        }
    }

    func testLostReplyCannotBecomeSuccessAndCannotStartAnotherRequest() throws {
        var session = PhoneUpgradeSession()
        let id = try XCTUnwrap(session.begin())
        XCTAssertNil(session.begin())
        XCTAssertTrue(session.unconfirmed(id))
        XCTAssertEqual(session.phase, .unconfirmed)
        XCTAssertTrue(session.pausesConnection)
        XCTAssertFalse(session.accepted(id))
        XCTAssertNil(session.begin())
        session.finish()
        XCTAssertNotNil(session.begin())
    }

    func testLateAcceptanceCannotAffectNewSession() throws {
        var session = PhoneUpgradeSession()
        let old = try XCTUnwrap(session.begin())
        session.finish()
        let current = try XCTUnwrap(session.begin())
        XCTAssertFalse(session.accepted(old))
        XCTAssertEqual(session.phase, .requesting)
        XCTAssertTrue(session.accepted(current))
        XCTAssertEqual(session.phase, .waitingForPhone)
        XCTAssertFalse(session.rejected(current))
    }

    func testRelaunchPreservesMaintenancePauseWithoutClaimingPendingRequestSucceeded() {
        XCTAssertEqual(PhoneUpgradeSession(restoring: .requesting).phase, .unconfirmed)
        XCTAssertTrue(PhoneUpgradeSession(restoring: .unconfirmed).pausesConnection)
        XCTAssertTrue(PhoneUpgradeSession(restoring: .waitingForPhone).pausesConnection)
        XCTAssertFalse(PhoneUpgradeSession().pausesConnection)
    }

    func testMaintenancePauseSurvivesDiscoveryNetworkAndMacWakeWithoutChangingPreference() {
        var recovery = ConnectionRecovery(enabled: true)
        recovery.suspend(.maintenance)
        recovery.suspend(.network); recovery.suspend(.sleep)
        recovery.resume(.network); recovery.resume(.sleep)
        XCTAssertTrue(recovery.enabled)
        XCTAssertFalse(recovery.deviceAvailable(now: 100))
        XCTAssertNil(recovery.nextDelay())
        recovery.resume(.maintenance)
        recovery.requireUserAction()
        XCTAssertNil(recovery.nextDelay())
        XCTAssertTrue(recovery.enabled)
        recovery.request()
        XCTAssertEqual(recovery.nextDelay(), 1)
    }
}
