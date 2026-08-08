# Wire Harness Documentation

Source-of-truth for every cable, connector and pigtail in the pad — both the
**stock** harnesses (recorded during the 2026-08-08 full teardown) and the
**replacement** harnesses this project needs built.

Diagrams are generated with [WireViz](https://github.com/wireviz/WireViz): each
`.yml` is a text description of connectors + cable + connections, and WireViz
renders an SVG/PNG harness drawing and a BOM from it. Text source means the
harnesses diff in git like everything else in this repo.

## Regenerating

```sh
uv tool install wireviz     # one-time; needs graphviz (brew install graphviz)
./gen.sh                    # renders every .yml to out/
```

Outputs land in `out/` and are **not** committed — regenerate from the `.yml`.

## Two kinds of file — keep them apart

**The current job is documenting the stock pad exactly as it was found**, in
full, before any conversation about what gets kept, reused or replaced. That
conversation happens *after* the record is complete, not during it.

So every file here is one of two things, and they must not blur:

- **STOCK** — what the teardown found. Descriptive. No design opinions, no
  "we could replace this with…". If something is unknown it is marked unknown.
- **PROPOSED** — a harness this project would need to build. Prescriptive, and
  mostly written *before* the teardown, so parts of it rest on pre-teardown
  assumptions that the stock record may well overturn.

Where a stock finding clearly bears on a proposed harness, the note goes in the
**proposed** file, never in the stock one.

### Stock — recorded at the 2026-08-08 teardown

Documented in power-flow order, wall inward. **The stock record is COMPLETE as
of 2026-08-08** — wall socket through to the FSRs, with nothing knowingly
undocumented. A short list of low-consequence unknowns is kept in
`MEASUREMENTS.md`.

| File | Harness | Count per pad |
|------|---------|---------------|
| `ac-input.yml` | Wall cord → YD06 EMI filter → locking IEC C13 | 1 |
| `psu-12v.yml` | C13 → YU1208 12V PSU → JST YL 2-way | 1 |
| `12v-distribution.yml` | YL 2-way → fork-terminal star point → 2nd YL 2-way + underglow SM 3P | 1 |
| `external-12v-input.yml` | Star point → barrel socket, external cabinet 12V in | 1 |
| `dcdc-5v.yml` | Star point → Daygreen B15-1224-05, 12V→5V 15A | 1 |
| `5v-distribution.yml` | Converter 5V out → fork terminals → SM 2P + VLR-04V | 1 |
| `5v-columns.yml` | VLP-04V → 3× Molex 39014041, one per panel column | 1 |
| `mcu-interface.yml` | 5V SM 2P + underglow DATA → 14-pin KF2510 → Micro, analog row | 1 |
| `mcu-panel-io.yml` | 17-pin KF2510 on the Micro's digital row → 9 panel signal lines (YLR-09V) + RJ-12 data bus | 1 |
| `panel-signal-lines.yml` | YLP-09V → nine 18 AWG home runs → panel terminal blocks | 1 |
| `panel-power-chain.yml` | Column feed → 3 panels daisy-chained at 5V, 2 jumpers each | 3 |
| `panel-data-chain.yml` | MCU RJ-12 → all 9 panels, serpentine data bus | 1 |

**Stock topology so far:** wall → EMI filter → 12V PSU → fork-terminal star
point → { underglow SM 3P, external 12V barrel socket, **12V→5V converter** }
→ 5V fork terminals → { **SM 2P → the MCU**, VLR-04V → 3× Molex to the panel
columns }.

**The 5V SM 2P powers the Arduino Micro**, and the same 25 cm cable carries the
underglow DATA line back out from the MCU. Both land on one 14-pin KF2510 crimp
housing that pushes straight onto the Micro's own header pins — no adapter
board. **Only 3 of its 14 positions are populated** (5 = DATA/A1, 12 = 5V,
14 = GND); the rest are empty. See `mcu-interface.yml`.

The Micro has **two header rows with a KF2510 on each**, and both are now
documented — every pin of the stock MCU is accounted for.

**The digital row is the entire panel interface** (`mcu-panel-io.yml`):

- **Nine per-panel signal lines** on D2–D10, one per panel, all landing on a
  JST **YLR-09V**. Their colors are the stock SMX panel map, so **Dn drives
  panel n−2** — D2 = panel 0 through D10 = panel 8.
- **The panel data bus** — a single **TX-only** line off D1/TX plus a ground,
  out through an **RJ-12** jack. D0/RX is covered by the housing and left
  unconnected, so nothing returns to the MCU on that bus.

**On both KF2510s, every position carries a contact** whether or not a wire is
attached — the unwired ones grip the header. An unwired position is not an
empty cavity.

**The underglow data chain is now complete end to end:** Arduino A1 → 2510
pin 5 → JST YLP-01V → YLR-01V → SM 3P pin 2 → the strips.

**The panel columns are fed at 5V**, three branches off one VL 4-way, each on a
4-circuit Molex. This is the stock answer to "how does power reach the three
chains" — and it is *not* the 12V-per-column topology the pre-teardown design
notes assume.

**The stock pad is a 5V pad.** The Daygreen converter is the voltage boundary:
12V exists only between the supply inputs and that box, and **everything
downstream of it — panels, LEDs, MCU — runs at 5V**, with 15A the whole budget.
Read any stock harness past the converter as 5V unless it says otherwise.

### Proposed — this project's own harnesses

Pre-teardown designs. Treat as provisional until the stock record is finished.

| File | Harness | Count per pad |
|------|---------|---------------|
| `underglow.yml` | Master → underglow strips | 1 |
| `power-column.yml` | 12V → column of 3 panels, daisy chain | 3 |
| `rs485-chain.yml` | Master → 9 panels, serpentine RS-485 chain | 1 |
| `int-home-run.yml` | Panel INT → master, one per panel | 9 |
| `fsr-panel.yml` | FSR → panel carrier, internal to a panel | 36 (4×9) |

`MEASUREMENTS.md` is the bench capture sheet — raw numbers go there first,
then get transcribed into the `.yml` files.

`PARTS.md` is the connector/contact/cable part-number index shared across the
harnesses.

`WIRE_COLORS.md` is the per-conductor color reference for every harness, with a
confirmed/TODO status on each. Colors are load-bearing during reassembly — on
the AC side a wrong one is a safety failure — so nothing there is guessed
silently; anything unrecorded is marked TODO rather than filled in.

## Conventions

- **Panel numbers are 0–8**, as everywhere else in this repo (0 = UL … 8 = DR).
  Teardown notes were spoken in 1–9 numbering; subtract one.
- **Lengths in metres**, as WireViz expects. Bench readings are taken in
  **centimetres**, so the conversion into a `.yml` is ÷100.
- **Sanity-check every length against something physical before recording it.**
  Every stock length taken on 2026-08-08 was reported in mm and meant in cm, and
  the whole set had to be corrected ×10 — it was caught only because nine panel
  home runs summed to an impossible 0.93 m of wire. A 7 mm pigtail or a 60 mm
  run across a ~900 mm pad should each have been questioned on sight.
- Wire colors use WireViz two-letter codes (`BK` black, `RD` red, `YE` yellow,
  `PK` pink, `GN` green, `WH` white, `BU` blue, `BN` brown, `GY` grey,
  `VT` violet, `OG` orange).
- Anything not yet measured or confirmed is marked `TODO` in a `notes:` field
  so it shows up on the rendered drawing rather than hiding in a comment.
- Stock-side facts get `(stock)` in the connector `type`; parts we buy get their
  LCSC/vendor number in `PARTS.md`.
- **Never write `->` (or a Unicode arrow) inside a `notes:` field.** Notes are
  emitted into a graphviz HTML-like label unescaped, and the arrow makes `dot`
  fail with an unhelpful syntax error pointing at the generated `.tmp` file.
  Write "then" instead.

## PSU capacity — needs a decision

The supply found in the pad at teardown is a **YU1208, 102 W, DC 12 V 8.5 A**.

This project's own budget in `CLAUDE.md` is 3 parallel column chains, each
3 panels × 25 LEDs × ~36 mA ≈ 2.7 A, so **~8.1 A with all nine panels at full
white** — about 95% of this supply, *before* any underglow current. Underglow is
132 physical LEDs on top of that.

**Decision 2026-08-08: a larger replacement supply will likely be bought.**
The specific part is not chosen yet.

The pad is believed to be a **Gen 5**, but `CLAUDE.md` records Gen 5 as
**12 V 15 A** and Gen 4 as **12 V 9 A** — 8.5 A matches neither. Either the
manual figure is wrong or these pads shipped with something else. Left
unresolved deliberately: the supply is being replaced regardless, so pinning
down the discrepancy buys nothing.

Nothing about the PCB designs changes either way; the supply is a
user-replaceable part sitting entirely upstream of the boards.

When the replacement is specified, size it against **8.1 A of panels + underglow
+ margin**, not against 8.5 A.

## Scope note: AC mains

`ac-input.yml` documents the **stock** mains wiring as found at teardown. This
project replaces the MCU and panel PCBs only — **nothing on the AC side changes**,
and none of it is a board we design. It is recorded so the pad can be
reassembled correctly and so the safety-critical details (the JST YLP-03V's
ground-in-the-middle pinout, the chassis bonding ring terminal) are written down
rather than remembered.
