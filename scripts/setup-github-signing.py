#!/usr/bin/env python3
"""Upload one explicitly selected Apple identity to this repository's Actions Secrets."""
import argparse
import base64
import getpass
import hashlib
import os
from pathlib import Path
import re
import secrets
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--p12", type=Path, help="Encrypted export of only the pinned signing identity")
    source.add_argument("--export-pinned-identity", action="store_true", help="Export only the pinned local identity and upload it; requires authorization to transfer its private key to GitHub")
    parser.add_argument("--repo", default="numbero/stick-s3_thin-cube")
    parser.add_argument("--identity", help="Certificate SHA-1; defaults to this Mac's pinned identity")
    args = parser.parse_args()
    pin = Path.home() / "Library/Application Support/ThinCube/Signing/identity.sha1"
    identity = (args.identity or pin.read_text().strip()).upper()
    if not re.fullmatch(r"[A-F0-9]{40}", identity) or not re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", args.repo):
        raise SystemExit("Invalid signing fingerprint or repository")
    if args.export_pinned_identity:
        password = secrets.token_urlsafe(32)
        with tempfile.TemporaryDirectory(prefix="thincube-ci-signing-") as temp:
            path = Path(temp) / "identity.p12"
            env = dict(os.environ)
            env["CLANG_MODULE_CACHE_PATH"] = str(Path(temp) / "clang-cache")
            subprocess.run(["swift", str(Path(__file__).with_name("export-pinned-signing.swift")), identity, str(path)],
                           input=password.encode(), env=env, check=True)
            upload(path.read_bytes(), password, identity, args.repo)
    else:
        upload_existing(args.p12, identity, args.repo)


def upload_existing(path, identity, repo):
    data = path.read_bytes()
    password = getpass.getpass("Password for the selected encrypted .p12: ")
    if not password:
        raise SystemExit("Use a non-empty export password")
    # Inspect the public leaf certificate only; never print private key material.
    cert = subprocess.run(["openssl", "pkcs12", "-in", str(path), "-clcerts", "-nokeys", "-passin", "stdin"],
                          input=password + "\n", capture_output=True, text=True, check=True)
    der = subprocess.run(["openssl", "x509", "-outform", "der"], input=cert.stdout.encode(), capture_output=True, check=True).stdout
    if hashlib.sha1(der).hexdigest().upper() != identity:
        raise SystemExit("The selected export is not the pinned ThinCube certificate; nothing was uploaded")
    upload(data, password, identity, repo)


def upload(data, password, identity, repo):
    subprocess.run(["gh", "repo", "view", repo, "--json", "nameWithOwner"], check=True, stdout=subprocess.DEVNULL)
    for name, value in (("MACOS_CERTIFICATE_P12_BASE64", base64.b64encode(data)), ("MACOS_CERTIFICATE_PASSWORD", password.encode())):
        subprocess.run(["gh", "secret", "set", name, "--repo", repo], input=value, check=True)
    subprocess.run(["gh", "variable", "set", "MACOS_SIGNING_IDENTITY_SHA1", "--repo", repo], input=identity.encode(), check=True)
    print(f"Configured the selected signing identity for {repo}. No private key was written to the repository.")
    print("Next: run the Release workflow manually on main to verify signed downloads before pushing a version tag.")


if __name__ == "__main__":
    main()
