/* SPDX-License-Identifier: GPL-3.0-only */
/* FMINI (custom): the Minimoog voice of FMini-1 by Guilherme Ruppert (github.com/ruppert/fmini-1, GPL-3.0-only, built
 * on Felucca), ported to Skiff as an engine.
 *
 * Kept as FMini-1 has it (eng_fmini.c there): three oscillators (TRI, SHARK, SAW, SQR, wide and narrow PULSE, ranges LO
 * 32' 16' 8' 4' 2', a slow drift each) and noise (white or pink) -> the mixer, which overloads softly -> the 24 dB
 * ladder modelled on the Minimoog's (zero-delay feedback, a tanh in front of each stage, the emphasis network's lag
 * pole; its laws in fmini/eng_fmini_filter.h, after the CA-72 project's measurements) with its own contour (ATTACK DECAY
 * SUSTAIN, the release the DECAY time) and keyboard tracking; the LFO or noise as the modulation, the mod wheel a vibrato.
 * The eight EDIT values are FMini-1's: CUT EMPH CNTR F.ATK F.DEC F.SUS MOD, and SLOT, the patch.
 *
 * What differs here: the patch (oscillators, mixer, switches, tuning) is one of FMini-1's 200 factory patches, picked by
 * SLOT (fmini/eng_fmini_presets.h FMINI_FACTORY, read where it is); FMini-1's own patch (OWN), its bank in flash, its
 * pages and its per-patch extras (arpeggiator, LFO, effects) are not ported. The 200 presets are FMini-1's, each on its
 * own SLOT. State per voice as in FMini-1: s[0..3] the ladder, s[4] the lag pole (Q8), s[5..6] the drift, s[7] the
 * contour; the part's noise once a block. */
#define ENGI_FMINI 16u
#define FMINI_POLY 4
#define FG_SIZE 32u
#define FMINI_NFACTORY 200u

enum {                           /* the patch's bytes (FMini-1's) */
    FG_R1, FG_R2, FG_R3, FG_W1, FG_W2, FG_W3, FG_F2, FG_F3, FG_L1, FG_L2, FG_L3, FG_LN, FG_SW1, FG_SW2, FG_TUNE, FG_MMIX,
    FG_KBD, FG_GLID, FG_DC2L, FG_DC2H, FG_DC3L, FG_DC3H, FG_BPML, FG_BPMH, FG_NUM
};
enum { FS_O1 = 1, FS_O2 = 2, FS_O3 = 4, FS_NZ = 8, FS_PINK = 16, FS_MOSC = 32, FS_MFLT = 64 };   /* FG_SW1 */
enum { FMS_GLIDE = 1, FMS_SQR = 2 };                                                              /* FG_SW2 */

static int32_t fmini_det(const uint8_t *g, uint32_t lo)
{
    int32_t m = g[lo] | (int32_t)(g[lo + 1] & 7) << 7;
    return g[lo + 1] & 8 ? -m : m;
}

#define FGP(r1, r2, r3, w1, w2, w3, f2, f3, l1, l2, l3, ln, s1, s2, tn, mm, kb, gl, d2l, d2h, d3l, d3h, bl, bh) \
    {r1, r2, r3, w1, w2, w3, f2, f3, l1, l2, l3, ln, s1, s2, tn, mm, kb, gl, d2l, d2h, d3l, d3h, bl, bh}
#define ALLO (FS_O1 | FS_O2 | FS_O3)

typedef struct {                 /* (FMini-1's per-patch extras: the table is in the header, not used here) */
    uint8_t arp[7], lfo[4], fx[12], level;
    int8_t mod[3], oct;
} fmini_x_t;
#include "fmini/eng_fmini_presets.h"
#include "fmini/eng_fmini_filter.h"
#undef PAT
#undef FGP

static int32_t fmini_nst[NTRK] = {0x2F6E2B1, 0x1A2B3C5, 0x3C4D5E7, 0x0F1E2D3, 0x2468ACE, 0x13579BD};
static int32_t fmini_pk[NTRK][3];
static int32_t fmini_nbuf[NTRK][CTL];
static int32_t fmini_sm[NTRK][3];
static uint8_t fmini_sm_ok;
static int32_t fmini_mn[NTRK];

/* SLOT: a stored value is -64..127 (SID records: value + 64 in a byte to 191), so the patch is SLOT + 64: 192 of the
 * 200, FMini-1's last eight (FX: SPACESHIP .. SUB DROP) left out; shown 000 .. 191 (F_SLOT) */
#define FMINI_NSLOT 192u
static const uint8_t *fmini_g(const track_t *t) { return FMINI_FACTORY[(uint32_t)clamp(t->p[P_E7] + 64, 0, FMINI_NSLOT - 1)]; }


static void fmini_block(track_t *t)
{
    uint32_t tr = (uint32_t)(t - trk), i;
    const uint8_t *g;
    if (tr >= NTRK)
        return;
    for (i = 0; i < NVOICE && !t->v[i].active; i++)
        ;
    {   /* the knobs' steps glide (FMini-1: 200 ms); silent: snap */
        static const uint8_t KN[3] = {P_E0, P_E1, P_E2};
        uint32_t j;
        for (j = 0; j < 3; j++) {
            int32_t want = (int32_t)clamp(t->p[KN[j]], 0, 127) << 16, d = want - fmini_sm[tr][j], st;
            if (i == NVOICE || !(fmini_sm_ok & (1u << tr))) {
                fmini_sm[tr][j] = want;
                continue;
            }
            st = d >= 0 ? (int32_t)(((int64_t)d * 238) >> 16) : -(int32_t)(((int64_t)-d * 238) >> 16);
            if (!st)
                st = d > 0 ? 1 : d < 0 ? -1 : 0;
            fmini_sm[tr][j] += st;
        }
        if (i == NVOICE)
            fmini_sm_ok &= (uint8_t)~(1u << tr);
        else
            fmini_sm_ok |= (uint8_t)(1u << tr);
    }
    if (i == NVOICE)
        return;
    g = fmini_g(t);
    fmini_mn[tr] = (int32_t)(noise32(&fmini_nst[tr]) >> 17) - 16384;
    if (g[FG_SW1] & FS_NZ) {
        int32_t b0 = fmini_pk[tr][0], b1 = fmini_pk[tr][1], b2 = fmini_pk[tr][2];
        for (i = 0; i < CTL; i++) {
            int32_t w = (int32_t)(noise32(&fmini_nst[tr]) >> 16) - 32768;
            if (g[FG_SW1] & FS_PINK) {
                b0 += ((w * 3245) >> 15) - ((b0 * 77) >> 15);
                b1 += ((w * 9716) >> 15) - ((b1 * 1212) >> 15);
                b2 += ((w * 34495) >> 15) - ((b2 * 14090) >> 15);
                w = (b0 + b1 + b2 + ((w * 6056) >> 15)) >> 1;
            }
            fmini_nbuf[tr][i] = w;
        }
        fmini_pk[tr][0] = b0;
        fmini_pk[tr][1] = b1;
        fmini_pk[tr][2] = b2;
    }
}

static inline int32_t fmini_shark(uint32_t ph)
{
    uint32_t x = ph >> 16;
    return x < 49152u ? (int32_t)((x * 43691u) >> 15) - 32768 : 32767 - (int32_t)((x - 49152u) * 4u);
}

static void fmini_wave(uint32_t w, uint32_t *php, uint32_t inc, int32_t g, int32_t *mix, uint32_t n)
{
    uint32_t ph = *php, i;
    switch (w) {
    case 0:
        for (i = 0; i < n; i++, ph += inc)
            mix[i] += mulq15(osc_tri(ph), g);
        break;
    case 1:
        for (i = 0; i < n; i++, ph += inc)
            mix[i] += mulq15(fmini_shark(ph), g);
        break;
    case 2:
        for (i = 0; i < n; i++, ph += inc)
            mix[i] += mulq15(osc_saw(ph, inc), g);
        break;
    case 3:
        for (i = 0; i < n; i++, ph += inc)
            mix[i] += mulq15(osc_pulse(ph, inc, 0x80000000u), g);
        break;
    case 4:
        for (i = 0; i < n; i++, ph += inc)
            mix[i] += mulq15(osc_pulse(ph, inc, 0x55555555u), g);
        break;
    default:
        for (i = 0; i < n; i++, ph += inc)
            mix[i] += mulq15(osc_pulse(ph, inc, 0x1C71C71Cu), g);
        break;
    }
    *php = ph;
}

static int32_t fmini_range16(uint32_t r)
{
    static const int16_t R[6] = {-48 * 16, -24 * 16, -12 * 16, 0, 12 * 16, 24 * 16};
    return R[r % 6u];
}

static void fmini_note_on(track_t *t, voice_t *v)
{
    uint32_t tr = (uint32_t)(t - trk) % NTRK;
    if (!v->env && !v->env_out) {
        v->s[0] = v->s[1] = v->s[2] = v->s[3] = 0;
        v->s[4] = 0;
        v->s[7] = 0;
        if (t->p[P_E1] >= 96)
            v->s[0] = (noise32(&fmini_nst[tr]) & 0x80000000u) ? 700 * 256 : -700 * 256;
        v->s[5] = (int32_t)(noise32(&fmini_nst[tr]) >> 20) - 2048;
        v->s[6] = (int32_t)(noise32(&fmini_nst[tr]) >> 20) - 2048;
    }
    v->s[7] = (int32_t)(1u << 24) | (v->s[7] & 0xFFFFFF);
}

static int32_t fmini_contour(const int16_t *p, voice_t *v)
{
    uint32_t st = (uint32_t)v->s[7] >> 24;
    int32_t e = v->s[7] & 0xFFFFFF, sus = (int32_t)p[P_E5] << 16, k = (int32_t)ENV_EXP[p[P_E4] & 127];
    if (!v->gate && st && st != 3u)
        st = 3;
    switch (st) {
    case 1: {
        int32_t step = (int32_t)(ENV_LIN[p[P_E3] & 127] >> 1), d = (int32_t)(((int64_t)((5 << 21) - e) * step * 824) >> 32);
        e += d < 1 ? 1 : d;
        if (e >= (1 << 23)) {
            e = 1 << 23;
            st = 2;
        }
        break;
    }
    case 2:
        e += mulq16(sus - e, (uint32_t)k);
        break;
    case 3:
        e -= mulq16(e, (uint32_t)k);
        break;
    default:
        break;
    }
    v->s[7] = (int32_t)(st << 24) | (e & 0xFFFFFF);
    return e >> 8;
}

static inline int32_t fmini_mulr(int32_t a, uint32_t k)
{
    return (int32_t)(((a >> 16) * (int32_t)k) + (int32_t)(((uint32_t)(a & 0xFFFF) * k + 32768u) >> 16));
}

static void fmini_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    const uint8_t *g = fmini_g(t);
    int32_t mix[CTL];
    int32_t env15, mod, lfo, vib, lc, p16, fine, k12, G16, gx16, c0, c1, c2, c3, inv14, kinv12, trim12;
    int32_t l1 = 0, l2 = 0, l3 = 0, ln = 0, dr1, dr2, dr3;
    uint32_t tr = (uint32_t)(t - trk) % NTRK, i, sw = g[FG_SW1];
    uint32_t inc1, inc2, inc3;
    int32_t s0 = v->s[0], s1 = v->s[1], s2 = v->s[2], s3 = v->s[3], sx = v->s[4];
    uint32_t ph0 = v->ph[0], ph1 = v->ph[1], ph2 = v->ph[2];
    if (n > CTL)
        n = CTL;
    env15 = fmini_contour(p, v);
    lfo = t->lfo_blk;                                   /* (the block's, as the voices' routings hear it: voice.c) */
    if (g[FG_SW2] & FMS_SQR)
        lfo = lfo >= 0 ? 32767 : -32767;
    mod = mulq15(lfo, 32767 - (int32_t)g[FG_MMIX] * 258) + mulq15(fmini_mn[tr] << 1, (int32_t)g[FG_MMIX] * 258);
    mod = mulq15(mod, (int32_t)p[P_E6] * 258);
    vib = (mulq15(lfo, (int32_t)t->mw * 258) * 16) >> 15;
    p16 = m->pitch16 + vib + (sw & FS_MOSC ? (mod * 16) >> 14 : 0);
    fine = m->fine + ((int32_t)g[FG_TUNE] - 64) * 237 / 64;
    {
        int32_t d1 = v->s[5], d2 = v->s[6];
        d1 += ((int32_t)(noise32(&fmini_nst[tr]) >> 20) - 2048 >> 6) - (d1 >> 11);
        d2 += ((int32_t)(noise32(&fmini_nst[tr]) >> 20) - 2048 >> 6) - (d2 >> 11);
        v->s[5] = d1;
        v->s[6] = d2;
        dr1 = d1 >> 8;
        dr2 = d2 >> 8;
        dr3 = (d1 - d2) >> 9;
    }
    inc1 = cents_inc(clamp(p16 + fmini_range16(g[FG_R1]), 0, 2047), dr1, fine);
    inc2 = cents_inc(clamp(p16 + fmini_range16(g[FG_R2]), 0, 2047), fmini_det(g, FG_DC2L) + dr2, fine);
    inc3 = cents_inc(clamp(p16 + fmini_range16(g[FG_R3]), 0, 2047), fmini_det(g, FG_DC3L) + dr3, fine);
    if (sw & FS_O1) l1 = (int32_t)(g[FG_L1] * g[FG_L1] * 2);
    if (sw & FS_O2) l2 = (int32_t)(g[FG_L2] * g[FG_L2] * 2);
    if (sw & FS_O3) l3 = (int32_t)(g[FG_L3] * g[FG_L3] * 2);
    if (sw & FS_NZ) ln = (int32_t)(g[FG_LN] * g[FG_LN] * 2);
    for (i = 0; i < n; i++)
        mix[i] = 0;
    if (l1) fmini_wave(g[FG_W1], &ph0, inc1, l1, mix, n); else ph0 += inc1 * n;
    if (l2) fmini_wave(g[FG_W2], &ph1, inc2, l2, mix, n); else ph1 += inc2 * n;
    if (l3) fmini_wave(g[FG_W3], &ph2, inc3, l3, mix, n); else ph2 += inc3 * n;
    if (ln)
        for (i = 0; i < n; i++)
            mix[i] += mulq15(fmini_nbuf[tr][i], ln);
    {
        int32_t kn = fmini_sm[tr][0], i0 = kn >> 16, fr = (kn >> 8) & 255;
        lc = FMINI_CUT[i0];
        if (i0 < 127)
            lc += ((FMINI_CUT[i0 + 1] - lc) * fr) >> 8;
    }
    lc += (m->cutoff * 55) >> 10;
    lc += (int32_t)(((int64_t)env15 * fmini_sm[tr][2] / (127 * 65536)) * 960) >> 15;
    lc += ((v->pitch_cur - FMINI_NOTE0 * 16) * (int32_t)g[FG_KBD] * 516) >> 16;
    if (sw & FS_MFLT)
        lc += (mod * 547) >> 15;
    lc = clamp(lc, FMINI_GM0 * 16, FMINI_FOSC_MAX16);
    {
        uint32_t ix = (uint32_t)((lc >> 4) - FMINI_GM0), fr = (uint32_t)lc & 15u, gq, gxq, g2, g3, g4, om;
        gq = FMINI_G[ix] + (uint32_t)(((int32_t)(FMINI_G[ix + 1] - FMINI_G[ix]) * (int32_t)fr) >> 4);
        gxq = FMINI_GX[ix] + (uint32_t)(((int32_t)(FMINI_GX[ix + 1] - FMINI_GX[ix]) * (int32_t)fr) >> 4);
        G16 = (int32_t)((gq + 128u) >> 8);
        gx16 = (int32_t)((gxq + 128u) >> 8);
        om = 65536u - (uint32_t)G16;
        g2 = ((uint32_t)G16 * (uint32_t)G16) >> 16;
        g3 = (g2 * (uint32_t)G16) >> 16;
        g4 = (g3 * (uint32_t)G16) >> 16;
        c3 = (int32_t)(om >> 2);
        c2 = (int32_t)(((uint32_t)G16 * om) >> 18);
        c1 = (int32_t)((g2 * om) >> 18);
        c0 = (int32_t)((g3 * om) >> 18);
        {
            int32_t kn = fmini_sm[tr][1], i0 = kn >> 16, fk = (kn >> 8) & 255, den;
            k12 = FMINI_K[i0];
            trim12 = FMINI_TRIM[i0];
            if (i0 < 127) {
                k12 += ((FMINI_K[i0 + 1] - k12) * fk) >> 8;
                trim12 += ((FMINI_TRIM[i0 + 1] - trim12) * fk) >> 8;
            }
            den = 65536 + ((k12 * (int32_t)((((uint32_t)gxq >> 8) * g4) >> 16)) >> 12);
            inv14 = (int32_t)((1u << 30) / (uint32_t)den);
            kinv12 = (k12 * inv14) >> 14;
        }
    }
    for (i = 0; i < n; i++) {
        int32_t x = softclip(mix[i]), S, fbc, u, v1, v2, v3, v4, y1, y2, y3, y4, w;
        S = (c0 * ((s0 + 128) >> 8) + c1 * ((s1 + 128) >> 8) + c2 * ((s2 + 128) >> 8) + c3 * ((s3 + 128) >> 8)) >> 14;
        fbc = sx + fmini_mulr((S << 8) - sx, (uint32_t)gx16);
        u = ((x * inv14) >> 14) - ((kinv12 * ((fbc + 128) >> 8)) >> 12);
        v1 = fmini_mulr((softclip(u) << 8) - s0, (uint32_t)G16);
        y1 = v1 + s0;
        s0 = y1 + v1;
        v2 = fmini_mulr((softclip((y1 + 128) >> 8) << 8) - s1, (uint32_t)G16);
        y2 = v2 + s1;
        s1 = y2 + v2;
        v3 = fmini_mulr((softclip((y2 + 128) >> 8) << 8) - s2, (uint32_t)G16);
        y3 = v3 + s2;
        s2 = y3 + v3;
        v4 = fmini_mulr((softclip((y3 + 128) >> 8) << 8) - s3, (uint32_t)G16);
        y4 = v4 + s3;
        s3 = y4 + v4;
        w = fmini_mulr(y4 - sx, (uint32_t)gx16);
        sx += w + w;
        out[i] += voice_amp2((((y4 + 128) >> 8) * trim12) >> 12, m, i);   /* ((2.5, #110) rounded once) */
    }
    v->ph[0] = ph0;
    v->ph[1] = ph1;
    v->ph[2] = ph2;
    v->s[0] = s0;
    v->s[1] = s1;
    v->s[2] = s2;
    v->s[3] = s3;
    v->s[4] = sx;
}

static const engine_t ENG_FMINI = {
    .name = "FMINI",
    .page_title = {"FILTER", "PATCH"},
    .edit = {
        {"CUT", 0, 0, 127, 64, F_PCT, 0},
        {"EMPH", 0, 0, 127, 40, F_PCT, 0},
        {"CNTR", 0, 0, 127, 64, F_PCT, 0},
        {"F.ATK", 0, 0, 127, 0, F_PCT, 0},
        {"F.DEC", 0, 0, 127, 64, F_PCT, 0},
        {"F.SUS", 0, 0, 127, 40, F_PCT, 0},
        {"MOD", 0, 0, 127, 0, F_PCT, 0},
        {"SLOT", 0, -64, 127, -64, F_SLOT, 0},   /* (shown 000 .. 191: the patch) */
    },
    .presets = FMINI_PRESETS,
    .npresets = NELEM(FMINI_PRESETS),
    .note_on = fmini_note_on,
    .render = fmini_render,
    .knob = {P_E0, P_E1, P_E2, P_E4},
    .poly = FMINI_POLY,
    .keep = 0x13,
    .block = fmini_block,
};
