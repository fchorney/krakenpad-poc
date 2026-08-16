# Dual-Panel (carrier + brain) — as-built reference

**This is the panel design that gets built.** The single-board `hardware/panel-pcb`
was retired on 2026-08-04 when the two-board split won; it is not in the working
tree any more, only in git history (last present at `1b41d1c`). The measurements
that decided it are archived at `docs/archive/DESIGN_COMPARISON.md`.

Everything below is read out of the current KiCad files — the schematic netlist
(`kicad-cli sch export netlist --format kicadxml`) and `dual-panel.kicad_pcb` —
not carried forward from plans. Last derived **2026-08-04**.

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
The prototype firmware in `firmware/panel/` predates both boards and uses
breadboard pin numbers; it is not a reference for either.

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

- **4 layers, JLC 4-layer standard** (0.3/0.45 via class): L1 components + signals,
  **In1 and In2 are BOTH solid GND, never split** (layer names `GND1.Cu`/`GND2.Cu`);
  B.Cu spillover + GND pour. There is **no 12 V plane** — 12 V is a routed trace
  tree (2 mm trunk, ≥0.5 mm LED branches, verified by widest-path analysis
  2026-08-06), and the 3.3 V/5 V islands are small F.Cu pours at the regulators,
  not inner-layer pours. An earlier draft of this file described In2 as power
  pours; that was never the as-built board. Chosen for analog noise —
  high-impedance FSR lines sharing a board with 25 switching LEDs — not for
  routing density.
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
B.Silkscreen, personal logo as exposed GND copper via the mask-opening technique.
Project logo pending artwork.

## Passthrough variant

A carrier populated with only connectors + the termination switch + R201 is
electrically a valid passthrough: power and RS-485 are bused connector-to-connector,
and INT floats safe-HIGH at the master. Mechanism is KiCad per-symbol DNP → a second
BOM/CPL export, so it is **one bare-PCB SKU with two assembly variants**. Nothing is
built yet — see `docs/MODULAR_PANEL_COUNT.md`.

## Accepted-for-rev-A risks

Bring-up measurements, **not order blockers** — these were accepted deliberately:
U303 thermal at real load (~50 mA), +5 V rail margin, INT-into-a-dead-panel (the
master-side 10 k pull-up is deliberately *not* stiff for exactly this case),
hot-plug/USB-attach behaviour, SI asymmetries, and the FSR runs on B.Cu.

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
| test points | TP201–TP218 — 12V ×3, LED data 5V, rest GND |

Protection lives at the ports on the carrier, so a plug or ESD event clamps
upstream and never crosses the interface.

## Brain (3xx) — 52 parts

| block | designators |
|---|---|
| MCU | **U306** RP2040 + decoupling **C309–C325**, **X301** 12 MHz + **C311/C313** 15pF |
| flash | **U307** W25Q32JV 4MB + **R307** 10k CS pull-up, **R310** 10k |
| RS-485 | **U308** THVD1429 (20 Mbps grade) |
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

Three M3 screws take all mechanical load; the sockets carry none.

| item | mm |
|---|---|
| male header plastic body | 2.45 |
| female socket body | 8.30 |
| **board-to-board separation** | **10.75** |
| brain PCB | 1.60 |
| M3 socket-cap head, brain underside | 3.00 |
| **total below the carrier underside** | **15.35** |
| available (6 mm to frame floor + 14 mm inside the opening) | 20.00 |
| **spare** | **4.65** |

- **Spacer: 11 mm M3, deliberately erring tall.** A 10 mm spacer would be 0.75 mm
  short and the screws would close that gap by flexing the boards, putting exactly
  the load on the connectors that the spacer exists to remove. **Never omit it.**
- **⚠ The table above predates the sourced parts, and 11 mm no longer clears.**
  The connectors chosen 2026-08-16 (LCSC **C5383116** header, **C7509515** socket)
  measure **2.54 mm** and **8.50 mm** of plastic against the 2.45/8.30 assumed
  here — so board-to-board is **11.04 mm**, and the plastics now meet 0.04 mm
  *before* an 11 mm spacer bottoms out. That is the exact failure mode the
  spacer exists to prevent, merely small.
  **Buy 12 mm M3 spacers as well and measure the parts on arrival.** At 12 mm
  the gap is 0.96 mm, pin engagement is still ~5 mm, and total depth below the
  carrier becomes 16.6 mm against 20.0 mm available — 3.4 mm spare, so the
  cavity fit is unaffected. Spacers cost pennies; buy both and settle it with
  calipers rather than by arithmetic on datasheet nominals.
- **The spacer sets separation, so socket body height only buys pin engagement.**
  Worth stating plainly, because it is counterintuitive: as long as the spacer is
  *taller* than header plastic + socket plastic, the plastics never touch, the
  connectors carry no load, and a shorter socket simply means the 6 mm pin goes
  less far in. A shorter socket cannot make the pin "stick out".

  | socket | body | stack | spacer | air gap | **pin engagement** | load on connectors |
  |---|---|---|---|---|---|---|
  | C7509515 | 8.50 | 11.04 | 11 mm | −0.04 | 6.00 (full) | ⚠ yes, slight |
  | **C7509515** | **8.50** | **11.04** | **12 mm** | **0.96** | **5.04** | **none ✓** |
  | C55218878 | 5.70 | 8.24 | 11 mm | 2.76 | 3.24 | none ✓ |

  The spacer is a bought part and freely chosen, so a shorter socket paired with
  a shorter spacer is equally valid on load — 5.7 mm socket + 9 mm spacer gives
  5.24 mm engagement, as good as 8.5 mm + 12 mm. **Height alone does not decide
  this.** What decides it is the next bullet.

- **What is in the gap, and why it does not constrain the spacer.** The brain's
  **F.Cu faces the carrier**, and that is the side carrying `J305` (USB-C) and
  both SOT-223 LDOs — verified from `dual-panel.kicad_pcb`: carrier headers on
  B.Cu at x≈91–138, brain sockets/USB-C/LDOs on F.Cu at x≈193–247. The tallest
  of them is the GCT USB4085 at **3.16 mm** (datasheet section view), so
  component clearance is satisfied by every spacer under discussion.

  **USB-C cable clearance is explicitly NOT a requirement.** `CLAUDE.md`: the
  port "is only used on the bench with the panel top off, so cable clearance is
  irrelevant". The in-situ access path is the **3-pin SWD header, which is on the
  CARRIER's F.Cu** (x≈113) and therefore reachable without separating the boards.
  Nothing needs to reach into the inter-board gap.
  *(A 2026-08-16 analysis argued the gap was a USB-cable budget and used that to
  justify the 8.5 mm socket. It was wrong — recorded so it is not re-derived.)*

- **Socket choice is therefore open, and close to free.** Both give ~5 mm
  engagement with a matched spacer and neither strains the depth budget:

  | socket | spacer | pin engagement | note |
  |---|---|---|---|
  | C7509515 (8.5) | 12 mm | 5.04 mm | satisfies the pin-shorter-than-bore rule |
  | C7509515 (8.5) | 11 mm | 6.00 mm | ⚠ connectors take load — do not use |
  | C55218878 (5.7) | 9–10 mm | ~5.2 mm | lower profile, more cavity spare |

  The only residual argument for the 8.5 mm part is that a 6 mm pin fits inside
  its bore, so it can never be *pin*-loaded even if assembled with a wrong-length
  spacer. Mildly more forgiving; not a strong reason.
- **Separation is set by the two plastics meeting, not by pins bottoming out** —
  which only holds while the mating pin is shorter than the socket is deep. Source
  headers as **6.0 mm mating pin with a ≥3.0 mm solder tail**; some "short" headers
  trim the tail instead, leaving nothing to solder through the carrier.
- **Everything deep must sit inside the cavity opening**, because the stack is
  10.75 mm at the connectors while the frame floor is only 6 mm below the carrier.
  Verified: all four interface connectors and all three screws clear the opening on
  every side. Re-check with `fit-test/gen_cavity_template.py` after any outline or
  placement change.
- Carrier underside carries only D201, R202, R203 and the four headers.

## Verification status

Re-run 2026-08-04 from clean project copies with
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

- **44 vias at 0.60 mm drill exceed JLC's 0.5 mm epoxy-fill limit.** All
  power-distribution, none in a pad, so unfilled is electrically fine — but POFV is
  normally applied board-wide. **Ask JLC how they handle a board mixing fillable and
  non-fillable vias** rather than eating an engineering query.
- ~~**The 8 interface connectors (J210–J213, J301–J304) carry no LCSC part
  number.**~~ **CLOSED 2026-08-16:** header **C5383116** (HanElectricity
  2541WV-08P, 6 mm pin / 3 mm tail) and socket **C7509515** (CONNFLY
  DS1023-1x8SF11, 8.5 mm body), 50 of each ordered against a need of 36.
  **They bring a spacer consequence — see the mechanical stack above.**
  Original note retained for the reasoning:
  **Type is NOT unknown** — read out of `dual-panel.kicad_pcb` 2026-08-16:
  J210–J213 are `PinHeader_1x08_P2.54mm_Vertical` (carrier, B.Cu) and J301–J304
  are `PinSocket_1x08_P2.54mm_Vertical` (brain). Ordinary 2.54 mm headers and
  sockets, four of each per panel, 32 pins per side. The *only* non-commodity
  spec is the **6.0 mm mating pin with ≥3.0 mm solder tail** from the mechanical
  stack above, which most "2.54 mm header" listings state neither half of.
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
