/* SPDX-License-Identifier: GPL-3.0-only
 * BLAZE (a DJ companion built on Felucca by Leo Kuroshita / Hügelton Instruments) */
/* 0.11 LOOP CAPTURE, the sound side (the audio ISR: fx.c mix_block calls bz_cap_mix after the buses, before the FX
 * layer's master, so COLOR and the rest still work on the loop). SHIFT + white 11 in BLAZE: from the next bar (stopped:
 * now) LENGTH bars (1, 2, 4; SHIFT + white 12) of the whole mix, the echo's and reverb's sends in it, are recorded; at
 * their end the loop plays in place of the tracks (SOLO: change the pattern underneath, nobody hears it); tapped
 * again, from the next bar the tracks come back over it (MIX: the loop is a second deck); tapped again, from the next
 * bar the loop glides out. RELEASE: out at once.
 * Memory: the echo's line, dly_buf (fx.c, 128 KiB), holds the loop as 22.05 kHz mono IMA ADPCM, two codes a byte:
 * 262144 codes, 11.9 s, 4 bars down to 81 BPM (slower: 2 bars). So no echo (ECHO, ECHO OUT's tail, the tracks' DELAY)
 * from the capture's start until the loop is gone (fx_dly_off); the main loop then clears the line (blaze_home_ui.c),
 * so the echo starts again from silence. SHIFT + white 16 (stopped, a loop held) keeps it in a USR slot
 * (blaze_cap_save.c): SAMPLE and SLICE play it from there. Events: PHOSPHOR's CC 45 (blaze_home.c) */
enum { BZC_IDLE, BZC_ARMED, BZC_REC, BZC_SOLO, BZC_MIX, BZC_FADE, BZC_CLEAR, BZC_SAVE };
#define BZC_MAX (DLY_LEN * 4u)                  /* codes the line holds (2 bytes a sample, 2 codes a byte) */
#define BZC_IDX0 32                             /* the ADPCM step index the loop starts from (a kick's attack: not
                                                 * from the quietest step) */
static volatile uint8_t bz_cap;                /* ISR (main: CLEAR -> IDLE, SAVE in and out) */
static volatile uint8_t bz_cap_pend;           /* ISR: the state at the next bar (0: none) */
static volatile uint8_t bz_cap_bars = 4;       /* main: LENGTH 1, 2, 4 bars */
static volatile uint8_t bz_cap_got;            /* ISR: the bars the loop holds (LENGTH, or fewer: too slow) */
static volatile uint32_t bz_cap_len;           /* ISR: the loop's codes (22.05 kHz samples) */
static volatile uint32_t bz_cap_i;             /* ISR: recording: codes written; playing: the next code read */
static uint32_t bz_cap_cd;                     /* ISR: samples to the bar (ARMED, a pending state), 0: none */
static int32_t bz_cap_pred, bz_cap_idx;        /* ISR: the ADPCM state (the encoder's, then the decoder's) */
static int32_t bz_cap_half;                    /* .. recording: the first of a pair; playing: the code before */
static int32_t bz_cap_cur;                     /* .. playing: the code now */
static uint8_t bz_cap_ph;                      /* .. which of the pair (44.1 kHz samples: two a code) */
static uint8_t bz_cap_run;                     /* .. the transport ran (a start: the loop from its 1 with it) */
static int32_t bz_cap_gl = 256, bz_cap_gp;     /* .. the tracks' gain, the loop's, Q8: gliding (256 samples: 6 ms) */

/* the ADPCM decoder's step (eng_sample.c sample_next): the state moved by code, the new sample */
static inline int32_t bz_ima_dec(uint32_t code)
{
    int32_t step = IMA_STEP[bz_cap_idx], vd = step >> 3;
    if (code & 4u)
        vd += step;
    if (code & 2u)
        vd += step >> 1;
    if (code & 1u)
        vd += step >> 2;
    bz_cap_pred = clamp(bz_cap_pred + ((code & 8u) ? -vd : vd), -32768, 32767);
    bz_cap_idx = clamp(bz_cap_idx + IMA_IDX[code & 7u], 0, 88);
    return bz_cap_pred;
}
/* the encoder: the code nearest x from the state now (then decoded: the state as the decoder will have it) */
static inline uint32_t bz_ima_enc(int32_t x)
{
    int32_t step = IMA_STEP[bz_cap_idx], d = x - bz_cap_pred;
    uint32_t code = 0;
    if (d < 0) {
        code = 8u;
        d = -d;
    }
    if (d >= step) {
        code |= 4u;
        d -= step;
    }
    if (d >= step >> 1) {
        code |= 2u;
        d -= step >> 1;
    }
    if (d >= step >> 2)
        code |= 1u;
    bz_ima_dec(code);
    return code;
}

/* the bars that fit the line at the tempo now (LENGTH, halved until they do) */
static uint32_t bz_cap_fit(uint32_t bars)
{
    while (bars > 1u && bars * 8u * div_samples(2) > BZC_MAX)
        bars >>= 1;
    return bars;
}
static void bz_cap_rewind(void)                 /* playing: from the loop's 1 */
{
    bz_cap_i = 0;
    bz_cap_pred = 0;
    bz_cap_idx = BZC_IDX0;
    bz_cap_ph = 0;
    bz_cap_cur = bz_cap_half = 0;
}
/* the bar came (or now): ARMED records, a pending state takes over */
static void bz_cap_go(void)
{
    if (bz_cap == BZC_ARMED) {
        bz_cap_got = (uint8_t)bz_cap_fit(bz_cap_bars);
        bz_cap_len = (uint32_t)bz_cap_got * 8u * div_samples(2);   /* (16 steps a bar, a code each 2 samples) */
        bz_cap_rewind();
        fx_dly_off = 1;                          /* (the line is the loop's from now) */
        bz_cap = BZC_REC;
    } else if (bz_cap_pend) {
        bz_cap = bz_cap_pend;
        bz_cap_pend = 0;
    }
}
/* samples to the next bar, at least 1 (stopped: 1, now) */
static uint32_t bz_cap_bar(void)
{
    uint32_t w = bz_to_bar();
    return w ? w : 1u;
}
/* SHIFT + white 11: the capture's next step (see the top) */
static void bz_cap_tap(void)
{
    switch (bz_cap) {
    case BZC_IDLE:
        bz_cap_cd = bz_cap_bar();
        bz_cap = BZC_ARMED;
        break;
    case BZC_ARMED:                              /* (not started: as if never) */
        bz_cap_cd = 0;
        bz_cap = BZC_IDLE;
        break;
    case BZC_REC:                                /* (stopped half way: the line cleared) */
        bz_cap_cd = 0;
        bz_cap = BZC_CLEAR;
        break;
    case BZC_SOLO:
    case BZC_MIX:
        if (bz_cap_pend == BZC_FADE) {           /* (out pending, tapped again: out now) */
            bz_cap_pend = 0;
            bz_cap_cd = 0;
            bz_cap = BZC_FADE;
        } else {
            bz_cap_pend = bz_cap == BZC_SOLO && bz_cap_pend != BZC_MIX ? BZC_MIX : BZC_FADE;
            bz_cap_cd = bz_cap_bar();
        }
        break;
    default:
        break;
    }
}
/* RELEASE: the loop out now (recording: dropped) */
static void bz_cap_off(void)
{
    bz_cap_cd = 0;
    bz_cap_pend = 0;
    if (bz_cap == BZC_ARMED)
        bz_cap = BZC_IDLE;
    else if (bz_cap == BZC_REC)
        bz_cap = BZC_CLEAR;
    else if (bz_cap == BZC_SOLO || bz_cap == BZC_MIX)
        bz_cap = BZC_FADE;
}

/* every block, after the buses: l r the dry mix, w the wet (echo + reverb + chorus, both sides). Recording: the mono
 * sum, half (headroom), averaged by pairs to 22.05 kHz and encoded; playing: decoded, the pair's first sample between
 * the codes (linear), into l r (doubled back), the tracks' l r w at their gain */
static __attribute__((noinline)) void bz_cap_mix(int32_t *l, int32_t *r, int32_t *w, uint32_t n)
{
    uint8_t *b = (uint8_t *)dly_buf;
    uint32_t i, st;
    int32_t x, y, tl, tp, gl = bz_cap_gl, gp = bz_cap_gp;
    if (bz_cap == BZC_IDLE || bz_cap == BZC_CLEAR || bz_cap == BZC_SAVE) {
        bz_cap_run = (uint8_t)(song.playing != 0);
        bz_cap_gl = 256;
        bz_cap_gp = 0;
        return;
    }
    if (song.playing && !bz_cap_run && (bz_cap == BZC_SOLO || bz_cap == BZC_MIX))
        bz_cap_rewind();                         /* (PLAY: the loop from its 1 with the transport's) */
    bz_cap_run = (uint8_t)(song.playing != 0);
    for (i = 0; i < n; i++) {
        if (bz_cap_cd && !--bz_cap_cd)
            bz_cap_go();
        st = bz_cap;
        y = 0;
        if (st == BZC_REC) {
            x = clamp((l[i] + r[i] + 2 * w[i]) >> 2, -32768, 32767);
            if (!bz_cap_ph)
                bz_cap_half = x;
            else {
                uint32_t c = bz_ima_enc((bz_cap_half + x) >> 1), j = bz_cap_i++;
                if (j & 1u)
                    b[j >> 1] = (uint8_t)(b[j >> 1] | c << 4);
                else
                    b[j >> 1] = (uint8_t)c;
                if (bz_cap_i >= bz_cap_len) {    /* (the bars full: the loop plays at once, from its 1) */
                    bz_cap_rewind();
                    bz_cap = BZC_SOLO;
                    bz_cap_ph = 1;               /* (the next sample starts a pair) */
                }
            }
            bz_cap_ph ^= 1u;
        } else if (st == BZC_SOLO || st == BZC_MIX || st == BZC_FADE) {
            if (!bz_cap_ph) {
                uint32_t j = bz_cap_i;
                bz_cap_half = bz_cap_cur;
                bz_cap_cur = bz_ima_dec((b[j >> 1] >> ((j & 1u) << 2)) & 15u);
                y = (bz_cap_half + bz_cap_cur) >> 1;
                if (++bz_cap_i >= bz_cap_len) {
                    bz_cap_i = 0;                /* (the loop's 1 again: the decoder from its first state) */
                    bz_cap_pred = 0;
                    bz_cap_idx = BZC_IDX0;
                }
            } else
                y = bz_cap_cur;
            bz_cap_ph ^= 1u;
        }
        tl = st == BZC_SOLO ? 0 : 256;
        tp = st == BZC_SOLO || st == BZC_MIX ? 256 : 0;
        gl += gl < tl ? 1 : gl > tl ? -1 : 0;
        gp += gp < tp ? 1 : gp > tp ? -1 : 0;
        if (st == BZC_FADE && !gp) {
            bz_cap = BZC_CLEAR;                  /* (out: the main loop clears the line, the echo is back) */
            bz_cap_pend = 0;
        }
        y = (y * gp) >> 7;                       /* (doubled back) */
        l[i] = ((l[i] * gl) >> 8) + y;
        r[i] = ((r[i] * gl) >> 8) + y;
        w[i] = (w[i] * gl) >> 8;
    }
    bz_cap_gl = gl;
    bz_cap_gp = gp;
}
