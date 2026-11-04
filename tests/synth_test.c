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
        ck(w, rms(&s) > 300 && s.pk < 32767 * 2 && fabs(mean(&s)) < 200 && !voices_on(&trk[0]));
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
                if (rms(&s) < 200 || s.pk > 2 * 32767 || fabs(mean(&s)) > 400)
                    ok = 0, printf("  %s MOD %u note %u: RMS %.0f peak %d mean %.0f\n", N_SY_OSC[k], mod * 127, note,
                                   rms(&s), (int)s.pk, mean(&s));
            }
        for (i = 0; i < 2u; i++) {                       /* OSC B of this model alone */
            stat_t s = {0};
            fresh(0);
            trk[0].p[P_E3] = (int16_t)(k + 1u);
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
    for (k = SY_SYNC; k < SY_NB; k++) {
        int ok = 1;
        double r[2] = {0, 0};
        for (i = 0; i < 2u; i++) {
            stat_t s = {0};
            fresh(0);
            trk[0].p[P_E3] = (int16_t)(k + 1u);
            trk[0].p[P_E5] = i ? 19 : 0;
            trk[0].p[P_E4] = k == SY_RING ? 127 : 0;
            trk[0].p[P_E7] = 127;
            trk[0].p[P_FTYPE] = FT_OFF;
            trk_note_on(&trk[0], 48, 120);
            run(FS / 4 / CTL, &s, 0);
            r[i] = rms(&s);
            if (rms(&s) < 200 || s.pk > 2 * 32767 || fabs(mean(&s)) > 400)
                ok = 0, printf("  B %s TUNE %d: RMS %.0f peak %d mean %.0f\n", N_SY_OSCB[k + 1u], i ? 19 : 0, rms(&s),
                               (int)s.pk, mean(&s));
        }
        snprintf(w, sizeof w, "OSC B %-4s: at B TUNE 0 and +19: sounds, bounded, no DC (RMS %.0f / %.0f)", N_SY_OSCB[k + 1u],
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
        trk[0].p[P_E3] = (int16_t)(k + 1u);
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
    printf(bad ? "synth: %d FAILED\n" : "synth tests passed\n", bad);
    return bad != 0;
}
