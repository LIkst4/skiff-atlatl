/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
#define FELUCCA_FAVORITES 1
/* Stable engine/preset references; fixed capacity independent of optional engines. */
typedef struct {
    uint8_t factory[16][32]; /* engine 0..15, preset 0..255 */
    uint32_t user, filter;
} favorites_t;
static favorites_t favorites;
/* (custom) the row of engine e's presets: SYNTH's and FMINI's (past 15: row 15 holds the settings) the rows of ANALOG and
 * TRIO (retired: retire.c), a retired engine's none (-1) */
static int32_t fav_row(uint32_t e)
{
#ifdef ENGI_FMINI                         /* (the firmware; tests/settings_test.c builds this alone) */
    if (e == ENGI_SYNTH)
        return (int32_t)ENGI_ANALOG;
    if (e == ENGI_FMINI)
        return (int32_t)ENGI_TRIO;
    return e < 15u && !eng_retired(e) ? (int32_t)e : -1;
#else
    return e < 15u ? (int32_t)e : -1;
#endif
}
static int favorite_has(uint32_t engine, uint32_t preset)
{
    int32_t r;
    if (engine == NENGINES)
        return preset < UP_SLOTS && ((favorites.user >> preset) & 1u);
    r = engine < NENGINES ? fav_row(engine) : -1;
    return r >= 0 && preset < 256u && ((favorites.factory[r][preset / 8u] >> (preset % 8u)) & 1u);
}
static int favorite_set(uint32_t engine, uint32_t preset, int on)
{
    int32_t r = engine < NENGINES ? fav_row(engine) : -1;
    if (engine > NENGINES || (engine == NENGINES ? preset >= UP_SLOTS : r < 0 || preset >= 256u))
        return 0;
    if (favorite_has(engine, preset) == !!on) return 0;
    if (engine == NENGINES) {
        if (on) favorites.user |= 1u << preset;
        else favorites.user &= ~(1u << preset);
    } else {
        uint8_t *b = &favorites.factory[r][preset / 8u];
        if (on) *b |= (uint8_t)(1u << (preset % 8u));
        else *b &= (uint8_t)~(1u << (preset % 8u));
    }
    return 1;
}
