# Master Schematic Plan — pin map & blocks (v0.1, 2026-07-18; wiring current as of 2026-07-20)

Working plan for `hardware/master-pcb/`. Same process as the panel: this doc
first, then the KiCad project. Drawing workflow (agreed 2026-07-18): Claude
creates the project and places/annotates symbols; the user does all wiring from
a connection list.

The master is deliberately simple: **no 12V anywhere on the board, no
regulators, no magnetics.** Everything runs from the Teensy's USB power (5V
USB rail + Teensy's onboard 3.3V regulator). The 12V distribution goes
PSU → panel columns directly and never touches this PCB.

## Decisions locked 2026-07-18

| Item | Decision |
|------|----------|
| MCU | Teensy 4.0, **socketed** (female headers, replaceable) |
| INT connector | **9-position 5.08mm pluggable terminal block (Euroblock)** — no GND position needed (return rides the power ground network); 5.08 pitch for finger room. MPN at BOM time |
| INT pull-ups | 10kΩ to 3.3V (deliberately not stiff — INT-into-dead-panel case from the panel design review), **resistor array**, exact array format = whatever is cheapest at BOM time |
| INT ESD | **3× SRV05-4** (SOT-23-6 rail-clamp array, 4ch each; TECH PUBLIC, LCSC C558418, ~$0.013/pc — decided 2026-07-18). D2=INT0–3, D3=INT4–7, D4=INT8 (3 spare ch NC); VN→GND, VP→+3.3VDC. Prototype's single SMBJ3.3A retired |
| GND tie + underglow out | **Merged into ONE 2-position KF301-style 5.08mm screw terminal (decided 2026-07-18):** pos 1 = GND (the mandatory tie lead to the PSU GND stud), pos 2 = LED-DATA-OUT. Rationale: a separate GND pin on the underglow connector is redundant as a DC return (strips ground at the PSU lugs, common with the tie), but keeping a GND position *adjacent* to DATA preserves the option of a paired/twisted return wire alongside the 800kHz data line (small return loop, less ringing) — a 5.08mm position clamps two conductors fine. Whether the harness actually runs data+GND paired is decided at teardown. Data gating is UI/software config only ("underglow: on/off") — no sense pin for now; a 12V-sense position (panel R18/R19-style divider) is the known upgrade path if config-only gating proves annoying. **DRAWN 2026-07-20: J3 deleted, the surviving 2-pos part is J4 — pin 1 = DATA (from R3), pin 2 = GND.** Note the pin order is the reverse of this row's original "pos 1 = GND" wording; as-drawn wins, and nothing depends on which position is which |
| Player ID | 3-bit DIP switch to GND, Teensy internal pull-ups (no external resistors) |
| Status LED | 1× LED + series resistor on a spare GPIO (because why not) |
| RS-485 out | Micro-Fit 3.0 3-pin, THT right-angle 43650-0300 — same part and pin-3-unpopulated keying as the panels |
| RS-485 transceiver | THVD1429 (same as panel), 3.3V, DE+/RE tied to one GPIO |
| Termination | 120Ω fixed at the master end (master is always a bus end — no DPDT needed here, unlike panels) |
| RS-485 bias | **DNP pair added 2026-07-20** (panel review item 3.a): R4 390R 1% +3.3VDC→RS485+, R5 390R 1% RS485−→GND. Not fitted — the THVD1429's integrated open/short/idle failsafe covers the idle-bus case. Footprints exist so bias can be added at one bus point (the master, correctly) if the bench ever disagrees |

## Teensy 4.0 pin map (proposal — pins are freely reassignable during layout
except where noted; firmware maps at bring-up, same convention as the panel)

| Pin | Function | Notes |
|-----|----------|-------|
| 0 (RX1) | UART1 RX ← THVD1429 RO | **Hard constraint: UART1 = pins 0/1** |
| 1 (TX1) | UART1 TX → THVD1429 DI | **Hard constraint** |
| 2 | RS-485 DE + /RE (tied) | Matches prototype firmware |
| 3–11 | INT0–INT8 (panels 0–8) | Inputs, external 10k pull-up to 3.3V; any digital pin is interrupt-capable on Teensy 4.x, so this range is pure convenience |
| 12 | Underglow LED data out → shifter A input | 3.3V push-pull |
| 14, 15, 16 | Player-ID DIP bits 0–2 | To GND, `INPUT_PULLUP`, read at boot |
| 17 | Status LED | As wired 2026-07-18 (plan originally said 20 — any spare pin is fine; 13 avoided so the onboard LED stays independently usable) |
| 3.3V | THVD1429 VCC, INT pull-up array top | Teensy onboard regulator, 250mA budget — total load here is single-digit mA |
| VIN | SN74AHCT125 VCC (5V) | On Teensy 4.0 VIN is fed from USB 5V while the VUSB↔VIN pad link is intact (default). **Verify against the PJRC pinout card when drawing** |
| GND | Star point | Teensy GND + shifter GND + TVS returns + GND-tie terminal + underglow GND all meet here |

Spare after this: ~14 digital pins. Unused pins → nothing (no test-pad
obligation; add if board space is free, panel-style).

## Blocks

1. **Teensy 4.0 module (socketed)**: 2 female header strips on the PCB, Teensy
   with male pins plugs in. Symbol/footprint: source a Teensy 4.0
   symbol+footprint (PJRC publishes official KiCad libraries — verify pin
   count/geometry against the real module before footprint freeze). Only the
   outer edge pins are needed — no underside SMD pads used (USB host, VBAT
   etc. unused).
2. **RS-485**: THVD1429 SOIC-8. VCC = 3.3V + 100nF decoupling. RO→pin 0,
   DI→pin 1, DE+/RE tied→pin 2. A/B → Micro-Fit 43650-0300 (A=pin 1, B=pin 2,
   pin 3 unpopulated keying — match the panel J8/J10 pinout exactly so the
   inter-board cable is straight-through). 120Ω termination across A/B,
   always fitted.
3. **INT block**: 9-pos 5.08mm pluggable Euroblock. Per line: 10k pull-up to
   3.3V (array), TVS channel to GND (array), then to Teensy pins 3–11.
   Slot→panel mapping fixed by wire color (stock SMX map, see BOM.md):
   0=Red 1=Orange 2=Yellow 3=Green 4=Blue 5=Brown 6=Grey 7=White 8=Black.
   Layout note for later: TVS + pull-ups + star ground live next to this
   connector.
4. **Underglow**: SN74AHCT125 (SOIC-14, same MPN as panel U4), VCC = 5V from
   VIN + 100nF. One gate used: A ← pin 12, /OE → GND, Y → ~330Ω series
   (mirrors panel R16) → DATA position of the merged GND-tie/underglow
   terminal (see block 5 and the decisions table). Three unused
   gates: tie inputs and /OE pins to GND (this board's shifter is always
   powered, unlike a future unpopulated-VCC scenario; the panel left its
   spares NC — either is defensible, GND-tied is the cleaner default for a
   fresh schematic).
5. **GND tie + underglow terminal (merged 2026-07-18, drawn 2026-07-20 as J4)**:
   one 2-pos KF301-style screw terminal — pin 1 = underglow DATA, pin 2 = GND
   lead to the PSU GND stud. The GND tie is not optional wiring for the user — INT and RS-485
   both need the common reference (documented bench failure mode without it).
   The underglow DATA position may simply sit empty if underglow is unused.
6. **Player-ID DIP**: 3-position DIP to GND on pins 14–16, internal pull-ups.
   Values 0–3 = P1–P4, 4–7 reserved.
7. **Status LED**: LED + 1kΩ on pin 17 (as wired; plan originally said 20),
   R2 on the anode side.
8. **RS-485 bias (R4/R5, DNP)**: 390R 1% each, +3.3VDC→RS485+ and
   RS485−→GND. Unpopulated by default; see the decisions table.
9. **Test points** (space permitting): RS-485 A, B, DE, 3.3V, 5V, GND,
   underglow data (both 3.3V and 5V sides).

## Deliberately absent (do not re-add)

- Any 12V input, distribution, or sensing — the master never sees 12V.
- Underglow 5V-in from the stock Daygreen converter — considered 2026-07-18,
  rejected: the Teensy USB rail already provides shifter VCC. The Daygreen
  brick remains unused by this design.
- Underglow presence sense — shelved with the 5V-in; UI gating for now,
  teardown may reopen it.
- GND position on the INT Euroblock — return is via the power ground network.
- RS-485 termination switch — master is always a bus end; fixed 120Ω.

## Open items (non-blocking, resolve by BOM/layout time)

- 10k resistor-array MPN/format (cheapest).
- Euroblock 9-pos 5.08mm MPN.
- 3-bit DIP switch MPN.
- Teensy 4.0 KiCad symbol/footprint source + verification.
- Underglow connector final form — 2-pos screw terminal is the interim
  decision; harness teardown decides the splice point and may change it.
- Master PCB target ~80×60mm; enclosure sized after layout.

## Wiring list (v0.1 schematic, 2026-07-18)

**STATUS: wired, cleaned up, and netlist-verified — current as of 2026-07-20.**
Every net below was checked pin-by-pin against a `kicad-cli` netlist export.
ERC is down to 4 `lib_symbol_mismatch` warnings on the 74AHCT125 units (the
known cached-vs-derived noise class) and **0 errors**; the floating no-connect
flags are gone.

Deviations from the list as originally written, all deliberate and all fine:

- **J3+J4 merge is now DRAWN** — J3 deleted, J4 is the surviving 2-pos screw
  terminal with pin 1 = DATA (via R3) and pin 2 = GND.
- **Status LED is on GPIO `17`**, not `20`, and R2 now sits on the **anode**
  side (GPIO17 → R2 → D1 anode, D1 cathode → GND). Moved there 2026-07-20;
  electrically identical to the cathode-side placement, clearer to read.
- **Rail names standardized** to the panel's ×VDC convention: `+3.3VDC` and
  `+5VDC_USB` (the `_USB` suffix is meaningful — this rail is the Teensy's
  USB 5V, a different source from the panel's +5VDC).
- **Spare Teensy pins carry readable net names** (`GPIO 13/LED`, `GPIO 18`
  through `GPIO 23`) instead of auto-generated `unconnected-(U1-…)` strings.
- **R4/R5 RS-485 bias pair added and wired** (review item 3.a): R4 390R 1%
  from +3.3VDC to RS485+, R5 390R 1% from RS485− to GND, **both DNP**. Fitted
  only if the THVD1429's integrated failsafe ever proves insufficient; the
  390R pair gives ≈236mV across the 60Ω loaded bus at 3.3V.

Parts are placed in `hardware/master-pcb/master-pcb.kicad_sch` (rev 0.1). Pin references below use the Teensy symbol's pin *names*
(GPIO numbers); the symbol itself is `master-pcb:Teensy_4.0` (DIP-28-style
numbering, custom — verify against the PJRC pinout card before layout).

**Power spine**
- U1 `3V3` → +3.3VDC rail (power symbol). U1 `VIN(USB5V)` → +5V rail — name it
  `+5VDC_USB` (it is the Teensy's USB 5V via the intact VUSB↔VIN link).
- U1 both `GND` pins, J4.2 (GND tie), all GND points below → GND.
- PWR_FLAG on +3.3VDC, +5VDC_USB, and GND (all are sourced by the module, not a
  KiCad power symbol, so ERC needs the flags).

**RS-485 (U2 THVD1429, C1, R1, J1)**
- U2.1 RO → U1 `0/RX1` · U2.4 DI → U1 `1/TX1` · U2.2 /RE + U2.3 DE tied
  together → U1 `2`
- U2.8 VCC → +3.3VDC, C1 across VCC/GND at U2 · U2.5 GND → GND
- U2.6 A → J1.1 and R1.1 · U2.7 B → J1.2 and R1.2 (R1 120Ω = always-fitted
  master-end termination) · J1.3 — leave unconnected (keying, matches panels)

**INT block (J2, RN1, D2–D4)**
- J2 pin n+1 = INTn from panel n (colors R,O,Y,G,Bu,Br,Gy,W,Bk)
- Each INTn: J2 pin → RN1 element pin (n+2) → Teensy GPIO(3+n) — i.e.
  INT0→`3` … INT8→`11` — and an SRV05-4 IO channel:
  D2 IO1–4 = INT0–3, D3 IO1–4 = INT4–7, D4 IO1 = INT8 (D4 IO2–4 leave NC)
- All three arrays: pin 2 VN → GND, pin 5 VP → +3.3VDC
- RN1 pin 1 (common) → +3.3VDC

**Underglow + GND tie (U3 gate A, R3, J4, C2)**
- U3A: pin 1 /1OE → GND · pin 2 1A → U1 `12` · pin 3 1Y → R3.1
- R3.2 → J4.1 (DATA) · J4.2 → GND (the merged GND-tie position)
- U3 pin 14 VCC → +5VDC_USB with C2 at the pin · pin 7 GND → GND
- Unused gates B/C/D: inputs (5, 9, 12) and /OEs (4, 10, 13) → GND,
  outputs (6, 8, 11) unconnected

**Player DIP (SW1) — GPIO `14`,`15`,`16`**, one switch each, other side of
all three → GND (internal pull-ups, no resistors).

**Status LED** — U1 `17` → R2 (1k) → D1 anode, D1 cathode → GND.

After wiring: annotate is already done (refs assigned); run ERC — expect it
to drop from ~128 unconnected-pin errors to ~spare-GPIO warnings only; the 7
`lib_symbol_mismatch` warnings are the known cached-vs-derived noise class
(same as the panel), verify-once then exclude.
