/* SPDX-License-Identifier: GPL-3.0-only
 * BLAZE (a DJ companion built on Felucca by Leo Kuroshita / Hügelton Instruments) */
/* ROLL, the main loop's side (ui_layer.c includes it): TAP and NUDGE (black keys 8..10), KNOB 1..4 BPM BUILD DECAY
 * LEVEL, OCT+ GROOVE / LAST, the LEDs, what it says. The pads and the rates are the ISR's (blaze_roll.c).
 * NUDGE: while held, the tempo 1/32 faster / slower (about 3 %: a jog wheel's push), back when let go: the FM-1
 * pulled back into line with a track by ear. TAP, NUDGE and KNOB 1 work on the internal clock only (an external
 * clock sets the tempo: they say so) */
static struct {
    uint32_t nk;                       /* the NUDGE keys held (bits 0..26) */
    int16_t bpm;                       /* the tempo before NUDGE */
    uint8_t mode, groove;              /* the PADS mode and GROOVE last said */
    uint16_t fl;                       /* pads hit lately (the LEDs, the screen) */
    uint32_t fl_t;                     /* .. when (fm1_ms) */
} rlu = {0, 0, 0, 1, 0, 0};
#define RL_FLASH_MS 70u
static const char *const RL_PADS_NAME[RL_NPADMODE] = {"16", "4", "1"};

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
        int32_t d;
        if (song.g[G_CLOCK]) {
            ui_message("NUDGE: CLK IS EXT");
            return;
        }
        if (!rlu.nk)
            rlu.bpm = song.g[G_BPM];
        rlu.nk |= 1u << k;
        d = rlu.bpm / 32 > 1 ? rlu.bpm / 32 : 1;
        song.g[G_BPM] = (int16_t)clamp(rlu.bpm + (p == RLK_NUP ? d : -d), GP[G_BPM].min, GP[G_BPM].max);
        ui_message(p == RLK_NUP ? "NUDGE +" : "NUDGE -");
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
    if (k == 0u) {
        if (song.g[G_CLOCK]) {
            ui_message("BPM: CLK IS EXT");
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

/* the map's header: "[ARP] GROOVE 16" */
static const char *rl_head(void)
{
    static char h[20];
    str_cpy(h, rl_groove ? "[ARP] GROOVE " : "[ARP] LAST ", sizeof h);
    str_cpy(h + str_len(h), RL_PADS_NAME[rl_padmode % RL_NPADMODE], sizeof h - str_len(h));
    return h;
}

/* the map's signature: what it shows */
static uint32_t rl_sig(void)
{
    track_t *t = rl_track();
    return (uint32_t)rl_held * 31u + (uint32_t)rlu.fl * 131u + (uint32_t)rl_rates * 7919u + rl_padmode * 3u +
           (uint32_t)rl_live() * 977u + rl_groove * 5u + rl_pick * 11u + rlu.nk * 13u + (uint32_t)song.g[G_CLOCK] * 17u +
           (uint32_t)drum_track(t) * 19u + trk_index(t) * 23u + (uint32_t)t->p[P_E0] * 29u;
}
