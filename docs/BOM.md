# Bill of Materials & Sourcing

What we are buying, and from where. Four orders, one section each.

**Build scope: 2 pads = 2 master PCBs + 20 panel PCBs** (2 × 9 + 2 spares).
Every quantity below is sized to that scope.

**This doc is derived from the boards, not maintained by hand.** The
per-part identity lives in the KiCad schematics (`LCSC` / `MPN` fields);
`tools/bom_census.py` reads the `.kicad_pcb` files and prints the need counts
used here. Re-run it after any part change and reconcile before ordering:

```
/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3 tools/bom_census.py
```

⚠ **LCSC prices and stock are point-in-time and have burned us before — verify
live in the cart before committing.** Prices below are the 2026-07-25 cart
export (`tmp/export_cart_20260725_104011.csv`), USD.

Superseded material — the DigiKey price snapshot the pivot was measured
against, the AliExpress comparisons, and the hand-solder-vs-JLC-assembly
analysis — is in `docs/archive/BOM_SOURCING_HISTORY.md`.

---

## Order 1 — JLCPCB

| Item | Qty | Note |
|------|-----|------|
| Panel PCB, 4-layer, **+ SMD assembly (PCBA)** | 20 | 113 placements/board (93 top / 20 bottom), DNP excluded. Part identity comes from the exported BOM/CPL's `LCSC` column and auto-matches. These parts never reach the bench and are **not** in order 2. |
| Master PCB, bare fab only | 2 | Hand-assembled — PCBA's ~$148 fixed overhead buys nothing for 19 parts. |

Files come from `hardware/panel-pcb/production/` (gerbers zip + BOM + CPL).
**Regenerate before ordering** — the committed export is stale; see
`docs/PRE_ORDER_CHECKLIST.md` §2 for the change list (the drill file
specifically must be regenerated, J9's holes are now 1.30mm).

Place this order **first or same-day** as order 2, then email
`support@lcsc.com` with both order numbers to combine the shipment.

## Order 2 — LCSC (~$110, 22 lines)

Everything hand-soldered on both boards, plus the harness connectors.
**Combined-shipped with the JLC order**, so it carries no extra freight.
Constraints on combining: same currency and customer ID, not to Mainland
China, cannot be unbound afterwards, shipping is recalculated. If either order
has already shipped, it's too late.

### Panel through-hole (13 parts/board × 20)

| LCSC | Part | Refs | Need | Cart | Ext $ |
|------|------|------|------|------|-------|
| **C503478** | Molex Micro-Fit 3.0 3-pin RA header (436500300) | J8, J10 (+ master J1) | 42 | 50 | 21.10 |
| **C192562** | Molex Micro-Fit 3.0 2-pin RA header (436500200) | J5, J11 | 40 | 50 | 19.93 |
| **C131337** | JST B2B-PH-K-S 2-pin 2mm vertical (genuine JST) | J3, J4, J6, J7 | 80 | 100 | 3.53 |
| **C53184807** | LCKELEC LCK-TCF829D USB-C, vertical all-THT | J1 | 20 | 30 | 7.70 |
| **C8465** | KANGNEX WJ500V-5.08-2P screw terminal | J9 (+ master J4) | 22 | 30 | 4.01 |
| **C52177925** | Zhongdi DS-04 DIP slide, 4-pos | SW1 | 20 | 30 | 6.03 |
| **C609835** | XKB SS22E01L5 DPDT slide | SW3 | 20 | 25 | 4.37 |
| **C2937625** | XFCN PZ254V-11-03P pin header 1×03 | J2 | 20 | 50 | 0.94 |

TP1–TP14 are bare probe holes and SW2 (BOOTSEL) is SMD — neither is ordered.

### Master — all of it (hand-assembled, SMD included)

| LCSC | Part | Refs | Need | Cart | Ext $ |
|------|------|------|------|------|-------|
| **C1850236** | TI THVD1429DR RS-485 transceiver, SOIC-8 | U2 | 2 | 4 | 16.28 |
| **C354152** | TI SN74AHCT125N quad buffer, DIP-14 | U3 | 2 | 5 | 3.04 |
| **C840655** | Bourns 4610X-101-103LF 10k ×9 bussed, SIP-10 | RN1 | 2 | 5 | 1.66 |
| **C46595747** | DORABO DS-3P-BU DIP slide, 3-pos | SW1 | 2 | 5 | 1.23 |
| **C2905420** | KH-2.54FH-1X14P-H8.5 female 1×14 socket | U1 socket (2/board) | 4 | 10 | 1.52 |
| **C113952** | SMAJ5.0A TVS, DO-214AC (MDD) | D2–D10 | 18 | 40 | 1.64 |
| **C844839** | 330R 1% 0805 (Vishay) | R3, R6–R14 | 20 | 50 | 0.75 |
| **C1791** | 1nF C0G 0805 (Samsung) | C3–C11 | 18 | 30 | 0.35 |
| **C83055** | 100nF X7R 0805 (Walsin) | C1, C2 | 4 | 10 | 0.24 |
| **C844816** | 120R 1% 0805 (Vishay) | R1 | 2 | 100 | 0.86 |
| **C192906** | 10k 1% 0805 (Yageo) | R15 | 2 | 50 | 0.90 |

Master J1 and J4 share the panel's Micro-Fit and terminal lines above.

**Two of these part numbers also appear on the panel, and the quantities here
deliberately do not cover that:** the panel's D30 (SMAJ5.0A, **C113952**) and U2
(THVD1429, **C1850236**) are SMD, so **JLCPCB sources and places them** as part
of order 1. Only the master's 18 TVS and 2 transceivers are hand-soldered and
bought here. Checking design totals against this table will therefore look 20
TVS and 20 transceivers short — that is correct, not an under-order.

### Harness (not on any board)

| LCSC | Part | Need | Cart | Ext $ |
|------|------|------|------|-------|
| **C114089** | Molex 436450200 Micro-Fit 2-ckt receptacle housing | 30 | 50 | 3.79 |
| **C259740** | Molex 436450300 Micro-Fit 3-ckt receptacle housing | 36 | 50 | 5.68 |
| **C259786** | Molex 430300001 Micro-Fit crimp, 20–24 AWG | 132 | 300 | 4.47 |

Housing/crimp counts derive from the stock SMX topology: 3 power columns of 3
panels (5 housings per column × 3 × 2 pads = 30), RS-485 serpentine with 9
segments × 2 ends × 2 pads = 36, and 132 crimps total. Buy crimps in bulk —
crimping has a learning curve. RS-485 3-pin housings populate only 2 circuits
(the empty third is the keying that stops 12V reaching a transceiver). Power
feed cables crimp only the panel end; the PSU end is fork/spade lugs.

## Order 3 — DigiKey / PJRC

| Item | Qty | Note |
|------|-----|------|
| Teensy 4.0 (PJRC 15583) | 2 | ~$36 each, the one part LCSC never carries. PJRC-direct, Adafruit or SparkFun work equally. **The only line left on this order.** |

## Order 4 — AliExpress

Commodity items and the two things the other vendors don't win. Each candidate
needs its match-check to pass before ordering.

| Item | Qty | Candidate | Match-check |
|------|-----|-----------|-------------|
| Euroblock 9-pos header **+ plug** (master J2) | 2 sets | [pack of 5](https://www.aliexpress.com/item/1005012001482158.html) | 5.08mm pitch, 9-pos, single-row; footprint is Molex 39531 P5.08. One pack covers both pads |
| 12V power cable, 2C 20 AWG jacketed | ~10m | [1005008621580316](https://www.aliexpress.com/item/1005008621580316.html) | 20 AWG (not 22/24), **stranded**, 2-conductor jacketed round |
| RS-485 cable, 22 AWG twisted pair | ~10m | [1005006546939974](https://www.aliexpress.com/item/1005006546939974.html) | genuine twisted pair, pure copper. Shielded is fine — see the shield note below |
| INT + hookup wire, 24 AWG, 10 colors | 1 pack | [1005008982254390](https://www.aliexpress.com/item/1005008982254390.html) | **stranded not solid**, pure copper, covers the 9-panel color map |
| Wire ferrules (~0.25mm² for 24 AWG) | ~60 | assortment box | for the INT screw terminations, ~30/pad |
| Heatshrink assortment, zip-tie anchors, grommets | — | — | grommets where cable crosses frame metal |

Buy mating housings and crimps from the **same** ecosystem — mixing clone
crimps into a different clone housing risks seat/latch failures. Micro-Fit
clones are otherwise fine at this 5A hobby load, but ours are genuine Molex
from LCSC anyway. Watch vertical-vs-**right-angle** on any PCB header.

## Not ordered

| Item | Why |
|------|-----|
| **Panel D12/D23** (PMEG3015EH, C552867) | **DNP** — the LM66200 ideal-diode mux (U8) replaced the Schottky power-OR. Footprints stay so populating both + removing U8 is a hand-solderable rescue |
| **Master R4/R5** (390R 1%) | **DNP** — THVD1429's integrated failsafe makes RS-485 bias unnecessary; footprints exist if the bench disagrees |
| FSR sensors | reuse stock SMX (Interlink FSR 408; iefsr.com if replacements are needed) |
| 18 AWG stranded (underglow / GND tie) | on hand |
| Spade/fork lugs (PSU ends + master GND tie) | on hand — size vs the PSU stud is a teardown item |
| M3 mounting hardware | on hand |
| Master enclosure | future — 3D-print from the KiCad 3D export once boards are in hand |

## Wire specification

All runs: **stranded pure copper** (never solid — vibration and flex
work-harden it; never CCA), PVC insulation (UL1007-class), any ≥80°C/300V
rating.

| Signal | Spec | Quantity |
|--------|------|----------|
| 12V power | 2×20 AWG jacketed round, red/black | 3 columns × 2 pads, ~5m/pad |
| RS-485 | 22–24 AWG **actual twisted pair** | 9 segments/pad, ~5m/pad. Fix an A/B color convention and never deviate |
| INT | 24 AWG, **9 distinct colors** | ~7–10m/pad. Stock SMX map, confirmed against the pad: 0=Red 1=Orange 2=Yellow 3=Green 4=Blue 5=Brown 6=Grey 7=White 8=Black — feeds panel-ID mismatch detection |
| Master GND tie | 1 lead (18 AWG on hand) to the PSU GND stud | **mandatory**, not optional wiring |

Spool quantization (25/100 ft) makes precise footage moot; the 9-color INT
requirement is the awkward line, and a multi-color assortment kit beats nine
spool minimums. Leave service-loop slack so a panel can be lifted out while
still connected.

**RS-485 shield (settled 2026-07-24, do not re-litigate):** the sourced cable
*is* shielded, because unshielded jacketed twisted pair is effectively
unavailable on AliExpress. The drain is left **unconnected at both ends**, and
that is electrically fine here — a shield couples symmetrically to both
conductors of a balanced pair, so what it picks up arrives as common mode and
the receiver rejects it. "Never leave a shield floating" is EMC-certification
guidance about radiated emissions and quarter-wave resonance on long cables;
neither applies to a 3m hobby run at 1 Mbps with no emissions requirement. The
real risk is an *intermittent* shield, so trim the drain flush and heatshrink
over it at both ends. If the bench ever shows noise, landing the drain on the
master's GND tie — one end only, never both — is a five-minute change.

## Rules of thumb

- **Price LCSC first.** Combined shipping with JLC makes its freight
  effectively free; it beat DigiKey by ~65% on everything it carries and
  stocks the Micro-Fit family as genuine Molex rather than clones.
- Fall to **DigiKey** only for what LCSC cannot supply.
- Use **AliExpress** for cable and bulk commodity items.
- Sub-9-panel passthrough builds (`docs/MODULAR_PANEL_COUNT.md`) are the same
  PCB with a different BOM. The quantities here do **not** include them.
