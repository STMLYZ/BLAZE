/* SPDX-License-Identifier: GPL-3.0-only
 * BLAZE (a DJ companion built on Felucca by Leo Kuroshita / Hügelton Instruments) */
/* ROLL, the main loop's side (ui_layer.c includes it): TAP and NUDGE (black keys 8..10), KNOB 1..4 BPM BUILD DECAY
 * LEVEL, OCT+ GROOVE / LAST, the LEDs, what it says. The pads and the rates are the ISR's (blaze_roll.c).
 * NUDGE: while held, the tempo 1/32 faster / slower (about 3 %: a jog wheel's push), back when let go: the FM-1
 * pulled back into line with a track by ear. TAP, NUDGE and KNOB 1 work on the internal clock only (an external
 * clock sets the tempo: they say so) */
static const char *rl_lane_name(const track_t *t, uint32_t l);   /* (blaze_roll_draw.c) */
static int rd_knob(uint32_t k, int32_t s);                       /* (0.14 READY ROLLS: below) */
static int bz_shift_held(void);                                  /* (blaze_home_ui.c) */
static void bz_lanes_ready(track_t *t);
static void rf_pass(void);                                       /* (0.19 ROLL FX: below) */
static int rf_knob(uint32_t k, int32_t s);
static uint8_t rl_fxpage;                                        /* 0.19: ROLL's third page (OCT-: ROLL READY FX) */
static uint8_t rl_sh_used;                                       /* 0.19: OCT+ held for SHIFT: its release no GROOVE */
static struct {
    uint32_t nk;                       /* the NUDGE keys held (bits 0..26) */
    int16_t bpm;                       /* the tempo before NUDGE */
    uint8_t mode, groove;              /* the PADS mode and GROOVE last said */
    uint16_t fl;                       /* pads hit lately (the LEDs, the screen) */
    uint32_t fl_t;                     /* .. when (fm1_ms) */
} rlu = {0, 0, 0, 1, 0, 0};
#define RL_FLASH_MS 70u
static const char *const RL_PADS_NAME[RL_NPADMODE] = {"16", "4", "1"};

/* NUDGE key k down (up: +): the tempo 1/32 off while it is held (rl_ui_pass puts it back); ROLL's and BLAZE's */
static void rl_nudge(uint32_t k, int up)
{
    int32_t d;
    if (song.g[G_CLOCK]) {
        ui_message("EXTERNAL CLOCK: NO NUDGE");
        return;
    }
    if (!rlu.nk)
        rlu.bpm = song.g[G_BPM];
    rlu.nk |= 1u << k;
    d = rlu.bpm / 32 > 1 ? rlu.bpm / 32 : 1;
    song.g[G_BPM] = (int16_t)clamp(rlu.bpm + (up ? d : -d), GP[G_BPM].min, GP[G_BPM].max);
    ui_message(up ? "NUDGE +" : "NUDGE -");
}

/* a key of the ROLL layer pressed (the main loop: what the ISR does not) */
static void rl_ui_key(uint32_t k)
{
    uint32_t p = key_place(k);
    if (!key_black(k))
        return;
    if (p < RL_NRATE) {
        ui_say("ROLL ", RL_RATE_NAME[p]);
    } else if (p == RLK_TAP) {
        glo_tap();
    } else if (p == RLK_NDN || p == RLK_NUP) {
        rl_nudge(k, p == RLK_NUP);
    }
}

/* each pass: NUDGE let go (the tempo back), the PADS mode and GROOVE said when they change, the pads' flashes */
static void rl_ui_pass(void)
{
    rf_pass();
    if (rlu.nk && !(rlu.nk & kb_layer)) {
        rlu.nk = 0;
        if (!song.g[G_CLOCK])
            song.g[G_BPM] = rlu.bpm;
    }
    if (rlu.mode != rl_padmode) {
        rlu.mode = rl_padmode;
        ui_say("PADS ", RL_PADS_NAME[rlu.mode % RL_NPADMODE]);
        ui.force = 1;
    }
    if (rl_flash) {
        rlu.fl = (uint16_t)(rlu.fl | rl_flash);
        rl_flash = 0;
        rlu.fl_t = fm1_ms;
    } else if (rlu.fl && fm1_ms - rlu.fl_t >= RL_FLASH_MS) {
        rlu.fl = 0;
    }
}

/* OCT+ in the ROLL layer: GROOVE <-> LAST (OCT-: nothing; neither moves the octave there) */
static void rl_groove_toggle(void)
{
    if (rl_sh_used) {                                   /* (0.19: OCT+ was SHIFT: nothing) */
        rl_sh_used = 0;
        return;
    }
    rl_groove = (uint8_t)!rl_groove;
    ui_message(rl_groove ? "GROOVE: RATES COMBINE" : "LAST: NEWEST RATE WINS");
    ui.force = 1;
}

/* KNOB 1..4: BPM (INT clock), BUILD, the drum track's DECY, its LEVEL */
static void rl_knob(uint32_t k, int32_t s)
{
    track_t *t = rl_track();
    int16_t *vp;
    if (rd_knob(k, s))                                  /* (0.14 READY ROLLS: a key held, its roll) */
        return;
    if (rf_knob(k, s))                                  /* (0.19 ROLL FX: its page) */
        return;
    if (k == 0u) {
        if (song.g[G_CLOCK]) {
            ui_message("EXTERNAL CLOCK SETS BPM");
            return;
        }
        song.g[G_BPM] = (int16_t)clamp(song.g[G_BPM] + accel(EN_K1, s, GP[G_BPM].max - GP[G_BPM].min), GP[G_BPM].min,
                                       GP[G_BPM].max);
        if (rlu.nk)
            rlu.bpm = (int16_t)clamp(rlu.bpm + (s > 0 ? 1 : -1), GP[G_BPM].min, GP[G_BPM].max);   /* (NUDGE held) */
        ui.bpm_t = 40;
    } else if (k == 1u) {
        rl_build = (uint8_t)clamp((int32_t)rl_build + accel(EN_K2, s, 100), 0, 100);
    } else if (k == 2u) {
        if (!drum_track(t)) {
            ui_message("NO DRUM TRACK");
            return;
        }
        vp = &t->p[P_E0 + 3];                   /* DRUM's DECY (eng_drum.c) */
        *vp = (int16_t)clamp(*vp + accel(EN_K3, s, 127), 0, 127);
    } else {
        vp = &t->p[P_LEVEL];
        *vp = (int16_t)clamp(*vp + accel(EN_K4, s, TP[P_LEVEL].max - TP[P_LEVEL].min), TP[P_LEVEL].min, TP[P_LEVEL].max);
    }
}

/* key k's LED in the ROLL layer: lit (held, sounding, the mode set), breathing (can be pressed), dark (locked) */
static void rl_led(uint32_t k, uint32_t *on, uint32_t *can)
{
    uint32_t p = key_place(k), live = rl_live();
    if (!key_black(k)) {
        if (p >= 16u)
            return;
        if (rl_ready) {                                 /* (0.14 READY ROLLS: rolling lit) */
            *can = 1;
            *on = (rd_on >> p) & 1u;
            return;
        }
        *can = (live >> p) & 1u;
        *on = *can && ((((uint32_t)rl_held | rlu.fl) >> p) & 1u);
        if (rl_pick && *can)
            *on = 1;                             /* (PADS held: the live pads lit) */
    } else if (p < RL_NRATE) {
        *on = (rl_rates >> p) & 1u;
        *can = 1;
    } else if (p == RLK_TAP) {
        *can = !song.g[G_CLOCK];
    } else if (p == RLK_NDN || p == RLK_NUP) {
        *on = (rlu.nk >> k) & 1u;
        *can = !song.g[G_CLOCK];
    } else if (p == RLK_PADS) {
        *on = rl_ready ? rd_hit_all : rl_padmode != RL_PADS16 || rl_pick;   /* (0.19: READY's HIT) */
        *can = 1;
    }
}

/* the map's header: "[ARP] GROOVE 16 PADS" */
/* 0.14 READY ROLLS, kept in GRAIN's favourites row (its presets use [0]): [6] 0xD3 when set, [7..22] each key's drum |
 * rate << 3 | ACCENT << 6. Unset: SNARE 1/8 1/16 1/32 1/16T, CLAP the same, HAT 1/16 1/32 1/16T, OPEN 1/8, KICK 1/8
 * 1/16, RIM 1/16, TOM 1/16T */
#define RD_ROW (&favorites.factory[8][6])
#define RD_MAGIC 0xD3u
static void rd_cfg_apply(void)
{
    static const uint8_t DEF[16] = {1u | 2u << 3, 1u | 4u << 3, 1u | 6u << 3, 1u | 5u << 3,
                                    2u | 2u << 3, 2u | 4u << 3, 2u | 6u << 3, 2u | 5u << 3,
                                    3u | 4u << 3, 3u | 6u << 3, 3u | 5u << 3, 4u | 2u << 3,
                                    0u | 2u << 3, 0u | 4u << 3, 6u | 4u << 3, 5u | 5u << 3};
    uint8_t *c = RD_ROW;
    uint32_t i;
    if (c[0] != RD_MAGIC) {
        for (i = 0; i < 16u; i++)
            c[1u + i] = DEF[i];
        c[0] = RD_MAGIC;
    }
    for (i = 0; i < 16u; i++) {
        uint32_t r = (c[1u + i] >> 3) & 7u;
        rd_lane[i] = (uint8_t)(c[1u + i] & 7u);
        rd_rate[i] = (uint8_t)(r >= 1u && r <= 7u ? r : 4u);   /* (0.19: 7 HIT) */
        rd_acc[i] = (uint8_t)((c[1u + i] >> 6) & 1u);
    }
}
/* .. key i's roll as a cell's name: "SNARE 16", "HAT 16T" (the rate's denominator) */
static const char *rd_name(uint32_t i, char *b, uint32_t n)
{
    static const char *const R[8] = {"", "4", "8", "8T", "16", "16T", "32", "HIT"};   /* (0.19: HIT) */
    static const char *const S[NLANE] = {"KCK", "SNR", "CLP", "HAT", "OPN", "TOM", "RIM", "BEL"};
    str_cpy(b, rl_lane_name(rl_track(), rd_lane[i % 16u] % NLANE), n);
    if (str_len(b) + 1u + str_len(R[rd_rate[i % 16u] % 8u]) > 8u)   /* (too long for a cell: "SNR 16T") */
        str_cpy(b, S[rd_lane[i % 16u] % NLANE], n);
    str_cpy(b + str_len(b), " ", n - str_len(b));
    str_cpy(b + str_len(b), R[rd_rate[i % 16u] % 8u], n - str_len(b));
    return b;
}
/* .. the white key held now on READY's page (16: none) */
static uint32_t rd_held(void)
{
    uint32_t k;
    for (k = 0; k < 27u; k++)
        if (((fm1_in.notes >> k) & 1u) && !key_black(k) && key_place(k) < 16u)
            return key_place(k);
    return 16u;
}
/* .. a key held + KNOB 1 its DRUM, KNOB 2 its RATE (1/4 .. 1/32, 0.19: then HIT), KNOB 3 ACCENT; kept. 0.19: KNOB 4
 * that drum's TUNE, SHIFT + KNOB 4 its DECAY (BLAZE's, a pad held there: the same) */
static int rd_knob(uint32_t k, int32_t s)
{
    uint32_t i = rd_held(), v;
    uint8_t *c = RD_ROW;
    char nm[16];
    if (!rl_ready || i >= 16u)
        return 0;
    if (k == 3u) {
        track_t *t = rl_track();
        uint32_t l = rd_lane[i] % NLANE, dec = (uint32_t)bz_shift_held();
        int16_t *vp;
        char m[24];
        if (!drum_track(t)) {
            ui_message("NO DRUM TRACK");
            return 1;
        }
        if (dec)
            rl_sh_used = 1;
        bz_lanes_ready(t);
        vp = &t->p[(dec ? BZ_LANE_DECAY : BZ_LANE_TUNE) + l];
        *vp = (int16_t)clamp(*vp + (s > 0 ? 1 : -1), 0, 127);
        str_cpy(m, rl_lane_name(t, l), sizeof m);
        str_cpy(m + str_len(m), dec ? " DECAY " : " TUNE ", sizeof m - str_len(m));
        fmt_int(m + str_len(m), *vp - 64);
        ui_message(m);
        ui.force = 1;
        return 1;
    }
    rd_cfg_apply();
    v = c[1u + i];
    if (k == 0u)
        v = (v & ~7u) | (((v & 7u) + (s > 0 ? 1u : NLANE - 1u)) % NLANE);
    else if (k == 1u) {
        uint32_t r = (v >> 3) & 7u;
        r = s > 0 ? (r < 7u ? r + 1u : 7u) : (r > 1u ? r - 1u : 1u);   /* (0.19: past 1/32, HIT) */
        v = (v & ~(7u << 3)) | r << 3;
    } else
        v = s > 0 ? v | 1u << 6 : v & ~(1u << 6);
    c[1u + i] = (uint8_t)v;
    rd_cfg_apply();
    ui_say(rd_acc[i] ? "ACCENT: " : "READY: ", rd_name(i, nm, sizeof nm));
    return 1;
}
static void rl_ready_toggle(void)                   /* OCT-: ROLL -> READY -> 0.19 ROLL FX -> ROLL */
{
    if (rl_fxpage) {
        rl_fxpage = 0;
        ui_message("ROLL: PADS AND RATES");
    } else if (rl_ready) {
        rl_ready = 0;
        rl_fxpage = 1;
        ui_message("ROLL FX: KNOBS SHAPE EVERY ROLL");
    } else {
        rl_ready = 1;
        rd_cfg_apply();
        ui_message("READY ROLLS: TAP ONE, THEN THE NEXT");
    }
    ui.force = 1;
}

/* ---- 0.19 ROLL FX, the main loop's: the page's knobs in 3 banks (SELECT), the genres (SHIFT + SELECT, any ROLL
 * page), BRIGHT and REVERB as the roll goes, kept in GRAIN's favourites row [23..31] (RD_ROW's after it): [23] 0xE5,
 * [24] the genre, [25] PITCH | DECAY << 4 | FLAM << 6, [26] VELOCITY, [27] BRIGHT, [28] SWING | GAP << 6 | STUTTER << 7,
 * [29] REVERB | CLAP << 6, [30] LENGTH ---- */
#define RF_ROW (&favorites.factory[8][23])
#define RF_MAGIC 0xE5u
enum { RFG_OFF, RFG_TRANCE, RFG_HARDSTYLE, RFG_HARDTECHNO, RFG_ELECTRO, RFG_HOUSE, RFG_CUSTOM, RFG_N };
static const char *const RFG_NAME[RFG_N] = {"OFF", "TRANCE", "HARDSTYLE", "HARD TECHNO", "ELECTRO", "HOUSE", "CUSTOM"};
/*                                       PITCH VEL BRIGHT LEN SWING DEC FLAM REV GAP STUT CLAP */
static const uint8_t RFG_SET[RFG_CUSTOM][RF_N] = {{0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0},
                                                  {12, 70, 40, 3, 0, 2, 1, 40, 0, 0, 1},
                                                  {12, 60, 40, 2, 0, 2, 2, 40, 1, 0, 1},
                                                  {3, 0, 0, 2, 0, 2, 0, 15, 0, 0, 0},
                                                  {5, 30, 0, 1, 15, 2, 2, 0, 0, 1, 0},
                                                  {0, 50, 30, 2, 20, 1, 0, 25, 0, 0, 0}};
static const uint8_t RF_MAX[RF_N] = {12, 100, 60, 4, 50, 3, 3, 60, 1, 1, 1};
static const uint8_t RF_STEP[RF_N] = {1, 5, 5, 1, 5, 1, 1, 5, 1, 1, 1};
static const char *const RFG_CARD[RFG_N] = {"OFF", "TRANCE", "HSTYLE", "HTECHNO", "ELECTRO", "HOUSE", "CUSTOM"};
static const char *const RF_NAME[RF_N] = {"PITCH", "VELOCITY", "BRIGHT", "LENGTH", "SWING", "DECAY", "FLAM", "REVERB",
                                          "GAP", "STUTTER", "CLAP"};
static const uint8_t RF_BANK[3][4] = {{RF_PITCH, RF_VEL, RF_BRIGHT, RF_LEN}, {RF_SWING, RF_DEC, RF_FLAM, RF_REV},
                                      {RF_GAP, RF_STUT, RF_CLAP, RF_N}};   /* (bank 3's KNOB 4: the genre) */
static const char *const RF_BANK_NAME[3] = {"RISE", "FEEL", "END"};
static struct {
    uint8_t genre, bank, loaded;
    uint8_t run;                       /* the drum track's TONE and REV taken (a roll runs) */
    uint8_t trk;
    int16_t tone, rev;                 /* .. as they were */
} rfu;
static void rf_apply_on(void)
{
    uint32_t i, on = 0;
    for (i = 0; i < RF_N; i++)
        if (i != RF_LEN && rf_v[i])
            on = 1;
    rf_on = (uint8_t)on;
}
static void rf_save(void)
{
    uint8_t *c = RF_ROW;
    c[0] = RF_MAGIC;
    c[1] = rfu.genre;
    c[2] = (uint8_t)(rf_v[RF_PITCH] | rf_v[RF_DEC] << 4 | rf_v[RF_FLAM] << 6);
    c[3] = rf_v[RF_VEL];
    c[4] = rf_v[RF_BRIGHT];
    c[5] = (uint8_t)(rf_v[RF_SWING] | rf_v[RF_GAP] << 6 | rf_v[RF_STUT] << 7);
    c[6] = (uint8_t)(rf_v[RF_REV] | rf_v[RF_CLAP] << 6);
    c[7] = rf_v[RF_LEN];
    settings_later();
}
static void rf_load(void)
{
    const uint8_t *c = RF_ROW;
    uint8_t v[RF_N];
    uint32_t i;
    rfu.loaded = 1;
    if (c[0] != RF_MAGIC) {
        for (i = 0; i < RF_N; i++)
            rf_v[i] = RFG_SET[RFG_OFF][i];
        rfu.genre = RFG_OFF;
        rf_apply_on();
        return;
    }
    v[RF_PITCH] = c[2] & 15u; v[RF_DEC] = (c[2] >> 4) & 3u; v[RF_FLAM] = c[2] >> 6;
    v[RF_VEL] = c[3]; v[RF_BRIGHT] = c[4];
    v[RF_SWING] = c[5] & 63u; v[RF_GAP] = (c[5] >> 6) & 1u; v[RF_STUT] = c[5] >> 7;
    v[RF_REV] = c[6] & 63u; v[RF_CLAP] = (c[6] >> 6) & 1u; v[RF_LEN] = c[7];
    for (i = 0; i < RF_N; i++)
        rf_v[i] = v[i] > RF_MAX[i] ? RF_MAX[i] : v[i];
    rfu.genre = c[1] < RFG_N ? c[1] : RFG_CUSTOM;
    rf_apply_on();
}
static void rf_genre(uint32_t g)
{
    uint32_t i;
    if (g >= RFG_CUSTOM)
        g = RFG_CUSTOM;
    rfu.genre = (uint8_t)g;
    if (g < RFG_CUSTOM)
        for (i = 0; i < RF_N; i++)
            rf_v[i] = RFG_SET[g][i];
    rf_apply_on();
    rf_save();
    ui_say("ROLL FX: ", RFG_NAME[g]);
    ui.force = 1;
}
/* SELECT in ROLL: SHIFT held, the genre (any of its pages); on ROLL FX, the bank. 1: taken */
static int rf_select(int32_t s)
{
    if (layer_open() != LAYER_ROLL)
        return 0;
    if (!rfu.loaded)
        rf_load();
    if (bz_shift_held()) {
        rl_sh_used = 1;
        rf_genre((rfu.genre + (s > 0 ? 1u : RFG_N - 1u)) % RFG_N);
        return 1;
    }
    if (rl_fxpage) {
        rfu.bank = (uint8_t)((rfu.bank + (s > 0 ? 1u : 2u)) % 3u);
        ui_say("ROLL FX: ", RF_BANK_NAME[rfu.bank]);
        ui.force = 1;
        return 1;
    }
    return 0;
}
static int rf_knob(uint32_t k, int32_t s)
{
    uint32_t f, v;
    if (!rl_fxpage || k > 3u)
        return 0;
    if (!rfu.loaded)
        rf_load();
    f = RF_BANK[rfu.bank % 3u][k];
    if (f >= RF_N) {                                    /* (bank 3's KNOB 4: the genre) */
        rf_genre((rfu.genre + (s > 0 ? 1u : RFG_N - 1u)) % RFG_N);
        return 1;
    }
    v = rf_v[f];
    v = s > 0 ? (v + RF_STEP[f] > RF_MAX[f] ? RF_MAX[f] : v + RF_STEP[f]) : (v >= RF_STEP[f] ? v - RF_STEP[f] : 0u);
    rf_v[f] = (uint8_t)v;
    rfu.genre = RFG_CUSTOM;                             /* (an edit: CUSTOM, kept) */
    rf_apply_on();
    rf_save();
    ui.force = 1;
    return 1;
}
/* a setting as its card shows it */
static const char *rf_fmt(uint32_t f, char *b, const char **unit)
{
    uint32_t v = rf_v[f];
    *unit = "";
    if (f == RF_LEN) {
        fmt_int(b, RF_BARS[v % 5u]);
        *unit = v ? "BARS" : "BAR";
    } else if (f >= RF_GAP) {
        str_cpy(b, v ? "ON" : "OFF", 8);
    } else if (!v) {
        str_cpy(b, "OFF", 8);
    } else if (f == RF_DEC || f == RF_FLAM) {
        static const char *const L[4] = {"OFF", "LOW", "MID", "HIGH"};
        str_cpy(b, L[v % 4u], 8);
    } else {
        fmt_int(b, v);
        *unit = f == RF_PITCH ? "ST" : "%";
    }
    return b;
}
/* each pass: a roll running takes the drum track's TONE and REV and lifts them over LENGTH (BRIGHT, REVERB); its end
 * puts them back */
static void rf_pass(void)
{
    track_t *t;
    if (!rfu.loaded)
        rf_load();
    if (rf_run && rf_on && (rf_v[RF_BRIGHT] || rf_v[RF_REV])) {
        uint32_t pr = rf_prog();
        t = rl_track();
        if (!drum_track(t))
            return;
        if (!rfu.run) {
            rfu.run = 1;
            rfu.trk = (uint8_t)trk_index(t);
            rfu.tone = t->p[P_E0 + 2];
            rfu.rev = t->p[P_REV];
        }
        t = &trk[rfu.trk % NTRK];
        t->p[P_E0 + 2] = (int16_t)clamp(rfu.tone + (int32_t)(rf_v[RF_BRIGHT] * pr >> 10), 0, 127);
        t->p[P_REV] = (int16_t)clamp(rfu.rev + (int32_t)(rf_v[RF_REV] * pr >> 10), TP[P_REV].min, TP[P_REV].max);
    } else if (rfu.run) {
        t = &trk[rfu.trk % NTRK];
        t->p[P_E0 + 2] = rfu.tone;
        t->p[P_REV] = rfu.rev;
        rfu.run = 0;
    }
}

static const char *rl_head(void)
{
    static char h[28];
    if (rl_ready)                                       /* (0.14; 0.19: HIT, TUNE) */
        return rd_held() < 16u ? "[ARP] READY: DRUM RATE ACC" : rd_hit_all ? "[ARP] READY: HITS" : "[ARP] READY ROLLS";
    if (rl_fxpage) {                                    /* 0.19: "[ARP] FX TRANCE  RISE" */
        str_cpy(h, "[ARP] FX ", sizeof h);
        str_cpy(h + str_len(h), RFG_NAME[rfu.genre % RFG_N], sizeof h - str_len(h));
        str_cpy(h + str_len(h), "  ", sizeof h - str_len(h));
        str_cpy(h + str_len(h), RF_BANK_NAME[rfu.bank % 3u], sizeof h - str_len(h));
        return h;
    }
    str_cpy(h, rl_groove ? "[ARP] GROOVE " : "[ARP] LAST ", sizeof h);
    str_cpy(h + str_len(h), RL_PADS_NAME[rl_padmode % RL_NPADMODE], sizeof h - str_len(h));
    str_cpy(h + str_len(h), rl_padmode == RL_PADS1 ? " PAD" : " PADS", sizeof h - str_len(h));
    return h;
}

/* the map's signature: what it shows */
static uint32_t rd_lanes_sig(void)
{
    uint32_t i, h = 0;
    for (i = 0; i < 16u; i++)
        h = h * 31u + rd_lane[i] + rd_rate[i] * 8u + rd_acc[i] * 64u;
    return h;
}
static uint32_t rl_sig(void)
{
    track_t *t = rl_track();
    return (uint32_t)rl_held * 31u + (uint32_t)rlu.fl * 131u + (uint32_t)rl_rates * 7919u + rl_latch * 4099u + rl_padmode * 3u +
           (uint32_t)rl_live() * 977u + rl_groove * 5u + rl_pick * 11u + rlu.nk * 13u + (uint32_t)song.g[G_CLOCK] * 17u + fm1_in.notes * 37u +
           (uint32_t)drum_track(t) * 19u + trk_index(t) * 23u + (uint32_t)t->p[P_E0] * 29u +
           rl_ready * 41u + (uint32_t)rd_on * 43u + (uint32_t)rd_latch * 47u + rd_lanes_sig() * 53u +   /* (0.14 READY) */
           rl_fxpage * 59u + rd_hit_all * 61u + rfu.genre * 67u + rfu.bank * 71u + (uint32_t)bz_shift_held() * 73u;   /* (0.19) */
}
