#!/usr/bin/env python

"""
Convert asm into c++ (flycast codes).
"""

import re
from typing import NamedTuple


class Patch(NamedTuple):
    addr: int
    value: int
    section: str


class Symbol(NamedTuple):
    addr: int
    name: str


r_ope = re.compile(r"^\s*?([0-9a-f]+):\s*([0-9a-f]+) ([0-9a-f]+)")
r_symbol = re.compile(r"^([0-9a-f]+) <(\w+)>:")
r_section = re.compile(r"^Disassembly of section ([0-9a-zA-Z._]+):")
patches = []
symbols = {}
section = None
start = False

for line in open('bin/gdxpatch.asm'):
    line = line.rstrip()
    if 'Disassembly' in line:
        start = True
    if not start:
        continue

    g = r_section.match(line)
    if g:
        section = g.group(1).strip()
        print("section", section)

    g = r_symbol.match(line)
    if g:
        addr = int(g.group(1), 16)
        name = g.group(2)
        symbols[addr] = Symbol(addr, name)
        print("  ", name)

    g = r_ope.match(line)
    if g:
        addr = int(g.group(1), 16)
        patches.append(Patch(addr + 0, int(g.group(2), 16), section))
        patches.append(Patch(addr + 1, int(g.group(3), 16), section))

with open('bin/flycast.patch', 'w') as f:
    f.write(f"#ifndef _CLION_IDE__\n")

    f.write(f"#define W WriteMem8_nommu\n")
    for i, p in enumerate(patches):
        if p.section == "gdx.data":
            f.write(f"W(0x{p.addr:08x}, 0x{p.value:02x}); ")
        else:
            f.write(f"W(0x{0x80000000 + p.addr:08x}, 0x{p.value:02x}); ")
        if i and i % 10 == 0:
            f.write("\n")
    f.write(f"\n#undef W\n")

    for p in symbols.values():
        f.write(f'symbols["{p.name}"] = 0x{p.addr:08x};\n')
    f.write(f"#endif\n")
