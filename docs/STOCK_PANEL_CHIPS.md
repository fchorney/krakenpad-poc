# Stock Gen4+ Panel PCB — chip inventory & insights

Companion to `STOCK_PANEL_PCB_MEASUREMENTS.md` (same physical board: Step Revolution
LLC **ASSY M146 040 K-r2**, pulled 2026-07-10 from the user's Gen5 pad). Sources:
`images/font_smx_pcb.png` / `images/back_smx_pcb.png` plus loupe reads of chip
markings by the user (2026-07-11).

## Chip inventory

| Ref(s) | Package | Marking (loupe-read) | Identification | Role |
|--------|---------|---------------------|----------------|------|
| U1 | TQFP-32 | `MEGA 328P U-TH` | **ATmega328P-AU** (Microchip AVR, same MCU as Arduino Uno) | Panel MCU. 16MHz crystal at Y1 |
| U6, U7, U8, U11, U12 | TSSOP-28 ×5 | `TLC5940` | **TI TLC5940** 16-ch constant-current LED sink driver, 12-bit PWM | LED drive: 5 × 16 = 80 channels for 25 RGB LEDs (75 used, 5 spare) |
| ×4 (one per sensor position, SOIC-8 next to each FSR/AMP jumper) | SOIC-8 | `B32` / `04H3` | **Unidentified** — topmark not decoded (checked SMD marking databases 2026-07-11, no confident match) | Sensor analog front-end for the AMP (load-cell) path; bypassed/reconfigured by the per-sensor `AMP`/`FSR` jumper |
| U10 | small SMD, only one on board | `7A` / TI logo / `M5P` | **Unidentified** TI part — topmark not decoded | Unknown. Sits near the south edge of the board (bottom-center-right area) |

Also on board (no marking read needed): 25× 4-pad RGB LEDs (D1–D25, dumb
common-anode type driven by the TLC5940s — **not addressable**), 4-pos DIP switch
(SW1, panel ID), 6-pin right-angle Dupont-style header (J1, bottom-right — matches
the standard 6-pin AVR ISP programming interface for the ATmega328P), RJ-style
modular jacks for data IN/OUT (J3/J2), blue 2-pos screw terminal (SIGNAL/GND), 2×
4-pin power connectors (J9/J10, silkscreened **+5V GND GND +5V**), per-sensor
`AMP`/`FSR` selection jumpers.

To resolve the two unidentified topmarks: TI's part-marking lookup
(https://www.ti.com/packaging/docs/partlookup.tsp) for `M5P`; generic SMD topmark
databases for `B32 04H3`. Neither blocks anything — recorded for completeness.

## Insights for our design

1. **Stock LEDs are not addressable.** Plain RGB LEDs on an 80-channel
   constant-current matrix (5× TLC5940 daisy-chained shift registers). Our
   WS2815 chain replaces 5 driver ICs + 75 routed channels with one shifter
   gate and a single data line — an architectural simplification, not a
   substitution. The 80-channel shift chain also plausibly explains the
   81-byte clock burst in the stock sensor-readback protocol (an 80-bit
   clocked cycle — see `INTERNAL_BUS_PROTOCOL.md` in stepmaniax-sdk-mp).

2. **Stock panels run entirely on 5V.** Power daisy-chain silkscreen reads
   +5V/GND/GND/+5V — the stock MCU box's 12V→5V DC/DC converts *before*
   distribution. Our 12V-to-panel + WS2815 + local LDO design is a real
   departure: don't transfer stock wiring-gauge assumptions (stock pushed
   more amps per conductor at 5V than we will at 12V), and the stock 4-pin
   power connector (paired +5V/GND) is not a precedent for our 2-pin
   Micro-Fit at 12V.

3. **Load-cell support is what the 4 mystery SOIC-8s buy.** One amp chip +
   jumper per sensor position. We dropped load cells, so our panel deletes
   all four plus jumpers — FSR dividers need no active front-end.

4. **Stock MCU is an 8-bit AVR at 16MHz.** The ~6ms lag window and 30Hz LED
   ceiling weren't just protocol choices — the platform had little headroom.
   Our RP2040 (dual-core 133MHz, PIO for LEDs) is orders of magnitude ahead;
   no stock firmware behavior should be treated as a performance ceiling.

5. **Brightness parity is an open question (raised 2026-07-11), testable for
   free.** WS2815/WS2812B drive each color die at a fixed ~12mA; stock's
   TLC5940s could be driving their dies at up to ~20mA (typical 5050 rating),
   i.e. stock may be up to ~1.5× brighter per LED at max (sub-linear in
   perceived output). Two checks before PCB layout, neither requiring
   redesign: (a) measure the IREF-pin resistor on a stock TLC5940
   (unpowered, resistor to GND; max channel current ≈ 39.06 / R_IREF ohms →
   amps) to learn stock's actual drive current; (b) decisive: put the 25×
   WS2812B prototype grid at full white under a real acrylic panel next to a
   lit stock panel and compare by eye. If it falls short, mitigation is a
   different 12V one-wire addressable part — the LED architecture doesn't
   reopen.

6. **Precedents that validate our choices:** screw terminal for the per-panel
   signal wire (ours: INT), 4-position DIP for panel ID, single-ended UART
   bus with no differential transceiver anywhere on the board (consistent
   with the 250kbaud single-ended bus in the protocol deep-dive; our RS-485
   at 1 Mbps is the upgrade path stock never had).
