#!/usr/bin/env python3
"""Check release compatibility without launching Flycast."""
import argparse
import plistlib
import re
import subprocess
from pathlib import Path


def output(*args):
    return subprocess.check_output(args, text=True)


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("app", type=Path)
parser.add_argument("--arch", choices=("x86_64", "arm64"), required=True)
args = parser.parse_args()
app = args.app
arch = args.arch
expected = "10.15" if arch == "x86_64" else "11.0"
binary = app / "Contents/MacOS/Flycast"
info = plistlib.loads((app / "Contents/Info.plist").read_bytes())
assert info["LSMinimumSystemVersion"] == expected, f"{arch}: incorrect app OS floor"
assert info["LSMinimumSystemVersionByArchitecture"][arch] == expected, \
    f"{arch}: incorrect architecture-specific OS floor"
assert info["LSEnvironment"]["MTL_HUD_ENABLED"] == "0", "Metal HUD must be disabled by default"

# Reject universal output, including accidentally bundled universal dylibs.
for macho in (binary, *sorted((app / "Contents/Frameworks").rglob("*.dylib"))):
    actual = output("lipo", "-archs", str(macho)).split()
    assert actual == [arch], f"Expected only {arch} in {macho}, found {actual}"

build = output("xcrun", "vtool", "-arch", arch, "-show-build", str(binary))
floor = re.search(r"^\s*(?:minos|version) ([\d.]+)$", build, re.MULTILINE)
assert floor and floor[1] == expected, f"{arch}: wrong Mach-O OS floor: {build}"
libraries = output("otool", "-arch", arch, "-L", str(binary))
metal_fx = [line for line in libraries.splitlines() if "/MetalFX.framework/" in line]
assert all(", weak)" in line for line in metal_fx), "MetalFX must be optional"
if arch == "arm64":
    assert len(metal_fx) == 1, "ARM release must include optional MetalFX"
assert "libMoltenVK" not in libraries, "MoltenVK must remain dynamically loaded"

# Xcode strips defined symbols from release apps. Objective-C class metadata
# must survive stripping so the runtime can instantiate the native renderer.
objc = output("otool", "-arch", arch, "-ov", str(binary))
metal4_class = re.search(r"^\s*name\s+0x[0-9a-fA-F]+\s+FCMetal4Queue$", objc, re.MULTILINE)
assert bool(metal4_class) == (arch == "arm64"), \
    f"{arch}: unexpected native Metal 4 renderer availability"
# Imported symbols must survive stripping because dyld still resolves them.
symbols = output("nm", "-arch", arch, "-m", str(binary))
for line in symbols.splitlines():
    if "(undefined)" in line and "_OBJC_CLASS_$_MTL" in line:
        if "_MTL4" in line or "_MTLFX" in line:
            assert "weak external" in line, f"New Metal API must be weak: {line}"
print(f"{arch} only: macOS {expected}, optional MetalFX, Metal 4 isolation OK")

for name in ("Assets.car", "AppIcon.icns", "default.metallib"):
    assert (app / "Contents/Resources" / name).is_file(), f"Missing resource: {name}"
print("macOS release metadata and resources OK; no runtime test performed")
