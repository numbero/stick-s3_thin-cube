// Export exactly one explicitly selected identity, never the whole keychain.
import Foundation
import Security
import CryptoKit

func fail(_ message: String) -> Never {
    FileHandle.standardError.write(Data((message + "\n").utf8))
    exit(1)
}
guard CommandLine.arguments.count == 3 else { fail("Usage: export-pinned-signing IDENTITY_SHA1 OUTPUT.p12 (password on stdin)") }
let expected = CommandLine.arguments[1].uppercased()
let passwordData = FileHandle.standardInput.readDataToEndOfFile()
guard let password = String(data: passwordData, encoding: .utf8), !password.isEmpty else { fail("Missing export password") }
var result: CFTypeRef?
let query = [kSecClass: kSecClassIdentity, kSecMatchLimit: kSecMatchLimitAll, kSecReturnRef: true] as CFDictionary
let status = SecItemCopyMatching(query, &result)
guard status == errSecSuccess, let identities = result as? [SecIdentity] else { fail("Cannot enumerate signing identities (\(status))") }
let matches = identities.filter { identity in
    var certificate: SecCertificate?
    guard SecIdentityCopyCertificate(identity, &certificate) == errSecSuccess, let certificate else { return false }
    let bytes = SecCertificateCopyData(certificate) as Data
    let hash = Insecure.SHA1.hash(data: bytes).map { String(format: "%02X", $0) }.joined()
    return hash == expected
}
guard matches.count == 1 else { fail("Expected exactly one identity matching the pinned certificate") }
var parameters = SecItemImportExportKeyParameters()
parameters.version = UInt32(SEC_KEY_IMPORT_EXPORT_PARAMS_VERSION)
let passphrase = password as CFString
parameters.passphrase = Unmanaged.passUnretained(passphrase)
var exported: CFData?
let exportStatus = withExtendedLifetime(passphrase) {
    SecItemExport(matches[0], .formatPKCS12, [], &parameters, &exported)
}
guard exportStatus == errSecSuccess, let exported else { fail("The selected signing identity could not be exported (\(exportStatus))") }
let url = URL(fileURLWithPath: CommandLine.arguments[2])
try (exported as Data).write(to: url, options: .atomic)
try FileManager.default.setAttributes([.posixPermissions: 0o600], ofItemAtPath: url.path)
