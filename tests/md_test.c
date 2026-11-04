/* SPDX-License-Identifier: GPL-3.0-only */
/* Host test of the MD kits (custom: src/md/md.c, Hortator's drum models, played as lanes of the DRUM engine,
 * src/eng_drum.c):
 * 1. every lane of every MD kit sounds through the engine, under full scale, with no DC left, and its voice ends.
 * 2. the kit's knobs (PITCH TONE DECAY SNAP) and a lane's TUNE / DECAY change the hit; at 64 / 0 the design.
 * 3. a roll (16 hits a beat on every lane) and a kit change while hits ring (MD -> STD -> MD -> another MD kit):
 *    bounded, and the voices end.
 * 4. a closed hat chokes the open one.
 * 5. the cost: a beat on each kit, ns a block (host).
 * Demos: build/md_demo/beat_<KIT>.wav.
 *   cc -O2 -w -Ibuild/gen -Ifirmware/src -o /tmp/md_test tests/md_test.c -lm && /tmp/md_test build/md_demo */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <sys/stat.h>

static int bad;
static void ck(const char *w, int ok)
{
    printf("md: %-84s %s\n", w, ok ? "ok" : "FAIL");
    bad += !ok;
}
static const char *const KN[MD_NKIT] = {"808", "909", "TRX", "EFM", "TRX-P", "MI", "626"};
/* (custom) the KIT value of MD kit k: 808 .. MI in a row, 626 after DX+ */
#define MDK(k) ((int16_t)((k) + 1u < MD_NKIT ? DK_MD808 + (k) : DK_MD626))
static const uint8_t LANE_NOTE[8] = {36, 38, 39, 42, 46, 45, 37, 56};   /* eng_drum.c DRUM_LANE_NOTE */

static void kit_fresh(int16_t kit)
{
    host_tracks_init();
    memset(trk[0].v, 0, sizeof trk[0].v);
    memset(eng_mem, 0, sizeof eng_mem);
    host_preset(&trk[0], ENGI_DRUM, 0);
    trk[0].p[P_E0] = kit;
    trk[0].p[P_REV] = trk[0].p[P_DLY] = trk[0].p[P_CHOR] = 0;   /* (dry: no send's tail in the measures) */
}
static uint32_t voices_on(const track_t *t)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++)
        n += t->v[i].active;
    return n;
}
/* nb blocks of the mix: peak, mean (DC), and a hash */
static int32_t blocks(uint32_t nb, double *mean, uint64_t *h)
{
    int32_t o[2 * CTL], pk = 0;
    uint32_t i, k;
    double s = 0;
    for (i = 0; i < nb; i++) {
        mix_block(o, CTL);
        for (k = 0; k < 2u * CTL; k++) {
            pk = abs(o[k]) > pk ? abs(o[k]) : pk;
            s += o[k];
            if (h) {
                *h ^= (uint32_t)o[k];
                *h *= 0x100000001B3ull;
            }
        }
    }
    if (mean)
        *mean = s / (nb * 2.0 * CTL);
    return pk;
}

static const uint8_t BEAT[][3] = {
    {0, 36, 120}, {0, 42, 90}, {2, 42, 70}, {4, 38, 110}, {4, 42, 90}, {6, 42, 70}, {7, 36, 90}, {8, 36, 120},
    {8, 42, 90}, {10, 46, 90}, {12, 38, 110}, {12, 39, 100}, {14, 42, 80}, {15, 37, 90},
    {16, 36, 120}, {16, 42, 90}, {18, 42, 70}, {19, 56, 90}, {20, 38, 110}, {20, 42, 90}, {22, 42, 70},
    {23, 36, 90}, {24, 36, 120}, {24, 42, 90}, {26, 46, 90}, {27, 37, 90}, {28, 38, 110}, {28, 45, 100},
    {29, 45, 100}, {30, 45, 110}, {31, 39, 100},
};

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : "build/md_demo";
    char w[200];
    uint32_t k, l, i;
    mkdir("build", 0755);
    mkdir(dir, 0755);

    /* 0 (custom) a lane's own MODEL (EDIT > LANE MIX): STD's kick as the 909's, the 808 kit's kick as BOOM; KIT (0) the
     * kit's own (bit for bit as before); every model on a lane sounds, bounded, ends */
    {
        uint64_t h0 = 0, h1 = 0, h2 = 0, h3 = 0;
        int ok = 1;
        uint32_t m;
        kit_fresh(DK_STD);
        trk_note_on(&trk[0], 36, 110);
        blocks(FS / 4 / CTL, 0, &h0);
        kit_fresh(DK_STD);
        trk[0].p[P_LMDL0] = 1 + MDM_K909;
        trk_note_on(&trk[0], 36, 110);
        blocks(FS / 4 / CTL, 0, &h1);
        ok &= trk[0].v[0].active || voices_on(&trk[0]);
        kit_fresh(DK_MD909);
        trk_note_on(&trk[0], 36, 110);
        blocks(FS / 4 / CTL, 0, &h2);
        snprintf(w, sizeof w, "(custom) lane MODEL: STD's kick as K909: another sound than STD's (the 909 kit's model)");
        ck(w, h1 != h0 && h2 != h0);
        kit_fresh(DK_MD808);
        trk[0].p[P_LMDL0] = 1 + MDM_BOOM;
        trk_note_on(&trk[0], 36, 110);
        blocks(FS / 4 / CTL, 0, &h3);
        kit_fresh(DK_MD808);
        h0 = 0;
        trk_note_on(&trk[0], 36, 110);
        blocks(FS / 4 / CTL, 0, &h0);
        ck("  the 808 kit's kick as BOOM: another sound", h3 != h0);
        for (m = 0; m < MDM_N; m++) {
            double mean;
            int32_t pk;
            uint32_t n;
            kit_fresh(DK_STD);
            trk[0].p[P_LMDL0 + 2] = (int16_t)(1 + m);       /* (the CLAP lane) */
            trk_note_on(&trk[0], 39, 110);
            trk_note_off(&trk[0], 39);
            pk = blocks(FS / CTL, &mean, 0);
            for (n = 0; n < 12u * FS / CTL && voices_on(&trk[0]); n++)
                blocks(1, 0, 0);
            if (pk < 300 || pk > 32767 || fabs(mean) > 300 || voices_on(&trk[0]))
                ok = 0, printf("  model %s on CLAP: peak %d mean %.0f voices %u\n", N_LMDL[1 + m], (int)pk, mean,
                               voices_on(&trk[0]));
        }
        snprintf(w, sizeof w, "  every one of the %u models on a lane of STD: sounds, bounded, no DC, ends", (unsigned)MDM_N);
        ck(w, ok);
    }

    /* 1 */
    for (k = 0; k < MD_NKIT; k++) {
        int ok = 1;
        int32_t lo = 1 << 30, hi = 0;
        for (l = 0; l < 8u; l++) {
            double mean;
            int32_t pk;
            uint32_t n;
            kit_fresh(MDK(k));
            trk_note_on(&trk[0], LANE_NOTE[l], 110);
            trk_note_off(&trk[0], LANE_NOTE[l]);
            pk = blocks(FS / CTL, &mean, 0);            /* 1 s */
            for (n = 0; n < 12u * FS / CTL && voices_on(&trk[0]); n++)
                blocks(1, 0, 0);
            if (pk < 600 || pk > 32767 || fabs(mean) > 300 || voices_on(&trk[0])) {
                ok = 0;
                printf("  kit %s lane %u: peak %d mean %.0f voices %u\n", KN[k], l, (int)pk, mean, voices_on(&trk[0]));
            }
            lo = pk < lo ? pk : lo;
            hi = pk > hi ? pk : hi;
        }
        snprintf(w, sizeof w, "kit %-5s: every lane sounds (peaks %d..%d), no DC, its voice ends", KN[k], (int)lo, (int)hi);
        ck(w, ok);
    }

    /* 2 */
    {
        static const struct { uint32_t id; int16_t v; const char *n; } KNOB[] = {
            {P_E1, 100, "PITCH"}, {P_E2, 10, "TONE"}, {P_E3, 20, "DECAY"}, {P_E4, 120, "SNAP"},
            {P_LTUNE0, 30, "the KICK lane's TUNE"}, {P_LDECAY0, -30, "the KICK lane's DECAY"}};
        for (k = 0; k < MD_NKIT; k++) {
            int ok = 1;
            uint64_t h0 = 1469598103934665603ull;
            kit_fresh(MDK(k));
            trk_note_on(&trk[0], 36, 110);
            blocks(FS / 2 / CTL, 0, &h0);
            for (i = 0; i < sizeof KNOB / sizeof KNOB[0]; i++) {
                uint64_t h = 1469598103934665603ull;
                kit_fresh(MDK(k));
                trk[0].p[KNOB[i].id] = KNOB[i].v;
                trk_note_on(&trk[0], 36, 110);
                blocks(FS / 2 / CTL, 0, &h);
                if (h == h0 && i != 3)                  /* (SNAP: a fourth knob some kicks do not have) */
                    ok = 0, printf("  kit %s: %s changes nothing\n", KN[k], KNOB[i].n);
            }
            snprintf(w, sizeof w, "kit %-5s: PITCH TONE DECAY and a lane's TUNE / DECAY move the kick", KN[k]);
            ck(w, ok);
        }
    }

    /* 3 */
    {
        int ok = 1;
        int32_t pk = 0;
        uint32_t n;
        kit_fresh(DK_MD808);
        for (i = 0; i < 64u; i++) {                     /* a roll: a hit every 1/64 at 120 BPM, every lane */
            for (l = 0; l < 8u; l++)
                trk_note_on(&trk[0], LANE_NOTE[l], 60 + (i * 7) % 60);
            if (i == 16)
                trk[0].p[P_E0] = DK_STD;
            if (i == 24)
                trk[0].p[P_E0] = DK_MDTRX;
            if (i == 40)
                trk[0].p[P_E0] = DK_MDMI;
            if (i == 48)
                trk[0].p[P_E0] = DK_MDEFM;
            {
                int32_t p = blocks(FS / 16 / CTL, 0, 0);
                pk = p > pk ? p : pk;
            }
        }
        for (n = 0; n < 12u * FS / CTL && voices_on(&trk[0]); n++)
            blocks(1, 0, 0);
        ok = pk < 4 * 32767 && !voices_on(&trk[0]);
        snprintf(w, sizeof w, "a roll on every lane, kits changed while hits ring: bounded (peak %d), the voices end", (int)pk);
        ck(w, ok);
    }

    /* 4 */
    for (k = 0; k < MD_NKIT; k++) {
        double a, b;
        int32_t p1, p2;
        kit_fresh(MDK(k));
        trk_note_on(&trk[0], 46, 110);
        blocks(FS / 10 / CTL, 0, 0);
        p1 = blocks(FS / 10 / CTL, &a, 0);
        kit_fresh(MDK(k));
        trk_note_on(&trk[0], 46, 110);
        blocks(FS / 10 / CTL, 0, 0);
        trk_note_on(&trk[0], 42, 1);                    /* a closed hat, as quiet as can be */
        blocks(FS / 20 / CTL, 0, 0);
        p2 = blocks(FS / 20 / CTL, &b, 0);
        {
            int32_t pc;                                 /* the closed hat alone, the same window */
            kit_fresh(MDK(k));
            blocks(FS / 10 / CTL, 0, 0);
            trk_note_on(&trk[0], 42, 1);
            blocks(FS / 20 / CTL, 0, 0);
            pc = blocks(FS / 20 / CTL, 0, 0);
            snprintf(w, sizeof w, "kit %-5s: the closed hat chokes the open one (%d -> %d; the closed alone %d)", KN[k],
                     (int)p1, (int)p2, (int)pc);
            ck(w, p2 < p1 / 3 + pc + 50);
        }
    }

    /* 5 + demos */
    for (k = 0; k < MD_NKIT; k++) {
        uint32_t f, frames = 32u * (FS / 8u) + FS, e = 0, sixteenth = FS / 8u;
        uint64_t t0, ns = 0;
        uint32_t nb = 0;
        FILE *fp;
        snprintf(w, sizeof w, "%s/beat_%s.wav", dir, KN[k]);
        fp = fopen(w, "wb");
        if (!fp)
            continue;
        wav_hdr(fp, frames);
        kit_fresh(MDK(k));
        for (f = 0; f < frames; f += CTL) {
            int32_t o[2 * CTL];
            uint32_t q;
            while (e < sizeof BEAT / sizeof BEAT[0] && BEAT[e][0] * sixteenth <= f) {
                trk_note_on(&trk[0], BEAT[e][1], BEAT[e][2]);
                trk_note_off(&trk[0], BEAT[e][1]);
                e++;
            }
            t0 = now_ns();
            mix_block(o, CTL);
            ns += now_ns() - t0;
            nb++;
            for (q = 0; q < CTL; q++)
                wav_put(fp, o[2 * q], o[2 * q + 1]);
        }
        fclose(fp);
        printf("md: kit %-5s %6.0f ns a block (host, the mix with the beat)  -> %s\n", KN[k], (double)ns / nb, w);
    }
    printf(bad ? "md: %d FAILED\n" : "md tests passed\n", bad);
    return bad != 0;
}
