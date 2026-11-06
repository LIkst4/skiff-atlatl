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
 *     ORGAN  (custom, WHEEL's voice: WHEEL retired) nine drawbar sines at 16' 5 1/3' 8' 4' 2 2/3' 2' 1 3/5' 1 1/3' 1',
 *            SHAPE morphs through WHEEL's 16 registrations (FLUTE .. FULL, SY_REG), the louder ones softly normalised;
 *            MOD (shown PERC) 1..63 a decaying 2nd harmonic (4'), 64..127 the 3rd (2 2/3'), louder up each half, and
 *            the key click with it. Partials above ~18 kHz are left out. For the rotor: the track's CHORUS / the LFO
 *     FOLD   (custom, after Plaits' waveshaper) a triangle through a sine wavefolder: SHAPE folds it (1x .. 9x, held down
 *            up the keyboard: less aliasing), MOD (shown SKEW) leans the triangle into a ramp (the even harmonics)
 *     CHORD  (custom, after Plaits' chords) four saws at a chord's notes: SHAPE picks it (OCT 5 SUS4 m m7 m9 m11 69 M9
 *            M7 M), MOD (shown INV) its inversion (0..4: the lowest notes an octave up)
 *     SWARM  (custom, after Plaits' swarm / the supersaw) seven saws spread around the note: SHAPE the spread (0 .. ~2
 *            semitones out, on a curve: half way ~70 cents), MOD (shown SIDES) the six around against the middle one
 *     SQR    (custom, ANALOG's square / pulse) SHAPE the width (50 % .. 5 %), MOD (shown SUB) as VA's
 *     TRI    (custom, ANALOG's triangle) SHAPE drives it round (a triangle .. a rounded trapezoid), MOD (SUB) as VA's
 *     PWM    (custom, ANALOG's PWM) a pulse whose width moves by itself: SHAPE the centre (50 % .. 10 %), MOD (shown
 *            DEPTH) how far an inner triangle (~0.8 Hz) swings it; the SHP routings move the centre
 *     303    (custom, X0X's TB-303 oscillator, Open303's) SHAPE its saw .. its square (a transistor-shaped saw, half as
 *            loud, as Open303 blends them), MOD (SUB) as VA's. With the ACID filter: the 303
 *     AWAVE  (custom) a wavetable of 8 waves modelled on analog circuits (CAP SAW JUNO SHARK TRI SAT RC SQR OB PULSE SYNC
 *            RESO SAW, tools/gen_synth_tables.py), SHAPE scans them, MOD (shown WARP) as WAVE's; three band limits (48,
 *            16 and 6 harmonics)
 *   (CHORD and SWARM keep their phases in the MODAL's area, synth_vc_t ma / mb: a model or the other)
 *   POLY MOD (custom, the Prophet-5's): ENV AMT (ENV 2: its own page) and OSC B (B's signal, at any MIX) on DEST: FREQ A
 *   (ENV: +-4 octaves; B: FM, A's rate +-2x at 127), PW A (SHAPE A), FILTER, FRQ+FLT or ALL. B moves PW A and FILTER
 *   once a block (B LOW: an LFO's rate; at audio rates only FREQ A follows it); B's models only (not SYNC, RING). B
 *   LOW: OSC B at 0.5 .. 8 Hz (B TUNE
 *   from -24), the keys left out of it, as the Prophet's LOW FREQ with KEYBOARD off
 *   OSC B also (custom, after TRIO and ANALOG's sync, TRIO retired):
 *     SYNC   a saw at B's pitch restarted at each of A's cycles (hard sync: B TUNE sweeps the formant); SHAPE B: the
 *            saw .. a square, as VA's
 *     RING   a sine at B's pitch times A (ring modulation: bells, metal); SHAPE B: the dry A .. the ring alone
 *   SHAPE A, SHAPE B, MOD (A's model names it; B's model reads it too), B TUNE (semitones), DETUNE (B, 0..50 cents),
 *   MIX (A .. B; B OFF: A alone).
 * The filter is the track's FILTER (FX > FILTER, FILTER 2: TYPE CUTOFF RESO CHAR DRIVE, flt.c), run here per voice so
 * each note has its own; fx.c leaves the track's off for this engine. ENV DEST / LFO DEST FLT and the matrix's CUT move
 * it (vmod_t cutoff), and it follows the keyboard at half. A COMB passes the voice through (its line is the track's).
 * SHAPE routings (ENV / LFO DEST SHP, the matrix's SHP) move SHAPE A.
 * State: the filter and the second / sub phases per voice in the part's engine area (synth_vc_t); voice_t's phases
 * ph[0] A, ph[1] B, ph[2] the sub (ORGAN: its 16'); s[0] the noise, s[1] its colour (ORGAN: the percussion's
 * envelope), s[2] its hold (ORGAN: the click's countdown); B's in synth_vc_t. */
#include "synth_tables.h"

enum { SY_VA, SY_WAVE, SY_ADD, SY_NOISE, SY_ORGAN, SY_MODAL, SY_FOLD, SY_CHORD, SY_SWARM, SY_SQR, SY_TRI, SY_PWM, SY_303,
       SY_AWAVE, SY_N, SY_SYNC = SY_N, SY_RING };   /* OSC A's (stored: appended); B's own two after */
/* OSC B's stored values (appended): OFF, the models 1..5 (1 + SY_*), SYNC, RING, (custom) MODAL, FOLD, CHORD, SWARM,
 * SQR, TRI, PWM, 303, AWAVE (B's value - SYB_MODAL + SY_MODAL: A's model) */
enum { SYB_OFF, SYB_SYNC = 6, SYB_RING, SYB_MODAL, SYB_FOLD, SYB_CHORD, SYB_SWARM, SYB_SQR, SYB_TRI, SYB_PWM, SYB_303,
       SYB_AWAVE, SYB_N };
static const char *const N_SY_OSC[] = {"VA", "WAVE", "ADD", "NOISE", "ORGAN", "MODAL", "FOLD", "CHORD", "SWARM", "SQR",
                                       "TRI", "PWM", "303", "AWAVE"};
static const char *const N_SY_OSCB[] = {"OFF", "VA", "WAVE", "ADD", "NOISE", "ORGAN", "SYNC", "RING", "MODAL", "FOLD",
                                        "CHORD", "SWARM", "SQR", "TRI", "PWM", "303", "AWAVE"};
_Static_assert(NELEM(N_SY_OSC) == SY_N && NELEM(N_SY_OSCB) == SYB_N, "SYNTH's model names");
_Static_assert(SYB_AWAVE - SYB_MODAL == SY_AWAVE - SY_MODAL, "OSC B's models as A's");
/* OSC B's stored value -> its model (SY_*, SY_SYNC, SY_RING); 0 (OFF) -> SY_BOFF */
#define SY_BOFF 31u
static uint32_t sy_bmodel(int32_t b)
{
    return b <= 0 ? SY_BOFF : b < SYB_SYNC ? (uint32_t)b - 1u : b == SYB_SYNC ? SY_SYNC : b == SYB_RING ? SY_RING
                                                                 : (uint32_t)(b - SYB_MODAL) + SY_MODAL;
}
/* (custom, after Plaits) a stored value of A's (P_E0) or B's (P_E3) is model md (MODAL and after) */
#define SY_IS_A(p, md) ((p)[P_E0] == (md))
#define SY_IS_B(p, md) ((p)[P_E3] == (md) - SY_MODAL + SYB_MODAL)

#define SY_NMODE 8               /* MODAL's modes ((custom) 8, after Rings': was 6) */
typedef struct {
    int32_t y[2 * SY_NMODE];     /* each mode's last two outputs */
    int32_t exc;                 /* the strike: samples left */
    int32_t lp;                  /* (after Rings) the strike's low-pass (BRIGHT) */
    uint8_t bright, pos;         /* (after Rings) BRIGHT, POSITION: the render sets them each block (P_SYMBRT, P_SYMPOS) */
} sy_modal_t;

typedef struct {
    flt_t f;                     /* the voice's filter (flt.c) */
    int32_t nb, nlb, hb;         /* OSC B's noise: state, colour, hold (ORGAN: percussion, click) */
    uint32_t subb;               /* OSC B's sub / 16' phase */
    uint32_t subph;              /* (custom) SUB's phase */
    int32_t walk;                /* DRIFT's random walk (cents x 16) */
    int32_t acc;                 /* the accent's envelope on the cutoff, Q15 (303) */
    uint32_t fm;                 /* (custom) POLY MOD's OSC B -> FREQ A: A's phase pushed by B (the sum) */
    sy_modal_t ma, mb;           /* MODAL's modes, A's and B's */
} synth_vc_t;

/* (custom, 303 / Devil Fish) the accents' sweep: each accent charges it (ACCENT), it drains over ~0.3 s; the cutoff
 * goes up with it, so a run of accents climbs higher and higher, as the 303's sweep capacitor. Per track */
static int32_t sy_sweep[NTRK];
/* (custom, after OberVave's VINTAGE) DRIFT: each voice card a little out of tune (cents) and its filter a little
 * apart, and a slow random walk on the pitch */
static const int8_t SY_CARD_CT[NVOICE] = {3, -5, 1, 6, -2, -6, 4, -1};
static const int8_t SY_CARD_CUT[NVOICE] = {-6, 4, 8, -3, 2, -8, 5, -1};
/* MODAL: the modes' ratios of three bodies, SHAPE morphing string -> bar -> bell (x 1024), their levels (Q15) */
static const uint16_t SY_MODE_R[3][SY_NMODE] = {{1024, 2048, 3072, 4096, 5120, 6144, 7168, 8192},       /* a string */
                                                {1024, 2822, 5534, 9147, 13660, 19087, 25406, 32635},   /* a free bar */
                                                {1024, 1219, 1536, 2048, 2580, 3072, 3686, 4301}};      /* a bell */
static const uint16_t SY_MODE_A[SY_NMODE] = {32767, 20000, 14000, 10000, 7000, 5000, 3500, 2500};
/* (custom, after Plaits) CHORD's chords (semitones over the root: OCT 5 SUS4 m m7 m9 m11 69 M9 M7 M), 2^(k/12) Q16 */
static const uint8_t SY_CHD[11][4] = {{0, 0, 12, 12}, {0, 7, 7, 12}, {0, 5, 7, 12}, {0, 3, 7, 12}, {0, 3, 7, 10},
                                      {0, 3, 10, 14}, {0, 3, 10, 17}, {0, 2, 9, 16}, {0, 4, 11, 14}, {0, 4, 7, 11},
                                      {0, 4, 7, 12}};
static const uint32_t SY_SEMI[12] = {65536, 69433, 73562, 77936, 82570, 87480, 92682, 98193, 104032, 110218, 116772,
                                     123715};
/* SWARM's saws: each one's place in the spread (Q15 of it; the middle one 0), after the supersaw's uneven offsets */
static const int16_t SY_SWM[7] = {0, -32767, 32112, -18728, 18513, -5814, 5930};

/* (custom) ORGAN: WHEEL's 16 registrations, one digit per bar: 16' 5 1/3' 8' 4' 2 2/3' 2' 1 3/5' 1 1/3' 1'; the bars'
 * harmonics of the 16' (x 1 3 2 4 6 8 10 12 16); a bar's level 0..8 -> Q15, 3 dB a step (eng_wheel.c, retired) */
static const char SY_REG[16][10] = {"008000000", "008400000", "800800000", "838000000", "888000000", "888800000",
                                    "888600006", "888000888", "800000888", "008080800", "008765432", "006876540",
                                    "004432000", "008888000", "858000000", "888888888"};
static const uint8_t SY_BAR[9] = {1, 3, 2, 4, 6, 8, 10, 12, 16};
static const uint16_t SY_BARLV[9] = {0, 2920, 4125, 5827, 8231, 11627, 16423, 23198, 32767};
#define SY_CLICK 220             /* the key click: samples (~5 ms) */

static synth_vc_t *synth_vc(track_t *t, voice_t *v)
{
    synth_vc_t *S = t >= &trk[0] && t < &trk[NPART] ? (synth_vc_t *)eng_part(t) : 0;
    uint32_t i = (uint32_t)(v - t->v);
    return S && i < NVOICE ? &S[i] : 0;
}

static void synth_note_on(track_t *t, voice_t *v)
{
    synth_vc_t *S = synth_vc(t, v);
    if (!v->env_out) {                                   /* a note from silence (voice.c voice_start: not a retrigger) */
        v->ph[0] = v->ph[2] = 0;                         /* starts the same every time: its phases, the filter, the */
        v->s[0] = 0x2468ACE;                             /* noise (a retrigger keeps them: no click) */
        v->s[1] = v->s[2] = 0;
        if (S) {                                         /* (the filter's type OFF: flt_run sets it up from rest) */
            memset(S, 0, sizeof *S);
            S->nb = 0x13579BD;
        }
    }
    if (!v->s[0])
        v->s[0] = 0x2468ACE + (int32_t)v->age;           /* noise states */
    v->ph[1] = v->ph[0] + 0x2C000000u;                   /* B a little apart from A */
    if (S && !S->nb)
        S->nb = 0x13579BD + (int32_t)v->age;
    if (t->p[P_E0] == SY_ORGAN) {                        /* ORGAN: the percussion and the click strike */
        v->s[1] = 32767;
        v->s[2] = SY_CLICK;
    }
    if (S && t->p[P_E3] == 1 + SY_ORGAN) {
        S->nlb = 32767;
        S->hb = SY_CLICK;
    }
    if (S && !v->env_out) {                              /* (custom) SWARM, CHORD: their saws apart (not in step) */
        uint32_t k;
        if (SY_IS_A(t->p, SY_SWARM) || SY_IS_A(t->p, SY_CHORD))
            for (k = 0; k < 7u; k++)
                S->ma.y[k] = (int32_t)(k * 0x9E3779B9u);
        if (SY_IS_B(t->p, SY_SWARM) || SY_IS_B(t->p, SY_CHORD))
            for (k = 0; k < 7u; k++)
                S->mb.y[k] = (int32_t)(k * 0x7F4A7C15u);
    }
    if (S) {
        if (t->p[P_E0] == SY_MODAL)                      /* (custom) MODAL: struck */
            S->ma.exc = 24;
        if (t->p[P_E3] == SYB_MODAL)
            S->mb.exc = 24;
        if (v->vel >= 120 && t->p[P_SYACC]) {            /* (303) an accent: its envelope, and the sweep charges */
            uint32_t k = (uint32_t)(t - trk) % NTRK;
            S->acc = 32767;
            sy_sweep[k] = clamp(sy_sweep[k] + t->p[P_SYACC] * 90, 0, 32767);
        } else {
            S->acc = 0;
        }
    }
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

/* (custom) MODAL: SY_NMODE two-pole resonators at the modes of a body (SHAPE: string .. bar .. bell), struck by a
 * short burst of noise (note on: exc); MOD damps them (0: rings for seconds, 127: a knock), the higher modes sooner.
 * The coefficients once a block from the pitch (inc); a mode past ~16 kHz sits out. Q30 coefficients, 64-bit products */
static __attribute__((noinline)) void sy_modal(sy_modal_t *mm, uint32_t inc, int32_t sh, int32_t mod, int32_t *y,
                                               uint32_t n, int32_t g, int32_t *nz)
{
    int32_t a1[SY_NMODE], a2[SY_NMODE], sw[SY_NMODE], i, k, t = sh >> 8, w = t < 64 ? t * 4 : (t - 64) * 4, m;
    uint32_t b = t < 64 ? 0u : 1u;
    if (!mm)
        return;
    m = (int32_t)SY_NMODE;
    for (k = 0; k < SY_NMODE; k++) {
        uint32_t r16 = (SY_MODE_R[b][k] * (uint32_t)(256 - w) + SY_MODE_R[b + 1u][k] * (uint32_t)w) >> 8;   /* x 1024 */
        uint32_t f = (uint32_t)(((uint64_t)inc * r16) >> 10), tau, r30;
        int32_t c, sn;
        if (f >= 0x5C000000u) {                          /* past ~16 kHz: out */
            m = k;
            break;
        }
        c = sine_i(f + 0x40000000u);                     /* cos w, sin w (Q15) */
        sn = sine_i(f);
        tau = (uint32_t)(48000 - mod * 370) / (uint32_t)(2 + k + (mod >> 4));   /* samples to 1/e: ~0.5 s .. a knock */
        if (tau < 64u)
            tau = 64u;
        r30 = (1u << 30) - (1u << 30) / tau;
        a1[k] = (int32_t)(((int64_t)r30 * c) >> 14);     /* 2 r cos w, Q30 */
        a2[k] = (int32_t)(((uint64_t)r30 * r30) >> 30);   /* r^2 */
        sw[k] = mulq15(sn < 0 ? -sn : sn, SY_MODE_A[k]);   /* the strike scaled by sin w: each mode at its level */
        if (mm->bright < 127u) {                         /* (after Rings) BRIGHT: the higher modes softer, k steps of */
            int32_t b = 11500 + mm->bright * 167, w = 32767, j;   /* 0.35 .. 1 (Q15) */
            for (j = 0; j < k; j++)
                w = mulq15(w, b);
            sw[k] = mulq15(sw[k], w);
        }
        if (mm->pos) {                                   /* (after Rings) POSITION: struck at pos of its length, mode k
                                                          * by |sin(pi (k + 1) pos)|, pos 0 .. 0.5 */
            int32_t q = sine_i((uint32_t)(k + 1) * mm->pos * 8454660u);   /* (2^31 / 254) */
            sw[k] = mulq15(sw[k], q < 0 ? -q : q);
        }
    }
    for (i = 0; i < (int32_t)n; i++) {
        int32_t x = 0, out = 0;
        if (mm->exc > 0) {                               /* the strike: noise, falling */
            x = (((int32_t)(noise32(nz) >> 17) - 16384) * mm->exc) >> 4;
            mm->exc--;
            if (mm->bright < 127u) {                     /* (BRIGHT: a softer mallet, its noise low-passed) */
                mm->lp += mulq15(x - mm->lp, 1500 + mm->bright * 246);
                x = mm->lp;
            }
        }
        for (k = 0; k < m; k++) {
            int32_t *s = &mm->y[2 * k], yk;
            yk = (int32_t)(((int64_t)a1[k] * s[0] + (1 << 29)) >> 30) -   /* (two products, each rounded and */
                 (int32_t)(((int64_t)a2[k] * s[1] + (1 << 29)) >> 30) +   /* shifted: no 64-bit multiply-subtract) */
                 mulq15(x, sw[k]);
            s[1] = s[0];
            s[0] = clamp(yk, -(1 << 20), 1 << 20);      /* (bounded: it can't run away) */
            out += yk;
        }
        y[i] += mulq15(clamp(out >> 2, -65536, 65536), g);
    }
}

/* (custom, after Plaits) FOLD: a triangle (leaning into a ramp: mod) through sin(pi/2 a x), a the fold (Q8) held
 * down by the pitch; CHORD: four saws at the chord's notes (xp: their phases); SWARM: seven saws spread around inc
 * (xp). Each into y at g; pm: X-MOD's phase offsets (FOLD) */
static __attribute__((noinline)) void sy_fold(uint32_t *ph, uint32_t inc, int32_t sh, int32_t mod, int32_t *y,
                                              uint32_t n, int32_t g, const int32_t *pm)
{
    uint32_t p = *ph, i, a = 256u + (uint32_t)sh / 16u, amax = inc > 0x00100000u ? 0x10000000u / (inc >> 8) : 0xFFFFu;   /* (a x the
                                                        * pitch under ~3 kHz) */
    uint32_t k16 = 32768u + (uint32_t)mod * 232u;          /* the rise: half the cycle .. ~95 % (Q16) */
    uint32_t up = 0xFFFFFFFFu / k16, dn = 0xFFFFFFFFu / (65536u - k16);   /* the slopes (Q16 of 65536 a cycle) */
    if (a > amax)
        a = amax < 256u ? 256u : amax;
    for (i = 0; i < n; i++, p += inc) {
        uint32_t q = (pm ? p + (uint32_t)pm[i] : p) >> 16;
        int32_t tr = q < k16 ? (int32_t)(((uint64_t)q * up) >> 16) - 32768
                             : 32767 - (int32_t)(((uint64_t)(q - k16) * dn) >> 16);
        y[i] += mulq15(sine_i(((uint32_t)clamp(tr, -32768, 32767) * a) << 7), g);   /* (x 1: a quarter cycle, 2^30) */
    }
    *ph = p;
}
static __attribute__((noinline)) void sy_chord(uint32_t *ph, uint32_t *xp, uint32_t inc, int32_t sh, int32_t mod,
                                               int32_t *y, uint32_t n, int32_t g)
{
    uint32_t c = (uint32_t)sh * 11u / (128u << 8), inv = (uint32_t)mod * 5u / 128u, i, k, nk = 0, ik[4];
    for (k = 0; k < 4u; k++) {                           /* each note's rate; the lowest inv an octave up (4: all, the
                                                          * root two) */
        uint32_t s = SY_CHD[c][k] + (k < inv ? 12u : 0u) + (inv > 3u && k == 0u ? 12u : 0u);
        uint64_t f = (((uint64_t)inc * SY_SEMI[s % 12u]) >> 16) << (s / 12u);
        if (f < 0x68000000ull)                           /* (past ~18 kHz: out) */
            ik[nk++] = (uint32_t)f;
    }
    for (i = 0; i < n; i++) {
        int32_t x = 0;
        for (k = 0; k < nk; k++) {
            x += osc_saw(xp[k], ik[k]);
            xp[k] += ik[k];
        }
        y[i] += mulq15(x >> 1, g);                       /* (four saws: -6 dB) */
    }
    *ph += inc * n;
}
static __attribute__((noinline)) void sy_swarm(uint32_t *ph, uint32_t *xp, uint32_t inc, int32_t sh, int32_t mod,
                                               int32_t *y, uint32_t n, int32_t g)
{
    int32_t sp = mulq15(sh, 9830 + mulq15(sh, 22937)), gs = mod * 258, gc = 32767 - mod * 140, nm;   /* ((custom) the
                                                          * spread on a curve: x (0.3 + 0.7 x), Q15 of ~12 %), the levels */
    uint32_t i, k, ik[7];
    nm = (int32_t)((32767u * 32768u) / (uint32_t)(gc + ((gs * 5) >> 1) + 1));   /* (the six ~ sqrt 6 = 2.5 x one) */
    if (nm > 32767)
        nm = 32767;
    for (k = 0; k < 7u; k++)                             /* (+-12 % at full SHAPE: ~2 semitones) */
        ik[k] = (uint32_t)((int64_t)inc + (((int64_t)inc * mulq15(SY_SWM[k], sp)) >> 18));
    gs = mulq15(gs, nm);
    gc = mulq15(gc, nm);
    for (i = 0; i < n; i++) {
        int32_t x = mulq15(osc_saw(xp[0], ik[0]), gc), z = 0;
        xp[0] += ik[0];
        for (k = 1; k < 7u; k++) {
            z += osc_saw(xp[k], ik[k]);
            xp[k] += ik[k];
        }
        y[i] += mulq15(x + mulq15(z, gs), g);
    }
    *ph += inc * n;
}

/* (custom) SQR TRI PWM: ANALOG's oscillators (Felucca's eng_analog.c, retired) as models; 303: X0X's (charlesvestal/
 * fm1-x0x dsp/bass303.c, after Open303 by Robin Schmidt, MIT) saw and square; AWAVE: the analog-modelled table.
 * *sub: SUB's phase (PWM: its inner triangle's) */
static __attribute__((noinline)) void sy_anlg(uint32_t md, uint32_t *ph, uint32_t inc, int32_t sh, int32_t mod,
                                              int32_t *y, uint32_t n, int32_t g, uint32_t *sub, const int32_t *pm)
{
    uint32_t p = *ph, s2 = *sub, i2 = inc >> 1, i;
    int32_t sl = md == SY_PWM || md == SY_AWAVE ? 0 : mod * 200;   /* SUB, Q15 */
#define PQ(i) (pm ? p + (uint32_t)pm[i] : p)
    if (md == SY_AWAVE) {                                    /* SHAPE scans the 8 waves, MOD warps (as WAVE) */
        int32_t pos = sh * (SYN_AWN - 1) / 127;
        uint32_t lim = inc < 0x026A0000u ? 0u : inc < 0x07400000u ? 1u : 2u;   /* below ~420 Hz, ~1.25 kHz, above */
        uint32_t k = mod ? 32768u - (uint32_t)mod * 240u : 0u;
        for (i = 0; i < n; i++, p += inc) {
            uint32_t q = sy_warp(PQ(i), k), w = (uint32_t)pos >> 8, j = q >> 25, fr = (q >> 10) & 0x7FFFu;
            const int16_t *a = SYN_AWT[w][lim], *c = SYN_AWT[w + 1u < SYN_AWN ? w + 1u : w][lim];
            int32_t xa = a[j] + (((a[(j + 1u) & 127u] - a[j]) * (int32_t)fr) >> 15);
            int32_t xc = c[j] + (((c[(j + 1u) & 127u] - c[j]) * (int32_t)fr) >> 15);
            y[i] += mulq15(xa + (((xc - xa) * (pos & 255)) >> 8), g);
        }
        *ph = p;
        return;
    }
    {
        uint32_t pw = 0x80000000u;
        int32_t dr = 0;
        if (md == SY_SQR)                                    /* 50 % .. 5 % */
            pw -= (uint32_t)sh * 59447u;
        else if (md == SY_PWM) {                             /* the centre 50 % .. 10 %, the inner triangle ~0.8 Hz */
            int32_t tr = osc_tri(s2), sw = mulq15(tr, mod * 258);   /* +-1 at DEPTH 127: +-40 % */
            int64_t w = (int64_t)0x80000000u - (int64_t)sh * 52842 + (int64_t)sw * 52428;
            pw = (uint32_t)(w < 0x08000000 ? 0x08000000 : w > 0xF8000000u ? 0xF8000000u : w);
            s2 += 77925u * n;
        } else if (md == SY_TRI)                             /* the drive: 1x .. 5x into a soft clip, blended in */
            dr = sh;
        for (i = 0; i < n; i++, p += inc) {
            uint32_t q = PQ(i);
            int32_t x;
            if (md == SY_TRI) {
                x = osc_tri(q);
                if (dr)
                    x += mulq15(softclip(x + ((x * (dr >> 6)) >> 7)) - x, dr);
            } else if (md == SY_303) {                       /* Open303's saw (inverted) .. its square */
                int32_t sw = -osc_saw(q + 0x80000000u, inc), sq;
                uint32_t j = q >> 24, fr = (q >> 16) & 255u;
                sq = SY_303SQ[j] + (((SY_303SQ[j + 1u] - SY_303SQ[j]) * (int32_t)fr) >> 8) - (blep(q, inc) >> 1);
                x = sw + mulq15(sq - sw, sh);
            } else {
                x = osc_pulse(q, inc, pw) << 1;
            }
            if (sl) {
                x += mulq15(osc_pulse(s2, i2, 0x80000000u) << 1, sl) >> 1;
                s2 += i2;
            }
            y[i] += mulq15(x, g);
        }
    }
#undef PQ
    *ph = p;
    *sub = s2;
}

/* one oscillator of model md into y[0..n) (added at gain g, Q15): its phase *ph at inc; shape 0..127 << 8 */
static __attribute__((noinline)) void sy_osc(uint32_t md, uint32_t *ph, uint32_t inc, int32_t shape, int32_t mod,
                                             int32_t *y, uint32_t n, int32_t g, uint32_t *sub, int32_t *nz,
                                             int32_t *nl, int32_t *hold, const int32_t *pm, sy_modal_t *mm)
{
    uint32_t p = *ph, i;
#define PQ(i) (pm ? p + (uint32_t)pm[i] : p)          /* ((custom) X-MOD: B's signal moves A's phase) */
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
                x = osc_saw(PQ(i), inc);
            else if (sq >= 32767)                            /* a pulse alone */
                x = osc_pulse(PQ(i), inc, pw) << 1;
            else {
                int32_t a = osc_saw(PQ(i), inc);
                x = a + ((((osc_pulse(PQ(i), inc, pw) << 1) - a) * sq) >> 15);
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
            y[i] += mulq15(sy_wt(sy_warp(PQ(i), k), pos, lim), g);
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
            uint32_t q = PQ(i);
            int32_t x = mulq15(sine_i(q), amp[0]);           /* the first three interpolated, the rest a plain */
            if (kmax > 1u)                                   /* lookup (quieter: its -55 dB a partial is far down) */
                x += mulq15(sine_i(q * 2u), amp[1]);
            if (kmax > 2u)
                x += mulq15(sine_i(q * 3u), amp[2]);
            for (k = 3; k < kmax; k++)
                x += mulq15(SINE[(q * (k + 1u)) >> 22], amp[k]);
            y[i] += mulq15(x, g);
        }
        break;
    }
    case SY_ORGAN: {                                         /* (custom) the drawbars on the 16' phase q */
        int32_t pos = sh * 15 / 127, f = pos & 255, amp[9], sum = 0, pl = 0, e = *nl, ck = *hold, cl = mod * 90;
        uint32_t r = (uint32_t)pos >> 8, r2 = r < 15u ? r + 1u : r, k, q = *sub, iq = inc >> 1, kmax = 0, ph2 = 4u;
        for (k = 0; k < 9u; k++) {
            amp[k] = (SY_BARLV[SY_REG[r][k] - '0'] * (256 - f) + SY_BARLV[SY_REG[r2][k] - '0'] * f) >> 8;
            if ((uint64_t)iq * SY_BAR[k] >= 0x68000000ull)  /* above ~18 kHz: out */
                amp[k] = 0;
            else if (amp[k])
                kmax = k + 1u;
            sum += amp[k];
        }
        if (mod) {                                           /* PERC: the 2nd (4') or the 3rd (2 2/3') */
            pl = (mod < 64 ? mod : mod - 63) * 500;
            ph2 = mod < 64 ? 4u : 6u;
            if ((uint64_t)iq * ph2 >= 0x68000000ull)
                pl = 0;
        }
        {   /* the louder registrations softly normalised; with PERC the bars 3 dB down (as the real circuit) */
            int32_t lim = pl ? 20000 : 28000, g2 = sum > lim ? (int32_t)((uint32_t)lim * 32768u / (uint32_t)sum) : 32767;
            if (pl)
                g2 = (g2 * 23170) >> 15;
            for (k = 0; k < kmax; k++)
                amp[k] = (amp[k] * g2) >> 15;
        }
        for (i = 0; i < n; i++, p += inc, q += iq) {
            int32_t x = mulq15(sine_i(q), amp[0]) + mulq15(sine_i(q * 2u), amp[2]);   /* 16' 8' interpolated */
            for (k = 1; k < kmax; k++)
                if (k != 2u && amp[k])
                    x += mulq15(SINE[(q * SY_BAR[k]) >> 22], amp[k]);
            if (e > 8) {
                if (pl)
                    x += mulq15(SINE[(q * ph2) >> 22], mulq15(pl, e));
                e -= (e >> 13) + 1;                          /* ~190 ms */
            }
            if (ck > 0) {                                    /* the click: noise, falling */
                x += (((int32_t)(noise32(nz) >> 17) - 16384) * (cl * ck / SY_CLICK)) >> 13;
                ck--;
            }
            y[i] += mulq15(x, g);
        }
        *sub = q;
        *nl = e;
        *hold = ck;
        break;
    }
    case SY_MODAL:                                           /* (custom) struck modes (sy_modal) */
        sy_modal(mm, inc, sh, mod, y, n, g, nz);
        p += inc * n;
        break;
    case SY_FOLD:                                            /* (custom, after Plaits) */
        sy_fold(&p, inc, sh, mod, y, n, g, pm);
        break;
    case SY_CHORD:
        if (mm)
            sy_chord(&p, (uint32_t *)mm->y, inc, sh, mod, y, n, g);
        break;
    case SY_SWARM:
        if (mm)
            sy_swarm(&p, (uint32_t *)mm->y, inc, sh, mod, y, n, g);
        break;
    case SY_SQR: case SY_TRI: case SY_PWM: case SY_303: case SY_AWAVE:   /* (custom) sy_anlg */
        sy_anlg(md, &p, inc, sh, mod, y, n, g, sub, pm);
        break;
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
#undef PQ
}

/* (custom) OSC B's SYNC: a saw (.. a square: shape) at incb restarted at each wrap of A's phase pa (inc ia), added
 * at g; RING: a sine at incb times A's own output ya (dry .. ring alone: shape), added at g */
static __attribute__((noinline)) void sy_sync(uint32_t *ph, uint32_t incb, uint32_t pa, uint32_t ia, int32_t shape,
                                              int32_t *y, uint32_t n, int32_t g)
{
    uint32_t p = *ph, i;
    int32_t sq = clamp(shape, 0, 127 << 8) * 2;               /* saw -> square, Q15 (0 .. 254 << 7) */
    for (i = 0; i < n; i++) {
        uint32_t a = pa;
        int32_t x = osc_saw(p, incb);
        if (sq)
            x += ((((p < 0x80000000u ? 32767 : -32767)) - x) * (sq > 32767 ? 32767 : sq)) >> 15;
        y[i] += mulq15(x, g);
        pa += ia;
        if (pa < a) {                                    /* A wrapped: B restarts where it would be by now */
            uint32_t fr = pa / ((ia >> 16) | 1u);        /* (the part of a sample since: Q16, 32-bit) */
            p = (incb >> 16) * (fr < 65535u ? fr : 65535u);
        } else
            p += incb;
    }
    *ph = p;
}
static __attribute__((noinline)) void sy_ring(uint32_t *ph, uint32_t incb, const int32_t *ya, int32_t shape,
                                              int32_t *y, uint32_t n, int32_t g)
{
    uint32_t p = *ph, i;
    int32_t w = clamp(shape, 0, 127 << 8) >> 2, d = 32767 - w * 2;   /* the ring's share, the dry's */
    if (d < 0)
        d = 0;
    for (i = 0; i < n; i++, p += incb)
        y[i] += mulq15(mulq15(mulq15(ya[i], sine_i(p)), w * 2 > 32767 ? 32767 : w * 2) + mulq15(ya[i], d), g);
    *ph = p;
}

static void synth_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    synth_vc_t *S = synth_vc(t, v);
    int32_t y[CTL], ya[CTL], yb[CTL], pmb[CTL], mix = p[P_E7] * 258, ga, gb, *pm = 0;
    uint32_t i, ma = (uint32_t)clamp(p[P_E0], 0, SY_N - 1), mb = sy_bmodel(clamp(p[P_E3], 0, SYB_N - 1));
    int32_t sha = (p[P_E1] << 8) + m->shape - (64 << 8);    /* the SHP routings around SHAPE A */
    uint32_t inca = m->inc, incb, sub = v->ph[2], pa0 = v->ph[0], vi = (uint32_t)(v - t->v) % NVOICE;
    int32_t drift = p[P_SYDRFT], cm, pme = p[P_SYPME], pob = p[P_SYPMB], e15 = 0, dct = 0, bs = 0, brd = 0;
    uint32_t pmd = (uint32_t)clamp(p[P_SYPMD], 0, 4), pfq = pmd == 0u || pmd >= 3u, ppw = pmd == 1u || pmd == 4u,
             pfl = pmd >= 2u;
    if (!S || n > CTL)
        return;
    if (pme)                                             /* (custom) POLY MOD's ENV: ENV 2 (voice.c ticks it) */
        e15 = v->env2 >> 9;
    S->ma.bright = S->mb.bright = (uint8_t)clamp(p[P_SYMBRT], 0, 127);   /* (MODAL's, after Rings) */
    S->ma.pos = S->mb.pos = (uint8_t)clamp(p[P_SYMPOS], 0, 127);
    if (drift) {                                         /* (custom, OberVave's VINTAGE) DRIFT: the card's tune and a walk */
        int32_t r = (int32_t)(noise32(&v->s[0]) >> 22) - 512;   /* (a step of the walk, +-512) */
        S->walk = clamp(S->walk + (r >> 5) - (S->walk >> 7), -1600, 1600);   /* (cents x 16, held near 0) */
        dct = (SY_CARD_CT[vi] * 16 + S->walk / 16) * drift / (127 * 16) + SY_CARD_CT[vi] * drift / 127;
    }
    if (pme && pfq)                                      /* (POLY MOD) ENV -> FREQ A: +-48 semitones at +-63 */
        dct += (e15 >> 5) * pme * 4800 / 64512;
    if (pme && ppw)                                      /* .. -> PW A: SHAPE A +-63 */
        sha += (e15 * pme) >> 7;
    if (dct)
        inca = cents_inc(m->pitch16, dct, m->fine);
    ga = mb == SY_BOFF ? 32767 : 32767 - mix;
    gb = mb == SY_BOFF ? 0 : mix;
    incb = mb != SY_BOFF ? cents_inc(clamp(m->pitch16 + p[P_E5] * 16, 0, 2047), (gb || p[P_SYXMOD]) ? p[P_E6] * 50 / 127 : 0,
                                  (gb || p[P_SYXMOD]) ? m->fine : 0) : 0u;
    if (mb != SY_BOFF && p[P_SYBLOW]) {                  /* (custom, the Prophet's LOW FREQ) B an LFO: 0.5 Hz x 2^(B TUNE
                                                          * + 24) / 12, the keys out */
        uint32_t st = (uint32_t)clamp(p[P_E5] + 24, 0, 48);
        incb = (uint32_t)(((uint64_t)((1ull << 31) / FS) * SY_SEMI[st % 12u]) >> 16) << (st / 12u);
    }
    if ((p[P_SYXMOD] || pob) && mb < SY_N) {             /* (custom, OberVave's X-MOD; the Prophet's POLY MOD) B first */
        int32_t xm = p[P_SYXMOD] * 1032;                 /* (127: B at full scale moves A a whole cycle) */
        int32_t kf = pfq ? (int32_t)((inca >> 10) * (uint32_t)pob / 64u) : 0;   /* (FM: +-2 x A's rate at 127) */
        uint32_t fm = S->fm;
        for (i = 0; i < n; i++)
            yb[i] = 0;
        sy_osc(mb, &v->ph[1], incb, p[P_E4] << 8, p[P_E2], yb, n, 32767, &S->subb, &S->nb, &S->nlb, &S->hb, 0, &S->mb);
        bs = yb[0];                                      /* (B's value this block: PW A, FILTER) */
        if (xm || kf) {                                  /* X-MOD: B on A's phase; FREQ A: A's phase moved by B's sum */
            for (i = 0; i < n; i++) {
                fm += (uint32_t)(int32_t)(((int64_t)yb[i] * kf) >> 5);
                pmb[i] = yb[i] * xm + (int32_t)fm;
            }
            pm = pmb;
        }
        S->fm = fm;
        if (pob && ppw)                                  /* OSC B -> PW A (once a block) */
            sha += (bs * pob) >> 7;
        brd = 1;
    }
    for (i = 0; i < n; i++)
        ya[i] = 0;
    sy_osc(ma, &v->ph[0], inca, sha, p[P_E2], ya, n, 32767, &sub, &v->s[0], &v->s[1], &v->s[2], pm, &S->ma);
    for (i = 0; i < n; i++)
        y[i] = mulq15(ya[i], ga);
    if (brd) {                                           /* (X-MOD, POLY MOD: B rendered above) */
        if (gb)
            for (i = 0; i < n; i++)
                y[i] += mulq15(yb[i], gb);
    } else if (mb != SY_BOFF && gb) {
        if (mb == SY_SYNC)
            sy_sync(&v->ph[1], incb, pa0, inca, p[P_E4] << 8, y, n, gb);
        else if (mb == SY_RING)
            sy_ring(&v->ph[1], incb, ya, p[P_E4] << 8, y, n, gb);
        else
            sy_osc(mb, &v->ph[1], incb, p[P_E4] << 8, p[P_E2], y, n, gb, &S->subb, &S->nb, &S->nlb, &S->hb, 0, &S->mb);
    } else if (mb != SY_BOFF) {
        v->ph[1] += incb * n;                            /* (B runs on, silent) */
    }
    v->ph[2] = sub;
    if (p[P_SYSUB]) {                                    /* (custom) SUB: a square one or two octaves below A */
        uint32_t is = inca >> (p[P_SYSUBO] ? 2 : 1), q = S->subph;
        int32_t sl = p[P_SYSUB] * 200;
        for (i = 0; i < n; i++, q += is)
            y[i] += mulq15(osc_pulse(q, is, 0x80000000u) << 1, sl);
        S->subph = q;
    }
    cm = (m->cutoff >> 5) + (((v->pitch16 - 60 * 16) * 9) >> 5);   /* the routings; half key tracking */
    if (pfl)                                             /* (POLY MOD) -> FILTER: ENV +-~5 octaves, B +-~4 */
        cm += ((e15 * pme) >> 12) + ((bs * pob) >> 13);   /* ((custom) ENV: >> 12, was >> 6: 32 x too far) */
    if (drift)
        cm += SY_CARD_CUT[vi] * drift / 64;              /* (DRIFT: the card's filter a little apart) */
    if (p[P_SYACC]) {                                    /* (303) the accent's envelope and the track's sweep */
        uint32_t k = (uint32_t)(t - trk) % NTRK;
        int32_t tb = 2 + p[P_SYADEC] * p[P_SYADEC] / 16;   /* ACC DEC: ~1.5 ms .. ~0.7 s */
        cm += (mulq15(S->acc, p[P_SYACC] * 2) + mulq15(sy_sweep[k], p[P_SYACC])) >> 1;
        S->acc -= S->acc / tb;
        if (vi == 0u)                                    /* (the sweep drains once a block, by voice 0's render) */
            sy_sweep[k] -= sy_sweep[k] / 400;
    }
    S->f.bl = (uint8_t)clamp(p[P_FBLND], 0, 127);        /* (custom) BLEND */
    flt_run(&S->f, 0, y, n, (uint32_t)clamp(p[P_FTYPE], 0, FT_N - 1), p[P_FCUT], p[P_FRES], p[P_FCHR], p[P_FDRV], cm);
    if (p[P_SYMUF]) {                                    /* (Devil Fish) MUFFLE: the VCA's soft clip, louder in */
        int32_t kn = 16000 - p[P_SYMUF] * 100;
        for (i = 0; i < n; i++)
            out[i] += voice_amp(soft_knee(y[i] >> 1, kn), m, i) << 1;
        return;
    }
    for (i = 0; i < n; i++)
        out[i] += voice_amp(soft_knee(y[i] >> 1, 16000), m, i) << 1;
}

static const param_desc_t SY_MOD_DESC[SY_N] = {
    {"SUB", 0, 0, 127, 0, F_PCT, 0}, {"WARP", 0, 0, 127, 0, F_PCT, 0}, {"ODD", 0, 0, 127, 0, F_PCT, 0},
    {"PITCH", 0, 0, 127, 0, F_PCT, 0}, {"PERC", 0, 0, 127, 0, F_PCT, 0}, {"DAMP", 0, 0, 127, 0, F_PCT, 0},
    {"SKEW", 0, 0, 127, 0, F_PCT, 0}, {"INV", 0, 0, 127, 0, F_PCT, 0}, {"SIDES", 0, 0, 127, 0, F_PCT, 0},
    {"SUB", 0, 0, 127, 0, F_PCT, 0}, {"SUB", 0, 0, 127, 0, F_PCT, 0}, {"DEPTH", 0, 0, 127, 0, F_PCT, 0},
    {"SUB", 0, 0, 127, 0, F_PCT, 0}, {"WARP", 0, 0, 127, 0, F_PCT, 0}};
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
    {"ACID LINE", {SY_303, 0, 0, 0, 0, 0, 0, 64}, {0, 52, 30, 20}, 60, 1, FX(20, 0, 45, 15),
     MORE(SYF(FT_ACID, 36, 110), {P_FCHR, 63}), TAG(PT_BASS)},   /* ((custom) the 303: its oscillator and filter) */
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
    /* (custom) ANALOG's, TRIO's, NOISE's and WHEEL's sounds, made here (those engines retired: their sounds load as these,
     * retire.c RETIRE_MAP); ANALOG's with its oscillators (SQR, PWM), its filter (ANLG) and its DETUNE in SYNTH's cents */
    {"SAW LEAD", {SY_VA, 0, 0, 1 + SY_VA, 0, 0, 30, 64}, {4, 70, 100, 50}, 20, 1, FX(0, 10, 45, 30),
     MORE(SYF(FT_ANLG, 90, 30), {P_FDRV, 10}), TAG(PT_LEAD)},
    {"SOFT PAD", {SY_VA, 0, 0, 1 + SY_VA, 0, 0, 51, 64}, {80, 90, 110, 95}, 10, 0, FX(0, 60, 20, 70),
     MORE(SYF(FT_ANLG, 60, 10)), TAG(PT_PAD)},
    {"SQR BASS", {SY_SQR, 0, 0, 0, 0, 0, 0, 0}, {0, 60, 40, 30}, 30, 1, FX(5, 0, 10, 10),
     MORE(SYF(FT_ANLG, 50, 70), {P_FDRV, 40}), TAG(PT_BASS)},
    {"PWM STR", {SY_PWM, 20, 80, SYB_PWM, 30, 0, 20, 50}, {60, 80, 110, 85}, 8, 0, FX(0, 50, 20, 60),
     MORE(SYF(FT_ANLG, 75, 20)), TAG(PT_PAD)},
    {"SUB BASS", {SY_WAVE, 0, 0, 0, 0, 0, 0, 0}, {0, 60, 100, 20}, 0, 1, FX(0, 0, 0, 10),
     MORE(SYF(FT_LPMG, 50, 0)), TAG(PT_BASS)},
    {"SYNTH PLUCK", {SY_VA, 0, 0, 1 + SY_VA, 0, 0, 20, 50}, {0, 88, 0, 60}, 55, 0, FX(0, 20, 50, 30),
     MORE(SYF(FT_ANLG, 30, 40)), TAG(PT_PLUCK)},
    {"SYNTH BRASS", {SY_VA, 0, 0, 1 + SY_VA, 0, 0, 25, 64}, {35, 70, 90, 45}, 40, 0, FX(0, 20, 20, 40),
     MORE(SYF(FT_ANLG, 45, 20), {P_FDRV, 10}), TAG(PT_LEAD)},
    {"REESE", {SY_VA, 0, 0, 1 + SY_VA, 0, 0, 101, 64}, {0, 64, 127, 30}, 10, 1, FX(10, 20, 5, 10),
     MORE(SYF(FT_ANLG, 52, 15), {P_LD_FLT, 8}), TAG(PT_BASS)},
    {"HARD SYNC", {SY_VA, 0, 0, SYB_SYNC, 0, 12, 0, 100}, {2, 70, 96, 40}, 16, 1, FX(0, 15, 40, 30),
     MORE(SYF(FT_LPMG, 88, 24), {P_GLIDE, 20}), TAG(PT_LEAD)},
    {"SUB SAW", {SY_VA, 0, 80, 0, 0, 0, 0, 0}, {0, 60, 110, 22}, 20, 1, FX(0, 0, 5, 10),
     MORE(SYF(FT_ANLG, 70, 22), {P_FDRV, 6}), TAG(PT_BASS)},
    {"SINE KEY", {SY_WAVE, 0, 0, 1 + SY_WAVE, 0, 12, 0, 30}, {2, 80, 30, 70}, 0, 0, FX(0, 30, 25, 40),
     MORE(SYF(FT_OFF, 127, 0)), TAG(PT_KEYS)},
    {"SYN TOM", {SY_WAVE, 0, 0, 0, 0, 0, 0, 0}, {0, 76, 0, 60}, 0, 0, FX(0, 0, 10, 45),
     MORE(SYF(FT_OFF, 127, 0), {P_ED_PIT, 63}), TAG(PT_DRUM)},
    {"SYNC LEAD", {SY_VA, 0, 0, SYB_SYNC, 30, 9, 0, 110}, {2, 70, 100, 40}, 18, 1, FX(0, 10, 40, 25),
     MORE(SYF(FT_LPMG, 82, 30)), TAG(PT_LEAD)},
    {"RING BELL", {SY_WAVE, 0, 0, SYB_RING, 127, 18, 0, 127}, {0, 92, 0, 80}, 0, 0, FX(0, 20, 35, 55),
     MORE(SYF(FT_SVF, 96, 30)), TAG(PT_KEYS)},
    {"CHIP CHOIR", {SY_VA, 100, 0, 1 + SY_VA, 80, 12, 20, 50}, {70, 90, 110, 85}, 30, 0, FX(0, 50, 20, 65),
     MORE(SYF(FT_LPMG, 62, 40)), TAG(PT_PAD)},
    {"FAT BASS", {SY_VA, 0, 0, 1 + SY_VA, 0, -12, 22, 64}, {0, 62, 60, 25}, 40, 1, FX(10, 0, 10, 10),
     MORE(SYF(FT_LPMG, 62, 45)), TAG(PT_BASS)},
    {"DIRTY BASS", {SY_VA, 64, 0, 1 + SY_VA, 64, -12, 20, 64}, {0, 50, 50, 20}, 35, 1, FX(72, 0, 5, 5),
     MORE(SYF(FT_LPMG, 62, 85), {P_FDRV, 80}), TAG(PT_BASS)},
    {"COLLIDER", {SY_VA, 0, 0, SYB_RING, 127, 23, 0, 127}, {0, 70, 0, 55}, 0, 0, FX(8, 10, 50, 50),
     MORE(SYF(FT_SVF, 60, 50), {P_ED_PIT, 36}), TAG(PT_FX)},
    {"WIND", {SY_NOISE, 70, 0, 0, 0, 0, 0, 0}, {90, 90, 110, 90}, 0, 0, FX(0, 30, 20, 80),
     MORE(SYF(FT_SVF, 100, 120), {P_LD_FLT, 20}), TAG(PT_FX)},
    {"RAIN", {SY_NOISE, 20, 0, 0, 0, 0, 0, 0}, {50, 90, 127, 70}, 0, 0, FX(0, 0, 20, 70),
     MORE(SYF(FT_SVF, 88, 40)), TAG(PT_FX)},
    {"ARCADE", {SY_NOISE, 0, 127, 0, 0, 0, 0, 0}, {0, 80, 0, 50}, 0, 0, FX(0, 0, 10, 20),
     MORE(SYF(FT_OFF, 127, 0)), TAG(PT_FX)},
    {"OCEAN", {SY_NOISE, 60, 0, 0, 0, 0, 0, 0}, {100, 90, 120, 110}, 0, 0, FX(0, 30, 10, 90),
     MORE(SYF(FT_LPMG, 60, 20), {P_LD_FLT, 40}), TAG(PT_FX)},
    {"METAL", {SY_VA, 127, 0, SYB_RING, 127, 7, 0, 127}, {0, 75, 40, 60}, 10, 0, FX(0, 20, 30, 45),
     MORE(SYF(FT_SVF, 100, 40)), TAG(PT_FX)},
    {"FULL ORGAN", {SY_ORGAN, 127, 0, 0, 0, 0, 0, 0}, {0, 64, 127, 45}, 0, 0, FX(20, 40, 10, 35),
     MORE(SYF(FT_OFF, 127, 0)), TAG(PT_KEYS)},
    {"JAZZ PERC", {SY_ORGAN, 34, 30, 0, 0, 0, 0, 0}, {0, 64, 127, 40}, 0, 0, FX(0, 30, 0, 25),
     MORE(SYF(FT_OFF, 127, 0)), TAG(PT_KEYS)},
    {"GOSPEL", {SY_ORGAN, 51, 70, 0, 0, 0, 0, 0}, {0, 64, 127, 45}, 0, 0, FX(10, 50, 0, 40),
     MORE(SYF(FT_OFF, 127, 0)), TAG(PT_KEYS)},
    {"SOFT FLUTE", {SY_ORGAN, 9, 0, 0, 0, 0, 0, 0}, {0, 64, 127, 55}, 0, 0, FX(0, 40, 15, 55),
     MORE(SYF(FT_OFF, 127, 0)), TAG(PT_KEYS)},
    {"ROCK DRIVE", {SY_ORGAN, 60, 0, 0, 0, 0, 0, 0}, {0, 64, 127, 40}, 0, 0, FX(35, 40, 10, 25),
     MORE(SYF(FT_LPMG, 110, 0), {P_FDRV, 90}), TAG(PT_KEYS)},
    {"CATHEDRAL", {SY_ORGAN, 102, 0, 0, 0, 0, 0, 0}, {48, 64, 127, 72}, 0, 0, FX(0, 0, 0, 110),
     MORE(SYF(FT_OFF, 127, 0)), TAG(PT_KEYS)},
    {"PEDAL BASS", {SY_ORGAN, 119, 0, 0, 0, 0, 0, 0}, {0, 64, 127, 30}, 0, 1, FX(0, 0, 0, 15),
     MORE(SYF(FT_OFF, 127, 0)), TAG(PT_BASS)},
    /* (custom) the new pieces: the 303 page, MODAL, X-MOD (OberVave's), DRIFT */
    {"DEVIL ACID", {SY_303, 127, 0, 0, 0, 0, 0, 64}, {0, 58, 20, 20}, 66, 1, FX(30, 0, 45, 15),
     MORE(SYF(FT_ACID, 34, 118), {P_SYACC, 100}), TAG(PT_BASS)},   /* ((custom) the 303's square) */
    {"STRUCK BAR", {SY_MODAL, 64, 40, 0, 0, 0, 0, 0}, {0, 90, 0, 60}, 0, 0, FX(0, 0, 10, 35),
     MORE(SYF(FT_OFF, 127, 0)), TAG(PT_PLUCK)},
    {"MODAL BELL", {SY_MODAL, 127, 10, 0, 0, 0, 0, 0}, {0, 100, 0, 90}, 0, 0, FX(0, 20, 30, 60),
     MORE(SYF(FT_OFF, 127, 0)), TAG(PT_KEYS)},
    {"PLUCK STRING", {SY_MODAL, 0, 60, 0, 0, 0, 0, 0}, {0, 80, 0, 50}, 0, 0, FX(0, 10, 25, 40),
     MORE(SYF(FT_LPMG, 110, 0)), TAG(PT_PLUCK)},
    {"XMOD BELL", {SY_WAVE, 0, 0, 1 + SY_WAVE, 0, 19, 0, 0}, {0, 95, 0, 80}, 0, 0, FX(0, 20, 40, 50),
     MORE(SYF(FT_LPMG, 100, 0), {P_SYXMOD, 70}), TAG(PT_KEYS)},
    {"VINTAGE POLY", {SY_VA, 0, 0, 1 + SY_VA, 0, 0, 12, 64}, {10, 80, 100, 50}, 30, 0, FX(0, 40, 20, 40),
     MORE(SYF(FT_OB24, 70, 20), {P_SYDRFT, 90}), TAG(PT_KEYS)},
    /* (custom, after Plaits) FOLD CHORD SWARM */
    {"SUPERSAW", {SY_SWARM, 70, 90, 0, 0, 0, 0, 0}, {2, 80, 110, 60}, 20, 0, FX(0, 30, 30, 50),
     MORE(SYF(FT_LPMG, 108, 10)), TAG(PT_LEAD)},
    {"SWARM PAD", {SY_SWARM, 40, 127, 1 + SY_WAVE, 20, 12, 10, 30}, {90, 90, 120, 100}, 8, 0, FX(0, 60, 20, 80),
     MORE(SYF(FT_OB12, 80, 20), {P_SYDRFT, 60}), TAG(PT_PAD)},
    {"STRING MACH", {SY_CHORD, 35, 0, 0, 0, 0, 0, 0}, {60, 90, 120, 90}, 10, 0, FX(0, 90, 15, 60),
     MORE(SYF(FT_LPMG, 88, 10)), TAG(PT_PAD)},
    {"MINOR STAB", {SY_CHORD, 50, 40, 0, 0, 0, 0, 0}, {0, 60, 0, 40}, 30, 0, FX(0, 10, 50, 40),
     MORE(SYF(FT_SK, 90, 40)), TAG(PT_KEYS)},
    {"WEST FOLD", {SY_FOLD, 70, 30, 0, 0, 0, 0, 0}, {0, 75, 40, 45}, 40, 0, FX(0, 0, 25, 35),
     MORE(SYF(FT_LPMG, 115, 20)), TAG(PT_LEAD)},
    {"FOLD BASS", {SY_FOLD, 40, 90, 0, 0, 0, 0, 0}, {0, 55, 60, 25}, 50, 1, FX(10, 0, 5, 10),
     MORE(SYF(FT_LPMG, 95, 30)), TAG(PT_BASS)},
#include "synth_obv.h"         /* (custom) after OberVave's factory programs (tools/obv_presets.py) */
};
_Static_assert(NELEM(SYNTH_PRESETS) <= 127, "SYNTH: the presets past 127 (the editor's LIST2, MIDI: 7 bits)");
/* (custom) presets' LFO and ARP (core.h preset_ext_t): FOLD CHORD SWARM's six (SY_PL0 ..), OberVave's programs' as
 * their own (tools/obv_presets.py). A preset with its ARP on brings it (MODE .. ORDER: params.c param_kept_by) */
#define SY_OBV0 ((uint8_t)(NELEM(SYNTH_PRESETS) - SY_OBV_N))
#define SY_PL0 ((uint8_t)(SY_OBV0 - 6u))
static const preset_ext_t SYNTH_PEXT[] = {
    {SY_PL0 + 0u, {{P_LRATE, 72}, {P_LWAVE, 0}, {P_LD_PIT, 6}, {P_LFADE, 60}}},                 /* SUPERSAW: vibrato */
    {SY_PL0 + 1u, {{P_LRATE, 30}, {P_LWAVE, 1}, {P_LD_FLT, 14}}},                               /* SWARM PAD: a sweep */
    {SY_PL0 + 2u, {{P_LRATE, 84}, {P_LWAVE, 0}, {P_LD_PIT, 4}}},                                /* STRING MACH: ensemble */
    {SY_PL0 + 3u, {{P_AMODE, 1}, {P_ARATE, 2}, {P_AOCT, 2}, {P_AGATE, 40}}},                     /* MINOR STAB: 1/16 up */
    {SY_PL0 + 4u, {{P_LRATE, 50}, {P_LWAVE, 1}, {P_LD_SHP, 22}}},                               /* WEST FOLD: the folds */
    {SY_PL0 + 5u, {{P_AMODE, 1}, {P_ARATE, 2}, {P_AOCT, 1}, {P_AGATE, 55}}},                     /* FOLD BASS: 1/16 up */
#define SY_OBV_PEXT
#include "synth_obv.h"
#undef SY_OBV_PEXT
};
#undef SYF

static const engine_t ENG_SYNTH = {
    .name = "SYNTH",
    .page_title = {"OSC A", "OSC B"},
    .edit = {
        {"OSC A", N_SY_OSC, 0, SY_N - 1, SY_VA, F_ENUM, 0},
        {"SHAPE A", 0, 0, 127, 0, F_PCT, 0},
        {"MOD", 0, 0, 127, 0, F_PCT, 0},
        {"OSC B", N_SY_OSCB, 0, SYB_N - 1, 0, F_ENUM, 0},
        {"SHAPE B", 0, 0, 127, 0, F_PCT, 0},
        {"B TUNE", 0, -24, 24, 0, F_INT, U_ST},
        {"DETUNE", 0, 0, 127, 0, F_INT, U_CT},
        {"MIX", 0, 0, 127, 64, F_PCT, 0},
    },
    .presets = SYNTH_PRESETS,
    .npresets = NELEM(SYNTH_PRESETS),
    .npext = NELEM(SYNTH_PEXT),
    .pext = SYNTH_PEXT,
    .note_on = synth_note_on,
    .render = synth_render,
    .desc = synth_desc,
    .knob = {P_E1, P_FCUT, P_FRES, P_E7},
};
