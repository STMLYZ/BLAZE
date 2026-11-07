/* SPDX-License-Identifier: GPL-3.0-only
 * BLAZE (a DJ companion built on Felucca by Leo Kuroshita / Hügelton Instruments) */
/* ROLL, the main loop's side (ui_layer.c includes it): TAP and NUDGE (black keys 8..10), KNOB 1..4 BPM BUILD DECAY
 * LEVEL, OCT+ GROOVE / LAST, the LEDs, what it says. The pads and the rates are the ISR's (blaze_roll.c).
 * NUDGE: while held, the tempo 1/32 faster / slower (about 3 %: a jog wheel's push), back when let go: the FM-1
 * pulled back into line with a track by ear. TAP, NUDGE and KNOB 1 work on the internal clock only (an external
 * clock sets the tempo: they say so) */
static const char *rl_lane_name(const track_t *t, uint32_t l);   /* (blaze_roll_draw.c) */
static int rd_knob(uint32_t k, int32_t s);                       /* (0.14 READY ROLLS: below) */
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
        *on = rl_padmode != RL_PADS16 || rl_pick;
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
        rd_rate[i] = (uint8_t)(r >= 1u && r <= 6u ? r : 4u);
        rd_acc[i] = (uint8_t)((c[1u + i] >> 6) & 1u);
    }
}
/* .. key i's roll as a cell's name: "SNARE 16", "HAT 16T" (the rate's denominator) */
static const char *rd_name(uint32_t i, char *b, uint32_t n)
{
    static const char *const R[7] = {"", "4", "8", "8T", "16", "16T", "32"};
    static const char *const S[NLANE] = {"KCK", "SNR", "CLP", "HAT", "OPN", "TOM", "RIM", "BEL"};
    str_cpy(b, rl_lane_name(rl_track(), rd_lane[i % 16u] % NLANE), n);
    if (str_len(b) + 1u + str_len(R[rd_rate[i % 16u] % 7u]) > 8u)   /* (too long for a cell: "SNR 16T") */
        str_cpy(b, S[rd_lane[i % 16u] % NLANE], n);
    str_cpy(b + str_len(b), " ", n - str_len(b));
    str_cpy(b + str_len(b), R[rd_rate[i % 16u] % 7u], n - str_len(b));
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
/* .. a key held + KNOB 1 its DRUM, KNOB 2 its RATE (1/4 .. 1/32), KNOB 3 ACCENT; kept */
static int rd_knob(uint32_t k, int32_t s)
{
    uint32_t i = rd_held(), v;
    uint8_t *c = RD_ROW;
    char nm[16];
    if (!rl_ready || i >= 16u || k > 2u)
        return 0;
    rd_cfg_apply();
    v = c[1u + i];
    if (k == 0u)
        v = (v & ~7u) | (((v & 7u) + (s > 0 ? 1u : NLANE - 1u)) % NLANE);
    else if (k == 1u) {
        uint32_t r = (v >> 3) & 7u;
        r = s > 0 ? (r < 6u ? r + 1u : 6u) : (r > 1u ? r - 1u : 1u);
        v = (v & ~(7u << 3)) | r << 3;
    } else
        v = s > 0 ? v | 1u << 6 : v & ~(1u << 6);
    c[1u + i] = (uint8_t)v;
    rd_cfg_apply();
    ui_say(rd_acc[i] ? "ACCENT: " : "READY: ", rd_name(i, nm, sizeof nm));
    return 1;
}
static void rl_ready_toggle(void)
{
    rl_ready = (uint8_t)!rl_ready;
    rd_cfg_apply();
    ui_message(rl_ready ? "READY ROLLS: TAP ONE, THEN THE NEXT" : "ROLL: PADS AND RATES");
}

static const char *rl_head(void)
{
    static char h[28];
    if (rl_ready)                                       /* (0.14) */
        return rd_held() < 16u ? "[ARP] READY: DRUM RATE ACC" : "[ARP] READY ROLLS";
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
           rl_ready * 41u + (uint32_t)rd_on * 43u + (uint32_t)rd_latch * 47u + rd_lanes_sig() * 53u;   /* (0.14 READY) */
}
