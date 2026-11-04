/* SPDX-License-Identifier: GPL-3.0-only
 * BLAZE (a DJ companion built on Felucca by Leo Kuroshita / Hügelton Instruments) */
/* BLAZE's map (ui_layer.c draw_layer; after blaze_roll_draw.c, whose cells it uses):
 *   a strip: the Camelot number in the Camelot wheel's colour and the key in note names ("8A  A MINOR"; no key:
 *     "NO KEY"), the phrase at the right: 16 bars, the past THEME, the bar now ACCENT filled to its beat, the rest
 *     LINE; its last bar: every one REC on the 1st and 3rd beats (the drop is next)
 *   6 rows of 4 in key order: the 13 white keys (the 4 pads in their lane colours, the 6 effects outlined ACCENT,
 *     BUILD 4 / 8 and IMPACT outlined REC), then the 11 black ones (the rates THEME, TAP NUDGE RELEASE THE ONE MID)
 *   lit (held, latched, running): filled. Cards: COLOR PITCH DECAY FX DEPTH */
#define BZD_Y0 23                                        /* the grid's first row (the strip above it) */
static const uint8_t BZD_H[6] = {14, 14, 14, 16, 16, 14};   /* rows with a rate: room for the "/" */

/* the Camelot wheel's colour of number n (1 teal, 2 green .. 4 orange, 5 pink .. 8 violet, 10 blue, 12 cyan) */
static uint16_t bz_cam_col(uint32_t n)
{
    int32_t h = (170 - 30 * ((int32_t)n - 1) + 720) % 360, f = h % 60, hi = ux.light ? 150 : 255, lo = ux.light ? 20 : 110;
    int32_t up = lo + (hi - lo) * f / 60, dn = hi - (hi - lo) * f / 60, r, g, b;
    if (ux.mono || (ux.raise == UI_BW_GREY && ux.dim == UI_BW_GREY))
        return T_TEXT;
    switch (h / 60) {
    case 0: r = hi; g = up; b = lo; break;
    case 1: r = dn; g = hi; b = lo; break;
    case 2: r = lo; g = hi; b = up; break;
    case 3: r = lo; g = dn; b = hi; break;
    case 4: r = up; g = lo; b = hi; break;
    default: r = hi; g = lo; b = dn; break;
    }
    return RGB(r, g, b);
}

static void bz_strip(void)
{
    char nm[16], n[4];
    uint32_t b, cam = bz_camelot(&b, nm, sizeof nm), beat, i;
    int32_t bar = bz_phrase(&beat), x;
    if (cam) {
        fmt_int(n, (int32_t)cam);
        str_cpy(n + str_len(n), b ? "B" : "A", sizeof n - str_len(n));
        x = 8 + text_w(&AF_M, n);
        cv_text_on(8, 2 + CAP_IN(M, 18), &AF_M, n, bz_cam_col(cam), T_SURF);
        cv_text_on(x + 6, 2 + CAP_IN(S, 18), &AF_S, nm, T_TEXT, T_SURF);
    } else {
        cv_text_on(8, 2 + CAP_IN(S, 18), &AF_S, nm[0] ? nm : "NO KEY", nm[0] ? T_TEXT : T_DIM, T_SURF);
    }
    for (i = 0; i < BZ_PHRASE; i++) {               /* 16 bars: 6 px, 1 apart, x 123 .. 233 */
        int32_t sx = 123 + 7 * (int32_t)i, y = 6, h = 10;
        uint16_t c = bar < 0 ? T_LINE : (int32_t)i < bar ? T_THEME : (int32_t)i == bar ? T_ACCENT : T_LINE;
        if (bar == (int32_t)BZ_PHRASE - 1 && !(beat & 1u) && (int32_t)i <= bar)
            c = T_REC;                              /* (the last bar: the drop is next) */
        if ((int32_t)i == bar && c != T_REC) {      /* (the bar now: filled to its beat) */
            cv_rect(sx, y, 6, h, T_LINE);
            h = (int32_t)(beat + 1u) * 10 / 4;
            y += 10 - h;
        }
        cv_rect(sx, y, 6, h, c);
    }
}

static void layer_blaze(void)
{
    track_t *t = rl_track();
    uint32_t i, fx = perf_kill ? 0u : perf_held | perf_latched, ok = perf_avail();
    int drum = drum_track(t), ext = song.g[G_CLOCK] != 0;
    int32_t y = BZD_Y0;
    bz_strip();
    for (i = 0; i < 24u; i++) {
        int32_t x = LC_X(i % 4u), h = BZD_H[i / 4u];
        if (i < 13u) {                                  /* the white keys 1..13 */
            if (i < BZ_FX0) {
                uint32_t pad = bz_pad_of(i);
                rl_cell(x, y, LC_W, h, pad < 16u ? rl_lane_name(t, pad % NLANE) : "-", rl_lane_col(pad % NLANE),
                        pad >= 16u || !drum ? RLC_DIM : (((uint32_t)rl_extra | rlu.fl) >> pad) & 1u ? RLC_FILL
                        : pad >= NLANE ? RLC_TINT : RLC_IDLE);
            } else if (i < BZ_FX0 + BZ_NFX) {
                uint32_t e = BZ_FX[i - BZ_FX0];
                rl_cell(x, y, LC_W, h, BZ_FX_NAME[i - BZ_FX0], T_ACCENT, !((ok >> e) & 1u) ? RLC_DIM
                        : (fx >> e) & 1u ? RLC_FILL : RLC_IDLE);
            } else {
                rl_cell(x, y, LC_W, h, i == BZ_B4 ? "BUILD 4" : i == BZ_B8 ? "BUILD 8" : "IMPACT", T_REC,
                        !drum ? RLC_DIM : (i == BZ_B4 && bz_build == 4u) || (i == BZ_B8 && bz_build == 8u) ||
                        (i == BZ_IMPACT && bz_thr) ? RLC_FILL : RLC_IDLE);
            }
        } else {                                        /* the black keys 1..11 */
            uint32_t p = i - 13u;
            if (p < 6u)
                rl_cell(x, y, LC_W, h, RL_RATE_CELL[p + 1u], T_THEME,
                        (rl_rates >> (p + 1u)) & 1u || (bz_build && bz_build_rate == p + 1u) ? RLC_FILL : RLC_IDLE);
            else
                rl_cell(x, y, LC_W, h, BZ_BLACK_NAME[p - 6u], T_MID,
                        (p == BZK_TAP || p == BZK_NDN || p == BZK_NUP) && ext ? RLC_DIM
                        : (p == BZK_NDN || p == BZK_NUP) && rl_nudging(p) ? RLC_FILL
                        : p == BZK_ONE && !song.playing ? RLC_DIM : RLC_IDLE);
        }
        if (i % 4u == 3u)
            y += h + 2;
    }
}

static void bz_cards(void)
{
    track_t *t = rl_track();
    char val[12];
    int32_t m = perf_k[0];
    fmt_int(val, m < 0 ? -m : m);
    draw_column(0, "COLOR", m ? val : "OFF", m < 0 ? "LP" : m > 0 ? "HP" : "", m ? VAL(0u) : T_DIM, (m + 100) * 5,
                ICON_CUTOFF);
    if (drum_track(t)) {
        fmt_int(val, t->p[P_E0 + 1] - 64);
        draw_column(1, "PITCH", val, "", VAL(1u), t->p[P_E0 + 1] * 1000 / 127, ICON_TUNE);
        fmt_int(val, t->p[P_E0 + 3] * 100 / 127);
        draw_column(2, "DECAY", val, "%", VAL(2u), t->p[P_E0 + 3] * 1000 / 127, ICON_DECAY);
    } else {
        draw_column(1, "PITCH", "-", "", T_DIM, -1, ICON_TUNE);
        draw_column(2, "DECAY", "-", "", T_DIM, -1, ICON_DECAY);
    }
    fmt_int(val, 100 - perf_k[3]);
    draw_column(3, "DEPTH", val, "%", VAL(3u), (100 - perf_k[3]) * 10, ICON_MIX);
}
