#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# (custom) Where the flash goes: the image's sections and its largest symbols, from `objdump -h -t` of the ELF.
#   tools/size_report.py build/felucca.elf [N]      (N: how many symbols, default 250)
# Needs JIELI_TOOLCHAIN (tools/get_toolchain.sh) for objdump. CI prints it after the build (build.yml), so the
# report is in the log without the toolchain at hand.
import os
import re
import subprocess
import sys
from collections import defaultdict

elf = sys.argv[1] if len(sys.argv) > 1 else "build/felucca.elf"
top = int(sys.argv[2]) if len(sys.argv) > 2 else 250
tc = os.environ.get("JIELI_TOOLCHAIN", os.path.expanduser("~/.jieli/toolchain"))
od = os.path.join(tc, "common", "bin", "objdump")
if not os.path.exists(od):
    od = "objdump"

hdr = subprocess.run([od, "-h", elf], capture_output=True, text=True, check=True).stdout
print("== sections")
flash = 0
for m in re.finditer(r"^\s*\d+\s+(\S+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9a-f]+)", hdr, re.M):
    name, size, vma, lma = m.group(1), int(m.group(2), 16), int(m.group(3), 16), int(m.group(4), 16)
    print(f"  {name:<14} {size:8d} B  vma {vma:#010x}  lma {lma:#010x}")
    if name in (".text", ".data", ".ram_text"):
        flash += size
print(f"  in the image (.text .data .ram_text): {flash} B")

syms = subprocess.run([od, "-t", elf], capture_output=True, text=True, check=True).stdout
rows = []
for line in syms.splitlines():
    m = re.match(r"^([0-9a-f]{8})\s+(.{7})\s+(\S+)\s+([0-9a-f]{8})\s+(\S+)$", line)
    if not m:
        continue
    sect, size, name = m.group(3), int(m.group(4), 16), m.group(5)
    if sect in (".text", ".data", ".ram_text") and size:
        kind = "F" if "F" in m.group(2) else "O"
        rows.append((size, sect, kind, name))
rows.sort(reverse=True)
code = sum(s for s, _, k, _ in rows if k == "F")
data = sum(s for s, _, k, _ in rows if k == "O")
print(f"== symbols in the image: {len(rows)}, functions {code} B, objects {data} B")

# by family: the name's prefix up to its first underscore (eng_*, ui_*, sy_*, ...), a rough map of the sources
fam = defaultdict(lambda: [0, 0])
for s, _, k, n in rows:
    p = n.split("_")[0] if "_" in n else n
    fam[p][0] += s
    fam[p][1] += 1
print("== by name prefix (top 60)")
for p, (s, c) in sorted(fam.items(), key=lambda x: -x[1][0])[:60]:
    print(f"  {s:7d} B  {c:4d}  {p}")
print(f"== the {top} largest")
for s, sect, k, n in rows[:top]:
    print(f"  {s:7d} {k} {sect:<10} {n}")
