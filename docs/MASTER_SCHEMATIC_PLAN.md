# Master Schematic Plan — pin map & blocks (v0.1, 2026-07-18)

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
| INT ESD | **Multi-channel TVS array(s)** covering all 9 lines, 3.3V working voltage class, whichever is cheapest at BOM time (prototype's single SMBJ3.3A retired) |
| GND tie | KF301-style 1P 5.08mm screw terminal (same part class as panel J9) → short lead to fork terminal on the PSU GND stud |
| Underglow out | **2-position KF301-style 5.08mm screw terminal: GND + LED-DATA-OUT.** Data gating is UI/software config only ("underglow: on/off") — no sense pin for now. Revisit after the harness teardown (splice point still unknown, see UNDERGLOW.md); a 12V-sense position (panel R18/R19-style divider) is the known upgrade path if config-only gating proves annoying |
| Player ID | 3-bit DIP switch to GND, Teensy internal pull-ups (no external resistors) |
| Status LED | 1× LED + series resistor on a spare GPIO (because why not) |
| RS-485 out | Micro-Fit 3.0 3-pin, THT right-angle 43650-0300 — same part and pin-3-unpopulated keying as the panels |
| RS-485 transceiver | THVD1419 (same as panel), 3.3V, DE+/RE tied to one GPIO |
| Termination | 120Ω fixed at the master end (master is always a bus end — no DPDT needed here, unlike panels) |

## Teensy 4.0 pin map (proposal — pins are freely reassignable during layout
except where noted; firmware maps at bring-up, same convention as the panel)

| Pin | Function | Notes |
|-----|----------|-------|
| 0 (RX1) | UART1 RX ← THVD1419 RO | **Hard constraint: UART1 = pins 0/1** |
| 1 (TX1) | UART1 TX → THVD1419 DI | **Hard constraint** |
| 2 | RS-485 DE + /RE (tied) | Matches prototype firmware |
| 3–11 | INT0–INT8 (panels 0–8) | Inputs, external 10k pull-up to 3.3V; any digital pin is interrupt-capable on Teensy 4.x, so this range is pure convenience |
| 12 | Underglow LED data out → shifter A input | 3.3V push-pull |
| 14, 15, 16 | Player-ID DIP bits 0–2 | To GND, `INPUT_PULLUP`, read at boot |
| 20 | Status LED | Any spare pin; 13 avoided so the onboard LED stays independently usable |
| 3.3V | THVD1419 VCC, INT pull-up array top | Teensy onboard regulator, 250mA budget — total load here is single-digit mA |
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
2. **RS-485**: THVD1419 SOIC-8. VCC = 3.3V + 100nF decoupling. RO→pin 0,
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
   (mirrors panel R16) → 2-pos screw terminal (DATA + GND). Three unused
   gates: tie inputs and /OE pins to GND (this board's shifter is always
   powered, unlike a future unpopulated-VCC scenario; the panel left its
   spares NC — either is defensible, GND-tied is the cleaner default for a
   fresh schematic).
5. **GND tie**: KF301-1P screw terminal to the PSU GND stud. This is not
   optional wiring for the user — INT and RS-485 both need the common
   reference (documented bench failure mode without it).
6. **Player-ID DIP**: 3-position DIP to GND on pins 14–16, internal pull-ups.
   Values 0–3 = P1–P4, 4–7 reserved.
7. **Status LED**: LED + 1kΩ on pin 20.
8. **Test points** (space permitting): RS-485 A, B, DE, 3.3V, 5V, GND,
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

- TVS array MPN (cheapest suitable multi-channel part).
- 10k resistor-array MPN/format (cheapest).
- Euroblock 9-pos 5.08mm MPN.
- 3-bit DIP switch MPN.
- Teensy 4.0 KiCad symbol/footprint source + verification.
- Underglow connector final form — 2-pos screw terminal is the interim
  decision; harness teardown decides the splice point and may change it.
- Master PCB target ~80×60mm; enclosure sized after layout.
