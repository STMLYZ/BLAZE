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

## 0.9 — tapped rates, PAD PRESETS, REKORDBOX

### Rates stay on when tapped (BLAZE and ROLL)

A rate key **tapped** (let go within a quarter second) stays on, tinted on screen: any pad you press then rolls at it,
with no rate key held. Tap it again: off. **Held**, a rate works as before (rolls while held, gone when let go). ALL OFF
turns every tapped rate off. Several tapped rates combine as held ones do (GROOVE / LAST).

### PAD PRESETS (BLAZE's pads)

Hold a pad, then hold SHIFT (OCT+), then tap:
- a **rate key**: that drum rolls at that rate whenever its pad is held, whatever rates are on (the label shows it:
  `SD 16`);
- an **effect key** (ECHO .. REPEAT): that effect comes on while the pad is held (`+` on the label). Effects work on the
  whole FM-1 output, not one drum.

The same key again removes it. While a pad and SHIFT are held the screen shows that pad's preset (its rate and effect
filled). Presets belong to the drum (lane), are kept with the settings, and play from BLAZE's pads. SHIFT on its own
still gives KIT 1–8 on white 1–8.

### REKORDBOX (OCT- after SLICE)

The FM-1 becomes a second controller: every key sends a MIDI note (127 on press, note-off on release) and plays nothing.
Map them in rekordbox: Preferences → MIDI (or the MIDI button) → device **Felucca** → MIDI LEARN.

| FM-1 | Sends | Suggested rekordbox function |
|---|---|---|
| White 1–8 | notes 36–43, channel 14 (deck 1) / 15 (deck 2) | Hot Cue A–H |
| White 9–12 | notes 44–47, ch 14 / 15 | Beat Loop 1 / 2 / 4 / 8 |
| White 13 | note 48, ch 14 / 15 | Reloop / Exit |
| White 14 / 15 | notes 49 / 50, ch 14 / 15 | Beat Jump − / + |
| White 16 | note 51, ch 14 / 15 | Sync |
| Black 1–8 | notes 60–67, channel 13 | Sampler slot 1–8 |
| Black 9 / 10 | notes 68 / 69, ch 13 | Beat FX on / off, Beat FX select |
| Black 11 | — (on the FM-1) | DECK 1 / 2: which channel the white keys use |
| KNOB 1–4 | CC 1–4, channel 13, 0–127 | Beat FX depth, Sampler volume, Beat FX beat, a filter |

Learn the white keys twice: once with DECK 1 (channel 14) to the deck 1 functions, once with DECK 2 (channel 15) to deck
2's. The black keys and knobs are the same on both decks. The screen names each key as in the table; pressing one shows
its note and channel.

## 0.10 — room to grow, SCATTER, BACKSPIN, MORPH

### Stripped and hidden

The FM-1 build leaves out the built-in PIANO, FLUTE and SAX samples (about 104 KB) and the PHYS engine (marimba,
kalimba, hand drum, harp ...): the firmware has room for BLAZE's next pages again. SLICE's BREAK stays; SLICE's PIANO
(the PIANO sample) is gone, so SAMPLE skips it. A project or user preset that used one of them loads, with that
sound silent. Your own samples (USR1–3, `fm1_sample_upload.py`) play in SAMPLE, SLICE and GRAIN as before.

PRESETS browsing no longer shows TINE EP, E.PIANO, FUNK KEY, MARIMBA, ORGAN, BRASS or PERC KIT (still in the
firmware; MIDI program change numbers are unchanged).

(Builds: `tools/build.py` makes `build/gen_fm1/felucca_samples.h` without the CC0 sets for the FM-1 and sets
FELUCCA_PHYS=0; the host and the tests keep everything.)

### SHIFT + white 9–16 (BLAZE, no pad held)

| SHIFT + | Does |
|---|---|
| White 9 SCATR | **SCATTER**: tap on / off, hold while held. Each 1/16 is the live mix or one of the last two 1/16s again: forwards, backwards, or as two 1/32s. KNOB 4 (WILD while it runs) sets how often: full, 9 in 10 scattered. Starts on the 1/16 |
| White 10 SPIN | **BACKSPIN**: records from the press, spins backwards to a stop on the next beat (at least a quarter beat later), silent until the next bar's 1, then back. Again: cancel |
| White 13 / 14 SET A / SET B | Held 0.7 s: save MORPH's A / B (COLOR, THROW, DEPTH; each track's FILTER, LEVEL, REVERB, DELAY); 0.11: kept after power-off |
| White 15 MORPH | MORPH on / off (A and B saved first): KNOB 2 (the MORPH card) slides everything from A to B |
| White 11, 12, 16 | LOOP CAPTURE (0.11, below) |

SCATTER and BACKSPIN are in Felucca's FX layer too (assignable keys: SCAT, SPIN; there BACKSPIN spins after half a
beat, silent until let go).

## 0.11 — LOOP CAPTURE, DROP, MORPH RIDE, ROLL SHAPE, PHOSPHOR events

### LOOP CAPTURE (BLAZE: SHIFT + white 11, 12, 16)

| SHIFT + | Does |
|---|---|
| White 11 CAPT | From the next bar (stopped: now) records LENGTH bars of the whole mix (sends included). At the end the loop plays **in place of the tracks** (SOLO: change the pattern underneath, nobody hears it). Tap: from the next bar the tracks come back **over** the loop (MIX: a second deck). Tap: from the next bar the loop glides out (tap twice: out now). While recording, a tap drops it |
| White 12 | LENGTH: 4 → 1 → 2 bars (kept). 4 bars need 81 BPM or faster; slower records 2 |
| White 16 SAVE | Stopped, with a loop held: writes it into a USR slot (the first empty one; all full: one a capture filled before; your uploads are never written over), named e.g. `CAP4B132`. SAMPLE and SLICE play it from there. The sound pauses about a second while the flash is written. A slot holds 7.4 s: 4 bars from 130 BPM, else 2 |

The cell shows the state: ARMED, REC, SOLO, MIX (tinted: a change waits for the bar), OUT. RELEASE (black 10)
takes the loop out at once. COLOR, THROW, the FX keys and DROP all work on the loop.

**The cost:** the loop lives in the echo's memory (128 KB, as 22 kHz mono ADPCM). From the moment recording starts
until the loop is gone there is **no echo**: ECHO, EXIT's tail and the tracks' DELAY are silent. When it's gone the
echo comes back from silence. The loop is mono and a little lo-fi (22 kHz, 4-bit).

PLAY while a loop is held starts it from its 1 with the transport.

### SHIFT + black 1–4 (BLAZE, no pad held)

| SHIFT + | Does |
|---|---|
| Black 1 RIDE 8 | MORPH glides from where it is to B (or, past halfway, back to A) over 8 bars, starting on the next bar. Stopped: in real time at the tempo. Turning KNOB 2 takes over. Again: stop |
| Black 2 RIDE16 | The same over 16 bars |
| Black 3 DROP | Playing: from the next bar to the phrase's next 1 (fewer than 2 bars left: the 1 after): COLOR sweeps up into the high-pass (slow, then fast, to +70), the RISER (its TYPE) runs over exactly those bars, a BACKSPIN on the last beat, and on the 1 everything lets go: COLOR back where it was. Again: cancel |
| Black 4 SHAPE | ROLL SHAPE (kept), for every roll: FLAT, ACC 2 (every other hit at half), ACC 4 (one in four full), FADE (down to 30 % over a bar) |

### PHOSPHOR events (MIDI out, channel 16)

For visuals: BLAZE sends what it's doing as CC on channel 16 (USB and DIN), only when a value changes.

| CC | Value |
|---|---|
| 40 | The phrase's bar, 1–16 (0 stopped) |
| 41 | DROP: 32 armed, 64 building, 127 on its 1 (for a beat), 0 |
| 42 | RISER: 1–127 as it rises, 0 off |
| 43 | BACKSPIN: 64 armed, 127 spinning, 0 |
| 44 | MORPH: 0 (A) – 127 (B) |
| 45 | LOOP CAPTURE: 32 armed, 64 recording, 96 solo / fading, 127 mix, 0 off |
| 46 | SCATTER: 127 on, 0 off |
| 47 | BUILD: 1–127 as it goes, 0 off |

PHOSPHOR (v5.2) can MIDI-learn any of them today with the FM-1 as its MIDI input; so can any visuals app.


## 0.11.1 — SHIFT LOCK

On BLAZE's own page, **double-tap OCT+** (two quick taps) and SHIFT stays on without holding it: the header reads
SHIFT LOCK and the footer OCT+ OFF. Every SHIFT key is one press: SCATTER, BACKSPIN, CAPTURE, DROP, RIDE, MORPH, the
KITS, and the knobs are TONE SNAP DRIVE THROW. **One tap of OCT+** unlocks; so does going to KEYS (OCT−) or leaving
BLAZE. While locked, white 1–8 are KIT 1–8 and black 1–4 RIDE / DROP / SHAPE, so the pads and the first four rates
are not there: unlock to roll. A tap is a press under 0.3 s with no key pressed meanwhile; the second must come within
0.4 s of the first.

### RAIL's bar (RAIL v7)

RAIL v7 can correct the phrase counter: click the bar you're actually on, or `[` / `]` for one bar back / on, or P
(this is a 1). It sends channel 16 **CC 32**: 1–32 = the bar playing now is that bar (the beat inside it kept; a
32-bar phrase's 17–32 land on 1–16), 64 = the nearest bar line is the 1. BLAZE's counter, and everything that counts
from it (BUILD, RISER, DROP, LOOPS, SLICE's QUANTIZE), moves with it. RAIL v7's tempo buttons set the Link tempo
through RAIL BRIDGE v5; the FM-1 follows the clock as before.

## 0.12 — a DJ first page, hold-to-set BUILD and DROP, FILL, FOLLOW, the chord generator

BLAZE's own page is reshuffled so the live tools need no SHIFT. Everything that moved is listed under each page.

### BLAZE (no SHIFT)

| Keys | What |
|---|---|
| white 1–4 | the 4 pads (hold one + **ALGORITHM**: another drum, see below) |
| white 5–8 | ECHO, REVERB, BRAKE, REPEAT (tap latches, hold plays while held) |
| white 9–12 | BUILD 4, BUILD 8, RISER, DROP — they **start when let go**; **hold one + the knobs** sets it up instead |
| white 13–16 | CAPTURE, EXIT, SCATTER, BACKSPIN |
| black 1–6 | rates 1/4, 1/8, 1/8T, 1/16, 1/16T, 1/32 |
| black 7 | RIDE (MORPH A → B over 8 bars; double-tap: 16) |
| black 8 | MORPH on / off |
| black 9 | THE 1 |
| black 10 | ALL OFF |
| black 11 | **FILL** (new) |
| knobs | COLOR, PITCH (MORPH while it's on), DEPTH (WILD while SCATTER is on), THROW |

### BLAZE + SHIFT (hold OCT+, or double-tap it to lock)

| Keys | What |
|---|---|
| white 1–8 | KIT 1–8 (hold to save, tap to load) |
| white 9, 10 | REVERSE, FREEZE |
| white 11, 12 | capture LENGTH, capture SAVE |
| white 13, 14 | MORPH A, B (hold to store) |
| white 15, 16 | SET previous / next song (RAIL) |
| black 1–3 | TAP, SLOWER, FASTER |
| black 4 | ROLL SHAPE |
| black 5 | **FOLLOW** (new) |
| knobs | PITCH, DECAY, TONE, DRIVE |

Moved from 0.11.1: REVERSE and FREEZE to SHIFT; SET to SHIFT; TAP and the nudges to SHIFT; THE 1 to black 9; DROP to
white 12 and no SHIFT; SCATTER, BACKSPIN and CAPTURE without SHIFT; RIDE and MORPH to black 7 / 8; DEPTH and THROW are
main knobs; SNAP is EDIT's HIT page only.

### BUILD and DROP set up by holding them

Hold **BUILD 4** or **BUILD 8** and turn: KNOB 1 **DRUM** (any lane of the kit, SNARE by default), KNOB 2 **SPEED**
(1/4 > 1/32, 1/8 > 1/32, straight 1/16, 1/16 > 1/32), KNOB 3 **SWELL** (how much quieter it starts, 0–100 %),
KNOB 4 **WITH** (nothing, FILTER — COLOR climbs into the high pass over the build and comes back after, ECHO — the last
beat into the echo, RISER — the riser over the same bars). Hold **DROP** and turn: KNOB 1 **FILTER** (how far COLOR
climbs, 0 = it stays), KNOB 2 **LENGTH** (to the phrase's 1, or 4 / 8 / 16 bars from the next bar), KNOB 3 **WITH**
(BACKSPIN, BACKSPIN + BUILD, BUILD, nothing), KNOB 4 **HIT** on the 1 (nothing, an ECHO throw, a REVERB throw, the
kit's crash). Let go after turning: nothing starts. All kept with the FM-1's settings. RISER and EXIT work the same way
(hold + knobs, let go: nothing starts).

### FILL (black 11)

A snare roll from now to the next bar's 1 on BUILD's drum: 1/8, then 1/16, then 1/32, louder as it goes. Under two
beats from the bar line it goes to the bar after. Stopped: one bar. Again while it runs: stopped.

### Pad swap

Hold a pad (white 1–4) and turn **ALGORITHM**: that pad takes another drum — SNARE, CLAP, closed HAT, OPEN hat, TOM,
RIM, BELL, KICK last — never one another pad already has. Kept with the settings. ALGORITHM with no pad held still
steps the KIT.

### FOLLOW (SHIFT + black 5, or RAIL)

Off: the FM-1 stops following rekordbox's Link clock and keeps playing, at the tempo it had, on its own clock — so
rekordbox can go back to the track's own tempo without dragging the FM-1. On: it follows the clock again (RAIL's
"Start on next 1" lines the bars up if they drifted). The FM-1 tells RAIL (CC 33). While following, the **SELECT knob**
nudges the Link tempo through RAIL: 0.1 a detent, faster turns more — rekordbox and the FM-1 both follow.

### KEYS

The white keys light by what they are: the home note full, the 3rd and the 5th up from it bright, the rest of the
scale low. Every white key is in key. **PRESETS** changes the KEYS track's sound and shows its name.

### CHORD: the generator (white 9–16)

White 8 is OFF as before. **9 AUTO**: the chords play themselves, a progression in the key (minor or major by the
track's scale). **10 NEW**: another progression. **11 / 12 / 13**: a chord every 1, 2 or 4 bars. **14 MELODY**: a line
on the KEYS track over the chords, 2 bars long, mostly the chord's notes, a new one every 2 bars — **hold 14 + KNOB 1
DENSITY, KNOB 2 RANGE** (1 or 2 octaves). **15 NEW**: a new line now. **16 KEEP**: the line repeats. A chord key
(1–7) turns AUTO off. Playing, it follows the phrase's bars; stopped, it runs on its own time.

### RAIL v8 (channel 16)

RAIL → FM-1: **CC 33** FOLLOW (127 on, 1 off); **CC 36** an action (1 BUILD 4, 2 BUILD 8, 3 RISER, 4 DROP, 5 EXIT,
6 BACKSPIN, 7 CAPTURE, 8 ALL OFF, 9 FILL, 10 SCATTER, 11 SHIFT LOCK, 12 BOOST); **CC 50–53** COLOR, MORPH, DEPTH, THROW (absolute;
a knob's first value is only noted, so nothing jumps). FM-1 → RAIL: **CC 33** FOLLOW's state, **CC 34** the SELECT
nudge (64 ± tenths of a BPM). RAIL v8 learns MiniLab pads for the actions and knobs for tempo (fine, coarse) and the
four knobs, and has FOLLOW and **Song BPM** (the Link tempo to the SET song's BPM).

## 0.12.1

**PRESETS on the CHORD page** changes the CHORD track's sound and shows its name, as it does on KEYS. The generator's
MELO line plays on the KEYS track: change its sound on the KEYS page.

## 0.13 — hands-free snare rolls, the RATE knob

**Hands-free roll.** Hold a pad and pick a rate (tap a rate key, or turn KNOB 4): the pad keeps rolling when you let go.
Pick another rate while it rolls (a rate key, or KNOB 4) and it switches, on the beat grid. Tap the pad again: it
stops. ALL OFF stops everything. A rate tapped *before* a pad still works as in 0.9: the pad then rolls only while held.

**RATE knob.** KNOB 4 is RATE while a pad is held or rolling on its own (THROW the rest of the time). It steps through
**EVEN** 1/8, 1/16, 1/32 by default. Hold a pad + SHIFT + KNOB 4 to pick the steps: EVEN, ALL (1/4 1/8 1/8T 1/16 1/16T
1/32) or TRIP (1/8T 1/16T). Kept with the settings.

**RAIL v9.** A learnable ROLL RATE knob (CC 54 on channel 16, across the same steps; its first value only noted).

**PHOSPHOR events.** CC 48: the fastest rate rolling (1 a bar .. 7 1/32, 0 none). CC 49: every roll hit, its velocity.

## 0.14 — the new look, READY ROLLS, keeping a capture

**Font:** Barlow Semi Condensed everywhere (narrower than Inter Tight at the same height: CAPTURE and SCATTER fit in
full). MENU > LARGE stays for bigger values.

**Palettes:** MENU > COLOR lists CHROME (the default), NEON, ACID, SUNSET, ORANGE, then Felucca's PAPER, MONO, SUN and
HI-CON. The others (GREY GREEN AMBER ICE VIOLET ROSE NIGHT BOOTH CAMERA) are no longer listed; a saved one shows as
CHROME. The five new ones light BLAZE's cells with their own fill (CHROME chrome, ACID chrome, ORANGE gloss, SUNSET
gold to magenta, NEON flat) and their own colours for the effects and the transitions; the Felucca ones stay flat.

**Key cells (every BLAZE page and ROLL):**

| State | Looks |
|---|---|
| Ready | dark, a line in its group's colour under the name |
| On (latched) | lit, a dot in the corner (an effect, a rate, a pad rolling, MORPH) |
| Running | lit, a bar along the foot filling to its end (BUILD, FILL, RISER, DROP) |
| Waiting | outlined, the outline breathing on the beat, a badge with the bars to go |
| Held | a dashed edge |
| Not now | hatched |

BUILD, RISER, DROP and SPIN carry an icon. The phrase strip marks the bar the DROP lands on, and the header says
**DROP IN n**.

**READY ROLLS:** on the ROLL page (hold ARP), **OCT−** flips to READY ROLLS: 16 keys, each a finished roll (a drum at its
own rate): SNARE 1/8 1/16 1/32 1/16T, CLAP the same, HAT 1/16 1/32 1/16T, OPEN 1/8, KICK 1/8 1/16, RIM 1/16, TOM 1/16T.
Tap one: it rolls on (a dot) until tapped again; hold: only while held. A key of the same drum takes over (SNARE 1/8 ->
SNARE 1/16, nothing to switch). Hold a key + KNOB 1 DRUM, KNOB 2 RATE, KNOB 3 ACCENT: kept. ALL OFF stops them. RAIL v10
plays keys 1..8 from MiniLab pads (channel 16 CC 37: n down, n + 64 up).

**Keeping a capture:** CAPTURE also on the CHORD and SLICE pages: **SHIFT + white 13**. When the FM-1 stops with a loop
held it asks **SAVE THE LOOP?** (OCT+ yes, OCT- no); saved (or SHIFT + white 12 on BLAZE's page), it asks **LOAD AS SLICE
PATTERN?**: yes makes a track (the SLICE track, else the selected one, else T3) SLICE of that USR slot with a pattern
playing the slices in order (16 a bar; 32 for 4 bars), a pattern for a SONG; no asks **LOAD INTO SAMPLER?**: yes puts the
loop on the keys (SAMPLE). UNDO brings the track back.

## 0.14.1 — BOOST

**SHIFT + black 6** (or RAIL's BOOST, CC 36 12) toggles **BOOST**: SNARE and CLAP — every hit, rolls, sequencer, pads —
are driven at DRIVE's top into a higher ceiling (x2.5 before the knee, not x1.25), and ROLL / READY ROLLS hits on those
lanes play at velocity 127. Measured at LEVEL 104: SNARE body −12.8 → −8.0 dB, CLAP −17.5 → −10.9 dB; KICK and the hats
as they were. The master limiter still holds the peaks, so it is louder body, not clipping. The cell shows lit with a lock
dot while on. Not saved: it is off at power-up.


## 0.15 — SET, GO, the sound pack, the decks

**SET** (MENU › HOME › SET, or GO + white 8): HOME's glance view for a set. The tempo big, the key in Camelot (its
colour) with its note name, the CPU load, the phrase (16 bars, the DROP's bar outlined), what runs now as chips (DROP IN
n, BOOST, FOLLOW, latched effects, BUILD, READY, SHIFT), and both rekordbox decks from rail-bridge v6: a waveform
scrolling round each playhead (beats ticked, bars taller, memory cues marked, ones named **DROP** in the transitions'
colour), the whole track under it, the title, the key, the BPM as played and the time left (red under 30 s). The deck
on air (the bridge's guess from the faders) has its letter lit. The four cards above stay HOME's.

**GO** (HOME held): a map of every place on the keys; a key goes there at once. White: BLAZE ROLL KEYS CHORD / LOOPS
SLICE RB MIDI SET / PRESETS EDIT FILTER ENV / LFO FX ARP MOD. Black: STEP SONG AUTO MIXER / PROJECT USER CLOCK SCALE /
INSERT TOOLS MENU. Let go of HOME without a key: the MENU (as holding HOME always did). A tap of HOME: HOME, as before.
The keys pressed with HOME sound nothing.

**CPU**: a meter left of the battery on every page (orange from 70 %, red from 85 %), its number too when it is high
and the slot is free; SET shows it in full. It is the audio interrupt's share of its time (MENU › INFO's reading).

**The sound pack** (29 sounds, appended after each engine's own, so every stored sound number plays what it did):
ANALOG REESE, ACID SQR, ACID SLIDE, OFFBEAT, ROLLER, DARK SUB (basses), SUPERSAW (UNISON), HOOVER, RAVE STAB, HOUSE STAB,
TRANCE PLK, COLD STRNG, DARK PAD, SIREN, DOWNLIFTER; PHASE COLD BASS, POST LEAD; LOFI LASER; VOICE ROBOT VOX, DARK
CHOIR; TRIO SCREECH, FIDGET, ELECTRO; WHEEL ORGAN STAB; NOISE RISER; DRUM HARD KIT, TRANCE KIT, BLOG KIT, HOUSE KIT.
Level-matched against the factory sounds by rendering each through the firmware's own engines. A new category, **STAB**,
browses after PLUCK (PRESETS › LIST). Felucca-Engines' six engines (a submodule BLAZE can't change) play as their own
engine with BLAZE's rows after theirs (blaze_sounds.c, checked field for field by tests/blaze_sounds_test.c). BZ_PX
gives a preset what a row cannot: UNISON, glide, the LFO, the INSERT.

**rail-bridge v6** sends the decks as SysEx `F0 7D 42 5A <cmd> … F7` (no reply): INFO, OVERVIEW, CUES, WINDOW, POS,
FOCUS (blaze_set.c has the layout). It reads rekordbox's analysis files (ANLZ) for the waveforms, beat grid and cues, a
rekordbox XML export (if there is one) for names, keys and cue names, and follows the FLX4's PLAY, CUE, hot cues, LOAD,
tempo faders and SYNC for where each deck is. Which track is on which deck: RAIL v12's DECKS panel, or `--auto`
(pyrekordbox, rekordbox's history). The position is an estimate: loops, jog nudges and scratching drift it; a SYNCed
deck is pulled onto the Link beat.

## 0.16 — DECKS lead the key and the phrase; SNAP

**DECKS** (on by default; SHIFT + black 7, or RAIL's DECKS, CC 36 13): with rail-bridge running, the FM-1 follows
rekordbox's decks.
- **The key**: the deck alone on air (the bridge's guess from the channel faders and the crossfader) is the FM-1's key,
  set on every notes track as RAIL's key sets it. In a blend (both on air) the key stays the one it was and the other
  deck's becomes the next key: BLEND, the notes both keys share. **KEY NOW** (RAIL CC 36 14) takes the other deck's key
  at once.
- **The phrase**: the newest deck playing on air (the incoming one in a blend) sets BLAZE's 16-bar phrase from its beat
  grid: bar 1 is its DROP cue (a memory cue named DROP), else its first downbeat. So BUILD / DROP land on the track's
  drop, LOOPS and the chords count the track's bars. It needs the FM-1 on rekordbox's clock (FOLLOW); it moves the
  phrase by whole beats only, after seeing the same shift twice, never while a BUILD / DROP runs.
- SET shows each deck's **bar.beat** (orange, before the title: compare with rekordbox's), the lead deck's **DROP n**
  (bars to its DROP cue) and **DECKS** while following.

rail-bridge v6.1 + RAIL v13: **SNAP** (tap on a 1 you hear: the deck goes to the nearest bar line of its grid), **BAR −
/ BAR +** (a whole bar), the **DECK BARS** knob (a bar a detent), **DECK A/B** (which deck those act on; else the one
alone on air), all learnable to MiniLab pads / a knob. Turning DECKS off leaves the key and the phrase to you and RAIL.
