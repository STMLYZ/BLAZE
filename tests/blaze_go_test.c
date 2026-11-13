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
static void home_up(void) { fm1_in.buttons &= ~(1u << panel.btn[B_HOME]); frames(500); }   /* (0.18.1: GO's HOME debounce, a jump's quiet) */
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
    go_key(wk(3));                                      /* white 4: KEYS (0.17) */
    ok &= ui.lock == LAYER_BLAZE && bz_keys && !ui.menu && !voices();
    bad += check("GO: HOME + white 4: BLAZE's KEYS, locked open; the key sounded nothing; HOME's release no menu", ok);
    go_key(wk(2));                                      /* white 3: ROLL (0.17) */
    ok = ui.lock == LAYER_ROLL;
    go_key(bk(3));                                      /* black 4: MIXER */
    ok &= !ui.lock && !ui.home && str_eq(cur_page()->title, "MIXER");
    bad += check("GO: white 3 ROLL (BLAZE's lock closed), black 4 MIXER (ROLL's closed)", ok);
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
    /* 0.17 PLAY: GO + white 2; a roll while held, HOLD latches the next, SHIFT + a roll key the next drum; a chord
     * while held; a lead note; QUANTIZE: past a 1/16's middle, on the next */
    {
        uint32_t v0;
        ui_power_on();
        frame();
        go_key(wk(1));
        ok = bz_play && ui.lock == LAYER_BLAZE && !bz_keys;
        key_down(bk(0)); ok &= pl_on == 1u && pl_lane == 1u;
        key_up(bk(0)); ok &= !pl_on;
        key_down(bk(10)); key_up(bk(10)); ok &= pl_hold;
        key_down(bk(1)); key_up(bk(1)); ok &= pl_on == 2u && pl_latch == 2u && !pl_hold;
        key_down(bk(1)); key_up(bk(1)); ok &= !pl_on;
        fm1_in.buttons |= 1u << panel.btn[B_OCTUP]; frame();
        key_down(bk(2)); key_up(bk(2));
        fm1_in.buttons &= ~(1u << panel.btn[B_OCTUP]); frame();
        ok &= pl_lane == 2u && !pl_on;
        bad += check("PLAY: GO + white 2; black 1 rolls while held; HOLD + black 2 rolls on, pressed again off; SHIFT + black 3: CLAP", ok);
        key_down(wk(0)); ok = bz_ch_sel == 1u;
        key_up(wk(0)); ok &= bz_ch_sel == 0u;
        v0 = voices();
        key_down(wk(8)); ok &= voices() > v0 && ((kb_bzn >> wk(8)) & 1u);
        key_up(wk(8)); ok &= !((kb_bzn >> wk(8)) & 1u);
        bad += check("PLAY: white 1 a chord while held; white 9 a lead note on KEYS' track", ok);
        pl_quant = 1; song.playing = 1; clk_pos = div_samples(2) * 3u / 4u;
        key_down(wk(9)); ok = ((kb_qpend >> wk(9)) & 1u) && !((kb_bzn >> wk(9)) & 1u);
        pl_tick(); ok &= ((kb_bzn >> wk(9)) & 1u) && !kb_qpend;
        key_up(wk(9));
        pl_quant = 0; song.playing = 0; clk_pos = CLK_START;
        bad += check("PLAY QUANTIZE: a lead note past the 1/16's middle waits for the next (pl_tick)", ok);
        bz_play_close();
    }
    /* 0.17 SET's knobs */
    {
        int16_t l0 = trk[0].p[P_LEVEL];
        perf_k[0] = 0;
        bz_set_knob(0, 5); ok = perf_k[0] == 10;
        bz_set_knob(1, -4); ok &= trk[0].p[P_LEVEL] == l0 - 4;
        bz_set_knob(2, 3); ok &= bzs_off == 3;
        bz_set_knob(2, 100); ok &= bzs_off == 50;
        perf_k[0] = 0; trk[0].p[P_LEVEL] = l0; bzs_off = 0;
        ui_home_view = HV_SET; go_home(); ui.force = 1; ui_draw(); ui_home_view = HV_SCOPE;
        bad += check("SET knobs: COLOR (filter), LEVEL (all four), OFFSET (-50..50 ms); the cards draw", ok);
    }
    /* 0.17b: SHIFT + K1..K3 on SET: the DJ EQ; KILL LOW takes a low tone out, KILL HIGH leaves it; PLAY's vibrato */
    {
        uint32_t i, j, n = CTL;
        int32_t lowk, highk, peak;
        fm1_in.buttons |= 1u << panel.btn[B_OCTUP];
        bz_set_knob(0, -40); ok = bz_eq_v[0] == -32;
        bz_set_knob(2, 5); ok &= bz_eq_v[2] == 5;
        ui_home_view = HV_SET; go_home(); ui.force = 1; ui_draw(); ui_home_view = HV_SCOPE;
        fm1_in.buttons &= ~(1u << panel.btn[B_OCTUP]);
        bz_eq_v[2] = 0;
        /* a 60 Hz tone through KILL LOW, then KILL HIGH: the level left after a settle */
        for (j = 0; j < 2u; j++) {
            bz_eq_v[0] = j ? 0 : -32;
            bz_eq_v[2] = j ? -32 : 0;
            bz_eq_run = 0;
            peak = 0;
            for (i = 0; i < 44100u; i += n) {
                uint32_t q;
                for (q = 0; q < n; q++) {
                    mix_l[q] = mix_r[q] = (int32_t)(40000.0 * sin(6.2831853 * 60.0 * (i + q) / 44100.0));
                    wet[q] = 0;
                }
                bz_eq_block(n);
                if (i > 22050u)
                    for (q = 0; q < n; q++)
                        peak = mix_l[q] > peak ? mix_l[q] : peak;
            }
            if (j) highk = peak; else lowk = peak;
        }
        bz_eq_v[0] = bz_eq_v[1] = bz_eq_v[2] = 0;
        for (i = 0; i < 80u && bz_eq_run; i++) {        /* 0.18: back at 0: the gains glide home, it fades out, then off */
            uint32_t q;
            for (q = 0; q < n; q++) { mix_l[q] = mix_r[q] = 1000; wet[q] = 0; }
            bz_eq_block(n);
        }
        ok &= !bz_eq_run && i > 8u && i < 80u;
        ok &= lowk < 40000 / 12 && highk > 40000 * 9 / 10;
        printf("  (60 Hz at 40000: KILL LOW leaves %d, KILL HIGH %d)\n", (int)lowk, (int)highk);
        bz_play_open();
        ok &= trk[bz_ktrk % NTRK].p[P_ATDST] == 11 && trk[bz_ktrk % NTRK].p[P_ATSNS] > 0;
        bz_play_close();
        bad += check("SET + SHIFT: the DJ EQ (KILL LOW, HIGH); PLAY: the lead's pressure to VIBRATO", ok);
    }
    /* 0.18: SET's OCT+ tapped: the octave (held with a knob: SHIFT, no octave); PLAY's vibrato only while it is open;
     * RAIL's EQ knobs (CC 55..57, the first value noted) */
    {
        track_t *lt;
        ui_power_on();
        frame();
        ui_home_view = HV_SET; go_home(); frame();
        song.octave = 0;
        press(B_OCTUP); frame();
        ok = song.octave == 1;
        fm1_in.buttons |= 1u << panel.btn[B_OCTUP]; host_pressed |= 1u << panel.btn[B_OCTUP]; frame();
        bz_set_knob(1, -3);
        fm1_in.buttons &= ~(1u << panel.btn[B_OCTUP]); frame(); frame();
        ok &= song.octave == 1 && bz_eq_v[1] == -3;
        press(B_OCTDN); frame();
        ok &= song.octave == 0;
        bz_eq_v[1] = 0;
        ui_home_view = HV_SCOPE;
        bad += check("SET (0.18): OCT+ tapped: the octave up; held with a knob: the EQ, no octave; OCT- down", ok);
        bz_play_open();
        lt = &trk[bz_ktrk % NTRK];
        ok = lt->p[P_ATDST] == 11 && lt->p[P_ATAMT] == 3;
        bz_play_close();
        ok &= lt->p[P_ATDST] == 0 && lt->p[P_ATAMT] == TP[P_ATAMT].def && lt->p[P_ATSNS] == TP[P_ATSNS].def;
        lt->p[P_ATDST] = 2; lt->p[P_ATAMT] = 20;        /* (its own routing: PLAY leaves it) */
        bz_play_open(); bz_play_close();
        ok &= lt->p[P_ATDST] == 2 && lt->p[P_ATAMT] == 20;
        lt->p[P_ATDST] = 0; lt->p[P_ATAMT] = TP[P_ATAMT].def;
        bad += check("PLAY (0.18): the vibrato routing only while PLAY is open; a track's own routing is left", ok);
        go_key(wk(0));                                  /* (BLAZE's page: its pass takes RAIL's CCs) */
        midi_event(0xB0u, 15u, 55u, 64u); frame();
        ok = bz_eq_v[0] == 0;
        midi_event(0xB0u, 15u, 55u, 0u); frame();
        ok &= bz_eq_v[0] == -32;
        midi_event(0xB0u, 15u, 57u, 100u); frame();
        midi_event(0xB0u, 15u, 57u, 127u); frame();
        ok &= bz_eq_v[2] == 32;
        midi_event(0xB0u, 15u, 55u, 64u); midi_event(0xB0u, 15u, 57u, 64u); frame();
        ok &= bz_eq_v[0] == 0 && bz_eq_v[2] == 0;
        bad += check("RAIL's EQ knobs (0.18): CC 55 LOW 0 = KILL, CC 57 HIGH 127 = +6 dB, 64 flat; the first value noted", ok);
    }
    /* 0.18 RIFF CAPTURE: the lead's pattern emptied, 4 bars of 1/16, the track armed; a lead note lands in a step;
     * again: disarmed, the riff stays; UNDO: the pattern as it was */
    {
        track_t *lt;
        step_t was[NSTEP];
        uint32_t i, n = 0;
        int16_t len, div;
        ui_power_on();
        frame();
        go_key(wk(1));
        lt = &trk[bz_ktrk % NTRK];
        lt->step[3].n = 1; lt->step[3].note[0] = 60; lt->step[3].time = ST_NOTE;
        lt->p[P_SLEN] = 12;
        memcpy(was, lt->step, sizeof was);
        len = lt->p[P_SLEN]; div = lt->p[P_SDIV];
        bz_riff_toggle();
        ok = !bzriff.on && !song.rec;                   /* (stopped: no) */
        song.playing = 1;
        bz_riff_toggle();
        ok &= bzriff.on && ((song.rec >> (bz_ktrk % NTRK)) & 1u) && lt->p[P_SLEN] == 64 && lt->p[P_SDIV] == 2 &&
              !lt->step[3].n && !click_wanted();
        key_down(wk(8)); key_up(wk(8));
        for (i = 0; i < NSTEP; i++)
            n += lt->step[i].n != 0;
        ok &= n == 1u;
        bz_riff_toggle();
        ok &= !bzriff.on && !song.rec && bzriff.has;
        for (i = 0, n = 0; i < NSTEP; i++)
            n += lt->step[i].n != 0;
        ok &= n == 1u;
        bz_riff_undo();
        ok &= !memcmp(was, lt->step, sizeof was) && lt->p[P_SLEN] == len && lt->p[P_SDIV] == div && !bzriff.has;
        bz_riff_bars(); ok &= bzriff.bars == 1u;
        bz_riff_bars(); bz_riff_bars(); ok &= bzriff.bars == 4u;
        fm1_in.buttons |= 1u << panel.btn[B_OCTUP]; frame();
        key_down(bk(3)); key_up(bk(3)); frame();        /* SHIFT + black 4: the key */
        ok &= bzriff.on;
        fm1_in.buttons &= ~(1u << panel.btn[B_OCTUP]); frame();
        bz_play_close();
        ok &= !bzriff.on && !song.rec;
        bz_riff_undo();
        song.playing = 0;
        bad += check("RIFF CAPTURE (0.18): 4 bars emptied and armed, a lead note recorded, kept when ended; UNDO; BARS; "
                     "SHIFT + black 4; leaving PLAY ends it", ok);
    }
    /* 0.18 SCENES: SAVE SCENE sends BZ_SC_N bytes on CC 44 45 46; the same bytes back on CC 38 39 40 set the
     * sounds, levels, mutes, sends, the kit and BOOST; a wrong sum: nothing */
    {
        uint8_t sc[BZ_SC_N + 2u];
        uint32_t i, n = 0, mo, sum = 0;
        track_t *d;
        ui_power_on();
        frame();
        usb.config = 1;
        d = rl_track();
        set_engine_of(&trk[0], 0); apply_preset_to(&trk[0], 3);
        trk[0].p[P_LEVEL] = 77; trk[1].p[P_MUTE] = 1; trk[0].p[P_REV] = 55;
        if (drum_track(d)) d->p[P_E0] = DK_RAVE;
        dv_boost = BZ_BOOST_LANES;
        mo = mo_w;
        bz_scene_save();
        for (i = 0; i < 200u && (bz_sc_txn || bz_tx_r != bz_tx_w); i++) {
            bz_scene_pass_tx();
            home_block(CTL, CTL);
        }
        for (; mo != mo_w; mo++) {
            uint32_t pk = midi_out_q[mo % MQ], cc = pk >> 16 & 127u, v = pk >> 24 & 127u;
            if ((pk >> 8 & 255u) == 0xBFu && cc >= 44u && cc <= 46u && n < sizeof sc)
                sc[n++] = (uint8_t)(cc == 45u ? v : cc == 44u ? v : v), sum += cc == 45u ? v : 0u;
        }
        ok = n == BZ_SC_N + 2u && sc[0] == BZ_SC_N && sc[BZ_SC_N + 1u] == (sum & 127u) && sc[1] == 1u;
        apply_preset_to(&trk[0], 0);                    /* another state */
        trk[0].p[P_LEVEL] = 100; trk[1].p[P_MUTE] = 0; trk[0].p[P_REV] = 0;
        if (drum_track(d)) d->p[P_E0] = DK_STD;
        dv_boost = 0;
        midi_event(0xB0u, 15u, 38u, BZ_SC_N);
        for (i = 0; i < BZ_SC_N; i++)
            midi_event(0xB0u, 15u, 39u, sc[1u + i]);
        midi_event(0xB0u, 15u, 40u, (sum + 1u) & 127u); /* (a wrong sum) */
        go_key(wk(0)); frame();
        ok &= !bz_sc_ready && trk[0].p[P_LEVEL] == 100;
        midi_event(0xB0u, 15u, 38u, BZ_SC_N);
        for (i = 0; i < BZ_SC_N; i++)
            midi_event(0xB0u, 15u, 39u, sc[1u + i]);
        midi_event(0xB0u, 15u, 40u, sum & 127u);
        frame();
        ok &= trk[0].preset == 3u && trk[0].p[P_LEVEL] == 77 && trk[1].p[P_MUTE] == 1 && trk[0].p[P_REV] == 55 &&
              dv_boost == BZ_BOOST_LANES && (!drum_track(d) || d->p[P_E0] == DK_RAVE);
        trk[1].p[P_MUTE] = 0; dv_boost = 0; usb.config = 0;
        bad += check("SCENES (0.18): SAVE SCENE out on CC 44 45 46; back on CC 38 39 40: preset, LEVEL, MUTE, REV, KIT, "
                     "BOOST; a wrong sum: nothing", ok);
    }
    /* 0.18.1: HOME's contact flickering (open a few scans while the key is down, or bouncing as it is let go) is not a
     * HOME tap: the page GO went to stays (it went back to HOME on the hardware) */
    {
        int g;
        ok = 1;
        for (g = 0; g < 2; g++) {
            ui_power_on(); frame(); frames(10);
            fm1_in.buttons |= 1u << panel.btn[B_HOME]; host_pressed |= 1u << panel.btn[B_HOME]; frames(400);
            key_down(wk(8)); frame();
            if (!g) { fm1_in.buttons &= ~(1u << panel.btn[B_HOME]); frame(); frame(); fm1_in.buttons |= 1u << panel.btn[B_HOME]; frame(); }
            key_up(wk(8)); frame();
            fm1_in.buttons &= ~(1u << panel.btn[B_HOME]); frame();
            if (g) { fm1_in.buttons |= 1u << panel.btn[B_HOME]; frame(); fm1_in.buttons &= ~(1u << panel.btn[B_HOME]); frame(); }
            frames(500);
            ok &= !ui.home && !ui.menu && str_eq(cur_page()->title, "PRESETS");
        }
        press(B_HOME); frames(100);
        ok &= ui.home;                                  /* (a plain tap later: HOME, as before) */
        bad += check("GO (0.18.1): HOME flickering with the key down, or bouncing as let go: the page stays; a later tap: HOME", ok);
    }
    printf("%s\n", bad ? "BLAZE GO: FAILED" : "BLAZE GO: all ok");
    return bad != 0;
}
