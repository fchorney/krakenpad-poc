# Bill of Materials (confirmed parts only)

Status: **pre-layout BOM** — parts confirmed by design decisions and bench testing as of
2026-07-10. Exact manufacturer part numbers for connectors/switches/passive packages get
pinned during KiCad footprint selection; entries below give the decided part or the decided
requirement. Items still under discussion are in the "Not yet decided" section at the bottom —
don't cart those yet.

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
| USB-C receptacle, 16-pin USB 2.0-only | 1 | DigiKey | For firmware flashing + animation loads. Chosen over micro-B 2026-07-10. Tie duplicated D+/D− pairs together. **Vertical DIP through-hole mount preferred (2026-07-10)** over right-angle — better mechanical durability for repeated plug/unplug, smaller footprint than a 90° part. Candidates: GCT USB4105/USB4085-series; exact part TBD. |
| 5.1kΩ resistor (CC1/CC2 pull-downs) | 2 | DigiKey | Required for VBUS with C-to-C cables — do not omit. |
| BOOTSEL tactile button | 1 | DigiKey | Grounds flash CS at boot for USB bootloader entry. |
| SWD header, 3-pin 2.54mm (SWDIO/GND/SWCLK) | 1 | DigiKey | Recovery/debug path independent of USB + firmware state. Can ship unpopulated. Probe side: Raspberry Pi Debug Probe or a Pico running picoprobe. |
| W25Q32JV (4MB QSPI flash, SOIC-8) | 1 | DigiKey | Required for RP2040 boot. Chosen over W25Q16JV (pin-compatible) for headroom. |
| THVD1419 (RS-485 transceiver, SOIC-8) | 1 | DigiKey | 3.3V-logic compatible. MAX3485 was breadboard substitute only. Do NOT buy MAX485. |
| SN74AHCT125N (level shifter) | 1 | DigiKey | VCC = 5V rail. Drives WS2815 data line only. DIP-14 used on breadboard; pick SMD variant (SN74AHCT125D/PW) at layout time. |
| AMS1117-5.0 (LDO, SOT-223) | 1 | DigiKey (genuine) | 12V→5V, feeds only the level shifter VCC. Amazon assortment chips are bench-test only. |
| AMS1117-3.3 (LDO, SOT-223) | 1 | DigiKey (genuine) | 5V→3.3V (cascaded from the 5.0). Powers RP2040 + transceiver + logic. |
| Output cap, 10–22µF tantalum or electrolytic | 2 | DigiKey | One per LDO output. Must have some ESR — AMS1117 can oscillate with only low-ESR ceramic. Also carry input caps per datasheet. |
| 10nF ceramic cap (ADC node → GND) | 4 | DigiKey | One per FSR ADC input. Bench-verified fix for ADC mux crosstalk; required. |
| 10kΩ resistor (FSR pull-down) | 4 | DigiKey | Voltage divider bottom leg. 12kΩ acceptable substitute. |
| 120Ω resistor (RS-485 termination) | 1 | DigiKey | Always populated; routed in/out by the DPDT switch. |
| DPDT slide switch (termination select) | 1 | DigiKey | Pole 1 switches the 120Ω across A/B; pole 2 reports state to a GPIO (pull-up, LOW = terminated). PN pinned at footprint stage. |
| 4-position DIP switch (panel ID) | 1 | DigiKey | Values 0–8 = ID, 9–13 = diagnostic modes. |
| WS2815 addressable LED, individual 5050 chips (12V) | 25 | LCSC / AliExpress (reel or cut tape) | **Confirmed 2026-07-10 over APA102-class SPI parts** — see docs/LED_OPTIONS.md. LEDs are placed directly on the panel PCB (not strips). 225 exact for 9 panels — buy ~300 or a small reel; not a DigiKey part. If panels are fab-assembled at JLCPCB, WS2815 is in LCSC stock — have the fab place them and skip buying separately. Prototype used WS2812B strip. |
| 100nF ceramic decoupling cap (one per LED, 12V-rated ≥25V) | 25 | DigiKey/LCSC | Standard WS281x practice: one close to each LED's VDD/GND. 225 exact — buy a full reel, they're near-free. |
| Series resistor on LED data input (~100–500Ω) | 1 | DigiKey/LCSC | Between shifter output and first LED's DIN; damps ringing on the data line. |
| Micro-Fit 3.0 2-pin right-angle header (Power IN / OUT) | 2 | DigiKey (Molex) | All panels identical; end-of-chain OUT sits empty. |
| Micro-Fit 3.0 3-pin right-angle header (RS-485 IN / OUT) | 2 | DigiKey (Molex) | All panels identical; end-of-chain OUT sits empty. |
| INT termination: screw terminal block (1–2 pos) or stud for ring terminal | 1 | DigiKey | **Changed from 2-pin Micro-Fit 2026-07-10** — INT is a single conductor (return rides shared power ground). Style (ferrule+screw vs ring+stud) picked at layout. |
| JST-PH 2-pin top-entry header, B2B-PH-K (FSR) | 4 | DigiKey (JST) | One per cardinal edge. **Corrected from JST-XH 2026-07-10** — existing FSR leads use PHR-2 plugs (verify before footprint). |
| Interlink FSR 408 (long strip) | 4 | iefsr.com | Not a DigiKey part. |
| RP2040 decoupling caps + misc passives | — | DigiKey | Specced during schematic capture (standard 100nF/1µF set per RP2040 hardware design guide). |

Layout note for the LED chain: WS2815's **BIN** (backup data) pin on each LED connects to
the same signal feeding the *previous* LED's DIN; the first LED's BIN ties to the data
source (shifter output). That's what makes the chain survive a single dead LED — don't
leave BIN floating.

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
Molex series to search on DigiKey (verify against photos/datasheets): **43651** right-angle
single-row headers, **43645** single-row receptacle housings, **43030** female crimp
terminals (20–24 AWG variant).

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
- **Exact Molex/JST part numbers** — user is selecting on DigiKey against the shopping list
  above; pin final PNs so KiCad footprints match ordered parts.
- **SMD packages for passives** (0603/0805) — layout-time choice.
- Master PCB may also want a local bulk cap / input protection — not yet designed.

## Already sourced for bench (not part of the production cart)

- Amazon AMS1117 assortment (70pc, incl. 3.3V/5.0V/ADJ) — bench cascade test only.
- Raspberry Pi Pico boards, Teensy 4.0, MAX3485s, WS2812B strip, SN74AHCT125N DIP,
  breadboard FSR dividers — see docs/PROTOTYPE_WIRING.md.
