/* SPDX-License-Identifier: GPL-3.0-only
 * BLAZE (a DJ companion built on Felucca by Leo Kuroshita / Hügelton Instruments) */
/* BLAZE, the home layer's sound side (the audio ISR). ENV held (or double-tapped: locked) makes the keys BLAZE's
 * (ui_layer.c LAYER_BLAZE; seq.c keyboard_block hands them here, kb_bz):
 *   white 1..4   the 4 pads ROLL's PADS 4 picked (rl_live4, in key order), rolled by black 1..6 as in ROLL
 *   white 5..10  ECHO, REVERB (the throws), REVERSE, BRAKE (tape stop), FREEZE, STUTTER (a 1/16 repeat): a tap
 *                latches it on (a tap again: off), a hold plays it while held (BZ_TAP_MS tells them apart)
 *   white 11, 12 BUILD 4, BUILD 8: the one pad (ROLL's PADS 1: rl_live1, SNARE) from the next bar, 1/4 -> 1/8 ->
 *                1/16 -> 1/32 over 4 or 8 bars, swelling, so it ends on the next phrase's 1. Again: stops it
 *   white 13     IMPACT: KICK and the last lane (BELL, a kit's CYMBAL) accented, a beat of REVERB throw
 *   black 1..6   the rates 1/4 1/8 1/8 T 1/16 1/16 T 1/32 (ROLL's 2..7)
 *   black 7..9   TAP, NUDGE -, NUDGE + (the main loop's: blaze_home_ui.c)
 *   black 10     RELEASE: every latched effect, the build, the rolls off (the main loop: COLOR and DEPTH home too)
 *   black 11     THE ONE: now is the 1 (the phrase counter's bar 1 beat 1, to the nearest beat)
 * The phrase: 1/16 steps counted while the transport runs (bz_s16, seq.c click_tick), from the 1 THE ONE set (bz_org;
 * the transport's start until then). The key: MIDI CC 20 / 21 / 22 on channel 16 (RAIL: the Camelot number, A / B,
 * the root) noted here for the main loop (blaze_home_ui.c), whatever ROUT is. */
#define BZ_NFX 6u
static const uint8_t BZ_FX[BZ_NFX] = {PF_ETHR, PF_RTHR, PF_REV, PF_TAPE, PF_FRZ, PF_R16};
#define BZ_PAD0 0u                              /* white places: the pads 0..3, the effects 4..9, BUILD 4 / 8, IMPACT */
#define BZ_FX0 4u
#define BZ_B4 10u
#define BZ_B8 11u
#define BZ_IMPACT 12u
enum { BZK_TAP = 6u, BZK_NDN, BZK_NUP, BZK_REL, BZK_ONE };   /* black places 6..10 (0..5: the rates) */
#define BZ_TAP_MS 250u
#define BZ_PHRASE 16u                           /* bars in a phrase */

static volatile uint32_t home_mask;            /* main: the ENV button's bit while BLAZE may own the keys */
static volatile uint32_t bz_s16;               /* ISR: 1/16 steps since the transport started */
static volatile uint32_t bz_org;               /* ISR: the step THE ONE made bar 1 */
static volatile uint8_t bz_build;              /* ISR: a build running: its bars (4, 8), 0 none */
static volatile uint8_t bz_build_rate;         /* ISR: its rate now (RL index), the main loop shows it */
static volatile uint8_t bz_rel;                /* ISR sets: RELEASE pressed (the main loop homes the knobs) */
static volatile uint8_t bz_cc_seq, bz_cc[3];   /* ISR: CC 20 21 22 on channel 16 (seq bumped on each) */
static uint8_t bz_follow;                      /* main: a key came from RAIL (the scale LEDs on: ui_input.c) */
static uint16_t bz_pads;                       /* ISR: the pads the white keys 1..4 hold (pad bits) */
static uint8_t bz_tapfx;                       /* ISR: the effects pressed down as a tap could be (bit i of BZ_FX) */
static uint8_t bz_offfx;                       /* .. pressed to turn a latched one off (its release does nothing) */
static uint32_t bz_fxt[BZ_NFX];                /* .. when (bz_now) */
static uint32_t bz_now;                        /* ISR: samples, real time */
static uint32_t bz_wait, bz_age, bz_next;      /* the build: samples to its start, since it started, to its next hit */
static uint32_t bz_thr;                        /* IMPACT: samples left of its REVERB throw */

/* white key i (0..3): the pad of ROLL's 4 it plays (in key order), 16 none */
static uint32_t bz_pad_of(uint32_t i)
{
    uint32_t p, n = 0, m = rl_live4;
    for (p = 0; p < 16u; p++)
        if (((m >> p) & 1u) && n++ == i)
            return p;
    return 16u;
}

/* the rate the build plays age samples in, of bars bars (1/4 1/8 1/16 1/32: a quarter of it each) */
static uint32_t bz_build_at(uint32_t age, uint32_t bars)
{
    static const uint8_t R[4] = {1u, 2u, 4u, 6u};
    uint32_t q = bars * 4u * beat_samples() / 4u, s = q ? age / q : 0u;
    return R[s < 4u ? s : 3u];
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

static void bz_build_start(uint32_t bars)
{
    if (bz_build == bars) {
        bz_build = 0;                            /* (again: stopped) */
        return;
    }
    bz_wait = bz_to_bar();
    bz_age = 0;
    bz_next = 0;
    bz_build_rate = (uint8_t)bz_build_at(0, bars);
    bz_build = (uint8_t)bars;
}

static void bz_release_all(void)
{
    uint32_t i;
    for (i = 0; i < BZ_NFX; i++)
        perf_held &= ~PF_BIT(BZ_FX[i]);
    perf_latched = 0;
    bz_tapfx = bz_offfx = 0;
    bz_build = 0;
    bz_thr = 0;
    bz_pads = 0;
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

/* a key of the BLAZE layer down / up (k: 0 = F3 .. 26 = G5). noinline: out of keyboard_block's loop */
static __attribute__((noinline)) void home_key_press(uint32_t k, int down)
{
    uint32_t p = key_place(k), pad;
    if (key_black(k)) {
        if (p < 6u)
            rl_rate(p + 1u, down);               /* (ROLL's rates 1/4 .. 1/32) */
        else if (p == BZK_REL && down)
            bz_release_all();
        else if (p == BZK_ONE && down)           /* the nearest step's beat is the 1 */
            bz_org = ((bz_s16 + (clk_pos != CLK_START && clk_pos * 2u >= div_samples(2) ? 1u : 0u)) + 2u) & ~3u;
        return;                                  /* (TAP, NUDGE: the main loop's) */
    }
    if (p < BZ_FX0) {
        pad = bz_pad_of(p);
        if (pad >= 16u)
            return;
        if (!down) {
            bz_pads &= (uint16_t)~(1u << p);
            rl_extra &= (uint16_t)~(1u << pad);
            rl_rerun();
            return;
        }
        bz_pads |= (uint16_t)(1u << p);
        rl_vel[pad] = (uint8_t)kb_vel;
        rl_extra |= (uint16_t)(1u << pad);
        if (!rl_nr)
            rl_hit(pad, rl_vel[pad]);
        else
            rl_rerun();
    } else if (p < BZ_FX0 + BZ_NFX) {
        bz_fx_key(p - BZ_FX0, down);
    } else if (!down) {
        return;
    } else if (p == BZ_B4 || p == BZ_B8) {
        bz_build_start(p == BZ_B4 ? 4u : 8u);
    } else if (p == BZ_IMPACT) {
        rl_hit(0u, 127u);
        rl_hit(NLANE - 1u, 127u);
        perf_held |= PF_BIT(PF_RTHR);
        perf_ord[PF_RTHR] = ++perf_seq;
        bz_thr = beat_samples();
    }
}

/* every block: n samples of real time, adv of the sequencer's (the build) */
static __attribute__((noinline)) void home_block(uint32_t n, uint32_t adv)
{
    uint32_t pad, total, r;
    bz_now += n;
    if (bz_thr) {
        if (bz_thr <= n) {
            bz_thr = 0;
            perf_held &= ~PF_BIT(PF_RTHR);
        } else
            bz_thr -= n;
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
    }
    total = (uint32_t)bz_build * 4u * beat_samples();
    pad = 16u;
    for (r = 0; r < 16u; r++)
        if ((rl_live1 >> r) & 1u) {
            pad = r;
            break;
        }
    while (bz_next < adv) {                      /* (hits due in this block: on their own grid) */
        uint32_t at = bz_age + bz_next;
        if (at >= total || pad >= 16u) {
            bz_build = 0;
            return;
        }
        r = bz_build_at(at, bz_build);
        bz_build_rate = (uint8_t)r;
        rl_hit(pad, 40u + 87u * at / (total ? total : 1u));
        bz_next += div_samples(RL_DIV[r]) ? div_samples(RL_DIV[r]) : 1u;
    }
    bz_next -= adv;
    bz_age += adv;
    if (bz_age >= total)
        bz_build = 0;                            /* (its last bar over: the drop) */
}
