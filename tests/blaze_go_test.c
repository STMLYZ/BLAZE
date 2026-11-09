/* SPDX-License-Identifier: GPL-3.0-only
 * BLAZE 0.15: GO, the launcher (HOME held: a key goes there; let go: the MENU; a tap: HOME), the SET view and the
 * decks from rail-bridge (blaze_set.c: its SysEx frames, the playhead run on by the speed), the CPU meter */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int bad;
static uint32_t wk(uint32_t place)
{
    uint32_t k;
    for (k = 0; k < 27u; k++)
        if (!key_black(k) && key_place(k) == place)
            return k;
    return 0;
}
static uint32_t bk(uint32_t place)
{
    uint32_t k;
    for (k = 0; k < 27u; k++)
        if (key_black(k) && key_place(k) == place)
            return k;
    return 0;
}
static void home_down(void) { fm1_in.buttons |= 1u << panel.btn[B_HOME]; host_pressed |= 1u << panel.btn[B_HOME]; frame(); }
static void home_up(void) { fm1_in.buttons &= ~(1u << panel.btn[B_HOME]); frame(); frame(); }
/* HOME down, key k pressed and let go, HOME let go */
static void go_key(uint32_t k)
{
    home_down();
    key_down(k);
    frame();
    key_up(k);
    frame();
    home_up();
}
static uint32_t voices(void)
{
    uint32_t p, i, n = 0;
    for (p = 0; p < NTRK; p++)
        for (i = 0; i < NVOICE; i++)
            n += trk[p].v[i].gate != 0;
    return n;
}
static void frame_u21(uint8_t *b, uint32_t v) { b[0] = (uint8_t)(v & 127u); b[1] = (uint8_t)(v >> 7 & 127u); b[2] = (uint8_t)(v >> 14 & 127u); }

int main(void)
{
    int ok;
    uint32_t i;
    ui_power_on();
    frame();
    /* GO */
    go_title("ENV");
    press(B_HOME);
    bad += check("GO: HOME tapped: HOME, as before (no map, no menu)", ui.home && !ui.menu && !go_shown());
    home_down();
    frames(400);
    ok = go_shown();
    ui.force = 1;
    ui_draw();                                          /* (the map draws) */
    home_up();
    bad += check("GO: HOME held: the map shows; let go with no key: the MENU", ok && ui.menu && !go_shown());
    press(B_HOME);
    frames(50);
    ok = !ui.menu;
    go_key(wk(2));                                      /* white 3: KEYS */
    ok &= ui.lock == LAYER_BLAZE && bz_keys && !ui.menu && !voices();
    bad += check("GO: HOME + white 3: BLAZE's KEYS, locked open; the key sounded nothing; HOME's release no menu", ok);
    go_key(wk(1));                                      /* white 2: ROLL */
    ok = ui.lock == LAYER_ROLL;
    go_key(bk(3));                                      /* black 4: MIXER */
    ok &= !ui.lock && !ui.home && str_eq(cur_page()->title, "MIXER");
    bad += check("GO: white 2 ROLL (BLAZE's lock closed), black 4 MIXER (ROLL's closed)", ok);
    go_key(wk(7));                                      /* white 8: SET */
    ok = ui.home && home_set() && !ui.lock;
    go_key(wk(8));                                      /* white 9: PRESETS */
    ok &= !ui.home && str_eq(cur_page()->title, "PRESETS");
    go_key(bk(10));                                     /* black 11: MENU */
    ok &= ui.menu;
    press(B_HOME);
    frames(50);
    bad += check("GO: white 8 the SET view (MENU > HOME SET), white 9 PRESETS, black 11 the MENU", ok && !ui.menu);
    ok = 1;
    for (i = 0; i < 27u; i++)                         /* (a cell's name: BZW less the number's tab, 3 px) */
        ok &= go_of(i) != 0 && go_of(i)->label && text_w(&AF_S, go_of(i)->label) <= BZW - (key_place(i) >= 9u ? 10 : 6) - 3;
    bad += check("GO: every key a place, its name fits its cell", ok);

    /* the decks: INFO, OVERVIEW, CUES, WINDOW, POS; the playhead runs on at the speed */
    {
        static uint8_t f[600];
        uint32_t n;
        f[0] = 1; f[1] = 0; f[2] = 1; frame_u21(f + 3, 12800); f[6] = 8; f[7] = 0; frame_u21(f + 8, 30 * 300);
        frame_u21(f + 11, 16 * 5); frame_u21(f + 14, 30 * 256 * 60 / 128);
        memcpy(f + 17, "TEST TRACK", 11);
        bz_wave_frame(f, 28);
        f[0] = 2; f[1] = 0;
        for (i = 0; i < BZW_OV; i++) f[2 + i] = (uint8_t)(i % 16u);
        bz_wave_frame(f, 2 + BZW_OV);
        f[0] = 3; f[1] = 0; f[2] = 2; f[3] = 1; frame_u21(f + 4, 30 * 120); f[7] = 0; frame_u21(f + 8, 30 * 60);
        bz_wave_frame(f, 11);
        f[0] = 4; f[1] = 0; frame_u21(f + 2, 3000); f[5] = (uint8_t)(BZW_WIN & 127u); f[6] = (uint8_t)(BZW_WIN >> 7);
        for (i = 0; i < BZW_WIN; i++) f[7 + i] = (uint8_t)((i * 7u) % 16u);
        n = 7 + BZW_WIN;
        bz_wave_frame(f, n);
        f[0] = 5; f[1] = 0; f[2] = 1; frame_u21(f + 3, 3200u * 16u); frame_u21(f + 6, 10200);
        bz_wave_frame(f, 9);
        ok = bzw.d[0].loaded && bzw.d[0].bpm100 == 12800u && bzw.d[0].cam == 8u && !strcmp((char *)bzw.d[0].title, "TEST TRACK") &&
             bzw.d[0].ncue == 2u && bzw.d[0].cue_k[0] == 1u && bzw.d[0].cue_c[0] == 3600u && bzw.d[0].wbase == 3000u &&
             bzw.d[0].wn == BZW_WIN && bzw_nib(bzw.d[0].win, 5) == 3u && bzw_nib(bzw.d[0].ov, 17) == 1u &&
             bzw.d[0].playing && bzw_pos(0) == 3200u * 16u;
        fm1_ms += 1000u;
        bzw.last_ms = fm1_ms;
        ok &= bzw_pos(0) >= 3200u * 16u + 30u * 16u * 102u / 100u - 2u && bzw_pos(0) <= 3200u * 16u + 30u * 16u * 102u / 100u + 2u;
        bad += check("SET's decks: INFO OVERVIEW CUES WINDOW POS parsed; the playhead runs on (1 s at +2 %: 30.6 columns)", ok);
        fm1_ms += BZW_STALE_MS;
        ok = !bzw_live() && bzw_pos(0) == 3200u * 16u;
        bzw.last_ms = fm1_ms;
        bad += check("SET's decks: nothing from the bridge for a while: idle, the playhead held", ok);
    }
    /* the SET view and the CPU meter draw (a hot load: its number in the header) */
    ui_home_view = HV_SET;
    go_home();
    song.cpu_q8 = 200u;                                 /* 78 % */
    ui.force = 1;
    ui_draw();
    ok = cpu_pct() == 78u && cpu_hot() == 1u && head_cpu_x > 150;
    song.cpu_q8 = 40u;
    ui.force = 1;
    ui_draw();
    ok &= cpu_pct() == 15u && !cpu_hot() && head_cpu_x >= 200;
    bad += check("SET draws; the CPU meter left of the battery, its number when 70 % and over", ok);
    ui_home_view = HV_SCOPE;

    /* 0.16 DECKS: the deck alone on air is the key; a blend: the other's the next key (BLEND); KEY NOW takes it; the
     * phrase follows the lead deck's grid (bar 1: its DROP cue), whole beats, the same shift twice */
    {
        static uint8_t f[64];
        uint32_t d;
        memset(&bzw, 0, sizeof bzw);
        bzd.keyd = 255; bzd.keyk = bzd.nextk = 0; bzd.pend = 0;
        bzd_follow = 1;
        for (d = 0; d < 2u; d++) {                      /* A 8A, B 10A; 128 BPM, 30 * 60 / 128 columns a beat */
            memset(f, 0, sizeof f);
            f[0] = 1; f[1] = (uint8_t)d; f[2] = 1; frame_u21(f + 3, 12800); f[6] = d ? 10 : 8; f[7] = 0;
            frame_u21(f + 8, 30 * 300); frame_u21(f + 11, 0); frame_u21(f + 14, 30 * 256 * 60 / 128);
            f[17] = 'T'; f[18] = 0;
            bz_wave_frame(f, 20);
        }
        f[0] = 3; f[1] = 0; f[2] = 1; f[3] = 1; frame_u21(f + 4, 1800);   /* A: DROP at 60 s = beat 128, bar 33 */
        bz_wave_frame(f, 7);
        f[0] = 6; f[1] = 0; bz_wave_frame(f, 2);        /* FOCUS A */
        bz_deck_poll();
        ok = trk[0].p[P_ROOT] == 9 && trk[0].p[P_SCALE] == 2 && !bz_blend;
        f[0] = 6; f[1] = 2; bz_wave_frame(f, 2);        /* both on air */
        bz_deck_poll();
        ok &= trk[0].p[P_ROOT] == 9 && bz_blend != 0;   /* (A still; B the next key: BLEND) */
        bzd_key_now();
        bz_deck_poll();
        ok &= trk[0].p[P_ROOT] == 11 && trk[0].p[P_SCALE] == 2;   /* (10A: B minor) */
        f[0] = 6; f[1] = 0; bz_wave_frame(f, 2);
        bz_deck_poll();
        ok &= trk[0].p[P_ROOT] == 9 && !bz_blend;
        bad += check("DECKS: A alone 8A (A minor); a blend: B's 10A the next key (BLEND); KEY NOW: B minor; A alone again", ok);

        /* the phrase: A playing at beat 130 (2 beats past its DROP: the phrase's beat 2); the FM-1 at beat 5 */
        song.playing = 1; song.g[G_CLOCK] = 1; bz_drop = BZD_IDLE;
        bz_s16 = 1000u; bz_org = 1000u - 5u * 4u;
        f[0] = 5; f[1] = 0; f[2] = 1;
        { uint32_t p16 = (uint32_t)(130 * 30 * 60 / 128) * 16u; frame_u21(f + 3, p16); }
        frame_u21(f + 6, 10000);
        bz_wave_frame(f, 9);
        bzw.d[0].pos_ms = fm1_ms;
        bzw.d[0].playing = 0;                           /* (held still: no run-on while the test waits) */
        bzd.ph_ms = fm1_ms - 1000u;
        bzd.pend = 0;
        bz_deck_poll();
        ok = bz_org == 1000u - 20u;                     /* (seen once: not yet) */
        bzw.d[0].playing = 1;
        bzw.d[0].pos_ms = fm1_ms;
        bzd.ph_ms = fm1_ms - 1000u;
        bz_deck_poll();
        bzd.ph_ms = fm1_ms - 1000u;
        bz_deck_poll();
        ok &= ((bz_s16 - bz_org) / 4u) % 64u == 2u;
        bad += check("DECKS: the phrase follows the lead deck's grid, bar 1 its DROP cue (beat 2 now), the same shift twice", ok);
        song.playing = 0; song.g[G_CLOCK] = 0;
    }
    printf("%s\n", bad ? "BLAZE GO: FAILED" : "BLAZE GO: all ok");
    return bad != 0;
}
