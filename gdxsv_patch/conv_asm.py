#!/usr/bin/env python

"""
Convert asm into c++ (flycast codes).
"""

import sys
import re
import time
from typing import NamedTuple

r_ope = re.compile(r"^\s*?([0-9a-f]+):\s*([0-9a-f]+) ([0-9a-f]+)\s*(.*)$")
r_symbol = re.compile(r"^([0-9a-f]+) <(\w+)>:")
r_section = re.compile(r"^Disassembly of section ([0-9a-zA-Z._]+):")
section = None
start = False

f = open('../core/gdxsv/gdxsv_patch.inc', 'w')
for line in open('bin/gdxsv_patch.asm'):
    line = line.rstrip()
    if 'Disassembly' in line:
        start = True
    if not start:
        continue

    g = r_section.match(line)
    if g:
        section = g.group(1).strip()
        print("section", section)
        f.write(f"//\n")
        f.write(f"// section {section}\n")
        f.write(f"//\n")

    g = r_ope.match(line)
    if g:
        addr = int(g.group(1), 16)
        data = int(g.group(2), 16) | int(g.group(3), 16) << 8
        if section in "gdx.func":
            addr += 0x80000000
        f.write(f"gdxsv_WriteMem16(0x{addr:08x}u, 0x{data:04x}u); // {g.group(4)} \n")

    g = r_symbol.match(line)
    if g:
        addr = int(g.group(1), 16)
        name = g.group(2)
        if section in ("gdx.data", "gdx.func"):
            f.write(f'symbols_["{name}"] = 0x{addr:08x};\n')

f.write(f'if (disk_ == 1) gdxsv_WriteMem32(0x8c181bb4, symbols_["gdx_dial_start_disk1"]);\n')
f.write(f'if (disk_ == 2) gdxsv_WriteMem32(0x8c1e0274, symbols_["gdx_dial_start_disk2"]);\n')
f.write(f'symbols_[":patch_id"] = {str(int(time.time()) % 100000000)};\n')
f.write(f'gdxsv_WriteMem32(symbols_["patch_id"], symbols_[":patch_id"]);\n')
f.close()
