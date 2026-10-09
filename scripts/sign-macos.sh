#!/bin/bash
set -euo pipefail
task_directory="${THINCUBE_SIGNING_DIR:-$HOME/Library/Application Support/ThinCube/Signing}"
task_pin="$task_directory/identity.sha1"
if [ ! -f "$task_pin" ]; then
    printf 'No pinned ThinCube signing identity. Run bash scripts/setup-macos-signing.sh once.\n' >&2
    exit 1
fi
task_identity="$(cat "$task_pin")"
if [[ ! "$task_identity" =~ ^[0-9A-Fa-f]{40}$ ]]; then
    printf 'Invalid ThinCube signing fingerprint. Refusing to fall back to ad-hoc signing.\n' >&2
    exit 1
fi
task_identity="$(printf '%s' "$task_identity" | tr '[:lower:]' '[:upper:]')"
task_hash="$(printf '%s' "$task_identity" | tr '[:upper:]' '[:lower:]')"
task_requirement="identifier \"app.thincube.mac\" and certificate leaf = H\"$task_hash\""
# These Apple certificate classes receive a stable teamid Keychain partition.
task_apple_requirement='anchor apple generic and ((certificate 1[field.1.2.840.113635.100.6.2.1] exists and certificate leaf[field.1.2.840.113635.100.6.1.12] exists) or (certificate 1[field.1.2.840.113635.100.6.2.6] exists and certificate leaf[field.1.2.840.113635.100.6.1.13] exists))'

task_check_identity() {
    if ! security find-identity -v -p codesigning | awk -v identity="$task_identity" '$2 == identity && $0 ~ /"(Apple Development:|Mac Developer:|Developer ID Application:)/ { found=1 } END { exit !found }'; then
        printf 'Pinned Apple signing identity is unavailable, invalid or self-signed: %s\n' "$task_identity" >&2
        printf 'Configure Apple Development in Xcode, then run setup-macos-signing.sh --identity CERTIFICATE_SHA1.\n' >&2
        exit 1
    fi
}

task_verify() {
    codesign --verify --deep --strict -R "=($task_requirement) and ($task_apple_requirement)" "$task_app"
    task_actual="$(codesign -d -r- "$task_app" 2>/dev/null)"
    if [ "$task_actual" != "designated => $task_requirement" ]; then
        printf 'ThinCube designated requirement is not the pinned stable identity.\n' >&2
        exit 1
    fi
    task_team="$(codesign -d --verbose=4 "$task_app" 2>&1 | sed -n 's/^TeamIdentifier=//p')"
    if [ -z "$task_team" ] || [ "$task_team" = "not set" ]; then
        printf 'The signed application has no stable Apple TeamIdentifier.\n' >&2
        exit 1
    fi
}

if [ "$#" -eq 1 ] && [ "$1" = "--check" ]; then
    task_check_identity
    exit 0
elif [ "$#" -eq 2 ] && [ "$1" = "--verify" ]; then
    task_app="$2"
    task_verify
elif [ "$#" -eq 1 ]; then
    task_app="$1"
    task_identifier="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "$task_app/Contents/Info.plist")"
    if [ "$task_identifier" != "app.thincube.mac" ]; then
        printf 'Refusing to sign an unexpected application identifier.\n' >&2
        exit 1
    fi
    task_check_identity
    codesign --force --timestamp=none --sign "$task_identity" \
        --requirements "=designated => $task_requirement" "$task_app"
    task_verify
else
    printf 'Usage: %s --check | --verify APP | APP\n' "$0" >&2
    exit 2
fi
