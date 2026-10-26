"""Release boundaries: stale images, wrong commits, corrupt downloads, and public immutability."""
import importlib.util
import json
from pathlib import Path
import plistlib
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

import release

spec = importlib.util.spec_from_file_location("publish_release", Path(__file__).with_name("publish-release.py"))
publish = importlib.util.module_from_spec(spec)
spec.loader.exec_module(publish)
VALUE = {"version": "0.4.79", "firmware": "v0.4.79", "macos": "0.4.39", "macos_build": 60, "protocol": 1}
COMMIT = "a" * 40


def image(version="v0.4.79", size=1024):
    data = bytearray(size)
    data[0], data[12] = 0xE9, 9
    data[32:36] = b"\x32\x54\xcd\xab"
    data[288:320] = b"ThinCube-StickS3-v1".ljust(32, b"\0")
    data[320:352] = version.encode().ljust(32, b"\0")
    return bytes(data)


class ReleaseTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def source(self):
        for name in ("firmware", "macos", "scripts"):
            (self.root / name).mkdir()
        release.write_json(self.root / "release.json", VALUE)
        (self.root / "firmware/platformio.ini").write_text('[env:m5stack-sticks3]\nbuild_flags = -DCLAWD_FW_VER=\\"v0.4.79\\"\n')
        self.info = {"CFBundleIdentifier": "app.thincube.mac", "CFBundleShortVersionString": "0.4.39", "CFBundleVersion": "60"}
        (self.root / "macos/Info.plist").write_bytes(plistlib.dumps(self.info))
        return self.root

    def downloads(self):
        directory = self.root / "downloads"
        directory.mkdir()
        ota = "thincube-v0.4.79.bin"
        usb = "thincube-usb-v0.4.79.zip"
        mac = "ThinCube-0.4.39-macos-arm64.zip"
        (directory / ota).write_bytes(image())
        (directory / usb).write_bytes(b"usb bundle")
        release.write_json(directory / "firmware-manifest.json", {"file": ota, "ver": VALUE["firmware"], "device": "ThinCube-StickS3-v1", "protocol": 1, "size": 1024, "sha256": release.digest(image())})
        with zipfile.ZipFile(directory / mac, "w") as archive:
            archive.writestr("ThinCube.app/Contents/Info.plist", plistlib.dumps({"CFBundleIdentifier": "app.thincube.mac", "CFBundleShortVersionString": VALUE["macos"], "CFBundleVersion": "60"}))
        for name, filenames, extra in (("firmware-build.json", [ota, usb, "firmware-manifest.json"], {"version": VALUE["firmware"]}),
                                       ("macos-build.json", [mac], {"version": VALUE["macos"], "build": 60, "arch": "arm64", "signing_identity": "B" * 40})):
            release.write_json(directory / name, {"commit": COMMIT, **extra, "files": {item: release.digest((directory / item).read_bytes()) for item in filenames}})
        return directory

    def test_component_versions_are_independent_but_must_match_source(self):
        self.assertEqual(release.versions(self.source()), VALUE)
        self.info["CFBundleVersion"] = "61"
        (self.root / "macos/Info.plist").write_bytes(plistlib.dumps(self.info))
        with self.assertRaisesRegex(ValueError, "Mac version/build"):
            release.versions(self.root)

    def test_diagnostic_firmware_cannot_masquerade_as_production(self):
        self.source()
        path = self.root / "firmware/platformio.ini"
        path.write_text(path.read_text().replace("v0.4.79", "v0.4.79-diag"))
        with self.assertRaisesRegex(ValueError, "firmware does not match"):
            release.versions(self.root)

    def test_tag_must_match_release_version(self):
        self.source()
        with self.assertRaisesRegex(ValueError, "Tag must equal"):
            release.validate_tag("v0.4.80", root=self.root)

    def test_tag_outside_main_is_rejected(self):
        self.source()
        with patch.object(release.subprocess, "run", side_effect=subprocess.CalledProcessError(1, ["git"])):
            with self.assertRaises(subprocess.CalledProcessError):
                release.validate_tag("v0.4.79", "origin/main", self.root)

    def test_tag_must_point_to_checked_out_commit(self):
        self.source()
        with patch.object(release.subprocess, "run"), patch.object(release, "run", side_effect=["b" * 40, COMMIT]):
            with self.assertRaisesRegex(ValueError, "checked-out commit"):
                release.validate_tag("v0.4.79", "origin/main", self.root)

    def test_packager_rejects_stale_and_wrong_board_images_before_writing(self):
        path = self.root / "image.bin"
        output = self.root / "packaged"
        for data in (image("v0.4.78"), image()[:32] + b"bad!" + image()[36:], b"\xff" * 1024, image(size=2 * 1024 * 1024 + 1)):
            path.write_bytes(data)
            result = subprocess.run([sys.executable, str(release.ROOT / "scripts/package-firmware.py"), "--image", str(path), "--output-dir", str(output), "--expected-version", VALUE["firmware"]], capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertFalse(output.exists())

    def usb_metadata(self):
        self.source()
        shutil.copy(release.ROOT / "scripts/flash-release.py", self.root / "scripts/flash-release.py")
        build = self.root / "firmware/.pio/build/m5stack-sticks3"
        build.mkdir(parents=True)
        (build / "firmware.bin").write_bytes(image())
        entries = []
        for offset, name in (("0x0", "bootloader.bin"), ("0x8000", "partitions.bin"), ("0xe000", "boot_app0.bin")):
            path = self.root / name
            path.write_bytes(b"fixture")
            entries.append({"offset": offset, "path": str(path)})
        metadata = {release.ENVIRONMENT: {"env_name": release.ENVIRONMENT, "extra": {"flash_images": entries, "application_offset": "0x10000"}}}
        path = self.root / "metadata.json"
        release.write_json(path, metadata)
        return path, metadata

    def test_usb_package_includes_ota_selector_and_no_nvs(self):
        metadata, _ = self.usb_metadata()
        output = self.root / "output"
        output.mkdir()
        result = release.package_usb(metadata, output, VALUE, self.root)
        with zipfile.ZipFile(result) as bundle:
            self.assertEqual(set(bundle.namelist()), {"bootloader.bin", "partitions.bin", "boot_app0.bin", "firmware.bin", "flash.json", "flash.py", "README.txt"})
            flash = json.loads(bundle.read("flash.json"))
            self.assertEqual([int(item["offset"], 0) for item in flash["images"]], [0, 0x8000, 0xE000, 0x10000])
            for item in flash["images"]:
                self.assertEqual(release.digest(bundle.read(item["file"])), item["sha256"])

    def test_usb_package_rejects_nvs_overlap_or_missing_image(self):
        metadata, value = self.usb_metadata()
        output = self.root / "output"
        output.mkdir()
        value[release.ENVIRONMENT]["extra"]["flash_images"][1]["offset"] = "0x9000"
        release.write_json(metadata, value)
        with self.assertRaisesRegex(ValueError, "Unexpected flash image"):
            release.package_usb(metadata, output, VALUE, self.root)

    def test_assembled_release_can_be_verified_after_download(self):
        directory = self.downloads()
        with patch.object(release, "versions", return_value=VALUE), patch.object(release, "run", return_value=COMMIT):
            release.assemble(directory)
        with patch.object(publish, "validate_tag", return_value=VALUE), patch.object(publish, "run", return_value=COMMIT):
            self.assertEqual(publish.verify(directory, "v0.4.79")["commit"], COMMIT)
            (directory / "thincube-v0.4.79.bin").write_bytes(b"corrupted")
            with self.assertRaisesRegex(ValueError, "failed verification"):
                publish.verify(directory, "v0.4.79")

    def test_assembly_rejects_mixed_commit_builds(self):
        directory = self.downloads()
        path = directory / "macos-build.json"
        data = json.loads(path.read_text())
        data["commit"] = "b" * 40
        release.write_json(path, data)
        with patch.object(release, "versions", return_value=VALUE), patch.object(release, "run", return_value=COMMIT):
            with self.assertRaisesRegex(ValueError, "Wrong commit"):
                release.assemble(directory)

    def test_assembly_rejects_unexpected_files(self):
        directory = self.downloads()
        (directory / "private.p12").write_bytes(b"must not publish")
        with patch.object(release, "versions", return_value=VALUE), patch.object(release, "run", return_value=COMMIT):
            with self.assertRaisesRegex(ValueError, "unexpected files"):
                release.assemble(directory)

    def test_published_release_and_foreign_draft_cannot_be_overwritten(self):
        with self.assertRaisesRegex(ValueError, "already published"):
            publish.require_draft({"draft": False}, "marker")
        with self.assertRaisesRegex(ValueError, "another build"):
            publish.require_draft({"draft": True, "body": "someone else's draft"}, "marker")
        publish.require_draft({"draft": True, "body": "marker"}, "marker")

    def test_draft_lookup_uses_paginated_listing_instead_of_published_tag_endpoint(self):
        draft = {"tag_name": "v0.4.79", "draft": True, "body": "marker"}
        response = subprocess.CompletedProcess([], 0, json.dumps([[{"tag_name": "v0.4.78", "draft": False}], [draft]]), "")
        with patch.object(publish.subprocess, "run", return_value=response) as api:
            self.assertEqual(publish.get_release("owner/repo", "v0.4.79"), draft)
            self.assertIn("repos/owner/repo/releases?per_page=100", api.call_args.args[0])
            self.assertIn("--paginate", api.call_args.args[0])
            self.assertIsNone(publish.get_release("owner/repo", "v0.4.80"))
        with self.assertRaisesRegex(ValueError, "not visible"):
            publish.require_draft(None, "marker")

    def test_release_lookup_does_not_treat_permission_errors_as_missing_tags(self):
        response = subprocess.CompletedProcess([], 1, "", "HTTP 403")
        with patch.object(publish.subprocess, "run", return_value=response):
            with self.assertRaisesRegex(RuntimeError, "403"):
                publish.get_release("owner/repo", "v0.4.79")

    def test_new_draft_visibility_delay_is_retried_and_bounded(self):
        draft = {"draft": True, "body": "marker"}
        with patch.object(publish, "get_release", side_effect=[None, None, draft]), patch.object(publish.time, "sleep") as sleep:
            self.assertEqual(publish.wait_for_draft("owner/repo", "v0.4.79", "marker"), draft)
            self.assertEqual(sleep.call_count, 2)
        with patch.object(publish, "get_release", return_value=None), patch.object(publish.time, "sleep") as sleep:
            with self.assertRaisesRegex(ValueError, "did not become visible"):
                publish.wait_for_draft("owner/repo", "v0.4.79", "marker", attempts=2)
            sleep.assert_called_once_with(3)


if __name__ == "__main__":
    unittest.main()
