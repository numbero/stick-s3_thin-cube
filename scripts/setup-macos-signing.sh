#!/bin/bash
# Pin an existing Apple-issued identity; never create or silently rotate a key.
set -euo pipefail
umask 077
task_directory="${THINCUBE_SIGNING_DIR:-$HOME/Library/Application Support/ThinCube/Signing}"
task_pin="$task_directory/identity.sha1"
task_root="$(cd "$(dirname "$0")/.." && pwd)"
task_identity=""
if [ "$#" -eq 2 ] && [ "$1" = "--identity" ]; then
    task_identity="$2"
elif [ "$#" -ne 0 ]; then
    printf 'Usage: %s [--identity CERTIFICATE_SHA1]\n' "$0" >&2
    exit 2
elif [ -f "$task_pin" ]; then
    bash "$task_root/scripts/sign-macos.sh" --check
    printf 'Reusing the pinned Apple signing identity.\n'
    exit 0
else
    task_identities="$(security find-identity -v -p codesigning | awk '/"(Apple Development:|Mac Developer:|Developer ID Application:)/ && !seen[$2]++ { print $2 }')"
    task_count="$(printf '%s\n' "$task_identities" | awk 'NF { count++ } END { print count+0 }')"
    if [ "$task_count" -ne 1 ]; then
        printf 'Choose one Apple Development or Developer ID identity using --identity CERTIFICATE_SHA1.\n' >&2
        printf 'In Xcode: Settings > Apple Accounts > your team > Manage Certificates.\n' >&2
        printf 'List available identities with: security find-identity -v -p codesigning\n' >&2
        exit 1
    fi
    task_identity="$task_identities"
fi
if [[ ! "$task_identity" =~ ^[0-9A-Fa-f]{40}$ ]]; then
    printf 'Expected a certificate SHA-1 fingerprint, not its display name.\n' >&2
    exit 1
fi
task_identity="$(printf '%s' "$task_identity" | tr '[:lower:]' '[:upper:]')"
task_temp="$(mktemp -d "${TMPDIR:-/tmp}/thincube-signing.XXXXXX")"
trap 'rm -rf "$task_temp"' EXIT
mkdir -p "$task_temp/config" "$task_temp/probe.app/Contents/MacOS"
printf '%s\n' "$task_identity" > "$task_temp/config/identity.sha1"
cp /usr/bin/true "$task_temp/probe.app/Contents/MacOS/SigningProbe"
cat > "$task_temp/probe.app/Contents/Info.plist" <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
<key>CFBundleIdentifier</key><string>app.thincube.mac</string>
<key>CFBundleExecutable</key><string>SigningProbe</string>
<key>CFBundlePackageType</key><string>APPL</string>
</dict></plist>
EOF
# Verify the actual signature, Apple chain, certificate class and TeamIdentifier
# before changing the persistent pin. Names alone are not a trust decision.
THINCUBE_SIGNING_DIR="$task_temp/config" bash "$task_root/scripts/sign-macos.sh" "$task_temp/probe.app"
codesign -d --extract-certificates="$task_temp/certificate-" "$task_temp/probe.app"
mkdir -p "$task_directory"
chmod 700 "$task_directory"
if [ -f "$task_pin" ] && [ "$(cat "$task_pin")" != "$task_identity" ]; then
    cp "$task_pin" "$task_directory/identity.previous.sha1"
    if [ -f "$task_directory/certificate.cer" ]; then
        cp "$task_directory/certificate.cer" "$task_directory/certificate.previous.cer"
    fi
fi
cp "$task_temp/certificate-0" "$task_directory/certificate.cer"
printf '%s\n' "$task_identity" > "$task_directory/identity.sha1.new"
mv "$task_directory/identity.sha1.new" "$task_pin"
printf 'Pinned Apple signing identity: %s\n' "$task_identity"
printf 'Public certificate and fingerprint: %s\n' "$task_directory"
printf 'The existing private key remains in the login keychain.\n'
