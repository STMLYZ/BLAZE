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
static void bz_keys_toggle(void)
{
    bz_ktrk = (uint8_t)bz_keys_track();
    bz_keys = (uint8_t)!bz_keys;
    ui_message(bz_keys ? "KEYS: THE KEYS PLAY IN KEY" : "BLAZE: PADS AND EFFECTS");
    ui.force = 1;
}

/* a key of the BLAZE layer pressed (the main loop: what the ISR does not, and what it says) */
static void bz_ui_key(uint32_t k)
{
    uint32_t p = key_place(k);
    track_t *t;
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
}

/* each pass: RELEASE's knobs, RAIL's key, RISER's 16 said */
static void bz_ui_pass(void)
{
    static uint8_t rs;
    if (bz_rel) {
        bz_rel = 0;
        perf_k[0] = perf_k[2] = perf_k[3] = 0;    /* (COLOR to the middle, THROW off, DEPTH full) */
        ui_message("RELEASE: ALL OFF");
        ui.force = 1;
    }
    if (bzu.cc_seq != bz_cc_seq) {
        bzu.cc_seq = bz_cc_seq;
        bz_key_apply();
    }
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
    return bz_keys ? "[ENV] KEYS" : bz_held_lane() < NLANE ? "[ENV] BLAZE DRUM" : bz_shift() ? "[ENV] BLAZE SHIFT"
                                                                                               : "[ENV] BLAZE";
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
        bz_held_lane() * 67u + bz_ktrk * 71u + (uint32_t)(bz_koct + 2) * 73u + (bz_keys ? fm1_in.notes * 79u : 0u);
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
