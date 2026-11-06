/* SPDX-License-Identifier: GPL-3.0-only
 * BLAZE (a DJ companion built on Felucca by Leo Kuroshita / Hügelton Instruments) */
/* ROLL (firmware/src/blaze_roll.c, blaze_roll_ui.c, blaze_roll_draw.c) on the real sequencer and UI sources, with the
 * stubs of tests/ui_test.c: a pad plays once; pad + rate rolls at the rate; stopped it starts at once, playing on its
 * own grid (at most a beat away); GROOVE combines the rates held, LAST plays the newest; the 16 / 4 / 1 lockout; PADS
 * tapped and held; ARP held opens the layer, double-tapped locks it; NUDGE; OCT+ GROOVE / LAST; the map draws. */
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

static uint32_t wk(uint32_t place)                 /* the key of white place p */
{
    uint32_t k;
    for (k = 0; k < 27u; k++)
        if (!key_black(k) && key_place(k) == place)
            return k;
    return 0;
}
static uint32_t bk(uint32_t place)                 /* .. of black place p */
{
    uint32_t k;
    for (k = 0; k < 27u; k++)
        if (key_black(k) && key_place(k) == place)
            return k;
    return 0;
}

static void roll_reset(void)
{
    ui_power_on();
    rl_groove = 1;
    rl_padmode = RL_PADS16;
    rl_live4 = 0x001Eu;
    rl_live1 = 0x0002u;
    rl_build = 0;
    rl_held = 0;
    rl_rates = 0;
    rl_nr = 0;
    rl_runs = 0;
    rl_rolling = 0;
    rl_pick = rl_picked = 0;
    rl_flash = 0;
    kb_roll = 0;
    memset(rl_offt, 0, sizeof rl_offt);
    memset(&rlu, 0, sizeof rlu);
    rlu.groove = 1;
    song.g[G_BPM] = 120;
    roll_mask = 1u << panel.btn[B_ARP];             /* ARP held: the keys are ROLL's */
    fm1_in.buttons |= roll_mask;
}

/* hits (voice starts) over `blocks` blocks of CTL samples; at[]: the sample each began in */
static uint32_t run(uint32_t blocks, uint32_t *at, uint32_t nat)
{
    uint32_t b, n = 0, v0;
    for (b = 0; b < blocks; b++) {
        v0 = vage;
        roll_block(CTL, CTL);
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
    uint32_t at[16], n, v0, q, s16, i;

    /* a pad alone: once, at once, on the DRUM track (track 4 by default) */
    roll_reset();
    v0 = vage;
    key_down(wk(1));
    ok = vage == v0 + 1u && rl_track() == &trk[3] && drum_track(rl_track());
    key_up(wk(1));
    n = run(200, 0, 0);
    bad += check("a pad alone plays once, at once, on the DRUM track", ok && n == 0u);
    key_down(wk(9));
    bad += check("  the accented row (places 8..15) plays the same lane", vage == v0 + 2u);
    key_up(wk(9));

    /* stopped: pad + 1/16 rolls at once, every 1/16 (120 BPM: 5512 samples) */
    roll_reset();
    q = beat_samples();
    s16 = div_samples(2);
    key_down(bk(4));                                   /* 1/16 */
    key_down(wk(1));                                   /* SNARE */
    n = run(q / CTL, at, 16);
    ok = n == 4u && near(at[0], 0) && near(at[1], s16) && near(at[2], 2u * s16) && near(at[3], 3u * s16);
    bad += check("stopped: pad + 1/16 rolls at once, 4 hits a beat, a 1/16 apart", ok);
    key_up(wk(1));
    n = run(q / CTL, 0, 0);
    bad += check("  the pad let go: the roll stops", n == 0u && !rl_rolling);
    key_down(wk(1));
    n = run(q / CTL, 0, 0);
    bad += check("  the rate still held, the pad again: it rolls again", n == 4u);
    key_up(wk(1));
    key_up(bk(4));

    /* each rate's spacing */
    {
        static const uint8_t PER_BEAT_X3[RL_NRATE] = {0, 3, 6, 9, 12, 18, 24};   /* hits in 3 beats (BAR: below) */
        ok = 1;
        for (i = 1; i < RL_NRATE; i++) {
            roll_reset();
            key_down(bk(i));
            key_down(wk(1));
            n = run(3u * beat_samples() / CTL, 0, 0);
            ok &= n == PER_BEAT_X3[i];
            key_up(wk(1));
            key_up(bk(i));
        }
        roll_reset();
        key_down(bk(0));
        key_down(wk(1));
        n = run(8u * beat_samples() / CTL - 1u, 0, 0);   /* BAR: 2 hits in 2 bars (at 0 and a bar on) */
        ok &= n == 2u;
        bad += check("every rate: 1/4 1/8 8T 1/16 16T 1/32 hit 3 6 9 12 18 24 times in 3 beats, BAR twice in 2 bars", ok);
    }

    /* playing: a rate starts on its own grid; a bar's or a 1/4 on the next beat */
    roll_reset();
    seq_start();
    song.playing = 1;
    click_tick(0);                                     /* (the bar's first 1/16) */
    click_tick(3000);                                  /* 3000 samples into it */
    s16 = div_samples(2);
    key_down(bk(2));                                   /* 1/8 */
    key_down(wk(1));
    n = run(div_samples(1) / CTL + 2u, at, 16);
    ok = n >= 1u && near(at[0], div_samples(1) - 3000u);
    bad += check("playing: a 1/8 roll's first hit on the next 1/8, not at once", ok);
    key_up(wk(1));
    key_up(bk(2));
    roll_reset();
    seq_start();
    song.playing = 1;
    click_tick(0);
    click_tick(3000);
    key_down(bk(0));                                   /* BAR */
    key_down(wk(1));
    n = run(beat_samples() / CTL + 2u, at, 16);
    ok = n == 1u && near(at[0], 4u * s16 - 3000u);
    bad += check("  a bar's roll starts on the next beat (never more than a beat away)", ok);
    key_up(wk(1));
    key_up(bk(0));

    /* GROOVE: the rates held combine; LAST: the newest only, the one before back when it is let go */
    roll_reset();
    key_down(bk(2));                                   /* 1/8 */
    key_down(bk(4));                                   /* + 1/16 */
    key_down(wk(1));
    n = run(beat_samples() / CTL, 0, 0);
    bad += check("GROOVE: 1/8 + 1/16 held: their hits combined (4 a beat, the shared ones once)", n == 4u);
    key_up(wk(1));
    key_up(bk(4));
    key_up(bk(2));
    roll_reset();
    rl_groove = 0;
    key_down(bk(4));                                   /* 1/16 */
    key_down(bk(2));                                   /* then 1/8: newest */
    key_down(wk(1));
    n = run(beat_samples() / CTL, 0, 0);
    ok = n == 2u;
    key_up(bk(2));                                     /* 1/8 let go: 1/16 again */
    n = run(beat_samples() / CTL, 0, 0);
    ok &= n == 4u;
    bad += check("LAST: the newest rate plays (1/8: 2 a beat); let go, the one held before (1/16: 4)", ok);
    key_up(wk(1));
    key_up(bk(4));

    /* the lockout: 1 pad live, the rest silent; PADS tapped cycles 16 -> 4 -> 1 -> 16; held, the white keys pick */
    roll_reset();
    rl_padmode = RL_PADS1;
    v0 = vage;
    key_down(wk(0));                                   /* KICK: locked */
    key_up(wk(0));
    ok = vage == v0;
    key_down(wk(1));                                   /* SNARE: the live one */
    key_up(wk(1));
    ok &= vage == v0 + 1u;
    bad += check("PADS 1: only the live pad sounds, a locked one is silent", ok);
    roll_reset();
    key_down(bk(RLK_PADS)); key_up(bk(RLK_PADS));
    ok = rl_padmode == RL_PADS4;
    key_down(bk(RLK_PADS)); key_up(bk(RLK_PADS));
    ok &= rl_padmode == RL_PADS1;
    key_down(bk(RLK_PADS)); key_up(bk(RLK_PADS));
    ok &= rl_padmode == RL_PADS16;
    bad += check("PADS tapped: 16 -> 4 -> 1 -> 16", ok);
    roll_reset();
    rl_padmode = RL_PADS1;
    v0 = vage;
    key_down(bk(RLK_PADS));
    key_down(wk(6)); key_up(wk(6));                    /* RIM picked */
    key_up(bk(RLK_PADS));
    ok = rl_live1 == (1u << 6) && rl_padmode == RL_PADS1 && vage == v0;   /* (picked, not played; the mode stays) */
    rl_padmode = RL_PADS4;
    key_down(bk(RLK_PADS));
    key_down(wk(1)); key_up(wk(1));                    /* SNARE out of the four */
    key_down(wk(7)); key_up(wk(7));                    /* BELL in */
    key_down(wk(8)); key_up(wk(8));                    /* a fifth: refused */
    key_up(bk(RLK_PADS));
    ok &= rl_live4 == (0x001Cu | 1u << 7);
    bad += check("PADS held: the white keys pick the live pads (1: the one; 4: on / off, at most 4), nothing plays", ok);

    /* the UI: ARP held opens the map; a tap still opens ARP's page; a double tap locks ROLL */
    ui_power_on();
    btn_down(B_ARP);
    frames(800);
    ok = ui.layer == LAYER_ROLL && (roll_mask & (1u << panel.btn[B_ARP]));
    btn_up(B_ARP);
    frame();
    ok &= ui.layer == 0;
    bad += check("UI: ARP held opens ROLL's map; let go, it closes", ok);
    ui_power_on();
    press(B_ARP);
    ok = !ui.home && cur_page()->fam == FAM_ARP;
    press(B_ARP);
    frame();                                           /* (the ISR's masks: the next pass) */
    ok &= ui.lock == LAYER_ROLL && (kb_lock & 4u) && ui.home;   /* (the double tap: the page it left, HOME, under it) */
    bad += check("UI: ARP tapped opens ARP's page; tapped again soon, ROLL locks open", ok);
    press(B_ARP);
    frame();
    bad += check("  ARP tapped again: the lock closes", !ui.lock && !(kb_lock & 4u));

    /* NUDGE (INT clock): faster while held, back when let go; OCT+ in ROLL: GROOVE / LAST, the octave stays */
    roll_reset();
    btn_down(B_ARP);
    frames(800);
    song.g[G_BPM] = 128;
    key_down(bk(RLK_NUP));
    frame();
    ok = song.g[G_BPM] == 132;                         /* (128 / 32 = 4) */
    key_up(bk(RLK_NUP));
    frame();
    ok &= song.g[G_BPM] == 128;
    bad += check("NUDGE +: 1/32 faster while held, the tempo back when let go", ok);
    press(B_OCTUP);
    ok = rl_groove == 0 && song.octave == 0;
    press(B_OCTUP);
    ok &= rl_groove == 1 && song.octave == 0;
    bad += check("OCT+ in ROLL: GROOVE <-> LAST, the octave untouched", ok);
    turn(EN_K2, 5);
    frame();
    bad += check("KNOB 2 in ROLL: BUILD", rl_build > 0u);
    btn_up(B_ARP);
    frame();

    if (bad)
        printf("blaze roll: %d FAILED\n", bad);
    else
        printf("blaze roll test passed\n");
    return bad != 0;
}
