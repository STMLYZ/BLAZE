/* SPDX-License-Identifier: GPL-3.0-only
 * BLAZE (a DJ companion built on Felucca by Leo Kuroshita / Hügelton Instruments) */
/* BLAZE, the home layer's sound side (the audio ISR). ENV held (or double-tapped: locked) makes the keys BLAZE's
 * (ui_layer.c LAYER_BLAZE; seq.c keyboard_block hands them here, kb_bz):
 *   white 1..4   the 4 pads ROLL's PADS 4 picked (rl_live4, in key order), rolled by black 1..6 as in ROLL
 *   white 5..10  ECHO, REVERB (the throws), REVERSE, BRAKE (tape stop), FREEZE, STUTTER (a 1/16 repeat): a tap
 *                latches it on (a tap again: off), a hold plays it while held (BZ_TAP_MS tells them apart)
 *   white 11, 12 BUILD 4, BUILD 8: the one pad (ROLL's PADS 1: rl_live1, SNARE) from the next bar, 1/4 -> 1/8 ->
 *                1/16 -> 1/32 over 4 or 8 bars, swelling, so it ends on the next phrase's 1. Again: stops it
 *   white 13     RISER: swept up and swelling from the next bar over 8 bars (tapped twice quickly: 16), cut on the 1
 *                after it. Again: stops it. Its sound (held + knobs, blaze_home_ui.c): TYPE NOISE AIR SIREN TONE
 *                (bz_riser_mix, fx.c mix_block: on the master, no voice) or SYNTH (a track's note bent up TOP semitones)
 *   white 15, 16 SET PREV / NEXT: to RAIL (CC 31 on channel 16: 1 PREV, 2 NEXT), which sends the song (its key, the
 *                next song's for the blend, BPM, KIT, the LOOPS tracks in: blaze_home_ui.c bz_song)
 *   white 14     ECHO OUT: from the next bar the FM-1 into the echo (ECHO THROW) for a beat, then every track muted
 *                while the echo rings on for BARS (1 2 4), then: the internal clock stops (the tracks back), an
 *                external one keeps them muted until ECHO OUT again. Pressed while it runs: back at once
 *   black 1..6   the rates 1/4 1/8 1/8 T 1/16 1/16 T 1/32 (ROLL's 2..7)
 *   black 7..9   TAP, NUDGE -, NUDGE + (the main loop's: blaze_home_ui.c)
 *   black 10     RELEASE: every latched effect, the build, the riser, the rolls off (the main loop: COLOR and DEPTH)
 *   black 11     THE ONE: now is the 1 (the phrase counter's bar 1 beat 1, to the nearest beat)
 * 0.11, SHIFT + black (no pad held): 1 / 2 MORPH RIDE 8 / 16 bars (the main loop's), 3 DROP (bz_drop_tap), 4 ROLL
 * SHAPE (the main loop's); SHIFT + white 11 LOOP CAPTURE (blaze_cap.c). PHOSPHOR's events out (bz_ev_block).
 * KEYS (OCT- in the layer, bz_keys): the white keys play the KEYS track (bz_ktrk: a track whose keys play notes) up its
 * scale from the root, 16 notes from octave bz_koct (the notes as they are: the arp does not map them again); the
 * black keys are the main loop's (blaze_home_ui.c: the track, ARP, LATCH, its rate, the octave).
 * The phrase: 1/16 steps counted while the transport runs (bz_s16, seq.c click_tick), from the 1 THE ONE set (bz_org;
 * the transport's start until then). The key: MIDI CC 20 / 21 / 22 on channel 16 (RAIL: the Camelot number, A / B,
 * the root) noted here for the main loop (blaze_home_ui.c), whatever ROUT is. */
#define BZ_NFX 6u
static const uint8_t BZ_FX[BZ_NFX + 1u] = {PF_ETHR, PF_RTHR, PF_REV, PF_TAPE, PF_FRZ, PF_R16,
                                           PF_SCAT};   /* (0.10: [6] SCATTER, SHIFT + white 9: bz_fx_key's too) */
#define BZ_SCAT 6u
/* 0.10, SHIFT + white 9..16 (no pad held): 9 SCATTER (tap: on, hold: while held), 10 BACKSPIN, 13 / 14 MORPH A / B
 * (held: saved), 15 MORPH on / off (blaze_home_ui.c); 11 12 16: 0.11's LOOP CAPTURE */
enum { BZB_IDLE, BZB_ARMED, BZB_SPUN };
#define BZ_PAD0 0u                              /* white places (0.12, BLAZE's page): the pads 0..3, ECHO REVERB BRAKE */
#define BZ_FX0 4u                               /* REPEAT 4..7, BUILD 4 / 8, RISER, DROP, CAPTURE, EXIT, SCATTER, SPIN */
#define BZ_B4 8u
#define BZ_B8 9u
#define BZ_RISER 10u
#define BZ_DROPK 11u
#define BZ_CAPK 12u
#define BZ_EOUT 13u
#define BZ_SCATK 14u
#define BZ_SPINK 15u
#define BZ_SPREV 14u                            /* SHIFT: SET PREV, NEXT (to RAIL: CC 31 on channel 16, 1 / 2) */
#define BZ_SNEXT 15u
enum { BZB_RIDE = 6u, BZB_MORPH, BZB_ONE, BZB_REL, BZB_FILL };   /* 0.12 black places (0..5: the rates) */
enum { BZR_NOISE, BZR_AIR, BZR_SIREN, BZR_TONE, BZR_SYNTH, BZR_NTYPE };
enum { BZE_IDLE, BZE_ARMED, BZE_ECHO, BZE_TAIL, BZE_OUT };
enum { BZK_TAP = 6u, BZK_NDN, BZK_NUP, BZK_REL, BZK_ONE };   /* (ROLL's and LOOPS' black places 6..10) */
/* 0.12: what each key of BLAZE's own page does, by place, SHIFT off / on (a pad held + SHIFT: PAD PRESET's, bz_fn).
 * A key keeps what it did at its press until it is let go (bz_kfn) */
enum {
    BZF_NONE, BZF_PAD0, BZF_PAD1, BZF_PAD2, BZF_PAD3,
    BZF_FX0, BZF_FX1, BZF_FX2, BZF_FX3, BZF_FX4, BZF_FX5,          /* BZ_FX[i]: ECHO REVERB REVERSE BRAKE FREEZE REPEAT */
    BZF_B4, BZF_B8, BZF_RISER, BZF_DROP, BZF_CAP, BZF_EOUT, BZF_SCAT, BZF_SPIN,
    BZF_KIT0, BZF_KIT7 = BZF_KIT0 + 7, BZF_CAPLEN, BZF_CAPSAVE, BZF_MA, BZF_MB, BZF_PREV, BZF_NEXT,
    BZF_RATE0, BZF_RATE5 = BZF_RATE0 + 5, BZF_RIDE, BZF_MORPH, BZF_ONE, BZF_REL, BZF_FILL,
    BZF_TAP, BZF_NDN, BZF_NUP, BZF_SHAPE, BZF_FOLLOW,
    BZF_PPR0, BZF_PPR5 = BZF_PPR0 + 5, BZF_PPF0, BZF_PPF5 = BZF_PPF0 + 5,
    BZF_BOOST,                                   /* (0.14.1) */
    BZF_N
};
static const uint8_t BZW[2][16] = {
    {BZF_PAD0, BZF_PAD1, BZF_PAD2, BZF_PAD3, BZF_FX0, BZF_FX1, BZF_FX3, BZF_FX5,
     BZF_B4, BZF_B8, BZF_RISER, BZF_DROP, BZF_CAP, BZF_EOUT, BZF_SCAT, BZF_SPIN},
    {BZF_KIT0, BZF_KIT0 + 1, BZF_KIT0 + 2, BZF_KIT0 + 3, BZF_KIT0 + 4, BZF_KIT0 + 5, BZF_KIT0 + 6, BZF_KIT7,
     BZF_FX2, BZF_FX4, BZF_CAPLEN, BZF_CAPSAVE, BZF_MA, BZF_MB, BZF_PREV, BZF_NEXT}};
static const uint8_t BZB[2][11] = {
    {BZF_RATE0, BZF_RATE0 + 1, BZF_RATE0 + 2, BZF_RATE0 + 3, BZF_RATE0 + 4, BZF_RATE5, BZF_RIDE, BZF_MORPH, BZF_ONE,
     BZF_REL, BZF_FILL},
    {BZF_TAP, BZF_NDN, BZF_NUP, BZF_SHAPE, BZF_FOLLOW, BZF_BOOST, BZF_NONE, BZF_NONE, BZF_NONE, BZF_NONE, BZF_NONE}};
/* key k's function on BLAZE's page: shift SHIFT on, pads a pad held (with SHIFT: a rate or an effect sets its PRESET) */
static uint32_t bz_fn(uint32_t k, uint32_t shift, uint32_t pads)
{
    uint32_t p = key_place(k), f;
    if (key_black(k))
        return p >= 11u ? BZF_NONE : pads && shift && p < 6u ? BZF_PPR0 + p : BZB[shift ? 1 : 0][p];
    if (p >= 16u)
        return BZF_NONE;
    if (pads && shift) {
        f = BZW[0][p] >= BZF_FX0 && BZW[0][p] <= BZF_FX5 ? BZW[0][p] : BZW[1][p];
        return f >= BZF_FX0 && f <= BZF_FX5 ? BZF_PPF0 + (f - BZF_FX0) : BZF_NONE;
    }
    return BZW[shift ? 1 : 0][p];
}
static volatile uint8_t bz_kfn[27];            /* ISR: what each key held does (its press's), BZF_NONE: not BLAZE's */
static volatile uint32_t bz_ked;               /* main: keys held to edit (a knob turned): their release fires nothing */
#define BZ_TAP_MS 250u
#define BZ_RS_TAP_MS 400u                       /* RISER: a second tap within this: 16 bars */
#define BZ_PHRASE 16u                           /* bars in a phrase */

static volatile uint32_t home_mask;            /* main: the ENV button's bit while BLAZE may own the keys */
static volatile uint8_t bz_chord;              /* main: the layer is CHORD (blaze_chord.c) */
static void bz_chord_key(uint32_t p);
static void bz_chord_stop(void);
static volatile uint8_t bz_loops;              /* main: the layer is LOOPS (blaze_loops.c) */
static volatile uint8_t bz_slice;              /* main: the layer is SLICE (blaze_slice.c, 0.8) */
/* 0.9 REKORDBOX (OCT- after SLICE): the FM-1 as a second controller. Every key sends a MIDI note (on 127, off on its
 * release) and plays nothing; rekordbox's MIDI LEARN maps them. White 1..16: notes 36..51 on the DECK's channel (14:
 * deck 1, 15: deck 2: black 11 switches it); black 1..10: notes 60..69 on channel 13 (the sampler, Beat FX: one for
 * both decks); KNOB 1..4: CC 1..4 on channel 13, 0..127 (blaze_home_ui.c; sent here, the one MIDI-out writer) */
#define BZ_RB_CH 12u                            /* (channel 13: status low nibble) */
#define BZ_RB_DECK_CH 13u                       /* (14: deck 1, 15: deck 2) */
#define BZ_RB_DECK_KEY 10u                      /* black 11: DECK 1 / 2 */
static volatile uint8_t bz_rb;                 /* main: the layer is REKORDBOX */
static volatile uint8_t bz_rb_deck;            /* ISR (black 11): 0 deck 1, 1 deck 2 */
static volatile uint8_t bz_rb_val[4] = {64, 100, 64, 64};   /* main: the knobs' values */
static uint8_t bz_rb_sent[4] = {64, 100, 64, 64};   /* ISR: the values last sent (one changed: sent) */
static volatile uint32_t bz_rb_down;           /* ISR: keys down here (the main loop's LEDs) */
static uint8_t bz_rb_kch[27];                  /* ISR: the channel each key's note went on (its note-off: the same) */
static void bz_loops_key(uint32_t p);
static volatile uint8_t bz_keys;               /* main: the layer is KEYS (OCT-) */
static volatile uint8_t bz_ktrk;               /* main: the KEYS track */
static volatile int8_t bz_koct;                /* main: KEYS' octave, -2..2 (0: the root at C3 .. B3) */
static volatile uint32_t bz_s16;               /* ISR: 1/16 steps since the transport started */
static volatile uint32_t bz_org;               /* ISR: the step THE ONE made bar 1 */
static volatile uint8_t bz_build;              /* ISR: a build running: its bars (4, 8), 0 none */
static volatile uint8_t bz_build_rate;         /* ISR: its rate now (RL index), the main loop shows it */
static volatile uint8_t bz_riser;              /* ISR: a riser armed or running: its bars (8, 16), 0 none */
static volatile uint8_t bz_rel;                /* ISR sets: RELEASE pressed (the main loop homes the knobs) */
static volatile uint8_t bz_rs_type, bz_rs_top = 100, bz_rs_lvl = 100, bz_rs_space = 32, bz_rs_trk = 1;   /* main: the
                                                * riser's sound (from the settings: blaze_home_ui.c bz_cfg) */
static volatile uint8_t bz_eo_bars = 2;        /* main: ECHO OUT's tail, bars */
static volatile uint8_t bz_eo;                 /* ISR: ECHO OUT's state (BZE_*) */
static uint32_t bz_eo_left;                    /* .. samples left of the state */
static uint8_t bz_rs_note, bz_rs_on;           /* SYNTH: its note, sounding on track bz_rs_on - 1 */
static volatile uint8_t bz_cc_seq, bz_cc[44], bz_cc_knob;   /* (0.12: CC 20..63; RAIL's knobs 50..54 heard: bits) */  /* ISR: CC 20 21 22 (the key), 23 24 25 (the next track's, for a blend; 23
                                                * 0: none), 26..30 (RAIL's SET: the song's BPM - 60, KIT + 1, LOOPS
                                                * 64 + the tracks in, the song's number, how many) on channel 16 */
static volatile uint8_t bz_set_n;              /* main: the SET's songs (RAIL), 0: none */
static volatile uint8_t bz_set_layers;         /* main: 0x80 | the tracks a song wants in (LOOPS takes them at its point) */
static volatile uint16_t bz_blend;             /* main: the next track's scale (bit pc, 0 = C), 0: no blend (KEYS and
                                                * CHORD keep to the notes both keys share) */
static volatile uint8_t bz_lock = 1;           /* main: MIDI IN's notes onto the key (0.7: midi_control.c bz_lock_note) */
static volatile uint8_t bz_shift_on;           /* main: SHIFT (OCT+) held in the layer (bz_ui_pass, each pass) */
static uint8_t bz_follow;                      /* main: a key came from RAIL (the scale LEDs on: ui_input.c) */
static volatile uint8_t bz_pp_dirty;           /* ISR sets: a PAD PRESET changed (0.9; the main loop saves, says it) */
static volatile uint8_t bz_pp_lane;            /* .. its lane */
static uint16_t bz_pads;                       /* ISR: the pads the white keys 1..4 hold (key bits 0..3) */
static uint8_t bz_tapfx;                       /* ISR: the effects pressed down as a tap could be (bit i of BZ_FX) */
static uint8_t bz_offfx;                       /* .. pressed to turn a latched one off (its release does nothing) */
static uint32_t bz_fxt[BZ_NFX + 1u];           /* .. when (bz_now) */
static volatile uint16_t bz_shk_held;          /* ISR: .. those still down (the main loop's MORPH keys read it) */
static volatile uint8_t bz_spin;               /* ISR: BACKSPIN: BZB_* */
static uint32_t bz_spin_left;                  /* .. samples to its spin (ARMED), to the 1 that ends it (SPUN) */
static uint32_t bz_now;                        /* ISR: samples, real time */
static uint32_t bz_wait, bz_age, bz_next;      /* the build: samples to its start, since it started, to its next hit */
static uint32_t bz_rs_wait, bz_rs_age, bz_rs_t;   /* the riser: samples to its start, since it started; its tap */
/* 0.12 BUILD's settings (hold BUILD + the knobs; blaze_home_ui.c keeps them): the drum, how it speeds up, how much it
 * swells, what joins it; FILL (black 11): a roll from now to the bar's 1 on the same drum */
enum { BZBC_QUARTERS, BZBC_EIGHTHS, BZBC_STRAIGHT, BZBC_SIXTEENTHS, BZBC_N };   /* 1/4>1/32 1/8>1/32 1/16 1/16>1/32 */
enum { BZBX_NONE, BZBX_FILTER, BZBX_ECHO, BZBX_RISER, BZBX_N };
static volatile uint8_t bz_b_lane = 1, bz_b_curve, bz_b_swell = 100, bz_b_fx;   /* main */
static volatile uint8_t bz_b_fill;             /* ISR: the build running is FILL's */
static uint8_t bz_b_fxon;                      /* ISR: its effect running (BZBX_*) */
static int8_t bz_b_k0;                         /* .. FILTER: COLOR before it */
static uint32_t bz_b_total;                    /* .. its length, samples */
/* 0.12 DROP's settings (hold DROP + the knobs): how far COLOR climbs, how long, what comes with it, what hits on the 1 */
enum { BZDL_PHRASE, BZDL_4, BZDL_8, BZDL_16, BZDL_N };
enum { BZDX_SPIN, BZDX_SPIN_BUILD, BZDX_BUILD, BZDX_NONE, BZDX_N };
enum { BZDH_NONE, BZDH_ECHO, BZDH_REVERB, BZDH_CRASH, BZDH_N };
static volatile uint8_t bz_d_top = 70, bz_d_len, bz_d_extra, bz_d_hit;   /* main */
static uint32_t bz_d_fxleft;                   /* ISR: the 1's ECHO / REVERB throw: samples left */
static uint8_t bz_d_fxbit;                     /* .. its effect (PF_*), 0 none */
/* 0.12: MIDI out from the main loop (FOLLOW's state, the SELECT knob's tempo nudges to RAIL): a ring, sent by the ISR */
static volatile uint32_t bz_tx[8];
static volatile uint8_t bz_tx_w, bz_tx_r;
/* 0.12: RAIL's actions (channel 16 CC 36: 1 BUILD 4, 2 BUILD 8, 3 RISER, 4 DROP, 5 EXIT, 6 BACKSPIN, 7 CAPTURE,
 * 8 ALL OFF, 9 FILL, 10 SCATTER): the main loop sets it, the ISR does it */
static volatile uint8_t bz_req;
/* 0.12 FOLLOW: the main loop switched the clock (G_CLOCK) and the transport plays on through it (seq.c events_block) */
static volatile uint8_t bz_clk_soft;
/* 0.12: BLAZE's four pads, each its own (hold one + ALGORITHM: another drum): pad index + 1, 0: ROLL's PADS 4's */
static volatile uint8_t bz_pad_slot[4];
/* 0.13 hands-free rolls: a pad held and a rate picked (a rate key, or the RATE knob: knob 4 with a pad held or rolling)
 * rolls on when let go (bz_padl, white 1..4: bits 0..3), until the pad is tapped again or ALL OFF. A rate picked while
 * a pad is held or rolling is the one rate (the others latched let go); the last pad stopped lets it go */
static volatile uint8_t bz_padl;               /* ISR: the pads rolling hands-free */
static uint8_t bz_rx;                          /* ISR: rate keys (bit r) whose press picked the rate (their release: none) */
static volatile uint8_t bz_rk_req;             /* main: the RATE knob's rate + 1 for the ISR (0: none) */
static volatile uint8_t bz_rk_rate = 4u;       /* main: the RATE knob's rate (ROLL's 1..6, 4: 1/16) */
static uint8_t bz_rk_on;                       /* ISR: a rate latched by the knob (or a double tap) */
static uint8_t bz_pad_dn[4];                   /* ISR: the pad each white 1..4 held plays (its press's: a swap meanwhile) */
enum { BZD_IDLE, BZD_ARMED, BZD_RUN };
static volatile uint8_t bz_drop;               /* ISR: 0.11 DROP: BZD_* */
static volatile uint8_t bz_dr_bars;            /* .. its bars (the main loop says them) */
static uint8_t bz_dr_spun;                     /* .. its BACKSPIN asked for */
static int8_t bz_dr_k0;                        /* .. COLOR before it (back on the 1) */
static uint32_t bz_dr_wait, bz_dr_age, bz_dr_total;   /* .. samples to its start, since, in all */
static uint32_t bz_dr_hit;                     /* .. the drop's 1 just came: samples left of its event (CC 41) */
static volatile uint8_t bz_ev_morph;           /* main: MORPH's place for PHOSPHOR, 0..127 (CC 44) */
static volatile uint8_t bz_ev_on;              /* main: BLAZE opened since power-on: PHOSPHOR's events go out */
static struct {
    int32_t lp[2], bp[2];                      /* the riser's two filters (left, right) */
    uint32_t s[2];                             /* .. their noise */
    uint32_t ph[2];                            /* TONE: its two saws' phases */
} bzr = {{0, 0}, {0, 0}, {0x2545F491u, 0x9E3779B9u}, {0, 0x40000000u}};

/* white key i (0..3): the pad of ROLL's 4 it plays (in key order), 16 none */
static uint32_t bz_pad_of(uint32_t i)
{
    uint32_t p, n = 0, m = rl_live4;
    if (i < 4u && bz_pad_slot[i])
        return (bz_pad_slot[i] - 1u) % 16u;      /* (0.12: swapped with ALGORITHM) */
    for (p = 0; p < 16u; p++)
        if (((m >> p) & 1u) && n++ == i)
            return p;
    return 16u;
}

/* KEYS: the note white key i (0..15) plays on track t: up the scale from the root (CHR: every semitone); in a blend
 * (bz_blend) only the notes both keys share */
static uint32_t bz_key_note(const track_t *t, uint32_t i)
{
    uint32_t mask = scale_mask(t), deg[12], n = 0, pc, r = (uint32_t)t->p[P_ROOT] % 12u, bl = bz_blend;
    int32_t note;
    for (pc = 0; pc < 12u; pc++)
        if ((mask >> pc) & 1u && (!bl || ((bl >> ((r + pc) % 12u)) & 1u)))
            deg[n++] = pc;
    if (!n)
        for (pc = 0; pc < 12u; pc++)
            if ((mask >> pc) & 1u)
                deg[n++] = pc;
    if (!n)
        return 60u;
    note = 48 + t->p[P_ROOT] % 12 + 12 * bz_koct + 12 * (int32_t)(i / n) + (int32_t)deg[i % n];
    return (uint32_t)clamp(note, 0, 127);
}

/* the rate (RL index) the build plays at age samples into total, a quarter of it each: BUILD's curve (0.12: BZBC_*),
 * FILL's 1/8 1/16 1/32 1/32 */
static uint32_t bz_build_at(uint32_t age, uint32_t total)
{
    static const uint8_t R[BZBC_N + 1u][4] = {{1u, 2u, 4u, 6u}, {2u, 2u, 4u, 6u}, {4u, 4u, 4u, 4u}, {4u, 4u, 6u, 6u},
                                              {2u, 4u, 6u, 6u}};
    uint32_t q = total / 4u, s = q ? age / q : 0u;
    return R[bz_b_fill ? BZBC_N : bz_b_curve % BZBC_N][s < 4u ? s : 3u];
}

/* samples to the next bar of the phrase (0: on it now, or stopped) */
static uint32_t bz_to_bar(void)
{
    uint32_t s16 = div_samples(2), in;
    if (!song.playing || clk_pos == CLK_START)
        return 0;
    in = (bz_s16 - bz_org) % 16u;
    return in || clk_pos ? (16u - in) * s16 - clk_pos : 0u;
}

/* ECHO OUT's mutes: every track (perform.c PF_M1..) */
#define BZ_MUTES (((1u << NTRK) - 1u) << PF_M1)
static void bz_eo_back(void)
{
    perf_held &= ~(PF_BIT(PF_ETHR) | BZ_MUTES);
    bz_eo = BZE_IDLE;
}
static void bz_eo_tap(void)
{
    if (bz_eo != BZE_IDLE) {
        bz_eo_back();                            /* (running or out: back at once) */
        return;
    }
    bz_eo_left = bz_to_bar();
    bz_eo = BZE_ARMED;
}

/* SYNTH riser: its note ends, its bend back */
static void bz_rs_synth_off(void)
{
    if (bz_rs_on) {
        trk_note_off(&trk[(bz_rs_on - 1u) % NTRK], bz_rs_note);
        midi_bend_target[(bz_rs_on - 1u) % NTRK] = 0;
        bz_rs_on = 0;
    }
}

/* the build's effect off (COLOR back, the echo let go) */
static void bz_build_fx_off(void)
{
    if (bz_b_fxon == BZBX_FILTER)
        perf_k[0] = bz_b_k0;
    else if (bz_b_fxon == BZBX_ECHO)
        perf_held &= ~PF_BIT(PF_ETHR);
    bz_b_fxon = BZBX_NONE;
}
static void bz_build_stop(void)
{
    bz_build_fx_off();
    bz_build = 0;
    bz_b_fill = 0;
}
/* a build: tag (4 / 8 / a DROP's bars / 1 FILL) shown, total samples long after wait; fill: FILL's rates, no effect */
static void bz_build_go(uint32_t tag, uint32_t total, uint32_t wait, uint32_t fill, uint32_t fx)
{
    bz_build_stop();
    bz_wait = wait;
    bz_age = 0;
    bz_next = 0;
    bz_b_fill = (uint8_t)fill;
    bz_b_total = total ? total : 1u;
    bz_build_rate = (uint8_t)bz_build_at(0, bz_b_total);
    bz_b_fxon = (uint8_t)(fx < BZBX_N ? fx : BZBX_NONE);   /* (FILTER takes COLOR when it starts) */
    if (bz_b_fxon == BZBX_RISER) {               /* (RISER: the riser over the same bars) */
        bz_b_fxon = BZBX_NONE;
        if (!bz_riser) {
            bz_rs_wait = wait;
            bz_rs_age = 0;
            bz_rs_t = bz_now;
            bzr.lp[0] = bzr.lp[1] = bzr.bp[0] = bzr.bp[1] = 0;
            bz_riser = (uint8_t)tag;
        }
    }
    bz_build = (uint8_t)(tag ? tag : 1u);
}
static void bz_build_start(uint32_t bars)
{
    if (bz_build == bars && !bz_b_fill) {
        bz_build_stop();                         /* (again: stopped) */
        return;
    }
    bz_build_go(bars, bars * 4u * beat_samples(), bz_to_bar(), 0, bz_b_fx);
}
/* the build, adv samples on (after its wait): its hits on their own grid, louder as it goes (SWELL), its effect */
static __attribute__((noinline)) void bz_build_more(uint32_t adv)
{
    uint32_t total = bz_b_total, r, b = beat_samples(), x;
    uint32_t pad = bz_b_lane % NLANE, sw = bz_b_swell > 100u ? 100u : bz_b_swell;
    while (bz_next < adv) {                      /* (hits due in this block: on their own grid) */
        uint32_t at = bz_age + bz_next;
        if (at >= total) {
            bz_build_stop();
            return;
        }
        r = bz_build_at(at, total);
        bz_build_rate = (uint8_t)r;
        rl_hit(pad, 127u - sw * 87u / 100u + sw * 87u / 100u * at / total);   /* (127 - 87 SWELL % -> 127) */
        bz_next += div_samples(RL_DIV[r]) ? div_samples(RL_DIV[r]) : 1u;
    }
    bz_next -= adv;
    bz_age += adv;
    if (bz_b_fxon == BZBX_FILTER) {              /* FILTER: COLOR up into the high pass, x^2 */
        int32_t k0 = bz_b_k0 < 60 ? bz_b_k0 : 60;
        x = bz_age >= total ? 32768u : (uint32_t)((uint64_t)bz_age * 32768u / total);
        perf_k[0] = (int8_t)(k0 + (int32_t)((uint32_t)(60 - k0) * (x * x >> 15) >> 15));
    } else if (bz_b_fxon == BZBX_ECHO && bz_age + b >= total && !(perf_held & PF_BIT(PF_ETHR))) {
        perf_held |= PF_BIT(PF_ETHR);            /* ECHO: the last beat into the echo */
        perf_ord[PF_ETHR] = ++perf_seq;
    }
    if (bz_age >= total)
        bz_build_stop();                         /* (its last bar over: the drop) */
}

/* 0.12 FILL (black 11): BUILD's drum from now (on the next 1/16) to the bar's 1, 1/8 -> 1/32; under 2 beats left:
 * the bar after. Stopped: one bar. Again: stopped */
static void bz_fill_tap(void)
{
    uint32_t w = bz_to_bar(), b = beat_samples(), s16 = div_samples(2);
    if (bz_build && bz_b_fill) {
        bz_build_stop();
        return;
    }
    if (!song.playing || !w)
        w = 4u * b;
    else if (w < 2u * b)
        w += 4u * b;
    bz_build_go(1, w, 0, 1, BZBX_NONE);
    if (song.playing && clk_pos != CLK_START && s16 && clk_pos)
        bz_next = s16 - clk_pos % s16;           /* (the first hit on the grid) */
}

/* RISER tapped: 8 bars from the next bar; tapped again quickly: 16; tapped later: stopped */
static void bz_riser_tap(void)
{
    if (bz_riser == 8u && bz_now - bz_rs_t < BZ_RS_TAP_MS * 441u / 10u) {
        bz_riser = 16u;
        return;
    }
    if (bz_riser) {
        bz_riser = 0;
        bz_rs_synth_off();
        return;
    }
    bz_rs_wait = bz_to_bar();
    bz_rs_age = 0;
    bz_rs_t = bz_now;
    bzr.lp[0] = bzr.lp[1] = bzr.bp[0] = bzr.bp[1] = 0;
    bz_riser = 8u;
}

/* 0.10 BACKSPIN: off (its effect let go, the cue back to the FX layer's own) */
static void bz_spin_off(void)
{
    perf_held &= ~PF_BIT(PF_SPIN);
    pf_spin_cue = 0;
    bz_spin = BZB_IDLE;
}
/* samples to the next beat of the phrase (0: on it now, or stopped) */
static uint32_t bz_to_beat(void)
{
    uint32_t s16 = div_samples(2), in;
    if (!song.playing || clk_pos == CLK_START)
        return 0;
    in = (bz_s16 - bz_org) % 4u;
    return in || clk_pos ? (4u - in) * s16 - clk_pos : 0u;
}
/* BACKSPIN pressed: it records from now and spins on the next beat (at least a quarter beat later; stopped: half a
 * beat on), backwards to a stop over half a beat, then silent until the next bar's 1 (stopped: a beat). Again: off */
static void bz_spin_tap(void)
{
    uint32_t w = bz_to_beat(), b = beat_samples();
    if (bz_spin != BZB_IDLE) {
        bz_spin_off();
        return;
    }
    if (!song.playing)
        w = b >> 1;
    else if (w < (b >> 2))
        w += b;
    perf_held |= PF_BIT(PF_SPIN);
    perf_ord[PF_SPIN] = ++perf_seq;
    pf_spin_cue = 1;
    bz_spin_left = w;
    bz_spin = BZB_ARMED;
}

/* 0.11.1: RAIL's bar (channel 16 CC 32, bz_cc[12]; the main loop's, so the MIDI path in the IRQ stays as it was): 1..32 the bar playing now is that bar of the phrase (16 bars: 17..32 wrap),
 * the beat inside it kept; 64 the nearest bar line is the 1 (RAIL's P, as THE ONE but to the bar). Builds, the riser,
 * DROP, LOOPS and SLICE's points count from the new 1 */
static void bz_bar_set(uint32_t v)
{
    uint32_t in = (bz_s16 - bz_org) % 16u;
    if (v >= 1u && v <= 32u)
        bz_org = bz_s16 - in - ((v - 1u) % BZ_PHRASE) * 16u;
    else if (v == 64u)
        bz_org = bz_s16 - in + (in >= 8u ? 16u : 0u);   /* (past the bar's middle: the next bar line) */
}

#include "blaze_cap.c"                           /* 0.11 LOOP CAPTURE (bz_to_bar above) */

/* 0.11 DROP (SHIFT + black 3, playing): from the next bar to the phrase's next 1 (fewer than 2 bars left: the one
 * after): COLOR sweeps up into the high pass (to +70, slow then fast), the RISER (its TYPE) runs over those bars, a
 * BACKSPIN on the last beat, and on the 1 it all lets go: COLOR back where it was. Again: off at once */
#define BZ_DR_TOP 70
static void bz_drop_off(void)
{
    if (bz_drop == BZD_RUN) {
        if (bz_d_top)
            perf_k[0] = bz_dr_k0;
        if ((bz_d_extra == BZDX_SPIN_BUILD || bz_d_extra == BZDX_BUILD) && bz_build && !bz_b_fill)
            bz_build_stop();                     /* (0.12: its BUILD with it) */
        bz_riser = 0;
        bz_rs_synth_off();
        if (bz_dr_spun && bz_spin != BZB_IDLE)
            bz_spin_off();
    }
    bz_drop = BZD_IDLE;
}
static void bz_drop_tap(void)
{
    uint32_t w, in, bar;
    if (bz_drop != BZD_IDLE) {
        bz_drop_off();
        return;
    }
    if (!song.playing)
        return;                                  /* (the main loop says why) */
    w = bz_to_bar();
    in = bz_s16 - bz_org;
    bar = (in / 16u + (w ? 1u : 0u)) % BZ_PHRASE;   /* (the bar it starts on, 0..15) */
    bar = BZ_PHRASE - bar;
    if (bar < 2u)
        bar += BZ_PHRASE;
    if (bz_d_len != BZDL_PHRASE)                 /* (0.12 LENGTH: 4 / 8 / 16 bars from the next bar) */
        bar = 4u << (bz_d_len - 1u);
    bz_dr_bars = (uint8_t)bar;
    bz_dr_wait = w;
    bz_dr_age = 0;
    bz_dr_spun = 0;
    bz_drop = BZD_ARMED;
}
/* 0.12 DROP's 1: an ECHO or REVERB throw for a beat, or the kit's crash (lane 8, accented) */
static void bz_drop_hit(uint32_t b)
{
    if (bz_d_hit == BZDH_ECHO || bz_d_hit == BZDH_REVERB) {
        bz_d_fxbit = (uint8_t)(bz_d_hit == BZDH_ECHO ? PF_ETHR : PF_RTHR);
        perf_held |= PF_BIT(bz_d_fxbit);
        perf_ord[bz_d_fxbit] = ++perf_seq;
        bz_d_fxleft = b;
    } else if (bz_d_hit == BZDH_CRASH)
        rl_hit(NLANE - 1u, 127u);
}
static __attribute__((noinline)) void bz_drop_block(uint32_t adv)
{
    uint32_t a = adv, b = beat_samples(), end, x;
    int32_t k0;
    if (bz_drop == BZD_ARMED) {
        if (bz_dr_wait > a) {
            bz_dr_wait -= a;
            return;
        }
        a -= bz_dr_wait;
        bz_dr_wait = 0;
        bz_dr_total = (uint32_t)bz_dr_bars * 16u * div_samples(2);
        bz_dr_k0 = perf_k[0];
        bz_rs_synth_off();                       /* (the riser: its bars from now, whatever it was doing) */
        bz_rs_wait = 0;
        bz_rs_age = 0;
        bz_rs_t = bz_now;
        bzr.lp[0] = bzr.lp[1] = bzr.bp[0] = bzr.bp[1] = 0;
        bz_riser = bz_dr_bars;
        if (bz_d_extra == BZDX_SPIN_BUILD || bz_d_extra == BZDX_BUILD)
            bz_build_go(bz_dr_bars, bz_dr_total, 0, 0, BZBX_NONE);   /* (0.12: a BUILD into it, BUILD's drum) */
        bz_drop = BZD_RUN;
    }
    bz_dr_age += a;
    if (bz_dr_age >= bz_dr_total) {              /* the 1: everything back */
        bz_drop_off();
        bz_dr_hit = b;
        bz_drop_hit(b);                          /* (0.12: what hits on it) */
        return;
    }
    end = bz_dr_total > b ? bz_dr_total - b : 1u;   /* (the sweep's top: the last beat's, the spin's) */
    x = bz_dr_age >= end ? 32768u : (uint32_t)((uint64_t)bz_dr_age * 32768u / end);
    if (bz_d_top) {                              /* (0.12 TOP: how far COLOR climbs, 0: it stays) */
        int32_t top = bz_d_top > 100u ? 100 : (int32_t)bz_d_top;
        k0 = bz_dr_k0 < top ? bz_dr_k0 : top;
        perf_k[0] = (int8_t)(k0 + (int32_t)((uint32_t)(top - k0) * (x * x >> 15) >> 15));
    }
    if (!bz_dr_spun && (bz_d_extra == BZDX_SPIN || bz_d_extra == BZDX_SPIN_BUILD) &&
        bz_dr_age + b + b / 2u >= bz_dr_total) {   /* (half way into beat 3 of the last bar: the
                                                  * spin on beat 4, silent to the 1) */
        bz_dr_spun = 1;
        if (bz_spin == BZB_IDLE)
            bz_spin_tap();
    }
}

/* 0.11 PHOSPHOR: BLAZE's moments out as CC on channel 16 for visuals (RAIL passes them on): 40 the phrase's bar
 * (1..16, 0 stopped), 41 DROP (32 armed, 64 building, 127 its 1 for a beat), 42 RISER (1..127 as it rises, 0 off),
 * 43 BACKSPIN (64 armed, 127 spinning), 44 MORPH's place (0 A .. 127 B), 45 LOOP CAPTURE (32 armed, 64 recording,
 * 96 solo, 127 mixed in), 46 SCATTER (127 on), 47 BUILD (1..127 as it goes). Sent when it changes, two a block,
 * once BLAZE (or ROLL) has been opened since power-on (Felucca alone sends nothing new) */
#define BZ_EV_N 9u                              /* (0.13: CC 48 the roll's rate; CC 49 each hit: blaze_roll.c) */
static uint8_t bz_ev_sent[BZ_EV_N];             /* (all 0: idle, stopped; nothing sent until something moves) */
static __attribute__((noinline)) void bz_ev_block(uint32_t n)
{
    static const uint8_t CAPV[8] = {0, 32, 64, 96, 127, 96, 0, 0};   /* (BZC_*: FADE still sounding) */
    uint8_t v[BZ_EV_N];
    uint32_t i, sent = 0, t;
    if (!bz_ev_on)
        return;
    v[0] = song.playing ? (uint8_t)((bz_s16 - bz_org) / 16u % BZ_PHRASE + 1u) : 0u;
    if (bz_dr_hit) {
        v[1] = 127;
        bz_dr_hit = bz_dr_hit > n ? bz_dr_hit - n : 0u;
    } else
        v[1] = bz_drop == BZD_ARMED ? 32u : bz_drop == BZD_RUN ? 64u : 0u;
    t = (uint32_t)bz_riser * 4u * beat_samples();
    v[2] = bz_riser && !bz_rs_wait && t ? (uint8_t)(1u + (bz_rs_age < t ? (uint32_t)((uint64_t)bz_rs_age * 126u / t)
                                                                  : 126u)) : 0u;
    v[3] = bz_spin == BZB_SPUN ? 127u : bz_spin == BZB_ARMED ? 64u : 0u;
    v[4] = (uint8_t)(bz_ev_morph & 127u);
    v[5] = CAPV[bz_cap & 7u];
    v[6] = ((perf_held | perf_latched) & PF_BIT(PF_SCAT)) ? 127u : 0u;
    t = bz_b_total;
    v[7] = bz_build && !bz_wait && t ? (uint8_t)(1u + (bz_age < t ? (uint32_t)((uint64_t)bz_age * 126u / t) : 126u))
                                     : 0u;
    for (v[8] = 0, i = RL_NRATE; i-- > 0u;)        /* 0.13: the fastest rate rolling: 1 (a bar) .. 7 (1/32), 0 none */
        if ((rl_runs >> i) & 1u) {
            v[8] = (uint8_t)(i + 1u);
            break;
        }
    for (i = 0; i < BZ_EV_N && sent < 2u; i++)
        if (v[i] != bz_ev_sent[i]) {
            bz_ev_sent[i] = v[i];
            midi_out_event(0x0Bu | 0xBFu << 8 | (40u + i) << 16 | (uint32_t)v[i] << 24);
            sent++;
        }
}

/* 0.9 PAD PRESETS: SHIFT + a rate (r1: 1..7) or an effect (f1: 1..6) key with BLAZE pads held: on that pad's lane,
 * toggled (the same again: none) */
static void bz_pp_set(uint32_t r1, uint32_t f1)
{
    uint32_t i, pad, l;
    for (i = 0; i < 4u; i++) {
        if (!((bz_pads >> i) & 1u) || (pad = bz_pad_of(i)) >= 16u)
            continue;
        l = pad % NLANE;
        if (r1)
            rl_pp_rate[l] = (uint8_t)(rl_pp_rate[l] == r1 ? 0u : r1);
        if (f1)
            rl_pp_fx[l] = (uint8_t)(rl_pp_fx[l] == f1 ? 0u : f1);
        bz_pp_lane = (uint8_t)l;
        bz_pp_dirty = 1;
    }
}
/* a pad's PRESET effect off when its pad lets go: unless another pad held carries it, or its own key holds it */
static void bz_pp_fx_off(uint32_t i, uint32_t f1)
{
    uint32_t j, pad, e = BZ_FX[f1 - 1u];
    for (j = 0; j < 4u; j++)
        if (j != i && ((bz_pads >> j) & 1u) && (pad = bz_pad_of(j)) < 16u && rl_pp_fx[pad % NLANE] == f1)
            return;
    if (!((bz_tapfx >> (f1 - 1u)) & 1u))
        perf_held &= ~PF_BIT(e);
}

static void bz_release_all(void)
{
    uint32_t i;
    rl_unlatch();                                /* (0.9: the tapped rates, the pads at their own rate) */
    rl_pp = 0;
    bz_drop_off();                               /* (0.11: DROP, COLOR back; LOOP CAPTURE out) */
    bz_cap_off();
    bz_build_stop();                             /* (0.12: the build's effect too) */
    bz_spin_off();                               /* (0.10: BACKSPIN; SCATTER below) */
    for (i = 0; i < BZ_NFX + 1u; i++)
        perf_held &= ~PF_BIT(BZ_FX[i]);
    perf_latched = 0;
    bz_tapfx = bz_offfx = 0;
    bz_build = 0;
    bz_riser = 0;
    bz_rs_synth_off();
    bz_eo_back();
    bz_chord_stop();
    bz_pads = 0;
    bz_padl = 0;                                 /* (0.13: the pads rolling on their own) */
    rd_on = rd_latch = 0;                        /* (0.14: READY ROLLS) */
    bz_rk_on = 0;
    bz_rx = 0;
    rl_extra = 0;
    rl_rerun();
    bz_rel = 1;
}

/* an effect key (i: 0..5) down / up: a tap latches, a hold plays while held */
static void bz_fx_key(uint32_t i, int down)
{
    uint32_t e = BZ_FX[i], b = PF_BIT(e), m = 1u << i;
    if (down) {
        if (perf_latched & b) {
            perf_latched &= ~b;
            bz_offfx |= (uint8_t)m;
            return;
        }
        perf_held |= b;
        perf_ord[e] = ++perf_seq;
        bz_tapfx |= (uint8_t)m;
        bz_fxt[i] = bz_now;
        return;
    }
    if (bz_offfx & m) {
        bz_offfx &= (uint8_t)~m;
        return;
    }
    perf_held &= ~b;
    if ((bz_tapfx & m) && bz_now - bz_fxt[i] < BZ_TAP_MS * 441u / 10u)
        perf_latched |= b;                       /* (a tap: on until tapped again or RELEASE) */
    bz_tapfx &= (uint8_t)~m;
}

/* 0.13: no pad held or rolling: the knob's rate let go (a tap then plays once again) */
static void bz_rk_let(void)
{
    if (bz_rk_on && !bz_pads && !bz_padl) {
        bz_rk_on = 0;
        rl_unlatch();
    } else
        rl_rerun();
}
/* 0.13: rate r picked with a pad held or rolling: the one rate, the pads held roll on when let go */
static void bz_rk_pick(uint32_t r)
{
    bz_rk_on = 1;
    bz_padl |= (uint8_t)bz_pads;
    rl_knob_rate(r % RL_NRATE);
}
/* 0.13: pad i (white 1..4) rolling on its own: stopped */
static void bz_pad_unlatch(uint32_t i)
{
    uint32_t pad = bz_pad_dn[i] % 16u;
    bz_padl &= (uint8_t)~(1u << i);
    rl_pp_key(pad, 0, 0);
    rl_extra &= (uint16_t)~(1u << pad);
    bz_rk_let();
}
/* a key of the BLAZE layer down / up (k: 0 = F3 .. 26 = G5). noinline: out of keyboard_block's loop */
/* 0.12: a key of BLAZE's page down (its function f, bz_fn) */
static __attribute__((noinline)) void bz_fn_down(uint32_t f, uint32_t k)
{
    uint32_t pad, i;
    if (f >= BZF_PAD0 && f <= BZF_PAD3) {
        i = f - BZF_PAD0;
        if ((bz_padl >> i) & 1u) {               /* 0.13: rolling on its own: this press stops it (its release: nothing) */
            bz_pad_unlatch(i);
            return;
        }
        pad = bz_pad_of(i);
        if (pad >= 16u)
            return;
        bz_pad_dn[i] = (uint8_t)pad;
        bz_pads |= (uint16_t)(1u << i);
        rl_vel[pad] = (uint8_t)kb_vel;
        if (rl_pp_fx[pad % NLANE]) {             /* 0.9: its PRESET effect, while held */
            uint32_t e = BZ_FX[(rl_pp_fx[pad % NLANE] - 1u) % BZ_NFX];
            perf_held |= PF_BIT(e);
            perf_ord[e] = ++perf_seq;
        }
        if (rl_pp_rate[pad % NLANE]) {           /* 0.9: its PRESET rate: rolls at it, apart from the rates on */
            rl_pp_key(pad, rl_pp_rate[pad % NLANE], 1);
            return;
        }
        rl_extra |= (uint16_t)(1u << pad);
        if (!rl_nr)
            rl_hit(pad, rl_vel[pad]);
        else
            rl_rerun();
    } else if (f >= BZF_FX0 && f <= BZF_FX5)
        bz_fx_key(f - BZF_FX0, 1);
    else if (f == BZF_SCAT)
        bz_fx_key(BZ_SCAT, 1);
    else if (f == BZF_SPIN)
        bz_spin_tap();
    else if (f == BZF_CAP)
        bz_cap_tap();                            /* (0.11 LOOP CAPTURE; LENGTH, SAVE: the main loop's) */
    else if (f == BZF_PREV || f == BZF_NEXT)     /* SET: RAIL moves its list and sends the song back */
        midi_out_event(0x0Bu | 0xBFu << 8 | 31u << 16 | (f == BZF_NEXT ? 2u : 1u) << 24);
    else if (f >= BZF_RATE0 && f <= BZF_RATE5) {  /* (ROLL's rates 1/4 .. 1/32) */
        uint32_t r = f - BZF_RATE0 + 1u;
        if (bz_pads || bz_padl) {                /* 0.13: a pad held or rolling: picked (it rolls on) */
            bz_rx |= (uint8_t)(1u << r);
            bz_rk_rate = (uint8_t)r;
            bz_rk_pick(r);
        } else
            rl_rate(r, 1);
    }
    else if (f == BZF_ONE)                       /* the nearest step's beat is the 1 */
        bz_org = ((bz_s16 + (clk_pos != CLK_START && clk_pos * 2u >= div_samples(2) ? 1u : 0u)) + 2u) & ~3u;
    else if (f == BZF_REL)
        bz_release_all();
    else if (f == BZF_FILL)
        bz_fill_tap();
    else if (f >= BZF_PPR0 && f <= BZF_PPR5)     /* 0.9: a pad + SHIFT + a rate: the pad's own rate */
        bz_pp_set(f - BZF_PPR0 + 2u, 0);
    else if (f >= BZF_PPF0 && f <= BZF_PPF5)     /* .. + an effect: the pad's own effect */
        bz_pp_set(0, f - BZF_PPF0 + 1u);
    (void)k;                                     /* (KITS, MORPH A / B, RIDE, MORPH, TAP, NUDGE, SHAPE, FOLLOW, LENGTH,
                                                  * SAVE: the main loop's, by the key held) */
}
/* .. let go (edited: a knob turned while it was held: BUILD, RISER, DROP, EXIT fire nothing) */
static __attribute__((noinline)) void bz_fn_up(uint32_t f, uint32_t k, uint32_t edited)
{
    uint32_t pad, i;
    if (f >= BZF_PAD0 && f <= BZF_PAD3) {
        i = f - BZF_PAD0;
        pad = bz_pad_dn[i] % 16u;
        if (!((bz_pads >> i) & 1u))
            return;
        if (rl_pp_fx[pad % NLANE])
            bz_pp_fx_off(i, rl_pp_fx[pad % NLANE]);
        bz_pads &= (uint16_t)~(1u << i);
        if ((bz_padl >> i) & 1u)                 /* 0.13: rolls on */
            return;
        rl_pp_key(pad, 0, 0);
        rl_extra &= (uint16_t)~(1u << pad);
        bz_rk_let();
    } else if (f >= BZF_FX0 && f <= BZF_FX5)
        bz_fx_key(f - BZF_FX0, 0);
    else if (f == BZF_SCAT)
        bz_fx_key(BZ_SCAT, 0);
    else if (f >= BZF_RATE0 && f <= BZF_RATE5) {
        uint32_t r = f - BZF_RATE0 + 1u;
        if ((bz_rx >> r) & 1u)                   /* (0.13: its press picked the rate) */
            bz_rx &= (uint8_t)~(1u << r);
        else
            rl_rate(r, 0);
    }
    else if (edited)
        return;
    else if (f == BZF_B4 || f == BZF_B8)         /* (0.12: these fire when let go: held, the knobs set them up) */
        bz_build_start(f == BZF_B4 ? 4u : 8u);
    else if (f == BZF_RISER)
        bz_riser_tap();
    else if (f == BZF_DROP)
        bz_drop_tap();
    else if (f == BZF_EOUT)
        bz_eo_tap();
    (void)k;
}

static __attribute__((noinline)) void home_key_press(uint32_t k, int down)
{
    uint32_t p = key_place(k), f;
    if (!down && bz_kfn[k] != BZF_NONE) {        /* (0.12: a key of BLAZE's page let go: there, whatever page is up) */
        f = bz_kfn[k];
        bz_kfn[k] = BZF_NONE;
        bz_fn_up(f, k, (bz_ked >> k) & 1u);
        bz_ked &= ~(1u << k);
        return;
    }
    if (bz_slice)                                /* SLICE: the white keys play (seq.c), the black ones: the main loop's */
        return;
    if (bz_rb || (!down && ((bz_rb_down >> k) & 1u))) {   /* 0.9 REKORDBOX: notes out, nothing sounds (a key let go
                                                  * after the page was left: its note-off still) */
        uint32_t ch, note, b = 1u << k;
        if (key_black(k) && p == BZ_RB_DECK_KEY && bz_rb) {
            if (down)
                bz_rb_deck = (uint8_t)!bz_rb_deck;
            return;
        }
        if (down) {
            ch = key_black(k) ? BZ_RB_CH : BZ_RB_DECK_CH + (bz_rb_deck & 1u);
            note = key_black(k) ? 60u + p : 36u + p;
            bz_rb_kch[k] = (uint8_t)ch;
            bz_rb_down |= b;
            midi_out_event(0x09u | (0x90u | ch) << 8 | note << 16 | 127u << 24);
        } else if (bz_rb_down & b) {
            bz_rb_down &= ~b;
            note = key_black(k) ? 60u + p : 36u + p;
            midi_out_event(0x08u | (0x80u | bz_rb_kch[k]) << 8 | note << 16);
        }
        return;
    }
    if (bz_chord) {                              /* CHORD: a white key latches a chord (the black ones: the main loop's) */
        if (!key_black(k) && down && !(bz_shift_on && p == 12u))   /* (0.14 SHIFT + white 13: CAPTURE, the main loop's) */
            bz_chord_key(p);
        return;
    }
    if (bz_loops) {                              /* LOOPS: a white key a layer's change (the black ones: the main loop's,
                                                  * but THE ONE, as in BLAZE) */
        if (!key_black(k) && down && p < 16u)
            bz_loops_key(p);
        else if (key_black(k) && down && p == BZK_ONE)
            bz_org = ((bz_s16 + (clk_pos != CLK_START && clk_pos * 2u >= div_samples(2) ? 1u : 0u)) + 2u) & ~3u;
        return;
    }
    if (bz_keys)
        return;                                  /* (KEYS: the white keys play (seq.c), the black ones: the main loop's) */
    if (down) {
        f = bz_fn(k, bz_shift_on, bz_pads != 0u);
        bz_kfn[k] = (uint8_t)f;
        bz_ked &= ~(1u << k);
        bz_fn_down(f, k);
    }
}

/* 0.12: RAIL's action (CC 36's value) */
/* 0.14.1 BOOST: the lanes it plays louder and harder (eng_drum.c dv_boost): SNARE and CLAP */
#define BZ_BOOST_LANES (1u << 1 | 1u << 2)
static void bz_rail_act(uint32_t q)
{
    if (q == 1u || q == 2u)
        bz_build_start(q == 1u ? 4u : 8u);
    else if (q == 3u)
        bz_riser_tap();
    else if (q == 4u)
        bz_drop_tap();
    else if (q == 5u)
        bz_eo_tap();
    else if (q == 6u)
        bz_spin_tap();
    else if (q == 7u)
        bz_cap_tap();
    else if (q == 8u)
        bz_release_all();
    else if (q == 9u)
        bz_fill_tap();
    else if (q == 10u) {                         /* (SCATTER: a tap: latched on / off) */
        bz_fx_key(BZ_SCAT, 1);
        bz_fx_key(BZ_SCAT, 0);
    }
}

/* every block: n samples of real time, adv of the sequencer's (the build, the riser) */
static __attribute__((noinline)) void home_block(uint32_t n, uint32_t adv)
{
    uint32_t total, r, a;
    bz_now += n;
    if (bz_drop != BZD_IDLE)                     /* 0.11 DROP (before BACKSPIN: it may ask for it) */
        bz_drop_block(adv);
    bz_ev_block(n);                              /* 0.11 PHOSPHOR's events */
    if (bz_spin == BZB_ARMED) {                  /* 0.10 BACKSPIN: the beat: spin (then silent to the next 1) */
        if (bz_spin_left <= adv) {
            pf_spin_cue = 2;
            bz_spin = BZB_SPUN;
            if (song.playing) {                  /* (to the next bar's 1, at least the half beat of the spin) */
                bz_spin_left = bz_to_bar();
                if (bz_spin_left < beat_samples() / 2u)
                    bz_spin_left += 4u * beat_samples();
            } else
                bz_spin_left = beat_samples();
        } else
            bz_spin_left -= adv;
    } else if (bz_spin == BZB_SPUN) {
        if (bz_spin_left <= adv)
            bz_spin_off();
        else
            bz_spin_left -= adv;
    }
    {                                            /* 0.9 REKORDBOX: a knob moved, its CC (channel 13, CC 1..4) */
        uint32_t i, v;
        for (i = 0; i < 4u; i++) {
            v = bz_rb_val[i] & 127u;
            if (bz_rb && v != bz_rb_sent[i])
                midi_out_event(0x0Bu | (0xB0u | BZ_RB_CH) << 8 | (1u + i) << 16 | v << 24);
            bz_rb_sent[i] = (uint8_t)v;          /* (outside the page: kept, nothing sent) */
        }
    }
    if (bz_eo != BZE_IDLE && bz_eo != BZE_OUT) { /* ECHO OUT: its states on the sequencer's time */
        a = adv;
        while (bz_eo != BZE_IDLE && bz_eo != BZE_OUT && bz_eo_left <= a) {
            a -= bz_eo_left;
            if (bz_eo == BZE_ARMED) {            /* the bar: into the echo for a beat */
                perf_held |= PF_BIT(PF_ETHR);
                perf_ord[PF_ETHR] = ++perf_seq;
                bz_eo = BZE_ECHO;
                bz_eo_left = beat_samples();
            } else if (bz_eo == BZE_ECHO) {      /* the dry cut: the echo rings on */
                perf_held |= BZ_MUTES;
                bz_eo = BZE_TAIL;
                bz_eo_left = (uint32_t)bz_eo_bars * 4u * beat_samples();
            } else {                             /* the tail over */
                perf_held &= ~PF_BIT(PF_ETHR);
                if (!song.g[G_CLOCK]) {
                    transport_req = 2;           /* (the internal clock: stopped, the tracks back for the next PLAY) */
                    perf_held &= ~BZ_MUTES;
                    bz_eo = BZE_IDLE;
                } else
                    bz_eo = BZE_OUT;
            }
        }
        if (bz_eo != BZE_IDLE && bz_eo != BZE_OUT)
            bz_eo_left -= a;
    }
    if (bz_riser) {                              /* the riser: waits for its bar, ends on the 1 after its bars */
        a = adv;
        if (bz_rs_wait) {
            r = bz_rs_wait < a ? bz_rs_wait : a;
            bz_rs_wait -= r;
            a -= r;
        }
        total = (uint32_t)bz_riser * 4u * beat_samples();
        if (!bz_rs_wait && bz_rs_type == BZR_SYNTH) {   /* SYNTH: the track's note, bent up as it goes */
            if (!bz_rs_on) {
                track_t *t = &trk[bz_rs_trk % NTRK];
                bz_rs_note = (uint8_t)(48 + t->p[P_ROOT] % 12);
                trk_note_on(t, bz_rs_note, 40u + bz_rs_lvl * 87u / 127u);
                bz_rs_on = (uint8_t)(bz_rs_trk % NTRK + 1u);
            }
            {   /* Q8 semitones: TOP 0..127 -> 0..24 semitones, x^2 */
                uint32_t x = total ? (uint32_t)((uint64_t)bz_rs_age * 4096u / total) : 4096u;
                x = x > 4096u ? 4096u : x;
                midi_bend_target[bz_rs_on - 1u] = (int32_t)((uint64_t)bz_rs_top * 24u * 256u / 127u * x * x >> 24);
            }
        }
        bz_rs_age += a;
        if (bz_rs_age >= total) {
            bz_riser = 0;
            bz_rs_synth_off();
        }
    }
    if (bz_d_fxleft) {                           /* 0.12: DROP's throw on the 1, a beat */
        if (bz_d_fxleft <= n) {
            bz_d_fxleft = 0;
            perf_held &= ~PF_BIT(bz_d_fxbit);
        } else
            bz_d_fxleft -= n;
    }
    if (bz_tx_r != bz_tx_w) {                    /* 0.12: the main loop's MIDI out (one a block) */
        midi_out_event(bz_tx[bz_tx_r % 8u]);
        bz_tx_r = (uint8_t)(bz_tx_r + 1u);
    }
    if (bz_rk_req) {                             /* 0.13: the RATE knob (FM-1 knob 4, RAIL's CC 54) */
        uint32_t r = (bz_rk_req - 1u) % RL_NRATE;
        bz_rk_req = 0;
        bz_rk_pick(r);
    }
    if (bz_req) {                                /* 0.12: RAIL's action (channel 16 CC 36) */
        uint32_t q = bz_req;
        bz_req = 0;
        bz_rail_act(q);
    }
    if (!bz_build)
        return;
    if (bz_wait) {
        if (bz_wait > adv) {
            bz_wait -= adv;
            return;
        }
        adv -= bz_wait;
        bz_wait = 0;
        if (bz_b_fxon == BZBX_FILTER)            /* (FILTER: COLOR from where it is) */
            bz_b_k0 = perf_k[0];
    }
    if (!bz_age && bz_b_fxon == BZBX_FILTER)
        bz_b_k0 = perf_k[0];
    bz_build_more(adv);
}

/* the riser into the mix (fx.c mix_block, before the FX layer and the buses: COLOR filters it, SPACE of it goes to
 * the reverb). NOISE: white noise (left and right their own) through a band + high pass; AIR: the high pass only, from
 * higher up; SIREN: a narrow resonant band (a whistle); TONE: two detuned saws from the key's root up two octaves
 * through an opening low pass. The cutoff rises 150 Hz (AIR 600, TONE 300) -> 1.5 .. 6 kHz (TOP) as x^3 (slow, then
 * fast) while the level rises as x^2 to LEVEL (127: about -11 dBFS); cut at the end. SYNTH: none here (home_block) */
#define BZR_PEAK 9000
static const uint16_t BZR_SEMI[13] = {32768, 34716, 36781, 38968, 41285, 43740, 46341, 49097, 52016, 55109, 58386,
                                      61858, 65535};   /* 2^(i/12), Q15 (the last: 2, clipped) */
static __attribute__((noinline)) void bz_riser_mix(int32_t *l, int32_t *r, int32_t *rev, uint32_t n)
{
    uint32_t i, c, total, type = bz_rs_type, inc[2] = {0, 0};
    int32_t x, fc, f, amp, q, f0, top, peak = BZR_PEAK * bz_rs_lvl / 127, sp = bz_rs_space;
    if (!bz_riser || bz_rs_wait || type >= BZR_SYNTH)
        return;
    total = (uint32_t)bz_riser * 4u * beat_samples();
    x = total ? (int32_t)((uint64_t)bz_rs_age * 32768u / total) : 32767;
    x = x > 32767 ? 32767 : x;
    f0 = type == BZR_AIR ? 600 : type == BZR_TONE ? 300 : type == BZR_SIREN ? 200 : 150;
    top = 1500 + bz_rs_top * 4500 / 127;
    fc = f0 + (int32_t)(((((int64_t)(top - f0) * x) >> 15) * x >> 15) * x >> 15);   /* f0 + (top - f0) x^3 */
    f = (int32_t)((uint64_t)(uint32_t)fc * 205887u / FS);   /* 2 pi fc / FS, Q15 (unsigned: no signed 64-bit divide) */
    q = type == BZR_SIREN ? 6000 : type == BZR_AIR ? 30000 : type == BZR_TONE ? 16000 : 23000;   /* damping, Q15 */
    amp = (int32_t)((int64_t)x * x >> 15);
    if (type == BZR_TONE) {                      /* the saws: the root at C2 .. B2, up 24 semitones as x */
        const track_t *t = &trk[bz_ktrk % NTRK];  /* (the KEYS track's root: RAIL sets every note track's) */
        int32_t st = (int32_t)((int64_t)x * 24 >> 15), oct = st / 12, hz;
        int32_t note = 36 + t->p[P_ROOT] % 12;
        hz = 16744;                              /* C2, 65.406 Hz, in Q8 Hz */
        hz = (int32_t)((int64_t)hz * BZR_SEMI[(note - 36) % 12] >> 15);
        hz = (int32_t)((int64_t)hz * BZR_SEMI[st % 12] >> 15) << oct;
        inc[0] = (uint32_t)((uint64_t)hz * 16777216u / FS);   /* (Q8 Hz: 2^32 / 256 = 2^24) */
        inc[1] = inc[0] + inc[0] / 200u;          /* (+ ~9 cents) */
    }
    for (c = 0; c < 2u; c++) {
        int32_t lp = bzr.lp[c], bp = bzr.bp[c];
        uint32_t s = bzr.s[c], ph = bzr.ph[c];
        int32_t *o = c ? r : l;
        for (i = 0; i < n; i++) {
            int32_t in, hp, y;
            if (type == BZR_TONE) {
                ph += inc[c];
                in = (int32_t)(ph >> 19) - 4096;  /* a saw, +-4096 */
            } else {
                s ^= s << 13;
                s ^= s >> 17;
                s ^= s << 5;
                in = (int32_t)s >> 19;           /* +-4096 */
            }
            lp += (f * bp) >> 15;
            hp = in - lp - ((q * bp) >> 15);
            bp += (f * hp) >> 15;
            y = type == BZR_AIR ? hp >> 1 : type == BZR_SIREN ? bp >> 1 : type == BZR_TONE ? lp : bp + (hp >> 1);
            y = (int32_t)(((int64_t)y * amp) >> 14);
            y = clamp(y, -peak, peak);
            o[i] += y;
            rev[i] += (y * sp) >> 9;
        }
        bzr.lp[c] = lp;
        bzr.bp[c] = bp;
        bzr.s[c] = s;
        bzr.ph[c] = ph;
    }
}
