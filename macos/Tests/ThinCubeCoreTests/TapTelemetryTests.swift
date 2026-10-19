import XCTest
@testable import ThinCubeCore

final class TapTelemetryTests: XCTestCase {
    private func snapshot(raw: UInt32 = 0, accepted: UInt32 = 0, revision: UInt32 = 1, ms: UInt32 = 1000, errors: UInt32 = 0, result: String = "ok", verified: Bool = true, persistence: Int? = nil, saved: Bool? = nil) throws -> TapTelemetry {
        var config = TapConfiguration(); config.enabled = true
        var value: [String: Any] = ["supported": true, "diagnosticOnly": true, "verified": verified, "enabled": true, "healthy": true,
            "revision": revision, "requestId": "request-1", "result": result, "raw": raw, "accepted": accepted, "errors": errors, "ms": ms,
            "config": try JSONSerialization.jsonObject(with: JSONEncoder().encode(config))]
        if let persistence { value["persistence"] = persistence }
        if let saved { value["saved"] = saved }
        return try JSONDecoder().decode(TapTelemetry.self, from: JSONSerialization.data(withJSONObject: value))
    }
    func testSavingRequiresExplicitStorageCapabilityAndConfirmation() throws {
        XCTAssertFalse(try snapshot().supportsPersistence)
        XCTAssertFalse(try snapshot().confirms("request-1", requireSaved: true))
        XCTAssertFalse(try snapshot(saved: true).confirms("request-1", requireSaved: true))
        XCTAssertFalse(try snapshot(persistence: 1, saved: false).confirms("request-1", requireSaved: true))
        XCTAssertFalse(try snapshot(persistence: 2, saved: true).confirms("request-1", requireSaved: true))
        let saved = try snapshot(persistence: 1, saved: true)
        XCTAssertTrue(saved.confirms("request-1", configuration: saved.config, requireSaved: true))
        XCTAssertFalse(saved.confirms("old-request", requireSaved: true))
        XCTAssertFalse(try snapshot(result: "storage_failed_restored", persistence: 1, saved: true).confirms("request-1", requireSaved: true))
        XCTAssertFalse(try snapshot(verified: false, persistence: 1, saved: true).confirms("request-1", requireSaved: true))
    }
    func testRequiresCorrelatedVerifiedReadback() throws {
        let status = try snapshot()
        XCTAssertFalse(status.confirms("old-request"))
        XCTAssertTrue(status.confirms("request-1", configuration: status.config))
        var changed = status.config!; changed.sensitivity = 0
        XCTAssertFalse(status.confirms("request-1", configuration: changed))
        XCTAssertFalse(try snapshot(result: "write_failed_restored").confirms("request-1"))
        XCTAssertFalse(try snapshot(verified: false).confirms("request-1"))
        let old = try JSONDecoder().decode(TapTelemetry.self, from: Data("{\"supported\":false}".utf8))
        XCTAssertFalse(old.confirms("request-1"))
    }
    func testConfigurationUsesNativeTimingGridAndBounds() {
        var c = TapConfiguration(); c.enabled = true
        c.windowMs = 251; XCTAssertFalse(c.isValid)
        c.windowMs = 1000; c.quietMs = 500; c.sensitivity = 0; XCTAssertTrue(c.isValid)
        c.doubleTap = false; XCTAssertFalse(c.isValid)
        c.triple = true; XCTAssertTrue(c.isValid)
        c.axis = 3; XCTAssertFalse(c.isValid)
        c.axis = 2; c.buttonGuardMs = -1; XCTAssertFalse(c.isValid)
        c.buttonGuardMs = 0; c.softGyroFilterMs = 100; XCTAssertTrue(c.isValid)
        c.softGyroFilterMs = 1001; XCTAssertFalse(c.isValid)
    }
    func testTransportHasIndependentAuthenticatedTapOperation() {
        XCTAssertEqual(DeviceBLEOperation.forPath("api/tap"), "tap")
        XCTAssertEqual(DeviceBLEOperation.forPath("api/status"), "status")
    }
}
