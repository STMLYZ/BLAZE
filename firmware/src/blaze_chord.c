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
/* 0.12 the generator (CHORD's white 9..16): AUTO plays one of the key's common progressions (minor keys their own,
 * major theirs), a chord every 1, 2 or 4 bars counted from the phrase's 1, as if its white keys were tapped (so the
 * styles, 7TH, INVERT, OCT and a blend apply); a chord tapped by hand takes over (AUTO off). MELODY plays a 2-bar line
 * on the KEYS track, on the 1/16 grid, leaning on the notes of the chord playing; a new line each 2 bars unless KEEP
 * holds the one playing; NEW: a new one at once. DENSITY (how many 1/16s play) and RANGE (1 or 2 octaves): hold
 * MELODY + KNOB 1 / 2 */
#define BZG_REST (-128)
static volatile uint8_t bz_g_auto, bz_g_prog, bz_g_every = 1;   /* main: AUTO, which progression, bars a chord */
static volatile uint8_t bz_g_mel, bz_g_keep, bz_g_new;          /* main: MELODY, KEEP, NEW asked */
static volatile uint8_t bz_g_dens = 55, bz_g_range = 1;         /* main: DENSITY %, RANGE octaves (1, 2) */
static volatile uint8_t bz_g_idx;              /* ISR: the progression's chord now (0..3), the main loop shows it */
static int8_t bzg_pat[32];                     /* ISR: the line: a step's scale steps above the chord's root (REST) */
static uint8_t bzg_len[32];                    /* .. its 1/16s held */
static uint32_t bzg_rng = 0x6D2B79F5u, bzg_t;  /* .. its dice; stopped: the samples counted for AUTO */
static uint32_t bzg_next, bzg_off;             /* .. samples to the next 1/16, to the note's end */
static uint8_t bzg_note, bzg_on, bzg_trk, bzg_ctr, bzg_made, bzg_was;
static const uint8_t BZG_MIN[5][4] = {{0, 5, 2, 6}, {0, 3, 5, 4}, {0, 6, 5, 6}, {0, 5, 3, 4}, {0, 2, 6, 5}};
static const uint8_t BZG_MAJ[5][4] = {{0, 4, 5, 3}, {5, 3, 0, 4}, {0, 3, 4, 3}, {0, 5, 3, 4}, {0, 3, 0, 4}};
#define BZG_NPROG 5u

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

/* chord degree d on track t keeps to the blend's notes (no blend: every chord does) */
static int bzc_fits(const track_t *t, uint32_t d)
{
    uint8_t v[BZC_N];
    uint32_t n, i, bl = bz_blend;
    if (!bl)
        return 1;
    n = bzc_build(t, d, v, bz_ch_7th, 0, 0);
    for (i = 0; i < n; i++)
        if (!((bl >> (v[i] % 12u)) & 1u))
            return 0;
    return 1;
}

/* a white key of CHORD down (p: its place): a chord latched, again: off; white 8: off. In a blend a chord with a note
 * the next key has not is refused */
static void bz_chord_key(uint32_t p)
{
    if (p < 8u)
        bz_g_auto = 0;                           /* (0.12: by hand: AUTO lets go) */
    if (p < 7u && !bzc_fits(&trk[bz_ctrk % NTRK], p))
        return;
    if (p < 7u && bz_ch_sel != p + 1u) {
        bz_ch_sel = (uint8_t)(p + 1u);
        bzc_vel = (uint8_t)kb_vel;
        bzc_start();
    } else if (p < 8u) {
        bz_chord_stop();
    }
}

static uint32_t bzg_rand(void)
{
    bzg_rng ^= bzg_rng << 13;
    bzg_rng ^= bzg_rng >> 17;
    bzg_rng ^= bzg_rng << 5;
    return bzg_rng;
}
/* a new 2-bar line: a walk over the scale (1 or 2 octaves), most notes on the chord's (its root, 3rd, 5th, octave),
 * downbeats likelier, now and then a note held over a 1/16 */
static void bzg_make(void)
{
    static const int8_t CT[4] = {0, 2, 4, 7};
    uint32_t i, top = 7u * (bz_g_range > 1u ? 2u : 1u), dens = bz_g_dens > 100u ? 100u : bz_g_dens;
    int32_t cur = (int32_t)CT[bzg_rand() % 3u];
    for (i = 0; i < 32u; i++) {
        uint32_t want = (i % 4u == 0u) ? dens + (100u - dens) / 2u : (i % 2u == 0u) ? dens : dens * 2u / 3u;
        bzg_len[i] = 1;
        if (bzg_rand() % 100u >= want) {
            bzg_pat[i] = BZG_REST;
            continue;
        }
        if (bzg_rand() % 100u < 60u)             /* (a chord note near where it is) */
            cur = CT[bzg_rand() % 4u] + (cur >= 7 && top > 7u ? 7 : 0);
        else
            cur += (int32_t)(bzg_rand() % 5u) - 2;
        cur = clamp(cur, 0, (int32_t)top);
        bzg_pat[i] = (int8_t)cur;
        if (i + 1u < 32u && bzg_rand() % 100u < 20u) {
            bzg_len[i] = 2;
            bzg_pat[++i] = BZG_REST;
        }
    }
    bzg_made++;
}
static void bzg_sound_off(void)
{
    if (bzg_on) {
        input_off(&trk[bzg_trk % NTRK], bzg_note);
        bzg_on = 0;
    }
}
/* AUTO: the progression's chord for now (playing: by the phrase's bars; stopped: by the time counted) */
static void bzg_auto(uint32_t adv)
{
    const track_t *t = &trk[bz_ctrk % NTRK];
    uint32_t every = bz_g_every ? bz_g_every : 1u, bar, idx, d;
    const uint8_t *pr = ((scale_mask(t) >> 4) & 1u) ? BZG_MAJ[bz_g_prog % BZG_NPROG] : BZG_MIN[bz_g_prog % BZG_NPROG];
    if (song.playing && clk_pos != CLK_START)
        bar = (bz_s16 - bz_org) / 16u;
    else {
        bzg_t += adv;
        bar = bzg_t / (4u * beat_samples() ? 4u * beat_samples() : 1u);
    }
    idx = (bar / every) % 4u;
    d = pr[idx];
    bz_g_idx = (uint8_t)idx;
    if (bz_ch_sel != d + 1u && bzc_fits(t, d)) {
        bz_ch_sel = (uint8_t)(d + 1u);
        bzc_start();
    }
}
/* MELODY, every block: its 1/16s on the grid (from the phrase's 1), the KEYS track */
static __attribute__((noinline)) void bzg_block(uint32_t adv)
{
    uint32_t period = div_samples(2), step;
    int32_t v;
    if (bzg_on) {
        if (bzg_off <= adv)
            bzg_sound_off();
        else
            bzg_off -= adv;
    }
    if (!bz_g_mel) {
        bzg_was = 0;
        return;
    }
    if (!bzg_was || bz_g_new) {                  /* (MELODY on, NEW: a new line, from the next 1/16) */
        bz_g_new = 0;
        bzg_make();
        if (!bzg_was)
            bzg_next = bzc_wait(period);
        bzg_was = 1;
    }
    if (bzg_next >= adv) {
        bzg_next -= adv;
        return;
    }
    step = song.playing && clk_pos != CLK_START
         ? (bz_s16 - bz_org + (clk_pos * 2u >= period ? 1u : 0u)) % 32u : bzg_ctr++ % 32u;
    if (step == 0u && !bz_g_keep && bzg_made)
        bzg_make();                              /* (each 2 bars a new one, unless KEEP) */
    v = bzg_pat[step];
    if (v != BZG_REST) {
        const track_t *kt = &trk[bz_ktrk % NTRK];
        uint32_t d = bz_ch_sel ? bz_ch_sel - 1u : 0u;
        bzg_sound_off();
        bzg_trk = (uint8_t)(bz_ktrk % NTRK);
        bzg_note = (uint8_t)bz_key_note(kt, 7u + d + (uint32_t)v);
        input_on(&trk[bzg_trk], bzg_note, step % 4u == 0u ? 110u : 84u);
        bzg_on = 1;
        bzg_off = period * bzg_len[step] * 4u / 5u;
    }
    bzg_next += period - adv;
}

/* every block: adv samples of the sequencer's time (the stabs, the gate) */
static __attribute__((noinline)) void chord_block(uint32_t adv)
{
    uint32_t period, step;
    if (bz_g_auto)
        bzg_auto(adv);                           /* (0.12 AUTO) */
    if (bz_g_mel || bzg_on || bzg_was)
        bzg_block(adv);                          /* (0.12 MELODY) */
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
