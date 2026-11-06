/* SPDX-License-Identifier: GPL-3.0-only
 * BLAZE (a DJ companion built on Felucca by Leo Kuroshita / Hügelton Instruments) */
/* SLICE, the sound side (the audio ISR; OCT- in the BLAZE layer: BLAZE -> KEYS -> CHORD -> LOOPS -> SLICE). The 16
 * white keys play the slices of a SLICE track's sample (Felucca's SLICE engine, eng_slice.c: BREAK, a user slot
 * USR1..3 loaded with tools/fm1_sample_upload.py, PIANO): white i plays slice i (+ START, + 16 on the 17..32 page).
 * QUANTIZE (1/16, 1/8, BEAT) holds a key's note-on and its note-off to the next point of that grid, counted from
 * the phrase's 1 (bz_org); stopped, or OFF: at once. The black keys and the knobs are the main loop's
 * (blaze_home_ui.c) */
static volatile uint8_t bz_strk;               /* main: SLICE's track */
static volatile uint8_t bz_sl_ok;              /* main: that track plays SLICE (else the white keys are silent) */
static volatile uint8_t bz_sl_page;            /* main: 0 slices 1..16, 1 slices 17..32 */
static volatile uint8_t bz_sl_q;               /* main: QUANTIZE 0 OFF, 1 1/16, 2 1/8, 3 BEAT */
static uint32_t bz_sl_on, bz_sl_off;           /* ISR: keys whose note-on / note-off wait for the point */
static uint32_t bz_sl_wait;                    /* ISR: samples to it */
static uint32_t kb_bzs;                        /* ISR: white keys playing slices (their release: here) */

static int bz_slice_trk(const track_t *t)
{
#if FELUCCA_SLICE
    return ENGINES[eng_idx(t->eng_req)] == &ENG_SLICE;
#else
    (void)t;
    return 0;
#endif
}

/* the slices of t's sound now (0: no material, or not SLICE) */
static uint32_t bz_slice_n(const track_t *t)
{
#if FELUCCA_SLICE
    const slc_src_t *s;
    if (!bz_slice_trk(t))
        return 0;
    s = slc_get(slc_src_of(t->p));
    return s ? slc_count(s, (uint32_t)clamp(t->p[P_E1], 0, SLC_DIV_MAN)) : 0u;
#else
    (void)t;
    return 0;
#endif
}

/* the note white key i plays (the engine: slice note - C4 - ROOT + START) */
static uint32_t bz_slice_note(const track_t *t, uint32_t i)
{
    return (uint32_t)clamp(60 + t->p[P_ROOT] + (int32_t)i + 16 * (int32_t)(bz_sl_page & 1u), 0, 127);
}

/* samples to the next QUANTIZE point (0: now, stopped, or OFF) */
static uint32_t bz_sl_point(void)
{
    static const uint8_t U[4] = {0u, 1u, 2u, 4u};
    uint32_t unit = U[bz_sl_q & 3u], in;
    if (!unit || !song.playing || clk_pos == CLK_START)
        return 0;
    in = (bz_s16 - bz_org) % unit;
    return in || clk_pos ? (unit - in) * div_samples(2) - clk_pos : 0u;
}

/* the point: the notes waiting start, then the releases (a key let go before its note started: its note-off waits
 * for the next point, so even a tap plays from the grid) */
static void bz_sl_fire(void)
{
    uint32_t k, m = bz_sl_on | bz_sl_off, b;
    for (k = 0; k < 27u && m; k++) {
        b = 1u << k;
        if (!(m & b))
            continue;
        m &= ~b;
        if ((bz_sl_off & b) && kb_chn[k]) {     /* (a note sounding: its key let go, or pressed again) */
            key_off(k, &trk[kb_trk[k] % NTRK]);
            bz_sl_off &= ~b;
        }
        if (bz_sl_on & b) {
            kb_raw = 1;
            key_on(k, &trk[kb_trk[k] % NTRK]);
            kb_raw = 0;
            bz_sl_on &= ~b;
            if (!((fm1_in.notes >> k) & 1u))
                bz_sl_off |= b;                  /* (already up: off at the next point) */
            else
                bz_sl_off &= ~b;
        }
    }
    if (bz_sl_on | bz_sl_off)
        bz_sl_wait = bz_sl_point();
}

/* a white key of SLICE down / up (keyboard_block): its note now or at the point */
static void bz_sl_key(uint32_t k, int down)
{
    uint32_t b = 1u << k;
    if (down)
        bz_sl_on |= b;
    else
        bz_sl_off |= b;
    bz_sl_wait = bz_sl_point();
    if (!bz_sl_wait)
        bz_sl_fire();
}

/* every block: adv samples of the sequencer's */
static __attribute__((noinline)) void slice_block(uint32_t adv)
{
    if (!(bz_sl_on | bz_sl_off))
        return;
    if (!bz_sl_q || !song.playing || bz_sl_wait <= adv)
        bz_sl_fire();
    else
        bz_sl_wait -= adv;
}
