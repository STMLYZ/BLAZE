/* SPDX-License-Identifier: GPL-3.0-only
 * BLAZE (a DJ companion built on Felucca by Leo Kuroshita / Hügelton Instruments) */
/* BLAZE 0.20 STACK, the audio side: a loop built layer by layer (DRUMS BASS CHORDS LEAD, a track each: blaze_stack_ui.c)
 * and arranged into sections, each layer OFF, A or B in each. A layer's A is its track's steps 0..L-1, its B the next
 * L (L = BARS x 16 steps of 1/16, B only at 1 or 2 bars): playing B swaps the two halves in place (stk_front: bit t, B in
 * front), so the sequencer, the project and the autosave see one pattern of LEN L as always. The arrangement is the
 * project's song (chain_config: rows of a slot per track, 0 A, 1 B, CHAIN_SILENT OFF; repeat = the section's loops),
 * marked STACK's (chain_config.rsv[0] = STK_MAGIC): so it is saved with the project. Played here, not by song_chain.c:
 * at each section's first 1/16 (stk_block, before seq_tick: its first step plays the new one) the tracks' mutes and
 * halves change. Its end stops the transport; the main loop puts the mutes and A back (stk_pass) */
#define STK_MAGIC 0xA7u
#define STK_ROWS 8u
static volatile uint8_t stk_run;               /* main starts, the ISR ends: the arrangement plays */
static volatile uint8_t stk_row, stk_n;        /* ISR: the section playing; main: how many */
static volatile uint8_t stk_endreq;            /* ISR -> main: the last section ended (put back) */
static volatile uint8_t stk_front;             /* main / ISR: bit t: track t's B in front */
static volatile uint32_t stk_left;             /* ISR: 1/16s left in the section */
static uint8_t stk_trk[4] = {3, 0, 1, 2};      /* main sets: the track of DRUMS BASS CHORDS LEAD */
static uint8_t stk_L = 16u;                    /* main sets: a loop's steps */
static struct { uint8_t v[4]; uint16_t n16; } stk_rows[STK_ROWS];   /* main sets before PLAY: each layer 0 A 1 B
                                                * CHAIN_SILENT OFF; the section's 1/16s */

/* track t's two halves exchanged (A <-> B in front). noinline: out of the ISR's loops */
static __attribute__((noinline)) void stk_swap(uint32_t t)
{
    uint32_t i, l = stk_L;
    step_t x, *s = trk[t % NTRK].step;
    if (2u * l > NSTEP)
        return;
    for (i = 0; i < l; i++) {
        x = s[i];
        s[i] = s[l + i];
        s[l + i] = x;
    }
    stk_front ^= (uint8_t)(1u << (t % NTRK));
}
/* section r's layers: muted (OFF) or A / B in front */
static __attribute__((noinline)) void stk_apply(uint32_t r)
{
    uint32_t i;
    for (i = 0; i < 4u; i++) {
        uint32_t t = stk_trk[i] % NTRK, v = stk_rows[r % STK_ROWS].v[i];
        if (v == CHAIN_SILENT) {
            if (!trk[t].p[P_MUTE])
                seq_release(&trk[t]);
            trk[t].p[P_MUTE] = 1;
            continue;
        }
        trk[t].p[P_MUTE] = 0;
        if (((stk_front >> t) & 1u) != (v == 1u))
            stk_swap(t);
    }
}
/* every block, before seq_tick (seq.c events): a 1/16 starting in it counts; the section's last gone, the next one */
static __attribute__((noinline)) void stk_block(uint32_t n)
{
    uint32_t p = div_samples(2);
    if (!song.playing || clk_pos == CLK_START || clk_pos + n < p)
        return;
    if (stk_left > 1u) {
        stk_left--;
        return;
    }
    if ((uint32_t)stk_row + 1u >= stk_n) {     /* (the last: the end) */
        stk_run = 0;
        stk_endreq = 1;
        transport_req = 2;
        return;
    }
    stk_row++;
    stk_apply(stk_row);
    stk_left = stk_rows[stk_row % STK_ROWS].n16 ? stk_rows[stk_row % STK_ROWS].n16 : 16u;
}
