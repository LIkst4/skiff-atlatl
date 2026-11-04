#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""(custom) SYNTH's OBERVAVE presets: OberVave's factory programs (zvenson, <https://github.com/zvenson/obervave>,
GPL-3.0-only) brought over as SYNTH presets, written to firmware/src/synth_obv.h (committed: the build doesn't need
OberVave's source).

    python3 tools/obv_presets.py <obervave checkout> [firmware/src/synth_obv.h]

A program is OberVave's defaults (obv_params.h PDEF) with its own values over them (obv_patches.h). Each becomes:
  OSC A    VA (SAW: SHAPE 0, PULSE: 64 + its width off 50 %, SAW+PLS: 32); OSC 1 OFF with OSC 2 OFF and NOISE: NOISE
  OSC B    VA as A, SYNC on: B's SYNC; OSC 2 OFF: OFF. B TUNE its SEMI, DETUNE its FINE (cents), MIX the levels (OberVave's
           square law: v^2)
  FILTER   LP 24: OB 24; LP 12, SEM, NOTCH, HP 12, BP 12: OB 12 (its CHAR: SEM's MORPH, or the type's place on it);
           CUTOFF to the same Hz (flt.c: 20 Hz x 2^(CUT x 0.0766)); RESO as is
  ENV      the amp's ADSR (both use Felucca's time table); ENV -> FILTER the filter envelope's sweep in semitones (one
           envelope here; past ENV -> FILTER's 63 half of the rest on the CUTOFF)
  FX       DRIVE, CHORUS MIX, DELAY MIX, REVERB MIX as the sends
  MONO     MODE MONO or UNISON
  the 4th value: X-MOD if any, else the filter's CHAR (not LP), else GLIDE (mono), else DRIFT (VINTAGE >= 40)
What has no place (the second envelope, the LFO, the arpeggiator, unison's voices) is left out: a sound after the
program, not the program. INIT and VIBE's six (65-70: the drums and two keys) are left out.
"""
import math
import os
import re
import sys

PT = {"BASS": "PT_BASS", "LEAD": "PT_LEAD", "PAD": "PT_PAD", "KEYS": "PT_KEYS", "PLUCK": "PT_PLUCK", "FX": "PT_FX"}
TAGS = [("BASS", ("BASS", " BS", "ACID")), ("PAD", ("PAD", "STRING", "CHOIR", "DRONE", "WASH", "ENSEMBLE", "SWEEP",
                                                    "RISE", "SPACE")),
        ("KEYS", ("KEYS", " EP", "PIANO", "WURLI", "ORGN", "BELL", "CLAV")),
        ("PLUCK", ("PLUCK", "STAB", "HIT", "ARP", "CHORD")),
        ("LEAD", ("LD", "LEAD", "SCREAM", "FLUTE", "BRASS", "HORN", "GROWL", "QUACK", "ZAP", "DRIVE", "FALL", "SAW",
                  "LASER"))]
SKIP = {"INIT", "KICK", "SNARE", "HIHAT", "CLAP"}
VIBE = False                                    # (65-70 VIBE's E-PIANO, WURLI: left out, SYNTH's presets stay under 128:
                                                # the editor's LIST2 and MIDI's program numbers are 7 bits)


def strip_comments(s):
    s = re.sub(r"/\*.*?\*/", " ", s, flags=re.S)
    return re.sub(r"//[^\n]*", " ", s)


def parse_enums(src, sym):
    for body in re.findall(r"enum\s*\{(.*?)\}", src, flags=re.S):
        v = 0
        for item in body.split(","):
            item = item.strip()
            if not item:
                continue
            if "=" in item:
                n, e = (x.strip() for x in item.split("=", 1))
                v = int(e) if re.fullmatch(r"-?\d+", e) else sym[e]
            else:
                n = item
            sym[n] = v
            v += 1


def main():
    root = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else "firmware/src/synth_obv.h"
    src_p = strip_comments(open(os.path.join(root, "firmware/src/obv_params.h")).read())
    src_x = strip_comments(open(os.path.join(root, "firmware/src/obv_patches.h")).read())
    sym = {"X": 255}
    parse_enums(src_p, sym)
    npar = sym["NPARAM"]
    dflt = [0] * npar
    for m in re.finditer(r"\[(P_\w+)\]\s*=\s*\{\"[^\"]*\",\s*([^,]+),\s*(\w+),", src_p):
        dflt[sym[m.group(1)]] = sym.get(m.group(3), None) if not m.group(3).isdigit() else int(m.group(3))
    arrays = {}
    for m in re.finditer(r"static const uint8_t (PF_\w+)\[\]\s*=\s*\{(.*?)\};", src_x, flags=re.S):
        toks = [t.strip() for t in m.group(2).split(",") if t.strip()]
        arrays[m.group(1)] = [int(t) if re.fullmatch(r"\d+", t) else sym[t] for t in toks]
    progs = []
    for tbl in ("FACTORY", "FACT_VIBE") if VIBE else ("FACTORY",):
        m = re.search(r"static const factory_t " + tbl + r"\[[^\]]*\]\s*=\s*\{(.*?)\};", src_x, flags=re.S)
        progs += re.findall(r"\{\"([^\"]+)\",\s*(PF_\w+)\}", m.group(1))
    s = sym

    def prog(arr):
        v = list(dflt)
        a = arrays[arr]
        i = 0
        while i < len(a) and a[i] != 255:
            v[a[i]] = a[i + 1]
            i += 2
        return v

    def shape(w, pw):
        if w == s["W_PLS"]:
            return min(127, 64 + abs(64 - pw) * 63 // 64)
        return 32 if w == s["W_BOTH"] else 0

    def cut(c):                                 # OberVave's CUT_HZ[(v + 8) % 137] ~ 8.18 Hz x 2^((v + 8) / 12)
        hz = 440.0 * 2 ** ((c + 8 - 69) / 12.0)
        return max(0, min(127, round(math.log2(max(hz, 20.0) / 20.0) / (8 * 98 / 10240))))

    def tag(name):
        for t, keys in TAGS:
            if any(k in " " + name for k in keys):
                return PT[t]
        return "PT_FX"

    here = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "firmware/src")
    seen = set()                                # (every engine's preset names: each its own, ui_test; a clash gets OV)
    for d, _, fs in os.walk(here):
        for f in fs:
            if f.endswith((".c", ".h")) and f != "synth_obv.h":
                seen |= set(re.findall(r'^\s*\{"([^"]+)", \{', open(os.path.join(d, f), errors="replace").read(),
                                       flags=re.M))
    rows = []
    for name, arr in progs:
        if name in SKIP:
            continue
        v = prog(arr)
        w1, w2 = v[s["P_O1_WAVE"]], v[s["P_O2_WAVE"]]
        l1 = (v[s["P_O1_LVL"]] ** 2) >> 7 if w1 != s["W_OFF"] else 0
        l2 = (v[s["P_O2_LVL"]] ** 2) >> 7 if w2 != s["W_OFF"] else 0
        noise = v[s["P_NOISE"]]
        if l1 == 0 and l2 == 0:
            a = ("SY_NOISE", 127 - noise // 2, 0)       # (the noise alone: its colour bright)
        else:
            a = ("SY_VA", shape(w1, v[s["P_O1_PW"]]), 0)
        if w2 == s["W_OFF"] or l2 == 0:
            b, bs, mix = "0", 0, 0
        else:
            b = "SYB_SYNC" if v[s["P_SYNC"]] else "1 + SY_VA"
            bs = shape(w2, v[s["P_O2_PW"]])
            mix = 127 if l1 == 0 else round(127 * l2 / (l1 + l2))
        semi = v[s["P_O2_SEMI"]] - 24
        det = min(127, round(abs(v[s["P_O2_FINE"]] - 64) * 2 * 100 / 256 * 127 / 50))
        ft = v[s["P_FTYPE"]]
        ftn = "FT_OB24" if ft == s["F_LP24"] else "FT_OB12"
        chr_ = {s["F_LP12"]: 0, s["F_NOTCH"]: 64, s["F_HP12"]: 110, s["F_BP12"]: 127,
                s["F_LP24"]: 0, s["F_SEM"]: v[s["P_FMORPH"]]}[ft]
        # the filter envelope's sweep: OberVave's amount a moves it a x 1.52 semitones at full (obv_engine.h: x 390 >> 15
        # of a 256-a-semitone index), ours f x 0.914 (voice.c >> 7, eng_synth.c >> 5 of flt.c's 9.8 octaves in 1024);
        # past 63 what is left lifts the cutoff by half of it (the peak a little lower, the floor a little higher)
        st = (v[s["P_FENV_AMT"]] - 64) * 1.52
        fenv = max(-64, min(63, round(st / 0.914)))
        lift = max(0.0, st - fenv * 0.914) / 2
        mode = v[s["P_MODE"]]
        mono = 1 if mode in (s["M_MONO"], s["M_UNISON"]) else 0
        more = ["SYF(%s, %d, %d)" % (ftn, min(127, cut(v[s["P_CUTOFF"]]) + round(lift / (12 * 8 * 98 / 10240))),
                                     v[s["P_RESO"]])]
        if v[s["P_XMOD"]]:
            more.append("{P_SYXMOD, %d}" % v[s["P_XMOD"]])
        elif chr_:
            more.append("{P_FCHR, %d}" % chr_)
        elif mono and v[s["P_GLIDE"]]:
            more.append("{P_GLIDE, %d}" % v[s["P_GLIDE"]])
        elif v[s["P_VINTAGE"]] >= 40:
            more.append("{P_SYDRFT, %d}" % v[s["P_VINTAGE"]])
        env = [v[s["P_AA"]], v[s["P_AD"]], v[s["P_AS"]], v[s["P_AR"]]]
        fx = [v[s["P_DRIVE"]], v[s["P_CHO_MIX"]], v[s["P_DLY_MIX"]], v[s["P_REV_MIX"]]]
        nm = name if name not in seen else "OV " + name if len(name) <= 9 else name + " 2"   # (12 at most)
        seen.add(nm)
        rows.append('    {"%s", {%s, %d, %d, %s, %d, %d, %d, %d}, {%d, %d, %d, %d}, %d, %d, FX(%d, %d, %d, %d),\n'
                    '     MORE(%s), TAG(%s)},' % (nm, a[0], a[1], a[2], b, bs, semi, det, mix, *env, fenv, mono,
                                                  *fx, ", ".join(more), tag(name)))
    hdr = ("/* SPDX-License-Identifier: GPL-3.0-only */\n"
           "/* (custom) SYNTH's presets after OberVave's factory programs (zvenson, "
           "<https://github.com/zvenson/obervave>,\n * GPL-3.0-only: obv_patches.h), %d of them. Generated by "
           "tools/obv_presets.py: not edited by hand. Included\n * at the end of SYNTH_PRESETS (eng_synth.c) */\n" % len(rows))
    open(out, "w").write(hdr + "\n".join(rows) + "\n")
    print("obv_presets: %d programs -> %s" % (len(rows), out))


if __name__ == "__main__":
    main()
