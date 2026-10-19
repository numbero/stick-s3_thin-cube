import XCTest
@testable import ThinCubeCore

final class DeviceGestureStreamTests: XCTestCase {
    func testLegacyFlickEventsAreAcknowledgedWithoutInjection() {
        var stream = DeviceGestureStream()
        _ = stream.receive(epoch: 7, voiceSession: "voice", mode: 1, event: nil, canInject: true)
        let tap = stream.receive(epoch: 7, voiceSession: "voice", mode: 1, event: .init(seq: 1, type: "double_tap"), canInject: true)
        XCTAssertTrue(tap.fire); XCTAssertEqual(tap.event?.trigger, .doubleTap)
        XCTAssertFalse(stream.receive(epoch: 7, voiceSession: "voice", mode: 1, event: .init(seq: 1, type: "double_tap"), canInject: true).fire)
        let flick = stream.receive(epoch: 7, voiceSession: "voice", mode: 1, event: .init(seq: 2, type: "double_flick"), canInject: true)
        XCTAssertFalse(flick.fire); XCTAssertNil(flick.event?.trigger); XCTAssertEqual(stream.acknowledgement, 2)
        XCTAssertFalse(stream.receive(epoch: 7, voiceSession: "voice", mode: 1, event: .init(seq: 3, type: "double_tap"), canInject: false).fire)
        XCTAssertEqual(stream.acknowledgement, 3)
        XCTAssertFalse(stream.receive(epoch: 7, voiceSession: "voice", mode: 1, event: .init(seq: 3, type: "double_tap"), canInject: true).fire)
        XCTAssertFalse(stream.receive(epoch: 7, voiceSession: "voice", mode: 1, event: .init(seq: 4, type: "a"), canInject: true).fire)
        XCTAssertTrue(stream.receive(epoch: 7, voiceSession: "voice", mode: 1, event: .init(seq: 5), canInject: true).fire)
        XCTAssertNil(ShortcutTrigger(rawValue: "double_flick"))
    }
    func testShortcutStoragePreservesExistingKeysAndSeparatesTap() {
        XCTAssertEqual(ShortcutTrigger.a.storageKey(for: "device"), "shortcut.device")
        XCTAssertEqual(ShortcutTrigger.b.storageKey(for: "device"), "shortcut.b.device")
        XCTAssertEqual(ShortcutTrigger.doubleTap.storageKey(for: "device"), "shortcut.double_tap.device")
        XCTAssertEqual(Set(ShortcutTrigger.allCases.map { $0.storageKey(for: "device") }).count, 3)
    }
    func testEventExecutesOnlyOnceAndBlockedEventsAreAcknowledged() {
        var stream = DeviceGestureStream()
        _ = stream.receive(epoch: 7, voiceSession: "voice", mode: 1, event: nil, canInject: true)
        XCTAssertTrue(stream.receive(epoch: 7, voiceSession: "voice", mode: 1, event: .init(seq: 1), canInject: true).fire)
        XCTAssertFalse(stream.receive(epoch: 7, voiceSession: "voice", mode: 1, event: .init(seq: 1), canInject: true).fire)
        XCTAssertFalse(stream.receive(epoch: 7, voiceSession: "voice", mode: 1, event: .init(seq: 2), canInject: false).fire)
        XCTAssertEqual(stream.acknowledgement, 2)
        XCTAssertFalse(stream.receive(epoch: 7, voiceSession: "voice", mode: 1, event: .init(seq: 2), canInject: true).fire)
        XCTAssertTrue(stream.receive(epoch: 7, voiceSession: "voice", mode: 1, event: .init(seq: 3), canInject: true).fire)
    }
    func testContextTransitionsAndExpiryNeverReplayAnEvent() {
        var stream = DeviceGestureStream()
        XCTAssertFalse(stream.receive(epoch: 1, voiceSession: "one", mode: 1, event: .init(seq: 1), canInject: true).fire)
        XCTAssertFalse(stream.receive(epoch: 1, voiceSession: "two", mode: 1, event: .init(seq: 2), canInject: true).fire)
        XCTAssertFalse(stream.receive(epoch: 1, voiceSession: "two", mode: 0, event: .init(seq: 3), canInject: true).fire)
        XCTAssertFalse(stream.receive(epoch: 1, voiceSession: "two", mode: 1, event: .init(seq: 4), canInject: true).fire)
        XCTAssertFalse(stream.receive(epoch: 1, voiceSession: "two", mode: 1, event: .init(seq: 5, ageMs: 751), canInject: true).fire)
        XCTAssertEqual(stream.acknowledgement, 5)
        XCTAssertFalse(stream.receive(epoch: 1, voiceSession: "two", mode: 1, event: .init(seq: 6, type: "future_gesture"), canInject: true).fire)
        XCTAssertTrue(stream.receive(epoch: 1, voiceSession: "two", mode: 1, event: .init(seq: 7, ageMs: 750), canInject: true).fire)
        XCTAssertFalse(stream.receive(epoch: 1, voiceSession: nil, mode: 1, event: .init(seq: 8), canInject: true).fire)
        XCTAssertEqual(stream.acknowledgement, 8)
        XCTAssertFalse(stream.receive(epoch: 2, voiceSession: "two", mode: 1, event: .init(seq: 1), canInject: true).fire)
        XCTAssertEqual(stream.acknowledgement, 1)
        XCTAssertTrue(stream.receive(epoch: 2, voiceSession: "two", mode: 1, event: .init(seq: 2), canInject: true).fire)
    }
    func testSequenceWrapAndOldPackets() {
        var stream = DeviceGestureStream()
        _ = stream.receive(epoch: 1, voiceSession: "voice", mode: 1, event: .init(seq: .max), canInject: false)
        XCTAssertTrue(stream.receive(epoch: 1, voiceSession: "voice", mode: 1, event: .init(seq: 1), canInject: true).fire)
        XCTAssertFalse(stream.receive(epoch: 1, voiceSession: "voice", mode: 1, event: .init(seq: .max), canInject: true).fire)
        XCTAssertEqual(stream.acknowledgement, 1)
    }
}
