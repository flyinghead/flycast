#!/usr/bin/env python3
"""Check release compatibility without launching Flycast."""
import plistlib
import re
import subprocess
import sys
from pathlib import Path


def output(*args):
    return subprocess.check_output(args, text=True)


app = Path(sys.argv[1])
binary = app / "Contents/MacOS/Flycast"
info = plistlib.loads((app / "Contents/Info.plist").read_bytes())
assert info["LSMinimumSystemVersion"] == "10.15", "App must allow Catalina"
assert info["LSMinimumSystemVersionByArchitecture"] == {
    "x86_64": "10.15", "arm64": "11.0"
}, "Unexpected architecture-specific OS floor"
assert info["LSEnvironment"]["MTL_HUD_ENABLED"] == "0", "Metal HUD must be disabled by default"

for arch, expected in (("x86_64", "10.15"), ("arm64", "11.0")):
    output("lipo", str(binary), "-verify_arch", arch)
    build = output("xcrun", "vtool", "-arch", arch, "-show-build", str(binary))
    floor = re.search(r"^\s*(?:minos|version) ([\d.]+)$", build, re.MULTILINE)
    assert floor and floor[1] == expected, f"{arch}: wrong Mach-O OS floor: {build}"
    libraries = output("otool", "-arch", arch, "-L", str(binary))
    metal_fx = [line for line in libraries.splitlines() if "/MetalFX.framework/" in line]
    assert len(metal_fx) == 1 and ", weak)" in metal_fx[0], "MetalFX must be optional"
    assert "libMoltenVK" not in libraries, "MoltenVK must remain dynamically loaded"
    symbols = output("nm", "-arch", arch, "-m", str(binary))
    assert ("_OBJC_CLASS_$_FCMetal4Queue" in symbols) == (arch == "arm64"), \
        f"{arch}: unexpected native Metal 4 renderer availability"
    for line in symbols.splitlines():
        if "(undefined)" in line and "_OBJC_CLASS_$_MTL" in line:
            if "_MTL4" in line or "_MTLFX" in line:
                assert "weak external" in line, f"New Metal API must be weak: {line}"
    print(f"{arch}: macOS {expected}, optional MetalFX, Metal 4 isolation OK")

for name in ("Assets.car", "AppIcon.icns", "default.metallib"):
    assert (app / "Contents/Resources" / name).is_file(), f"Missing resource: {name}"
print("Catalina release metadata and resources OK; no runtime test performed")
