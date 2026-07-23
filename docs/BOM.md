# Bill of Materials (confirmed parts only)

Status: **layout-phase BOM** — parts confirmed by design decisions and bench testing as of
2026-07-16 (schematic v1.0 signed off 2026-07-11; footprints assigned). Items still under
discussion are in the "Not yet decided" section at the bottom — don't cart those yet.

Quantities: panel parts are **per panel**; build needs ×9 (order ~×12 for spares/assembly
losses on a first spin).

**All 9 panel PCBs are identical** (decided 2026-07-10) — any panel works in any position, so
every panel gets the full connector set (Power IN+OUT, RS-485 IN+OUT); a panel at the end of
a chain just leaves its OUT empty. No position-specific builds.

**Assembly: JLCPCB (decided 2026-07-10).** Prefer parts in LCSC stock — ideally JLC "basic"
parts — so the fab places as much as possible. Applies especially to the WS2815s (in LCSC
stock, 225 placements nobody wants to hand-solder), passives, and the RP2040 itself (LCSC
stocks it). DigiKey cart shrinks to whatever LCSC can't cover + bench spares.

## Panel PCB (×9)

| Part | Qty/panel | Source | Notes |
|------|-----------|--------|-------|
| RP2040 MCU (bare chip, QFN-56) | 1 | DigiKey | **Decided 2026-07-10: bare chip, not RP2040-Zero module.** Follow the official "Hardware design with RP2040" minimal design reference for the support circuit. |
| 12MHz crystal (per RP2040 reference design) + load caps | 1 | DigiKey | ABM8-272-T3 class per the reference design; pin exact PN at schematic time. |
| USB-C receptacle, 16-pin USB 2.0-only | 1 | DigiKey | **DECIDED 2026-07-11: GCT USB4085-GF-A** — horizontal all-through-hole (16 signal + 8 anchor TH positions), 20k mating cycles, stock KiCad footprint (assigned to J1). Orientation requirement dropped (bench-flashing-only port, panel top off); TH mounting is the load-bearing requirement. LCSC vertical "DIP" candidates all turned out SMD-vertical on inspection. Tie duplicated D+/D− pairs together. |
| 5.1kΩ resistor (CC1/CC2 pull-downs) | 2 | DigiKey | Required for VBUS with C-to-C cables — do not omit. |
| USBLC6-2SC6 (USB ESD array, SOT-23-6) | 1 | DigiKey | **Added 2026-07-17 (U7).** Clamps D+/D−/VBUS at the connector. Dance-pad rationale: shoe-on-rubber tribocharging, not just bench plug events. Connector side of the 27Ω series resistors, flow-through routed. |
| BOOTSEL tactile button | 1 | DigiKey | Grounds flash CS at boot for USB bootloader entry. |
| SWD header, 3-pin 2.54mm (SWDIO/GND/SWCLK) | 1 | DigiKey | Recovery/debug path independent of USB + firmware state. Can ship unpopulated. Probe side: Raspberry Pi Debug Probe or a Pico running picoprobe. |
| W25Q32JV (4MB QSPI flash, SOIC-8) | 1 | DigiKey | Required for RP2040 boot. Chosen over W25Q16JV (pin-compatible) for headroom. |
| THVD1429 (RS-485 transceiver, SOIC-8) | 1 | DigiKey | 3.3V-logic compatible. MAX3485 was breadboard substitute only. Do NOT buy MAX485. |
| SN74AHCT125D (level shifter, SOIC-14) | 1 | DigiKey | VCC = 5V rail. Drives WS2815 data line only. DIP-14 (SN74AHCT125N) was breadboard-only; schematic/PCB use the SOIC-14 footprint (U4). |
| AMS1117-5.0 (LDO, SOT-223) | 1 | DigiKey (genuine) | 12V→5V, feeds only the level shifter VCC. Amazon assortment chips are bench-test only. |
| AP7361C-33ER-13 (LDO, SOT-223R) | 1 | LCSC C3743528 | 5V→3.3V (cascaded from the 5.0). Powers RP2040 + transceiver + logic. **Swapped in for AMS1117-3.3 2026-07-16** — low dropout (~360mV@1A vs ~1.1V) fixes USB-VBUS-only powering headroom. Vin abs max 6V — downstream of the 5V stage only. **Must be the `-33ER-` suffix (SOT223R)**: pinout matches AMS1117 (GND/OUT/IN, tab=OUT); the plain `-33E-` variant is pin-reversed and will NOT work on this footprint. |
| LM66200DRLR (ideal-diode mux, SOT-583 8-pin) | 1 | LCSC C3235556 | **U8 — the 5V power-OR as of 2026-07-20** (review 4.m). Auto-selects the higher of VBUS and the AMS1117 5V output; no external caps; ~$0.38 @10+. `~ON` tied to GND to enable, `ST` floats. Removes the Schottky VF drop that left the +5VDC rail at ~4.7V against the SN74AHCT125's 4.5V VCC minimum. |
| PMEG3015EH,115 (Schottky, SOD-123F) | 2 (**DNP**) + 1 | LCSC C552867 | D12/D23 = the former power-OR diodes, now **unpopulated fallback footprints** — populate both and remove U8 to restore the Schottky OR by hand. (They had replaced 1N5819W 2026-07-16 for the ultra-low 255mV VF.) D29 is a separate, **fitted** part: the 12V-sense clamp on GPIO17. |
| 22µF 16V tantalum, EIA-3528 case B (TAJB226K016RNJ) | 1 | LCSC C8020 | C38, AMS1117-5.0 output cap ONLY. Must have some ESR — AMS1117 can oscillate with only low-ESR ceramic; this part's 2.3Ω @100kHz is in the stable window. 16V = 3.2× derating on the 5V rail (tantalum wants ≥2×). |
| 10µF 25V X5R 0805 MLCC (CL21A106KAYNNNE) | 4 | LCSC C15850 (JLC basic) | C37+C52 (AMS1117-5.0 input, **2× in parallel** — 12V DC bias derates each 0805 X5R to ~4–5µF effective, the pair restores ~8–10µF without paying an extended-part fee for a 1206; MLCC fine on the *input*, ESR rule is output-only), C44/C50 (AP7361C in/out — CMOS LDO, ceramic-stable per datasheet, ≥1µF in / ≥2.2µF out). |
| 470µF 25V SMD aluminum electrolytic, V-chip 10×10.2mm (RVT1E471M1010) | 1 | LCSC C72518 | C51, 12V bulk at power IN. **SMD swap for the TH radial 2026-07-16** (lower profile, machine-placeable). Deliberately NOT tantalum: low-impedance 12V bus + hot-plug inrush is the classic tantalum surge-failure scenario, and 470µF/25V tantalum costs dollars, not cents. |
| 10nF ceramic cap (ADC node → GND) | 4 | DigiKey | One per FSR ADC input. Bench-verified fix for ADC mux crosstalk; required. |
| 10kΩ resistor (FSR pull-down) | 4 | DigiKey | Voltage divider bottom leg. 12kΩ acceptable substitute. |
| 120Ω resistor (RS-485 termination) | 1 | DigiKey | Always populated; routed in/out by the DPDT switch. |
| DPDT slide switch (termination select) | 1 | DigiKey | **DECIDED 2026-07-11: E-Switch EG2201A** — DPDT vertical THT slide, ON-ON, 200mA/30VDC (signal-level, fine for 120Ω termination + GPIO sense). Pole 1 switches the 120Ω across A/B; pole 2 reports state to a GPIO (pull-up, LOW = terminated). Footprint imported from SnapEDA into project lib (`panel-pcb:SW_EG2201A`, ref SW3). |
| 4-position DIP switch (panel ID) | 1 | DigiKey | Values 0–8 = ID, 9–13 = diagnostic modes. |
| WS2815 addressable LED, individual 5050 chips (12V) | 25 | LCSC / AliExpress (reel or cut tape) | **Confirmed 2026-07-10 over APA102-class SPI parts** — see docs/LED_OPTIONS.md. LEDs are placed directly on the panel PCB (not strips). 225 exact for 9 panels — buy ~300 or a small reel; not a DigiKey part. If panels are fab-assembled at JLCPCB, WS2815 is in LCSC stock — have the fab place them and skip buying separately. Prototype used WS2812B strip. |
| 100nF ceramic filter cap (one per LED) | 25 | DigiKey/LCSC | **Corrected 2026-07-11 against the WS2815 datasheet**: the cap goes from **VCC (pin 1) to GND**, not across VDD/GND — VCC is the IC's internal-regulator node ("Suspended or connected with a filter capacitor to GROUND"), and the datasheet's recommended application circuit shows exactly this 100nF per LED. The earlier "VDD/GND, ≥25V" note was generic 5V WS281x practice; the VCC node is low-voltage, so ≥25V isn't required (harmless if that's what's on the reel). 225 exact — buy a full reel, they're near-free. Bulk 12V decoupling for the LED VDD rail is a layout concern (pours + a few bulk caps), not per-LED. |
| Series resistor on LED data input (~100–500Ω) | 1 | DigiKey/LCSC | Between shifter output and first LED's DIN; damps ringing on the data line. |
| Micro-Fit 3.0 2-pin right-angle THT header, Molex 43650-0200 (Power IN / OUT) | 2 | DigiKey (Molex) | All panels identical; end-of-chain OUT sits empty. THT decided 2026-07-17 (was SMD -0210 variant); footprint `..._43650-0200_1x02_P3.00mm_Horizontal`. |
| Micro-Fit 3.0 3-pin right-angle THT header, Molex 43650-0300 (RS-485 IN / OUT) | 2 | DigiKey (Molex) | All panels identical; end-of-chain OUT sits empty. THT decided 2026-07-17 (was SMD -0310 variant); footprint `..._43650-0300_1x03_P3.00mm_Horizontal`. |
| INT termination: screw terminal block, 1-pos 5.08mm | 1 | LCSC/DigiKey | **DECIDED 2026-07-11: generic KF301-style 1P 5.08mm** (brand-name Phoenix MKDS 1,5/1 equivalent acceptable; same screw-cage design). Takes 24 AWG + ferrule. KiCad stock footprint `TerminalBlock_bornier-1_P5.08mm` assigned to J9. INT is a single conductor (return rides shared power ground). |
| JST-PH 2-pin top-entry header, B2B-PH-K (FSR) | 4 | DigiKey (JST) | One per cardinal edge. **Corrected from JST-XH 2026-07-10** — existing FSR leads use PHR-2 plugs (verify before footprint). |
| Interlink FSR 408 (long strip) | 4 | iefsr.com | Not a DigiKey part. |
| RP2040 decoupling caps + misc passives | — | DigiKey | Specced during schematic capture (standard 100nF/1µF set per RP2040 hardware design guide). |

Layout note for the LED chain: WS2815's **BIN** (backup data) pin on each LED connects to
the same signal feeding the *previous* LED's DIN; **the first LED's BIN ties to GND**
(corrected 2026-07-17 — an earlier note here claimed the datasheet ties BIN(1) to the data
source, but the vendor application circuit actually grounds the first pixel's BIN; caught
in design review). That's what makes the chain survive a single dead LED — don't leave BIN
floating. Last LED's DOUT is left unconnected.

## Master PCB (×1)

**No 12V on the master** (corrected 2026-07-10): power runs PSU → panel columns directly;
the master is USB-powered and is not in the 12V distribution path. It DOES need its ground
tied into the pad ground network — INT and RS-485 require a common reference, and separate
grounds was a real bench failure (see multi-panel bring-up notes). Plan: a GND tie point on
the master PCB (2-pin connector or screw pad) → short lead → fork/spade lug on the PSU's
GND stud. Only remaining 12V question: underglow power source (see underglow row below).

| Part | Qty | Source | Notes |
|------|-----|--------|-------|
| Teensy 4.0 | 1 | PJRC / distributors | Not DigiKey's Molex aisle but DigiKey does stock Teensy (DEV boards section). |
| GND tie connector/pad + fork lug | 1 | DigiKey | Master ground → PSU GND stud. Connector style TBD at layout. |
| Micro-Fit 3.0 3-pin right-angle header (RS-485 OUT) | 1 | DigiKey (Molex) | |
| INT bundle: 9–10 position pluggable terminal block (Euroblock style) | 1 | DigiKey | **Changed from 10-pin Micro-Fit 2026-07-10** — 9 single-conductor INT wires with ferrules, one position each (+ spare, fine if empty). Pluggable = all 9 wires detach from the master as one block. Wires color-coded per panel (stock SMX convention — document the color map). |
| 10kΩ resistor (INT pull-up) | 9 | DigiKey | One per INT line, to 3.3V. |
| TVS diode (INT line ESD protection) | 9 lines | DigiKey | **Part not yet selected** — needs 3.3V working voltage; pick single vs array at schematic time. |
| 120Ω resistor (RS-485 termination, master end) | 1 | DigiKey | Master end is always terminated. |
| Underglow DATA output connector | 1 | TBD | **Data line only** (resolved 2026-07-10): underglow 12V/GND already come from the PSU lugs in the stock harness, and the master is grounded via its PSU GND tie — so the master just outputs DATA from a shifter channel (5V, off the Teensy USB rail). Harness splice point reopened 2026-07-10 (leads crimped directly into a 12-pin Dupont housing at the stock MCU, no reusable intermediate connector) — pending full teardown. Connector type TBD at master layout time. |
| 3-position DIP switch (player/pad ID) | 1 | DigiKey | **Decided 2026-07-10**, replaces stock jumper-based P1/P2 selection. 8 values (0–7); 0–3 = P1–P4 used initially, 4–7 reserved. |

## Micro-Fit 3.0 shopping list (full 9-panel build, 2026-07-10)

INT no longer uses Micro-Fit (single-conductor, screw/ring terminations — see rows above),
so Micro-Fit is only power (2-pin) and RS-485 (3-pin). RS-485 deliberately stays 3-pin with
position 3 empty: with a 2-conductor cable it would otherwise be physically identical to
power, and the different shell is what makes plugging 12V into the transceiver impossible.

User already owns: Micro-Fit crimp tools + a handful of 20–24 AWG pin/socket terminals.
**Header PNs decided 2026-07-17: Molex 43650-0200 (2-pin) / 43650-0300 (3-pin) —
right-angle THROUGH-HOLE.** (The earlier "43651" series pointer was wrong — 43650 *is*
the right-angle single-row series; the suffix encodes THT/SMT/peg. The originally
assigned KiCad footprints `43650-0210`/`-0310` turned out to be the SMD-pad variant;
switched to the THT `-0200`/`-0300` for plug-cycle/cable-yank robustness, same rationale
as the TH USB-C. Hand-solder with the other TH parts — avoids JLC THT-assembly fees.)
Cable side: **43645** single-row receptacle housings, **43030** female crimp terminals
(20–24 AWG variant) — verify against photos/datasheets when carting.

Quantities below include the **5 prototype passthrough PCBs** (exact-need; order ~10–20%
spares):

| Header | Exact need | Breakdown |
|--------|-----------|-----------|
| 2-pin RA | 28 | Power IN+OUT ×2 on 9 panels (18) + 5 passthroughs (10) |
| 3-pin RA | 29 | RS-485 IN+OUT ×2 on 9 panels (18) + 5 passthroughs (10), master RS-485 OUT ×1 |

Cable-side receptacle housings (cable count is per-slot and doesn't change with
passthroughs — all 9 slots get cabled either way):

| Housing | Exact need | Breakdown |
|---------|-----------|-----------|
| 2-pin | 15 | 3 PSU→column-top feed cables (Micro-Fit at panel end only; fork/spade lug at PSU end) + 6 inter-panel power cables × 2 ends |
| 3-pin | 18 | 9 RS-485 cables × 2 ends |

Female crimp terminals: power 30 (feed cables crimp only the panel end) + RS-485 36 (only
2 of 3 positions crimped) = **66 exact; order ~100** for crimp scrap.

Also: fork/spade lugs for the 3 column feed cables' PSU ends + 1 for the master GND tie
(match stud size on the stock PSU); **wire ferrules** (~0.25mm² for 24 AWG) for INT screw
terminations, ~30 exact incl. master end — buy an assortment box; optionally ring terminals
instead at the panel end (style TBD at layout).

## Wire / cable (spec decided 2026-07-10; lengths are estimates — measure the stock harness before ordering)

Goal: jacketed multi-conductor cable with exactly the conductors needed, no redundant grounds.

General spec (all runs): **stranded pure copper** (never solid — vibration; never CCA —
check cheap listings), **unshielded** (RS-485 is differential and INT is filtered open-drain;
shield buys nothing at <3m/1 Mbps), **PVC insulation (UL1007-class)** — chosen over silicone:
tougher against frame-edge abrasion, no cold-flow under clamps, and these cables flex only
during assembly. Any ≥80°C/300V rating is fine; don't pay for more.

| Run | Cable | Est. total length | Notes |
|-----|-------|-------------------|-------|
| Power (3 feeds + 6 inter-panel) | 2-conductor 20 AWG jacketed round (PVC), red/black conductors | ~4–5m | Security/alarm-style 2C cable; verify pure copper |
| RS-485 (9 links, serpentine) | 1 twisted pair 22–24 AWG | ~4–5m | Buy 1-pair jacketed cable OR self-twist two UL1007 colors (~1–2 twists/inch, heatshrink ends). Fix an A/B color convention (e.g. yellow=A, blue=B) and never deviate |
| INT (9 home-runs) | single-conductor 24 AWG UL1007 | ~7–10m total (runs ~0.3–1.3m each) | **One distinct color per panel — stock SMX map adopted (2026-07-10, from user's pad):** 0=Red (UL), 1=Orange (U), 2=Yellow (UR), 3=Green (L), 4=Blue (C), 5=Brown (R), 6=Grey (DL), 7=White (D), 8=Black (DR) |

Supporting bits: heatshrink assortment (end labels, pair dressing, strain relief); zip-tie
anchor points near each PCB connector so yanks load the tie, not the crimp; grommets/edge
protection wherever cable crosses frame metal; cut with service-loop slack so a panel can be
lifted out while connected. Pre-crimped Micro-Fit pigtails are an option to skip the 66 hand
crimps.

Pad is ~850mm square (3× ~280mm panels); the estimates above assume routing slack. Verify
against the stock harness before cutting/ordering.

## AliExpress sourcing candidates (2026-07-23) — primary; DigiKey = fallback

Sourcing strategy: **AliExpress for panel parts** (needed in 9× multiples, where bulk packs pay
off) and cable; **DigiKey for master-only parts** (only ~1–2 needed, so the higher unit price is
fine and not worth a separate AliExpress order/min-qty). Candidates below are unverified against
datasheets/footprints — **each has a match-check that must pass before ordering.** DigiKey PNs in
`BOM_PRICED.md` remain the fallback for anything that fails its check.

### Cable (see Wire/cable section above for full spec)
| Run | AliExpress candidate | Price | Match-check |
|-----|----------------------|-------|-------------|
| 12V power | PVC 2C 20AWG oxygen-free tinned copper — [1005008621580316](https://www.aliexpress.com/item/1005008621580316.html) | ~$14.68/10m + $9.39 ship (~$2.41/m) | 20 AWG (not 22/24), stranded, 2-cond jacketed round |
| RS-485 | 22AWG shielded twisted pair — [1005006546939974](https://www.aliexpress.com/item/1005006546939974.html) | ~$20.92/10m free ship | genuine twisted pair, pure copper; **leave shield/drain unconnected** |
| INT + hookup | 10-color 24AWG stranded pack — [1005008982254390](https://www.aliexpress.com/item/1005008982254390.html) | ~$16.44 free ship | **stranded not solid**, colors cover the 9-panel map (record actual mapping if not exact), pure copper |

### Connectors & switches
| Part / use | Qty needed | AliExpress candidate | Match-check | Fallback |
|-----------|-----------|----------------------|-------------|----------|
| Euroblock 9p (master INT), header+plug | 1 | pack of 5 — [1005012001482158](https://www.aliexpress.com/item/1005012001482158.html) | **5.08mm pitch**, 9-pos, single-row (master ftpt = Molex 39531 P5.08; master not laid out yet so ftpt adjustable) | DigiKey |
| DPDT slide, RS-485 term (panel SW3) | 9 (+spares) | SS-22H88 — [1005010555541589](https://www.aliexpress.com/item/1005010555541589.html) | **⚠️ different footprint from `SW_EG2201A`** (panel's current ftpt). Only viable if panel SW3 footprint is changed to SS-22H88 first | E-Switch **EG2201A** @ DigiKey (matches current ftpt) |
| 4-pos DIP, panel ID (panel SW1) | 9 (+spares) | [1005009296124199](https://www.aliexpress.com/item/1005009296124199.html) (~$2.43) or 10-pk [1005005866801107](https://www.aliexpress.com/item/1005005866801107.html) | **4-position, 2.54mm pitch, 7.62mm (0.3") wide**; qty ≥9 | DigiKey |
| 3-pos DIP, player ID (master SW1) | 1–2 | (use panel 4-pos + re-foot master to 4-pos, OR buy 3-pos) | master ftpt = SPSTx03 W7.62 P2.54 | **DigiKey (priced OK, only need ~2)** |
| Micro-Fit 3.0 **3p** PCB header, RA | ~19 (18 panel + 1 master) | [1005008706326809](https://www.aliexpress.com/item/1005008706326809.html) | **RIGHT-ANGLE (horizontal)**, 3.0mm pitch = 43650-0300 | DigiKey |
| Micro-Fit 3.0 **2p** PCB header, RA | ~18 | [1005012059959598](https://www.aliexpress.com/item/1005012059959598.html) | **RIGHT-ANGLE (horizontal)**, 3.0mm pitch = 43650-0200 | DigiKey |
| Micro-Fit 3.0 crimp terminals | 66 (buy ~100 for waste) | [1005011606773268](https://www.aliexpress.com/item/1005011606773268.html) | Micro-Fit **3.0** (not Mini-Fit 4.2), gauge range covers **20 AWG** | DigiKey |
| Micro-Fit 3.0 **2p** plug housing (cable side) | ~15 | [1005008919717941](https://www.aliexpress.com/item/1005008919717941.html) (marginal savings) | 3.0mm; **buy housings + crimps same ecosystem** so they seat/latch | DigiKey |
| Micro-Fit 3.0 **3p** plug housing (cable side) | ~18 | — (AliExpress ≈ or > DigiKey per pricing check) | — | **DigiKey (primary here)** |

Cross-cutting notes: Micro-Fit clones are fine for this 5A hobby load; the traps are (1) vertical
vs **right-angle** on PCB headers, and (2) mixing clone crimps into a different clone housing —
buy mating housing+crimp from one source. Link #2 originally pasted a duplicate URL; the DPDT and
the 4-pos DIP are genuinely different parts/links (now split above).

## FSR connector — JST-PH, not XH (corrected 2026-07-10)

User identified the existing FSR leads use **JST PHR-2** plugs (PH series, 2.0mm pitch) —
earlier docs said JST-XH (2.5mm), which would not mate. Panel PCB should use the PH
top-entry shrouded header (B2B-PH-K series) ×4 per panel. **Verify a PHR-2 plug against an
actual FSR lead before finalizing the footprint.** A dual-footprint (PH + something else)
was floated and shelved — not pursuing for now.

## Pad 12V distribution (corrected 2026-07-10)

Stock pad's 12V lives on **fork-terminal studs** (currently feeding the stock 12V→5V DC/DC
converter, which our design removes — master logic is USB-powered). The 3 column feed
cables attach directly to those PSU studs with fork/spade lugs and run to each column's
first panel — **the master PCB carries no 12V and no power connectors** (an earlier BOM
revision wrongly routed 8–9A through the master). The master's only tie to the power system
is its GND lead to the PSU ground stud (see Master PCB section).

## Passthrough PCB — ×5 for the prototype build (2026-07-10)

Connector set identical to a real panel: Power IN/OUT 2-pin + RS-485 IN/OUT 3-pin Micro-Fit
right-angle, INT termination footprint (present for uniformity, connected to nothing) +
120Ω resistor + DPDT slide switch. No MCU/LED/FSR parts. Like real panels, all passthroughs
are identical — full IN/OUT set, end-of-chain OUT sits empty. Per 5 boards: 10× 2-pin RA,
10× 3-pin RA, 5× 120Ω, 5× DPDT (already included in the shopping-list totals above).

## Not yet decided — do NOT cart yet

- **TVS diode part number** for the 9 INT lines at the master — deferred, depends on master
  PCB layout; easy to pick at schematic time (2026-07-10).
- **Exact JST part numbers** — Micro-Fit header PNs are now pinned (43650-0200/-0300, see
  shopping list); cable-side housings/terminals and JST-PH parts still to verify against
  photos/datasheets when carting.
- ~~SMD packages for passives~~ **decided at layout**: 0603 default; 0402 for the 12
  RP2040 decoupling caps (placement room at the QFN escape); 0805 for the 10µF MLCCs.
- Master PCB may also want a local bulk cap / input protection — not yet designed.

## Already sourced for bench (not part of the production cart)

- Amazon AMS1117 assortment (70pc, incl. 3.3V/5.0V/ADJ) — bench cascade test only.
- Raspberry Pi Pico boards, Teensy 4.0, MAX3485s, WS2812B strip, SN74AHCT125N DIP,
  breadboard FSR dividers — see docs/PROTOTYPE_WIRING.md.
