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

static int bz_shift(void) { return ((fm1_in.buttons >> panel.btn[B_OCTUP]) & 1u) != 0u; }
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
static const char *const BZR_NAME[BZR_NTYPE] = {"NOISE", "AIR", "SIREN", "TONE", "SYNTH"};
static const uint8_t BZ_EO_BARS[3] = {1u, 2u, 4u};
/* the settings into the ISR's copies (an unmarked or old setting: the defaults first) */
static void bz_cfg_apply(void)
{
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
}
static void bz_keys_toggle(void)                    /* (OCT-: BLAZE -> KEYS -> CHORD -> LOOPS -> BLAZE) */
{
    if (!bz_keys && !bz_chord && !bz_loops) {
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
    } else {
        bz_loops = 0;
    }
    ui_message(bz_keys ? "KEYS: THE KEYS PLAY IN KEY" : bz_chord ? "CHORD: THE KEY'S SEVEN CHORDS"
               : bz_loops ? "LOOPS: LAYERS IN AND OUT ON THE BEAT" : "BLAZE: PADS AND EFFECTS");
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
static int bz_riser_held(void) { return !bz_keys && !bz_chord && bz_white_held(BZ_RISER); }
static int bz_eo_held(void) { return !bz_keys && !bz_chord && bz_white_held(BZ_EOUT); }

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

/* a key of the BLAZE layer pressed (the main loop: what the ISR does not, and what it says) */
static void bz_ui_key(uint32_t k)
{
    uint32_t p = key_place(k);
    track_t *t;
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
        if (p < NTRK) {
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
    if (key_black(k)) {
        if (p < 6u)
            ui_say("ROLL ", RL_RATE_NAME[p + 1u]);
        else if (p == BZK_TAP)
            glo_tap();
        else if (p == BZK_NDN || p == BZK_NUP)
            rl_nudge(k, p == BZK_NUP);
        else if (p == BZK_ONE)
            ui_message("THE ONE: BAR 1 OF THE PHRASE");
        return;
    }
    if (p >= BZ_FX0 && p < BZ_FX0 + BZ_NFX)
        ui_message(BZ_FX_NAME[p - BZ_FX0]);
    else if (p == BZ_B4 || p == BZ_B8)
        ui_message(!bz_build ? "BUILD STOPPED" : p == BZ_B4 ? "BUILD 4 BARS FROM THE NEXT BAR"
                                                           : "BUILD 8 BARS FROM THE NEXT BAR");
    else if (p == BZ_RISER)
        ui_message(!bz_riser ? "RISER STOPPED" : bz_riser == 16u ? "RISER 16 BARS" : "RISER 8 BARS FROM THE NEXT BAR");
    else if (p == BZ_EOUT)
        ui_message(bz_eo == BZE_IDLE ? "BACK IN" : "ECHO OUT FROM THE NEXT BAR");
    else if (p == BZ_SPREV || p == BZ_SNEXT)
        ui_message(!bz_set_n ? "NO SET: LOAD ONE IN RAIL" : p == BZ_SNEXT ? "NEXT SONG" : "PREVIOUS SONG");
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
        bzu.cc_seq = bz_cc_seq;
        bz_key_apply();
        bz_song();
    }
    bz_ramp_pass();
    if (rs != bz_riser) {
        if (bz_riser == 16u && rs == 8u)
            ui_message("RISER 16 BARS");
        rs = bz_riser;
    }
}

/* KNOB k turned s: the four of the mode (see the top) */
static void bz_knob(uint32_t k, int32_t s)
{
    track_t *t = rl_track(), *kt = &trk[bz_ktrk % NTRK];
    uint32_t l = bz_held_lane();
    int16_t *vp;
    if (bz_riser_held()) {                          /* RISER held: TYPE TOP LEVEL SPACE (SYNTH: TRACK), kept */
        uint8_t *c = bz_cfg;
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
        if (k == 0u) {
            bz_cfg[BZC_EOB] = (uint8_t)clamp((int32_t)bz_cfg[BZC_EOB] % 3 + (s > 0 ? 1 : -1), 0, 2);
            bz_cfg_apply();
            lys.rp_dirty = 1;
        }
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
    if (k == 0u && l == NLANE && !bz_shift()) {
        perf_k[0] = (int8_t)clamp(perf_k[0] + accel(EN_K1, s, 200), -100, 100);
        return;
    }
    if (k == 3u && l == NLANE) {
        if (bz_shift())                             /* THROW: the mix into the echo and the reverb */
            perf_k[2] = (int8_t)clamp(perf_k[2] + accel(EN_K4, s, 100), 0, 100);
        else
            perf_k[3] = (int8_t)clamp(perf_k[3] - accel(EN_K4, s, 100), 0, 100);   /* (DEPTH rises to the right) */
        return;
    }
    if (!drum_track(t)) {
        ui_message("NO DRUM TRACK");
        return;
    }
    if (l < NLANE) {                                /* a pad held: that drum's TUNE DECAY LEVEL */
        if (k == 3u)
            return;
        if (k < 2u) {
            bz_lanes_ready(t);
            vp = &t->p[(k ? BZ_LANE_DECAY : BZ_LANE_TUNE) + l];
        } else {
            vp = &t->p[P_LN0 + l];
        }
        *vp = (int16_t)clamp(*vp + accel(EN_K1 + k, s, 127), 0, 127);
        return;
    }
    {   /* DRUM's TUNE DECY, SHIFT: TONE SNAP DRV (eng_drum.c) */
        static const uint8_t E[2][3] = {{0, 1, 3}, {2, 4, 7}};
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
    d = track_desc(t, P_E0);
    v = enum_step(d, t->p[P_E0], clamp(t->p[P_E0] + (s > 0 ? 1 : -1), d->min, d->max));
    t->p[P_E0] = (int16_t)v;
    ui_say("KIT ", d->names[v]);
    ui.force = 1;
}

/* key k's LED in the BLAZE layer: lit (held, latched, running, the value), breathing (can be pressed), dark */
static void bz_led(uint32_t k, uint32_t *on, uint32_t *can)
{
    uint32_t p = key_place(k), fx = perf_held | perf_latched;
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
            *can = p < 8u;
            *on = p < 7u && bz_ch_sel == p + 1u;
            return;
        }
        *can = p < 11u;
        *on = p <= 4u ? bz_ch_style == p : p == 5u ? bz_ch_7th != 0u : p == 6u ? bz_ch_inv != 0u : p == 7u ? bz_ch_oct < 0
              : p == 8u ? bz_ch_oct > 0 : p == 10u && t->p[P_AMODE] != 0;
        return;
    }
    if (bz_keys) {
        const track_t *t = &trk[bz_ktrk % NTRK];
        if (!key_black(k)) {                        /* the scale's notes breathe, the root's lit, a key held lit */
            uint32_t n = 0, pc;
            for (pc = 0; pc < 12u; pc++)
                n += (scale_mask(t) >> pc) & 1u;
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
    if (key_black(k)) {
        if (p < 6u) {
            *on = (rl_rates >> (p + 1u)) & 1u;
            *can = 1;
        } else if (p == BZK_TAP || p == BZK_NDN || p == BZK_NUP) {
            *on = (rlu.nk >> k) & 1u;
            *can = !song.g[G_CLOCK];
        } else {
            *can = p == BZK_REL || (p == BZK_ONE && song.playing);
        }
        return;
    }
    if (p < BZ_FX0) {
        uint32_t pad = bz_pad_of(p);
        *can = pad < 16u;
        *on = *can && ((((uint32_t)rl_extra | rlu.fl) >> pad) & 1u);
    } else if (p < BZ_FX0 + BZ_NFX) {
        *can = (perf_avail() >> BZ_FX[p - BZ_FX0]) & 1u;
        *on = *can && ((fx >> BZ_FX[p - BZ_FX0]) & 1u);
    } else if (p == BZ_B4 || p == BZ_B8) {
        *on = bz_build == (p == BZ_B4 ? 4u : 8u);
        *can = 1;
    } else if (p == BZ_RISER) {
        *on = bz_riser != 0u;
        *can = 1;
    } else if (p == BZ_EOUT) {
        *on = bz_eo != BZE_IDLE;
        *can = 1;
    } else if (p == BZ_SPREV || p == BZ_SNEXT) {
        *can = bz_set_n != 0u;
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
    str_cpy(h, bz_keys ? "[ENV] KEYS" : bz_chord ? "[ENV] CHORD" : bz_loops ? "[ENV] LOOPS" : bz_riser_held() ? "[ENV] RISER SOUND"
               : bz_eo_held() ? "[ENV] ECHO OUT" : bz_held_lane() < NLANE ? "[ENV] BLAZE DRUM"
               : bz_shift() ? "[ENV] BLAZE SHIFT" : "[ENV] BLAZE", sizeof h);
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
        fm1_in.notes * 239u;                       /* (0.6: the key held, its cell outlined) */
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
