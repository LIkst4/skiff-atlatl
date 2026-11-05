#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""(custom) Where the firmware uses instruction forms Simon Johansson's FM-1 emulator does not decode (its
docs/instruction-gaps.md): ED5D..ED5F, a signed halfword load with a pre-increment past 255 bytes. The hardware runs
them; the emulator stops. Lists each with its function, from tools/build.py's listing:
    tools/emu_gaps.py build/felucca.dis"""
import re, sys
GAPS = {0xed5d, 0xed5e, 0xed5f}
fn, hits = "?", []
for line in open(sys.argv[1], encoding="utf-8", errors="replace"):
    m = re.match(r"^(?:[0-9a-f]+ <([^>]+)>|([A-Za-z_][A-Za-z_0-9.]*)):\s*$", line)
    if m:
        fn = m.group(1) or m.group(2)
        continue
    m = re.match(r"^\s*([0-9a-f]+):\s+([0-9a-f]{2}) ([0-9a-f]{2})", line)
    if m and int(m.group(3) + m.group(2), 16) in GAPS:
        hits.append((m.group(1), fn, line.strip()))
for a, f, l in hits:
    print(f"emu gap: {a} in {f}: {l}")
print(f"emu gaps: {len(hits)}")
