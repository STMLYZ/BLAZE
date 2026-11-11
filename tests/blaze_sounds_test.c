/* SPDX-License-Identifier: GPL-3.0-only
 * BLAZE 0.15 sound pack (firmware/src/blaze_sounds.c): Felucca-Engines' six engines play as ENG_*_BZ. Checks that
 * each is its original field for field but the preset table, that the table starts with the original rows in their
 * order (stored numbers keep their sound), that every BLAZE preset (appended) loads, sounds, stays below full scale,
 * has a category of its own, and that BZ_PX names real presets and sound parameters. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int same_row(const preset_t *a, const preset_t *b)
{
    return !strcmp(a->name, b->name) && !memcmp(a->e, b->e, sizeof a->e) && !memcmp(a->env, b->env, sizeof a->env) &&
           a->fenv == b->fenv && a->mono == b->mono && !memcmp(a->fx, b->fx, sizeof a->fx) && a->pat == b->pat;
}

static int same_engine(const engine_t *o, const engine_t *z)
{
    engine_t a = *o, b = *z;
    uint32_t i;
    if (z->npresets <= o->npresets)
        return 0;
    for (i = 0; i < o->npresets; i++)
        if (!same_row(&o->presets[i], &z->presets[i]))
            return 0;
    a.presets = b.presets = 0;
    a.npresets = b.npresets = 0;
    return !memcmp(&a, &b, sizeof a);
}

/* engine e preset k on T1 alone, a chord held 1 s: its peak (Q15), 0 when silent */
static int32_t sound_peak(uint32_t e, uint32_t k)
{
    static int32_t out[2 * CTL];
    track_t *t = &trk[0];
    uint32_t b, j, note = e == ENGI_DRUM ? 36u : 60u;
    int32_t pk = 0;
    for (j = 0; j < NTRK; j++)
        trk[j].p[P_MUTE] = 1;
    set_engine_of(t, e);
    apply_preset_to(t, k);
    t->p[P_MUTE] = 0;
    for (b = 0; b < 50u; b++)
        mix_block(out, CTL);
    trk_note_on(t, note, 110);
    if (e != ENGI_DRUM)
        trk_note_on(t, note + 7u, 110);
    for (b = 0; b < 44100u / CTL; b++) {
        memset(out, 0, sizeof out);
        mix_block(out, CTL);
        for (j = 0; j < 2u * CTL; j++) {
            int32_t v = out[j] < 0 ? -out[j] : out[j];
            if (v > pk)
                pk = v;
        }
    }
    trk_note_off(t, note);
    trk_note_off(t, note + 7u);
    for (b = 0; b < 200u; b++)
        mix_block(out, CTL);
    return pk;
}

static int bad;
int main(void)
{
    static const struct {
        const engine_t *o, *z;
    } W[] = {{&ENG_ANALOG, &ENG_ANALOG_BZ}, {&ENG_PHASE, &ENG_PHASE_BZ}, {&ENG_LOFI, &ENG_LOFI_BZ},
             {&ENG_TRIO, &ENG_TRIO_BZ},     {&ENG_WHEEL, &ENG_WHEEL_BZ}, {&ENG_NOISE, &ENG_NOISE_BZ}};
    /* the engines with BLAZE presets and the first of them */
    static const struct {
        uint32_t e, first;
    } P[] = {{0, 12}, {2, 6}, {3, 5}, {5, 4}, {6, 5}, {7, 5}, {10, 4}, {11, 4}};
    uint32_t i, k, n = 0, quiet = 0, loud = 0, nocat = 0, stab = 0;
    int ok = 1;
    ui_power_on();
    song.master_q12 = 4096;
    for (i = 0; i < NELEM(W); i++)
        ok &= same_engine(W[i].o, W[i].z);
    bad += check("the six engines: their own engine_t but the presets, their rows first, in order", ok);
    for (i = 0; i < NELEM(W); i++)
        ok &= ENGINES[0] != W[i].o && (ENGINES[0] == &ENG_ANALOG_BZ);
    bad += check("ENGINES[] plays the _BZ engines", ok && ENGINES[2] == &ENG_PHASE_BZ && ENGINES[11] == &ENG_NOISE_BZ);
    ok = 1;
    for (i = 0; i < NELEM(BZ_PX); i++)
        for (k = 0; k < 7u && BZ_PX[i].p[k]; k++)
            ok &= BZ_PX[i].e < NENGINES && BZ_PX[i].k < ENGINES[BZ_PX[i].e]->npresets && BZ_PX[i].k >= 4u &&
                  BZ_PX[i].p[k] < P_E0;
    bad += check("BZ_PX: real presets (BLAZE's), sound parameters below E1", ok);
    for (i = 0; i < NELEM(P); i++)
        for (k = P[i].first; k < ENGINES[P[i].e]->npresets; k++) {
            int32_t pk = sound_peak(P[i].e, k);
            uint32_t c = preset_cat(P[i].e, k);
            n++;
            if (pk < 600)
                printf("  quiet: %s %d\n", ENGINES[P[i].e]->presets[k].name, pk), quiet++;
            if (pk > 32767)
                printf("  over: %s %d\n", ENGINES[P[i].e]->presets[k].name, pk), loud++;
            if (c == cat_of_engine(P[i].e) && P[i].e != ENGI_DRUM)
                nocat++;
            stab += c == CAT_STAB;
        }
    printf("sound pack: %u presets\n", n);
    bad += check("every BLAZE preset sounds (a chord held a second), below full scale", n == 32u && !quiet && !loud);
    bad += check("every BLAZE preset has its category; STAB has RAVE STAB, HOUSE STAB, ORGAN STAB", !nocat && stab == 3u);
    {   /* SUPERSAW: UNISON, its detune and spread from BZ_PX; the next preset loads without them */
        track_t *t = &trk[0];
        set_engine_of(t, 0);
        apply_preset_to(t, 18);
        ok = !strcmp(ENGINES[0]->presets[18].name, "SUPERSAW") && t->p[P_VOICE] == V_UNISON && t->p[P_DETUNE] == 70 &&
             t->p[P_SPRD] == 110;
        apply_preset_to(t, 0);
        ok &= t->p[P_VOICE] != V_UNISON && t->p[P_DETUNE] == TP[P_DETUNE].def;
    }
    bad += check("SUPERSAW: UNISON with its detune and spread; SAW LEAD after it: none of them", ok);
    printf("%s\n", bad ? "BLAZE SOUNDS: FAILED" : "BLAZE SOUNDS: all ok");
    return bad != 0;
}
