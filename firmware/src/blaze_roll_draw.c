/* SPDX-License-Identifier: GPL-3.0-only
 * BLAZE (a DJ companion built on Felucca by Leo Kuroshita / Hügelton Instruments) */
/* ROLL's map (ui_layer.c draw_layer), in full words and outlined by kind so it reads in glare:
 *   the 16 pads, 4 a row (white keys 1..8 the lanes, 9..16 the same accented: their inside tinted the lane's colour),
 *   each outlined 2 px in its lane's colour (KICK red, SNARE yellow, CLAP pink, HAT cyan, OPEN (hat) blue, TOM green,
 *   RIM violet, BELL orange; a light palette: the darker set). Held or sounding: filled solid in it.
 *   under them the 11 black keys and OCT+ as a 4 x 3 grid: the 7 rates outlined THEME (held: filled), then TAP SLOWER
 *   FASTER (NUDGE) PADS and GROOVE / LAST outlined MID (the settings). (0.6: the key cells, bzk_cell)
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
        return "CRASH";
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

/* ---- the key cells (ROLL, BLAZE and its pages): a key's cell looks like its key, a white key's light with dark text,
 * a black key's black with light text, each outlined 2 px in its kind's colour; on its left a tab with the key's
 * number, inverted (a white key: black with a white figure; a black key: white with a black figure). A key held now:
 * its outline the palette's TEXT (bright). States: IDLE, TINT (its colour faint: an accent, a change waiting), FILL
 * (its colour solid: on), SEL (THEME: picked), DIM (nothing there). A cell of no key (BZK_BTN: OCT+'s GROOVE): no tab,
 * the panel's look. Cells 57 x h, 1 px apart, x 5 .. 236 (BZX) */
#define BZX(c) (5 + 58 * (int32_t)(c))
#define BZW 57
enum { BZK_BTN, BZK_W, BZK_B };
static const uint8_t BZ_DIGIT[10][5] = {        /* 3 x 5 figures, a row a byte (bit 2: the left pixel) */
    {7, 5, 5, 5, 7}, {2, 6, 2, 2, 7}, {7, 1, 7, 4, 7}, {7, 1, 3, 1, 7}, {5, 5, 7, 1, 1},
    {7, 4, 7, 1, 7}, {7, 4, 7, 5, 7}, {7, 1, 1, 2, 2}, {7, 5, 7, 5, 7}, {7, 5, 7, 1, 7}};
static uint32_t bzk_held_w, bzk_held_b;         /* the white / black keys held now, by number (bit n - 1) */
static void bzk_held_update(void)
{
    uint32_t k;
    bzk_held_w = bzk_held_b = 0;
    for (k = 0; k < 27u; k++)
        if ((fm1_in.notes >> k) & 1u) {
            if (key_black(k))
                bzk_held_b |= 1u << key_place(k);
            else
                bzk_held_w |= 1u << key_place(k);
        }
}
static uint16_t bzk_base(uint32_t kind, int ink)  /* a white / black key's fill, or its text */
{
    if (kind == BZK_W)
        return ink ? RGB(12, 12, 12) : ux.light ? RGB(255, 255, 255) : RGB(210, 210, 210);
    return ink ? RGB(236, 236, 236) : ux.light ? RGB(28, 28, 28) : RGB(0, 0, 0);
}
static uint16_t bzk_on_ink(uint16_t c)          /* text on a solid colour: dark or light by its brightness */
{
    uint32_t r = (c >> 11) << 3, g = ((c >> 5) & 63u) << 2, b = (c & 31u) << 3;
    return r * 299u + g * 587u + b * 114u > 140000u ? RGB(12, 12, 12) : RGB(236, 236, 236);
}
/* BLAZE 0.14: what a key cell says beyond its state (the caller sets it before bzk_cell, which clears it): prog
 * 0..1000 how far a running thing is (a bar along its foot; -1 none), wait the bars before it starts (a badge; 255: its
 * beat, no number), lock: latched (a dot), icon (BZI_*), pulse: the beat's half (a waiting outline breathes) */
enum { BZI_NONE, BZI_BUILD, BZI_RISER, BZI_DROP, BZI_SPIN };
static struct { int16_t prog; uint8_t wait, lock, icon, pulse; } bzx = {-1, 0, 0, 0, 0};
static const uint8_t BZ_ICON[5][7] = {          /* 7 x 7, a row a byte (bit 6: the left pixel) */
    {0}, {0x01, 0x01, 0x05, 0x05, 0x15, 0x15, 0x55},                  /* BUILD: bars rising */
    {0x0F, 0x03, 0x05, 0x09, 0x10, 0x20, 0x40},                       /* RISER: up and to the right */
    {0x08, 0x08, 0x08, 0x2A, 0x1C, 0x08, 0x7F},                       /* DROP: down to the floor */
    {0x1C, 0x22, 0x41, 0x41, 0x47, 0x23, 0x1F}};                      /* SPIN: around */
/* a lit cell's colour at row r of h: the palette's fill (0.14 UI_PAL_FILL: flat, chrome, gloss, sunset) */
static uint16_t bzk_grad(uint16_t c, int32_t r, int32_t h)
{
    int32_t t = h > 1 ? r * 100 / (h - 1) : 0;
    switch (ux.bfill) {
    case 1:                                      /* CHROME: a bright top running into the colour, darker at the foot */
        return r == 1 ? ux_mix(c, RGB(255, 255, 255), 90) : t < 45 ? ux_mix(ux_mix(c, RGB(255, 255, 255), 80), c, t * 100 / 45)
               : ux_mix(c, ux_mix(c, RGB(40, 44, 52), 55), (t - 45) * 100 / 55);
    case 2:                                      /* GLOSS: the top half lit, a hard line, the foot darker */
        return t < 50 ? ux_mix(ux_mix(c, RGB(255, 255, 255), 30), c, t * 2) : ux_mix(c, ux_mix(c, RGB(0, 0, 0), 35), (t - 50) * 2);
    case 3:                                      /* SUNSET: gold at the top to magenta at the foot */
        return ux_mix(ux_mix(c, RGB(255, 226, 90), 55), ux_mix(c, RGB(200, 30, 110), 45), t);
    default:
        return c;
    }
}
/* a key's cell (BZK_W / BZK_B), 0.14: dark, its group's colour as a line under the name (IDLE); faint in it (TINT; a
 * waiting thing: outlined, breathing on the beat, a badge of the bars to go); lit in it (FILL: the palette's fill, a
 * dot when latched, a bar for how far a running thing is); THEME (SEL: picked); hatched (DIM: not now). The key's
 * number on a tab, a white key's light, a black key's dark. Held now: a dashed edge. A cell of no key (BZK_BTN):
 * the panel's look */
static void bzk_cell(int32_t x, int32_t y, int32_t h, const char *txt, uint16_t col, uint32_t how, uint32_t kind,
                     uint32_t num)
{
    uint16_t in, ink, tab, fig, cell = ux_mix(T_BG, T_TEXT, ux.light ? 8 : 9);
    int32_t sw = 0, i, r, w = BZW, tx, tw;
    int held = 0;
    char nb[3];
    if (kind == BZK_BTN) {
        uint16_t edge = how == RLC_DIM ? T_LINE : col;
        in = how == RLC_FILL ? col : how == RLC_SEL ? T_THEME : how == RLC_TINT ? ux_mix(T_BG, col, ux.light ? 20 : 40)
           : how == RLC_DIM ? T_BG : ux.style ? T_BG : T_SURF;
        ink = how == RLC_FILL ? bzk_on_ink(col) : how == RLC_SEL ? T_INK : how == RLC_DIM ? T_DIM : T_TEXT;
        cv_rrect(x, y, w, h, 4, edge, T_SURF);
        cv_rrect(x + 2, y + 2, w - 4, h - 4, 2, in, edge);
        GFX_HOOK_ALIGN(x + 2, y, x + w - 2, y + h, AL_HV, "key cell name centred");
        cv_text_in(x + 2, y + CAP_IN(S, h), w - 4, &AF_S, txt, ink, in);
        bzx.prog = -1; bzx.wait = bzx.lock = bzx.icon = 0;
        return;
    }
    held = (int)((((kind == BZK_W ? bzk_held_w : bzk_held_b) >> (num - 1u)) & 1u) && how != RLC_DIM);
    switch (how) {
    case RLC_FILL: in = bzk_grad(col, h / 2, h); ink = bzk_on_ink(col); break;
    case RLC_SEL: in = T_THEME; ink = T_INK; break;
    case RLC_TINT: in = ux_mix(cell, col, 30); ink = T_TEXT; break;
    case RLC_DIM: in = cell; ink = T_DIM; break;
    default: in = cell; ink = T_TEXT; break;
    }
    cv_rrect(x, y, w, h, 3, in, T_SURF);
    if (how == RLC_FILL && ux.bfill)             /* (the palette's fill, a row at a time inside the rounded corners) */
        for (r = 1; r < h - 1; r++)
            cv_rect(x + 1, y + r, w - 2, 1, bzk_grad(col, r, h));
    if (how == RLC_DIM)                          /* (hatched: not now) */
        for (r = -h; r < w; r += 4)
            for (i = 0; i < h - 2; i++)
                if (x + r + i >= x + 1 && r + i < w - 1)
                    cv_rect(x + r + i, y + h - 2 - i, 1, 1, ux_mix(cell, T_DIM, 35));
    sw = num >= 10u ? 10 : 6;                    /* the number's tab: 2 + 4 a figure */
    tab = kind == BZK_W ? ux_mix(T_KEY, T_BG, how == RLC_DIM ? 50 : 0) : ux_mix(T_BG, T_TEXT, 22);
    fig = kind == BZK_W ? T_INK : T_TEXT;
    if (how == RLC_FILL) {                       /* (lit: the figure in the cell's ink, no tab) */
        tab = in;
        fig = ink;
    } else
        cv_rrect(x, y, sw, h, 3, tab, T_SURF);
    if (how != RLC_FILL)
        cv_rect(x + sw - 3, y, 3, h, tab);
    nb[0] = (char)(num >= 10u ? '0' + num / 10u : '0' + num);
    nb[1] = (char)(num >= 10u ? '0' + num % 10u : 0);
    nb[2] = 0;
    for (i = 0; nb[i]; i++)
        for (r = 0; r < 5; r++) {
            uint32_t bits = BZ_DIGIT[nb[i] - '0'][r], c;
            for (c = 0; c < 3u; c++)
                if ((bits >> (2u - c)) & 1u)
                    cv_rect(x + 2 + 4 * i + (int32_t)c, y + (h - 5) / 2 + r, 1, 1, fig);
        }
    tx = x + sw + 1;
    tw = w - sw - 3;
    if (how == RLC_IDLE || how == RLC_TINT)      /* the group's line under the name */
        cv_rect(tx + 1, y + h - 2, tw - 2, 1, ux_mix(in, col, 75));
    if (bzx.wait && how != RLC_FILL) {           /* waiting: outlined (breathing), the bars to go on a badge */
        uint16_t o = bzx.pulse ? ux_mix(col, in, 55) : col;
        cv_rect(x, y, w, 1, o); cv_rect(x, y + h - 1, w, 1, o);
        cv_rect(x, y, 1, h, o); cv_rect(x + w - 1, y, 1, h, o);
        if (bzx.wait != 255u) {
            uint32_t d = bzx.wait > 9u ? 9u : bzx.wait;
            cv_rect(x + w - 8, y + 2, 6, h - 4, col);
            for (r = 0; r < 5; r++)
                for (i = 0; i < 3; i++)
                    if ((BZ_DIGIT[d][r] >> (2 - i)) & 1u)
                        cv_rect(x + w - 7 + i, y + (h - 5) / 2 + r, 1, 1, bzk_on_ink(col));
            tw -= 8;
        }
    }
    if (held)                                    /* held now: a dashed edge, top and foot */
        for (i = x + 1; i < x + w - 1; i += 4) {
            cv_rect(i, y, 2, 1, T_TEXT);
            cv_rect(i, y + h - 1, 2, 1, T_TEXT);
        }
    if (bzx.lock && how == RLC_FILL)             /* latched: a dot in the corner */
        cv_rect(x + w - 5, y + 2, 2, 2, ink);
    if (bzx.prog >= 0 && how == RLC_FILL) {      /* running: a bar along the foot, how far */
        int32_t pw = (tw - 2) * (bzx.prog > 1000 ? 1000 : bzx.prog) / 1000;
        cv_rect(tx + 1, y + h - 3, tw - 2, 2, ux_mix(in, ink, 30));
        cv_rect(tx + 1, y + h - 3, pw, 2, ink);
    }
    if (bzx.icon && bzx.icon < 5u) {             /* the transitions' icon, left of the name */
        uint16_t ic = how == RLC_FILL || how == RLC_SEL ? ink : how == RLC_DIM ? T_DIM : col;
        for (r = 0; r < 7; r++)
            for (i = 0; i < 7; i++)
                if ((BZ_ICON[bzx.icon][r] >> (6 - i)) & 1u)
                    cv_rect(tx + 1 + i, y + (h - 7) / 2 + r, 1, 1, ic);
        tx += 9;
        tw -= 9;
    }
    GFX_HOOK_ALIGN(tx, y, tx + tw, y + h, AL_HV, "key cell name centred");
    cv_text_in(tx, y + CAP_IN(S, h), tw, &AF_S, txt, ink, in);
    bzx.prog = -1; bzx.wait = bzx.lock = bzx.icon = bzx.pulse = 0;
}
/* (a cell as before 0.6: no key's look, any width) */
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

/* ROLL: white keys 1..16 (the pads) as 4 rows of 4, then black keys 1..11 and OCT+ as 3 rows of 4 */
static void layer_roll(void)
{
    track_t *t = rl_track();
    uint32_t p, live = rl_live(), lit = (uint32_t)rl_held | rlu.fl, i;
    int drum = drum_track(t), ext = song.g[G_CLOCK] != 0;
    bzk_held_update();
    for (p = 0; p < 16u; p++) {
        uint32_t how = !((live >> p) & 1u) || !drum ? RLC_DIM : (lit >> p) & 1u ? RLC_FILL : rl_pick ? RLC_SEL
                     : p >= NLANE ? RLC_TINT : RLC_IDLE;
        bzk_cell(BZX(p % 4u), RLD_Y0 + (RLD_PH + RLD_GAP) * (int32_t)(p / 4u), RLD_PH, rl_lane_name(t, p % NLANE),
                 rl_lane_col(p % NLANE), how, BZK_W, p + 1u);
    }
    for (i = 0; i < 12u; i++) {                         /* black keys 1..11 in order, then OCT+ */
        int32_t x = BZX(i % 4u), y = RLD_Y1 + (RLD_BH + RLD_GAP) * (int32_t)(i / 4u);
        if (i < RL_NRATE) {
            bzk_cell(x, y, RLD_BH, RL_RATE_CELL[i], T_THEME, ((rl_rates & ~rl_latch) >> i) & 1u ? RLC_FILL
                     : (rl_latch >> i) & 1u ? RLC_TINT : RLC_IDLE, BZK_B, i + 1u);   /* (0.9: tapped on: tinted) */
        } else if (i == RLK_TAP || i == RLK_NDN || i == RLK_NUP) {
            static const char *const N[3] = {"TAP", "SLOWER", "FASTER"};
            bzk_cell(x, y, RLD_BH, N[i - RLK_TAP], T_MID, ext ? RLC_DIM : rl_nudging(i) ? RLC_FILL : RLC_IDLE, BZK_B,
                     i + 1u);
        } else if (i == RLK_PADS) {
            bzk_cell(x, y, RLD_BH, "PADS", T_MID, rl_pick || rl_padmode != RL_PADS16 ? RLC_FILL : RLC_IDLE, BZK_B,
                     i + 1u);
        } else {
            bzk_cell(x, y, RLD_BH, rl_groove ? "GROOVE" : "LAST", T_MID, rl_groove ? RLC_FILL : RLC_IDLE, BZK_BTN, 0);
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
