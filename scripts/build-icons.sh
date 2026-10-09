#!/bin/bash
set -euo pipefail
task_root="$(cd "$(dirname "$0")/.." && pwd)"
task_resources="$task_root/macos/Resources"
task_iconset="$task_resources/AppIcon.iconset"
task_selected="$task_root/macos/Design/open-contour-v6/exports/A-porcelain"
mkdir -p "$task_iconset"

# Install the exact approved exports, including their native ICNS encoding.
for task_asset in AppIcon.svg AppIcon.pdf AppIcon.png AppIcon.icns; do
    cp "$task_selected/$task_asset" "$task_resources/$task_asset"
done
cp "$task_selected/AppIcon.iconset/"*.png "$task_iconset/"
printf 'Application icon selected: A / porcelain — %s\n' "$task_resources/AppIcon.icns"
