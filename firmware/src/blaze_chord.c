/* SPDX-License-Identifier: GPL-3.0-only
 * BLAZE (a DJ companion built on Felucca by Leo Kuroshita / Hügelton Instruments) */
/* CHORD, the sound side (the audio ISR; OCT- in the BLAZE layer: BLAZE -> KEYS -> CHORD). The white keys 1..7 are the
 * key's seven chords, stacked in thirds up the CHORD track's scale from each degree (A minor: Am Bdim C Dm Em F G);
 * white 8 OFF. A tap latches a chord (it plays on after the layer closes); another chord takes over; the same chord
 * again, OFF or RELEASE end it. How it plays (bz_ch_style, the black keys: blaze_home_ui.c): HOLD (held as long as it
 * is latched; the track's ARP, if on, plays it), stabs on 1/4 1/8 1/16 (each LENGTH % of its step), or GATE (1/16
 * steps in one of four patterns: ALGO). Stabs and the gate start on their own grid while the transport plays (at most a
 * beat away), at once stopped; the gate's pattern follows the phrase's 1/16 steps (bz_s16 from bz_org). 7TH adds the
 * seventh, INV turns the chord over (its lowest notes an octave up), OCT moves it. The notes go to the track as the
 * keys' do (input_on: its envelope, its ARP). */
#define BZC_N 4u                                /* notes in a chord at most (a seventh chord) */
enum { BZC_HOLD, BZC_R4, BZC_R8, BZC_R16, BZC_GATE, BZC_NSTYLE };
static const uint16_t BZC_GATES[4] = {0xFFFFu, 0xDDDDu, 0x5DBBu, 0x1449u};   /* STRAIGHT x.xx SHUFFLE TRESILLO (bit i:
                                                * 1/16 step i) */
static volatile uint8_t bz_chord;              /* main: the layer is CHORD */
static volatile uint8_t bz_ctrk = 2;           /* main: the CHORD track */
static volatile uint8_t bz_ch_style, bz_ch_7th, bz_ch_inv, bz_ch_gate;   /* main: how it plays */
static volatile int8_t bz_ch_oct;              /* main: its octave, -2..2 */
static volatile uint8_t bz_ch_len = 50;        /* main: a stab's / gate step's LENGTH, % of its step (5..100) */
static volatile uint8_t bz_ch_dirty;           /* main: how it plays changed (the ISR plays the chord again) */
static volatile uint8_t bz_ch_sel;             /* ISR: the chord latched (its degree + 1), 0 none */
static uint8_t bzc_note[BZC_N], bzc_n, bzc_on, bzc_trk;   /* ISR: the chord's notes; sounding now, on track bzc_trk */
static uint8_t bzc_vel = 100;
static uint32_t bzc_next, bzc_off;             /* ISR: samples to the next stab / gate step, to the sounding one's end */
static uint8_t bzc_step;                       /* ISR: the gate's step while stopped (playing: the phrase's) */

/* chord degree d on track t: up the scale in thirds from d (sev: with its seventh), turned over inv times, oct
 * octaves from C3; its notes, low to high, in out; returns how many */
static uint32_t bzc_build(const track_t *t, uint32_t d, uint8_t *out, uint32_t sev, uint32_t inv, int32_t oct)
{
    uint32_t mask = scale_mask(t), deg[12], n = 0, pc, k, m = sev ? 4u : 3u, i;
    int32_t base = 48 + t->p[P_ROOT] % 12 + 12 * oct, v[BZC_N];
    for (pc = 0; pc < 12u; pc++)
        if ((mask >> pc) & 1u)
            deg[n++] = pc;
    if (!n)
        return 0;
    for (k = 0; k < m; k++) {
        uint32_t j = d + 2u * k;
        v[k] = base + 12 * (int32_t)(j / n) + (int32_t)deg[j % n];
    }
    for (inv %= m; inv; inv--) {                 /* (the lowest an octave up, the rest down one place) */
        int32_t lo = v[0] + 12;
        for (i = 0; i + 1u < m; i++)
            v[i] = v[i + 1u];
        v[m - 1u] = lo;
    }
    for (k = 0; k < m; k++)
        out[k] = (uint8_t)clamp(v[k], 0, 127);
    return m;
}

static void bzc_sound(int on)
{
    uint32_t i;
    track_t *t = &trk[bzc_trk % NTRK];
    if (on == (int)bzc_on)
        return;
    for (i = 0; i < bzc_n; i++) {
        if (on)
            input_on(t, bzc_note[i], bzc_vel);
        else
            input_off(t, bzc_note[i]);
    }
    bzc_on = (uint8_t)on;
}

/* samples to the next step of `period` on the grid (0: now, or stopped) */
static uint32_t bzc_wait(uint32_t period)
{
    uint32_t s16 = div_samples(2), q = 4u * s16, g = period < q ? period : q, pos;
    if (!song.playing || clk_pos == CLK_START || !g)
        return 0;
    pos = ((clk_step % 4u) * s16 + clk_pos) % g;
    return pos ? g - pos : 0u;
}
static uint32_t bzc_period(void)
{
    static const uint8_t DIV[BZC_NSTYLE] = {0u, 0u, 1u, 2u, 2u};   /* (fx.c div_samples: 1/4 1/8 1/16, the gate 1/16) */
    return div_samples(DIV[bz_ch_style % BZC_NSTYLE]);
}

/* (again) the chord latched now, on the CHORD track */
static void bzc_start(void)
{
    bzc_sound(0);
    bzc_trk = (uint8_t)(bz_ctrk % NTRK);
    bzc_n = bz_ch_sel ? (uint8_t)bzc_build(&trk[bzc_trk], bz_ch_sel - 1u, bzc_note, bz_ch_7th, bz_ch_inv, bz_ch_oct) : 0u;
    bzc_off = 0;
    if (!bz_ch_sel || !bzc_n)
        return;
    if (bz_ch_style == BZC_HOLD)
        bzc_sound(1);
    else
        bzc_next = bzc_wait(bzc_period());
    bzc_step = 0;
}

static void bz_chord_stop(void)
{
    bz_ch_sel = 0;
    bzc_sound(0);
}

/* a white key of CHORD down (p: its place): a chord latched, again: off; white 8: off */
static void bz_chord_key(uint32_t p)
{
    if (p < 7u && bz_ch_sel != p + 1u) {
        bz_ch_sel = (uint8_t)(p + 1u);
        bzc_vel = (uint8_t)kb_vel;
        bzc_start();
    } else if (p < 8u) {
        bz_chord_stop();
    }
}

/* every block: adv samples of the sequencer's time (the stabs, the gate) */
static __attribute__((noinline)) void chord_block(uint32_t adv)
{
    uint32_t period, step;
    if (bz_ch_dirty) {
        bz_ch_dirty = 0;
        if (bz_ch_sel)
            bzc_start();
    }
    if (!bz_ch_sel || bz_ch_style == BZC_HOLD)
        return;
    if (bzc_on) {                                /* the stab / gate step ends after LENGTH % */
        if (bzc_off <= adv)
            bzc_sound(0);
        else
            bzc_off -= adv;
    }
    if (bzc_next >= adv) {
        bzc_next -= adv;
        return;
    }
    period = bzc_period();
    step = song.playing && clk_pos != CLK_START
         ? (bz_s16 - bz_org + (clk_pos * 2u >= div_samples(2) ? 1u : 0u)) % 16u : bzc_step++ % 16u;
    if (bz_ch_style != BZC_GATE || ((BZC_GATES[bz_ch_gate % 4u] >> step) & 1u)) {
        bzc_sound(0);
        bzc_sound(1);
        bzc_off = period * bz_ch_len / 100u;
    }
    bzc_next += period - adv;
}
