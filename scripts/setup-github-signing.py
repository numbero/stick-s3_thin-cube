#!/usr/bin/env python3
"""Upload one explicitly selected Apple identity to this repository's Actions Secrets."""
import argparse
import base64
import getpass
import hashlib
from pathlib import Path
import re
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--p12", required=True, type=Path, help="Encrypted export of only the pinned signing identity")
    parser.add_argument("--repo", default="numbero/stick-s3_thin-cube")
    parser.add_argument("--identity", help="Certificate SHA-1; defaults to this Mac's pinned identity")
    args = parser.parse_args()
    pin = Path.home() / "Library/Application Support/ThinCube/Signing/identity.sha1"
    identity = (args.identity or pin.read_text().strip()).upper()
    if not re.fullmatch(r"[A-F0-9]{40}", identity) or not re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", args.repo):
        raise SystemExit("Invalid signing fingerprint or repository")
    data = args.p12.read_bytes()
    password = getpass.getpass("Password for the selected encrypted .p12: ")
    if not password:
        raise SystemExit("Use a non-empty export password")
    # Inspect the public leaf certificate only; never print private key material.
    cert = subprocess.run(["openssl", "pkcs12", "-in", str(args.p12), "-clcerts", "-nokeys", "-passin", "stdin"],
                          input=password + "\n", capture_output=True, text=True, check=True)
    der = subprocess.run(["openssl", "x509", "-outform", "der"], input=cert.stdout.encode(), capture_output=True, check=True).stdout
    if hashlib.sha1(der).hexdigest().upper() != identity:
        raise SystemExit("The selected export is not the pinned ThinCube certificate; nothing was uploaded")
    subprocess.run(["gh", "repo", "view", args.repo, "--json", "nameWithOwner"], check=True, stdout=subprocess.DEVNULL)
    for name, value in (("MACOS_CERTIFICATE_P12_BASE64", base64.b64encode(data)), ("MACOS_CERTIFICATE_PASSWORD", password.encode())):
        subprocess.run(["gh", "secret", "set", name, "--repo", args.repo], input=value, check=True)
    subprocess.run(["gh", "variable", "set", "MACOS_SIGNING_IDENTITY_SHA1", "--repo", args.repo], input=identity.encode(), check=True)
    print(f"Configured the selected signing identity for {args.repo}. No private key was written to the repository.")
    print("Next: run the Release workflow manually on main to verify signed downloads before pushing a version tag.")


if __name__ == "__main__":
    main()
