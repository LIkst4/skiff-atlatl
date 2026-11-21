/* SPDX-License-Identifier: GPL-3.0-only */
/* (custom) DRUM's user kits (USR1..3) in the standard build, without the sampler (FELUCCA_SAMPLER 0; eng_drum.c
 * FELUCCA_USER_KITS): a slot made from two real WAVs as the editor's Samples tab makes it (tools/fm1_sample_upload.py
 * build: the same IMA ADPCM encoder as the editor), read by smp_user_scan as at boot:
 *   drumusr_test KIT.hdr KIT.bin     (tests/drumusr_wavs.py, then fm1_sample_upload.py build MYKIT KIT
 *                                     kick.wav:36:36-36 snare.wav:38:38-38: each on its lane's note alone)
 * 1. KIT names USR1..USR3 and the knob reaches them (not STD's aliases);
 * 2. USR1: KICK plays the kick WAV, its length and its shape (the decoded sample's correlation with the WAV's), SNARE
 *    the snare, a lane without a sample (BELL) silent;
 * 3. the lane's TUNE plays it faster, its DECAY shorter;
 * 4. an empty slot (USR2) silent; STD as before. */
#include <stdarg.h>
#include <stdint.h>
static uint32_t host_slots[3u * 0x14000u / 4u];          /* USR1..3, as the flash at 0xA0000 */
#define SMP_USER_XIP(k) ((const uint8_t *)host_slots + (k) * SMP_USER_SIZE)
#define main hostsim_main
#include "hostsim.c"
#undef main

static int fails;
static void check(const char *what, int ok, const char *fmt, ...)
{
    char d[160] = "";
    va_list ap;
    if (fmt) {
        va_start(ap, fmt);
        vsnprintf(d, sizeof d, fmt, ap);
        va_end(ap);
    }
    printf("drumusr: %-96s %s%s%s\n", what, ok ? "ok" : "FAIL", d[0] ? "  " : "", d);
    fails += !ok;
}
static long load(const char *path, void *dst, long max)
{
    FILE *f = fopen(path, "rb");
    long n;
    if (!f)
        return -1;
    n = (long)fread(dst, 1, (size_t)max, f);
    fclose(f);
    return n;
}
static track_t *kit_track(int16_t kit)
{
    track_t *t = &trk[0];
    uint32_t k, j;
    int32_t o[2 * CTL];
    for (k = 0; k < NTRK; k++)
        for (j = 0; j < NVOICE; j++)
            trk[k].v[j].active = trk[k].v[j].gate = 0;
    host_tracks_init();
    song.g[G_RTYPE] = 0;
    host_preset(t, ENGI_DRUM, 0);
    t->p[P_E0] = kit;
    t->p[P_REV] = t->p[P_DLY] = t->p[P_CHOR] = t->p[P_DIST] = 0;
    memset(drum_mute, 0, sizeof drum_mute);
    mix_block(o, CTL);
    return t;
}
/* note struck on t: nb blocks of the dry mix into y (when given); how many blocks are above 64 */
static uint32_t kit_play(track_t *t, uint32_t note, uint32_t nb, int32_t *y)
{
    uint32_t k, i, n = 0;
    trk_note_on(t, note, 127);
    for (k = 0; k < nb; k++) {
        int32_t o[2 * CTL], pk = 0;
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++) {
            pk = abs(mix_l[i]) > pk ? abs(mix_l[i]) : pk;
            if (y)
                y[k * CTL + i] = mix_l[i];
        }
        n += pk > 64;
    }
    trk_note_off(t, note);
    return n;
}
/* the kick WAV as the test made it (drumusr_wavs.py), at FS: the shape the lane should play */
static double kick_ref(uint32_t i)
{
    static double ph;
    static uint32_t at;
    if (!i)
        ph = 0, at = 0;
    for (; at <= i; at++)
        ph += 2.0 * 3.14159265358979 * (50.0 + 120.0 * exp(-(double)at / 2000.0)) / 44100.0;
    return sin(ph) * exp(-(double)i / 6000.0);
}

int main(int argc, char **argv)
{
    static int32_t y[600 * CTL];
    const smp_user_hdr_t *h = (const smp_user_hdr_t *)host_slots;
    track_t *t;
    uint32_t kick, snare, bell, kick_up, kick_short, empty, std, i, j, best = 0;
    double bc = 0;
    long n;
    if (argc < 3) {
        fprintf(stderr, "drumusr_test KIT.hdr KIT.bin\n");
        return 2;
    }
    memset(host_slots, 0xFF, sizeof host_slots);
    n = load(argv[1], host_slots, SMP_USER_DATA);
    n = n == (long)sizeof(smp_user_hdr_t) ? load(argv[2], (uint8_t *)host_slots + SMP_USER_DATA, SMP_USER_SIZE - SMP_USER_DATA)
                                           : -1;
    if (n <= 0) {
        fprintf(stderr, "drumusr: no slot image (%s / %s)\n", argv[1], argv[2]);
        return 2;
    }
    smp_user_scan(0);
    smp_user_scan(1);
    check("the slot made from the WAVs (as the editor uploads it): valid at the scan, its two zones", usr_nz[0] == 2u &&
          h->nz == 2u, "%u zones", h->nz);
    {
        const param_desc_t *d = &ENG_DRUM.edit[0];
        check("KIT: USR1..USR3 named so and reached by the knob (FELUCCA_USER_KITS; not STD's aliases)",
              str_eq(d->names[DK_USR1], "USR1") && str_eq(d->names[DK_USR3], "USR3") &&
              param_turn(d, DK_PERC, 1) == DK_USR1 && param_turn(d, DK_USR1, 1) == DK_USR2, "%s, PERC + 1: %d",
              d->names[DK_USR1], (int)param_turn(d, DK_PERC, 1));
    }
    t = kit_track(DK_USR1);
    kick = kit_play(t, 36, 600, y);
    for (j = 0; j < 64u; j++) {                          /* (the lane's onset: a few blocks in at most) */
        double sxy = 0, sxx = 0, syy = 0, c;
        for (i = 0; i < 6000u; i++) {
            double r = kick_ref(i), v = (double)y[j + i];
            sxy += r * v, sxx += r * r, syy += v * v;
        }
        c = syy > 0 ? sxy / sqrt(sxx * syy) : 0;
        if (c > bc)
            bc = c, best = j;
    }
    snare = kit_play(kit_track(DK_USR1), 38, 600, 0);
    bell = kit_play(kit_track(DK_USR1), 56, 600, 0);
    check("USR1: KICK plays the kick WAV (its 0.30 s; its shape, correlated), SNARE the snare's 0.15 s, BELL silent",
          kick * CTL >= 12000u && kick * CTL <= 14500u && bc > 0.9 && snare * CTL >= 5000u && snare * CTL <= 7600u && !bell,
          "kick %u ms (r %.3f at %u), snare %u ms, bell %u", kick * CTL * 1000u / FS, bc, best, snare * CTL * 1000u / FS,
          bell);
    t = kit_track(DK_USR1);
    t->p[P_LTUNE0] = 127;
    kick_up = kit_play(t, 36, 600, 0);
    t = kit_track(DK_USR1);
    t->p[P_LDECAY0] = -40;
    kick_short = kit_play(t, 36, 600, 0);
    check("the lane's TUNE up: faster (about half as long); its DECAY down: shorter",
          kick_up * 2u <= kick + 6u && kick_up * 2u + 6u >= kick && kick_short < kick * 3u / 4u, "%u %u of %u blocks",
          kick_up, kick_short, kick);
    empty = kit_play(kit_track(DK_USR2), 36, 100, 0);
    std = kit_play(kit_track(DK_STD), 36, 100, 0);
    check("an empty slot (USR2): silent; STD: its own KICK", !empty && std > 0, "%u %u", empty, std);
    printf(fails ? "drumusr: %d FAILED\n" : "drumusr: DRUM's user kits passed\n", fails);
    return fails != 0;
}
