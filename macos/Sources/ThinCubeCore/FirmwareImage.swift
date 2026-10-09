import Foundation
import CryptoKit

public struct FirmwareImage {
    public let data: Data, sha256: String, version: String
    public init(data: Data) throws {
        func string(_ range: Range<Int>) -> String { String(decoding: data[range].prefix { $0 != 0 }, as: UTF8.self) }
        guard data.count >= 352, data.count <= 2 * 1024 * 1024, data[0] == 0xe9,
              data[12] == 9, data[13] == 0,
              Array(data[32..<36]) == [0x32, 0x54, 0xcd, 0xab],
              string(288..<320) == "ThinCube-StickS3-v1" else { throw ImageError.wrongDevice }
        self.data = data
        sha256 = SHA256.hash(data: data).map { String(format: "%02x", $0) }.joined()
        version = string(320..<352)
    }
    public enum ImageError: LocalizedError {
        case wrongDevice
        public var errorDescription: String? { "请选择 ThinCube StickS3 的应用固件 .bin（不是整片备份或其他设备固件）。" }
    }
}
