# Master PCB — as-built reference

Condensed 2026-07-23 from `MASTER_SCHEMATIC_PLAN.md` (drafting history,
including the retired SRV05-4 TVS approach and the original Serial1 pin plan,
lives in git). Everything below is verified against the current schematic
netlist.

**Status:** schematic and layout complete, **ERC/DRC clean (both re-run
2026-07-24)** — **not yet ordered.** External AI review (2026-07-23) triaged +
reworked 2026-07-24; **no fab-blockers** — the review's flagged SW1↔Teensy-socket
"short" was a false positive (a rotation-sign error in the manual pad-overlap
math; KiCad's clean copper-clearance DRC was correct all along).

The master is deliberately simple: **no 12V anywhere on the board, no
regulators, no magnetics.** Everything runs from the Teensy's USB power
(+5VDC_USB rail from VIN + the Teensy's onboard 3.3V regulator). 12V runs
PSU → panel columns directly and never touches this PCB. **The master's GND
must be tied to the PSU ground stud** (J4 pin 2) — INT and RS-485 need the
common reference; separate grounds was a real bench failure mode.

**Assembly: bare PCB fab only, hand-soldered** (PCBA's ~$148 fixed overhead
still isn't worth it at this volume × 2 boards; the 2026-07-24 INT-filter
additions — 9× 330R + 9× 1nF, all 0805 — roughly double the passive count but
stay easy to hand-place). Sourcing is by MPN,
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
| 5 | 3 | `DIP_ID2` | SW1 |
| 6 | 4 | `DIP_ID1` | SW1 |
| 7 | 5 | `DIP_ID0` | SW1 |
| 8 | 6 | `RS485_DE` | U2 DE + /RE |
| 9 | 7 | `RS485_RX` | U2 RO — **Serial2 (RX2)** |
| 10 | 8 | `RS485_TX` | U2 DI — **Serial2 (TX2)** |
| 13 | 11 | `UNDERGLOW_DATA` | U3 gate A input + **R15 10k pull-down → GND** (holds U3 input LOW at boot, before firmware drives the pin) |
| 22–30 | 15–23 | `INT_DR` … `INT_UL` | see INT table below |
| 31 | — | `+3.3VDC` | Teensy 3V3 out → U2 VCC, RN1 common, TP4 |
| 33 | — | `+5VDC_USB` | Teensy VIN (USB 5V via intact VUSB↔VIN link) → U3 VCC. **The board taps VIN, not the raw VUSB pad — the on-Teensy VUSB↔VIN bridge must stay intact (do not cut it), or U3 loses its 5V supply.** |
| spare | 0, 1, 2, 9, 10, 12, 13, 14 | — | **GPIO 0/1 (Serial1) kept free on purpose** — the last spare hardware UART (see below). GPIO13 = Teensy onboard LED, now the status LED |

**No board status LED (removed 2026-07-24):** the discrete D1/R2 were dropped as
redundant — the Teensy's onboard LED (GPIO13) serves as the status indicator, so
firmware just drives pin 13. GPIO1 (freed) went to the DIP.

The one hard constraint is that RS-485 TX/RX sit on a matched hardware UART
pair — **as built that's Serial2 (GPIO 7/8)**. The INT lines occupy GPIO 15–23,
which blocks Serial3 (14/15), Serial4 (16/17), and Serial5 (20/21); with Serial2
used by RS-485, that leaves **Serial1 (0/1) as the only free hardware UART** on
the socketed pins — which is why the DIP was placed on GPIO 3/4/5 (plain GPIOs)
rather than 0/1, keeping Serial1 open for a future debug/aux UART.
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

**Per line — two-stage ESD/EMI protection + filter (added 2026-07-24, resolves
review F3/F4):**
1. Off J2 the line first meets an **SMAJ5.0A unidirectional TVS to GND** (Dx,
   cathode on the line / anode to GND, own GND via — negative transients
   forward-conduct at ~0.7V). This is the *entry* node.
2. Then a **330Ω series R** (R6–R14) into the Teensy-side node `INT_xx`, which
   carries the **10k pull-up** (RN1, bussed — deliberately **not stiff**, for the
   INT-into-dead-panel case) + a **1nF C0G cap** (C3–C11) to GND + the Teensy pin.

The series R limits residual ESD current into the Teensy's internal clamp
(~16mA at the ~9.2V TVS clamp — this is what makes a 5V-standoff TVS safe on a
non-5V-tolerant pin) and forms a ~330ns RC low-pass with the cap, killing
ESD/EMI without meaningful press latency (the INT edge is the **sole** game
input — no FSR veto — so integrity matters; see `docs/USB_PROTOCOL.md`). Pull-up
sits on the Teensy-side node so the pin is defined HIGH locally and stays safe
if the series R ever opens. **Per-line refs:** DR=R6/C3, D=R7/C4, DL=R8/C5,
R=R9/C6, C=R10/C7, L=R11/C8, UR=R12/C9, U=R13/C10, UL=R14/C11. TVS orientation
matters at assembly. (The discrete TVS-per-line approach replaced 3× SRV05-4
arrays 2026-07-22.) A twisted-pair signal+GND return is a reserved cabling-only
mitigation if the bench ever shows spurious triggers — no board change needed.

## Parts (refs → identity)

| Ref(s) | Part | Notes |
|--------|------|-------|
| U1 | Teensy 4.0 (PJRC 15583), socketed | 2× PPPC141LFBN-RC 14-pos female headers |
| U2 | THVD1429DR (SOIC-8) | same part as panels; VCC +3.3VDC, C1 100nF |
| U3 | SN74AHCT125N (DIP-14) | underglow shifter, VCC +5VDC_USB, C2 100nF; gate A: in ← GPIO11, out → R3 330R → J4.1; unused inputs/OEs → GND, outputs NC |
| D2–D10 | SMAJ5.0A TVS (DO-214AC) | one per INT line, entry-node ESD clamp — see table above |
| R6–R14 | 330R 0805 | INT series R per line (ESD limit + RC filter into Teensy) |
| C3–C11 | 1nF C0G 0805 | INT filter cap per line, Teensy-side node → GND |
| R15 | 10k 0805 | `UNDERGLOW_DATA` pull-down (defines U3 gate-A input LOW at boot) |
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
- ~~Master INT filter caps~~ **DONE 2026-07-24** — full two-stage TVS + 330Ω
  series R (R6–R14) + 1nF RC (C3–C11) per INT line is now on the board (see the
  INT block section above).
- Firmware: USB HID to the PC is the one major master-side piece not started
  (RS-485 + INT handling already proven on the prototype). Note the master-side
  **glitch-qualify window** requirement (µs-scale, hidden in USB dead-time,
  never average on master) — see `docs/USB_PROTOCOL.md`.
