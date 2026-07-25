# Bill of Materials & Sourcing

One doc for everything purchasable: what each board needs, order quantities,
vendor picks, and the priced cart snapshot. Merged 2026-07-23 from the old
`BOM.md`, `DIGIKEY_SHOPPING_LIST.md`, and `BOM_PRICED.md`.

**Build scope: 2 pads = 2 master PCBs + 20 panel PCBs** (2 × 9 + 2 spares).
Per-board part identity (refs, MPNs, footprints) lives in the schematics and in
`docs/PANEL_PCB.md` / `docs/MASTER_PCB.md`; this doc is about *buying*.

**Assembly split (decided):**

- **Panel SMD → JLCPCB PCBA** (113 placements/board — 93 top / 20 bottom,
  DNP excluded; recounted from the PCB 2026-07-24). Part
  identity for those lines is the schematic's `LCSC` + `MPN` fields — the
  exported BOM/CPL auto-match at JLC. They never reach the bench and are not
  in the shopping lists below.
- **Panel through-hole → hand-solder** (~57 joints/panel; see the THT analysis
  at the bottom).
- **Master → bare PCB fab only, everything hand-assembled** (19 parts once —
  PCBA's ~$148 fixed overhead buys nothing). Parts still bought from LCSC (see
  strategy below), just hand-soldered rather than machine-placed.

## Sourcing strategy — four orders (revised 2026-07-25)

Supersedes the old "AliExpress primary + DigiKey fallback" plan. Building an
LCSC cart (2026-07-25) showed LCSC beats DigiKey by ~65% on everything it
carries and stocks the key connectors as **genuine Molex**, so LCSC became the
primary and DigiKey shrank to a residual. Four orders, each chosen for a
specific reason:

| Order | Gets | Why this vendor |
|-------|------|-----------------|
| **1. JLCPCB** | Both bare PCBs + **panel SMD assembly** (PCBA) | Only JLC does the panel's 113 SMD placements; the fab and assembly are one order. Master is bare-fab only (hand-assembled — PCBA's ~$148 fixed overhead buys nothing for 19 parts). |
| **2. LCSC** | Master semis + passives, all Micro-Fit (headers/housings/crimps), DIP + DPDT switches, screw terminals, RN1 array — the whole hand-solder electronics tier | **Cheapest by far (~$94.57 vs $267 DigiKey, ~65% off) and genuine Molex, not clones.** **Combined-shipped with the JLC order** (email `support@lcsc.com` with both order numbers) so it costs no extra freight. See §G2. |
| **3. DigiKey** | **Teensy 4.0 ×2 only** | Everything else migrated to LCSC (USB-C, sockets, passives all turned out to be there). Teensy is PJRC-direct — the one part LCSC never carries. Could also buy it from PJRC/Adafruit directly. |
| **4. AliExpress** | Euroblock (header+plug), cable/wire, lugs, ferrules, heatshrink, consumables | Cheapest for bulk/multiple commodity items and the parts neither LCSC nor DigiKey win on. Each candidate carries a match-check (§G) before ordering. |

**Rule of thumb for future parts:** price LCSC first (combined-ship makes it
nearly free freight), fall to DigiKey only for what LCSC can't supply, and use
AliExpress for cable and high-count commodity connectors. The per-part vendor
+ C-number breakdown is **§G2**; per-part reasoning is in that section's
tables.

---

## A. Panel PCB — through-hole, hand-soldered (× 20 boards)

| Qty | Ref/panel | Part | Per panel | MPN | Status |
|-----|-----------|------|-----------|-----|--------|
| 40 | J5, J11 | Micro-Fit 3.0 2-pin, right-angle THT (12V IN/OUT) | 2 | Molex **43650-0200** | decided |
| 40 | J8, J10 | Micro-Fit 3.0 3-pin, right-angle THT (RS-485 IN/OUT) | 2 | Molex **43650-0300** | decided |
| 80 | J3, J4, J6, J7 | JST-PH 2-pin vertical, board side (FSR) | 4 | JST **B2B-PH-K-S** | decided — **LCSC C131337** (genuine JST, 100/$3.53); footprint matches exactly. Stock FSR leads have PHR-2 plugs; **verify mating before ordering qty** |
| 20 | J1 | USB-C receptacle, 16P USB2.0, all-THT | 1 | GCT **USB4085-GF-A** | decided |
| 20 | J2 | Pin header 1×03, 2.54mm vertical (SWD) | 1 | Würth 61300311121 or generic | pick any |
| 20 | J9 | Screw terminal 2-pos 5.08mm (INT out) | 1 | Adam Tech **MRR522-5.08-V** | decided 2026-07-23 (footprint rebuilt from drawing; replaced the earlier KF301 pick) |
| 20 | SW1 | DIP slide switch, 4-pos SPST, 2.54mm, W7.62mm (panel ID) | 1 | CUI **DS01C-254-S-04BE** | **decided (source settled 2026-07-24: DigiKey, $0.70 ea — LCSC/AliExpress alternatives dropped)** |
| 20 | SW3 | Slide switch DPDT (RS-485 termination) | 1 | E-Switch **EG2201A** | decided (custom footprint `panel-pcb:SW_EG2201A`) |

TP1–TP14 are bare probe holes — no parts (TP13 `RS485+` / TP14 `RS485-` added
2026-07-24). SW2 (BOOTSEL) is SMD — JLC places
it, do not order.

Notes carried from the design phase:

- **RS-485 stays 3-pin with position 3 unpopulated** — with a 2-conductor
  cable it would otherwise be physically identical to 2-pin power; the
  different shell is what makes plugging 12V into the transceiver impossible.
- The 43650 series **is** the right-angle single-row Micro-Fit family (an
  earlier "43651" pointer was wrong); THT `-0200`/`-0300` chosen over the SMD
  `-0210`/`-0310` variants for plug-cycle/cable-yank robustness.
- FSR sensors themselves: **reuse stock SMX** (Interlink FSR 408 strips,
  iefsr.com if replacements are ever needed).

## B. Master PCB (× 2 boards, all hand-assembled)

| Qty | Ref | Part | MPN | DigiKey PN |
|-----|-----|------|-----|-----------|
| 2 | U1 | Teensy 4.0 | PJRC 15583 | 1568-15583-ND |
| 2 | U2 | RS-485 transceiver, SOIC-8 | THVD1429DR | 296-THVD1429DRCT-ND |
| 2 | U3 | Quad buffer 5V, DIP-14 | SN74AHCT125N | 296-4655-5-ND |
| 2 | RN1 | Resistor array 10k ×9 bussed, SIP-10 | Bourns 4610M-101-103LF | 4610M-101-103LF-ND |
| 2 | SW1 | DIP slide switch 3-pos (player ID) | CUI DS01C-254-S-03BE | 2223-DS01C-254-S-03BE-ND |
| 2 | J2 | Euroblock 9-pos 5.08mm pluggable, header | Molex 0395316009 | WM25993-ND |
| 2 | J2 | … matching plug | Molex 0395337009 | WM25575-ND |
| 2 | J1 | Micro-Fit 3.0 3-pin RA (RS-485 out) | 43650-0300 | (shared line, see C) |
| 2 | J4 | Screw terminal 2-pos 5.08mm (GND tie + underglow DATA) | MRR522-5.08-V | (shared line, see C) |
| 4 | — | Female header 14-pos 2.54mm (Teensy socket, 2/board) | PPPC141LFBN-RC | S7047-ND |
| 4 | C1, C2 | 100nF X7R 50V, 0805 | C0805F104K1RACAUTO | 399-C0805F104K1RACAUTOCT-ND |
| 2 | R1 | 120R 0805 (RS-485 termination) | RC0805FR-07120RL | 311-120CRCT-ND |
| 2 | R3 | 330R 0805 (underglow data series) | RC0805FR-07330RL | 311-330CRCT-ND |
| 18 | D2–D10 | TVS 5V unidirectional, DO-214AC/SMA (INT ESD) | SMAJ5.0A | SMAJ5.0ALFCT-ND |
| 18 | R6–R14 | 330R 0805 (INT series R / RC filter) | RC0805FR-07330RL | 311-330CRCT-ND |
| 18 | C3–C11 | 1nF C0G 0805 (INT filter cap) | KEMET C0805C102J5GACTU | 399-C0805C102J5GACTUCT-ND |
| 2 | R15 | 10k 0805 (underglow pull-down) | RC0805FR-0710KL | 311-10.0KCRCT-ND |

**R4/R5 (390R 1% RS-485 bias) are DNP — do not order.**

## C. Shared through-hole (merged across both boards)

| Qty | Part | Breakdown |
|-----|------|-----------|
| **42** | Molex 43650-0300 (3-pin RA) | 40 panel (J8/J10) + 2 master (J1) |
| **25** | MRR522-5.08-V (2-pos screw terminal) | 20 panel (J9) + 2 master (J4) + spares |

## D. Harness — mating connectors (per 2 pads)

Assumes stock SMX topology: 3 power columns of 3 panels, RS-485 serpentine
daisy-chain master → 0 → 3 → 6 → 7 → 4 → 1 → 2 → 5 → 8, INT home-run ×9.

| Qty | Part | Derivation |
|-----|------|-----------|
| 30 | Micro-Fit 3.0 receptacle housing 2-ckt (43645-0200) | 5 per power column × 3 columns × 2 pads |
| 36 | Micro-Fit 3.0 receptacle housing 3-ckt (43645-0300) | 9 RS-485 segments × 2 ends × 2 pads |
| 200+ | Micro-Fit 3.0 female crimps (43030 series) | 132 needed; buy bulk — crimping has a learning curve |

- Crimp suffix depends on **both wire gauge and insulation OD** — pick the wire
  first, then the 43030 variant to match. Verify against the 43030 datasheet.
- RS-485 3-pin housings populate only 2 circuits (keying).
- Buy mating housing + crimps from the **same ecosystem/source** — mixing clone
  crimps into a different clone housing risks seat/latch failures.
- Power feed cables crimp only the panel end (PSU end is fork/spade lugs).

Also needed: fork/spade lugs for the 3 column feeds' PSU ends + 1 for the
master GND tie (match the PSU stud size — teardown item); wire ferrules
(~0.25mm² for 24 AWG) for INT screw terminations, ~30 per pad — assortment box.

## E. Wire (types decided 2026-07-22; lengths placeholder until pad is measured)

General spec, all runs: **stranded pure copper** (never solid — vibration/flex
work-hardening; never CCA), PVC insulation (UL1007-class), any ≥80°C/300V
rating. Shielding is **not required** — RS-485 is differential, INT is
filtered open-drain, and shield buys nothing at <3m / 1 Mbps.

**RS-485 shield decision (settled 2026-07-24, do not re-litigate):** the
sourced cable *is* shielded, because jacketed twisted pair **without** a
shield is effectively unavailable on AliExpress. The drain is left
**unconnected at both ends**, and that is electrically fine here: the shield
couples symmetrically to both conductors of a balanced pair, so what it picks
up appears as common mode and the receiver rejects it. The "never leave a
shield floating" rule is EMC-certification guidance about radiated emissions
and quarter-wave resonance on long cables — neither applies to a 3m hobby run
at 1 Mbps with no emissions requirement.

- **The one real risk is an *intermittent* shield**, not a floating one. Trim
  the drain flush and heatshrink over it at both ends so it can never
  intermittently touch a connector pin or the frame.
- If the bench ever shows noise, landing the drain on the master's GND tie
  (one end only, never both) is a five-minute change.

| Signal | Spec | Notes |
|--------|------|-------|
| 12V power | 2×20 AWG jacketed round, red/black | 3 columns × 2 pads; ~5m/pad |
| RS-485 | 22–24 AWG **actual twisted pair** | 9 segments/pad, ~5m/pad; fix an A/B color convention and never deviate. **Shielded cable is fine and is what's sourced** — see the shield note below |
| INT | 24 AWG, **9 distinct colors** | ~7–10m/pad total. Stock SMX color map (confirmed against the pad): 0=Red 1=Orange 2=Yellow 3=Green 4=Blue 5=Brown 6=Grey 7=White 8=Black — feeds panel-ID mismatch detection |
| Master GND tie | 1 lead (18 AWG on hand) to PSU GND stud | **mandatory** — not optional wiring |

Spool quantization (25/100 ft) makes placeholder footage mostly moot. The
9-color INT requirement is the awkward line — a multi-color assortment kit
beats nine spool minimums. Supporting bits: heatshrink assortment, zip-tie
anchor points near each connector, grommets where cable crosses frame metal,
service-loop slack so a panel can be lifted out while connected.

---

## F. Priced DigiKey cart — snapshot 2026-07-22, CAD (mostly SUPERSEDED)

> **⚠ Historical / fallback only.** As of 2026-07-25 most of these lines move
> to **LCSC (§G2)** at ~65% less, or to AliExpress. Keep this as the DigiKey
> price reference and the fallback if an LCSC/AliExpress part fails its check,
> but **do not build the actual order from here** — the live plan is the
> four-order strategy at the top. Only Teensy, USB-C, and Teensy sockets are
> still genuinely DigiKey lines.

The **Unit** column is what to compare against other vendors. This snapshot is
the DigiKey / hand-solder side only (panel SMD is JLC).

### Master electronics (×2 boards) — subtotal **153.48**

Passives updated 2026-07-24 for the post-review rework (status LED D1/R2 gone;
INT RC filter R6–R14 + C3–C11 and underglow pull-down R15 added). Resistor
unit price is the 0805 thick-film cut-tape rate from the same snapshot.

| Part | MPN | Qty | Unit $ | Ext $ |
|------|-----|-----|--------|-------|
| Teensy 4.0 | 15583 | 2 | 36.260 | 72.52 |
| THVD1429DR | THVD1429DR | 2 | 7.150 | 14.30 |
| SN74AHCT125N | SN74AHCT125N | 2 | 1.520 | 3.04 |
| 10k ×9 array | 4610M-101-103LF | 2 | 1.330 | 2.66 |
| DIP 3-pos | DS01C-254-S-03BE | 2 | 1.010 | 2.02 |
| Euro header 9-pos | 0395316009 | 2 | 5.400 | 10.80 |
| Euro plug 9-pos | 0395337009 | 2 | 13.980 | 27.96 |
| 100nF 0805 | C0805F104K1RACAUTO | 4 | 0.360 | 1.44 |
| 1nF C0G 0805 (INT filter) | C0805C102J5GACTU | 18 | 0.070 | 1.26 |
| 120R 0805 | RC0805FR-07120RL | 2 | 0.160 | 0.32 |
| 330R 0805 (R3 + R6–R14) | RC0805FR-07330RL | 20 | 0.160 | 3.20 |
| 10k 0805 (R15) | RC0805FR-0710KL | 2 | 0.160 | 0.32 |
| TVS SMAJ5.0A | SMAJ5.0A | 18 | 0.431 | 7.76 |
| Teensy socket 14-pos | PPPC141LFBN-RC | 4 | 1.470 | 5.88 |

### Panel THT (×20 boards) + shared — subtotal **208.06**

| Part | MPN | Qty | Unit $ | Ext $ |
|------|-----|-----|--------|-------|
| Micro-Fit 2pin RA | 0436500200 | 40 | 1.370 | 54.80 |
| Micro-Fit 3pin RA | 0436500300 | 42 | 1.551 | 65.14 |
| USB-C USB4085-GF-A | USB4085-GF-A | 20 | 1.158 | 23.16 |
| Terminal 2-pos MRR52 | MRR522-5.08-V | 25 | 0.674 | 16.85 |
| DIP 4-pos | DS01C-254-S-04BE | 20 | 0.700 | 14.00 |
| DPDT EG2201A | EG2201A | 20 | 1.529 | 30.58 |
| Pin header 3-pos | 61300311121 | 25 | 0.141 | 3.53 |

### Harness — subtotal **50.27**

| Part | MPN | Qty | Unit $ | Ext $ |
|------|-----|-----|--------|-------|
| Micro-Fit hsg 2pos | 0436450200 | 30 | 0.470 | 14.10 |
| Micro-Fit hsg 3pos | 0436450300 | 36 | 0.496 | 17.87 |
| Micro-Fit crimp 20-24AWG | 0430300001 | 200 | 0.091 | 18.30 |

### Wire (PLACEHOLDER lengths) — subtotal **147.55**

| Part | MPN | Qty (ft) | Ext $ | Note |
|------|-----|----------|-------|------|
| 2C 20AWG jacketed | 5400FE 008500 | 10 | 14.46 | this Belden PN is shielded/premium — sub a plain jacketed 2C 20AWG |
| 2C 22AWG twisted RS-485 | 8761 06010000 | 10 | 41.57 | premium; any 22–24AWG twisted pair works |
| 24AWG stranded ×9 colors | (9 lines) | 90 | 91.52 | assortment kit likely beats 9 spools |

### Totals

| Section | CAD |
|---------|-----|
| Master electronics | 153.48 |
| Panel THT | 208.06 |
| Harness | 50.27 |
| Wire | 147.55 |
| **GRAND TOTAL** | **559.36** |

Cost-shopping notes: wire is ~26% of the cart and all placeholder — the
biggest lever. Teensy 4.0 ($36) is the biggest single line and rarely
discounted (PJRC direct / Adafruit / SparkFun). Micro-Fit + crimps add up
across 20 panels — AliExpress/LCSC clones much cheaper at some QC risk. The
3-pin Micro-Fit (WM1861) was on backorder at cart time — check stock.

## G. AliExpress candidates (2026-07-23) — primary; DigiKey = fallback

Candidates are unverified against datasheets/footprints — **each has a
match-check that must pass before ordering.** DigiKey PNs above remain the
fallback for anything that fails.

### Cable

| Run | Candidate | Price | Match-check |
|-----|-----------|-------|-------------|
| 12V power | PVC 2C 20AWG oxygen-free tinned copper — [1005008621580316](https://www.aliexpress.com/item/1005008621580316.html) | ~$14.68/10m + $9.39 ship | 20 AWG (not 22/24), stranded, 2-cond jacketed round |
| RS-485 | 22AWG shielded twisted pair — [1005006546939974](https://www.aliexpress.com/item/1005006546939974.html) | ~$20.92/10m free ship | genuine twisted pair, pure copper; **leave shield/drain unconnected at BOTH ends — deliberate, see §E** |
| INT + hookup | 10-color 24AWG stranded pack — [1005008982254390](https://www.aliexpress.com/item/1005008982254390.html) | ~$16.44 free ship | **stranded not solid**, colors cover the 9-panel map, pure copper |

### Connectors & switches

| Part / use | Candidate | Match-check | Fallback |
|-----------|-----------|-------------|----------|
| Euroblock 9p (master J2), header+plug | pack of 5 — [1005012001482158](https://www.aliexpress.com/item/1005012001482158.html) | 5.08mm pitch, 9-pos, single-row (master ftpt = Molex 39531 P5.08) | DigiKey |
| DPDT slide (panel SW3) | listing calls it SS-22H88 — [1005010555541589](https://www.aliexpress.com/item/1005010555541589.html); **the dimensional drawing on the listing is actually labelled `SS-22F04`** (verify which part actually ships) | **Footprint built 2026-07-25: `panel-pcb:SW_SS-22F04`** (from the listing's PCB-layout view — 6 pins 2×3, col pitch 3.0mm, row 3.2mm, legs 12.5mm apart, pin numbering matches SW_EG2201A so it's a drop-in for SW3). **Verify dims + drill against the physical part before ordering.** | EG2201A @ DigiKey (matches the *current* `SW_EG2201A` footprint) |
| ~~4-pos DIP (panel SW1)~~ | **DROPPED 2026-07-24 — buy from DigiKey** (CUI DS01C-254-S-04BE, $0.70 ea, in stock). No LCSC order exists to attach the $0.12 alternative to, so a separate shipment would cost more than it saves | — | — |
| 3-pos DIP (master SW1) | (use panel 4-pos + re-foot master, OR buy 3-pos) | master ftpt = SPSTx03 W7.62 P2.54 | **DigiKey (primary — only need 2)** |
| Micro-Fit 3p header RA | [1005008706326809](https://www.aliexpress.com/item/1005008706326809.html) | **RIGHT-ANGLE**, 3.0mm pitch = 43650-0300 | DigiKey |
| Micro-Fit 2p header RA | [1005012059959598](https://www.aliexpress.com/item/1005012059959598.html) | **RIGHT-ANGLE**, 3.0mm pitch = 43650-0200 | DigiKey |
| Micro-Fit crimps | [1005011606773268](https://www.aliexpress.com/item/1005011606773268.html) | Micro-Fit **3.0** (not Mini-Fit 4.2), covers 20 AWG | DigiKey |
| Micro-Fit 2p plug housing | [1005008919717941](https://www.aliexpress.com/item/1005008919717941.html) (marginal savings) | 3.0mm; same ecosystem as crimps | DigiKey |
| Micro-Fit 3p plug housing | — (AliExpress ≈ or > DigiKey) | — | **DigiKey (primary here)** |
| ~~FSR JST B2B-PH-K header~~ | **SUPERSEDED 2026-07-25 → LCSC C131337** (genuine JST, 100/$3.53, combined-ship). Old AliExpress pack [1005012304829514](https://www.aliexpress.com/item/1005012304829514.html) ($9.89 + $8.07 ship) was a clone at ~5× the cost | — | — |

Traps: (1) vertical vs **right-angle** on PCB headers; (2) clone-mixing across
housings/crimps. Micro-Fit clones are otherwise fine for this 5A hobby load.

## G2. LCSC cross-reference (researched 2026-07-24)

**Why:** LCSC and JLCPCB orders **can be combined into one shipment** — place
both, then email `support@lcsc.com` with the two order numbers. Constraints:
same currency and same customer ID, cannot ship to Mainland China, cannot be
split/unbound afterwards, and shipping is recalculated (they invoice any
difference). If either order has already shipped, it's too late. That makes a
"parts ride along with the boards" strategy viable.

**⚠ Prices and stock are point-in-time and have burned us before — verify
live in the LCSC cart before committing.**

### The actual LCSC cart (built 2026-07-25, `tmp/export_project_20260725_100317.xls`)

**EST TOTAL $94.57** for 16 lines — and most lines include spares (either
free headroom or the LCSC minimum/multiple). The comparable DigiKey lines, at
*exact* quantities needed, came to **$267.21**, so this saves **~$173 (65%)
while buying MORE parts.** The Micro-Fit family alone drops from **$170 →
$55**, and LCSC stocks it as **genuine Molex**, not clones. This clearly beats
DigiKey for everything it covers; DigiKey shrinks to just the parts LCSC
doesn't carry.

**Drop-in — genuine/standard package, footprint unchanged:**

| Ref | Part | LCSC | Cart qty | Ext $ | Need |
|-----|------|------|----------|-------|------|
| master D2–D10 | SMAJ5.0A TVS | **C113952** (MDD) | 40 | 1.64 | 18 |
| master U3 | SN74AHCT125N DIP-14 | **C354152** (TI) | 5 | 3.04 | 2 |
| master C1/C2 | 100nF X7R 0805 | **C83055** (Walsin) | 10 | 0.24 | 4 |
| master C3–C11 | 1nF C0G 0805 | **C1791** (Samsung) | 30 | 0.35 | 18 |
| master R3,R6–R14 | 330R 1% 0805 | **C844839** (Vishay) | 50 | 0.75 | 20 |
| master U2 | THVD1429DR SOIC-8 | **C1850236** (TI) | 4 | 16.28 | 2 |
| panel J8/J10 + master J1 | Micro-Fit 3p RA header | **C503478** = Molex **436500300** | 50 | 21.10 | 42 |
| panel J5/J11 | Micro-Fit 2p RA header | **C192562** = Molex **436500200** | 50 | 19.93 | 40 |
| harness | Micro-Fit 2p housing | **C114089** = Molex 436450200 | 50 | 3.79 | 30 |
| harness | Micro-Fit 3p housing | **C259740** = Molex 436450300 | 50 | 5.68 | 36 |
| harness | Micro-Fit crimp 20–24AWG | **C259786** = Molex 430300001 | 300 | 4.47 | 132 |
| panel J3/J4/J6/J7 | JST **B2B-PH-K-S** 2p 2mm vertical THT | **C131337** (genuine JST) | 100 | 3.53 | 72 (80 w/ spares) |

The JST row is a **guaranteed mate**: it's genuine JST B2B-PH-K-S and the
footprint (`Connector_JST:JST_PH_B2B-PH-K_1x02_P2.00mm_Vertical`) was drawn
for exactly that part. It replaces the AliExpress 100-pc pack (~$18 shipped,
*clone* with a mate-risk) — cheaper, genuine, and combined-shipped free.
Still worth the one-time physical check that a stock FSR lead's PHR-2 plug
seats, but that's now confirming the real part, not a substitute.

**Footprint WILL change (user accepted 2026-07-25) — build/verify before order:**

| Ref | LCSC part | Cart | Ext $ | Footprint action |
|-----|-----------|------|-------|------------------|
| master RN1 | **C840655** Bourns **4610X**-101-103LF | 5 | 1.66 | Was 4610**M**. Both are bussed 10k×9 SIP-10 (9 res, pin 1 = common) — same land pattern; just **confirm pin-1-common orientation** against the master `R_Network09` symbol. Low risk. |
| panel SW1 | **C52177925** Zhongdi **DS-04** | 30 | 6.03 | 4-pos DIP slide. Verify 2.54mm pitch / 7.62mm row width vs `SW_DIP_SPSTx04...W7.62`. Supersedes the CUI DigiKey pick settled 2026-07-24. |
| master SW1 | **C46595747** DORABO **DS-3P-BU** | 5 | 1.23 | 3-pos DIP slide. Verify vs `SW_DIP_SPSTx03...W7.62`. |
| panel J9 + master J4 | **C8465** KANGNEX **WJ500V-5.08-2P** | 30 | 4.01 | Different maker from Adam Tech MRR52. 5.08mm pitch matches; **check body/pin Ø against `TerminalBlock_MRR52-5.08-2P` (1.5mm holes)** — likely compatible, verify. Affects BOTH boards. |
| panel SW3 | **C609835** XKB **SS22E01L5** (DPDT, 11×6.2mm) | 25 | 4.37 | **Needs a NEW footprint** — matches neither `SW_EG2201A` nor the `SW_SS-22F04` built 2026-07-25 (that one is now likely moot). Pull the EasyEDA/JLC footprint or build from the SS22E01 datasheet (6 pins 2×3, get pitch/row from the drawing). |
| panel J1 ×20 | USB-C, THT (see options below) | — | — | **Needs a NEW footprint** — any of these is a different land pattern from the GCT USB4085 the custom `USB_C_Receptacle_GCT_USB4085_EdgeTrim` was drawn for. USB-C is the highest-risk footprint to hand-build (16-pin mapping + CC/D+/D−/shield tabs) — **pull the EasyEDA/JLC footprint**, don't reverse-engineer. Must re-map to the same J1 schematic pins (A6/B6=D+, A7/B7=D−, A5=CC1, B5=CC2, VBUS/GND/SBU as-is). |

USB-C options found on LCSC (2026-07-25 — correcting the earlier "LCSC has no
all-THT USB-C" claim, which was wrong):
- **C53184807** LCKELEC LCK-TCF829D — **vertical**, THT, 30/$7.70. **Preferred:**
  cheaper, matches the original vertical intent, height clears the ~35mm panel
  budget easily.
- C49302689 G-Switch GT-USB-7107B — right-angle, THT, 30/$15.11. 2× the price.

### Newly confirmed LCSC parts (2026-07-25) — drop-in, footprint unchanged

| Ref | Part | LCSC | Cart | $ | Note |
|-----|------|------|------|---|------|
| master R1 | 120R 1% 0805 (Vishay CRCW0805120RFKEA) | **C844816** | 100 | 0.86 | verified 0805 1% |
| master R15 | 10k 1% 0805 (Yageo AF0805FR-0710KL) | **C192906** | 50 | 0.90 | verified 0805 1% |
| panel J2 (SWD) | male 1×3 2.54mm header (XFCN PZ254V-11-03P) | **C2937625** | 50 | 0.94 | matches the SWD pin-header footprint |
| master U1 socket ×4 | **female 1×14 2.54mm THT** socket (KH-2.54FH-1X14P-H8.5) | **C2905420** | 10 | 1.52 | correct Teensy socket (2/board); fits the `Teensy40_Socketed` pin holes |

### Still NOT in the LCSC cart — source elsewhere

| Ref | Part | Where | Note |
|-----|------|-------|------|
| master U1 ×2 | Teensy 4.0 | **DigiKey / PJRC** | $72.52, PJRC-direct, unavoidable — **the only thing left on the DigiKey order** |
| master J2 ×2 | Euroblock 9-pos header + plug | **AliExpress** | cheapest there (§G, pack of 5); the DigiKey Molex was a fallback only |
| wire / lugs / ferrules / heatshrink | — | **AliExpress** / hand | §E, §D |

### Four-order plan (confirmed viable)

1. **JLCPCB** — both bare PCBs + panel PCBA (SMD placement from the panel BOM/CPL).
2. **LCSC** — this cart (~$94.57), **combined-shipped with the JLC order** (email `support@lcsc.com` with both order numbers).
3. **DigiKey** — **Teensy 4.0 ×2 only** (PJRC-direct). Everything else migrated to LCSC.
4. **AliExpress** — Euroblock (header+plug), cable/wire, lugs, ferrules, heatshrink, consumables.

### Order quantities — per pad vs 2 pads

All harness/panel "qty needed" derivations are per pad (1 master + 9 panels);
double for the 2-pad build except where one pack covers both:

| Item | Per pad | 2 pads | Note |
|------|---------|--------|------|
| Micro-Fit 2p header | 18 | 36 | ×2 |
| Micro-Fit 3p header | 19 | 38 | ×2 |
| Micro-Fit 2p plug housing | 15 | 30 | ×2 |
| Micro-Fit 3p plug housing | 18 | 36 | ×2 |
| Micro-Fit crimps | 66 (buy ~100) | 132 (buy ~200) | ×2 |
| Panel DIP 4-pos | 9 | 18 | two 10-packs |
| DPDT | 9 | 18 | ×2 |
| FSR JST header | 36 | 72 | **one 100-pc pack covers both** |
| Master DIP 3-pos | 1 | 2 | trivial |
| Euroblock 9p | 1 | 2 | **pack of 5 covers both** |
| Power cable | ~5m | ~10m | 10m reel tight — buy 2 |
| RS-485 cable | ~5m | ~10m | buy 2 reels |
| Hookup 10-color pack | ~1 pad | ~2 pads | **one pack covers 2** |

## H. Not on any order — have on hand or source elsewhere

| Item | Status |
|------|--------|
| FSR sensors | reuse stock SMX (Interlink FSR 408) |
| FSR JST headers (B2B-PH-K-S) | **SOURCED — LCSC C131337** (genuine JST, 100/$3.53, combined-ship; verify PHR-2 mate before qty buy) |
| 18 AWG stranded (underglow/GND-tie) | have at home |
| Spade/fork lugs (PSU ends + master GND tie) | have at home (size vs PSU stud = teardown item) |
| M3 mounting hardware | have at home |
| Master enclosure | future — 3D-print from KiCad 3D export once boards are in hand |

## I. Panel THT: hand-solder vs JLC assembly (analysis 2026-07-23)

Per-panel DigiKey THT ≈ $10.77 CAD → 20 panels ≈ $215; ~57 joints/panel →
~1140 joints (~10–19 hours). JLC THT would add ~$6–11/panel labor plus
consignment overhead — several key parts (GCT USB4085, Adam Tech MRR52, Würth
header, CUI DIPs) are likely not in the LCSC catalog and would have to be
bought at DigiKey and shipped to China.

**Recommendation: hand-solder the THT (as planned).** JLC THT ≈ $17–22/panel
vs $10.77 + time; over 20 panels JLC adds roughly $130–220 plus logistics. To
get exact numbers, run a JLC quote with THT enabled and read which parts it
flags unavailable + the fee delta. A middle path (JLC does only LCSC-stocked
THT) fragments the workflow for little gain.

## J. Passthrough boards (deferred)

The passive passthrough PCB concept (connector-only boards for sub-9-panel
kits, see `docs/MODULAR_PANEL_COUNT.md`) is same-PCB/different-BOM — no
separate design. The current 2-pad order quantities above do **not** include
passthrough builds; add connector/termination quantities if any are ordered.
