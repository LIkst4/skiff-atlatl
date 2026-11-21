/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Icons: 4-bit alpha cells of the Fukiai icon font (MIT), Huffman-coded (cv_alpha_hc), 12 px (parameters, lists) and 16 px
 * (header, dialogs, menu): web/fukiai.ttf -> build/gen/ui_icons.h
 * (tools/gen_aa_icons.py; names from assets/icons.json). Which icon a parameter gets is decided
 * here, by its label. FELUCCA_ICONS=0 turns the parameter icons off (labels get their full width back). */
#include "ui_icons.h"
#ifndef FELUCCA_ICONS
#define FELUCCA_ICONS 1
#endif
#define ICON_CELL 12

/* track k's indicator (1.0.2): not an icon of the font (its numerals alias at 12 to 16 px) but the digit 1..4 in
 * Inter Tight on a cushion, a rounded square in the keycap's language, drawn by cv_icon_on: the cushion in the
 * icon's ink (the selected track's: the accent), the digit in what lies under it; rounded in every STYLE, as the
 * keycaps are. (filled: kept for the callers; every cushion is filled) */
#define ICON_TRK 0xF0u                                /* + k (above ICON_COUNT, below ICON_AUTO) */
static uint32_t trk_icon(uint32_t k, int filled) { (void)filled; return ICON_TRK + k % NTRK; }
_Static_assert(NTRK <= UI_NTRK && ICON_TRK + NTRK <= 0xFEu,   /* ((Skiff phase 6) 0xFE: ICON_AUTO) */
               "a colour (tools/gen_ui_palettes.py) and an icon id for every track");
/* (Skiff) BLOCKS: no cushion, the number alone in the track's colour, as the chosen mockup's HOME lanes and PERFORM
 * (design/mock.py, B): in a 16 px cushion's place V (17 px figures), in a 12 px one's M (12 px); on a light ground (a
 * hot tile, a held cell) INK, a silent track DIM. Its cell: the figure's advance (tabular: 1..4 alike) by size rows,
 * the face's capitals' band (the figures') centred in them, as text is (CAP_IN). (The TAKE box's chip is ui_take.c's) */
static int trk_bare(uint32_t size) { return size <= 16u; }
static const aafont_t *trk_face(uint32_t size) { return size >= 16u ? &AF_V : &AF_M; }
#define TRK_CAP_H(size) ((size) >= 16u ? AF_V_CAP_H : AF_M_CAP_H)
#define TRK_TOP(size) HALF_UP((int32_t)(size) - TRK_CAP_H(size))   /* the capitals' first row in the cell */
static int32_t cv_trk(int32_t x, int32_t y, uint32_t size, uint32_t k, uint16_t fill, uint16_t under)
{
    const aafont_t *f = size > 16u ? &AF_M : &AF_S;
    char s[2] = {(char)('1' + k), 0};
    const aag_t *g = &f->g[glyph(f, (uint8_t)s[0])];
    int32_t in = size > 16u ? 2 : 0, w = (int32_t)size - 2 * in;
    int32_t nud = k == 0u ? 1 : 0;   /* the 1's ink centred looks right of centre (its flag): nudged left 1 px (2 looked left) */
    if (trk_bare(size)) {
        if (ux.neon) {                                /* ((Skiff N2) NEON: slanted, half its slant left: centred as before) */
            cv_fx_on(y + TRK_TOP(size) - (size >= 16u ? AF_V_CAP_Y : AF_M_CAP_Y), trk_face(size), 0);
            x -= cv_fx_w(trk_face(size)) / 2;
        }
        cv_text_on(x, y + TRK_TOP(size) - (size >= 16u ? AF_V_CAP_Y : AF_M_CAP_Y), trk_face(size), s,
                   fill == T_DIM ? T_DIM : under == T_BG || under == T_SURF || under == T_RAISE ? T_TRK(k) : T_INK, under);
        cv_fx_off();
        return text_w(trk_face(size), s);
    }
    if (fill != T_DIM)                                /* BLOCKS: every track in its own colour (the number on it) */
        fill = T_TRK(k);
    cv_rrect(x + in, y + in, w, w, -(w / 4), fill, under);
    GFX_HOOK_ALIGN(x + in - nud, y + in, x + in + w - nud, y + in + w, AL_HV, "track cushion digit");   /* (the 1: measured against its nudge) */
    cv_text_on(x + in + (w - g->bw) / 2 - g->bx - nud, y + in + (w - g->bh + 1) / 2 - g->by, f, s, under, fill);   /* its ink
                                                     * centred (up/down: a pixel left over goes down, Barlow's odd digits) */
    GFX_HOOK_ITEM(x + in, y + in, x + in + w, y + in + w);
    return (int32_t)size;
}

/* icon id's cell number at *size 12, 16 or 24 px (missing at 16 or 24: its 12 px cell, *size 12), -1: none */
static int32_t icon_cell(uint32_t *size, uint32_t id)
{
    const uint8_t *idx = *size == 24u ? AI24_IDX : *size == 16u ? AI16_IDX : AI12_IDX;
    if (id >= ICON_COUNT)
        return -1;
    if (idx[id] == 0xFFu) {
        if (*size == 12u || AI12_IDX[id] == 0xFFu)
            return -1;
        *size = 12u;
        idx = AI12_IDX;
    }
    return idx[id];
}
/* the ink box of icon id drawn at (0, 0) in a size px cell: b = x0 y0 x1 y1 (past the ink), from the generator's
 * table (tools/gen_aa_icons.py AI*_INK); a cushion: the cushion; nothing: the cell */
static void icon_ink(uint32_t size, uint32_t id, int32_t *b)
{
    uint32_t sz = size, v, i;
    int32_t k = id - ICON_TRK < NTRK ? -1 : icon_cell(&sz, id), in = id - ICON_TRK < NTRK && size > 16u ? 2 : 0;
    if (id - ICON_TRK < NTRK && trk_bare(size)) {   /* (Skiff) BLOCKS: the bare figure (cv_trk): across its ink, */
        char s[2] = {(char)('1' + (id - ICON_TRK)), 0};   /* up and down the capitals' band */
        text_ink(trk_face(size), s, b);
        b[1] = TRK_TOP(size);
        b[3] = b[1] + TRK_CAP_H(size);
        return;
    }
    if (k < 0) {
        b[0] = b[1] = in;
        b[2] = b[3] = (int32_t)size - in;
        return;
    }
    if (sz == 24u) {
        v = AI24_INK[k];                            /* x0 y0 x1 y1: a byte each */
    } else {                                        /* 12 16: a nibble each, x1-1 y1-1: as the 24's */
        v = sz == 16u ? AI16_INK[k] : AI12_INK[k];
        v = ((v & 15u) | (v & 0xF0u) << 4 | (v & 0xF00u) << 8 | (v & 0xF000u) << 12) + 0x01010000u;
    }
    for (i = 0; i < 4u; i++)
        b[i] = (int32_t)(v >> (8u * i) & 255u);
}

/* (Skiff phase 6) 1: the icons drawn now give the lint (ui_render.c) their ink, not their cell, as their box: a cell
 * with no room for an icon's empty margins (ui_layer.c l6_mute's speaker); 0 else */
static uint8_t icon_lint_ink;
/* icon id at size 12, 16 or 24 px (24: the header battery and USB, the FX map; one missing: its 12 px cell),
 * fg on bg; returns its width */
static int32_t cv_icon_on(int32_t x, int32_t y, uint32_t size, uint32_t id, uint16_t fg, uint16_t bg)
{
    int32_t k;
    if (id - ICON_TRK < NTRK)
        return cv_trk(x, y, size, id - ICON_TRK, fg, bg);
    if ((k = icon_cell(&size, id)) < 0)
        return 0;
    cv_alpha_hc(x, y, size, size, size == 24u ? AI24_DATA + AI24_OFF[k] : size == 16u ? AI16_DATA + AI16_OFF[k] :
                AI12_DATA + AI12_OFF[k], AI_HC, ramp(fg, bg));
    GFX_HOOK_ICON(x, y, size, id);
    if (GFX_HOOKS && icon_lint_ink) {                /* ((Skiff phase 6) a cell with no room for its empty margins) */
        int32_t b[4];
        icon_ink(size, id, b);
        GFX_HOOK_TEXT(x + b[0], y + b[1] + cv_oy, x + b[2], y + b[3] + cv_oy, "icon", 4u);
    } else {
        GFX_HOOK_TEXT(x, y + cv_oy, x + (int32_t)size, y + cv_oy + (int32_t)size, "icon", 4u);
    }
    return (int32_t)size;
}
/* the same, its ink centred across x .. x + w (w 0: the cell at x) and up and down in y .. y + h (h 0: the cell at y);
 * a pixel left over goes left / up */
static int32_t cv_icon_in(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t size, uint32_t id, uint16_t fg, uint16_t bg)
{
    int32_t b[4];
    icon_ink(size, id, b);
    if (w)
        x += HALF_UP(w - (b[2] - b[0])) - b[0];
    if (h)
        y += HALF_UP(h - (b[3] - b[1])) - b[1];
    return cv_icon_on(x, y, size, id, fg, bg);
}
/* the same, its ink centred on the line cy (between rows cy - 1 and cy: the middle of rows 0 .. 2 cy - 1, the odd row
 * left over going up; header icons: each glyph sits differently in its cell) */
static int32_t cv_icon_mid(int32_t x, int32_t cy, uint32_t size, uint32_t id, uint16_t fg, uint16_t bg)
{
    GFX_HOOK_ALIGN(0, 0, 0, 2 * cy, AL_V | AL_PASS, "header icon on its middle");
    return cv_icon_in(x, 0, 0, 2 * cy, size, id, fg, bg);
}
#define ICON_NONE 0xFFu                   /* no icon (empty column) */
#define ICON_AUTO 0xFEu                   /* draw_column: look the label up */
#define ICON_GAP 2                        /* px between the icon and the label */

#if FELUCCA_ICONS
typedef struct {
    const char *label;
    uint8_t icon;
} icon_map_t;

/* label -> icon. Labels shared by several parameters (RATE, WAVE, SET) are
 * told apart by descriptor in param_icon(). Unknown labels get ICON_GENERIC. */
/* ((custom) the labels and their icons apart, as two lists from one: a pointer and a byte each, not an 8-byte pair) */
#define ICON_MAP_LIST(IM) \
    /* track: ENV, LFO, destinations */ \
    IM("LVL", ICON_LEVEL) IM("ATK", ICON_ATTACK) IM("DEC", ICON_DECAY) IM("SUS", ICON_SUSTAIN) \
    IM("REL", ICON_RELEASE) IM("FLT", ICON_CUTOFF) IM("SHP", ICON_SHAPE) \
    IM("FX", ICON_MOD) IM("RATE", ICON_RATE) IM("WAVE", ICON_WAVE) \
    IM("FADE", ICON_FADE) IM("AMP", ICON_LEVEL) \
    /* arp, scale, pattern */ \
    IM("MODE", ICON_ARP) IM("OCT", ICON_OCTAVE) IM("GATE", ICON_GATE) \
    IM("HOLD", ICON_HOLD) IM("ORD", ICON_ORDER) IM("ROOT", ICON_PITCH) \
    IM("LEN", ICON_LENGTH) \
    IM("DIV", ICON_DIVISION)   /* (CHRD: below, PHYS's) */ \
    /* fx sends, voice */ \
    IM("CHO", ICON_CHORUS) IM("DLY", ICON_DELAY) IM("REV", ICON_REVERB) \
    IM("ALLOC", ICON_VOICE) IM("PAN", ICON_PAN) IM("MUTE", ICON_MUTE) \
    /* global */ \
    IM("BPM", ICON_TEMPO) IM("CLK", ICON_TEMPO) IM("TUNE", ICON_TUNE) IM("TIME", ICON_TIME) \
    IM("FDBK", ICON_FEEDBACK) IM("MIX", ICON_MIX) IM("SIZE", ICON_SIZE) \
    IM("DAMP", ICON_DAMP) IM("TYPE", ICON_REVERB) IM("MIDI", ICON_MIDI) \
    IM("SYNC", ICON_TEMPO) IM("CPU", ICON_CHIP) IM("SLOT", ICON_SAVE) \
    IM("LOAD", ICON_LOAD) IM("SAVE", ICON_SAVE) IM("ENG", ICON_WAVE) IM("CLRSQ", ICON_CLEAR) \
    IM("INIT", ICON_CLEAR) IM("ERASE", ICON_CLEAR) IM("CH", ICON_MIDI) IM("LEVEL", ICON_LEVEL) \
    /* engines (eng_*.c edit[] labels) */ \
    IM("NOIS", ICON_NOISE) IM("CUT", ICON_CUTOFF) IM("RES", ICON_RESO) \
    IM("ALG", ICON_ALGORITHM) IM("R2", ICON_RATIO) \
    IM("R3", ICON_RATIO) IM("R4", ICON_RATIO) IM("IDX", ICON_MOD) IM("MDEC", ICON_DECAY) \
    IM("FB", ICON_FEEDBACK) IM("CHIP", ICON_CHIP) IM("DUTY", ICON_PULSE) \
    IM("VIB", ICON_VIBRATO) IM("ARP", ICON_ARP) IM("TONE", ICON_TONE) \
    IM("SET", ICON_SAMPLE) IM("BITS", ICON_BITS) IM("LOOP", ICON_LOOP) IM("WAVE2", ICON_WAVE) \
    IM("DCW", ICON_SHAPE) IM("ENV", ICON_ENV) IM("LINE", ICON_MIX) IM("SUB", ICON_SUB) \
    IM("FOLD", ICON_FOLD) IM("WAV#", ICON_WAVE) IM("DCY", ICON_DECAY) \
    IM("PW", ICON_PULSE) \
    IM("REG", ICON_DRAWBAR) IM("BODY", ICON_LEVEL) IM("TOP", ICON_TONE) IM("PERC", ICON_DECAY) \
    IM("CLICK", ICON_ATTACK) \
    IM("SRC", ICON_SAMPLE) IM("START", ICON_STEPS) \
    IM("POS", ICON_PHASE) \
    IM("MODEL", ICON_PHYS) IM("BOW", ICON_SUSTAIN)   /* PHYS */ \
    IM("HARM", ICON_RATIO) IM("BEND", ICON_SWEEP) IM("CHRD", ICON_SCALE) IM("SYMP", ICON_MIX) \
    IM("SNAP", ICON_NOISE) IM("KIT", ICON_SAMPLE) IM("KICK", ICON_ATTACK)       /* PHYS: MEMB, SYMP; DRUM */ \
    IM("TALK", ICON_SWEEP) IM("SHIFT", ICON_TRANSPOSE) \
    IM("BUZZ", ICON_PULSE) IM("Q", ICON_RESO) IM("RAND", ICON_PROB) \
    IM("SLOT", ICON_LOAD)                                                    /* FM6 (ALG, FB: above) */ \
    /* fixed columns drawn by ui_draw.c (STEP page, preset browser, SYSTEM) */ \
    IM("NOTE", ICON_PITCH) IM("STEP", ICON_STEPS) IM("FLAG", ICON_ACCENT) IM("ACC", ICON_ACCENT) IM("LANE", ICON_DRUM) IM("HIT", ICON_GATE) IM("SLD", ICON_SLIDE) IM("USB", ICON_MIDI) \
    IM("TRACK", ICON_MIX)                  /* TRACKS page (LEVEL, LEN, PAN: above) */ \
    IM("PAT", ICON_STEPS) IM("DEPTH", ICON_MIX)   /* SLICER page (RATE: param_icon) */ \
    /* (Skiff U4) the words spelled out (L1, L3): the same icons */ \
    IM("ATTACK", ICON_ATTACK) IM("DECAY", ICON_DECAY) IM("SUSTAIN", ICON_SUSTAIN) IM("RELEASE", ICON_RELEASE) \
    IM("CUTOFF", ICON_CUTOFF) IM("PITCH", ICON_PITCH) IM("SHAPE", ICON_SHAPE) IM("PHASE", ICON_PHASE) IM("OCTAVE", ICON_OCTAVE) \
    IM("SWING", ICON_SWING) IM("SWING+", ICON_SWING) IM("CHANCE", ICON_PROB) IM("ORDER", ICON_ORDER) IM("SCALE", ICON_SCALE) \
    IM("QUANTIZE", ICON_QUANTIZE) IM("TRANSP", ICON_TRANSPOSE) IM("LENGTH", ICON_LENGTH) IM("DIST", ICON_DIST) \
    IM("CHORUS", ICON_CHORUS) IM("DELAY", ICON_DELAY) IM("REVERB", ICON_REVERB) IM("VOICE", ICON_VOICE) IM("GLIDE", ICON_GLIDE) \
    IM("GL MODE", ICON_GLIDE) IM("PRIORITY", ICON_ORDER) IM("DETUNE", ICON_DETUNE) IM("SLICER", ICON_SLICE) \
    IM("PATTERN", ICON_STEPS) IM("CHORD", ICON_SCALE) IM("VOICING", ICON_OCTAVE) IM("CLOCK", ICON_TEMPO) \
    IM("FEEDBACK", ICON_FEEDBACK) IM("COLOR", ICON_TONE) IM("ROUTE", ICON_MIX) IM("ENGINE", ICON_WAVE) IM("NOISE", ICON_NOISE) \
    IM("DRIVE", ICON_DRIVE) IM("KEYTRK", ICON_KEYTRACK) IM("CRUSH", ICON_BITS) IM("SWEEP", ICON_SWEEP) \
    IM("VIBRATO", ICON_VIBRATO) IM("VOWEL", ICON_VOICE) IM("VOWEL2", ICON_VOICE) IM("BREATH", ICON_NOISE) \
    IM("RANDOM", ICON_PROB) IM("PITCH2", ICON_TRANSPOSE) IM("PITCH3", ICON_TRANSPOSE) IM("ROTOR", ICON_VIBRATO) \
    IM("SOURCE", ICON_SAMPLE) IM("DENSITY", ICON_GRAIN) IM("SPREAD", ICON_NOISE) IM("STRUCT", ICON_SHAPE) \
    IM("BRIGHT", ICON_TONE) IM("ACCENT", ICON_ACCENT) IM("EXCITE", ICON_MIX) IM("DRIFT", ICON_SWEEP) IM("ALGO", ICON_ALGORITHM) \
    IM("MOD LVL", ICON_MOD) IM("RATIO", ICON_RATIO) IM("MOD ENV", ICON_DECAY) IM("VEL MOD", ICON_ACCENT) IM("PATCH", ICON_LOAD) \
    IM("RVRS", ICON_X_REVERSE) IM("MORPH", ICON_SHAPE) IM("VELOCITY", ICON_ACCENT) IM("RATCHET", ICON_STEPS)
#define IM_LABEL(l, i) l,
#define IM_ICON(l, i) i,
static const char *const ICON_MAP_L[] = {ICON_MAP_LIST(IM_LABEL)};
static const uint8_t ICON_MAP_I[] = {ICON_MAP_LIST(IM_ICON)};

static uint32_t icon_for_label(const char *l)
{
    uint32_t i;
    if (!l || !l[0] || l[0] == '-')
        return ICON_NONE;
    for (i = 0; i < sizeof(ICON_MAP_I); i++)
        if (str_eq(l, ICON_MAP_L[i]))
            return ICON_MAP_I[i];
    return ICON_GENERIC;
}

/* WAVE / WAVE2 set to a shape: that shape's icon (value names of the engines and the LFO) */
static const icon_map_t WAVE_ICON[] = {
    {"SIN", ICON_W_SIN}, {"TRI", ICON_W_TRI}, {"SAW", ICON_W_SAW}, {"SQR", ICON_W_SQR},
    {"PLS", ICON_W_PLS}, {"PWM", ICON_W_PWM}, {"S&H", ICON_W_SH}, {"NOIS", ICON_NOISE},
    {"DSIN", ICON_W_DSIN}, {"SPLS", ICON_W_SPLS}, {"RSAW", ICON_W_RSAW}, {"RTRI", ICON_W_RTRI},
    {"RTRP", ICON_W_RTRP},
};

static uint32_t param_icon(const param_desc_t *d, int32_t v)
{
    uint32_t i;
    if (!d)
        return ICON_NONE;
    if (d->fmt == F_ENUM && d->names && v >= d->min && v <= d->max &&
        (str_eq(d->label, "WAVE") || str_eq(d->label, "WAVE2")))
        for (i = 0; i < sizeof(WAVE_ICON) / sizeof(WAVE_ICON[0]); i++)
            if (str_eq(d->names[v], WAVE_ICON[i].label))
                return WAVE_ICON[i].icon;
    if (d == &TP[P_LWAVE])
        return ICON_LFO_WAVE;                 /* "WAVE" is also the oscillator wave */
    if (d == &TP[P_ARATE] || d == &TP[P_SLRATE])
        return ICON_DIVISION;                 /* arp / SLICER RATE is a note division, not Hz */
    if (d->names == N_FMODE)
        return ICON_TRANSPOSE;                /* ((2.1) FOLLOW's MODE: how the notes move, not the arp mode) */
    if (d == &TP[P_FOLLOW])
        return ICON_SCALE;                    /* ((2.1) the track whose chords it follows, as CHORD's) */
#if FELUCCA_SLICE
    if (d->names == N_SLC_DIV)
        return ICON_SLICE;                    /* SLICE: DIV is the slicing, MODE the gate, REV the direction */
    if (d->names == N_SLC_MODE)
        return ICON_GATE;
    if (d->names == N_SLC_REV)
        return ICON_ORDER;
#endif
    return icon_for_label(d->label);
}

/* the MOD page: the icon of a source, of a destination (an engine parameter: its own) */
static uint32_t mod_src_icon(int32_t s)
{
    static const uint8_t I[MS_N] = {ICON_MOD, ICON_LFO_WAVE, ICON_ENV, ICON_ACCENT, ICON_KEYTRACK, ICON_PROB,
                                    ICON_MOD, ICON_MIDI, ICON_LEVEL, ICON_ENV};   /* ((Skiff phase 9) ENV2: ENV's) */
    return I[clamp(s, 0, MS_N - 1)];
}
static uint32_t mod_dst_icon(const track_t *t, int32_t d)
{
    static const uint8_t I[MD_E1] = {ICON_MOD, ICON_PITCH, ICON_CUTOFF, ICON_SHAPE, ICON_LEVEL, ICON_PAN, ICON_DIST,
                                     ICON_CHORUS, ICON_DELAY, ICON_REVERB, ICON_RATE, ICON_VIBRATO};
    uint32_t id;
    d = clamp(d, 0, MD_N - 1);
    if (d < MD_E1)
        return I[d];
    id = P_E0 + (uint32_t)(d - MD_E1);
    return param_icon(track_desc(t, id), t->p[id]);
}

/* engine name (ENGINES[]->name) -> icon */
static uint32_t engine_icon(const char *name)
{
    static const icon_map_t M[] = {
        {"ANALOG", ICON_WAVE},
#if FELUCCA_FM4
        {"DIGITAL", ICON_ALGORITHM},
#endif
        {"PHASE", ICON_PHASE}, {"LOFI", ICON_BITS},
        {"SAMPLE", ICON_SAMPLE}, {"VOICE", ICON_MOUTH}, {"TRIO", ICON_TRIO}, {"WHEEL", ICON_DRAWBAR},
        {"SLICE", ICON_SLICE},
        {"GRAIN", ICON_GRAIN},
        {"PHYS", ICON_PHYS},
        {"DRUM", ICON_DRUM},
        {"DRUM-X", ICON_DRUM},
        {"NOISE", ICON_NOISE},
        {"FM6", ICON_MOD},                    /* (symbol_modular: six operators patched; a glyph of its own wanted) */
    };
    uint32_t i;
    for (i = 0; i < sizeof M / sizeof M[0]; i++)
        if (str_eq(name, M[i].label))
            return M[i].icon;
    return ICON_GENERIC;
}

/* MOTION: the knob and its trace; recording into it (REC armed on the selected track, playing): its REC form */
static uint32_t motion_icon(void) { return rec_on(TSEL) ? ICON_X_MOTION_REC : ICON_X_MOTION; }

/* the icon beside a page's title in the footer (the pages that have one of their own; else ICON_NONE) */
static uint32_t page_icon(const page_t *pg)
{
    switch (pg->graph) {
    case GR_TRK: return ICON_X_MIXER;         /* MIXER (GLO): vertical faders */
    case GR_BANK: return ICON_X_PATTERN;      /* (Skiff) SAVE > PATTERNS: the bank */
    case GR_SONG: return ICON_X_SONG;         /* (Skiff phase 5) SONG: the disc */
    case GR_FEEL: return ICON_PROB;           /* (Skiff 2.0) FEEL: the die (CHANCE's) */
    case GR_EVENTS: return motion_icon();     /* AUTOMATION ((Skiff U3) the list) */
    default: return ICON_NONE;
    }
}

#else
static uint32_t icon_for_label(const char *l) { (void)l; return ICON_NONE; }
static uint32_t param_icon(const param_desc_t *d, int32_t v) { (void)d; (void)v; return ICON_NONE; }
static uint32_t engine_icon(const char *name) { (void)name; return ICON_NONE; }
static uint32_t mod_src_icon(int32_t s) { (void)s; return ICON_NONE; }
static uint32_t mod_dst_icon(const track_t *t, int32_t d) { (void)t; (void)d; return ICON_NONE; }
static uint32_t motion_icon(void) { return ICON_NONE; }
static uint32_t page_icon(const page_t *pg) { (void)pg; return ICON_NONE; }
#endif
