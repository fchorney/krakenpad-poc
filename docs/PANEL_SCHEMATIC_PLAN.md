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
   **Power/crystal/RUN/TESTEN/SWD portion drafted 2026-07-10** in
   `hardware/panel-pcb/panel-pcb.kicad_sch` (flash and USB-C are now both drafted
   too, see below). ERC-clean (0 errors; remaining warnings are expected — isolated
   global labels for GPIO pins that later blocks will pick up by name). Real values
   sourced from the official
   Raspberry Pi Pico schematic (Appendix B of the Pico datasheet — same RP2040
   support circuitry applies to a bare-chip design), not invented:
   - IOVDD/DVDD: 100nF local decoupling per pin group; VREG_VIN/VREG_VOUT: 1µF each.
   - **ADC_AVDD filter corrected 2026-07-10**: single RC stage, not two — 3V3 → R1
     (201Ω) → ADC_AVDD, with one 2.2µF cap to GND at the ADC_AVDD pin. Verified
     directly against the real Pico 2/RP2350 schematic documentation; an earlier
     two-stage version here was wrong (mistakenly relayed, never actually the
     official design) and has been fixed in the schematic and this doc.
   - TESTEN → GND directly.
   - Crystal: 12MHz, 15pF NP0/C0G load caps on XIN and (after a 1kΩ series R on
     XOUT) on XR; crystal case pins → GND. Confirmed against the official
     "Hardware design with RP2040" guide (Section 2.3, Figure 8): Y1 (ABM8-272-T3,
     CL=10pF) + C2/C3 15pF load caps + R5 1kΩ series resistor — matches exactly.
   - RUN: RP2040 has an internal ~50kΩ pull-up (per the Pico datasheet — Pico
     itself adds no external resistor), but we add an external 10kΩ pull-up anyway
     for a lower-impedance, more noise-immune reset line, plus a test point.
   - **Flash (W25Q32JV) drafted 2026-07-10** — `Memory_Flash:W25Q32JVSS` KiCad
     symbol (SOIC-8, matches BOM footprint exactly). QSPI_SD0-3/QSPI_SCLK/QSPI_SS
     wired directly to the RP2040's existing QSPI global labels; VCC gets a 100nF
     decoupling cap (standard practice, no RP2040-specific value published for the
     flash side). Sourced from "Hardware design with RP2040" Section 2.2 (Flash
     storage), not invented: chip-select (QSPI_SS) has a 10kΩ pull-up to 3V3
     **marked DNF/DNP** (this specific flash's CS defaults high enough on its own;
     kept as a footprint in case a different flash is ever substituted) plus a 1kΩ
     resistor to a BOOTSEL control — official guide uses a 2-pin jumper header
     here, but per CLAUDE.md's existing "BOOTSEL button" decision we used an actual
     momentary pushbutton (`Switch:SW_Push`) instead, pulling QSPI_SS low when
     pressed (combined with a RUN toggle, forces the RP2040 into USB bootloader
     mode). Verified via `kicad-cli sch erc` (0 errors) **and** a netlist export
     (confirmed QSPI_SS net contains exactly {U1 pin56, U2 pin1, R4, R5} and
     nothing extra — ERC alone doesn't catch wiring topology mistakes, learned the
     hard way on the SWD header earlier).
   - **USB-C connector drafted 2026-07-10** — `Connector:USB_C_Receptacle_USB2.0_16P`
     (J2, exact match for the 16-pin USB2.0-only spec; footprint left TBD, exact
     part still being sourced per docs/BOM.md). D+ pins (A6/B6) tied together, D-
     pins (A7/B7) tied together, each pair through its own 27Ω 1% series resistor
     (R6, R7) before reaching the RP2040's existing USB_DP/USB_DM global labels —
     sourced from "Hardware design with RP2040" Section 2.4.1 (USB), same doc used
     for the rest of this block. CC1 and CC2 each get their own 5.1kΩ pull-down to
     GND (R8, R9) — standard USB-C sink/device-only implementation, advertises
     default-current draw to a host. VBUS and SBU1/SBU2 are intentionally left
     unconnected (no-connect flagged) — this panel is not USB-bus-powered (final
     power comes from the 12V LDO cascade per CLAUDE.md Power section), and SBU is
     only relevant for Alt Mode/accessory detection which this plain-USB2.0 design
     doesn't use. GND/SHIELD tied into the board's ground network. Verified via
     `kicad-cli sch erc` (0 errors) and a full netlist export (every net checked
     pin-by-pin: D+/D-/CC1/CC2 each contain exactly their expected members, VBUS/
     SBU1/SBU2 confirmed isolated, no duplicate reference designators). Hit and
     fixed two real bugs while drafting, worth remembering for future hand-authored
     KiCad blocks: (1) an embedded `lib_symbols` copy must be named with the full
     `"Library:SymbolName"` string (e.g. `"Connector:USB_C_Receptacle_USB2.0_16P"`),
     not just the bare symbol name as it appears inside the standalone library
     file — using the bare name silently produces phantom unresolvable pins with no
     parse error. (2) Confirmed (via the flash block's R4, cross-checked against
     its verified-good netlist) that KiCad flips the Y-axis between a symbol's
     library-frame pin coordinates and the schematic sheet even at 0° rotation:
     `absolute = (origin_x + pin_x, origin_y − pin_y)` — X adds directly, Y must be
     subtracted. Got this wrong on the first pass (caught by ERC's pin-not-found
     errors before it caused a silent miswiring), corrected on the second.
3. **RS-485 — drafted** in `hardware/panel-pcb/panel-pcb.kicad_sch` (U5): THVD1419,
   A/B bused across 3-pin IN (J5) + OUT (J6) Micro-Fit connectors (pin3 deliberately
   NC on both, per the "can't be confused with 2-pin power" design intent), 120Ω via
   DPDT (SW2) pole 1, pole 2 → GPIO10 sense line. **No external fail-safe biasing
   needed** — confirmed from TI's own THVD1419 datasheet description: it has internal
   fail-safe biasing (drives RO safe-HIGH on open/short/idle bus) and integrated TVS
   surge protection, so A/B need no external bias resistors. Symbol note: KiCad has
   no exact THVD1419 symbol; used `Interface_UART:THVD1420D` (same SOIC-8 pinout,
   same TI family, confirmed via its `extends` chain to the standard
   `LTC2850xS8`/`MAX481E`-style RO/RE/DE/DI/GND/A/B/VCC pinout) with Value set to
   "THVD1419" to match the actual BOM part. ~RE + DE tied together on GPIO2. ERC
   clean (0 errors); netlist-verified A/B/GPIO0-2/GPIO10 nets and the switch-gated
   termination path are all wired exactly as intended, no shorts, no duplicate refs.
4. **FSR inputs ×4**: JST-PH 2-pin → divider (FSR to 3.3V, 10kΩ pull-down on board),
   node → ADC pin with 10nF C0G right at the RP2040.
5. **LED chain**: SN74AHCT125 (5V VCC, 100nF), one gate for LED data (input from
   GPIO16, 3.3V logic OK at AHCT), ~330Ω series R → LED1 DIN. 25× WS2815 in the
   staggered 4-3-4-3-4-3-4 lattice — **physical measurements 2026-07-10 (supersede
   the old photo-estimate pitches): row pitch 17mm, column pitch 34mm, row 1 at
   Y=11mm from top edge** (see STOCK_PANEL_PCB_MEASUREMENTS.md), serpentine order
   per PROTOTYPE_LED_LAYOUT.md, BIN(n) ← DIN-signal(n-1), BIN(1) ← shifter output.
   100nF ≥25V at every LED. Spare shifter gates: tie inputs to GND, or use one for
   the debug LED.
6. **INT out**: GPIO5 → screw terminal / ring stud (single position), left edge
   ~93mm from top (physical measurement). Series ~100Ω optional for ESD robustness
   (panel side has no TVS — that lives at the master).
7. **ID/config**: 4-pos DIP to GND on GPIO6–9.
8. **Board**: outline per physical measurement — **not a perfect square**, edges
   128/127/128/127mm (X locked ~127mm by the left/right power+RS-485 connectors,
   Y has ~20mm slack per end if ever needed — see CLAUDE.md Physical Dimensions).
   Mounting holes: 4.5mm dia, 114mm center-to-center (X and Y), inset 6mm on two
   edges + 7mm on the other two (verify which two against the frame/standoffs
   before footprint freeze). Standoff height ~6mm, ~35mm usable height budget to
   the panel platform above the PCB. Power connectors top-left/top-right ~25mm
   from top edge; data IN left edge ~103mm from top, data OUT right edge ~95mm
   from top (all physical measurements, STOCK_PANEL_PCB_MEASUREMENTS.md).
   Power12V netclass (2mm tracks min, pours for LED VDD/GND — 25 LEDs ≈ 900mA
   peak this board).
