/* SPDX-License-Identifier: GPL-3.0-only */
/* MD: the drum models of Hortator (github.com/deadactive/hortator, by DEADACTIVE, GPL-3.0-only, itself built on
 * Felucca by Leo Kuroshita / Hugelton Instruments) played as lanes of Skiff's DRUM kits (eng_drum.c, the MD kits).
 *
 * The files beside this one are Hortator's, unchanged but for dm_state.h (its SAMPLE model's loop state left out of
 * the union: the SAMPLE, ACID and SKIN models are not ported). Hortator wrote them for its own track_t, whose
 * parameters P_E0..P_E7 are the model's eight knobs and nt the step's note offset; here each kit lane has an
 * md_trk_t of its own holding exactly that, so the models run as written. The TRX and E-FM families follow
 * measurements of an emulator of a classic digital drum machine (Hortator's note: measurements only, no code or data
 * of it); the M1-C models (BOOM PUNCH SNAP CRACK METAL HISS) are integer ports of Mutable Instruments' Plaits drums
 * (MIT, Emilie Gillet), their notices in dm_dsp.c.
 *
 * Names that would clash with Skiff's are kept to this file: Hortator's HZ, Q24 and PD macros, and its track type,
 * P_E0..P_E7 and F_* formats as macros for the length of the includes. */
#pragma push_macro("HZ")
#pragma push_macro("Q24")
#pragma push_macro("PD")
#pragma push_macro("PE")
#undef PE
#undef HZ
#undef Q24
#undef PD

typedef struct md_trk {
    int16_t p[8];                /* the model's knobs, as Hortator's P_E0..P_E7 */
    int8_t nt;                   /* the note offset (0: the lane's tuning is in p[0]) */
    int32_t wow, dtail;          /* (ACID's, drum_core's: unused by the models ported) */
} md_trk_t;
#define track md_trk
#define track_t md_trk_t
#define P_E0 0
#define P_E1 1
#define P_E2 2
#define P_E3 3
#define P_E4 4
#define P_E5 5
#define P_E6 6
#define P_E7 7
#define F_SEMI 0
#define F_INT 0
#define F_PCT 0
typedef struct { const char *label; int8_t fmt; int16_t min, max, def; } md_pd_t;
#define PD(l, f, mn, mx, df) {l, (int8_t)(f), mn, mx, df}
#define PE(l, n, df) {l, 0, 0, (int16_t)(sizeof(n) / sizeof(n[0]) - 1), df}

typedef struct { int32_t a1, a2, a3, k; } dsvf_t;
#include "dm_state.h"
typedef struct {                 /* one drum voice (Hortator's dvoice_t, its sample playback left out) */
    uint8_t active, vel;
    uint32_t age;
    uint32_t t;
    uint32_t ph[3], inc[3];
    int32_t env[3];
    uint32_t k[3];
    dsvf_t c[2];
    int32_t f[4];
    int32_t x[4];
    int32_t rng;
    int32_t last;
    dm_state_t ms;
} dvoice_t;
typedef struct {
    const char *name;
    uint8_t voices, choke;
    md_pd_t edit[8];
    void (*trigger)(md_trk_t *t, dvoice_t *v);
    void (*render)(md_trk_t *t, dvoice_t *v, int32_t *out, uint32_t n);
    uint8_t weight;              /* (Hortator's voice budget weight: unused here) */
} dmodel_t;
static uint32_t dvage;

#include "dm_dsp.c"
#include "dm_metal.c"
#include "dm_kick.c"
#include "dm_snare.c"
#include "dm_perc.c"
#include "dm_fm.c"
#include "dm_ktrx.c"
#include "dm_sdtrx.c"
#include "dm_xttrx.c"
#include "dm_trxn.c"
#include "dm_rstrx.c"
#include "dm_trxm.c"
#include "dm_trxcb.c"
#include "dm_cltrx.c"
#include "dm_b2trx.c"
#include "dm_bdefm.c"
#include "dm_efmn.c"
#include "dm_efmm.c"

/* the models, in the order MD_* names them (append only: the kits below store these numbers in no record, but keep
 * it stable anyway) */
enum { MDM_K808, MDM_K909, MDM_S808, MDM_S909, MDM_C808, MDM_C909, MDM_HATC, MDM_HATO, MDM_CYMB, MDM_COWB, MDM_TOM, MDM_CONGA,
       MDM_RIM, MDM_CLAVE, MDM_BOOM, MDM_PUNCH, MDM_SNAP, MDM_CRACK, MDM_METAL, MDM_HISS, MDM_FM, MDM_BDTRX, MDM_SDTRX, MDM_TMTRX,
       MDM_CGTRX, MDM_CPTRX, MDM_RSTRX, MDM_CBTRX, MDM_CHTRX, MDM_OHTRX, MDM_CYTRX, MDM_SHTRX, MDM_CLTRX, MDM_B2TRX, MDM_BDEFM,
       MDM_SDEFM, MDM_TMEFM, MDM_CPEFM, MDM_RSEFM, MDM_CBEFM, MDM_HHEFM, MDM_CYEFM, MDM_N };
static const dmodel_t MD_MODELS[MDM_N] = {
    DM_K808_DEF, DM_K909_DEF, DM_S808_DEF, DM_S909_DEF, DM_C808_DEF, DM_C909_DEF, DM_HATC_DEF, DM_HATO_DEF,
    DM_CYMB_DEF, DM_COWB_DEF, DM_TOM_DEF, DM_CONGA_DEF, DM_RIM_DEF, DM_CLAVE_DEF, DM_KBOOM_DEF, DM_KPUNC_DEF,
    DM_SSNAP_DEF, DM_SCRAK_DEF, DM_HMETL_DEF, DM_HNOIS_DEF, DM_FM_DEF, DM_KTRX_DEF, DM_SDTRX_DEF, DM_XTTRX_DEF,
    DM_XCTRX_DEF, DM_CPTRX_DEF, DM_RSTRX_DEF, DM_CBTRX_DEF, DM_CHTRX_DEF, DM_OHTRX_DEF, DM_CYTRX_DEF, DM_MATRX_DEF,
    DM_CLTRX_DEF, DM_B2TRX_DEF, DM_BDEFM_DEF, DM_SDEFM_DEF, DM_XTEFM_DEF, DM_CPEFM_DEF, DM_RSEFM_DEF, DM_CBEFM_DEF,
    DM_HHEFM_DEF, DM_CYEFM_DEF};

/* the MD kits: a model a lane (KICK SNARE CLAP CL HAT OP HAT TOM RIM BELL, eng_drum.c's lanes) */
#define MD_NKIT 6
static const uint8_t MD_KIT[MD_NKIT][8] = {
    {MDM_K808, MDM_S808, MDM_C808, MDM_HATC, MDM_HATO, MDM_TOM, MDM_RIM, MDM_COWB},             /* 808 */
    {MDM_K909, MDM_S909, MDM_C909, MDM_HATC, MDM_HATO, MDM_TOM, MDM_RIM, MDM_CYMB},             /* 909 */
    {MDM_BDTRX, MDM_SDTRX, MDM_CPTRX, MDM_CHTRX, MDM_OHTRX, MDM_TMTRX, MDM_RSTRX, MDM_CBTRX},   /* TRX: the digital machine's */
    {MDM_BDEFM, MDM_SDEFM, MDM_CPEFM, MDM_HHEFM, MDM_CYEFM, MDM_TMEFM, MDM_RSEFM, MDM_CBEFM},   /* EFM: its FM family */
    {MDM_B2TRX, MDM_SDTRX, MDM_CLTRX, MDM_SHTRX, MDM_CYTRX, MDM_CGTRX, MDM_CLAVE, MDM_CBTRX},   /* TRX-P: its percussion */
    {MDM_BOOM, MDM_SNAP, MDM_CRACK, MDM_METAL, MDM_HISS, MDM_CONGA, MDM_RIM, MDM_FM},           /* MI: Plaits' drums */
};

/* a lane: the model's knobs and its voice (in the DRUM lane, eng_drum.c drum_lane_t) */
typedef struct {
    md_trk_t trk;
    dvoice_t v;
    uint8_t model;               /* MD_* + 1, 0 = none yet */
    uint8_t again;               /* struck while it rang: eng_drum.c's declick takes the jump */
    uint8_t choke;               /* choked (the open hat by the closed): out over the next block */
} md_lane_t;

#undef track
#undef track_t
#undef P_E0
#undef P_E1
#undef P_E2
#undef P_E3
#undef P_E4
#undef P_E5
#undef P_E6
#undef P_E7
#undef F_SEMI
#undef F_INT
#undef F_PCT
#undef PD
#undef PE
#undef HZ
#undef Q24
#pragma pop_macro("PE")
#pragma pop_macro("PD")
#pragma pop_macro("Q24")
#pragma pop_macro("HZ")

/* the lane's knobs: the model's defaults, moved by the kit's knobs (Skiff's DRUM: PITCH TONE DECAY SNAP, each 64 the
 * design) and the lane's own TUNE and DECAY (EDIT > LANES); st the GM note's semitones */
static void md_knobs(md_trk_t *k, uint32_t model, const int16_t *p, uint32_t ln, int32_t st)
{
    const md_pd_t *e = MD_MODELS[model % MDM_N].edit;
    int32_t i, d[8] = {0};
    d[0] = st + ((p[P_E1] - 64) * 12) / 64 + ((p[P_LTUNE0 + ln] - 64) * 12) / 64;   /* +-12 semitones each */
    d[1] = (p[P_E3] - 64) + p[P_LDECAY0 + ln];
    d[2] = p[P_E2] - 64;                                /* TONE: the model's third knob */
    d[3] = p[P_E4] - 64;                                /* SNAP: its fourth */
    for (i = 0; i < 8; i++)
        k->p[i] = (int16_t)clamp(e[i].def + d[i], e[i].min, e[i].max);
    k->nt = 0;
}


/* a hit: Hortator's dv_init (velocity at full: Skiff's voice amplitude carries the hit's velocity), then the model */
static void md_hit(md_lane_t *M, uint32_t model)
{
    dvoice_t *v = &M->v;
    memset(v, 0, sizeof *v);
    v->active = 1;
    v->vel = 127;
    v->age = ++dvage;
    v->rng = (int32_t)(dvage * 2654435761u) | 1;
    MD_MODELS[model % MDM_N].trigger(&M->trk, v);
}
