/* SPDX-License-Identifier: GPL-3.0-only */
/* Host test of FMINI (custom: src/eng_fmini.c, FMini-1's Minimoog voice):
 * 1. every SLOT's patch plays: it sounds, bounded, no DC, its voices end (the regression's golden renders hold each one).
 * 2. EMPH at its top with the mixer's patch silent sings: a sine at the singing pitch (the ladder self-oscillates).
 * 3. CUT moves the brightness: the high band falls with CUT.
 * 4. the cost against the other engines: ns a block, 4 voices.
 * Demos: build/fmini_demo/<PRESET>.wav (the first 12 presets).
 *   cc -O2 -w -Ibuild/gen -Ifirmware/src -o /tmp/fmini_test tests/fmini_test.c -lm && /tmp/fmini_test build/fmini_demo */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <sys/stat.h>

static int bad;
static void ck(const char *w, int ok)
{
    printf("fmini: %-82s %s\n", w, ok ? "ok" : "FAIL");
    bad += !ok;
}
static void fresh(uint32_t preset)
{
    host_tracks_init();
    memset(trk[0].v, 0, sizeof trk[0].v);
    memset(eng_mem, 0, sizeof eng_mem);
    host_preset(&trk[0], ENGI_FMINI, preset);
    fmini_sm_ok = 0;                                    /* (the knobs snap: no glide from the test before) */
    trk[0].p[P_REV] = trk[0].p[P_DLY] = trk[0].p[P_CHOR] = 0;
}
static uint32_t voices_on(const track_t *t)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++)
        n += t->v[i].active;
    return n;
}
typedef struct { int32_t pk; double sum, sq, hf, hfp; uint32_t n; } stat_t;
static void run(uint32_t nb, stat_t *s, FILE *w)
{
    int32_t o[2 * CTL];
    uint32_t i, k;
    for (i = 0; i < nb; i++) {
        mix_block(o, CTL);
        for (k = 0; k < 2u * CTL; k += 2) {
            int32_t a = abs(o[k]);
            if (s) {
                double d = o[k] - s->hfp;
                s->pk = a > s->pk ? a : s->pk;
                s->sum += o[k];
                s->sq += (double)o[k] * o[k];
                s->hf += d * d;
                s->hfp = o[k];
                s->n++;
            }
        }
        if (w)
            for (k = 0; k < CTL; k++)
                wav_put(w, o[2 * k], o[2 * k + 1]);
    }
}
static double rms(const stat_t *s) { return s->n ? sqrt(s->sq / s->n) : 0; }

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : "build/fmini_demo";
    static const uint8_t LINE[] = {36, 48, 43, 46, 36, 51, 48, 55};
    char w[200];
    uint32_t k, i, n;
    int ok = 1;
    mkdir("build", 0755);
    mkdir(dir, 0755);
    for (k = 0; k < ENG_FMINI.npresets; k++) {           /* 1 */
        stat_t s = {0};
        FILE *fp = 0;
        uint32_t f, step = FS / 8u, frames = 8u * step + FS;
        if (k < 12u) {
            snprintf(w, sizeof w, "%s/%02u_%s.wav", dir, k, ENG_FMINI.presets[k].name);
            for (i = 0; w[i]; i++)
                if (w[i] == ' ')
                    w[i] = '_';
            if ((fp = fopen(w, "wb")))
                wav_hdr(fp, frames);
        }
        fresh(k);
        for (f = 0, i = 0; f < frames; f += CTL) {
            if (i < 8u && f >= i * step) {
                if (i)
                    trk_note_off(&trk[0], LINE[i - 1] + 12);
                trk_note_on(&trk[0], LINE[i] + 12, 100);
                i++;
            }
            if (i == 8u && f >= 8u * step)
                trk_note_off(&trk[0], LINE[7] + 12), i++;
            run(1, &s, fp);
        }
        if (fp)
            fclose(fp);
        for (n = 0; n < 30u * FS / CTL && voices_on(&trk[0]); n++)
            run(1, 0, 0);
        if (s.pk > 2 * 32767 || fabs(s.sum / s.n) > 400 || voices_on(&trk[0]) || (rms(&s) < 30 && k < 190u))
            ok = 0, printf("  %u %s: RMS %.0f peak %d mean %.0f voices %u\n", k, ENG_FMINI.presets[k].name, rms(&s),
                           (int)s.pk, s.sum / s.n, voices_on(&trk[0]));
    }
    snprintf(w, sizeof w, "all %u patches: bounded, no DC, the voices end", (unsigned)ENG_FMINI.npresets);
    ck(w, ok);
    {                                                    /* 2: silent mixer, EMPH 127 */
        stat_t s = {0};
        fresh(0);
        trk[0].p[P_E1] = 127;
        trk[0].p[P_E2] = 0;
        trk[0].p[P_E7] = 0;
        trk_note_on(&trk[0], 60, 100);
        run(FS / 2 / CTL, 0, 0);
        run(FS / 2 / CTL, &s, 0);
        snprintf(w, sizeof w, "EMPH 127 sings (RMS %.0f after 0.5 s)", rms(&s));
        ck(w, rms(&s) > 300);
    }
    {                                                    /* 3 */
        double hf[2];
        for (i = 0; i < 2u; i++) {
            stat_t s = {0};
            fresh(1);
            trk[0].p[P_E0] = i ? 100 : 30;
            trk[0].p[P_E2] = 0;
            trk_note_on(&trk[0], 36, 110);
            run(FS / 4 / CTL, &s, 0);
            hf[i] = s.hf / (s.sq + 1);
        }
        snprintf(w, sizeof w, "CUT 30 -> 100 brightens (high band share %.4f -> %.4f)", hf[0], hf[1]);
        ck(w, hf[1] > hf[0] * 2);
    }
    {                                                    /* 4 */
        static const struct { uint32_t e; const char *n; } R[] = {{ENGI_FMINI, "FMINI"}, {0, "ANALOG"}, {12, "FM6"}};
        for (k = 0; k < 3u; k++) {
            uint64_t t0, ns = 0;
            fresh(0);
            host_preset(&trk[0], R[k].e, 0);
            for (i = 0; i < 4u; i++)
                trk_note_on(&trk[0], 48 + i * 5, 100);
            for (i = 0; i < 400u; i++) {
                int32_t o[2 * CTL];
                t0 = now_ns();
                mix_block(o, CTL);
                ns += now_ns() - t0;
            }
            printf("fmini: %-6s preset 0, 4 voices: %6.0f ns a block (host)\n", R[k].n, (double)ns / 400);
        }
    }
    printf(bad ? "fmini: %d FAILED\n" : "fmini tests passed\n", bad);
    return bad != 0;
}
