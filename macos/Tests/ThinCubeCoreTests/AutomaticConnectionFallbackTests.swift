import XCTest
@testable import ThinCubeCore

final class AutomaticConnectionFallbackTests: XCTestCase {
    private let target = ConnectionTarget(host: "192.168.0.10", deviceID: "ThinCube-A")
    private let ble = ConnectionEndpoint(deviceID: "ThinCube-A", host: "ble://A")

    func testAdvertisementInterruptsUnconfirmedWiFiWithoutWaitingForTimeout() {
        var fallback = AutomaticConnectionFallback()
        XCTAssertTrue(fallback.redirect(mode: .automatic, target: target, advertised: ble, stage: .channel, now: 0.08))
    }

    func testManualWiFiAndBLEModesKeepTheirExplicitTransport() {
        for mode in [ConnectionMode.wifi, .ble] {
            var fallback = AutomaticConnectionFallback()
            XCTAssertFalse(fallback.redirect(mode: mode, target: target, advertised: ble, stage: .channel, now: 1))
        }
    }

    func testNeverInterruptsConfirmedWiFiPairingOrConnectedSession() {
        for stage in [ConnectionStage.idle, .authorization, .pairing, .verifying, .session, .connected] {
            var fallback = AutomaticConnectionFallback()
            XCTAssertFalse(fallback.redirect(mode: .automatic, target: target, advertised: ble, stage: stage, now: 1))
        }
    }

    func testOnlyFreshBLEForTheSelectedIdentityCanRedirect() {
        var fallback = AutomaticConnectionFallback()
        let other = ConnectionEndpoint(deviceID: "ThinCube-B", host: "ble://B")
        let bonjour = ConnectionEndpoint(deviceID: "ThinCube-A", host: "192.168.0.11")
        for endpoint in [other, bonjour] {
            XCTAssertFalse(fallback.redirect(mode: .automatic, target: target, advertised: endpoint, stage: .channel, now: 1))
        }
        for current in [ConnectionTarget(host: target.host, deviceID: nil), ConnectionTarget(host: "", deviceID: "ThinCube-A"), ConnectionTarget(host: ble.host, deviceID: "ThinCube-A")] {
            XCTAssertFalse(fallback.redirect(mode: .automatic, target: current, advertised: ble, stage: .channel, now: 1))
        }
        XCTAssertTrue(fallback.redirect(mode: .automatic, target: target, advertised: ble, stage: .channel, now: 1))
    }

    func testFailedBLECannotTurnRepeatedAdvertisementsIntoWiFiCancellationLoop() {
        var fallback = AutomaticConnectionFallback()
        XCTAssertTrue(fallback.redirect(mode: .automatic, target: target, advertised: ble, stage: .channel, now: 10))
        for time in [10.01, 11, 12.99] {
            XCTAssertFalse(fallback.redirect(mode: .automatic, target: target, advertised: ble, stage: .channel, now: time))
        }
        XCTAssertTrue(fallback.redirect(mode: .automatic, target: target, advertised: ble, stage: .channel, now: 13))
    }

    func testNewDeviceOrSuccessfulSessionDoesNotInheritPreviousThrottle() {
        var fallback = AutomaticConnectionFallback()
        XCTAssertTrue(fallback.redirect(mode: .automatic, target: target, advertised: ble, stage: .channel, now: 10))
        let other = ConnectionTarget(host: "192.168.0.20", deviceID: "ThinCube-B")
        XCTAssertTrue(fallback.redirect(mode: .automatic, target: other, advertised: ConnectionEndpoint(deviceID: "ThinCube-B", host: "ble://B"), stage: .channel, now: 11))
        fallback.reset()
        XCTAssertTrue(fallback.redirect(mode: .automatic, target: other, advertised: ConnectionEndpoint(deviceID: "ThinCube-B", host: "ble://B"), stage: .channel, now: 11.1))
    }

    func testFailedCachedWiFiCannotReplaceBLEWhileWaitingForItsAdvertisement() {
        var fallback = AutomaticConnectionFallback()
        fallback.wifiFailed(target: target, now: 10)
        let wifi = ConnectionEndpoint(deviceID: "ThinCube-A", host: target.host)
        XCTAssertFalse(fallback.accepts(wifi, now: 10.01))
        XCTAssertFalse(fallback.accepts(wifi, now: 19.99))
        XCTAssertTrue(fallback.accepts(ble, now: 11))
        XCTAssertTrue(fallback.accepts(wifi, now: 20))
    }

    func testWiFiCooldownDoesNotHideChangedAddressOtherDeviceOrSuccessfulRetry() {
        var fallback = AutomaticConnectionFallback()
        fallback.wifiFailed(target: target, now: 10)
        XCTAssertTrue(fallback.accepts(ConnectionEndpoint(deviceID: "ThinCube-A", host: "192.168.0.11"), now: 11))
        XCTAssertTrue(fallback.accepts(ConnectionEndpoint(deviceID: "ThinCube-B", host: target.host), now: 11))
        fallback.reset()
        XCTAssertTrue(fallback.accepts(ConnectionEndpoint(deviceID: "ThinCube-A", host: target.host), now: 11))
    }

    func testBLEFailureDoesNotSuppressWiFi() {
        var fallback = AutomaticConnectionFallback()
        fallback.wifiFailed(target: ConnectionTarget(host: ble.host, deviceID: ble.deviceID), now: 10)
        XCTAssertTrue(fallback.accepts(ble, now: 11))
        XCTAssertTrue(fallback.accepts(ConnectionEndpoint(deviceID: "ThinCube-A", host: target.host), now: 11))
    }

    func testCancelledWiFiResponseCannotAdvanceReplacementAttempt() {
        var progress = ConnectionProgress()
        let wifiAttempt = UUID(), bleAttempt = UUID()
        progress.begin(wifiAttempt)
        progress.reset(); progress.begin(bleAttempt)
        XCTAssertFalse(progress.advance(.authorization, attempt: wifiAttempt))
        XCTAssertEqual(progress.stage, .channel)
        XCTAssertTrue(progress.advance(.authorization, attempt: bleAttempt))
    }
}
