/* SPDX-License-Identifier: GPL-3.0-only
 * BLAZE (a DJ companion built on Felucca by Leo Kuroshita / Hügelton Instruments) */
/* BLAZE's home layer (firmware/src/blaze_home.c, blaze_home_ui.c, blaze_home_draw.c) on the real sequencer and UI
 * sources, with the stubs of tests/ui_test.c: ENV held opens it; the 4 pads are ROLL's PADS 4 in key order and roll
 * with the rates; an effect tapped latches, held plays while held; BUILD 4 runs 1/4 1/8 1/16 1/32 a bar each, louder,
 * from the next bar while playing, and stops; RISER; RELEASE; THE ONE; RAIL's key CCs set ROOT and SCALE; SHIFT's
 * knobs, a pad held edits that drum, ALGO the KIT; KEYS (OCT-): the white keys play the scale, the black ones the arp. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

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
    key_down(wk(BZ_FX0 + 4u));                         /* FREEZE */
    run(500, 0, 0);                                    /* (~750 ms: a hold) */
    ok = (perf_held & PF_BIT(PF_FRZ)) != 0u;
    key_up(wk(BZ_FX0 + 4u));
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
    key_down(bk(BZK_ONE)); key_up(bk(BZK_ONE));
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

    /* the knobs: COLOR; SHIFT (OCT+ held): TONE; a pad held: that drum's TUNE and LEVEL; ALGO: the KIT */
    ui_power_on();
    btn_down(B_ENV);
    frames(800);
    {
        track_t *d = &trk[3];
        int16_t tone = d->p[P_E0 + 2], kit = d->p[P_E0];
        perf_k[0] = 0;
        turn(EN_K1, 3);
        ok = perf_k[0] > 0 && d->p[P_E0 + 2] == tone;
        btn_down(B_OCTUP);
        frame();
        turn(EN_K1, 3);
        ok &= d->p[P_E0 + 2] > tone && !strcmp(bz_head(), "[ENV] BLAZE SHIFT");
        btn_up(B_OCTUP);
        frame();
        ok &= song.octave == 0;
        bad += check("knobs: COLOR; OCT+ held (SHIFT): TONE instead, no octave", ok);
        d->p[BZ_LANE_MARK] = 127;                       /* (a sound from before: no lane values) */
        key_down(wk(0));                                /* SNARE held */
        frame();
        turn(EN_K1, 4);
        turn(EN_K3, -4);
        ok = d->p[BZ_LANE_MARK] == BZ_LANE_MAGIC && d->p[BZ_LANE_TUNE + 1] > 64 && d->p[BZ_LANE_TUNE] == 64 &&
             d->p[P_LN0 + 1] < 127 && d->p[P_LN0] == 127 && !strcmp(bz_head(), "[ENV] BLAZE DRUM");
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
    press(B_OCTDN);
    ok = bz_keys && bz_ktrk == 0u && !strcmp(bz_head(), "[ENV] KEYS");
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
    ok = bz_keys && !bz_chord;
    press(B_OCTDN);
    ok &= !bz_keys && bz_chord && !strcmp(bz_head(), "[ENV] CHORD");
    press(B_OCTDN);
    ok &= !bz_keys && !bz_chord && bz_loops && !strcmp(bz_head(), "[ENV] LOOPS");
    press(B_OCTDN);
    ok &= !bz_keys && !bz_chord && !bz_loops;
    bad += check("UI: OCT- steps BLAZE -> KEYS -> CHORD -> LOOPS -> BLAZE", ok);
    bz_cfg[BZC_MAGIC_I] = 0;
    bz_cfg_apply();
    key_down(wk(BZ_RISER));
    frame();
    turn(EN_K1, 3);
    turn(EN_K3, -5);
    ok = !strcmp(bz_head(), "[ENV] RISER SOUND") && bz_cfg[BZC_MAGIC_I] == BZ_CFG_MAGIC && bz_rs_type != BZR_NOISE &&
         bz_rs_type == bz_cfg[BZC_TYPE] && bz_rs_lvl < 100u && lys.rp_dirty;
    key_up(wk(BZ_RISER));
    key_down(wk(BZ_RISER)); key_up(wk(BZ_RISER));       /* (a tap toggles it: stop the one the hold started) */
    bz_riser = 0;
    frame();
    bad += check("UI: RISER held: KNOB 1 its TYPE, KNOB 3 its LEVEL, kept in the settings", ok);
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

    printf("%s\n", bad ? "BLAZE HOME: FAILED" : "BLAZE HOME: all ok");
    return bad != 0;
}
