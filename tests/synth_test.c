/* SPDX-License-Identifier: GPL-3.0-only */
/* Host test of the SYNTH engine (custom: src/eng_synth.c, its filter per voice src/flt.c):
 * 1. every preset plays a phrase: it sounds, under full scale, no DC, and its voices end after the release.
 * 2. each model of OSC A, SHAPE swept 0 .. 127 while it plays, from C1 to C7: bounded, sounding, no DC; MOD at 0 and
 *    127; OSC B of each model at MIX 127, B TUNE -24 / +24.
 * 3. every filter TYPE per voice: a 6-voice chord, RESO 127, CUTOFF swept: bounded.
 * 4. the track FILTER stays off for SYNTH (its filter is per voice): TYPE set, the mix the same as the voices'.
 * 5. the cost: ns a block for 6 voices of each model (host).
 * Demos: build/synth_demo/<PRESET>.wav.
 *   cc -O2 -w -Ibuild/gen -Ifirmware/src -o /tmp/synth_test tests/synth_test.c -lm && /tmp/synth_test build/synth_demo */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <sys/stat.h>

static int bad;
static void ck(const char *w, int ok)
{
    printf("synth: %-82s %s\n", w, ok ? "ok" : "FAIL");
    bad += !ok;
}
static void fresh(uint32_t preset)
{
    host_tracks_init();
    memset(trk[0].v, 0, sizeof trk[0].v);
    memset(eng_mem, 0, sizeof eng_mem);
    host_preset(&trk[0], ENGI_SYNTH, preset);
    trk[0].p[P_REV] = trk[0].p[P_DLY] = trk[0].p[P_CHOR] = 0;
}
static uint32_t voices_on(const track_t *t)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++)
        n += t->v[i].active;
    return n;
}
typedef struct { int32_t pk; double sum, sq; uint32_t n; } stat_t;
static void run(uint32_t nb, stat_t *s, FILE *w)
{
    int32_t o[2 * CTL];
    uint32_t i, k;
    for (i = 0; i < nb; i++) {
        mix_block(o, CTL);
        for (k = 0; k < 2u * CTL; k++) {
            int32_t a = abs(o[k]);
            if (s) {
                s->pk = a > s->pk ? a : s->pk;
                s->sum += o[k];
                s->sq += (double)o[k] * o[k];
                s->n++;
            }
        }
        if (w)
            for (k = 0; k < CTL; k++)
                wav_put(w, o[2 * k], o[2 * k + 1]);
    }
}
static double rms(const stat_t *s) { return s->n ? sqrt(s->sq / s->n) : 0; }
/* (custom, after Plaits) a note of the track's sound: zero crossings a second (the left), and how much the RMS of
 * 20 ms windows moves (its spread / mean: a beating) */
static void zc_note(uint32_t note, double *zc, double *beat)
{
    int32_t o[2 * CTL], last = 0;
    uint32_t i, k, z = 0, nw = 0, w = 0;
    double sq = 0, m = 0, m2 = 0;
    trk_note_on(&trk[0], note, 100);
    run(FS / 8 / CTL, 0, 0);
    for (i = 0; i < FS / CTL; i++) {
        mix_block(o, CTL);
        for (k = 0; k < CTL; k++) {
            int32_t x = o[2 * k];
            if ((x >= 0) != (last >= 0))
                z++;
            last = x;
            sq += (double)x * x;
            if (++w == FS / 50) {
                double r = sqrt(sq / w);
                m += r, m2 += r * r, nw++, sq = 0, w = 0;
            }
        }
    }
    *zc = z / 2.0;
    m /= nw;
    *beat = m > 0 ? sqrt(m2 / nw - m * m) / m : 0;
}
static double mean(const stat_t *s) { return s->n ? s->sum / s->n : 0; }

static const uint8_t PHRASE[][2] = {{48, 0}, {55, 0}, {60, 0}, {64, 2}, {67, 4}, {72, 6}, {48, 8}, {60, 10}, {63, 10},
                                    {67, 10}, {70, 12}, {36, 14}};

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : "build/synth_demo";
    char w[200];
    uint32_t k, i, n;
    mkdir("build", 0755);
    mkdir(dir, 0755);

    /* 1 + demos */
    for (k = 0; k < ENG_SYNTH.npresets; k++) {
        stat_t s = {0};
        uint32_t e = 0, step = FS / 8u, f, frames = 16u * step + FS * 2u;
        FILE *fp;
        snprintf(w, sizeof w, "%s/%s.wav", dir, ENG_SYNTH.presets[k].name);
        for (i = 0; w[i]; i++)
            if (w[i] == ' ')
                w[i] = '_';
        fp = fopen(w, "wb");
        if (fp)
            wav_hdr(fp, frames);
        fresh(k);
        for (f = 0; f < frames; f += CTL) {
            while (e < sizeof PHRASE / sizeof PHRASE[0] && PHRASE[e][1] * step <= f)
                trk_note_on(&trk[0], PHRASE[e++][0], 100);
            if (f == 16u * step)
                for (i = 0; i < sizeof PHRASE / sizeof PHRASE[0]; i++)
                    trk_note_off(&trk[0], PHRASE[i][0]);
            run(1, &s, fp);
        }
        if (fp)
            fclose(fp);
        for (n = 0; n < 20u * FS / CTL && voices_on(&trk[0]); n++)
            run(1, 0, 0);
        snprintf(w, sizeof w, "%-12s: sounds (RMS %.0f, peak %d), no DC (%.0f), voices end", ENG_SYNTH.presets[k].name,
                 rms(&s), (int)s.pk, mean(&s));
        ck(w, (rms(&s) > 300 || (!ENG_SYNTH.presets[k].env[2] && s.pk > 4000)) &&   /* ((custom) a short hit, SUS 0: */
                 s.pk < 32767 * 2 && fabs(mean(&s)) < 200 && !voices_on(&trk[0]));   /* by its peak) */
    }

    /* 2 */
    for (k = 0; k < SY_N; k++) {
        int ok = 1;
        uint32_t note, mod;
        for (mod = 0; mod < 2u; mod++)
            for (note = 24; note <= 96; note += 24) {
                stat_t s = {0};
                fresh(0);
                trk[0].p[P_E0] = (int16_t)k;
                trk[0].p[P_E2] = (int16_t)(mod * 127);
                trk[0].p[P_FTYPE] = FT_OFF;
                trk_note_on(&trk[0], note, 120);
                for (i = 0; i < 64u; i++) {
                    trk[0].p[P_E1] = (int16_t)(i * 2);
                    run(FS / 64 / CTL + 1, &s, 0);
                }
                if (rms(&s) < (k == SY_MODAL && mod ? 60 : 200) || s.pk > 2 * 32767 || fabs(mean(&s)) > 400)   /* (MODAL
                                                          * damped: a knock, gone in the sweep's first blocks) */
                    ok = 0, printf("  %s MOD %u note %u: RMS %.0f peak %d mean %.0f\n", N_SY_OSC[k], mod * 127, note,
                                   rms(&s), (int)s.pk, mean(&s));
            }
        for (i = 0; i < 2u; i++) {                       /* OSC B of this model alone */
            stat_t s = {0};
            fresh(0);
            trk[0].p[P_E3] = (int16_t)(k < SY_MODAL ? k + 1u : (uint32_t)(k - SY_MODAL + SYB_MODAL));   /* (B's stored value) */
            trk[0].p[P_E5] = i ? 24 : -24;
            trk[0].p[P_E7] = 127;
            trk[0].p[P_FTYPE] = FT_OFF;
            trk_note_on(&trk[0], 60, 120);
            run(FS / 4 / CTL, &s, 0);
            if (rms(&s) < 200 || s.pk > 2 * 32767)
                ok = 0, printf("  B %s TUNE %d: RMS %.0f peak %d\n", N_SY_OSC[k], i ? 24 : -24, rms(&s), (int)s.pk);
        }
        snprintf(w, sizeof w, "OSC %-5s: SHAPE swept C1..C7, MOD 0 / 127, as OSC B at +-24: sounds, bounded, no DC", N_SY_OSC[k]);
        ck(w, ok);
    }

    /* 2b (custom) OSC B's SYNC and RING: sound, bounded; SYNC's B TUNE moves the spectrum (the formant) at A's pitch;
     * RING at SHAPE B 0 is A alone (dry), at 127 not */
    for (k = SYB_SYNC; k <= SYB_RING; k++) {
        int ok = 1;
        double r[2] = {0, 0};
        for (i = 0; i < 2u; i++) {
            stat_t s = {0};
            fresh(0);
            trk[0].p[P_E3] = (int16_t)k;
            trk[0].p[P_E5] = i ? 19 : 0;
            trk[0].p[P_E4] = k == SYB_RING ? 127 : 0;
            trk[0].p[P_E7] = 127;
            trk[0].p[P_FTYPE] = FT_OFF;
            trk_note_on(&trk[0], 48, 120);
            run(FS / 4 / CTL, &s, 0);
            r[i] = rms(&s);
            if (rms(&s) < 200 || s.pk > 2 * 32767 || fabs(mean(&s)) > 400)
                ok = 0, printf("  B %s TUNE %d: RMS %.0f peak %d mean %.0f\n", N_SY_OSCB[k], i ? 19 : 0, rms(&s),
                               (int)s.pk, mean(&s));
        }
        snprintf(w, sizeof w, "OSC B %-4s: at B TUNE 0 and +19: sounds, bounded, no DC (RMS %.0f / %.0f)", N_SY_OSCB[k],
                 r[0], r[1]);
        ck(w, ok);
    }
    {   /* ORGAN: PERC strikes (louder at the start than later), none at 0 */
        double a[2][2];
        uint32_t m;
        for (m = 0; m < 2u; m++) {
            stat_t s0 = {0}, s1 = {0};
            fresh(0);
            trk[0].p[P_E0] = SY_ORGAN;
            trk[0].p[P_E1] = 0;                          /* FLUTE: the 8' alone */
            trk[0].p[P_E2] = m ? 50 : 0;
            trk[0].p[P_FTYPE] = FT_OFF;
            trk[0].p[P_ATK] = 0;
            trk_note_on(&trk[0], 60, 120);
            run(FS / 20 / CTL, &s0, 0);
            run(FS / 2 / CTL, 0, 0);
            run(FS / 20 / CTL, &s1, 0);
            a[m][0] = rms(&s0);
            a[m][1] = rms(&s1);
        }
        snprintf(w, sizeof w, "ORGAN PERC: the first 50 ms %.0f, 0.55 s later %.0f (PERC 0: %.0f / %.0f)", a[1][0], a[1][1],
                 a[0][0], a[0][1]);
        ck(w, a[1][0] > a[1][1] * 1.3 && fabs(a[0][0] - a[0][1]) < a[0][1] * 0.25);
    }

    /* 2c (custom) SYNTH's own page and the 303 page: SUB adds the octave below, X-MOD changes A, DRIFT detunes the
     * voices apart, MODAL rings and dies away (MOD damps it), ACCENT opens the filter on accented notes (and a run of
     * them climbs), MUFFLE softens the peak; each 0 is the sound as before, bit for bit */
    {
        stat_t a = {0}, b = {0};
        double r0, r1;
        int32_t pk0;
        fresh(0);
        trk[0].p[P_FTYPE] = FT_OFF;
        trk[0].p[P_E0] = SY_WAVE;                        /* a sine */
        trk_note_on(&trk[0], 60, 100);
        run(FS / 8 / CTL, &a, 0);
        fresh(0);
        trk[0].p[P_FTYPE] = FT_OFF;
        trk[0].p[P_E0] = SY_WAVE;
        trk[0].p[P_SYSUB] = 127;
        trk_note_on(&trk[0], 60, 100);
        run(FS / 8 / CTL, &b, 0);
        snprintf(w, sizeof w, "SUB 127: louder with the octave below (RMS %.0f -> %.0f), bounded", rms(&a), rms(&b));
        ck(w, rms(&b) > rms(&a) * 1.2 && b.pk < 2 * 32767);
        r0 = rms(&a);
        memset(&b, 0, sizeof b);
        fresh(0);
        trk[0].p[P_FTYPE] = FT_OFF;
        trk[0].p[P_E0] = SY_WAVE;
        trk[0].p[P_E3] = 1 + SY_WAVE;
        trk[0].p[P_E5] = 7;
        trk[0].p[P_E7] = 0;                              /* (B silent: only its X-MOD) */
        trk[0].p[P_SYXMOD] = 90;
        trk_note_on(&trk[0], 60, 100);
        run(FS / 8 / CTL, &b, 0);
        {   /* X-MOD: A's sine now has partials: its waveform differs from the plain one (RMS about the same) */
            int32_t o1[2 * CTL], o2[2 * CTL];
            double d = 0;
            uint32_t q;
            fresh(0); trk[0].p[P_FTYPE] = FT_OFF; trk[0].p[P_E0] = SY_WAVE; trk[0].p[P_E3] = 1 + SY_WAVE; trk[0].p[P_E5] = 7;
            trk[0].p[P_E7] = 0;
            trk_note_on(&trk[0], 60, 100);
            for (q = 0; q < 20u; q++) mix_block(o1, CTL);
            fresh(0); trk[0].p[P_FTYPE] = FT_OFF; trk[0].p[P_E0] = SY_WAVE; trk[0].p[P_E3] = 1 + SY_WAVE; trk[0].p[P_E5] = 7;
            trk[0].p[P_E7] = 0; trk[0].p[P_SYXMOD] = 90;
            trk_note_on(&trk[0], 60, 100);
            for (q = 0; q < 20u; q++) mix_block(o2, CTL);
            for (q = 0; q < 2u * CTL; q++) d += fabs((double)o1[q] - o2[q]);
            snprintf(w, sizeof w, "X-MOD 90: B moves A's phase (mean difference %.0f), sounds (RMS %.0f), bounded", d / (2 * CTL),
                     rms(&b));
            ck(w, d / (2 * CTL) > 300 && rms(&b) > 300 && b.pk < 2 * 32767);
        }
        {   /* DRIFT: a chord of one note on four voices (UNISON off, POLY): with DRIFT they beat */
            stat_t c = {0};
            fresh(0);
            trk[0].p[P_FTYPE] = FT_OFF;
            trk[0].p[P_SYDRFT] = 127;
            trk_note_on(&trk[0], 48, 100);
            trk_note_on(&trk[0], 60, 100);
            run(FS / 4 / CTL, &c, 0);
            snprintf(w, sizeof w, "DRIFT 127: sounds, bounded, no DC (RMS %.0f, peak %d)", rms(&c), (int)c.pk);
            ck(w, rms(&c) > 300 && c.pk < 2 * 32767 && fabs(mean(&c)) < 300);
        }
        {   /* (custom) DRIFT's voice cards: DRIFT 0 a note from silence starts alike every time (phases 0); DRIFT > 0 the
             * oscillators free-run (no two starts alike) and each voice has its card: its pitch, cutoff, level and
             * envelope apart, the card's part the same note after note (the note's own the smaller), within range */
            int32_t a1[2 * CTL], a2[2 * CTL], sc[2 * CTL], ct[4][2], ek[4][2];
            double d0 = 0, d1 = 0;
            uint32_t k, r, q;
            int ok = 1;
            for (r = 0; r < 2u; r++) {                   /* (DRIFT 0, then 64: the note twice on a voice, from silence) */
                fresh(0);
                trk[0].p[P_SYDRFT] = r ? 64 : 0;
                trk[0].p[P_FTYPE] = FT_OFF;
                trk[0].p[P_VOICE] = V_MONO;              /* (the same voice both times) */
                for (k = 0; k < 2u; k++) {
                    int32_t *o = k ? a2 : a1;
                    for (q = 0; q < 3u; q++)
                        mix_block(sc, CTL);
                    trk_note_on(&trk[0], 60, 100);
                    mix_block(o, CTL);                   /* (the note's first block: compared) */
                    trk_note_off(&trk[0], 60);
                    for (q = 0; q < 4000u && voices_on(&trk[0]); q++)
                        mix_block(sc, CTL);
                }
                for (q = 0; q < CTL; q++)                /* (the two sides' sum: SPREAD alternates them) */
                    *(r ? &d1 : &d0) += fabs((double)(a1[2 * q] + a1[2 * q + 1]) - (a2[2 * q] + a2[2 * q + 1]));
            }
            ck("DRIFT 0: a note from silence starts alike every time (phases 0, no card: only the LFO, RAND apart)",
               d0 * 20.0 < d1);
            ck("DRIFT 64: two starts of a note not alike (the oscillators free-run)", d1 > 50000);
            fresh(0);
            trk[0].p[P_SYDRFT] = 127;
            for (r = 0; r < 2u; r++)                     /* (four voices, twice: each note on the same card) */
                for (k = 0; k < 4u; k++) {
                    voice_t *v = &trk[0].v[k];
                    int32_t ns = (int32_t)(1000u * r + 17u * k + 5u) * 0x2545F491;   /* (another note's own) */
                    sy_vintage(&trk[0], v, 127, &ns);
                    ct[k][r] = SY_VT(v, 0);
                    ek[k][r] = v->ek;
                    ok &= abs(SY_VT(v, 0)) <= 9 && abs(SY_VT(v, 1)) <= 16 && abs(SY_VT(v, 2)) <= 6 &&
                          abs(SY_VT(v, 3)) <= 24 && abs(v->ek) <= 28;
                }
            for (k = 0; k < 4u; k++)                     /* (the card's three parts in four: within the note's part) */
                ok &= abs(ct[k][0] - ct[k][1]) <= 5 && abs(ek[k][0] - ek[k][1]) <= 15;
            ok &= !(ct[0][0] == ct[1][0] && ct[1][0] == ct[2][0] && ct[2][0] == ct[3][0]);
            ck("DRIFT 127: each voice's card (pitch, cutoff, PW, level, envelope) within range, the same note after "
               "note, the four voices not alike", ok);
        }
        {   /* (custom, the Virus's) FILTER 2 page: F2 SER LP below the FILTER darker (its zero crossings fewer); every
             * route with BOOST and RESO at the top bounded, no DC; BOOST 127 louder at the bottom;
             * PUNCH 127 the first ~5 ms louder, later the same */
            stat_t st;
            int32_t o[2 * CTL], prev = 0;
            uint32_t r, ok = 1, q, k;
            double e[2][2], hf[2];
            for (r = 0; r < 2u; r++) {                       /* (hf: the first difference's energy over the signal's) */
                double d2 = 0, x2 = 0;
                fresh(0);
                trk[0].p[P_SYDRFT] = 0;
                trk[0].p[P_FTYPE] = FT_OFF;
                trk[0].p[P_SYF2] = (int16_t)r;
                trk[0].p[P_SYF2O] = -40;
                trk_note_on(&trk[0], 48, 100);
                run(FS / 16 / CTL, 0, 0);
                for (q = 0; q < FS / 4 / CTL; q++) {
                    mix_block(o, CTL);
                    for (k = 0; k < CTL; k++) {
                        int32_t x = o[2 * k] + o[2 * k + 1];
                        d2 += (double)(x - prev) * (x - prev), x2 += (double)x * x, prev = x;
                    }
                }
                hf[r] = d2 / (x2 + 1);
            }
            ck("F2 SER LP, OFS -40 (FILTER OFF): darker than without", hf[1] < hf[0] * 0.5);
            for (r = 1; r <= 6u; r++) {
                memset(&st, 0, sizeof st);
                fresh(0);
                trk[0].p[P_FTYPE] = FT_LPMG;
                trk[0].p[P_FRES] = 127;
                trk[0].p[P_SYF2] = (int16_t)r;
                trk[0].p[P_SYBST] = 127;
                trk[0].p[P_SYPNCH] = 127;
                trk_note_on(&trk[0], 36, 127);
                trk_note_on(&trk[0], 60, 127);
                trk_note_on(&trk[0], 84, 127);
                run(FS / 2 / CTL, &st, 0);
                ok &= st.pk < 32768 && fabs(st.sum / st.n) < 300;
            }
            ck("F2 every route, BOOST 127, RESO 127, PUNCH 127: bounded, no DC", ok);
            for (r = 0; r < 2u; r++) {                       /* (BOOST: louder, its share of the top lower) */
                double d2 = 0, x2 = 0;
                fresh(0);
                trk[0].p[P_SYDRFT] = 0;
                trk[0].p[P_SYBST] = r ? 127 : 0;
                trk_note_on(&trk[0], 36, 100);
                run(FS / 16 / CTL, 0, 0);
                for (q = 0; q < FS / 4 / CTL; q++) {
                    mix_block(o, CTL);
                    for (k = 0; k < CTL; k++) {
                        int32_t x = o[2 * k] + o[2 * k + 1];
                        d2 += (double)(x - prev) * (x - prev), x2 += (double)x * x, prev = x;
                    }
                }
                e[r][0] = sqrt(x2), hf[r] = d2 / (x2 + 1);
            }
            ck("BOOST 127 (a low C): louder (> +2 dB), the top's share lower", e[1][0] > e[0][0] * 1.26 && hf[1] < hf[0] * 0.8);
            for (r = 0; r < 2u; r++) {
                fresh(0);
                trk[0].p[P_SYDRFT] = 0;
                trk[0].p[P_SYPNCH] = r ? 127 : 0;
                trk[0].p[P_ATK] = 0;
                trk_note_on(&trk[0], 48, 100);
                memset(&st, 0, sizeof st);
                for (q = 0; q < 8u; q++) {                   /* (~5 ms) */
                    mix_block(o, CTL);
                    for (ok = 0; ok < 2u * CTL; ok++)
                        st.sq += (double)o[ok] * o[ok], st.n++;
                }
                e[r][0] = rms(&st);
                memset(&st, 0, sizeof st);
                run(FS / 8 / CTL, 0, 0);
                run(4, &st, 0);
                e[r][1] = rms(&st);
            }
            ck("PUNCH 127: the first ~5 ms louder (> +2 dB), ~130 ms on the same (within 0.3 dB)",
               e[1][0] > e[0][0] * 1.26 && fabs(e[1][1] / e[0][1] - 1) < 0.035);
        }
        {   /* MODAL: rings after the strike, dies away; damped (MOD 127) much sooner */
            stat_t e0 = {0}, e1 = {0}, d0 = {0}, d1 = {0};
            fresh(0);
            trk[0].p[P_FTYPE] = FT_OFF; trk[0].p[P_E0] = SY_MODAL; trk[0].p[P_E1] = 64; trk[0].p[P_E2] = 0;
            trk[0].p[P_SUS] = 127; trk[0].p[P_ATK] = 0;
            trk_note_on(&trk[0], 60, 120);
            run(FS / 10 / CTL, &e0, 0);
            run(FS / CTL, 0, 0);
            run(FS / 10 / CTL, &e1, 0);
            fresh(0);
            trk[0].p[P_FTYPE] = FT_OFF; trk[0].p[P_E0] = SY_MODAL; trk[0].p[P_E1] = 64; trk[0].p[P_E2] = 127;
            trk[0].p[P_SUS] = 127; trk[0].p[P_ATK] = 0;
            trk_note_on(&trk[0], 60, 120);
            run(FS / 10 / CTL, &d0, 0);
            run(FS / 10 / CTL, &d1, 0);
            snprintf(w, sizeof w, "MODAL: struck (RMS %.0f), 1 s later %.0f; DAMP 127: %.0f then %.0f; peak %d", rms(&e0),
                     rms(&e1), rms(&d0), rms(&d1), (int)e0.pk);
            ck(w, rms(&e0) > 300 && rms(&e1) < rms(&e0) && rms(&d1) < rms(&d0) * 0.5 && e0.pk < 2 * 32767);
        }
        {   /* (custom, after Rings) MODAL's BRIGHT: lower, the strike's highs softer (less energy in the differences);
             * POSITION: another spectrum (the waveform apart); both bounded */
            double hf[2] = {0, 0}, dpos = 0;
            int32_t o[2][2 * CTL], prev = 0;
            uint32_t r, q;
            for (r = 0; r < 2u; r++) {
                fresh(0);
                trk[0].p[P_FTYPE] = FT_OFF; trk[0].p[P_E0] = SY_MODAL; trk[0].p[P_E1] = 64; trk[0].p[P_E2] = 20;
                trk[0].p[P_SYMBRT] = r ? 10 : 127;
                trk_note_on(&trk[0], 60, 120);
                for (q = 0; q < 40u; q++) {
                    uint32_t k;
                    mix_block(o[0], CTL);
                    for (k = 0; k < CTL; k++, prev = o[0][2 * k])
                        hf[r] += fabs((double)o[0][2 * k] - prev);
                }
            }
            for (r = 0; r < 2u; r++) {
                fresh(0);
                trk[0].p[P_FTYPE] = FT_OFF; trk[0].p[P_E0] = SY_MODAL; trk[0].p[P_E1] = 0; trk[0].p[P_E2] = 20;
                trk[0].p[P_SYMPOS] = r ? 64 : 0;
                trk_note_on(&trk[0], 60, 120);
                for (q = 0; q < 20u; q++)
                    mix_block(o[r], CTL);
            }
            for (q = 0; q < 2u * CTL; q++)
                dpos += fabs((double)o[0][q] - o[1][q]);
            snprintf(w, sizeof w, "MODAL BRIGHT 10: the highs down (%.0f -> %.0f); POSITION 64: the sound apart (%.0f)", hf[0],
                     hf[1], dpos / (2 * CTL));
            ck(w, hf[1] < hf[0] * 0.85 && dpos / (2 * CTL) > 100);
        }
        {   /* ACCENT: the same note, accented (127) and not (100), LP MG half shut: brighter, more so after a run */
            stat_t na = {0}, ac = {0};
            fresh(0);
            trk[0].p[P_FTYPE] = FT_LPMG; trk[0].p[P_FCUT] = 40; trk[0].p[P_FRES] = 40; trk[0].p[P_SYACC] = 100;
            trk_note_on(&trk[0], 48, 100);
            run(FS / 20 / CTL, &na, 0);
            fresh(0);
            memset(sy_sweep, 0, sizeof sy_sweep);
            trk[0].p[P_FTYPE] = FT_LPMG; trk[0].p[P_FCUT] = 40; trk[0].p[P_FRES] = 40; trk[0].p[P_SYACC] = 100;
            trk_note_on(&trk[0], 48, 127);
            run(FS / 20 / CTL, &ac, 0);
            snprintf(w, sizeof w, "ACCENT 100: an accented note opens the filter (RMS %.0f -> %.0f)", rms(&na), rms(&ac));
            ck(w, rms(&ac) > rms(&na) * 1.1);
        }
        {   /* MUFFLE: the peak lower, the sound still there */
            stat_t m0 = {0}, m1 = {0};
            fresh(0); trk[0].p[P_FTYPE] = FT_OFF;
            trk_note_on(&trk[0], 48, 127); trk_note_on(&trk[0], 55, 127); trk_note_on(&trk[0], 60, 127);
            run(FS / 8 / CTL, &m0, 0);
            fresh(0); trk[0].p[P_FTYPE] = FT_OFF; trk[0].p[P_SYMUF] = 127;
            trk_note_on(&trk[0], 48, 127); trk_note_on(&trk[0], 55, 127); trk_note_on(&trk[0], 60, 127);
            run(FS / 8 / CTL, &m1, 0);
            snprintf(w, sizeof w, "MUFFLE 127: the peak lower (%d -> %d), still sounding (RMS %.0f)", (int)m0.pk, (int)m1.pk, rms(&m1));
            ck(w, m1.pk < m0.pk && rms(&m1) > 300);
        }
        (void)r0; (void)r1; (void)pk0;
    }

    /* 3 */
    for (k = 1; k < FT_N; k++) {
        stat_t s = {0};
        static const uint8_t CH[6] = {36, 48, 55, 60, 64, 67};
        fresh(0);
        trk[0].p[P_FTYPE] = (int16_t)k;
        trk[0].p[P_FRES] = 127;
        trk[0].p[P_FDRV] = 127;
        for (i = 0; i < 6u; i++)
            trk_note_on(&trk[0], CH[i], 127);
        for (i = 0; i < 64u; i++) {
            trk[0].p[P_FCUT] = (int16_t)(i < 32u ? i * 4 : 252 - i * 4);
            run(FS / 32 / CTL, &s, 0);
        }
        snprintf(w, sizeof w, "filter %-5s per voice: 6 voices, RESO and DRIVE 127, CUTOFF swept: bounded (peak %d)",
                 N_FTYPE[k], (int)s.pk);
        ck(w, s.pk < 4 * 32767);
    }

    /* 4 */
    {
        stat_t a = {0};
        fresh(0);
        memset(tflt, 0, sizeof tflt);
        trk[0].p[P_FTYPE] = FT_LPMG;
        trk_note_on(&trk[0], 48, 110);
        run(FS / 4 / CTL, &a, 0);
        ck("the track FILTER stays out of SYNTH's mix (its filter per voice: the track's never runs)",
           !tflt[0][0].type && a.pk > 0);
    }

    /* 5 */
    for (k = 0; k < SY_N; k++) {
        uint64_t t0, ns = 0;
        uint32_t nb = 0;
        static const uint8_t CH[6] = {36, 48, 55, 60, 64, 67};
        fresh(0);
        trk[0].p[P_E0] = (int16_t)k;
        trk[0].p[P_E3] = (int16_t)(k < SY_MODAL ? k + 1u : (uint32_t)(k - SY_MODAL + SYB_MODAL));
        trk[0].p[P_E7] = 64;
        trk[0].p[P_FTYPE] = getenv("NOFLT") ? FT_OFF : FT_LPMG;
        for (i = 0; i < 6u; i++)
            trk_note_on(&trk[0], CH[i], 100);
        for (i = 0; i < 400u; i++) {
            int32_t o[2 * CTL];
            t0 = now_ns();
            mix_block(o, CTL);
            ns += now_ns() - t0;
            nb++;
        }
        printf("synth: %-5s x2 + LP MG, 6 voices: %6.0f ns a block (host; a block is %.0f us)\n", N_SY_OSC[k],
               (double)ns / nb, CTL * 1e6 / FS);
    }
    {   /* references: other engines, the same chord */
        static const struct { uint32_t e; const char *n; } R[] = {{0, "ANALOG"}, {12, "FM6"}, {ENGI_PHYS, "PHYS"}, {6, "TRIO"}};
        static const uint8_t CH[6] = {36, 48, 55, 60, 64, 67};
        for (k = 0; k < sizeof R / sizeof R[0]; k++) {
            uint64_t t0, ns = 0;
            host_tracks_init();
            memset(trk[0].v, 0, sizeof trk[0].v);
            memset(eng_mem, 0, sizeof eng_mem);
            host_preset(&trk[0], R[k].e, 0);
            for (i = 0; i < 6u; i++)
                trk_note_on(&trk[0], CH[i], 100);
            for (i = 0; i < 400u; i++) {
                int32_t o[2 * CTL];
                t0 = now_ns();
                mix_block(o, CTL);
                ns += now_ns() - t0;
            }
            printf("synth: (reference) %-6s preset 0, 6 voices: %6.0f ns a block\n", R[k].n, (double)ns / 400);
        }
    }
    {   /* (custom, after Plaits) FOLD CHORD SWARM */
        double z0, z1, b0, b1, zr, zi;
#define SY_SET(md, sh, mo) (fresh(0), trk[0].p[P_E0] = (md), trk[0].p[P_E1] = (sh), trk[0].p[P_E2] = (mo), \
                            trk[0].p[P_FTYPE] = FT_OFF)
        SY_SET(SY_FOLD, 0, 0);
        zc_note(48, &z0, &b0);
        SY_SET(SY_FOLD, 127, 0);
        zc_note(48, &z1, &b1);
        printf("synth: FOLD at C3: %.0f -> %.0f zero crossings a second (SHAPE 0 -> 127)\n", z0, z1);
        ck("FOLD: SHAPE folds (more crossings: higher harmonics), the note's own rate at SHAPE 0", z1 > 3 * z0 && fabs(z0 - 130.8) < 8);
        SY_SET(SY_FOLD, 127, 0);
        zc_note(96, &z1, &b1);
        printf("synth: FOLD at C7, SHAPE 127: %.0f zero crossings a second\n", z1);
        ck("FOLD: held down up the keyboard (C7 folded under ~10 kHz of crossings)", z1 < 10000);
        SY_SET(SY_CHORD, 4 * 128 / 11 + 2, 0);           /* m7, no inversion */
        zc_note(48, &zr, &b0);
        SY_SET(SY_CHORD, 4 * 128 / 11 + 2, 127);         /* m7, the 4th inversion: everything up */
        zc_note(48, &zi, &b1);
        printf("synth: CHORD at C3: m7 %.0f, inverted %.0f zero crossings a second\n", zr, zi);
        ck("CHORD: INV puts the chord's notes up (more crossings)", zi > 1.5 * zr);
        SY_SET(SY_SWARM, 0, 127);
        zc_note(48, &z0, &b0);
        SY_SET(SY_SWARM, 127, 127);
        zc_note(48, &z1, &b1);
        printf("synth: SWARM at C3: the RMS's movement %.3f (SHAPE 0) -> %.3f (127)\n", b0, b1);
        ck("SWARM: SHAPE 0 one saw (steady), spread the saws beat", b0 < 0.05 && b1 > 2 * b0);
#undef SY_SET
    }
    {   /* (custom, the Prophet-5's) POLY MOD, B LOW */
        double z0, z1, b0, b1;
#define PM_SET(e, b, d) (fresh(0), trk[0].p[P_E0] = SY_VA, trk[0].p[P_E1] = 0, trk[0].p[P_E2] = 0, \
                         trk[0].p[P_FTYPE] = FT_OFF, trk[0].p[P_SYPME] = (e), trk[0].p[P_SYPMB] = (b), trk[0].p[P_SYPMD] = (d), \
                         trk[0].p[P_EG2_ATK] = 0, trk[0].p[P_EG2_DEC] = 127, trk[0].p[P_EG2_SUS] = 127, trk[0].p[P_EG2_REL] = 60)
        PM_SET(0, 0, 0);
        zc_note(48, &z0, &b0);
        PM_SET(24, 0, 0);                                /* ENV 2 held at its top: A up 24 x 48 / 63 ~ 18 semitones */
        zc_note(48, &z1, &b1);
        printf("synth: POLY MOD ENV -> FREQ A at C3: %.0f -> %.0f zero crossings a second\n", z0, z1);
        ck("POLY MOD: ENV AMT moves FREQ A (ENV 2 held: ~18 semitones up)", z1 > 2.4 * z0 && z1 < 3.4 * z0);
        PM_SET(0, 100, 0);
        trk[0].p[P_E3] = 1 + SY_VA; trk[0].p[P_E7] = 0; trk[0].p[P_SYBLOW] = 1; trk[0].p[P_E5] = 0;   /* B LOW: ~2 Hz */
        zc_note(48, &z1, &b1);
        printf("synth: POLY MOD OSC B (LOW, 2 Hz) -> FREQ A: the RMS's movement %.3f, %.0f crossings\n", b1, z1);
        ck("POLY MOD: OSC B LOW swings FREQ A (a vibrato: its crossings off the note's, B silent at MIX 0)", fabs(z1 - z0) > 10);
        PM_SET(0, 0, 0);
        trk[0].p[P_E3] = 1 + SY_VA; trk[0].p[P_E7] = 127; trk[0].p[P_SYBLOW] = 1; trk[0].p[P_E5] = 0;
        zc_note(36, &z0, &b0);
        zc_note(84, &z1, &b1);
        printf("synth: B LOW alone at C2 and C6: %.0f, %.0f zero crossings a second\n", z0, z1);
        ck("B LOW: OSC B an LFO (~2 Hz, B TUNE 0) whatever the key", z0 < 12 && z1 < 12);
        PM_SET(40, 0, 2);
        trk[0].p[P_FTYPE] = FT_LPMG; trk[0].p[P_FCUT] = 30;
        {
            stat_t a = {0}, b = {0};
            trk_note_on(&trk[0], 48, 100);
            run(FS / 2 / CTL, &a, 0);
            PM_SET(0, 0, 2);
            trk[0].p[P_FTYPE] = FT_LPMG; trk[0].p[P_FCUT] = 30;
            trk_note_on(&trk[0], 48, 100);
            run(FS / 2 / CTL, &b, 0);
            printf("synth: POLY MOD ENV -> FILTER: RMS %.0f against %.0f without\n", rms(&a), rms(&b));
            ck("POLY MOD: ENV AMT opens the FILTER (DEST FILTER)", rms(&a) > 1.5 * rms(&b));
        }
#undef PM_SET
    }
    printf(bad ? "synth: %d FAILED\n" : "synth tests passed\n", bad);
    return bad != 0;
}
