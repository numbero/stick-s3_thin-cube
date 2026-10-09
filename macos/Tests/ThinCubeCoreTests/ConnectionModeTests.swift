import XCTest
@testable import ThinCubeCore

final class ConnectionModeTests: XCTestCase {
    private let wifi = ConnectionEndpoint(deviceID: "ThinCube-A", host: "192.168.0.10")
    private let ble = ConnectionEndpoint(deviceID: "ThinCube-A", host: "ble://A")

    private func resolve(_ mode: ConnectionMode, _ endpoints: [ConnectionEndpoint], preferred: String = "wifi",
                         network: Bool = true, bluetooth: Bool = true) -> ConnectionEndpoint? {
        mode.endpoint(for: "ThinCube-A", in: endpoints, preferredTransport: preferred,
                      wifiAvailable: network, bluetoothAvailable: bluetooth)
    }

    func testAutomaticFollowsDeviceWhenOnlyOppositeTransportIsDiscovered() {
        XCTAssertEqual(resolve(.automatic, [ble]), ble)
        XCTAssertEqual(resolve(.automatic, [wifi], preferred: "ble"), wifi)
    }

    func testManualModesNeverFallBackToOppositeTransport() {
        XCTAssertNil(resolve(.wifi, [ble]))
        XCTAssertNil(resolve(.ble, [wifi]))
        XCTAssertEqual(resolve(.ble, [wifi, ble]), ble)
    }

    func testAutomaticKeepsPreferredEndpointWhenBothAreAvailable() {
        XCTAssertEqual(resolve(.automatic, [ble, wifi]), wifi)
        XCTAssertEqual(resolve(.automatic, [wifi, ble], preferred: "ble"), ble)
    }

    func testNetworkAndBluetoothAvailabilityDoNotBlockOtherTransport() {
        XCTAssertEqual(resolve(.automatic, [wifi, ble], network: false), ble)
        XCTAssertEqual(resolve(.automatic, [wifi, ble], preferred: "ble", bluetooth: false), wifi)
        XCTAssertNil(resolve(.wifi, [wifi], network: false))
        XCTAssertNil(resolve(.ble, [ble], bluetooth: false))
    }

    func testAutomaticNeverSelectsAnotherDevice() {
        let other = ConnectionEndpoint(deviceID: "ThinCube-B", host: "ble://B")
        XCTAssertNil(resolve(.automatic, [other]))
        XCTAssertEqual(resolve(.automatic, [other, wifi]), wifi)
    }

    func testAuthorizationCodeIsExactlySixASCIIDigits() {
        XCTAssertTrue(PairingCode.isValid("001234"))
        for code in ["12345", "1234567", "12a456", "１２３４５６", "123 45", "123456\n"] {
            XCTAssertFalse(PairingCode.isValid(code))
        }
    }
}
