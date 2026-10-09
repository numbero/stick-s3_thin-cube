#!/bin/sh
# 将备份的原始固件写回 StickS3
# 用法: ./restore.sh [串口设备]
#   串口默认 /dev/cu.usbmodem1401，可用 `ls /dev/cu.usb*` 查看实际值
set -e

PORT="${1:-/dev/cu.usbmodem1401}"
DIR="$(cd "$(dirname "$0")" && pwd)"
BIN="$DIR/sticks3-original-8MB-20260923.bin"

if [ ! -f "$BIN" ]; then
  echo "找不到备份文件: $BIN" >&2
  exit 1
fi

echo "即将把原始固件写回 $PORT（整片 8MB，含 bootloader / 分区表 / 应用 / NVS）"
echo "设备需先进入下载模式：长按侧边复位键约 2 秒，直到内部绿色 LED 闪烁"
printf "确认写入? [y/N] "
read -r ans
[ "$ans" = "y" ] || { echo "已取消"; exit 0; }

python3 -m esptool --port "$PORT" --baud 921600 write_flash 0x0 "$BIN"

echo "写入完成，设备已自动复位。"