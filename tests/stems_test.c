/* SPDX-License-Identifier: GPL-3.0-only */
/* Host test of the multitrack USB input (custom, after Kitten Synths' fix-17: src/fx.c uac_stem, src/usb.c uac_tap,
 * uac_packet with FELUCCA_UAC_CH 8):
 * 1. a silent track's stem is silent; a playing one's is not; with the sends at 0 and quiet levels the six stems add
 *    up to the master's dry mono before master_out (within the rounding).
 * 2. a track's stem follows its LEVEL and its MUTE (muted: silent).
 * 3. uac_tap writes 4 words a frame: the master, then the stems as pairs (3-4 T1 T2, 5-6 T3 T4, 7-8 T5 T6);
 *    uac_packet hands 44 / 45 frames of 8 channels; an underrun repeats the whole last frame.
 * 4. the descriptors: 8 channels in the input terminal and the format, 16 bytes a frame in the endpoint's size.
 *   cc -O1 -w -Ibuild/gen -Ifirmware/src -o /tmp/stems_test tests/stems_test.c -lm && /tmp/stems_test */
#define FELUCCA_UAC 1
#define FELUCCA_UAC_CH 8
#define FELUCCA_CDC 0
#define main hostsim_main
#include "hostsim.c"
#undef main

static int bad;
static void ck(const char *w, int ok)
{
    printf("stems: %-84s %s\n", w, ok ? "ok" : "FAIL");
    bad += !ok;
}
static void fresh(void)
{
    uint32_t k;
    host_tracks_init();
    for (k = 0; k < NTRK; k++) {
        trk[k].p[P_REV] = trk[k].p[P_DLY] = trk[k].p[P_CHOR] = 0;
        trk[k].p[P_LEVEL] = 60;
    }
}
static double stem_rms(uint32_t k, uint32_t nb, double *mono_err)
{
    int32_t o[2 * CTL];
    double s = 0, e = 0;
    uint32_t b, i, j;
    for (b = 0; b < nb; b++) {
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++) {
            int32_t sum = 0;
            for (j = 0; j < NTRK; j++)
                sum += uac_stem[j][i];
            s += (double)uac_stem[k][i] * uac_stem[k][i];
            /* the master's dry sum at its gain, before master_out (whose DC blocker and limiter the stems skip) */
            e += fabs((double)sum - ((((mix_l[i] + mix_r[i]) >> 1) >> 2) * (int32_t)song.master_q12 >> 10));
        }
    }
    if (mono_err)
        *mono_err = e / (nb * CTL);
    return sqrt(s / (nb * CTL));
}

int main(void)
{
    double err, r0, r1, r2;
    char w[160];
    fresh();
    trk_note_on(&trk[0], 48, 100);                       /* T1 plays, the others are silent */
    stem_rms(0, 40, 0);
    r0 = stem_rms(0, 40, &err);
    r1 = stem_rms(1, 1, 0);
    snprintf(w, sizeof w, "T1 playing: its stem RMS %.0f, T2's %.0f; the stems add up to the dry master (mean error %.2f)",
             r0, r1, err);
    ck(w, r0 > 50 && r1 == 0 && err < 2.0);
    trk[0].p[P_LEVEL] = 30;
    stem_rms(0, 20, 0);
    r2 = stem_rms(0, 40, 0);
    snprintf(w, sizeof w, "LEVEL 60 -> 30: the stem falls (%.0f -> %.0f)", r0, r2);
    ck(w, r2 < r0 * 0.8);
    trk[0].p[P_MUTE] = 1;
    stem_rms(0, 60, 0);
    r2 = stem_rms(0, 10, 0);
    snprintf(w, sizeof w, "MUTE: the stem silent (%.0f)", r2);
    ck(w, r2 < 2);
    trk[0].p[P_MUTE] = 0;

    /* 3 */
    {
        int32_t o[2 * CTL];
        static const uint8_t zlo[2 * CTL];
        uint32_t d[UA_MAXF * UA_W], n, i, k, ok = 1;
        fresh();
        trk_note_on(&trk[2], 60, 100);                   /* T3 */
        uac.feed = 1;
        uac.go = 1;
        uac.ring48 = uac.r48 = 0;
        ua_w = ua_r = 0;
        for (i = 0; i < 6u; i++) {
            mix_block(o, CTL);
            uac_tap(o, zlo, CTL);                         /* ((2.5) the low bits: none) */
        }
        if (ua_w != 6u * CTL)
            printf("  ua_w %u\n", ua_w), ok = 0;
        for (i = 0; i < CTL; i++) {                      /* the last block: master, T1|T2, T3|T4, T5|T6 */
            const uint32_t *f = &ua_ring[UA_AT(5u * CTL + i)];
            ok &= f[0] == ((uint32_t)(uint16_t)uac_s16(o[2 * i]) | (uint32_t)(uint16_t)uac_s16(o[2 * i + 1]) << 16);
            for (k = 0; k < 3u; k++)
                ok &= f[1u + k] == ((uint32_t)(uint16_t)uac_s16(uac_stem[2u * k][i]) |
                                    (uint32_t)(uint16_t)uac_s16(uac_stem[2u * k + 1u][i]) << 16);
            ok &= uac_stem[3][i] == 0 && uac_stem[5][i] == 0;
        }
        snprintf(w, sizeof w, "uac_tap: 4 words a frame, the master then T1|T2 T3|T4 T5|T6 (T3 playing, T4 T6 silent)");
        ck(w, ok);
        uac.fill_min = 100;
        n = uac_packet(d);
        ok = (n == 44u || n == 45u) && ua_r == n;
        for (i = 0; i < n * UA_W; i++)
            ok &= d[i] == ua_ring[UA_AT(i / UA_W) + i % UA_W];
        ck("uac_packet: 44 / 45 frames of 8 channels, from the ring in order", ok);
        {
            uint32_t last[UA_W];
            memcpy(last, &d[(n - 1u) * UA_W], sizeof last);
            ua_r = ua_w;                                 /* empty: an underrun */
            uac.fill_min = 100;
            n = uac_packet(d);
            ok = 1;
            for (i = 0; i < n; i++)
                ok &= !memcmp(&d[i * UA_W], last, sizeof last);
        }
        ck("an underrun repeats the whole last frame sent (8 channels)", ok);
    }
    /* 4 */
    {
        const uint8_t *c = CFG_DESC_PLAIN;
        uint32_t i, len = sizeof CFG_DESC_PLAIN, it = 0, fmt = 0, ep = 0;
        for (i = 0; i + 1 < len; i += c[i]) {
            if (!c[i])
                break;
            if (c[i + 1] == 0x24 && c[i + 2] == 2 && c[i] == 12)
                it = c[i + 7];                           /* input terminal: bNrChannels */
            if (c[i + 1] == 0x24 && c[i + 2] == 2 && c[i] == 11)
                fmt = c[i + 4];                          /* format type I: bNrChannels */
            if (c[i + 1] == 5 && c[i + 2] == 0x84)
                ep = c[i + 4] | c[i + 5] << 8;
        }
        snprintf(w, sizeof w, "descriptors: input terminal %u ch, format %u ch, EP4 %u bytes (46 frames x 16)", it, fmt, ep);
        ck(w, it == 8u && fmt == 8u && ep == 46u * 16u);
    }
    printf(bad ? "stems: %d FAILED\n" : "stems tests passed\n", bad);
    return bad != 0;
}
