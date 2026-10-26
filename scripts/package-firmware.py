#!/usr/bin/env python3
"""Package an already-built app image. Never flash hardware."""
import hashlib
import json
import argparse
import re
from pathlib import Path

root = Path(__file__).resolve().parent.parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--image", type=Path, default=root / "firmware/.pio/build/m5stack-sticks3/firmware.bin")
parser.add_argument("--output-dir", type=Path, default=root / "artifacts")
parser.add_argument("--expected-version", help="Reject a stale or incorrectly versioned build")
args = parser.parse_args()
data = args.image.read_bytes()
identity = b"ThinCube-StickS3-v1".ljust(32, b"\0")
if not (352 <= len(data) <= 2 * 1024 * 1024 and data[0] == 0xE9
        and int.from_bytes(data[12:14], "little") == 9
        and data[32:36] == b"\x32\x54\xcd\xab" and data[288:320] == identity):
    raise SystemExit("Not a compatible ThinCube StickS3 app image")
version = data[320:352].split(b"\0", 1)[0].decode("ascii")
if not re.fullmatch(r"v?\d+\.\d+\.\d+(?:[-+][A-Za-z0-9.+_-]+)?", version):
    raise SystemExit("Invalid firmware version")
if args.expected_version and version != args.expected_version:
    raise SystemExit(f"Firmware version {version} does not match {args.expected_version}")
output = args.output_dir
output.mkdir(parents=True, exist_ok=True)
name = f"thincube-{version}.bin"
(output / name).write_bytes(data)
manifest = {"file": name, "ver": version, "device": "ThinCube-StickS3-v1",
            "protocol": 1, "size": len(data), "sha256": hashlib.sha256(data).hexdigest()}
(output / "firmware-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
print(json.dumps(manifest, indent=2))
