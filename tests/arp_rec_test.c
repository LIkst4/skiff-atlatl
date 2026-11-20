/* SPDX-License-Identifier: GPL-3.0-only */
/* (custom) The ARP recorded into the sequencer stays on its steps however long it is held: each arp step as long as the
 * sequencer's (no whole blocks' rounding: 120 BPM, a 1/16 of 5512 samples, was 5536 a step, a step late in 15 bars),
 * and an arp started while the sequencer plays starts on its grid (a key late in a step: that step, on from the next
 * one on the grid; past its middle: the next step, on the grid). Every recorded step a NOTE, no TIEs, no FEEL nudge but
 * the first, late, note's. Stopped, the arp starts with the key as before.
 * (custom, 2026-11-20) Held from before PLAY (REC from stop, a count-in, HOLD): the arp starts again from its first note
 * on step 0, on the grid (it went on half a step off: recorded a step early, against what was heard); a hold running
 * over the pattern writes its steps again, not chords of every pass (POLY); faster than DIV (ARP 1/32 on 1/16 steps):
 * one note a step, in order (it recorded the root's alone); stopped and armed, SEQ's step entry writes what the arp
 * plays, a step each from the cursor, for as long as it is held.
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
    bad += check("  step 0 written again by the passes after (on the grid): no nudge left", t->step[0].nudge == 0);
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

    /* (custom, 2026-11-20) a key held from before PLAY: the arp off the grid, REC from stop */
    {
        uint32_t i, ok = 1;
        setup();
        transport_req = 2;                         /* (stopped) */
        blocks(2);
        t->p[P_AOCT] = 2;
        key_down(K_C4);
        blocks(per / CTL * 3u + per / CTL / 2u);  /* (the arp half a step into its own free time) */
        transport_req = 1;                         /* PLAY, armed */
        blocks(1);
        ok = t->arp_note == 60u && t->seq_idx == 0u;
        bad += check("held from before PLAY: the arp starts again with PLAY, from its first note, on step 0", ok);
        blocks(16u * per / CTL);
        for (i = 0; i < 16u; i++)
            ok &= t->step[i].time == ST_NOTE && t->step[i].n == 1u && t->step[i].note[0] == (i & 1u ? 72u : 60u) &&
                  t->step[i].nudge == 0;
        bad += check("  recorded as heard: 60 72 60 72 .. from step 0, none a step early", ok);
        bad += check("  the arp's GATE (64) the sequencer's: its notes as long", t->p[P_SGATE] == 64);
        key_up(K_C4);
        blocks(4);
    }
    /* a hold over two passes, POLY, a 3-note cycle on 16 steps: each step the last pass's note, no chords */
    {
        uint32_t i, ok = 1;
        setup();
        t->p[P_VOICE] = V_POLY;
        t->p[P_AOCT] = 3;
        key_down(K_C4);
        blocks(2u * 16u * per / CTL - 4u);
        for (i = 0; i < 16u; i++)
            ok &= t->step[i].time == ST_NOTE && t->step[i].n == 1u && t->step[i].note[0] == 60u + 12u * ((16u + i) % 3u);
        bad += check("POLY, held over the pattern twice: each step the second pass's note alone, no chords", ok);
        key_up(K_C4);
        blocks(4);
    }
    /* faster than the steps: ARP 1/32 on 1/16 steps; and ARP 1/16 on 1/8 steps */
    {
        uint32_t i, r, ok;
        for (r = 0; r < 2u; r++) {
            setup();
            t->p[P_AOCT] = 2;
            t->p[P_ARATE] = r ? 2 : 3;
            t->p[P_SDIV] = r ? 1 : 2;
            key_down(K_C4);
            blocks(8u * per / CTL);                /* (16 arp notes at 1/32, 4 at 1/16 .. enough for 8 steps) */
            ok = 1;
            for (i = 0; i < 8u; i++)
                ok &= t->step[i].time == ST_NOTE && t->step[i].n == 1u && t->step[i].note[0] == (i & 1u ? 72u : 60u);
            bad += check(r ? "ARP 1/16 on 1/8 steps: one arp note a step, in order (60 72 60 72 ..)"
                           : "ARP 1/32 on 1/16 steps: one arp note a step, in order, not the root's alone", ok);
            bad += check("  a step each: the arp's GATE the sequencer's", t->p[P_SGATE] == 64);
            key_up(K_C4);
            blocks(4);
        }
    }
    /* stopped, armed, SEQ's step entry: what the arp plays, a step each from the cursor */
    {
        uint32_t i, ok = 1, n0;
        setup();
        transport_req = 2;
        blocks(2);
        song.rec = 1u << song.sel;
        t->p[P_AOCT] = 2;
        go_page(GR_ROLL);
        cursor_set(2);
        frame();
        key_down(K_C4);
        for (i = 0; i < 40u; i++) {                /* (frames with the audio's blocks between: ~5 arp steps) */
            frame();
            blocks(22);
        }
        key_up(K_C4);                              /* (what it played up to now still comes: the frame after) */
        frame();
        blocks(22);
        frame();
        n0 = ui.cursor;
        ok = n0 >= 6u && n0 <= 10u && !t->step[0].n && !t->step[1].n && t->step[n0].time == ST_REST;
        for (i = 2; i < n0; i++)
            ok &= t->step[i].time == ST_NOTE && t->step[i].n == 1u && t->step[i].note[0] == (i & 1u ? 72u : 60u);
        bad += check("stopped, armed, step entry: each arp note a step from the cursor (60 72 ..), the cursor past them",
                     ok);
        blocks(400);
        frame();
        bad += check("  let go: nothing more written", ui.cursor == n0);
        bad += check("  the arp's GATE the sequencer's", t->p[P_SGATE] == 64);
    }
    /* ARP 1/8 on 1/16 steps, playing: a note a step long, then a rest (its GATE 64 of 1/8: the whole 1/16) */
    {
        setup();
        t->p[P_ARATE] = 1;
        key_down(K_C4);
        blocks(4u * per / CTL);
        bad += check("ARP 1/8 on 1/16 steps, playing: a note then a rest, the GATE a whole step (127)",
                     t->step[0].time == ST_NOTE && t->step[1].time == ST_REST && t->step[2].time == ST_NOTE &&
                     t->p[P_SGATE] == 127);
        key_up(K_C4);
        blocks(4);
    }
    /* REC with the ARP on, stopped, away from SEQ: armed, no PLAY; the arp writes the steps from the cursor */
    {
        uint32_t i, ok;
        setup();
        transport_req = 2;
        blocks(2);
        song.rec = 0;
        t->p[P_AOCT] = 2;
        go_page(GR_ARP);
        cursor_set(0);
        frame();
        press(B_REC);
        blocks(2);
        ok = (song.rec & 1u) && !song.playing && !transport_req && !seq_counting() && msg_is("ARP WRITES STEPS");
        bad += check("REC with the ARP on, stopped, on the ARP page: armed, PLAY not started, ARP WRITES STEPS", ok);
        key_down(K_C4);
        for (i = 0; i < 40u; i++) {
            frame();
            blocks(22);
        }
        key_up(K_C4);
        frame();
        blocks(22);
        frame();
        ok = ui.cursor >= 4u && !song.playing;
        for (i = 0; i < ui.cursor; i++)
            ok &= t->step[i].time == ST_NOTE && t->step[i].note[0] == (i & 1u ? 72u : 60u);
        bad += check("  a key held: what the arp plays, a step each from step 1, still stopped", ok);
        t->p[P_AMODE] = 0;
        song.rec = 0;
        frame();
        press(B_REC);
        blocks(2);
        bad += check("  the ARP off: REC from stop starts PLAY, as before", transport_req == 1u || song.playing ||
                                                                           seq_counting());
        transport_req = 2;
        blocks(2);
    }

    printf(bad ? "arp_rec_test: %d FAILED\n" : "arp_rec_test: all passed\n", bad);
    return bad != 0;
}
