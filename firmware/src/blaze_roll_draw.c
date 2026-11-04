/* SPDX-License-Identifier: GPL-3.0-only
 * BLAZE (a DJ companion built on Felucca by Leo Kuroshita / Hügelton Instruments) */
/* ROLL's map (ui_layer.c draw_layer), in full words and outlined by kind so it reads in glare:
 *   the 16 pads, 4 a row (white keys 1..8 the lanes, 9..16 the same accented: their inside tinted the lane's colour),
 *   each outlined 2 px in its lane's colour (KICK red, SNARE yellow, CLAP pink, HAT cyan, OPEN (hat) blue, TOM green,
 *   RIM violet, BELL orange; a light palette: the darker set). Held or sounding: filled solid in it.
 *   under them the 11 black keys and OCT+ as a 4 x 3 grid: the 7 rates outlined THEME (held: filled), then TAP NUDGE -
 *   NUDGE + PADS and GROOVE / LAST outlined MID (the settings).
 *   Locked or unavailable: a LINE outline, DIM text. GREY and MONO: every outline TEXT (they keep to their greys).
 * Its cards: BPM (INT / EXT) BUILD DECAY LEVEL */
#define RLD_Y0 3                                         /* the pads' first row */
#define RLD_PH 14                                        /* a pad's height (cap 9 inside the 2 px outline) */
#define RLD_BH 16                                        /* the grid's (room for the rates' "/") */
#define RLD_GAP 2
#define RLD_Y1 (RLD_Y0 + 4 * (RLD_PH + RLD_GAP) + 1)      /* the grid under the pads (68: ends at 120 of 122) */
enum { RLC_IDLE, RLC_TINT, RLC_FILL, RLC_SEL, RLC_DIM };

/* a lane's colour (dark / light palettes) */
static uint16_t rl_lane_col(uint32_t l)
{
    static const uint16_t D[NLANE] = {RGB(255, 80, 60), RGB(255, 214, 40), RGB(255, 110, 200), RGB(40, 220, 255),
                                      RGB(120, 170, 255), RGB(90, 230, 110), RGB(190, 140, 255), RGB(255, 160, 40)};
    static const uint16_t L[NLANE] = {RGB(190, 30, 20), RGB(140, 100, 0), RGB(180, 30, 120), RGB(0, 120, 160),
                                      RGB(30, 80, 200), RGB(20, 130, 40), RGB(110, 50, 200), RGB(170, 80, 0)};
    if (ux.mono || (ux.raise == UI_BW_GREY && ux.dim == UI_BW_GREY))
        return T_TEXT;                                   /* (GREY, MONO: no lane colours) */
    return ux.light ? L[l & (NLANE - 1u)] : D[l & (NLANE - 1u)];
}

/* the lane in full words, as the track's KIT plays it */
static const char *rl_lane_name(const track_t *t, uint32_t l)
{
    static const char *const N[NLANE] = {"KICK", "SNARE", "CLAP", "HAT", "OPEN", "TOM", "RIM", "BELL"};
    const char *k = drum_lane_name(t, l);
    l &= NLANE - 1u;
    if (l == DV_BELL && k[0] == 'C')
        return "CYMBAL";
    return (l == DV_TOM || l == DV_RIM) && k[0] == 'C' ? k : N[l];   /* (CONGA, CLAVE) */
}

/* NUDGE held: is black key p (RLK_NDN / RLK_NUP) one of them */
static int rl_nudging(uint32_t p)
{
    uint32_t k;
    for (k = 0; k < 27u; k++)
        if (((rlu.nk >> k) & 1u) && key_black(k) && key_place(k) == p)
            return 1;
    return 0;
}

/* a cell: a 2 px outline in col, then its inside: IDLE raised, TINT col faint, FILL col solid, SEL THEME, DIM */
static void rl_cell(int32_t x, int32_t y, int32_t w, int32_t h, const char *txt, uint16_t col, uint32_t how)
{
    uint16_t edge = how == RLC_DIM ? T_LINE : col, in, ink;
    switch (how) {
    case RLC_FILL: in = col; ink = T_BG; break;
    case RLC_SEL: in = T_THEME; ink = T_INK; edge = T_THEME; break;
    case RLC_TINT: in = ux_mix(T_BG, col, ux.light ? 20 : 40); ink = T_TEXT; break;
    case RLC_DIM: in = T_BG; ink = T_DIM; break;
    default: in = ux.style ? T_BG : T_SURF; ink = T_TEXT; break;
    }
    cv_rrect(x, y, w, h, 4, edge, T_BG);
    cv_rrect(x + 2, y + 2, w - 4, h - 4, 2, in, edge);
    GFX_HOOK_ALIGN(x + 2, y, x + w - 2, y + h, AL_HV, "roll cell name centred");
    cv_text_in(x + 2, y + CAP_IN(S, h), w - 4, &AF_S, txt, ink, in);
}

static void layer_roll(void)
{
    track_t *t = rl_track();
    uint32_t p, live = rl_live(), lit = (uint32_t)rl_held | rlu.fl, i;
    int drum = drum_track(t), ext = song.g[G_CLOCK] != 0;
    char b[12];
    for (p = 0; p < 16u; p++) {
        uint32_t how = !((live >> p) & 1u) || !drum ? RLC_DIM : (lit >> p) & 1u ? RLC_FILL : rl_pick ? RLC_SEL
                     : p >= NLANE ? RLC_TINT : RLC_IDLE;
        rl_cell(LC_X(p % 4u), RLD_Y0 + (RLD_PH + RLD_GAP) * (int32_t)(p / 4u), LC_W, RLD_PH, rl_lane_name(t, p % NLANE),
                rl_lane_col(p % NLANE), how);
    }
    for (i = 0; i < 12u; i++) {                         /* black keys 1..11 in order, then OCT+ */
        int32_t x = LC_X(i % 4u), y = RLD_Y1 + (RLD_BH + RLD_GAP) * (int32_t)(i / 4u);
        if (i < RL_NRATE) {
            rl_cell(x, y, LC_W, RLD_BH, RL_RATE_CELL[i], T_THEME, (rl_rates >> i) & 1u ? RLC_FILL : RLC_IDLE);
        } else if (i == RLK_TAP || i == RLK_NDN || i == RLK_NUP) {
            static const char *const N[3] = {"TAP", "NUDGE-", "NUDGE+"};
            rl_cell(x, y, LC_W, RLD_BH, N[i - RLK_TAP], T_MID, ext ? RLC_DIM : rl_nudging(i) ? RLC_FILL : RLC_IDLE);
        } else if (i == RLK_PADS) {
            str_cpy(b, "PADS ", sizeof b);
            str_cpy(b + str_len(b), RL_PADS_NAME[rl_padmode % RL_NPADMODE], sizeof b - str_len(b));
            rl_cell(x, y, LC_W, RLD_BH, b, T_MID, rl_pick || rl_padmode != RL_PADS16 ? RLC_FILL : RLC_IDLE);
        } else {
            rl_cell(x, y, LC_W, RLD_BH, rl_groove ? "GROOVE" : "LAST", T_MID, rl_groove ? RLC_FILL : RLC_IDLE);
        }
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
