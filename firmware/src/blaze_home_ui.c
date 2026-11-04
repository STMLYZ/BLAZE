/* SPDX-License-Identifier: GPL-3.0-only
 * BLAZE (a DJ companion built on Felucca by Leo Kuroshita / Hügelton Instruments) */
/* BLAZE, the main loop's side (ui_layer.c includes it after blaze_roll_ui.c): TAP and NUDGE (black keys 7..9: ROLL's),
 * RELEASE's knobs, what it says, KNOB 1..4 COLOR PITCH DECAY FX DEPTH, the LEDs, and the key.
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
static void bz_key_apply(void)
{
    uint32_t i, r = bz_cc[2] % 12u, b = bz_cc[1] >= 64u, cam, bb;
    char nm[16], m[24];
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

static const char *const BZ_FX_NAME[BZ_NFX] = {"ECHO", "REVERB", "REVERSE", "BRAKE", "FREEZE", "STUTTER"};
static const char *const BZ_BLACK_NAME[5] = {"TAP", "NUDGE-", "NUDGE+", "RELEASE", "THE ONE"};

/* a key of the BLAZE layer pressed (the main loop: what the ISR does not, and what it says) */
static void bz_ui_key(uint32_t k)
{
    uint32_t p = key_place(k);
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
    else if (p == BZ_IMPACT)
        ui_message("IMPACT");
}

/* each pass: RELEASE's knobs, RAIL's key */
static void bz_ui_pass(void)
{
    if (bz_rel) {
        bz_rel = 0;
        perf_k[0] = perf_k[3] = 0;                /* (COLOR to the middle, DEPTH full) */
        ui_message("RELEASE: ALL OFF");
        ui.force = 1;
    }
    if (bzu.cc_seq != bz_cc_seq) {
        bzu.cc_seq = bz_cc_seq;
        bz_key_apply();
    }
}

/* KNOB 1..4: COLOR (the FX macro FILTER: left low-pass, right high-pass), PITCH and DECAY of the drum track, FX DEPTH
 * (the buffer effects' level: the FX macro DEPTH) */
static void bz_knob(uint32_t k, int32_t s)
{
    track_t *t = rl_track();
    int16_t *vp;
    if (k == 0u) {
        perf_k[0] = (int8_t)clamp(perf_k[0] + accel(EN_K1, s, 200), -100, 100);
    } else if (k == 3u) {
        perf_k[3] = (int8_t)clamp(perf_k[3] - accel(EN_K4, s, 100), 0, 100);   /* (rises to the right) */
    } else if (!drum_track(t)) {
        ui_message("NO DRUM TRACK");
    } else {
        vp = &t->p[P_E0 + (k == 1u ? 1u : 3u)];   /* DRUM's TUNE, DECY (eng_drum.c) */
        *vp = (int16_t)clamp(*vp + accel(EN_K1 + k, s, 127), 0, 127);
    }
}

/* key k's LED in the BLAZE layer: lit (held, latched, running), breathing (can be pressed), dark (nothing) */
static void bz_led(uint32_t k, uint32_t *on, uint32_t *can)
{
    uint32_t p = key_place(k), fx = perf_held | perf_latched;
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
    } else if (p == BZ_IMPACT) {
        *on = bz_thr != 0u;
        *can = 1;
    }
}

/* the phrase now: bar 0..15, beat 0..3 (-1: stopped) */
static int32_t bz_phrase(uint32_t *beat)
{
    uint32_t s = bz_s16 - bz_org;
    *beat = (s / 4u) % 4u;
    return song.playing && clk_pos != CLK_START ? (int32_t)((s / 16u) % BZ_PHRASE) : -1;
}

/* the map's header: "[ENV] BLAZE" */
static const char *bz_head(void) { return "[ENV] BLAZE"; }

/* the map's signature: what it shows */
static uint32_t bz_sig(void)
{
    track_t *t = rl_track();
    const track_t *kt = bz_key_track();
    uint32_t beat, h;
    int32_t bar = bz_phrase(&beat);
    h = (uint32_t)rl_extra * 31u + (uint32_t)rlu.fl * 131u + (uint32_t)rl_rates * 7919u + (uint32_t)rl_live4 * 977u +
        (perf_held | perf_latched) * 13u + bz_build * 5u + bz_build_rate * 11u + (bz_thr != 0u) * 3u +
        rlu.nk * 17u + (uint32_t)song.g[G_CLOCK] * 19u + (uint32_t)drum_track(t) * 23u + (uint32_t)t->p[P_E0] * 29u +
        (uint32_t)(bar + 1) * 37u + beat * 41u + perf_avail() * 43u;
    if (kt)
        h += (uint32_t)kt->p[P_ROOT] * 47u + (uint32_t)kt->p[P_SCALE] * 53u;
    return h;
}
