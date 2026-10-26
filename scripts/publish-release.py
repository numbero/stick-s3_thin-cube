#!/usr/bin/env python3
"""Publish a complete, verified release through a draft; never replace a public release."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

from release import ROOT, digest, run, validate_tag


def gh(*args):
    return run("gh", *args)


def verify(directory, tag):
    value = validate_tag(tag)
    manifest = json.loads((directory / "release-manifest.json").read_text())
    commit = run("git", "rev-parse", "HEAD")
    if manifest["tag"] != tag or manifest["commit"] != commit or any(manifest[key] != item for key, item in value.items()):
        raise ValueError("Release metadata does not match the selected commit/version")
    expected = {f"thincube-{value['firmware']}.bin", f"thincube-usb-{value['firmware']}.zip",
                f"ThinCube-{value['macos']}-macos-arm64.zip", "firmware-manifest.json",
                "firmware-build.json", "macos-build.json"}
    if set(manifest["files"]) != expected:
        raise ValueError("Unexpected release manifest file list")
    names = expected | {"release-manifest.json", "SHA256SUMS.txt"}
    if {p.name for p in directory.iterdir()} != names or any(p.is_symlink() or not p.is_file() for p in directory.iterdir()):
        raise ValueError("Incomplete release or unexpected attachments")
    for name, item in manifest["files"].items():
        data = (directory / name).read_bytes()
        if len(data) != item["size"] or digest(data) != item["sha256"]:
            raise ValueError(f"Release attachment failed verification: {name}")
    checksums = "".join(f"{digest((directory / name).read_bytes())}  {name}\n" for name in sorted(names - {"SHA256SUMS.txt"}))
    if (directory / "SHA256SUMS.txt").read_text() != checksums:
        raise ValueError("Invalid SHA256SUMS.txt")
    return manifest


def get_release(repo, tag):
    result = subprocess.run(["gh", "api", f"repos/{repo}/releases/tags/{tag}"], capture_output=True, text=True)
    if result.returncode == 0:
        return json.loads(result.stdout)
    if "(HTTP 404)" in result.stderr:
        return None
    raise RuntimeError(result.stderr.strip())


def require_draft(release, marker):
    if not release["draft"]:
        raise ValueError("This version is already published; use a new version/tag")
    if marker not in (release.get("body") or ""):
        raise ValueError("Existing draft belongs to another build; refusing to replace it")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--directory", required=True, type=Path)
    parser.add_argument("--tag", required=True)
    args = parser.parse_args()
    directory = args.directory.resolve()
    manifest = verify(directory, args.tag)
    repo = os.environ["GH_REPO"]
    if not re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", repo):
        raise ValueError("Invalid GH_REPO")
    reference = json.loads(gh("api", f"repos/{repo}/git/ref/tags/{args.tag}"))["object"]
    while reference["type"] == "tag":
        reference = json.loads(gh("api", f"repos/{repo}/git/tags/{reference['sha']}"))["object"]
    if reference["type"] != "commit" or reference["sha"] != manifest["commit"]:
        raise ValueError("Remote tag moved or does not point to this build")
    marker = f"<!-- thincube-release:{manifest['commit']} -->"
    current = get_release(repo, args.tag)
    if current:
        require_draft(current, marker)
    with tempfile.TemporaryDirectory(prefix="thincube-publish-") as temp:
        notes = Path(temp) / "notes.md"
        notes.write_text(f"ThinCube {args.tag}\n\n"
                         f"- StickS3 firmware: **{manifest['firmware']}**, protocol {manifest['protocol']}.\n"
                         f"- macOS app: **{manifest['macos']}** (build {manifest['macos_build']}), Apple Silicon, macOS 13+.\n"
                         f"- Source commit: `{manifest['commit']}`.\n\n"
                         "Download `thincube-<firmware-version>.bin` for phone / Wi-Fi OTA.\n"
                         "The USB ZIP is for first installation / recovery and includes `flash.py` instructions.\n"
                         "Unzip the macOS app and install `ThinCube.app` in `/Applications`.\n"
                         "This personal-use Mac build uses a fixed Apple signing identity and is **not notarized**; macOS may require approval to open it.\n"
                         "BlackHole 2ch is installed separately. Verify downloads against `SHA256SUMS.txt`.\n\n"
                         f"{marker}\n")
        if not current:
            gh("release", "create", args.tag, "--repo", repo, "--verify-tag", "--target", manifest["commit"],
               "--draft", "--title", f"ThinCube {args.tag}", "--notes-file", str(notes))
        current = get_release(repo, args.tag)
        require_draft(current, marker)
        names = {p.name for p in directory.iterdir()}
        if {item["name"] for item in current["assets"]} - names:
            raise ValueError("Draft has unexpected assets; refusing to publish")
        gh("release", "upload", args.tag, "--repo", repo, "--clobber", *[str(directory / name) for name in sorted(names)])
        require_draft(get_release(repo, args.tag), marker)
        downloaded = Path(temp) / "downloaded"
        gh("release", "download", args.tag, "--repo", repo, "--dir", str(downloaded))
        verify(downloaded, args.tag)
        # A slower older build must not replace a newer version's Latest badge.
        releases = json.loads(gh("release", "list", "--repo", repo, "--limit", "1000", "--json", "tagName,isDraft"))
        newer = any(not item["isDraft"] and re.fullmatch(r"v\d+\.\d+\.\d+", item["tagName"])
                    and tuple(map(int, item["tagName"][1:].split("."))) > tuple(map(int, manifest["version"].split("."))) for item in releases)
        gh("release", "edit", args.tag, "--repo", repo, "--notes-file", str(notes), "--draft=false", "--latest=false" if newer else "--latest")
        print(gh("release", "view", args.tag, "--repo", repo, "--json", "url", "--jq", ".url"))


if __name__ == "__main__":
    try:
        main()
    except (ValueError, KeyError, OSError, RuntimeError, subprocess.CalledProcessError) as error:
        raise SystemExit(str(error)) from error
