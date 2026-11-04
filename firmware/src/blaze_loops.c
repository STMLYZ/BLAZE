/* SPDX-License-Identifier: GPL-3.0-only
 * BLAZE (a DJ companion built on Felucca by Leo Kuroshita / Hügelton Instruments) */
/* LOOPS, the sound side (the audio ISR; OCT- in the BLAZE layer: BLAZE -> KEYS -> CHORD -> LOOPS). The four tracks
 * as layers of the set, brought in and out on the beat:
 *   white 1..4   T1..T4 IN / OUT (its MUTE) at the next QUANTIZE point
 *   white 5..8   T1..T4 FADE in / out over FADE bars from that point (a fade out ends muted)
 *   white 9..12  T1..T4 FILL: for one bar from that point the other way round (a layer out plays a bar; one in drops)
 *   white 13..16 ALL IN, DRUMS ONLY (every other track out), BREAK (the DRUM tracks out), RESTART (every sequence
 *                from its step 1)
 * A key pressed again before its point takes its change back. QUANTIZE (the main loop's black keys): NOW, the next
 * BAR, the next 4 BARS, the next PHRASE (16 bars), counted from the phrase's 1 (bz_org); stopped: at once.
 * DUCK: the tracks that are not DRUM dip under each kick (eng_drum.c bz_kick) by AMOUNT and come back over RELEASE.
 * Fades and DUCK set the tracks' gains (perform.c bz_gain: the FX layer's mute ramp, 2.9 ms, smooths them) */
enum { BZL_NONE, BZL_IN, BZL_OUT, BZL_FIN, BZL_FOUT, BZL_FILL };
static volatile uint8_t bz_loops;              /* main: the layer is LOOPS */
static volatile uint8_t bz_lp_q = 1;           /* main: QUANTIZE: 0 NOW, 1 BAR, 2 4 BARS, 3 PHRASE */
static volatile uint8_t bz_lp_fbars = 4;       /* main: FADE's bars (2 4 8) */
static volatile uint8_t bz_duck;               /* main: DUCK's AMOUNT, 0 (off) .. 100 */
static volatile uint16_t bz_duck_ms = 150;     /* main: DUCK's RELEASE, ms */
static volatile uint8_t bz_lp_op[NTRK];        /* ISR: each track's change waiting for the point (BZL_*) */
static volatile uint8_t bz_lp_restart;         /* ISR: RESTART waiting */
static volatile uint8_t bz_lp_cancel;          /* main: CANCEL every change waiting */
static uint32_t bz_lp_wait;                    /* ISR: samples to the point (the changes waiting) */
static volatile int32_t bz_fd_gain[NTRK] = {32768, 32768, 32768, 32768};   /* ISR: each track's fade gain, Q15 */
static int32_t bz_fd_from[NTRK], bz_fd_to[NTRK];
static uint32_t bz_fd_len[NTRK], bz_fd_age[NTRK];   /* a fade running: its samples, those gone */
static uint8_t bz_fd_out;                      /* bit k: track k muted by a fade (its gain held at 0 while muted) */
static volatile uint32_t bz_fill_left[NTRK];   /* ISR: samples of the FILL left (0: none) */
static int32_t bz_duck_env;                    /* ISR: the dip now, Q15 */

static int bz_waiting(void)
{
    uint32_t k;
    for (k = 0; k < NTRK; k++)
        if (bz_lp_op[k])
            return 1;
    return bz_lp_restart;
}

/* samples to the next QUANTIZE point (0: now, or stopped) */
static uint32_t bz_lp_point(void)
{
    static const uint8_t BARS[4] = {0u, 1u, 4u, 16u};
    uint32_t unit = 16u * BARS[bz_lp_q & 3u], s16 = div_samples(2), in;
    if (!unit || !song.playing || clk_pos == CLK_START)
        return 0;
    in = (bz_s16 - bz_org) % unit;
    return in || clk_pos ? (unit - in) * s16 - clk_pos : 0u;
}

static void bz_fade(uint32_t k, int32_t from, int32_t to)
{
    bz_fd_from[k] = from;
    bz_fd_to[k] = to;
    bz_fd_len[k] = (uint32_t)bz_lp_fbars * 4u * beat_samples();
    bz_fd_age[k] = 0;
    bz_fd_gain[k] = from;
}

/* the point: every change waiting happens */
static void bz_lp_fire(void)
{
    uint32_t k;
    for (k = 0; k < NTRK; k++) {
        track_t *t = &trk[k];
        switch (bz_lp_op[k]) {
        case BZL_IN: t->p[P_MUTE] = 0; bz_fd_out &= (uint8_t)~(1u << k); break;
        case BZL_OUT: t->p[P_MUTE] = 1; break;
        case BZL_FIN: bz_fd_out &= (uint8_t)~(1u << k); bz_fade(k, 0, 32768); t->p[P_MUTE] = 0; break;
        case BZL_FOUT: bz_fade(k, bz_fd_gain[k], 0); break;
        case BZL_FILL: t->p[P_MUTE] = (int16_t)!t->p[P_MUTE]; bz_fill_left[k] = 4u * beat_samples(); break;
        default: break;
        }
        bz_lp_op[k] = BZL_NONE;
    }
    if (bz_lp_restart && song.playing)
        transport_req = 3;                       /* (every sequence from its step 1) */
    bz_lp_restart = 0;
}

/* a white key of LOOPS down (p: its place) */
static void bz_loops_key(uint32_t p)
{
    uint32_t k, d;
    if (p < 12u) {
        k = p % 4u;
        if (bz_lp_op[k]) {
            bz_lp_op[k] = BZL_NONE;              /* (again before the point: taken back) */
            return;
        }
        bz_lp_op[k] = (uint8_t)(p < 4u ? (trk[k].p[P_MUTE] ? BZL_IN : BZL_OUT)
                              : p < 8u ? (trk[k].p[P_MUTE] ? BZL_FIN : BZL_FOUT) : BZL_FILL);
        if (bz_lp_op[k] == BZL_FIN)
            bz_fd_gain[k] = 0;                   /* (muted now: from silence, so it starts at 0) */
    } else if (p < 15u) {                        /* ALL IN, DRUMS ONLY, BREAK */
        for (k = 0; k < NTRK; k++) {
            uint32_t m = trk[k].p[P_MUTE] != 0;
            d = (uint32_t)drum_track(&trk[k]);
            if (p == 12u ? m : p == 13u ? (d ? m : !m) : d && !m)
                bz_lp_op[k] = (uint8_t)(m ? BZL_IN : BZL_OUT);
        }
    } else if (p == 15u) {
        bz_lp_restart = (uint8_t)!bz_lp_restart;
    }
    bz_lp_wait = bz_lp_point();
    if (!bz_lp_wait)
        bz_lp_fire();
}

/* every block: n samples of real time (DUCK), adv of the sequencer's (the point, fades, fills) */
static __attribute__((noinline)) void loops_block(uint32_t n, uint32_t adv)
{
    uint32_t k;
    int32_t dg;
    if (bz_lp_cancel) {
        bz_lp_cancel = 0;
        for (k = 0; k < NTRK; k++)
            bz_lp_op[k] = BZL_NONE;
        bz_lp_restart = 0;
    }
    if (bz_waiting()) {
        if (bz_lp_wait <= adv)
            bz_lp_fire();
        else
            bz_lp_wait -= adv;
    }
    if (bz_kick) {                               /* DUCK: a kick dips the rest, back over RELEASE */
        bz_kick = 0;
        if (bz_duck)
            bz_duck_env = bz_duck * 245;         /* (100: 24500 of 32768, about -12 dB) */
    }
    if (bz_duck_env) {
        int32_t rel = (int32_t)bz_duck_ms * (FS / 1000), step = rel ? bz_duck * 245 * (int32_t)n / rel : 32768;
        bz_duck_env = bz_duck_env > step ? bz_duck_env - (step ? step : 1) : 0;
    }
    dg = 32768 - bz_duck_env;
    for (k = 0; k < NTRK; k++) {
        int32_t g;
        if (bz_fill_left[k]) {                   /* FILL: back the other way after its bar */
            if (bz_fill_left[k] <= adv) {
                bz_fill_left[k] = 0;
                trk[k].p[P_MUTE] = (int16_t)!trk[k].p[P_MUTE];
            } else
                bz_fill_left[k] -= adv;
        }
        if (bz_fd_len[k]) {                      /* a fade running */
            bz_fd_age[k] += adv;
            if (bz_fd_age[k] >= bz_fd_len[k]) {
                bz_fd_gain[k] = bz_fd_to[k];
                bz_fd_len[k] = 0;
                if (!bz_fd_to[k]) {              /* (faded out: muted, its gain held at 0 while it is) */
                    trk[k].p[P_MUTE] = 1;
                    bz_fd_out |= (uint8_t)(1u << k);
                }
            } else {
                bz_fd_gain[k] = bz_fd_from[k] + (int32_t)((int64_t)(bz_fd_to[k] - bz_fd_from[k]) * bz_fd_age[k] /
                                                          bz_fd_len[k]);
            }
        }
        if (((bz_fd_out >> k) & 1u) && !trk[k].p[P_MUTE]) {   /* (unmuted another way: as it is) */
            bz_fd_out &= (uint8_t)~(1u << k);
            bz_fd_gain[k] = 32768;
        }
        if (!bz_fd_len[k] && bz_fd_gain[k] == 0 && !((bz_fd_out >> k) & 1u) && bz_lp_op[k] != BZL_FIN)
            bz_fd_gain[k] = 32768;               /* (a fade in taken back: as it is) */
        g = bz_fd_gain[k];
        if (!drum_track(&trk[k]))
            g = (int32_t)((int64_t)g * dg >> 15);
        bz_gain[k] = g;
    }
}
