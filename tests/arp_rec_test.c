/* SPDX-License-Identifier: GPL-3.0-only */
/* (custom) The ARP recorded into the sequencer stays on its steps however long it is held: each arp step as long as the
 * sequencer's (no whole blocks' rounding: 120 BPM, a 1/16 of 5512 samples, was 5536 a step, a step late in 15 bars),
 * and an arp started while the sequencer plays starts on its grid (a key late in a step: that step, on from the next
 * one on the grid; past its middle: the next step, on the grid). Every recorded step a NOTE, no TIEs, no FEEL nudge but
 * the first, late, note's. Stopped, the arp starts with the key as before.
 * Run by tests/run_tests.sh (needs build/gen from one firmware build). */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

#define K_C4 7u

static void blocks(uint32_t n) { while (n--) events_block(CTL); }

static void setup(void)
{
    track_t *t = &trk[0];
    uint32_t i;
    ui_power_on();
    fm1_in.notes = kb_prev = 0;
    song.g[G_BPM] = 120;
    t->p[P_VOICE] = V_MONO;
    t->p[P_SDIV] = 2;                              /* 1/16 */
    t->p[P_SLEN] = 16;
    t->p[P_ARATE] = 2;                             /* 1/16 */
    t->p[P_AMODE] = AM_UP;
    t->p[P_AGATE] = 64;
    t->p[P_ASWING] = 0;
    for (i = 0; i < NSTEP; i++) t->step[i] = (step_t){{0}, 0, ST_REST, 0, 0};
    rec_feel = 1;
    song.rec = 1;
    transport_req = 1;
    events_block(CTL);
}

/* steps 0..15 after holding: each a NOTE of note 60, nudge 0 but step first's (its own) */
static int steps_ok(uint32_t first, int32_t first_nudge)
{
    const track_t *t = &trk[0];
    uint32_t i, ok = 1;
    for (i = 0; i < 16u; i++) {
        const step_t *s = &t->step[i];
        int32_t want = i == first ? first_nudge : 0;
        if (s->time != ST_NOTE || s->n != 1u || s->note[0] != 60u || (want >= 0 && s->nudge != want)) {
            printf("  step %u: time %u n %u note %u nudge %d\n", i, s->time, s->n, s->note[0], s->nudge);
            ok = 0;
        }
    }
    return ok;
}

int main(void)
{
    int bad = 0;
    track_t *t = &trk[0];
    uint32_t per;

    setup();
    per = div_samples(2);
    bad += check("120 BPM: a 1/16 not a whole number of blocks (the case that drifted)", per % CTL != 0u);
    blocks(37);                                    /* into step 0, before its middle */
    key_down(K_C4);
    blocks(16u * 16u * per / CTL);                 /* 16 bars held */
    bad += check("16 bars of a held arp: on the steps still (its step's place = the sequencer's)",
                 t->arp_pos == t->seq_pos);
    bad += check("  every step a NOTE of it, no TIE, FEEL's nudge only on the first (late) one", steps_ok(0, -1));
    bad += check("  the first's nudge: late, under half a step", t->step[0].nudge > 0 && t->step[0].nudge < 32);
    key_up(K_C4);
    blocks(4);
    bad += check("  key up: nothing held, nothing sounding from the arp", !t->nheld && !t->arp_note);

    setup();
    blocks(2u * per / CTL + (per * 3u / 4u) / CTL);   /* step 2, past its middle */
    key_down(K_C4);
    bad += check("a key past a step's middle: nothing yet (the arp waits for the next step)", !t->arp_note);
    blocks(per / 4u / CTL + 2u);
    bad += check("  then it plays, on the step", t->arp_note == 60u && t->seq_idx == 3u);
    blocks(16u * 4u * per / CTL);
    bad += check("  4 bars on: on the steps", t->arp_pos == t->seq_pos);
    bad += check("  every step a NOTE, none nudged", steps_ok(99, 0));
    key_up(K_C4);
    blocks(4);

    transport_req = 2;
    blocks(2);
    t->p[P_AMODE] = AM_UP;
    key_down(K_C4);
    blocks(1);
    bad += check("stopped: the arp starts with the key", t->arp_note == 60u);
    key_up(K_C4);

    printf(bad ? "arp_rec_test: %d FAILED\n" : "arp_rec_test: all passed\n", bad);
    return bad != 0;
}
