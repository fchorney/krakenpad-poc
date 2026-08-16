# Wire Colors — stock SMX pad

Every conductor in the stock pad, by harness, as found at the 2026-08-08
teardown. Colors are load-bearing during reassembly — on the AC side getting
one wrong is a safety failure, not a bug.

**Colors are inconsistent BETWEEN harnesses and must not be carried across
one.** On the 5 V side green is GND and white is +5 V; on the mains side green
is earth and white is neutral; underglow DATA is also white. Pin *order* also
flips between the VL 4-way (`GND,5V,5V,GND`) and the Molex (`5V,GND,GND,5V`).
**Wire by color within a harness, never by pin number across one.**

Status column: **confirmed** = observed at the teardown. **TODO** = not
recorded; ask before assuming.

The two-letter codes are WireViz's, used in the `.yml` sources:
`BK` black, `WH` white, `RD` red, `GN` green, `YE` yellow, `BU` blue,
`BN` brown, `PK` pink, `GY` grey, `VT` violet, `OG` orange.
Bicolor is two codes concatenated — `GNYE` is green with a yellow stripe.

Our own harnesses' colors live in `hardware/harness/WIRE_COLORS.md`.

## AC mains input — `ac-input.yml`

**The 16 AWG cable and its branch pigtails use different color conventions.**
That is not a mistake in the recording; it is how the pad is built. Do not
assume a green wire on one side of a splice matches a green wire on the other.

| Conductor | Gauge | Color | Code | Status |
|-----------|-------|-------|------|--------|
| Main cable, L | 16 AWG | black | `BK` | confirmed |
| Main cable, N | 16 AWG | white | `WH` | confirmed |
| Main cable, GND | 16 AWG | green | `GN` | confirmed |
| Splice to YL 3-way, L | 18 AWG | **brown** | `BN` | confirmed |
| Splice to YL 3-way, N | 18 AWG | **blue** | `BU` | confirmed |
| Splice to YL 3-way, GND | 18 AWG | green | `GN` | confirmed |
| Splice to chassis ring, GND | 18 AWG | **green/yellow stripe** | `GNYE` | confirmed |
| Wall cord | — | — | — | n/a — ordinary PC power cord, purchased |

Two things to hold onto here:

- **The L and N pigtails switch to the IEC convention** (brown = live,
  blue = neutral) while the cable they splice into uses the North American one
  (black = live, white = neutral). Brown-to-black and blue-to-white are the
  correct pairings even though no color matches across the splice.
- **The two ground branches are deliberately distinguishable.** Plain green
  goes to the YL 3-way; green/yellow goes to the chassis ring terminal. If both
  were plain green there would be no way to tell them apart once the splice is
  covered.

**On the JST YL connectors specifically: trust the colors, not the pin numbers.**
Plug (`YLP`) and receptacle (`YLR`) are hard to tell apart by eye in this series,
and their housings are **mirror images** — so a misread flips pin 1 and pin 3
when you count contacts off the part in your hand.

The 3-way has now been called both ways twice. Recorded as a `YLP-03V`,
"corrected" to a `YLR-03V` (which reversed the numbering, putting brown/L on
pin 3), and **reverted to `YLP-03V` on 2026-08-08** when the gender audit found
that JST's *receptacle* is the male-pin half, not the female one — the reverse of
the intuitive reading that drove the first correction. **Brown/L is back on
pin 1.** Full rule in `PARTS.md`, "Connector gender conventions".

**That reverted reading was then verified against the part itself** in the
2026-08-08 pinout pass: pin 1 brown, pin 2 green, pin 3 blue. Settled.

Through all of that the color record never moved, which is exactly why it is the
primary record here. Note also that the mirroring means **pin 1 always mates
pin 1**, so the harness pinouts stayed mutually consistent the whole time — the
error was only ever a *sourcing* error, never a wiring one.

## 12V PSU output — `psu-12v.yml`

| Conductor | Gauge | Color | Code | Status |
|-----------|-------|-------|------|--------|
| PSU captive output, conductor A | 18 AWG | black, plain | `BK` | confirmed |
| PSU captive output, conductor B | 18 AWG | black, **with ridge** | `BK` | confirmed |
| Tail to YL 2-way, +12V | 20 AWG | red | `RD` | confirmed |
| Tail to YL 2-way, GND | 20 AWG | black | `BK` | confirmed |

**The PSU's captive cable is the one place in the pad where color does not
identify a conductor.** Both are black; one carries a moulded ridge, and that
ridge is the only distinguishing mark. WireViz cannot draw a ridge, so the two
render identically in `psu-12v.yml` — the note there is the real record.

**Which conductor is +12V is not established, and must not be guessed.** The
ridge-marks-positive convention is not consistent across supplies, and reversing
it puts −12 V into every panel. Resolve with a meter: the tail past the joint is
unambiguous (red = 12 V, black = GND), so continuity through the joint settles
it, or measure the powered supply directly.

The nameplate's **outer = negative, inner = positive** is a barrel-connector
polarity marking and says nothing about the cable.

## 12V distribution / underglow feed — `12v-distribution.yml`

| Conductor | Gauge | Color | Code | Status |
|-----------|-------|-------|------|--------|
| +12V, all branches | 16 AWG | **yellow** | `YE` | confirmed |
| GND, all branches | 16 AWG | black | `BK` | confirmed |
| Underglow DATA | 18 AWG | white | `WH` | confirmed |

**Pin order on all three YL 2-ways is GND on pin 1, +12V on pin 2** — read off
the parts 2026-08-08, reversing an earlier derived reading that had 12 V first.
Applies to the PSU output, both star-point halves and the barrel-input half.

**12V changes color at this harness's inlet:** it is *red* on the PSU tail and
*yellow* from the fork terminals onward. Black is GND on both sides. The
red-to-yellow join happens across the YL 2-way mating, not at a splice.

**Gauge is NOT a reliable tell inside this harness** — an earlier note claimed
power was 16 AWG against 18 AWG data. The YL branches are 16 AWG, but the SM 3P
branch's power conductors were re-measured on 2026-08-08 as **18 AWG, the same
as the data conductor**. Identify the data line by **color** — white — not by
thickness.

## External 12V input — `external-12v-input.yml`

| Conductor | Gauge | Color | Code | Status |
|-----------|-------|-------|------|--------|
| +12V | 16 AWG | yellow | `YE` | confirmed |
| GND | 16 AWG | black | `BK` | confirmed |

Same yellow/black convention as the rest of the 12 V side downstream of the
fork terminals.

## 5V distribution — `5v-distribution.yml`

**Read this one carefully — the mains-side color instincts do not apply.**

| Conductor | Gauge | Color | Code | Net | Status |
|-----------|-------|-------|------|-----|--------|
| To SM 2P, 5V | 18 AWG | red | `RD` | +5V | confirmed |
| To SM 2P, GND | 18 AWG | black | `BK` | GND | confirmed |
| To 4-way pin 1 | 18 AWG | **green** | `GN` | **GND** | confirmed |
| To 4-way pin 2 | 18 AWG | **white** | `WH` | **+5V** | confirmed |
| To 4-way pin 3 | 18 AWG | red | `RD` | +5V | confirmed |
| To 4-way pin 4 | 18 AWG | black | `BK` | GND | confirmed |

- **Green is a GND here**, not a protective earth. Elsewhere in the pad green
  and green/yellow mean mains earth (`ac-input.yml`).
- **White is a +5V rail here**, not a neutral and not a data line. Elsewhere
  white is mains neutral (`ac-input.yml`) and underglow data
  (`12v-distribution.yml`).
- **+5V and GND are each doubled** across two contacts of the 4-way — a
  current-capacity measure. Both 5V contacts are one net; both grounds are one
  net.
- **Gauge corrected 2026-08-08: these are 18 AWG, not 12 AWG.** The old figure
  also contradicted `5v-columns.yml`, which already said 18 AWG for the same
  conductors downstream; the disagreement is resolved in favour of 18. The
  doubling is still a current-capacity measure, but it no longer follows from a
  12 AWG premise.

## 5V column feed — `5v-columns.yml`

Same four nets as `5v-distribution.yml`, but **the pin order flips**.

| Molex pin | Color | Code | Net |
|-----------|-------|------|-----|
| 1 | red | `RD` | +5V |
| 2 | black | `BK` | GND |
| 3 | **green** | `GN` | **GND** |
| 4 | **white** | `WH` | **+5V** |

VL 4-way order is `GND, 5V, 5V, GND`; Molex order is `5V, GND, GND, 5V`. Both
symmetric, but opposite — **pin numbers do not carry across the harness, colors
do.** All twelve conductors (3 branches × 4) are 18 AWG.

## MCU interface — `mcu-interface.yml`

| Conductor | Gauge | Color | Code | Net | Status |
|-----------|-------|-------|------|-----|--------|
| 2510 pin 12 → SM 2P pin 1 | 22 AWG | red | `RD` | +5V | confirmed |
| 2510 pin 14 → SM 2P pin 2 | 22 AWG | black | `BK` | GND | confirmed |
| 2510 pin 5 → YLP-01V | 22 AWG | **white** | `WH` | **underglow DATA** | confirmed |
| 2510 pins 1–4, 6–11, 13 | — | — | — | — | **empty — no crimp fitted** |

- **White is a data line here** — consistent with the underglow data conductor
  in `12v-distribution.yml`, but the *opposite* of `5v-distribution.yml` where
  white is a +5V rail. White means different things in adjacent harnesses.
- **Note the gauge step at the SM 2P mating:** 18 AWG on the converter side,
  22 AWG on the MCU side. The MCU draws very little, and the step happens at the
  connector rather than at a splice.
- All three conductors share one ~25 cm bundle, so **power and underglow data
  run together** in this cable.

## MCU panel I/O — `mcu-panel-io.yml`

**The nine panel signal lines carry the stock SMX panel color map**, so each
conductor names its own panel. Map (`docs/BOM.md`, re-confirmed in
`docs/STOCK_PANEL_REFERENCE.md`): 0=Red 1=Orange 2=Yellow 3=Green 4=Blue
5=Brown 6=Grey 7=White 8=Black.

| Panel | Color | Code | Micro pin | KF2510 pos | YLR-09V pin | Status |
|-------|-------|------|-----------|-----------|-------------|--------|
| 0 | red | `RD` | D2 | 11 | 3 | confirmed |
| 1 | orange | `OG` | D3 | 10 | 2 | confirmed |
| 2 | yellow | `YE` | D4 | 9 | 1 | confirmed |
| 3 | green | `GN` | D5 | 8 | 6 | confirmed |
| 4 | blue | `BU` | D6 | 7 | 5 | confirmed |
| 5 | brown | `BN` | D7 | 6 | 4 | confirmed |
| 6 | grey | `GY` | D8 | 5 | 9 | confirmed |
| 7 | white | `WH` | D9 | 4 | 8 | confirmed |
| 8 | black | `BK` | D10 | 3 | 7 | confirmed |

All nine are 22 AWG, ~30 cm.

- **The Arduino side is sequential: `Dn` drives panel `n−2`.** D2 = panel 0
  through D10 = panel 8.
- **The YLR-09V side is grouped in threes, descending within each group** —
  pins 1–3 = panels 2,1,0; 4–6 = panels 5,4,3; 7–9 = panels 8,7,6. That is the
  pad's 3×3 grid row by row, reversed within each row.
- **The two ends are ordered differently, so pin numbers do not carry across.**
  Wire by color — and here the color also names the panel.

Panel data bus, same harness:

| Conductor | Gauge | Color | Code | Net | Status |
|-----------|-------|-------|------|-----|--------|
| Micro D1/TX → RJ-12 **pin 3** | 26 AWG | **red** | `RD` | DATA | called from thickness; markings unreadable |
| Micro GND → RJ-12 **pin 4** | 26 AWG | **green** | `GN` | GND | called from thickness; markings unreadable |

Both ~15 cm — half the length of the signal bundle beside them, and they land
on the RJ-12's **centre pair** (positions 3 and 4).

- **Red is a DATA line here.** Everywhere else in the pad red is a supply rail
  (+12V on the PSU tail, +5V on the 5V side). This is the one harness where it
  is a signal.
- **Green is GND here**, as on the 5V side — but *not* protective earth as on
  the mains side.

## Panel signal lines — `panel-signal-lines.yml`

Same nine colors and the same panel map as the MCU side, but **18 AWG** here —
the gauge steps *up* for the long home runs. Each ends in a crimp pin into a
panel terminal block.

| Panel | Color | Code | YLP-09V pin | Length | Status |
|-------|-------|------|-------------|--------|--------|
| 0 (UL) | red | `RD` | 3 | 90 | confirmed |
| 1 (U) | orange | `OG` | 2 | 60 | confirmed |
| 2 (UR) | yellow | `YE` | 1 | 70 | confirmed |
| 3 (L) | green | `GN` | 6 | 120 | confirmed |
| 4 (C) | blue | `BU` | 5 | 90 | confirmed |
| 5 (R) | brown | `BN` | 4 | 100 | confirmed |
| 6 (DL) | grey | `GY` | 9 | 150 | confirmed |
| 7 (D) | white | `WH` | 8 | 120 | confirmed |
| 8 (DR) | black | `BK` | 7 | 130 | confirmed |

**Lengths are centimetres**, confirmed 2026-08-08. Total 9.3 m of 18 AWG for the
nine home runs, before slack.

## Panel power chain — `panel-power-chain.yml`

Same four conductors as the column feed, 18 AWG, 60 cm per jumper, six jumpers
per pad.

| Conductor | Gauge | Color | Code | Net |
|-----------|-------|-------|------|-----|
| 5V | 18 AWG | red | `RD` | +5V |
| GND | 18 AWG | black | `BK` | GND |
| GND | 18 AWG | **green** | `GN` | GND |
| 5V | 18 AWG | **white** | `WH` | +5V |

**The invariant worth memorising:**

| End | Housing | Pin 1 | Pin 2 | Pin 3 | Pin 4 |
|-----|---------|-------|-------|-------|-------|
| Panel **INPUT** | natural/clear | red | black | green | white |
| Panel **OUTPUT** | **red** | white | green | black | red |

- **Housing colour tells you which end you have** — clear plugs into an input,
  red into an output. Same Molex 39014041 part either way; the colour is a build
  convention, not a different component.
- **The conductor order reverses between the two ends** of every jumper, because
  the panel's two headers are the same part facing opposite ways.
- **The net order does not reverse.** `5V, GND, GND, 5V` is a palindrome, so both
  headers read identically by net. Only which *coloured* conductor sits where
  changes: red↔white swap, black↔green swap, and each keeps its net.

## Panel signal terminal / FSR / LEDs

| Conductor | Gauge | Colors | Notes |
|-----------|-------|--------|-------|
| FSR lead | **30 AWG** | red + black | ~10 cm, ends in a JST PHR-2 |
| Panel LED string | — | — | **no wiring — PCB traces only** |

The signal-line terminal block on each panel is 2-position, **signal used, GND
position empty**, and the positions are marked on the block itself.

