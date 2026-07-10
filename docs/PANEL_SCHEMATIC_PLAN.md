# Panel Schematic Plan — pin map & blocks (v0.1, 2026-07-10)

Working plan for `hardware/panel-pcb/`. Draw as one flat schematic or hierarchical
sheets, one block each. All decided parts per `docs/BOM.md`.

## RP2040 pin map (final panel — supersedes prototype pins where noted)

| GPIO | Function | Notes |
|------|----------|-------|
| GPIO0 | UART0 TX → THVD1419 DI | RS-485 |
| GPIO1 | UART0 RX ← THVD1419 RO | RS-485 |
| GPIO2 | RS-485 DE + /RE (tied) | Driver enable, matches prototype firmware |
| GPIO3 | Debug press LED (push-pull) | Optional populate; matches prototype |
| GPIO5 | INT out (open-drain: drive LOW / hi-Z) | Matches prototype firmware |
| GPIO6–9 | DIP switch bits 0–3 | Panel ID 0–8, diag modes 9–13. Switch to GND, internal pull-ups, read at boot (prototype's GPIO6 jumper becomes bit 0) |
| GPIO10 | Termination sense (DPDT pole 2) | Internal pull-up; LOW = terminated |
| GPIO16 | LED data → SN74AHCT125 A-input | PIO WS2815 out |
| GPIO26–29 | ADC0–3: FSR North/East/South/West | 10nF to GND at each pin + 10kΩ divider pull-down |
| QSPI (6 dedicated) | W25Q32JV | Per RP2040 minimal design; BOOTSEL button pulls flash /CS low |
| USB DP/DM | USB-C 16-pin | 27Ω1% series pair per reference design; 5.1kΩ CC pull-downs |
| XIN/XOUT | 12MHz crystal | ABM8 class + load caps per reference design |
| RUN | 10kΩ pull-up + test pad | Optional reset button footprint |
| SWD (SWCLK/SWDIO) | 3-pin header w/ GND | Recovery/debug |

Unused GPIOs → test pads if board space is free.

## Blocks

1. **Power in**: Micro-Fit 2-pin IN + OUT (12V bus, straight-through heavy copper).
   12V → AMS1117-5.0 → AMS1117-3.3 cascade. 10–22µF tantalum/electrolytic on each LDO
   output (ESR required — no ceramic-only), input caps per datasheet. 5V rail feeds ONLY
   the shifter VCC.
2. **RP2040 core**: chip + decoupling set + crystal + flash + USB-C + BOOTSEL + SWD,
   copied from the official "Hardware design with RP2040" minimal design (Appendix A).
   RP2040's internal 1.1V regulator per reference (VREG_IN/VREG_VOUT + caps).
3. **RS-485**: THVD1419, A/B to 3-pin Micro-Fit IN + OUT (bused), 120Ω via DPDT pole 1,
   pole 2 → GPIO10. Fail-safe biasing only if THVD1419 lacks it internally (check
   datasheet at capture time).
4. **FSR inputs ×4**: JST-PH 2-pin → divider (FSR to 3.3V, 10kΩ pull-down on board),
   node → ADC pin with 10nF C0G right at the RP2040.
5. **LED chain**: SN74AHCT125 (5V VCC, 100nF), one gate for LED data (input from
   GPIO16, 3.3V logic OK at AHCT), ~330Ω series R → LED1 DIN. 25× WS2815 in the
   staggered 4-3-4-3-4-3-4 lattice (16.6mm rows / 33.5mm cols, centered — see
   STOCK_PANEL_PCB_MEASUREMENTS.md), serpentine order per PROTOTYPE_LED_LAYOUT.md,
   BIN(n) ← DIN-signal(n-1), BIN(1) ← shifter output. 100nF ≥25V at every LED.
   Spare shifter gates: tie inputs to GND, or use one for the debug LED.
6. **INT out**: GPIO5 → screw terminal / ring stud (single position). Series ~100Ω
   optional for ESD robustness (panel side has no TVS — that lives at the master).
7. **ID/config**: 4-pos DIP to GND on GPIO6–9.
8. **Board**: 127×127mm, mounting holes ~7mm inset (verify calipers), Power12V netclass
   (2mm tracks min, pours for LED VDD/GND — 25 LEDs ≈ 900mA peak this board).
