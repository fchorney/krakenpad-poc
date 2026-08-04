> **ARCHIVED 2026-08-04 — the question is settled: `dual-panel` won.** This is the
> evidence that decided it, kept as the record. `hardware/panel-pcb` has been
> removed from the tree (git history, last present at `1b41d1c`), so the
> "Open questions for a reviewer" section at the bottom is closed. The as-built
> design is `docs/DUAL_PANEL.md`.

# Single board vs two boards — measured comparison

Deciding between `hardware/panel-pcb` (one 127×127 board) and `hardware/dual-panel`
(carrier + socketed brain). Measured 2026-07-31.

**The two are the same circuit.** Identical schematic, identical BOM — 115
placements, 35 component lines, same parts, same quantities, same LCSC codes. The
only differences are *layout* and *the 32-pin interface*. That makes the comparison
unusually narrow: it isn't "which design is better", it's "does splitting it hurt".

## Cost: settled, and near-neutral

Both quoted at qty 5, ENIG, epoxy filled & capped, SMD-only assembly:

| | panel-pcb | dual-panel | delta |
|---|---|---|---|
| PCB | $78.05 | $89.01 | +$10.96 |
| PCBA | $199.96 | $199.67 | −$0.29 |
| Advanced | $8.70 | $8.70 | — |
| **Total** | **$286.71** | **$297.38** | **+$10.67** |

**$2.13 per panel**, all of it bare board area, and 70% of that extra area is rails
and dead space rather than the brain itself. Assembly is free. Full detail in
[`hardware/dual-panel/panel/QUOTE-2026-07-31.md`](../../hardware/dual-panel/panel/QUOTE-2026-07-31.md).

Cost is therefore **not** the deciding factor. Layout quality is.

## FSR routing — dual-panel measures better on every channel

The FSR lines are the highest-impedance nets on the board and the stated layout
priority ("FSRs as far from power/LED as possible").

| metric | panel-pcb | dual-panel |
|---|---|---|
| FSR_North | **401.3 mm** | 83.2 mm |
| FSR_East | 36.6 mm | 90.3 mm |
| FSR_South | 137.6 mm | 93.2 mm |
| FSR_West | 266.7 mm | 91.6 mm |
| spread across channels | **11× (36.6 → 401.3)** | **1.1× (83.2 → 93.2)** |
| worst same-layer clearance to LED data / 12V | **1.06 mm** | **3.18 mm** |
| FSR segments on F.Cu / B.Cu | 90 / 57 | 27 / 37 |
| cross-layer crossings under aggressors | 0 | 21 (B.Cu under F.Cu) |

**Different isolation strategies.** `panel-pcb` routes FSR predominantly on F.Cu —
the same layer as all 332 LED-chain segments and the 12V distribution — and relies
on 1–4 mm of lateral spacing. `dual-panel` routes them mostly on B.Cu and lets them
cross *under* the aggressors, separated by the In1 and In2 ground planes. Those 21
crossings are by design and benign; two ground planes is far better isolation than
3 mm of coplanar air.

**The asymmetry is the sharper finding.** An 11× spread means the four channels have
materially different pickup areas — East is a 36 mm stub, North is a 401 mm run
alongside a field of PWM-switched 12 V LEDs. They would have different noise floors,
so per-channel calibration would be doing real work rather than trimming.

FSR_North on `panel-pcb` is 68 segments spanning a 131 × 123 mm bounding box —
essentially the whole board — for a signal that only has to get from a board-edge
connector to a central MCU.

## Other critical nets — no meaningful difference

| net | panel-pcb | dual-panel |
|---|---|---|
| RS485+ / RS485− | 150.4 / 153.6 mm, 3.2 mm mismatch | 209.1 / 208.1 mm, 1.0 mm mismatch |
| INT_OUT | 85.1 mm, 2 vias | 99.1 mm, 0 vias |

`dual-panel`'s RS-485 is longer because it crosses the interface, but better matched.
At 1 Mbps over <3 m of cable neither figure matters; the mismatch is noise either way.
INT is comparable, and it is the sole press path, so worth noting nothing degraded.

## What this does and does not show

**Does not show `panel-pcb` would misbehave.** This measures geometry, not behaviour.
The FSR signal is sampled at 4 kHz through a 10 k‖10 nF filter with a 1.6 kHz corner,
and thresholded against per-channel calibration held in flash. A 401 mm trace adds
roughly 40 pF against a 10 nF cap — irrelevant for settling. The exposure is *noise
pickup*, which is a risk-profile difference, not a proven defect.

**Review status matters here.** `panel-pcb`'s **human review covered the schematic
only** — its *layout* has only ever been reviewed by AI. So the FSR asymmetry has not
had expert eyes on it, and an AI review is unlikely to have measured trace lengths.
It should not be read as "reviewed and found fine".

**The counterweight is not electrical.** Walking each crossing net against what
actually stresses a connector — FSR at kHz and 10 kΩ, INT as one open-drain edge,
RS-485 at 1 Mbps, LED data as one damped edge behind R16 — the interface is benign at
these speeds and impedances. The real risk of the split is **mechanical**: 32 spring
contacts in a device that is stomped on. That is what the M3 screws and the 11 mm
spacer are for, and it is the question no measurement here answers.

## Open questions for a reviewer

Frame it narrowly — these implement an identical schematic:

1. Does the 32-pin interface degrade anything, given the signal speeds above?
2. Is `panel-pcb`'s FSR routing (401 mm, 1.06 mm from the LED field, on the same
   layer) acceptable, or a real noise problem?
3. Is the mechanical reliability of a socketed brain acceptable in a stomped device?

## Method

Measured directly from the `.kicad_pcb` files with `pcbnew`: per-net track length by
layer, via counts, and segment-to-segment minimum distance between each FSR net and
every LED-chain / `LED_DATA*` / `+12VDC` net. Clearance is computed **layer-aware** —
same-layer minimum separately from cross-layer crossings, because a crossing with two
ground planes between is not a clearance problem and a naive 2D measurement reports it
misleadingly as 0.00 mm.
