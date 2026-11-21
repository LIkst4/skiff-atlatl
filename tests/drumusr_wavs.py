#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# (custom) two WAVs for tests/drumusr_test.c: a kick (a falling sine, 0.30 s) and a snare (noise, 0.15 s), 44.1 kHz
# 16-bit mono, as a user would drop them on the editor's Samples tab
#   drumusr_wavs.py OUT_DIR
import math
import random
import struct
import sys
import wave
from pathlib import Path


def put(path, xs, rate=44100):
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(b"".join(struct.pack("<h", max(-32767, min(32767, int(x * 30000)))) for x in xs))


out = Path(sys.argv[1])
out.mkdir(parents=True, exist_ok=True)
n = int(0.30 * 44100)
ph, kick = 0.0, []
for i in range(n):
    f = 50 + 120 * math.exp(-i / 2000.0)
    ph += 2 * math.pi * f / 44100
    kick.append(math.sin(ph) * math.exp(-i / 6000.0))
put(out / "kick.wav", kick)
rnd = random.Random(7)
n = int(0.15 * 44100)
put(out / "snare.wav", [(rnd.random() * 2 - 1) * math.exp(-i / 1500.0) for i in range(n)])
