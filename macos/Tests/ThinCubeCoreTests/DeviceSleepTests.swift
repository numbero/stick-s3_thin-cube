import XCTest
@testable import ThinCubeCore
final class DeviceSleepTests: XCTestCase {
    func testOnlyAcknowledgedCurrentNoticeMarksExpectedDisconnect() {
        var tracker = DeviceSleepTracker()
        tracker.observe(.init(id: 4, phase: "preparing"), epoch: 9, now: 10)
        XCTAssertFalse(tracker.didAcknowledge(id: 5, epoch: 9, now: 10.1))
        XCTAssertFalse(tracker.didAcknowledge(id: 4, epoch: 10, now: 10.1))
        XCTAssertFalse(tracker.didDisconnect(now: 10.2))
        tracker.observe(.init(id: 5, phase: "preparing"), epoch: 9, now: 11)
        XCTAssertTrue(tracker.didAcknowledge(id: 5, epoch: 9, now: 11.1))
        XCTAssertTrue(tracker.didDisconnect(now: 11.5))
        XCTAssertTrue(tracker.didDisconnect(now: 40)) // retries preserve expected sleep
        tracker.observe(.init(id: 0, phase: "awake"), epoch: 10, now: 41)
        XCTAssertFalse(tracker.sleeping)
    }
    func testCancelledReplayedAndLateNoticesDoNotMaskFailures() {
        var tracker = DeviceSleepTracker()
        tracker.observe(.init(id: 1, phase: "preparing"), epoch: 7, now: 10)
        XCTAssertTrue(tracker.didAcknowledge(id: 1, epoch: 7, now: 10.1))
        tracker.observe(.init(id: 1, phase: "awake"), epoch: 7, now: 10.2)
        XCTAssertFalse(tracker.didDisconnect(now: 10.3))
        tracker.observe(.init(id: 2, phase: "preparing"), epoch: 7, now: 20)
        XCTAssertTrue(tracker.didAcknowledge(id: 2, epoch: 7, now: 20.1))
        tracker.observe(.init(id: 2, phase: "preparing"), epoch: 7, now: 24.9)
        XCTAssertFalse(tracker.didDisconnect(now: 25.1))
        tracker.observe(.init(id: 3, phase: "preparing"), epoch: 7, now: 30)
        XCTAssertFalse(tracker.didAcknowledge(id: 3, epoch: 7, now: 35.1))
        tracker.observe(.init(id: 0, phase: "preparing"), epoch: 7, now: 40)
        XCTAssertNil(tracker.acknowledgementNeeded)
        tracker.observe(.init(id: 1, phase: "preparing"), epoch: 0, now: 41)
        XCTAssertNil(tracker.acknowledgementNeeded)
    }
    func testFastDeltaCarriesSleepAndCancellationWithinEpoch() throws {
        var snapshots = BLEStatusSnapshots()
        _ = snapshots.accept(try JSONSerialization.data(withJSONObject: ["epoch": 8, "deepSleep": true, "deepSleepAvailable": true, "deepSleepStaged": true, "deepSleepUnified": true]))
        let live: [Any] = [8, 0, String(repeating: "00", count: 16), 0.0, "face_up", "sleeping", 0]
        for phase in ["preparing", "awake"] {
            let result = try XCTUnwrap(snapshots.accept(try JSONSerialization.data(withJSONObject: ["s": live, "edge": NSNull(), "sleep": ["id": 9, "phase": phase]])))
            let value = try XCTUnwrap(JSONSerialization.jsonObject(with: result) as? [String: Any])
            XCTAssertEqual((value["sleep"] as? [String: Any])?["phase"] as? String, phase)
            XCTAssertEqual(value["deepSleep"] as? Bool, true)
            XCTAssertEqual(value["deepSleepStaged"] as? Bool, true)
            XCTAssertEqual(value["deepSleepUnified"] as? Bool, true)
        }
        snapshots.reset()
        XCTAssertNil(snapshots.accept(try JSONSerialization.data(withJSONObject: ["s": live, "edge": NSNull(), "sleep": ["id": 9, "phase": "preparing"]])))
    }
    func testLegacySnapshotDoesNotInheritUnifiedPolicyFromPreviousBoot() throws {
        var snapshots = BLEStatusSnapshots()
        _ = snapshots.accept(try JSONSerialization.data(withJSONObject: ["epoch": 8, "deepSleep": true, "deepSleepStaged": true, "deepSleepUnified": true]))
        // After changing firmware/epoch, the old optional policy must not be
        // presented as automatic or enabled by a stale full snapshot.
        let baseline = try XCTUnwrap(snapshots.accept(try JSONSerialization.data(withJSONObject: ["epoch": 9, "deepSleep": false, "deepSleepStaged": true])))
        let legacy = try XCTUnwrap(JSONSerialization.jsonObject(with: baseline) as? [String: Any])
        XCTAssertNil(legacy["deepSleepUnified"])
        XCTAssertEqual(legacy["deepSleep"] as? Bool, false)
        let live: [Any] = [9, 0, String(repeating: "00", count: 16), 0.0, "face_up", "sleeping", 0]
        let merged = try XCTUnwrap(snapshots.accept(try JSONSerialization.data(withJSONObject: ["s": live, "edge": NSNull(), "sleep": ["id": 10, "phase": "awake"]])))
        let value = try XCTUnwrap(JSONSerialization.jsonObject(with: merged) as? [String: Any])
        XCTAssertNil(value["deepSleepUnified"])
        XCTAssertEqual(value["deepSleep"] as? Bool, false)
    }
}
