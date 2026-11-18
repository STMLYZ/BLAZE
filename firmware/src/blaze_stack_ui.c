/* SPDX-License-Identifier: GPL-3.0-only
 * BLAZE (a DJ companion built on Felucca by Leo Kuroshita / Hügelton Instruments) */
/* BLAZE 0.20 STACK, the main loop's (ui_layer.c includes it after blaze_home_ui.c; the audio side: blaze_stack.c).
 * BUILD: one layer at a time, DRUMS BASS CHORDS LEAD (NEXT, BACK: black 10, 9). DRUMS: the white keys are the loop's
 * steps for the drum picked on black 1..8 (K4: the next 16); BASS CHORDS LEAD: the white keys play the layer's track in
 * key and record into its loop while it plays (black 3 REC on / off; black 1 2 the octave). SHIFT + NEXT: the layer's B
 * (from a copy of A), SHIFT + BACK its A again. CLEAR (black 11): the version shown emptied (SHIFT, drums: the drum
 * picked only). K1 BARS (1 2 4: every layer), K2 the layer's SOUND, K3 its LEVEL, K4 the steps shown / the octave.
 * After LEAD, NEXT: ARRANGE. The white keys a grid, 4 layers (rows) x 4 sections (columns; black 5: sections 5..8),
 * a tap OFF -> A -> B; black 1..4 the shown sections' lengths (4 8 16 32 bars); black 6 or SHIFT + SELECT a template;
 * SHIFT + black 7 clears it; black 11 plays it from the top (and stops it); BACK to BUILD. Kept in the project's song
 * (blaze_stack.c) */
enum { STK_BUILD, STK_ARRANGE };
enum { STL_DRUMS, STL_BASS, STL_CHORDS, STL_LEAD };
static const char *const STL_NAME[4] = {"DRUMS", "BASS", "CHORDS", "LEAD"};
static const char *const STL_SHORT[4] = {"DRM", "BAS", "CHD", "LED"};
static const char *const STK_TMPL_NAME[3] = {"CLUB", "DJ TOOL", "LOOP"};
static struct {
    uint8_t stage, layer, lane, page, spage, bars, hasb, rec, tmpl, pend, sel0, open;
    uint8_t mute0[NTRK];
    uint8_t cell[STK_ROWS][4];         /* each section, each layer: 0 A 1 B CHAIN_SILENT OFF */
    uint8_t sbars[STK_ROWS];           /* each section's bars (0: unused) */
} stk = {0, 0, 1, 0, 0, 2, 0, 1};

static uint32_t stk_t(void) { return stk_trk[stk.layer % 4u] % NTRK; }
static int stk_marked(void) { return chain_config.rsv[0] == STK_MAGIC; }
static uint32_t stk_count(void)                     /* the sections played: up to the last one not all OFF */
{
    uint32_t s, j, n = 0;
    for (s = 0; s < STK_ROWS; s++)
        for (j = 0; j < 4u; j++)
            if (stk.sbars[s] && stk.cell[s][j] != CHAIN_SILENT)
                n = s + 1u;
    return n;
}
/* the four layers' tracks: DRUMS the drum track (kept with the song once arranged), the others in order */
static void stk_map(void)
{
    uint32_t d, i, j = 1;
    if (stk_marked())
        d = (chain_config.rsv[2] >> 2) & 3u;
    else {
        track_t *t = rl_track();
        d = drum_track(t) ? trk_index(t) : NTRK - 1u;
    }
    stk_trk[0] = (uint8_t)d;
    for (i = 0; i < NTRK; i++)
        if (i != d)
            stk_trk[j++] = (uint8_t)i;
}
static void stk_lengths(void)                       /* every layer's track: LEN the loop, DIV 1/16 */
{
    uint32_t i;
    stk_L = (uint8_t)(16u * stk.bars);
    for (i = 0; i < 4u; i++) {
        trk[stk_trk[i] % NTRK].p[P_SLEN] = stk_L;
        trk[stk_trk[i] % NTRK].p[P_SDIV] = 2;
    }
}
/* the arrangement -> the project's song, marked STACK's */
static void stk_store(void)
{
    uint32_t s, j, n = stk_count();
    chain_config_t *c = &chain_config;
    memset(c, 0, sizeof *c);
    c->rsv[0] = STK_MAGIC;
    c->rsv[1] = (uint8_t)(stk.hasb | stk_front << 4);
    c->rsv[2] = (uint8_t)((stk.bars == 4u ? 2u : stk.bars == 2u ? 1u : 0u) | (stk_trk[0] & 3u) << 2);
    c->count = (uint8_t)n;
    for (s = 0; s < CHAIN_ROWS; s++)
        c->row[s].repeat = 1;
    for (s = 0; s < n; s++) {
        uint32_t rep = stk.sbars[s] / stk.bars;
        for (j = 0; j < NTRK; j++)
            c->row[s].slot[j] = CHAIN_SILENT;
        for (j = 0; j < 4u; j++)
            c->row[s].slot[stk_trk[j] % NTRK] = stk.cell[s][j];
        c->row[s].repeat = (uint8_t)(rep < 1u ? 1u : rep > 16u ? 16u : rep);
    }
}
static void stk_load(void)                          /* the project's song -> the arrangement (marked: STACK's) */
{
    const chain_config_t *c = &chain_config;
    uint32_t s, j, b = c->rsv[2] & 3u;
    memset(stk.cell, CHAIN_SILENT, sizeof stk.cell);
    memset(stk.sbars, 0, sizeof stk.sbars);
    stk.bars = (uint8_t)(b == 2u ? 4u : b == 1u ? 2u : 1u);
    stk.hasb = (uint8_t)(c->rsv[1] & 15u);
    for (s = 0; s < c->count && s < STK_ROWS; s++) {
        uint32_t bars = (uint32_t)c->row[s].repeat * stk.bars;
        stk.sbars[s] = (uint8_t)(bars > 32u ? 32u : bars);
        for (j = 0; j < 4u; j++) {
            uint32_t v = c->row[s].slot[stk_trk[j] % NTRK];
            stk.cell[s][j] = (uint8_t)(v == 1u && ((stk.hasb >> j) & 1u) ? 1u : v == CHAIN_SILENT ? CHAIN_SILENT : 0u);
        }
    }
}
/* every layer's A in front (stopped: the halves the arrangement or a B edit left) */
static void stk_all_a(void)
{
    uint32_t i;
    for (i = 0; i < 4u; i++)
        if ((stk_front >> (stk_trk[i] % NTRK)) & 1u)
            stk_swap(stk_trk[i] % NTRK);
}
static void stk_rec_arm(int on)                     /* a melodic layer's track records what the keys play */
{
    uint32_t i;
    for (i = 0; i < 4u; i++)
        song.rec &= (uint8_t)~(1u << (stk_trk[i] % NTRK));
    bz_riff_on = 0;
    if (on && stk.layer != STL_DRUMS && stk.stage == STK_BUILD && stk.rec) {
        song.rec |= (uint8_t)(1u << stk_t());
        bz_riff_on = 1;                             /* (no metronome click for it: seq.c click_wanted) */
    }
}
static void stk_say(void)
{
    char m[28];
    str_cpy(m, STL_NAME[stk.layer % 4u], sizeof m);
    str_cpy(m + str_len(m), (stk_front >> stk_t()) & 1u ? " B" : " A", sizeof m - str_len(m));
    ui_say("STACK: ", m);
}
/* the layer j shown, its version b (1: B, made from a copy of A the first time) */
static void stk_layer(uint32_t j, uint32_t b)
{
    uint32_t t;
    if (!stk_run)
        stk_all_a();
    stk.layer = (uint8_t)(j % 4u);
    t = stk_t();
    if (b && !stk_run) {
        if (2u * stk_L > NSTEP) {
            ui_message("B VERSIONS: 1 OR 2 BARS (K1)");
            b = 0;
        } else {
            if (!((stk.hasb >> stk.layer) & 1u)) {
                memcpy(&trk[t].step[stk_L], &trk[t].step[0], stk_L * sizeof(step_t));
                stk.hasb |= (uint8_t)(1u << stk.layer);
            }
            stk_swap(t);
        }
    }
    stk.page = 0;
    song.sel = (uint8_t)t;
    bz_ktrk = (uint8_t)t;
    stk_keys_play = (uint8_t)(stk.stage == STK_BUILD && stk.layer != STL_DRUMS);
    stk_rec_arm(1);
    if (stk_marked())
        chain_config.rsv[1] = (uint8_t)(stk.hasb | stk_front << 4);
    stk_say();
    ui.force = 1;
}
static void stk_template(uint32_t n);
static void stk_open(void)
{
    track_t *d;
    stk_map();
    d = &trk[stk_trk[0] % NTRK];
    stk.sel0 = song.sel;
    if (stk_marked()) {
        stk_load();
        stk_front = (uint8_t)(chain_config.rsv[1] >> 4);
    } else {
        int16_t len = d->p[P_SLEN];
        stk.bars = (uint8_t)(len == 16 ? 1u : len == 64 ? 4u : 2u);
        stk.hasb = 0;
        stk_front = 0;
        memset(stk.cell, CHAIN_SILENT, sizeof stk.cell);
        memset(stk.sbars, 0, sizeof stk.sbars);
    }
    stk_L = (uint8_t)(16u * stk.bars);
    if (!stk_run)
        stk_all_a();
    stk_lengths();
    stk.stage = STK_BUILD;
    stk.open = 1;
    bz_stack = 1;
    stk_layer(STL_DRUMS, 0);
    ui_message(stk_marked() ? "STACK: YOUR LOOP AND ITS SONG" : "STACK: DRUMS, THEN BASS, CHORDS, LEAD");
}
static void stk_close(void)
{
    if (!bz_stack)
        return;
    if (!stk_run)
        stk_all_a();
    stk_rec_arm(0);
    stk_keys_play = 0;
    bz_stack = 0;
    stk.open = 0;
    if (stk_marked())
        chain_config.rsv[1] = (uint8_t)(stk.hasb | stk_front << 4);
    ui.force = 1;
}
/* the arrangement from the top (playing: stops it) */
static void stk_start(void)
{
    uint32_t s, j, n = stk_count(), t;
    for (s = 0; s < n; s++) {
        for (j = 0; j < 4u; j++)
            stk_rows[s].v[j] = stk.cell[s][j];
        stk_rows[s].n16 = (uint16_t)(16u * (stk.sbars[s] ? stk.sbars[s] : 4u));
    }
    for (t = 0; t < NTRK; t++)
        stk.mute0[t] = (uint8_t)trk[t].p[P_MUTE];
    stk_rec_arm(0);
    stk_all_a();
    stk_n = (uint8_t)n;
    stk_row = 0;
    stk_left = stk_rows[0].n16;
    stk_endreq = 0;
    stk_apply(0);
    stk_run = 1;
    transport_req = 1;
    ui_message("STACK: THE SONG FROM THE TOP");
}
static void stk_play(void)
{
    if (stk_run || stk.pend) {
        stk.pend = 0;
        transport_req = 2;
        return;
    }
    if (!stk_marked())
        stk_store();
    stk_map();
    stk_load();
    stk_L = (uint8_t)(16u * stk.bars);
    if (!stk_count()) {
        ui_message("STACK: NO SECTION ON (ARRANGE)");
        return;
    }
    if (song.playing || chain_busy()) {             /* (from the top: stopped first, started next pass) */
        transport_req = 2;
        stk.pend = 1;
        return;
    }
    stk_start();
}
/* each pass (bz_ui_pass): a start waiting for the stop; the end (or a stop): the mutes and A back */
static void stk_pass(void)
{
    uint32_t t;
    if (stk.pend && !song.playing && transport_req != 2u && !chain_busy()) {
        stk.pend = 0;
        stk_start();
        return;
    }
    if (stk_endreq || (stk_run && !song.playing && transport_req != 1u)) {
        stk_run = 0;
        stk_endreq = 0;
        stk_all_a();
        for (t = 0; t < NTRK; t++)
            trk[t].p[P_MUTE] = stk.mute0[t];
        if (bz_stack)
            stk_rec_arm(1);
        ui.force = 1;
    }
    if (stk_run && stk_marked())
        chain_config.rsv[1] = (uint8_t)(stk.hasb | stk_front << 4);
}
/* a template: its sections, B where the layer has one */
static void stk_template(uint32_t n)
{
    static const uint8_t TB[3][STK_ROWS] = {{16, 8, 16, 8, 16, 16, 0, 0}, {32, 16, 32, 16, 32, 0, 0, 0},
                                            {16, 0, 0, 0, 0, 0, 0, 0}};
    /* bit j: layer j on; bit 4 + j: its B */
    static const uint8_t TC[3][STK_ROWS] = {{0x01, 0x03, 0x0F, 0x0C, 0xFF, 0x01, 0, 0}, {0x01, 0x03, 0x0F, 0x03, 0x01, 0, 0, 0},
                                            {0x0F, 0, 0, 0, 0, 0, 0, 0}};
    uint32_t s, j;
    stk.tmpl = (uint8_t)(n % 3u);
    for (s = 0; s < STK_ROWS; s++) {
        uint32_t b = TB[stk.tmpl][s], m = TC[stk.tmpl][s];
        if (b && b / stk.bars > 16u)
            b = 16u * stk.bars;
        stk.sbars[s] = (uint8_t)b;
        for (j = 0; j < 4u; j++)
            stk.cell[s][j] = (uint8_t)(!b || !((m >> j) & 1u) ? CHAIN_SILENT
                                       : ((m >> (4u + j)) & 1u) && ((stk.hasb >> j) & 1u) ? 1u : 0u);
    }
    stk_store();
    ui_say("STACK SONG: ", STK_TMPL_NAME[stk.tmpl]);
    ui.force = 1;
}
static void stk_stage(uint32_t s)
{
    if (!stk_run)
        stk_all_a();
    stk.stage = (uint8_t)s;
    if (s == STK_ARRANGE) {
        stk_keys_play = 0;
        stk_rec_arm(0);
        if (!stk_marked() || !stk_count())
            stk_template(stk.tmpl);
        ui_message("ARRANGE: TAP A CELL OFF / A / B");
    } else
        stk_layer(STL_LEAD, 0);
    ui.force = 1;
}
/* a key of the STACK page pressed (bz_ui_key) */
static void stk_key(uint32_t k)
{
    uint32_t p = key_place(k), t = stk_t(), sh = (uint32_t)bz_shift();
    if (stk.stage == STK_ARRANGE) {
        if (!key_black(k)) {
            uint32_t j = p / 4u, s = stk.spage * 4u + p % 4u, v = stk.cell[s][j];
            if (stk_run) {
                ui_message("STOP THE SONG TO EDIT IT");
                return;
            }
            if (!stk.sbars[s])
                stk.sbars[s] = 16u * stk.bars <= 16u ? 8u : 16u;
            v = v == CHAIN_SILENT ? 0u : v == 0u && ((stk.hasb >> j) & 1u) ? 1u : CHAIN_SILENT;
            stk.cell[s][j] = (uint8_t)v;
            stk_store();
        } else if (p < 4u) {
            uint32_t s = stk.spage * 4u + p, b = stk.sbars[s];
            if (stk_run) {
                ui_message("STOP THE SONG TO EDIT IT");
                return;
            }
            do
                b = b >= 32u || !b ? 4u : b * 2u;
            while (b / stk.bars > 16u || b < stk.bars);
            stk.sbars[s] = (uint8_t)b;
            stk_store();
        } else if (p == 4u) {
            stk.spage = (uint8_t)!stk.spage;
        } else if (p == 5u) {
            if (!stk_run)
                stk_template(stk.tmpl + 1u);
        } else if (p == 6u) {
            if (!sh)
                ui_message("SHIFT + BLACK 7: CLEAR THE SONG");
            else if (!stk_run) {
                memset(stk.cell, CHAIN_SILENT, sizeof stk.cell);
                memset(stk.sbars, 0, sizeof stk.sbars);
                stk_store();
                ui_message("STACK SONG CLEARED");
            }
        } else if (p == 8u) {
            stk_stage(STK_BUILD);
        } else if (p == 10u) {
            stk_play();
        }
        ui.force = 1;
        return;
    }
    if (key_black(k)) {
        if (p == 8u) {                              /* BACK (SHIFT: this layer's A) */
            if (sh)
                stk_layer(stk.layer, 0);
            else if (stk.layer)
                stk_layer(stk.layer - 1u, 0);
        } else if (p == 9u) {                       /* NEXT (SHIFT: this layer's B) */
            if (sh)
                stk_layer(stk.layer, 1);
            else if (stk.layer < STL_LEAD)
                stk_layer(stk.layer + 1u, 0);
            else
                stk_stage(STK_ARRANGE);
        } else if (p == 10u) {                      /* CLEAR (SHIFT, drums: the drum picked) */
            uint32_t i;
            if (stk_run) {
                ui_message("STOP THE SONG FIRST");
                return;
            }
            for (i = 0; i < stk_L; i++)
                if (stk.layer == STL_DRUMS && sh)
                    grid_hit(&trk[t], i, stk.lane, 0);
                else
                    step_clear(&trk[t].step[i]);
            ui_message(stk.layer == STL_DRUMS && sh ? "STACK: THAT DRUM CLEARED" : "STACK: LAYER CLEARED");
        } else if (stk.layer == STL_DRUMS && p < NLANE) {
            stk.lane = (uint8_t)p;
            ui_say("DRUM: ", rl_lane_name(&trk[t], p));
        } else if (stk.layer != STL_DRUMS && p < 2u) {
            bz_koct = (int8_t)clamp(bz_koct + (p ? 1 : -1), -2, 2);
        } else if (stk.layer != STL_DRUMS && p == 2u) {
            stk.rec = (uint8_t)!stk.rec;
            stk_rec_arm(1);
            ui_message(stk.rec ? "STACK REC: WHAT YOU PLAY STAYS" : "STACK REC OFF: JUST PLAY");
        }
        ui.force = 1;
        return;
    }
    if (stk.layer == STL_DRUMS && p < 16u) {        /* a step of the drum picked */
        uint32_t i = stk.page * 16u + p;
        if (i >= stk_L)
            return;
        grid_hit(&trk[t], i, stk.lane, 2);
        if ((step_lanes(&trk[t].step[i]) >> stk.lane) & 1u)
            stk_aud = (uint8_t)(stk.lane + 1u);
        ui.force = 1;
    }
}
/* KNOB 1..4: BUILD: BARS SOUND LEVEL, the steps shown / the octave; ARRANGE: KNOB 1 the tempo */
static void stk_knob(uint32_t k, int32_t s)
{
    track_t *t = &trk[stk_t()];
    if (stk.stage == STK_ARRANGE) {
        if (k == 0u && !song.g[G_CLOCK]) {
            song.g[G_BPM] = (int16_t)clamp(song.g[G_BPM] + (s > 0 ? 10 : -10), GP[G_BPM].min, GP[G_BPM].max);
            ui.bpm_t = 40;
        }
        return;
    }
    if (k == 0u) {
        uint32_t b = s > 0 ? (stk.bars == 1u ? 2u : 4u) : (stk.bars == 4u ? 2u : 1u);
        char m[4];
        if (b == stk.bars)
            return;
        if (stk_run) {
            ui_message("STOP THE SONG FIRST");
            return;
        }
        if (b == 4u && stk.hasb) {
            ui_message("B VERSIONS: 1 OR 2 BARS ONLY");
            return;
        }
        stk_all_a();
        stk.bars = (uint8_t)b;
        stk_lengths();
        if (stk_marked())
            stk_store();
        fmt_int(m, b);
        ui_say(b == 1u ? "STACK: BAR " : "STACK: BARS ", m);
        stk_layer(stk.layer, 0);
    } else if (k == 1u) {
        song.sel = (uint8_t)stk_t();
        eng_list_step(s > 0 ? 1 : -1);
    } else if (k == 2u) {
        t->p[P_LEVEL] = (int16_t)clamp(t->p[P_LEVEL] + accel(EN_K3, s, TP[P_LEVEL].max - TP[P_LEVEL].min),
                                       TP[P_LEVEL].min, TP[P_LEVEL].max);
    } else if (stk.layer == STL_DRUMS) {
        uint32_t pages = stk_L / 16u;
        stk.page = (uint8_t)((stk.page + (s > 0 ? 1u : pages - 1u)) % (pages ? pages : 1u));
    } else {
        bz_koct = (int8_t)clamp(bz_koct + (s > 0 ? 1 : -1), -2, 2);
    }
    ui.force = 1;
}
/* SHIFT + SELECT on ARRANGE: the template; 1: taken */
static int stk_select(int32_t s)
{
    if (!bz_stack || stk.stage != STK_ARRANGE || !bz_shift_held() || stk_run)
        return 0;
    stk_template(stk.tmpl + (s > 0 ? 1u : 2u));
    return 1;
}
static const char *stk_head(void)
{
    static char h[28];
    if (stk.stage == STK_ARRANGE) {
        str_cpy(h, stk_run ? "[ENV] STACK SONG PLAYING" : "[ENV] STACK ARRANGE", sizeof h);
        return h;
    }
    str_cpy(h, "[ENV] STACK ", sizeof h);
    fmt_int(h + str_len(h), stk.layer + 1u);
    str_cpy(h + str_len(h), "/4 ", sizeof h - str_len(h));
    str_cpy(h + str_len(h), STL_NAME[stk.layer % 4u], sizeof h - str_len(h));
    str_cpy(h + str_len(h), (stk_front >> stk_t()) & 1u ? " B" : " A", sizeof h - str_len(h));
    return h;
}
static void stk_led(uint32_t k, uint32_t *on, uint32_t *can)
{
    uint32_t p = key_place(k), t = stk_t();
    if (stk.stage == STK_ARRANGE) {
        if (!key_black(k)) {
            *can = 1;
            *on = stk.cell[stk.spage * 4u + p % 4u][p / 4u % 4u] != CHAIN_SILENT;
        } else {
            *can = p < 7u || p == 8u || p == 10u;
            *on = p == 10u && stk_run;
        }
        return;
    }
    if (!key_black(k)) {
        uint32_t i = stk.page * 16u + p;
        *can = p < 16u && (stk.layer != STL_DRUMS || i < stk_L);
        *on = stk.layer == STL_DRUMS ? *can && ((step_lanes(&trk[t].step[i % NSTEP]) >> stk.lane) & 1u)
                                     : (fm1_in.notes >> k) & 1u;
    } else {
        *can = stk.layer == STL_DRUMS ? p < NLANE || p >= 8u : p < 3u || p >= 8u;
        *on = stk.layer == STL_DRUMS ? p == stk.lane : p == 2u && stk.rec;
    }
}
static uint32_t stk_sig(void)
{
    uint32_t h = stk.stage * 3u + stk.layer * 7u + stk.lane * 11u + stk.page * 13u + stk.spage * 17u + stk.bars * 19u +
                 stk.hasb * 23u + stk.rec * 29u + stk.tmpl * 31u + stk_run * 37u + stk_row * 41u + stk_front * 43u, s;
    const track_t *t = &trk[stk_t()];
    for (s = 0; s < STK_ROWS; s++)
        h = h * 33u + stk.sbars[s] + stk.cell[s][0] * 5u + stk.cell[s][1] * 25u + stk.cell[s][2] * 125u + stk.cell[s][3] * 625u;
    for (s = 0; s < stk_L && s < NSTEP; s++)
        h = h * 31u + step_lanes(&t->step[s]) + t->step[s].n;
    return h + (uint32_t)(t->seq_idx * 977u);
}
