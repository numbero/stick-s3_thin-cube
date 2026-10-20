import XCTest
import ThinCubeCore

@MainActor final class ConnectionOptimizationTests: XCTestCase {
    private let secret = String(repeating: "12", count: 16)
    private let nonce = String(repeating: "34", count: 16)

    func testHandshakeUsesOneStatusAndNeverTransmitsLongTermKey() async throws {
        var requests: [[String: Any]] = []
        let value = try await BLESessionHandshake.establish(device: "ThinCube-ABC123", secret: secret, exchange: { request in
            requests.append(request)
            switch request["op"] as? String {
            case "challenge": return ["nonce": self.nonce]
            case "auth": return ["authenticated": true]
            default: return ["epoch": 42]
            }
        }, valid: { $0["epoch"] as? Int == 42 })
        XCTAssertEqual(value["epoch"] as? Int, 42)
        XCTAssertEqual(requests.compactMap { $0["op"] as? String }, ["challenge", "auth", "status"])
        XCTAssertEqual((requests[1]["proof"] as? String)?.count, 64)
        // Independently computed protocol vector: binary 128-bit key, UTF-8 message.
        XCTAssertEqual(requests[1]["proof"] as? String, "dbbb78309421f65e6e043c21a6f7a51eadec5e836f24ada2df619e416ddfdbc3")
        XCTAssertNil(requests[1]["key"])
        XCTAssertFalse(requests.contains { $0.values.contains { ($0 as? String) == secret } })
    }
    func testInvalidInitialBaselineIsRetriedWithoutReauthenticating() async throws {
        var operations: [String] = [], statuses = 0, pauses = 0
        let value = try await BLESessionHandshake.establish(device: "ThinCube-A", secret: secret, exchange: { request in
            let op = request["op"] as! String; operations.append(op)
            if op == "challenge" { return ["nonce": self.nonce] }
            if op == "auth" { return [:] }
            statuses += 1; return ["epoch": statuses == 1 ? 0 : 99]
        }, valid: { ($0["epoch"] as? Int ?? 0) > 0 }, pause: { pauses += 1 })
        XCTAssertEqual(value["epoch"] as? Int, 99)
        XCTAssertEqual(operations, ["challenge", "auth", "status", "status"])
        XCTAssertEqual(pauses, 1)
    }
    func testBadNonceDoesNotSendProofOrStatus() async {
        var calls = 0
        do {
            _ = try await BLESessionHandshake.establish(device: "ThinCube-A", secret: secret, exchange: { _ in
                calls += 1; return ["nonce": "invalid"]
            }, valid: { _ in true })
            XCTFail("Invalid nonce was accepted")
        } catch { XCTAssertTrue(error is BLESessionHandshake.Failure) }
        XCTAssertEqual(calls, 1)
    }
    func testStatusValidationIsBounded() async {
        var statuses = 0, pauses = 0
        do {
            _ = try await BLESessionHandshake.establish(device: "ThinCube-A", secret: secret, exchange: { request in
                if request["op"] as? String == "challenge" { return ["nonce": self.nonce] }
                if request["op"] as? String == "status" { statuses += 1 }
                return [:]
            }, valid: { _ in false }, pause: { pauses += 1 })
            XCTFail("No baseline was accepted")
        } catch { XCTAssertTrue(error is BLESessionHandshake.Failure) }
        XCTAssertEqual(statuses, 10); XCTAssertEqual(pauses, 9)
    }
    func testCancelledHandshakeCannotSendAuthenticationToReplacementLink() async {
        let gate = Gate(), started = expectation(description: "challenge started")
        var operations: [String] = []
        let task = Task {
            try await BLESessionHandshake.establish(device: "ThinCube-A", secret: secret, exchange: { request in
                operations.append(request["op"] as! String)
                started.fulfill(); await gate.wait(); return ["nonce": self.nonce]
            }, valid: { _ in true })
        }
        await fulfillment(of: [started], timeout: 2)
        task.cancel(); gate.open()
        do { _ = try await task.value; XCTFail("Cancellation was ignored") }
        catch { XCTAssertTrue(error is CancellationError) }
        XCTAssertEqual(operations, ["challenge"])
    }
    func testSlowAudioPreparationDoesNotBlockHandshake() async {
        let audio = AudioPreparation(), gate = Gate()
        let started = expectation(description: "audio preparing"), ready = expectation(description: "audio ready")
        audio.start(prepare: { started.fulfill(); await gate.wait() }, ready: { ready.fulfill() }, failed: { _ in XCTFail() })
        await fulfillment(of: [started], timeout: 2)
        var controlReady = false
        let handshake = Task { controlReady = true }
        await handshake.value
        XCTAssertTrue(controlReady); XCTAssertTrue(audio.busy)
        gate.open(); await fulfillment(of: [ready], timeout: 2)
        XCTAssertFalse(audio.busy)
    }
    func testCancelledAudioCompletionCannotCommitOrClearNewPreparation() async {
        let audio = AudioPreparation(), old = Gate(), next = Gate()
        let first = expectation(description: "old started"), second = expectation(description: "new started")
        let returned = expectation(description: "old returned"), ready = expectation(description: "new ready")
        var oldCommitted = false
        audio.start(prepare: { first.fulfill(); await old.wait(); returned.fulfill() },
                    ready: { oldCommitted = true }, failed: { _ in XCTFail() })
        await fulfillment(of: [first], timeout: 2); audio.cancel()
        audio.start(prepare: { second.fulfill(); await next.wait() }, ready: { ready.fulfill() }, failed: { _ in XCTFail() })
        await fulfillment(of: [second], timeout: 2)
        old.open(); await fulfillment(of: [returned], timeout: 2)
        XCTAssertFalse(oldCommitted); XCTAssertTrue(audio.busy)
        next.open(); await fulfillment(of: [ready], timeout: 2)
        XCTAssertFalse(audio.busy)
    }
    func testAudioFailureAndCommitFailureCanBeRetried() async {
        enum Failure: Error { case failed }
        let audio = AudioPreparation(), failed = expectation(description: "prepare failure")
        audio.start(prepare: { throw Failure.failed }, ready: { XCTFail() }, failed: { _ in failed.fulfill() })
        await fulfillment(of: [failed], timeout: 2); XCTAssertFalse(audio.busy)
        let commit = expectation(description: "receiver failure")
        audio.start(prepare: {}, ready: { throw Failure.failed }, failed: { _ in commit.fulfill() })
        await fulfillment(of: [commit], timeout: 2); XCTAssertFalse(audio.busy)
    }
    func testTimelineRejectsStaleDuplicateAndPostCancellationEvents() throws {
        var timeline = ConnectionTimeline(); let attempt = UUID()
        _ = timeline.begin(attempt: attempt, transport: "ble", now: 10)
        let channel = try XCTUnwrap(timeline.mark(.channel, attempt: attempt, now: 10.025))
        XCTAssertEqual(channel.elapsedMs, 25, accuracy: 0.001)
        XCTAssertNil(timeline.mark(.channel, attempt: attempt, now: 11))
        XCTAssertNil(timeline.mark(.audioReady, attempt: UUID(), now: 11))
        XCTAssertNil(timeline.mark(.info, attempt: attempt, now: 9))
        _ = timeline.mark(.cancelled, attempt: attempt, now: 12)
        XCTAssertNil(timeline.mark(.audioReady, attempt: attempt, now: 13))
        let replacement = UUID(); _ = timeline.begin(attempt: replacement, transport: "wifi", now: 14)
        XCTAssertNil(timeline.mark(.status, attempt: attempt, now: 15))
        XCTAssertEqual(timeline.records.count, 1)
    }
    private final class Gate {
        private var continuation: CheckedContinuation<Void, Never>?
        func wait() async { await withCheckedContinuation { continuation = $0 } }
        func open() { continuation?.resume(); continuation = nil }
    }
}
