/* SPDX-License-Identifier: GPL-3.0-only
 * BLAZE 0.15: the sound pack. Felucca-Engines (MIT, the submodule firmware/engines/) keeps its six engines' preset
 * tables; BLAZE cannot append to them there, so each of those engines plays here as ENG_<NAME>_BZ: the engine's own
 * engine_t, field for field (tests/blaze_sounds_test.c checks it against the original), with a preset table that is
 * the original's rows, copied in their order (checked the same way), then BLAZE's, appended. A stored preset number
 * (projects, user presets, program change, the editor) keeps playing the same sound. The rows copied from
 * Felucca-Engines are MIT (Copyright (C) 2026 Hügelton Instruments); BLAZE's own are GPL-3.0-only with the rest.
 * Included by engines.c after the engines, before ENGINES[]. VOICE and DRUM (in-tree) take theirs in their own files.
 *
 * BZ_PX: what a preset row cannot hold (felucca_engine.h preset_t: E1..E8, the ENV, FLT, MONO, the sends), set by
 * apply_preset_to (ui.c) after the row: UNISON, its detune and spread, glide, ENV -> PITCH, the LFO, the INSERT,
 * the filter type. Every other parameter of the sound is at its default, as for any preset. */
#if !BLAZE_SOUNDS_OFF

/* ANALOG: the original ANALOG_PRESETS rows (Felucca-Engines), then BLAZE's */
static const preset_t BZ_ANALOG_PRESETS[] = {
    {"SAW LEAD", {0, 12, 64, 0, 90, 30, 10, 64}, {4, 70, 100, 50}, 20, 1, FX(0, 10, 45, 30), PAT(4)},
    {"SOFT PAD", {0, 20, 64, 4, 60, 10, 0, 32}, {80, 90, 110, 95}, 10, 0, FX(0, 60, 20, 70), PAT(5)},
    {"SQR BASS", {1, 0, 0, 0, 50, 70, 40, 64}, {0, 60, 40, 30}, 30, 1, FX(5, 0, 10, 10), PAT(2)},
    {"PWM STR", {4, 8, 40, 0, 75, 20, 0, 48}, {60, 80, 110, 85}, 8, 0, FX(0, 50, 20, 60), PAT(5)},
    {"ACID", {0, 0, 0, 0, 50, 100, 25, 64}, {0, 55, 20, 30}, 48, 1, FX(20, 0, 45, 15), PAT(1)},
    {"SINE KEY", {3, 6, 50, 0, 127, 0, 0, 0}, {2, 80, 30, 70}, 0, 0, FX(0, 30, 25, 40), PAT(6)},
    {"RAVE", {4, 30, 64, 0, 85, 20, 30, 64}, {20, 80, 110, 60}, 10, 1, FX(30, 40, 30, 30), PAT(13)},
    {"SUB BASS", {3, 0, 0, 0, 40, 0, 20, 0}, {0, 60, 100, 20}, 0, 1, FX(0, 0, 0, 10), PAT(8)},
    {"PLUCK", {0, 8, 50, 0, 30, 40, 0, 64}, {0, 88, 0, 60}, 55, 0, FX(0, 20, 50, 30), PAT(3)},
    {"BRASS", {0, 10, 64, 0, 45, 20, 10, 64}, {35, 70, 90, 45}, 40, 0, FX(0, 20, 20, 40), PAT(6)},
    {"WIND", {0, 0, 0, 90, 30, 90, 0, 0}, {60, 90, 60, 80}, 50, 0, FX(0, 30, 30, 70), PAT(5)},
    {"STRINGS", {0, 25, 64, 0, 70, 10, 0, 32}, {70, 90, 115, 90}, 5, 0, FX(0, 60, 20, 70), PAT(5)},
    /* BLAZE 0.15 sound pack, appended (stored numbers stay): name, {WAVE DTN MIX NOIS CUT RES DRV KTR}, {A D S R},
     * fenv, mono; BZ_PX below adds what a preset row cannot hold (UNISON, glide, LFO, ENV -> PITCH, INSERT) */
    {"REESE", {0, 34, 64, 0, 52, 22, 20, 40}, {0, 70, 110, 30}, 6, 1, FX(10, 20, 0, 12), PAT(8)},
    {"ACID SQR", {1, 0, 0, 0, 38, 112, 50, 64}, {0, 52, 18, 24}, 56, 1, FX(25, 0, 40, 10), PAT(1)},
    {"ACID SLIDE", {0, 0, 0, 0, 44, 118, 60, 64}, {0, 56, 30, 24}, 50, 1, FX(35, 0, 45, 12), PAT(1)},
    {"OFFBEAT", {0, 8, 64, 0, 48, 36, 30, 48}, {0, 66, 20, 40}, 34, 1, FX(0, 0, 0, 6), PAT(2)},
    {"ROLLER", {6, 4, 90, 0, 40, 30, 50, 40}, {0, 66, 40, 30}, 30, 1, FX(10, 0, 0, 4), PAT(2)},
    {"DARK SUB", {3, 0, 0, 0, 40, 0, 70, 0}, {0, 70, 110, 22}, 0, 1, FX(20, 0, 0, 6), PAT(8)},
    {"SUPERSAW", {0, 16, 64, 0, 100, 14, 10, 64}, {6, 80, 112, 56}, 6, 0, FX(0, 40, 35, 48), PAT(4)},
    {"HOOVER", {4, 30, 64, 0, 92, 20, 40, 64}, {14, 80, 110, 50}, 0, 1, FX(10, 50, 20, 30), PAT(4)},
    {"RAVE STAB", {0, 22, 64, 0, 74, 34, 30, 64}, {0, 74, 56, 50}, 40, 0, FX(15, 20, 35, 30), PAT(6)},
    {"HOUSE STAB", {4, 10, 52, 0, 64, 28, 10, 48}, {0, 66, 30, 56}, 30, 0, FX(0, 30, 30, 46), PAT(6)},
    {"TRANCE PLK", {0, 14, 64, 0, 36, 30, 10, 64}, {0, 82, 0, 60}, 52, 0, FX(0, 20, 55, 40), PAT(13)},
    {"COLD STRNG", {4, 12, 40, 0, 64, 12, 0, 40}, {70, 90, 110, 88}, 6, 0, FX(0, 70, 25, 70), PAT(5)},
    {"DARK PAD", {0, 26, 64, 6, 46, 24, 10, 32}, {84, 90, 115, 96}, 8, 0, FX(0, 60, 30, 80), PAT(5)},
    {"SIREN", {1, 0, 0, 0, 90, 10, 20, 64}, {4, 64, 120, 60}, 0, 1, FX(10, 0, 40, 40), PAT(5)},
    {"DOWNLIFTER", {0, 40, 64, 110, 110, 40, 0, 0}, {0, 112, 0, 100}, -40, 0, FX(0, 30, 40, 80), PAT(5)},
};
static const engine_t ENG_ANALOG_BZ = {
    .name = "ANALOG",
    .page_title = {"OSC", "FLT"},
    .edit = {
        {"WAVE", F_ENUM, 0, 6, 0, N_ANALOG_WAVE, 0},
        {"DTN", F_INT, 0, 127, 10, 0, "ct"},
        {"MIX", F_PCT, 0, 127, 64, 0, 0},
        {"NOIS", F_PCT, 0, 127, 0, 0, 0},
        {"CUT", F_CUTOFF, 0, 127, 90, 0, 0},
        {"RES", F_PCT, 0, 127, 30, 0, 0},
        {"DRV", F_PCT, 0, 127, 0, 0, 0},
        {"KTR", F_PCT, 0, 127, 64, 0, 0},
    },
    .presets = BZ_ANALOG_PRESETS,
    .npresets = NELEM(BZ_ANALOG_PRESETS),
    .note_on = analog_note_on,
    .render = analog_render,
    .desc = analog_desc,
    .knob = {P_E4, P_E5, P_ATK, P_REL},
    .keep = 0x03,                /* the filter */
};

/* PHASE: the original PHASE_PRESETS rows (Felucca-Engines), then BLAZE's */
static const preset_t BZ_PHASE_PRESETS[] = {
    /* name, {WAVE, WAVE2, DCW, ENV, DTN, LINE, SUB, FOLD}, {A D S R}, fenv, mono */
    {"BRASS", {0, 0, 30, 90, 0, 0, 0, 0}, {8, 70, 90, 40}, 0, 0, FX(0, 20, 20, 40), PAT(6)},
    {"ORGAN", {3, 0, 40, 0, 0, 0, 0, 0}, {0, 127, 127, 30}, 0, 0, FX(0, 40, 0, 30), PAT(6)},
    {"STRING", {0, 4, 50, 40, 12, 0, 0, 0}, {40, 90, 100, 70}, 0, 0, FX(0, 50, 20, 60), PAT(5)},
    {"RESO", {5, 0, 60, 60, 0, 0, 0, 0}, {0, 70, 30, 60}, 0, 0, FX(0, 0, 40, 40), PAT(1)},
    {"BELL", {6, 0, 80, 50, 0, 0, 0, 0}, {0, 95, 0, 90}, 0, 0, FX(0, 0, 30, 70), PAT(7)},
    {"WIRE", {4, 7, 70, 40, 7, 0, 0, 0}, {10, 80, 80, 60}, 0, 0, FX(15, 30, 30, 40), PAT(4)},
    /* BLAZE 0.15 sound pack, appended: name, {WAVE WAVE2 DCW ENV DTN LINE SUB FOLD}, {A D S R}, fenv, mono */
    {"COLD BASS", {5, 0, 56, 70, 0, 0, 64, 0}, {0, 60, 50, 24}, 0, 1, FX(10, 10, 0, 10), PAT(2)},
    {"POST LEAD", {6, 4, 70, 40, 9, 0, 0, 10}, {2, 80, 100, 50}, 0, 1, FX(0, 50, 40, 40), PAT(4)},
};
static const engine_t ENG_PHASE_BZ = {
    .name = "PHASE",
    .page_title = {"PHS", "LINE"},
    .edit = {
        {"WAVE", F_ENUM, 0, 7, 0, N_PD_WAVE, 0},
        {"WAVE2", F_ENUM, 0, 8, 0, N_PD_WAVE2, 0},
        {"DCW", F_PCT, 0, 127, 60, 0, 0},
        {"ENV", F_PCT, 0, 127, 64, 0, 0},
        {"DTN", F_INT, 0, 127, 0, 0, "ct"},
        {"LINE", F_ENUM, 0, 1, 0, N_PD_LINE, 0},
        {"SUB", F_PCT, 0, 127, 0, 0, 0},
        {"FOLD", F_PCT, 0, 127, 0, 0, 0},       /* 0 = off */
    },
    .presets = BZ_PHASE_PRESETS,
    .npresets = NELEM(BZ_PHASE_PRESETS),
    .note_on = phase_note_on,
    .render = phase_render,
    .keep = 0x30,                /* FOLD's DC blocker: a retrigger does not restart it */
    .knob = {P_E2, P_E3, P_E4, P_REL},
};

/* LOFI: the original LOFI_PRESETS rows (Felucca-Engines), then BLAZE's */
static const preset_t BZ_LOFI_PRESETS[] = {
    {"PULSE LD", {0, 0, 32, 0, 0, 20, 0, 127}, {0, 60, 90, 30}, 0, 1, FX(0, 0, 40, 20), PAT(4)},
    {"WAVE BASS", {1, 1, 0, 0, 0, 0, 0, 90}, {0, 50, 70, 20}, 0, 1, FX(0, 0, 10, 0), PAT(2)},
    {"ARP 8BIT", {2, 0, 96, 0, 0, 0, 2, 110}, {0, 60, 80, 40}, 0, 0, FX(0, 0, 30, 20), PAT(13)},
    /* wave RAM VOXA (DUTY 92 / 8 = 11), a little vibrato */
    {"WAVE LEAD", {0, 4, 92, 0, 0, 18, 0, 110}, {0, 70, 90, 30}, 0, 1, FX(0, 0, 40, 25), PAT(3)},
    /* STEP: 25 % pulse, DCY 34 = D7 (a 15-step decay over 0.5 s), REL ~54 ms of staircase after note-off */
    {"STEP LEAD", {4, 0, 40, 34, 0, 16, 0, 127}, {0, 64, 127, 55}, 0, 1, FX(0, 0, 40, 20), PAT(4)},
    /* BLAZE 0.15 sound pack, appended: name, {CHIP WAVE DUTY CRSH SWP VIB ARP TONE}, {A D S R}, fenv, mono */
    {"LASER", {2, 0, 40, 0, 90, 0, 0, 127}, {0, 74, 0, 40}, 0, 1, FX(0, 0, 50, 30), PAT(7)},
};
static const engine_t ENG_LOFI_BZ = {
    .name = "LOFI",
    .page_title = {"CHIP", "MOTN"},
    .edit = {
        {"CHIP", F_ENUM, 0, 4, 0, N_CHIP, 0},
        {"WAVE", F_ENUM, 0, 4, 0, N_RWAVE, 0},
        {"DUTY", F_INT, 0, 127, 64, 0, 0},              /* WRAM: the table, DUTY / 8 (lofi_desc) */
        {"CRSH", F_PCT, 0, 127, 0, 0, 0},               /* STEP: the envelope (lofi_desc) */
        {"SWP", F_PCT, 0, 127, 0, 0, 0},
        {"VIB", F_PCT, 0, 127, 0, 0, 0},
        {"ARP", F_ENUM, 0, 3, 0, N_RARP, 0},
        {"TONE", F_PCT, 0, 127, 127, 0, 0},
    },
    .presets = BZ_LOFI_PRESETS,
    .npresets = NELEM(BZ_LOFI_PRESETS),
    .note_on = lofi_note_on,
    .render = lofi_render,
    .knob = {P_E1, P_E2, P_E3, P_REL},
    .keep = 0x11,                /* the held sample and the low-pass */
    .amp = lofi_amp,
    .desc = lofi_desc,
};

/* TRIO: the original TRIO_PRESETS rows (Felucca-Engines), then BLAZE's */
static const preset_t BZ_TRIO_PRESETS[] = {
    /* name, {WAVE, INT2, INT3, DTN, MODE, CUT, RES, PW}, {A D S R}, fenv, mono */
    {"FAT BASS", {0, 0, -12, 9, 0, 62, 45, 64}, {0, 62, 60, 25}, 40, 1, FX(10, 0, 10, 10), PAT(2)},
    {"ARP LEAD", {2, 12, 0, 4, 0, 88, 25, 32}, {0, 60, 90, 30}, 8, 0, FX(0, 0, 35, 20), PAT(13)},
    {"SYNC LEAD", {9, 9, 0, 0, 0, 82, 30, 64}, {2, 70, 100, 40}, 18, 1, FX(0, 10, 40, 25), PAT(4)},
    {"RING BELL", {13, 0, 18, 6, 1, 96, 30, 64}, {0, 92, 0, 80}, 0, 0, FX(0, 20, 35, 55), PAT(7)},
    {"CHIP CHOIR", {1, 0, 12, 7, 1, 62, 95, 40}, {70, 90, 110, 85}, 30, 0, FX(0, 50, 20, 65), PAT(5)},
    /* BLAZE 0.15 sound pack, appended: name, {WAVE INT2 INT3 DTN MODE CUT RES PW}, {A D S R}, fenv, mono */
    {"SCREECH", {9, 19, 0, 0, 0, 96, 44, 64}, {0, 70, 110, 34}, 20, 1, FX(40, 0, 30, 24), PAT(4)},
    {"FIDGET", {10, 12, 0, 0, 0, 84, 50, 40}, {0, 44, 30, 20}, 30, 1, FX(20, 0, 20, 10), PAT(1)},
    {"ELECTRO", {0, 0, -12, 14, 0, 70, 56, 64}, {0, 56, 60, 20}, 40, 1, FX(50, 0, 10, 8), PAT(2)},
};
static const engine_t ENG_TRIO_BZ = {
    .name = "TRIO",
    .page_title = {"OSC", "TONE"},
    .edit = {
        {"WAVE", F_ENUM, 0, 15, 0, N_TRIO_WAVE, 0},
        {"INT2", F_SEMI, -24, 24, 0, 0, 0},
        {"INT3", F_SEMI, -24, 24, -12, 0, 0},
        {"DTN", F_INT, 0, 50, 6, 0, "ct"},
        {"MODE", F_ENUM, 0, 3, 0, N_TRIO_MODE, 0},
        {"CUT", F_CUTOFF, 0, 127, 80, 0, 0},
        {"RES", F_PCT, 0, 127, 40, 0, 0},
        {"PW", F_PCT, 0, 127, 64, 0, 0},
    },
    .presets = BZ_TRIO_PRESETS,
    .npresets = NELEM(BZ_TRIO_PRESETS),
    .note_on = trio_note_on,
    .render = trio_render,
    .knob = {P_E5, P_E6, P_E7, P_E1},
};

/* WHEEL: the original WHEEL_PRESETS rows (Felucca-Engines), then BLAZE's */
static const preset_t BZ_WHEEL_PRESETS[] = {
    /* name, {REG, SUB, BODY, TOP, PERC, CLICK, DRIVE, ROTR}, {A D S R}, fenv, mono */
    {"FULL ORGAN", {15, 0, 0, 0, 0, 30, 20, 1}, {0, 64, 127, 45}, 0, 0, FX(0, 0, 10, 35), PAT(5)},
    {"JAZZ PERC", {4, 0, 0, 0, 2, 50, 8, 1}, {0, 64, 127, 40}, 0, 0, FX(0, 0, 0, 25), PAT(3)},
    {"GOSPEL", {6, 0, 0, 0, 1, 60, 40, 2}, {0, 64, 127, 45}, 0, 0, FX(0, 0, 0, 40), PAT(6)},
    {"SOFT FLUTE", {1, 0, -2, 0, 0, 10, 0, 1}, {0, 64, 127, 55}, 0, 0, FX(0, 20, 15, 55), PAT(5)},
    {"ROCK DRIVE", {7, 0, 0, 0, 0, 70, 100, 2}, {0, 64, 127, 40}, 0, 0, FX(35, 0, 10, 25), PAT(4)},
    /* BLAZE 0.15 sound pack, appended: name, {REG SUB BODY TOP PERC CLICK DRIVE ROTR}, {A D S R}, fenv, mono */
    {"ORGAN STAB", {8, -2, 0, 2, 1, 90, 40, 0}, {0, 68, 0, 40}, 0, 0, FX(10, 0, 20, 30), PAT(6)},
};
static const engine_t ENG_WHEEL_BZ = {
    .name = "WHEEL",
    .page_title = {"BARS", "TONE"},
    .edit = {
        {"REG", F_ENUM, 0, 15, 4, N_DRW_REG, 0},
        {"SUB", F_INT, -8, 8, 0, 0, 0},
        {"BODY", F_INT, -8, 8, 0, 0, 0},
        {"TOP", F_INT, -8, 8, 0, 0, 0},
        {"PERC", F_ENUM, 0, 6, 0, N_DRW_PERC, 0},
        {"CLICK", F_PCT, 0, 127, 40, 0, 0},
        {"DRV", F_PCT, 0, 127, 0, 0, 0},
        {"ROTR", F_ENUM, 0, 2, 1, N_DRW_ROTR, 0},
    },
    .presets = BZ_WHEEL_PRESETS,
    .npresets = NELEM(BZ_WHEEL_PRESETS),
    .note_on = wheel_note_on,
    .render = wheel_render,
    .knob = {P_E0, P_E4, P_E6, P_E7},
    .block = wheel_block,
};

/* NOISE: the original NOISE_PRESETS rows (Felucca-Engines), then BLAZE's */
static const preset_t BZ_NOISE_PRESETS[] = {
    {"WIND", {NZ_ANLG, 64, 70, 100, 127, 0, 70, 0}, {90, 90, 110, 90}, 0, 0, FX(0, 30, 20, 80), PAT(5)},
    {"RAIN", {NZ_DUST, 30, 88, 80, 64, 105, 40, 0}, {10, 90, 127, 70}, 0, 0, FX(0, 0, 20, 70), PAT(5)},
    /* a 7-bit register (127 steps) clocked 84 semitones (7 octaves) above the key: one period per key period,
     * a pitched chip-style buzz */
    {"ARCADE", {NZ_LFSR, 0, 127, 0, 127, 112, 0, 84}, {0, 80, 0, 50}, 0, 0, FX(0, 0, 10, 20), PAT(3)},
    {"METAL", {NZ_META, 0, 100, 40, 100, 32, 0, 0}, {0, 75, 40, 60}, 10, 0, FX(0, 20, 30, 45), PAT(7)},
    /* BLAZE 0.15 sound pack, appended: name, {MODE COLR FREQ RES TRK DENS DRFT CRSH}, {A D S R}, fenv, mono */
    {"RISER", {NZ_ANLG, 64, 40, 90, 0, 0, 30, 0}, {112, 100, 127, 60}, 63, 0, FX(0, 30, 30, 70), PAT(5)},
};
static const engine_t ENG_NOISE_BZ = {
    .name = "NOISE",
    .page_title = {"SRC", "MOVE"},
    .edit = {
        {"MODE", F_ENUM, 0, 3, 0, N_NOISE_MODE, 0},
        {"COLR", F_PCT, 0, 127, 64, 0, 0},
        {"FREQ", F_CUTOFF, 0, 127, 80, 0, 0},
        {"RES", F_PCT, 0, 127, 30, 0, 0},
        {"TRK", F_PCT, 0, 127, 64, 0, 0},
        {"DENS", F_PCT, 0, 127, 0, 0, 0},           /* LFSR, META: the register (noise_desc) */
        {"DRFT", F_PCT, 0, 127, 0, 0, 0},
        {"CRSH", F_PCT, 0, 127, 0, 0, 0},           /* LFSR, META: the clock above the key */
    },
    .presets = BZ_NOISE_PRESETS,
    .npresets = NELEM(BZ_NOISE_PRESETS),
    .note_on = noise_note_on,
    .render = noise_render,
    .knob = {P_E2, P_E3, P_E1, P_REL},
    .keep = 0xF3,                /* the filter, the colour poles */
    .desc = noise_desc,
};

/* engine e's presets as Felucca-Engines has them (its plugin's): BLAZE's appended ones left out (platforms/host
 * fh_test_engine: an engine and its plugin compared preset for preset) */
static uint32_t bz_own_npresets(const engine_t *x)
{
    return x == &ENG_ANALOG_BZ ? NELEM(ANALOG_PRESETS) : x == &ENG_PHASE_BZ ? NELEM(PHASE_PRESETS)
         : x == &ENG_LOFI_BZ ? NELEM(LOFI_PRESETS) : x == &ENG_TRIO_BZ ? NELEM(TRIO_PRESETS)
         : x == &ENG_WHEEL_BZ ? NELEM(WHEEL_PRESETS) : x == &ENG_NOISE_BZ ? NELEM(NOISE_PRESETS) : x->npresets;
}

/* BZ_PX: a preset's extra values (engine, preset, then up to 7 parameter = value pairs, 0 = none) */
typedef struct {
    uint8_t e, k;
    uint8_t p[7];
    int8_t v[7];
} bz_px_t;
#define BZPX(e, k, ...) {(e), (k), __VA_ARGS__}
static const bz_px_t BZ_PX[] = {
    /* ANALOG (0): 12 REESE .. 26 DOWNLIFTER */
    {0, 12, {P_LRATE, P_LD_FLT, P_ITYPE, P_IA}, {20, 10, 1, 70}},                       /* REESE: slow filter drift, SOFT */
    {0, 13, {P_GLIDE, P_GLMODE}, {40, 1}},                                              /* ACID SQR: slides */
    {0, 14, {P_GLIDE, P_GLMODE, P_ITYPE, P_IA}, {44, 1, 1, 80}},                         /* ACID SLIDE */
    {0, 17, {P_ITYPE, P_IA}, {1, 96}},                                                  /* DARK SUB: SOFT */
    {0, 18, {P_VOICE, P_DETUNE, P_SPRD}, {V_UNISON, 70, 110}},                         /* SUPERSAW */
    {0, 19, {P_VOICE, P_DETUNE, P_SPRD, P_GLIDE, P_GLMODE}, {V_UNISON, 84, 100, 30, 1}},   /* HOOVER: the swoop */
    {0, 20, {P_VOICE, P_DETUNE, P_SPRD}, {V_UNISON, 50, 90}},                          /* RAVE STAB */
    {0, 22, {P_VOICE, P_DETUNE, P_SPRD}, {V_UNISON, 44, 100}},                         /* TRANCE PLK */
    {0, 23, {P_LRATE, P_LD_PIT}, {40, 2}},                                              /* COLD STRNG: a little vibrato */
    {0, 24, {P_LRATE, P_LD_FLT}, {14, 18}},                                             /* DARK PAD: the filter breathes */
    {0, 25, {P_LRATE, P_LWAVE, P_LD_PIT}, {70, 1, 30}},                                  /* SIREN: TRI on pitch */
    /* TRIO (6): 5 SCREECH, 6 FIDGET, 7 ELECTRO */
    {6, 5, {P_ITYPE, P_IA, P_IC}, {4, 80, 108}},                                        /* SCREECH: FUZZ, up */
    {6, 6, {P_GLIDE, P_GLMODE}, {24, 1}},                                               /* FIDGET */
    {6, 7, {P_ITYPE, P_IA, P_IC}, {4, 90, 100}},                                        /* ELECTRO: FUZZ */
    /* PHASE (2): 6 COLD BASS */
    {2, 6, {P_ITYPE, P_IA, P_IC}, {1, 60, 104}},                                        /* COLD BASS: SOFT, up */
    /* LOFI (3): 5 LASER */
    {3, 5, {P_ED_PIT}, {63}},                                                           /* LASER: a fast drop */
    /* NOISE (11): 4 RISER */
};
/* engine e's preset k's extras, onto t (after the row: apply_preset_to) */
static void bz_px_apply(track_t *t, uint32_t e, uint32_t k)
{
    uint32_t i, j;
    for (i = 0; i < NELEM(BZ_PX); i++)
        if (BZ_PX[i].e == e && BZ_PX[i].k == k)
            for (j = 0; j < 7u && BZ_PX[i].p[j]; j++)
                t->p[BZ_PX[i].p[j]] = BZ_PX[i].v[j];
}
#endif
