# Sourcing history (archived 2026-07-26)

Superseded material moved out of `docs/BOM.md` when that doc was rewritten as a
"what we're buying and from where" list derived from the boards. **Nothing here
is a live instruction.** It is kept because it is the record that justified the
current plan — the DigiKey prices are what the LCSC pivot was measured against,
and the AliExpress comparisons are why several parts are *not* bought there.

For what to actually order, see `docs/BOM.md`.

---

## G2. LCSC cross-reference (researched 2026-07-24)

**Why:** LCSC and JLCPCB orders **can be combined into one shipment** — place
both, then email `support@lcsc.com` with the two order numbers. Constraints:
same currency and same customer ID, cannot ship to Mainland China, cannot be
split/unbound afterwards, and shipping is recalculated (they invoice any
difference). If either order has already shipped, it's too late. That makes a
"parts ride along with the boards" strategy viable.

**⚠ Prices and stock are point-in-time and have burned us before — verify
live in the LCSC cart before committing.**

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

---

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
| ~~DPDT slide (panel SW3)~~ **SUPERSEDED 2026-07-25 — on the LCSC cart** as XKB SS22E01L5 (C609835); the AliExpress candidate and its reverse-engineered `SW_SS-22F04` footprint are both dropped. Original note: listing calls it SS-22H88 — [1005010555541589](https://www.aliexpress.com/item/1005010555541589.html); **the dimensional drawing on the listing is actually labelled `SS-22F04`** (verify which part actually ships) | **Footprint built 2026-07-25: `panel-pcb:SW_SS-22F04`** (from the listing's PCB-layout view — 6 pins 2×3, col pitch 3.0mm, row 3.2mm, legs 12.5mm apart, pin numbering matches SW_EG2201A so it's a drop-in for SW3). **Verify dims + drill against the physical part before ordering.** | EG2201A @ DigiKey (matches the *current* `SW_EG2201A` footprint) |
| ~~4-pos DIP (panel SW1)~~ | **SUPERSEDED 2026-07-25 — now on the LCSC cart** as Zhongdi DS-04 (**C52177925**, 30/$6.03). The 2026-07-24 "buy from DigiKey" call assumed no LCSC order existed to attach it to; the pivot created one | — | — |
| 3-pos DIP (master SW1) | **SUPERSEDED — on the LCSC cart** as DORABO DS-3P-BU (**C46595747**, 5/$1.23) | master ftpt = SPSTx03 W7.62 P2.54 — verified match | ~~DigiKey~~ |
| Micro-Fit 3p header RA | [1005008706326809](https://www.aliexpress.com/item/1005008706326809.html) | **RIGHT-ANGLE**, 3.0mm pitch = 43650-0300 | DigiKey |
| Micro-Fit 2p header RA | [1005012059959598](https://www.aliexpress.com/item/1005012059959598.html) | **RIGHT-ANGLE**, 3.0mm pitch = 43650-0200 | DigiKey |
| Micro-Fit crimps | [1005011606773268](https://www.aliexpress.com/item/1005011606773268.html) | Micro-Fit **3.0** (not Mini-Fit 4.2), covers 20 AWG | DigiKey |
| Micro-Fit 2p plug housing | [1005008919717941](https://www.aliexpress.com/item/1005008919717941.html) (marginal savings) | 3.0mm; same ecosystem as crimps | DigiKey |
| Micro-Fit 3p plug housing | — (AliExpress ≈ or > DigiKey) | — | **DigiKey (primary here)** |
| ~~FSR JST B2B-PH-K header~~ | **SUPERSEDED 2026-07-25 → LCSC C131337** (genuine JST, 100/$3.53, combined-ship). Old AliExpress pack [1005012304829514](https://www.aliexpress.com/item/1005012304829514.html) ($9.89 + $8.07 ship) was a clone at ~5× the cost | — | — |

Traps: (1) vertical vs **right-angle** on PCB headers; (2) clone-mixing across
housings/crimps. Micro-Fit clones are otherwise fine for this 5A hobby load.

---

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
