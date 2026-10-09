#!/bin/bash
# Real signatures and a disposable Keychain item; never reads device credentials.
set -euo pipefail
umask 077
task_root="$(cd "$(dirname "$0")/.." && pwd)"
task_temp="$(mktemp -d "${TMPDIR:-/tmp}/thincube-signing-test.XXXXXX")"
task_service="app.thincube.signing-validation.$(uuidgen)"
task_added=0
task_first="$task_temp/first.app"
task_second="$task_temp/second.app"
task_cleanup() {
    if [ "$task_added" -eq 1 ]; then
        if ! "$task_first/Contents/MacOS/SigningProbe" delete "$task_service"; then
            if [ -d "$task_temp/original.app" ]; then
                rm -rf "$task_first"
                mv "$task_temp/original.app" "$task_first"
                "$task_first/Contents/MacOS/SigningProbe" delete "$task_service"
            fi
        fi
    fi
    rm -rf "$task_temp"
}
trap task_cleanup EXIT
bash "$task_root/scripts/sign-macos.sh" --check
export CLANG_MODULE_CACHE_PATH="$task_root/macos/.build/clang-cache"
export SWIFTPM_MODULECACHE_OVERRIDE="$task_root/macos/.build/swift-cache"

cat > "$task_temp/probe.swift.in" <<'EOF'
import Foundation
import Security

guard CommandLine.arguments.count == 3 else { exit(2) }
let revision = "__REVISION__"
let operation = CommandLine.arguments[1]
let service = CommandLine.arguments[2]
guard SecKeychainSetUserInteractionAllowed(false) == errSecSuccess else { exit(3) }
let query: [String: Any] = [
    kSecClass as String: kSecClassGenericPassword,
    kSecAttrService as String: service,
    kSecAttrAccount as String: "signing-validation"
]
let value = Data("disposable-signing-validation".utf8)
let status: OSStatus
switch operation {
case "add":
    var attributes = query
    attributes[kSecValueData as String] = value
    attributes[kSecAttrAccessible as String] = kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly
    status = SecItemAdd(attributes as CFDictionary, nil)
case "read":
    var attributes = query
    attributes[kSecReturnData as String] = true
    attributes[kSecMatchLimit as String] = kSecMatchLimitOne
    var result: CFTypeRef?
    status = SecItemCopyMatching(attributes as CFDictionary, &result)
    if status == errSecSuccess && result as? Data != value { exit(4) }
case "delete":
    status = SecItemDelete(query as CFDictionary)
default:
    exit(2)
}
guard status == errSecSuccess else {
    fputs("Signing probe \(revision): \(operation) failed (\(status))\n", stderr)
    exit(1)
}
print("Signing probe \(revision): \(operation) succeeded without user interaction")
EOF

for task_revision in first second; do
    task_app="$task_temp/$task_revision.app"
    mkdir -p "$task_app/Contents/MacOS"
    sed "s/__REVISION__/$task_revision/" "$task_temp/probe.swift.in" > "$task_temp/$task_revision.swift"
    swiftc "$task_temp/$task_revision.swift" -o "$task_app/Contents/MacOS/SigningProbe"
    cat > "$task_app/Contents/Info.plist" <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
<key>CFBundleIdentifier</key><string>app.thincube.mac</string>
<key>CFBundleExecutable</key><string>SigningProbe</string>
<key>CFBundlePackageType</key><string>APPL</string>
</dict></plist>
EOF
    bash "$task_root/scripts/sign-macos.sh" "$task_app"
done

task_first_dr="$(codesign -d -r- "$task_first" 2>/dev/null | sed -E 's/^(# )?designated => //')"
task_second_dr="$(codesign -d -r- "$task_second" 2>/dev/null | sed -E 's/^(# )?designated => //')"
task_first_hash="$(codesign -d --verbose=4 "$task_first" 2>&1 | sed -n 's/^CDHash=//p')"
task_second_hash="$(codesign -d --verbose=4 "$task_second" 2>&1 | sed -n 's/^CDHash=//p')"
test "$task_first_dr" = "$task_second_dr"
test "$task_first_hash" != "$task_second_hash"
codesign --verify --strict -R "=$task_first_dr" "$task_second"
codesign --verify --strict -R "=$task_second_dr" "$task_first"
printf 'Different program contents satisfy the same designated requirement.\n'

"$task_first/Contents/MacOS/SigningProbe" add "$task_service"
task_added=1
# Match the installed app's real update behavior: replace contents at one path.
mv "$task_first" "$task_temp/original.app"
ditto "$task_second" "$task_first"
"$task_first/Contents/MacOS/SigningProbe" read "$task_service"

ditto "$task_second" "$task_temp/rejected.app"
codesign --force --sign - "$task_temp/rejected.app"
if bash "$task_root/scripts/sign-macos.sh" --verify "$task_temp/rejected.app" > "$task_temp/rejection.log" 2>&1; then
    printf 'ERROR: An ad-hoc signature passed the fixed-identity policy.\n' >&2
    exit 1
fi
if THINCUBE_SIGNING_DIR="$task_temp/missing" bash "$task_root/scripts/sign-macos.sh" --check > "$task_temp/missing.log" 2>&1; then
    printf 'ERROR: A missing identity silently passed.\n' >&2
    exit 1
fi
printf 'Ad-hoc signatures and missing identities are rejected.\n'
