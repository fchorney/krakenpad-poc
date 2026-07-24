# Master PCB — as-built reference

Condensed 2026-07-23 from `MASTER_SCHEMATIC_PLAN.md` (drafting history,
including the retired SRV05-4 TVS approach and the original Serial1 pin plan,
lives in git). Everything below is verified against the current schematic
netlist.

**Status:** schematic and layout complete, ERC/DRC clean, committed —
**not yet ordered.** External AI review sent 2026-07-23, triage pending.

The master is deliberately simple: **no 12V anywhere on the board, no
regulators, no magnetics.** Everything runs from the Teensy's USB power
(+5VDC_USB rail from VIN + the Teensy's onboard 3.3V regulator). 12V runs
PSU → panel columns directly and never touches this PCB. **The master's GND
must be tied to the PSU ground stud** (J4 pin 2) — INT and RS-485 need the
common reference; separate grounds was a real bench failure mode.

**Assembly: bare PCB fab only, all 19 components hand-soldered** (PCBA's
~$148 fixed overhead buys nothing at 19 parts × 2 boards). Sourcing is by MPN,
not LCSC stock — see `docs/BOM.md` section B. Hot air available; SOT-23-6 and
SOIC-8 are fine. Do not re-litigate: THVD1429 has no DIP equivalent worth the
downgrade (MAX3485CPA loses failsafe + surge), and the passives are 0805 by
choice (hand-placed, not dense).

## Teensy 4.0 pin map (as built, netlist-verified 2026-07-23)

Socketed on two 14-pin edge rows (pads 1–14 and 20–33 = 28 pads; the
short-end 15–19 row, the bottom-side SMT pads, and pad 34/VUSB were all
**deliberately removed** from the symbol/footprint — unreachable or unused
through a socket, and trimming is required for a clean schematic-parity
check). Symbol/footprint vendored from
XenGi/teensy_library (MIT), locally trimmed; 3D model + attribution in
`hardware/master-pcb/3dmodels/`.

| Pad | GPIO | Net | Connects to |
|----:|------|-----|-------------|
| 1, 32 | GND | `GND` | star point |
| 3 | 1 | `STATUS_LED` | R2 1k → D1 |
| 5 | 3 | `RS485_DE` | U2 DE + /RE |
| 6 | 4 | `DIP_ID2` | SW1 |
| 7 | 5 | `DIP_ID1` | SW1 |
| 8 | 6 | `DIP_ID0` | SW1 |
| 9 | 7 | `RS485_RX` | U2 RO — **Serial2 (RX2)** |
| 10 | 8 | `RS485_TX` | U2 DI — **Serial2 (TX2)** |
| 13 | 11 | `UNDERGLOW_DATA` | U3 gate A input |
| 22–30 | 15–23 | `INT_DR` … `INT_UL` | see INT table below |
| 31 | — | `+3.3VDC` | Teensy 3V3 out → U2 VCC, RN1 common, TP4 |
| 33 | — | `+5VDC_USB` | Teensy VIN (USB 5V via intact VUSB↔VIN link) → U3 VCC. **The board taps VIN, not the raw VUSB pad — the on-Teensy VUSB↔VIN bridge must stay intact (do not cut it), or U3 loses its 5V supply.** |
| spare | 0, 2, 9, 10, 12, 13, 14 | — | GPIO13 deliberately spare (onboard LED) |

The one hard constraint is that RS-485 TX/RX sit on a matched hardware UART
pair — **as built that's Serial2 (GPIO 7/8)**, moved off Serial1 during
routing. Reachable pairs on a socketed Teensy 4.0 if ever needed again:
Serial1 0/1, Serial2 7/8, Serial3 15/14, Serial4 16/17, Serial5 21/20.
All digital pins are interrupt-capable, so the INT lines are unconstrained.

### INT block mapping (J2 position ↔ panel ↔ GPIO)

**Note the order: J2 position 1 is panel 8 (DR) and position 9 is panel 0
(UL)** — reversed from the original plan; the netlist is authoritative. Wire
colors are the stock SMX map.

| J2 pos | Net | Panel | Color | TVS | RN1 | Teensy pad / GPIO |
|-------:|-----|-------|-------|-----|-----|-------------------|
| 1 | `INT_DR` | 8 (DR) | Black | D2 | .10 | 22 / GPIO15 |
| 2 | `INT_D` | 7 (D) | White | D3 | .9 | 23 / GPIO16 |
| 3 | `INT_DL` | 6 (DL) | Grey | D4 | .8 | 24 / GPIO17 |
| 4 | `INT_R` | 5 (R) | Brown | D5 | .7 | 25 / GPIO18 |
| 5 | `INT_C` | 4 (C) | Blue | D6 | .6 | 26 / GPIO19 |
| 6 | `INT_L` | 3 (L) | Green | D7 | .5 | 27 / GPIO20 |
| 7 | `INT_UR` | 2 (UR) | Yellow | D8 | .4 | 28 / GPIO21 |
| 8 | `INT_U` | 1 (U) | Orange | D9 | .3 | 29 / GPIO22 |
| 9 | `INT_UL` | 0 (UL) | Red | D10 | .2 | 30 / GPIO23 |

Per line: 10k pull-up to +3.3VDC (RN1, bussed — deliberately **not stiff**,
for the INT-into-dead-panel case) + one SMAJ5.0A unidirectional TVS to GND
(cathode on the line, anode to GND; negative transients forward-conduct at
~0.7V). Each TVS sits **between J2 and everything else** — first thing the
line meets off the connector, anode pad with its own GND via. The discrete
TVS-per-line approach replaced 3× SRV05-4 arrays 2026-07-22 (routing: a shunt
stub per line beats fanning four lines into one 6-pin part; also lower clamp
loop). Orientation matters at assembly.

## Parts (refs → identity)

| Ref(s) | Part | Notes |
|--------|------|-------|
| U1 | Teensy 4.0 (PJRC 15583), socketed | 2× PPPC141LFBN-RC 14-pos female headers |
| U2 | THVD1429DR (SOIC-8) | same part as panels; VCC +3.3VDC, C1 100nF |
| U3 | SN74AHCT125N (DIP-14) | underglow shifter, VCC +5VDC_USB, C2 100nF; gate A: in ← GPIO11, out → R3 330R → J4.1; unused inputs/OEs → GND, outputs NC |
| D1 | status LED 0805 (150080BS75000) | GPIO1 → R2 1k → anode |
| D2–D10 | SMAJ5.0A TVS (DO-214AC) | one per INT line, see table above |
| R1 | 120R | RS-485 termination, always fitted (master is always a bus end — no switch) |
| R4/R5 | 390R 1% — **DNP** | RS-485 failsafe bias (+3.3VDC→RS485+, RS485−→GND). THVD1429's integrated open/short/idle failsafe makes them unnecessary; footprints exist so bias can be added at the one correct bus point if the bench ever disagrees (≈236mV across the 60Ω loaded bus) |
| RN1 | 4610M-101-103LF (SIP-10, 10k ×9 bussed) | pin 1 common → +3.3VDC |
| SW1 | DS01C-254-S-03BE (DIP-3) | player ID 0–7 to GND, internal pull-ups |
| J1 | Micro-Fit 43650-0300 (RS-485 OUT) | A=pin 1, B=pin 2, pin 3 unpopulated — **matches panel J8/J10 exactly** so the cable is straight-through |
| J2 | Molex 0395316009 9-pos 5.08mm pluggable Euroblock (+ 0395337009 plug) | all 9 INT wires detach as one block; no GND position (return rides the power ground network); footprint rebuilt from the real drawing |
| J4 | MRR522-5.08-V 2-pos screw terminal | pin 1 = underglow DATA (from R3), pin 2 = **mandatory GND tie** to the PSU ground stud. DATA position may sit empty if underglow unused |
| TP1–TP8 | THT probe holes | RS485+ / RS485− / DE / +3.3VDC / +5VDC_USB / GND / underglow 3.3V side / underglow 5V side |
| H1–H4 | M3 mounting holes | |

## Layout (as built)

- **4 layers: Sig+Pwr / GND / GND / Sig+Pwr** (JLC04161H-7628 stackup). Each
  outer layer references its adjacent GND plane; no power plane needed —
  logic-only currents (single-digit mA on 3.3V). The +3.3V net is routed on
  the outer layers. In1/In2 stitched liberally (same net), concentrated at J2,
  U2, and the Teensy.
- No SMD parts under the Teensy socket; hot-air approach room around D2–D10
  and the 0805s (electrically they belong at J2, but clear of the tall
  Euroblock body and socket).
- Target ~80×60mm; enclosure to be modeled from the KiCad 3D export once
  boards are in hand.

## Deliberately absent (do not re-add)

- Any 12V input, distribution, or sensing — the master never sees 12V.
- Underglow 5V-in from the stock Daygreen converter (Teensy USB rail already
  provides shifter VCC) and an underglow presence-sense pin — UI/config
  gating for now; the harness teardown may reopen it (a 12V-sense divider like
  the panel's is the known upgrade path).
- GND position on the INT Euroblock — return via the power ground network.
- RS-485 termination switch — master end is always terminated (R1 fixed).

## Open items

- Underglow connector final form — the J4 screw terminal is the interim
  decision; the harness splice point (stock leads crimp into a 12-pin Dupont
  at the old MCU) is decided at teardown and may change it. A GND position
  adjacent to DATA preserves the option of a paired/twisted return wire.
- Master INT filter caps (~1nF per line at the Teensy pins, under the socket)
  — leaning yes, not on the board yet.
- Firmware: USB HID to the PC is the one major master-side piece not started
  (RS-485 + INT handling already proven on the prototype).
