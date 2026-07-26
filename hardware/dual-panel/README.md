# Dual-board panel — exploration (NOT the production design)

**Status: experiment, parallel to the real thing.** `hardware/panel-pcb/` remains
the design that gets ordered. This folder exists to iterate on splitting the
panel into two boards and see whether it ends up looking better. Nothing here
is committed to, and neither board needs to stay in sync with `panel-pcb/`
while it's being played with.

```
hardware/dual-panel/
  3dmodels/      shared by both copies (one copy of 6.5MB, not two)
  display-pcb/   the LED + IO carrier — sits where panel-pcb sits today
  brain-pcb/     the MCU/power/comms board — sockets underneath, in the frame cavity
```

**Both are full copies of `panel-pcb` as of 2026-07-26**, taken so you can pick
each apart rather than build up from nothing. Each is a self-contained KiCad
project with its own `.kicad_pro/.kicad_sch/.kicad_pcb/.kicad_sym/.kicad_dru`,
its own `<name>.pretty`, and its own lib tables. All 397 schematic + 186 board
`panel-pcb:` library references were rewritten to the new project name, so
nothing points back at the production design. 3D models are the one shared
thing — footprints reference `${KIPRJMOD}/../3dmodels/` so the 6.5MB of STEP
files isn't duplicated twice more.

Both copies verify **DRC 0 / unconnected 0 / schematic-parity 0 / ERC 0**, i.e.
they open as clean projects, not half-renamed ones.

**Metadata already changed:** project + sheet names, lib nicknames and URIs,
title block (`KrakenPad Display/Brain PCB (dual-board exploration)`), rev `0.1`,
and comment 1 noting the copy's provenance.

**Metadata deliberately left as-is**, because it's role-specific and yours to
decide as each board takes shape: the 4-layer stackup and its physical layer
settings, net classes (`Default`/`Power12V`), the `.kicad_dru` JLC rules (right
for 4-layer — revisit if the carrier goes 2-layer or the brain goes 6), the
127×127mm board outline with its connector tabs, and `used_designators`.

## The idea

Split the current 127×127mm panel into a **display/IO carrier** (25 WS2815s,
every connector, the switches) and a **brain** (RP2040, flash, transceiver,
regulators, level shifter) that plugs into the carrier's underside. The frame has
a cavity under the PCB (photographed 2026-07-26) roughly **95–105mm square** —
scaled off the 127mm PCB footprint, so caliper it before trusting it — with the
four PCB standoff bosses at its corners. The brain's dense cluster needs about
60×50mm, so **plan area is not the constraint**.

**Why bother** (user's motivations, in order):

1. **Layout satisfaction / routing separation.** On one board the LED field and
   the brain compete for the same planes — hence three power pours on In2 and
   crossing-trace anxiety. Split, each board gets a stackup suited to its job.
2. **ESD and protection.** Clamp at the ports on the carrier; the brain sits
   behind the connector.
3. **Fix the brain without re-fabbing the panels.** Not about upgrades — about a
   routing or hardware bug found at bring-up. Re-spinning a small brain board is
   much cheaper in money and materials than re-spinning 20 carriers.

## Why this beats the alternatives (analysis 2026-07-26)

Three ways to get the routing separation, compared on outcome, money, and
permanent consequences:

| | Board cost vs today | Routing outcome | Permanent consequences | Fixes brain alone |
|---|---|---|---|---|
| Brain on the bottom, one board | unchanged | good plan separation, still shares 4 layers | SWD/BOOTSEL buried | no |
| 6-layer, one board | **+$170–250** | clean planes, brain still interleaved with LEDs | none | no |
| **This split** | roughly neutral, **+~$120–140** overhead | **best available** | connector in the impact path; interface spec to get right | **yes** |

- **6-layer is the weakest option**: JLCPCB lists 4-layer 100×100mm from ~$7 vs
  6-layer from ~$35 (≈5×, since 4-layer is heavily subsidised). Our qty-5 quote
  put the PCB side at $40.95 ($25 eng fee + $11.40 boards + $3.51 via plating),
  so 6-layer at qty 20 plausibly lands $250–350 — the most expensive route, and
  it delivers neither the best routing environment nor fault isolation.
- **The split's board cost is roughly neutral** because 6-layer cost scales with
  area: the brain at ~30cm² is ~1/5 the panel's ~161cm², so **6-layer on the
  brain costs about what 4-layer costs on the whole panel today**, while the
  carrier plausibly drops to **2 layers** (LED chain + 12V + GND), which is
  *cheaper* than now. The real premium is the second PCBA order's per-order
  lines: **$51.12 setup + $16.42 stencil + $4.93 storage + $25 eng fee ≈ $97**,
  plus ~$20–40 for 20 header/socket sets. Feeder and component fees split across
  the two orders rather than doubling.
- Prices are point-in-time — verify live, per the warning in `docs/BOM.md`.

## Decisions already made

- **Brain goes underneath**, in the frame cavity. The LEDs own the top surface;
  anything mounted above them shadows the play surface.
- **USB-C stays on the carrier**, at the board edge where it is today. Routing
  D+/D− across a board-to-board connector is a non-issue at Full Speed (12 Mbps,
  ~4ns edges) and it makes ESD *better*: keep J1, the 5.1kΩ CC pull-downs, and
  the USBLC6 array on the carrier so a plug event clamps **upstream of the
  connector** and never crosses into the brain. The 27Ω series resistors go on
  the brain, near the RP2040.
- **SWD and BOOTSEL also stay on the carrier**, so nothing you touch during
  bring-up is buried. SWD is trivially fine (a few MHz). BOOTSEL works because
  our existing circuit already isolates it:
  `SW2 → USB_BOOT → R7 (1k) → QSPI_SS → U1.56 + U3.1 (flash CS) + R12 (10k)`.
  The fast net is `QSPI_SS`, which stays short on the brain between MCU and
  flash; **`USB_BOOT` sits behind the 1kΩ and carries no fast edges**, so
  crossing the connector with it adds no load to chip-select. That resistor is
  exactly what the reference design puts there.
- **Screws take all mechanical load** — two M3 into the cavity's corner bosses.
  The socket carries none, reducing the impact-path worry to ordinary solder-joint
  reliability.
- **Protection lives at the ports on the carrier**, same philosophy as USB: D30
  (SMAJ5.0A) and R17 stay next to the INT terminal, not on the brain.

## Open gates

1. **Cavity depth + standoff height** — the only hard physical gate. Need: how
   far the PCB underside sits above the frame's inner floor, and how much deeper
   the recess goes. Stack is socket height + 1.6mm brain PCB + its tallest part
   (~3mm, the AMS1117 or the electrolytic) + clearance.
2. **The carrier's underside is already populated** in the current design — 20
   parts spread over X 82–203, Y 46–110mm, tallest being **D30 (SMA, ~2.3mm)**.
   Either the socket stands off >~3mm, or those parts move to the top on the
   carrier redesign (there's room now that the brain is gone).
3. **Can the carrier really be 2 layers?** LED chain + 12V distribution + a GND
   pour, with the RS-485 pair and four FSR lines crossing to the connector. Looks
   plausible; wants a real routing attempt to confirm, since the GND pour gets
   chopped by the brain footprint and the remaining bottom-side parts.
4. **Interface spec** — the one mistake that would force changes on *both*
   boards. See below.

## Board-to-board interface

20 signal/power nets cross. Recommended connector: **2×15 (30-pin) 2.54mm**
header/socket — 20 nets + 6 grounds + 4 spares.

| Group | Nets | Notes |
|---|---|---|
| Power | `+12VDC`, `GND` ×6 | brain logic only (~50mA); **LED power never crosses** — 12V arrives on the carrier and the LEDs are on the carrier |
| Reference | `+3.3VDC` | worth bringing over for probing and any future carrier-side logic |
| USB | `USB_D+`, `USB_D-` | adjacent pins, GND either side |
| LED | `LED_DATA` | post-shifter, 5V logic |
| Bus | `RS485_A`, `RS485_B` | keep as a pair; termination (SW3 + 120Ω R2) stays on the carrier |
| Input | `INT_OUT` | open-drain to the master; TVS + series R stay at the carrier's terminal |
| Analog | `FSR_N`, `FSR_E`, `FSR_S`, `FSR_W` | **highest-impedance nets crossing** — keep away from `LED_DATA`, flank with GND |
| Config | `DIP_ID0..3`, `TERM_SENSE` | switches live on the carrier |
| Debug | `SWDIO`, `SWCLK`, `USB_BOOT` | see the BOOTSEL note above |

Placement rules that fall out: the USB pair and the FSR group want to be at
opposite ends with grounds between, and `LED_DATA` (the only fast-edged
single-ended signal) should not sit adjacent to any FSR line.

## Functional split of the current BOM

Derived from `hardware/panel-pcb/` by net membership, so it starts from the real
design rather than guesswork. Block-level assignment is unambiguous; the
judgment calls are called out.

**Carrier (display-pcb):** the 25 WS2815s + their 25 pin-1 caps · FSR front end
(J3/J4/J6/J7, dividers R8–R11, caps C16–C19) · power entry J5/J11 + the 470µF
bulk C51 · RS-485 connectors J8/J10 + termination SW3 + R2 · INT terminal J9 +
D30 + R17 · panel-ID DIP SW1 · USB-C J1 + R13/R14 + U7 · BOOTSEL SW2 · SWD
header J2 · debug LED D1 (pointless on a buried board) · board outline, mounting
holes, logo.

**Brain (brain-pcb):** U1 RP2040 + its decoupling · U3 flash + R12 + R7 · X1 +
C12/C14 + R6 · U2 THVD1429 (+ DE) · U4 SN74AHCT125 level shifter · U5
AMS1117-5.0 · U6 AP7361C-3.3 · U8 LM66200 + D12/D23 (DNP rescue) and their caps ·
R3/R4 27Ω USB series · 12V sense divider R18/R19/C53 + D29 clamp.

**Judgment calls left open:** `R16` (LED-data series resistor — either next to U4
on the brain or next to the first LED on the carrier); which test points go where
(rail probes belong on the brain, bus/12V probes on the carrier); whether the
carrier keeps any bottom-side parts at all once the brain vacates the top.

## When KiCad projects get created

Not yet — the folders exist so this can be iterated on without disturbing the
production design. When starting: copy `hardware/panel-pcb/panel-pcb.kicad_dru`
so the JLC rules come along, and reuse the existing custom footprints and symbols
from `panel-pcb.pretty/` and `panel-pcb.kicad_sym` (WS2815 PLCC6, the USB-C
receptacle, SW_SS22E01L5, the terminal block, AP7361C, LM66200) rather than
rebuilding them.
