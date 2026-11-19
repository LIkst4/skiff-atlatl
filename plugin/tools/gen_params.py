#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The plugin's automatable parameters, from the firmware's own descriptors.

  plugin/tools/gen_params.py DESC_JSON SIDS_TXT OUT_H

DESC_JSON is build/host/desc.json (tests/descdump.c), SIDS_TXT the output of plugin/tools/plugin_sids.c (each track
parameter's stored id). OUT_H (plugin/src/gen/params.h) lists every track parameter the device stores, with:

- its plugin id: the stored id (SID), fixed for good, so a DAW project's automation keeps its target across firmware
  versions; DRUM's lane models share their SIDs with SYNTH's own parameters (core.h sid_lanes), so theirs are 256 + SID;
- its name: the device's label, prefixed by its page where another parameter has the same label ("LFO RATE",
  "KICK TUNE"); E1..E8 are the engine's (their meaning changes with the engine);
- its group (CLAP module): the device's page family and page;
- its range, default and enum names.

The inert ones (the retired DIGITAL engine's operator envelopes, ids 61..80) are left out. tests/run_local.sh checks
the header is up to date (rerun this after a parameter is added)."""
import collections
import json
import sys

FAM = {1: "ENV", 2: "LFO", 3: "FX", 4: "SCL", 5: "EDIT", 8: "ARP", 9: "SEQ"}
LANES = ["KICK", "SNARE", "CLAP", "CL HAT", "OP HAT", "TOM", "RIM", "BELL"]
INERT = set(range(61, 81)) | {8}                       # (8: P_ED_FX, unused)
# the envelope's and the LFO's depths (on no page of their own: the device's EDIT pages carry them)
OWN = {5: ("ENV", "ENV", "ENV CUTOFF"), 6: ("ENV", "ENV", "ENV PITCH"), 7: ("ENV", "ENV", "ENV SHAPE"),
       13: ("LFO", "LFO", "LFO PITCH"), 14: ("LFO", "LFO", "LFO CUTOFF"), 15: ("LFO", "LFO", "LFO SHAPE"),
       16: ("LFO", "LFO", "LFO AMP"), 91: ("SEQ", "PATTERN", "PATTERN DIR"), 125: ("FX", "INSERT", "INSERT MIX")}
# the MOD matrix's four slots (the device's MOD page shows one slot at a time)
OWN.update({49 + 3 * k + j: ("LFO", "MOD", ("SRC", "DEST", "AMT")[j] + str(k + 1)) for k in range(4) for j in range(3)})
# pages whose first slot only is the track's (the others the shared bus's, globals)
BUS_PAGES = {"DELAY", "REVERB"}


def c_str(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def main(desc_path, sids_path, out_path):
    d = json.load(open(desc_path))
    tp, pe0, pcount = d["TP"], d["P_E0"], d["P_COUNT"]
    sid, lanes = {}, set()
    for line in open(sids_path):
        f = line.split()
        if f[0].isdigit():
            sid[int(f[0])] = int(f[1])
            if f[2] == "1":
                lanes.add(int(f[0]))
    group = {}
    titles = collections.Counter(t for t, fam, ids in d["PAGES"])
    for title, fam, ids in d["PAGES"]:
        name = title if titles[title] == 1 else FAM.get(fam, "") + " " + title
        for i in ids[:1] if title in BUS_PAGES else ids:
            if i >= 0 and i not in group:
                group[i] = (FAM.get(fam, "TRACK"), name)
    # the DRUM lanes' own: levels (the lane's name), TUNE, DECAY, PAN, MODEL, eight each
    lane_of = {}
    for i, p in enumerate(tp):
        if p["label"] in LANES:
            lane_of[i] = (LANES.index(p["label"]), True)
    for label in ("TUNE", "DECAY", "PAN", "MODEL"):
        run = [i for i, p in enumerate(tp) if p["label"] == label]
        if len(run) >= 8:
            k = next(j for j in range(len(run) - 7) if run[j + 7] == run[j] + 7)
            for n in range(8):
                lane_of[run[k + n]] = (n, False)
    for i in lane_of:
        group[i] = ("DRUM", "LANES")
    labels = collections.Counter(p["label"] for i, p in enumerate(tp) if i not in INERT and i not in lane_of and
                                 i not in OWN)
    rows = []
    for i, p in enumerate(tp):
        if i in INERT or i not in sid:
            continue
        fam, page = group.get(i, ("TRACK", "TRACK"))
        if i in OWN:
            fam, page, name = OWN[i]
        elif i in lane_of:
            n, level = lane_of[i]
            name = LANES[n] + ("" if level else " " + p["label"])
        elif labels[p["label"]] > 1:
            name = page + " " + p["label"]
        else:
            name = p["label"]
        pid = sid[i] + (256 if i in lanes else 0)
        rows.append((pid, i, p["min"], p["max"], p["def"], p["fmt"], name, fam + "/" + page, p["names"] or []))
    # E1..E8: the engine's; their range the widest any engine gives them
    for k in range(8):
        lo = min(e["edit"][k]["min"] for e in d["ENG"] if e["name"] != "-")
        hi = max(e["edit"][k]["max"] for e in d["ENG"] if e["name"] != "-")
        df = next(e["edit"][k]["def"] for e in d["ENG"] if e["name"] == "SYNTH")
        rows.append((sid[pe0 + k], pe0 + k, lo, hi, df, 0, "E%d" % (k + 1), "EDIT/ENGINE", []))
    names = collections.Counter(r[6] for r in rows)
    assert all(v == 1 for v in names.values()), [n for n, v in names.items() if v > 1]
    ids = collections.Counter(r[0] for r in rows)
    assert all(v == 1 for v in ids.values())
    out = ["/* SPDX-License-Identifier: GPL-3.0-only */",
           "/* generated by plugin/tools/gen_params.py from build/host/desc.json: do not edit */",
           "#pragma once", "#include <cstdint>", "",
           "namespace fm1 {", "",
           "constexpr uint32_t kFirmwarePCount = %d, kFirmwarePE0 = %d;" % (pcount, pe0),
           "constexpr int kGEngSel = %d;   /* the global that sets the selected track's engine */" % d["G_ENGSEL"], "",
           "struct ParamDef {",
           "    uint32_t id;        /* the plugin's id: the stored id (DRUM's lane models 256 + it) */",
           "    uint16_t p;         /* the firmware's parameter (TRACK_PARAM's id) */",
           "    int16_t min, max, def;",
           "    uint8_t fmt;        /* params.c F_* (8: an enum) */",
           "    const char *name, *module;",
           "    const char *const *names;  /* an enum's, max - min + 1 of them, or none */",
           "};", ""]
    for r in rows:
        if r[8]:
            out.append("static const char *const kNames%d[] = {%s};" % (r[1], ", ".join(c_str(n) for n in r[8])))
    out += ["", "static const ParamDef kParams[] = {"]
    for r in rows:
        out.append("    {%d, %d, %d, %d, %d, %d, %s, %s, %s}," % (
            r[0], r[1], r[2], r[3], r[4], r[5], c_str(r[6]), c_str(r[7]), ("kNames%d" % r[1]) if r[8] else "nullptr"))
    out += ["};", "constexpr uint32_t kParamCount = sizeof kParams / sizeof kParams[0];", "", "}  // namespace fm1", ""]
    open(out_path, "w").write("\n".join(out))


if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    main(*sys.argv[1:])
