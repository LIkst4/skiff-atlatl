/* SPDX-License-Identifier: GPL-3.0-only */
/* (custom) GLO > MULTI, the master's 3-band compressor (fx.c mb_block): the bands add up to the mix exactly at unity;
 * a band's AMOUNT takes its band down and leaves the others; UPWARD brings a quiet band up, not silence; ATTACK and
 * RELEASE set how fast; GAIN; the reverb's wet goes through it; its globals stored (SKF4: GSIDs 40..) and a project
 * before it loads OFF. MIX 0 (OFF) is the master as before: tests/golden.txt unchanged.
 * Run by tests/run_tests.sh (needs build/gen from one firmware build). */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include <math.h>

static int32_t L[CTL], R[CTL], W[CTL];

static void mb_reset(void)
{
    uint32_t b;
    memset(&mb, 0, sizeof mb);
    song.g[G_MBMIX] = 127;
    song.g[G_MBUP] = 0;
    song.g[G_MBXLO] = GP[G_MBXLO].def;
    song.g[G_MBXHI] = GP[G_MBXHI].def;
    for (b = 0; b < 3u; b++) {
        song.g[G_MBAMT0 + 4u * b] = 0;
        song.g[G_MBATK0 + 4u * b] = GP[G_MBATK0 + 4u * b].def;
        song.g[G_MBREL0 + 4u * b] = GP[G_MBREL0 + 4u * b].def;
        song.g[G_MBGAIN0 + 4u * b] = 0;
    }
}

/* nblk blocks of a sine (hz, amp) on both sides; the RMS of the output's last half, and of the input's */
static double run(double hz, double amp, uint32_t nblk, double *in_rms, uint32_t *t0)
{
    double so = 0, si = 0;
    uint32_t k, i, cnt = 0;
    for (k = 0; k < nblk; k++) {
        for (i = 0; i < CTL; i++) {
            double x = amp * sin(2.0 * M_PI * hz * (double)(*t0) / 44100.0);
            L[i] = R[i] = (int32_t)lrint(x);
            W[i] = 0;
            (*t0)++;
        }
        if (k >= nblk / 2u)
            for (i = 0; i < CTL; i++)
                si += (double)L[i] * L[i];
        mb_block(L, R, W, CTL);
        if (k >= nblk / 2u) {
            for (i = 0; i < CTL; i++)
                so += (double)L[i] * L[i];
            cnt += CTL;
        }
    }
    if (in_rms)
        *in_rms = sqrt(si / cnt);
    return sqrt(so / cnt);
}
static double db(double a, double b) { return 20.0 * log10(a / b); }

int main(void)
{
    int bad = 0, ok;
    uint32_t t0 = 0, k, i;
    double in, out;
    ui_power_on();

    /* the bands at unity add up to an all-pass of the mix: its level flat (LR's), from 40 Hz to 16 kHz; the reverb's
     * wet in it */
    ok = 1;
    {
        static const double HZ[] = {40, 132, 264, 600, 1000, 2500, 4211, 8000, 16000};
        for (k = 0; k < NELEM(HZ); k++) {
            mb_reset();
            out = run(HZ[k], 8000.0, 400, &in, &t0);
            ok &= fabs(db(out, in)) < 0.15;
            if (fabs(db(out, in)) >= 0.15)
                printf("  (%.0f Hz: %+.2f dB)\n", HZ[k], db(out, in));
        }
        mb_reset();
        for (i = 0; i < CTL; i++)
            L[i] = R[i] = 0, W[i] = 1000;
        mb_block(L, R, W, CTL);
        ok &= !W[0] && L[CTL - 1u] != 0;
    }
    bad += check("unity (AMOUNT 0, GAIN 0): the three bands' sum flat within 0.15 dB, 40 Hz .. 16 kHz, the wet in it", ok);

    /* AMOUNT on LOW: a loud 60 Hz comes down, an 8 kHz as loud stays */
    mb_reset();
    song.g[G_MBAMT0] = 127;
    out = run(60.0, 28000.0, 600, &in, &t0);
    bad += check("LOW's AMOUNT at full: a loud 60 Hz down by more than 9 dB", db(out, in) < -9.0);
    printf("  (60 Hz: %+.1f dB)\n", db(out, in));
    mb_reset();
    song.g[G_MBAMT0] = 127;
    out = run(8000.0, 28000.0, 600, &in, &t0);
    bad += check("  an 8 kHz as loud: within 1 dB (HIGH's, AMOUNT 0)", fabs(db(out, in)) < 1.0);
    printf("  (8 kHz: %+.2f dB)\n", db(out, in));

    /* UPWARD: a quiet 1 kHz up with MID's AMOUNT; not without UPWARD; silence stays silent */
    mb_reset();
    song.g[G_MBAMT1] = 100;
    out = run(1000.0, 900.0, 800, &in, &t0);
    bad += check("UPWARD OFF: a quiet 1 kHz (-31 dB) not brought up", db(out, in) < 0.5);
    mb_reset();
    song.g[G_MBAMT1] = 100;
    song.g[G_MBUP] = 1;
    out = run(1000.0, 900.0, 800, &in, &t0);
    bad += check("UPWARD ON: the quiet 1 kHz up by more than 4 dB", db(out, in) > 4.0);
    printf("  (1 kHz upward: %+.1f dB)\n", db(out, in));
    mb_reset();
    song.g[G_MBAMT0] = song.g[G_MBAMT1] = song.g[G_MBAMT2] = 127;
    song.g[G_MBUP] = 1;
    {
        double si = 0, so = 0;
        int32_t mx = 0;
        for (k = 0; k < 200u; k++) {
            for (i = 0; i < CTL; i++) {
                L[i] = R[i] = (int32_t)(rand() % 3) - 1;      /* (a bit of dither: under -60 dB) */
                W[i] = 0;
                si += (double)L[i] * L[i];
            }
            mb_block(L, R, W, CTL);
            for (i = 0; i < CTL; i++) {
                so += (double)L[i] * L[i];
                mx = abs(L[i]) > mx ? abs(L[i]) : mx;
            }
        }
        ok = so <= si * 1.6 && mx <= 4;               /* (the all-pass's rounding, no gain: within 2 dB, 4 LSB) */
        printf("  (the last bit: %+.1f dB, peak %d)\n", 10.0 * log10(so / si), mx);
    }
    bad += check("  silence (the last bit) stays silent: nothing under -60 dB brought up", ok);

    /* ATTACK: from quiet to loud, how much of its way down the gain is 5 ms on (fast and slow ATTACK) */
    for (k = 0; k < 2u; k++) {
        int32_t g0;
        mb_reset();
        song.g[G_MBAMT0] = 127;
        song.g[G_MBATK0] = k ? 80 : 4;
        song.g[G_MBREL0] = 60;
        (void)run(60.0, 300.0, 100, 0, &t0);
        g0 = mb.g[0];
        (void)run(60.0, 28000.0, 2u * 7u, 0, &t0);  /* (7 blocks of 32: 5 ms; run counts its halves) */
        {
            int32_t g5 = mb.g[0], gf;
            (void)run(60.0, 28000.0, 2u * 3000u, 0, &t0);
            gf = mb.g[0];
            double part = (double)(g0 - g5) / (double)(g0 - gf);
            bad += check(k ? "ATTACK 330 ms: 5 ms on, under a third of the way down" :
                             "ATTACK 0.3 ms: 5 ms on, most of the way down", k ? part < 0.33 : part > 0.7);
            printf("  (ATTACK %s: %.0f %% in 5 ms)\n", k ? "slow" : "fast", part * 100.0);
        }
    }

    /* GAIN: +6 dB on all three, AMOUNT 0: the mix 6 dB up */
    mb_reset();
    song.g[G_MBGAIN0] = song.g[G_MBGAIN1] = song.g[G_MBGAIN2] = 6;
    out = run(440.0, 4000.0, 200, &in, &t0);
    bad += check("GAIN +6 dB on the three: the mix 6 dB up (within 0.3)", fabs(db(out, in) - 6.0) < 0.3);
    printf("  (GAIN: %+.2f dB)\n", db(out, in));

    /* stored: SKF4's globals by GSID, the fork's at 40..; a record of 28 (Skiff 2.5's, the fork's before) leaves them */
    {
        static int16_t g[G_COUNT], h[G_COUNT];
        static uint8_t b[256];
        uint32_t n;
        for (i = 0; i < G_COUNT; i++)
            g[i] = (int16_t)(i * 3u + 1u);
        n = gsid_put(b, g);
        memset(h, 0, sizeof h);
        ok = n == 1u + 2u * GSID_COUNT && b[0] == GSID_COUNT && GSID_COUNT == 56u && gsid_get(h, b, n) == n &&
             !memcmp(g, h, sizeof g) && b[1u + 2u * 30u] == 0 && b[1u + 2u * GSID_FORK] == (uint8_t)g[G_MBMIX];
        b[0] = 28;
        for (i = 0; i < G_COUNT; i++)
            h[i] = 77;
        ok &= gsid_get(h, b, 1u + 2u * 28u) == 1u + 2u * 28u && h[G_DMODE] == g[G_DMODE] && h[G_MBMIX] == 77;
        bad += check("stored: MULTI's globals by GSID 40.. (28..39 Skiff's next), a record of 28 leaves them", ok);
        bad += check("  a project without them: MULTI OFF (MIX's default 0)", GP[G_MBMIX].def == 0);
    }

    printf(bad ? "mb_test: %d FAILED\n" : "mb_test: all passed\n", bad);
    return bad != 0;
}
