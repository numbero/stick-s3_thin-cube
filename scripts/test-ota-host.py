#!/usr/bin/env python3
"""Compile and exercise the real OTA worker against a small deterministic host port.

Only ota.cpp's leading includes are replaced. Its handlers, anonymous-namespace
state, cancellation boundary and boot-health hook run unchanged. The fixture
models ESP/HTTP side effects; it does not emulate flash hardware or SHA-256.
"""
from pathlib import Path
import os
import re
import subprocess
import tempfile


def main():
    root = Path(__file__).resolve().parent.parent
    source = (root / "firmware/src/ota.cpp").read_text()
    includes = re.match(r"(?:#include [^\n]+\n)+", source)
    if not includes:
        raise SystemExit("ota.cpp no longer starts with an include block; inspect the harness")
    with tempfile.TemporaryDirectory(prefix="thincube-ota-host-") as directory:
        temporary = Path(directory)
        # ota.h remains the production API. Its ESP header has already been
        # supplied by the host port before that header is included.
        (temporary / "esp_http_server.h").write_text("#pragma once\n")
        translation_unit = temporary / "ota_host.cpp"
        translation_unit.write_text(
            '#include "ota_host_fixture.h"\n'
            '#include "ota_image_policy.h"\n'
            f'#line {source[:includes.end()].count(chr(10)) + 1} "firmware/src/ota.cpp"\n'
            + source[includes.end():]
            + '\n#include "ota_host_fixture.cpp"\n'
        )
        executable = temporary / "ota_host"
        subprocess.run([
            os.environ.get("CXX", "c++"), "-std=c++17", "-O1", "-g",
            "-Wall", "-Wextra", "-Werror", "-Wno-c++20-designator",
            "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
            "-I", str(temporary), "-I", str(root / "firmware/test"),
            "-I", str(root / "firmware/src"), str(translation_unit),
            "-o", str(executable),
        ], check=True)
        subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    main()
