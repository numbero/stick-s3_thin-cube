import XCTest
@testable import ThinCubeCore

final class PowerTelemetryTests: XCTestCase {
    private func sample(boot: UInt32 = 7, ms: UInt32 = 1000, percent: Int = 70) throws -> PowerTelemetry {
        let fields: [String: Any] = ["boot": boot, "ms": ms, "rawMv": 3860, "medianMv": 3860,
            "filteredMv": 3860, "percent": percent, "otaPercent": percent, "source": 0,
            "vinMv": 0, "charging": false, "chargeKnown": true, "low": false,
            "cutoffMv": 2500, "failures": 0]
        return try JSONDecoder().decode(PowerTelemetry.self, from: JSONSerialization.data(withJSONObject: fields))
    }
    func testUnknownChargeIsNotDecodedAsEmpty() throws {
        let value = try sample(percent: -1)
        XCTAssertEqual(value.percent, -1)
        XCTAssertEqual(value.dictionary["percent"] as? Int, -1)
        XCTAssertFalse(value.charging)
    }
    func testCalibrationMetadataAndSafeLogIdentity() throws {
        var fields = try sample().dictionary
        fields["chip"] = "6046E5B1E87C"; fields["actualBrightness"] = 12; fields["model"] = "linear"
        fields["cal"] = ["run": 42, "state": 4, "elapsed": 1000, "phase": 0,
                         "phaseMs": 1000, "stableMs": 180000, "reason": 0]
        let value = try JSONDecoder().decode(PowerTelemetry.self, from: JSONSerialization.data(withJSONObject: fields))
        XCTAssertEqual(value.cal?.state, 4)
        XCTAssertEqual(value.actualBrightness, 12)
        XCTAssertEqual(value.calibrationLogName, "battery-6046e5b1e87c-7-42.jsonl")
        XCTAssertEqual((value.dictionary["cal"] as? [String: Any])?["run"] as? UInt32, 42)
        fields["chip"] = "../../escape"
        let invalid = try JSONDecoder().decode(PowerTelemetry.self, from: JSONSerialization.data(withJSONObject: fields))
        XCTAssertNil(invalid.calibrationLogName)
        XCTAssertNil(try sample().calibrationLogName) // v0.4.15 remains compatible.
    }
    func testRepeatedAndDelayedSnapshotsDoNotDuplicateBatteryLog() throws {
        var cursor = PowerSampleCursor()
        XCTAssertTrue(cursor.accept(try sample(ms: 2000), device: "unit-a"))
        XCTAssertFalse(cursor.accept(try sample(ms: 2000), device: "unit-a"))
        XCTAssertFalse(cursor.accept(try sample(ms: 1000), device: "unit-a"))
        XCTAssertTrue(cursor.accept(try sample(ms: 3000), device: "unit-a"))
        XCTAssertTrue(cursor.accept(try sample(boot: 8, ms: 100), device: "unit-a"))
        XCTAssertTrue(cursor.accept(try sample(boot: 8, ms: 100), device: "unit-b"))
    }
    func testMCUTimeWrapAndLatePreWrapSample() throws {
        var cursor = PowerSampleCursor()
        XCTAssertTrue(cursor.accept(try sample(ms: UInt32.max - 100), device: "unit"))
        XCTAssertTrue(cursor.accept(try sample(ms: 899), device: "unit"))
        XCTAssertFalse(cursor.accept(try sample(ms: UInt32.max - 100), device: "unit"))
    }
    func testPowerDeltaPreservesControlAndSurvivesFollowingDeltas() throws {
        var snapshots = BLEStatusSnapshots()
        _ = snapshots.accept(try JSONSerialization.data(withJSONObject: ["epoch": 7, "brightness": 134]))
        let live: [Any] = [7, 0, String(repeating: "00", count: 16), 0.25, "face_up", "invert", 0]
        let power = try sample().dictionary
        let first = try XCTUnwrap(snapshots.accept(try JSONSerialization.data(withJSONObject:
            ["s": live, "edge": NSNull(), "power": power])))
        let value = try XCTUnwrap(JSONSerialization.jsonObject(with: first) as? [String: Any])
        XCTAssertEqual(value["level"] as? Double, 0.25)
        XCTAssertEqual(value["brightness"] as? Int, 134)
        XCTAssertEqual((value["power"] as? [String: Any])?["percent"] as? Int, 70)
        let second = try XCTUnwrap(snapshots.accept(try JSONSerialization.data(withJSONObject:
            ["s": live, "edge": NSNull()])))
        let retained = try XCTUnwrap(JSONSerialization.jsonObject(with: second) as? [String: Any])
        XCTAssertEqual((retained["power"] as? [String: Any])?["ms"] as? Int, 1000)
        snapshots.reset()
        XCTAssertNil(snapshots.accept(try JSONSerialization.data(withJSONObject: ["s": live, "edge": NSNull(), "power": power])))
    }
}
