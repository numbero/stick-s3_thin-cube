#!/usr/bin/env python3
"""Validate versions and assemble ThinCube release files. Never flash hardware."""
import argparse
import configparser
import hashlib
import json
from pathlib import Path
import plistlib
import re
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parent.parent
ENVIRONMENT = "m5stack-sticks3"
VERSION = re.compile(r"(?:0|[1-9]\d*)\.(?:0|[1-9]\d*)\.(?:0|[1-9]\d*)")


def run(*args, cwd=ROOT):
    return subprocess.check_output(args, cwd=cwd, text=True).strip()


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False) + "\n")


def digest(data):
    return hashlib.sha256(data).hexdigest()


def versions(root=ROOT):
    value = json.loads((root / "release.json").read_text())
    if set(value) != {"version", "firmware", "macos", "macos_build", "protocol"}:
        raise ValueError("release.json has missing or unknown fields")
    for key in ("version", "macos"):
        if not isinstance(value[key], str) or not VERSION.fullmatch(value[key]):
            raise ValueError(f"Invalid release.json {key}")
    if not isinstance(value["firmware"], str) or not value["firmware"].startswith("v") or not VERSION.fullmatch(value["firmware"][1:]):
        raise ValueError("Official firmware must have a vX.Y.Z version")
    if type(value["macos_build"]) is not int or value["macos_build"] < 1 or type(value["protocol"]) is not int or value["protocol"] != 1:
        raise ValueError("Invalid Mac build number or unsupported protocol")
    config = configparser.ConfigParser(interpolation=None)
    config.read(root / "firmware/platformio.ini")
    flags = config[f"env:{ENVIRONMENT}"]["build_flags"].replace('\\"', '"')
    found = re.findall(r'-DCLAWD_FW_VER="([^"]+)"', flags)
    info = plistlib.loads((root / "macos/Info.plist").read_bytes())
    if found != [value["firmware"]]:
        raise ValueError("release.json firmware does not match the production PlatformIO environment")
    if info["CFBundleShortVersionString"] != value["macos"] or info["CFBundleVersion"] != str(value["macos_build"]):
        raise ValueError("release.json Mac version/build does not match Info.plist")
    return value


def validate_tag(tag, main_ref=None, root=ROOT):
    value = versions(root)
    if tag != "v" + value["version"]:
        raise ValueError("Tag must equal v + release.json version")
    if main_ref:
        subprocess.run(["git", "merge-base", "--is-ancestor", "HEAD", main_ref], cwd=root, check=True)
        if run("git", "rev-parse", f"refs/tags/{tag}^{{commit}}", cwd=root) != run("git", "rev-parse", "HEAD", cwd=root):
            raise ValueError("Tag does not point at the checked-out commit")
    return value


def package_usb(metadata_path, output, value, root=ROOT):
    metadata = json.loads(metadata_path.read_text())[ENVIRONMENT]
    extra = metadata["extra"]
    if metadata["env_name"] != ENVIRONMENT:
        raise ValueError("Unexpected PlatformIO environment")
    images = list(extra["flash_images"]) + [{"offset": extra["application_offset"], "path": str(root / f"firmware/.pio/build/{ENVIRONMENT}/firmware.bin")}]
    images.sort(key=lambda item: int(item["offset"], 0))
    expected = [(0, "bootloader.bin", 0x8000), (0x8000, "partitions.bin", 0x1000),
                (0xE000, "boot_app0.bin", 0x2000), (0x10000, "firmware.bin", 0x200000)]
    if len(images) != len(expected):
        raise ValueError("Unexpected USB image count")
    archive = output / f"thincube-usb-{value['firmware']}.zip"
    flash = {"device": "ThinCube-StickS3-v1", "version": value["firmware"], "esptool": "4.11.0", "images": []}
    # Separate images leave the NVS range at 0x9000 untouched. Never merge raw gaps.
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as bundle:
        for image, (offset, name, maximum) in zip(images, expected):
            path = Path(image["path"])
            data = path.read_bytes()
            if int(image["offset"], 0) != offset or path.name != name or not 0 < len(data) <= maximum:
                raise ValueError(f"Unexpected flash image, offset or size: {name}")
            bundle.writestr(name, data)
            flash["images"].append({"file": name, "offset": hex(offset), "size": len(data), "sha256": digest(data)})
        bundle.writestr("flash.json", json.dumps(flash, indent=2) + "\n")
        bundle.write(root / "scripts/flash-release.py", "flash.py")
        bundle.writestr("README.txt", "ThinCube StickS3 USB install / recovery\n\n"
                        "Do not upload this ZIP or its bootloader to OTA. For routine updates use thincube-<version>.bin.\n"
                        "Install the flashing tool (no compiler needed): python3 -m pip install esptool==4.11.0\n"
                        "Connect StickS3 with a data USB cable, enter download mode, then run:\n"
                        "  python3 flash.py --port /dev/cu.usbmodem...\n"
                        "Windows example: python flash.py --port COM3\n"
                        "Keep USB connected. Flashing resets OTA selection and is intended for first install/recovery.\n"
                        "The files do not include device credentials, NVS, or a whole-device flash dump.\n")
    return archive


def firmware(metadata, output):
    value = versions()
    output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):
        raise ValueError("Firmware output directory must be empty")
    subprocess.run([sys.executable, str(ROOT / "scripts/package-firmware.py"),
                    "--expected-version", value["firmware"], "--output-dir", str(output)], check=True)
    package_usb(metadata, output, value)
    files = sorted(output.glob("*"))
    write_json(output / "firmware-build.json", {
        "commit": run("git", "rev-parse", "HEAD"), "version": value["firmware"],
        "platformio": run("pio", "--version"),
        "files": {path.name: digest(path.read_bytes()) for path in files},
    })


def macos(app, output):
    value = versions()
    info = plistlib.loads((app / "Contents/Info.plist").read_bytes())
    if info["CFBundleIdentifier"] != "app.thincube.mac" or info["CFBundleShortVersionString"] != value["macos"] or info["CFBundleVersion"] != str(value["macos_build"]):
        raise ValueError("Built app version does not match release.json")
    binary = app / "Contents/MacOS/ThinCube"
    if run("lipo", "-archs", str(binary)) != "arm64":
        raise ValueError("The first release supports arm64 only")
    deployment = run("xcrun", "vtool", "-show-build", str(binary))
    if re.findall(r"minos\s+(\S+)", deployment) != ["13.0"]:
        raise ValueError("Unexpected macOS deployment target; expected 13.0")
    subprocess.run(["bash", str(ROOT / "scripts/sign-macos.sh"), "--verify", str(app)], check=True)
    signature = subprocess.run(["codesign", "-d", "-r-", str(app)], capture_output=True, text=True, check=True)
    identity = re.search(r'certificate leaf = H"([a-fA-F0-9]{40})"', signature.stderr + signature.stdout)
    if not identity:
        raise ValueError("App does not use the pinned certificate requirement")
    output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):
        raise ValueError("Mac output directory must be empty")
    archive = output / f"ThinCube-{value['macos']}-macos-arm64.zip"
    subprocess.run(["ditto", "-c", "-k", "--sequesterRsrc", "--keepParent", str(app), str(archive)], check=True)
    write_json(output / "macos-build.json", {
        "commit": run("git", "rev-parse", "HEAD"), "version": value["macos"], "build": value["macos_build"],
        "arch": "arm64", "signing_identity": identity.group(1).upper(), "notarized": False,
        "swift": run("swift", "--version"), "xcode": run("xcodebuild", "-version"),
        "files": {archive.name: digest(archive.read_bytes())},
    })


def assemble(directory):
    value = versions()
    commit = run("git", "rev-parse", "HEAD")
    ota = f"thincube-{value['firmware']}.bin"
    usb = f"thincube-usb-{value['firmware']}.zip"
    mac = f"ThinCube-{value['macos']}-macos-arm64.zip"
    expected = {ota, usb, mac, "firmware-manifest.json", "firmware-build.json", "macos-build.json"}
    if {p.name for p in directory.iterdir()} != expected or any(p.is_symlink() or not p.is_file() for p in directory.iterdir()):
        raise ValueError("Release directory has missing or unexpected files")
    for name, payloads in (("firmware-build.json", {ota, usb, "firmware-manifest.json"}), ("macos-build.json", {mac})):
        build = json.loads((directory / name).read_text())
        if build["commit"] != commit or set(build["files"]) != payloads:
            raise ValueError(f"Wrong commit or file list in {name}")
        for filename, checksum in build["files"].items():
            if digest((directory / filename).read_bytes()) != checksum:
                raise ValueError(f"Checksum mismatch: {filename}")
    if json.loads((directory / "firmware-build.json").read_text())["version"] != value["firmware"]:
        raise ValueError("Unexpected firmware build version")
    fw = json.loads((directory / "firmware-manifest.json").read_text())
    if fw["file"] != ota or fw["ver"] != value["firmware"] or fw["protocol"] != value["protocol"] or fw["device"] != "ThinCube-StickS3-v1" or fw["size"] != (directory / ota).stat().st_size or fw["sha256"] != digest((directory / ota).read_bytes()):
        raise ValueError("Invalid OTA manifest")
    mac_build = json.loads((directory / "macos-build.json").read_text())
    if mac_build["version"] != value["macos"] or mac_build["build"] != value["macos_build"] or mac_build["arch"] != "arm64" or not re.fullmatch(r"[A-F0-9]{40}", mac_build["signing_identity"]):
        raise ValueError("Invalid Mac build metadata")
    with zipfile.ZipFile(directory / mac) as archive:
        info = plistlib.loads(archive.read("ThinCube.app/Contents/Info.plist"))
        if info["CFBundleShortVersionString"] != value["macos"] or info["CFBundleVersion"] != str(value["macos_build"]) or info["CFBundleIdentifier"] != "app.thincube.mac":
            raise ValueError("Mac ZIP contains an unexpected application")
    manifest = {**value, "tag": "v" + value["version"], "commit": commit,
                "macos_signing": "fixed-apple-identity-not-notarized",
                "files": {name: {"size": (directory / name).stat().st_size, "sha256": digest((directory / name).read_bytes())} for name in sorted(expected)}}
    write_json(directory / "release-manifest.json", manifest)
    checksums = [f"{digest(path.read_bytes())}  {path.name}\n" for path in sorted(directory.iterdir())]
    (directory / "SHA256SUMS.txt").write_text("".join(checksums))
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    validate = commands.add_parser("validate")
    validate.add_argument("--tag")
    validate.add_argument("--main-ref")
    validate.add_argument("--github-output", type=Path)
    pack = commands.add_parser("firmware")
    pack.add_argument("--metadata", type=Path, required=True)
    pack.add_argument("--output-dir", type=Path, default=ROOT / "artifacts/release-firmware")
    collect = commands.add_parser("assemble")
    collect.add_argument("--directory", type=Path, required=True)
    app = commands.add_parser("macos")
    app.add_argument("--app", type=Path, default=ROOT / "artifacts/ThinCube.app")
    app.add_argument("--output-dir", type=Path, default=ROOT / "artifacts/release-macos")
    args = parser.parse_args()
    if args.command == "validate":
        value = validate_tag(args.tag, args.main_ref) if args.tag else versions()
        print(json.dumps(value, indent=2))
        if args.github_output:
            with args.github_output.open("a") as output:
                output.write(f"tag=v{value['version']}\nfirmware={value['firmware']}\nmacos={value['macos']}\n")
    elif args.command == "firmware":
        firmware(args.metadata.resolve(), args.output_dir.resolve())
    elif args.command == "assemble":
        print(json.dumps(assemble(args.directory), indent=2))
    elif args.command == "macos":
        macos(args.app.resolve(), args.output_dir.resolve())


if __name__ == "__main__":
    try:
        main()
    except (ValueError, KeyError, OSError, subprocess.CalledProcessError) as error:
        raise SystemExit(str(error)) from error
