/* SPDX-License-Identifier: GPL-3.0-only
 * BLAZE (a DJ companion built on Felucca by Leo Kuroshita / Hügelton Instruments) */
/* BLAZE's home layer (firmware/src/blaze_home.c, blaze_home_ui.c, blaze_home_draw.c) on the real sequencer and UI
 * sources, with the stubs of tests/ui_test.c: ENV held opens it; the 4 pads are ROLL's PADS 4 in key order and roll
 * with the rates; an effect tapped latches, held plays while held; BUILD 4 runs 1/4 1/8 1/16 1/32 a bar each, louder,
 * from the next bar while playing, and stops; RISER; RELEASE; THE ONE; RAIL's key CCs set ROOT and SCALE; SHIFT's
 * knobs, a pad held edits that drum, ALGO the KIT; KEYS (OCT-): the white keys play the scale, the black ones the arp. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
/* 0.9: a rate key tapped (let go within 250 ms) stays on; a key let go here was held 300 ms (fm1_ms), so the presses
 * of the tests before 0.9 stay holds. (key_up)(k): let go at once (a tap) */
static void key_up_held(uint32_t k)
{
    fm1_ms += 300u;
    (key_up)(k);
}
#define key_up(k) key_up_held(k)

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

static void home_reset(void)
{
    ui_power_on();
    rl_groove = 1;
    rl_padmode = RL_PADS16;
    rl_live4 = 0x001Eu;                            /* SNARE CLAP HAT OPEN */
    rl_live1 = 0x0002u;                            /* SNARE */
    rl_build = 0;
    rl_held = rl_extra = 0;
    rl_rates = 0;
    rl_nr = rl_runs = rl_rolling = 0;
    rl_flash = 0;
    memset(rl_offt, 0, sizeof rl_offt);
    kb_roll = kb_bz = 0;
    perf_held = perf_latched = 0;
    bz_build = 0;
    bz_riser = 0;
    bz_keys = 0;
    bz_chord = 0;
    bz_ch_sel = 0;
    bz_eo = 0;
    bz_loops = 0;
    bz_blend = 0;
    bz_duck = 0;
    bz_lp_q = 1;
    bz_lp_fbars = 4;
    {
        uint32_t t;
        for (t = 0; t < NTRK; t++) {
            bz_lp_op[t] = 0;
            bz_fd_len[t] = 0;
            bz_fd_gain[t] = 32768;
            bz_fill_left[t] = 0;
            bz_gain[t] = 32768;
            trk[t].p[P_MUTE] = 0;
        }
        bz_fd_out = 0;
        bz_lp_restart = 0;
    }
    bz_pads = 0;
    bz_tapfx = bz_offfx = 0;
    bz_slice = 0;
    bz_rb = 0;
    bz_sl_on = bz_sl_off = 0;
    bz_sl_q = 0;
    bz_sl_page = 0;
    kb_bzs = 0;
    song.g[G_BPM] = 120;
    song.g[G_CLOCK] = 0;
    home_mask = 1u << panel.btn[B_ENV];            /* ENV held: the keys are BLAZE's */
    fm1_in.buttons |= home_mask;
}

/* voice starts over `blocks` blocks of CTL samples (rolls and the build); at[]: the sample each began in */
static uint32_t run(uint32_t blocks, uint32_t *at, uint32_t nat)
{
    uint32_t b, n = 0, v0;
    for (b = 0; b < blocks; b++) {
        v0 = vage;
        roll_block(CTL, CTL);
        home_block(CTL, CTL);
        chord_block(CTL);
        loops_block(CTL, CTL);
        for (; v0 < vage; v0++, n++)
            if (at && n < nat)
                at[n] = b * CTL;
    }
    return n;
}
static int near(uint32_t a, uint32_t b) { return (a > b ? a - b : b - a) <= CTL; }
static int bz_gate_note(const track_t *t, uint32_t note)
{
    uint32_t i;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active && t->v[i].gate && t->v[i].note == note)
            return 1;
    return 0;
}
/* 0.7: n MIDI clock pulses at bpm10 / 10 BPM from *t (ms, exact), each stamped in whole ms off by up to +-jit (a
 * fixed pseudo-random walk); the lowest and highest BPM shown after the first `settle` pulses */
static uint32_t clk_seed = 12345u;
static void clk_feed(double *t, uint32_t bpm10, uint32_t n, uint32_t jit, uint32_t settle, int *lo, int *hi)
{
    uint32_t i;
    double step = 60000.0 * 10.0 / bpm10 / 24.0;
    for (i = 0; i < n; i++) {
        int32_t j;
        clk_seed = clk_seed * 1103515245u + 12345u;
        j = jit ? (int32_t)((clk_seed >> 16) % (2u * jit + 1u)) - (int32_t)jit : 0;
        *t += step;
        midi_clock_pulse((uint32_t)((int32_t)(*t + 0.5) + j));
        if (i >= settle) {
            if (song.g[G_BPM] < *lo) *lo = song.g[G_BPM];
            if (song.g[G_BPM] > *hi) *hi = song.g[G_BPM];
        }
    }
}

static int fake_save(void) { return 2; }   /* (0.14: a capture kept in USR 2) */
int main(void)
{
    int bad = 0, ok;
    uint32_t at[64], n, v0, bar, i, b;
    char nm[16];

    /* the pads: white 1..4 are ROLL's 4 in key order; with a rate they roll */
    home_reset();
    v0 = vage;
    key_down(wk(0));
    ok = vage == v0 + 1u && bz_pad_of(0) == 1u && bz_pad_of(3) == 4u && bz_pad_of(4) == 16u;
    key_up(wk(0));
    ok &= run(200, 0, 0) == 0u;
    bad += check("BLAZE: white 1 plays ROLL's first PADS 4 pad (SNARE) once, at once", ok);
    key_down(bk(3));                                   /* 1/16 (ROLL's rate 4) */
    key_down(wk(0));
    n = run(beat_samples() / CTL, 0, 0);
    key_up(wk(0));
    key_up(bk(3));
    bad += check("  pad + black 4 (1/16): 4 hits a beat, whatever ROLL's PADS mode", n == 4u);
    rl_padmode = RL_PADS1;                             /* (ROLL's 1 mode: CLAP is not its pad, BLAZE still plays it) */
    v0 = vage;
    key_down(wk(1));
    key_up(wk(1));
    bad += check("  ROLL on PADS 1: BLAZE's 4 pads still play", vage == v0 + 1u);
    rl_live4 = 0x0001u;
    v0 = vage;
    key_down(wk(2));
    key_up(wk(2));
    bad += check("  a white key past the pads picked: silent", vage == v0);

    /* the effects: a tap latches (a tap again: off), a hold plays while held */
    home_reset();
    key_down(wk(BZ_FX0));                              /* ECHO */
    ok = (perf_held & PF_BIT(PF_ETHR)) != 0u;
    run(20, 0, 0);                                     /* (~30 ms) */
    key_up(wk(BZ_FX0));
    ok &= (perf_latched & PF_BIT(PF_ETHR)) && !(perf_held & PF_BIT(PF_ETHR));
    bad += check("effects: ECHO tapped latches on", ok);
    key_down(wk(BZ_FX0));
    ok = !(perf_latched & PF_BIT(PF_ETHR));
    key_up(wk(BZ_FX0));
    ok &= !(perf_latched & PF_BIT(PF_ETHR)) && !(perf_held & PF_BIT(PF_ETHR));
    bad += check("  tapped again: off (its release does nothing)", ok);
    bz_shift_on = 1;                                   /* (0.12: FREEZE is SHIFT + white 10) */
    key_down(wk(9));                                   /* FREEZE */
    bz_shift_on = 0;
    run(500, 0, 0);                                    /* (~750 ms: a hold) */
    ok = (perf_held & PF_BIT(PF_FRZ)) != 0u;
    key_up(wk(9));
    ok &= !(perf_held & PF_BIT(PF_FRZ)) && !(perf_latched & PF_BIT(PF_FRZ));
    bad += check("  FREEZE held: on while held, off when let go (not latched)", ok);

    /* BUILD 4, stopped: at once, a bar each of 1/4 1/8 1/16 1/32 (4 + 8 + 16 + 32 hits), louder, then it stops */
    home_reset();
    key_down(wk(BZ_B4));
    key_up(wk(BZ_B4));
    bar = 4u * beat_samples();
    n = 0;
    ok = bz_build == 4u;
    {
        uint32_t per[4] = {0}, b0 = 0, vfirst = 0, vlast = 0;
        for (b = 0; b < 4u * bar / CTL + 4u; b++) {
            v0 = vage;
            home_block(CTL, CTL);
            if (vage != v0) {
                uint32_t s = (b * CTL + CTL - 1u) / bar;   /* (the block that holds the bar line: the new bar) */
                per[s < 4u ? s : 3u] += vage - v0;
                n += vage - v0;
                if (!b0++)
                    vfirst = rl_vel_last;
                vlast = rl_vel_last;
            }
        }
        ok &= per[0] == 4u && per[1] == 8u && per[2] == 16u && per[3] == 32u && !bz_build && vlast > vfirst + 60u;
        printf("    build hits: %u %u %u %u, velocity %u -> %u\n", per[0], per[1], per[2], per[3], vfirst, vlast);
    }
    bad += check("BUILD 4 (stopped): 1/4 1/8 1/16 1/32 a bar each on the 1 pad, swelling, then stops", ok);

    /* BUILD 8 playing: waits for the next bar; pressed again: stops */
    home_reset();
    seq_start();
    song.playing = 1;
    click_tick(0);
    for (i = 0; i < 5u; i++)
        click_tick(div_samples(2));                    /* (5 steps into the bar) */
    key_down(wk(BZ_B8));
    key_up(wk(BZ_B8));
    n = run(bar / CTL + 2u, at, 64);
    ok = bz_build == 8u && n >= 1u && near(at[0], 11u * div_samples(2));
    bad += check("BUILD 8 (playing): its first hit on the next bar", ok);
    key_down(wk(BZ_B8));
    key_up(wk(BZ_B8));
    bad += check("  pressed again: stopped", !bz_build);

    /* RISER: stopped, at once; quieter and darker early, louder late; ends after its 8 bars; tapped twice: 16 */
    home_reset();
    key_down(wk(BZ_RISER)); key_up(wk(BZ_RISER));
    {
        uint32_t blocks = 8u * bar / CTL;
        double q1 = 0, q4 = 0;
        static int32_t L[CTL], R[CTL], V[CTL];
        ok = bz_riser == 8u && !bz_rs_wait;
        for (b = 0; b < blocks + 4u; b++) {
            double e = 0;
            memset(L, 0, sizeof L); memset(R, 0, sizeof R); memset(V, 0, sizeof V);
            bz_riser_mix(L, R, V, CTL);
            home_block(CTL, CTL);
            for (i = 0; i < CTL; i++) {
                e += (double)L[i] * L[i];
                ok &= L[i] >= -BZR_PEAK && L[i] <= BZR_PEAK;
            }
            if (b < blocks / 4u) q1 += e;
            else if (b > blocks * 3u / 4u && b < blocks) q4 += e;
        }
        printf("    riser energy: first quarter %.3g, last quarter %.3g\n", q1, q4);
        ok &= q4 > 20.0 * (q1 + 1.0) && !bz_riser;
        memset(L, 0, sizeof L);
        bz_riser_mix(L, R, V, CTL);
        ok &= !L[0] && !L[CTL / 2u];
    }
    bad += check("RISER (stopped): swells over 8 bars, within its peak, then silent", ok);
    home_reset();
    key_down(wk(BZ_RISER)); key_up(wk(BZ_RISER));
    run(10, 0, 0);
    key_down(wk(BZ_RISER)); key_up(wk(BZ_RISER));
    ok = bz_riser == 16u;
    run(2000, 0, 0);
    key_down(wk(BZ_RISER)); key_up(wk(BZ_RISER));
    bad += check("  tapped twice quickly: 16 bars; tapped later: stopped", ok && !bz_riser);
    home_reset();
    seq_start();
    song.playing = 1;
    click_tick(0);
    for (i = 0; i < 5u; i++)
        click_tick(div_samples(2));
    key_down(wk(BZ_RISER)); key_up(wk(BZ_RISER));
    ok = bz_riser == 8u && near(bz_rs_wait, 11u * div_samples(2));
    bad += check("  playing: it waits for the next bar", ok);

    /* RELEASE: latched effects, the build off; the main loop homes COLOR and DEPTH */
    home_reset();
    key_down(wk(BZ_FX0 + 1u)); key_up(wk(BZ_FX0 + 1u));   /* REVERB latched */
    key_down(wk(BZ_B4)); key_up(wk(BZ_B4));
    perf_k[0] = -50;
    perf_k[3] = 40;
    key_down(wk(BZ_RISER)); key_up(wk(BZ_RISER));
    key_down(bk(BZK_REL)); key_up(bk(BZK_REL));
    ok = !perf_latched && !bz_build && !bz_riser && bz_rel;
    bz_ui_pass();
    ok &= !perf_k[0] && !perf_k[3] && !bz_rel;
    bad += check("RELEASE: effects, the build and the riser off, COLOR and DEPTH home", ok);

    /* THE ONE: now is bar 1 (to the nearest beat) */
    home_reset();
    seq_start();
    song.playing = 1;
    click_tick(0);
    for (i = 0; i < 37u; i++)
        click_tick(div_samples(2));                    /* bar 3, beat 2, its 2nd step (step 37) */
    key_down(bk(BZB_ONE)); key_up(bk(BZB_ONE));
    {
        uint32_t beat;
        int32_t br = bz_phrase(&beat);
        ok = br == 0 && beat == 0u;                    /* (a step past the beat: that beat is the 1) */
        for (i = 0; i < 4u; i++)
            click_tick(div_samples(2));
        br = bz_phrase(&beat);
        ok &= br == 0 && beat == 1u;
        for (i = 0; i < 12u; i++)
            click_tick(div_samples(2));
        br = bz_phrase(&beat);
        ok &= br == 1 && beat == 0u;
    }
    bad += check("THE ONE: the nearest beat is bar 1 beat 1 of the phrase", ok);

    /* RAIL's key: CC 20 21 22 on channel 16 (ROUT CH1-4 or not) */
    home_reset();
    song.g[G_ROUTE] = 0;
    midi_event(0xB0u, 15u, 20u, 8u);
    midi_event(0xB0u, 15u, 21u, 0u);
    midi_event(0xB0u, 15u, 22u, 9u);
    frame();
    ok = trk[0].p[P_ROOT] == 9 && trk[0].p[P_SCALE] == 2 && bz_follow;
    ok &= drum_track(&trk[3]) && trk[3].p[P_SCALE] != 2;   /* (the DRUM track left alone) */
    song.sel = 0;
    ok &= bz_camelot(&b, nm, sizeof nm) == 8u && !b && !strcmp(nm, "A MINOR");
    midi_event(0xB0u, 15u, 21u, 127u);
    midi_event(0xB0u, 15u, 22u, 3u);
    frame();
    ok &= bz_camelot(&b, nm, sizeof nm) == 5u && b && !strcmp(nm, "Eb MAJOR");
    bad += check("RAIL key: 8A sets A minor on the note tracks (not DRUM); 5B Eb MAJOR", ok);

    /* the UI: ENV held opens the map (and draws it); a tap still opens ENV's page; a double tap locks it */
    ui_power_on();
    btn_down(B_ENV);
    frames(800);
    ok = ui.layer == LAYER_BLAZE && (home_mask & (1u << panel.btn[B_ENV]));
    btn_up(B_ENV);
    frame();
    ok &= ui.layer == 0;
    bad += check("UI: ENV held opens BLAZE's map; let go, it closes", ok);
    ui_power_on();
    press(B_ENV);
    ok = !ui.home && cur_page()->fam == FAM_ENV;
    press(B_ENV);
    frame();
    ok &= ui.lock == LAYER_BLAZE && (kb_lock & 8u);
    bad += check("UI: ENV tapped opens ENV's page; tapped again soon, BLAZE locks open", ok);
    press(B_ENV);
    frame();
    bad += check("  ENV tapped again: the lock closes", !ui.lock && !(kb_lock & 8u));

    /* the knobs: COLOR; SHIFT (OCT+ held): 0.12 KNOB 1 PITCH; a pad held: that drum's TUNE and LEVEL; ALGO: the KIT */
    ui_power_on();
    btn_down(B_ENV);
    frames(800);
    {
        track_t *d = &trk[3];
        int16_t tone = d->p[P_E0 + 1], kit = d->p[P_E0];
        perf_k[0] = 0;
        turn(EN_K1, 3);
        ok = perf_k[0] > 0 && d->p[P_E0 + 1] == tone;
        btn_down(B_OCTUP);
        frame();
        turn(EN_K1, 3);
        ok &= d->p[P_E0 + 1] > tone && !strncmp(bz_head(), "[ENV] BLAZE SHIFT", 17);
        btn_up(B_OCTUP);
        frame();
        ok &= song.octave == 0;
        bad += check("knobs: COLOR; OCT+ held (SHIFT): PITCH instead, no octave", ok);
        d->p[BZ_LANE_MARK] = 127;                       /* (a sound from before: no lane values) */
        key_down(wk(0));                                /* SNARE held */
        frame();
        turn(EN_K1, 4);
        turn(EN_K3, -4);
        ok = d->p[BZ_LANE_MARK] == BZ_LANE_MAGIC && d->p[BZ_LANE_TUNE + 1] > 64 && d->p[BZ_LANE_TUNE] == 64 &&
             d->p[P_LN0 + 1] < 127 && d->p[P_LN0] == 127 && !strncmp(bz_head(), "[ENV] BLAZE DRUM", 16);
        key_up(wk(0));
        frame();
        bad += check("a pad held: KNOB 1 its TUNE, KNOB 3 its LEVEL (that drum only; the slots marked once)", ok);
        turn(EN_ALGO, 1);
        bad += check("ALGO in BLAZE: the next KIT", d->p[P_E0] != kit);
    }

    /* KEYS: OCT- flips the layer; the white keys play track 1 up A minor; the black keys the arp */
    trk[0].p[P_ROOT] = 9;
    trk[0].p[P_SCALE] = 2;
    trk[0].p[P_AMODE] = 0;
    trk[0].p[P_AHOLD] = 0;
    song.sel = 0;
    press(B_OCTDN);                                      /* (0.17: PLAY first) */
    ok = bz_play && !strncmp(bz_head(), "[ENV] PLAY", 10);
    press(B_OCTDN);
    ok = ok && bz_keys && !bz_play && bz_ktrk == 0u && !strncmp(bz_head(), "[ENV] KEYS", 10);
    v0 = vage;
    key_down(wk(0)); key_down(wk(2)); key_down(wk(7));
    ok &= kb_note[wk(0)] == 57u && kb_note[wk(2)] == 60u && kb_note[wk(7)] == 69u && kb_trk[wk(0)] == 0u;
    ok &= vage > v0 && kb_chn[wk(0)] && kb_chn[wk(2)] && kb_chn[wk(7)];   /* (track 1 may be MONO: one voice) */
    key_up(wk(0)); key_up(wk(2)); key_up(wk(7));
    frame();
    bad += check("KEYS: white 1 3 8 play A3 C4 A4 (A minor up from the root) on track 1", ok);
    key_down(bk(BZK_ARP)); frame(); key_up(bk(BZK_ARP)); frame();
    key_down(bk(BZK_LATCH)); frame(); key_up(bk(BZK_LATCH)); frame();
    key_down(bk(BZK_R32)); frame(); key_up(bk(BZK_R32)); frame();
    key_down(bk(3)); frame(); key_up(bk(3)); frame();           /* T4: the DRUM track, refused */
    ok = trk[0].p[P_AMODE] == 1 && trk[0].p[P_AHOLD] == 1 && trk[0].p[P_ARATE] == 3 && bz_ktrk == 0u;
    bad += check("  black keys: ARP on, LATCH on, 1/32; T4 (DRUM) refused", ok);
    press(B_OCTDN);
    ok = !bz_keys && bz_chord;
    press(B_OCTDN);
    ok &= bz_loops;
    press(B_OCTDN);
    bad += check("  OCT- again: CHORD, LOOPS, then back to BLAZE", ok && !bz_keys && !bz_chord && !bz_loops);
    btn_up(B_ENV);
    frame();

    /* 0.4: the riser's types; SYNTH plays a track's note bent up and lets it go */
    {
        static int32_t L[CTL], R[CTL], V[CTL];
        uint32_t ty;
        ok = 1;
        for (ty = 0; ty < BZR_SYNTH; ty++) {
            int32_t mx = 0;
            home_reset();
            bz_rs_type = (uint8_t)ty;
            bz_rs_lvl = 127;
            bz_riser_tap();
            bz_rs_age = 6u * 4u * beat_samples();       /* (three quarters in) */
            for (b = 0; b < 200u; b++) {
                memset(L, 0, sizeof L); memset(R, 0, sizeof R); memset(V, 0, sizeof V);
                bz_riser_mix(L, R, V, CTL);
                for (i = 0; i < CTL; i++)
                    mx = L[i] > mx ? L[i] : -L[i] > mx ? -L[i] : mx;
            }
            printf("    riser %s: peak %d\n", BZR_NAME[ty], mx);
            ok &= mx > 500 && mx <= BZR_PEAK;
        }
        bad += check("RISER types NOISE AIR SIREN TONE: each sounds, within the peak", ok);
        home_reset();
        bz_rs_type = BZR_SYNTH;
        bz_rs_trk = 0;
        bz_rs_top = 127;
        v0 = vage;
        bz_riser_tap();
        run(10, 0, 0);
        ok = vage > v0 && bz_rs_on == 1u;
        bz_rs_age = 7u * 4u * beat_samples();
        run(2, 0, 0);
        ok &= midi_bend_target[0] > 12 * 256;              /* (7/8 in: more than 12 of its 24 semitones) */
        run(4u * beat_samples() / CTL + 4u, 0, 0);
        ok &= !bz_riser && !bz_rs_on && midi_bend_target[0] == 0;
        bad += check("RISER SYNTH: track 1's note bent up to 24 semitones, then let go and the bend back", ok);
    }

    /* 0.4: ECHO OUT (stopped, the internal clock): the echo a beat, then the tracks muted for its bars, then stopped */
    home_reset();
    bz_eo_bars = 2;
    transport_req = 0;
    key_down(wk(BZ_EOUT)); key_up(wk(BZ_EOUT));
    run(2, 0, 0);
    ok = bz_eo == BZE_ECHO && (perf_held & PF_BIT(PF_ETHR)) && !(perf_held & BZ_MUTES);
    run(beat_samples() / CTL + 2u, 0, 0);
    ok &= bz_eo == BZE_TAIL && (perf_held & BZ_MUTES) == BZ_MUTES;
    run(8u * beat_samples() / CTL + 4u, 0, 0);
    ok &= bz_eo == BZE_IDLE && transport_req == 2u && !(perf_held & (BZ_MUTES | PF_BIT(PF_ETHR)));
    bad += check("ECHO OUT: a beat into the echo, the tracks cut while it rings 2 bars, then stopped and back", ok);
    home_reset();
    song.g[G_CLOCK] = 1;
    key_down(wk(BZ_EOUT)); key_up(wk(BZ_EOUT));
    run(12u * beat_samples() / CTL, 0, 0);
    ok = bz_eo == BZE_OUT && (perf_held & BZ_MUTES) == BZ_MUTES;
    key_down(wk(BZ_EOUT)); key_up(wk(BZ_EOUT));
    ok &= bz_eo == BZE_IDLE && !(perf_held & BZ_MUTES);
    song.g[G_CLOCK] = 0;
    bad += check("  an external clock: muted after it until ECHO OUT again", ok);

    /* 0.4: CHORD: A minor's chords; HOLD latches; stabs on the grid; OFF */
    home_reset();
    trk[2].p[P_ROOT] = 9;
    trk[2].p[P_SCALE] = 2;
    {
        uint8_t v[4];
        uint32_t n = bzc_build(&trk[2], 0, v, 0, 0, 0);
        ok = n == 3u && v[0] == 57u && v[1] == 60u && v[2] == 64u;
        n = bzc_build(&trk[2], 4, v, 1, 0, 0);          /* Em7: E G B D */
        ok &= n == 4u && v[0] == 64u && v[1] == 67u && v[2] == 71u && v[3] == 74u;
        n = bzc_build(&trk[2], 0, v, 0, 1, 0);          /* Am, 1st inversion: C E A */
        ok &= v[0] == 60u && v[1] == 64u && v[2] == 69u;
        bz_chord_name(&trk[2], 1, (char *)v, 4);
    }
    {
        char nm[12];
        bz_ch_7th = 0;
        bz_chord_name(&trk[2], 1, nm, sizeof nm);
        ok &= !strcmp(nm, "Bdim");
        bz_ch_7th = 1;
        bz_chord_name(&trk[2], 2, nm, sizeof nm);
        ok &= !strcmp(nm, "Cmaj7");
        bz_ch_7th = 0;
    }
    bad += check("CHORD: A minor: Am = A C E, Em7 = E G B D, inverted C E A; names Bdim Cmaj7", ok);
    home_reset();
    bz_chord = 1;
    bz_ctrk = 2;
    bz_ch_style = BZC_HOLD;
    trk[2].p[P_ROOT] = 9;
    trk[2].p[P_SCALE] = 2;
    v0 = vage;
    key_down(wk(0)); key_up(wk(0));
    ok = bz_ch_sel == 1u && bzc_on && vage > v0;
    run(200, 0, 0);
    ok &= bzc_on;                                      /* (held: still sounding) */
    key_down(wk(3)); key_up(wk(3));
    ok &= bz_ch_sel == 4u && bzc_note[0] == 62u;       /* (Dm took over) */
    key_down(wk(7)); key_up(wk(7));
    ok &= !bz_ch_sel && !bzc_on;
    bad += check("  HOLD: a tap latches it (it rings on), another chord takes over, OFF ends it", ok);
    bz_ch_style = BZC_R8;
    key_down(wk(0)); key_up(wk(0));
    {
        uint32_t ons = 0, was = 0;
        for (b = 0; b < beat_samples() / CTL; b++) {
            chord_block(CTL);
            if (bzc_on && !was)
                ons++;
            was = bzc_on;
        }
        ok = ons == 2u;
        printf("    1/8 stabs in a beat: %u\n", ons);
    }
    bz_ch_style = BZC_GATE;
    bz_ch_gate = 3;                                    /* TRESILLO: x..x ..x. ..x. x... */
    bz_ch_dirty = 1;
    {
        uint32_t ons = 0, was = 0;
        for (b = 0; b < 4u * beat_samples() / CTL; b++) {
            chord_block(CTL);
            if (bzc_on && !was)
                ons++;
            was = bzc_on;
        }
        ok &= ons == 5u;
        printf("    TRESILLO gate in a bar: %u\n", ons);
    }
    bz_chord_stop();
    bad += check("  stabs: 1/8 twice a beat; GATE TRESILLO 5 steps a bar", ok);

    /* 0.4 UI: OCT- BLAZE -> KEYS -> CHORD -> BLAZE; RISER held: its knobs, kept in the settings */
    ui_power_on();
    bz_keys = bz_chord = 0;
    btn_down(B_ENV);
    frames(800);
    press(B_OCTDN);
    ok = bz_play && !bz_keys;                            /* (0.17: PLAY first) */
    press(B_OCTDN);
    ok &= bz_keys && !bz_chord && !bz_play;
    press(B_OCTDN);
    ok &= !bz_keys && bz_chord && !strncmp(bz_head(), "[ENV] CHORD", 11);
    press(B_OCTDN);
    ok &= !bz_keys && !bz_chord && bz_loops && !strncmp(bz_head(), "[ENV] LOOPS", 11);
    press(B_OCTDN);
    ok &= !bz_keys && !bz_chord && !bz_loops && bz_slice && !strncmp(bz_head(), "[ENV] SLICE", 11);
    press(B_OCTDN);
    ok &= !bz_slice && bz_rb;
    press(B_OCTDN);
    ok &= !bz_keys && !bz_chord && !bz_loops && !bz_slice && !bz_rb;
    bad += check("UI: OCT- steps BLAZE -> PLAY -> KEYS -> CHORD -> LOOPS -> SLICE -> REKORDBOX -> BLAZE", ok);
    bz_cfg[BZC_MAGIC_I] = 0;
    bz_cfg_apply();
    key_down(wk(BZ_RISER));
    frame();
    turn(EN_K1, 3);
    turn(EN_K3, -5);
    ok = !strncmp(bz_head(), "[ENV] RISER SOUND", 17) && bz_cfg[BZC_MAGIC_I] == BZ_CFG_MAGIC && bz_rs_type != BZR_NOISE &&
         bz_rs_type == bz_cfg[BZC_TYPE] && bz_rs_lvl < 100u && lys.rp_dirty;
    key_up(wk(BZ_RISER));
    ok &= !bz_riser;                                    /* (0.12: held to set it up: let go, nothing starts) */
    frame();
    bad += check("UI: RISER held: KNOB 1 its TYPE, KNOB 3 its LEVEL, kept; let go after: no riser", ok);
    btn_up(B_ENV);
    frame();

    /* 0.5: LOOPS: IN / OUT on the next bar; FADE over its bars; FILL a bar; ALL IN, DRUMS ONLY, BREAK; taken back */
    home_reset();
    bz_loops = 1;
    seq_start();
    song.playing = 1;
    click_tick(0);
    for (i = 0; i < 5u; i++)
        click_tick(div_samples(2));
    key_down(wk(1)); key_up(wk(1));                 /* T2 OUT, on the next bar */
    ok = bz_lp_op[1] == BZL_OUT && !trk[1].p[P_MUTE];
    run(10u * div_samples(2) / CTL, 0, 0);
    ok &= !trk[1].p[P_MUTE];                        /* (not yet) */
    run(2u * div_samples(2) / CTL + 2u, 0, 0);
    ok &= trk[1].p[P_MUTE] && !bz_lp_op[1];
    bad += check("LOOPS: T2 OUT waits for the next bar, then muted", ok);
    key_down(wk(1)); key_up(wk(1));                 /* T2 IN ... */
    key_down(wk(1)); key_up(wk(1));                 /* ... taken back */
    ok = !bz_lp_op[1];
    bad += check("  pressed again before its point: taken back", ok);
    song.playing = 0;
    bz_lp_fbars = 2;
    key_down(wk(4)); key_up(wk(4));                 /* FADE T1 out (stopped: at once) */
    run(4u * beat_samples() / CTL, 0, 0);          /* (a bar in: half way) */
    ok = bz_fd_gain[0] > 12000 && bz_fd_gain[0] < 20000 && bz_gain[0] == bz_fd_gain[0] && !trk[0].p[P_MUTE];
    run(4u * beat_samples() / CTL + 4u, 0, 0);
    ok &= trk[0].p[P_MUTE] && bz_gain[0] == 0;
    key_down(wk(4)); key_up(wk(4));                 /* FADE T1 in */
    run(2, 0, 0);
    ok &= !trk[0].p[P_MUTE] && bz_gain[0] < 2000;
    run(8u * beat_samples() / CTL + 4u, 0, 0);
    ok &= bz_gain[0] == 32768;
    bad += check("  FADE (2 bars): out halfway after a bar, then muted at 0; in again from 0 to full", ok);
    key_down(wk(8 + 2)); key_up(wk(8 + 2));         /* FILL T3: drops a bar */
    ok = trk[2].p[P_MUTE] && bz_fill_left[2];
    run(4u * beat_samples() / CTL + 4u, 0, 0);
    ok &= !trk[2].p[P_MUTE] && !bz_fill_left[2];
    bad += check("  FILL: a layer in drops for one bar, then back", ok);
    key_down(wk(13)); key_up(wk(13));               /* DRUMS ONLY */
    ok = trk[0].p[P_MUTE] && trk[1].p[P_MUTE] && trk[2].p[P_MUTE] && !trk[3].p[P_MUTE];
    key_down(wk(14)); key_up(wk(14));               /* BREAK */
    ok &= trk[3].p[P_MUTE];
    key_down(wk(12)); key_up(wk(12));               /* ALL IN */
    ok &= !trk[0].p[P_MUTE] && !trk[1].p[P_MUTE] && !trk[2].p[P_MUTE] && !trk[3].p[P_MUTE];
    bad += check("  DRUMS ONLY, BREAK, ALL IN", ok);

    /* 0.5: DUCK: a kick dips the tracks that are not DRUM, back over RELEASE */
    home_reset();
    bz_duck = 100;
    bz_duck_ms = 100;
    v0 = vage;
    rl_hit(0u, 120u);                               /* (KICK) */
    run(1, 0, 0);
    ok = bz_kick == 0 && bz_gain[0] < 12000 && bz_gain[3] == 32768;
    run(150u * 44u / CTL, 0, 0);                    /* (150 ms) */
    ok &= bz_gain[0] == 32768;
    bz_duck = 0;
    bad += check("DUCK: a kick dips the other tracks (not DRUM) ~12 dB, back within RELEASE", ok);

    /* 0.5: a blend (the next key 9A, E minor): KEYS skips F; CHORD refuses Dm (F) and plays Am */
    home_reset();
    trk[0].p[P_ROOT] = 9; trk[0].p[P_SCALE] = 2;
    trk[2].p[P_ROOT] = 9; trk[2].p[P_SCALE] = 2;
    bz_ktrk = 0;
    bz_koct = 0;
    bz_cc_seq++;
    bz_cc[0] = 8; bz_cc[1] = 0; bz_cc[2] = 9; bz_cc[3] = 9; bz_cc[4] = 0; bz_cc[5] = 4;
    bz_ui_pass();
    ok = bz_blend == 0x0AD5u;
    ok &= bz_key_note(&trk[0], 5) == 67u && bz_key_note(&trk[0], 6) == 69u;   /* (A B C D E G A: no F) */
    bz_chord = 1;
    bz_ctrk = 2;
    bz_ch_style = BZC_HOLD;
    key_down(wk(3)); key_up(wk(3));                 /* Dm: refused */
    ok &= !bz_ch_sel;
    key_down(wk(0)); key_up(wk(0));                 /* Am */
    ok &= bz_ch_sel == 1u;
    bz_chord_stop();
    bz_cc_seq++;
    bz_cc[3] = 0;
    bz_ui_pass();
    ok &= !bz_blend && bz_key_note(&trk[0], 5) == 65u;
    bad += check("BLEND: 8A + 9A: KEYS A B C D E G (no F), CHORD refuses Dm, plays Am; cleared: F back", ok);

    /* 0.5: RAMP: 120 -> 128 over 4 bars on the internal clock */
    home_reset();
    song.g[G_BPM] = 120;
    seq_start();
    song.playing = 1;
    click_tick(0);
    bzl.to = 128;
    bzl.bars = 4;
    bz_loops_black(8);
    for (i = 0; i < 32u; i++)
        click_tick(div_samples(2));
    bz_ui_pass();
    ok = song.g[G_BPM] == 124 && bzl.on;
    for (i = 0; i < 32u; i++)
        click_tick(div_samples(2));
    bz_ui_pass();
    ok &= song.g[G_BPM] == 128 && !bzl.on;
    song.playing = 0;
    bad += check("RAMP: 120 -> 128 over 4 bars: 124 half way, 128 at the end", ok);

    /* 0.5: the key lights by fours; the battery in 10 % steps */
    {
        uint32_t lo, hi;
        bz_groups(0x7FFFFFFu, &lo, &hi);
        ok = ((lo >> wk(0)) & 1u) && ((hi >> wk(4)) & 1u) && ((lo >> wk(8)) & 1u) && ((hi >> wk(12)) & 1u) &&
             ((lo >> bk(0)) & 1u) && ((hi >> bk(4)) & 1u) && !(lo & hi);
        song.batt_raw = 501; ok &= batt_pct() == 0;
        song.batt_raw = 561; ok &= batt_pct() == 50;
        song.batt_raw = 640; ok &= batt_pct() == 100;
        bad += check("lights by fours (1st / 3rd low, 2nd / 4th brighter); battery 501 0 %, 561 50 %, full 100 %", ok);
    }

    /* 0.6: RAIL's SET sends a song: the tempo, the KIT, the tracks in at the next point; the key on CC 20..25 */
    home_reset();
    song.g[G_CLOCK] = 0;
    song.g[G_BPM] = 120;
    bz_lp_q = 0;                                    /* (NOW: the tracks at once) */
    bz_cc_seq++;
    bz_cc[0] = 9; bz_cc[1] = 0; bz_cc[2] = 4;       /* 9A, E minor */
    bz_cc[3] = 0;
    bz_cc[6] = 128 - 60; bz_cc[7] = 10 + 1; bz_cc[8] = 64 | 0x9; bz_cc[10] = 8; bz_cc[9] = 3;   /* 128, TIGHT, T1 T4 */
    bz_ui_pass();
    run(2, 0, 0);
    ok = song.g[G_BPM] == 128 && trk[3].p[P_E0] == 10 && bz_set_n == 8u && trk[0].p[P_ROOT] == 4 &&
         !trk[0].p[P_MUTE] && trk[1].p[P_MUTE] && trk[2].p[P_MUTE] && !trk[3].p[P_MUTE];
    bad += check("SET: a song from RAIL: 128 BPM, KIT TIGHT, T1 + T4 in (T2 T3 out), key E minor; 8 songs", ok);
    bz_cc_seq++;
    bz_cc[6] = 140 - 60;
    song.g[G_CLOCK] = 1;
    bz_cc[9] = 4;
    bz_ui_pass();
    ok = song.g[G_BPM] == 128;
    song.g[G_CLOCK] = 0;
    bad += check("  following an external clock: the song's BPM is not applied (Link sets it)", ok);


    /* 0.7: the external clock's tempo: over 4 beats, not one 16th; USB jitter of +-2 ms no longer moves the BPM shown */
    home_reset();
    {
        double t = 1000.0;
        int lo = 999, hi = 0, lo2 = 999, hi2 = 0, lo3 = 999, hi3 = 0;
        memset(&midi_clock, 0, sizeof midi_clock);
        song.g[G_CLOCK] = 1;
        midi_clock.mode = 1;
        clk_feed(&t, 1280, 24u * 32u, 2u, 24u * 4u, &lo, &hi);         /* 128, jitter +-2 ms: 32 beats */
        ok = lo == 128 && hi == 128 && midi_beat_samples > (uint32_t)FS * 60u / 129u &&
             midi_beat_samples < (uint32_t)FS * 60u / 127u;
        bad += check("CLOCK: 128 BPM with +-2 ms of USB jitter reads 128 throughout (it read 126..130)", ok);
        clk_feed(&t, 1400, 24u * 2u, 2u, 24u, &lo2, &hi2);                /* a jump to 140: near it in the 2nd beat, */
        ok = lo2 >= 139 && hi2 <= 144;                                  /* on it from the 3rd */
        lo2 = 999; hi2 = 0;
        clk_feed(&t, 1400, 24u * 8u, 2u, 0u, &lo2, &hi2);
        bad += check("  a jump to 140: 139..144 in the 2nd beat, 140 from the 3rd on", ok && lo2 == 140 && hi2 == 140);
        clk_feed(&t, 1275, 24u * 16u, 1u, 24u * 4u, &lo3, &hi3);          /* 127.5: steady on one of its sides */
        bad += check("  127.5 BPM: steady (no 127 / 128 flicker)", lo3 == hi3 && (lo3 == 127 || lo3 == 128));
        song.g[G_CLOCK] = 0;
        memset(&midi_clock, 0, sizeof midi_clock);
        midi_beat_samples = 0;
    }

    /* 0.7: KEY LOCK: MIDI IN onto the key RAIL sent (A minor): C# plays C; its note-off releases the C; off: as played */
    home_reset();
    {
        track_t *mt = midi_track(0);
        int sc = !drum_track(mt);
        bz_cfg_apply();
        bz_follow = 1;
        mt->p[P_ROOT] = 9;
        mt->p[P_SCALE] = 2;
        bz_lock = 1;
        midi_event(0x90u, 0, 61, 100);
        ok = sc && bz_gate_note(mt, 60) && !bz_gate_note(mt, 61);
        midi_event(0x80u, 0, 61, 0);
        ok &= !bz_gate_note(mt, 60);
        mt->p[P_ROOT] = 4;                          /* (a key change while held: the note-off still finds it) */
        mt->p[P_ROOT] = 9;
        midi_event(0x90u, 0, 63, 100);              /* D# -> D */
        mt->p[P_ROOT] = 1;
        midi_event(0x80u, 0, 63, 0);
        ok &= !bz_gate_note(mt, 62);
        mt->p[P_ROOT] = 9;
        bz_lock = 0;
        midi_event(0x90u, 0, 61, 100);
        ok &= bz_gate_note(mt, 61);
        midi_event(0x80u, 0, 61, 0);
        bz_lock = 1;
        bad += check("KEY LOCK: MIDI IN C# plays C in A minor, its note-off releases it (the key changed meanwhile "
                     "too); off: C# as played", ok);
    }

    /* 0.7: KITS: SHIFT + white 1 held a second saves the DRUM track's sound in U25 "KIT 1" (stopped); a tap loads it */
    home_reset();
    {
        track_t *d = rl_track();
        uint32_t sh = 1u << panel.btn[B_OCTUP], k0 = BZ_KIT0, v1 = vage;
        song.playing = 0;
        up_put(k0, 0);
        d->p[P_E0] = 10;                            /* (TIGHT) */
        fm1_in.buttons |= sh;
        fm1_ms = 1000;
        bz_ui_pass();
        key_down(wk(0));
        bz_ui_key(wk(0));
        ok = vage == v1;                            /* (the pad did not play) */
        fm1_ms = 1500;
        bz_ui_pass();
        ok &= !up_used(k0);
        fm1_ms = 2000;
        bz_ui_pass();
        ok &= up_used(k0) && bz_kit_used(0);
        up_name(k0, nm);
        ok &= !strcmp(nm, "KIT 1");
        key_up(wk(0));
        bz_ui_pass();
        ok &= bzkit.k == 0;
        d->p[P_E0] = 0;
        fm1_ms = 3000;
        key_down(wk(0));
        bz_ui_key(wk(0));
        fm1_ms = 3100;
        key_up(wk(0));
        bz_ui_pass();
        ok &= d->p[P_E0] == 10 && d->user == k0 + 1u;
        fm1_in.buttons &= ~sh;
        bz_ui_pass();
        v1 = vage;
        key_down(wk(0));
        ok &= vage == v1 + 1u;                      /* (no SHIFT: the pad again) */
        key_up(wk(0));
        bad += check("KITS: SHIFT + white 1 held 1 s saves KIT 1 in U25 (the pad silent), a tap loads it back", ok);
        up_put(k0 + 1u, 0);
        {
            up_rec_t r;
            memset(&r, 0, sizeof r);
            r.used = UP_USED; r.ver = UP_VER; r.engine = ENGI_DRUM; r.np = P_COUNT;
            memcpy(r.name, "MY BEATS", 8);
            up_put(k0 + 1u, &r);
        }
        fm1_in.buttons |= sh;
        bz_ui_pass();
        fm1_ms = 5000;
        key_down(wk(1));
        bz_ui_key(wk(1));
        fm1_ms = 7000;
        bz_ui_pass();
        up_name(k0 + 1u, nm);
        ok = !strcmp(nm, "MY BEATS");
        key_up(wk(1));
        bz_ui_pass();
        fm1_in.buttons &= ~sh;
        bz_ui_pass();
        bad += check("  a slot holding another sound (U26 \"MY BEATS\") is not written over", ok);
        up_put(k0, 0);
        up_put(k0 + 1u, 0);
    }


    /* 0.8: SLICE: SHIFT + black 2 makes T2 SLICE; white 3 plays slice 3 (C4 + ROOT + 2) on T2; QUANTIZE 1/16 holds a
     * press made mid-16th to the next 16th; no SLICE track: the white keys are silent (no pad) */
    home_reset();
    {
        uint32_t sh = 1u << panel.btn[B_OCTUP], v1;
        track_t *st = &trk[1];
        bz_loops = 1;
        bz_keys_toggle();                           /* (LOOPS -> SLICE) */
        ok = bz_slice && !bz_loops;
        bz_strk = 0;
        bz_sl_ok = (uint8_t)bz_slice_trk(&trk[0]);
        v1 = vage;
        key_down(wk(2));
        ok &= !bz_sl_ok && vage == v1;              /* (T1 is DRUM: silent, and no pad) */
        key_up(wk(2));
        fm1_in.buttons |= sh;
        bz_ui_pass();
        bz_slice_black(1);                          /* SHIFT + T2: T2 becomes SLICE, and SLICE's track */
        fm1_in.buttons &= ~sh;
        bz_ui_pass();
        ok &= bz_slice_trk(st) && bz_strk == 1u && bz_sl_ok && bz_slice_n(st) == 16u;
        st->engine = st->eng_req;                   /* (the ISR's switch at the next block start, done) */
        st->xf_on = 0;
        st->xp_n = 0;
        st->p[P_ROOT] = 0;
        key_down(wk(2));
        ok &= bz_gate_note(st, 62);
        key_up(wk(2));
        bad += check("SLICE: SHIFT + T2 makes T2 SLICE (16 slices of BREAK); white 3 plays its slice 3; a DRUM track: "
                     "silent", ok);

        bz_sl_q = 1;                                /* QUANTIZE 1/16 */
        seq_start();
        song.playing = 1;
        click_tick(0);
        click_tick(div_samples(2) / 2u);            /* (half a 16th in) */
        v1 = vage;
        key_down(wk(4));
        ok = vage == v1 && bz_sl_on != 0u;
        run(div_samples(2) / 2u / CTL - 2u, 0, 0);
        ok &= !bz_gate_note(st, 64);
        run(4, 0, 0);
        ok &= bz_gate_note(st, 64) && !bz_sl_on;
        key_up(wk(4));
        ok &= bz_sl_off != 0u;                       /* (the note-off waits for the next 16th too) */
        click_tick(div_samples(2) / 2u);
        run(div_samples(2) / CTL + 2u, 0, 0);
        ok &= !bz_sl_off;
        bad += check("  QUANTIZE 1/16: a press mid-16th plays on the next 16th, its release on the one after", ok);
        song.playing = 0;
        bz_sl_q = 0;
        bz_slice_black(BZS_PAGE);
        ok = !bz_sl_page;                           /* (16 slices: one page) */
        st->p[P_E1] = 3;                            /* 32 slices */
        bz_slice_black(BZS_PAGE);
        key_down(wk(0));
        ok &= bz_sl_page && bz_gate_note(st, 76);
        key_up(wk(0));
        bad += check("  32 slices: 17-32 puts slice 17 on white 1 (16 or fewer: one page)", ok);
        bz_slice = 0;
    }


    /* 0.9: a rate key TAPPED stays on: a pad then rolls at it with no rate held; tapped again: off; held: as before;
     * ALL OFF: every tapped rate off */
    home_reset();
    {
        uint32_t n1, n2, n3;
        key_down(bk(3));
        (key_up)(bk(3));                            /* (a tap: let go at once) */
        ok = (rl_latch >> 4) & 1u;
        v0 = vage;
        key_down(wk(0));
        n1 = vage - v0 + run(beat_samples() / CTL, 0, 0);
        key_up(wk(0));
        key_down(bk(3));
        (key_up)(bk(3));                            /* (tapped again: off) */
        ok &= !rl_latch && !rl_nr;
        v0 = vage;
        key_down(wk(0));
        n2 = vage - v0 + run(beat_samples() / CTL, 0, 0);
        key_up(wk(0));
        key_down(bk(3));
        key_up(bk(3));                              /* (held 300 ms: not latched) */
        ok &= !rl_latch && !rl_nr;
        key_down(bk(1));
        (key_up)(bk(1));
        key_down(bk(9));                            /* ALL OFF */
        key_up(bk(9));
        ok &= !rl_latch && !rl_nr;
        v0 = vage;
        key_down(wk(0));
        n3 = vage - v0 + run(beat_samples() / CTL, 0, 0);
        key_up(wk(0));
        bad += check("RATES (0.9): 1/16 tapped on: the pad alone rolls 4 a beat; tapped again: once; held: not kept; "
                     "ALL OFF: off", ok && n1 == 4u && n2 == 1u && n3 == 1u);
    }

    /* 0.9: PAD PRESETS: SNARE's pad held + SHIFT + black 4 (1/16) + white 5 (ECHO): saved; the pad alone then rolls at
     * 1/16 with ECHO on while held; the same keys again: plain */
    home_reset();
    {
        uint32_t sh = 1u << panel.btn[B_OCTUP], n1, n2, echo;
        bz_cfg_apply();
        key_down(wk(0));
        fm1_in.buttons |= sh;
        bz_ui_pass();
        key_down(bk(3));
        key_up(bk(3));
        key_down(wk(4));
        key_up(wk(4));
        bz_ui_pass();
        ok = rl_pp_rate[1] == 5u && rl_pp_fx[1] == 1u && bz_cfg_pp[1] == (5u | 1u << 4) && !rl_latch && !rl_nr &&
             !(perf_latched & PF_BIT(PF_ETHR));
        fm1_in.buttons &= ~sh;
        bz_ui_pass();
        key_up(wk(0));
        ok &= !(perf_held & PF_BIT(PF_ETHR));
        v0 = vage;
        key_down(wk(0));
        echo = (perf_held & PF_BIT(PF_ETHR)) != 0u;
        n1 = vage - v0 + run(beat_samples() / CTL, 0, 0);
        key_up(wk(0));
        ok &= echo && !(perf_held & PF_BIT(PF_ETHR)) && !rl_pp;
        bad += check("PAD PRESETS (0.9): SNARE + SHIFT + 1/16 + ECHO: saved; the pad alone rolls 4 a beat with ECHO on "
                     "while held", ok && n1 == 4u);
        key_down(wk(0));
        fm1_in.buttons |= sh;
        bz_ui_pass();
        key_down(bk(3));
        key_up(bk(3));
        key_down(wk(4));
        key_up(wk(4));
        fm1_in.buttons &= ~sh;
        bz_ui_pass();
        key_up(wk(0));
        v0 = vage;
        key_down(wk(0));
        n2 = vage - v0 + run(beat_samples() / CTL, 0, 0);
        key_up(wk(0));
        bad += check("  the same keys again: plain (once), saved as none", n2 == 1u && !rl_pp_rate[1] && !rl_pp_fx[1] &&
                     !bz_cfg_pp[1]);
    }


    /* 0.9 REKORDBOX: OCT- after SLICE; white 1 sends note 36 on channel 14 (deck 1), its release the note-off; black 11:
     * deck 2 (channel 15); black 1: note 60 on channel 13; KNOB 1: CC 1 on channel 13; nothing sounds */
    home_reset();
    {
        uint32_t mo, v1, pk[8], np = 0, i;
        bz_slice = 1;
        bz_keys_toggle();
        ok = bz_rb && !bz_slice && !strncmp(bz_head(), "[ENV] REKORDBOX", 15);
        usb.config = 1;
        for (i = 0; i < 8u; i++)
            home_block(CTL, CTL);                   /* (0.11: PHOSPHOR's first events out first) */
        mo = mo_w;
        v1 = vage;
        key_down(wk(0));
        key_up(wk(0));
        key_down(bk(10));
        key_up(bk(10));
        key_down(wk(2));
        key_up(wk(2));
        key_down(bk(0));
        key_up(bk(0));
        bz_rb_val[0] = 90;
        home_block(CTL, CTL);
        for (i = mo; i != mo_w && np < 8u; i++)
            pk[np++] = midi_out_q[i % MQ];
        ok &= vage == v1 && np == 7u && bz_rb_deck == 1u &&
              pk[0] == (0x09u | 0x9Du << 8 | 36u << 16 | 127u << 24) && pk[1] == (0x08u | 0x8Du << 8 | 36u << 16) &&
              pk[2] == (0x09u | 0x9Eu << 8 | 38u << 16 | 127u << 24) && pk[3] == (0x08u | 0x8Eu << 8 | 38u << 16) &&
              pk[4] == (0x09u | 0x9Cu << 8 | 60u << 16 | 127u << 24) && pk[5] == (0x08u | 0x8Cu << 8 | 60u << 16) &&
              pk[6] == (0x0Bu | 0xBCu << 8 | 1u << 16 | 90u << 24);
        bad += check("REKORDBOX (0.9): white 1 note 36 ch 14, black 11 deck 2 (white 3: note 38 ch 15), black 1 note 60 "
                     "ch 13, KNOB 1 CC 1 ch 13; silent", ok);
        mo = mo_w;
        key_down(wk(5));
        bz_keys_toggle();                           /* (left the page with the key down) */
        key_up(wk(5));
        ok = !bz_rb && mo_w == mo + 2u && midi_out_q[(mo + 1u) % MQ] == (0x08u | 0x8Eu << 8 | 41u << 16);
        bad += check("  a key let go after the page was left: its note-off still goes", ok);
        bz_rb_deck = 0;
        usb.config = 0;
    }


    /* 0.10 / 0.12: white 15 SCATTER (tap: on; again: off), white 16 BACKSPIN (on the next beat, off at the 1),
     * MORPH: SHIFT + hold 13 / 14 saves A / B, black 8 on, KNOB 2 slides COLOR and the tracks' LEVEL between them */
    home_reset();
    {
        uint32_t sh = 1u << panel.btn[B_OCTUP], i;
        key_down(wk(BZ_SCATK));
        (key_up)(wk(BZ_SCATK));
        ok = (perf_latched & PF_BIT(PF_SCAT)) != 0u && !(perf_held & PF_BIT(PF_SCAT));
        key_down(wk(BZ_SCATK));
        (key_up)(wk(BZ_SCATK));
        ok &= !(perf_latched & PF_BIT(PF_SCAT));
        key_down(wk(BZ_SCATK));
        ok &= (perf_held & PF_BIT(PF_SCAT)) != 0u;
        bz_now += BZ_TAP_MS * 441u / 10u + 1u;      /* (held past a tap: the effects time it in samples) */
        key_up(wk(BZ_SCATK));                       /* (held: off when let go) */
        ok &= !((perf_held | perf_latched) & PF_BIT(PF_SCAT));
        bad += check("SCATTER (0.12: white 15): tapped on / off, held while held", ok);

        seq_start();
        song.playing = 1;
        click_tick(0);
        click_tick(div_samples(2));                 /* (a 1/16 in: the next beat 3/16 away) */
        key_down(wk(BZ_SPINK));
        key_up(wk(BZ_SPINK));
        ok = bz_spin == BZB_ARMED && (perf_held & PF_BIT(PF_SPIN)) && pf_spin_cue == 1u;
        home_block(CTL, 3u * div_samples(2) - CTL);
        ok &= pf_spin_cue == 1u;
        home_block(CTL, 2u * CTL);
        ok &= bz_spin == BZB_SPUN && pf_spin_cue == 2u && bz_spin_left > beat_samples() / 2u;
        home_block(CTL, bz_spin_left + CTL);
        ok &= bz_spin == BZB_IDLE && !(perf_held & PF_BIT(PF_SPIN)) && !pf_spin_cue;
        bad += check("BACKSPIN (0.12: white 16): armed, spins on the next beat (a quarter beat on at least), off at the next 1", ok);
        song.playing = 0;

        for (i = 0; i < NTRK; i++)
            trk[i].p[P_LEVEL] = 100;
        perf_k[0] = -50;
        fm1_ms = 10000;
        fm1_in.buttons |= sh;
        bz_ui_pass();
        key_down(wk(12));
        bz_ui_key(wk(12));
        fm1_ms = 10800;
        bz_ui_pass();
        key_up(wk(12));
        bz_ui_pass();
        for (i = 0; i < NTRK; i++)
            trk[i].p[P_LEVEL] = 20;
        perf_k[0] = 80;
        fm1_ms = 20000;
        key_down(wk(13));
        bz_ui_key(wk(13));
        fm1_ms = 20200;
        bz_ui_pass();
        ok = bzm.ok == 1u;                          /* (B not yet: held too short) */
        fm1_ms = 20800;
        bz_ui_pass();
        key_up(wk(13));
        bz_ui_pass();
        ok &= bzm.ok == 3u;
        fm1_in.buttons &= ~sh;
        bz_ui_pass();
        key_down(bk(BZB_MORPH));                    /* (0.12: black 8, no SHIFT) */
        bz_ui_key(bk(BZB_MORPH));
        key_up(bk(BZB_MORPH));
        ok &= bzm.on;
        bzm.pos = 0;
        bz_knob(1, 0);
        bz_knob(1, 1);                              /* (a step: to A + a little) */
        bzm.pos = 50;
        bzm_apply();
        ok &= perf_k[0] == 15 && trk[0].p[P_LEVEL] == 60 && trk[3].p[P_LEVEL] == 60;
        bzm.pos = 100;
        bzm_apply();
        ok &= perf_k[0] == 80 && trk[2].p[P_LEVEL] == 20;
        bad += check("MORPH: SHIFT + held 13 / 14 save A / B (a short hold does not), black 8 on, KNOB 2 halfway: halfway", ok);
        bzm.on = 0;
        bzm.ok = 0;
        perf_k[0] = 0;
    }


    /* 0.10: browsing hides TINE EP, BRASS, MARIMBA, ORGAN (FM6: BELL, FM BASS, PAD, PLUCK shown); PERC KIT (DRUM); program
     * change still counts them (FM6 PC 4: PAD) */
    {
        const engine_t *f = ENGINES[ENGI_FM6];
        uint32_t i, ok2 = preset_shown(ENGI_FM6) == 4u && preset_pc_n(ENGI_FM6) == 8u;
        for (i = 0; i < f->npresets; i++)
            ok2 &= preset_hidden(f, i) == (!strcmp(f->presets[i].name, "TINE EP") || !strcmp(f->presets[i].name, "BRASS") ||
                                           !strcmp(f->presets[i].name, "MARIMBA") || !strcmp(f->presets[i].name, "ORGAN"));
        for (i = 0; i < ENGINES[ENGI_DRUM]->npresets; i++)
            if (!strcmp(ENGINES[ENGI_DRUM]->presets[i].name, "PERC KIT"))
                ok2 &= preset_hidden(ENGINES[ENGI_DRUM], i);
        bad += check("PRESETS (0.10): browsing hides TINE EP BRASS MARIMBA ORGAN PERC KIT; program change counts them", ok2);
    }

    /* 0.11 ROLL SHAPE: ACCENT 2 every other hit at half; FADE falls over the bar */
    home_reset();
    {
        uint32_t v[8], nv = 0, b, v0, f[4], nf = 0;
        rl_shape = RLS_ACC2;
        key_down(wk(0));
        key_down(bk(3));                            /* (1/16) */
        for (b = 0; b < 4u * beat_samples() / CTL && nv < 8u; b++) {
            v0 = vage;
            roll_block(CTL, CTL);
            if (vage != v0)
                v[nv++] = rl_vel_last;
        }
        ok = nv == 8u && v[0] == v[2] && v[1] == v[3] && v[2] == v[4] && v[1] * 2u <= v[0] + 1u && v[1] > 0u;
        key_up(bk(3));
        key_up(wk(0));
        bz_release_all();                           /* (0.13: a rate picked with the pad held: it rolled on) */
        rl_shape = RLS_FADE;
        key_down(wk(0));
        key_down(bk(1));                            /* (1/8: 8 a bar) */
        for (b = 0; b < 4u * beat_samples() / CTL + 2u && nf < 4u; b++) {
            v0 = vage;
            roll_block(CTL, CTL * 2u);              /* (twice as fast: 4 hits over the bar's first half) */
            if (vage != v0)
                f[nf++] = rl_vel_last;
        }
        ok &= nf == 4u && f[0] > f[1] && f[1] > f[2] && f[2] > f[3];
        key_up(bk(1));
        key_up(wk(0));
        bz_release_all();
        rl_shape = RLS_FLAT;
        bad += check("ROLL SHAPE (0.11): ACCENT 2 halves every other hit; FADE falls along the bar", ok);
    }

    /* 0.11 DROP: SHIFT + black 3 playing, a 1/16 into bar 1: from bar 2 to the phrase's next 1 (15 bars): the RISER over
     * them, COLOR up into the high pass, BACKSPIN asked for on the last beat, all back on the 1 (CC 41 127 for PHOSPHOR) */
    home_reset();
    {
        uint32_t sh = 1u << panel.btn[B_OCTUP], tot, i, mo, seen = 0;
        seq_start();
        song.playing = 1;
        click_tick(0);
        click_tick(div_samples(2));
        perf_k[0] = -20;
        bz_spin = BZB_IDLE;
        bz_ev_on = 1;                               /* (BLAZE opened: PHOSPHOR's events out) */
        (void)sh;
        key_down(wk(BZ_DROPK));                     /* (0.12: white 12; it arms when let go) */
        key_up(wk(BZ_DROPK));
        ok = bz_drop == BZD_ARMED && bz_dr_bars == 15u && !(rl_runs | rl_latch);
        home_block(CTL, 15u * div_samples(2) + CTL);
        tot = 15u * 16u * div_samples(2);
        ok &= bz_drop == BZD_RUN && bz_riser == 15u && perf_k[0] <= -19;
        home_block(CTL, tot / 2u);
        ok &= perf_k[0] > -10 && perf_k[0] < 40;
        home_block(CTL, tot / 2u - 2u * beat_samples());
        ok &= perf_k[0] > 50 && bz_spin == BZB_IDLE;
        home_block(CTL, beat_samples() / 2u + CTL);
        ok &= bz_spin == BZB_ARMED;
        usb.config = 1;
        for (i = 0; i < 8u; i++)
            home_block(CTL, 0);
        mo = mo_w;
        home_block(CTL, beat_samples() + beat_samples() / 2u);
        home_block(CTL, 0);
        for (i = mo; i != mo_w; i++)
            seen |= midi_out_q[i % MQ] == (0x0Bu | 0xBFu << 8 | 41u << 16 | 127u << 24);
        ok &= bz_drop == BZD_IDLE && perf_k[0] == -20 && !bz_riser && seen;
        bad += check("DROP (0.11): to the phrase's 1: RISER over the bars, COLOR up, BACKSPIN on the last beat, back on the 1", ok);
        key_down(wk(BZ_DROPK));                     /* (again while armed: off) */
        key_up(wk(BZ_DROPK));
        ok = bz_drop == BZD_ARMED;
        key_down(wk(BZ_DROPK));
        key_up(wk(BZ_DROPK));
        ok &= bz_drop == BZD_IDLE;
        key_down(wk(BZ_DROPK));                     /* (held + a knob: set up, nothing armed) */
        bz_knob(1, 1);
        key_up(wk(BZ_DROPK));
        ok &= bz_drop == BZD_IDLE && bz_d_len == BZDL_4;
        bz_cfg3[BZC3_DROP] = 0;
        bz_cfg_apply();
        bad += check("  tapped again: off; held + a knob: its LENGTH, nothing armed", ok);
        mo = mo_w;
        bz_s16 = bz_org + 16u * 4u + 3u;            /* (bar 5) */
        for (i = 0; i < 8u; i++)
            home_block(CTL, 0);
        seen = 0;
        for (i = mo; i != mo_w; i++)
            seen |= midi_out_q[i % MQ] == (0x0Bu | 0xBFu << 8 | 40u << 16 | 5u << 24);
        bad += check("PHOSPHOR (0.11): CC 40 on channel 16 says the phrase's bar", seen);
        usb.config = 0;
        song.playing = 0;
        perf_k[0] = 0;
        bz_spin_off();
    }

    /* 0.11 MORPH A / B kept in the settings; RIDE 8 (stopped: in real time) slides A -> B and lets go */
    home_reset();
    {
        uint32_t sh = 1u << panel.btn[B_OCTUP], i, len;
        memset(&bzm, 0, sizeof bzm);
        for (i = 0; i < BZM_N; i++) {
            bzm.v[0][i] = (int16_t)(i == 0u ? -60 : 10);
            bzm.v[1][i] = (int16_t)(i == 0u ? 40 : 90);
        }
        bzm_store(0);
        bzm_store(1);
        memset(&bzm, 0, sizeof bzm);
        bzm_load();
        ok = bzm.ok == 3u && bzm.v[0][0] == -60 && bzm.v[1][0] == 40 && bzm.v[1][BZM_N - 1u] == 90 &&
             favorites.factory[ENGI_PHYS][8] == BZM_MAGIC && favorites.factory[ENGI_SAMPLE][8] == BZM_MAGIC;
        bad += check("MORPH (0.11): A and B kept with the settings, back after a power-off", ok);
        song.playing = 0;
        fm1_ms = 50000;
        key_down(bk(BZB_RIDE));                     /* (0.12: black 7, no SHIFT) */
        bz_ui_key(bk(BZB_RIDE));
        key_up(bk(BZB_RIDE));
        len = bzride.len;
        ok = bzride.on && bzm.on && bzride.to == 100u && !(rl_rates | rl_latch) && len > 15000u && len < 17000u;
        fm1_ms += len / 2u;
        bz_ui_pass();
        ok &= bzm.pos >= 49u && bzm.pos <= 51u && perf_k[0] >= -11 && perf_k[0] <= -9;
        fm1_ms += len;
        bz_ui_pass();
        ok &= !bzride.on && bzm.pos == 100u && perf_k[0] == 40 && bz_ev_morph == 127u;
        bad += check("RIDE (0.12: black 7) slides A -> B over 8 bars, then stops on B", ok);
        fm1_in.buttons |= sh;
        bz_ui_pass();
        key_down(bk(3));                            /* SHIFT + black 4: ROLL SHAPE, kept */
        bz_ui_key(bk(3));
        key_up(bk(3));
        ok = rl_shape == RLS_ACC2 && bz_cfg2[BZC2_SHAPE] == RLS_ACC2 && !(rl_rates | rl_latch);
        rl_shape = RLS_FLAT;
        bz_cfg2[BZC2_SHAPE] = 0;
        bad += check("  SHIFT + black 4: ROLL SHAPE cycles, kept", ok);
        fm1_in.buttons &= ~sh;
        bz_ui_pass();
        memset(&bzm, 0, sizeof bzm);
        favorites.factory[ENGI_PHYS][8] = favorites.factory[ENGI_SAMPLE][8] = 0;
        perf_k[0] = 0;
    }

    /* 0.11 LOOP CAPTURE: stopped, SHIFT + white 11: 1 bar recorded now (the echo off); then the loop in place of the
     * tracks, as recorded (ADPCM: within 20 dB); again: MIX (the tracks back over it); RELEASE: out, the line cleared,
     * the echo back. LENGTH (SHIFT + white 12) 4 -> 1 -> 2; 4 bars do not fit below 81 BPM */
    home_reset();
    {
        static int32_t in[2 * 44100], l[CTL], r[CTL], w[CTL];
        uint32_t sh = 1u << panel.btn[B_OCTUP], i, j, t = 0, len;
        double e = 0, p = 0;
        bz_cap = BZC_IDLE;
        bz_cap_pend = 0;
        bz_cfg2[BZC2_CAPLEN] = 0;
        bz_cfg_apply();
        ok = bz_cap_bars == 4u;
        fm1_in.buttons |= sh;
        bz_ui_pass();
        key_down(wk(10));                           /* (0.12: SHIFT + white 11) */
        bz_ui_key(wk(10));
        (key_up)(wk(10));
        ok &= bz_cap_bars == 1u && bz_cfg2[BZC2_CAPLEN] == 1u;
        song.g[G_BPM] = 60;
        ok &= bz_cap_fit(4) == 2u && bz_cap_fit(2) == 2u;
        song.g[G_BPM] = 120;
        fm1_in.buttons &= ~sh;                      /* (0.12: CAPTURE is white 13, no SHIFT) */
        bz_ui_pass();
        key_down(wk(BZ_CAPK));
        (key_up)(wk(BZ_CAPK));
        ok &= bz_cap == BZC_ARMED && !fx_dly_off;
        len = 16u * div_samples(2);
        for (i = 0; i < 2u * 44100u; i++)
            in[i] = (int32_t)(8000.0 * sin(i * 2.0 * 3.14159265 * 440.0 / 44100.0) + 3000.0 * sin(i * 2.0 * 3.14159265 * 97.0 / 44100.0));
        while (t < len + CTL) {
            for (j = 0; j < CTL; j++) {
                l[j] = r[j] = in[t + j];
                w[j] = 0;
            }
            bz_cap_mix(l, r, w, CTL);
            t += CTL;
            if (t == CTL)
                ok &= bz_cap == BZC_REC && fx_dly_off;
        }
        ok &= bz_cap == BZC_SOLO && bz_cap_got == 1u && bz_cap_len == len / 2u;
        bad += check("LOOP CAPTURE (0.11): LENGTH 4 -> 1 (4 bars at 60 BPM: 2); stopped: 1 bar recorded now, the echo off",
                     ok);
        bz_cap_rewind();                            /* (from the loop's 1, the tracks silent: what plays is the loop) */
        bz_cap_gp = 256;
        bz_cap_gl = 0;
        for (t = 0; t < 22050u; t += CTL) {
            for (j = 0; j < CTL; j++)
                l[j] = r[j] = w[j] = 9999;
            bz_cap_mix(l, r, w, CTL);
            for (j = 0; j < CTL; j++)
                if (t + j > 64u && t + j + 1u < 22050u) {
                    double d = (double)l[j] - in[t + j - 1u];
                    e += d * d;
                    p += (double)in[t + j - 1u] * in[t + j - 1u];
                }
        }
        ok = 10.0 * log10(p / (e + 1.0)) > 20.0 && w[0] == 0;
        printf("  (capture: %.1f dB)\n", 10.0 * log10(p / (e + 1.0)));
        bad += check("  SOLO: the loop alone, as recorded (ADPCM within 20 dB)", ok);
        key_down(wk(BZ_CAPK));
        (key_up)(wk(BZ_CAPK));
        ok = bz_cap == BZC_SOLO && bz_cap_pend == BZC_MIX;
        for (t = 0; t < 512u; t += CTL) {
            for (j = 0; j < CTL; j++) {
                l[j] = r[j] = 1000;
                w[j] = 0;
            }
            bz_cap_mix(l, r, w, CTL);
        }
        ok &= bz_cap == BZC_MIX && bz_cap_gl == 256 && bz_cap_gp == 256;
        bad += check("  again: MIX at the bar (stopped: now), the tracks back over the loop", ok);
        bz_release_all();
        ok = bz_cap == BZC_FADE;
        for (t = 0; t < 512u; t += CTL)
            bz_cap_mix(l, r, w, CTL);
        ok &= bz_cap == BZC_CLEAR && fx_dly_off;
        bz_ui_pass();
        ok &= bz_cap == BZC_IDLE && !fx_dly_off && !dly_buf[100] && !dly_buf[20000];
        bad += check("  RELEASE: the loop out, the line cleared, the echo back", ok);
        fm1_in.buttons |= sh;
        bz_ui_pass();
        key_down(wk(11));                           /* (SAVE, SHIFT + white 12: no loop: says so, nothing changes) */
        bz_ui_key(wk(11));
        (key_up)(wk(11));
        ok = bz_cap == BZC_IDLE;
        bad += check("  SAVE without a loop: nothing", ok);
        fm1_in.buttons &= ~sh;
        bz_ui_pass();
        bz_cfg2[BZC2_CAPLEN] = 0;
        bz_cap_bars = 4;
    }

    /* 0.11.1 SHIFT LOCK: OCT+ double-tapped on BLAZE's page locks SHIFT (BACKSPIN with one press); one tap unlocks;
     * slow taps, long holds or a key pressed with it do not; another BLAZE page drops it */
    home_reset();
    {
        uint32_t sh = 1u << panel.btn[B_OCTUP], ly = ui.layer;
#define OCT_TAP(ms_down, ms_after) do { fm1_in.buttons |= sh; bz_ui_pass(); fm1_ms += (ms_down); \
        fm1_in.buttons &= ~sh; bz_ui_pass(); fm1_ms += (ms_after); } while (0)
        ui.layer = LAYER_BLAZE;
        bz_spin = BZB_IDLE;
        fm1_ms = 100000;
        OCT_TAP(80, 600);
        OCT_TAP(80, 100);
        ok = !bz_shlock;                            /* (too slow: two single taps) */
        OCT_TAP(400, 100);
        OCT_TAP(400, 100);
        ok &= !bz_shlock;                           /* (held: SHIFT used, not tapped) */
        OCT_TAP(80, 150);
        OCT_TAP(80, 100);
        bz_ui_pass();
        ok &= bz_shlock && bz_shift() && bz_shift_on && !(fm1_in.buttons & sh);
        bad += check("SHIFT LOCK (0.11.1): OCT+ double-tapped locks SHIFT; slow taps and holds do not", ok);
        key_down(wk(9));                            /* (0.12: SHIFT's FREEZE on white 10, one press: latched) */
        (key_up)(wk(9));
        ok = (perf_latched & PF_BIT(PF_FRZ)) != 0u;
        key_down(wk(9));
        (key_up)(wk(9));
        ok &= !(perf_latched & PF_BIT(PF_FRZ));
        OCT_TAP(80, 600);
        bz_ui_pass();
        ok &= !bz_shlock && !bz_shift_on;
        bad += check("  locked: white 10 is FREEZE with no OCT+ held; one tap unlocks", ok);
        OCT_TAP(80, 150);
        OCT_TAP(80, 100);
        ok = bz_shlock;
        bz_keys = 1;                                /* (KEYS: another BLAZE page) */
        bz_ui_pass();
        ok &= !bz_shlock;
        bz_keys = 0;
        bz_ui_pass();
        bad += check("  another BLAZE page: unlocked", ok);
        ui.layer = ly;
#undef OCT_TAP
    }

    /* 0.11.1: RAIL's bar (ch16 CC 32): 5 = the bar playing is bar 5, the beat kept; 64 = the nearest bar line is the 1 */
    home_reset();
    {
        bz_org = 1000;
        bz_s16 = 1000 + 16u * 9u + 3u;              /* (bar 10, its 4th 16th) */
        midi_event(0xB0u, 15u, 32u, 5u);
        bz_ui_pass();
        ok = (bz_s16 - bz_org) / 16u % BZ_PHRASE == 4u && (bz_s16 - bz_org) % 16u == 3u;
        midi_event(0xB0u, 15u, 32u, 20u);
        bz_ui_pass();           /* (a 32-bar phrase's bar 20: bar 4 of 16) */
        ok &= (bz_s16 - bz_org) / 16u % BZ_PHRASE == 3u && (bz_s16 - bz_org) % 16u == 3u;
        midi_event(0xB0u, 15u, 32u, 64u);           /* (early in the bar: this bar is the 1) */
        bz_ui_pass();
        ok &= bz_s16 - bz_org == 3u;
        bz_s16 += 8u;                               /* (late in the bar: the next bar line is the 1) */
        midi_event(0xB0u, 15u, 32u, 64u);           /* (the same value again: taken again) */
        bz_ui_pass();
        ok &= (bz_s16 - bz_org) % 256u == 256u - 5u;
        bad += check("RAIL BAR (0.11.1): CC 32 sets the phrase's bar (the beat kept); 64: the nearest bar line is the 1", ok);
        bz_org = bz_s16 = 0;
    }

    /* 0.12 FILL (black 11): stopped, one bar of BUILD's drum, 1/8 1/16 1/32 1/32 a beat each (2 + 4 + 8 + 8), then it
     * stops; again while it runs: stopped */
    home_reset();
    bz_cfg3[BZC3_MAGIC_I] = 0;
    bz_cfg_apply();
    {
        uint32_t bar4 = 4u * beat_samples();
        key_down(bk(BZB_FILL));
        (key_up)(bk(BZB_FILL));
        ok = bz_build == 1u && bz_b_fill;
        n = run(bar4 / CTL + 4u, 0, 0);
        ok &= n == 22u && !bz_build && !bz_b_fill;
        printf("    fill hits: %u\n", n);
        bad += check("FILL (0.12): stopped: a bar of 1/8 1/16 1/32 1/32 on BUILD's drum (22 hits), then stops", ok);
        key_down(bk(BZB_FILL));
        (key_up)(bk(BZB_FILL));
        run(10, 0, 0);
        key_down(bk(BZB_FILL));
        (key_up)(bk(BZB_FILL));
        bad += check("  again while it runs: stopped", !bz_build);
    }

    /* 0.12 BUILD held + the knobs: DRUM, SPEED, SWELL, WITH, kept; let go after: nothing starts. A tap: EIGHTHS
     * (1/8 1/8 1/16 1/32: 8 + 8 + 16 + 32), SWELL 0 (all 127), FILTER (COLOR up, back after) */
    home_reset();
    bz_cfg3[BZC3_MAGIC_I] = 0;
    bz_cfg_apply();
    {
        uint32_t bar4 = 4u * beat_samples(), lo = 127, hi = 0;
        int8_t kmax = -100;
        key_down(wk(BZ_B4));
        bz_knob(0, 1);                              /* (SNARE -> CLAP) */
        bz_knob(1, 1);                              /* (1/4 > 1/32 -> 1/8 > 1/32) */
        bz_knob(3, 1);                              /* (none -> FILTER) */
        key_up(wk(BZ_B4));
        ok = !bz_build && bz_b_lane == 2u && bz_b_curve == BZBC_EIGHTHS && bz_b_fx == BZBX_FILTER &&
             bz_cfg3[BZC3_MAGIC_I] == BZ_CFG3_MAGIC && bz_cfg3[BZC3_BUILD] == (2u | BZBC_EIGHTHS << 3 | BZBX_FILTER << 5);
        bad += check("BUILD (0.12): held + KNOB 1 / 2 / 4: DRUM CLAP, SPEED 1/8 > 1/32, WITH FILTER, kept; nothing starts", ok);
        bz_cfg3[BZC3_SWELL] = 0;
        bz_cfg_apply();
        perf_k[0] = -30;
        key_down(wk(BZ_B4));
        (key_up)(wk(BZ_B4));
        n = 0;
        for (b = 0; b < 4u * bar4 / CTL + 4u; b++) {
            v0 = vage;
            home_block(CTL, CTL);
            if (vage != v0) {
                n += vage - v0;
                lo = rl_vel_last < lo ? rl_vel_last : lo;
                hi = rl_vel_last > hi ? rl_vel_last : hi;
            }
            kmax = perf_k[0] > kmax ? perf_k[0] : kmax;
            if (!bz_build)
                break;
        }
        ok = n == 64u && lo == 127u && hi == 127u && kmax >= 55 && perf_k[0] == -30 && !bz_build;
        printf("    build: %u hits, velocity %u..%u, COLOR to %d, back %d\n", n, lo, hi, kmax, perf_k[0]);
        bad += check("  a tap: 1/8 1/8 1/16 1/32 (64 hits), SWELL 0 all 127, FILTER: COLOR up, back after", ok);
        bz_cfg3[BZC3_MAGIC_I] = 0;
        bz_cfg_apply();
    }

    /* 0.12 DROP set up: LENGTH 4 bars, WITH BUILD (no BACKSPIN), ON THE 1 CRASH; TOP 0: COLOR stays */
    home_reset();
    bz_cfg3[BZC3_MAGIC_I] = 0;
    bz_cfg_apply();
    {
        uint32_t tot, v1;
        bz_cfg3[BZC3_DROP] = (uint8_t)(BZDL_4 | BZDX_BUILD << 2 | BZDH_CRASH << 4);
        bz_cfg3[BZC3_TOP] = 0;
        bz_cfg_apply();
        seq_start();
        song.playing = 1;
        click_tick(0);
        click_tick(div_samples(2));
        perf_k[0] = -20;
        bz_spin = BZB_IDLE;
        key_down(wk(BZ_DROPK));
        key_up(wk(BZ_DROPK));
        ok = bz_drop == BZD_ARMED && bz_dr_bars == 4u;
        home_block(CTL, 15u * div_samples(2) + CTL);
        ok &= bz_drop == BZD_RUN && bz_build == 4u && !bz_b_fill;
        tot = 4u * 16u * div_samples(2);
        for (b = 0; b < tot / CTL - 2u; b++)
            home_block(CTL, CTL);
        ok &= perf_k[0] == -20 && bz_spin == BZB_IDLE && bz_drop == BZD_RUN;
        v1 = vage;
        for (b = 0; b < 8u && bz_drop != BZD_IDLE; b++)
            home_block(CTL, CTL);
        ok &= bz_drop == BZD_IDLE && vage > v1 && rl_vel_last == 127u && !bz_build && perf_k[0] == -20;
        bad += check("DROP (0.12): LENGTH 4 bars, WITH BUILD (no BACKSPIN), TOP 0 (COLOR stays), CRASH on the 1", ok);
        bz_cfg3[BZC3_MAGIC_I] = 0;
        bz_cfg_apply();
        song.playing = 0;
    }

    /* 0.12 a pad held + ALGO: that pad another drum, none the other pads have, kept; the key plays it */
    home_reset();
    ui.layer = LAYER_BLAZE;
    memset((void *)bz_pad_slot, 0, sizeof bz_pad_slot);
    for (i = 0; i < 4u; i++)
        bz_cfg_slot[i] = 0;
    {
        uint32_t p0 = bz_pad_of(0), p1 = bz_pad_of(1), p2 = bz_pad_of(2), p3 = bz_pad_of(3), q;
        key_down(wk(0));
        bz_algo(1);
        q = bz_pad_of(0);
        key_up(wk(0));
        ok = q != p0 && q != p1 && q != p2 && q != p3 && q < NLANE && bz_pad_of(1) == p1 && bz_cfg_slot[0] == q + 1u &&
             bz_cfg_slot[1] == p1 + 1u;
        bz_cfg_apply();
        ok &= bz_pad_of(0) == q;
        printf("    pad 1: lane %u -> %u (the others %u %u %u)\n", p0, q, p1, p2, p3);
        bad += check("PAD SWAP (0.12): pad held + ALGO: another drum, none the others have, kept", ok);
        memset((void *)bz_pad_slot, 0, sizeof bz_pad_slot);
        for (i = 0; i < 4u; i++)
            bz_cfg_slot[i] = 0;
    }

    /* 0.12 FOLLOW (SHIFT + black 5, RAIL's CC 33): off: the clock internal at the tempo it had, still playing, CC 33 0
     * out; RAIL's 127: back on */
    home_reset();
    ui.layer = LAYER_BLAZE;
    {
        uint32_t bpm;
        song.g[G_CLOCK] = 1;
        events_block(CTL);
        seq_start();
        song.playing = 1;
        bz_tx_r = bz_tx_w;
        fm1_in.buttons |= 1u << panel.btn[B_OCTUP];   /* (SHIFT held) */
        bz_ui_pass();
        key_down(bk(4));
        bz_ui_key(bk(4));
        (key_up)(bk(4));
        fm1_in.buttons &= ~(1u << panel.btn[B_OCTUP]);
        bz_ui_pass();
        bpm = (FS * 60u + beat_samples() / 2u) / beat_samples();
        ok = !song.g[G_CLOCK] && song.g[G_BPM] == (int16_t)bpm && bz_clk_soft && bz_tx_w == (uint8_t)(bz_tx_r + 1u) &&
             bz_tx[bz_tx_r % 8u] == (0x0Bu | 0xBFu << 8 | 33u << 16);
        events_block(CTL);
        ok &= song.playing && !bz_clk_soft && midi_clock.mode == 0u;
        bad += check("FOLLOW (0.12): SHIFT + black 5: off, the FM-1's own clock at its tempo, playing on, CC 33 0 to RAIL", ok);
        midi_event(0xB0u, 15u, 33u, 127u);
        bz_ui_pass();
        ok = song.g[G_CLOCK] == 1;
        events_block(CTL);
        ok &= song.playing;
        midi_event(0xB0u, 15u, 33u, 1u);
        bz_ui_pass();
        ok &= !song.g[G_CLOCK];
        bad += check("  RAIL's CC 33: 127 on (still playing), 1 off", ok);
        song.playing = 0;
        song.g[G_CLOCK] = 0;
        events_block(CTL);
    }

    /* 0.12 RAIL: CC 36 actions (9 FILL), CC 50..53 knobs (the first value only taken as where it is); the SELECT knob
     * following: CC 34 64 + tenths out */
    home_reset();
    ui.layer = LAYER_BLAZE;
    {
        uint32_t mo, seen = 0;
        midi_event(0xB0u, 15u, 36u, 9u);
        bz_ui_pass();
        home_block(CTL, CTL);
        ok = bz_build == 1u && bz_b_fill;
        midi_event(0xB0u, 15u, 36u, 8u);
        bz_ui_pass();
        home_block(CTL, CTL);
        ok &= !bz_build;
        bad += check("RAIL ACTIONS (0.12): CC 36 9 FILL, 8 ALL OFF", ok);
        for (i = 0; i < 4u; i++)                    /* (ALL OFF's COLOR and DEPTH home, settled) */
            bz_ui_pass();
        perf_k[0] = perf_k[2] = 0;
        perf_k[3] = 50;
        midi_event(0xB0u, 15u, 50u, 40u);
        midi_event(0xB0u, 15u, 52u, 40u);
        midi_event(0xB0u, 15u, 53u, 40u);
        bz_ui_pass();
        ok = perf_k[0] == 0 && perf_k[2] == 0 && perf_k[3] == 50;
        midi_event(0xB0u, 15u, 50u, 127u);
        midi_event(0xB0u, 15u, 52u, 127u);
        midi_event(0xB0u, 15u, 53u, 127u);
        bz_ui_pass();
        ok &= perf_k[0] == 100 && perf_k[3] == 0 && perf_k[2] == 100;
        midi_event(0xB0u, 15u, 50u, 0u);
        bz_ui_pass();
        ok &= perf_k[0] == -100;
        bad += check("RAIL KNOBS (0.12): CC 50 COLOR, 52 DEPTH, 53 THROW; the first value only noted", ok);
        perf_k[0] = perf_k[2] = 0;
        perf_k[3] = 100;
        song.g[G_CLOCK] = 1;
        bz_ev_on = 1;
        usb.config = 1;
        mo = mo_w;
        ok = bz_select_turn(1);
        home_block(CTL, 0);
        for (i = mo; i != mo_w; i++) {
            uint32_t m = midi_out_q[i % MQ];
            seen |= (m & 0xFFFFFFu) == (0x0Bu | 0xBFu << 8 | 34u << 16) && (m >> 24) > 64u;
        }
        song.g[G_CLOCK] = 0;
        ok &= seen && !bz_select_turn(1);
        bad += check("SELECT (0.12): following: CC 34 (64 + tenths) to RAIL; on its own clock: not taken", ok);
    }

    /* 0.12 KEYS: white keys by degree: home lit, its 3rd and 5th brighter, the rest low; PRESETS: the KEYS track's sound */
    ui_power_on();
    btn_down(B_ENV);
    frames(800);
    {
        uint32_t on, can, lo, hi, cm = 0, tot0, c0, c1;
        trk[0].p[P_ROOT] = 9;
        trk[0].p[P_SCALE] = 2;
        bz_ktrk = 0;
        bz_keys = 1;
        frame();
        for (i = 0; i < 27u; i++) {
            bz_led(i, &on, &can);
            cm |= can << i;
        }
        bz_groups(cm, &lo, &hi);
        bz_led(wk(0), &on, &can);
        ok = on;
        bz_led(wk(7), &on, &can);
        ok &= on;
        bz_led(wk(1), &on, &can);
        ok &= !on;
        ok &= ((hi >> wk(2)) & 1u) && ((hi >> wk(4)) & 1u) && ((hi >> wk(9)) & 1u) && ((lo >> wk(1)) & 1u) &&
              ((lo >> wk(3)) & 1u) && !((hi | lo) >> bk(0) & 1u);
        bad += check("KEYS (0.12): home lit, the 3rd and 5th brighter, the rest of the scale low, black keys apart", ok);
        song.sel = 1;
        track_select(0);
        c0 = preset_pos(&tot0);
        turn(EN_PRESET, 1);
        c1 = preset_pos(&tot0);
        ok = c1 != c0 && TSEL == &trk[0] && !strncmp(ui.msg, "KEYS: ", 6);
        bad += check("  PRESETS: the KEYS track's next sound, named", ok);
        bz_keys = 0;
        bz_chord = 1;
        frame();
        track_select(0);
        c0 = preset_pos(&tot0);
        turn(EN_PRESET, 1);
        ok = TSEL == &trk[bz_ctrk % NTRK] && !strncmp(ui.msg, "CHORD: ", 7) && trk[0].p[P_SCALE] == 2;
        bad += check("  CHORD (0.12.1): PRESETS: the CHORD track's next sound, named", ok);
        bz_chord = 0;
        frame();
    }
    btn_up(B_ENV);
    frame();

    /* 0.12 CHORD's generator: AUTO plays the progression a bar a chord (playing); MELODY plays the KEYS track over it */
    home_reset();
    {
        uint32_t sel[4], seen = 0, notes = 0, bar4 = 4u * beat_samples(), j;
        trk[0].p[P_ROOT] = 9;
        trk[0].p[P_SCALE] = 2;
        trk[1].p[P_ROOT] = 9;
        trk[1].p[P_SCALE] = 2;
        bz_ctrk = 1;
        bz_ktrk = 0;
        bz_chord = 1;
        seq_start();
        song.playing = 1;
        click_tick(0);
        bz_org = bz_s16 = 0;
        bz_gen_key(0);                              /* (AUTO) */
        bz_gen_key(5);                              /* (MELODY) */
        ok = bz_g_auto && bz_g_mel;
        for (j = 0; j < 4u; j++) {
            for (b = 0; b < bar4 / CTL; b++) {
                uint32_t on0 = bzg_on;
                chord_block(CTL);
                notes += bzg_on && !on0;
                if (b % 2u == 0u)
                    bz_s16 = (j * bar4 + b * CTL) / div_samples(2);
            }
            sel[j] = bz_ch_sel;
            seen |= 1u << (bz_ch_sel & 15u);
            bz_s16 = (j + 1u) * 16u;
        }
        ok &= sel[0] && sel[0] != sel[1] && notes >= 4u;
        printf("    AUTO chords %u %u %u %u, melody notes %u\n", sel[0], sel[1], sel[2], sel[3], notes);
        bad += check("CHORD GENERATOR (0.12): AUTO a chord a bar from the progression; MELODY notes over it", ok);
        bz_gen_key(0);
        bz_gen_key(5);
        chord_block(CTL);
        ok = !bz_g_auto && !bz_g_mel;
        bad += check("  AUTO and MELODY again: off", ok);
        bz_chord = 0;
        song.playing = 0;
        (void)seen;
    }


    /* 0.14 READY ROLLS: key 2 SNARE 1/16 tapped: rolls on, 4 a beat; key 3 SNARE 1/32 held: takes over (one drum, one
     * rate), let go after a hold: stopped; key 2 again: rolls; ALL OFF: stopped. A key held + KNOB 2: its rate, kept */
    home_reset();
    {
        uint32_t beat = beat_samples(), n16, n32;
        RD_ROW[0] = 0;
        rd_cfg_apply();
        rl_ready = 1;
        ok = rd_lane[1] == 1u && rd_rate[1] == 4u && rd_lane[2] == 1u && rd_rate[2] == 6u && rd_lane[4] == 2u;
        rd_key(1, 1);
        rd_key(1, 0);                                /* (tapped: fm1_ms unchanged) */
        ok &= rd_on == 2u && rd_latch == 2u;
        run(beat / CTL, 0, 0);
        n16 = run(beat / CTL, 0, 0);
        rd_key(2, 1);
        ok &= rd_on == 4u && !rd_latch;
        run(beat / CTL, 0, 0);
        n32 = run(beat / CTL, 0, 0);
        fm1_ms += 400u;
        rd_key(2, 0);
        ok &= !rd_on;
        printf("    ready rolls: 1/16 %u a beat, 1/32 %u\n", n16, n32);
        ok &= n16 == 4u && n32 == 8u;
        rd_key(1, 1);
        rd_key(1, 0);
        bz_release_all();
        ok &= !rd_on && !rd_latch;
        bad += check("READY ROLLS (0.14): tapped rolls on, the same drum's next key takes over, held: while held, ALL OFF", ok);
        rd_key(1, 1);
        rd_key(1, 0);
        rd_key(1, 1);
        ok = !rd_on;
        rd_key(1, 0);
        fm1_in.notes = 1u << wk(0);
        ok &= rd_knob(1, 1) && rd_rate[0] == 3u && ((RD_ROW[1] >> 3) & 7u) == 3u && rd_knob(0, 1) && rd_lane[0] == 2u;
        fm1_in.notes = 0;
        ok &= !rd_knob(1, 1);
        bad += check("  tapped again: stopped; a key held + KNOB 1 / 2: its drum, its rate, kept", ok);
        RD_ROW[0] = 0;
        rd_cfg_apply();
        midi_event(0xB0u, 15u, 37u, 5u);           /* (RAIL: key 5 down, at once up: tapped) */
        midi_event(0xB0u, 15u, 37u, 69u);
        ok = rd_on == (1u << 4) && rd_latch == (1u << 4);
        midi_event(0xB0u, 15u, 37u, 5u);
        midi_event(0xB0u, 15u, 37u, 69u);
        ok &= !rd_on;
        bad += check("  RAIL's CC 37: 5 then 69 taps key 5 (CLAP 1/8) on; again: off", ok);
        RD_ROW[0] = 0;
        rd_cfg_apply();
        rl_ready = 0;
    }

    /* 0.14 LOOP CAPTURE kept and loaded: stopped with a loop held: SAVE THE LOOP?; YES saves (USR 2 here), then LOAD AS
     * SLICE PATTERN?; NO: LOAD INTO SAMPLER?; YES: T3 plays USR2 as SAMPLE. LOAD AS SLICE: 16 slices, one a step */
    ui_power_on();
    {
        int (*was)(void) = bz_cap_saver;
        track_t *t3 = &trk[2];
        uint32_t i;
        bz_cap_saver = fake_save;
        song.sel = 0;
        trk[0].eng_req = ENGI_DRUM;                    /* (the selected track DRUM: T3 is the one) */
        bz_cap = BZC_SOLO;
        song.playing = 1;
        bz_cap_pass();
        song.playing = 0;
        bz_cap_pass();
        ok = ui.confirm == CF_CAP_SAVE;
        press(B_OCTUP);
        ok &= ui.confirm == CF_CAP_SLICE && (ui.confirm_trk >> 4) == 2u && (ui.confirm_trk & 15u) == 2u;
        press(B_OCTDN);
        ok &= ui.confirm == CF_CAP_SAMPLE;
        press(B_OCTUP);
        ok &= !ui.confirm && t3->eng_req == ENGI_SAMPLE && t3->p[P_E0] == (int16_t)(SMP_NSETS + 1u);
        bz_cap_pass();
        ok &= !ui.confirm;                           /* (asked once a loop) */
        bad += check("CAPTURE (0.14): stopped: SAVE THE LOOP?; then SLICE PATTERN? NO: SAMPLER? YES: T3 plays USR2", ok);
        bz_cap_load_slice(1, 2);
        ok = t3->eng_req == 13u && t3->p[P_E0] == 1 && t3->p[P_E1] == 2 && t3->p[P_SLEN] == 16;
        for (i = 0; i < 16u; i++)
            ok &= t3->step[i].n == 1u && t3->step[i].time == ST_NOTE &&
                  t3->step[i].note[0] == (uint8_t)(60 + t3->p[P_ROOT] + (int32_t)i);
        bad += check("  LOAD AS SLICE PATTERN: T3 SLICE of USR1, 16 slices, a pattern of them in order", ok);
        bz_cap = BZC_IDLE;
        bz_cap_saver = was;
    }

    /* 0.13 hands-free rolls: a pad held + a rate tapped rolls on when let go; another rate tapped: the one rate; the
     * pad tapped: stopped (that press silent), the rate let go (the pad plays once again); ALL OFF stops them too */
    home_reset();
    ui.layer = LAYER_BLAZE;
    {
        uint32_t beat = beat_samples(), n8, n16, n1;
        key_down(wk(0));
        key_down(bk(1));                            /* (1/8) */
        (key_up)(bk(1));
        key_up(wk(0));
        ok = bz_padl == 1u && rl_latch == (1u << 2);
        n8 = run(beat / CTL, 0, 0);
        ok &= bz_fn_state(BZF_PAD0, wk(0)) == BZS_FILL;
        key_down(bk(3));                            /* (1/16, no pad held) */
        (key_up)(bk(3));
        run(beat / CTL, 0, 0);                      /* (onto its grid) */
        n16 = run(beat / CTL, 0, 0);
        ok &= rl_latch == (1u << 4) && (rl_runs == 1u << 4);
        n1 = vage;
        key_down(wk(0));                            /* (the pad: stops) */
        n1 = vage - n1 + run(4u, 0, 0);
        (key_up)(wk(0));
        printf("    hands-free: 1/8 %u a beat, 1/16 %u, the stop press %u\n", n8, n16, n1);
        ok &= !bz_padl && !rl_latch && !rl_runs && n1 == 0u;
        ok &= n8 == 2u && n16 == 4u;
        n1 = vage;
        key_down(wk(0));
        n1 = vage - n1 + run(beat / CTL, 0, 0);
        (key_up)(wk(0));
        ok &= n1 == 1u;
        bad += check("HANDS-FREE ROLL (0.13): pad + rate tapped rolls on; 1/16 tapped: 4 a beat; the pad: stops", ok);
        key_down(wk(1));
        key_down(bk(5));                            /* (1/32) */
        (key_up)(bk(5));
        key_up(wk(1));
        ok = bz_padl == 2u;
        key_down(bk(9));                            /* (ALL OFF) */
        (key_up)(bk(9));
        ok &= !bz_padl && !rl_runs && !rl_latch;
        bad += check("  ALL OFF: the pads rolling on their own stopped", ok);
    }

    /* 0.13 the RATE knob (knob 4 with a pad held or rolling): EVEN steps 1/8 1/16 1/32 a detent each, the pad rolls on;
     * SHIFT + a pad held: ALL, kept; rolling with no pad held: knob 4 RATE, not THROW */
    home_reset();
    ui.layer = LAYER_BLAZE;
    bz_cfg3[BZC3_MAGIC_I] = 0;
    bz_cfg_apply();
    {
        int8_t throw0;
        bz_rk_rate = 4;
        key_down(wk(0));
        bz_knob(3, 1);
        ok = bz_rk_rate == 6u && bz_rk_req == 7u;
        home_block(CTL, CTL);
        ok &= rl_latch == (1u << 6) && bz_padl == 1u;
        bz_knob(3, 1);
        ok &= bz_rk_rate == 6u;                     /* (the top: stays) */
        bz_knob(3, -1);
        bz_knob(3, -1);
        home_block(CTL, CTL);
        ok &= bz_rk_rate == 2u && rl_latch == (1u << 2);
        bad += check("RATE KNOB (0.13): a pad held: 1/16 -> 1/32 (top stays) -> 1/16 -> 1/8, latched", ok);
        fm1_in.buttons |= 1u << panel.btn[B_OCTUP];
        bz_knob(3, 1);
        ok = bz_rk_set == 1u && bz_cfg3[BZC3_RSET] == 1u;
        bz_knob(3, 1);
        ok &= bz_rk_set == 2u;
        bz_knob(3, -1);
        bz_knob(3, -1);
        fm1_in.buttons &= ~(1u << panel.btn[B_OCTUP]);
        ok &= bz_rk_set == 0u;
        bad += check("  SHIFT + a pad held: EVEN -> ALL -> TRIPLETS -> back, kept", ok);
        key_up(wk(0));
        throw0 = perf_k[2];
        ok = bz_padl == 1u;
        bz_knob(3, 1);
        home_block(CTL, CTL);
        ok &= perf_k[2] == throw0 && rl_latch == (1u << bz_rk_rate);
        bz_release_all();
        bz_knob(3, 1);
        ok &= perf_k[2] != throw0 && !bz_rk_req;
        bad += check("  rolling, no pad held: knob 4 RATE (THROW untouched); stopped: THROW again", ok);
        perf_k[2] = 0;
        bz_cfg3[BZC3_MAGIC_I] = 0;
        bz_cfg_apply();
    }

    /* 0.13 RAIL's ROLL RATE (CC 54): the first value noted; then across the steps (EVEN: 127 1/32); PHOSPHOR: CC 48 the
     * rate rolling (1/32: 7), CC 49 each hit's velocity */
    home_reset();
    ui.layer = LAYER_BLAZE;
    {
        uint32_t mo, cc48 = 0, cc49 = 0, m;
        bz_rk_rate = 4;
        midi_event(0xB0u, 15u, 54u, 10u);
        bz_ui_pass();
        ok = bz_rk_rate == 4u;
        midi_event(0xB0u, 15u, 54u, 127u);
        bz_ui_pass();
        ok &= bz_rk_rate == 6u && !bz_rk_req;       /* (no pad: only kept for the next) */
        key_down(wk(0));
        midi_event(0xB0u, 15u, 54u, 64u);
        bz_ui_pass();
        ok &= bz_rk_rate == 4u && bz_rk_req == 5u;
        key_up(wk(0));
        bad += check("RAIL ROLL RATE (0.13): CC 54 first noted, then across EVEN's steps; a pad held: picked", ok);
        bz_ev_on = 1;
        rl_ev_out = 1;
        usb.config = 1;
        bz_rk_req = 7u;                             /* (1/32) */
        key_down(wk(0));
        mo_r = mo_w;                                /* (the queue drained: nothing reads it here) */
        mo = mo_w;
        run(beat_samples() / CTL, 0, 0);
        for (i = mo; i != mo_w; i++) {
            m = midi_out_q[i % MQ];
            if ((m & 0xFFFFFFu) == (0x0Bu | 0xBFu << 8 | 48u << 16))
                cc48 = m >> 24;
            if ((m & 0xFFFFFFu) == (0x0Bu | 0xBFu << 8 | 49u << 16))
                cc49++;
        }
        key_up(wk(0));
        bz_release_all();
        printf("    PHOSPHOR: CC 48 %u, CC 49 x %u\n", cc48, cc49);
        ok = cc48 == 7u && cc49 >= 8u;
        bad += check("PHOSPHOR (0.13): CC 48 the rate rolling (1/32: 7), CC 49 a hit's velocity each hit", ok);
        rl_ev_out = 0;
    }

    /* 0.14.1 BOOST (SHIFT + black 6, RAIL's CC 36 12): SNARE and CLAP louder and harder, their roll hits at 127; the
     * kick as it was; again: off */
    home_reset();
    ui.layer = LAYER_BLAZE;
    {
        uint32_t f0 = bz_fn_state(BZF_BOOST, bk(5));
        fm1_in.buttons |= 1u << panel.btn[B_OCTUP];
        bz_ui_pass();
        key_down(bk(5));
        bz_ui_key(bk(5));
        (key_up)(bk(5));
        fm1_in.buttons &= ~(1u << panel.btn[B_OCTUP]);
        bz_ui_pass();
        ok = f0 == BZS_IDLE && dv_boost == 6u && bz_fn_state(BZF_BOOST, bk(5)) == BZS_FILL;
        midi_event(0xB0u, 15u, 36u, 12u);
        bz_ui_pass();
        ok &= dv_boost == 0u;
        midi_event(0xB0u, 15u, 36u, 12u);
        bz_ui_pass();
        ok &= dv_boost == 6u;
        bad += check("BOOST (0.14.1): SHIFT + black 6 on (SNARE CLAP), RAIL CC 36 12 off, on", ok);
        dv_boost = 0;
    }

    printf("%s\n", bad ? "BLAZE HOME: FAILED" : "BLAZE HOME: all ok");
    return bad != 0;
}
