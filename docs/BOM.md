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
| **C8465** | KANGNEX WJ500V-5.08-2P screw terminal | J9 (+ master J2) | 22 | 30 | 4.01 |
| **C52177925** | Zhongdi DS-04 DIP slide, 4-pos | SW1 | 20 | 30 | 6.03 |
| **C609835** | XKB SS22E01L5 DPDT slide | SW3 | 20 | 25 | 4.37 |
| **C2937625** | XFCN PZ254V-11-03P pin header 1×03 | J2 | 20 | 50 | 0.94 |

TP1–TP14 are bare probe holes and SW2 (BOOTSEL) is SMD — neither is ordered.

### Master — all of it (hand-assembled, SMD included)

| LCSC | Part | Refs | Need | Cart | Ext $ |
|------|------|------|------|------|-------|
| **C1850236** | TI THVD1429DR RS-485 transceiver, SOIC-8 | U1 | 2 | 4 | 16.28 |
| **C7484** | TI SN74AHCT1G125DBVR single buffer, SOT-23-5 (was C354152 quad DIP, swapped 2026-08-03) | U3 | 2 | 5 | 0.23 |
| **C840655** | Bourns 4610X-101-103LF 10k ×9 bussed, SIP-10 | RN1 | 2 | 5 | 1.66 |
| **C46595747** | DORABO DS-3P-BU DIP slide, 3-pos | SW1 | 2 | 5 | 1.23 |
| **C2905420** | KH-2.54FH-1X14P-H8.5 female 1×14 socket | U2 socket (2/board) | 4 | 10 | 1.52 |
| **C113952** | SMAJ5.0A TVS, DO-214AC (MDD) | D1–D9 | 18 | 40 | 1.64 |
| **C844839** | 330R 1% 0805 (Vishay) | R5, R6–R14 | 20 | 50 | 0.75 |
| **C1791** | 1nF C0G 0805 (Samsung) | C3–C11 | 18 | 30 | 0.35 |
| **C83055** | 100nF X7R 0805 (Walsin) | C1, C2 | 4 | 10 | 0.24 |
| **C844816** | 120R 1% 0805 (Vishay) | R3 | 2 | 100 | 0.86 |
| **C192906** | 10k 1% 0805 (Yageo) | R4 | 2 | 50 | 0.90 |
| **C158012** | JST B2B-XH-A 2-pin 2.5mm vertical THT | J3–J11 (INT) | 18 | 40 | — |

Master J1 and J2 share the panel's Micro-Fit and terminal lines above.

**J3–J11 replaced the 9-pos Euroblock 2026-07-26** (INT went to twisted
pair, signal + dedicated GND). That moved the INT connector off the AliExpress
order and onto this one. Nine per board, 18 for two pads.

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
| **C259786** | Molex 430300001 Micro-Fit crimp, 20–24 AWG | 168 | 300 | 4.47 |
| **C144401** | JST XHP-2 2-pos housing (INT, wire side) | 18 | 50 | — |
| **C385122** | JST SXH-001T-P0.6N XH crimp, 22–26 AWG | 36 | 100 | — |

Housing/crimp counts derive from the stock SMX topology: 3 power columns of 3
panels (5 housings per column × 3 × 2 pads = 30), RS-485 serpentine with 9
segments × 2 ends × 2 pads = 36 housings. Buy crimps in bulk — crimping has a
learning curve.

**Micro-Fit crimp count rose 132 → 168 on 2026-07-26:** RS-485 3-pin housings
now populate **all three** circuits, because pin 3 carries the cable shield
(36 housings × 3 = 108, plus 60 for power). The third position is no longer
empty — keying against 2-pin power is the 3-circuit housing itself, and pin 3
carries shield only, so a mis-mate still cannot put 12V on a transceiver.

XH counts: 9 INT connectors per pad × 2 pads = 18 housings, 2 contacts each =
36. 100 contacts ordered deliberately — XH crimping wants practice crimps.

Power feed cables crimp only the panel end; the PSU end is fork/spade lugs.

## Order 3 — DigiKey / PJRC

| Item | Qty | Note |
|------|-----|------|
| Teensy 4.0 (PJRC 15583) | 2 | ~$36 each, the one part LCSC never carries. PJRC-direct, Adafruit or SparkFun work equally. **The only line left on this order.** |

## Order 4 — AliExpress

Commodity items and the two things the other vendors don't win. Each candidate
needs its match-check to pass before ordering.

| Item | Qty | Candidate | Match-check |
|------|-----|-----------|-------------|
| 12V power cable, 2C 20 AWG jacketed | ~10m | [1005008621580316](https://www.aliexpress.com/item/1005008621580316.html) | 20 AWG (not 22/24), **stranded**, 2-conductor jacketed round |
| RS-485 cable, 22 AWG shielded twisted pair (RVSP) | ~10m | [1005006546939974](https://www.aliexpress.com/item/1005006546939974.html) | **shield is now required, not merely tolerated** — it lands on Micro-Fit pin 3. See the shield note below |
| INT cable, 24 AWG 2-core shielded twisted pair (RVSP) | **length TBD — gated on the pad teardown** | [1005006546939974](https://www.aliexpress.com/item/1005006546939974.html) (same listing, 24 AWG / 2-core) | See the three checks below. Listing offers 10/20/30/50m; unverified estimate is ~8.5m of actual need, so **10m has almost no margin** |
| Wire ferrules (~0.25mm² for 24 AWG) | ~40 | assortment box | panel-side **J9 screw terminal only** — 2 conductors × 9 panels × 2 pads = 36. The master end is now JST XH crimps, not screw terminations |
| Colored + printed heatshrink, zip-tie anchors, grommets | — | — | heatshrink is now **load-bearing**: it carries the per-panel INT identification (see wire spec). Grommets where cable crosses frame metal |

**INT cable — verify on arrival before crimping 36 contacts:**

1. **Conductor insulation OD must be 1.3–1.9mm** (SXH-001T-P0.6N spec). This is
   the *individual conductor's* insulation, **not** the outer jacket OD. It is a
   **floor**, and typical RVSP 24 AWG measures ~1.3–1.4mm — right at the edge.
   If it comes up short: fold the insulation back, or step to 22 AWG (the contact
   covers 22–26 and current here is microamps, so gauge is purely mechanical).
2. **Genuinely twisted**, not 2-core parallel/zip.
3. **Stranded, not solid** — solid cracks at the crimp after a few re-dresses.

Checks 2 and 3 are already satisfied by the type code: **RVSP** = R 软 flexible
(stranded) + V PVC + S 双绞 (twisted) + P shielded. Only the OD is open.

The Euroblock line (the master's old 9-pos header + plug) was **removed 2026-07-26** — INT
moved to JST XH, which is sourced on the LCSC order.

Buy mating housings and crimps from the **same** ecosystem — mixing clone
crimps into a different clone housing risks seat/latch failures. Micro-Fit
clones are otherwise fine at this 5A hobby load, but ours are genuine Molex
from LCSC anyway. Watch vertical-vs-**right-angle** on any PCB header.

## Not ordered

| Item | Why |
|------|-----|
| **Panel D12/D23** (PMEG3015EH, C552867) | **DNP** — the LM66200 ideal-diode mux (U8) replaced the Schottky power-OR. Footprints stay so populating both + removing U8 is a hand-solderable rescue |
| **Master R1/R2** (390R 1%) | **DNP** — THVD1429's integrated failsafe makes RS-485 bias unnecessary; footprints exist if the bench disagrees |
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
| INT | 24 AWG **2-core twisted pair** (signal + dedicated GND), single color | Length **gated on the pad teardown** — 9 *home runs* per pad, not a chain, so the stock harness is no guide. Unverified estimate ~8.5m/pad |
| Master GND tie | 1 lead (18 AWG on hand) to the PSU GND stud | **mandatory**, not optional wiring |

**INT identification changed 2026-07-26.** The old spec called for 9 distinct
wire colors; the twisted-pair cable that meets the mechanical requirements
(RVSP) only comes in one color, so per-panel identity moved to **colored or
printed heat-shrink at both ends of every cable**, plus silkscreen panel names
on the master (`UL`/`U`/`UR`/`L`/`C`/`R`/`DL`/`D`/`DR`, left to right = panel
0→8). The stock SMX map is retained as the marker scheme: 0=Red 1=Orange
2=Yellow 3=Green 4=Blue 5=Brown 6=Grey 7=White 8=Black.

Prefer heat-shrink over tape — tape unwinds and its adhesive migrates in a warm
pad. Printed labels under clear heat-shrink beat colors outright, since "P4"
needs no lookup table. Marking is a convenience, not a correctness requirement:
the `'I'` identify pulse learns the real mapping and reports mismatches
(`docs/RS485_PROTOCOL.md`).

Spool quantization (25/100 ft) makes precise footage moot. Leave service-loop
slack so a panel can be lifted out while still connected.

**RS-485 shield — REVERSED 2026-07-26.** The previous decision (settled
2026-07-24) left the drain **unconnected at both ends**, reasoning that a shield
couples symmetrically to a balanced pair so its pickup arrives as common mode
and the receiver rejects it, and that resonance guidance targets EMC
certification rather than a 3m hobby run. That reasoning was sound as far as it
went, and the reversal is not a correction of an error — it is taking the
five-minute upgrade the old note itself described as available.

**The shield is now terminated on Micro-Fit pin 3**, previously left
unpopulated. Hybrid grounding, one continuous shield master → panel 8:

- **Master:** J1 pin 3 → GND, plain trace. The single DC reference for the
  whole network. Omit it and the shield floats — strictly worse than no shield.
- **Each panel:** `RS485_Shield` passes J8 pad 3 → J10 pad 3 with **no local
  GND tie**, plus C57 100nF ‖ R20 1M to GND. The cap RF-grounds the shield
  (~6–8m of single-end-grounded foil resonates near 10 MHz, inside 1 Mbps
  harmonic content) while blocking DC, so no ground loop forms against the 12V
  ground network. The 1M bleeds tribocharge — rubber soles on the panels charge
  the pad, the same reasoning behind the panel USB ESD array.
- **Panel 8:** far end, shield terminates.

Each cable segment lands the shield at **both** its connectors; the panel
pass-through traces make it continuous. "Grounded at one end" describes the
network, not each segment. If the cable has no drain wire, gather the braid,
solder a short lead, heatshrink the joint, and crimp that — an intermittent or
stray-strand shield is still the real failure mode, exactly as the old note said.

**Not applied to INT.** JST XH has only 2 positions, so a shield on the INT
cable is trimmed and heatshrunk at both ends. Do **not** bond it to the INT GND
conductor: that is a signal return, and paralleling a shield across it restores
the loop area the twisting exists to eliminate.

## Rules of thumb

- **Price LCSC first.** Combined shipping with JLC makes its freight
  effectively free; it beat DigiKey by ~65% on everything it carries and
  stocks the Micro-Fit family as genuine Molex rather than clones.
- Fall to **DigiKey** only for what LCSC cannot supply.
- Use **AliExpress** for cable and bulk commodity items.
- Sub-9-panel passthrough builds (`docs/MODULAR_PANEL_COUNT.md`) are the same
  PCB with a different BOM. The quantities here do **not** include them.
