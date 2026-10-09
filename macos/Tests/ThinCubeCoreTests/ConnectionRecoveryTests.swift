import XCTest
@testable import ThinCubeCore

final class ConnectionRecoveryTests: XCTestCase {
    func testSavedConnectionRestoresAtStartupAndManualDisconnectSurvivesRelaunch() {
        var recovery = ConnectionRecovery(enabled: true)
        XCTAssertTrue(recovery.canAttempt)
        recovery.disconnect()
        XCTAssertNil(recovery.nextDelay())
        XCTAssertFalse(ConnectionRecovery(enabled: recovery.enabled).canAttempt)
        recovery.request()
        XCTAssertTrue(ConnectionRecovery(enabled: recovery.enabled).canAttempt)
    }

    func testOfflineRetriesBackOffAndContinueUntilDeviceReturns() {
        var recovery = ConnectionRecovery(enabled: true)
        XCTAssertEqual((0..<9).compactMap { _ in recovery.nextDelay() }, [1, 2, 4, 8, 15, 30, 30, 30, 30])
        recovery.didConnect()
        XCTAssertEqual(recovery.nextDelay(), 1)
    }

    func testLockSleepAndSessionMustAllResumeBeforeConnection() {
        var recovery = ConnectionRecovery(enabled: true)
        recovery.suspend(.lock); recovery.suspend(.sleep); recovery.suspend(.inactiveSession)
        recovery.resume(.sleep)
        XCTAssertNil(recovery.nextDelay())
        recovery.resume(.lock)
        XCTAssertFalse(recovery.canAttempt)
        recovery.resume(.inactiveSession)
        XCTAssertEqual(recovery.nextDelay(), 1)
    }

    func testNetworkReturnCannotOverrideAnIntentionalDisconnect() {
        var recovery = ConnectionRecovery(enabled: true)
        recovery.suspend(.network); recovery.disconnect(); recovery.resume(.network)
        XCTAssertFalse(recovery.canAttempt)
        recovery.request()
        XCTAssertTrue(recovery.canAttempt)
    }

    func testCredentialDenialDoesNotCauseRepeatedPromptsButManualRetryWorks() {
        var recovery = ConnectionRecovery(enabled: true)
        recovery.requireUserAction()
        recovery.suspend(.network); recovery.resume(.network)
        XCTAssertNil(recovery.nextDelay())
        recovery.request()
        XCTAssertEqual(recovery.nextDelay(), 1)
    }

    func testFirmwareUpdateDefersRecoveryUntilUploadHasFinished() {
        var recovery = ConnectionRecovery(enabled: true)
        recovery.suspend(.update); recovery.suspend(.network); recovery.resume(.network)
        XCTAssertNil(recovery.nextDelay())
        recovery.resume(.update)
        XCTAssertEqual(recovery.nextDelay(), 1)
    }

    func testQuittingRetainsStartupIntentButPreventsInProcessRecovery() {
        var recovery = ConnectionRecovery(enabled: true)
        recovery.suspend(.termination)
        XCTAssertTrue(recovery.enabled)
        recovery.resume(.network)
        XCTAssertFalse(recovery.canAttempt)
        XCTAssertTrue(ConnectionRecovery(enabled: recovery.enabled).canAttempt)
    }

    func testBonjourOnlyChangesAddressForRememberedDeviceIdentity() {
        var target = ConnectionTarget(host: "192.168.0.20", deviceID: "ThinCube-ABC123")
        XCTAssertFalse(target.discovered(id: "ThinCube-OTHER", host: "192.168.0.99"))
        XCTAssertEqual(target.host, "192.168.0.20")
        XCTAssertTrue(target.discovered(id: "ThinCube-ABC123", host: "192.168.0.30"))
        XCTAssertTrue(target.accepts("ThinCube-ABC123"))
        XCTAssertFalse(target.accepts("ThinCube-OTHER"))
        XCTAssertFalse(target.discovered(id: "ThinCube-ABC123", host: "192.168.0.30"))
    }

    func testLegacyHostCanBeIdentifiedWithoutChoosingAnUnrelatedBonjourDevice() {
        var target = ConnectionTarget(host: "192.168.0.20", deviceID: nil)
        XCTAssertTrue(target.accepts("ThinCube-ABC123"))
        XCTAssertFalse(target.discovered(id: "ThinCube-ABC123", host: "192.168.0.30"))
        target.deviceID = "ThinCube-ABC123"
        XCTAssertTrue(target.discovered(id: "ThinCube-ABC123", host: "192.168.0.30"))
    }
}
