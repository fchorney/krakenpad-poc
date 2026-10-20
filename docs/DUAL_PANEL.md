# Dual-Panel (carrier + brain) — as built, rev 1

**This is the panel in use: rev 1, ordered 2026-08-18 (tag `rev1-fab`), nine
boards assembled and running.** Changes proposed for the next revision are in
`docs/REV2_CANDIDATES.md`; nothing below describes them. The single-board
`hardware/panel-pcb` was retired on 2026-08-04 when the two-board split won (git
history, last present at `1b41d1c`); the measurements that decided it are in
`docs/archive/DESIGN_COMPARISON.md`.

Everything below is read out of the KiCad files: the schematic netlist
(`kicad-cli sch export netlist --format kicadxml`) and `dual-panel.kicad_pcb`.
Last derived **2026-08-04**, re-verified after the order-day edits of
2026-08-18. Bring-up results per board are in `docs/BRINGUP_LOG.md`.

## Project shape

One KiCad project, two hierarchical sheets, **one `.kicad_pcb` holding both board
outlines** so they fab and assemble as a single panel:

```
hardware/dual-panel/
  dual-panel.kicad_sch     root — two sheet symbols, nothing else
    carrier.kicad_sch      LED field + every external connector + switches
    brain.kicad_sch        RP2040, flash, transceiver, regulators, USB
  dual-panel.kicad_pcb     BOTH outlines
  panel/                   panelisation + JLC fab package (gen_panel.py, gen_fab.py)
  fit-test/                1:1 printable cavity fit template
```

KiCad has no multi-board feature, but one project is already one hierarchy → one
netlist → one board file, and two `Edge.Cuts` outlines is the supported way to
panelise. The payoff is that **ERC polices the board-to-board interface** — the
one mistake that would force both boards to be re-spun — and it is one JLC
engineering fee and one PCBA setup instead of two.

| | size | layers |
|---|---|---|
| carrier | 139.30 × 127.10 mm (127 × 127 core, 139.3 across the connector tabs) | 4 |
| brain | 70.91 × 62.75 mm | 4 |

174 footprints total, 1107 vias, 4-layer stackup shared by both boards.

**Reference designators are blocked per sheet: carrier = 2xx, brain = 3xx.**
Every type is contiguous within its block with no duplicates across sheets. This
is the multi-sheet exception r/PrintedCircuitBoard's rules explicitly allow.

## Board-to-board interface — 32 pins, netlist-verified

Four independent 1×8 2.54 mm pairs. **Carrier carries the pin headers on B.Cu
(J210–J213); the brain carries the sockets (J301–J304)** and hangs underneath in
the frame cavity.

Every one of the 32 nets below was checked to land on the same net at both ends.

| | J210 ↔ J301 | J211 ↔ J302 | J212 ↔ J303 | J213 ↔ J304 |
|---|---|---|---|---|
| 1 | `+3.3VDC` | `DIP_ID3` | `+12VDC` | `+3.3VDC` |
| 2 | `GND` | `DIP_ID2` | `GND` | `FSR_North` |
| 3 | `TERM_SENSE` | `DIP_ID1` | `GND` | `FSR_West` |
| 4 | `GND` | `DIP_ID0` | `GND` | `GND` |
| 5 | `RS485+` | `GND` | `LED_DATA_5V` | `GND` |
| 6 | `RS485−` | `INT_OUT` | `GND` | `+12VDC` |
| 7 | `GND` | `GND` | `SWCLK` | `GND` |
| 8 | `FSR_East` | `FSR_South` | `SWDIO` | `DEBUG_LED` |

**16 signals, 4 rail pins (+3.3VDC ×2, +12VDC ×2), 12 GND.** Each of the four FSR
lines sits on a different connector with GND adjacent, and `LED_DATA_5V` — the only
fast-edged single-ended signal — is on J212 with GND on both sides, away from every
FSR line.

Two things worth knowing about this interface:

- **USB does not cross it.** `USB_D±`, `VBUS` and `USB_BOOT` are all brain-local
  now that J305 and SW301 moved there (2026-07-28). Earlier planning documents
  that show USB-C on the carrier and a 34-pin two-row interface describe a design
  that was never built.
- **`+3.3VDC` is load-bearing, not a probe convenience.** The four FSR connectors
  sit on the carrier while their dividers sit on the brain, so this rail is the
  **FSR excitation supply** and the reference the ADC readings are ratiometric to.
- **180° rotation is prevented geometrically** — the four rows sit at unequal
  distances from the brain's centreline, so a rotated brain physically will not
  seat. That matters: rotated, `+12VDC` would land on the `+3.3VDC` pin.

## RP2040 (U306) GPIO map — netlist-verified

**This differs from the retired `panel-pcb` map. Firmware must use these.**
`firmware/panel/c/main.c` and `bringup/main.c` both use this map (main.c
since 2026-10-18; its breadboard version is in git history).

| pad | function | net |
|---|---|---|
| 2 | GPIO0 | `RS485_TX` |
| 3 | GPIO1 | `RS485_RX` |
| 6 | GPIO4 | `RS485_DE` (DE + R̅E̅ tied) |
| 13 | GPIO10 | `TERM_SENSE` |
| 14 | GPIO11 | `LED_DATA` (→ U301 shifter → R301 330R → carrier) |
| 27 | GPIO16 | `DEBUG_LED` |
| 28 | GPIO17 | `SENSE_12V` |
| 29–32 | GPIO18–21 | `DIP_ID3`, `DIP_ID2`, `DIP_ID1`, `DIP_ID0` (bit order reversed) |
| 34 | GPIO22 | `INT_OUT` — open-drain to master, the sole gameplay press path |
| 38–41 | GPIO26–29 / ADC0–3 | `FSR_South`, `FSR_West`, `FSR_North`, `FSR_East` |
| 25 | SWDIO | `SWDIO` (SWCLK on pad 24) |
| 46/47 | USB_DM / USB_DP | brain-local, behind R305/R306 27Ω |
| 51–56 | QSPI | `QSPI_SD3/SCLK/SD0/SD2/SD1/SS` → U307 flash |
| 20 | XIN | X301 12 MHz |

**16 of 30 GPIO used.** Spare: 2, 3, 5–9, 12–15, 23–25.

### Pin-capability audit (2026-08-04)

Checked every assignment against the RP2040 datasheet's GPIO function table.
**No blockers — the board is wired correctly. Four things firmware must know:**

- ✅ **`RS485_TX`/`RS485_RX` are on GPIO0/GPIO1, which really are UART0 TX/RX.**
  Hardware UART, not PIO. (GPIO16/17 would have been the other UART0 pair, and
  they're used for other things — no conflict.)
- ⚠ **`RS485_DE` on GPIO4 is not a UART0 RTS pin** — those are GPIO3, 15 and 19.
  DE must be driven in software (or by a PIO UART), timed off the TX FIFO
  draining. This is not a layout mistake: the RP2040's PL011 has no RS-485
  auto-direction mode on *any* pin, so no pin choice would have bought it. But
  it is the classic RS-485 bug — **release DE too early and the last byte is
  truncated, too late and you collide with the next talker.** The breadboard
  prototype already ran 1 Mbps with manual DE and 0 CRC errors, so the approach
  is proven; it just has to be re-derived on the real pin.
- ⚠ **`SENSE_12V` on GPIO17 is a digital threshold, not a measurement.** Only
  GPIO26–29 have ADC, and all four are taken by FSRs — **there is no spare ADC
  channel.** The divider is sized for a clean presence detect, verified from the
  netlist:

  | | |
  |---|---|
  | divider | R313 100k / R314 33k → ratio 0.2481 |
  | at 12.0 V | 2.977 V at the pin |
  | reads HIGH above | 8.65 V input (VIH = 0.65·3V3 = 2.145 V) |
  | reads LOW below | 4.65 V input (VIL = 0.35·3V3 = 1.155 V) |
  | over-voltage | divider alone survives to 14.5 V; **D303 clamps to +3.3VDC** past that |
  | quiescent | 90 µA |

  So it answers "is the 12 V bus up?" with a wide, safe hysteresis band. It
  cannot report *what* the rail voltage is, and adding that later would mean
  giving up an FSR channel.
- ⚠ **`INT_OUT` on GPIO22 must be emulated open-drain.** The RP2040 has no true
  open-drain output mode. Assert = drive output LOW; release = switch the pin to
  **input (hi-Z)**, never drive it HIGH. Driving push-pull would fight the
  master's 10 k pull-up and break the documented safe-failure behaviour
  (disconnected wire reads HIGH = not pressed). The pin itself is fine; this is
  purely a firmware contract.

Everything else is a plain SIO input or output with no special requirement:
`TERM_SENSE` (GPIO10), the four `DIP_ID` lines (GPIO18–21, internal pull-ups, no
board resistors), and `DEBUG_LED` (GPIO16 — PWM0A is available on that pin if
brightness control is ever wanted). `LED_DATA` on GPIO11 is driven by PIO, which
can drive any GPIO.

> **`SENSE_12V` on a non-ADC pin is CLOSED — do not raise it again.** The
> 2026-07-23 external AI review raised it as its finding F1; the user confirmed
> then that 12 V sense is a **digital present/absent check only** — it gates
> whether it is safe to drive the WS2815s — and was **never** meant to measure bus
> voltage. So a non-ADC pin is correct, not a defect. This was recorded in
> `docs/PANEL_PCB.md`; that file was deleted with the single-board design on
> 2026-08-04 and the note was nearly lost, which is why it is restated here.

## Signal conventions (netlist-verified)

Small facts that are easy to get wrong at firmware or assembly time.

- **Panel-ID DIP bit order is reversed.** `SW201` pins 1–4 drive `DIP_ID0`–`DIP_ID3`,
  but on the MCU **GPIO18 = bit 3 … GPIO21 = bit 0**. Switch closes to GND, internal
  pull-ups, so **closed = 0**.
- **`TERM_SENSE`: LOW = terminated.** `SW202` is DPDT — pole A puts `R201` (120 Ω)
  across the pair, pole B reports the state so firmware can never disagree with the
  copper. Wiring as built: pin 2 = `RS485+` (pole-A common), pin 1 → `R201` →
  `RS485−`, pin 5 = `TERM_SENSE` (pole-B common), pin 4 = GND, pins 3 and 6 NC,
  **pins 7/8 are mounting lugs tied to GND**.
- **FSR connectors are `pin 2 = +3.3VDC`, `pin 1 = ADC node`.** The FSR itself is
  non-polarised, so a reversed lead is harmless.
- **WS2815 chain has three endpoints per net, not two** — the backup ring. Verified:
  `LD1 = D203.DOUT + D208.DIN + D213.BIN`. Rule is `BIN(n) ← DIN-signal(n−1)`, the
  **first LED's BIN ties to GND** (D203 pins 5 and 6 are both on GND), and the last
  DOUT is left unconnected. That is ~1.5× the routing of a plain daisy chain and it
  is deliberate: it lets the chain survive one dead LED.
- **The 25-LED field is wired serpentine** — rows alternate direction, same physical
  topology as the stock SMX panel. Animation tooling has to account for the mapping;
  see `docs/ANIMATIONS.md`. The row of 3 is intentionally rotated 180° from the row
  of 4, which is a consequence of that layout and not a placement error.
- **Support pins:** `RUN` = R307 10 k pull-up; `QSPI_SS` = R310 10 k pull-up —
  **fitted deliberately, it closes an early-power-ramp race** — plus R309 1 k to
  `SW301` BOOTSEL; `ADC_AVDD` = R308 200 Ω from +3.3VDC into C318 2.2 µF.

## Layout and routing

- **4 layers, JLC 4-layer standard** (0.30 mm drill vias on 0.60 mm pads since
  2026-08-17; three stay at 0.45 mm for clearance): L1 components + signals,
  **In1 and In2 are BOTH solid GND, never split** (layer names `GND1.Cu`/`GND2.Cu`);
  B.Cu spillover + GND pour. There is **no 12 V plane** — 12 V is a routed trace
  tree (2 mm trunk, ≥0.5 mm LED branches, verified by widest-path analysis
  2026-08-06), and the 3.3 V/5 V islands are small F.Cu pours at the regulators,
  not inner-layer pours. An earlier draft of this file described In2 as power
  pours; that was never the as-built board. Chosen for analog noise —
  high-impedance FSR lines sharing a board with 25 switching LEDs — not for
  routing density.
- ⚠ **The two boards need SEPARATE GND zones, and this has already broken once.**
  As built there are **four** zones: `GND_Planes` (priority 0, carrier, 4 layers),
  **`GND_Brain` (priority 1, brain, 4 layers)**, and the two F.Cu regulator islands
  `LD0 5VDC AMS` (2) and `LD0 3.3VDC` (3).

  `GND_Brain` was **deleted by commit `96f43c0` (2026-08-03, the shifter swap —
  a 62 704-line full-file rewrite)** and the loss went unnoticed because
  `GND_Planes` was then simply stretched east to cover both boards. That fills
  correctly in KiCad, so **DRC, ERC, schematic parity and an external review all
  passed** — but KiKit extracts each board with a rectangular `sourceArea`, so the
  one spanning zone was attributed to the carrier and clipped to it, and **the
  brain came out of panelization with no ground pour on any of its four layers.**
  Two fab packages were generated in that state. Restored 2026-08-17.

  A zone spanning the split cannot survive panelization. `gen_panel.py` now fails
  the build if any board ends up without a ground pour on every copper layer.
- **Assembly is double-sided**: 115 SMD placements, **101 top / 14 bottom**. THT
  (connectors, switches, the interface headers) is hand-soldered, not JLC.
- **High-current path:** at the IN connector of a column's first panel the 12 V
  daisy-chain carries **~1.3 A** (three panels), of which ~0.9 A passes through
  to OUT and ~0.44 A is local — fat copper on L1/In2 is now generous rather than
  necessary. **Corrected 2026-08-16** from 3.7 A / 1.24 A: the WS2815 datasheet's
  15 mA is **per pixel, not per channel** (three dies in series, unlit ones
  shorted out), so per-LED draw is ~15 mA, not 47 mA. Bench-measured at
  10.5 mA/pixel. See `hardware/harness/README.md` → "Power budget".
- **10 nF ADC caps sit physically at the RP2040 pins.** The crosstalk fix is
  placement-sensitive; do not relocate them for routing convenience.
- **QSPI is length-matched to a 20 nm spread across the five data/clock nets**
  (19.82 mm each, 0.15 mm wide). Do not disturb it. Width changes are safe — they do
  not affect length.

### Trace / via width conventions (the user's routing rules)

| Class | Trace | Via |
|---|---|---|
| 12 V trunk (IN/OUT + ground return, ≤1.3 A) | 2 mm | 0.6 mm |
| LED power feeds off the 12 V plane + grounds | 1 mm | 0.3 mm |
| 5 V / 3V3 top-layer power | 1.5–2 mm | 0.6 mm |
| Data | 0.2 mm (0.15 mm where needed, e.g. the QFN escape) | 0.3 mm |
| Ground (non-trunk) | 2 mm | 0.3 mm |
| Decoupling | ~2 mm, short beats wide | 0.3 mm |
| QSPI | **0.15 mm**, length-matched | — |
| USB / RS-485 | per impedance below | — |

**CLOSED 2026-08-16 — no re-route needed.** This was opened when the class
current appeared to rise from 2.7 A to 3.7 A, leaving 2 mm of 1 oz outer copper
at only a 1.05× margin. That rise came from misreading the WS2815 datasheet's
15 mA as per-channel; the real class current is **1.3 A**, against ~3.9 A for
2 mm of 1 oz outer copper at a 10 °C rise — a **3× margin**. The trunk and its
vias are comfortably oversized as drawn. Nothing to widen; nothing to decide.

As-built deviation worth knowing: **`XIN`/`XOUT` are routed at 0.2 mm, not the
0.15 mm the table implies.** That is fine — the 0.15 mm figure exists for the QSPI
pinch at the QFN escape, not for crystal nets.

### Differential pairs

Measured from the board, not calculated forward:

| pair | width / spacing | length | layers |
|---|---|---|---|
| `USB+` / `USB−` | 0.25 mm / 0.1375 mm | 3.02 / 3.04 mm | F.Cu only |
| `RS485+` / `RS485−` | 0.15 mm / 0.2 mm (~119 Ω) | 209.06 / 208.05 mm | F.Cu + ~34 mm B.Cu each |

USB is now **3 mm long**, because the receptacle moved onto the brain right beside
the RP2040 — impedance control is close to irrelevant at that length, and the
0.1375 mm pair spacing is deliberate coupling, not a clearance violation. RS-485
mismatches by 1.01 mm over ~208 mm; at 1 Mbps that is ~7 ps, i.e. nothing.

Impedance figures come from Hammerstad-Jensen cross-checked against IPC-2141 (they
agree within 5%); ±10% is the honest band for a non-impedance-controlled order.

**Retired finding — do not port it forward.** `panel-pcb` had deliberate one-sided
B.Cu hops on `USB_D+` and `RS485+`, where one conductor dove to B.Cu to cross under
its partner because the pair arrived in the wrong order for its destination pins.
Every automated review flagged them (2026-07-23 findings F7/F8) and they were
closed WONTFIX. **They do not exist on `dual-panel`** — verified: both USB legs are
F.Cu-only across 3 segments each, and both RS-485 legs carry a comparable amount of
B.Cu (~34 mm) because the pair crosses the board-to-board interface, which is not a
crossover. If a future review reports a one-sided hop, it is a new finding.

> **The stackup lesson, so it is never re-derived.** On `panel-pcb` the pairs were
> first drawn at USB 0.528 mm / RS-485 0.2787 mm — correct for KiCad's **default**
> stackup (3 × 0.48 mm FR4), which is what they had been calculated against. Applying
> the real JLC 4-layer stackup put the reference plane at **0.2104 mm** instead of
> 0.48 mm. Impedance follows w/h, so a 2.28× thinner dielectric needs a ~2.1×
> narrower trace; the original geometry was landing near 55 Ω / 92 Ω.
> **Rule of thumb: 90 Ω USB on a normal 4-layer stackup is 0.2–0.3 mm. If you have
> drawn 0.5 mm, the stackup is wrong.**

## Physical

127 × 127 mm carrier core (real edges 128/127/128/127), mounting holes 4.5 mm on
114 mm centres except the top pair at 113 mm, LED lattice at 33.5 mm column /
17 mm row pitch — all measured off the stock board and confirmed with a 1:1
printout (`stock-smx/PANEL_PCB.md`). **X is locked at ~127 mm by the edge
connectors; Y has ~20 mm of slack per end** if a future revision needs it. Height
budget above the PCB is ~35 mm.

Silkscreen: "Kraken Pad by SenPi / Rev. 1.0", JLC order-number placeholder on
B.Silkscreen, and the project logo.

⚠ **Corrected 2026-08-18: the logo is SILKSCREEN, not exposed copper.** This line
previously described it as "exposed GND copper via the mask-opening technique"
with artwork still pending. Verified against the board — all graphics live on
`F.Silkscreen` (10 items) and `User.Drawings` (5); **there is no copper/mask logo
pair anywhere.** The stale claim mattered because it was the stated reason the
surface finish was an aesthetic choice, which propagated into
`docs/rev1-order/PRE_ORDER_CHECKLIST.md`. Finish is **ENIG**, decided on solderability and
probe life, not appearance.

## Passthrough variant

A carrier populated with only connectors + the termination switch + R201 is
electrically a valid passthrough: power and RS-485 are bused connector-to-connector,
and INT floats safe-HIGH at the master. Mechanism is KiCad per-symbol DNP → a second
BOM/CPL export, so it is **one bare-PCB SKU with two assembly variants**. Nothing is
built yet — see `docs/MODULAR_PANEL_COUNT.md`.

## Rev-1 risks accepted at order, and what bring-up found

These were accepted as bench questions rather than order blockers.

| Risk | Result |
|---|---|
| `U303` thermal at real load | Barely warm running the board on 12 V only (first board) |
| +5 V rail margin | `U303` output 4.965–5.034 V across boards; VBUS 5.25 V wins the `U304` mux whenever USB is present |
| Hot-plug / USB attach | **Found a real limit:** the brain back-drives VBUS to 2.78 V on 12 V, so a host won't attach to a powered panel. Connect USB before 12 V. Rev-2 candidate #3 |
| SI asymmetries on the bus | Nine panels on one bus, 0 errors in steady state (`BRINGUP_LOG.md`, master #2, 2026-10-19) |
| FSR runs on B.Cu | A hard press moves no neighbouring channel; `ADC_DUMMY_READ = 0` confirmed |
| INT into a dead panel | Not recorded as tested |

## Part choices and their history

Why the brain and carrier use the parts they do. Moved here from `CLAUDE.md`
on 2026-10-20 so that file can stay short; the text is unchanged except where
noted. Rev-2 changes to any of these are in `docs/REV2_CANDIDATES.md`.

### MCU: bare RP2040 (QFN-56), decided 2026-07-10

Chosen over the RP2040-Zero module: ~$3–4/panel support BOM vs ~$10 module, full pin control (clean ADC routing incl. GPIO29, 10nF caps at the pins), keeps the 4MB W25Q32JV decision (Zero is fixed 2MB), and the whole panel becomes one flat fab-assemblable PCB. Support circuit per the official "Hardware design with RP2040" minimal reference: 12MHz crystal + load caps, decoupling set, QSPI flash, **USB-C for flashing** (16-pin USB 2.0-only receptacle, **through-hole or TH-reinforced mounting required** — TH pins take the mechanical stress of repeated plug/unplug during flashing. Orientation is free (clarified 2026-07-11, supersedes the 2026-07-10 "vertical preferred"): the port is only used on the bench with the panel top off, so cable clearance is irrelevant; only constraint is receptacle body height must clear the panel platform during play (trivial, ~35mm budget). **Part as built: GCT USB4085-GF-A** (horizontal, all-TH), LCSC **C7095263**, footprint `dual-panel:USB_C_Receptacle_GCT_USB4085_EdgeTrim`, **on the brain as J305** — it moved off the carrier 2026-07-28. The vertical LCK TCF829D (C53184807) was the earlier pick; its footprint `USB_C_Receptacle_LCK_TCF829D_TEMPLATE` is still in `dual-panel.pretty` as a deliberate revert path, so going back to vertical is a footprint re-pick. Note **J305's contact holes sit at exactly 0.45mm edge-to-edge — zero margin against JLC's multilayer floor** (re-verified 2026-08-04); the `.kicad_dru` pad-to-pad rule is relaxed to 0.45mm for it, and it stays on the DFM-confirmation list. Rejected earlier: GCT USB4105 (SMT signals). 5.1kΩ pull-down on each CC pin — required for C-to-C cables; tie duplicated D+/D− pairs), **USBLC6-2SC6 ESD array on D+/D−/VBUS (U305, added 2026-07-17)** — rationale is dance-pad specific: rubber-soled shoes rubbing the panels tribocharge the pad, so USB ESD exposure isn't limited to bench plug events; sits on the connector side of the 27Ω series resistors, flow-through routed — BOOTSEL button, and a 3-pin SWD header (SWDIO/GND/SWCLK) as the firmware-independent recovery/debug path.

### RS-485 transceiver

- RS-485 transceiver: **THVD1450DR** (final PCB, both boards — LCSC **C2671361**, TI, SOIC-8). History: THVD1419 (250kbps, too slow for the 1Mbps bus) → THVD1429 (20Mbps grade) on 2026-07-19 → **THVD1450 on 2026-08-18**. The 1450 has the **identical SOIC-8 pinout** (R1, RE̅2, DE3, D4, GND5, A6, B7, VCC8 — verified against both datasheets), the same open/short/idle failsafe, same 1/8-unit-load 256-node bus, same 3–5.5V supply. **What changed and why:** it drops the 1429's on-die ±2.5kV IEC 61000-4-5 surge TVS, which TI's own datasheet frames as protection against *lightning and industrial power-grid transients* — not a threat this indoor, ~3m, in-chassis, shielded bus faces. In exchange it is **better on the threat that IS documented here** (ESD from a tribocharged pad and from hand-servicing connectors): **±18kV vs ±8kV IEC 61000-4-2 contact**, ±30kV vs ±16kV HBM. It also improves common-mode range (±15V vs ±12V), bus-fault range (±18V vs ±15V), and adds glitch-free hot-plug. It was **3.6× cheaper ($0.99 vs $3.60) and had 84× the JLC stock (5,056 vs 60)**, the latter having been the tightest line in the whole BOM. ⚠ **The RS-485 A/B pair has no external TVS** — D201 is on the INT line — so the transceiver's own rating (±18kV IEC 61000-4-2 contact) is the only bus protection. **DECIDED 2026-08-18: this ships as-is for rev A, and an external TVS on A/B is a rev-2 candidate.** It is purely additive — two parts plus placement and routing on the carrier — so deferring costs nothing but a rev. Do not re-open for rev A. Alternative: **MAX3485** (budget breadboard substitute, same SOIC-8 footprint, 10Mbps) — must be 3.3V-logic compatible. Do NOT use MAX485 (5V-only, logic thresholds marginal at 3.3V)

### Flash

- **Flash**: W25Q32JV (4MB QSPI, SOIC-8) — required for RP2040 boot. 4MB chosen over 2MB (W25Q16JV) for future-proofing; both are pin-compatible, same footprint, negligible cost difference. Stores firmware + released/pressed animation slots + config. Flash map: `docs/FIELD_UPDATE.md` → "Flash map".

### LED data level shifter

- **LED data level shifter**: **SN74AHCT1G125 (SOT-23-5, single gate) as of 2026-08-03** — swapped in on all three boards (panel, dual-panel brain, master) from the quad SN74AHCT125 after an external review noted only one gate was used. The reviewer suggested 74LVC1G34/74LVC1G17; both were rejected on datasheet grounds (LVC at 5V has VIH = 0.7×VCC = 3.5V; the 1G17's Schmitt VT+ max reaches 3.33V at 5.5V — same marginal-threshold trap as MAX485). AHCT keeps the guaranteed TTL VIH = 2.0V. LCSC **C7484**, DigiKey **296-4708-1-ND**, MPN SN74AHCT1G125DBVR; OE̅ (pin 1) tied to GND. Breadboard prototype keeps the quad SN74AHCT125N DIP. VCC = 5V in all cases. Its output stays at ~5V regardless — the chip cannot run from 12V (its own VCC abs max is far below that) and doesn't need to: WS2815's DIN comparator threshold is not scaled to its 12V power rail, so a normal 5V logic signal is sufficient (well-established in the addressable-LED community; only the LED VDD/GND pins are 12V, never the data line). Direct 3.3V connection works for WS2812B in practice (spec says 3.5V min) but the shifter is required on the final PCB.

### Logic regulation

- **Panel logic regulation — DECIDED 2026-07-09: cascaded linear LDOs.** AMS1117-5.0 (12V → 5V) feeding the 3.3V stage — **which is AP7361C-33ER-13 as of 2026-07-16** (swapped in for AMS1117-3.3: low-dropout CMOS, ~360mV@1A vs ~1.1V, gives >1V of headroom when powered from USB VBUS alone through the power-OR diode; Vin abs max 6V so it must stay downstream of the 5V stage, never raw 12V; **order the `-33ER-` suffix only** — SOT223R pinout matches AMS1117 GND/OUT/IN tab=OUT, while plain `-33E-` is pin-reversed). **5V power-OR is U304, an LM66200 ideal-diode mux (LCSC C3235556, SOT-583 8-pin) as of 2026-07-20** — auto-selects the higher of VBUS and the AMS1117 5V output, needs no external caps, and removes the Schottky VF drop that left the shifter rail at ~4.7V against a 4.5V VCC minimum. The former power-OR diodes D301/D302 (**PMEG3015EH**, SOD-123F, 255mV typ @100mA; they had replaced 1N5819W on 2026-07-16) remain on the board as **DNP footprints** — populating both and removing U304 restores the Schottky OR as a hand-solderable rescue. Discrete cross-coupled P-FETs were considered and rejected: with both supplies present both FETs turn off and conduction falls to the body diodes, sagging the rail to ~4.3V — worse than the Schottky. Cap rules: AMS1117-5.0 output (C308) = 22µF 16V tantalum (needs ESR — ceramic-only can oscillate); AMS1117 input (C306) and both AP7361C caps (C303/C307) = 10µF 0805 MLCC (AP7361C is ceramic-stable per datasheet). The 5V rail powers only the SN74AHCT1G125 level shifter (single-digit mA — this rail was previously missing from the design; the prototype borrowed a separate USB 5V supply, see docs/archive/PROTOTYPE_WIRING.md, but the final panel is self-contained on 12V bus power). The 3.3V rail powers RP2040, RS-485 transceiver, and remaining logic (~30–50mA total).
  - Why cascade over two parallel LDOs off 12V: second stage's PSRR gives a very clean 3.3V for the RP2040's noise-sensitive ADC (FSR readings); dissipation splits favorably (~0.35W in the 5.0, ~0.07W in the 3.3 — fine for SOT-223 with a copper pour); same part family = BOM coherence.
  - Why not a buck (or buck + LDO hybrid): total logic draw is ~0.44W worst case from 12V — no thermal problem to solve. A buck adds inductor/caps/layout risk, switching noise near the FSR analog inputs, and at single-digit mA loads many bucks drop into PFM/pulse-skipping with worse ripple. Hybrid only becomes worthwhile if logic-rail load grows to ~200mA+.
  - 3.3V-stage dropout is a non-issue since the AP7361C swap (~tens of mV at the ~30–50mA panel load); the old AMS1117-3.3's ~1.1V dropout was the USB-VBUS-only weak point that motivated the swap.
  - **AP2112K-3.3 is NOT usable anywhere here** — absolute max input 6.5V, far below 12V. MCP1804 (28V max) remains a fallback if AMS1117 sourcing fails.
  - LED strip VDD/GND stay straight 12V, untouched — the shifter only drives the data line into the first LED, never LED power.

The cascade runs every rev-1 board; `U303`/`U302` regulate at 4.965–5.034 V and
3.29–3.30 V on 12 V alone (`docs/BRINGUP_LOG.md`).

## Carrier (2xx) — 76 parts

| block | designators |
|---|---|
| LED field | 25× WS2815 **D203–D227**, pin-1 100nF **C203–C227** (Cn pairs with Dn) |
| bulk | **C201** 470µF 25V elec |
| FSR connectors | **J201** W, **J202** S, **J203** E, **J206** N — JST B2B-PH-K (C131337). Dividers are on the brain |
| power | **J205** 12V_IN, **J208** 12V_OUT — Micro-Fit 43650-0200 (C192562) |
| RS-485 | **J204** IN, **J207** OUT — Micro-Fit 43650-0300 (C503478); **SW202** termination (SS22E01L5, C609835) + **R201** 120Ω; shield network **R202** 1M ‖ **C202** 100nF |
| INT | **J214** WJ500V-5.08-2P (C8465), **D201** SMAJ5.0A, **R203** 100Ω |
| panel ID | **SW201** 4-pos DIP (DS-04, C52177925) |
| SWD | **J209** 3-pin PZ254V-11-03P (C2937625) |
| debug LED | **D202** + **R204** 1k |
| interface | **J210–J213** 1×8 headers, B.Cu |
| mechanical | **H201** (86.83, 106.19), **H202** (123.03, 53.86), **H203** (146.03, 108.06) |
| test points | TP201–TP218, from the netlist: **+12VDC** TP203, TP205 · **TERM_SENSE** TP201 · **RS485+/−** TP202/TP204 · **+3.3VDC** TP206 · **INT** (`D201` K) TP211 · **LED_DATA_5V** TP214 · **LED chain nodes** LD6/LD12/LD18/LD23 = TP215/TP216/TP217/TP218 · **GND** TP207–TP210, TP212, TP213. ⚠ Nine of these footprints carry a wrong *Value* field (most say `GND`); fab layer only, not printed. Corrected 2026-10-02 |

Protection lives at the ports on the carrier, so a plug or ESD event clamps
upstream and never crosses the interface.

## Brain (3xx) — 52 parts

| block | designators |
|---|---|
| MCU | **U306** RP2040 + decoupling **C309–C325**, **X301** 12 MHz + **C311/C313** 15pF + **R302** 1k `XOUT` series, **R307** 10k `RUN` pull-up |
| flash | **U307** W25Q32JV 4MB + **R310** 10k `QSPI_SS` pull-up + **R309** 1k to `SW301` (BOOTSEL). **`R307` is NOT here — it is the 10k `RUN` pull-up**, see "Signal conventions" |
| RS-485 | **U308** THVD1450 (50 Mbps grade, LCSC C2671361 — swapped from THVD1429 2026-08-18). ⏸ **Rev-2 candidate: external TVS on A/B** — the bus has none (D201 is INT-only) and the 1450 has no on-die surge cell. Deliberate for rev A |
| LED shifter | **U301** SN74AHCT1G125 (single gate, SOT-23-5) + **R301** 330Ω series |
| power | **U303** AMS1117-5.0 (12V→5V) → **U304** LM66200 ideal-diode OR → **U302** AP7361C-33ER-13 (5V→3.3V) |
| power-OR rescue | **D301/D302** PMEG3015EH, **DNP** — populate both and remove U304 to restore a hand-solderable Schottky OR |
| USB | **J305** GCT USB4085-GF-A (C7095263), **U305** USBLC6-2SC6 ESD, **R303/R304** 5.1k CC, **R305/R306** 27Ω series, **SW301** BOOTSEL |
| FSR front end | **R311/R312/R315/R316** 10k 1% dividers, **C324/C326/C329/C330** 10nF **C0G** |
| 12V sense | **R313** 100k / **R314** 33k divider, **D303** PMEG3015EH clamp |
| ADC supply | **R308** 200Ω + **C318** 2.2µF to `ADC_AVDD` |
| mechanical | **H301** (188.51, 89.32), **H302** (224.71, 36.99), **H303** (247.71, 91.19) |
| test points | TP301–TP312 — rails, UART, RS-485 DE, RS485±, LED data 3.3V, GND |

The FSR dividers are on the brain deliberately: the divider ratio is a tuning
parameter (10k vs 12k, or a different ratio once real FSRs are characterised
under real feet), so it belongs on the board that can be re-spun alone. Likewise
`C324/C326/C329/C330` sit at GPIO26–29 because the RP2040's SAR ADC dumps its
sampling cap onto the pin each conversion and needs that charge back from a
low-inductance local reservoir — across a 2.54 mm connector hop you have put
inductance in series with exactly the thing that must respond in nanoseconds.

**All four FSR signal-path caps are C0G (Class I).** That is deliberate and it is
the standard fix for MLCC microphonics — do not "simplify" them to X7R. The only
Class II part on the analog path is C318 (X5R) on `ADC_AVDD`, which is shared by
all four channels so any shift is common-mode.

## Mechanical stack

Three M3 screws take all mechanical load; the interface connectors carry none.

| item | mm |
|---|---|
| header plastic, `C5383116` (HanElectricity 2541WV-08P, carrier B.Cu) | 2.54 |
| socket body, `C7509515` (CONNFLY DS1023-1x8SF11, brain) | 8.50 |
| connector stack | **11.04** |
| **spacer: M3 female-female, 12 mm** | **12.00** |
| air gap between the plastics | 0.96 |
| pin engagement (6.0 mm mating pin) | ~5.04 |
| brain PCB + M3 socket-cap head | 1.60 + 3.00 |
| **total below the carrier underside** | **16.60** |
| available (6 mm to frame floor + 14 mm inside the opening) | 20.00 |
| **spare** | **3.40** |

- **The spacer sets the separation and must never be omitted.** It has to be
  taller than the connector stack, or tightening pulls the brain into the
  carrier and the connectors take the clamping force. An 11 mm spacer is
  0.04 mm short of the 11.04 mm stack, which is why 12 mm was chosen
  (2026-08-16). The table before the connectors were sourced assumed
  2.45 + 8.30 = 10.75 mm and an 11 mm spacer; that is superseded.
- **Socket height only buys pin engagement**, not separation. A shorter socket
  with a matched spacer is equally valid on load: `C55218878` (5.7 mm) with a
  9–10 mm spacer gives ~5.2 mm engagement and more cavity spare. The 8.5 mm part
  keeps the 6 mm pin inside its bore, so it can't be pin-loaded even with a
  wrong spacer.
- **Source headers by mating-pin length:** 6.0 mm mating pin **and** ≥3.0 mm
  solder tail. Some "short" headers trim the tail instead, leaving nothing to
  solder through the carrier. Most listings state neither dimension.
- **What sits in the gap:** the brain's **F.Cu faces the carrier**, carrying
  `J305` (USB-C) and both SOT-223 LDOs. The tallest is the GCT USB4085 at
  3.16 mm, clear of every spacer considered. USB-C cable clearance is not a
  requirement (the port is used on the bench with the panel top off), and the
  in-situ access path is the SWD header `J209` on the **carrier's** F.Cu. A
  2026-08-16 analysis that treated the gap as a USB-cable budget was wrong.
- **Everything deep must sit inside the cavity opening**, because the frame floor
  is only 6 mm below the carrier. Verified: all four interface connectors and all
  three screws clear the opening on every side. Re-check with
  `fit-test/gen_cavity_template.py` after any outline or placement change.
- **Assembly:** fit both headers into their sockets and mate the boards, **then**
  solder the second connector's pins with the stack held together. Four
  connectors must align in X, Y and rotation at once.
- Carrier underside carries only D201, R202, R203 and the four headers.

## First bring-up — blank RP2040 over USB

Written 2026-09-02, before the first board was powered, from
`dual-panel.kicad_pcb`. Its predictions held on all nine boards brought up.

> **The staged procedure and the firmware that runs it are now
> `docs/PANEL_BRINGUP.md` + `firmware/panel/c/bringup/`** (2026-09-03). This
> section stays as the netlist-level reference it has always been; that one is
> what to follow at the bench.

**A blank board is the easy case.** The RP2040 bootrom checksums the first 256
bytes of QSPI flash; blank flash (`0xFF`) fails that check and the chip drops
into the USB bootloader **automatically — no BOOTSEL press needed**, enumerating
as `RPI-RP2`. `SW301` only matters once a valid image is on the flash.

**The brain is standalone.** MCU, flash, crystal, both regulators, the power-OR,
USB and the transceiver are all 3xx parts, so brains can be flashed on the bench
before being mated to a carrier.

### USB-only power path

```
J305 VBUS ──→ U304 pin 3 (LM66200 IN1) ─┐
                                        ├─→ +5VDC ──→ U302 AP7361C-33 ──→ +3.3VDC
U303 AMS1117-5.0 out ──→ U304 pin 6 ────┘   (dead at 0 V with no +12VDC)
```

With no 12V the mux simply selects VBUS, and the AP7361C's dropout at the ~50 mA
panel load is tens of mV — **this is the case the AP7361C swap was made for**
(the old AMS1117-3.3's ~1.1 V dropout was the USB-VBUS-only weak point). The
LM66200 blocks reverse conduction, so nothing back-feeds the host.

**No inrush concern:** `C308` (22 µF tantalum) sits on the AMS1117 *input* side
of the mux, so USB never charges it. VBUS only sees `C304`, `C301`, `C303` and
`C309` — well inside the 100 mA pre-enumeration budget.

### If a board does not enumerate

The suspect list is short, because USB needs the PLL and the PLL needs the crystal:

1. **`X301` / `C311` / `C313` / `R302`** — no 12 MHz XOSC means *completely* dead
   USB, not degraded USB. Check this first. `R302` (1 k on `XOUT`) is present and
   correct — the classic RP2040 omission is not one of our failure modes.
2. **Rails** — probe `TP302` (+5VDC), then +3.3VDC. `J305` is the all-TH GCT part,
   so VBUS joints are visually inspectable.
3. **`RUN` held low** — `R307` is the 10 k pull-up; **`TP303` is a `RUN` test
   point**, so it both probes and (momentarily shorted to GND) resets the chip.
4. **`USB_D±`** — `R305`/`R306` 27 Ω, `U305` USBLC6 on the connector side. A cold
   joint on the QFN-56 USB pads.

⚠ **BOOTSEL enumerating does NOT prove the flash is good.** The bootrom enters
BOOTSEL *because* it could not read a valid image, so a dead, mis-oriented or
unsoldered `U307` is indistinguishable from a blank one. `U307` is only validated
when the first `picotool load` write-verifies **and** the board runs the program.
Do not tick the flash off the list at enumeration.

macOS: use `picotool load`, never the `RPI-RP2` drag-drop. `picotool info`
segfaults on RP2040.

### Do not drive LEDs on USB-only power

`U301` runs from `+5VDC`, which is live from VBUS, so it will drive
`LED_DATA_5V` into WS2815 `DIN` pins whose 12V rail is dead — forward-biasing
their input protection. `R301` (330 Ω) limits this to ~10 mA so it is not
destructive, but it is not a valid LED test either. Only applies to a mated
brain; standalone, the net terminates at `J303` pin 5.

**The board detects this case for free.** `SENSE_12V` is the `R313`/`R314`
100 k/33 k divider into **GPIO17, a digital pin — not an ADC**. 12V present reads
12 × 33/133 ≈ **2.98 V = logic high**; USB-only reads 0. A one-line guard
(`if (!gpio_get(17)) skip_led_output();`) is that pin's intended job.

### `RS485_DE` at rest: closed 2026-09-07

`RS485_DE` has no external pull resistor (the net is only GPIO4, `U308` pins 2/3
and `TP306`), so DE/R̅E̅ is undriven until firmware configures the pin, and for
the whole time a board sits in BOOTSEL. The RP2040's reset-default pull-down
holds it: **`TP306` measured 35 mV** on a blank brain in BOOTSEL. The THVD1450
parks in receive, so an unflashed panel can't jam the bus. This is a property of
the design and the silicon, so it isn't repeated per board, and the rev-2
external pull-down was dropped.

## Verification status

Re-run 2026-08-04 and again 2026-08-18 after the order-day edits (same counts),
from clean project copies with
`kicad-cli pcb drc --severity-all --schematic-parity`:

| check | result |
|---|---|
| DRC violations | **0** |
| unconnected items | **19** — the permanent board-to-board gap floor |
| ERC | **0** |
| PCB ↔ exported netlist | **0** mismatches / 0 missing / 0 extra across 577 pads |
| tightest copper | 0.1266 mm |
| minimum via drill | 0.30 mm |

### Do not re-flag these

- **19 unconnected** is KiCad drawing airwires across the mating gap. There is no
  way to tell it "these mate mechanically." Each was individually confirmed to be a
  genuine crossing net.
- **90 schematic-parity items from `kicad-cli`** are a **CLI-only artifact — the
  GUI reports none.** Net-name disagreements, not topology; the exported netlist
  agrees with the PCB pad-for-pad and gerbers carry no net names.
- **`USB+`/`USB−` at 0.1375 mm** is a differential pair, deliberately coupled.
- **`QSPI_SS` mixed 0.15/0.20 mm widths** — deliberate; the 0.20 mm segments run
  where nothing is nearby. QSPI is length-matched to a 20 nm spread; **do not
  disturb it.** Width changes are safe, they do not affect length.
- **`starved_thermal` on J211 pad 5** appears in any clearance sweep ≥0.10 mm. It
  is a sweep artifact, not real at the actual rule.
- **Per-LED pin-1 100nF caps** are vendor-sanctioned; two Worldsemi documents
  bracketing our WS2815B-V1 revision give pin 1 as "VCC … Suspended or connected
  with a filter capacitor to GROUND." No per-LED VDD decoupling is wanted.

### Known open items

- **POFV and the 0.60 mm vias:** answered at order. Only the 0.30 mm drill vias
  were asked to be filled; the 44 at 0.60 mm are power distribution and none sit
  in a pad (`docs/rev1-order/ORDER_NOTES.md` §1).
- **`.kicad_dru` silently overrides the netclass clearance.** The rule "Minimum
  Trace Width and Spacing" carries `(constraint clearance (min 0.09mm))`, and in
  KiCad a custom rule **replaces** the netclass value rather than acting as a floor
  — so it overrode Default's 0.2 mm board-wide. Proven by deleting just that
  constraint: 77 violations appear, actual range 0.0900–0.1993 mm. If 0.2 mm should
  ever be enforced, drop the `clearance` constraint from that rule (keep
  `track_width`) and let the netclass govern.

## Layout traps worth remembering

- **Any 45° fanout at 0.4 mm pitch lands near 0.09 mm clearance.** The five QSPI
  nets escape the RP2040 at 0.400 mm pitch, which becomes 0.400·cos45° = 0.283 mm
  perpendicular on a diagonal; at 0.20 mm width that leaves ~0.083 mm. Fixed by
  narrowing QSPI to 0.15 mm **uniformly** — a local neck adds two impedance steps,
  uniform adds none.
- **Sub-micron track-endpoint/via-centre misalignment hides under zero-length track
  fragments.** Deleting the fragments exposes it as fresh DRC violations. The fix is
  to centre the endpoints, not restore the fragments — and KiCad's "Cleanup Tracks &
  Vias" does **not** fix off-centre endpoints; it reports nothing to do.
- **Of 109 sub-0.15 mm tracks, 102 were mid-route jogs with copper on both
  endpoints.** Deleting those breaks real routes. Do not "clean up" short tracks by
  length alone.
- Fab **resolution** is a non-issue: gerbers are `%FSLAX46Y46*%` = 1 nm, drill files
  metric 3-decimal = 1 µm, 50–100× finer than fab positional tolerance. Fine
  placement cannot produce an unmanufacturable file. **Feature size** is the thing
  that can bite.
