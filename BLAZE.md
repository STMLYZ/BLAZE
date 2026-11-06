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

## 0.5 — LOOPS, DUCK, RAMP, blends, the lights by fours, the battery in %

### LOOPS (OCT- three times: BLAZE → KEYS → CHORD → LOOPS → BLAZE)

The four tracks are the set's layers, brought in and out on the beat.

| Keys | |
|---|---|
| White 1–4 | TRACK 1–4 in / out at the next QUANTIZE point |
| White 5–8 | FADE 1–4 in / out over the FADE bars from that point (a fade out ends muted) |
| White 9–12 | FILL 1–4: one bar the other way round (a layer out plays a bar; a layer in drops a bar) |
| White 13–16 | ALL IN, DRUMS (only the drums), BREAK (the drums out), RESTART (every sequence from its step 1) |
| Black 1–4 | QUANTIZE: NOW, the next BAR, the next 4 BARS, the next PHRASE |
| Black 5–7 | FADE over 2, 4, 8 bars |
| Black 8 | DUCK on / off: the other tracks dip under the FM-1's kick |
| Black 9 | RAMP: the FM-1's own tempo glides to RAMP TO over BARS (with Link, rekordbox sets the tempo) |
| Black 10 / 11 | CANCEL (every change waiting), THE ONE |
| KNOB 1–4 | DUCK amount, RELEASE, RAMP TO, BARS |

A change waiting is tinted on screen and its key breathes; press it again before its point to take it back.

### Blends (RAIL v5)

RAIL has a NEXT TRACK row: tap the incoming track's key while you blend. The FM-1 then plays only the notes both keys
share: KEYS skips the others, CHORD dims and refuses chords that would clash. "Blend done" in RAIL makes the next key
the key; Clear ends the blend. RAIL sends it on channel 16, CC 23 24 25 (CC 23 = 0: none).

### The lights by fours

In ROLL and BLAZE the keys that do something glow by fours: the 1st and 3rd four of the white keys (and of the black
keys) a low glow, the 2nd and 4th a little brighter. Lit: on. Breathing: a change waiting (LOOPS).

### The battery

On the battery the header shows an estimated percentage in 10 % steps (on USB: the plug). It is calibrated from the
stock three-bar thresholds; run it from full to shutdown once and note what it showed to calibrate it.

## 0.6 — key cells you can tell apart, SET

### Every page: white keys first, black keys after, each numbered

- White-key cells are light with dark text; black-key cells are black with light text. The coloured outline still
  says what kind of control it is.
- Each cell has its key's number on a tab at its left: white keys 1–16, black keys 1–11. A white key's tab is black
  with a white figure, a black key's white with a black figure.
- White keys always come first, four a row in order (1–4, 5–8, 9–12, 13–16), then the black keys, four a row (1–4,
  5–8, 9–11). No row mixes them.
- The key you are holding has its cell outlined bright, so a wrong press shows at once.
- Shorter names where a number tab needed the room: REPEAT (was STUTTER), SLOWER / FASTER (NUDGE − / +), ALL OFF
  (RELEASE), THE 1, FROM 1 (LOOPS' RESTART), PADS (ROLL; the mode is in the header), CRASH (a kit's cymbal).
- KEYS shows its 16 notes as 4 rows of 4; the key is in the header (with "+9A" in a blend).

### SET (RAIL v6)

RAIL has a SET panel: your planned songs, each with its key, BPM, kit and (optionally) which tracks play. Paste a list
("title, key, BPM" a line) or add songs by hand. On the FM-1, BLAZE's white 15 / 16 are PREV / NEXT: RAIL moves
through the list and sends the song: its key, the next song's key (so KEYS and CHORD keep to the notes both share
while you blend), BPM (on the FM-1's own clock; with Link rekordbox keeps the tempo), the kit, and the tracks in or
out at LOOPS' QUANTIZE point. RAIL's own Prev / Next buttons do the same. Channel 16: CC 26–30 to the FM-1, CC 31
from it (1 PREV, 2 NEXT).

## 0.7 — steady external tempo, KITS, KEY LOCK

### External clock: the BPM holds still

Felucca measured the incoming tempo over one 16th (six clock ticks) in whole milliseconds, so a millisecond of USB
jitter showed as 127 / 128 / 129 (up to ±3 with the computer busy). BLAZE measures over the last 4 beats instead and
moves the BPM shown only when it is 0.8 off. A real tempo change (two 16ths in a row more than 1/16 off) starts the
measurement over: a jump reads close in its second beat and exact from the third. The beat itself always follows
the clock pulses, as before. Pair it with RAIL BRIDGE v4.

### KITS (BLAZE, SHIFT + white 1–8)

Hold SHIFT (OCT+): white 1–8 become KIT 1–8. Tap one to load it onto the DRUM track (playing or stopped). Hold one
for a second to save the DRUM track's sound there (the kit, each drum's TUNE / DECAY / LEVEL, TONE, SNAP, DRIVE, ...)
— stopped only: the FM-1 can't write flash while playing. The KITs live in the user presets U25–U32, named
"KIT 1".."KIT 8", so PRESETS can load them too. A slot holding another sound is never written over. On screen: the
loaded KIT tinted, saved ones normal, empty ones dim.

### KEY LOCK (MIDI IN)

Notes coming in over MIDI (a keyboard on TRS or USB) are moved onto the key, the way QNT SNAP moves the FM-1's own
keys: down to the nearest note of the scale. It works once BLAZE has a key (from RAIL / SET) or the track's QNT is
on, on tracks with a scale (not CHR), never on DRUM or sliced sounds. On by default; KEYS + SHIFT + black 1 (LOCK)
turns it off and on (kept with the settings).

## 0.8 — SLICE

OCT- now steps BLAZE → KEYS → CHORD → LOOPS → **SLICE** → BLAZE. SLICE puts a sample's slices on the 16 white keys,
using Felucca's own SLICE engine: its BREAK, your own samples in USR1–USR3, or PIANO.

- **White 1–16:** slices S1..S16 (the screen shows which slice each key plays). START (knob 1) moves them along. With
  fewer slices than keys the extra keys wrap round and show dim. Slice 1 is tinted.
- **Black 1–4 T1..T4:** which track SLICE plays. **SHIFT + T1..T4** turns that track into a SLICE track (UNDO brings
  its old sound back). With no SLICE track the white keys stay silent.
- **Black 5 MODE:** ONE (plays the slice to its end), GATE (while held), LOOP (repeats while held).
- **Black 6 REV:** slices backwards.
- **Black 7 SLICES:** 4 / 8 / 16 / 32 equal slices, AUTO (one per hit Felucca finds), MAN (your own, set on
  Felucca's SLICES page).
- **Black 8 SAMPLE:** BREAK, USR1, USR2, USR3, PIANO. An empty slot says so.
- **Black 9 17–32:** the second page when there are more than 16 slices.
- **Black 10 QUANTIZE:** OFF, 1/16, 1/8, BEAT. A press waits for the next point of that grid (counted from the
  phrase's 1), and so does its release. Stopped: at once.
- **Black 11 CHOP:** the track's SLICER insert, OFF → GATE → STUTTER, in time with the tempo.
- **Knobs:** START, PITCH (±24 semitones), DECAY, TONE.

Loading your own sample (Mac, FM-1 on USB, BLAZE installed):
`python3.12 tools/fm1_sample_upload.py load 1 VOX vocal.wav` puts `vocal.wav` in USR1 (about 7 s max per slot, any
WAV; it is converted to mono 22 kHz). Then SAMPLE → USR1 and SLICES → AUTO.
