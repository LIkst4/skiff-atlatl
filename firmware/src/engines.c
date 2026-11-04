/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Engine table (order = PRESETS browsing order and the engine numbers of the editor protocol), the
 * the parts' sounds at power-on. */
/* Skiff: the parts' shared engine memory (eng_mem, after the engines): each engine's per-part state lives in its
 * part's area, never in arrays of its own for all the parts */
static void *eng_part(const track_t *t);
#include "dsp.c"
#include "eng_phase.c"
#include "eng_lofi.c"
#include "eng_sample.c"
#include "eng_formant.c"
#include "eng_grain.c"
#include "eng_phys.c"           /* PHYS: DaisySP physical models (phys_dsp.c, MIT) */
#include "eng_drum.c"           /* DRUM: the 8-lane kit (drum_voice.c) */
#include "eng_drumx.c"          /* (Skiff) DRUM-X: JIANT FM's drum engine (drumx_voice.c, GPL-3.0-only) */
#include "flt.c"                /* (custom) the filter library: the track FILTER (fx.c), SYNTH's per voice */
#include "eng_synth.c"          /* (custom) SYNTH: two oscillators (VA WAVE ADD NOISE) into a filter per voice */
#include "eng_fmini.c"          /* (custom) FMINI: FMini-1's Minimoog voice (Guilherme Ruppert, GPL-3.0-only) */
#include "retire.c"             /* (custom) ANALOG TRIO WHEEL NOISE retired: their sounds load as SYNTH */
#include "eng_fm6.c"            /* FM6: 6-operator FM, msfa ported (fm6_core.c, Apache-2.0) */
#include "fm4_convert.c"        /* DIGITAL's tables, and its sounds -> FM6 */
#if FELUCCA_FM4
#include "eng_digital.c"        /* DIGITAL: four-operator FM (retired; FELUCCA_FM4=1 builds it) */
#endif
#if FELUCCA_SLICE
#include "eng_slice.c"
#endif

/* Skiff: a track runs one engine at a time, and an engine switch fades the old one out before the new one starts
 * (voice.c engine_block), so a part's engines share one area, as big as the biggest's (PHYS: its 3 slots): ~48 KB for
 * the four parts instead of ~99 KB. Only the audio interrupt touches it, through the engines' own accessors (their
 * notes, blocks and renders); the switch zeroes it, the state each engine starts from at power-on (GRAIN rebuilds
 * its index, PHYS its model, from zero). Data the main loop writes (FM6's patches, WHEEL's bars) stays out. */
typedef union {
    phys_slot_t phys[PHYS_POLY];
    gr_part_t grain;
    drum_lane_t drum[DV_NLANE];
    fm6_note_t fm6[FM6_POLY];
    synth_vc_t synth[NVOICE];   /* (custom) SYNTH's filters */
#if FELUCCA_SLICE
    int16_t slice[NVOICE][SLC_RB];   /* SLICE's reverse windows */
#endif
} eng_mem_t;
static eng_mem_t eng_mem[NHEAVY] __attribute__((section(".pool")));
/* (Skiff phase 6) a light track's area (the tracks past NHEAVY): the engines it may play, no PHYS, no GRAIN (their
 * accessors give it none: a heavy engine forced there is silent); DRUM's lanes the biggest, 1.8 KB */
typedef union {
    drum_lane_t drum[DV_NLANE];
    fm6_note_t fm6[FM6_POLY];
    synth_vc_t synth[NVOICE];   /* (custom) */
#if FELUCCA_SLICE
    int16_t slice[NVOICE][SLC_RB];
#endif
} eng_lite_t;
static eng_lite_t eng_lite[NTRK - NHEAVY] __attribute__((section(".pool")));
static void *eng_part(const track_t *t)
{
    uint32_t k = (uint32_t)(t - trk) % NPART;
    if (k >= NHEAVY)
        return &eng_lite[k - NHEAVY];
    return &eng_mem[k];
}
static uint32_t eng_size(const track_t *t)            /* its area's bytes (the engine switch zeroes them) */
{
    if ((uint32_t)(t - trk) % NPART >= NHEAVY)
        return sizeof(eng_lite_t);
    return sizeof(eng_mem_t);
}

/* the editor protocol, user presets and projects store these indices: append, never reorder */
static const engine_t *const ENGINES[NENGINES] = {
    &ENG_RETIRED,                /* 0: ANALOG, (custom) retired (retire.c) */
#if FELUCCA_FM4
    &ENG_DIGITAL,                /* 1 (ENGI_DIGITAL) */
#else
    &ENG_FM4_GONE,               /* 1: reserved (DIGITAL, retired: its sounds convert to FM6, fm4_convert.c) */
#endif
    &ENG_PHASE,                  /* 2 */
    &ENG_LOFI,                   /* 3 */
#if FELUCCA_SAMPLER
    &ENG_SAMPLE,                 /* 4 */
#else
    &ENG_RETIRED,                /* 4: SAMPLE, (custom) without the sampler: SYNTH's synthesized sounds (retire.c) */
#endif
    &ENG_FORMANT,                /* 5 VOICE (eng_formant.c: "voice" is a sounding note in voice.c) */
    &ENG_RETIRED,                /* 6: TRIO, (custom) retired */
    &ENG_RETIRED,                /* 7: WHEEL, (custom) retired */
    &ENG_GRAIN,                  /* 8 */
    &ENG_PHYS,                   /* 9 (ENGI_PHYS) */
    &ENG_DRUM,                   /* 10 (ENGI_DRUM) */
    &ENG_RETIRED,                /* 11: NOISE, (custom) retired */
    &ENG_FM6,                    /* 12 (ENGI_FM6) */
#if FELUCCA_SLICE
    &ENG_SLICE,                  /* 13 (ENGI_SLICE) */
#else
    &ENG_RETIRED,                /* 13: reserved (FELUCCA_SLICE=0 builds without SLICE: never offered, eng_ok; (custom) its
                                  * sounds load as DRUM's kit, retire.c) */
#endif
    &ENG_RETIRED,                /* 14: DRUM-X (Skiff), (custom) retired: DRUM's DX kits */
    &ENG_SYNTH,                  /* 15 (ENGI_SYNTH, custom) */
    &ENG_FMINI,                  /* 16 (ENGI_FMINI, custom) */
};

/* a track's engine number as an index (the audio paths: a compare, cheaper than % NENGINES; a bad number: 0) */
static inline uint32_t eng_idx(uint32_t e) { return e < NENGINES ? e : 0u; }

/* the sample source a track's sound plays that has no data (SAMPLE's SET, GRAIN's SRC, SLICE's SRC: an empty or
 * invalid user slot, a set or PIANO this build lacks): its name (it plays a sine: eng_sample.c smp_sine), 0 = none;
 * *code a small number for it (1.., per engine and source) so the UI says it once (ui_input.c sample_notice) */
static uint8_t snd_said[NTRK];   /* per track: the code last said, 0 = none (a project load clears them: said again) */
static const char *snd_missing(const track_t *t, uint32_t *code)
{
    const engine_t *e = ENGINES[eng_idx(t->eng_req)];
    uint32_t si = (uint32_t)t->p[P_E0] % SMP_NALL;
#if FELUCCA_SLICE
    if (e == &ENG_SLICE) {
        si = slc_src_of(t->p);
        *code = 64u + si;
        return slc_get(si) ? 0 : N_SLC_SRC[si];
    }
#endif
    if (e != &ENG_SAMPLE && e != &ENG_GRAIN)
        return 0;
    *code = (e == &ENG_GRAIN ? 32u : 1u) + si % 32u;
    return smp_set_missing(si) ? SMP_ALL_NAMES[si] : 0;
}

/* the order the engines are shown in (PRESETS browsing and its ENG knob, the EDIT layer's keys, the editor's list):
 * engine indices, never DIGITAL's reserved 1 (with FELUCCA_FM4 it follows FM6). The indices stay as they are (the
 * stores and the protocol hold them); only this table orders them */
static const uint8_t ENGINE_ORDER[NENG_SHOWN] = {
    15,                          /* (custom) SYNTH (ANALOG's place: it makes ANALOG TRIO WHEEL NOISE's sounds) */
    16,                          /* (custom) FMINI */
    12,                          /* FM6 */
#if FELUCCA_FM4
    1,                           /* DIGITAL */
#endif
    2, 3,                        /* PHASE LOFI */
#if FELUCCA_SAMPLER
    4,                           /* SAMPLE ((custom) with the sampler only) */
#endif
    5, 8, 9,                     /* VOICE GRAIN PHYS ((custom) TRIO WHEEL NOISE retired) */
#if FELUCCA_SLICE
    13,                          /* SLICE */
#endif
    10,                          /* DRUM ((custom) DRUM-X's voice its DX kits) */
};

/* the engines one can pick (engine 1 only with FELUCCA_FM4), in ENGINE_ORDER: eng_ok(e), the n-th of them
 * eng_vis(n), e's place among them eng_rank(e), the next / previous one eng_step(e, dir) (wraps) */
static int eng_ok(uint32_t e) { return e < NENGINES && (FELUCCA_FM4 || e != ENGI_DIGITAL) && (FELUCCA_SLICE || e != ENGI_SLICE) &&
                                        !eng_retired(e); }
static uint32_t eng_vis(uint32_t n) { return ENGINE_ORDER[n % NENG_SHOWN]; }
static uint32_t eng_rank(uint32_t e)
{
    uint32_t n;
    if (!eng_ok(e))
        e = eng_retired(e) ? ENGI_SYNTH : ENGI_FM6;   /* (DIGITAL without FELUCCA_FM4: its sounds play as FM6; (custom) a
                                                 * retired one's as SYNTH) */
    for (n = 0; n < NENG_SHOWN && ENGINE_ORDER[n] != e; n++)
        ;
    return n < NENG_SHOWN ? n : 0u;
}
static uint32_t eng_step(uint32_t e, int32_t dir)
{
    return eng_vis((eng_rank(e % NENGINES) + (dir > 0 ? 1u : NENG_SHOWN - 1u)) % NENG_SHOWN);
}
/* (Skiff phase 6) engine e on track k: every one on the first NHEAVY, the light ones after (no PHYS, no GRAIN: core.h
 * NHEAVY). The one rule: picking, presets, user presets, the editor, projects ask it */
static int eng_allowed(uint32_t k, uint32_t e)
{
    return k < NHEAVY || (e % NENGINES != ENGI_PHYS && e % NENGINES != ENGI_GRAIN);
}
static uint32_t eng_step_for(uint32_t k, uint32_t e, int32_t dir)   /* eng_step past what track k may not play */
{
    uint32_t n = 0;
    do
        e = eng_step(e, dir);
    while (!eng_allowed(k, e) && ++n < NENG_SHOWN);
    return e;
}

/* the parts at power-on (engine, preset; the sequencers empty): bass, pad, lead, drums, (phase 6) a sample sound
 * and a second kit: ((custom) SYNTH ACID LINE, was ANALOG ACID), FM6 PAD (was DIGITAL PAD), LOFI PULSE LD, DRUM KIT, PIANO, DRUM KIT */
/* ((custom) without the factory samples (SMP_FACTORY 0) track 5 plays SYNTH's SY PIANO, the PIANO synthesized) */
static const uint8_t TRK_DEF[NPART][2] = {{ENGI_SYNTH, 4}, {ENGI_FM6, 4}, {3, 0}, {ENGI_DRUM, 0},
                                          {SMP_FACTORY && FELUCCA_SAMPLER ? ENGI_SAMPLE : ENGI_SYNTH, SMP_FACTORY && FELUCCA_SAMPLER ? 0 : 14}, {ENGI_DRUM, 0}};
static uint32_t trk_def_engine(uint32_t i) { return TRK_DEF[i % NPART][0]; }
