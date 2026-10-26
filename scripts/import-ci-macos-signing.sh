#!/bin/bash
# GitHub-hosted macOS only. Import the selected identity into a disposable keychain.
set -euo pipefail
umask 077
: "${RUNNER_TEMP:?Only run this script in GitHub Actions}"
: "${GITHUB_ENV:?Only run this script in GitHub Actions}"
: "${MACOS_CERTIFICATE_P12_BASE64:?Configure the signing certificate Secret}"
: "${MACOS_CERTIFICATE_PASSWORD:?Configure the certificate password Secret}"
: "${MACOS_SIGNING_IDENTITY_SHA1:?Configure the expected signing identity variable}"
if [[ ! "$MACOS_SIGNING_IDENTITY_SHA1" =~ ^[0-9A-Fa-f]{40}$ ]]; then
    printf 'Expected a 40-character signing certificate SHA-1.\n' >&2
    exit 1
fi
task_root="$(cd "$(dirname "$0")/.." && pwd)"
task_keychain="$RUNNER_TEMP/thincube-signing.keychain-db"
task_certificate="$RUNNER_TEMP/thincube-signing.p12"
task_signing="$RUNNER_TEMP/thincube-signing"
task_password="$(openssl rand -hex 24)"
task_cleanup() {
    task_status=$?
    rm -f "$task_certificate"
    if [ "$task_status" -ne 0 ]; then
        security delete-keychain "$task_keychain" >/dev/null 2>&1 || true
    fi
    exit "$task_status"
}
trap task_cleanup EXIT
printf 'THINCUBE_CI_KEYCHAIN=%s\nTHINCUBE_SIGNING_DIR=%s\n' "$task_keychain" "$task_signing" >> "$GITHUB_ENV"
python3 - "$task_certificate" <<'PY'
import base64, os, pathlib, sys
pathlib.Path(sys.argv[1]).write_bytes(base64.b64decode(os.environ['MACOS_CERTIFICATE_P12_BASE64'], validate=True))
PY
security create-keychain -p "$task_password" "$task_keychain"
security set-keychain-settings -lut 21600 "$task_keychain"
security unlock-keychain -p "$task_password" "$task_keychain"
security import "$task_certificate" -P "$MACOS_CERTIFICATE_PASSWORD" \
    -k "$task_keychain" -T /usr/bin/codesign -T /usr/bin/security
security set-key-partition-list -S apple-tool:,apple:,codesign: -s -k "$task_password" "$task_keychain" >/dev/null
security list-keychains -d user -s "$task_keychain" "$HOME/Library/Keychains/login.keychain-db"
mkdir -p "$task_signing"
printf '%s\n' "$MACOS_SIGNING_IDENTITY_SHA1" | tr '[:lower:]' '[:upper:]' > "$task_signing/identity.sha1"
THINCUBE_SIGNING_DIR="$task_signing" bash "$task_root/scripts/sign-macos.sh" --check
