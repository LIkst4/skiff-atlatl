/* SPDX-License-Identifier: GPL-3.0-only */
/* FLT: the filter library (fixed point, no float, no 64-bit). One instance filters a mono block in place.
 * Shared by the track FILTER (a slot after any engine) and, later, the per-voice filter of an editable synth engine.
 *
 * Every type is an original implementation of a well-known topology; no code from Oberheim, Moog, Korg or
 * Roland products is used. The names describe the style, not a product.
 *
 *   TYPE    style                                  CHAR (the type's own knob)
 *   OFF     -                                      -
 *   LP MG   4-pole transistor ladder, driven input  tone: how hard the input stage saturates
 *   LD      ladder with a variable slope           slope: 6, 12, 18 or 24 dB/oct (LP)
 *   SVF     state variable, very resonant          morph LP > BP > HP > NOTCH
 *   SK      Sallen-Key (MS-20 style), diode-clipped resonance   LP / BP / HP
 *   OB 12   2-pole state variable, SEM / OB-X style  morph LP > NOTCH > HP, the top 16 values BP
 *   OB 24   4-pole OTA cascade, OB-Xa style        saturation of each stage
 *   ACID    (custom) the 303's: after Open303's TeeBeeFilter, its feedback through a high-pass   FB HP: that
 *           high-pass, ~30 .. ~430 Hz (63: ~150 Hz, the 303's)
 *   COMB    comb with damping, bipolar             left of centre: - feedback, right: +; farther: darker
 *   VOWEL   3 formant band-passes                  morph A > E > I > O > U (CUT shifts them +-1 oct)
 *   ANLG    (custom) ANALOG's (Felucca's ANALOG engine): its 2-pole trapezoidal low-pass, DRIVE a soft clip
 *           before it as ANALOG's   BP: the low-pass .. the band-pass
 *
 * Levels: the block is the track's sum (one voice peaks near 24000). Inside, x >> 2, clamped to +-32000; the states
 * are clamped to +-65000 so every product below fits 32 bits (g <= 13208 Q12, G <= 65535 Q16, k <= 17000 Q12). */
#include "flt_tables.h"

enum { FT_OFF, FT_LPMG, FT_LD, FT_SVF, FT_SK, FT_OB12, FT_OB24, FT_ACID, FT_COMB, FT_VOWEL, FT_ANLG, FT_N };

#define FLT_LINE 512u             /* COMB's delay line (int16): down to 87 Hz */
#define FLT_SMAX 65000

typedef struct {
    int32_t s[8];                 /* integrator states (the ladders 4, an SVF 2, VOWEL 3 x 2) */
    int32_t ci;                   /* the cutoff index as heard (glides to the target a block at a time) */
    int32_t hp;                   /* a DC / low cut state (COMB's output); ACID: its b0 a block ago */
    int32_t y;                    /* the ladders' last output (the unit-delay feedback); ACID: its k a block ago */
    uint16_t wp;                  /* COMB's write position */
    uint8_t type;                 /* the type the states belong to (a change clears them) */
    uint8_t bl;                   /* (custom) BLEND (P_FBLND), set by the caller before flt_run: 0 LP .. 64 BP .. 127 HP */
} flt_t;

/* (custom) BLEND, as Vital's filters: 0 the type's own output (LP, or what CHAR makes of it) .. 64 its band-pass ..
 * 127 its high-pass; the ladders' from their stages (as the Oberheim Xpander's modes: BP 4 (y2 - 2 y3 + y4), HP
 * u - 4 y1 + 6 y2 - 4 y3 + y4), the state-variable ones' their own outputs. COMB and VOWEL keep theirs. fl_bw: the
 * block's weight (Q12) and which pair it blends */
typedef struct { int32_t w; uint32_t hi; } fl_bw_t;
static inline fl_bw_t fl_bw(uint32_t bl)
{
    fl_bw_t r;
    r.hi = bl > 64u;
    r.w = r.hi ? (int32_t)(bl - 64u) * 65 : (int32_t)bl * 64;
    return r;
}
static inline int32_t fl_mix(int32_t lp, int32_t bp, int32_t hp, fl_bw_t w)
{
    return w.hi ? bp + (int32_t)(((int64_t)(hp - bp) * w.w) >> 12) : lp + (int32_t)(((int64_t)(bp - lp) * w.w) >> 12);
}

static inline int32_t fl_sat(int32_t v) { return v < -FLT_SMAX ? -FLT_SMAX : v > FLT_SMAX ? FLT_SMAX : v; }
static inline int32_t fl_q12(int32_t a, int32_t k) { return (a * k) >> 12; }
/* the cutoff index 0..1024 -> g (Q12), linear between the table's points (the index is already integral) */
static inline int32_t fl_g(int32_t ci) { return FLT_G[clamp(ci, 0, 1024)]; }
/* a TPT one-pole's G = g / (1 + g), Q16 */
static inline uint32_t fl_G(int32_t g) { return (uint32_t)(((uint32_t)g << 16) / (uint32_t)(4096 + g)); }
/* one TPT one-pole step: returns the low-pass, updates the state */
static inline int32_t fl_op(int32_t *s, int32_t x, uint32_t G)
{
    int32_t v = (int32_t)(((int64_t)(x - *s) * (int32_t)G) >> 16), y = v + *s;   /* (one 32x32 -> 64 multiply) */
    *s = fl_sat(y + v);
    return y;
}
/* .. without the clamp: inside the ladders, whose input is already through a tanh (|u| < 16400) and whose one-poles
 * never pass their input's peak, so the states stay within it */
static inline int32_t fl_opu(int32_t *s, int32_t x, uint32_t G)
{
    int32_t v = (int32_t)(((int64_t)(x - *s) * (int32_t)G) >> 16), y = v + *s;
    *s = y + v;
    return y;
}
/* a saturating stage: tanh around +-1.0 = 16384 here (softclip's Q15 at twice the input) */
static inline int32_t fl_tanh(int32_t x) { return softclip(x << 1) >> 1; }

/* the ladders (LP MG, LD, OB 24, ACID): four one-poles, the output fed back a sample late.
 * k: the feedback (Q12, 4.0 = 16384 the edge of self-oscillation), sat: 0 the input clip only, 1 every stage */
static void fl_ladder(flt_t *f, int32_t *b, uint32_t n, int32_t g, int32_t k, int32_t drv, uint32_t taps,
                      int32_t sat, int32_t comp)
{
    uint32_t G = fl_G(g), i;
    fl_bw_t bw = fl_bw(f->bl);
    if (taps == 4u && !sat && !f->bl) {                    /* LP MG, LD at 24 dB: the plain four poles, the common case */
        int32_t s0 = f->s[0], s1 = f->s[1], s2 = f->s[2], s3 = f->s[3], yl = f->y;
        for (i = 0; i < n; i++) {
            int32_t x = clamp(b[i] >> 2, -32000, 32000), u;
            u = fl_tanh(fl_q12(x, drv) - fl_q12(yl, k));
            u = fl_opu(&s0, u, G);
            u = fl_opu(&s1, u, G);
            u = fl_opu(&s2, u, G);
            yl = fl_opu(&s3, u, G);
            b[i] = fl_sat(fl_q12(yl, comp)) << 2;
        }
        f->s[0] = s0, f->s[1] = s1, f->s[2] = s2, f->s[3] = s3, f->y = yl;
        return;
    }
    for (i = 0; i < n; i++) {
        int32_t x = clamp(b[i] >> 2, -32000, 32000), u, y1, y2, y3, y4, o;
        u = fl_tanh(fl_q12(x, drv) - fl_q12(f->y, k));
        y1 = fl_op(&f->s[0], u, G);
        if (sat)
            y1 = fl_tanh(y1);
        y2 = fl_op(&f->s[1], y1, G);
        if (sat)
            y2 = fl_tanh(y2);
        y3 = fl_op(&f->s[2], y2, G);
        y4 = fl_op(&f->s[3], y3, G);
        f->y = fl_sat(y4);
        switch (taps) {                         /* LD's slope; ACID's 3-pole share is taps 5.. */
        case 1: o = y1; break;
        case 2: o = y2; break;
        case 3: o = y3; break;
        case 4: o = y4; break;
        default: o = y4 + fl_q12(y3 - y4, (int32_t)taps - 5); break;   /* taps 5 + 0..4095: y4 .. y3 */
        }
        if (f->bl)                              /* (custom) BLEND: the stages' band- and high-pass */
            o = fl_mix(o, 4 * (y2 - 2 * y3 + y4), u - 4 * y1 + 6 * y2 - 4 * y3 + y4, bw);
        b[i] = fl_sat(fl_q12(o, comp)) << 2;
    }
}

/* the TPT state variable filter. R: damping (Q12, 2R = 1/Q), nl: OTA-style soft integrators (OB 12, SK).
 * out: 0 LP, 1 BP, 2 HP, 3 NOTCH; m (0..4095) blends out with out + 1 (the morphs) */
static void fl_svf(flt_t *f, int32_t *b, uint32_t n, int32_t g, int32_t R, int32_t drv, uint32_t mode, int32_t m,
                   int32_t nl, int32_t comp)
{
    int32_t c1 = 2 * R + g, den = 4096 + fl_q12(2 * R, g) + fl_q12(g, g);
    uint32_t a = (uint32_t)((1u << 28) / (uint32_t)den), i;   /* 1 / den, Q16 */
    fl_bw_t bw = fl_bw(f->bl);
    for (i = 0; i < n; i++) {
        int32_t x = fl_tanh(fl_q12(clamp(b[i] >> 2, -32000, 32000), drv)), hp, bp, lp, o[4], y;   /* DRIVE: soft, before */
        int32_t s1 = f->s[0];
        if (nl)
            s1 = fl_tanh(s1);                   /* the OTA's curve on the resonance path */
        hp = fl_sat(mulq16(x - fl_q12(c1, s1) - f->s[1], a));
        bp = fl_q12(g, hp) + f->s[0];
        f->s[0] = fl_sat(fl_q12(g, hp) + bp);
        lp = fl_q12(g, bp) + f->s[1];
        f->s[1] = fl_sat(fl_q12(g, bp) + lp);
        bp = fl_q12(bp, 2 * R);                 /* BP at unity gain at its peak */
        o[0] = lp;
        o[1] = bp;
        o[2] = hp;
        o[3] = lp + hp;                         /* NOTCH */
        y = mode == 5u ? lp + fl_q12(hp - lp, m) : o[mode] + fl_q12(o[mode + 1u] - o[mode], m);
        if (f->bl)                              /* (custom) BLEND */
            y = fl_mix(y, bp, hp, bw);
        b[i] = fl_sat(fl_q12(y, comp)) << 2;
    }
}

/* SK: the 2-pole with its resonance through a diode pair (hard-ish clip): screams, stays bounded */
static void fl_sk(flt_t *f, int32_t *b, uint32_t n, int32_t g, int32_t k, int32_t drv, uint32_t mode)
{
    uint32_t G = fl_G(g), i;
    fl_bw_t bw = fl_bw(f->bl);
    for (i = 0; i < n; i++) {
        int32_t x = fl_tanh(fl_q12(clamp(b[i] >> 2, -32000, 32000), drv)), fb, u, y1, y2, o;
        fb = fl_q12(f->s[2], k);                /* last band-pass, scaled */
        fb = fl_tanh(fb << 1) >> 1;             /* the diodes */
        u = x + fb;
        y1 = fl_op(&f->s[0], u, G);
        y2 = fl_op(&f->s[1], y1, G);
        f->s[2] = fl_sat(y1 - y2);              /* the band between the poles feeds back */
        o = mode == 0 ? y2 : mode == 1 ? (y1 - y2) << 1 : x - y1 - (y1 - y2);
        if (f->bl)                              /* (custom) BLEND */
            o = fl_mix(o, (y1 - y2) << 1, x - y1 - (y1 - y2), bw);
        b[i] = fl_sat(o) << 2;
    }
}

/* COMB: delay = one period of the cutoff's frequency; fb Q12 bipolar; damp Q16 (the loop's one-pole) */
static void fl_comb(flt_t *f, int16_t *line, int32_t *b, uint32_t n, int32_t ci, int32_t fb, uint32_t damp)
{
    /* the period in samples, Q8, from the cutoff index: fs / f, f = 20 * 2^(ci * 9.8 / 1024); limited 2..1020 */
    int32_t e = clamp(ci, 225, 1024);           /* (225: ~ 87 Hz, the line's end) */
    uint32_t oct = (uint32_t)(e * 98) / 10240u, fr = (uint32_t)(e * 98) % 10240u;   /* octaves above 20 Hz */
    uint32_t per = (uint32_t)((2205u << 8) >> oct);
    uint32_t i;
    per = ((per >> 4) * (10240u - fr * 6u / 10u) / 10240u) << 4;   /* ~ 2^-fr (linear, within 4 %) */
    per = per < (2u << 8) ? (2u << 8) : per > (508u << 8) ? (508u << 8) : per;
    for (i = 0; i < n; i++) {
        int32_t x = clamp(b[i] >> 2, -32000, 32000), r0, r1, d, y;
        uint32_t rp = (uint32_t)f->wp + FLT_LINE * 4u - (per >> 8);
        r0 = line[rp & (FLT_LINE - 1u)];
        r1 = line[(rp - 1u) & (FLT_LINE - 1u)];
        d = r0 + (((r1 - r0) * (int32_t)(per & 255u)) >> 8);
        f->s[0] += mulq16(d - f->s[0], damp);  /* the damping */
        y = x + fl_q12(f->s[0], fb);
        line[f->wp] = (int16_t)clamp(fl_tanh(y), -32767, 32767);
        f->wp = (uint16_t)((f->wp + 1u) & (FLT_LINE - 1u));
        f->hp += (y - f->hp) >> 9;              /* DC out */
        b[i] = fl_sat((y - f->hp) >> 1) << 2;
    }
}

/* VOWEL: three band-passes on the formants of the vowel between two of A E I O U */
static void fl_vowel(flt_t *f, int32_t *b, uint32_t n, int32_t shift, int32_t R, int32_t chr)
{
    int32_t pos = chr * 4 * 4096 / 127, v = pos >> 12, m = pos & 4095, k, j;
    int32_t g[3], c1[3], lvl[3] = {4096, 2600, 1500};
    uint32_t a[3], i;
    if (v >= 4) {
        v = 3;
        m = 4095;
    }
    for (k = 0; k < 3; k++) {
        int32_t ci = FLT_VOWEL[v][k] + fl_q12(FLT_VOWEL[v + 1][k] - FLT_VOWEL[v][k], m) + shift;
        g[k] = fl_g(ci);
        c1[k] = 2 * R + g[k];
        a[k] = (uint32_t)((1u << 28) / (uint32_t)(4096 + fl_q12(2 * R, g[k]) + fl_q12(g[k], g[k])));
    }
    for (i = 0; i < n; i++) {
        int32_t x = clamp(b[i] >> 2, -32000, 32000), y = 0;
        for (j = 0; j < 3; j++) {
            int32_t *s = &f->s[2 * j], hp, bp, lp;
            hp = fl_sat(mulq16(x - fl_q12(c1[j], s[0]) - s[1], a[j]));
            bp = fl_q12(g[j], hp) + s[0];
            s[0] = fl_sat(fl_q12(g[j], hp) + bp);
            lp = fl_q12(g[j], bp) + s[1];
            s[1] = fl_sat(fl_q12(g[j], bp) + lp);
            y += fl_q12(fl_q12(bp, 2 * R), lvl[j]);
        }
        b[i] = fl_sat(y << 1) << 2;
    }
}

/* (custom) ACID, the 303's: after Open303's TeeBeeFilter (Robin Schmidt, MIT) in its TB_303 mode, as X0X's
 * bass303.c runs it (charlesvestal/fm1-x0x, GPL-3.0-only): four one-poles that load each other (a diode ladder's
 * stages), the resonance fed back through a high-pass (so the bass stays when it rings), a 44 Hz high-pass before it
 * and the output made up with the resonance (g2); here in fixed point at 1x, the input to the ladder a soft clip (it
 * squelches rather than running away), b0 and k glided across the block; the cutoff an octave above the index (the
 * 303's nominal cutoff is where it rings: without RESO it is darker than the other types'). States s[0..3] the stages, s[4] s[5] the
 * feedback high-pass, s[6] the input's high-pass (44.5 Hz), s[7] the output's (24 Hz, as Open303's after it: what the
 * clipped resonance leaves of DC); hp, y: b0, k a block ago */
#define ACID_BP 6                        /* (custom) BLEND: ACID's band-pass made up to ~1 at its peak */
static void fl_acid(flt_t *f, int32_t *b, uint32_t n, int32_t ci, int32_t res, int32_t chr, int32_t dq)
{
    int32_t j = clamp(ci + 104, 0, 1024), i8 = j >> 3, fr = j & 7, i9 = i8 < 128 ? i8 + 1 : 128;
    int32_t b0 = FLT_TB_B0[i8] + (((FLT_TB_B0[i9] - FLT_TB_B0[i8]) * fr) >> 3);       /* Q16 */
    int32_t k1 = FLT_TB_K[i8] + (((FLT_TB_K[i9] - FLT_TB_K[i8]) * fr) >> 3);          /* Q8, at RESO 1 */
    int32_t r = FLT_TB_R[clamp(res, 0, 127)], kr = (k1 * r) >> 12;                    /* Q12; Q8 */
    int32_t g2 = (((((k1 * 16) / 17 - 4096) * r) >> 12) + 4096) * ((4096 + r) >> 1) >> 10;   /* Q12: 2 (.. ) (1 + r) */
    int32_t hz = 30 + chr * chr / 40, fa = 65536 - ((hz * 598) >> 6), fb0 = (65536 + fa) >> 1;   /* FB HP, Q16 */
    int32_t y1 = f->s[0], y2 = f->s[1], y3 = f->s[2], y4 = f->s[3], fx = f->s[4], fy = f->s[5], li = f->s[6],
            lo = f->s[7];
    int32_t bq = f->hp ? f->hp : b0, kq = f->hp ? f->y : kr, db = (b0 - bq) / (int32_t)n, dk = (kr - kq) / (int32_t)n;
    uint32_t i;
    fl_bw_t bw = fl_bw(f->bl);
    if (!n)
        return;
    for (i = 0; i < n; i++) {                    /* (the states 64 x the level: the small b0 of low cutoffs keep their
                                                  * steps; the soft clip on the level as the other types) */
        int32_t x = clamp(b[i] >> 2, -32000, 32000) << 6, t, u, o, hy;
        bq += db;
        kq += dk;
        hy = x - li;                                                    /* 44.5 Hz high-pass (Open303's) */
        li += (int32_t)(((int64_t)hy * 415) >> 16);
        t = (int32_t)(((int64_t)kq * y4) >> 8);                         /* the resonance, through its high-pass */
        fy = (int32_t)(((int64_t)(t - fx) * fb0 + (int64_t)fy * fa) >> 16);
        fx = t;
        u = clamp((int32_t)(((int64_t)hy * dq) >> 12) - fy, -60000000, 60000000);
        u = (softclip(u >> 6) << 6) + (u & 63);                        /* the squelch: soft at +-32k */
        y1 += (int32_t)(((int64_t)(bq << 1) * (u - y1 + y2)) >> 16);
        y2 += (int32_t)(((int64_t)bq * (y1 - 2 * y2 + y3)) >> 16);
        y3 += (int32_t)(((int64_t)bq * (y2 - 2 * y3 + y4)) >> 16);
        y4 = clamp(y4 + (int32_t)(((int64_t)bq * (y3 - 2 * y4)) >> 16), -25000000, 25000000);
        o = (int32_t)(((((int64_t)g2 * y4) >> 18) * (4096 + r)) >> 12);
        if (f->bl)                              /* (custom) BLEND: the coupled stages' own band- and high-pass, unmade */
            o = fl_mix(o, (ACID_BP * (y3 - 2 * y4)) >> 6, (u - y4) >> 6, bw);   /* (made up: g2, and 1 + r more: as loud as
                                                                         * the other types when it rings) */
        o = clamp(o, -2000000, 2000000) << 8;
        lo += (int32_t)(((int64_t)(o - lo) * 224) >> 16);               /* 24 Hz high-pass */
        b[i] = fl_sat((o - lo) >> 8) << 2;
    }
    f->s[0] = y1, f->s[1] = y2, f->s[2] = y3, f->s[3] = y4, f->s[4] = fx, f->s[5] = fy, f->s[6] = li, f->s[7] = lo;
    f->hp = b0;
    f->y = kr;
}

/* (custom) ANLG: ANALOG's filter (Felucca's eng_analog.c, retired): DRIVE its soft clip (1x .. 3x), the 2-pole
 * trapezoidal SVF (dsp.c tsvf) on half the level; CHAR blends the band-pass in. s[0] s[1] its integrators */
static void fl_anlg(flt_t *f, int32_t *b, uint32_t n, int32_t ci, int32_t res, int32_t chr, int32_t drv)
{
    tsvf_t c;
    int32_t drive = 32768 + clamp(drv, 0, 127) * 512, ic1 = f->s[0], ic2 = f->s[1];
    int32_t kd = 8192 - clamp(res, 0, 127) * 7600 / 127;   /* tsvf_coef's damping, Q12: BLEND's high-pass */
    uint32_t i;
    fl_bw_t bw = fl_bw(f->bl);
    tsvf_coef(&c, clamp(ci, 0, 1024) * 32, clamp(res, 0, 127));
    for (i = 0; i < n; i++) {
        int32_t s = clamp(b[i], -600000, 600000), v1, v2, v3;
        if (drv)
            s = softclip(((s >> 2) * (drive >> 2)) >> 11);
        v3 = (s >> 1) - ic2;
        v1 = (c.a1 * ic1 + c.a2 * v3) >> 13;
        v2 = ic2 + ((c.a2 * ic1 + c.a3 * v3) >> 13);
        ic1 = clamp(2 * v1 - ic1, -150000, 150000);
        ic2 = clamp(2 * v2 - ic2, -150000, 150000);
        v3 = v2 + (((v1 - v2) * chr) >> 7);
        if (f->bl)                              /* (custom) BLEND: the band-pass at unity, the high-pass */
            v3 = fl_mix(v3, (v1 * kd) >> 12, (s >> 1) - ((v1 * kd) >> 12) - v2, bw);
        b[i] = fl_sat(v3) << 1;
    }
    f->s[0] = ic1;
    f->s[1] = ic2;
}

/* the filter on b[0..n): ty FT_*, cut 0..127, res 0..127, chr 0..127, drv 0..127, cmod: added to the cutoff index
 * (an envelope or LFO, 8 a semitone-ish: 1024 over 9.8 octaves = 8.7 an octave / 12 ... see flt_tables).
 * line: COMB's FLT_LINE int16 (may be 0 for the other types). */
static void flt_run(flt_t *f, int16_t *line, int32_t *b, uint32_t n, uint32_t ty, int32_t cut, int32_t res,
                    int32_t chr, int32_t drv, int32_t cmod)
{
    int32_t tgt, g, k, R, dq;
    if (ty >= FT_N || ty == FT_OFF) {
        f->type = FT_OFF;
        return;                                  /* bit for bit */
    }
    if (f->type != ty) {                         /* a new type from rest */
        uint8_t bl = f->bl;
        memset(f, 0, sizeof *f);
        f->bl = bl;
        f->type = (uint8_t)ty;
        f->ci = clamp(cut, 0, 127) * 8;
        if (line)
            memset(line, 0, FLT_LINE * sizeof *line);
    }
    tgt = clamp(clamp(cut, 0, 127) * 8 + cmod, 0, 1024);
    f->ci += (tgt - f->ci + (tgt > f->ci ? 1 : 0)) >> 1;   /* glide over a couple of blocks: no zipper */
    g = fl_g(f->ci);
    res = clamp(res, 0, 127);
    chr = clamp(chr, 0, 127);
    dq = 4096 + clamp(drv, 0, 127) * 220;        /* DRIVE: 1x .. ~7.8x into the filter, Q12 */
    switch (ty) {
    case FT_LPMG:                                /* k 0 .. 4.1: self-oscillates at the top; the bass kept */
        k = res * 132;
        fl_ladder(f, b, n, g, k, dq + chr * 160, 4, 0, 4096 + k / 3);
        break;
    case FT_LD:
        k = res * 128;
        fl_ladder(f, b, n, g, k, dq, (uint32_t)(chr >> 5) + 1u, 0, 4096 + k / 4);
        break;
    case FT_OB24:                                /* softer, every stage saturating: CHAR how much */
        k = res * 126;
        fl_ladder(f, b, n, g, k, dq + chr * 120, 4, chr > 0, 4096 + k / 4);
        break;
    case FT_ACID:                                /* (custom) the 303's (fl_acid) */
        fl_acid(f, b, n, f->ci, res, chr, dq);
        break;
    case FT_ANLG:                                /* (custom) ANALOG's (fl_anlg): its own DRIVE */
        fl_anlg(f, b, n, f->ci, res, chr, drv);
        break;
    case FT_SVF:                                 /* R 1.0 .. 0.008: rings, never runs away */
        R = 4096 - res * 32;
        fl_svf(f, b, n, g, R, dq, (uint32_t)(chr * 3 / 128), (chr * 3 % 128) * 32, 0, 4096);
        break;
    case FT_OB12:                                /* SEM / OB-X: LP > NOTCH > HP, the top 16 values BP; R >= 0.05 */
        R = 4096 - res * 30;
        if (chr >= 112)
            fl_svf(f, b, n, g, R, dq, 1, 0, 1, 4096);
        else                                     /* LP .. (LP + HP: the notch at the middle) .. HP */
            fl_svf(f, b, n, g, R, dq, 5, chr * 4095 / 111, 1, 4096);
        break;
    case FT_SK:
        k = res * 150;                           /* past ~2.0 the diodes hold it */
        fl_sk(f, b, n, g, k, dq, (uint32_t)(chr * 3 / 128));
        break;
    case FT_COMB:
        if (!line)
            return;
        {
            int32_t side = chr - 64, fb = res * 31 * (side < 0 ? -1 : 1);   /* +-0.96 */
            uint32_t damp = 65535u - (uint32_t)(side < 0 ? -side : side) * 900u;
            fl_comb(f, line, b, n, f->ci, fb, damp);
        }
        break;
    case FT_VOWEL:
        R = 900 - res * 6;                       /* Q 2.3 .. ~25 */
        fl_vowel(f, b, n, f->ci - 512, R, chr);  /* CUT 64: the formants as tabled */
        break;
    }
}
