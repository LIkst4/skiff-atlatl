/* SPDX-License-Identifier: GPL-3.0-only */
/* (custom) ARP > ARP STEP: the track's arp step pattern (seq.c arp_pat), a sheet (ui.c
 * sheet_t) of one window, on every track. Included by ui_input.c.
 *   KNOB 1 STEPS     the pattern's length, 1..16 (16, all ON at 0: the arp as before)
 *   KNOB 2 STEP      the step the other two edit (1..STEPS; PRESETS moves it too)
 *   KNOB 3 TRANSP    its note's semitones up or down, -24..+24
 *   KNOB 4 STATE     ON, OFF (a rest: the arp's note passed over) or STACC (a quarter of the GATE)
 * The panel (graph_arp_step): the 16 steps, a bar each from the middle up or down by its TRANSPOSE (a thin one: STACC;
 * an outline: OFF; past STEPS dim), the step edited in the track's colour, the one playing marked under it */
static uint8_t arp_ui_step, arp_ui_win;
static const char *const ARP_ST_NAME[AS_N] = {"ON", "OFF", "STACC"};

static uint32_t arp_ui_trk(void) { return (uint32_t)(TSEL - trk) % NTRK; }
static void arp_step_turn(int32_t s)                    /* (PRESETS) the step edited */
{
    arp_ui_step = (uint8_t)clamp((int32_t)arp_ui_step + (s > 0 ? 1 : -1), 0, (int32_t)arp_len(arp_ui_trk()) - 1);
}
static const char *arp_step_title(void)                 /* the header's: "ARP STEP 5" */
{
    static char t[12];
    str_cpy(t, "ARP STEP ", sizeof t);
    fmt_int(t + 9, (int32_t)arp_ui_step + 1);
    return t;
}

static void arp_step_knob(uint32_t k, int32_t s)
{
    uint32_t a = arp_ui_trk(), i;
    arp_pat_t *P = &arp_pat[a];
    int32_t d = s > 0 ? 1 : -1;
    i = arp_ui_step % ARP_NS;
    switch (k & 3u) {
    case 0:                                             /* STEPS */
        P->len = (uint8_t)clamp((int32_t)arp_len(a) + accel(EN_K1, s, 15), 1, (int32_t)ARP_NS);
        if (arp_ui_step >= P->len)
            arp_ui_step = (uint8_t)(P->len - 1u);
        break;
    case 1:                                             /* STEP */
        arp_ui_step = (uint8_t)clamp((int32_t)arp_ui_step + accel(EN_K1 + 1u, s, 15), 0, (int32_t)arp_len(a) - 1);
        break;
    case 2:                                             /* TRANSPOSE */
        P->tr[i] = (int8_t)clamp(P->tr[i] + accel(EN_K1 + 2u, s, 48), -24, 24);
        break;
    default:                                            /* STATE */
        P->st[i] = (uint8_t)clamp((int32_t)(P->st[i] % AS_N) + d, 0, AS_N - 1);
        break;
    }
}

static void arp_step_cards(void)
{
    uint32_t a = arp_ui_trk(), i = arp_ui_step % ARP_NS, st = arp_pat[a].st[i] % AS_N;
    int32_t tr = arp_pat[a].tr[i];
    char v[12];
    fmt_int(v, (int32_t)arp_len(a));
    draw_column(0, "STEPS", v, "", VR_LIVE, (int32_t)arp_len(a) * 1000 / (int32_t)ARP_NS, ICON_STEPS);
    fmt_int(v, (int32_t)i + 1);
    draw_column(1, "STEP", v, "", VR_LIVE, ((int32_t)i + 1) * 1000 / (int32_t)arp_len(a), ICON_ORDER);
    v[0] = tr > 0 ? '+' : 0;
    v[1] = 0;
    fmt_int(v + (tr > 0), tr);
    draw_column(2, "TRANSP", v, "st", tr ? VR_LIVE : VR_DIM, 500 + tr * 500 / 24, ICON_TRANSPOSE);
    draw_column(3, "STATE", ARP_ST_NAME[st], "", st == AS_OFF ? VR_DIM : VR_LIVE, st == AS_ON ? 1000 : st == AS_STACC ? 500 : 0,
                ICON_GATE);
}

static uint32_t arp_step_sig(void)
{
    uint32_t a = arp_ui_trk(), h = arp_ui_step * 31u + arp_len(a) * 977u + arp_spos[a] * 7919u, i;
    for (i = 0; i < ARP_NS; i++)
        h = (h ^ (uint32_t)arp_pat[a].st[i] << 8 ^ (uint8_t)arp_pat[a].tr[i]) * 16777619u;
    return h;
}

/* the panel, from its top: 16 columns of 13 px, 1 apart (x 8 .. 231) */
static void graph_arp_step(const track_t *t, uint16_t c)
{
    uint32_t a = (uint32_t)(t - trk) % NTRK, len = arp_len(a), i, play = t->arp_note || t->nheld ?
                                                                       (arp_spos[a] + len - 1u) % len : 0xFFu;
    int32_t top = 10, h = (int32_t)cv_h - 34, mid = top + h / 2, half = h / 2 - 2;
    cv_rect(8, mid, 223, 1, T_RAISE);                      /* the 0 line */
    for (i = 0; i < ARP_NS; i++) {
        int32_t x = 8 + (int32_t)i * 14, tr = arp_pat[a].tr[i], bh = tr * half / 24, st = arp_pat[a].st[i] % AS_N;
        int sel = i == arp_ui_step % ARP_NS, in = i < len;
        uint16_t col = !in ? T_RAISE : sel ? c : T_MID;
        int32_t w = st == AS_STACC ? 5 : 13, xo = x + (13 - w) / 2, y0 = bh >= 0 ? mid - bh : mid, hh = bh >= 0 ? bh : -bh;
        if (hh < 2)
            hh = 2, y0 = mid - 1;
        if (st == AS_OFF && hh < 8)                        /* (a rest at 0: an outline you can see) */
            hh = 8, y0 = mid - 4;
        if (st == AS_OFF) {                                /* a rest: an outline */
            cv_rect(xo, y0, w, 1, col);
            cv_rect(xo, y0 + hh - 1, w, 1, col);
            cv_rect(xo, y0, 1, hh, col);
            cv_rect(xo + w - 1, y0, 1, hh, col);
        } else {
            cv_rect(xo, y0, w, hh, col);
        }
        cv_rect(x, top + h + 3, 13, 2, i == play && in ? c : in ? T_RAISE : cv_bg);   /* the steps' row, the one playing lit */
    }
}
