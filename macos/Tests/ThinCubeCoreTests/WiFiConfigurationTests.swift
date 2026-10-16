import XCTest
@testable import ThinCubeCore

final class WiFiConfigurationTests: XCTestCase {
    func testBothConfigurationAndPairingBeginRouteOverBLE() {
        XCTAssertEqual(DeviceBLEOperation.forPath("api/wifi"), "wifi")
        XCTAssertEqual(DeviceBLEOperation.forPath("api/pair/begin"), "pair_begin")
        XCTAssertEqual(DeviceBLEOperation.forPath("api/settings"), "settings")
        XCTAssertEqual(DeviceBLEOperation.forPath("api/network"), "network_result")
        XCTAssertNil(DeviceBLEOperation.forPath("ota/begin"))
    }
    func testReconfigurationIgnoresStaleBLEThenCanRestoreBLE() {
        XCTAssertEqual(WiFiConfigurationRecovery.mode(elapsed: 0, bluetoothRediscovered: true), .wifi)
        XCTAssertEqual(WiFiConfigurationRecovery.mode(elapsed: 9, bluetoothRediscovered: true), .wifi)
        XCTAssertEqual(WiFiConfigurationRecovery.mode(elapsed: 40, bluetoothRediscovered: false), .wifi)
        XCTAssertEqual(WiFiConfigurationRecovery.mode(elapsed: 40, bluetoothRediscovered: true), .ble)
    }
    func testOldMacDoesNotOccupyBLEWhileAnotherMacPairs() {
        XCTAssertTrue(PairingRecovery.pauseAutomaticConnection(automatic: true, paired: true, windowOpen: true))
        XCTAssertFalse(PairingRecovery.pauseAutomaticConnection(automatic: false, paired: true, windowOpen: true))
        XCTAssertFalse(PairingRecovery.pauseAutomaticConnection(automatic: true, paired: false, windowOpen: true))
        XCTAssertFalse(PairingRecovery.pauseAutomaticConnection(automatic: true, paired: true, windowOpen: false))
    }
    func testCredentialLimitsUseUTF8BytesAndRetainSpaces() {
        XCTAssertNil(WiFiConfiguration(ssid: " network ", password: "").validationError)
        XCTAssertNil(WiFiConfiguration(ssid: String(repeating: "中", count: 10), password: "12345678").validationError)
        XCTAssertNotNil(WiFiConfiguration(ssid: String(repeating: "中", count: 11), password: "12345678").validationError)
        XCTAssertNotNil(WiFiConfiguration(ssid: "n\0x", password: "12345678").validationError)
        XCTAssertNotNil(WiFiConfiguration(ssid: "network", password: "short").validationError)
        XCTAssertNotNil(WiFiConfiguration(ssid: "network", password: String(repeating: "x", count: 64)).validationError)
    }
    func testResultsSeparateAcceptanceFromSuccessfulValidation() throws {
        let pending = try JSONDecoder().decode(NetworkConfigurationStatus.self, from: Data(#"{"requestId":"one","phase":"pending","error":""}"#.utf8))
        XCTAssertFalse(pending.finished)
        let failed = try JSONDecoder().decode(NetworkConfigurationStatus.self, from: Data(#"{"requestId":"one","phase":"failed","error":"network or password"}"#.utf8))
        XCTAssertTrue(failed.finished)
        XCTAssertEqual(failed.failureDescription, "无法连接网络，请检查网络名称和密码")
        let success = try JSONDecoder().decode(NetworkConfigurationStatus.self, from: Data(#"{"requestId":"one","phase":"succeeded","error":""}"#.utf8))
        XCTAssertTrue(success.finished)
    }
}
