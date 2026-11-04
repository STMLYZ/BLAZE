# BLAZE

**Build · Loop · Accent · Zap · Echo-out** — a DJ companion firmware for the M-VAVE FM-1, built on
[Felucca](https://github.com/hugelton/Felucca) by Leo Kuroshita / Hügelton Instruments (GPL-3.0-only).
Everything Felucca does stays; BLAZE adds performance pages on top.

## 0.1 — ROLL

Hold **ARP** (or double-tap it to lock) and the keys become the ROLL layer. Tapping ARP still opens ARP's page.

| Keys | What they do |
|---|---|
| White 1–8 | Drum pads: KICK SNARE CLAP HAT CL HAT OP TOM RIM BELL (the DRUM track's lanes) |
| White 9–16 | The same pads, accented |
| Black 1–7 | Roll rates: 1 bar, 1/4, 1/8, 1/8 triplet, 1/16, 1/16 triplet, 1/32 |
| Black 8 | TAP tempo |
| Black 9 / 10 | NUDGE − / + (tempo 1/32 slower / faster while held) |
| Black 11 | PADS: tap for 16 → 4 → 1 live pads; hold + tap white keys to pick them |
| OCT+ | GROOVE (held rates combine) ↔ LAST (newest rate wins) |
| KNOB 1–4 | BPM (internal clock), BUILD (rolls swell over a bar), DECAY, LEVEL |

- A pad alone plays once. Pad + rate rolls.
- While the transport plays, a roll starts on its own grid (never more than a beat away). Stopped, it starts at once.
- Rolls follow the external MIDI clock (rail-bridge / rekordbox Link) when it runs.
- Hits go to the DRUM track and out over USB MIDI.

Tests: `tests/blaze_roll_test.c` (in `tests/run_tests.sh`), screens in `tests/ui_render.c` (`blaze_roll*`).

ROLL's map is in full words, each kind of key outlined in its own colour (each drum lane its own; rates the palette's
theme colour; settings grey), filled solid while held, so it reads in glare.

## 0.2 — BLAZE (the home layer), themes, the key from RAIL

### BLAZE: hold ENV (double-tap to lock)

Tapping ENV still opens ENV's page.

| Keys | What they do |
|---|---|
| White 1–4 | The 4 pads ROLL's PADS 4 picked (default SNARE CLAP HAT OPEN), rolled by the black rates |
| White 5–10 | ECHO, REVERB, REVERSE, BRAKE (tape stop), FREEZE, STUTTER — **tap**: latched on (tap again: off); **hold**: on while held |
| White 11 / 12 | BUILD 4 / BUILD 8: ROLL's PADS 1 pad (SNARE) from the next bar, 1/4 → 1/8 → 1/16 → 1/32 over 4 or 8 bars, getting louder, ending on the next phrase's 1. Press again: stop |
| White 13 | RISER (0.3): noise sweeping up and swelling from the next bar over 8 bars (tap twice quickly: 16), cut on the 1 after it. Press again: stop |
| Black 1–6 | Roll rates 1/4, 1/8, 1/8 T, 1/16, 1/16 T, 1/32 |
| Black 7 | TAP tempo |
| Black 8 / 9 | NUDGE − / + |
| Black 10 | RELEASE: every latched effect, the build, the riser off; COLOR to the middle, THROW off, DEPTH full |
| Black 11 | THE ONE: now is bar 1 beat 1 of the phrase (to the nearest beat) |
| KNOB 1–4 | COLOR (left low-pass, right high-pass; it stays where you leave it), PITCH and DECAY of the drum track, DEPTH (the buffer effects' level) |

The strip on top: the key as Camelot (in the Camelot wheel's colour) and in note names (sharps and flats), and the
phrase: 16 bars, the bar now filling beat by beat, the last bar flashing red (the drop is next).

### The key from RAIL

RAIL (v4) sends the key it is set to on MIDI channel 16 — CC 20 the Camelot number, CC 21 A (0) / B (127), CC 22 the
root — to the FM-1 as well as to the visuals. BLAZE sets ROOT and SCALE (B: major, A: minor) of every track whose keys
play notes (not DRUM, not SLICE), and from then on the keys of the scale glow (as MENU > SCALE LEDS ON). CC 20–22 on
channel 16 are BLAZE's whatever ROUT is. SCL's layer still sets the key by hand; BLAZE shows whichever was set last.

### Themes

MENU > THEME has three more palettes: **BOOTH** (black, hot orange, cyan: a dark booth), **SUN** (white, deep blue,
black text, violet: daylight and glare), **CAMERA** (near-black, violet, gold, nothing pure white: on stream).

Tests: `tests/blaze_home_test.c`; screens `blaze_home*` in `tests/ui_render.c`.

## 0.3 — SHIFT, each drum its own, RISER, KEYS

In BLAZE (hold ENV):

| Control | What it does |
|---|---|
| KNOB 1–4 | COLOR, PITCH, DECAY, DEPTH (as 0.2) |
| OCT+ held (SHIFT) + KNOB 1–4 | TONE, SNAP, DRIVE (the kit), THROW (how much of the FM-1 goes into the echo and reverb) |
| A pad held + KNOB 1–3 | That drum only: TUNE, DECAY, LEVEL. Saved with the sound and the project |
| ALGO | The KIT (80 10 66 55 77 DEEP TIGHT PERC ...) |
| White 13 | RISER (IMPACT is gone) |
| OCT- | KEYS ↔ BLAZE |

### KEYS (OCT- in BLAZE)

| Keys | What they do |
|---|---|
| White 1–16 | The KEYS track's notes up its scale from the root (the key RAIL sent, or SCL's): every key in key |
| Black 1–4 | The KEYS track: T1–T4 (a DRUM track is refused) |
| Black 5 / 6 | ARP on / off, LATCH (the arp keeps running after you let go) |
| Black 7–9 | Arp rate 1/8, 1/16, 1/32 |
| Black 10 / 11 | Octave − / + |
| KNOB 1–4 | FILTER, GATE (the arp's), RANGE (octaves the arp climbs), SPACE (reverb send) |
| ALGO | The arp's pattern: UP, DOWN, UP-DOWN, RANDOM, ... |

Each drum's TUNE and DECAY live in parameters DIGITAL's operator envelopes held (inert since 1.0), marked as BLAZE's;
a sound or project from before plays as designed.

## 0.4 — the riser's sound, EXIT (echo out), CHORD

### RISER: hold it and turn the knobs (kept with the FM-1's settings)

| Knob | |
|---|---|
| 1 TYPE | NOISE, AIR (thin, bright hiss), SIREN (a resonant whistle), TONE (two saws climbing two octaves from the key's root), SYNTH (one of your tracks: its note bent up) |
| 2 TOP / BEND | How bright it ends (SYNTH: how far it bends, up to 2 octaves) |
| 3 LEVEL | How loud it peaks |
| 4 SPACE / TRACK | How much goes into the reverb (SYNTH: which track plays it) |

### EXIT (white 14): the echo out

From the next bar the FM-1 goes into the echo for a beat, then every track is cut while the echo rings on for BARS
(hold EXIT + KNOB 1: 1, 2 or 4). On its own clock the FM-1 then stops, ready for PLAY; following rekordbox (Link) it
stays silent (OUT) until EXIT again. EXIT while it runs, or RELEASE: back at once.

The BLAZE map now shows the key in the header and the phrase as a thin bar across the top.

### CHORD (OCT- twice: BLAZE → KEYS → CHORD → BLAZE)

| Keys | |
|---|---|
| White 1–7 | The key's seven chords (A minor: Am Bdim C Dm Em F G). Tap: latched (it plays on after you let go of ENV); another chord takes over; the same one again ends it |
| White 8 | OFF |
| Black 1–5 | How it plays: HOLD, stabs on 1/4 1/8 1/16, GATE (a 1/16 pattern: ALGO picks STRAIGHT SKIP SHUFFLE TRESILLO) |
| Black 6 / 7 | 7TH (four-note chords: Am7 Cmaj7 G7), INVERT (the low notes moved up) |
| Black 8 / 9 | Octave − / + |
| Black 10 / 11 | TRACK (which track plays the chords), ARP (the chord arpeggiated) |
| KNOB 1–4 | FILTER, LENGTH (of each stab / gate step), SPACE, LEVEL |

RELEASE (in BLAZE) also ends the chord.
