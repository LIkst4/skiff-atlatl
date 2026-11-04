/* SPDX-License-Identifier: GPL-3.0-only */
/* SYNTH (custom): an editable synth voice, two oscillators of the models below into a filter per voice.
 *
 *   OSC A, OSC B   the model of each (B also OFF):
 *     VA     a band-limited saw that turns into a square (SHAPE 0..63) and then narrows (64..127: 50 % .. 10 %);
 *            MOD (shown SUB) a square an octave below
 *     WAVE   a wavetable of 8 waves (SINE TRI SAW SQUARE PULSE ORGAN VOX DIGI, tools/gen_synth_tables.py), SHAPE scans
 *            them (each to the next, interpolated); MOD (shown WARP) bends the phase (a phase-distortion knee); two
 *            band limits (32 and 8 harmonics, 128 points each) picked by the pitch
 *     ADD    additive: 16 partials, SHAPE their brightness (the slope k^-3 .. k^-0.3), MOD (shown ODD) takes the even
 *            ones out; partials above 18 kHz are left out
 *     NOISE  white noise darkened by SHAPE (a one-pole); MOD (shown PITCH) holds it at the note's rate (a pitched hiss)
 *   SHAPE A, SHAPE B, MOD (A's model names it; B's model reads it too), B TUNE (semitones), DETUNE (B, 0..50 cents),
 *   MIX (A .. B; B OFF: A alone).
 * The filter is the track's FILTER (FX > FILTER, FILTER 2: TYPE CUTOFF RESO CHAR DRIVE, flt.c), run here per voice so
 * each note has its own; fx.c leaves the track's off for this engine. ENV DEST / LFO DEST FLT and the matrix's CUT move
 * it (vmod_t cutoff), and it follows the keyboard at half. A COMB passes the voice through (its line is the track's).
 * SHAPE routings (ENV / LFO DEST SHP, the matrix's SHP) move SHAPE A.
 * State: the filter and the second / sub phases per voice in the part's engine area (synth_vc_t); voice_t's phases
 * ph[0] A, ph[1] B, ph[2] the sub; s[0] the noise, s[1] its colour, s[2] its hold. */
#include "synth_tables.h"

enum { SY_VA, SY_WAVE, SY_ADD, SY_NOISE, SY_N };
static const char *const N_SY_OSC[] = {"VA", "WAVE", "ADD", "NOISE"};
static const char *const N_SY_OSCB[] = {"OFF", "VA", "WAVE", "ADD", "NOISE"};

typedef struct {
    flt_t f;                     /* the voice's filter (flt.c) */
    int32_t nb, nlb, hb;         /* OSC B's noise: state, colour, hold */
} synth_vc_t;

static synth_vc_t *synth_vc(track_t *t, voice_t *v)
{
    synth_vc_t *S = t >= &trk[0] && t < &trk[NPART] ? (synth_vc_t *)eng_part(t) : 0;
    uint32_t i = (uint32_t)(v - t->v);
    return S && i < NVOICE ? &S[i] : 0;
}

static void synth_note_on(track_t *t, voice_t *v)
{
    synth_vc_t *S = synth_vc(t, v);
    if (!v->s[0])
        v->s[0] = 0x2468ACE + (int32_t)v->age;           /* noise states */
    v->ph[1] = v->ph[0] + 0x2C000000u;                   /* B a little apart from A */
    if (S && !S->nb)
        S->nb = 0x13579BD + (int32_t)v->age;
}

/* the wavetable at phase ph: wave position pos (0 .. (SYN_WN - 1) << 8), band limit lim */
static inline int32_t sy_wt(uint32_t ph, int32_t pos, uint32_t lim)
{
    uint32_t w = (uint32_t)pos >> 8, i = ph >> 25, fr = (ph >> 10) & 0x7FFFu;   /* (128 points) */
    const int16_t *a = SYN_WT[w][lim], *b = SYN_WT[w + 1u < SYN_WN ? w + 1u : w][lim];
    int32_t x = a[i] + (((a[(i + 1u) & 127u] - a[i]) * (int32_t)fr) >> 15);
    int32_t y = b[i] + (((b[(i + 1u) & 127u] - b[i]) * (int32_t)fr) >> 15);
    return x + (((y - x) * (pos & 255)) >> 8);
}
/* WARP: a knee in the phase (0: none); the knee at k (Q16 of the cycle), the first part squeezed */
static inline uint32_t sy_warp(uint32_t ph, uint32_t k)
{
    uint32_t p = ph >> 16;
    if (!k)
        return ph;
    return p < k ? (p * 32768u / k) << 16 : (32768u + (p - k) * 32768u / (65536u - k)) << 16;   /* (p, k < 2^16: 32 bits) */
}

/* one oscillator of model md into y[0..n) (added at gain g, Q15): its phase *ph at inc; shape 0..127 << 8 */
static __attribute__((noinline)) void sy_osc(uint32_t md, uint32_t *ph, uint32_t inc, int32_t shape, int32_t mod,
                                             int32_t *y, uint32_t n, int32_t g, uint32_t *sub, int32_t *nz,
                                             int32_t *nl, int32_t *hold)
{
    uint32_t p = *ph, i;
    int32_t sh = clamp(shape, 0, 127 << 8);
    switch (md) {
    case SY_VA: {
        int32_t sq = sh < (64 << 8) ? sh * 4 : 32767;        /* saw -> square, Q15 */
        uint32_t pw = sh < (64 << 8) ? 0x80000000u : 0x80000000u - (uint32_t)(sh - (64 << 8)) * 0x3333u;   /* 50 .. 10 % */
        int32_t sl = mod * 200;                              /* SUB, Q15 */
        uint32_t s2 = *sub, i2 = inc >> 1;
        for (i = 0; i < n; i++) {
            int32_t x;
            if (!sq)                                         /* a saw: one polyBLEP, not three */
                x = osc_saw(p, inc);
            else if (sq >= 32767)                            /* a pulse alone */
                x = osc_pulse(p, inc, pw) << 1;
            else {
                int32_t a = osc_saw(p, inc);
                x = a + ((((osc_pulse(p, inc, pw) << 1) - a) * sq) >> 15);
            }
            if (sl)
                x += mulq15(osc_pulse(s2, i2, 0x80000000u) << 1, sl) >> 1;
            y[i] += mulq15(x, g);
            p += inc;
            s2 += i2;
        }
        *sub = s2;
        break;
    }
    case SY_WAVE: {
        int32_t pos = sh * (SYN_WN - 1) / 127;               /* 0 .. (SYN_WN - 1) << 8 */
        uint32_t lim = inc < 0x03B00000u ? 0u : 1u;            /* 32 harmonics below ~640 Hz, 8 above (flash: two band
                                                              * limits of 128 points) */
        uint32_t k = mod ? 32768u - (uint32_t)mod * 240u : 0u;
        for (i = 0; i < n; i++, p += inc)
            y[i] += mulq15(sy_wt(sy_warp(p, k), pos, lim), g);
        break;
    }
    case SY_ADD: {
        int32_t b = sh >> 6, f = sh & 63, amp[16], odd = mod * 258;   /* brightness 0..127 -> 0..32 steps */
        uint32_t kmax = 0, k;
        for (k = 0; k < 16u; k++) {
            int32_t a0 = SYN_ADD[b > 32 ? 32 : b][k], a1 = SYN_ADD[b + 1 > 32 ? 32 : b + 1][k];
            amp[k] = a0 + (((a1 - a0) * f) >> 6);
            if (k & 1u)                                      /* (k + 1 even) */
                amp[k] = mulq15(amp[k], 32767 - odd);
            if ((uint64_t)inc * (k + 1u) < 0x68000000ull)    /* below ~18 kHz */
                kmax = k + 1u;
        }
        while (kmax > 4u && amp[kmax - 1u] < 40)          /* (the partials too quiet to hear: out) */
            kmax--;
        for (i = 0; i < n; i++, p += inc) {
            int32_t x = mulq15(sine_i(p), amp[0]);           /* the first three interpolated, the rest a plain */
            if (kmax > 1u)                                   /* lookup (quieter: its -55 dB a partial is far down) */
                x += mulq15(sine_i(p * 2u), amp[1]);
            if (kmax > 2u)
                x += mulq15(sine_i(p * 3u), amp[2]);
            for (k = 3; k < kmax; k++)
                x += mulq15(SINE[(p * (k + 1u)) >> 22], amp[k]);
            y[i] += mulq15(x, g);
        }
        break;
    }
    default: {                                               /* NOISE */
        int32_t c = 32767 - (sh >> 1) * 2, h = *hold, l = *nl;   /* the colour: a one-pole */
        for (i = 0; i < n; i++) {
            uint32_t q = p;
            p += inc;
            if (!mod || p < q)                               /* PITCH: a new value once a cycle */
                h = (int32_t)(noise32(nz) >> 17) - 16384;
            {
                int32_t w = mod ? h + mulq15((int32_t)(noise32(nz) >> 17) - 16384, 32767 - mod * 258) : h;
                l += mulq15(w - l, c);
            }
            y[i] += mulq15(l << 1, g);
        }
        *hold = h;
        *nl = l;
        break;
    }
    }
    *ph = p;
}

static void synth_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    synth_vc_t *S = synth_vc(t, v);
    int32_t y[CTL], mix = p[P_E7] * 258, ga, gb;
    uint32_t i, ma = (uint32_t)clamp(p[P_E0], 0, SY_N - 1), mb = (uint32_t)clamp(p[P_E3], 0, SY_N);
    int32_t sha = (p[P_E1] << 8) + m->shape - (64 << 8);    /* the SHP routings around SHAPE A */
    uint32_t incb, sub = v->ph[2];
    if (!S || n > CTL)
        return;
    for (i = 0; i < n; i++)
        y[i] = 0;
    ga = mb ? 32767 - mix : 32767;
    gb = mix;
    sy_osc(ma, &v->ph[0], m->inc, sha, p[P_E2], y, n, ga, &sub, &v->s[0], &v->s[1], &v->s[2]);
    if (mb && gb) {
        incb = cents_inc(clamp(m->pitch16 + p[P_E5] * 16, 0, 2047), p[P_E6] * 50 / 127, m->fine);
        sy_osc(mb - 1u, &v->ph[1], incb, p[P_E4] << 8, p[P_E2], y, n, gb, &sub, &S->nb, &S->nlb, &S->hb);
    } else if (mb) {
        v->ph[1] += cents_inc(clamp(m->pitch16 + p[P_E5] * 16, 0, 2047), 0, 0) * n;   /* (B runs on, silent) */
    }
    v->ph[2] = sub;
    {
        int32_t cm = (m->cutoff >> 5) + (((v->pitch16 - 60 * 16) * 9) >> 5);   /* the routings; half key tracking */
        flt_run(&S->f, 0, y, n, (uint32_t)clamp(p[P_FTYPE], 0, FT_N - 1), p[P_FCUT], p[P_FRES], p[P_FCHR], p[P_FDRV], cm);
    }
    for (i = 0; i < n; i++)
        out[i] += voice_amp(soft_knee(y[i] >> 1, 16000), m, i) << 1;
}

static const param_desc_t SY_MOD_DESC[SY_N] = {
    {"SUB", 0, 0, 127, 0, F_PCT, 0}, {"WARP", 0, 0, 127, 0, F_PCT, 0}, {"ODD", 0, 0, 127, 0, F_PCT, 0},
    {"PITCH", 0, 0, 127, 0, F_PCT, 0}};
static const param_desc_t *synth_desc(const track_t *t, uint32_t k)
{
    if (k == 2u)
        return &SY_MOD_DESC[clamp(t->p[P_E0], 0, SY_N - 1)];
    return 0;
}

/* {OSC A, SHAPE A, MOD, OSC B, SHAPE B, B TUNE, DETUNE, MIX}; the filter's TYPE CUTOFF RESO and the ENV -> FILTER in MORE */
#define SYF(ty, cut, res) {P_FTYPE, ty}, {P_FCUT, cut}, {P_FRES, res}
static const preset_t SYNTH_PRESETS[] = {
    {"INIT SYNTH", {SY_VA, 0, 0, 0, 0, 0, 0, 64}, {2, 80, 100, 40}, 20, 0, FX(0, 0, 0, 20),
     MORE(SYF(FT_LPMG, 90, 20), {P_FCHR, 0}), TAG(PT_LEAD)},
    {"OB BRASS", {SY_VA, 10, 0, 1 + SY_VA, 12, 0, 18, 64}, {30, 80, 100, 50}, 30, 0, FX(0, 30, 10, 40),
     MORE(SYF(FT_OB12, 60, 40), {P_FCHR, 0}), TAG(PT_LEAD)},
    {"OB STRINGS", {SY_VA, 70, 0, 1 + SY_VA, 90, 0, 30, 64}, {70, 90, 115, 80}, 8, 0, FX(0, 80, 15, 60),
     MORE(SYF(FT_OB12, 72, 25), {P_LD_SHP, 30}), TAG(PT_PAD)},
    {"LADDER BASS", {SY_VA, 0, 60, 1 + SY_VA, 0, -12, 10, 50}, {0, 60, 50, 30}, 40, 1, FX(0, 0, 5, 10),
     MORE(SYF(FT_LPMG, 40, 70), {P_FDRV, 40}), TAG(PT_BASS)},
    {"ACID LINE", {SY_VA, 0, 0, 0, 0, 0, 0, 64}, {0, 55, 20, 30}, 50, 1, FX(20, 0, 45, 15),
     MORE(SYF(FT_ACID, 40, 105), {P_FCHR, 40}), TAG(PT_BASS)},
    {"WAVE SCAN", {SY_WAVE, 20, 0, 1 + SY_WAVE, 60, 12, 20, 50}, {40, 90, 110, 70}, 10, 0, FX(0, 40, 30, 60),
     MORE(SYF(FT_SVF, 90, 30), {P_LD_SHP, 50}), TAG(PT_PAD)},
    {"VOX PAD", {SY_WAVE, 108, 0, 1 + SY_WAVE, 100, 0, 25, 64}, {80, 90, 118, 90}, 0, 0, FX(0, 70, 20, 80),
     MORE(SYF(FT_VOWEL, 64, 70), {P_FCHR, 30}), TAG(PT_PAD)},
    {"DIGI BELL", {SY_WAVE, 127, 40, 1 + SY_ADD, 90, 19, 0, 40}, {0, 85, 0, 80}, 0, 0, FX(0, 20, 40, 60),
     MORE(SYF(FT_SVF, 110, 10)), TAG(PT_KEYS)},
    {"ORGAN WT", {SY_WAVE, 90, 0, 0, 0, 0, 0, 0}, {0, 100, 127, 20}, 0, 0, FX(0, 40, 0, 40),
     MORE(SYF(FT_LD, 110, 0), {P_FCHR, 127}), TAG(PT_KEYS)},
    {"ADD GLASS", {SY_ADD, 90, 0, 1 + SY_ADD, 110, 12, 8, 40}, {5, 90, 60, 80}, 0, 0, FX(0, 30, 30, 70),
     MORE(SYF(FT_SVF, 120, 0)), TAG(PT_KEYS)},
    {"ADD CLARINET", {SY_ADD, 70, 127, 0, 0, 0, 0, 0}, {20, 80, 110, 40}, 0, 1, FX(0, 10, 10, 40),
     MORE(SYF(FT_LPMG, 100, 10), {P_LD_PIT, 6}), TAG(PT_LEAD)},
    {"SK SCREAM", {SY_VA, 30, 0, 1 + SY_VA, 30, 7, 30, 64}, {0, 70, 100, 40}, 40, 1, FX(10, 0, 30, 20),
     MORE(SYF(FT_SK, 70, 110), {P_FDRV, 50}), TAG(PT_LEAD)},
    {"OB24 POLY", {SY_VA, 0, 0, 1 + SY_VA, 40, 0, 22, 64}, {5, 80, 90, 45}, 30, 0, FX(0, 30, 25, 40),
     MORE(SYF(FT_OB24, 70, 50), {P_FCHR, 60}), TAG(PT_KEYS)},
    {"COMB WIND", {SY_NOISE, 40, 0, 1 + SY_NOISE, 100, 0, 0, 64}, {90, 90, 110, 100}, 30, 0, FX(0, 30, 30, 90),
     MORE(SYF(FT_VOWEL, 50, 100), {P_FCHR, 64}), TAG(PT_FX)},
    /* the factory samples' sounds, synthesized (the firmware holds no sample data by default: FELUCCA_BUILTIN_SAMPLES) */
    {"SY PIANO", {SY_ADD, 84, 0, 1 + SY_ADD, 70, 12, 6, 30}, {0, 84, 0, 70}, 34, 0, FX(0, 10, 0, 30),
     MORE(SYF(FT_LPMG, 72, 8), {P_FDRV, 10}), TAG(PT_KEYS)},
    {"SY LOFI PNO", {SY_ADD, 70, 0, 1 + SY_ADD, 60, 12, 14, 30}, {0, 80, 0, 64}, 28, 0, FX(30, 20, 10, 40),
     MORE(SYF(FT_LD, 64, 10), {P_LD_PIT, 8}), TAG(PT_KEYS)},
    {"SY FLUTE", {SY_ADD, 28, 110, 1 + SY_NOISE, 100, 0, 0, 14}, {14, 80, 118, 50}, 6, 1, FX(0, 10, 15, 45),
     MORE(SYF(FT_SVF, 104, 12), {P_LD_PIT, 6}), TAG(PT_LEAD)},
    {"SY FLUTE PAD", {SY_ADD, 34, 100, 1 + SY_NOISE, 90, 0, 0, 10}, {80, 90, 120, 100}, 0, 0, FX(0, 60, 25, 90),
     MORE(SYF(FT_SVF, 96, 10), {P_LD_PIT, 4}), TAG(PT_PAD)},
    {"SY SAX", {SY_VA, 58, 0, 1 + SY_VA, 40, 0, 9, 40}, {10, 80, 116, 40}, 30, 1, FX(10, 10, 15, 40),
     MORE(SYF(FT_VOWEL, 66, 58), {P_FCHR, 22}), TAG(PT_LEAD)},
    {"SY DARK SAX", {SY_VA, 50, 0, 1 + SY_VA, 30, -12, 9, 30}, {22, 80, 120, 55}, 24, 1, FX(0, 15, 20, 60),
     MORE(SYF(FT_VOWEL, 52, 50), {P_FCHR, 60}), TAG(PT_LEAD)},
    {"SY MUSICBOX", {SY_WAVE, 127, 30, 1 + SY_ADD, 110, 24, 0, 40}, {0, 90, 0, 80}, 0, 0, FX(0, 30, 40, 60),
     MORE(SYF(FT_SVF, 118, 0)), TAG(PT_PLUCK)},
    {"SY KOTO", {SY_VA, 20, 0, 1 + SY_WAVE, 110, 12, 4, 34}, {0, 72, 0, 50}, 50, 0, FX(0, 0, 25, 30),
     MORE(SYF(FT_SK, 70, 60), {P_ED_PIT, 2}), TAG(PT_PLUCK)},
};
#undef SYF

static const engine_t ENG_SYNTH = {
    .name = "SYNTH",
    .page_title = {"OSC A", "OSC B"},
    .edit = {
        {"OSC A", N_SY_OSC, 0, SY_N - 1, SY_VA, F_ENUM, 0},
        {"SHAPE A", 0, 0, 127, 0, F_PCT, 0},
        {"MOD", 0, 0, 127, 0, F_PCT, 0},
        {"OSC B", N_SY_OSCB, 0, SY_N, 0, F_ENUM, 0},
        {"SHAPE B", 0, 0, 127, 0, F_PCT, 0},
        {"B TUNE", 0, -24, 24, 0, F_INT, U_ST},
        {"DETUNE", 0, 0, 127, 0, F_INT, U_CT},
        {"MIX", 0, 0, 127, 64, F_PCT, 0},
    },
    .presets = SYNTH_PRESETS,
    .npresets = NELEM(SYNTH_PRESETS),
    .note_on = synth_note_on,
    .render = synth_render,
    .desc = synth_desc,
    .knob = {P_E1, P_FCUT, P_FRES, P_E7},
};
