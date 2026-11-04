/* SPDX-License-Identifier: GPL-3.0-only */
/* (custom) Retired engines: ANALOG (0), SAMPLE (4), TRIO (6), WHEEL (7), NOISE (11); SLICE (13) when built without it
 * (FELUCCA_SLICE 0, the default: no sampler; its sounds load as DRUM KIT); DRUM-X (14): DRUM plays its voice as the DX
 * kits (eng_drum.c), and its sounds load there, its knobs as they were (retire_values). SYNTH makes their sounds (its VA WAVE ADD NOISE
 * ORGAN oscillators, OSC B's SYNC and RING, the filter library: eng_synth.c, its presets from SAW LEAD on), so their
 * DSP left the build to free flash for other things. Their numbers stay reserved, as DIGITAL's (fm4_convert.c): the
 * stores name engines by number and the table is append-only. Here each is ENG_RETIRED: no presets, never offered
 * (engines.c eng_ok, ENGINE_ORDER), silent. A sound of one that arrives anyway -- a project of any format, a user
 * preset, the editor's SET G_ENGSEL / PRESET / UP_PUT, a full backup, an old project's power-on sound -- loads as SYNTH
 * with the preset RETIRE_MAP names for its preset (ui.c retire_track, project.c proj_retire): the sound's own E values
 * do not carry over (another engine's meanings), the rest of the track (its envelope, effects, steps) stays. */

static void retired_note_on(struct track *t, voice_t *v) { (void)t; (void)v; }
static void retired_render(struct track *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    (void)t; (void)v; (void)out; (void)n; (void)m;
}
#define RT_P {"-", 0, 0, 127, 0, F_INT, 0}
static const engine_t ENG_RETIRED = {
    .name = "-",
    .page_title = {"-", "-"},
    .edit = {RT_P, RT_P, RT_P, RT_P, RT_P, RT_P, RT_P, RT_P},   /* (0..127: a stored value fits until it converts) */
    .note_on = retired_note_on,
    .render = retired_render,
    .knob = {P_E0, P_E1, P_E2, P_E3},
};
#undef RT_P

static int eng_retired(uint32_t e)
{
    if (e >= NENGINES)                   /* (not an engine: never one, as eng_ok) */
        return 0;
    return e == ENGI_ANALOG || e == ENGI_TRIO || e == ENGI_WHEEL || e == ENGI_NOISE || e == ENGI_DRUMX ||
           (!FELUCCA_SAMPLER && e == ENGI_SAMPLE) || (!FELUCCA_SLICE && e == ENGI_SLICE);
}
/* the engine a retired engine's sounds load as: DRUM-X's and SLICE's (breaks) DRUM, the others' SYNTH */
static uint32_t retire_engine(uint32_t e)
{
    e %= NENGINES;
    return e == ENGI_DRUMX || e == ENGI_SLICE ? ENGI_DRUM : ENGI_SYNTH;
}

/* the SYNTH preset each retired preset loads as (eng_synth.c SYNTH_PRESETS; tests/retire_test.c checks the names) */
static const uint8_t RETIRE_ANALOG[] = {
    22, 23, 24, 25, 4, 32, 22, 26,   /* SAW LEAD, SOFT PAD, SQR BASS, PWM STR, ACID (ACID LINE), SINE KEY, RAVE (SAW
                                      * LEAD), SUB BASS */
    27, 28, 40, 2, 16, 1, 2, 3,      /* SYNTH PLUCK, SYNTH BRASS, WIND SWEEP (WIND), STRINGS (OB STRINGS), BONE FLUTE (SY
                                      * FLUTE), WAR HORN (OB BRASS), DARK STRINGS (OB STRINGS), NIGHT BASS (LADDER BASS) */
    22, 23, 28, 33, 42, 29, 38, 30,  /* NEON LEAD (SAW LEAD), SUNSET PAD (SOFT PAD), HERO BRASS (SYNTH BRASS), SYN TOM,
                                      * QUANTA (ARCADE), REESE, DRIVE BASS (DIRTY BASS), HARD SYNC */
    31,                              /* SUB SAW */
};
static const uint8_t RETIRE_TRIO[] = {37, 22, 34, 35, 36, 34, 27, 39, 38};   /* FAT BASS, ARP LEAD (SAW LEAD), SYNC LEAD,
                                      * RING BELL, CHIP CHOIR, TURBO SYNC (SYNC LEAD), NEON ARP (SYNTH PLUCK), COLLIDER,
                                      * DIRTY BASS */
static const uint8_t RETIRE_NOISE[] = {40, 41, 42, 44, 13, 41, 13, 43};   /* WIND, RAIN, ARCADE, METAL, CAVE WIND (COMB
                                      * WIND), GEIGER (RAIN), PLASMA (COMB WIND), OCEAN */
static const uint8_t RETIRE_WHEEL[] = {45, 46, 47, 48, 49, 50, 51, 10};   /* FULL ORGAN, JAZZ PERC, GOSPEL, SOFT FLUTE,
                                      * ROCK DRIVE, CATHEDRAL, PEDAL BASS, CLARINET (ADD CLARINET) */

static const uint8_t RETIRE_SAMPLE[] = {14, 14, 16, 18, 14, 15, 20, 17, 19, 21};   /* PIANO (and its aliases) SY PIANO,
                                      * FLUTE SY FLUTE, SAX SY SAX, LOFI PIANO SY LOFI PNO, MUSIC BOX SY MUSICBOX, FLUTE
                                      * PAD SY FLUTE PAD, DARK SAX SY DARK SAX, KOTO SY KOTO (the factory samples' sounds
                                      * synthesized: SAMPLE retired) */

/* the preset of retire_engine(e) for preset pi of retired engine e (a number past its list: its first) */
static uint32_t retire_preset(uint32_t e, uint32_t pi)
{
    const uint8_t *m;
    uint32_t n;
    switch (e % NENGINES) {
    case ENGI_DRUMX: return BOARD_SMK37 ? 0u : DRUM_PDX + (pi < DRUM_NDX ? pi : 0u);   /* DRUM-X's: DRUM's DX presets */
    case ENGI_SLICE: return 0u;                       /* SLICE's breaks: DRUM KIT */
    case ENGI_SAMPLE: m = RETIRE_SAMPLE; n = sizeof RETIRE_SAMPLE; break;
    case ENGI_ANALOG: m = RETIRE_ANALOG; n = sizeof RETIRE_ANALOG; break;
    case ENGI_TRIO: m = RETIRE_TRIO; n = sizeof RETIRE_TRIO; break;
    case ENGI_WHEEL: m = RETIRE_WHEEL; n = sizeof RETIRE_WHEEL; break;
    default: m = RETIRE_NOISE; n = sizeof RETIRE_NOISE; break;
    }
    return m[pi < n ? pi : 0u] % NELEM(SYNTH_PRESETS);
}
