# Panel PCB — layout considerations (pre-layout decisions)

Decisions and constraints agreed 2026-07-11, to be applied when the layout phase
starts (after the schematic GUI tidy-up + label/class/footprint normalization
pass). Physical source of truth for outline/holes/pitches:
`STOCK_PANEL_PCB_MEASUREMENTS.md`. Stock chip-level reference:
`STOCK_PANEL_CHIPS.md`.

## Component sides: single-sided (top) assembly

Aim for **all parts on top; bottom stays bare** (stock does exactly this — its
back side is completely unpopulated). Bottom side is a fallback if placement gets
tight, not a plan.

- Everything user-facing is forced to the top anyway: LEDs, USB-C, BOOTSEL, SWD
  header, ID DIP, termination slide switch, all edge connectors.
- Area is generous: 127×127mm, LED lattice at 17mm row / 34mm column pitch leaves
  wide component bands; stock fit MCU + 5 LED drivers + 4 sensor amps + passives
  on top of the same size board, and our support circuitry is smaller.
- JLCPCB economic PCBA is **single-side only** — double-sided placement bumps to
  standard assembly with extra setup + a second stencil, multiplied across boards.
- Bare bottom is mechanically robust: nothing to crunch in handling, on the
  standoffs, or under stomp-flex.

## Layer count: 4-layer

Chosen for noise, not routing density. This system is demonstrably
analog-noise-sensitive (the bench ADC-crosstalk problem that required the 10nF
per-ADC-node caps), and 25 WS2815s dump PWM switching current into the 12V/GND
net on the same board as high-ish-impedance FSR divider nodes.

Stackup:

| Layer | Use |
|-------|-----|
| L1 | Components + signals |
| L2 | **Solid GND plane — never split, never route through it** |
| L3 | Power: big 12V pour under the LED field; 3.3V / 5V islands under the MCU/logic zone. Don't route signals across pour splits (returns live on L2, so this is easy to honor) |
| L4 | Spillover signals / pours |

- Clean L2 return paths largely decouple LED switching current from the analog
  corner, and make 1Mbps RS-485 + USB routing trivial to do well.
- Cost premium 2→4 layer at this size/quantity is a few dollars per board
  (price exactly at order time) — cheap insurance for the product's core
  function (clean FSR readings).

## High-current path

The 12V daisy-chain pass-through carries up to **~2.7A** (three panels
downstream on a column). The IN-connector → OUT-connector power path needs
deliberately fat copper on L1/L3 regardless of layer count. Local panel LED load
is ~0.9A on top of pass-through at the IN side.

## Passthrough boards = same PCB, different assembly BOM (decided 2026-07-11)

KiCad is strictly one-schematic ↔ one-board per project, and that's fine — the
passthrough is **not a separate design**:

- The panel schematic's power and RS-485 IN/OUT connectors are bused
  connector-to-connector (RP2040/transceiver hang off the bus, not in series),
  so a panel board populated with **only connectors + termination DPDT + 120Ω**
  is already electrically a valid passthrough (INT connector present,
  unconnected line floats safe-HIGH at the master; see
  `MODULAR_PANEL_COUNT.md`).
- Mechanism: KiCad 7+ per-symbol **DNP** attribute → export a second BOM+CPL
  pair for JLCPCB. One bare-PCB SKU, two assembly variants, zero
  schematic-drift risk. (KiVar plugin exists for fancier variant management if
  ever needed; native DNP suffices for two variants.)
- Trade-off accepted: a passthrough pays full-size 4-layer bare-board cost
  (a few $ each; 5 planned at prototype quantities). A dedicated cheap
  passthrough board only becomes worth designing at production volumes, from a
  frozen spec.
- Layout implication: nothing special — but keep the termination switch +
  resistor + connectors placed so they read sensibly on an otherwise-empty
  assembled board.

## Other standing layout constraints (collected from earlier decisions)

- 10nF C0G caps at each ADC input **physically at the RP2040 pins** (crosstalk
  fix is placement-sensitive).
- Keep FSR analog traces/dividers away from the LED field and LED data line;
  analog corner near the MCU, over unbroken L2 GND.
- AMS1117 pair (SOT-223) wants copper pour for dissipation (~0.35W on the 5.0).
- LDO output caps C16–C19: tantalum/electrolytic with some ESR (AMS1117
  stability) — footprint choice happens in the normalization pass.
- USB-C: vertical DIP through-hole mount (mechanical stress on through-hole
  pins; no right-angle cable-clearance footprint cost).
- X dimension fixed ~127mm (edge connectors left/right); Y has ~20mm slack per
  end if ever needed. Mounting holes 4.5mm, ~114mm centers, 6/7mm insets —
  verify which edges are 7mm against the frame before finalizing.
- Connector edge positions should mirror stock's measured locations (cable
  reach + cavity fit) — see measurements doc.
