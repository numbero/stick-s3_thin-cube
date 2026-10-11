#!/usr/bin/env python3
"""Play and capture all thirteen diagnostic stories over one USB connection.

Requires pyserial and Pillow. Does not flash hardware or read pairing secrets.
Opening native USB on macOS can reset the device; wait for a healthy boot.
"""
import argparse
from pathlib import Path
import re
import struct
import time
import serial
from PIL import Image

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("port")
parser.add_argument("output", type=Path)
parser.add_argument("--version", help="Expected firmware version; defaults to platformio.ini")
parser.add_argument("--count", type=int, choices=(5, 13), default=13,
                    help="Use 5 for old firmware; current firmware previews all 13")
parser.add_argument("--start", type=int, default=1, help="Resume verification from this diagnostic number")
args = parser.parse_args()
if not 1 <= args.start <= args.count:
    parser.error("--start must be between 1 and --count")
if not args.version:
    config=(Path(__file__).resolve().parent.parent / "firmware/platformio.ini").read_text()
    args.version=re.search(r'CLAWD_FW_VER=.*?(v[0-9.]+)',config).group(1)
args.output.mkdir(parents=True, exist_ok=True)
port = serial.Serial()
port.port, port.baudrate, port.timeout = args.port, 115200, .15
port.dtr = port.rts = False
scenes = [("repair", 5.7), ("camp", 5.8), ("gravity", 4.4), ("sticker", 3.8), ("hole", 6.3),
          ("space", 6.0), ("water", 4.1), ("photo", 5.7), ("dance", 3.5),
          ("garden", 6.5), ("plane", 3.5), ("whale", 4.3), ("portal", 5.4)][:args.count]
active_name = None

class PreviewCancelled(RuntimeError):
    """Real device state preempted an explicitly requested preview."""

with (args.output / "device-demo.log").open("w") as log:
    def read_line():
        line = port.readline()
        text = line.decode(errors="replace")
        log.write(text)
        log.flush()
        if "[story]" in text or "=== clawd" in text or "[ota]" in text:
            print(text.strip(), flush=True)
        if active_name and f"[story] cancel {active_name} demo" in text:
            raise PreviewCancelled(active_name)
        return text

    def wait_for(fragment, timeout):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            text = read_line()
            if fragment in text:
                return text
        raise RuntimeError(f"Timed out waiting for {fragment}")

    def capture(name):
        capture_started = time.monotonic()
        port.write(b"screen\n")
        deadline = time.monotonic() + 8
        while time.monotonic() < deadline:
            line = read_line()
            fields = line.strip().split()
            if len(fields) == 5 and fields[0] == "TCFB":
                break
            if "TCFB unavailable" in line:
                raise RuntimeError("Device rejected capture during a busy state")
        else:
            raise RuntimeError("No framebuffer header")
        _, width, height, fmt, count = fields
        width, height, count = int(width), int(height), int(count)
        if (width, height, fmt, count) != (135, 240, "RGB565LE", 64800):
            raise RuntimeError("Unexpected framebuffer format")
        data = bytearray()
        while len(data) < count and time.monotonic() < deadline:
            data.extend(port.read(count - len(data)))
        if len(data) != count or port.readline().strip() or port.readline().strip() != b"TCFB-END":
            raise RuntimeError("Incomplete framebuffer")
        pixels = [(((p >> 11) & 31) * 255 // 31, ((p >> 5) & 63) * 255 // 63, (p & 31) * 255 // 31)
                  for (p,) in struct.iter_unpack("<H", data)]
        image = Image.new("RGB", (width, height))
        image.putdata(pixels)
        image.save(args.output / f"{name}.png")
        print(f"Captured actual LCD framebuffer: {name} ({time.monotonic() - capture_started:.2f}s)", flush=True)

    try:
        port.open()
        opened = time.monotonic()
        wait_for(f"=== clawd {args.version} ===", 20)
        wait_for("[hb] up 10s", 15)
        for number, (name, key_time) in enumerate(scenes[args.start - 1:], args.start):
            for attempt in range(3):
                try:
                    active_name = name
                    port.write(f"story {number}\n".encode())
                    wait_for(f"[story] start {name} demo", 4)
                    started = time.monotonic()
                    while time.monotonic() - started < key_time:
                        read_line()
                    capture(name)
                    # Allow USB export to finish before the next animation tick.
                    wait_for(f"[story] finish {name} demo", 8)
                    print(f"Confirmed {name} complete after {time.monotonic() - started:.2f}s including capture", flush=True)
                    active_name = None
                    break
                except PreviewCancelled:
                    active_name = None
                    if attempt == 2:
                        raise
                    print(f"Device state cancelled {name}; wait before explicit retry {attempt + 2}/3", flush=True)
                    stable_after = time.monotonic() + 3
                    while time.monotonic() < stable_after:
                        read_line()
        capture("diagnostic")
        # End diagnostic ownership and verify continued normal operation.
        port.write(b"story stop\n")
        wait_for("[story] active none demo 0", 8)
        capture("normal")
        # Observe beyond the 60-second boot-health window with preview stopped.
        deadline = max(time.monotonic() + 20, opened + 70)
        while time.monotonic() < deadline:
            read_line()
        print(f"Stories {args.start}–{len(scenes)} completed; diagnostic preview stopped", flush=True)
    finally:
        if port.is_open:
            active_name = None
            port.write(b"story stop\n")
            port.close()
