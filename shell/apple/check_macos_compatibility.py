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
parser.add_argument("--arch", choices=("universal", "x86_64", "arm64"), default="universal")
parser.add_argument("--dsym", type=Path)
parser.add_argument("--symbols", type=Path)
args = parser.parse_args()
app = args.app
architectures = ("x86_64", "arm64") if args.arch == "universal" else (args.arch,)
minimum = {"x86_64": "10.15", "arm64": "11.0"}
binary = app / "Contents/MacOS/Flycast"
info = plistlib.loads((app / "Contents/Info.plist").read_bytes())
assert info["LSMinimumSystemVersion"] == minimum[architectures[0]], "Incorrect app OS floor"
for arch in architectures:
    assert info["LSMinimumSystemVersionByArchitecture"][arch] == minimum[arch], \
        f"{arch}: incorrect architecture-specific OS floor"
assert info["LSEnvironment"]["MTL_HUD_ENABLED"] == "0", "Metal HUD must be disabled by default"

for macho in (binary, *sorted((app / "Contents/Frameworks").rglob("*.dylib"))):
    actual = output("lipo", "-archs", str(macho)).split()
    assert set(actual) == set(architectures), \
        f"Expected {architectures} in {macho}, found {actual}"

for arch in architectures:
    build = output("xcrun", "vtool", "-arch", arch, "-show-build", str(binary))
    floor = re.search(r"^\s*(?:minos|version) ([\d.]+)$", build, re.MULTILINE)
    assert floor and floor[1] == minimum[arch], f"{arch}: wrong Mach-O OS floor: {build}"
    libraries = output("otool", "-arch", arch, "-L", str(binary))
    metal_fx = [line for line in libraries.splitlines() if "/MetalFX.framework/" in line]
    assert all(", weak)" in line for line in metal_fx), "MetalFX must be optional"
    if arch == "arm64":
        assert len(metal_fx) == 1, "ARM release must include optional MetalFX"
    assert "libMoltenVK" not in libraries, "MoltenVK must remain dynamically loaded"

    # Defined symbols may be stripped; Objective-C runtime metadata survives.
    objc = output("otool", "-arch", arch, "-ov", str(binary))
    metal4_class = re.search(r"^\s*name\s+0x[0-9a-fA-F]+\s+FCMetal4Queue$", objc, re.MULTILINE)
    assert bool(metal4_class) == (arch == "arm64"), \
        f"{arch}: unexpected native Metal 4 renderer availability"
    # Imported symbols survive stripping because dyld still resolves them.
    symbols = output("nm", "-arch", arch, "-m", str(binary))
    for line in symbols.splitlines():
        if "(undefined)" in line and "_OBJC_CLASS_$_MTL" in line:
            if "_MTL4" in line or "_MTLFX" in line:
                assert "weak external" in line, f"New Metal API must be weak: {line}"
    print(f"{arch}: macOS {minimum[arch]}, optional MetalFX, Metal 4 isolation OK")

for name in ("Assets.car", "AppIcon.icns", "default.metallib"):
    assert (app / "Contents/Resources" / name).is_file(), f"Missing resource: {name}"

if args.dsym or args.symbols:
    assert args.dsym and args.symbols, "Provide both --dsym and --symbols"
    def uuids(path):
        return {arch: uuid.upper() for uuid, arch in re.findall(
            r"UUID: ([0-9A-Fa-f-]+) \(([^)]+)\)", output("dwarfdump", "--uuid", str(path)))}
    app_ids, debug_ids = uuids(binary), uuids(args.dsym)
    assert set(app_ids) == set(debug_ids) == set(architectures), "dSYM architectures mismatch"
    assert app_ids == debug_ids, "dSYM UUIDs do not match the packaged app"
    for arch in architectures:
        suffix = "x64" if arch == "x86_64" else arch
        path = args.symbols / f"Flycast-{suffix}.sym"
        with path.open() as symbol_file:
            header = symbol_file.readline().split()
            expected_id = app_ids[arch].replace("-", "") + "0"
            assert header == ["MODULE", "mac", arch, expected_id, "Flycast"], \
                f"Wrong Breakpad module in {path}: {header}"
            counts = {"FILE": 0, "FUNC": 0}
            for line in symbol_file:
                kind = line.split(" ", 1)[0]
                if kind in counts:
                    counts[kind] += 1
            assert all(counts.values()), f"Empty Breakpad debug information: {path}"
        print(f"{arch}: matching dSYM and Breakpad symbols OK")
print("macOS release metadata and resources OK; no runtime test performed")
