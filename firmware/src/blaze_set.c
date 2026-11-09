/* SPDX-License-Identifier: GPL-3.0-only
 * BLAZE 0.15: HOME's SET view (MENU > HOME SET) and the decks' waveforms it shows.
 *
 * SET is the set's glance page: the tempo, the key (Camelot, its colour, and its note name), the CPU load, the
 * phrase (16 bars, the DROP's bar marked), what is running now (BOOST, FOLLOW, READY, latched effects, a BUILD ..), and
 * the two rekordbox decks: each a scrolling waveform around its playhead (its beats and bars ticked, its memory cues
 * marked, "DROP" ones in the transitions' colour) and the whole track under it, with its title, key, BPM as played
 * and the time left.
 *
 * The decks come from rail-bridge (v6, on the computer): it reads rekordbox's own analysis (the ANLZ files, through
 * pyrekordbox) for the waveforms and cues, follows the FLX4's PLAY / CUE / HOT CUE / tempo fader for where each deck
 * is, and sends it here as SysEx frames of their own, F0 7D 42 5A <cmd> .. F7 ("BZ"), no reply (editor.c ed_service
 * hands them over). Every byte 7 bit; u21 = 3 bytes LSB first:
 *   1 INFO     deck, flags (1 loaded), track BPM x 100 (u21), Camelot 1..12, B (1 major), length in columns (u21),
 *              the first beat (columns x 16, u21), a beat (columns x 256, u21), the title (ASCII, 0-ended, <= 15)
 *   2 OVERVIEW deck, 224 heights 0..15 (the whole track)
 *   3 CUES     deck, n (<= 8), n x (kind: 0 memory, 1 DROP, 2 hot cue; its column, u21)
 *   4 WINDOW   deck, first column (u21), count (2 bytes, <= BZW_WIN), count heights 0..15
 *   5 POS      deck, flags (1 playing), the playhead (columns x 16, u21), the speed x 10000 (u21: the tempo fader)
 *   6 FOCUS    0 deck A, 1 deck B, 2 both (on air: the bridge's guess from the faders)
 * A column is 1/30 s of the track. Between POS frames the playhead runs on by the speed (bzw_pos). Nothing here for a
 * while (BZW_STALE_MS): the decks show as idle. */

#define BZW_OV 224u                                     /* the overview's columns (1 px each) */
#define BZW_WIN 448u                                    /* the window's columns (2 views: the bridge resends ahead) */
#define BZW_COLS_S 30u                                  /* columns per second */
#define BZW_STALE_MS 4000u
static struct {
    struct {
        uint8_t loaded, playing, cam, b, ncue;
        uint8_t title[16];
        uint32_t bpm100, len, beat0, beat256, pos16, pos_ms, speed;
        uint32_t wbase;                                 /* the window's first column */
        uint16_t wn;
        uint8_t ov[BZW_OV / 2u];                        /* heights, 2 per byte */
        uint8_t win[BZW_WIN / 2u];
        uint8_t cue_k[8];
        uint32_t cue_c[8];
    } d[2];
    uint8_t focus, seen;                                /* seen: a frame has come */
    uint32_t last_ms;                                   /* the last frame's time */
} bzw;

static uint32_t bzw_u21(const uint8_t *a) { return (uint32_t)a[0] | (uint32_t)a[1] << 7 | (uint32_t)a[2] << 14; }
static uint32_t bzw_nib(const uint8_t *p, uint32_t i) { return (p[i >> 1] >> ((i & 1u) * 4u)) & 15u; }
static void bzw_set_nib(uint8_t *p, uint32_t i, uint32_t v)
{
    uint32_t s = (i & 1u) * 4u;
    p[i >> 1] = (uint8_t)((p[i >> 1] & ~(15u << s)) | ((v & 15u) << s));
}

/* a frame's bytes after F0 7D 42 5A: a[0] the command (editor.c ed_service) */
static void bz_wave_frame(const uint8_t *a, uint32_t n)
{
    uint32_t cmd, i;
    if (n < 2u)
        return;
    cmd = a[0];
    a++;
    n--;
    bzw.last_ms = fm1_ms;
    bzw.seen = 1;
    if (cmd == 6u) {
        bzw.focus = (uint8_t)(a[0] > 2u ? 2u : a[0]);
        return;
    }
    if (a[0] > 1u)
        return;
    {
        __typeof__(bzw.d[0]) *d = &bzw.d[a[0]];
        a++;
        n--;
        switch (cmd) {
        case 1:                                         /* INFO */
            if (n < 18u)
                return;
            d->loaded = a[0] & 1u;
            d->bpm100 = bzw_u21(a + 1);
            d->cam = (uint8_t)(a[4] <= 12u ? a[4] : 0u);
            d->b = a[5] & 1u;
            d->len = bzw_u21(a + 6);
            d->beat0 = bzw_u21(a + 9);
            d->beat256 = bzw_u21(a + 12);
            for (i = 0; i < 15u && 15u + i < n && a[15 + i]; i++)
                d->title[i] = (uint8_t)(a[15 + i] >= 32u ? a[15 + i] : '?');
            d->title[i] = 0;
            d->ncue = 0;
            d->wn = 0;
            d->playing = 0;
            d->pos16 = 0;
            d->speed = 10000u;
            memset(d->ov, 0, sizeof d->ov);
            break;
        case 2:                                         /* OVERVIEW */
            for (i = 0; i < BZW_OV && i < n; i++)
                bzw_set_nib(d->ov, i, a[i]);
            break;
        case 3:                                         /* CUES */
            d->ncue = 0;
            for (i = 0; i < a[0] && i < 8u && 1u + 4u * i + 3u < n; i++) {
                d->cue_k[i] = a[1 + 4 * i];
                d->cue_c[i] = bzw_u21(a + 2 + 4 * i);
                d->ncue++;
            }
            break;
        case 4: {                                       /* WINDOW */
            uint32_t c;
            if (n < 5u)
                return;
            c = (uint32_t)a[3] | (uint32_t)a[4] << 7;
            c = c > BZW_WIN ? BZW_WIN : c;
            c = c > n - 5u ? n - 5u : c;
            d->wbase = bzw_u21(a);
            d->wn = (uint16_t)c;
            for (i = 0; i < c; i++)
                bzw_set_nib(d->win, i, a[5 + i]);
            break;
        }
        case 5:                                         /* POS */
            if (n < 7u)
                return;
            d->playing = a[0] & 1u;
            d->pos16 = bzw_u21(a + 1);
            d->speed = bzw_u21(a + 4);
            d->pos_ms = fm1_ms;
            break;
        default:
            break;
        }
    }
}

static int bzw_live(void) { return bzw.seen && fm1_ms - bzw.last_ms < BZW_STALE_MS; }
/* deck d's playhead now (columns x 16): the last POS run on by its speed while it plays (2 s at most) */
static uint32_t bzw_pos(uint32_t d)
{
    uint32_t e = fm1_ms - bzw.d[d].pos_ms, r;
    if (!bzw.d[d].playing || !bzw_live())
        return bzw.d[d].pos16;
    e = e > 2000u ? 2000u : e;
    r = BZW_COLS_S * 16u * (bzw.d[d].speed > 30000u ? 30000u : bzw.d[d].speed) / 10000u;   /* (x16 columns a second) */
    return bzw.d[d].pos16 + e * r / 1000u;
}

/* ---- drawing (ui_graph.c draw_graph: HOME with MENU > HOME SET) ---- */
#define BZS_X 8                                         /* the view's left and right ink */
#define BZS_R 232
#define BZS_DW 224                                      /* a deck's waveform: 224 columns at x 8 */

/* the chips row: what runs now, as small pills, left to right while they fit */
static int32_t bzs_chip(int32_t x, int32_t y, const char *s, uint16_t col, int lit)
{
    int32_t w = text_w(&AF_S, s) + 8;
    if (x + w > BZS_R)
        return x;
    if (lit) {
        cv_rrect(x, y, w, 12, 3, col, T_SURF);
        cv_text_on(x + 4, y + CAP_IN(S, 12), &AF_S, s, ux.light ? T_SURF : T_BG, col);
    } else {
        cv_rrect(x, y, w, 12, 3, T_LINE, T_SURF);
        cv_rrect(x + 1, y + 1, w - 2, 10, 2, T_SURF, T_LINE);
        cv_text_on(x + 4, y + CAP_IN(S, 12), &AF_S, s, col, T_SURF);
    }
    return x + w + 4;
}

static void bzs_chips(int32_t y)
{
    static const uint8_t F[] = {BZF_BOOST, BZF_FOLLOW, BZF_MORPH, BZF_RIDE, BZF_FX0, BZF_FX1, BZF_FX2, BZF_FX3,
                                BZF_FX4, BZF_FX5, BZF_SCAT, BZF_B4, BZF_B8, BZF_RISER, BZF_FILL, BZF_EOUT};
    int32_t x = BZS_X, x0 = x;
    uint32_t i;
    char nm[16];
    if (bz_drop != BZD_IDLE && bz_phrase(&i) >= 0) {   /* the DROP: its bars to go first */
        char t[16] = "DROP IN ";
        fmt_int(t + 8, (int32_t)bz_drop_left());
        x = bzs_chip(x, y, t, T_REC, 1);
    }
    for (i = 0; i < NELEM(F); i++)
        if (bz_fn_state(F[i], 0) == BZS_FILL)
            x = bzs_chip(x, y, bz_fn_label(F[i], nm), bz_fn_col(F[i]), 1);
    if (rl_ready)
        x = bzs_chip(x, y, "READY", ux.btr, 1);
    if (bz_shlock)
        x = bzs_chip(x, y, "SHIFT", T_ACCENT, 1);
    if (x == x0)
        cv_text_on(BZS_X, y + CAP_IN(S, 12), &AF_S, "NOTHING RUNNING", T_DIM, T_SURF);
}

/* "m:ss" of columns c */
static void bzs_time(char *t, uint32_t c)
{
    uint32_t s = c / BZW_COLS_S;
    fmt_int(t, (int32_t)(s / 60u));
    str_cpy(t + str_len(t), ":", 8);
    t += str_len(t);
    t[0] = (char)('0' + s % 60u / 10u);
    t[1] = (char)('0' + s % 10u);
    t[2] = 0;
}

/* deck d at y (29 px): its line (A / B, the title, the key, the BPM as played, the time left: 10 px), the window
 * around the playhead (12 px), the whole track (5 px) */
#define BZS_WH 12
static void bzs_deck(uint32_t d, int32_t y)
{
    __typeof__(bzw.d[0]) *k = &bzw.d[d];
    uint32_t pos = bzw_pos(d), pc = pos >> 4, i;
    int32_t rx = BZS_R, mid = BZS_X + BZS_DW / 2, wy = y + 11, oy = y + 24;
    int on = bzw_live() && k->loaded, air = bzw.focus == 2u || bzw.focus == d;
    char t[24];
    t[0] = (char)('A' + d);
    t[1] = 0;
    cv_rrect(BZS_X, y, 11, 10, 2, on && k->playing ? (air ? T_ACCENT : T_THEME) : T_LINE, T_SURF);
    cv_text_on(BZS_X + 6 - text_w(&AF_S, t) / 2, y + CAP_IN(S, 10), &AF_S, t, on && k->playing ? T_BG : T_MID,
               on && k->playing ? (air ? T_ACCENT : T_THEME) : T_LINE);
    if (!on) {
        cv_text_on(BZS_X + 16, y + CAP_IN(S, 10), &AF_S, bzw.seen ? "NO TRACK" : "DECKS: RAIL BRIDGE", T_DIM, T_SURF);
        cv_rect(BZS_X, wy + BZS_WH / 2, BZS_DW, 1, T_LINE);
        cv_rect(BZS_X, oy + 2, BZS_DW, 1, T_LINE);
        return;
    }
    if (k->len > pc) {                                  /* the time left, right */
        t[0] = '-';
        bzs_time(t + 1, (k->len - pc) * 10000u / (k->speed ? k->speed : 10000u));
        rx -= text_w(&AF_S, t);
        cv_text_on(rx, y + CAP_IN(S, 10), &AF_S, t, k->len - pc < 30u * BZW_COLS_S ? T_REC : T_TEXT, T_SURF);
        rx -= 6;
    }
    if (k->bpm100) {                                    /* the BPM as played, one decimal */
        uint32_t b = (k->bpm100 * (k->speed ? k->speed : 10000u) / 10000u + 5u) / 10u;
        fmt_int(t, (int32_t)(b / 10u));
        str_cpy(t + str_len(t), ".", sizeof t - str_len(t));
        fmt_int(t + str_len(t), (int32_t)(b % 10u));
        rx -= text_w(&AF_S, t);
        cv_text_on(rx, y + CAP_IN(S, 10), &AF_S, t, T_THEME, T_SURF);
        rx -= 6;
    }
    if (k->cam) {                                       /* its key */
        fmt_int(t, k->cam);
        str_cpy(t + str_len(t), k->b ? "B" : "A", sizeof t - str_len(t));
        rx -= text_w(&AF_S, t);
        cv_text_on(rx, y + CAP_IN(S, 10), &AF_S, t, bz_cam_col(k->cam), T_SURF);
        rx -= 6;
    }
    cv_free_text(BZS_X + 16, y + CAP_IN(S, 10), &AF_S, (const char *)k->title, T_TEXT, T_SURF, rx - BZS_X - 20);
    /* the window: column pc at mid; mirrored bars, played (left) dim; beats ticked, bars taller; cues */
    for (i = 0; i < BZS_DW; i++) {
        int32_t c = (int32_t)pc - BZS_DW / 2 + (int32_t)i, h;
        uint32_t hv;
        if (c < 0 || (uint32_t)c < k->wbase || (uint32_t)c >= k->wbase + k->wn || (uint32_t)c >= k->len)
            continue;
        hv = bzw_nib(k->win, (uint32_t)c - k->wbase);
        h = (int32_t)(hv * BZS_WH + 14u) / 15;
        if (h)
            cv_rect(BZS_X + (int32_t)i, wy + BZS_WH / 2 - (h + 1) / 2, 1, h, BZS_X + (int32_t)i < mid ? T_MID : T_THEME);
    }
    if (k->beat256 >= 256u) {                           /* beats: 2 px ticks under, a bar's 4 over and under */
        uint32_t b0 = k->beat0, bt;
        int32_t c0 = (int32_t)pc - BZS_DW / 2;
        uint32_t first = c0 <= (int32_t)(b0 >> 4) ? 0u : (((uint32_t)c0 << 4) - b0) * 16u / k->beat256 + 1u;
        for (bt = first; bt < first + 64u; bt++) {
            int32_t c = (int32_t)((b0 + bt * k->beat256 / 16u) >> 4), px = BZS_X + c - c0;
            if (px >= BZS_X + BZS_DW)
                break;
            if (px < BZS_X)
                continue;
            cv_rect(px, wy + BZS_WH - 1, 1, 2, bt % 4u ? T_LINE : T_MID);
            if (!(bt % 4u))
                cv_rect(px, wy - 1, 1, 2, T_MID);
        }
    }
    for (i = 0; i < k->ncue; i++) {                     /* cues: DROP in the transitions' colour */
        int32_t px = BZS_X + (int32_t)k->cue_c[i] - ((int32_t)pc - BZS_DW / 2);
        uint16_t c = k->cue_k[i] == 1u ? ux.btr : k->cue_k[i] == 2u ? T_TEXT : T_ACCENT;
        if (px >= BZS_X && px < BZS_X + BZS_DW)
            cv_rect(px, wy - 1, 1, BZS_WH + 2, c);
        if (k->len) {
            int32_t ox = BZS_X + (int32_t)(k->cue_c[i] * BZW_OV / k->len);
            cv_rect(ox, oy - 1, 1, 7, c);
        }
    }
    cv_rect(mid, wy - 1, 1, BZS_WH + 2, T_TEXT);        /* the playhead */
    for (i = 0; i < BZW_OV; i++) {                      /* the whole track: played dim, the playhead */
        uint32_t hv = bzw_nib(k->ov, i);
        int32_t h = (int32_t)(hv * 5u + 14u) / 15;
        int played = k->len && i * k->len / BZW_OV < pc;
        if (h)
            cv_rect(BZS_X + (int32_t)i, oy + 5 - h, 1, h, played ? T_LINE : T_MID);
    }
    if (k->len)
        cv_rect(BZS_X + (int32_t)(pc < k->len ? pc * BZW_OV / k->len : BZW_OV - 1u), oy - 1, 1, 7, T_TEXT);
}

/* the view: 122 px of panel (the four cards above stay HOME's) */
static void bz_set_draw(void)
{
    char t[16], nm[16];
    uint32_t b, cam = bz_camelot(&b, nm, sizeof nm), beat, i;
    int32_t x, bar = bz_phrase(&beat), ly = 2 + CAP_IN(L, 30), sy;
    cv_begin(240, H_GRAPH, T_BG);
    cv_rrect(3, 0, 234, H_GRAPH, 5, T_SURF, T_BG);
    cv_bg = T_SURF;
    /* the tempo, big; BPM after it on the caps' foot */
    sy = ly + AF_L_CAP_Y + AF_L_CAP_H - AF_S_CAP_Y - AF_S_CAP_H;
    fmt_int(t, song.g[G_BPM]);
    x = cv_text_on(BZS_X + 2, ly, &AF_L, t, T_THEME, T_SURF);
    cv_text_on(x + 3, sy, &AF_S, song.g[G_CLOCK] ? "BPM EXT" : "BPM", T_MID, T_SURF);
    /* the key: Camelot big in its colour, the note name and the CPU load right of it */
    if (cam) {
        fmt_int(t, (int32_t)cam);
        str_cpy(t + str_len(t), b ? "B" : "A", sizeof t - str_len(t));
        x = cv_text_on(104, ly, &AF_L, t, bz_cam_col(cam), T_SURF);
    } else {
        x = cv_text_on(104, sy, &AF_S, "NO KEY", T_DIM, T_SURF);
        nm[0] = 0;
    }
    if (nm[0])
        cv_text_r(BZS_R, ly + AF_L_CAP_Y - AF_S_CAP_Y, &AF_S, nm, T_TEXT, T_SURF);
    str_cpy(t, "CPU ", sizeof t);
    fmt_int(t + 4, (int32_t)cpu_pct());
    str_cpy(t + str_len(t), "%", sizeof t - str_len(t));
    cv_text_r(BZS_R, sy, &AF_S, t, cpu_hot() ? cpu_col() : T_MID, T_SURF);
    (void)x;
    /* the phrase: 16 bars of 13 px, the bar now filled to its beat, the DROP's bar outlined */
    for (i = 0; i < BZ_PHRASE; i++) {
        int32_t sx = BZS_X + 14 * (int32_t)i, w = 13;
        uint16_t c = bar < 0 ? T_LINE : (int32_t)i < bar ? T_THEME : (int32_t)i == bar ? T_ACCENT : T_LINE;
        if (bar == (int32_t)BZ_PHRASE - 1 && !(beat & 1u) && (int32_t)i <= bar)
            c = T_REC;
        if ((int32_t)i == bar && c != T_REC) {
            cv_rect(sx, 34, 13, 5, T_LINE);
            w = 13 * (int32_t)(beat + 1u) / 4;
        }
        cv_rect(sx, 34, w, 5, c);
    }
    if (bar >= 0 && bz_drop != BZD_IDLE) {
        int32_t tb = (bar + (int32_t)bz_drop_left()) % (int32_t)BZ_PHRASE, sx = BZS_X + 14 * tb;
        cv_rect(sx, 32, 13, 1, ux.btr);
        cv_rect(sx, 40, 13, 1, ux.btr);
        cv_rect(sx, 32, 1, 9, ux.btr);
        cv_rect(sx + 12, 32, 1, 9, ux.btr);
    }
    bzs_chips(43);
    bzs_deck(0, 58);
    bzs_deck(1, 88);
    cv_blit(0, graph_y());
}
/* what the view shows changes: the frame (the decks, the playheads) is enough, every other frame as the scope */
