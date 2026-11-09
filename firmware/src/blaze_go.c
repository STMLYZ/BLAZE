/* SPDX-License-Identifier: GPL-3.0-only
 * BLAZE 0.15: GO, the launcher. HOME held shows a map of every place on the keys; a key goes there at once (BLAZE and
 * its pages, ROLL, the SET view, the sound and song pages, the MENU). Let go of HOME without a key: the MENU, as
 * HOME held always did (it opened at 0.7 s; now on the release, once the map has shown). HOME tapped: HOME, as before.
 * While HOME is down its keys are the map's (silent: ui_layer.c layer_masks puts HOME in kb_mask), so a key pressed
 * at once goes too, before the map shows. Not in the menu, a dialog, NAME, or with a layer's button held.
 * Included by ui_layer.c after the BLAZE files (it opens their pages); ui_input.c calls go_poll and go_keys,
 * ui_draw.c go_shown and go_draw. */

enum { GO_BLAZE, GO_ROLL, GO_PAGE, GO_SET, GO_MENU };
typedef struct {
    const char *label;
    uint8_t kind, arg;                                  /* GO_BLAZE: its page (0 BLAZE 1 KEYS 2 CHORD 3 LOOPS 4 SLICE 5 RB) */
    const char *page;                                   /* GO_PAGE: PAGES[]' title */
} go_t;
/* the white keys 1..16 (4 rows of 4): perform, then the views, then the sound; the black keys 1..11: the song,
 * the project, the rest */
static const go_t GO_W[16] = {
    {"BLAZE", GO_BLAZE, 0, 0}, {"PLAY", GO_BLAZE, 6, 0}, {"ROLL", GO_ROLL, 0, 0}, {"KEYS", GO_BLAZE, 1, 0},
    {"CHORD", GO_BLAZE, 2, 0}, {"LOOPS", GO_BLAZE, 3, 0}, {"SLICE", GO_BLAZE, 4, 0}, {"SET", GO_SET, 0, 0},
    {"PRESETS", GO_PAGE, 0, "PRESETS"}, {"EDIT", GO_PAGE, 0, "EDIT 1"}, {"FILTER", GO_PAGE, 0, "FILTER"},
    {"ENV", GO_PAGE, 0, "ENV"}, {"LFO", GO_PAGE, 0, "LFO"}, {"FX", GO_PAGE, 0, "FX"}, {"ARP", GO_PAGE, 0, "ARP"},
    {"RB MIDI", GO_BLAZE, 5, 0}};   /* (0.17: PLAY in, MOD out: EDIT's pages reach it) */
static const go_t GO_B[11] = {
    {"STEP", GO_PAGE, 0, "STEP"}, {"SONG", GO_PAGE, 0, "SONG"}, {"AUTO", GO_PAGE, 0, "AUTOMATION"},
    {"MIXER", GO_PAGE, 0, "MIXER"}, {"PROJECT", GO_PAGE, 0, "PROJECT"}, {"USER", GO_PAGE, 0, "USER"},
    {"CLOCK", GO_PAGE, 0, "CLOCK"}, {"SCALE", GO_PAGE, 0, "SCL"}, {"INSERT", GO_PAGE, 0, "INSERT"},
    {"TOOLS", GO_PAGE, 0, "TOOLS"}, {"MENU", GO_MENU, 0, 0}};
#define GO_SHOW_MS 250u                                 /* HOME held this long: the map */

static struct {
    uint8_t down, ok, shown, used;
    uint32_t t0;
} go;

static int go_shown(void) { return go.shown && go.ok; }
static int go_armed(void) { return go.down && go.ok; }  /* (ui_layer.c layer_masks: the keys are the map's) */
static int go_hold_ok(void) { return go.down && !go.ok; }   /* (HOME held where the map is not: the menu at 0.7 s) */

static const go_t *go_of(uint32_t k)
{
    return key_black(k) ? (key_place(k) < 11u ? &GO_B[key_place(k)] : 0) : key_place(k) < 16u ? &GO_W[key_place(k)] : 0;
}

static void go_menu(void)                               /* (as HOME held opened it: ui_input.c) */
{
    ui.menu = 1;
    ui.confirm = 0;
    name_close();
    ui.force = 1;
    song.seq_mode = 0;
}

/* the page titled s, if it shows now (a track's engine decides some): there; 0: not there */
static int bzgo_page(const char *s)
{
    uint32_t i;
    for (i = 0; i < NPAGES; i++)
        if (str_eq(PAGES[i].title, s) && page_visible(i)) {
            ui.home = 0;
            ui.page = (uint8_t)i;
            page_entered();
            return 1;
        }
    return 0;
}

/* layer l locked open over the page shown (as a double tap of its button locks it, #83) */
static void go_lock(uint32_t l)
{
    ui.lock = (uint8_t)l;
    ui.ly_t0 = (fm1_ticks() & ~15u) | 1u | LY_OPEN;
    ui.force = 1;
    layer_opened(l);
}

static void go_to(const go_t *g)
{
    if (!g)
        return;
    if (ui.lock)                                        /* (another place: the lock open now closes) */
        layer_let_go(1);
    switch (g->kind) {
    case GO_BLAZE:
        if (bz_play)
            bz_play_close();
        bz_keys = bz_chord = bz_loops = bz_slice = bz_rb = 0;
        if (g->arg == 6u) {
            bz_play_open();
        } else if (g->arg == 1u) {
            bz_ktrk = (uint8_t)bz_keys_track();
            bz_keys = 1;
        } else if (g->arg == 2u) {
            if (!bz_notes_track(&trk[bz_ctrk % NTRK]))
                bz_ctrk = (uint8_t)bz_keys_track();
            bz_chord = 1;
        } else if (g->arg == 3u) {
            bz_loops = 1;
        } else if (g->arg == 4u) {
            bz_strk = (uint8_t)bz_slice_first();
            bz_sl_ok = (uint8_t)bz_slice_trk(&trk[bz_strk % NTRK]);
            bz_slice = 1;
        } else if (g->arg == 5u) {
            bz_rb = 1;
        }
        go_lock(LAYER_BLAZE);
        break;
    case GO_ROLL:
        go_lock(LAYER_ROLL);
        break;
    case GO_SET:
        if (ui_home_view != HV_SET) {
            ui_home_view = HV_SET;
            settings_later();
        }
        go_home();
        break;
    case GO_MENU:
        go_menu();
        break;
    default:
        if (!bzgo_page(g->page))
            ui_message("NOT ON THIS TRACK");
        break;
    }
    ui_say("GO: ", g->label);
    ui.force = 1;
}

/* ui_input.c, every pass before HOME's tap and hold are read: HOME down (its edge), the map once held, the release */
static void go_poll(uint32_t now)                      /* now: fm1_ticks(), as btn_hold's */
{
    int dn = (int)((fm1_in.buttons >> panel.btn[B_HOME]) & 1u);
    if (dn && !go.down) {
        go.down = 1;
        go.t0 = now;
        go.used = go.shown = 0;
        go.ok = (uint8_t)(!ui.menu && !ui.confirm && !name_on() && !ui.uboot && !layer_held());
    } else if (dn && go.ok && !go.shown && now - go.t0 >= GO_SHOW_MS * 1000u * FM1_TICKS_PER_US) {
        go.shown = 1;
        ui.force = 1;
    } else if (!dn && go.down) {
        go.down = 0;
        if (go.ok && go.shown && !go.used) {            /* held, no key: the MENU (no tap: HOME stays where it is) */
            ui.home_t0 |= 2u;
            go_menu();
        }
        if (go.shown)
            ui.force = 1;
        go.shown = 0;
    }
}

/* ui_input.c: the layer keys pressed now (lkeys); with HOME down they are the map's: the first goes; -> those left */
static uint32_t go_keys(uint32_t lkeys)
{
    uint32_t k;
    if (!go_armed() || !lkeys)
        return lkeys;
    for (k = 0; k < 27u && !((lkeys >> k) & 1u); k++)
        ;
    if (k < 27u && !go.used) {
        go.used = 1;
        ui.home_t0 |= 2u;                               /* (HOME's release: no tap, no menu) */
        go_to(go_of(k));
    }
    return 0;
}

/* where it is now: the cell lit */
static int go_here(const go_t *g)
{
    switch (g->kind) {
    case GO_BLAZE:
        return ui.lock == LAYER_BLAZE && (g->arg == 6u ? bz_play : g->arg == 1u ? bz_keys : g->arg == 2u ? bz_chord : g->arg == 3u ? bz_loops
                                          : g->arg == 4u ? bz_slice : g->arg == 5u ? bz_rb
                                          : !(bz_play || bz_keys || bz_chord || bz_loops || bz_slice || bz_rb));
    case GO_ROLL:
        return ui.lock == LAYER_ROLL;
    case GO_SET:
        return !ui.lock && ui.home && home_set();
    case GO_PAGE:
        return !ui.lock && !ui.home && str_eq(cur_page()->title, g->page);
    default:
        return 0;
    }
}

static uint16_t go_col(const go_t *g)                   /* perform: the transitions' colour; views THEME; sound FX's */
{
    return g->kind == GO_BLAZE || g->kind == GO_ROLL ? ux.btr : g->kind == GO_SET || g->kind == GO_MENU ? T_THEME
         : ux.bfx;
}

/* ui_draw.c: the map over the page: a strip where the cards are, the keys' cells in the panel, the footer */
static void go_draw(void)
{
    static uint32_t sig;
    uint32_t i, s = (uint32_t)ui.lock * 31u + ui.page * 7u + ui.home * 3u + (uint32_t)bz_keys + bz_chord * 2u +
                    bz_loops * 4u + bz_slice * 8u + bz_rb * 16u + ux.gen * 977u;
    if (!ui.force && s == sig)
        return;
    sig = s;
    cv_begin(240, Y_GRAPH - H_HEAD, T_BG);              /* the strip: what this is */
    cv_rrect(3, 4, 234, Y_GRAPH - H_HEAD - 8, 5, T_SURF, T_BG);
    cv_text_on(12, 4 + CAP_IN(M, 22), &AF_M, "GO TO", T_THEME, T_SURF);
    cv_text_on(12, 26 + CAP_IN(S, 16), &AF_S, "PRESS A KEY.  LET GO: MENU", T_MID, T_SURF);
    cv_blit(0, H_HEAD);
    cv_begin(240, H_GRAPH, T_BG);
    cv_rrect(3, 0, 234, H_GRAPH, 5, T_SURF, T_BG);
    cv_bg = T_SURF;
    for (i = 0; i < 16u; i++) {
        const go_t *g = &GO_W[i];
        bzk_cell(BZX(i % 4u), BZD_Y7 + 16 * (int32_t)(i / 4u), 14, g->label, go_col(g), go_here(g) ? RLC_FILL : RLC_IDLE,
                 BZK_W, i + 1u);
    }
    for (i = 0; i < 11u; i++) {
        const go_t *g = &GO_B[i];
        bzk_cell(BZX(i % 4u), BZD_YB + 18 * (int32_t)(i / 4u), i < 8u ? 16 : 14, g->label, go_col(g),
                 go_here(g) ? RLC_FILL : RLC_IDLE, BZK_B, i + 1u);
    }
    cv_blit(0, Y_GRAPH);
    cv_begin(240, H_FOOT, T_BG);
    cv_text_on(8, 9 + CAP_IN(S, 16), &AF_S, "WHITE: PERFORM, VIEWS, SOUND", T_MID, T_BG);
    cv_text_r(232, 9 + CAP_IN(S, 16), &AF_S, "BLACK: SONG", T_MID, T_BG);
    cv_blit(0, Y_FOOT);
}
