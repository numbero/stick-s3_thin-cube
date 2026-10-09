#!/bin/bash
# CPU-only checks. Never opens a serial device or flashes hardware.
set -euo pipefail
task_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$task_root"
for task_test in firmware/test/*_test.cpp; do
    task_binary="${TMPDIR:-/tmp}/thincube-$(basename "$task_test" .cpp)"
    clang++ -std=c++17 -Wall -Wextra -pedantic "$task_test" -o "$task_binary"
    "$task_binary"
done
export CLANG_MODULE_CACHE_PATH="$task_root/macos/.build/clang-cache"
export SWIFTPM_MODULECACHE_OVERRIDE="$task_root/macos/.build/swift-cache"
swift test --package-path macos --disable-sandbox
