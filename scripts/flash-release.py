#!/usr/bin/env python3
"""Flash the adjacent USB release bundle, without downloading or building source."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True, help="USB serial port, e.g. /dev/cu.usbmodem...")
    parser.add_argument("--baud", type=int, default=921600)
    args = parser.parse_args()
    root = Path(__file__).resolve().parent
    manifest = json.loads((root / "flash.json").read_text())
    images = manifest["images"]
    expected = [(0, "bootloader.bin"), (0x8000, "partitions.bin"),
                (0xE000, "boot_app0.bin"), (0x10000, "firmware.bin")]
    if [(int(item["offset"], 0), item["file"]) for item in images] != expected:
        raise SystemExit("Unexpected StickS3 flash layout")
    command = [sys.executable, "-m", "esptool", "--chip", "esp32s3",
               "--port", args.port, "--baud", str(args.baud),
               "--before", "default_reset", "--after", "watchdog_reset",
               "write_flash", "--flash_size", "8MB"]
    for item in images:
        path = root / item["file"]
        data = path.read_bytes()
        if len(data) != item["size"] or hashlib.sha256(data).hexdigest() != item["sha256"]:
            raise SystemExit(f"Corrupt or incomplete USB image: {path.name}")
        command += [item["offset"], str(path)]
    print("Writing bootloader, partition table, OTA selector and application over USB.", flush=True)
    print("Use the OTA .bin instead for an ordinary update that preserves device settings.", flush=True)
    subprocess.run(command, check=True)


if __name__ == "__main__":
    main()
