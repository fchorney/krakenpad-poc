# Stock SMX pad — reference documentation

**What a factory StepManiaX pad actually contains**, recorded from a full
teardown. This tree is descriptive and stands on its own: it is working
knowledge of stock SMX hardware, useful to anyone repairing or studying a pad,
and it is **deliberately independent of this project's replacement design**.

> **This directory contains no design opinions.** Nothing here says what we
> would replace, improve or remove. Our own hardware lives in `hardware/` and
> `docs/`; our replacement harnesses are in `hardware/harness/`.

## Contents

| File | What it covers |
|------|----------------|
| `harness/` | Every stock cable and connector, wall socket to FSRs — 12 WireViz drawings + a one-page topology map |
| `MEASUREMENTS.md` | The teardown bench sheet: raw lengths, gauges, pinouts, and the verification passes |
| `WIRE_COLORS.md` | Per-conductor colors for every stock harness, with confirmed/TODO status |
| `PARTS.md` | Stock connector/contact index, the JST-vs-Molex gender conventions, and JST YL current ratings |
| `PANEL_PCB.md` | The stock panel PCB — reverse-engineering reference |

## The harness drawings

Documented in power-flow order, wall inward. **The record is COMPLETE as of
2026-08-08** — nothing knowingly undocumented. A short list of low-consequence
unknowns is kept in `MEASUREMENTS.md`.

| File | Harness | Count per pad |
|------|---------|---------------|
| `ac-input.yml` | Wall cord → YD06 EMI filter → locking IEC C13 | 1 |
| `psu-12v.yml` | C13 → YU1208 12V PSU → 5.5×2.5 barrel → JST YL 2-way | 1 |
| `12v-distribution.yml` | YL 2-way → fork-terminal star point → 2nd YL 2-way + underglow SM 3P | 1 |
| `external-12v-input.yml` | Star point → barrel socket, external cabinet 12V in | 1 |
| `dcdc-5v.yml` | Star point → Daygreen B15-1224-05, 12V→5V 15A | 1 |
| `5v-distribution.yml` | Converter 5V out → fork terminals → SM 2P + VLR-04V | 1 |
| `5v-columns.yml` | VLP-04V → 3× Molex 39014041, one per panel column | 1 |
| `mcu-interface.yml` | 5V SM 2P + underglow DATA → 14-pin KF2510 → Micro, analog row | 1 |
| `mcu-panel-io.yml` | 17-pin KF2510 on the Micro's digital row → 9 panel signal lines + RJ-12 data bus | 1 |
| `panel-signal-lines.yml` | YLP-09V → nine 18 AWG home runs → panel terminal blocks | 1 |
| `panel-power-chain.yml` | Column feed → 3 panels daisy-chained at 5V, 2 jumpers each | 3 |
| `panel-data-chain.yml` | MCU RJ-12 → all 9 panels, serpentine data bus | 1 |

`harness/stock-overview.dot` is the whole pad on one sheet — graphviz, not
WireViz, because it is a topology map rather than a harness drawing. It carries
no detail that is not already in the `.yml` files.

## Regenerating the drawings

```sh
uv tool install wireviz     # one-time; needs graphviz (brew install graphviz)
cd harness && ./gen.sh      # renders every .yml to harness/out/
```

Outputs land in `harness/out/` and are **not** committed.

## The headline findings

**The stock pad is a 5 V pad.** 12 V exists only between the supply and a
**Daygreen B15-1224-05** DC-DC converter; everything downstream — panels, LEDs,
MCU — runs at 5 V, with 15 A the whole budget. Read any stock harness past the
converter as 5 V unless it says otherwise.

**Topology:** wall → EMI filter → 12 V PSU → 5.5×2.5 barrel → fork-terminal star
point (physically the converter's input screw terminals) → { underglow SM 3P,
external 12 V barrel socket, **12 V→5 V converter** } → 5 V fork terminals →
{ **SM 2P → the MCU**, VLR-04V → 3× Molex to the panel columns }.

**The PSU has no terminal block and no ground stud.** It is a brick with one
captive output cable. Any description of "PSU lugs" or a "PSU GND stud" is
wrong — those came from pre-teardown guesswork elsewhere in this repo and have
been corrected.

**The MCU is fully accounted for.** Both header rows carry a KF2510 crimp
housing pushed straight onto the Arduino Micro's own pins — no adapter board.
Nine per-panel signal lines on D2–D10 (so **Dn drives panel n−2**), a **TX-only**
data bus off D1 through an RJ-12, and A1 driving the underglow. **On both
housings every position carries a contact whether wired or not** — an unwired
position is not an empty cavity.

**Panel LEDs have no wiring at all** — the 25-LED string is entirely PCB traces.
The only LED wiring in the pad is the underglow.

## Conventions used throughout

- **Panel numbers are 0–8** (0 = UL … 8 = DR). Teardown notes were spoken in
  1–9 numbering; subtract one.
- **Lengths in metres**, as WireViz expects. Bench readings were taken in
  **centimetres**, so the conversion is ÷100.
- **Sanity-check every length against something physical.** Every length taken
  on 2026-08-08 was reported in mm and meant in cm, and the whole set had to be
  corrected ×10 — caught only because nine panel home runs summed to an
  impossible 0.93 m of wire.
- **Wire by color within a harness, never by pin number across one.** Colors are
  inconsistent between harnesses; see `WIRE_COLORS.md`.
- Anything not measured or confirmed is marked `TODO` in a `notes:` field so it
  appears on the rendered drawing rather than hiding in a comment.
- **Never write `->` or a Unicode arrow inside a `notes:` field.** Notes are
  emitted into a graphviz HTML-like label unescaped and `dot` fails with an
  unhelpful syntax error. Write "then".

## Scope note: AC mains

`harness/ac-input.yml` documents the mains wiring as found. This project
replaces the MCU and panel PCBs only — nothing on the AC side changes. It is
recorded so a pad can be reassembled correctly and so the safety-critical
details (the JST YLP-03V's ground-in-the-middle pinout, the chassis bonding ring
terminal) are written down rather than remembered.
