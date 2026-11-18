/* SPDX-License-Identifier: GPL-3.0-only
 * BLAZE (a DJ companion built on Felucca by Leo Kuroshita / Hügelton Instruments) */
/* BLAZE 0.20 STACK's map and cards (ui_layer.c includes it after blaze_home_draw.c; the page: blaze_stack_ui.c) */
static uint16_t stk_layer_col(uint32_t j)            /* DRUMS BASS CHORDS LEAD */
{
    return j % 4u == 0u ? rl_lane_col(DV_SNARE) : j % 4u == 1u ? ux.bfx : j % 4u == 2u ? T_THEME : ux.btr;
}
static void layer_stack_map(void)
{
    const track_t *t = &trk[stk_t()];
    uint32_t i;
    char nm[16];
    bzk_held_update();
    if (stk.stage == STK_ARRANGE) {
        for (i = 0; i < 16u; i++) {                     /* 4 layers (rows) x 4 sections (columns) */
            int32_t x = BZX(i % 4u), y = BZD_Y7 + 16 * (int32_t)(i / 4u);
            uint32_t j = i / 4u, s = stk.spage * 4u + i % 4u, v = stk.cell[s][j];
            str_cpy(nm, STL_SHORT[j], sizeof nm);
            str_cpy(nm + str_len(nm), v == CHAIN_SILENT ? " -" : v ? " B" : " A", sizeof nm - str_len(nm));
            bzk_cell(x, y, 14, nm, stk_layer_col(j), v == CHAIN_SILENT ? RLC_DIM
                     : stk_run && stk_row == s ? RLC_FILL : RLC_TINT, BZK_W, i + 1u);
        }
        for (i = 0; i < 11u; i++) {
            int32_t y = BZD_YB + 18 * (int32_t)(i / 4u), h = i < 8u ? 16 : 14;
            uint32_t how = RLC_IDLE;
            uint16_t c = T_MID;
            const char *lab = "";
            if (i < 4u) {
                uint32_t s = stk.spage * 4u + i;
                if (stk.sbars[s]) {
                    fmt_int(nm, stk.sbars[s]);
                    str_cpy(nm + str_len(nm), " BARS", sizeof nm - str_len(nm));
                } else
                    str_cpy(nm, "-", sizeof nm);
                lab = nm;
                c = T_THEME;
                how = stk_run && stk_row == s ? RLC_FILL : stk.sbars[s] ? RLC_IDLE : RLC_DIM;
            } else if (i == 4u) {
                lab = stk.spage ? "5-8" : "1-4";
            } else if (i == 5u) {
                lab = STK_TMPL_NAME[stk.tmpl % 3u];
                c = T_THEME;
            } else if (i == 6u) {
                lab = "CLEAR";
                how = bz_shift() ? RLC_IDLE : RLC_DIM;
            } else if (i == 8u) {
                lab = "BACK";
            } else if (i == 10u) {
                lab = stk_run ? "STOP" : "PLAY";
                c = ux.btr;
                how = stk_run ? RLC_FILL : RLC_IDLE;
            } else {
                how = RLC_DIM;
            }
            bzk_cell(BZX(i % 4u), y, h, lab, c, how, BZK_B, i + 1u);
        }
        return;
    }
    for (i = 0; i < 16u; i++) {
        int32_t x = BZX(i % 4u), y = BZD_Y7 + 16 * (int32_t)(i / 4u);
        if (stk.layer == STL_DRUMS) {                   /* the steps of the drum picked */
            uint32_t s = stk.page * 16u + i, ln = s < NSTEP ? step_lanes(&t->step[s]) : 0u;
            fmt_int(nm, s + 1u);
            bzk_cell(x, y, 14, nm, rl_lane_col(stk.lane), s >= stk_L ? RLC_DIM : (ln >> stk.lane) & 1u ? RLC_FILL
                     : song.playing && t->seq_idx == s ? RLC_SEL : ln ? RLC_TINT : RLC_IDLE, BZK_W, i + 1u);
        } else {                                        /* the layer's notes, in key */
            uint32_t note = bz_key_note(t, i);
            int root = note % 12u == (uint32_t)t->p[P_ROOT] % 12u;
            bzk_cell(x, y, 14, N_NOTE[note % 12u], stk_layer_col(stk.layer), ((bzk_held_w >> i) & 1u) ? RLC_FILL
                     : root ? RLC_TINT : RLC_IDLE, BZK_W, i + 1u);
        }
    }
    for (i = 0; i < 11u; i++) {
        int32_t y = BZD_YB + 18 * (int32_t)(i / 4u), h = i < 8u ? 16 : 14;
        uint32_t how = RLC_IDLE;
        uint16_t c = T_MID;
        const char *lab = "";
        if (i == 8u) {
            lab = bz_shift() ? "A" : "BACK";
            how = stk.layer || bz_shift() ? RLC_IDLE : RLC_DIM;
        } else if (i == 9u) {
            lab = bz_shift() ? "B" : stk.layer == STL_LEAD ? "ARRANGE" : "NEXT";
            c = ux.btr;
        } else if (i == 10u) {
            lab = "CLEAR";
        } else if (stk.layer == STL_DRUMS) {
            lab = rl_lane_name(t, i);
            c = rl_lane_col(i);
            how = i == stk.lane ? RLC_FILL : RLC_IDLE;
        } else if (i < 3u) {
            static const char *const M[3] = {"OCT-", "OCT+", "REC"};
            lab = M[i];
            how = i == 2u && stk.rec ? RLC_FILL : RLC_IDLE;
            c = i == 2u ? ux.btr : T_MID;
        } else {
            how = RLC_DIM;
        }
        bzk_cell(BZX(i % 4u), y, h, lab, c, how, BZK_B, i + 1u);
    }
}
static void stk_cards(void)
{
    const track_t *t = &trk[stk_t()];
    char val[16];
    const char *unit;
    if (stk.stage == STK_ARRANGE) {
        uint32_t s, n = stk_count(), bars = 0;
        for (s = 0; s < n; s++)
            bars += stk.sbars[s];
        fmt_int(val, song.g[G_BPM]);
        draw_column(0, "BPM", val, song.g[G_CLOCK] ? "EXT" : "INT", VAL(0u),
                    (song.g[G_BPM] - GP[G_BPM].min) * 1000 / (GP[G_BPM].max - GP[G_BPM].min), ICON_TEMPO);
        fmt_int(val, n);
        draw_column(1, "SECTIONS", val, "", n ? VAL(1u) : T_DIM, (int32_t)n * 125, ICON_NONE);
        fmt_int(val, bars);
        draw_column(2, "LENGTH", val, "BARS", n ? VAL(2u) : T_DIM, (int32_t)(bars > 128u ? 1000u : bars * 1000u / 128u), ICON_NONE);
        if (stk_run) {
            fmt_int(val, stk_row + 1u);
            str_cpy(val + str_len(val), "/", sizeof val - str_len(val));
            fmt_int(val + str_len(val), stk_n);
            draw_column(3, "SECTION", val, "", VAL(3u), (int32_t)(stk_row + 1u) * 1000 / (stk_n ? stk_n : 1u), ICON_NONE);
        } else
            draw_column(3, "SONG", STK_TMPL_NAME[stk.tmpl % 3u], "", VAL(3u), -1, ICON_NONE);
        return;
    }
    fmt_int(val, stk.bars);
    draw_column(0, "BARS", val, stk.hasb ? "B ON" : "", VAL(0u), stk.bars * 250, ICON_NONE);
    sound_name(t, val);
    val[8] = 0;
    draw_column(1, "SOUND", val, "", VAL(1u), -1, ICON_NONE);
    param_format(&TP[P_LEVEL], t->p[P_LEVEL], val, &unit);
    draw_column(2, "LEVEL", val, unit, VAL(2u), RATIO(&TP[P_LEVEL], t->p[P_LEVEL]), ICON_LEVEL);
    if (stk.layer == STL_DRUMS) {
        fmt_int(val, stk.page * 16u + 1u);
        str_cpy(val + str_len(val), "-", sizeof val - str_len(val));
        fmt_int(val + str_len(val), stk.page * 16u + 16u);
        draw_column(3, "STEPS", val, "", VAL(3u), (int32_t)(stk.page + 1u) * 1000 / (stk_L / 16u ? stk_L / 16u : 1u), ICON_NONE);
    } else {
        fmt_int(val, bz_koct);
        draw_column(3, "OCTAVE", val, "", VAL(3u), (bz_koct + 2) * 250, ICON_NONE);
    }
}
