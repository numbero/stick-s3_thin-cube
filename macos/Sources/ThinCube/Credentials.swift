import Foundation
import Security

enum CredentialError: LocalizedError {
    case missing, inaccessible(OSStatus)
    var errorDescription: String? {
        switch self {
        case .missing: return "未找到此设备的已保存配对，请输入设备上的六位配对码。"
        case .inaccessible: return "系统尚未允许读取已保存配对。请点击“连接”，并在系统钥匙串提示中允许访问。"
        }
    }
}

enum Credentials {
    private static let service = "app.thincube.device"
    static func load(_ name: String) throws -> String {
        let query: [String: Any] = [kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: service, kSecAttrAccount as String: name,
            kSecReturnData as String: true, kSecMatchLimit as String: kSecMatchLimitOne]
        var result: CFTypeRef?
        let status = SecItemCopyMatching(query as CFDictionary, &result)
        if status == errSecItemNotFound { throw CredentialError.missing }
        guard status == errSecSuccess, let data = result as? Data,
              let secret = String(data: data, encoding: .utf8), secret.count == 32 else {
            throw CredentialError.inaccessible(status)
        }
        return secret
    }
    static func save(_ key: String, name: String) throws {
        let query: [String: Any] = [kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: service, kSecAttrAccount as String: name]
        let value = Data(key.utf8)
        let update = SecItemUpdate(query as CFDictionary, [kSecValueData as String: value] as CFDictionary)
        if update == errSecSuccess { return }
        guard update == errSecItemNotFound else { throw ClientError.message("无法更新设备配对凭据：\(update)") }
        var add = query; add[kSecValueData as String] = value
        add[kSecAttrAccessible as String] = kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly
        guard SecItemAdd(add as CFDictionary, nil) == errSecSuccess else { throw ClientError.message("无法保存设备配对凭据") }
    }
}
