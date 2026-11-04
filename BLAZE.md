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
