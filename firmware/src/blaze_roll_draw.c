/* SPDX-License-Identifier: GPL-3.0-only
 * BLAZE (a DJ companion built on Felucca by Leo Kuroshita / Hügelton Instruments) */
/* ROLL's map (ui_layer.c draw_layer): the 16 pads 4 a row (the lane in two letters, the accented row with "!"; a
 * locked pad dim, a held or sounding one the accent, PADS held: the live ones selected), under them the 7 rates
 * (held: the accent) and a row of TAP NUDGE- NUDGE+ PADS and the mode. Its cards: BPM (INT / EXT) BUILD DECAY LEVEL */
#define RLD_PAD_H 16
#define RLD_ROW_H 15
static void rl_cell(int32_t x, int32_t y, int32_t w, int32_t h, const char *txt, uint32_t st)
{
    uint16_t ink, fill = lc_box(x, y, w, h, lc_fill(st, &ink));
    GFX_HOOK_ALIGN(x, y, x + w, y + h, AL_HV, "roll cell name centred");
    cv_text_in(x, y + CAP_IN(S, h), w, &AF_S, txt, ink, fill);
}

static void layer_roll(void)
{
    track_t *t = rl_track();
    uint32_t p, live = rl_live(), lit = (uint32_t)rl_held | rlu.fl, r, i;
    char b[8];
    int drum = drum_track(t);
    for (p = 0; p < 16u; p++) {
        uint32_t st = !((live >> p) & 1u) || !drum ? LS_DIM : (lit >> p) & 1u ? LS_HELD : rl_pick ? LS_SEL
                    : LS_OFF | (p & 1u ? LS_ALT : 0u);
        str_cpy(b, drum_lane_abbr(t, p % NLANE), sizeof b);
        if (p >= NLANE)
            str_cpy(b + str_len(b), "!", sizeof b - str_len(b));   /* (the accented row) */
        rl_cell(LC_X(p % 4u), 4 + (RLD_PAD_H + 4) * (int32_t)(p / 4u), LC_W, RLD_PAD_H, b, st);
    }
    for (r = 0; r < RL_NRATE; r++)                      /* the rates: 7 cells of 29 px, 4 apart (x 6 .. 233) */
        rl_cell(6 + 33 * (int32_t)r, 84, 29, RLD_ROW_H, RL_RATE_CELL[r], (rl_rates >> r) & 1u ? LS_HELD : LS_OFF);
    for (i = 0; i < 5u; i++) {                          /* TAP NUDGE- NUDGE+ (34 px), PADS, the mode (55 px), 4 apart */
        static const char *const N[4] = {"TAP", "NDG-", "NDG+", "PADS "};
        static const int16_t X[5] = {6, 44, 82, 120, 179}, W[5] = {34, 34, 34, 55, 55};
        uint32_t st = song.g[G_CLOCK] && i < 3u ? LS_DIM : LS_OFF;
        if (i < 4u)
            str_cpy(b, N[i], sizeof b);
        if (i == 3u) {
            str_cpy(b + str_len(b), RL_PADS_NAME[rl_padmode % RL_NPADMODE], sizeof b - str_len(b));
            st = rl_pick ? LS_HELD : rl_padmode != RL_PADS16 ? LS_SEL : LS_OFF;
        } else if (i == 4u) {
            str_cpy(b, rl_groove ? "GROOVE" : "LAST", sizeof b);
            st = LS_SEL;
        }
        rl_cell(X[i], 103, W[i], RLD_ROW_H, b, st);
    }
}

static void rl_cards(void)
{
    track_t *t = rl_track();
    char val[12];
    const char *unit;
    int32_t bpm = song.g[G_BPM];
    fmt_int(val, bpm);
    draw_column(0, "BPM", val, song.g[G_CLOCK] ? "EXT" : "INT", song.g[G_CLOCK] ? T_DIM : VAL(0u),
                (bpm - GP[G_BPM].min) * 1000 / (GP[G_BPM].max - GP[G_BPM].min), ICON_TEMPO);
    fmt_int(val, rl_build);
    draw_column(1, "BUILD", rl_build ? val : "OFF", rl_build ? "%" : "", rl_build ? VAL(1u) : T_DIM, rl_build * 10,
                ICON_SWEEP);
    if (drum_track(t)) {
        fmt_int(val, t->p[P_E0 + 3] * 100 / 127);
        draw_column(2, "DECAY", val, "%", VAL(2u), t->p[P_E0 + 3] * 1000 / 127, ICON_DECAY);
    } else {
        draw_column(2, "DECAY", "-", "", T_DIM, -1, ICON_DECAY);
    }
    param_format(&TP[P_LEVEL], t->p[P_LEVEL], val, &unit);
    draw_column(3, "LEVEL", val, unit, VAL(3u), RATIO(&TP[P_LEVEL], t->p[P_LEVEL]), ICON_LEVEL);
}
