# DigiKey Shopping List — 2 masters + 20 panels

Generated 2026-07-22. Quantities: **2 master PCBs**, **20 panel PCBs** (2 pads = 18 + 2 spares).
All master parts + all panel **through-hole** parts (panel SMD is JLC PCBA, never reaches the bench).

## A. Panel PCB — through-hole only (× 20 boards)

| Qty | Ref/panel | Part | Per panel | MPN | Status |
|-----|-----------|------|-----------|-----|--------|
| 40 | J5, J11 | Micro-Fit 3.0 2-pin, right-angle THT | 2 | Molex **43650-0200** | decided |
| 40 | J8, J10 | Micro-Fit 3.0 3-pin, right-angle THT | 2 | Molex **43650-0300** | decided |
| 80 | J3, J4, J6, J7 | JST-PH 2-pin vertical, board side (FSR) | 4 | JST **B2B-PH-K-S** | decided — leads already have PHR-2 plugs |
| 20 | J1 | USB-C receptacle, 16P USB2.0, all-THT | 1 | GCT **USB4085-GF-A** | decided |
| 20 | J2 | Pin header 1×03, 2.54mm vertical (SWD) | 1 | generic | pick any |
| 20 | J9 | Screw terminal 2-pos 5.08mm | 1 | KF301-style | **PITCH UNRESOLVED** |
| 20 | SW1 | DIP slide switch, 4-pos SPST, 2.54mm, W7.62mm | 1 | DSWB04LHGET | **needs DigiKey equivalent** |
| 20 | SW3 | Slide switch DPDT (RS-485 termination) | 1 | E-Switch **EG2201A** | decided |

TP1–TP12 are bare pads — no parts.
SW2 is SMD — JLC places it, do not order.

## B. Master PCB (× 2 boards)

| Qty | Ref | Part | MPN | Status |
|-----|-----|------|-----|--------|
| 2 | U1 | Teensy 4.0 | PJRC Teensy 4.0 | verify DigiKey stocks it |
| 2 | U2 | RS-485 transceiver, SOIC-8 | TI **THVD1429DR** | decided |
| 2 | U3 | Quad buffer 5V, DIP-14 | TI **SN74AHCT125N** | decided |
| 2 | RN1 | Resistor array 10k ×9, SIP-10 **bussed** | — | **needs DigiKey equivalent** |
| 2 | SW1 | DIP slide switch, 3-pos SPST, 2.54mm, W7.62mm | — | **needs DigiKey equivalent** |
| 2 | J2 | Terminal block 9-pos, 5.08mm pitch | — | **NOT PICKED** |
| 4 | — | Female header 14-pos, 2.54mm (Teensy socket) | generic | **not in schematic** |
| 4 | C1, C2 | 100nF X7R 50V, 0805 | — | not picked |
| 2 | R1 | 120R, 0805 (RS-485 termination) | — | not picked |
| 2 | R2 | 1k, 0805 (status LED series) | — | not picked |
| 2 | R3 | 330R, 0805 (underglow data series) | — | not picked |
| 2 | D1 | LED, 0805 | — | not picked |
| 18 | D2–D10 | TVS 5V unidirectional, 0805 | — | not picked |

**R4/R5 are DNP — do not order.**

## C. Shared through-hole (merged across both boards)

| Qty | Part | Breakdown |
|-----|------|-----------|
| **42** | Molex 43650-0300 (3-pin RA) | 40 panel (J8/J10) + 2 master (J1) |
| **22** | Screw terminal 2-pos 5.08mm | 20 panel (J9) + 2 master (J4) — **pitch unresolved** |

## D. Harness — mating connectors

Assumes stock SMX topology: 3 power columns of 3 panels, RS-485 serpentine daisy-chain
master → 0 → 3 → 6 → 7 → 4 → 1 → 2 → 5 → 8, INT home-run ×9. **Per pad, ×2 pads.**

| Qty | Part | Derivation |
|-----|------|-----------|
| 30 | Micro-Fit 3.0 receptacle housing, 2-circuit (43645-0200) | 5 per power column × 3 columns × 2 pads |
| 36 | Micro-Fit 3.0 receptacle housing, 3-circuit (43645-0300) | 9 RS-485 segments × 2 ends × 2 pads |
| **200+** | Micro-Fit 3.0 female crimp terminals (43030 series) | 132 needed; buy bulk, crimping has a learning curve |

Crimp terminal suffix depends on wire gauge — verify against 43030 datasheet before ordering.
RS-485 3-pin connectors populate only 2 circuits (3rd deliberately empty as a keying feature).

## E. Wire

| Signal | Spec | Notes |
|--------|------|-------|
| 12V power | 2×20 AWG jacketed | 3 columns × 2 pads; PSU end is fork/spade lugs |
| RS-485 | 22–24 AWG twisted pair | 9 segments per pad |
| INT | 24 AWG, **9 distinct colours** | Red, Orange, Yellow, Green, Blue, Brown, Grey, White, Black — stock SMX map, feeds panel-ID mismatch detection |
| Master GND tie | 1 lead to PSU GND stud | **mandatory** — not optional wiring |

**Approach (decided 2026-07-22): pick the wire *type* now, order a placeholder length, refine
once the pad is measured.** Lengths depend on pad geometry and are not yet known.

### Notes that affect the type choice (not the length)

- **Stranded, not solid.** A dance pad vibrates and the panels get stepped on; solid conductor
  work-hardens at flex points and eventually cracks. This matters most on the INT home-runs and
  RS-485, which route between moving panels.
- **Pick wire before crimps.** Molex 43030 terminals are specified by *both* conductor gauge and
  **insulation outer diameter**. A wire that is the right AWG but has fat insulation will not
  crimp correctly. Choose the wire, then select the 43030 suffix to match it.
- **Spool quantisation makes placeholder footage mostly moot** — hookup wire sells in fixed
  lengths (25 / 100 ft), so the order snaps to those increments regardless.
- **The 9-colour INT requirement is the awkward line.** Nine distinct colours × 2 pads. A
  multi-colour hookup wire assortment kit is likely cheaper and less wasteful than nine
  separate spools, since each colour only needs a couple of panel-to-master runs.
- **RS-485 needs actual twisted pair**, not two conductors run alongside each other — the
  twist is what gives the differential pair its noise rejection.

## F. Parts — RESOLVED 2026-07-23

All terminal-block and switch questions are settled; DigiKey cart built (see tmp/*.csv).
Footprints rebuilt from real drawings.

| Ref(s) | Part | DigiKey | Footprint status |
|--------|------|---------|------------------|
| master J2 | Molex 0395316009, 9-pos **pluggable** 5.08mm euro header + plug 0395337009 | WM25993-ND (hdr) + WM25575-ND (plug) | **rebuilt** `TerminalBlock_Molex_39531_1x09_P5.08mm_Vertical` (hole Ø1.60) |
| master J4 + panel J9 | Adam Tech MRR522-5.08-V, 2-pos 5.08mm screw terminal | 5245-MRR522-5.08-V-ND | **rebuilt** `TerminalBlock_MRR52-5.08-2P` (hole Ø1.50) |
| master SW1 | CUI DS01C-254-S-03BE (3-pos slide DIP) | 2223-DS01C-254-S-03BE-ND | stock `SW_DIP_SPSTx03` matches (2.54/7.62) |
| panel SW1 | CUI DS01C-254-S-04BE (4-pos slide DIP) | 2223-DS01C-254-S-04BE-ND | stock `SW_DIP_SPSTx04` matches |
| RN1 | Bourns 4610M-101-103LF (10k ×9 bussed SIP-10) | 4610M-101-103LF-ND | stock |
| D2–D10 | Littelfuse SMAJ5.0A (5V unidir TVS, DO-214AC) | SMAJ5.0ALFCT-ND | `Diode_SMD:D_SMA_Handsoldering` |
| SWD (panel J2) | Würth 61300311121 (3-pos 2.54mm header) | 732-5316-ND | stock, drop-in |
| Teensy socket | Sullins PPPC141LFBN-RC (14-pos, ×2/master) | S7047-ND | into existing holes |

**Chose pluggable for master J2** (all 9 INT wires detach as one block).

**Still needed:** FSR JST (B2B-PH-K, ×80) sourced elsewhere — out of stock at DigiKey; spade/fork
lugs for PSU power ends (hardware store); C1/C2/R1-R3/D1 datasheet URLs (commodity, low value).

## G. THT: hand-solder vs JLC assembly — analysis 2026-07-23

**Per-panel DigiKey THT parts ≈ $10.77 CAD → 20 panels ≈ $215.** Hand-soldering is ~57 joints
per panel → ~1140 joints for 20 panels (~10–19 hours).

The panel is already JLC **SMD** PCBA. The question is whether to also have JLC do the THT.

**Cost drivers against JLC THT:**
- **Part availability.** JLC assembles only LCSC-catalog parts. Micro-Fit (43650), JST-PH, and
  EG2201A are likely in LCSC; the **GCT USB4085, Adam Tech MRR52, Würth 61300311121, and CUI
  DS01C are probably not.** Non-catalog parts must be *consigned* — you buy them at DigiKey and
  ship them to China, adding handling fees, shipping, lead time, and risk. That is strictly worse
  than just hand-soldering them.
- **THT labor is priced to discourage.** JLC bills through-hole per joint/'hand-solder' (order
  ~$0.10–0.20/joint). At 57 joints/panel that is ~$6–11/panel of labor *on top of* parts.
- **Setup/engineering** fees rise with unique-part count.

**Rough JLC THT ≈ parts + ~$6–11/panel labor + consignment overhead ≈ $17–22/panel**, vs
DigiKey **$10.77/panel + your time**. Over 20 panels JLC adds roughly **$130–220** in labor/fees,
plus the consignment hassle for the ≥4 non-LCSC parts.

**Recommendation: hand-solder the THT yourself (as planned).** JLC THT is very likely more
expensive in dollars *and* logistically messy because several key connectors aren't LCSC parts.
JLC THT only wins if you value avoiding ~10–19 hours of soldering more than the extra cost — and
even then only if enough parts are LCSC-stocked to avoid consignment.

**To get exact numbers:** run a JLCPCB quote for the panel with THT assembly enabled and read
(a) which parts it flags unavailable, (b) the THT fee delta. That is the only way to a firm price.
A middle path exists — let JLC do only the LCSC-stocked THT (Micro-Fit/JST/EG2201A) and
hand-solder the oddballs — but it fragments the workflow for little gain.
