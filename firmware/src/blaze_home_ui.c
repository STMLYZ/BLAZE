/* SPDX-License-Identifier: GPL-3.0-only
 * BLAZE (a DJ companion built on Felucca by Leo Kuroshita / Hügelton Instruments) */
/* BLAZE, the main loop's side (ui_layer.c includes it after blaze_roll_ui.c): TAP and NUDGE (black keys 7..9: ROLL's),
 * RELEASE's knobs, what it says, the knobs, the LEDs, KEYS (OCT-: the keys play a synth track), and the key.
 * KNOB 1..4: COLOR PITCH DECAY DEPTH; OCT+ held (SHIFT): TONE SNAP DRIVE THROW; a pad held: that drum's own TUNE DECAY
 * LEVEL (eng_drum.c BZ_LANE_*); ALGO: the KIT. KEYS: FILTER GATE RANGE SPACE; ALGO: the arp's MODE.
 * The key: RAIL's CC 20 / 21 / 22 on channel 16 (blaze_home.c bz_cc) set ROOT and SCALE (B: MAJ, A: MIN) of every
 * track whose keys play notes (not DRUM, not SLICE: an engine that maps the keys itself), and from then on the keys of
 * the scale glow as MENU > SCALE LEDS ON shows them (bz_follow; ui_input.c scale_leds). The Camelot number and the key
 * in note names (sharps and flats) are shown from the track's ROOT and SCALE, so SCL's layer sets them too */
static struct {
    uint8_t cc_seq, rel;                       /* the CC seq and RELEASE handled */
} bzu;

/* the key's names: a minor (Camelot A) and a major (B) key on each root (0 = C .. 11 = B), as DJs spell them */
static const char *const BZ_MIN[12] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
static const char *const BZ_MAJ[12] = {"C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
static const uint8_t BZ_CAM_MIN[12] = {5, 12, 7, 2, 9, 4, 11, 6, 1, 8, 3, 10};   /* root -> its Camelot number */
static const uint8_t BZ_CAM_MAJ[12] = {8, 3, 10, 5, 12, 7, 2, 9, 4, 11, 6, 1};

static int bz_notes_track(const track_t *t)
{
    return !drum_track(t) && !ENGINES[eng_idx(t->eng_req)]->keys;
}
/* the track whose key BLAZE shows: the selected one if its keys play notes, else the first that does (0: none) */
static const track_t *bz_key_track(void)
{
    uint32_t i;
    if (bz_notes_track(TSEL))
        return TSEL;
    for (i = 0; i < NTRK; i++)
        if (bz_notes_track(&trk[i]))
            return &trk[i];
    return 0;
}
/* its Camelot number (1..12; 0: neither MAJ nor MIN, *name empty), *b 1 for B (major), *name "A MINOR" style */
static uint32_t bz_camelot(uint32_t *b, char *name, uint32_t len)
{
    const track_t *t = bz_key_track();
    uint32_t r, sc;
    *b = 0;
    name[0] = 0;
    if (!t)
        return 0;
    r = (uint32_t)t->p[P_ROOT] % 12u;
    sc = (uint32_t)t->p[P_SCALE];
    if (sc != 1u && sc != 2u)
        return 0;                                /* (no major / minor key: "NO KEY") */
    *b = sc == 1u;
    str_cpy(name, *b ? BZ_MAJ[r] : BZ_MIN[r], len);
    str_cpy(name + str_len(name), *b ? " MAJOR" : " MINOR", len - str_len(name));
    return *b ? BZ_CAM_MAJ[r] : BZ_CAM_MIN[r];
}

/* RAIL's key arrived: every track whose keys play notes takes its root and MAJ / MIN */
/* RAIL's SET sent a song (CC 26..30, the number last): the tempo (the internal clock only), the KIT, the tracks in
 * (LOOPS: at its point); the key and the next song's came on CC 20..25 (bz_key_apply) */
static void bz_song(void)
{
    static uint32_t last = 0xFFFFFFFFu;
    uint32_t s = (uint32_t)bz_cc[6] | (uint32_t)bz_cc[7] << 8 | (uint32_t)bz_cc[8] << 16 | (uint32_t)bz_cc[9] << 24;
    track_t *d = rl_track();
    char m[24];
    bz_set_n = bz_cc[10];
    if (s == last || !bz_cc[9])
        return;
    last = s;
    if (bz_cc[6] && !song.g[G_CLOCK])
        song.g[G_BPM] = (int16_t)clamp(bz_cc[6] + 60, GP[G_BPM].min, GP[G_BPM].max);
    if (bz_cc[7] && drum_track(d))
        d->p[P_E0] = (int16_t)param_fit(track_desc(d, P_E0), bz_cc[7] - 1);
    if (bz_cc[8] & 64u)
        bz_set_layers = (uint8_t)(0x80u | (bz_cc[8] & 15u));
    fmt_int(m, bz_cc[9]);
    str_cpy(m + str_len(m), " OF ", sizeof m - str_len(m));
    fmt_int(m + str_len(m), bz_cc[10]);
    ui_say("SONG ", m);
    ui.force = 1;
}

static void bz_key_apply(void)
{
    static uint32_t last = 0xFFFFFFFFu, lastn = 0xFFFFFFFFu;
    uint32_t i, r = bz_cc[2] % 12u, b = bz_cc[1] >= 64u, cam, bb, key = (uint32_t)bz_cc[0] | b << 8 | r << 16;
    uint32_t nk = (uint32_t)bz_cc[3] | (uint32_t)(bz_cc[4] >= 64u) << 8 | (uint32_t)(bz_cc[5] % 12u) << 16;
    char nm[16], m[24];
    if (nk != lastn) {                              /* the next track's key: the notes both share (0: none) */
        lastn = nk;
        if (bz_cc[3] >= 1u && bz_cc[3] <= 12u) {
            uint32_t sm = SCALE_MASK[bz_cc[4] >= 64u ? 1 : 2], r2 = bz_cc[5] % 12u;
            bz_blend = (uint16_t)(((sm << r2) | (sm >> (12u - r2))) & 0xFFFu);
            fmt_int(m, bz_cc[3]);
            str_cpy(m + str_len(m), bz_cc[4] >= 64u ? "B: SHARED NOTES" : "A: SHARED NOTES", sizeof m - str_len(m));
            ui_say("BLEND ", m);
        } else if (bz_blend) {
            bz_blend = 0;
            ui_message("BLEND OFF");
        }
        ui.force = 1;
    }
    if (key == last)
        return;
    last = key;
    for (i = 0; i < NTRK; i++)
        if (bz_notes_track(&trk[i])) {
            trk[i].p[P_ROOT] = (int16_t)r;
            trk[i].p[P_SCALE] = (int16_t)(b ? 1 : 2);
        }
    bz_follow = 1;
    cam = bz_camelot(&bb, nm, sizeof nm);
    if (!cam)
        return;
    fmt_int(m, (int32_t)cam);
    str_cpy(m + str_len(m), bb ? "B  " : "A  ", sizeof m - str_len(m));
    str_cpy(m + str_len(m), nm, sizeof m - str_len(m));
    ui_say("KEY ", m);
    ui.force = 1;
}

static const char *const BZ_FX_NAME[BZ_NFX] = {"ECHO", "REVERB", "REVERSE", "BRAKE", "FREEZE", "REPEAT"};
static const char *const BZ_BLACK_NAME[5] = {"TAP", "SLOWER", "FASTER", "ALL OFF", "THE 1"};
enum { BZK_ARP = 4u, BZK_LATCH, BZK_R8, BZK_R16, BZK_R32, BZK_ODN, BZK_OUP };   /* KEYS' black places (0..3: T1..T4) */
static const char *const BZ_KEYS_NAME[7] = {"ARP", "LATCH", "1/8", "1/16", "1/32", "OCT-", "OCT+"};

/* SHIFT: OCT+ held; 0.11.1 SHIFT LOCK: OCT+ double-tapped on BLAZE's own page keeps it on (one tap: off; leaving the
 * page: off), so its keys (BACKSPIN, DROP, CAPTURE, ...) are one press each. The knobs are SHIFT's too while locked */
static uint8_t bz_shlock;
static int bz_shift_held(void) { return ((fm1_in.buttons >> panel.btn[B_OCTUP]) & 1u) != 0u; }
static int bz_shift(void) { return bz_shlock || bz_shift_held(); }
#define BZ_SHL_TAP_MS 300u                          /* a tap: let go within this, nothing pressed meanwhile */
#define BZ_DTAP_MS 400u                             /* a double tap: the second press within this of the first release */
static void bz_shlock_pass(void)
{
    static uint8_t was, used;
    static uint32_t t_dn, t_up;
    uint32_t now = (uint32_t)bz_shift_held();
    if (ui.layer != LAYER_BLAZE || bz_keys || bz_chord || bz_loops || bz_slice || bz_rb) {
        if (bz_shlock) {
            bz_shlock = 0;
            ui.force = 1;
        }
        was = (uint8_t)now;
        t_up = 0;
        return;
    }
    if (now && !was) {
        used = fm1_in.notes != 0u;                  /* (a pad held first: SHIFT for a PAD PRESET, not a tap) */
        t_dn = fm1_ms;
    } else if (now && fm1_in.notes)
        used = 1;                                   /* (a key pressed with it: SHIFT used, not tapped) */
    else if (!now && was) {
        if (!used && fm1_ms - t_dn < BZ_SHL_TAP_MS) {
            if (bz_shlock) {
                bz_shlock = 0;
                t_up = 0;
                ui_message("SHIFT UNLOCKED");
            } else if (t_up && t_dn - t_up < BZ_DTAP_MS) {
                bz_shlock = 1;
                t_up = 0;
                ui_message("SHIFT LOCKED: TAP OCT+ TO UNLOCK");
            } else
                t_up = fm1_ms;
            ui.force = 1;
        } else
            t_up = 0;
    }
    was = (uint8_t)now;
}
/* the lane a held pad edits (its white key the lowest held), NLANE none */
static uint32_t bz_held_lane(void)
{
    uint32_t i, pad;
    if (bz_keys)
        return NLANE;
    for (i = 0; i < 4u; i++)
        if ((bz_pads >> i) & 1u && (pad = bz_pad_of(i)) < 16u)
            return pad % NLANE;
    return NLANE;
}
/* the DRUM track's lanes made editable: its slots set as designed (64) once, then marked */
static void bz_lanes_ready(track_t *t)
{
    uint32_t i;
    if (t->p[BZ_LANE_MARK] == BZ_LANE_MAGIC)
        return;
    for (i = 0; i < 2u * NLANE; i++)
        t->p[BZ_LANE_TUNE + i] = 64;
    t->p[BZ_LANE_MARK] = BZ_LANE_MAGIC;
}
static int32_t bz_lane_get(const track_t *t, uint32_t slot)
{
    return t->p[BZ_LANE_MARK] == BZ_LANE_MAGIC ? t->p[slot] : 64;
}

/* the KEYS track: kept while it plays notes; else the selected one if it does, else the first that does */
static uint32_t bz_keys_track(void)
{
    uint32_t i;
    if (bz_notes_track(&trk[bz_ktrk % NTRK]))
        return bz_ktrk % NTRK;
    if (bz_notes_track(TSEL))
        return song.sel % NTRK;
    for (i = 0; i < NTRK; i++)
        if (bz_notes_track(&trk[i]))
            return i;
    return bz_ktrk % NTRK;
}

/* OCT- in the layer: BLAZE <-> KEYS */
/* ---- the settings BLAZE keeps (the RISER's sound, ECHO OUT's bars): 8 bytes of the DRUM row of the favourites that
 * no DRUM preset uses (its presets are far fewer than 192), marked; kept with the settings */
#define bz_cfg (&favorites.factory[ENGI_DRUM][24])
#define BZ_CFG_MAGIC 0xB7u
enum { BZC_MAGIC_I, BZC_TYPE, BZC_TOP, BZC_LVL, BZC_SPACE, BZC_TRK, BZC_EOB, BZC_DUCK };
_Static_assert(sizeof DRUM_PRESETS / sizeof DRUM_PRESETS[0] <= 192u, "BLAZE settings bytes in the DRUM favourites");
/* 0.7: 8 more of that row (16..23), marked their own way: [1] KEY LOCK (MIDI IN onto the key) 1 on, 0 off */
#define bz_cfg2 (&favorites.factory[ENGI_DRUM][16])
#define BZ_CFG2_MAGIC 0xB9u
enum { BZC2_MAGIC_I, BZC2_LOCK, BZC2_PP, BZC2_SHAPE, BZC2_CAPLEN };   /* (0.11: [3] ROLL SHAPE, [4] CAPTURE LENGTH:
                                                                       * 0 4 bars, 1 one, 2 two) */
static const uint8_t BZ_CAP_BARS[3] = {4u, 1u, 2u};
/* 0.9: the PAD PRESETS, a byte a lane in 8..15 of the row (valid once bz_cfg2[BZC2_PP] is BZ_PP_MAGIC): the rate + 1
 * (low 4 bits), the effect + 1 (high 4) */
#define bz_cfg_pp (&favorites.factory[ENGI_DRUM][8])
#define BZ_PP_MAGIC 0x5Au
/* 0.12: BUILD's and DROP's settings, 6 bytes of the PHYS row (2..7, past its 9 presets; MORPH A takes 8..27): [0]
 * marked, [1] BUILD drum (low 3 bits) curve (3..4) effect (5..6), [2] BUILD SWELL %, [3] DROP length (0..1) with
 * (2..3) hit (4..5), [4] DROP TOP %. And BLAZE's pads (hold one + ALGORITHM): the SAMPLE row's 28..31, a pad + 1 each */
#define bz_cfg3 (&favorites.factory[ENGI_PHYS][2])
#define BZ_CFG3_MAGIC 0xC7u
#define bz_cfg_slot (&favorites.factory[ENGI_SAMPLE][28])
enum { BZC3_MAGIC_I, BZC3_BUILD, BZC3_SWELL, BZC3_DROP, BZC3_TOP, BZC3_RSET };   /* (0.13 [5]: the RATE knob's steps) */
/* 0.13 the RATE knob's steps (ROLL's rates 1 1/4 .. 6 1/32): EVEN 1/8 1/16 1/32, ALL (triplets too), TRIPLETS */
static const uint8_t BZ_RK_SET[3][6] = {{2u, 4u, 6u}, {1u, 2u, 3u, 4u, 5u, 6u}, {3u, 5u}};
static const uint8_t BZ_RK_N[3] = {3u, 6u, 2u};
static const char *const BZ_RK_NAME[3] = {"EVEN", "ALL", "TRIPLETS"};
static uint8_t bz_rk_set;                           /* main: BZ_RK_SET's row */
static void bz_rk_to(uint32_t i);                   /* (below, by bz_knob) */
static const char *const BZR_NAME[BZR_NTYPE] = {"NOISE", "AIR", "SIREN", "TONE", "SYNTH"};
static const uint8_t BZ_EO_BARS[3] = {1u, 2u, 4u};
static void bzm_load(void);                         /* (0.11: MORPH's A and B, kept: below) */
/* the settings into the ISR's copies (an unmarked or old setting: the defaults first) */
static void bz_cfg_apply(void)
{
    rd_cfg_apply();                                 /* (0.14 READY ROLLS: blaze_roll_ui.c) */
    uint8_t *c = bz_cfg;
    if (c[BZC_MAGIC_I] != BZ_CFG_MAGIC) {
        c[BZC_TYPE] = BZR_NOISE;
        c[BZC_TOP] = 100;
        c[BZC_LVL] = 100;
        c[BZC_SPACE] = 32;
        c[BZC_TRK] = 1;
        c[BZC_EOB] = 1;
        c[BZC_DUCK] = 0;
        c[BZC_MAGIC_I] = BZ_CFG_MAGIC;
    }
    bz_rs_type = (uint8_t)(c[BZC_TYPE] % BZR_NTYPE);
    bz_rs_top = (uint8_t)(c[BZC_TOP] & 127u);
    bz_rs_lvl = (uint8_t)(c[BZC_LVL] & 127u);
    bz_rs_space = (uint8_t)(c[BZC_SPACE] & 127u);
    bz_rs_trk = (uint8_t)(c[BZC_TRK] % NTRK);
    bz_eo_bars = BZ_EO_BARS[c[BZC_EOB] % 3u];
    bz_duck = (uint8_t)(c[BZC_DUCK] > 100u ? 100u : c[BZC_DUCK]);
    c = bz_cfg2;
    if (c[BZC2_MAGIC_I] != BZ_CFG2_MAGIC) {
        memset(c, 0, 8);
        c[BZC2_LOCK] = 1;
        c[BZC2_MAGIC_I] = BZ_CFG2_MAGIC;
    }
    bz_lock = (uint8_t)(c[BZC2_LOCK] != 0u);
    if (c[BZC2_PP] != BZ_PP_MAGIC) {
        memset(bz_cfg_pp, 0, NLANE);
        c[BZC2_PP] = BZ_PP_MAGIC;
    }
    {
        uint32_t l;
        for (l = 0; l < NLANE; l++) {
            uint32_t r = bz_cfg_pp[l] & 15u, f = bz_cfg_pp[l] >> 4;
            rl_pp_rate[l] = (uint8_t)(r <= RL_NRATE ? r : 0u);
            rl_pp_fx[l] = (uint8_t)(f <= BZ_NFX ? f : 0u);
        }
    }
    rl_shape = (uint8_t)(c[BZC2_SHAPE] % RLS_N);
    bz_cap_bars = BZ_CAP_BARS[c[BZC2_CAPLEN] % 3u];
    bzm_load();
    c = bz_cfg3;                                    /* (0.12: BUILD's, DROP's) */
    if (c[BZC3_MAGIC_I] != BZ_CFG3_MAGIC) {
        c[BZC3_BUILD] = 1u;                         /* (SNARE, 1/4 > 1/32, no effect) */
        c[BZC3_SWELL] = 100u;
        c[BZC3_DROP] = 0u;                          /* (to the phrase, BACKSPIN, nothing on the 1) */
        c[BZC3_TOP] = 70u;
        c[BZC3_RSET] = 0u;                          /* (0.13: STRAIGHT) */
        c[BZC3_MAGIC_I] = BZ_CFG3_MAGIC;
    }
    bz_b_lane = (uint8_t)(c[BZC3_BUILD] & 7u);
    bz_b_curve = (uint8_t)((c[BZC3_BUILD] >> 3) & 3u);
    bz_b_fx = (uint8_t)((c[BZC3_BUILD] >> 5) & 3u);
    bz_b_swell = (uint8_t)(c[BZC3_SWELL] > 100u ? 100u : c[BZC3_SWELL]);
    bz_d_len = (uint8_t)(c[BZC3_DROP] & 3u);
    bz_d_extra = (uint8_t)((c[BZC3_DROP] >> 2) & 3u);
    bz_d_hit = (uint8_t)((c[BZC3_DROP] >> 4) & 3u);
    bz_d_top = (uint8_t)(c[BZC3_TOP] > 100u ? 100u : c[BZC3_TOP]);
    bz_rk_set = (uint8_t)(c[BZC3_RSET] % 3u);
    {
        uint32_t i;
        for (i = 0; i < 4u; i++)
            bz_pad_slot[i] = (uint8_t)(bz_cfg_slot[i] <= 16u ? bz_cfg_slot[i] : 0u);
    }
}

static const char *rl_lane_name(const track_t *t, uint32_t l);   /* (blaze_roll_draw.c) */
/* 0.9: a PAD PRESET changed (the ISR): kept with the settings, and said */
static void bz_pp_pass(void)
{
    uint32_t l = bz_pp_lane % NLANE, r, f;
    char m[40];
    if (!bz_pp_dirty)
        return;
    bz_pp_dirty = 0;
    r = rl_pp_rate[l];
    f = rl_pp_fx[l];
    bz_cfg_pp[l] = (uint8_t)(r | f << 4);
    lys.rp_dirty = 1;
    str_cpy(m, rl_lane_name(rl_track(), l), sizeof m);
    str_cpy(m + str_len(m), r || f ? ":" : ": PLAIN AGAIN", sizeof m - str_len(m));
    if (r) {
        str_cpy(m + str_len(m), " ROLL ", sizeof m - str_len(m));
        str_cpy(m + str_len(m), RL_RATE_CELL[r - 1u], sizeof m - str_len(m));
    }
    if (f) {
        str_cpy(m + str_len(m), r ? " + " : " ", sizeof m - str_len(m));
        str_cpy(m + str_len(m), BZ_FX_NAME[f - 1u], sizeof m - str_len(m));
    }
    ui_message(m);
    ui.force = 1;
}

/* ---- 0.7: KITS. SHIFT + white 1..8: KIT 1..8, the DRUM track's sound (its KIT, each drum's TUNE DECAY LEVEL, TONE
 * SNAP DRIVE ...) in the user presets U25..U32, named "KIT 1".."KIT 8": a tap loads it (playing too), held a second
 * saves it there (stopped: no flash erase while playing). A slot holding another sound is not written over ---- */
#define BZ_KIT0 (UP_SLOTS - 8u)
#define BZ_KIT_SAVE_MS 900u
static struct { uint8_t k, saved, key; uint32_t t; } bzkit;   /* k: the KIT key held + 1; key: its key (held: fm1_in) */
static int bz_kit_mine(uint32_t k)                  /* slot k empty or one of BLAZE's KITs */
{
    char b[16];
    if (!up_used(k))
        return 1;
    up_name(k, b);
    return b[0] == 'K' && b[1] == 'I' && b[2] == 'T' && b[3] == ' ';
}
static int bz_kit_used(uint32_t i) { return up_used(BZ_KIT0 + i) && bz_kit_mine(BZ_KIT0 + i); }
/* the DRUM track as the selected one for up_store / up_load (TSEL), then back */
static void bz_kit_do(uint32_t i, int save)
{
    track_t *d = rl_track();
    uint32_t k = BZ_KIT0 + i, sel = song.sel;
    char nm[8] = {'K', 'I', 'T', ' ', (char)('1' + i), 0}, m[28], u[4];
    int rc;
    up_slot_label(u, k);
    if (!drum_track(d)) {
        ui_message("NO DRUM TRACK");
        return;
    }
    if (!bz_kit_mine(k)) {
        str_cpy(m, u, sizeof m);
        str_cpy(m + str_len(m), " HOLDS ANOTHER SOUND", sizeof m - str_len(m));
        ui_message(m);
        return;
    }
    if (!save && (!up_used(k) || up_engine(k) != ENGI_DRUM)) {
        str_cpy(m, nm, sizeof m);
        str_cpy(m + str_len(m), " EMPTY: HOLD TO SAVE", sizeof m - str_len(m));
        ui_message(m);
        return;
    }
    song.sel = (uint8_t)(d - trk);
    rc = save ? up_store(k, nm) : up_load(k);
    song.sel = (uint8_t)sel;
    if (save && rc == 2)
        return;                                     /* (up_put said why: STOP TO SAVE, or the flash) */
    str_cpy(m, nm, sizeof m);
    if (rc == 3)
        rc = 0;                                     /* (no flash: kept in RAM, as SAVE does) */
    str_cpy(m + str_len(m), save ? (rc ? " NOT SAVED" : " SAVED IN ") : " LOADED", sizeof m - str_len(m));
    if (save && !rc)
        str_cpy(m + str_len(m), u, sizeof m - str_len(m));
    ui_message(m);
    ui.force = 1;
}
static void bz_kit_pass(void)                       /* each pass: the KIT key held a second saves, let go loads */
{
    uint32_t i;
    if (!bzkit.k)
        return;
    i = bzkit.k - 1u;
    if ((fm1_in.notes >> bzkit.key) & 1u) {           /* (the key itself: whichever of the ISR and here saw it first) */
        if (!bzkit.saved && fm1_ms - bzkit.t >= BZ_KIT_SAVE_MS) {
            bzkit.saved = 1;
            bz_kit_do(i, 1);
        }
        return;
    }
    if (!bzkit.saved)
        bz_kit_do(i, 0);
    bzkit.k = 0;
}
/* ---- 0.10 MORPH: two states, A and B, of COLOR THROW DEPTH and each track's FILTER LEVEL REVERB DELAY; held a
 * second (SHIFT + white 13 / 14) saves the state now as A / B (0.11: kept with the settings, so they are there after
 * power-off: A in the PHYS row of the favourites, B in the SAMPLE row's, bytes 8..27 of each, past their presets;
 * not DIGITAL's: settings_persist.c clears that row at every load); SHIFT + white 15 MORPH on: KNOB 2 (PITCH's) slides them all from A (0) to B (100).
 * 0.11 RIDE: SHIFT + black 1 / 2 slides it there over 8 / 16 bars from the next bar (to B; from B's half: to A) ---- */
#define BZM_N (3u + 4u * NTRK)
#define BZM_SAVE_MS 700u
static const uint8_t BZM_P[4] = {P_ED_FLT, P_LEVEL, P_REV, P_DLY};
static struct {
    int16_t v[2][BZM_N];
    uint8_t ok;                                     /* bit 0 A saved, bit 1 B */
    uint8_t on, pos;                                /* MORPH on; where it is, 0 (A) .. 100 (B) */
    uint8_t k, key, saved;                          /* the A / B key held (+ 1: 1 A, 2 B), its key, saved yet */
    uint32_t t;
} bzm;
static void bzm_take(int16_t *v)
{
    uint32_t i, j;
    v[0] = perf_k[0];
    v[1] = perf_k[2];
    v[2] = perf_k[3];
    for (i = 0; i < NTRK; i++)
        for (j = 0; j < 4u; j++)
            v[3u + 4u * i + j] = trk[i].p[BZM_P[j]];
}
#define BZM_MAGIC 0xC5u
_Static_assert(BZM_N <= 19u, "MORPH A / B: 19 bytes each in the favourites");
_Static_assert(SMP_NPRESETS <= 64, "MORPH B in the SAMPLE favourites' bytes 8..27");
static uint8_t *bzm_row(uint32_t ab) { return ab ? &favorites.factory[ENGI_SAMPLE][8] : &favorites.factory[ENGI_PHYS][8]; }
static void bzm_store(uint32_t ab)                  /* A (0) / B (1) into the settings (saved with them) */
{
    uint8_t *c = bzm_row(ab);
    uint32_t i;
    for (i = 0; i < BZM_N; i++)
        c[1u + i] = (uint8_t)(int8_t)clamp(bzm.v[ab][i], -128, 127);
    c[0] = BZM_MAGIC;
    lys.rp_dirty = 1;
}
static void bzm_load(void)
{
    uint32_t ab, i;
    for (ab = 0; ab < 2u; ab++) {
        const uint8_t *c = bzm_row(ab);
        if (c[0] != BZM_MAGIC || ((bzm.ok >> ab) & 1u))
            continue;                               /* (none kept; or one saved here since: that one) */
        for (i = 0; i < BZM_N; i++)
            bzm.v[ab][i] = (int8_t)c[1u + i];
        bzm.ok |= (uint8_t)(1u << ab);
    }
}
static struct {
    uint8_t on, bars, from, to;                     /* 0.11 RIDE: on, its bars, MORPH's place from / to */
    uint32_t s0, t0, len;                           /* .. playing: the step it starts on; stopped: when (ms), how long */
} bzride;
static void bzm_apply(void)                         /* the state at pos, between A and B */
{
    uint32_t i, j;
    int32_t p = bzm.pos;
    int16_t m[BZM_N];
    for (i = 0; i < BZM_N; i++)
        m[i] = (int16_t)(bzm.v[0][i] + (bzm.v[1][i] - bzm.v[0][i]) * p / 100);
    perf_k[0] = (int8_t)clamp(m[0], -100, 100);
    perf_k[2] = (int8_t)clamp(m[1], 0, 100);
    perf_k[3] = (int8_t)clamp(m[2], 0, 100);
    for (i = 0; i < NTRK; i++)
        for (j = 0; j < 4u; j++)
            trk[i].p[BZM_P[j]] = (int16_t)clamp(m[3u + 4u * i + j], TP[BZM_P[j]].min, TP[BZM_P[j]].max);
}
static void bz_cap_say(void);                       /* (0.11 LOOP CAPTURE: below) */
static void bz_cap_save_key(void);
/* 0.12 RIDE (black 7): MORPH glides from where it is to B (past halfway: to A) over 8 bars from the next bar;
 * tapped again quickly: 16; later: stopped */
static uint32_t bzride_tk;
static void bz_ride_key(void)
{
    char m[48];
    uint32_t in = (bz_s16 - bz_org) % 16u;
    if (bzride.on && bzride.bars == 8u && fm1_ms - bzride_tk < BZ_DTAP_MS) {
        bzride.bars = 16;                           /* (a double tap: 16 bars, from the same start) */
        bzride.len *= 2u;
        ui_message(bzride.to ? "RIDE TO B OVER 16 BARS" : "RIDE TO A OVER 16 BARS");
        return;
    }
    if (bzride.on) {
        bzride.on = 0;
        ui_message("RIDE STOPPED");
        return;
    }
    if ((bzm.ok & 3u) != 3u) {
        ui_message("RIDE: SAVE MORPH A AND B FIRST (SHIFT + WHITE 13, 14)");
        return;
    }
    bzride_tk = fm1_ms;
    bzride.bars = 8;
    bzride.from = bzm.pos;
    bzride.to = (uint8_t)(bzm.pos < 50u ? 100u : 0u);
    bzride.s0 = bz_s16 + (in || clk_pos ? 16u - in : 0u);
    bzride.t0 = fm1_ms;
    bzride.len = (uint32_t)bzride.bars * 4u * beat_samples() / 441u * 10u;   /* (ms; 32-bit: the main loop's code has
                                                                             * no 64-bit divide) */
    bzride.on = 1;
    bzm.on = 1;
    str_cpy(m, bzride.to ? "RIDE TO B OVER 8 BARS" : "RIDE TO A OVER 8 BARS", sizeof m);
    str_cpy(m + str_len(m), song.playing ? " FROM THE NEXT BAR" : "", sizeof m - str_len(m));
    ui_message(m);
}

/* 0.12 FOLLOW (SHIFT + black 5, or RAIL's): on, the FM-1 runs on the clock from RAIL's bridge (rekordbox's Link);
 * off, on its own at the tempo it had, playing on. Back on: the clock's tempo again (the bars may sit apart from
 * rekordbox's: RAIL's Start on next 1 lines them up). RAIL hears it (CC 33 on channel 16: 127 on, 0 off) */
static uint8_t bz_follow_src = 1;                   /* the external clock FOLLOW goes back to (USB 1, TRS 2) */
static void bz_tx_cc(uint32_t cc, uint32_t v)       /* a CC on channel 16 out, by the ISR */
{
    if ((uint8_t)(bz_tx_w - bz_tx_r) >= 8u)
        return;
    bz_tx[bz_tx_w % 8u] = 0x0Bu | 0xBFu << 8 | (cc & 127u) << 16 | (v & 127u) << 24;
    bz_tx_w = (uint8_t)(bz_tx_w + 1u);
}
static void bz_follow_set(int on)
{
    if (on == (song.g[G_CLOCK] != 0))
        return;
    bz_clk_soft = 1;                                /* (seq.c: the switch keeps playing) */
    if (!on) {
        uint32_t b = beat_samples();
        bz_follow_src = (uint8_t)song.g[G_CLOCK];
        if (b)
            song.g[G_BPM] = (int16_t)clamp((int32_t)((FS * 60u + b / 2u) / b), GP[G_BPM].min, GP[G_BPM].max);
        song.g[G_CLOCK] = 0;
    } else
        song.g[G_CLOCK] = (int16_t)(bz_follow_src ? bz_follow_src : 1u);
    bz_tx_cc(33u, on ? 127u : 0u);
    ui_message(on ? "FOLLOW ON: THE FM-1 FOLLOWS REKORDBOX" : "FOLLOW OFF: THE FM-1 PLAYS ON ITS OWN");
    ui.force = 1;
}

/* 0.12 CHORD's white 9..16: AUTO, NEW (progression), every 1 / 2 / 4 bars, MELODY, NEW (line), KEEP */
static void bz_gen_key(uint32_t i)
{
    static const char *const EV[3] = {"AUTO: A CHORD EVERY BAR", "AUTO: A CHORD EVERY 2 BARS",
                                      "AUTO: A CHORD EVERY 4 BARS"};
    if (i == 0u) {
        bz_g_auto = (uint8_t)!bz_g_auto;
        ui_message(bz_g_auto ? "AUTO ON: THE CHORDS PLAY THEMSELVES" : "AUTO OFF");
    } else if (i == 1u) {
        bz_g_prog = (uint8_t)((bz_g_prog + 1u) % BZG_NPROG);
        bz_g_auto = 1;
        ui_message("A NEW PROGRESSION");
    } else if (i <= 4u) {
        bz_g_every = (uint8_t)(1u << (i - 2u));
        ui_message(EV[i - 2u]);
    } else if (i == 5u) {
        bz_g_mel = (uint8_t)!bz_g_mel;
        ui_message(!bz_g_mel ? "MELODY OFF" : bz_notes_track(&trk[bz_ktrk % NTRK])
                   ? "MELODY ON THE KEYS TRACK: HOLD + KNOBS 1 / 2 FOR DENSITY, RANGE" : "MELODY: THE KEYS TRACK PLAYS NO NOTES");
    } else if (i == 6u) {
        bz_g_new = 1;
        bz_g_mel = 1;
        ui_message("A NEW MELODY");
    } else {
        bz_g_keep = (uint8_t)!bz_g_keep;
        ui_message(bz_g_keep ? "KEEP: THIS MELODY REPEATS" : "KEEP OFF: A NEW ONE EACH 2 BARS");
    }
    ui.force = 1;
}

/* 0.12: SELECT while the FM-1 follows an external clock (and BLAZE has been opened: RAIL is there): a nudge of the
 * Link tempo to RAIL, CC 34 on channel 16, 64 + tenths of a BPM (RAIL's bridge sets it; rekordbox and the FM-1
 * follow). 1: taken */
static int bz_select_turn(int32_t s)
{
    int32_t d;
    char m[24];
    if (!song.g[G_CLOCK] || !bz_ev_on)
        return 0;
    d = accel(EN_SELECT, s, 200);                   /* (tenths: a detent 0.1, fast 1 and more) */
    d = clamp(d, -63, 63);
    bz_tx_cc(34u, (uint32_t)(64 + d));
    str_cpy(m, "LINK TEMPO ", sizeof m);
    str_cpy(m + str_len(m), d > 0 ? "+" : "-", sizeof m - str_len(m));
    fmt_int(m + str_len(m), (d < 0 ? -d : d) / 10);
    str_cpy(m + str_len(m), ".", sizeof m - str_len(m));
    fmt_int(m + str_len(m), (d < 0 ? -d : d) % 10);
    ui_message(m);
    return 1;
}

/* 0.12: a key of BLAZE's own page pressed, doing f (bz_fn): what the main loop does, what it says */
static const char *const RLS_NAME[RLS_N] = {"FLAT", "ACCENT 2", "ACCENT 4", "FADE"};
static void bz_home_key(uint32_t k, uint32_t f)
{
    if (f >= BZF_KIT0 && f <= BZF_KIT7) {           /* 0.7: a KIT (bz_kit_pass: held, saved; let go, loaded) */
        bzkit.k = (uint8_t)(f - BZF_KIT0 + 1u);
        bzkit.key = (uint8_t)k;
        bzkit.saved = 0;
        bzkit.t = fm1_ms;
    } else if (f >= BZF_FX0 && f <= BZF_FX5)
        ui_message(BZ_FX_NAME[f - BZF_FX0]);
    else if (f >= BZF_RATE0 && f <= BZF_RATE5)
        ui_say("ROLL ", RL_RATE_NAME[f - BZF_RATE0 + 1u]);
    else if (f == BZF_B4 || f == BZF_B8)
        ui_message("BUILD: LET GO TO START IT, HOLD + KNOBS TO SET IT UP");
    else if (f == BZF_RISER)
        ui_message("RISER: LET GO TO START IT, HOLD + KNOBS FOR ITS SOUND");
    else if (f == BZF_DROP)
        ui_message(song.playing ? "DROP: LET GO TO ARM IT, HOLD + KNOBS TO SET IT UP"
                                : "DROP: PLAY FIRST (IT LANDS ON THE PHRASE); HOLD + KNOBS TO SET IT UP");
    else if (f == BZF_EOUT)
        ui_message("EXIT: LET GO FOR THE ECHO OUT, HOLD + KNOB 1 FOR ITS BARS");
    else if (f == BZF_SCAT)
        ui_message("SCATTER: KNOB 3 SETS HOW WILD");
    else if (f == BZF_SPIN)
        ui_message(bz_spin != BZB_IDLE ? "BACKSPIN ON THE NEXT BEAT, BACK ON THE 1" : "BACKSPIN OFF");
    else if (f == BZF_CAP)
        bz_cap_say();
    else if (f == BZF_MA || f == BZF_MB) {
        bzm.k = (uint8_t)(f == BZF_MA ? 1u : 2u);
        bzm.key = (uint8_t)k;
        bzm.saved = 0;
        bzm.t = fm1_ms;
    } else if (f == BZF_MORPH) {
        if ((bzm.ok & 3u) != 3u)
            ui_message(!(bzm.ok & 1u) ? "MORPH: SHIFT + HOLD WHITE 13 TO SAVE A FIRST"
                                     : "MORPH: SHIFT + HOLD WHITE 14 TO SAVE B FIRST");
        else {
            bzm.on = (uint8_t)!bzm.on;
            ui_message(bzm.on ? "MORPH ON: KNOB 2 SLIDES A -> B" : "MORPH OFF: KNOB 2 IS PITCH");
        }
    } else if (f == BZF_CAPLEN) {
        uint32_t c = (bz_cfg2[BZC2_CAPLEN] + 1u) % 3u;
        char m[40];
        bz_cfg2[BZC2_CAPLEN] = (uint8_t)c;
        bz_cap_bars = BZ_CAP_BARS[c];
        lys.rp_dirty = 1;
        str_cpy(m, "CAPTURE LENGTH: ", sizeof m);
        fmt_int(m + str_len(m), bz_cap_bars);
        str_cpy(m + str_len(m), bz_cap_bars == 1u ? " BAR" : " BARS", sizeof m - str_len(m));
        if (bz_cap_fit(bz_cap_bars) < bz_cap_bars)
            str_cpy(m + str_len(m), " (TOO SLOW: FEWER)", sizeof m - str_len(m));
        ui_message(m);
    } else if (f == BZF_CAPSAVE)
        bz_cap_save_key();
    else if (f == BZF_PREV || f == BZF_NEXT)
        ui_message(!bz_set_n ? "NO SET: LOAD ONE IN RAIL" : f == BZF_NEXT ? "NEXT SONG" : "PREVIOUS SONG");
    else if (f == BZF_RIDE)
        bz_ride_key();
    else if (f == BZF_ONE)
        ui_message("THE ONE: BAR 1 OF THE PHRASE");
    else if (f == BZF_TAP)
        glo_tap();
    else if (f == BZF_NDN || f == BZF_NUP)
        rl_nudge(k, f == BZF_NUP);
    else if (f == BZF_SHAPE) {
        rl_shape = (uint8_t)((rl_shape + 1u) % RLS_N);
        bz_cfg2[BZC2_SHAPE] = rl_shape;
        lys.rp_dirty = 1;
        ui_say("ROLL SHAPE ", RLS_NAME[rl_shape]);
    } else if (f == BZF_FOLLOW)
        bz_follow_set(song.g[G_CLOCK] == 0);
    ui.force = 1;
}

/* 0.12: what the ISR started or stopped on a key's release (BUILD RISER DROP EXIT FILL), said */
static void bz_fire_say(void)
{
    static uint8_t b, r, d, e;
    char m[48];
    if (bz_build != b) {
        if (!bz_build)
            ui_message(b == 1u ? "FILL DONE" : "BUILD DONE");
        else if (bz_b_fill)
            ui_message("FILL: SNARE ROLL TO THE 1");
        else if (bz_drop == BZD_IDLE) {
            str_cpy(m, "BUILD ", sizeof m);
            fmt_int(m + str_len(m), bz_build);
            str_cpy(m + str_len(m), " BARS FROM THE NEXT BAR", sizeof m - str_len(m));
            ui_message(m);
        }
        b = bz_build;
        ui.force = 1;
    }
    if (bz_riser != r) {
        if (bz_drop == BZD_IDLE && !bz_build)
            ui_message(!bz_riser ? "RISER DONE" : bz_riser == 16u ? "RISER 16 BARS" : "RISER 8 BARS FROM THE NEXT BAR");
        r = bz_riser;
        ui.force = 1;
    }
    if (bz_drop != d) {
        if (bz_drop == BZD_ARMED) {
            str_cpy(m, "DROP IN ", sizeof m);
            fmt_int(m + str_len(m), bz_dr_bars);
            str_cpy(m + str_len(m), " BARS (AGAIN: CANCEL)", sizeof m - str_len(m));
            ui_message(m);
        } else if (bz_drop == BZD_IDLE && d == BZD_RUN)
            ui_message("DROP!");
        else if (bz_drop == BZD_IDLE)
            ui_message("DROP CANCELLED");
        d = bz_drop;
        ui.force = 1;
    }
    if (bz_eo != e) {
        if (bz_eo == BZE_ARMED)
            ui_message("ECHO OUT FROM THE NEXT BAR");
        else if (bz_eo == BZE_IDLE && e == BZE_ARMED)
            ui_message("ECHO OUT CANCELLED");
        e = bz_eo;
        ui.force = 1;
    }
}

static void bz_ride_pass(void)                      /* each pass: RIDE moves MORPH; PHOSPHOR hears where it is */
{
    if (bzride.on) {
        uint32_t f, span = (uint32_t)bzride.bars * 16u * div_samples(2);
        if (song.playing) {
            uint32_t s = bz_s16, d = s - bzride.s0;
            f = (int32_t)d < 0 ? 0u : (d * div_samples(2) + clk_pos) / (span / 1000u + 1u);
        } else {
            uint32_t e = fm1_ms - bzride.t0;
            f = (e < bzride.len ? e : bzride.len) * 1000u / (bzride.len ? bzride.len : 1u);
        }
        f = f > 1000u ? 1000u : f;
        bzm.pos = (uint8_t)((int32_t)bzride.from + ((int32_t)bzride.to - (int32_t)bzride.from) * (int32_t)f / 1000);
        bzm_apply();
        if (f >= 1000u) {
            bzride.on = 0;
            ui_message(bzride.to ? "RIDE DONE: ON B" : "RIDE DONE: ON A");
        }
        ui.force = 1;
    }
    bz_ev_morph = (uint8_t)(bzm.on ? bzm.pos * 127u / 100u : 0u);
}

/* 0.11 LOOP CAPTURE, the main loop's: the line cleared after the loop (the echo back), what it does said */
static const char *bz_cap_state_name(void)
{
    static const char *const N[8] = {"OFF", "FROM THE NEXT BAR", "RECORDING", "SOLO: THE LOOP ALONE",
                                     "MIX: THE TRACKS OVER IT", "OUT", "OFF", "SAVING"};
    return N[bz_cap & 7u];
}
static void bz_cap_say(void)
{
    char m[48];
    str_cpy(m, "CAPTURE ", sizeof m);
    if (bz_cap == BZC_ARMED) {
        fmt_int(m + str_len(m), bz_cap_fit(bz_cap_bars));
        str_cpy(m + str_len(m), bz_cap_fit(bz_cap_bars) == 1u ? " BAR " : " BARS ", sizeof m - str_len(m));
    } else if ((bz_cap == BZC_SOLO || bz_cap == BZC_MIX) && bz_cap_pend)
        str_cpy(m + str_len(m), bz_cap_pend == BZC_MIX ? "MIX " : "OUT ", sizeof m - str_len(m));
    str_cpy(m + str_len(m), (bz_cap == BZC_SOLO || bz_cap == BZC_MIX) && bz_cap_pend ? "FROM THE NEXT BAR"
                            : bz_cap_state_name(), sizeof m - str_len(m));
    ui_message(m);
}
static int (*bz_cap_saver)(void);                   /* blaze_cap_save.c (the device: main.c sets it), 0: none */
static void bz_cap_save_key(void)                   /* SHIFT + white 16: the loop into a USR slot */
{
    int rc;
    char m[40];
    if (bz_cap != BZC_SOLO && bz_cap != BZC_MIX) {
        ui_message("SAVE: CAPTURE A LOOP FIRST (SHIFT + WHITE 11)");
        return;
    }
    if (song.playing) {
        ui_message("SAVE: STOP FIRST (THE FLASH WRITE PAUSES THE SOUND)");
        return;
    }
    if (bz_cap_pend) {
        ui_message("SAVE: WAIT FOR THE BAR");
        return;
    }
    if (!bz_cap_saver) {
        ui_message("SAVE: NOT ON THIS BUILD");
        return;
    }
    rc = bz_cap_saver();
    if (rc > 0) {
        str_cpy(m, "LOOP SAVED TO USR", sizeof m);
        fmt_int(m + str_len(m), rc);
        str_cpy(m + str_len(m), ": SAMPLE / SLICE", sizeof m - str_len(m));
        ui_message(m);
    } else
        ui_message(rc == -1 ? "SAVE: USR SLOTS FULL (CLEAR ONE IN THE EDITOR)" : "SAVE FAILED");
}
static void bz_cap_pass(void)
{
    static uint8_t last;
    if (bz_cap == BZC_CLEAR) {                      /* (the loop gone: the line silent, then the echo again) */
        memset(dly_buf, 0, sizeof dly_buf);
        fx.dly_lp = 0;
        fx_dly_off = 0;
        bz_cap = BZC_IDLE;
    }
    if (bz_cap != last) {
        uint8_t was = last;
        last = bz_cap;
        if (bz_cap == BZC_REC || bz_cap == BZC_SOLO || bz_cap == BZC_MIX || (bz_cap == BZC_IDLE && was != BZC_ARMED &&
                                                                               was != BZC_SAVE))
            bz_cap_say();
        ui.force = 1;
    }
}
static void bzm_pass(void)                          /* each pass: an A / B key held long enough saves; let go early: how */
{
    if (!bzm.k)
        return;
    if ((fm1_in.notes >> bzm.key) & 1u) {
        if (!bzm.saved && fm1_ms - bzm.t >= BZM_SAVE_MS) {
            bzm_take(bzm.v[bzm.k - 1u]);
            bzm.ok |= (uint8_t)bzm.k;
            bzm_store(bzm.k - 1u);
            bzm.saved = 1;
            if (bzm.k == 1u)
                bzm.pos = 0;
            ui_message(bzm.k == 1u ? "MORPH A SAVED" : "MORPH B SAVED");
            ui.force = 1;
        }
        return;
    }
    if (!bzm.saved)
        ui_message(bzm.k == 1u ? "HOLD TO SAVE MORPH A" : "HOLD TO SAVE MORPH B");
    bzm.k = 0;
}

static uint32_t bz_slice_first(void)               /* SLICE's track: the selected one if it plays SLICE, else the first */
{
    uint32_t i;
    if (bz_slice_trk(TSEL))
        return song.sel;
    for (i = 0; i < NTRK; i++)
        if (bz_slice_trk(&trk[i]))
            return i;
    return bz_strk % NTRK;
}
static void bz_keys_toggle(void)                    /* (OCT-: BLAZE -> KEYS -> CHORD -> LOOPS -> SLICE -> BLAZE) */
{
    if (!bz_keys && !bz_chord && !bz_loops && !bz_slice && !bz_rb) {
        bz_ktrk = (uint8_t)bz_keys_track();
        bz_keys = 1;
    } else if (bz_keys) {
        bz_keys = 0;
        if (!bz_notes_track(&trk[bz_ctrk % NTRK]))
            bz_ctrk = (uint8_t)bz_keys_track();
        bz_chord = 1;
    } else if (bz_chord) {
        bz_chord = 0;
        bz_loops = 1;
    } else if (bz_loops) {
        bz_loops = 0;
        bz_strk = (uint8_t)bz_slice_first();
        bz_sl_ok = (uint8_t)bz_slice_trk(&trk[bz_strk % NTRK]);
        bz_slice = 1;
    } else if (bz_slice) {
        bz_slice = 0;
        bz_rb = 1;
    } else {
        bz_rb = 0;
    }
    ui_message(bz_keys ? "KEYS: THE KEYS PLAY IN KEY" : bz_chord ? "CHORD: THE KEY'S SEVEN CHORDS"
               : bz_loops ? "LOOPS: LAYERS IN AND OUT ON THE BEAT"
               : bz_slice ? (bz_sl_ok ? "SLICE: THE SAMPLE ON THE KEYS" : "SLICE: NO SLICE TRACK (SHIFT + T1..T4 MAKES ONE)")
               : bz_rb ? "REKORDBOX: THE KEYS SEND MIDI (MIDI LEARN)"
               : "BLAZE: PADS AND EFFECTS");
    ui.force = 1;
}

/* ---- LOOPS' black keys (the main loop): QUANTIZE, FADE, DUCK, RAMP, CANCEL; THE ONE is the ISR's ---- */
static const char *const BZ_LOOPS_BLACK[11] = {"NOW", "BAR", "4 BARS", "PHRASE", "FADE 2", "FADE 4", "FADE 8", "DUCK",
                                               "RAMP", "CANCEL", "THE 1"};
static const char *const BZ_LOOPS_WHITE[4] = {"ALL IN", "DRUMS", "BREAK", "FROM 1"};
static struct {
    uint8_t on, bars;                           /* RAMP: gliding; over its bars (4 8 16 32) */
    int16_t from, to;                           /* .. the tempo it left, the one it goes to */
    uint32_t s0;                                /* .. the 1/16 it began on (bz_s16) */
    uint8_t duck_last;                          /* DUCK's AMOUNT before it was turned off */
} bzl = {0, 8, 0, 128, 0, 50};
static void bz_loops_black(uint32_t p)
{
    static const uint8_t FB[3] = {2u, 4u, 8u};
    static const char *const QN[4] = {"QUANTIZE: NOW", "QUANTIZE: THE NEXT BAR", "QUANTIZE: THE NEXT 4 BARS",
                                      "QUANTIZE: THE NEXT PHRASE"};
    if (p < 4u) {
        bz_lp_q = (uint8_t)p;
        ui_message(QN[p]);
    } else if (p < 7u) {
        bz_lp_fbars = FB[p - 4u];
        ui_message(p == 4u ? "FADES OVER 2 BARS" : p == 5u ? "FADES OVER 4 BARS" : "FADES OVER 8 BARS");
    } else if (p == 7u) {
        if (bz_duck) {
            bzl.duck_last = bz_duck;
            bz_duck = 0;
        } else {
            bz_duck = bzl.duck_last ? bzl.duck_last : 50u;
        }
        bz_cfg[BZC_DUCK] = bz_duck;
        lys.rp_dirty = 1;
        ui_message(bz_duck ? "DUCK ON: THE REST DIPS UNDER THE KICK" : "DUCK OFF");
    } else if (p == 8u) {
        if (bzl.on) {
            bzl.on = 0;
            ui_message("RAMP STOPPED");
        } else if (song.g[G_CLOCK]) {
            ui_message("EXTERNAL CLOCK SETS THE TEMPO");
        } else {
            bzl.on = 1;
            bzl.from = song.g[G_BPM];
            bzl.s0 = bz_s16;
            ui_message("RAMP: GLIDING TO THE TEMPO SET");
        }
    } else if (p == 9u) {
        bz_lp_cancel = 1;
        ui_message("CHANGES CANCELLED");
    } else if (p == 10u) {
        ui_message("THE ONE: BAR 1 OF THE PHRASE");
    }
    ui.force = 1;
}
/* RAMP, each pass: the tempo from where it was to where it goes, along the bars (playing: by the phrase's 1/16s) */
static void bz_ramp_pass(void)
{
    uint32_t steps, done;
    if (!bzl.on)
        return;
    if (song.g[G_CLOCK]) {
        bzl.on = 0;
        return;
    }
    if (!song.playing)
        return;
    steps = (uint32_t)bzl.bars * 16u;
    done = bz_s16 - bzl.s0;
    if (done >= steps) {
        song.g[G_BPM] = bzl.to;
        bzl.on = 0;
        ui_message("RAMP DONE");
        return;
    }
    song.g[G_BPM] = (int16_t)(bzl.from + ((int32_t)bzl.to - bzl.from) * (int32_t)done / (int32_t)steps);
}

/* white key at place p (0..15) held now */
static int bz_white_held(uint32_t p)
{
    uint32_t k;
    for (k = 0; k < 27u; k++)
        if (!key_black(k) && key_place(k) == p)
            return (int)((fm1_in.notes >> k) & 1u);
    return 0;
}
/* 0.12: BLAZE's own page up (not KEYS CHORD LOOPS SLICE REKORDBOX) */
static int bz_home(void) { return !bz_keys && !bz_chord && !bz_loops && !bz_slice && !bz_rb; }
/* a key held now doing f (its press's, the ISR's bz_kfn): its number + 1, 0 none */
static uint32_t bz_fn_held(uint32_t f)
{
    uint32_t k;
    for (k = 0; k < 27u; k++)
        if (((fm1_in.notes >> k) & 1u) && bz_kfn[k] == f)
            return k + 1u;
    return 0;
}
static int bz_riser_held(void) { return bz_home() && bz_fn_held(BZF_RISER); }
static int bz_eo_held(void) { return bz_home() && bz_fn_held(BZF_EOUT); }
static int bz_build_held(void) { return bz_home() && (bz_fn_held(BZF_B4) || bz_fn_held(BZF_B8)); }
static int bz_drop_held(void) { return bz_home() && bz_fn_held(BZF_DROP); }
/* a knob turned while one of these is held: its release fires nothing (the ISR's bz_fn_up) */
static void bz_mark_edit(void)
{
    static const uint8_t F[5] = {BZF_B4, BZF_B8, BZF_RISER, BZF_DROP, BZF_EOUT};
    uint32_t i, k;
    for (i = 0; i < 5u; i++)
        if ((k = bz_fn_held(F[i])) != 0u)
            bz_ked |= 1u << (k - 1u);
}

/* CHORD: chord degree d's name on track t ("Am", "G7", "Bdim", "Cmaj7"; root position) */
static void bz_chord_name(const track_t *t, uint32_t d, char *out, uint32_t len)
{
    uint8_t v[BZC_N];
    uint32_t n = bzc_build(t, d, v, bz_ch_7th, 0, 0), a, b, c = 0;
    const char *q;
    out[0] = 0;
    if (n < 3u)
        return;
    a = (uint32_t)(v[1] - v[0]);
    b = (uint32_t)(v[2] - v[0]);
    if (n > 3u)
        c = (uint32_t)(v[3] - v[0]);
    q = a == 4u && b == 7u ? (c == 11u ? "maj7" : c == 10u ? "7" : "")
      : a == 3u && b == 7u ? (c == 10u ? "m7" : c == 11u ? "mM7" : "m")
      : a == 3u && b == 6u ? (c == 10u ? "m7b5" : c == 9u ? "dim7" : "dim")
      : a == 4u && b == 8u ? "aug" : a == 5u && b == 7u ? "sus4" : a == 2u && b == 7u ? "sus2" : "?";
    str_cpy(out, (t->p[P_SCALE] == 1 ? BZ_MAJ : BZ_MIN)[v[0] % 12u], len);
    str_cpy(out + str_len(out), q, len - str_len(out));
}
static const char *const BZ_CHORD_BLACK[11] = {"HOLD", "1/4", "1/8", "1/16", "GATE", "7TH", "INVERT", "OCT-", "OCT+",
                                               "TRACK", "ARP"};
static const char *const BZ_GATE_NAME[4] = {"STRAIGHT", "SKIP", "SHUFFLE", "TRESILLO"};

/* CHORD: a black key (place p) */
static void bz_chord_black(uint32_t p)
{
    track_t *t = &trk[bz_ctrk % NTRK];
    uint32_t i;
    if (p <= 4u) {
        bz_ch_style = (uint8_t)p;
        ui_message(p == 4u ? "CHORD GATE" : p ? "CHORD STABS" : "CHORD HELD");
    } else if (p == 5u) {
        bz_ch_7th = (uint8_t)!bz_ch_7th;
        ui_message(bz_ch_7th ? "SEVENTH CHORDS" : "THREE-NOTE CHORDS");
    } else if (p == 6u) {
        bz_ch_inv = (uint8_t)((bz_ch_inv + 1u) % (bz_ch_7th ? 4u : 3u));
        ui_message(!bz_ch_inv ? "ROOT POSITION" : bz_ch_inv == 1u ? "1ST INVERSION" : bz_ch_inv == 2u ? "2ND INVERSION"
                                                                                    : "3RD INVERSION");
    } else if (p == 7u || p == 8u) {
        bz_ch_oct = (int8_t)clamp(bz_ch_oct + (p == 8u ? 1 : -1), -2, 2);
        ui_message(p == 8u ? "CHORDS OCTAVE UP" : "CHORDS OCTAVE DOWN");
    } else if (p == 9u) {                           /* the next track whose keys play notes */
        for (i = 1; i <= NTRK; i++)
            if (bz_notes_track(&trk[(bz_ctrk + i) % NTRK])) {
                bz_ctrk = (uint8_t)((bz_ctrk + i) % NTRK);
                break;
            }
        {
            char m[16] = "CHORDS ON T1";
            m[11] = (char)('1' + bz_ctrk % NTRK);
            ui_message(m);
        }
    } else if (p == 10u) {
        t->p[P_AMODE] = (int16_t)(t->p[P_AMODE] ? 0 : 1);
        ui_message(t->p[P_AMODE] ? "CHORD ARP ON" : "CHORD ARP OFF");
    }
    bz_ch_dirty = 1;
    ui.force = 1;
}

/* ---- SLICE's black keys (the main loop): T1..T4 (SHIFT: make it a SLICE track), MODE, REV, DIV, SRC, 17-32,
 * QUANTIZE, CHOP (the track's SLICER insert: OFF GATE STUT) ---- */
/* 0.9 REKORDBOX: what each key is for (rekordbox's MIDI LEARN decides; these are the names to learn them as) */
static const char *const BZ_RB_WHITE[16] = {"CUE A", "CUE B", "CUE C", "CUE D", "CUE E", "CUE F", "CUE G", "CUE H",
                                            "LOOP 1", "LOOP 2", "LOOP 4", "LOOP 8", "RELOOP", "JUMP -", "JUMP +", "SYNC"};
static const char *const BZ_RB_BLACK[10] = {"SMP 1", "SMP 2", "SMP 3", "SMP 4", "SMP 5", "SMP 6", "SMP 7", "SMP 8",
                                            "FX ON", "FX NEXT"};
static const char *const BZ_RB_KNOB[4] = {"DEPTH", "VOLUME", "BEAT", "FILTER"};   /* (Beat FX depth, sampler volume, Beat FX beat, a filter) */
enum { BZS_MODE = 4, BZS_REV, BZS_DIV, BZS_SRC, BZS_PAGE, BZS_Q, BZS_CHOP };
static const char *const BZS_Q_NAME[4] = {"Q OFF", "Q 1/16", "Q 1/8", "Q BEAT"};
static void bz_say_param(const track_t *t, uint32_t id, const char *pre)
{
    char v[12];
    const char *u;
    param_format(track_desc(t, id), t->p[id], v, &u);
    ui_say(pre, v);
}
static void bz_slice_black(uint32_t p)
{
    track_t *t = &trk[bz_strk % NTRK];
    char m[40];
    if (p < NTRK) {
        if (bz_shift() && !bz_slice_trk(&trk[p])) { /* SHIFT + T: that track becomes SLICE (UNDO brings its sound back) */
#if FELUCCA_SLICE
            set_engine_of(&trk[p], 13u);
#endif
            str_cpy(m, "T1 IS SLICE NOW (UNDO: ITS SOUND BACK)", sizeof m);
            m[1] = (char)('1' + p);
            ui_message(m);
        }
        if (bz_slice_trk(&trk[p])) {
            bz_strk = (uint8_t)p;
            bz_sl_page = 0;
            if (!bz_shift()) {
                str_cpy(m, "SLICES ON T1", sizeof m);
                m[11] = (char)('1' + p);
                ui_message(m);
            }
        } else if (!bz_shift()) {
            str_cpy(m, "T1 IS NOT SLICE: SHIFT + T1 MAKES IT", sizeof m);
            m[1] = m[35] = (char)('1' + p);
            ui_message(m);
        }
    } else if (!bz_slice_trk(t)) {
        ui_message("NO SLICE TRACK: SHIFT + T1..T4 MAKES ONE");
    } else if (p == BZS_MODE || p == BZS_DIV || p == BZS_SRC) {
        uint32_t id = p == BZS_MODE ? P_E4 : p == BZS_DIV ? P_E1 : P_E0;
        const param_desc_t *d = track_desc(t, id);
        t->p[id] = (int16_t)(t->p[id] >= d->max ? d->min : t->p[id] + 1);
        if (p == BZS_SRC && t->p[id] == d->max && !bz_slice_n(t))   /* (0.10: PIANO, last, has no material in the FM-1
                                                         * build: skipped) */
            t->p[id] = (int16_t)d->min;
        bz_say_param(t, id, p == BZS_MODE ? "MODE " : p == BZS_DIV ? "SLICES " : "SAMPLE ");
        if (p == BZS_SRC && !bz_slice_n(t))
            ui_message("SAMPLE EMPTY: LOAD IT WITH fm1_sample_upload.py");
        if (bz_slice_n(t) <= 16u)
            bz_sl_page = 0;
    } else if (p == BZS_REV) {
        t->p[P_E5] = (int16_t)!t->p[P_E5];
        ui_message(t->p[P_E5] ? "REVERSE ON" : "REVERSE OFF");
    } else if (p == BZS_PAGE) {
        if (bz_slice_n(t) > 16u) {
            bz_sl_page = (uint8_t)!bz_sl_page;
            ui_message(bz_sl_page ? "SLICES 17-32" : "SLICES 1-16");
        } else
            ui_message("16 SLICES OR FEWER: ONE PAGE");
    } else if (p == BZS_Q) {
        bz_sl_q = (uint8_t)((bz_sl_q + 1u) & 3u);
        ui_message(BZS_Q_NAME[bz_sl_q]);
    } else if (p == BZS_CHOP) {
        t->p[P_SLCR] = (int16_t)((t->p[P_SLCR] + 1) % 3);
        ui_message(t->p[P_SLCR] == SL_GATE ? "CHOP: GATE" : t->p[P_SLCR] == SL_STUT ? "CHOP: STUTTER" : "CHOP OFF");
    }
    bz_sl_ok = (uint8_t)bz_slice_trk(&trk[bz_strk % NTRK]);
    ui.force = 1;
}

/* a key of the BLAZE layer pressed (the main loop: what the ISR does not, and what it says) */
static void bz_ui_key(uint32_t k)
{
    uint32_t p = key_place(k);
    track_t *t;
    if (bz_rb) {                                    /* 0.9 REKORDBOX: what the key sends (the ISR sends it) */
        char m[40];
        uint32_t ch;
        if (key_black(k) && p == BZ_RB_DECK_KEY) {
            ui.force = 1;                           /* (the ISR switches it: bz_ui_pass says which) */
            return;
        }
        ch = key_black(k) ? BZ_RB_CH + 1u : BZ_RB_DECK_CH + 1u + (bz_rb_deck & 1u);
        str_cpy(m, key_black(k) ? BZ_RB_BLACK[p % 10u] : BZ_RB_WHITE[p % 16u], sizeof m);
        str_cpy(m + str_len(m), ": NOTE ", sizeof m - str_len(m));
        fmt_int(m + str_len(m), (int32_t)(key_black(k) ? 60u + p : 36u + p));
        str_cpy(m + str_len(m), " CH ", sizeof m - str_len(m));
        fmt_int(m + str_len(m), (int32_t)ch);
        ui_message(m);
        ui.force = 1;
        return;
    }
    if (bz_slice) {                                 /* SLICE: the black keys (the white ones play: seq.c) */
        if (key_black(k))
            bz_slice_black(p);
        else if (!bz_sl_ok)
            ui_message("NO SLICE TRACK: SHIFT + T1..T4 MAKES ONE");
        else
            ui.force = 1;
        return;
    }
    if (bz_loops) {                                 /* LOOPS: the black keys (the white ones: the ISR's) */
        if (key_black(k))
            bz_loops_black(p);
        else
            ui.force = 1;
        return;
    }
    if (bz_chord) {                                 /* CHORD: the black keys (the white ones: the ISR's) */
        if (key_black(k))
            bz_chord_black(p);
        else if (p >= 8u && p < 16u)                /* 0.12: the generator */
            bz_gen_key(p - 8u);
        else if (p < 7u && !bzc_fits(&trk[bz_ctrk % NTRK], p))
            ui_message("NOT IN THE NEXT KEY: IT WOULD CLASH");
        else
            ui.force = 1;
        return;
    }
    if (bz_keys) {                                  /* KEYS: the black keys (the white ones play: seq.c) */
        if (!key_black(k))
            return;
        t = &trk[bz_ktrk % NTRK];
        if (p == 0u && bz_shift()) {                /* 0.7: SHIFT + black 1: KEY LOCK (MIDI IN onto the key) */
            bz_lock = (uint8_t)!bz_lock;
            bz_cfg2[BZC2_LOCK] = bz_lock;
            lys.rp_dirty = 1;
            ui_message(bz_lock ? "KEY LOCK ON: MIDI IN PLAYS IN KEY" : "KEY LOCK OFF: MIDI IN AS PLAYED");
        } else if (p < NTRK) {
            if (!bz_notes_track(&trk[p])) {
                ui_message("THAT TRACK PLAYS NO NOTES");
                return;
            }
            bz_ktrk = (uint8_t)p;
            track_select(p);
        } else if (p == BZK_ARP) {
            t->p[P_AMODE] = (int16_t)(t->p[P_AMODE] ? 0 : 1);   /* (on: UP; ALGO picks another) */
            ui_message(t->p[P_AMODE] ? "ARP ON" : "ARP OFF");
        } else if (p == BZK_LATCH) {
            t->p[P_AHOLD] = (int16_t)!t->p[P_AHOLD];
            ui_message(t->p[P_AHOLD] ? "LATCH ON: THE ARP RUNS ON" : "LATCH OFF");
        } else if (p >= BZK_R8 && p <= BZK_R32) {
            t->p[P_ARATE] = (int16_t)(p - BZK_R8 + 1u);   /* (N_DIV: 1/8 1/16 1/32) */
            ui_say("ARP ", N_DIV[t->p[P_ARATE]]);
        } else if (p == BZK_ODN || p == BZK_OUP) {
            bz_koct = (int8_t)clamp(bz_koct + (p == BZK_OUP ? 1 : -1), -2, 2);
            ui_message(p == BZK_OUP ? "KEYS OCTAVE UP" : "KEYS OCTAVE DOWN");
        }
        ui.force = 1;
        return;
    }
    bz_home_key(k, bz_fn(k, (uint32_t)bz_shift(), bz_pads != 0u));
}

/* each pass: RELEASE's knobs, RAIL's key, RISER's 16 said */
static void bz_ui_pass(void)
{
    static uint8_t rs, cfg;
    if (!cfg) {
        cfg = 1;
        bz_cfg_apply();
    }
    if (bz_rel) {
        bz_rel = 0;
        perf_k[0] = perf_k[2] = perf_k[3] = 0;    /* (COLOR to the middle, THROW off, DEPTH full) */
        ui.force = 1;
        ui_message("RELEASE: ALL OFF");
        ui.force = 1;
    }
    if (bzu.cc_seq != bz_cc_seq) {
        static uint8_t keyc[11], knob[5] = {255, 255, 255, 255, 255};
        uint32_t i;
        bzu.cc_seq = bz_cc_seq;
        if (bz_cc[12]) {                            /* 0.11.1: RAIL's bar (CC 32), taken once */
            bz_bar_set(bz_cc[12]);
            bz_cc[12] = 0;
        }
        if (bz_cc[13]) {                            /* 0.12: RAIL's FOLLOW (CC 33: 127 on, 1 off), taken once */
            bz_follow_set(bz_cc[13] >= 64u);
            bz_cc[13] = 0;
        }
        if (bz_cc[16]) {                            /* 0.12: RAIL's action (CC 36), taken once */
            if (bz_cc[16] == 11u) {                 /* (SHIFT LOCK: here) */
                bz_shlock = (uint8_t)!bz_shlock;
                ui_message(bz_shlock ? "SHIFT LOCKED (RAIL)" : "SHIFT UNLOCKED (RAIL)");
                ui.force = 1;
            } else
                bz_req = bz_cc[16];
            bz_cc[16] = 0;
        }
        for (i = 0; i < 4u; i++)                    /* 0.12: RAIL's knobs (CC 50..53): COLOR MORPH DEPTH THROW;
                                                     * a knob's first value only noted (no jump to where it sits) */
            if (((bz_cc_knob >> i) & 1u) && bz_cc[30u + i] != knob[i]) {
                uint32_t v = bz_cc[30u + i];
                if (knob[i] != 255u) {
                    if (i == 0u)
                        perf_k[0] = (int8_t)clamp((int32_t)v * 200 / 127 - 100, -100, 100);
                    else if (i == 1u && (bzm.ok & 3u) == 3u) {
                        bzm.on = 1;
                        bzm.pos = (uint8_t)(v * 100u / 127u);
                        bzm_apply();
                        bzride.on = 0;
                    } else if (i == 2u)
                        perf_k[3] = (int8_t)(100 - (int32_t)(v * 100u / 127u));
                    else if (i == 3u)
                        perf_k[2] = (int8_t)(v * 100u / 127u);
                    ui.force = 1;
                }
                knob[i] = (uint8_t)v;
            }
        if (((bz_cc_knob >> 4) & 1u) && bz_cc[34] != knob[4]) {   /* 0.13: RAIL's ROLL RATE (CC 54), first noted */
            if (knob[4] != 255u)
                bz_rk_to(bz_cc[34] * BZ_RK_N[bz_rk_set % 3u] / 128u);
            knob[4] = bz_cc[34];
        }
        for (i = 0; i < 11u && keyc[i] == bz_cc[i]; i++)
            ;
        if (i < 11u) {                              /* (the key, the SET's song: only when they move) */
            for (i = 0; i < 11u; i++)
                keyc[i] = bz_cc[i];
            bz_key_apply();
            bz_song();
        }
    }
    bz_ramp_pass();
    bz_shlock_pass();                               /* (0.11.1 SHIFT LOCK) */
    bz_shift_on = (uint8_t)bz_shift();
    if (ui.layer == LAYER_BLAZE || ui.layer == LAYER_ROLL) {
        bz_ev_on = 1;                               /* (0.11 PHOSPHOR: from the first time BLAZE is open) */
        rl_ev_out = 1;                              /* (0.13: the hits' CC 49 too) */
    }
    bz_pp_pass();
    {                                               /* 0.9 REKORDBOX: DECK switched (the ISR): said */
        static uint8_t deck;
        if (bz_rb_deck != deck) {
            deck = bz_rb_deck;
            ui_message(deck ? "DECK 2: WHITE KEYS ON CHANNEL 15" : "DECK 1: WHITE KEYS ON CHANNEL 14");
            ui.force = 1;
        }
    }
    if (bz_slice)                                   /* (0.8: SLICE's track could change engine elsewhere) */
        bz_sl_ok = (uint8_t)bz_slice_trk(&trk[bz_strk % NTRK]);
    bz_kit_pass();
    bzm_pass();
    bz_ride_pass();                                 /* (0.11) */
    bz_cap_pass();
    bz_fire_say();                                  /* (0.12) */
    (void)rs;
}

/* KNOB k turned s: the four of the mode (see the top) */
/* 0.13: the RATE knob's place in its steps: the step at or above the rate it has (the last if none) */
static uint32_t bz_rk_pos(void)
{
    uint32_t i, n = BZ_RK_N[bz_rk_set % 3u];
    for (i = 0; i < n; i++)
        if (BZ_RK_SET[bz_rk_set % 3u][i] >= bz_rk_rate)
            return i;
    return n - 1u;
}
/* .. a rate for it (step i of its steps); the ISR latches it only with a pad held or rolling on its own */
static void bz_rk_to(uint32_t i)
{
    uint32_t n = BZ_RK_N[bz_rk_set % 3u];
    bz_rk_rate = BZ_RK_SET[bz_rk_set % 3u][i < n ? i : n - 1u];
    if (bz_pads || bz_padl)
        bz_rk_req = (uint8_t)(bz_rk_rate + 1u);
    lys.rp_dirty = 1;
    ui.force = 1;
}
/* .. knob 4 with a pad held or rolling: a step a detent; SHIFT with a pad held: STRAIGHT / ALL / TRIPLETS, kept */
static void bz_rk_knob(int32_t s)
{
    uint32_t p = bz_rk_pos(), n;
    if (bz_shift() && bz_pads) {
        bz_cfg_apply();
        bz_rk_set = (uint8_t)((bz_rk_set + (s > 0 ? 1u : 2u)) % 3u);
        bz_cfg3[BZC3_RSET] = bz_rk_set;
        ui_say("ROLL KNOB: ", BZ_RK_NAME[bz_rk_set]);
        bz_rk_to(bz_rk_pos());
        return;
    }
    n = BZ_RK_N[bz_rk_set % 3u];
    p = s > 0 ? (p + 1u < n ? p + 1u : p) : (p ? p - 1u : 0u);
    bz_rk_to(p);
}
static void bz_knob(uint32_t k, int32_t s)
{
    track_t *t = rl_track(), *kt = &trk[bz_ktrk % NTRK];
    uint32_t l = bz_held_lane();
    int16_t *vp;
    if (bz_build_held()) {                          /* 0.12 BUILD held: DRUM CURVE SWELL WITH, kept */
        uint8_t *c = bz_cfg3;
        int32_t d = s > 0 ? 1 : -1;
        uint32_t lane = c[BZC3_BUILD] & 7u, cur = (c[BZC3_BUILD] >> 3) & 3u, fx = (c[BZC3_BUILD] >> 5) & 3u;
        bz_cfg_apply();
        if (k == 0u)
            lane = (lane + (uint32_t)(d + NLANE)) % NLANE;
        else if (k == 1u)
            cur = (uint32_t)clamp((int32_t)cur + d, 0, BZBC_N - 1);
        else if (k == 2u)
            c[BZC3_SWELL] = (uint8_t)clamp((int32_t)c[BZC3_SWELL] + accel(EN_K3, s, 100), 0, 100);
        else
            fx = (uint32_t)clamp((int32_t)fx + d, 0, BZBX_N - 1);
        c[BZC3_BUILD] = (uint8_t)(lane | cur << 3 | fx << 5);
        bz_cfg_apply();
        bz_mark_edit();
        lys.rp_dirty = 1;
        return;
    }
    if (bz_drop_held()) {                           /* 0.12 DROP held: TOP LENGTH WITH ON THE 1, kept */
        uint8_t *c = bz_cfg3;
        int32_t d = s > 0 ? 1 : -1;
        uint32_t len = c[BZC3_DROP] & 3u, ex = (c[BZC3_DROP] >> 2) & 3u, hit = (c[BZC3_DROP] >> 4) & 3u;
        bz_cfg_apply();
        if (k == 0u)
            c[BZC3_TOP] = (uint8_t)clamp((int32_t)c[BZC3_TOP] + accel(EN_K1, s, 100), 0, 100);
        else if (k == 1u)
            len = (uint32_t)clamp((int32_t)len + d, 0, BZDL_N - 1);
        else if (k == 2u)
            ex = (uint32_t)clamp((int32_t)ex + d, 0, BZDX_N - 1);
        else
            hit = (uint32_t)clamp((int32_t)hit + d, 0, BZDH_N - 1);
        c[BZC3_DROP] = (uint8_t)(len | ex << 2 | hit << 4);
        bz_cfg_apply();
        bz_mark_edit();
        lys.rp_dirty = 1;
        return;
    }
    if (bz_riser_held()) {                          /* RISER held: TYPE TOP LEVEL SPACE (SYNTH: TRACK), kept */
        uint8_t *c = bz_cfg;
        bz_mark_edit();
        bz_cfg_apply();
        if (k == 0u)
            c[BZC_TYPE] = (uint8_t)clamp((int32_t)c[BZC_TYPE] + (s > 0 ? 1 : -1), 0, BZR_NTYPE - 1);
        else if (k == 3u && c[BZC_TYPE] == BZR_SYNTH)
            c[BZC_TRK] = (uint8_t)clamp((int32_t)c[BZC_TRK] + (s > 0 ? 1 : -1), 0, NTRK - 1);
        else
            c[k == 1u ? BZC_TOP : k == 2u ? BZC_LVL : BZC_SPACE] = (uint8_t)clamp(
                (int32_t)c[k == 1u ? BZC_TOP : k == 2u ? BZC_LVL : BZC_SPACE] + accel(EN_K1 + k, s, 127), 0, 127);
        bz_cfg_apply();
        lys.rp_dirty = 1;                           /* (kept when the layer lets go: ui_layer.c layer_let_go) */
        return;
    }
    if (bz_eo_held()) {                             /* ECHO OUT held: KNOB 1 its BARS (1 2 4), kept */
        bz_mark_edit();
        if (k == 0u) {
            bz_cfg[BZC_EOB] = (uint8_t)clamp((int32_t)bz_cfg[BZC_EOB] % 3 + (s > 0 ? 1 : -1), 0, 2);
            bz_cfg_apply();
            lys.rp_dirty = 1;
        }
        return;
    }
    if (bz_rb) {                                    /* 0.9 REKORDBOX: CC 1..4 on channel 13 (the ISR sends) */
        bz_rb_val[k] = (uint8_t)clamp((int32_t)bz_rb_val[k] + accel(EN_K1 + k, s, 127), 0, 127);
        return;
    }
    if (bz_slice) {                                 /* SLICE: START PITCH DECAY TONE of the SLICE track */
        static const uint8_t ID[4] = {P_E2, P_E3, P_E6, P_E7};
        track_t *st = &trk[bz_strk % NTRK];
        const param_desc_t *d;
        if (!bz_slice_trk(st))
            return;
        d = track_desc(st, ID[k]);
        vp = &st->p[ID[k]];
        *vp = (int16_t)clamp(*vp + accel(EN_K1 + k, s, d->max - d->min), d->min, d->max);
        return;
    }
    if (bz_loops) {                                 /* LOOPS: DUCK, RELEASE, RAMP TO, RAMP BARS */
        if (k == 0u) {
            bz_duck = (uint8_t)clamp(bz_duck + accel(EN_K1, s, 100), 0, 100);
            bz_cfg[BZC_DUCK] = bz_duck;
            lys.rp_dirty = 1;
        } else if (k == 1u) {
            bz_duck_ms = (uint16_t)clamp(bz_duck_ms + 10 * accel(EN_K2, s, 36), 40, 400);
        } else if (k == 2u) {
            bzl.to = (int16_t)clamp(bzl.to + accel(EN_K3, s, GP[G_BPM].max - GP[G_BPM].min), GP[G_BPM].min,
                                    GP[G_BPM].max);
        } else {
            static const uint8_t B[4] = {4u, 8u, 16u, 32u};
            uint32_t i;
            for (i = 0; i < 3u && B[i] != bzl.bars; i++)
                ;
            bzl.bars = B[clamp((int32_t)i + (s > 0 ? 1 : -1), 0, 3)];
        }
        return;
    }
    if (bz_chord && bz_white_held(13u)) {           /* 0.12 MELODY held: DENSITY, RANGE */
        if (k == 0u)
            bz_g_dens = (uint8_t)clamp((int32_t)bz_g_dens + accel(EN_K1, s, 100), 5, 100);
        else if (k == 1u)
            bz_g_range = (uint8_t)clamp((int32_t)bz_g_range + (s > 0 ? 1 : -1), 1, 2);
        ui.force = 1;
        return;
    }
    if (bz_chord) {                                 /* CHORD: FILTER LENGTH SPACE LEVEL of the CHORD track */
        track_t *ct = &trk[bz_ctrk % NTRK];
        static const uint8_t ID[4] = {P_ED_FLT, 0, P_REV, P_LEVEL};
        if (k == 1u) {
            bz_ch_len = (uint8_t)clamp(bz_ch_len + accel(EN_K2, s, 95), 5, 100);
            return;
        }
        vp = &ct->p[ID[k]];
        *vp = (int16_t)clamp(*vp + accel(EN_K1 + k, s, TP[ID[k]].max - TP[ID[k]].min), TP[ID[k]].min, TP[ID[k]].max);
        return;
    }
    if (bz_keys) {                                  /* KEYS: FILTER GATE RANGE SPACE of the KEYS track */
        static const uint8_t ID[4] = {P_ED_FLT, P_AGATE, P_AOCT, P_REV};
        vp = &kt->p[ID[k]];
        *vp = (int16_t)clamp(*vp + accel(EN_K1 + k, s, TP[ID[k]].max - TP[ID[k]].min), TP[ID[k]].min, TP[ID[k]].max);
        return;
    }
    /* 0.12: COLOR, PITCH (MORPH on: MORPH), DEPTH, THROW; SHIFT: PITCH DECAY TONE DRIVE */
    if (k == 0u && l == NLANE && !bz_shift()) {
        perf_k[0] = (int8_t)clamp(perf_k[0] + accel(EN_K1, s, 200), -100, 100);
        return;
    }
    if (k == 2u && l == NLANE && !bz_shift()) {     /* DEPTH (rises to the right) */
        perf_k[3] = (int8_t)clamp(perf_k[3] - accel(EN_K3, s, 100), 0, 100);
        return;
    }
    if (k == 3u && l == NLANE && bz_padl && !bz_shift()) {   /* 0.13: a pad rolling on its own: RATE */
        bz_rk_knob(s);
        return;
    }
    if (k == 3u && l == NLANE && !bz_shift()) {     /* THROW: the mix into the echo and the reverb */
        perf_k[2] = (int8_t)clamp(perf_k[2] + accel(EN_K4, s, 100), 0, 100);
        return;
    }
    if (k == 1u && l == NLANE && !bz_shift() && bzm.on) {   /* 0.10 MORPH: KNOB 2 slides A -> B */
        bzm.pos = (uint8_t)clamp((int32_t)bzm.pos + accel(EN_K2, s, 100), 0, 100);
        bzm_apply();
        bzride.on = 0;                              /* (0.11: the hand takes over from a RIDE) */
        return;
    }
    if (!drum_track(t)) {
        ui_message("NO DRUM TRACK");
        return;
    }
    if (l < NLANE) {                                /* a pad held: that drum's TUNE DECAY LEVEL; 0.13: RATE */
        if (k == 3u) {
            bz_rk_knob(s);
            return;
        }
        if (k < 2u) {
            bz_lanes_ready(t);
            vp = &t->p[(k ? BZ_LANE_DECAY : BZ_LANE_TUNE) + l];
        } else {
            vp = &t->p[P_LN0 + l];
        }
        *vp = (int16_t)clamp(*vp + accel(EN_K1 + k, s, 127), 0, 127);
        return;
    }
    {   /* DRUM's TUNE (PITCH); SHIFT: TUNE DECY TONE DRV (eng_drum.c) */
        static const uint8_t E[2][4] = {{0, 1, 0, 0}, {1, 3, 2, 7}};
        if (!bz_shift() && k != 1u)
            return;
        vp = &t->p[P_E0 + E[bz_shift()][k]];
        *vp = (int16_t)clamp(*vp + accel(EN_K1 + k, s, 127), 0, 127);
    }
}

/* ALGO in the layer: the KIT (KEYS: the arp's MODE, ARP on) */
static void bz_algo(int32_t s)
{
    track_t *t = bz_keys ? &trk[bz_ktrk % NTRK] : rl_track();
    const param_desc_t *d;
    int32_t v;
    if (bz_chord) {                                 /* CHORD: the gate's pattern */
        bz_ch_gate = (uint8_t)clamp((int32_t)bz_ch_gate + (s > 0 ? 1 : -1), 0, 3);
        ui_say("GATE ", BZ_GATE_NAME[bz_ch_gate]);
        ui.force = 1;
        return;
    }
    if (bz_keys) {
        d = &TP[P_AMODE];
        v = clamp(t->p[P_AMODE] + (s > 0 ? 1 : -1), 1, d->max);
        t->p[P_AMODE] = (int16_t)v;
        ui_say("ARP ", d->names[v]);
        ui.force = 1;
        return;
    }
    if (!drum_track(t)) {
        ui_message("NO DRUM TRACK");
        return;
    }
    {   /* 0.12: a pad held: that pad another drum (SNARE CLAP HATS TOM RIM BELL, KICK last), none the others have */
        static const uint8_t ORD[NLANE] = {1, 2, 3, 4, 5, 6, 7, 0};
        uint32_t i, j, n, cur, o, lane;
        for (i = 0; i < 4u && !bz_fn_held(BZF_PAD0 + i); i++)
            ;
        if (i < 4u) {
            char m[24];
            for (j = 0; j < 4u; j++)                /* (the four fixed as they are: one swapped, the rest stay put) */
                if (!bz_pad_slot[j] && bz_pad_of(j) < 16u)
                    bz_pad_slot[j] = (uint8_t)(bz_pad_of(j) + 1u);
            cur = bz_pad_of(i) % NLANE;
            for (o = 0; o < NLANE && ORD[o] != cur; o++)
                ;
            for (n = 1; n <= NLANE; n++) {
                lane = ORD[(o + (uint32_t)(s > 0 ? (int32_t)n : (int32_t)(NLANE * 8u - n))) % NLANE];
                for (j = 0; j < 4u && (j == i || !bz_pad_slot[j] || (bz_pad_slot[j] - 1u) % NLANE != lane); j++)
                    ;
                if (j == 4u)
                    break;
            }
            bz_pad_slot[i] = (uint8_t)(lane + 1u);
            for (j = 0; j < 4u; j++)
                bz_cfg_slot[j] = bz_pad_slot[j];
            lys.rp_dirty = 1;
            str_cpy(m, "PAD ", sizeof m);
            fmt_int(m + str_len(m), (int32_t)i + 1);
            str_cpy(m + str_len(m), ": ", sizeof m - str_len(m));
            str_cpy(m + str_len(m), rl_lane_name(t, lane), sizeof m - str_len(m));
            ui_message(m);
            ui.force = 1;
            return;
        }
    }
    d = track_desc(t, P_E0);
    v = enum_step(d, t->p[P_E0], clamp(t->p[P_E0] + (s > 0 ? 1 : -1), d->min, d->max));
    t->p[P_E0] = (int16_t)v;
    ui_say("KIT ", d->names[v]);
    ui.force = 1;
}

/* 0.12: a function's state on BLAZE's page, for its cell and its light: DIM (nothing to do), IDLE, TINT (latched,
 * armed, saved), FILL (on, running, held) */
enum { BZS_DIM, BZS_IDLE, BZS_TINT, BZS_FILL };
static int32_t bz_phrase(uint32_t *beat);
/* 0.14: samples s as bars to go (a part bar counts as one), 1..9 */
static uint32_t bz_bars_of(uint32_t s)
{
    uint32_t bar = 4u * beat_samples(), n = bar ? (s + bar - 1u) / bar : 1u;
    return n < 1u ? 1u : n > 9u ? 9u : n;
}
/* .. a / b as 0..1000 (no 64-bit maths in the main loop: both scaled down first) */
static int16_t bz_permille(uint32_t a, uint32_t b)
{
    a >>= 6;
    b >>= 6;
    return (int16_t)(b ? (a >= b ? 1000u : a * 1000u / b) : 0u);
}
/* 0.14: the bars before the DROP lands on its 1 (0: no drop) */
static uint32_t bz_drop_left(void)
{
    if (bz_drop == BZD_IDLE)
        return 0;
    return bz_bars_of(bz_dr_wait + (bz_dr_total > bz_dr_age ? bz_dr_total - bz_dr_age : 0u));
}
static uint32_t bz_fn_state(uint32_t f, uint32_t k)
{
    const track_t *t = rl_track();
    uint32_t fx = perf_kill ? 0u : perf_held | perf_latched, drum = (uint32_t)drum_track(t), pad, l, i;
    if (f >= BZF_PAD0 && f <= BZF_PAD3) {
        pad = bz_pad_of(f - BZF_PAD0);
        l = pad % NLANE;
        return pad >= 16u || !drum ? BZS_DIM : (((uint32_t)rl_extra | rl_pp | rlu.fl) >> pad) & 1u ? BZS_FILL
               : pad >= NLANE || rl_pp_rate[l] || rl_pp_fx[l] ? BZS_TINT : BZS_IDLE;
    }
    if (f >= BZF_FX0 && f <= BZF_FX5) {
        uint32_t e = BZ_FX[f - BZF_FX0];
        return !((perf_avail() >> e) & 1u) ? BZS_DIM : (fx >> e) & 1u ? BZS_FILL : BZS_IDLE;
    }
    if (f >= BZF_KIT0 && f <= BZF_KIT7) {
        i = f - BZF_KIT0;
        return bzkit.k == i + 1u ? BZS_FILL : !bz_kit_used(i) ? BZS_DIM : t->user == BZ_KIT0 + i + 1u ? BZS_TINT
               : BZS_IDLE;
    }
    if (f >= BZF_RATE0 && f <= BZF_RATE5) {
        i = f - BZF_RATE0 + 1u;
        return ((rl_rates & ~rl_latch) >> i) & 1u || (bz_build && bz_build_rate == i) ? BZS_FILL
               : (rl_latch >> i) & 1u ? BZS_TINT : BZS_IDLE;
    }
    if (f >= BZF_PPR0 && f <= BZF_PPR5) {
        pad = bz_pad_of(0);
        for (i = 0; i < 4u; i++)
            if ((bz_pads >> i) & 1u) {
                pad = bz_pad_of(i);
                break;
            }
        return pad < 16u && rl_pp_rate[pad % NLANE] == f - BZF_PPR0 + 2u ? BZS_FILL : BZS_IDLE;
    }
    if (f >= BZF_PPF0 && f <= BZF_PPF5) {
        pad = bz_pad_of(0);
        for (i = 0; i < 4u; i++)
            if ((bz_pads >> i) & 1u) {
                pad = bz_pad_of(i);
                break;
            }
        return pad < 16u && rl_pp_fx[pad % NLANE] == f - BZF_PPF0 + 1u ? BZS_FILL : BZS_IDLE;
    }
    switch (f) {
    case BZF_B4: case BZF_B8:
        return !drum ? BZS_DIM : bz_build == (f == BZF_B4 ? 4u : 8u) && !bz_b_fill && bz_drop == BZD_IDLE ? BZS_FILL
               : BZS_IDLE;
    case BZF_RISER: return bz_riser ? BZS_FILL : BZS_IDLE;
    case BZF_DROP: return bz_drop == BZD_RUN ? BZS_FILL : bz_drop == BZD_ARMED ? BZS_TINT : song.playing ? BZS_IDLE
                          : BZS_DIM;
    case BZF_CAP: return bz_cap == BZC_IDLE || bz_cap == BZC_CLEAR ? BZS_IDLE : bz_cap == BZC_ARMED || bz_cap_pend
                         ? BZS_TINT : BZS_FILL;
    case BZF_EOUT: return bz_eo != BZE_IDLE ? BZS_FILL : BZS_IDLE;
    case BZF_SCAT: return (fx >> PF_SCAT) & 1u ? BZS_FILL : BZS_IDLE;
    case BZF_SPIN: return bz_spin != BZB_IDLE ? BZS_FILL : BZS_IDLE;
    case BZF_CAPLEN: return BZS_IDLE;
    case BZF_CAPSAVE: return (bz_cap == BZC_SOLO || bz_cap == BZC_MIX) && !song.playing ? BZS_IDLE : BZS_DIM;
    case BZF_MA: case BZF_MB:
        return bzm.k == (f == BZF_MA ? 1u : 2u) ? BZS_FILL : (bzm.ok >> (f == BZF_MA ? 0 : 1)) & 1u ? BZS_TINT
               : BZS_IDLE;
    case BZF_PREV: case BZF_NEXT: return bz_set_n ? BZS_IDLE : BZS_DIM;
    case BZF_RIDE: return (bzm.ok & 3u) != 3u ? BZS_DIM : bzride.on ? BZS_FILL : BZS_IDLE;
    case BZF_MORPH: return (bzm.ok & 3u) != 3u ? BZS_DIM : bzm.on ? BZS_FILL : BZS_IDLE;
    case BZF_ONE: return song.playing ? BZS_IDLE : BZS_DIM;
    case BZF_REL: return BZS_IDLE;
    case BZF_FILL: return !drum ? BZS_DIM : bz_build && bz_b_fill ? BZS_FILL : BZS_IDLE;
    case BZF_TAP: case BZF_NDN: case BZF_NUP:
        return song.g[G_CLOCK] ? BZS_DIM : (rlu.nk >> k) & 1u ? BZS_FILL : BZS_IDLE;
    case BZF_SHAPE: return rl_shape ? BZS_TINT : BZS_IDLE;
    case BZF_FOLLOW: return song.g[G_CLOCK] ? BZS_FILL : BZS_IDLE;
    default: return BZS_DIM;
    }
}
/* KEYS: the scale's notes the white keys walk (bz_key_note's: in a blend, the shared ones) */
static uint32_t bz_keys_n(const track_t *t)
{
    uint32_t mask = scale_mask(t), n = 0, pc, r = (uint32_t)t->p[P_ROOT] % 12u, bl = bz_blend;
    for (pc = 0; pc < 12u; pc++)
        if ((mask >> pc) & 1u && (!bl || ((bl >> ((r + pc) % 12u)) & 1u)))
            n++;
    if (!n)
        for (pc = 0; pc < 12u; pc++)
            n += (mask >> pc) & 1u;
    return n;
}

/* key k's LED in the BLAZE layer: lit (held, latched, running, the value), breathing (can be pressed), dark */
static void bz_led(uint32_t k, uint32_t *on, uint32_t *can)
{
    uint32_t p = key_place(k);
    if (bz_rb) {                                    /* 0.9 REKORDBOX: every key can; held: lit; DECK 2: lit */
        *can = !key_black(k) || p < 11u;
        *on = key_black(k) && p == BZ_RB_DECK_KEY ? bz_rb_deck != 0u : (bz_rb_down >> k) & 1u;
        return;
    }
    if (bz_slice) {                                 /* SLICE: the slices there are breathe, a key held lit */
        const track_t *t = &trk[bz_strk % NTRK];
        if (!key_black(k)) {
            *can = bz_sl_ok && p + 16u * bz_sl_page < bz_slice_n(t);
            *on = (fm1_in.notes >> k) & 1u;
            return;
        }
        *can = p < 11u;
        *on = p < NTRK ? p == bz_strk % NTRK && bz_sl_ok : !bz_sl_ok ? 0u : p == BZS_MODE ? t->p[P_E4] != 0
              : p == BZS_REV ? t->p[P_E5] != 0 : p == BZS_PAGE ? bz_sl_page != 0u : p == BZS_Q ? bz_sl_q != 0u
              : p == BZS_CHOP && t->p[P_SLCR] != 0;
        return;
    }
    if (bz_loops) {
        if (!key_black(k)) {
            uint32_t t = p % 4u;
            *can = p < 16u;
            *on = p < 4u ? !trk[t].p[P_MUTE] : p < 8u ? bz_fd_len[t] != 0u : p < 12u && bz_fill_left[t] != 0u;
            return;
        }
        *can = p < 11u;
        *on = p < 4u ? bz_lp_q == p : p < 7u ? bz_lp_fbars == (2u << (p - 4u)) : p == 7u ? bz_duck != 0u
              : p == 8u && bzl.on;
        return;
    }
    if (bz_chord) {
        const track_t *t = &trk[bz_ctrk % NTRK];
        if (!key_black(k)) {
            *can = p < 16u;
            *on = p < 7u ? bz_ch_sel == p + 1u : p == 8u ? bz_g_auto != 0u : p >= 10u && p <= 12u
                  ? bz_g_every == (1u << (p - 10u)) : p == 13u ? bz_g_mel != 0u : p == 15u && bz_g_keep;
            return;
        }
        *can = p < 11u;
        *on = p <= 4u ? bz_ch_style == p : p == 5u ? bz_ch_7th != 0u : p == 6u ? bz_ch_inv != 0u : p == 7u ? bz_ch_oct < 0
              : p == 8u ? bz_ch_oct > 0 : p == 10u && t->p[P_AMODE] != 0;
        return;
    }
    if (bz_keys) {
        const track_t *t = &trk[bz_ktrk % NTRK];
        if (!key_black(k)) {                        /* every white key is in key: the home notes lit, a key held lit
                                                     * (0.12: the chord's notes brighter, bz_groups) */
            uint32_t n = bz_keys_n(t);
            *can = p < 16u;
            *on = *can && (((fm1_in.notes >> k) & 1u) || (n && p % n == 0u));
            return;
        }
        *can = p < NTRK ? (uint32_t)bz_notes_track(&trk[p]) : p <= BZK_OUP;
        *on = p < NTRK ? p == bz_ktrk % NTRK : p == BZK_ARP ? t->p[P_AMODE] != 0 : p == BZK_LATCH ? t->p[P_AHOLD] != 0
              : p >= BZK_R8 && p <= BZK_R32 ? t->p[P_ARATE] == (int16_t)(p - BZK_R8 + 1u)
              : p == BZK_ODN ? bz_koct < 0 : p == BZK_OUP && bz_koct > 0;
        return;
    }
    {                                               /* 0.12: BLAZE's own page, by what each key does now */
        uint32_t f = bz_fn(k, (uint32_t)bz_shift(), bz_pads != 0u), st = bz_fn_state(f, k);
        *can = st != BZS_DIM && f != BZF_NONE;
        *on = st >= BZS_TINT;
    }
}

/* the phrase now: bar 0..15, beat 0..3 (-1: stopped) */
static int32_t bz_phrase(uint32_t *beat)
{
    uint32_t s = bz_s16 - bz_org;
    *beat = (s / 4u) % 4u;
    return song.playing && clk_pos != CLK_START ? (int32_t)((s / 16u) % BZ_PHRASE) : -1;
}

/* the map's header: "[ENV] BLAZE", "[ENV] BLAZE SHIFT", "[ENV] KEYS" */
static const char *bz_head(void)
{
    static char h[28];
    uint32_t b, cam;
    char nm[16];
    str_cpy(h, bz_keys ? (bz_shift() ? "[ENV] KEYS SHIFT" : "[ENV] KEYS") : bz_chord ? "[ENV] CHORD" : bz_loops ? "[ENV] LOOPS" : bz_slice ? (bz_shift() ? "[ENV] SLICE SHIFT" : "[ENV] SLICE") : bz_rb ? "[ENV] REKORDBOX" : bz_build_held() ? "[ENV] BUILD SETUP" : bz_drop_held() ? "[ENV] DROP SETUP" : bz_riser_held() ? "[ENV] RISER SOUND"
               : bz_eo_held() ? "[ENV] ECHO OUT" : bz_held_lane() < NLANE ? "[ENV] BLAZE DRUM"
               : bz_shlock ? "[ENV] SHIFT LOCK" : bz_shift() ? "[ENV] BLAZE SHIFT" : "[ENV] BLAZE", sizeof h);
    if (!bz_keys && !bz_chord && !bz_loops && !bz_slice && !bz_rb && bz_drop != BZD_IDLE) {   /* 0.14 */
        uint32_t n = bz_drop_left();
        str_cpy(h, bz_drop == BZD_RUN && n <= 1u ? "[ENV] DROP IN 1" : "[ENV] DROP IN ", sizeof h);
        if (!(bz_drop == BZD_RUN && n <= 1u)) {
            nm[0] = (char)('0' + n);
            nm[1] = 0;
            str_cpy(h + str_len(h), nm, sizeof h - str_len(h));
        }
        return h;
    }
    if (!bz_chord && (cam = bz_camelot(&b, nm, sizeof nm)) != 0 && str_len(h) < 12u) {   /* (KEYS, LOOPS too) */
        str_cpy(h + str_len(h), "  ", sizeof h - str_len(h));   /* (BLAZE: the key in the header, the strip is the phrase) */
        fmt_int(nm, (int32_t)cam);
        str_cpy(nm + str_len(nm), b ? "B" : "A", sizeof nm - str_len(nm));
        str_cpy(h + str_len(h), nm, sizeof h - str_len(h));
        if (bz_blend && bz_cc[3] >= 1u && bz_cc[3] <= 12u) {   /* (a blend: + the next key) */
            fmt_int(nm, bz_cc[3]);
            str_cpy(nm + str_len(nm), bz_cc[4] >= 64u ? "B" : "A", sizeof nm - str_len(nm));
            str_cpy(h + str_len(h), "+", sizeof h - str_len(h));
            str_cpy(h + str_len(h), nm, sizeof h - str_len(h));
        }
    }
    return h;
}

/* the map's signature: what it shows */
static uint32_t bz_sig(void)
{
    track_t *t = rl_track();
    const track_t *kt = bz_key_track(), *k2 = &trk[bz_ktrk % NTRK];
    uint32_t beat, h, i;
    int32_t bar = bz_phrase(&beat);
    h = (uint32_t)rl_extra * 31u + (uint32_t)rlu.fl * 131u + (uint32_t)rl_rates * 7919u + (uint32_t)rl_live4 * 977u +
        (perf_held | perf_latched) * 13u + bz_build * 5u + bz_build_rate * 11u + bz_riser * 3u +
        rlu.nk * 17u + (uint32_t)song.g[G_CLOCK] * 19u + (uint32_t)drum_track(t) * 23u + (uint32_t)t->p[P_E0] * 29u +
        (uint32_t)(bar + 1) * 37u + beat * 41u + perf_avail() * 43u + bz_keys * 59u + (uint32_t)bz_shift() * 61u +
        bz_held_lane() * 67u + bz_ktrk * 71u + (uint32_t)(bz_koct + 2) * 73u + (bz_keys ? fm1_in.notes * 79u : 0u) +
        bz_chord * 83u + bz_ch_sel * 89u + bz_ch_style * 97u + bz_ch_7th * 101u + bz_ch_inv * 103u +
        (uint32_t)(bz_ch_oct + 2) * 107u + bz_ctrk * 109u + bz_ch_len * 113u + bz_ch_gate * 127u + bz_eo * 131u +
        (uint32_t)bz_riser_held() * 137u + (uint32_t)bz_eo_held() * 139u + bz_rs_type * 149u + bz_rs_top * 151u +
        bz_rs_lvl * 157u + bz_rs_space * 163u + bz_rs_trk * 167u + bz_eo_bars * 173u + bz_loops * 179u +
        bz_lp_q * 181u + bz_lp_fbars * 191u + bz_duck * 193u + bz_duck_ms * 197u + (uint32_t)bzl.on * 199u +
        (uint32_t)bzl.to * 211u + bzl.bars * 223u + bz_blend * 227u + bz_lp_restart * 229u + bz_set_n * 233u +
        fm1_in.notes * 239u + up_gen * 241u + bz_lock * 251u + (uint32_t)bzkit.k * 257u;   /* (0.6: the key held,
                                                  * its cell outlined; 0.7: the KITs, KEY LOCK) */
    h = h * 379u + bz_spin * 383u + bz_shk_held * 389u + bzm.on * 397u + bzm.pos * 401u + bzm.ok * 409u +
        (uint32_t)bzm.k * 419u;                     /* (0.10: BACKSPIN, SHIFT 9..16, MORPH) */
    h = h * 601u + bz_g_auto * 607u + bz_g_prog * 613u + bz_g_every * 617u + bz_g_mel * 619u + bz_g_keep * 631u +
        bz_g_dens * 641u + bz_g_range * 643u + bz_g_idx * 647u + (uint32_t)(bz_chord && bz_white_held(13u)) * 653u;
    h = h * 467u + bz_b_lane * 479u + bz_b_curve * 487u + bz_b_swell * 491u + bz_b_fx * 499u + bz_d_top * 503u +
        bz_d_len * 509u + bz_d_extra * 521u + bz_d_hit * 523u + bz_b_fill * 541u + bz_build * 547u +
        (uint32_t)song.g[G_CLOCK] * 557u + bz_pad_slot[0] * 563u + bz_pad_slot[1] * 569u + bz_pad_slot[2] * 571u +
        bz_pad_slot[3] * 577u + (uint32_t)bz_build_held() * 587u + (uint32_t)bz_drop_held() * 593u + bz_pads * 599u;
    h = h * 613u + (bz_build ? bz_age >> 14 : 0u) * 617u + (bz_riser ? bz_rs_age >> 14 : 0u) * 619u +
        (bz_drop ? (bz_dr_age >> 14) + bz_drop_left() * 1021u : 0u) * 631u + bz_padl * 641u;   /* (0.14: the bars, dots) */
    h = h * 421u + bz_shlock * 463u + bz_cap * 431u + bz_cap_pend * 433u + bz_drop * 439u + rl_shape * 443u + bzride.on * 449u +
        bz_cap_bars * 457u;                         /* (0.11) */
    h = h * 331u + bz_rb * 337u + bz_rb_deck * 347u + bz_rb_down * 349u + bz_rb_val[0] * 353u + bz_rb_val[1] * 359u +
        bz_rb_val[2] * 367u + bz_rb_val[3] * 373u;   /* (0.9 REKORDBOX) */
    h = h * 293u + rl_latch * 307u + rl_pp * 311u + bz_pads * 313u;   /* (0.9: tapped rates, PAD PRESETS) */
    for (i = 0; i < NLANE; i++)
        h = h * 17u + rl_pp_rate[i] + rl_pp_fx[i] * 9u;
    h = h * 263u + bz_slice * 269u + bz_strk * 271u + bz_sl_ok * 277u + bz_sl_page * 281u + bz_sl_q * 283u;
    if (bz_slice)                                    /* (0.8: SLICE's track: its sample, slices, knobs) */
        for (i = P_E0; i <= P_E7; i++)
            h = h * 31u + (uint32_t)trk[bz_strk % NTRK].p[i] + (uint32_t)trk[bz_strk % NTRK].p[P_SLCR] * 7u +
                bz_slice_n(&trk[bz_strk % NTRK]) * 13u;
    for (i = 0; i < NTRK; i++)
        h = h * 33u + (uint32_t)trk[i].p[P_MUTE] + bz_lp_op[i] * 2u + (bz_fd_len[i] ? (bz_fd_age[i] * 8u / bz_fd_len[i]) * 16u
            + 8u : 0u) + (bz_fill_left[i] ? 256u : 0u) + (uint32_t)drum_track(&trk[i]) * 512u;
    {
        const track_t *ct = &trk[bz_ctrk % NTRK];
        h = h * 33u + (uint32_t)ct->p[P_ED_FLT] + (uint32_t)ct->p[P_REV] * 131u + (uint32_t)ct->p[P_LEVEL] * 977u +
            (uint32_t)ct->p[P_AMODE] * 7u + (uint32_t)ct->p[P_ROOT] * 11u + (uint32_t)ct->p[P_SCALE] * 13u;
    }
    for (i = 0; i < 8u; i++)
        h = h * 33u + (uint32_t)t->p[P_E0 + i];
    for (i = P_AMODE; i <= P_AHOLD; i++)
        h = h * 33u + (uint32_t)k2->p[i];
    h = h * 33u + (uint32_t)k2->p[P_ED_FLT] + (uint32_t)k2->p[P_REV] * 7u;
    if (bz_held_lane() < NLANE)
        h = h * 33u + (uint32_t)bz_lane_get(t, BZ_LANE_TUNE + bz_held_lane()) +
            (uint32_t)bz_lane_get(t, BZ_LANE_DECAY + bz_held_lane()) * 128u + (uint32_t)t->p[P_LN0 + bz_held_lane()] * 16384u;
    if (kt)
        h += (uint32_t)kt->p[P_ROOT] * 47u + (uint32_t)kt->p[P_SCALE] * 53u;
    return h;
}

/* ---- the key lights by fours (ui_input.c ui_leds; ROLL and BLAZE): the keys that can be pressed glow, the fours
 * in turn steady low (the 1st and 3rd four of the white keys, of the black) and a little brighter (the 2nd and 4th),
 * so the sets read apart; lit: on; breathing: a change waiting (LOOPS) */
static int bz_groups_on(void) { return ui.layer == LAYER_BLAZE || ui.layer == LAYER_ROLL; }
static void bz_groups(uint32_t can, uint32_t *lo, uint32_t *hi)
{
    uint32_t k;
    *lo = *hi = 0;
    if (ui.layer == LAYER_BLAZE && bz_keys) {        /* 0.12 KEYS: the chord's notes (the 3rd, the 5th up from home)
                                                     * brighter, the rest of the scale low; the black keys dark unless on */
        uint32_t n = bz_keys_n(&trk[bz_ktrk % NTRK]), d;
        for (k = 0; k < 27u; k++)
            if (((can >> k) & 1u) && !key_black(k) && key_place(k) < 16u) {
                d = n ? key_place(k) % n : 0u;
                if (d == 2u || d == 4u)
                    *hi |= 1u << k;
                else
                    *lo |= 1u << k;
            }
        return;
    }
    for (k = 0; k < 27u; k++)
        if ((can >> k) & 1u) {
            if ((key_place(k) / 4u) & 1u)
                *hi |= 1u << k;
            else
                *lo |= 1u << k;
        }
}
static uint32_t bz_waiting_keys(void)
{
    uint32_t k, m = 0, p;
    if (ui.layer != LAYER_BLAZE || !bz_loops)
        return 0;
    for (k = 0; k < 27u; k++) {
        p = key_place(k);
        if (!key_black(k) && ((p < 12u && bz_lp_op[p % 4u] && (p / 4u) == (bz_lp_op[p % 4u] == BZL_FILL ? 2u :
             bz_lp_op[p % 4u] >= BZL_FIN ? 1u : 0u)) || (p == 15u && bz_lp_restart)))
            m |= 1u << k;
    }
    return m;
}
