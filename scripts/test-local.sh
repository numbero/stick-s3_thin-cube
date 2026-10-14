#!/bin/bash
# CPU-only checks. Never opens a serial device or flashes hardware.
set -euo pipefail
task_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$task_root"
task_compiler="${CXX:-c++}"
for task_test in firmware/test/*_test.cpp; do
    task_binary="${TMPDIR:-/tmp}/thincube-$(basename "$task_test" .cpp)"
    "$task_compiler" -std=c++17 -Wall -Wextra -pedantic "$task_test" -o "$task_binary"
    "$task_binary"
done
python3 -m unittest discover -s scripts -p 'test_battery_calibration.py'
if [[ "$(uname -s)" != Darwin ]]; then
    echo "Firmware checks complete; macOS Swift tests require a Mac and were skipped."
    exit 0
fi
export CLANG_MODULE_CACHE_PATH="$task_root/macos/.build/clang-cache"
export SWIFTPM_MODULECACHE_OVERRIDE="$task_root/macos/.build/swift-cache"
swift test --package-path macos --disable-sandbox
