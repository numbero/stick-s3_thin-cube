#!/bin/bash
set -euo pipefail
task_root="$(cd "$(dirname "$0")/.." && pwd)"
task_dir="$(mktemp -d "${TMPDIR:-/tmp}/thincube-legacy-tests.XXXXXX")"
trap 'rm -rf "$task_dir"' EXIT
"${CC:-cc}" -std=c99 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I"$task_root/firmware/lib/BoschBMI270/src" \
  "$task_root/firmware/test/legacy_sdk_boundary.c" \
  "$task_root/firmware/lib/BoschBMI270/src/bmi2.c" -o "$task_dir/sdk"
"$task_dir/sdk"
"${CXX:-c++}" -std=c++17 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  "$task_root/firmware/test/tap_diagnostic_test.cpp" -o "$task_dir/policy"
"$task_dir/policy"
