# Stock Gen4+/Gen5 Panel PCB — reference (measurements + chip inventory)

Everything known about the stock Step Revolution panel PCB (**ASSY M146 040
K-r2**, pulled 2026-07-10 from the user's Gen5 pad). Merged 2026-07-23 from
`STOCK_PANEL_PCB_MEASUREMENTS.md` and `STOCK_PANEL_CHIPS.md`.

Two measurement sources, kept side by side: an earlier **photo estimate**
(Step Revolution shop photo, `PCB-Gen4_1024x1024.jpg`, scaled by the known
127mm dimension → 4.65 px/mm, ±1–1.5mm accuracy) and **physical caliper
measurements** (2026-07-10 onward, authoritative where the two disagree).
Chip markings from `images/font_smx_pcb.png` / `images/back_smx_pcb.png` plus
loupe reads by the user (2026-07-11). Coordinates: mm from the board's
top-left corner, +X right, +Y down, unless noted otherwise.

## Outline

- Photo estimate: 127 × 127mm square (5×5").
- **Physical: not a perfect square** — edges measured 128 / 127 / 128 / 127mm
  going around. Corner notches have angled sides that are hard to caliper
  precisely. Four small tabs jut out for connectors; physical board confirms
  tabs exist but exact protrusion not re-measured:

| Tab | Location (Y range) | Protrusion | Stock use |
|-----|-------------------|-----------|-----------|
| Upper-left | ~16–37mm | ~3mm | 4-pin power (+5V GND GND +5V) |
| Upper-right | ~16–37mm | ~4mm | Same, mirrored (power daisy-chain) |
| Lower-left (stepped) | ~76–118mm | up to ~11mm | RJ-style data IN + blue 2-pos screw terminal (SIGNAL/GND) |
| Lower-right | ~80–103mm | up to ~12.5mm | RJ-style data OUT |

Implication for our board: the cavity tolerates up to ~12mm of overhang on the
left/right edges in those zones — usable room if 127mm ever proves tight, but
tabs are optional (stock uses them for bulky jacks we don't have).

## Mounting holes

- **Hole diameter 4.5mm.** Center-to-center ~114mm — except the **top pair
  (top-left to top-right), which is 113mm** (refined 2026-07-12, calipers
  re-measured twice + pixel measurement on `images/back_smx_pcb.png`
  cross-check; user visually confirmed it's the two top holes that sit
  inboard). Our panel layout matches: top holes at X=57/170, bottom at
  X=56.5/170.5.
- Inset (hole center to board edge) is 6mm on two edges, 7mm on the other two —
  the which-edges question was mooted 2026-07-17 when our board's hole
  positions were confirmed by a 1:1 printout fit-tested against the physical
  standoffs.
- **Standoffs are plastic poles with retention tabs, not metal screws** — no
  grounded-screw consideration needed.

## Standoff / mounting height

PCB bottom surface sits ~6mm above the floor (standoff height). Floor to the
bottom of the pad's outer panel/platform is ~37.5mm total — budget **35mm** as
the usable design height. PCB thickness 1.6mm.

## Edge connector positions (physical)

Measured from the top edge unless noted:

- Power IN: top-left, ~25mm. Power OUT: top-right, ~25mm (mirrors IN).
- INT: left edge, ~93mm. Data IN: left edge, ~103mm. Data OUT: right edge, ~95mm.

## FSR positions (physical)

Each FSR sits inset **0–1mm from the edge** — pushed as close to the panel
edge as physically possible, roughly (not precision-) centered along its edge.

## LED lattice (25 LEDs, staggered 4–3–4–3–4–3–4)

Regular staggered lattice, centered on the board; wiring order per the
serpentine map in `docs/ANIMATIONS.md`.

Final refined values (2026-07-12, calipers — these are what the panel PCB
uses): **column pitch 33.5mm** (resolved by reconciling three independent
readings that only sum exactly to the 127mm board width at 33.5mm), **row
pitch 17mm**, insets **left 13.5mm / right 13.0mm** (0.5mm asymmetry, mirrors
the mounting-hole asymmetry), **top/bottom 12.5mm symmetric**. Earlier
readings (34mm pitch, 11mm top offset, photo-estimate 16.6mm row pitch) are
superseded.

## Chip inventory

| Ref(s) | Package | Marking (loupe-read) | Identification | Role |
|--------|---------|---------------------|----------------|------|
| U1 | TQFP-32 | `MEGA 328P U-TH` | **ATmega328P-AU** (Microchip AVR, same MCU as Arduino Uno) | Panel MCU. 16MHz crystal at Y1 |
| U6, U7, U8, U11, U12 | TSSOP-28 ×5 | `TLC5940` | **TI TLC5940** 16-ch constant-current LED sink driver, 12-bit PWM | LED drive: 5 × 16 = 80 channels for 25 RGB LEDs (75 used, 5 spare) |
| ×4 (one per sensor position, SOIC-8 next to each FSR/AMP jumper) | SOIC-8 | `B32` / `04H3` | **Unidentified** — topmark not decoded (checked SMD marking databases 2026-07-11) | Sensor analog front-end for the AMP (load-cell) path; bypassed/reconfigured by the per-sensor `AMP`/`FSR` jumper |
| U10 | small SMD, only one on board | `7A` / TI logo / `M5P` | **Unidentified** TI part | Unknown; south edge, bottom-center-right |

Also on board: 25× 4-pad RGB LEDs (D1–D25, dumb common-anode driven by the
TLC5940s — **not addressable**), 4-pos DIP switch (SW1, panel ID), 6-pin
right-angle header (J1 — standard AVR ISP programming interface), RJ-style
modular jacks for data IN/OUT (J3/J2), blue 2-pos screw terminal
(SIGNAL/GND), 2× 4-pin power connectors (J9/J10, silkscreened
**+5V GND GND +5V**), per-sensor `AMP`/`FSR` selection jumpers.

A **sacrificial stock panel PCB is inbound** (friend sending one, noted
2026-07-11) — parts can be desoldered/measured destructively if any exact
value is ever needed. To resolve the two unidentified topmarks: TI's
part-marking lookup for `M5P`; generic SMD topmark databases for `B32 04H3`.
Neither blocks anything.

## Insights for our design

1. **Stock LEDs are not addressable.** Plain RGB LEDs on an 80-channel
   constant-current matrix (5× TLC5940 daisy-chained shift registers). Our
   WS2815 chain replaces 5 driver ICs + 75 routed channels with one shifter
   gate and a single data line. The 80-channel shift chain also plausibly
   explains the 81-byte clock burst in the stock sensor-readback protocol
   (an 80-bit clocked cycle — see `INTERNAL_BUS_PROTOCOL.md` in
   stepmaniax-sdk-mp).

2. **Stock panels run entirely on 5V** (power daisy-chain silkscreen reads
   +5V/GND/GND/+5V; the stock MCU box's 12V→5V DC/DC converts *before*
   distribution). Our 12V-to-panel + WS2815 + local LDO design is a real
   departure — don't transfer stock wiring-gauge assumptions, and the stock
   4-pin power connector is not a precedent for our 2-pin Micro-Fit at 12V.

3. **Load-cell support is what the 4 mystery SOIC-8s buy.** One amp chip +
   jumper per sensor position. We dropped load cells, so our panel deletes all
   four plus jumpers — FSR dividers need no active front-end.

4. **Stock MCU is an 8-bit AVR at 16MHz.** The ~6ms lag window and 30Hz LED
   ceiling weren't just protocol choices — the platform had little headroom.
   No stock firmware behavior should be treated as a performance ceiling.

5. **Brightness parity — RESOLVED 2026-07-11: not a concern.** IREF resistors
   measure 2kΩ in-circuit → stock max channel current ≈ 19.5mA per die (≤20mA
   firm upper bound), ~1.6× our WS2815's fixed ~12mA — perceived difference
   sub-linear, likely ~20–30%. **Verdict (user): stock is widely considered
   TOO bright** — many players (including the user) apply tint film. A ~20–30%
   dimmer max is acceptable, likely preferable; software global-brightness
   scaling covers further adjustment. Optional curiosity, not a gate: a
   side-by-side under real acrylic. If it ever surprised badly, mitigation is
   a different 12V one-wire addressable part — the architecture doesn't
   reopen.

6. **Precedents that validate our choices:** screw terminal for the per-panel
   signal wire (ours: INT), 4-position DIP for panel ID, single-ended UART bus
   with no differential transceiver anywhere (our RS-485 at 1 Mbps is the
   upgrade path stock never had). The blue SIGNAL/GND screw terminal is
   stock's interrupt/signal line — amusing validation of our screw-terminal
   INT plan.

## Open items (pending full teardown)

1. **PSU stud size** — for sizing the fork/spade lugs at the PSU end of the
   12V column runs.
2. **Underglow data connector type/pinout at the old MCU** — gates the master
   PCB's underglow DATA-out connector choice (see `docs/UNDERGLOW.md`).
   Related: stock leads crimp directly into a 12-pin Dupont housing at the
   stock MCU, no reusable intermediate connector.
3. **Harness run lengths** — power hops, RS-485 links, and the 9 INT
   home-runs — real measurements to replace the estimates in `docs/BOM.md`.
   User's plan: snake a wire through the pad along the real routing.

## Other stock observations (photo estimate, not re-verified physically)

MCU + crystal right-of-center, DIP switch bottom-right, 4 "FSR AMP" labeled
zones (one per edge), bottom-center connector. Stock power is +5V on this
board family — ours is 12V, no conflict.

## INT wire color map

**Confirmed correct 2026-07-10** against the physical pad — matches the stock
SMX color map adopted in `docs/BOM.md`: 0=Red, 1=Orange, 2=Yellow, 3=Green,
4=Blue, 5=Brown, 6=Grey, 7=White, 8=Black.
