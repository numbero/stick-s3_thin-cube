#!/bin/bash
set -euo pipefail
task_root="$(cd "$(dirname "$0")/.." && pwd)"
task_install=1
if [ "$#" -gt 0 ]; then
    if [ "$#" -eq 1 ] && [ "$1" = "--no-install" ]; then
        task_install=0
    else
        printf 'Usage: %s [--no-install]\n' "$0" >&2
        exit 2
    fi
fi
bash "$task_root/scripts/sign-macos.sh" --check
cd "$task_root/macos"
task_scratch="${THINCUBE_BUILD_SCRATCH:-$task_root/macos/.build}"
export CLANG_MODULE_CACHE_PATH="$task_scratch/clang-cache"
export SWIFTPM_MODULECACHE_OVERRIDE="$task_scratch/swift-cache"
swift build -c release --disable-sandbox --scratch-path "$task_scratch"
task_bin="$(swift build -c release --disable-sandbox --scratch-path "$task_scratch" --show-bin-path)"
task_app="$task_root/artifacts/ThinCube.app"
mkdir -p "$task_app/Contents/MacOS" "$task_app/Contents/Resources"
cp "$task_bin/ThinCube" "$task_app/Contents/MacOS/ThinCube"
cp "$task_root/macos/Info.plist" "$task_app/Contents/Info.plist"
cp "$task_root/macos/Resources/AppIcon.icns" "$task_app/Contents/Resources/AppIcon.icns"
cp "$task_root/macos/Resources/MenuBarIconTemplate.pdf" "$task_app/Contents/Resources/MenuBarIconTemplate.pdf"
bash "$task_root/scripts/sign-macos.sh" "$task_app"
printf '%s\n' "$task_app"
if [ "$task_install" -eq 1 ]; then
    bash "$task_root/scripts/install-macos.sh"
fi
