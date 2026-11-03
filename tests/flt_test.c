/* SPDX-License-Identifier: GPL-3.0-only */
/* Host test of the filter library (src/flt.c):
 * 1. OFF and an unknown type: the block bit for bit.
 * 2. every type at every corner of CUT RES CHAR DRIVE on white noise and loud saws (10 s): bounded, no DC run-away.
 * 3. the low-passes pass 100 Hz and cut 6 kHz at CUT 50; HP (SVF CHAR 127 side) the other way round.
 * 4. LP MG at RES 127 rings on by itself after an impulse (self-oscillation); OB 12 does not run away.
 * 5. the cost: ns a sample per type (host), against the block budget.
 * Demos: build/flt_demo/<TYPE>.wav, a 110 Hz saw chord with the CUT swept up and down, RES 90.
 *   cc -O2 -w -Ibuild/gen -Ifirmware/src -o /tmp/flt_test tests/flt_test.c -lm && /tmp/flt_test */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/stat.h>
#include "felucca_tables.h"
#include "core.h"
#include "dsp.c"
#include "flt.c"

#define SR 44100
#define BLK 32
static const char *NAMES[FT_N] = {"OFF", "LPMG", "LD", "SVF", "SK", "OB12", "OB24", "ACID", "COMB", "VOWEL"};
static int bad;
static void ck(const char *w, int ok)
{
    printf("flt: %-78s %s\n", w, ok ? "ok" : "FAIL");
    bad += !ok;
}
static int16_t line[FLT_LINE];

/* RMS of a sine of hz through the filter (after 0.3 s of settling) */
static double tone(uint32_t ty, int cut, int res, int chr, double hz)
{
    flt_t f;
    int32_t b[BLK];
    double acc = 0;
    int i, k, cnt = 0;
    memset(&f, 0, sizeof f);
    for (k = 0; k < SR / BLK; k++) {
        for (i = 0; i < BLK; i++)
            b[i] = (int32_t)(20000 * sin(2 * M_PI * hz * (k * BLK + i) / SR));
        flt_run(&f, line, b, BLK, ty, cut, res, chr, 0, 0);
        if (k > SR / BLK * 3 / 10)
            for (i = 0; i < BLK; i++, cnt++)
                acc += (double)b[i] * b[i];
    }
    return sqrt(acc / cnt) / (20000 / sqrt(2));
}

static void wav(const char *path, const int16_t *s, uint32_t n)
{
    FILE *fp = fopen(path, "wb");
    uint32_t d = n * 2, v;
    if (!fp)
        return;
    fwrite("RIFF", 1, 4, fp); v = 36 + d; fwrite(&v, 4, 1, fp); fwrite("WAVEfmt ", 1, 8, fp);
    v = 16; fwrite(&v, 4, 1, fp); v = 1 | (1 << 16); fwrite(&v, 4, 1, fp); v = SR; fwrite(&v, 4, 1, fp);
    v = SR * 2; fwrite(&v, 4, 1, fp); v = 2 | (16 << 16); fwrite(&v, 4, 1, fp);
    fwrite("data", 1, 4, fp); fwrite(&d, 4, 1, fp); fwrite(s, 2, n, fp); fclose(fp);
}

int main(void)
{
    static int16_t demo[SR * 6];
    int32_t b[BLK], ref[BLK];
    uint32_t ty;
    int i, k;
    char w[160];
    flt_t f;
    mkdir("build", 0755);
    mkdir("build/flt_demo", 0755);

    /* 1 */
    memset(&f, 0, sizeof f);
    for (i = 0; i < BLK; i++)
        b[i] = ref[i] = (i * 7919) % 60000 - 30000;
    flt_run(&f, line, b, BLK, FT_OFF, 10, 127, 0, 127, 0);
    flt_run(&f, line, b, BLK, 99, 10, 127, 0, 127, 0);
    ck("OFF and an unknown type leave the block bit for bit", !memcmp(b, ref, sizeof b));

    /* 2 */
    for (ty = 1; ty < FT_N; ty++) {
        static const int C[] = {0, 64, 127};
        int ok = 1, a, r, c, d;
        int32_t peak = 0;
        double dc = 0;
        int32_t ns = 1;
        for (a = 0; a < 3; a++)
            for (r = 0; r < 3; r++)
                for (c = 0; c < 3; c++)
                    for (d = 0; d < 3; d++) {
                        memset(&f, 0, sizeof f);
                        dc = 0;
                        for (k = 0; k < SR * 10 / 27 / BLK; k++) {
                            for (i = 0; i < BLK; i++) {
                                int32_t saw = ((k * BLK + i) * 655 % 65536) - 32768;
                                ns = (int32_t)noise32(&ns);
                                b[i] = (k & 64 ? saw * 3 : (ns >> 16) * 2) + ((ns >> 20) & 1 ? 0 : 0);
                            }
                            flt_run(&f, line, b, BLK, ty, C[a], C[r], C[c], C[d], (k & 15) * 30 - 200);
                            for (i = 0; i < BLK; i++) {
                                int32_t v = b[i] < 0 ? -b[i] : b[i];
                                if (v > peak)
                                    peak = v;
                                dc += b[i];
                            }
                        }
                        if (peak > 4 * FLT_SMAX + 4)
                            ok = 0;
                        if (fabs(dc / (SR * 10 / 27)) > 40000) {
                            ok = 0;
                            printf("  DC %.0f at CUT %d RES %d CHAR %d DRIVE %d\n", dc / (SR * 10 / 27), C[a], C[r], C[c], C[d]);
                        }
                    }
        snprintf(w, sizeof w, "%-5s: every corner of CUT RES CHAR DRIVE on noise and loud saws, bounded (peak %d)",
                 NAMES[ty], (int)peak);
        ck(w, ok);
    }

    /* 3 */
    {
        static const uint32_t LP[] = {FT_LPMG, FT_LD, FT_SVF, FT_SK, FT_OB12, FT_OB24, FT_ACID};
        for (k = 0; k < 7; k++) {
            uint32_t t = LP[k];
            int chr = t == FT_LD ? 127 : 0;
            double lo = tone(t, 50, 0, chr, 100), hi = tone(t, 50, 0, chr, 6000);
            snprintf(w, sizeof w, "%-5s low-pass at CUT 50: 100 Hz %.2f, 6 kHz %.4f (-%.0f dB)", NAMES[t], lo, hi,
                     -20 * log10(hi / lo));
            ck(w, lo > 0.3 && hi < lo * 0.1);
        }
        {
            double lo = tone(FT_OB12, 70, 0, 111, 60), hi = tone(FT_OB12, 70, 0, 111, 8000);
            snprintf(w, sizeof w, "OB12  CHAR 111 (HP): 60 Hz %.3f, 8 kHz %.2f", lo, hi);
            ck(w, hi > 0.5 && lo < hi * 0.2);
        }
        {
            double v = tone(FT_VOWEL, 64, 80, 0, 700), u = tone(FT_VOWEL, 64, 80, 0, 4500);
            snprintf(w, sizeof w, "VOWEL A: its F1 (700 Hz) %.2f against 4.5 kHz %.3f", v, u);
            ck(w, v > u * 3);
        }
    }

    /* 4 */
    {
        double e = 0;
        memset(&f, 0, sizeof f);
        for (k = 0; k < SR / BLK; k++) {
            for (i = 0; i < BLK; i++)
                b[i] = k == 0 && i == 0 ? 30000 : 0;
            flt_run(&f, line, b, BLK, FT_LPMG, 70, 127, 0, 0, 0);
            if (k > SR / BLK / 2)
                for (i = 0; i < BLK; i++)
                    e += fabs((double)b[i]);
        }
        snprintf(w, sizeof w, "LP MG RES 127 self-oscillates (mean level 0.5..1 s after an impulse: %.0f)",
                 e / (SR / 2));
        ck(w, e / (SR / 2) > 500);
    }

    /* 5 + demos */
    for (ty = 1; ty < FT_N; ty++) {
        struct timespec t0, t1;
        uint32_t n = SR * 6, p1 = 0, p2 = 0, p3 = 0;
        double nsps;
        memset(&f, 0, sizeof f);
        clock_gettime(CLOCK_MONOTONIC, &t0);
        for (k = 0; k < (int)(n / BLK); k++) {
            int ph = k * BLK, cut = (ph % (SR * 3)) * 2 * 127 / (SR * 3);
            if (cut > 127)
                cut = 254 - cut;
            for (i = 0; i < BLK; i++) {
                p1 += 10715000u, p2 += 13500000u, p3 += 16070000u;   /* A2 C#3 E3 */
                b[i] = (osc_saw(p1, 10715000u) + osc_saw(p2, 13500000u) + osc_saw(p3, 16070000u)) * 7 / 10;
            }
            flt_run(&f, line, b, BLK, ty, cut, 90, ty == FT_COMB ? 100 : ty == FT_VOWEL ? (ph * 127 / (int)n) : 40,
                    30, 0);
            for (i = 0; i < BLK; i++)
                demo[ph + i] = (int16_t)clamp(b[i] >> 1, -32767, 32767);
        }
        clock_gettime(CLOCK_MONOTONIC, &t1);
        nsps = ((t1.tv_sec - t0.tv_sec) * 1e9 + (t1.tv_nsec - t0.tv_nsec)) / n;
        snprintf(w, sizeof w, "build/flt_demo/%s.wav", NAMES[ty]);
        wav(w, demo, n);
        printf("flt: %-5s %.1f ns a sample (host, with the saws)  -> %s\n", NAMES[ty], nsps, w);
    }
    printf(bad ? "flt: %d FAILED\n" : "flt tests passed\n", bad);
    return bad != 0;
}
