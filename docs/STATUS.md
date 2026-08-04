# Project Status / Next Steps

Living doc — updated at the end of each work session so it's easy to pick back
up. See `CLAUDE.md` for architecture; topic docs in `docs/` for detail.
Last updated **2026-08-04**.

## The decision that reshapes everything else

**The panel is the two-board design.** `hardware/dual-panel` — a 127×127 mm
LED/IO carrier with a 70.9×62.8 mm MCU brain socketed underneath in the frame
cavity — is what gets built. The single-board `hardware/panel-pcb` was removed
from the tree on 2026-08-04; it is in git history at `1b41d1c` if it is ever
wanted back, and the measurements that decided it are archived at
`docs/archive/DESIGN_COMPARISON.md`.

Two live boards remain: **`master-pcb`** and **`dual-panel`**.
As-built references: `docs/MASTER_PCB.md`, `docs/DUAL_PANEL.md`.

## Board state — verified 2026-08-04

Re-run from clean project copies with
`kicad-cli pcb drc --severity-all --schematic-parity`:

| board | DRC | unconnected | parity | ERC |
|---|---|---|---|---|
| master-pcb | 1 ✱ | 0 | 0 | 0 |
| dual-panel | 0 | 19 ✱✱ | 90 ✱✱✱ | 0 |

✱ One `courtyards_overlap`, R4 against U2. R4 sits deliberately under the
socketed Teensy; the violation is an accepted exclusion, not an open defect.

✱✱ The permanent board-to-board mating-gap floor — KiCad cannot model "these
mate mechanically." Each was confirmed to be a genuine crossing net.

✱✱✱ A **`kicad-cli`-only artifact; the GUI reports none.** Net *naming*, not
topology — the exported netlist agrees with the PCB pad-for-pad across all 577
pads, and gerbers carry no net names.

## What's proven on the bench

- **Full sensor path**: FSR → ADC (hysteresis + persistence filter, no chatter) →
  open-drain INT wire → Teensy interrupt, sub-millisecond, validated end to end.
- **Multi-panel RS-485 bus**: 2 Picos + 1 Teensy, 1 Mbps, 100% poll replies, 0 CRC
  errors sustained; per-panel LED frames + FSR telemetry + live threshold tuning
  (`docs/RS485_PROTOCOL.md`).
- **USB High Speed confirmed** on Teensy 4.0 — 8000 Hz HID polling achievable
  off the shelf (`bInterval=1`).
- **ADC mux crosstalk fixed in hardware** — 10 nF caps per FSR channel,
  bench-verified, carried onto the brain.
- **Power cascade bench-verified** from 12 V under realistic load.
- **Tooling**: CLI flashing for both MCUs, `tools/fsr_monitor.py`,
  `tools/usb_speed_test.py`, KiCad scripting via bundled python + `kicad-cli`.

Note the bench firmware in `firmware/panel/` uses **breadboard pin numbers** and
matches neither board. The as-built RP2040 GPIO map is in `docs/DUAL_PANEL.md`.

## The only thing blocking an order

**The physical pad teardown.** It settles the **J2 underglow connector** form on
the master — the stock underglow leads crimp directly into a 12-pin Dupont-style
housing at the old MCU, so there is no intermediate connector to reuse and a
splice is required. J2's screw terminal is explicitly interim
(`docs/UNDERGLOW.md`). Stated goal: **order by ~2026-08-08.**

Everything else is a question to *ask JLC*, not layout work:

- Epoxy-fill (POFV) surcharge at 4 layers — never verified against a live quote.
- How POFV treats the **44 vias at 0.60 mm drill**, which exceed the 0.5 mm fill
  limit. All power-distribution, none in a pad, so unfilled is electrically fine —
  but POFV is normally applied board-wide.
- 0.075 mm via annular ring — **partially answered**: the master quoted fine at
  0.075 mm on a standard 4-layer build.

**Master quoted ≈$8 for 5 boards.** Board cost is negligible; **shipping
dominates**, so optimise the number of shipments. A master re-spin is cheap in
boards and expensive in freight — which is the real argument for letting the
teardown land first, or batching master + dual-panel into one order.

## Open sourcing gap

**The 8 board-to-board interface connectors carry no LCSC part number** —
carrier headers J210–J213 and brain sockets J301–J304, 160 pieces across a
20-panel build. They also have a constraint most "2.54 mm header" listings don't
state: **6.0 mm mating pin with a ≥3.0 mm solder tail**, because the separation is
set by the two plastics meeting, not by pins bottoming out, and some "short"
headers trim the tail instead of the mating end. See `docs/DUAL_PANEL.md` →
*Mechanical stack*.

## Other open design questions (not blocking)

- **Underglow harness splice point** — needs the teardown (`docs/UNDERGLOW.md`);
  also PSU stud size and real harness run lengths.
- **INT cable OD check on arrival** — conductor insulation must be 1.3–1.9 mm for
  the JST XH contact. Do this **before** crimping 36 contacts (`docs/BOM.md`).
- **INT cable length** — gated on the teardown; 9 home runs, not a chain.
- **Slotted broadcast polling** — future protocol optimization
  (`docs/RS485_PROTOCOL.md`).

## Reviews

- 2026-07-19 human schematic review — fully triaged and folded in
  (`docs/archive/REVIEW_RESPONSES_2026-07-19.md`).
- 2026-07-23 external AI reviews, both boards — triaged. The master "fab blocker"
  was a false positive from a rotation-sign bug.
- 2026-07/08 external reviews — drove the **THVD1429** speed-grade fix, the
  **LM66200** power-OR, and the **SN74AHCT1G125** single-gate shifter swap. The
  MLCC-microphonics concern was closed with no board change: the FSR signal-path
  caps are **C0G**, which is Class I and not piezoelectric.
- `review/` holds the r/PrintedCircuitBoard image package, regenerated 2026-08-04
  from the current boards via `tools/gen_review_images.py`.

## Naming / branding

Working name **"KrakenPad"** (not final). Repo:
`github.com/fchorney/krakenpad-poc`. pid.codes VID/PID registration still gated on
a LICENSE file + public repo. Project logo pending artwork.

## Concrete next steps (pick from here)

1. **Pad + harness teardown** — underglow splice point, PSU stud size, cable runs.
   This is the blocker.
2. Source the interface headers/sockets (above).
3. Run `docs/PRE_ORDER_CHECKLIST.md` → regenerate the fab package with
   `hardware/dual-panel/panel/gen_fab.py` → order.
4. Place the parts orders (`docs/BOM.md`).
5. Master firmware: USB HID reports to the PC — the one major master piece not
   started.
6. Panel firmware: port to the as-built GPIO map, then flash-backed animation
   playback + config storage (`docs/PANEL_CONFIG.md`, `docs/ANIMATIONS.md`).
7. Extend `stepmaniax-gif-maker` to export `.smxa`.
