/* SPDX-License-Identifier: GPL-3.0-only
 * BLAZE (a DJ companion built on Felucca by Leo Kuroshita / Hügelton Instruments) */
/* ROLL, the sound side (the audio ISR). ARP held (or double-tapped: locked) makes the keys the ROLL layer
 * (ui_layer.c LAYER_ROLL; seq.c keyboard_block hands them here, kb_roll):
 *   white keys F3 .. G5 (places 0..15): drum pads, the DRUM lanes KICK SNARE CLAP HAT CL HAT OP TOM RIM BELL
 *     (eng_drum.c DRUM_LANE_NOTE) on places 0..7, the same lanes accented (velocity 127) on 8..15
 *   black keys 1..7 (places 0..6): the roll rates 1 (a bar) 1/4 1/8 8T 1/16 16T 1/32 (RL_DIV: fx.c div_samples)
 *   black keys 8..10: TAP, NUDGE -, NUDGE + (the main loop's: ui_layer.c rl_ui_key)
 *   black key 11: PADS. Tapped: 16 -> 4 -> 1 live pads -> 16 (the others dark and silent, so a fast hand never hits
 *     a neighbour). Held: the white keys pick the live pads of the mode (4: toggle, at most 4; 1: the one)
 * A pad with no rate held plays once, at once. A pad and a rate held: the pad rolls at that rate. 0.9: a rate key
 * tapped stays on (rl_latch) until tapped again: a pad then rolls at it with no rate key held. Every rate starts on
 * its own grid while the transport plays (a 1/8 roll on the next 1/8, a 1/4 or a bar's on the next beat: never more
 * than a beat away), at once while stopped. GROOVE (rl_groove 1): every rate held plays, their hits combined, a hit two
 * of them share accented; LAST (0): the newest rate held only, the one held before it takes over when it is let go.
 * BUILD (KNOB 2, rl_build 0..100): a roll starts that much softer and swells to full over a bar.
 * Time: the sequencer's (seq_n: the external clock's while it runs), so a roll stays locked to the clock; stopped, the
 * tempo now (BPM or the clock's last). Hits go to the drum track (the selected one if it is DRUM, else the first
 * DRUM track) and to MIDI OUT on its channel (PHOSPHOR, a computer). */
#define RL_NRATE 7u
static const uint8_t RL_DIV[RL_NRATE] = {7u, 0u, 1u, 4u, 2u, 5u, 3u};   /* 1/1 1/4 1/8 8T 1/16 16T 1/32 */
static const char *const RL_RATE_NAME[RL_NRATE] = {"1 BAR", "1/4", "1/8", "1/8 TRIP", "1/16", "1/16 TRIP", "1/32"};
static const char *const RL_RATE_CELL[RL_NRATE] = {"1 BAR", "1/4", "1/8", "1/8 T", "1/16", "1/16 T", "1/32"};
enum { RLK_TAP = RL_NRATE, RLK_NDN, RLK_NUP, RLK_PADS };   /* black places 7..10 */
enum { RL_PADS16, RL_PADS4, RL_PADS1, RL_NPADMODE };
#define RL_OFF_SAMPLES 441u                     /* a hit's note-off 10 ms after it (a one-shot lane plays on) */
#define RL_ALL_PADS 0xFFFFu

static volatile uint32_t roll_mask;            /* main: the ARP button's bit while ROLL may own the keys (ui_layer.c) */
static volatile uint8_t rl_groove = 1;         /* main / ISR: 1 GROOVE, 0 LAST */
static volatile uint8_t rl_padmode;            /* ISR (PADS), the main loop reads: RL_PADS16 / 4 / 1 */
static volatile uint16_t rl_live4 = 0x001Eu;   /* the 4 mode's live pads: SNARE CLAP HAT CL HAT OP */
static volatile uint16_t rl_live1 = 0x0002u;   /* the 1 mode's: SNARE */
static volatile uint8_t rl_build;              /* main: KNOB 2, 0..100 */
static volatile uint16_t rl_held;              /* ISR: the pads held (playing) */
static volatile uint16_t rl_extra;             /* ISR: pads the BLAZE layer holds (blaze_home.c: live whatever PADS is) */
static volatile uint8_t rl_rates;              /* ISR: the rates held (bit r) */
static volatile uint16_t rl_flash;             /* ISR sets, the main loop clears: pads hit since it looked */
static volatile uint8_t rl_rolling;            /* ISR: a roll sounds (the main loop's LEDs, screen) */
static uint8_t rl_vel_last;                    /* ISR: the last hit's velocity (the tests) */
static uint8_t rl_pick, rl_picked;             /* ISR: PADS held; a pad picked while it was */
static uint8_t rl_ord[RL_NRATE], rl_nr;        /* ISR: the rates held, in the order pressed */
static uint8_t rl_runs;                        /* ISR: the rates running now (bit r) */
static uint32_t rl_next[RL_NRATE];             /* ISR: samples until that rate's next hit */
static uint32_t rl_age;                        /* ISR: samples since the roll began (BUILD) */
static uint8_t rl_vel[16];                     /* ISR: each pad's velocity as pressed */
static uint32_t rl_offt[NLANE];                /* ISR: samples until that lane's note-off (0: none) */
static uint8_t rl_offn[NLANE], rl_offc[NLANE];   /* .. its note and MIDI channel */
/* 0.9: a rate key TAPPED stays on (rl_latch: a pad pressed then rolls at it, no rate held); tapped again: off. HELD it
 * plays while held, as before (a latched one held stays latched). */
#define RL_TAP_MS 250u
static volatile uint8_t rl_latch;              /* ISR (the main loop reads): the rates tapped on */
static uint8_t rl_wasl;                        /* ISR: the rates latched when their key went down */
static uint32_t rl_rdown[RL_NRATE];            /* ISR: when (fm1_ms) */
/* 0.9: PAD PRESETS (BLAZE's pads): a lane can carry its own rate (rolls at it while its pad is held, whatever rates are
 * on) and an effect (on while the pad is held): blaze_home.c bz_pp_key, set with a pad + SHIFT + a rate / effect key */
static volatile uint8_t rl_pp_rate[NLANE];     /* main / ISR: the lane's rate + 1 (0: none) */
static volatile uint8_t rl_pp_fx[NLANE];       /* main / ISR: the lane's effect (BLAZE's BZ_FX index) + 1 (0: none) */
static volatile uint16_t rl_pp;                /* ISR: pads rolling at their lane's own rate */
static volatile uint8_t rl_ev_out;             /* main: 0.13 BLAZE opened: each hit's velocity to PHOSPHOR (ch16 CC 49) */
/* 0.11 ROLL SHAPE (SHIFT + black 4 in BLAZE): the velocities along a roll. FLAT as played; ACCENT 2 / 4 every 2nd /
 * 4th hit full, the rest at half; FADE down to 30 % over its first bar (then on at that) */
enum { RLS_FLAT, RLS_ACC2, RLS_ACC4, RLS_FADE, RLS_N };
static volatile uint8_t rl_shape;              /* main: RLS_* */
static uint32_t rl_hitn;                       /* ISR: the roll's hits so far (ACCENT) */
static uint32_t rl_ppn[16];                    /* ISR: samples to each one's next hit */
/* 0.14 READY ROLLS: ROLL's second page (OCT- in ROLL): 16 white keys, each a finished roll, a drum at its own rate
 * (the main loop keeps them: blaze_roll_ui.c rd_*). Tapped: it rolls on (a dot) until tapped again; held: while held.
 * One drum rolls at one rate: another key of the same drum takes over (SNARE 1/8 -> SNARE 1/16, nothing switched).
 * Hits on the rate's own grid (as a rate key's), the velocity played (ACCENT: full). ALL OFF (BLAZE) stops them */
static volatile uint8_t rl_ready;              /* main: the page shown (the white keys are READY's) */
static volatile uint8_t rd_lane[16], rd_rate[16], rd_acc[16];   /* main: each key's drum, rate (1 1/4 .. 6 1/32), ACCENT */
static volatile uint16_t rd_on;                /* ISR: the keys rolling (the main loop reads) */
static volatile uint16_t rd_latch;             /* ISR: .. tapped on (rolls on when let go) */
static uint16_t rd_x;                          /* ISR: keys whose press stopped them (their release: nothing) */
static uint8_t rd_vel[16];                     /* ISR: each one's velocity */
static uint32_t rd_n[16], rd_dn[16];           /* ISR: samples to its next hit; when (fm1_ms) it was pressed */

/* 0.17 PLAY's rolls (blaze_home.c play_key): black 1..3 roll one drum (pl_lane) at 1/8, 1/16, 1/32 while held, or on
 * once HOLD armed them (pl_latch); one at a time (another rate takes over). Hits on the rate's grid, as READY's */
static void rl_hit(uint32_t p, uint32_t vel);
static uint32_t rl_wait(uint32_t r);
static const uint8_t PL_RATE[3] = {2, 4, 6};    /* (RL_DIV's index: 1/8, 1/16, 1/32) */
static volatile uint8_t pl_lane = 1;           /* main / ISR: the drum (SNARE CLAP HAT OPEN TOM RIM: 1..6) */
static volatile uint8_t pl_on, pl_latch;       /* ISR: the rate rolling (bit 0..2), latched */
static uint8_t pl_vel;
static uint32_t pl_n;                          /* ISR: samples to its next hit */
static void pl_roll(uint32_t r, int down, int latch)
{
    uint8_t b = (uint8_t)(1u << r);
    if (!down) {
        if (!(pl_latch & b))
            pl_on = (uint8_t)(pl_on & ~b);
        return;
    }
    if (pl_on & b) {                             /* (rolling: this press stops it) */
        pl_on = pl_latch = 0;
        return;
    }
    pl_on = b;                                   /* (one rate at a time) */
    pl_latch = latch ? b : 0u;
    pl_vel = (uint8_t)kb_vel;
    pl_n = rl_wait(PL_RATE[r]);
    if (!pl_n) {
        rl_hit(pl_lane % NLANE, pl_vel);
        pl_n = div_samples(RL_DIV[PL_RATE[r]]);
    }
}

static uint16_t rl_live(void)
{
    return rl_padmode == RL_PADS4 ? rl_live4 : rl_padmode == RL_PADS1 ? rl_live1 : (uint16_t)RL_ALL_PADS;
}
static uint32_t rl_count(uint32_t m)
{
    uint32_t n = 0;
    for (; m; m &= m - 1u)
        n++;
    return n;
}

/* the track the pads play: the selected one if it is a DRUM track, else the first DRUM track, else the selected */
static track_t *rl_track(void)
{
    uint32_t i;
    if (drum_track(TSEL))
        return TSEL;
    for (i = 0; i < NTRK; i++)
        if (drum_track(&trk[i]))
            return &trk[i];
    return TSEL;
}

static void rl_note_off(uint32_t l)
{
    track_t *t = rl_track();
    trk_note_off(t, rl_offn[l]);
    midi_out_event(0x08u | (0x80u | rl_offc[l]) << 8 | (uint32_t)rl_offn[l] << 16);
    rl_offt[l] = 0;
}

/* pad p sounds now at velocity vel (the accented row: 127) */
static void rl_hit(uint32_t p, uint32_t vel)
{
    track_t *t = rl_track();
    uint32_t l = p % NLANE, note = DRUM_LANE_NOTE[l], mc = trk_midi_ch(trk_index(t));
    if (p >= NLANE || ((dv_boost >> l) & 1u))   /* (accented; 0.14.1 BOOST's lanes: full velocity) */
        vel = 127u;
    vel = vel < 1u ? 1u : vel > 127u ? 127u : vel;
    rl_vel_last = (uint8_t)vel;
    if (rl_offt[l])
        rl_note_off(l);                          /* (the lane's last hit ends first) */
    trk_note_on(t, note, vel);
    midi_out_event(0x09u | (0x90u | mc) << 8 | note << 16 | vel << 24);
    rl_offt[l] = RL_OFF_SAMPLES;
    rl_offn[l] = (uint8_t)note;
    rl_offc[l] = (uint8_t)mc;
    rl_flash |= (uint16_t)(1u << p);
    if (rl_ev_out)                               /* 0.13: PHOSPHOR's hit (its velocity) */
        midi_out_event(0x0Bu | 0xBFu << 8 | 49u << 16 | vel << 24);
}

/* samples until rate r's first hit: its own grid while the transport plays (at most a beat away), at once stopped */
static uint32_t rl_wait(uint32_t r)
{
    uint32_t s16, q, period, g, pos;
    if (!song.playing || clk_pos == CLK_START)
        return 0;
    s16 = div_samples(2);
    q = 4u * s16;                                /* (click_beat_len: the beat as the 1/16 steps count it) */
    period = div_samples(RL_DIV[r]);
    g = period < q ? period : q;
    pos = (clk_step % 4u) * s16 + clk_pos;       /* samples into the beat */
    if (!g)
        return 0;
    pos %= g;
    return pos ? g - pos : 0u;
}

/* the rates that run now: every one held (GROOVE), or the newest (LAST); one that starts running is put on its grid */
static void rl_rerun(void)
{
    uint32_t want = 0, r, i;
    if ((rl_held & rl_live()) | rl_extra) {
        if (rl_groove)
            for (i = 0; i < rl_nr; i++)
                want |= 1u << rl_ord[i];
        else if (rl_nr)
            want = 1u << rl_ord[rl_nr - 1u];
    }
    if (want && !rl_runs) {
        rl_age = 0;                              /* (BUILD: a roll begins) */
        rl_hitn = 0;                             /* (0.11 SHAPE: from its first hit) */
    }
    for (r = 0; r < RL_NRATE; r++)
        if (((want >> r) & 1u) && !((rl_runs >> r) & 1u))
            rl_next[r] = rl_wait(r);
    rl_runs = (uint8_t)want;
    rl_rolling = (uint8_t)(want != 0u);
}

static void rl_rate(uint32_t r, int down)
{
    uint32_t i, k = 0, b = 1u << r;
    if (down) {
        rl_rdown[r] = fm1_ms;
        rl_wasl = (uint8_t)((rl_wasl & ~b) | (rl_latch & b));
    } else {                                     /* 0.9: a tap toggles it on / off, a hold was only while held */
        int tap = fm1_ms - rl_rdown[r] < RL_TAP_MS;
        if (tap ? !(rl_wasl & b) : (rl_wasl & b) != 0u) {
            rl_latch |= (uint8_t)b;
            return;                              /* (stays on: in the order, its bit) */
        }
        rl_latch &= (uint8_t)~b;
    }
    for (i = 0; i < rl_nr; i++)                  /* (out of the order, then on its end if pressed) */
        if (rl_ord[i] != r)
            rl_ord[k++] = rl_ord[i];
    rl_nr = (uint8_t)k;
    if (down && rl_nr < RL_NRATE)
        rl_ord[rl_nr++] = (uint8_t)r;
    rl_rates = (uint8_t)(down ? rl_rates | 1u << r : rl_rates & ~(1u << r));
    rl_rerun();
}

/* 0.13 BLAZE's RATE knob: rate r the one latched (the others latched let go; a rate key held stays), on its grid */
static void rl_knob_rate(uint32_t r)
{
    uint32_t i, k = 0, q;
    for (i = 0; i < rl_nr; i++) {
        q = rl_ord[i];
        if (q == r || ((rl_latch >> q) & 1u)) {
            if (q != r)
                rl_rates = (uint8_t)(rl_rates & ~(1u << q));
            continue;
        }
        rl_ord[k++] = (uint8_t)q;
    }
    rl_ord[k++] = (uint8_t)r;
    rl_nr = (uint8_t)k;
    rl_latch = (uint8_t)(1u << r);
    rl_wasl = (uint8_t)(rl_wasl & (1u << r));
    rl_rates = (uint8_t)(rl_rates | 1u << r);
    rl_rerun();
}

/* 0.9: every tapped rate off (BLAZE's ALL OFF) */
static void rl_unlatch(void)
{
    uint32_t i, k = 0;
    for (i = 0; i < rl_nr; i++)
        if (!((rl_latch >> rl_ord[i]) & 1u))
            rl_ord[k++] = rl_ord[i];
    rl_nr = (uint8_t)k;
    rl_rates = (uint8_t)(rl_rates & ~rl_latch);
    rl_latch = rl_wasl = 0;
    rl_rerun();
}

/* 0.9: pad p (its lane's own rate r + 1) down / up: it rolls at it on its grid while held, apart from the rest */
static void rl_pp_key(uint32_t p, uint32_t r1, int down)
{
    uint16_t b = (uint16_t)(1u << p);
    if (!down) {
        rl_pp = (uint16_t)(rl_pp & ~b);
        return;
    }
    rl_ppn[p] = rl_wait(r1 - 1u);
    if (!rl_ppn[p]) {
        rl_hit(p, rl_vel[p]);                    /* (on the grid now, or stopped: a hit at once, then the rate) */
        rl_ppn[p] = div_samples(RL_DIV[r1 - 1u]);
    }
    rl_pp = (uint16_t)(rl_pp | b);
}

/* 0.14 READY ROLLS: white key p down / up */
static void rd_key(uint32_t p, int down)
{
    uint16_t b = (uint16_t)(1u << p);
    uint32_t q, r1 = rd_rate[p] % 7u ? rd_rate[p] % 7u : 4u;
    if (!down) {
        if (rd_x & b) {                          /* (its press stopped it) */
            rd_x = (uint16_t)(rd_x & ~b);
            return;
        }
        if (fm1_ms - rd_dn[p] < RL_TAP_MS)       /* tapped: rolls on */
            rd_latch = (uint16_t)(rd_latch | b);
        else                                     /* held: only while held */
            rd_on = (uint16_t)(rd_on & ~b);
        return;
    }
    if (rd_on & b) {                             /* rolling: this press stops it */
        rd_on = (uint16_t)(rd_on & ~b);
        rd_latch = (uint16_t)(rd_latch & ~b);
        rd_x = (uint16_t)(rd_x | b);
        return;
    }
    for (q = 0; q < 16u; q++)                    /* (one drum, one rate: the same drum's other key lets go) */
        if (q != p && ((rd_on >> q) & 1u) && rd_lane[q] % NLANE == rd_lane[p] % NLANE) {
            rd_on = (uint16_t)(rd_on & ~(1u << q));
            rd_latch = (uint16_t)(rd_latch & ~(1u << q));
        }
    rd_vel[p] = (uint8_t)(rd_acc[p] ? 127u : kb_vel);
    rd_dn[p] = fm1_ms;
    rd_latch = (uint16_t)(rd_latch & ~b);
    rd_n[p] = rl_wait(r1);                       /* (r1: ROLL's rate, 1 1/4 .. 6 1/32, RL_DIV's index) */
    if (!rd_n[p]) {
        rl_hit(rd_lane[p] % NLANE + (rd_acc[p] ? NLANE : 0u), rd_vel[p]);
        rd_n[p] = div_samples(RL_DIV[r1]);
    }
    rd_on = (uint16_t)(rd_on | b);
}

/* PADS held: white place p picked for the mode (4: on / off, at most 4; 1: the one) */
static void rl_pick_pad(uint32_t p)
{
    uint16_t b = (uint16_t)(1u << p);
    rl_picked = 1;
    if (rl_padmode == RL_PADS1)
        rl_live1 = b;
    else if (rl_padmode == RL_PADS4) {
        if (rl_live4 & b)
            rl_live4 = (uint16_t)(rl_live4 & ~b);
        else if (rl_count(rl_live4) < 4u)
            rl_live4 = (uint16_t)(rl_live4 | b);
    }
}

/* a key of the ROLL layer down / up (k: 0 = F3 .. 26 = G5). noinline: out of keyboard_block's loop */
static __attribute__((noinline)) void roll_key_press(uint32_t k, int down)
{
    uint32_t p = key_place(k);
    if (key_black(k)) {
        if (p < RL_NRATE)
            rl_rate(p, down);
        else if (p == RLK_PADS && down) {
            rl_pick = 1;
            rl_picked = 0;
        } else if (p == RLK_PADS) {
            if (!rl_picked)                      /* tapped: the next mode */
                rl_padmode = (uint8_t)((rl_padmode + 1u) % RL_NPADMODE);
            rl_pick = 0;
            rl_held = (uint16_t)(rl_held & rl_live());   /* (a pad locked now no longer plays) */
            rl_rerun();
        }
        return;                                  /* (TAP, NUDGE: the main loop's) */
    }
    if (p >= 16u)
        return;
    if (rl_ready) {                              /* 0.14 READY ROLLS: the white keys are its rolls */
        rd_key(p, down);
        return;
    }
    if (rl_pick) {
        if (down)
            rl_pick_pad(p);
        return;
    }
    if (!down) {
        rl_held = (uint16_t)(rl_held & ~(1u << p));
        rl_rerun();
        return;
    }
    if (!((rl_live() >> p) & 1u))
        return;                                  /* a locked pad: dark and silent */
    rl_vel[p] = (uint8_t)kb_vel;
    rl_held = (uint16_t)(rl_held | 1u << p);
    if (!rl_nr)
        rl_hit(p, rl_vel[p]);                    /* no rate held: once, now */
    else
        rl_rerun();
}

/* every block: n samples of real time (the note-offs), adv of the sequencer's (the rates) */
static __attribute__((noinline)) void roll_block(uint32_t n, uint32_t adv)   /* (out of events_block: target_budget.py) */
{
    uint32_t l, r, hits = 0, both = 0, p, held;
    for (l = 0; l < NLANE; l++)
        if (rl_offt[l]) {
            if (rl_offt[l] <= n)
                rl_note_off(l);
            else
                rl_offt[l] -= n;
        }
    if (rl_pp)                                   /* 0.9: pads at their own rate */
        for (p = 0; p < 16u; p++)
            if ((rl_pp >> p) & 1u) {
                uint32_t r1 = rl_pp_rate[p % NLANE], period = r1 ? div_samples(RL_DIV[r1 - 1u]) : 0u;
                if (!period) {
                    rl_pp = (uint16_t)(rl_pp & ~(1u << p));   /* (its preset gone meanwhile) */
                    continue;
                }
                if (rl_ppn[p] < adv) {
                    rl_hit(p, rl_vel[p]);
                    while (rl_ppn[p] < adv)
                        rl_ppn[p] += period;
                }
                rl_ppn[p] -= adv;
            }
    if (pl_on) {                                 /* 0.17 PLAY's roll */
        uint32_t r = pl_on & 1u ? 0u : pl_on & 2u ? 1u : 2u, period = div_samples(RL_DIV[PL_RATE[r]]);
        if (!period)
            period = 1;
        if (pl_n < adv) {
            rl_hit(pl_lane % NLANE, pl_vel);
            while (pl_n < adv)
                pl_n += period;
        }
        pl_n -= adv;
    }
    if (rd_on)                                   /* 0.14 READY ROLLS: each at its own rate */
        for (p = 0; p < 16u; p++)
            if ((rd_on >> p) & 1u) {
                uint32_t r1 = rd_rate[p] % 7u ? rd_rate[p] % 7u : 4u, period = div_samples(RL_DIV[r1]);
                if (!period)
                    period = 1;
                if (rd_n[p] < adv) {
                    rl_hit(rd_lane[p] % NLANE + (rd_acc[p] ? NLANE : 0u), rd_vel[p]);
                    while (rd_n[p] < adv)
                        rd_n[p] += period;
                }
                rd_n[p] -= adv;
            }
    if (!rl_runs)
        return;
    for (r = 0; r < RL_NRATE; r++) {
        uint32_t period;
        if (!((rl_runs >> r) & 1u))
            continue;
        period = div_samples(RL_DIV[r]);
        if (!period)
            period = 1;
        if (rl_next[r] < adv) {                  /* due in this block (once: a period is longer than a block) */
            if (hits)
                both = 1;                        /* (a hit another rate makes too: accented) */
            hits = 1;
            while (rl_next[r] < adv)
                rl_next[r] += period;
        }
        rl_next[r] -= adv;
    }
    rl_age += adv;
    if (!hits)
        return;
    held = (rl_held & rl_live()) | rl_extra;
    {
        uint32_t bar = 4u * beat_samples(), age = rl_age < bar ? rl_age : bar;
        uint32_t swell = (100u - rl_build) + rl_build * age / (bar ? bar : 1u);   /* 100 - BUILD .. 100 % */
        uint32_t sh = rl_shape;
        if (sh == RLS_ACC2 || sh == RLS_ACC4)    /* (0.11 SHAPE: the hits between the accents at half) */
            swell = (rl_hitn & (sh == RLS_ACC2 ? 1u : 3u)) ? swell / 2u : swell;
        else if (sh == RLS_FADE)
            swell = swell * (100u - 70u * age / (bar ? bar : 1u)) / 100u;
        rl_hitn++;
        for (p = 0; p < 16u; p++)
            if ((held >> p) & 1u) {
                uint32_t v = rl_vel[p] * swell / 100u;
                rl_hit(p, both ? v + 24u : v);
            }
    }
}
