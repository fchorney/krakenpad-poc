# Bill of Materials & Sourcing

One doc for everything purchasable: what each board needs, order quantities,
vendor picks, and the priced cart snapshot. Merged 2026-07-23 from the old
`BOM.md`, `DIGIKEY_SHOPPING_LIST.md`, and `BOM_PRICED.md`.

**Build scope: 2 pads = 2 master PCBs + 20 panel PCBs** (2 × 9 + 2 spares).
Per-board part identity (refs, MPNs, footprints) lives in the schematics and in
`docs/PANEL_PCB.md` / `docs/MASTER_PCB.md`; this doc is about *buying*.

**Assembly split (decided):**

- **Panel SMD → JLCPCB PCBA** (110 placements/board, double-sided). Part
  identity for those lines is the schematic's `LCSC` + `MPN` fields — the
  exported BOM/CPL auto-match at JLC. They never reach the bench and are not
  in the shopping lists below.
- **Panel through-hole → hand-solder** (~57 joints/panel; see the THT analysis
  at the bottom).
- **Master → bare PCB fab only, everything hand-assembled** (19 parts once —
  PCBA's ~$148 fixed overhead buys nothing; sourcing unconstrained by LCSC
  stock).

**Sourcing strategy (2026-07-23): AliExpress primary for panel-multiple parts
and cable** (9×/20× multiples where bulk packs pay off), **DigiKey for
master-only parts** (1–2 needed, unit price irrelevant) **and as fallback** for
any AliExpress candidate that fails its match-check.

---

## A. Panel PCB — through-hole, hand-soldered (× 20 boards)

| Qty | Ref/panel | Part | Per panel | MPN | Status |
|-----|-----------|------|-----------|-----|--------|
| 40 | J5, J11 | Micro-Fit 3.0 2-pin, right-angle THT (12V IN/OUT) | 2 | Molex **43650-0200** | decided |
| 40 | J8, J10 | Micro-Fit 3.0 3-pin, right-angle THT (RS-485 IN/OUT) | 2 | Molex **43650-0300** | decided |
| 80 | J3, J4, J6, J7 | JST-PH 2-pin vertical, board side (FSR) | 4 | JST **B2B-PH-K-S** | decided — stock FSR leads already have PHR-2 plugs; **verify mating before ordering qty** |
| 20 | J1 | USB-C receptacle, 16P USB2.0, all-THT | 1 | GCT **USB4085-GF-A** | decided |
| 20 | J2 | Pin header 1×03, 2.54mm vertical (SWD) | 1 | Würth 61300311121 or generic | pick any |
| 20 | J9 | Screw terminal 2-pos 5.08mm (INT out) | 1 | Adam Tech **MRR522-5.08-V** | decided 2026-07-23 (footprint rebuilt from drawing; replaced the earlier KF301 pick) |
| 20 | SW1 | DIP slide switch, 4-pos SPST, 2.54mm, W7.62mm (panel ID) | 1 | CUI **DS01C-254-S-04BE** | decided |
| 20 | SW3 | Slide switch DPDT (RS-485 termination) | 1 | E-Switch **EG2201A** | decided (custom footprint `panel-pcb:SW_EG2201A`) |

TP1–TP12 are bare probe holes — no parts. SW2 (BOOTSEL) is SMD — JLC places
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
| 2 | R2 | 1k 0805 (status LED) | CRCW08051K00FKEA | 541-1.00KCCT-ND |
| 2 | R3 | 330R 0805 (underglow data series) | RC0805FR-07330RL | 311-330CRCT-ND |
| 2 | D1 | LED blue 0805 | 150080BS75000 | 732-4982-1-ND |
| 18 | D2–D10 | TVS 5V unidirectional, DO-214AC/SMA (INT ESD) | SMAJ5.0A | SMAJ5.0ALFCT-ND |

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
work-hardening; never CCA), **unshielded** (RS-485 is differential, INT is
filtered open-drain; shield buys nothing at <3m / 1 Mbps), PVC insulation
(UL1007-class), any ≥80°C/300V rating.

| Signal | Spec | Notes |
|--------|------|-------|
| 12V power | 2×20 AWG jacketed round, red/black | 3 columns × 2 pads; ~5m/pad |
| RS-485 | 22–24 AWG **actual twisted pair** | 9 segments/pad, ~5m/pad; fix an A/B color convention and never deviate |
| INT | 24 AWG, **9 distinct colors** | ~7–10m/pad total. Stock SMX color map (confirmed against the pad): 0=Red 1=Orange 2=Yellow 3=Green 4=Blue 5=Brown 6=Grey 7=White 8=Black — feeds panel-ID mismatch detection |
| Master GND tie | 1 lead (18 AWG on hand) to PSU GND stud | **mandatory** — not optional wiring |

Spool quantization (25/100 ft) makes placeholder footage mostly moot. The
9-color INT requirement is the awkward line — a multi-color assortment kit
beats nine spool minimums. Supporting bits: heatshrink assortment, zip-tie
anchor points near each connector, grommets where cable crosses frame metal,
service-loop slack so a panel can be lifted out while connected.

---

## F. Priced DigiKey cart — snapshot 2026-07-22, CAD

The **Unit** column is what to compare against other vendors. This snapshot is
the DigiKey / hand-solder side only (panel SMD is JLC).

### Master electronics (×2 boards) — subtotal **149.94**

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
| 120R / 1k / 330R 0805 | (3 lines) | 6 | 0.160 | 0.96 |
| LED blue 0805 | 150080BS75000 | 2 | 0.300 | 0.60 |
| TVS SMAJ5.0A | SMAJ5.0A | 18 | 0.431 | 7.76 |
| Teensy socket 14-pos | PPPC141LFBN-RC | 4 | 1.470 | 5.88 |

### Panel THT (×20 boards) + shared — subtotal **212.10**

| Part | MPN | Qty | Unit $ | Ext $ |
|------|-----|-----|--------|-------|
| Micro-Fit 2pin RA | 0436500200 | 40 | 1.370 | 54.80 |
| Micro-Fit 3pin RA | 0436500300 | 42 | 1.551 | 65.14 |
| USB-C USB4085-GF-A | USB4085-GF-A | 20 | 1.158 | 23.16 |
| Terminal 2-pos MRR52 | MRR522-5.08-V | 25 | 0.674 | 16.85 |
| DIP 4-pos | DS01C-254-S-04BE | 20 | 0.902 | 18.04 |
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
| Master electronics | 149.94 |
| Panel THT | 212.10 |
| Harness | 50.27 |
| Wire | 147.55 |
| **GRAND TOTAL** | **559.86** |

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
| RS-485 | 22AWG shielded twisted pair — [1005006546939974](https://www.aliexpress.com/item/1005006546939974.html) | ~$20.92/10m free ship | genuine twisted pair, pure copper; **leave shield/drain unconnected** |
| INT + hookup | 10-color 24AWG stranded pack — [1005008982254390](https://www.aliexpress.com/item/1005008982254390.html) | ~$16.44 free ship | **stranded not solid**, colors cover the 9-panel map, pure copper |

### Connectors & switches

| Part / use | Candidate | Match-check | Fallback |
|-----------|-----------|-------------|----------|
| Euroblock 9p (master J2), header+plug | pack of 5 — [1005012001482158](https://www.aliexpress.com/item/1005012001482158.html) | 5.08mm pitch, 9-pos, single-row (master ftpt = Molex 39531 P5.08) | DigiKey |
| DPDT slide (panel SW3) | SS-22H88 — [1005010555541589](https://www.aliexpress.com/item/1005010555541589.html) | **⚠️ different footprint from `SW_EG2201A`** — only viable if the panel footprint is changed first | EG2201A @ DigiKey (matches current ftpt) |
| 4-pos DIP (panel SW1) | [1005009296124199](https://www.aliexpress.com/item/1005009296124199.html) or 10-pk [1005005866801107](https://www.aliexpress.com/item/1005005866801107.html) | 4-pos, 2.54mm pitch, 7.62mm (0.3") wide | DigiKey |
| 3-pos DIP (master SW1) | (use panel 4-pos + re-foot master, OR buy 3-pos) | master ftpt = SPSTx03 W7.62 P2.54 | **DigiKey (primary — only need 2)** |
| Micro-Fit 3p header RA | [1005008706326809](https://www.aliexpress.com/item/1005008706326809.html) | **RIGHT-ANGLE**, 3.0mm pitch = 43650-0300 | DigiKey |
| Micro-Fit 2p header RA | [1005012059959598](https://www.aliexpress.com/item/1005012059959598.html) | **RIGHT-ANGLE**, 3.0mm pitch = 43650-0200 | DigiKey |
| Micro-Fit crimps | [1005011606773268](https://www.aliexpress.com/item/1005011606773268.html) | Micro-Fit **3.0** (not Mini-Fit 4.2), covers 20 AWG | DigiKey |
| Micro-Fit 2p plug housing | [1005008919717941](https://www.aliexpress.com/item/1005008919717941.html) (marginal savings) | 3.0mm; same ecosystem as crimps | DigiKey |
| Micro-Fit 3p plug housing | — (AliExpress ≈ or > DigiKey) | — | **DigiKey (primary here)** |
| FSR JST B2B-PH-K header | JST-PH kits, cheap 50-pc bags | **B2B-PH-K top-entry 2.0mm**; verify a stock PHR-2 FSR lead seats before buying qty. Cable side NOT needed (reuse stock FSR leads) | DigiKey (**was out of stock** — that's why it fell off the cart; still needs a source) |

Traps: (1) vertical vs **right-angle** on PCB headers; (2) clone-mixing across
housings/crimps. Micro-Fit clones are otherwise fine for this 5A hobby load.

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
| FSR JST header | 36 | 72 | ×2 |
| Master DIP 3-pos | 1 | 2 | trivial |
| Euroblock 9p | 1 | 2 | **pack of 5 covers both** |
| Power cable | ~5m | ~10m | 10m reel tight — buy 2 |
| RS-485 cable | ~5m | ~10m | buy 2 reels |
| Hookup 10-color pack | ~1 pad | ~2 pads | **one pack covers 2** |

## H. Not on any order — have on hand or source elsewhere

| Item | Status |
|------|--------|
| FSR sensors | reuse stock SMX (Interlink FSR 408) |
| FSR JST headers (B2B-PH-K-S) | **NEEDS A SOURCE** — 80 needed, out of stock at DigiKey |
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
