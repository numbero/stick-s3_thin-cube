#!/usr/bin/env python3
"""Exercise real capture and CPU profile code without a device or audio source."""
from pathlib import Path
import os
import re
import subprocess
import tempfile


def main():
    root = Path(__file__).resolve().parent.parent
    with tempfile.TemporaryDirectory(prefix="thincube-audio-power-host-") as directory:
        temporary = Path(directory)
        units = ['#include "audio_power_host_fixture.h"\n']
        for name in ("power_save.cpp", "audio_input.cpp"):
            source = (root / "firmware/src" / name).read_text()
            includes = re.match(r"(?:#include [^\n]+\n)+", source)
            if not includes:
                raise SystemExit(f"{name} no longer starts with an include block")
            units.append(f'#line {source[:includes.end()].count(chr(10)) + 1} "firmware/src/{name}"\n')
            units.append(source[includes.end():])
        units.append('\n#include "audio_power_host_fixture.cpp"\n')
        translation_unit = temporary / "audio_power_host.cpp"
        translation_unit.write_text("\n".join(units))
        executable = temporary / "audio_power_host"
        subprocess.run([
            os.environ.get("CXX", "c++"), "-std=c++17", "-O1", "-g",
            "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined",
            "-fno-omit-frame-pointer", "-I", str(root / "firmware/test"),
            "-I", str(root / "firmware/src"), str(translation_unit), "-o", str(executable),
        ], check=True)
        subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    main()
