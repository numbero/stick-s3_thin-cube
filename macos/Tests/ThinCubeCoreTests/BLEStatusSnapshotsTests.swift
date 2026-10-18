import XCTest
@testable import ThinCubeCore

final class BLEStatusSnapshotsTests: XCTestCase {
    func testDualKeyDeltaPreservesNegotiationAndClearsB() throws {
        var decoder = BLEStatusSnapshots()
        _ = decoder.accept(try data(["epoch": 7, "keyProtocol": 2, "keyMode": 1, "heldB": false]))
        let live: [Any] = [7, 201, String(repeating: "00", count: 16), 0, "inverted", "", 0]
        var delta: [String: Any] = ["s": live, "edge": ["seq": 1, "down": true, "key": "b"], "keyMode": 2, "heldB": true]
        let accepted = try XCTUnwrap(decoder.accept(try data(delta)))
        let status = try XCTUnwrap(JSONSerialization.jsonObject(with: accepted) as? [String: Any])
        XCTAssertEqual(status["keyProtocol"] as? Int, 2); XCTAssertEqual(status["keyMode"] as? Int, 2)
        XCTAssertEqual(status["heldB"] as? Bool, true)
        for malformed in ["c" as Any, NSNull(), 1] {
            delta["edge"] = ["seq": 2, "down": false, "key": malformed]
            XCTAssertNil(decoder.accept(try data(delta)))
        }
        delta["edge"] = NSNull(); delta["heldB"] = 1
        XCTAssertNil(decoder.accept(try data(delta)))
        delta["heldB"] = false; delta["keyMode"] = 3
        XCTAssertNil(decoder.accept(try data(delta)))
        delta["keyMode"] = 2
        let cleared = try XCTUnwrap(decoder.accept(try data(delta)))
        XCTAssertEqual((try JSONSerialization.jsonObject(with: cleared) as? [String: Any])?["heldB"] as? Bool, false)
    }

    private func data(_ value: [String: Any]) throws -> Data { try JSONSerialization.data(withJSONObject: value) }
    func testLiveSnapshotsRequireMatchingBaselineAndPreserveSettings() throws {
        var decoder = BLEStatusSnapshots()
        let live: [String: Any] = ["s": [UInt32.max, 201, String(repeating: "ab", count: 16), 0.25, "inverted", "", 3] as [Any], "edge": ["seq": 42, "down": true]]
        XCTAssertNil(decoder.accept(try data(live)))
        let full: [String: Any] = ["epoch": UInt32.max, "brightness": 134, "gain": 1.5, "streaming": false, "calibrated": true]
        XCTAssertNotNil(decoder.accept(try data(full)))
        let output = try XCTUnwrap(decoder.accept(try data(live)))
        let status = try XCTUnwrap(JSONSerialization.jsonObject(with: output) as? [String: Any])
        XCTAssertEqual(status["brightness"] as? Int, 134)
        XCTAssertEqual(status["gain"] as? Double, 1.5)
        XCTAssertEqual(status["allowed"] as? Bool, true)
        XCTAssertEqual(status["streaming"] as? Bool, true)
        XCTAssertEqual(status["micReady"] as? Bool, true)
        XCTAssertEqual(status["inverted"] as? Bool, true)
        XCTAssertEqual(status["held"] as? Bool, false)
        var next = live; next["edge"] = NSNull()
        let cleared = try XCTUnwrap(decoder.accept(try data(next)))
        XCTAssertTrue((try JSONSerialization.jsonObject(with: cleared) as? [String: Any])?["edge"] is NSNull)
        next["s"] = [1, 0, String(repeating: "ab", count: 16), 0, "face_up", "invert", 0] as [Any]
        XCTAssertNil(decoder.accept(try data(next)))
        decoder.reset(); XCTAssertNil(decoder.accept(try data(live)))
    }
    func testMalformedDeltaDoesNotReplaceBaseline() throws {
        var decoder = BLEStatusSnapshots()
        _ = decoder.accept(try data(["epoch": 7, "brightness": 134]))
        let valid: [Any] = [7, 0, String(repeating: "00", count: 16), 0, "face_up", "invert", 0]
        for (index, wrong) in [(0, -1 as Any), (1, 256), (2, "bad"), (3, 2), (6, -1), (0, true), (1, true), (3, true), (6, true)] {
            var fields = valid; fields[index] = wrong
            XCTAssertNil(decoder.accept(try data(["s": fields, "edge": NSNull()])))
        }
        XCTAssertNil(decoder.accept(try data(["s": valid])))
        XCTAssertNil(decoder.accept(try data(["s": valid, "edge": ["down": true]])))
        XCTAssertNil(decoder.accept(try data(["s": valid, "edge": ["seq": 1, "down": 1]])))
        XCTAssertNotNil(decoder.accept(try data(["s": valid, "edge": NSNull()])))
    }
}
