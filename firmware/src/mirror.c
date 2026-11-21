/* SPDX-License-Identifier: GPL-3.0-only */
/* (custom) the screen over USB: what is painted also goes, when an app asks, to Patrick F. Kellogg's FM-1 Mirror
 * (github.com/pfkellogg/fm1-mirror), through his screen_mirror module (screen_mirror/, MIT; after OberVave's
 * obv_mirror.h), and the sound playing (mirror_sound). Nothing is sent unless an app asks (its hello, F0 7D 46 53
 * 00/01/02 F7, about once a second); 3 s after the last one it stops by itself. (The web editor's Screen tab is Skiff
 * 2.7's cast since the merge: cast.c.)
 *
 * lcd.c hands every blit and fill here (mirror_rows): the canvas's pixels are byte-swapped for the panel, the module
 * wants RGB565 as numbers. The rows queue in SM_BUFFER (4 KB: RAM is short; a whole screen fills it many times over,
 * so a full one waits for the USB as it goes); the hello is taken from the SysEx frame before the editor would leave
 * it (main.c: mirror_poll before ed_service). Included by felucca.c after usb.c. */
#if FELUCCA_OTA
#define SM_BUFFER 4096u
#define buffer sm_buffer                             /* (its file-scope names, unity build: kept apart from ours) */
#define length sm_length
#define sent sm_sent
#define on sm_on_
#define hello sm_hello
#define full sm_full
#define heard sm_heard
#include "screen_mirror/screen_mirror.c"
#undef buffer
#undef length
#undef sent
#undef on
#undef hello
#undef full
#undef heard

static uint16_t mr_row[240];
static uint8_t mr_lost;                              /* rows did not fit: everything drawn again */

static int mr_send_wait(const uint8_t *m, unsigned n)   /* (blocking: a picture's rows past the buffer) */
{
    return usb.config && !ota_wire_send(m, n) ? 0 : -1;
}
static int mr_send(const uint8_t *m, unsigned n)        /* (main loop: only what the USB queue takes now) */
{
    if (!usb.config)
        return -1;
    if (SXQ - (so_w - so_r) < (n + 2u) / 3u + 1u)
        return -2;
    return ota_wire_send(m, n) ? -1 : 0;
}

/* w x h pixels at (x, y) went to the panel: px (byte-swapped, the panel's order), or one colour c (px 0) */
static void mirror_rows(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px, uint16_t c)
{
    uint32_t r, i;
    if (!sm_on() || !w || w > 240u)
        return;
    for (r = 0; r < h && y + r < 240u; r++) {
        for (i = 0; i < w; i++) {
            uint32_t v = px ? px[r * w + i] : c;
            mr_row[i] = px ? (uint16_t)((v >> 8) | (v << 8)) : (uint16_t)v;
        }
        if (!sm_rows(mr_row, x, y + r, w, 1)) {
            sm_poll(fm1_ms, mr_send_wait);           /* full: send what waits, then this row */
            if (!sm_rows(mr_row, x, y + r, w, 1))
                mr_lost = 1;
        }
    }
}

/* (FM-1 Mirror 1.3) the sound playing, F0 7D 'F' 'S' 02 <name> F7 ("T1 SYNTH SAW LEAD", ASCII): after each hello
 * and whenever it changes, while an app asks; its hello 02 asks for only that, no screen. 3 s after the last hello it
 * stops, as the rows */
static void mirror_name(char *b);                    /* (ui_draw.c: the selected track, its engine, its sound; 40) */
static char mr_snd[40];
static uint8_t mr_snd_want;
static uint32_t mr_snd_heard, mr_snd_ms;

static void mirror_sound(void)
{
    char b[40];
    uint8_t m[48];
    uint32_t i, k = 0;
    if (!mr_snd_heard || fm1_ms - mr_snd_heard > 3000u) {
        mr_snd_heard = 0;
        return;
    }
    if (!mr_snd_want && fm1_ms - mr_snd_ms < 100u)
        return;                                      /* (looked at ten times a second) */
    mr_snd_ms = fm1_ms;
    mirror_name(b);
    if (!mr_snd_want && str_eq(b, mr_snd))
        return;
    m[k++] = 0xF0, m[k++] = 0x7D, m[k++] = 'F', m[k++] = 'S', m[k++] = 2;
    for (i = 0; b[i] && k < sizeof m - 1u; i++)
        m[k++] = (uint8_t)(b[i] >= 0x20 && b[i] < 0x7F ? b[i] : ' ');
    m[k++] = 0xF7;
    if (mr_send(m, k))
        return;                                      /* (the USB queue full: the next look) */
    str_cpy(mr_snd, b, sizeof mr_snd);
    mr_snd_want = 0;
}

/* every main-loop pass, before ed_service: an app's hello (taken), then what is queued; 1 = draw the whole screen
 * again (a new viewer, or rows lost) */
static int mirror_poll(void)
{
    const uint8_t *p;
    uint32_t n;
    if (ota_frame_get(&p, &n) && n == 4u && p[0] == 0x7Du && p[1] == 'F' && p[2] == 'S' && p[3] <= 2u) {
        const uint8_t m[6] = {0xF0, 0x7D, 'F', 'S', p[3], 0xF7};
        if (p[3] <= 1u)
            sm_sysex(m, 6);                          /* (02: the sound alone) */
        ota_frame_done();
        mr_snd_heard = fm1_ms ? fm1_ms : 1u;
        mr_snd_want = 1;
    }
    sm_poll(fm1_ms, mr_send);
    mirror_sound();
    if (sm_take_full_request() || (mr_lost && sm_on())) {
        mr_lost = 0;
        return 1;
    }
    return 0;
}
#else
static void mirror_rows(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px, uint16_t c)
{
    (void)x, (void)y, (void)w, (void)h, (void)px, (void)c;
}
static int mirror_poll(void) { return 0; }
#endif
