# Dual-board panel — the production panel design

**This is the panel that gets built.** The single-board alternative
(`hardware/panel-pcb`) was retired on 2026-08-04; it lives in git history only,
last present at `1b41d1c`.

**For as-built facts — interface pin map, RP2040 GPIO map, part inventory,
mechanical stack, verification status — read [`docs/DUAL_PANEL.md`](../../docs/DUAL_PANEL.md).**
This file covers only *why the project is shaped the way it is*; anything
enumerable is derived from the KiCad files and lives in that doc, so it cannot
drift.

> Earlier revisions of this file described a 34-pin two-row interface (J12/J13),
> USB-C on the carrier, and two mounting screws. **None of that was built.** The
> interface is four 1×8 rows (32 pins), USB-C is on the brain, and there are three
> screws. Trust `docs/DUAL_PANEL.md`.

## The idea

Split the 127×127 mm panel into a **display/IO carrier** — 25 WS2815s, every
external connector, the switches — and a **brain** — RP2040, flash, transceiver,
regulators, level shifter, USB — that plugs into the carrier's underside and sits
in the frame cavity. The LEDs own the top surface; anything mounted above them
shadows the play surface.

Motivations, in the order they mattered:

1. **Layout separation.** On one board the LED field and the brain compete for the
   same planes. Split, each board's routing is a different problem.
2. **ESD and protection.** Clamp at the ports on the carrier; the brain sits behind
   the connector.
3. **Fix the brain without re-fabbing the panels.** Not about upgrades — about a
   routing or hardware bug found at bring-up. Re-spinning a small brain board is far
   cheaper in money and materials than re-spinning 20 carriers.

The measured comparison that settled single-vs-dual is archived at
[`docs/archive/DESIGN_COMPARISON.md`](../../docs/archive/DESIGN_COMPARISON.md). Short
version: cost was near-neutral at **$2.13/panel**, and the split measured *better*
on FSR isolation — 1.1× channel-length spread vs 11×, and 3.18 mm worst same-layer
clearance to LED data / 12V vs 1.06 mm.

## Why one KiCad project rather than two

KiCad has no multi-board feature, but a project is already *one hierarchy → one
netlist → one `.kicad_pcb`*, and two `Edge.Cuts` outlines in a single board file is
the supported way to panelise. Two things fall out:

- **ERC checks the board-to-board interface for you.** With two separate projects,
  every interface change means editing two schematics and hand-checking that they
  agree — and the interface spec is precisely the mistake that would force *both*
  boards to be re-spun.
- **One JLC engineering fee and one PCBA setup instead of two.**

The cost is a known wart: the ratsnest draws airwires across the mating gap and DRC
reports 19 `unconnected_items`. There is no way to tell KiCad "these mate
mechanically." Giving each side distinct net names would kill the airwires but throw
away the cross-checking that motivated this structure, so the 19 are accepted and
enumerated instead.

## Why both boards are 4-layer on one panel

Tempting idea: carrier at 2 layers, brain at 4, since the carrier looks like it
might route single-sided. It costs more, not less, and it is not close.

A 2-layer carrier saves only the bare-fab delta on one board — the qty-5 quote put
the *entire* board cost for the 127×127 4-layer panel at **$14.91**, so even if
2-layer PCBs were free the saving caps around **$55** at qty 20; realistically
**$25–35**. A second run costs fixed per-order overhead: $25 eng fee + $51.12 PCBA
setup + $16.42 stencil + $4.93 storage ≈ **$97**, plus a second ~$40 shipment ⇒
**~$137**.

| | one 4L panel (both boards) | 2L carrier + 4L brain |
|---|---|---|
| bare fab | ~$55–60 | ~$25–30 + ~$15 |
| order overhead | $97 × 1 | $97 × 2 |
| shipping | ~$40 | ~$80 |
| **net** | — | **~$110–140 worse** |

There is no quantity where this flips: the saving scales with board count, the
penalty doesn't. Both boards need PCBA regardless — WS2815 is MSL 5a with a 24 h
floor life, so hand-assembling the carrier was never on the table.

What you get for the $25–35 you'd have "saved" is two extra copper layers on the
noisiest board in the system: an uninterrupted GND plane plus a power plane, under a
25-LED field with four high-impedance FSR lines crossing it. That is motivation #1
delivered better than the 2-layer version would have.

**6-layer was the weakest option considered** — JLCPCB lists 4-layer 100×100 mm from
~$7 vs 6-layer from ~$35, and it delivers neither the best routing environment nor
fault isolation. Only a 6-layer *brain* would genuinely force two fab runs; the brain
is already 4-layer-proven, since all of its content routed on 4 layers back when it
was still sharing them with the LED field and 12V distribution.

*Prices are point-in-time — verify live, per the warning in `docs/BOM.md`.*

## Directory map

| path | what |
|---|---|
| `dual-panel.kicad_sch` | root sheet — two sheet symbols, nothing else |
| `carrier.kicad_sch` / `brain.kicad_sch` | the two roles |
| `dual-panel.kicad_pcb` | **both** outlines, carrier left of x=182.55 mm, brain right |
| `dual-panel.pretty` / `.kicad_sym` / `3dmodels/` | project-local libraries |
| `panel/` | panelisation + JLC fab package — `gen_panel.py`, `gen_fab.py`, `QUOTE-2026-07-31.md` |
| `fit-test/` | 1:1 printable cavity fit template — re-run after any outline change |

Both sheets were seeded as full copies of `panel-pcb` rev 1.0 and stripped to role,
which is why some schematic sheet descriptions still say so. That is history, not a
live dependency: **`dual-panel` references no `panel-pcb` library**, and its
`fp-lib-table`/`sym-lib-table` point only at its own project-local libs.
