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
    bad += check("  OCT- again: back to BLAZE", !bz_keys);
    btn_up(B_ENV);
    frame();

    printf("%s\n", bad ? "BLAZE HOME: FAILED" : "BLAZE HOME: all ok");
    return bad != 0;
}
