# Draft post — master board

**Title**

> Review Request: 4-layer USB-HID master controller for a dance pad — Teensy 4.0 (i.MX RT1062), RS-485 bus out, 9 interrupt inputs, WS2815 data out

**Images** (upload each separately, with these captions — rule 7B wants one
labelled link per image, never a link to the project)

| Caption | File |
|---|---|
| Schematic | `01-master/01-schematic-1-master.png` (or the PDF) |
| 2D PCB top (F.Cu + silkscreen + outline) | `01-master/02-2d-top.png` |
| 2D PCB inner layer 1 (GND) | `01-master/03-2d-in1.png` |
| 2D PCB inner layer 2 | `01-master/04-2d-in2.png` |
| 2D PCB bottom, mirrored | `01-master/05-2d-bottom-mirrored.png` |
| 2D PCB top silkscreen only | `01-master/06-2d-silkscreen-top.png` |
| 2D PCB top, no pour | `01-master/07-2d-top-no-pour.png` |
| 2D PCB inner 1, no pour | `01-master/08-2d-in1-no-pour.png` |
| 2D PCB inner 2, no pour | `01-master/09-2d-in2-no-pour.png` |
| 2D PCB bottom, no pour, mirrored | `01-master/10-2d-bottom-no-pour-mirrored.png` |
| 3D PCB top, plan view | `01-master/11-3d-top.png` |
| 3D PCB bottom, plan view | `01-master/12-3d-bottom.png` |

**Body**

Replacement master controller for a 9-panel dance pad, driving open-source
rhythm-game software. It is the host end of a small distributed system: nine
identical sensor/LED panels hang off one RS-485 bus, and each panel also has a
dedicated interrupt wire home-run back to this board so a press is reported
without waiting for the bus cycle.

Board: 77.6 × 65.6 mm, 4 layers, 1.6 mm, all through-hole and 0805-and-up SMD,
hand-assembled.

- **MCU**: Teensy 4.0 module on socket headers (i.MX RT1062, 600 MHz). USB HID
  to the PC; the board itself is USB-powered, there is no 12 V anywhere on it.
- **9× interrupt inputs**: open-drain from the panels, pulled up by a 10 k
  resistor network (RN1), each line protected by a 5 V TVS (D2–D10) plus a
  330 R series resistor and 1 nF to ground for glitch filtering. Inputs land on
  nine JST XH 2.54 mm connectors, one per panel position (silkscreened
  UL/U/UR/L/C/R/DL/D/DR), each carrying signal + its own ground return.
- **RS-485**: THVD1450 half-duplex transceiver, DE/RE tied to one GPIO, 120 R
  termination at this end of the bus, bus out on a 3-circuit 3 mm connector
  (A / B / cable shield). Idle-bus bias resistors R4/R5 are laid out but marked
  DNP — the transceiver has integrated failsafe, so they are insurance only.
- **Underglow LED data out**: one SN74AHCT125 channel shifts 3.3 V → 5 V for an
  external WS2812-family strip, 330 R in series. The strip's own 12 V comes from
  the PSU, not from this board; the two-position screw terminal carries that
  data line plus the mandatory ground tie back to the supply.
- **Player ID**: 3-position DIP switch.
- 10 test points on the rails and the RS-485/underglow signals.

What I would most like eyes on:

1. The interrupt input protection network — TVS, 330 R, 1 nF per line — for
   both the part choice and the layout of the return paths.
2. Ground handling: this board's only ground reference to the rest of the pad is
   the screw-terminal lead to the supply's ground stud, plus the per-panel
   returns on the XH connectors.
3. Anything in the 4-layer stackup / plane splits that will bite me.
4. Silkscreen and general layout hygiene.

Not asking anyone to choose an architecture for me — purely a check on this
board as drawn.
