# Dual-board panel — exploration (NOT the production design)

**Status: experiment, parallel to the real thing.** `hardware/panel-pcb/` remains
the design that gets ordered. This folder exists to iterate on splitting the
panel into two boards and see whether it ends up looking better. Nothing here
is committed to, and neither board needs to stay in sync with `panel-pcb/`
while it's being played with.

## Project structure — one project, two sheets, one board file (2026-07-27)

```
hardware/dual-panel/
  dual-panel.kicad_pro
  dual-panel.kicad_sch   root sheet — two sheet symbols, nothing else
    carrier.kicad_sch    the LED + IO carrier — sits where panel-pcb sits today
    brain.kicad_sch      the MCU/power/comms board — sockets underneath, in the cavity
  dual-panel.kicad_pcb   BOTH board outlines live here (one panel, V-cut between them)
  dual-panel.kicad_sym / dual-panel.pretty / dual-panel.kicad_dru
  3dmodels/              referenced as ${KIPRJMOD}/3dmodels/
```

**Why one project rather than two.** KiCad has no multi-board feature, but a
project is already *one hierarchy → one netlist → one `.kicad_pcb`*, and two
Edge.Cuts outlines in a single board file is the supported way to panelise. Two
things fall out of that:

- **ERC checks the board-to-board interface for you.** With two separate
  projects, every interface change means editing two schematics and hand-checking
  that they agree — and the interface spec is precisely the mistake that would
  force *both* boards to be re-spun.
- **One JLC engineering fee and one PCBA setup instead of two** — that is exactly
  the ~$97/order overhead this document calls the split's only real premium.

**Baseline decided 2026-07-27: both boards at 4 layers, on one panel, in one
order.** Panelising requires a shared stackup (layer count, thickness, copper
weight, `.kicad_dru`), and the money says don't break that — see
[Why not split the stackups](#why-not-split-the-stackups-2026-07-27) below. A
2-layer carrier would save ~$25–35 of fab and cost ~$137 in a second order.

Only a **6-layer brain** would genuinely force two runs, and the brain is already
4-layer-proven (see the end of [Order of work](#order-of-work)). If that ever
happens, the fallback is two projects sharing one hierarchical sheet file for the
interface — KiCad allows a `.kicad_sch` outside the project dir, referenced by
both, so the pinout is still edited once.

**Known wart to plan for:** the ratsnest will draw airwires between the mating
socket and header, and DRC will report N `unconnected_items` across the gap.
There is no way to tell KiCad "these mate mechanically." Accept a fixed,
enumerable exclusion list — giving each side distinct net names would kill the
airwires but throw away the cross-checking that motivated this structure.

### What was seeded, and how

**Both sheets are full copies of `panel-pcb` rev 1.0** (taken 2026-07-26,
restructured 2026-07-27), so each is **stripped down to its role** rather than
built up from nothing. Consequences to expect on first open:

- **~134 duplicate reference designators.** Both sheets carry every part, so
  KiCad reports annotation errors and `kicad-cli sch export netlist` refuses to
  run until stripping is done. This is the cost of the seed-and-strip approach
  and it disappears as parts are deleted.
- ERC currently reports **14 errors**, all of the same kind: global nets
  (`GND`, `+3V3`, `QSPI_SS`, …) now driven from two identical sheets, so
  power-output and output pins collide with their own twin. Not real faults.
- Every UUID in `brain.kicad_sch` was regenerated so the two sheets are distinct
  instances, not the same sheet twice.

**`dual-panel.kicad_pcb` is the panel layout, placed and routed, kept as-is.**
All 144 footprint sheet-paths were rewritten to the **carrier** sheet, since the
existing 127×127 board *is* the carrier. As each sheet is stripped,
update-PCB-from-schematic drops the orphans; then add the brain's Edge.Cuts
outline alongside and pull its footprints into it. Board DRC is **0 violations /
0 unconnected / 0 parity** in this seeded state.

**Left role-specific on purpose:** net classes (`Default`/`Power12V`), the
127×127mm outline with its connector tabs, and `used_designators`. The 4-layer
stackup, its physical layer settings and the JLC `.kicad_dru` are now the
**shared** baseline for both boards and should stay that way.

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
| **This split** | roughly neutral; overhead now **~$0** on one panel (was +$120–140 as two orders) | **best available** | connector in the impact path; interface spec to get right | **yes** |

> Updated 2026-07-27: the "+$120–140 overhead" row assumed two separate PCBA
> orders. The single-project / one-panel structure deletes that line entirely,
> which makes the split cheaper than it looked here.

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

## Why not split the stackups (2026-07-27)

Tempting idea: carrier at 2 layers, brain at 4, since the carrier looks like it
might route single-sided. It costs more, not less, and it isn't close.

**A 2-layer carrier saves only the bare-fab delta on one board.** The qty-5 quote
put the *entire* board cost for the 127×127 4-layer panel at **$14.91** ($11.40
boards + $3.51 via plating) — about $3/board. Scaled to qty 20 that's ~$55–60 of
4-layer fab, so **even if 2-layer PCBs were free the saving would cap at ~$55**.
At a realistic 40–60% discount it's **~$25–35**.

**A second run costs the per-order overhead, which is fixed.** $25 eng fee +
$51.12 PCBA setup + $16.42 stencil + $4.93 storage = **$97.47**, plus a second
~$40 shipment ⇒ **~$137**.

| | One 4L panel (both boards) | 2L carrier + 4L brain |
|---|---|---|
| Bare fab | ~$55–60 | ~$25–30 carrier + ~$15 brain |
| Order overhead | $97 × 1 | $97 × 2 |
| Shipping | ~$40 | ~$80 |
| **Net** | — | **~$110–140 worse** |

There is no quantity where this flips: the saving scales with board count, the
penalty doesn't. Both boards also still need PCBA regardless — WS2815 is MSL 5a
with a 24h floor life, so hand-assembling the carrier was never on the table.

**What you get for the ~$25–35 you'd have "saved":** two extra copper layers on
the noisiest board in the system — an uninterrupted GND plane plus a power plane,
under a 25-LED field with four high-impedance FSR lines crossing it. That is
motivation #1 (routing separation) delivered *better* than the 2-layer version
would have, and it's worth paying for on its own merits.

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

**23 nets cross** — 19 signals + 3 rails + GND. Derived from the `panel-pcb`
netlist by assigning every part a side and taking the nets with nodes on both
(2026-07-27); an earlier hand-written version of this table said 20 and was
missing `VBUS` and `DEBUG_LED`.

Connector: **two independent 1×17 2.54mm** header/socket pairs — **J12** and
**J13**, 34 pins total, wired 2026-07-27. (A single 2×17 was the earlier plan;
two rows was chosen instead and it buys real isolation — see below.)

| | Carries | GND | Spare |
|---|---|---|---|
| **J12** | +3.3VDC, FSR ×4, INT_OUT, DIP_ID0–3, DEBUG_LED, SWCLK, SWDIO, VBUS | 3 | 0 |
| **J13** | +12VDC, USB_D±, RS485±, TERM_SENSE, LED data, USB_BOOT | 6 | 2 |

Splitting the interface this way puts **the whole FSR group on one connector and
every fast signal on the other** — better isolation than any single-connector
pin ordering could achieve, since row-to-row spacing in a 2×17 is 2.54mm, the
same as pin-to-pin.

Two things two connectors cost you, both handled at layout:

- **180° rotation hazard.** Two identical 1×17 rows can mate rotated if they sit
  symmetric about the brain's centerline — which would land J12.1 `+3.3VDC` onto
  J13.17 `+12VDC` (12V into the 3.3V rail) and VBUS into GND. **Place the two
  rows at unequal distances from the centerline** so a rotated brain physically
  won't seat. Asymmetric M3 positions help only if they're actually asymmetric.
- **Mating tolerance.** Two connectors must align in X, Y *and* rotation at once,
  where one 2×17 only had to align as a unit. Assemble both headers into their
  sockets, mate the boards, then solder the second connector's pins with the
  stack held together.

| Group | Nets | Notes |
|---|---|---|
| Power | `+12VDC`, `GND` ×6 | brain logic only (~50mA); **LED power never crosses** — 12V arrives on the carrier and the LEDs are on the carrier |
| Rail | `+3.3VDC` | **load-bearing, not a probe convenience** — J3/J4/J6/J7 sit on it, so this is the **FSR excitation rail** carrying all four dividers' current, and the reference the ADC readings are ratiometric to. Size and decouple it as a real supply pin |
| Rail | `VBUS` | J1's VBUS has to reach **U8** (LM66200 power-OR) on the brain. Unavoidable short of moving the whole 5V OR to the carrier, which only swaps it for a `+5VDC` crossing |
| USB | `USB_D+`, `USB_D-` | adjacent pins, GND either side |
| LED | `/LD0` | post-shifter **and post-R16** — the connector carries an already-damped edge |
| Bus | `RS485+`, `RS485-` | keep as a pair; termination (SW3 + 120Ω R2) stays on the carrier |
| Input | `INT_OUT` | open-drain to the master; TVS + series R stay at the carrier's terminal |
| Analog | `FSR_North/East/South/West` | **raw sensor lines** now that the dividers moved to the brain — the highest-impedance nets crossing. Keep away from `/LD0`, flank with GND |
| Config | `DIP_ID0..3`, `TERM_SENSE` | switches live on the carrier |
| Debug | `SWDIO`, `SWCLK`, `USB_BOOT`, `DEBUG_LED` | see the BOOTSEL note above; `DEBUG_LED` crosses because D1+R15 stay on the carrier |

Placement rules that fall out: the USB pair and the FSR group want to be at
opposite ends with grounds between, and `/LD0` (the only fast-edged
single-ended signal) should not sit adjacent to any FSR line.

## Functional split of the current BOM

Enumerated from the `panel-pcb` netlist, so it starts from the real design
rather than guesswork. **Carrier 76 parts, brain 52.**

### Carrier (`carrier.kicad_sch`) — 76 parts

| Block | Designators |
|---|---|
| LED field | 25× WS2815: **D2–D11, D13–D22, D24–D28** |
| LED pin-1 caps | 25× 100nF: **C22–C36, C39–C43, C45–C49** |
| FSR connectors | **J3** (N), **J4** (E), **J6** (S), **J7** (W) — connectors only; dividers and caps are on the brain |
| Power entry/exit | **J5** (12V_IN), **J11** (12V_OUT), **C51** (470µF bulk) |
| RS-485 | **J8** (IN), **J10** (OUT), **SW3** (termination), **R2** (120Ω) · shield network **R20** (1M) ‖ **C57** (100nF) |
| INT | **J9** (terminal), **D30** (SMAJ5.0A), **R17** (100Ω) |
| Panel ID | **SW1** (4-pos DIP) |
| USB-C | **J1**, **R13**/**R14** (5.1k CC), **U7** (USBLC6-2SC6), **C54** (VBUS cap) |
| BOOTSEL | **SW2** |
| SWD | **J2** |
| Debug LED | **D1** + **R15** (1k) — stays on the carrier; a buried brain's LED is invisible |

Plus board outline, mounting holes, logo, and the probe-side test points:
**TP2/TP3/TP12** (GND), **TP8** (+12V), **TP9** (`/LD0`), **TP13/TP14** (RS485±).

### Brain (`brain.kicad_sch`) — 52 parts

U1 RP2040 + decoupling (**C1–C11, C13, C15, C20**, **R1** RUN pull-up, **R5**
ADC_AVDD filter) · **U3** flash + **R12** + **R7** · **X1** + **C12/C14** + **R6** ·
**U2** THVD1429 · **U4** SN74AHCT125 + **R16** · **U5** AMS1117-5.0 · **U6**
AP7361C-3.3 · **U8** LM66200 + **D12/D23** (DNP rescue) + **C21, C37, C38, C44,
C50, C52, C55, C56** · **R3/R4** 27Ω USB series · 12V sense **R18/R19/C53** +
**D29** clamp · **FSR dividers R8–R11 + caps C16–C19**.

### Placement calls settled 2026-07-27

- **`C16–C19` → brain, at GPIO26–29.** Not a preference: the RP2040's SAR ADC
  dumps its internal sampling cap onto the pin each conversion and needs that
  charge back from a low-inductance local reservoir. Across a 2.54mm connector
  hop you've put inductance in series with exactly the thing that must respond in
  nanoseconds.
- **`R8–R11` → brain.** Electrically near-neutral — the resistor's position on
  the net barely changes the impedance-to-ground at any point, since the trace
  resistance between them is negligible. What decides it: **the divider ratio is
  a tuning parameter** (10k vs 12k, or a different ratio once real FSRs are
  characterized under real feet), so it belongs on the board that can be re-spun
  alone. That is motivation #3.
  - Mapping is **not** sequential: N→**R11**/C16, E→**R9**/C18, S→**R10**/C17,
    W→**R8**/C19.
  - The 10k×10nF 1.6kHz corner is not a latency problem: pressing collapses the
    FSR to ~500Ω, so source impedance during the press transition is ~475Ω and
    τ ≈ **5µs**. The slow tail is on *release*, not the latency-critical
    direction.
- **`R16` (330Ω LED data) → brain, at U4's output.** Series termination works at
  the source; this also means the connector carries an already-damped edge and
  the carrier has no stub on the only fast signal.
- **`R3/R4` (27Ω USB) → brain, at U1's pins.** Same principle, and the stronger
  case: these are **source-series termination**, summing with the RP2040's driver
  output impedance to match the line so the far-end reflection is absorbed. On
  the far side of the connector they are no longer in series with the driver —
  that isn't the same part doing a different job, it's the part not doing its
  job. (At Full Speed, ~4ns edges, the hop is forgiving in practice — but there's
  no upside to the carrier placement.) Protection at the port, termination at the
  driver: **U7 + R13/R14 stay at J1 on the carrier** so a plug event clamps
  upstream and never crosses.

**Judgment calls left open:** which test points go where (rail probes belong on
the brain, bus/12V probes on the carrier); whether the carrier keeps any
bottom-side parts at all once the brain vacates the top.

## Order of work

1. **Strip the two sheets to role** using the functional split above. This clears
   the duplicate-reference errors and produces the first real netlist.
2. **Draw the interface** — J12/J13 headers on `brain.kicad_sch`, their sockets on
   `carrier.kicad_sch`, wired to the nets in the table above. One netlist means
   ERC now polices the pairing.
3. **Probe how hard the carrier is to route** — a *diagnostic*, not a fab
   decision. The carrier ships at 4 layers either way (see
   [Why not split the stackups](#why-not-split-the-stackups-2026-07-27)); the
   point of the exercise is to find out how much slack there is. Route *only*
   the LED field (25 LEDs + pin-1 caps + 12V/GND + the chain including the BIN
   backup links) on 2 layers, ignoring FSR/RS-485/connectors. Verified facts that
   make this look plausible: the 25-LED chain **already routes single-sided today
   — 1899.6mm on F.Cu with zero vias**, and the only non-top copper on those nets
   (7.1mm + 4 vias on `LED_DATA`) is the pre-shifter RP2040→U4 escape, which
   *moves to the brain and disappears from the carrier*. So the carrier's bottom
   layer can start as an essentially unbroken GND pour.
   - The crux is **not** the LED chain; it is everything that must cross the LED
     field to reach a centrally-mounted brain connector (4 FSR lines, the RS-485
     pair, INT, DIP, USB) while 12V distribution (25 VDD + 28 GND pads in the
     field) competes for the same top layer.
   - **23 of 25 `/LDn` nets have three endpoints**, not two, because of the
     WS2815 backup chain (`/LD1 → D2.DOUT + D7.DIN + D13.BIN`) — roughly 1.5× the
     signal routing of a plain daisy chain. If the field gets tight, dropping the
     BIN backup chain is the lever; it nearly halves the lattice routing, but
     that is a *functional* trade (one dead LED kills the rest of the chain), not
     a layout one. **With 4 layers to spend, don't reach for it** — it only comes
     back on the table if the real 4-layer route somehow won't close.
   - **Reading the result:** if the field closes in 2 layers, that's the good
     outcome — it means the shipping 4-layer carrier gets a solid unbroken GND
     plane *and* a spare power plane, with the FSR lines running over clean
     reference. If it doesn't close, you've learned where the congestion is
     before committing to placement, at no cost.
4. **Place the brain connector on the carrier.** Now the most consequential
   placement decision on that board: nearer the FSR-connector edge keeps the
   analog runs short and off the LED field, at the cost of sitting off-centre in
   the cavity (fine — cavity ~95–105mm, brain needs ~60×50mm).
5. **Brain outline + placement** in the same `.kicad_pcb`, then panelise.

The brain is already **4-layer-proven**: all of its content routes on 4 layers
today *while sharing them* with the LED field, 12V distribution and the FSR front
end. A dedicated 60×50mm board removes all of that competition.
