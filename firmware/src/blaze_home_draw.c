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
    static const char *const G[8] = {"AUTO", "NEW", "1 BAR", "2 BAR", "4 BAR", "MELO", "NEW", "KEEP"};
    const track_t *t = &trk[bz_ctrk % NTRK];
    uint32_t i;
    char nm[12];
    bzk_held_update();
    bz_phrase_bar();
    for (i = 0; i < 16u; i++) {                      /* white keys 1..16: 4 rows of 4; 0.12: 9..16 the generator */
        int32_t x = BZX(i % 4u), y = BZD_Y7 + 16 * (int32_t)(i / 4u);
        uint32_t how;
        const char *lab;
        if (i < 7u) {
            char *j;
            bz_chord_name(t, i, nm, sizeof nm);
            for (j = nm; *j; j++)                    /* ("maj7" -> "M7": the 14 px cell holds no descender) */
                if (j[0] == 'm' && j[1] == 'a' && j[2] == 'j') {
                    j[0] = 'M';
                    str_cpy(j + 1, j + 3, sizeof nm - (uint32_t)(j + 1 - nm));
                    break;
                }
            lab = nm;
            how = bz_ch_sel == i + 1u ? RLC_FILL : !bzc_fits(t, i) ? RLC_DIM : RLC_IDLE;
        } else if (i == 7u) {
            lab = "OFF";
            how = !bz_ch_sel ? RLC_DIM : RLC_IDLE;
        } else {
            lab = G[i - 8u];
            how = i == 8u ? (bz_g_auto ? RLC_FILL : RLC_IDLE) : i == 9u ? (bz_g_auto ? RLC_TINT : RLC_IDLE)
                : i <= 12u ? (bz_g_every == (1u << (i - 10u)) ? RLC_TINT : RLC_IDLE)
                : i == 13u ? (bz_g_mel ? RLC_FILL : RLC_IDLE) : i == 14u ? (bz_g_mel ? RLC_IDLE : RLC_DIM)
                : (bz_g_keep ? RLC_FILL : bz_g_mel ? RLC_IDLE : RLC_DIM);
        }
        bzk_cell(x, y, 14, lab, i < 7u ? T_THEME : i == 7u ? T_MID : i < 13u ? T_REC : T_ACCENT, how, BZK_W, i + 1u);
    }
    for (i = 0; i < 11u; i++) {                      /* black keys 1..11: 3 rows of 4 */
        uint32_t how = i <= 4u ? (bz_ch_style == i ? RLC_FILL : RLC_IDLE) : i == 5u ? (bz_ch_7th ? RLC_FILL : RLC_IDLE)
                     : i == 6u ? (bz_ch_inv ? RLC_FILL : RLC_IDLE) : i == 7u ? (bz_ch_oct < 0 ? RLC_FILL : RLC_IDLE)
                     : i == 8u ? (bz_ch_oct > 0 ? RLC_FILL : RLC_IDLE) : i == 10u ? (t->p[P_AMODE] ? RLC_FILL : RLC_IDLE)
                     : RLC_IDLE;
        bzk_cell(BZX(i % 4u), BZD_YB + 18 * (int32_t)(i / 4u), i < 8u ? 16 : 14, BZ_CHORD_BLACK[i],
                 i <= 4u ? T_ACCENT : T_MID, how, BZK_B, i + 1u);
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
        if (i == 0u && bz_shift()) {                /* 0.7: SHIFT: black 1 KEY LOCK */
            bzk_cell(BZX(0), ry, h, "LOCK", T_ACCENT, bz_lock ? RLC_FILL : RLC_IDLE, BZK_B, 1u);
            continue;
        } else if (i < NTRK) {
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

/* 0.8 SLICE: white 1..16 the slices (S1.., START moves them; the ones past the last wrap: dimmed), black: T1..T4,
 * MODE, REV, DIV, SAMPLE, 17-32, QUANTIZE, CHOP */
static void layer_slice_map(void)
{
    const track_t *t = &trk[bz_strk % NTRK];
    uint32_t i, n = bz_slice_n(t), ok = bz_sl_ok;
    char v[12], nm[8];
    const char *u;
    bzk_held_update();
    bz_phrase_bar();
    for (i = 0; i < 16u; i++) {
        uint32_t j = i + 16u * bz_sl_page, sl = n ? (j + (uint32_t)clamp(t->p[P_E2], 0, 31)) % n : 0u;
        nm[0] = 'S';
        fmt_int(nm + 1, (int32_t)sl + 1);
        bzk_cell(BZX(i % 4u), BZD_Y7 + 16 * (int32_t)(i / 4u), 14, ok && n ? nm : "-", T_THEME,
                 !ok || !n ? RLC_DIM : ((bzk_held_w >> i) & 1u) ? RLC_FILL : j >= n ? RLC_DIM : sl == 0u ? RLC_TINT
                 : RLC_IDLE, BZK_W, i + 1u);
    }
    for (i = 0; i < 11u; i++) {
        int32_t h = i < 4u ? 14 : 16, ry = BZD_YB + (i < 4u ? 0 : i < 8u ? 16 : 34);
        uint32_t how = RLC_IDLE;
        uint16_t c = T_ACCENT;
        const char *lab = v;
        if (i < NTRK) {
            v[0] = 'T';
            v[1] = (char)('1' + i);
            v[2] = 0;
            how = bz_slice_trk(&trk[i]) ? (i == bz_strk % NTRK ? RLC_FILL : RLC_IDLE) : bz_shift() ? RLC_TINT : RLC_DIM;
            c = T_THEME;
        } else if (!ok) {
            static const char *const L[7] = {"MODE", "REV", "SLICES", "SAMPLE", "17-32", "Q OFF", "CHOP"};
            lab = L[i - NTRK];
            how = RLC_DIM;
        } else if (i == BZS_MODE || i == BZS_DIV || i == BZS_SRC) {
            uint32_t id = i == BZS_MODE ? P_E4 : i == BZS_DIV ? P_E1 : P_E0;
            param_format(track_desc(t, id), t->p[id], v, &u);
            if (i == BZS_DIV && t->p[P_E1] < 4) {    /* (4 8 16 32: "16 SL") */
                str_cpy(v + str_len(v), " SL", sizeof v - str_len(v));
            }
            how = i == BZS_MODE && t->p[P_E4] ? RLC_FILL : i == BZS_SRC && !n ? RLC_DIM : RLC_IDLE;
        } else if (i == BZS_REV) {
            lab = "REV";
            how = t->p[P_E5] ? RLC_FILL : RLC_IDLE;
        } else if (i == BZS_PAGE) {
            lab = "17-32";
            how = n <= 16u ? RLC_DIM : bz_sl_page ? RLC_FILL : RLC_IDLE;
        } else if (i == BZS_Q) {
            lab = BZS_Q_NAME[bz_sl_q & 3u];
            how = bz_sl_q ? RLC_FILL : RLC_IDLE;
            c = T_MID;
        } else {
            lab = t->p[P_SLCR] == SL_GATE ? "C GATE" : t->p[P_SLCR] == SL_STUT ? "C STUT" : "CHOP";
            how = t->p[P_SLCR] ? RLC_FILL : RLC_IDLE;
            c = T_REC;
        }
        bzk_cell(BZX(i % 4u), ry, h, lab, c, how, BZK_B, i + 1u);
    }
}

/* 0.9 REKORDBOX: what each key sends, named for MIDI LEARN; the white keys tinted by deck (2: filled DECK key) */
static void layer_rb_map(void)
{
    uint32_t i;
    bzk_held_update();
    bz_phrase_bar();
    for (i = 0; i < 16u; i++)
        bzk_cell(BZX(i % 4u), BZD_Y7 + 16 * (int32_t)(i / 4u), 14, BZ_RB_WHITE[i], i < 8u ? T_THEME : i < 12u ? T_ACCENT
                 : T_MID, ((bzk_held_w >> i) & 1u) ? RLC_FILL : RLC_IDLE, BZK_W, i + 1u);
    for (i = 0; i < 11u; i++) {
        int32_t h = i < 8u ? 16 : 14, ry = BZD_YB + 18 * (int32_t)(i / 4u);
        if (i == BZ_RB_DECK_KEY)
            bzk_cell(BZX(i % 4u), ry, h, bz_rb_deck ? "DECK 2" : "DECK 1", T_MID, bz_rb_deck ? RLC_FILL : RLC_IDLE,
                     BZK_B, i + 1u);
        else
            bzk_cell(BZX(i % 4u), ry, h, BZ_RB_BLACK[i], i < 8u ? T_THEME : T_REC,
                     ((bzk_held_b >> i) & 1u) ? RLC_FILL : RLC_IDLE, BZK_B, i + 1u);
    }
}

/* .. its name on its cell (nm: 16 bytes) */
static const char *const RLS_CELL[RLS_N] = {"FLAT", "ACC 2", "ACC 4", "FADE"};
static const char *bz_fn_label(uint32_t f, char *nm)
{
    static const char *const CN[8] = {"CAPT", "ARMED", "REC", "SOLO", "MIX", "OUT", "CAPT", "SAVE"};
    const track_t *t = rl_track();
    if (f >= BZF_PAD0 && f <= BZF_PAD3) {
        uint32_t pad = bz_pad_of(f - BZF_PAD0), l = pad % NLANE;
        str_cpy(nm, pad < 16u ? rl_lane_name(t, l) : "-", 16);
        if (pad < 16u && rl_pp_rate[l]) {           /* 0.9: a PAD PRESET: its rate (and + its effect) */
            static const char *const R[RL_NRATE] = {"BAR", "4", "8", "8T", "16", "16T", "32"};
            str_cpy(nm, drum_lane_abbr(t, l), 16);
            str_cpy(nm + str_len(nm), " ", 16 - str_len(nm));
            str_cpy(nm + str_len(nm), R[rl_pp_rate[l] - 1u], 16 - str_len(nm));
        }
        if (pad < 16u && rl_pp_fx[l])
            str_cpy(nm + str_len(nm), "+", 16 - str_len(nm));
        return nm;
    }
    if (f >= BZF_FX0 && f <= BZF_FX5)
        return BZ_FX_NAME[f - BZF_FX0];
    if (f >= BZF_PPF0 && f <= BZF_PPF5)
        return BZ_FX_NAME[f - BZF_PPF0];
    if (f >= BZF_KIT0 && f <= BZF_KIT7) {
        str_cpy(nm, "KIT ", 16);
        nm[4] = (char)('1' + f - BZF_KIT0);
        nm[5] = 0;
        return nm;
    }
    if (f >= BZF_RATE0 && f <= BZF_RATE5)
        return RL_RATE_CELL[f - BZF_RATE0 + 1u];
    if (f >= BZF_PPR0 && f <= BZF_PPR5)
        return RL_RATE_CELL[f - BZF_PPR0 + 1u];
    switch (f) {
    case BZF_B4: return "BUILD 4";
    case BZF_B8: return "BUILD 8";
    case BZF_RISER: return "RISER";
    case BZF_DROP: return "DROP";
    case BZF_CAP: return CN[bz_cap & 7u];
    case BZF_EOUT: return bz_eo == BZE_OUT ? "OUT" : "EXIT";
    case BZF_SCAT: return "SCATR";
    case BZF_SPIN: return "SPIN";
    case BZF_CAPLEN: return bz_cap_bars == 1u ? "1 BAR" : bz_cap_bars == 2u ? "2 BAR" : "4 BAR";
    case BZF_CAPSAVE: return "SAVE";
    case BZF_MA: return "SET A";
    case BZF_MB: return "SET B";
    case BZF_PREV: return "PREV";
    case BZF_NEXT: return "NEXT";
    case BZF_RIDE: return bzride.on && bzride.bars == 16u ? "RIDE16" : "RIDE";
    case BZF_MORPH: return "MORPH";
    case BZF_ONE: return "THE 1";
    case BZF_REL: return "ALL OFF";
    case BZF_FILL: return "FILL";
    case BZF_TAP: return "TAP";
    case BZF_NDN: return "SLOWER";
    case BZF_NUP: return "FASTER";
    case BZF_SHAPE: return RLS_CELL[rl_shape % RLS_N];
    case BZF_FOLLOW: return "FOLLOW";
    default: return "-";
    }
}
/* .. its kind's colour */
static uint16_t bz_fn_col(uint32_t f)
{
    if (f >= BZF_PAD0 && f <= BZF_PAD3)
        return rl_lane_col(bz_pad_of(f - BZF_PAD0) % NLANE);
    if ((f >= BZF_FX0 && f <= BZF_FX5) || (f >= BZF_PPF0 && f <= BZF_PPF5) || f == BZF_SCAT || f == BZF_SPIN)
        return T_ACCENT;
    if (f == BZF_B4 || f == BZF_B8 || f == BZF_RISER || f == BZF_DROP || f == BZF_CAP || f == BZF_EOUT ||
        f == BZF_FILL || f == BZF_CAPSAVE)
        return T_REC;
    if (f == BZF_TAP || f == BZF_NDN || f == BZF_NUP || f == BZF_ONE || f == BZF_REL || f == BZF_NONE)
        return T_MID;
    return T_THEME;
}

/* the key at place p of the white (black 0) or black (1) row */
static uint32_t bz_key_of(uint32_t black, uint32_t p)
{
    uint32_t k;
    for (k = 0; k < 27u; k++)
        if ((uint32_t)!!key_black(k) == black && key_place(k) == p)
            return k;
    return 0;
}
static void layer_blaze(void)
{
    track_t *t = rl_track();
    uint32_t i, fx = perf_kill ? 0u : perf_held | perf_latched, ok = perf_avail(), pe;
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
    if (bz_slice) {
        layer_slice_map();
        return;
    }
    if (bz_rb) {
        layer_rb_map();
        return;
    }
    bzk_held_update();
    bz_phrase_bar();
    for (i = 0; i < 16u; i++) {                         /* 0.12: white keys 1..16, 4 rows of 4, by what each does now */
        int32_t x = BZX(i % 4u), y = BZD_Y7 + 16 * (int32_t)(i / 4u);
        uint32_t k = bz_key_of(0, i), f = bz_fn(k, (uint32_t)bz_shift(), bz_pads != 0u), st = bz_fn_state(f, k);
        char nm[16];
        bzk_cell(x, y, 14, bz_fn_label(f, nm), bz_fn_col(f), st == BZS_DIM ? RLC_DIM : st == BZS_FILL ? RLC_FILL
                 : st == BZS_TINT ? RLC_TINT : RLC_IDLE, BZK_W, i + 1u);
    }
    for (i = 0; i < 11u; i++) {                         /* black keys 1..11: 3 rows of 4 */
        int32_t x = BZX(i % 4u), y = BZD_YB + 18 * (int32_t)(i / 4u), h = i < 8u ? 16 : 14;
        uint32_t k = bz_key_of(1, i), f = bz_fn(k, (uint32_t)bz_shift(), bz_pads != 0u), st = bz_fn_state(f, k);
        char nm[16];
        bzk_cell(x, y, h, bz_fn_label(f, nm), bz_fn_col(f), st == BZS_DIM ? RLC_DIM : st == BZS_FILL ? RLC_FILL
                 : st == BZS_TINT ? RLC_TINT : RLC_IDLE, BZK_B, i + 1u);
    }
    (void)t; (void)fx; (void)ok; (void)pe; (void)drum; (void)ext;
}

/* a card of a 0..127 value shown -64..63 (a TUNE) or as % */
static void bz_card127(uint32_t c, const char *label, int32_t v, int sign, uint32_t icon)
{
    char val[12];
    fmt_int(val, sign ? v - 64 : v * 100 / 127);
    draw_column(c, label, val, sign ? "" : "%", VAL(c), v * 1000 / 127, icon);
}

/* 0.13 KNOB 4's card with a pad held or rolling: RATE (its rate); a pad held + SHIFT: the knob's steps */
static void bz_rk_card(int held)
{
    static const char *const SET[3] = {"EVEN", "ALL", "TRIP"};
    uint32_t n = BZ_RK_N[bz_rk_set % 3u], p = bz_rk_pos();
    if (held && bz_shift())
        draw_column(3, "STEPS", SET[bz_rk_set % 3u], "", VAL(3u), -1, ICON_STEPS);
    else
        draw_column(3, "RATE", RL_RATE_CELL[bz_rk_rate % RL_NRATE], "", VAL(3u), n > 1u ? (int32_t)(p * 1000u / (n - 1u)) : 1000,
                    ICON_RATE);
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
    if (bz_build_held()) {                              /* 0.12 BUILD held: DRUM CURVE SWELL WITH */
        static const char *const CU[BZBC_N] = {"4>32", "8>32", "16", "16>32"};
        static const char *const WX[BZBX_N] = {"NONE", "FILTER", "ECHO", "RISER"};
        draw_column(0, "DRUM", drum_track(t) ? rl_lane_name(t, bz_b_lane % NLANE) : "-", "", VAL(0u),
                    (int32_t)(bz_b_lane % NLANE) * 142, ICON_DRUM);
        draw_column(1, "SPEED", CU[bz_b_curve % BZBC_N], "", VAL(1u), (int32_t)(bz_b_curve % BZBC_N) * 333, ICON_DIVISION);
        fmt_int(val, bz_b_swell);
        draw_column(2, "SWELL", val, "%", VAL(2u), bz_b_swell * 10, ICON_LEVEL);
        draw_column(3, "WITH", WX[bz_b_fx % BZBX_N], "", bz_b_fx ? VAL(3u) : T_DIM, (int32_t)(bz_b_fx % BZBX_N) * 333,
                    ICON_MIX);
        return;
    }
    if (bz_drop_held()) {                               /* 0.12 DROP held: TOP LENGTH WITH ON THE 1 */
        static const char *const LN[BZDL_N] = {"PHRASE", "4 BARS", "8 BARS", "16 BARS"};
        static const char *const WX[BZDX_N] = {"SPIN", "SPIN+BLD", "BUILD", "NONE"};
        static const char *const HT[BZDH_N] = {"NONE", "ECHO", "REVERB", "CRASH"};
        fmt_int(val, bz_d_top);
        draw_column(0, "FILTER", bz_d_top ? val : "OFF", bz_d_top ? "%" : "", bz_d_top ? VAL(0u) : T_DIM, bz_d_top * 10,
                    ICON_CUTOFF);
        draw_column(1, "LENGTH", LN[bz_d_len % BZDL_N], "", VAL(1u), (int32_t)(bz_d_len % BZDL_N) * 333, ICON_LENGTH);
        draw_column(2, "WITH", WX[bz_d_extra % BZDX_N], "", VAL(2u), (int32_t)(bz_d_extra % BZDX_N) * 333, ICON_MIX);
        draw_column(3, "HIT", HT[bz_d_hit % BZDH_N], "", bz_d_hit ? VAL(3u) : T_DIM, (int32_t)(bz_d_hit % BZDH_N) * 333,
                    ICON_ACCENT);
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
    if (bz_rb) {                                        /* 0.9 REKORDBOX: CC 1..4 on channel 13 */
        static const uint8_t IC[4] = {ICON_MIX, ICON_SAMPLE, ICON_DIVISION, ICON_CUTOFF};
        uint32_t c;
        for (c = 0; c < 4u; c++)
            bz_card127(c, BZ_RB_KNOB[c], bz_rb_val[c], 0, IC[c]);
        return;
    }
    if (bz_slice) {                                     /* SLICE: START PITCH DECAY TONE of the SLICE track */
        static const uint8_t ID[4] = {P_E2, P_E3, P_E6, P_E7};
        static const char *const NM[4] = {"START", "PITCH", "DECAY", "TONE"};
        static const uint8_t IC[4] = {ICON_LENGTH, ICON_TUNE, ICON_DECAY, ICON_CUTOFF};
        const track_t *st = &trk[bz_strk % NTRK];
        uint32_t c;
        for (c = 0; c < 4u; c++) {
            const param_desc_t *d;
            if (!bz_sl_ok) {
                draw_column(c, NM[c], "-", "", T_DIM, -1, IC[c]);
                continue;
            }
            d = track_desc(st, ID[c]);
            param_format(d, st->p[ID[c]], val, &unit);
            draw_column(c, NM[c], val, unit, VAL(c), RATIO(d, st->p[ID[c]]), IC[c]);
        }
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
    if (bz_chord && bz_white_held(13u)) {               /* 0.12 MELODY held: DENSITY RANGE */
        fmt_int(val, bz_g_dens);
        draw_column(0, "DENSITY", val, "%", VAL(0u), bz_g_dens * 10, ICON_DIVISION);
        fmt_int(val, bz_g_range);
        draw_column(1, "RANGE", val, bz_g_range > 1u ? "OCTS" : "OCT", VAL(1u), bz_g_range * 500, ICON_OCTAVE);
        draw_column(2, "", "", "", T_DIM, -1, ICON_NONE);
        draw_column(3, "", "", "", T_DIM, -1, ICON_NONE);
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
        bz_rk_card(1);
        return;
    }
    if (bz_shift()) {                                   /* 0.12 SHIFT: PITCH DECAY TONE DRIVE (the kit) */
        if (drum_track(t)) {
            bz_card127(0, "PITCH", t->p[P_E0 + 1], 1, ICON_TUNE);
            bz_card127(1, "DECAY", t->p[P_E0 + 3], 0, ICON_DECAY);
            bz_card127(2, "TONE", t->p[P_E0 + 2], 1, ICON_TONE);
            bz_card127(3, "DRIVE", t->p[P_E0 + 7], 0, ICON_DRIVE);
        } else {
            draw_column(0, "PITCH", "-", "", T_DIM, -1, ICON_TUNE);
            draw_column(1, "DECAY", "-", "", T_DIM, -1, ICON_DECAY);
            draw_column(2, "TONE", "-", "", T_DIM, -1, ICON_TONE);
            draw_column(3, "DRIVE", "-", "", T_DIM, -1, ICON_DRIVE);
        }
        return;
    }
    /* 0.12: COLOR, PITCH (MORPH on: MORPH), DEPTH (SCATTER: WILD), THROW */
    fmt_int(val, m < 0 ? -m : m);
    draw_column(0, "COLOR", m ? val : "OFF", m < 0 ? "LP" : m > 0 ? "HP" : "", m ? VAL(0u) : T_DIM, (m + 100) * 5,
                ICON_CUTOFF);
    if (bzm.on) {                                       /* 0.10 MORPH: KNOB 2 is A .. B */
        val[0] = bzm.pos < 50u ? 'A' : 'B';
        val[1] = 0;
        draw_column(1, "MORPH", val, "", VAL(1u), bzm.pos * 10, ICON_SWEEP);
    } else if (drum_track(t))
        bz_card127(1, "PITCH", t->p[P_E0 + 1], 1, ICON_TUNE);
    else
        draw_column(1, "PITCH", "-", "", T_DIM, -1, ICON_TUNE);
    fmt_int(val, 100 - perf_k[3]);
    draw_column(2, (perf_held | perf_latched) & PF_BIT(PF_SCAT) ? "WILD" : "DEPTH", val, "%", VAL(2u),
                (100 - perf_k[3]) * 10, ICON_MIX);
    if (bz_padl) {                                      /* 0.13: a pad rolling on its own: KNOB 4 is RATE */
        bz_rk_card(0);
        return;
    }
    fmt_int(val, perf_k[2]);
    draw_column(3, "THROW", perf_k[2] ? val : "OFF", perf_k[2] ? "%" : "", perf_k[2] ? VAL(3u) : T_DIM,
                perf_k[2] * 10, ICON_DELAY);
}
