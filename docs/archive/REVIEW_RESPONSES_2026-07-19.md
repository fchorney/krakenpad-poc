> **ARCHIVED 2026-07-23** — Record of the first external schematic review and its follow-ups; all action items were resolved and folded into the design by 2026-07-20.

# Human Schematic Review — Responses & Action Items (2026-07-19)

First external human review of the panel PCB **schematic** (PCB layout not yet reviewed).
Responses below are written so they can be sent back to the reviewer mostly as-is.
Action items are collected at the bottom.

**Status as of 2026-07-20:** replies for 1.a, 2.a, 3.a and 3.b have been sent;
the 4.x replies are still to go out. Almost all of the 4.x items have since
been *implemented* rather than deferred — nothing has been ordered yet, so the
"rev B" bucket was pulled forward into rev A while the design is still open.
Per-item status is in the table below and the action list at the bottom.

---

## 1.a — CRITICAL: "LED data must be level-shifted to 12V, not 5V" → **Rebutted by the current WS2815B datasheet; 12V shift would actually violate abs-max**

The 0.7·VDD VIH line appears in older/other WorldSemi datasheets, but the
current WS2815B spec (WS2815B-V1 datasheet V2.0, 2022-10-20, "IC upgraded,
parameters updated" — pulled and verified 2026-07-19) gives **absolute** input
thresholds, not ratiometric ones:

- Electrical characteristics: **VIH min 2.7V / max 5.7V** (DIN, SET);
  VIL max 1.5V
- Absolute maximum ratings: **Logical input voltage VI = −0.3 to +5.7V**
  (separate from the VDD supply rating of +9.5 to +13.5V)

So a 5V data signal is explicitly in spec, and a 12V-shifted signal would
**exceed the DIN pin's 5.7V absolute maximum** — the proposed fix would damage
the part. Our shifter output (~4.7V after the power-OR diode drop) clears the
2.7V VIH with ~2V margin.

This also matches field practice: the QuinLED-Dig family (the most widely
deployed open-source WS2815 controllers) drives 12V WS2815 through exactly our
part — a 74AHCT125 buffer at 5V VCC. And a 12V shift isn't physically available
from this chip anyway (SN74AHCT125 VCC max 5.5V).

Remaining diligence (downgraded from order-gate to sanity check, since the
datasheet now closes the question):

- [x] **Confirmed 2026-07-19:** LCSC C5446699 is exactly WS2815B-V1, the variant
      this datasheet revision covers.
- [ ] Optional: bench-drive a WS2815 strip from the prototype's SN74AHCT125N
      at 5V (our breadboard prototype used WS2812B, so this would be our first
      hands-on WS2815 test — cheap insurance, no longer a blocker). **Still the
      one open hardware-verification item.**

## 2.a — SEVERE: "R8–R11 appear to be standard resistors / RC-decay scheme?" → **Misread; no design change**

R8–R11 *are* standard resistors — they're the 10kΩ 1% pull-downs of a plain
resistive divider, not the FSRs. The FSRs (Interlink FSR 408 strips) are
off-board, plugging in via J3–J6 (JST-PH, one per cardinal edge). Topology per
channel: 3.3V → FSR (via connector) → ADC node → 10k to GND. The ADC reads the
divider midpoint directly — no charge/decay measurement. The 10nF C0G at each
ADC pin is an anti-alias / charge-reservoir cap for the RP2040's sampling ADC
(our design choice, standard SAR-ADC input practice), not a timing element. Pull-down on-board
means an unplugged FSR reads ~0 (same as unloaded) — safe failure mode.

That this was misreadable is fair feedback though — see actions: schematic text
note explaining the FSR divider, and functional net names (ties into 4.k).

## 3.a — RS-485 bias resistors → **Not needed: transceiver has integrated failsafe**

The THVD14x9 transceiver has open/short/idle-bus failsafe receivers built in
(TI family feature), so external bias isn't required for a defined idle state. Also,
bias belongs at *one* point on a bus (normally the master), not on every node —
nine bias pairs in parallel would load the differential budget. No change;
we'll add a schematic note documenting the rationale.

- [x] **DONE 2026-07-20 (master PCB):** DNP bias pair placed and wired — R4 390R
      1% from +3.3VDC to RS485+, R5 390R 1% from RS485− to GND, both flagged
      DNP. The 390R pair would give ≈236mV across the 60Ω loaded bus at 3.3V.
      Unpopulated by default; the footprints exist so bias can be added at the
      one correct point on the bus if the bench ever disagrees with the
      datasheet.

## 3.b — "Remove the 5V LDO, run the 3.3V LDO from 12V" → **No; premise depends on 1.a, and the 3V3 LDO can't take 12V**

Since the 5V level shift is valid (1.a), the 5V rail stays. Additionally the
3.3V regulator (AP7361C-33) has a 6V absolute-max input — it must stay
downstream of the 5V stage. The cascade was a deliberate choice: second-stage
PSRR gives a clean 3.3V for the FSR ADC, and dissipation splits across two
SOT-223s. The USB VBUS diode-OR you describe is already in the design (D12/D23
PMEG3015EH — **since superseded by an LM66200 ideal-diode mux, see 4.m**), and
"USB-only won't run the LEDs" is intentional — USB exists only
for bench flashing with the panel top off.

## 4.x — OFIs

| # | Verdict | Response |
|---|---------|----------|
| 4.a Global label directions | **DONE** | All directions fixed by hand and then position-audited instance-by-instance against the netlist: drivers set output, consumers input, buses bidirectional, the 14 unused GPIOs left passive by joint decision, VBUS passive. ERC 0 errors after. |
| 4.b Datasheet links | **DONE** | 22 Datasheet properties filled across the ICs, connectors, crystal and switches. Jellybean R/C deliberately get none — Value + LCSC part number is already a complete spec for those. Two notes: SW3's link is a DigiKey product page (E-Switch publishes no stable direct PDF), and J9 is a generic KF301 clone family with no authoritative MPN or datasheet in existence, so it got an explanatory Description instead. |
| 4.c MPNs missing package | **DONE** | 52 MPN properties added, every one verified against a live LCSC listing: U3 → `W25Q32JVSSIQ`, U4 → `SN74AHCT125DR`, U5 → `AMS1117-5.0`, U7 → `USBLC6-2SC6`, D12/D23/D29 → `PMEG3015EH,115`, X1 → `ABM8-272-T3`, C38 → `TAJB226K016RNJ`, C51 → `RVT1E471M1010`, SW2 → `B3U-1000P`, J1 → `USB4085-GF-A`, JST → `B2B-PH-K-S(LF)(SN)`, Micro-Fit → `43650-0200`/`-0300`, WS2815B-V1 ×25. **This item is what surfaced the THVD1419 speed-grade bug — see the bonus finding below.** |
| 4.d Distributor PNs in design | **DONE (as described)** | Kept the `LCSC` property — it is what drives JLCPCB assembly auto-matching — and added `MPN` as a separate field alongside it rather than as a replacement. Values stay short and human-readable; MPN and LCSC carry the exact ordering identity. |
| 4.e Rail naming (+3V3 vs +5.0V) | **DONE — pulled forward from rev B** | Standardized on a ×VDC convention: `+12VDC` / `+5VDC` / `+3.3VDC` / `+1.1VDC`, and `+5VDC_USB` on the master (the suffix is meaningful — different source). Done as one careful pass with the PCB zones re-pointed to match; netlist diffed against the previous commit to prove renames only, zero connectivity changes. |
| 4.f MLCC DC-bias derating | **No change — already handled** | The 12V-biased input caps were re-specced as 2× parallel 0805 MLCC for exactly this reason, before the review. C38 is tantalum because the AMS1117 needs the ESR to stay stable. The 10µF X5R 25V parts on the 5V/3.3V rails retain most of their capacitance at those biases, and the AP7361C is ceramic-stable per its datasheet. On your follow-up question about the lower bound: AP7361C's datasheet minimum is 1µF and the AMS1117 wants 22µF with ESR on the output, so we have margin above both. |
| 4.g Test clips / probe-via TPs | **DONE — pulled forward from rev B** | All 12 test points swapped from `TestPoint_Pad_1.5x1.5mm` to `TestPoint_THTPad_D2.0mm_Drill1.0mm` — probe-hole style, hookable. 12 extra THT holes is no cost change at JLC. May revisit to SMD loops if the through-holes prove annoying in layout. |
| 4.h SW3 drawn as 2× SPDT | **DONE** | Redrawn as a proper DPDT in the custom `panel-pcb:EG2201A` symbol. Cosmetic only — no net or pin changes. |
| 4.i Crystal 1k / add 1M DNP | **No change** | The 15pF/15pF/1k network is the RP2040 hardware design guide's exact reference circuit for this crystal (ABM8-272, CL=10pF). The reference contains no external 1M — oscillator biasing is on-chip — and every Pico-class board ships without one. Going with the vendor reference. |
| 4.j 0402 parts vs hand rework | **Still open — rev A risk accepted** | Boards are JLC-assembled both sides, not hand-populated, and only a couple of 0402s exist (C9/C10 1µF near the RP2040). A space check for bumping them to 0603 is on the PCB-session list; if the room isn't there, rework under magnification stays accepted for rev A. |
| 4.k Functional net names | **DONE — pulled forward from rev B** | Nets now read `RS485_TX`/`RX`/`DE`, `LED_DATA`, `INT_OUT`, `TERM_SENSE`, `SENSE_12V`, `DIP_ID0`–`ID3`, and `FSR_North`/`South`/`East`/`West` spelled out in full. Done in the same synced pass as 4.e — 20 clean renames, 0 connectivity changes, verified by netlist diff. |
| 4.l Wires vs labels | **CLOSED** | The LED chain is now direct-wired point-to-point, which was the case where the label indirection genuinely hurt readability. Everything else keeps labels — those are the cross-sheet-distance nets where direct wires would create crossings. |
| 4.m Diode drop on 5V rail | **DONE — pulled forward, and it changed the design** | Your point stood up: rather than accept ~4.7V for rev A, the 5V power-OR is now an **LM66200 ideal-diode mux** (U8, `LM66200DRLR`, LCSC C3235556, SOT-583 8-pin, ~$0.38 — auto-selects the higher supply, no external caps). D12/D23 stay on the board as **DNP footprints**: populate those two and remove U8 and you're back to the Schottky OR as a hand-solderable rescue. Worth recording why discrete cross-coupled P-FETs were **rejected** — with both supplies present both FETs turn off, conduction falls to the body diodes at ~0.7V, and the rail sags to ~4.3V, *below* the SN74AHCT125's 4.5V VCC minimum. That's worse than the Schottky it replaces. (A single gate-to-GND P-FET is reverse-polarity protection, not an OR — it cannot do this job at all.) |

## Bonus finding — your 4.c point earned its keep

Completing the MPNs (action from 4.c) surfaced a real selection bug neither
review caught: the specified RS-485 transceiver **THVD1419 is TI's 250 kbps
speed grade** (the family splits 1419 = 250kbps / 1429 = 20Mbps), while the
bus runs at 1 Mbps. It never showed on the bench because the breadboard used a
MAX3485 (10 Mbps). Fixed 2026-07-19: **U2 → THVD1429DR** (identical pinout,
package, surge protection, and failsafe — and cheaper at LCSC). Swapped in both
panel and master schematics + all docs.

## 5/6 — Considered & Kudos

5.b.1 (12V presence sense), 5.b.2 (diode-OR), 5.b.4 (USB sink + TVS) all read
correctly — good confirmation the schematic communicates intent. Thanks for the
kudos on sectioning, logical pin arrangement, and test points.

---

## Consolidated Action List

Status as of 2026-07-20. Nothing has been ordered, so items originally parked
for rev B were pulled forward while the design is still open.

**Done:**
1. **1.a closed by datasheet** (WS2815B VIH is an absolute 2.7V min; a 12V shift
   would violate the 5.7V abs-max). LCSC C5446699 confirmed to be WS2815B-V1.
2. Global label directions (4.a) — fixed and position-audited, ERC 0.
3. MPNs with package suffix (4.c) + MPN fields alongside LCSC (4.d) — 52 MPN
   properties, all verified against live listings. Datasheet links (4.b) — 22
   filled.
4. **THVD1419 → THVD1429 swap** — the 250kbps-vs-1Mbps speed-grade bug that
   4.c surfaced. Swapped in panel schematic, panel PCB, and master schematic;
   docs swept. See the bonus finding above.
5. Functional net names (4.k) + rail-name standardization (4.e) — one synced
   pass, netlist-diffed to prove renames only.
6. Probe-hole test points (4.g) — 12 TPs converted to THT.
7. Direct-wired LED chain (4.l).
8. **LM66200 ideal-diode OR (4.m)** — replaces the D12/D23 Schottky OR, which
   stays as a DNP fallback. Wired and netlist-verified 2026-07-20.
9. **Master DNP RS-485 bias footprints (3.a)** — R4/R5 390R 1%, placed and
   wired on the master schematic.

**Still open — before ordering:**
10. **WS2815 strip bench test at 5V** — the one remaining hardware verification.
    Optional insurance rather than a blocker (1.a is closed on paper), but it
    would be our first hands-on WS2815 test since the prototype used WS2812B.
11. 0402 → 0603 space check (4.j) — the only remaining layout question. See the
    decoupling-layout note below; the answer may well be "leave them alone".

**Closed 2026-07-20 (later):**

- Schematic text notes — **DONE**. The FSR block now carries "3.3V → off board
  FSR (J3–J7) → ADC node, 10k pull-down; ADC reads the divider directly; 10nF =
  ADC filter, not a timing element" (closes 2.a), and the RS-485 block carries
  "No external bus biasing: THVD1429 has integrated open/short/idle failsafe"
  (closes 3.a).
- SW3 drawn as DPDT (4.h) — **DONE**.
- **Logo-vs-GND clearance — RESOLVED, and more elegantly than the options I
  listed.** Rather than grounding the logo polys or adding a keepout, the F.Cu
  copper polygons were simply deleted, leaving only the 5 F.Mask polys. The
  mask opening then exposes the F.Cu GND pour that already covers that region —
  verified by point-in-polygon test against the filled zone, 8/8 sample points
  inside the fill. Same visual result, no separate copper island to clear, and
  the exposed metal is now ground rather than a floating shape. **Panel DRC is
  0 errors** (75 remaining violations are all the known benign warnings: 48 silk
  self-clipping, 25 footprint-rotation false positives, 2 J1 silk-vs-edge).

**No action (rebutted or already handled):** 2.a, 3.b, 4.f, 4.i.