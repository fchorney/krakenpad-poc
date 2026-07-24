# Panel PCB — as-built reference

Condensed 2026-07-23 from `PANEL_SCHEMATIC_PLAN.md` + `PANEL_PCB_LAYOUT_NOTES.md`
(drafting history lives in git). Everything below is verified against the
current schematic netlist, not carried from plans.

**Status:** schematic and layout complete, ERC 0 / DRC 0 (no exclusions),
**not yet ordered** — see `docs/PRE_ORDER_CHECKLIST.md`. External AI review of
the full board pair sent 2026-07-23, triage pending. A part swap is still just
a footprint edit + re-DRC until the order is placed.

System-level architecture (why FSR + INT wire + RS-485, power topology,
LED/level-shifter rationale) lives in `CLAUDE.md` — not repeated here.

## RP2040 pin map (as built, netlist-verified 2026-07-23)

| GPIO | Net | Function |
|------|-----|----------|
| 0 | `RS485_TX` | UART0 TX → U2 THVD1429 DI |
| 1 | `RS485_RX` | UART0 RX ← U2 RO |
| 3 | `LED_DATA` | PIO WS2815 out → U4 shifter A-input |
| 6 | `RS485_DE` | THVD1429 DE + /RE (tied) |
| 10 | `INT_OUT` | Open-drain INT to master, via R17 100R → J9 |
| 13 | `TERM_SENSE` | Termination DPDT pole 2 (internal pull-up; LOW = terminated) |
| 17 | `SENSE_12V` | 12V-presence divider (R18 100k / R19 33k, ~2.98V at 12V; C53 100nF filter; D29 PMEG3015EH clamp to +3.3VDC). **Digital present/absent check only** — firmware reads it as a plain GPIO HIGH/LOW to decide whether the 12V LED rail is live and it's safe to drive the WS2815s. Deliberately **not** on an ADC pin (all four ADCs are on the FSRs); it was never meant to measure bus voltage, so GPIO17 being non-ADC is correct, not a defect. |
| 18–21 | `DIP_ID3`–`DIP_ID0` | Panel-ID DIP (note bit order: GPIO18 = bit 3, GPIO21 = bit 0); switch to GND, internal pull-ups |
| 22 | `DEBUG_LED` | R15 1k → D1 (diagnostic-mode-gated, see PANEL_CONFIG.md) |
| 26–29 | ADC0–3 | FSR **South / West / North / East** (in that ADC order); 10nF C0G at each pin (C16–C19), 10k 1% pull-downs (R8–R11) |
| spare | — | GPIO 2, 4, 5, 7–9, 11, 12, 14–16, 23–25 (unconnected) |

Hard constraints only: UART0 on GPIO0/1 and ADC on GPIO26–29. Everything else
was assigned freely during routing; **firmware maps to this table at bring-up**
(the prototype firmware's pin choices do not apply).

Support pins: RUN = R1 10k pull-up + TP1; QSPI_SS = R12 10k pull-up (fitted —
closes the early-power-ramp race) + R7 1k → SW2 BOOTSEL; XOUT via R6 1k to the
crystal; ADC_AVDD = R5 200R from +3.3VDC + C4 2.2µF (single-RC filter per the
official Pico schematic); TESTEN → GND.

## Parts (refs → identity)

| Ref(s) | Part | Notes |
|--------|------|-------|
| U1 | RP2040 (QFN-56) | bare chip per the official "Hardware design with RP2040" reference |
| U2 | THVD1429 (SOIC-8) | RS-485; 20Mbps grade — the THVD1419 was the 250kbps grade, swapped 2026-07-19 |
| U3 | W25Q32JV (4MB QSPI, SOIC-8) | boot flash + animation/config slots (`docs/ANIMATIONS.md`) |
| U4 | SN74AHCT125DR (SOIC-14) | 5V level shifter, one gate → R16 330R → LED chain |
| U5 | AMS1117-5.0 (SOT-223) | 12V→5V stage; C38 22µF tantalum output (ESR required), C37+C52 2× 10µF 0805 input |
| U6 | AP7361C-33ER-13 (SOT-223R) | 5V→3.3V; **`-33ER-` suffix only** — plain `-33E-` is pin-reversed |
| U7 | USBLC6-2SC6 (SOT-23-6) | USB ESD array, connector side of R3/R4 27R |
| U8 | LM66200 (SOT-583-8) | ideal-diode 5V power-OR (VBUS vs AMS1117 output) |
| D12, D23 | PMEG3015EH — **DNP** | Schottky-OR fallback: populate both + remove U8 to rescue by hand |
| D29 | PMEG3015EH — fitted | SENSE_12V clamp to +3.3VDC |
| D2–D28 (except D12/D23) | 25× WS2815B-V1 | 12V addressable, custom PLCC6 footprint; per-LED 100nF on **VCC (pin 1)** to GND (internal-regulator filter — correct per datasheet, do not "fix"); chain rule: BIN(n) ← DIN-signal(n−1), **first LED's BIN → GND**, last DOUT NC |
| D1 | debug LED (0603) | in the JLC BOM (not DNP-flagged) — decide at order time |
| X1 | ABM8-272-T3 12MHz | 15pF C0G loads (C12/C14) |
| J1 | GCT USB4085-GF-A | all-THT USB-C, bench flashing only |
| J2 | 1×3 SWD header | SWDIO/GND/SWCLK |
| J3/J4/J6/J7 | JST B2B-PH-K (FSR N/E/S/W) | pin 2 = 3.3V, pin 1 = ADC node; FSR is non-polarized |
| J5/J11 | Micro-Fit 43650-0200 (12V IN/OUT) | straight-through heavy copper |
| J8/J10 | Micro-Fit 43650-0300 (RS-485 IN/OUT) | pin 3 unpopulated (keying vs power) |
| J9 | MRR522-5.08-V 2-pos screw terminal (INT) | **pin 1 = INT signal (`Net-(J9-Pin_1)`, via R17), pin 2 = dedicated GND** (decided 2026-07-24, supersedes both-pins-bridged). D30 SMAJ5.0A clamps signal→pin-2 GND at the connector. Single-conductor cable leaves pin 2 unpopulated; pin 2 pre-provisions a signal+GND paired cable for free if the bench ever shows spurious triggers |
| SW1 | DS01C-254-S-04BE (DIP-4) | panel ID 0–8, diag modes 9–13 (`docs/PANEL_CONFIG.md`) |
| SW2 | B3U-1000P | BOOTSEL |
| SW3 | EG2201A (DPDT) | pole 1 = 120R (R2) across A/B; pole 2 = TERM_SENSE |
| R13/R14 | 5.1k CC pull-downs | required for C-to-C cables |
| C51 | 470µF 25V SMD electrolytic | 12V bulk — deliberately not tantalum (hot-plug surge) |

Full identity (MPN/LCSC per line) lives in the schematic fields; ordering info
in `docs/BOM.md`.

## Layout (as built)

- **4 layers, JLC 4-layer standard** (JLC04161H-7628 stackup, 0.3/0.45 via
  class): L1 components+signals, **In1 solid GND (never split)**, In2 power
  pours (12V under the LED field, 3.3V/5V islands under the logic zone), B.Cu
  spillover + GND pour with ~270 stitching vias. Chosen for analog noise
  (FSR dividers share the board with 25 WS2815s), not routing density.
- **Assembly is double-sided** (110 SMD placements: 97 top / 13 bottom — RP2040
  decoupling + CC pull-downs on the back; the ~$25 double-sided delta was
  accepted 2026-07-18). The earlier "single-sided top" aim is superseded.
  Panel THT (connectors/switches) is hand-soldered, not JLC.
- **High-current path:** 12V IN→OUT daisy-chain carries up to ~2.7A
  pass-through (+~0.9A local LED load at the IN side) — fat copper on L1/In2.
- Impedance-controlled pairs (from the real stackup): USB 90Ω W=0.528/S=0.15mm;
  RS-485 120Ω W=0.2787/S=0.2mm.
- Routing discipline: B.Cu traces stay under their own net's In2 island;
  post-shifter LED data is F.Cu-only (the pre-shifter RP2040→U4 segment
  legitimately uses vias to escape the QFN nest).
- 10nF ADC caps physically at the RP2040 pins (the crosstalk fix is
  placement-sensitive); analog corner near the MCU over unbroken In1 GND, away
  from the LED field.
- QSPI length-matched to the RP2040 (including via count) per the RP2040
  hardware design guide.

### Trace / via width conventions (user's routing rules)

| Class | Trace | Via |
|-------|-------|-----|
| 12V trunk (IN/OUT + its ground return, ≤2.7A) | 2mm | 0.6mm |
| LED power feeds off the 12V plane + their grounds | 1mm | 0.3mm |
| 5V / 3V3 top-layer power | 1.5–2mm | 0.6mm |
| Data | 0.2mm (0.15mm where needed, e.g. QFN escape) | 0.3mm |
| Ground (non-trunk) | 2mm | 0.3mm |
| Decoupling | ~2mm (short > wide) | 0.3mm |
| USB / RS-485 | per impedance calc above | — |
| QSPI / crystal | 1.5–2mm, length-matched | — |

## Physical

127×127mm outline (edges actually 128/127/128/127), mounting holes 4.5mm on
114mm centers except the top pair at 113mm, LED lattice at 33.5mm column /
17mm row pitch — all measured off the stock board and fit-test-confirmed with
a 1:1 printout; see `docs/STOCK_PANEL_REFERENCE.md`. X is locked ~127mm by the
edge connectors; Y has ~20mm slack per end if ever needed. Height budget above
the PCB: ~35mm. Silkscreen: "Kraken Pad by SenPi / Rev. 1.0", JLC order-number
placeholder on B.SilkS, personal logo as exposed GND copper (mask-opening
technique); project logo pending artwork.

## Passthrough variant

A panel board populated with only connectors + termination DPDT + 120R is
electrically a valid passthrough (power and RS-485 are bused
connector-to-connector; INT floats safe-HIGH at the master). Mechanism: KiCad
per-symbol DNP → second BOM+CPL export. One bare-PCB SKU, two assembly
variants. See `docs/MODULAR_PANEL_COUNT.md`.

## Accepted-for-rev-A risks (bring-up measurements, not order blockers)

U5 thermal at real load (~50mA), +5V rail margin, INT-into-dead-panel (the
master-side 10k pull-up is deliberately not stiff for this reason),
hot-plug/USB-attach behavior, SI asymmetries, ADC B.Cu runs. Plus the one open
hardware verification: **bench-drive a WS2815 strip from the SN74AHCT125N at
5V** (we've only hands-on tested WS2812B; the datasheet closes the question on
paper — VIH abs 2.7V min, 12V would violate the 5.7V abs-max).
