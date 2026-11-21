/* SPDX-License-Identifier: GPL-3.0-only */
/* (custom) the screen mirror (firmware/src/mirror.c, screen_mirror/ MIT): nothing goes out until an app's hello; a
 * hello asks for the whole screen; what is painted then (blits, the panel's byte order, and fills) arrives as rows that
 * rebuild the picture exactly; a busy USB queue waits; 3 s after the last hello it stops. (FM-1 Mirror 1.3) the sound
 * playing (F0 7D 'F' 'S' 02 name F7) after each hello and when it changes; a hello 02 asks for it alone.
 *   cc -O1 -Ifirmware/src -o build/host/mirror_test tests/mirror_test.c && build/host/mirror_test */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define FELUCCA_OTA 1
static volatile uint32_t fm1_ms;
static struct { uint8_t config; } usb = {1};
#define SXQ 64u
static uint32_t so_w, so_r;
static uint16_t screen[240][240];                   /* what the app rebuilt */
static uint32_t msgs;
static const uint8_t *frame_p;
static uint32_t frame_n, frame_ready;

static char snd[48];                                /* the app's: the last sound named */
static uint32_t snds;
static int ota_wire_send(const uint8_t *m, uint32_t n)   /* the app's side: decode the row */
{
    uint32_t y, x, k;
    if (n >= 6u && m[0] == 0xF0 && m[1] == 0x7D && m[2] == 'F' && m[3] == 'S' && m[4] == 2 && m[n - 1] == 0xF7) {
        memcpy(snd, m + 5, n - 6u);
        snd[n - 6u] = 0;
        snds++;
        return 0;
    }
    if (n < 11u || m[0] != 0xF0 || m[1] != 0x7D || m[2] != 'F' || m[3] != 'S' || m[4] != 1 || m[n - 1] != 0xF7 ||
        n > 70u)
        return -1;
    y = m[5] | (uint32_t)m[6] << 7;
    x = m[7] | (uint32_t)m[8] << 7;
    for (k = 9; k + 4u <= n - 1u; k += 4u) {
        uint32_t run = m[k], c = m[k + 1] | (uint32_t)m[k + 2] << 7 | (uint32_t)m[k + 3] << 14;
        while (run-- && x < 240u && y < 240u)
            screen[y][x++] = (uint16_t)c;
    }
    msgs++;
    return 0;
}
static int ota_frame_get(const uint8_t **p, uint32_t *n)
{
    if (!frame_ready)
        return 0;
    *p = frame_p, *n = frame_n;
    return 1;
}
static void ota_frame_done(void) { frame_ready = 0; }

static int str_eq(const char *a, const char *b) { return !strcmp(a, b); }
static void str_cpy(char *d, const char *s, unsigned max)
{
    unsigned i = 0;
    while (s[i] && i + 1 < max) {
        d[i] = s[i];
        i++;
    }
    d[i] = 0;
}
static const char *playing = "T1 SYNTH SAW LEAD";   /* (ui_draw.c's, the selected track's) */
static void mirror_name(char *b) { str_cpy(b, playing, 40); }
#include "mirror.c"

static int bad;
static void check(const char *w, int ok)
{
    printf("mirror: %-92s %s\n", w, ok ? "ok" : "FAIL");
    bad += !ok;
}
static void hello(uint8_t full)
{
    static uint8_t f[4] = {0x7D, 'F', 'S', 0};
    f[3] = full;
    frame_p = f, frame_n = 4, frame_ready = 1;
}
static uint16_t sw(uint16_t v) { return (uint16_t)((v >> 8) | (v << 8)); }

int main(void)
{
    static uint16_t canvas[240 * 60];
    uint32_t x, y, ok, full;
    for (y = 0; y < 60u; y++)
        for (x = 0; x < 240u; x++)                   /* (stripes, a gradient: runs of every length) */
            canvas[y * 240u + x] = sw((uint16_t)(((x / 7u) & 1u) ? 0xF800u : (x * 37u + y * 11u) & 0xFFFFu));
    mirror_rows(0, 0, 240, 60, canvas, 0);
    check("nobody asking: nothing queued, nothing sent", !mirror_poll() && !msgs && !sm_length);
    hello(1);
    full = mirror_poll();
    check("a hello (01): taken from the frame, the whole screen asked for", full == 1u && !frame_ready);
    mirror_rows(0, 0, 240, 60, canvas, 0);           /* (four times the buffer: it waits for the USB as it goes) */
    mirror_rows(0, 60, 240, 60, canvas, 0);
    mirror_rows(0, 120, 240, 60, canvas, 0);
    mirror_rows(10, 180, 100, 50, 0, 0x07E0u);       /* a fill */
    mirror_poll();
    for (ok = 1, y = 0; y < 180u; y++)
        for (x = 0; x < 240u; x++)
            ok &= screen[y][x] == sw(canvas[(y % 60u) * 240u + x]);
    for (y = 180; y < 230u; y++)
        for (x = 10; x < 110u; x++)
            ok &= screen[y][x] == 0x07E0u;
    check("the rows rebuild the picture exactly (blits in the panel's byte order, a fill), none lost", ok && !mr_lost);
    so_w = SXQ;                                      /* (the USB queue full: the main loop's send waits) */
    mirror_rows(0, 0, 8, 1, canvas, 0);
    x = msgs;
    mirror_poll();
    check("a full USB queue: the row waits in the buffer", msgs == x && sm_length > 0u);
    so_w = 0;
    mirror_poll();
    check("  .. and goes when there is room", msgs == x + 1u && !sm_length);
    fm1_ms += 2500u;
    hello(0);
    check("a hello (00) within 3 s: on, no full screen", !mirror_poll() && sm_on());
    fm1_ms += 3100u;
    mirror_poll();
    x = msgs;
    mirror_rows(0, 0, 240, 1, canvas, 0);
    mirror_poll();
    check("3 s after the last hello: off, nothing sent", !sm_on() && msgs == x);
    fm1_ms += 1000u;                                 /* (custom) the sound playing */
    snds = 0;
    hello(0);
    mirror_poll();
    check("(FM-1 Mirror 1.3) a hello: the sound playing named, F0 7D 'F' 'S' 02 T1 SYNTH SAW LEAD F7",
          snds == 1u && !strcmp(snd, "T1 SYNTH SAW LEAD"));
    fm1_ms += 500u;
    mirror_poll();
    check("  .. not again while it stays the same", snds == 1u);
    playing = "T2 FM6 ICE BELL";
    fm1_ms += 150u;
    mirror_poll();
    check("  .. again when it changes (another track, another sound)", snds == 2u && !strcmp(snd, "T2 FM6 ICE BELL"));
    fm1_ms += 3100u;
    snds = 0;
    hello(2);
    x = msgs;
    mirror_poll();
    mirror_rows(0, 0, 240, 1, canvas, 0);
    mirror_poll();
    check("a hello (02): the sound alone, no screen", snds == 1u && !sm_on() && msgs == x && !frame_ready);
    fm1_ms += 3100u;
    playing = "T3 DRUM 909";
    mirror_poll();
    check("3 s after it: the sound no more", snds == 1u);
    printf(bad ? "mirror: %d FAILED\n" : "mirror tests passed\n", bad);
    return bad != 0;
}
