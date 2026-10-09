#!/bin/bash
set -euo pipefail
task_root="$(cd "$(dirname "$0")/.." && pwd)"
task_app="$task_root/artifacts/ThinCube.app"
task_destination="/Applications/ThinCube.app"
task_was_running=0
task_stage=""

task_identifier="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "$task_app/Contents/Info.plist")"
if [ "$task_identifier" != "app.thincube.mac" ]; then
    printf 'Refusing to install an unexpected application bundle.\n' >&2
    exit 1
fi
bash "$task_root/scripts/sign-macos.sh" --verify "$task_app"
if [ -d "$task_destination" ]; then
    task_installed_identifier="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "$task_destination/Contents/Info.plist")"
    if [ "$task_installed_identifier" != "$task_identifier" ]; then
        printf 'Refusing to replace a different application at %s\n' "$task_destination" >&2
        exit 1
    fi
fi

task_cleanup() {
    task_status=$?
    trap - EXIT
    if [ "$task_status" -ne 0 ] && [ -d "$task_stage/previous.app" ]; then
        if [ -d "$task_destination" ]; then
            mv "$task_destination" "$task_stage/failed.app"
        fi
        mv "$task_stage/previous.app" "$task_destination"
    fi
    if [ "$task_was_running" -eq 1 ] && [ -d "$task_destination" ]; then
        open "$task_destination" || true
    fi
    if [ -n "$task_stage" ] && [ -d "$task_stage" ]; then
        rm -rf "$task_stage"
    fi
    exit "$task_status"
}

# Stage and verify the complete new bundle before stopping the running app.
task_stage="$(mktemp -d /Applications/.ThinCube-install.XXXXXX)"
trap task_cleanup EXIT
ditto "$task_app" "$task_stage/ThinCube.app"
bash "$task_root/scripts/sign-macos.sh" --verify "$task_stage/ThinCube.app"

task_pids="$(pgrep -u "$(id -u)" -x ThinCube || true)"
if [ -n "$task_pids" ]; then
    task_was_running=1
    while IFS= read -r task_pid; do
        kill -TERM "$task_pid"
    done <<< "$task_pids"
    for ((task_attempt=0; task_attempt<50; task_attempt++)); do
        if ! pgrep -u "$(id -u)" -x ThinCube >/dev/null; then
            break
        fi
        sleep 0.2
    done
    if pgrep -u "$(id -u)" -x ThinCube >/dev/null; then
        printf 'ThinCube did not exit; the installed bundle was left unchanged.\n' >&2
        exit 1
    fi
fi

if [ -d "$task_destination" ]; then
    mv "$task_destination" "$task_stage/previous.app"
fi
mv "$task_stage/ThinCube.app" "$task_destination"
bash "$task_root/scripts/sign-macos.sh" --verify "$task_destination"
diff -qr "$task_app" "$task_destination"
touch "$task_destination"

if [ -d "$task_stage/previous.app" ]; then
    task_backup="$task_root/artifacts/install-backups/$(date +%Y%m%d-%H%M%S)-$$"
    mkdir -p "$task_backup"
    mv "$task_stage/previous.app" "$task_backup/ThinCube.app"
    printf 'Previous application saved: %s\n' "$task_backup/ThinCube.app"
fi
printf 'Installed and verified: %s\n' "$task_destination"
if [ "$task_was_running" -eq 1 ]; then
    printf 'Restarting ThinCube from /Applications.\n'
fi
