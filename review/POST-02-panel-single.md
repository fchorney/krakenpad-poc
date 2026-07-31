# Draft post — panel, single-board design

**Title**

> Review Request: 4-layer sensor + LED panel for a dance pad — RP2040, USB-C, RS-485 (THVD1429), 25× WS2815 12 V LEDs, 4× FSR analog inputs

**Images** (upload each separately with its own caption)

| Caption | File |
|---|---|
| Schematic | `02-panel-single/01-schematic-1-panel.png` (or the PDF) |
| 2D PCB top (F.Cu + silkscreen + outline) | `02-panel-single/02-2d-top.png` |
| 2D PCB inner layer 1 (GND) | `02-panel-single/03-2d-in1.png` |
| 2D PCB inner layer 2 | `02-panel-single/04-2d-in2.png` |
| 2D PCB bottom, mirrored | `02-panel-single/05-2d-bottom-mirrored.png` |
| 2D PCB top silkscreen only | `02-panel-single/06-2d-silkscreen-top.png` |
| 2D PCB top, no pour | `02-panel-single/07-2d-top-no-pour.png` |
| 2D PCB inner 1, no pour | `02-panel-single/08-2d-in1-no-pour.png` |
| 2D PCB inner 2, no pour | `02-panel-single/09-2d-in2-no-pour.png` |
| 2D PCB bottom, no pour, mirrored | `02-panel-single/10-2d-bottom-no-pour-mirrored.png` |
| 3D PCB top, plan view | `02-panel-single/11-3d-top.png` |
| 3D PCB bottom, plan view | `02-panel-single/12-3d-bottom.png` |

**Body**

One of nine identical panels in a replacement dance-pad electronics set. Each
panel reads four force-sensing resistors, drives 25 addressable LEDs, and talks
to a master controller over a shared RS-485 bus. All nine boards are the same;
position and bus termination are set by switches, not by build variants.

Board: 127 × 127 mm core, 139.8 mm across the connector tabs, 4 layers, 1.6 mm.
Machine-assembled. Powered from a 12 V bus that daisy-chains through the pad.

- **MCU**: bare RP2040 (QFN-56) with the reference support circuit — 12 MHz
  crystal, W25Q32JV 4 MB QSPI flash, BOOTSEL button, 3-pin SWD header.
- **USB-C** (bench flashing only, not used in play): USB 2.0 16-pin receptacle,
  5.1 k CC pull-downs, 27 R series on D+/D−, USBLC6-2SC6 ESD array on the
  connector side of the series resistors. The pad gets tribocharged by rubber
  soles, so ESD exposure here isn't limited to plug events.
- **Sensors**: 4× FSR on ADC0–ADC3 (GPIO26–29), each a simple divider — FSR to
  3.3 V, 10 k 1 % to ground, 10 nF at the pin. FSRs are off-board on JST PH
  2.0 mm leads, one per cardinal edge.
- **LEDs**: 25× WS2815 (12 V parts, 5 V data). One SN74AHCT125 channel shifts
  3.3 V → 5 V into the first LED, 330 R in series; 100 nF at each LED's VDD.
- **RS-485**: THVD1429, 1 Mbps, in/out on 3-circuit 3 mm connectors so the bus
  daisy-chains through the panel; pin 3 is the cable shield, passed through with
  a 100 nF ‖ 1 M to local ground rather than tied. 120 R termination is switched
  in by a DPDT slide switch (SW3) so any panel can be the last one on the bus.
- **Interrupt out**: open-drain GPIO to a 2-position 5.08 mm screw terminal
  (signal + dedicated ground return), 100 R series and an SMAJ5.0A TVS. This is
  the actual gameplay input path — bus traffic is telemetry only.
- **Power**: 12 V in → AMS1117-5.0 → AP7361C-33ER 3.3 V, a cascade rather than
  two regulators off 12 V, so the second stage's PSRR cleans up the rail feeding
  the ADC. Total logic load is well under 100 mA. An LM66200 ideal-diode mux
  (U8) ORs USB VBUS against the 5 V rail; the two PMEG3015EH Schottkys beside it
  (D12/D23) are DNP footprints kept as a hand-solderable fallback if the mux
  disappoints. 470 µF bulk on the 12 V input.
- **Config**: 4-position DIP for panel ID (values above 8 select diagnostic
  modes in firmware).
- 14 test points.

What I would most like eyes on:

1. Analog integrity: the four ADC channels run a long way across a board that is
   mostly LED switching current. Is the plane/return strategy sane, and are the
   10 nF caps in the right place?
2. The 12 V → 5 V → 3.3 V linear cascade and the ideal-diode OR around U8.
3. WS2815 data topology across 25 LEDs at 60 Hz+, and the single shifter channel
   driving the chain.
4. RS-485 shield handling (pass-through, AC-coupled per panel, DC-grounded only
   at the master).
5. Silkscreen and layout hygiene generally.
