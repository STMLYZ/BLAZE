/* SPDX-License-Identifier: GPL-3.0-only
 * BLAZE (a DJ companion built on Felucca by Leo Kuroshita / Hügelton Instruments) */
/* BLAZE's map (ui_layer.c draw_layer; after blaze_roll_draw.c, whose cells it uses):
 *   a strip: the Camelot number in the Camelot wheel's colour and the key in note names ("8A  A MINOR"; no key:
 *     "NO KEY"), the phrase at the right: 16 bars, the past THEME, the bar now ACCENT filled to its beat, the rest
 *     LINE; its last bar: every one REC on the 1st and 3rd beats (the drop is next)
 *   (BLAZE itself: the key in the header, the phrase a 4 px bar across the top, 16 bars)
 *   7 rows of 4 in key order: the 14 white keys (the 4 pads in their lane colours, the 6 effects outlined ACCENT,
 *     BUILD 4 / 8, RISER and EXIT (ECHO OUT) outlined REC), then the 11 black ones (the rates THEME, TAP NUDGE RELEASE THE
 *     ONE MID). RISER held: cards TYPE TOP LEVEL SPACE (SYNTH: TRACK); ECHO OUT held: BARS
 *   lit (held, latched, running): filled. Cards: COLOR PITCH DECAY DEPTH; SHIFT: TONE SNAP DRIVE THROW; a pad held:
 *   TUNE DECAY LEVEL of that drum
 * KEYS: the strip, then the 16 white keys as 2 rows of 8 (the note each plays; the root's tinted the Camelot colour,
 *   held: filled), then the black keys as 3 rows of 4: T1..T4 (the KEYS track: filled; no notes: dim), ARP LATCH,
 *   1/8 1/16 1/32 (the arp's rate), OCT- OCT+. Cards: FILTER GATE RANGE SPACE
 * CHORD: the strip, the white keys 1..8 as 2 rows of 4 (the chord each latches by name, "Am" "G7"; the latched one
 *   filled; 8: OFF), the black keys as 3 rows of 4: HOLD 1/4 1/8 1/16 GATE (how it plays: filled), 7TH INVERT
 *   OCT- OCT+ TRACK ARP. Cards: FILTER LENGTH SPACE LEVEL */
#define BZD_Y0 23                                        /* the grid's first row (the strip above it) */
#define BZD_Y7 6                                         /* the white keys' first row (the phrase bar above it) */
#define BZD_YB 70                                        /* the black keys' first row (BLAZE, KEYS, LOOPS) */

/* the Camelot number n's colour, RAIL's and PHOSPHOR's (hue 140 + 30 (n - 1): 1 green, 4 blue, 7 magenta, 8 rose,
 * 9 orange, 11 yellow-green) */
static uint16_t bz_cam_col(uint32_t n)
{
    int32_t h = (140 + 30 * ((int32_t)n - 1)) % 360, f = h % 60, hi = ux.light ? 150 : 255, lo = ux.light ? 20 : 110;
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
        if (bz_blend && bz_cc[3] >= 1u && bz_cc[3] <= 12u) {   /* a blend: the next key, the shared notes only */
            fmt_int(nm, bz_cc[3]);
            str_cpy(nm + str_len(nm), bz_cc[4] >= 64u ? "B SHARED" : "A SHARED", sizeof nm - str_len(nm));
            x = cv_text_on(x + 6, 2 + CAP_IN(S, 18), &AF_S, "+", T_MID, T_SURF);
            cv_text_on(x + 4, 2 + CAP_IN(S, 18), &AF_S, nm, bz_cam_col(bz_cc[3]), T_SURF);
        } else {
            cv_text_on(x + 6, 2 + CAP_IN(S, 18), &AF_S, nm, T_TEXT, T_SURF);
        }
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

/* BLAZE: the phrase as a bar across the top (y 1..4): 16 bars of 13 px, 1 apart; the bar now filled to its beat */
static void bz_phrase_bar(void)
{
    uint32_t beat, i;
    int32_t bar = bz_phrase(&beat);
    for (i = 0; i < BZ_PHRASE; i++) {
        int32_t sx = 6 + 14 * (int32_t)i, w = 13;
        uint16_t c = bar < 0 ? T_LINE : (int32_t)i < bar ? T_THEME : (int32_t)i == bar ? T_ACCENT : T_LINE;
        if (bar == (int32_t)BZ_PHRASE - 1 && !(beat & 1u) && (int32_t)i <= bar)
            c = T_REC;
        if ((int32_t)i == bar && c != T_REC) {
            cv_rect(sx, 1, 13, 4, T_LINE);
            w = 13 * (int32_t)(beat + 1u) / 4;
        }
        cv_rect(sx, 1, w, 4, c);
    }
}

/* LOOPS: the phrase bar; the white keys as 4 rows of 4: TRACK 1..4 (playing: filled), FADE 1..4 (fading: filled),
 * FILL 1..4 (its bar: filled), ALL IN DRUMS BREAK RESTART; a change waiting: tinted (its key breathes). Black keys:
 * NOW BAR 4 BARS PHRASE (QUANTIZE), FADE 2 4 8, DUCK RAMP CANCEL THE ONE. Cards: DUCK RELEASE RAMP TO BARS */
static void layer_loops_map(void)
{
    uint32_t i;
    bzk_held_update();
    bz_phrase_bar();
    for (i = 0; i < 16u; i++) {                      /* white keys 1..16: 4 rows of 4 */
        uint32_t t = i % 4u, op = bz_lp_op[t], how;
        char nm[10];
        uint16_t c = i < 4u ? T_THEME : i < 12u ? T_ACCENT : T_MID;
        if (i < 12u) {
            str_cpy(nm, i < 4u ? "TRACK " : i < 8u ? "FADE " : "FILL ", sizeof nm);
            nm[str_len(nm) + 1u] = 0;
            nm[str_len(nm)] = (char)('1' + t);
        } else {
            str_cpy(nm, BZ_LOOPS_WHITE[i - 12u], sizeof nm);
        }
        how = i < 4u ? (op == BZL_IN || op == BZL_OUT ? RLC_TINT : !trk[t].p[P_MUTE] ? RLC_FILL : RLC_IDLE)
            : i < 8u ? (op == BZL_FIN || op == BZL_FOUT ? RLC_TINT : bz_fd_len[t] ? RLC_FILL : RLC_IDLE)
            : i < 12u ? (op == BZL_FILL ? RLC_TINT : bz_fill_left[t] ? RLC_FILL : RLC_IDLE)
            : i == 15u && bz_lp_restart ? RLC_TINT : RLC_IDLE;   /* (waiting: tinted, its key breathing) */
        bzk_cell(BZX(i % 4u), BZD_Y7 + 16 * (int32_t)(i / 4u), 14, nm, c, how, BZK_W, i + 1u);
    }
    for (i = 0; i < 11u; i++) {                      /* black keys 1..11: 3 rows of 4 */
        uint32_t how = i < 4u ? (bz_lp_q == i ? RLC_FILL : RLC_IDLE) : i < 7u ? (bz_lp_fbars == (2u << (i - 4u)) ? RLC_FILL
                     : RLC_IDLE) : i == 7u ? (bz_duck ? RLC_FILL : RLC_IDLE) : i == 8u ? (bzl.on ? RLC_FILL
                     : song.g[G_CLOCK] ? RLC_DIM : RLC_IDLE) : i == 9u ? (bz_waiting() ? RLC_IDLE : RLC_DIM)
                     : song.playing ? RLC_IDLE : RLC_DIM;
        bzk_cell(BZX(i % 4u), BZD_YB + 18 * (int32_t)(i / 4u), i < 8u ? 16 : 14, BZ_LOOPS_BLACK[i],
                 i < 4u ? T_THEME : i < 7u ? T_ACCENT : T_MID, how, BZK_B, i + 1u);
    }
}

static void layer_chord_map(void)
{
    const track_t *t = &trk[bz_ctrk % NTRK];
    uint32_t i;
    char nm[12];
    bzk_held_update();
    bz_strip();
    for (i = 0; i < 8u; i++) {                       /* white keys 1..8: 2 rows of 4 (18 px: "maj7"'s j) */
        if (i < 7u)
            bz_chord_name(t, i, nm, sizeof nm);
        bzk_cell(BZX(i % 4u), BZD_Y0 + 20 * (int32_t)(i / 4u), 18, i < 7u ? nm : "OFF", i < 7u ? T_THEME : T_MID,
                 i < 7u && bz_ch_sel == i + 1u ? RLC_FILL : (i == 7u && !bz_ch_sel) || (i < 7u && !bzc_fits(t, i)) ? RLC_DIM
                 : RLC_IDLE, BZK_W, i + 1u);
    }
    for (i = 0; i < 11u; i++) {                      /* black keys 1..11: 3 rows of 4 (16 px: the rates' "/") */
        uint32_t how = i <= 4u ? (bz_ch_style == i ? RLC_FILL : RLC_IDLE) : i == 5u ? (bz_ch_7th ? RLC_FILL : RLC_IDLE)
                     : i == 6u ? (bz_ch_inv ? RLC_FILL : RLC_IDLE) : i == 7u ? (bz_ch_oct < 0 ? RLC_FILL : RLC_IDLE)
                     : i == 8u ? (bz_ch_oct > 0 ? RLC_FILL : RLC_IDLE) : i == 10u ? (t->p[P_AMODE] ? RLC_FILL : RLC_IDLE)
                     : RLC_IDLE;
        bzk_cell(BZX(i % 4u), BZD_Y0 + 41 + 18 * (int32_t)(i / 4u), 16, BZ_CHORD_BLACK[i], i <= 4u ? T_ACCENT : T_MID,
                 how, BZK_B, i + 1u);
    }
}

static void layer_keys_map(void)
{
    const track_t *t = &trk[bz_ktrk % NTRK];
    uint32_t i, n = 0, pc, b, cam = bz_camelot(&b, (char[16]){0}, 16);
    uint16_t rc = cam ? bz_cam_col(cam) : T_THEME;
    for (pc = 0; pc < 12u; pc++)
        n += (scale_mask(t) >> pc) & 1u;
    bzk_held_update();
    bz_phrase_bar();
    for (i = 0; i < 16u; i++) {                     /* white keys 1..16: 4 rows of 4, the note each plays */
        uint32_t note = bz_key_note(t, i);
        int root = note % 12u == (uint32_t)t->p[P_ROOT] % 12u;
        bzk_cell(BZX(i % 4u), BZD_Y7 + 16 * (int32_t)(i / 4u), 14, N_NOTE[note % 12u], root ? rc : T_THEME,
                 ((bzk_held_w >> i) & 1u) ? RLC_FILL : root ? RLC_TINT : RLC_IDLE, BZK_W, i + 1u);
    }
    for (i = 0; i < 11u; i++) {                     /* black keys 1..11: 3 rows of 4 (rows 2 and 3: the rates' "/") */
        int32_t h = i < 4u ? 14 : 16, ry = BZD_YB + (i < 4u ? 0 : i < 8u ? 16 : 34);
        uint32_t how;
        uint16_t c = T_MID;
        char nm[4] = {'T', (char)('1' + i), 0, 0};
        if (i < NTRK) {
            how = !bz_notes_track(&trk[i]) ? RLC_DIM : i == bz_ktrk % NTRK ? RLC_FILL : RLC_IDLE;
            c = T_THEME;
        } else if (i == BZK_ARP || i == BZK_LATCH) {
            how = (i == BZK_ARP ? t->p[P_AMODE] : t->p[P_AHOLD]) ? RLC_FILL : RLC_IDLE;
            c = T_ACCENT;
        } else if (i <= BZK_R32) {
            how = t->p[P_ARATE] == (int16_t)(i - BZK_R8 + 1u) ? RLC_FILL : RLC_IDLE;
            c = T_ACCENT;
        } else {
            how = (i == BZK_ODN ? bz_koct < 0 : bz_koct > 0) ? RLC_FILL : RLC_IDLE;
        }
        bzk_cell(BZX(i % 4u), ry, h, i < NTRK ? nm : BZ_KEYS_NAME[i - BZK_ARP], c, how, BZK_B, i + 1u);
    }
}

static void layer_blaze(void)
{
    track_t *t = rl_track();
    uint32_t i, fx = perf_kill ? 0u : perf_held | perf_latched, ok = perf_avail();
    int drum = drum_track(t), ext = song.g[G_CLOCK] != 0;
    if (bz_keys) {
        layer_keys_map();
        return;
    }
    if (bz_chord) {
        layer_chord_map();
        return;
    }
    if (bz_loops) {
        layer_loops_map();
        return;
    }
    bzk_held_update();
    bz_phrase_bar();
    for (i = 0; i < 16u; i++) {                         /* white keys 1..16: 4 rows of 4 (15, 16: SET) */
        int32_t x = BZX(i % 4u), y = BZD_Y7 + 16 * (int32_t)(i / 4u);
        if (i < BZ_FX0) {
            uint32_t pad = bz_pad_of(i);
            bzk_cell(x, y, 14, pad < 16u ? rl_lane_name(t, pad % NLANE) : "-", rl_lane_col(pad % NLANE),
                     pad >= 16u || !drum ? RLC_DIM : (((uint32_t)rl_extra | rlu.fl) >> pad) & 1u ? RLC_FILL
                     : pad >= NLANE ? RLC_TINT : RLC_IDLE, BZK_W, i + 1u);
        } else if (i < BZ_FX0 + BZ_NFX) {
            uint32_t e = BZ_FX[i - BZ_FX0];
            bzk_cell(x, y, 14, BZ_FX_NAME[i - BZ_FX0], T_ACCENT, !((ok >> e) & 1u) ? RLC_DIM : (fx >> e) & 1u ? RLC_FILL
                     : RLC_IDLE, BZK_W, i + 1u);
        } else if (i == BZ_RISER) {
            bzk_cell(x, y, 14, "RISER", T_REC, bz_riser ? RLC_FILL : RLC_IDLE, BZK_W, i + 1u);
        } else if (i == BZ_EOUT) {
            bzk_cell(x, y, 14, bz_eo == BZE_OUT ? "OUT" : "EXIT", T_REC, bz_eo != BZE_IDLE ? RLC_FILL : RLC_IDLE, BZK_W,
                     i + 1u);
        } else if (i == BZ_B4 || i == BZ_B8) {
            bzk_cell(x, y, 14, i == BZ_B4 ? "BUILD 4" : "BUILD 8", T_REC,
                     !drum ? RLC_DIM : (i == BZ_B4 && bz_build == 4u) || (i == BZ_B8 && bz_build == 8u) ? RLC_FILL
                     : RLC_IDLE, BZK_W, i + 1u);
        } else {                                        /* 15 PREV, 16 NEXT: the SET from RAIL */
            bzk_cell(x, y, 14, i == BZ_SPREV ? "PREV" : "NEXT", T_THEME, bz_set_n ? RLC_IDLE : RLC_DIM, BZK_W, i + 1u);
        }
    }
    for (i = 0; i < 11u; i++) {                         /* black keys 1..11: 3 rows of 4 */
        int32_t x = BZX(i % 4u), y = BZD_YB + 18 * (int32_t)(i / 4u), h = i < 8u ? 16 : 14;
        if (i < 6u)
            bzk_cell(x, y, h, RL_RATE_CELL[i + 1u], T_THEME,
                     (rl_rates >> (i + 1u)) & 1u || (bz_build && bz_build_rate == i + 1u) ? RLC_FILL : RLC_IDLE, BZK_B,
                     i + 1u);
        else
            bzk_cell(x, y, h, BZ_BLACK_NAME[i - 6u], T_MID,
                     (i == BZK_TAP || i == BZK_NDN || i == BZK_NUP) && ext ? RLC_DIM
                     : (i == BZK_NDN || i == BZK_NUP) && rl_nudging(i) ? RLC_FILL
                     : i == BZK_ONE && !song.playing ? RLC_DIM : RLC_IDLE, BZK_B, i + 1u);
    }
}

/* a card of a 0..127 value shown -64..63 (a TUNE) or as % */
static void bz_card127(uint32_t c, const char *label, int32_t v, int sign, uint32_t icon)
{
    char val[12];
    fmt_int(val, sign ? v - 64 : v * 100 / 127);
    draw_column(c, label, val, sign ? "" : "%", VAL(c), v * 1000 / 127, icon);
}

static void bz_cards(void)
{
    track_t *t = rl_track();
    char val[12];
    const char *unit;
    int32_t m = perf_k[0];
    uint32_t l = bz_held_lane();
    if (bz_riser_held()) {                              /* RISER held: its sound */
        draw_column(0, "TYPE", BZR_NAME[bz_rs_type % BZR_NTYPE], "", VAL(0u), bz_rs_type * 1000 / (BZR_NTYPE - 1),
                    ICON_NOISE);
        bz_card127(1, bz_rs_type == BZR_SYNTH ? "BEND" : "TOP", bz_rs_top, 0, ICON_SWEEP);
        bz_card127(2, "LEVEL", bz_rs_lvl, 0, ICON_LEVEL);
        if (bz_rs_type == BZR_SYNTH) {
            char tn[4] = {'T', (char)('1' + bz_rs_trk % NTRK), 0, 0};
            draw_column(3, "TRACK", tn, "", VAL(3u), (int32_t)(bz_rs_trk % NTRK) * 333, ICON_TRK);
        } else {
            bz_card127(3, "SPACE", bz_rs_space, 0, ICON_REVERB);
        }
        return;
    }
    if (bz_eo_held()) {                                 /* ECHO OUT held: its bars */
        fmt_int(val, bz_eo_bars);
        draw_column(0, "BARS", val, "", VAL(0u), bz_eo_bars * 250, ICON_DELAY);
        draw_column(1, "", "", "", T_DIM, -1, ICON_NONE);
        draw_column(2, "", "", "", T_DIM, -1, ICON_NONE);
        draw_column(3, "", "", "", T_DIM, -1, ICON_NONE);
        return;
    }
    if (bz_loops) {                                     /* LOOPS: DUCK RELEASE RAMP TO BARS */
        fmt_int(val, bz_duck);
        draw_column(0, "DUCK", bz_duck ? val : "OFF", bz_duck ? "%" : "", bz_duck ? VAL(0u) : T_DIM, bz_duck * 10,
                    ICON_LEVEL);
        fmt_int(val, bz_duck_ms);
        draw_column(1, "RELEASE", val, "MS", VAL(1u), (bz_duck_ms - 40) * 1000 / 360, ICON_RELEASE);
        fmt_int(val, bzl.to);
        draw_column(2, "RAMP TO", val, "BPM", song.g[G_CLOCK] ? T_DIM : VAL(2u),
                    (bzl.to - GP[G_BPM].min) * 1000 / (GP[G_BPM].max - GP[G_BPM].min), ICON_TEMPO);
        fmt_int(val, bzl.bars);
        draw_column(3, "BARS", val, "", VAL(3u), bzl.bars * 1000 / 32, ICON_LENGTH);
        return;
    }
    if (bz_chord) {                                     /* CHORD: FILTER LENGTH SPACE LEVEL of the CHORD track */
        const track_t *ct = &trk[bz_ctrk % NTRK];
        m = ct->p[P_ED_FLT];
        fmt_int(val, m);
        draw_column(0, "FILTER", val, "", VAL(0u), (m + 64) * 1000 / 127, ICON_CUTOFF);
        fmt_int(val, bz_ch_len);
        draw_column(1, "LENGTH", val, "%", VAL(1u), bz_ch_len * 10, ICON_GATE);
        bz_card127(2, "SPACE", ct->p[P_REV], 0, ICON_REVERB);
        param_format(&TP[P_LEVEL], ct->p[P_LEVEL], val, &unit);
        draw_column(3, "LEVEL", val, unit, VAL(3u), RATIO(&TP[P_LEVEL], ct->p[P_LEVEL]), ICON_LEVEL);
        return;
    }
    if (bz_keys) {                                      /* KEYS: FILTER GATE RANGE SPACE of the KEYS track */
        const track_t *kt = &trk[bz_ktrk % NTRK];
        m = kt->p[P_ED_FLT];
        fmt_int(val, m);
        draw_column(0, "FILTER", m ? val : "0", "", VAL(0u), (m + 64) * 1000 / 127, ICON_CUTOFF);
        bz_card127(1, "GATE", kt->p[P_AGATE], 0, ICON_GATE);
        fmt_int(val, kt->p[P_AOCT]);
        draw_column(2, "RANGE", val, kt->p[P_AOCT] > 1 ? "OCTS" : "OCT", VAL(2u), (kt->p[P_AOCT] - 1) * 333, ICON_OCTAVE);
        bz_card127(3, "SPACE", kt->p[P_REV], 0, ICON_REVERB);
        return;
    }
    if (l < NLANE && drum_track(t)) {                   /* a pad held: that drum */
        bz_card127(0, "TUNE", bz_lane_get(t, BZ_LANE_TUNE + l), 1, ICON_TUNE);
        bz_card127(1, "DECAY", bz_lane_get(t, BZ_LANE_DECAY + l), 0, ICON_DECAY);
        param_format(&TP[P_LN0 + l], t->p[P_LN0 + l], val, &unit);
        draw_column(2, "LEVEL", val, unit, VAL(2u), t->p[P_LN0 + l] * 1000 / 127, ICON_LEVEL);
        draw_column(3, rl_lane_name(t, l), "", "", T_DIM, -1, ICON_DRUM);
        return;
    }
    if (bz_shift()) {                                   /* SHIFT: TONE SNAP DRIVE THROW */
        if (drum_track(t)) {
            bz_card127(0, "TONE", t->p[P_E0 + 2], 1, ICON_TONE);
            bz_card127(1, "SNAP", t->p[P_E0 + 4], 1, ICON_ACCENT);
            bz_card127(2, "DRIVE", t->p[P_E0 + 7], 0, ICON_DRIVE);
        } else {
            draw_column(0, "TONE", "-", "", T_DIM, -1, ICON_TONE);
            draw_column(1, "SNAP", "-", "", T_DIM, -1, ICON_ACCENT);
            draw_column(2, "DRIVE", "-", "", T_DIM, -1, ICON_DRIVE);
        }
        fmt_int(val, perf_k[2]);
        draw_column(3, "THROW", perf_k[2] ? val : "OFF", perf_k[2] ? "%" : "", perf_k[2] ? VAL(3u) : T_DIM,
                    perf_k[2] * 10, ICON_DELAY);
        return;
    }
    fmt_int(val, m < 0 ? -m : m);
    draw_column(0, "COLOR", m ? val : "OFF", m < 0 ? "LP" : m > 0 ? "HP" : "", m ? VAL(0u) : T_DIM, (m + 100) * 5,
                ICON_CUTOFF);
    if (drum_track(t)) {
        bz_card127(1, "PITCH", t->p[P_E0 + 1], 1, ICON_TUNE);
        bz_card127(2, "DECAY", t->p[P_E0 + 3], 0, ICON_DECAY);
    } else {
        draw_column(1, "PITCH", "-", "", T_DIM, -1, ICON_TUNE);
        draw_column(2, "DECAY", "-", "", T_DIM, -1, ICON_DECAY);
    }
    fmt_int(val, 100 - perf_k[3]);
    draw_column(3, "DEPTH", val, "%", VAL(3u), (100 - perf_k[3]) * 10, ICON_MIX);
}
