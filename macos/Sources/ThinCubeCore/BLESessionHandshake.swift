import Foundation
import CryptoKit

/// Each status request follows the auth response. Firmware commits the queued
/// connect and publishes its epoch before it can transmit that response.
@MainActor public enum BLESessionHandshake {
    public enum Failure: LocalizedError {
        case invalidChallenge, invalidStatus
        public var errorDescription: String? {
            switch self {
            case .invalidChallenge: return "BLE 认证挑战无效"
            case .invalidStatus: return "BLE 未返回有效设备状态"
            }
        }
    }
    public static func establish(
        device: String, secret: String,
        exchange: ([String: Any]) async throws -> [String: Any],
        valid: ([String: Any]) -> Bool,
        stage: (ConnectionTimeline.Stage) -> Void = { _ in },
        pause: () async throws -> Void = { try await Task.sleep(nanoseconds: 100_000_000) }
    ) async throws -> [String: Any] {
        try Task.checkCancellation()
        let challenge = try await exchange(["op": "challenge"])
        try Task.checkCancellation()
        guard let nonce = challenge["nonce"] as? String, Data(hex: nonce)?.count == 16,
              let key = Data(hex: secret), key.count == 16 else { throw Failure.invalidChallenge }
        stage(.challenge)
        let message = Data("ThinCube-BLE-v1|\(device)|\(nonce)".utf8)
        let proof = HMAC<SHA256>.authenticationCode(for: message, using: SymmetricKey(data: key))
            .map { String(format: "%02x", $0) }.joined()
        _ = try await exchange(["op": "auth", "proof": proof])
        try Task.checkCancellation()
        stage(.authenticated)
        for index in 0..<10 {
            let snapshot = try await exchange(["op": "status"])
            try Task.checkCancellation()
            if valid(snapshot) { stage(.status); return snapshot }
            if index < 9 { try await pause() }
        }
        throw Failure.invalidStatus
    }
}
