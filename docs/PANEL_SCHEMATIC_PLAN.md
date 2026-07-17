# Panel Schematic Plan — pin map & blocks (v0.1, 2026-07-10)

Working plan for `hardware/panel-pcb/`. Draw as one flat schematic or hierarchical
sheets, one block each. All decided parts per `docs/BOM.md`.

> **⚠️ Re-annotation notice (2026-07-11):** after the schematic was completed and
> reorganized in the GUI, all reference designators were re-annotated by sheet
> position, and a value/type/footprint normalization pass was applied. Refs in the
> block-by-block draft history below are **pre-annotation** and no longer match the
> schematic. Current authoritative refs:
>
> | Block | Current refs |
> |-------|-------------|
> | RP2040 / flash / crystal | U1 RP2040, U3 W25Q32JV, X1 12MHz, C12/C14 15pF C0G crystal caps, R5 201Ω ADC filter |
> | LDOs | U5 AMS1117-5.0, U6 **AP7361C-33ER-13** (swapped in for AMS1117-3.3 2026-07-16 — `-33ER-` suffix ONLY, plain `-33E-` is pin-reversed); caps: C37+C52 = 2× 10µF 25V X5R 0805 MLCC in parallel (AMS1117-5.0 input, DC-bias derating fix), C38 = 22µF 16V **tantalum** (AMS1117-5.0 output — the only ESR-required spot), C44/C50 = 10µF 25V X5R 0805 MLCC (AP7361C in/out, ceramic-stable) |
> | 12V bulk | C51 470µF 25V **SMD V-chip aluminum electrolytic** (RVT1E471M1010; swapped from TH radial 2026-07-16 — deliberately not tantalum, 12V hot-plug surge) |
> | Power-OR (VBUS/12V) | D12, D23 (**PMEG3015EH** Schottky, SOD-123F — swapped in for 1N5819W 2026-07-16) |
> | USB-C | J1 = GCT **USB4085-GF-A** (decided 2026-07-11, footprint `USB_C_Receptacle_GCT_USB4085`), R3/R4 27Ω 1% series, R13/R14 5.1k CC pull-downs |
> | SWD / BOOTSEL / RUN | J2 SWD header, SW2 BOOTSEL, R7 1k, R12 10k DNF, R1 10k RUN pull-up, TP1 RUN |
> | RS-485 | U2 THVD1419, J8 IN / J10 OUT (Micro-Fit 3-pin), SW3 termination DPDT (E-Switch EG2201A, footprint panel-pcb:SW_EG2201A), R2 120Ω |
> | FSR inputs | J3/J4/J6/J7 = N/E/S/W (JST-PH), R8–R11 10k 1% pull-downs, C16–C19 10nF C0G |
> | LED chain | 25× WS2815 = D2–D11, D13–D22, D24–D28 (chain order ≠ ref order after re-annotation; footprint `panel-pcb:WS2815_PLCC6_5.0x5.0mm_P1.6mm`, corrected 2026-07-13 — the stock PLCC4/PLCC6 footprints are both wrong for WS2815), U4 SN74AHCT125 (SOIC-14), R16 330Ω, per-LED 100nF = C22–C36, C39–C43, C45–C49 (C21 is the shifter VCC decoupler, not an LED cap) |
> | Debug LED | D1 + R15 1k (GPIO3, optional populate) |
> | INT out | R17 100Ω + J9 (decided 2026-07-11: KF301-style 1P 5.08mm screw terminal, footprint `panel-pcb:TerminalBlock_KF301-1P_P5.08mm` — verify drill/pad vs sourced part before ordering) |
> | Panel ID DIP | SW1 (GPIO6–9) |
> | 12V bus | J5 IN / J11 OUT (Micro-Fit 2-pin) |
> | Test points | TP1 RUN, TP2/TP3/TP12 GND, TP4 UART RX, TP5 UART TX, TP6 RS-485 dir (GPIO2), TP7 LED data 3.3V, TP8 +12V, TP9 LED data 5V, TP10 +5V, TP11 +3V3 |
>
> **Part-swap notice (2026-07-16):** mentions of AMS1117-3.3, 1N5819W, and
> radial-TH electrolytics in the block-by-block draft history below are
> superseded by the table above (AP7361C-33ER-13 / PMEG3015EH / all-SMD caps).
> The history is kept as design rationale, not as a parts list.

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
   node → ADC pin with 10nF C0G right at the RP2040. **Components placed (unwired)
   2026-07-11**: J7–J10 (N/E/S/W), R11–R14 (10k), C21–C24 (10nF C0G), top-right of
   sheet; FSR is non-polarized so connector pin assignment (pin 2 → 3V3, pin 1 →
   node) is arbitrary.
5. **LED chain**: SN74AHCT125 (5V VCC, 100nF), one gate for LED data (input from
   GPIO16, 3.3V logic OK at AHCT), ~330Ω series R → LED1 DIN. 25× WS2815 in the
   staggered 4-3-4-3-4-3-4 lattice — **physical measurements, refined 2026-07-13:
   row pitch 17mm, column pitch 33.5mm (not 34mm), insets 13.5mm left / 13.0mm
   right / 12.5mm top+bottom** (see STOCK_PANEL_PCB_MEASUREMENTS.md), serpentine order
   per PROTOTYPE_LED_LAYOUT.md, BIN(n) ← DIN-signal(n-1), BIN(1) ← shifter output.
   Spare shifter gates: tie inputs to GND. **Drafted + netlist-verified 2026-07-11**
   (this block was placed fully pre-connected — net labels directly on pins +
   coincident power symbols — rather than left for hand-wiring like earlier blocks,
   since it's 25 identical cells):
   - **Per-LED cap corrected against the WS2815 datasheet**: 100nF goes **VCC (pin
     1) → GND** (VCC = internal-regulator node, "suspended or connected with a
     filter capacitor to GROUND"; recommended application circuit shows this cap),
     NOT across VDD/GND and no ≥25V requirement — BOM.md updated to match. Pinout
     verified from datasheet: 1 VCC, 2 VDD(+12V), 3 DO, 4 DIN, 5 GND, 6 BIN.
   - Symbol: KiCad has no WS2815, so `LED:WS2813` is embedded (identical pin
     numbering 1–6, verified) with Value "WS2815" and the VCC pin's electrical
     type changed power_in → passive in the embedded copy (so a lone filter cap
     doesn't trip ERC's power-driven check; matches WS2815 VCC semantics).
   - D3–D27 = chain positions 1–25 (schematic order is logical; physical
     serpentine mapping happens at layout). Chain nets `LD0`–`LD24` via local
     labels placed directly on DIN/BIN/DO pins: LDn = {DO(n), DIN(n+1), BIN(n+2)};
     LD0 = {R16, DIN(1), BIN(1), BIN(2)}; last DOUT no-connect flagged. C25–C49 =
     per-LED VCC caps (pin-coincident, pre-connected).
   - U6 = 74AHCT125 flattened from its `extends` parent 74LS125 (file convention:
     no extends in embedded lib_symbols; inner unit names renamed). Unit A: GPIO16
     → gate → R16 330Ω → LD0; /OE grounded. Units B–D: inputs + /OE grounded,
     outputs no-connect flagged. Unit E: 5V power + C50 100nF.
   - GPIO3 debug press LED (R17 1kΩ + D28, optional populate) driven directly
     from the GPIO (push-pull per pin map), not via a spare shifter gate.
   - Verified: ERC clean (only expected spare-GPIO isolated-label +
     embedded-symbol-mismatch warnings) and netlist pin-exact: all 25 chain nets,
     25 VCC-cap nets, +12.0V contains all 25 VDD pins, +5.0V picks up U6-14 +
     C50, spare gates tied/isolated as intended, GPIO16/GPIO3 reach U1.
6. **INT out**: GPIO5 → screw terminal / ring stud (single position), left edge
   ~93mm from top (physical measurement). Series ~100Ω optional for ESD robustness
   (panel side has no TVS — that lives at the master). **Components placed (unwired)
   2026-07-11**: R15 (100R) + J11 (Conn_01x01, "INT OUT" — symbol is generic; actual
   part screw terminal vs ring stud still TBD at layout).
7. **ID/config**: 4-pos DIP to GND on GPIO6–9. **Components placed (unwired)
   2026-07-11**: SW3 (`Switch:SW_DIP_x04`, value PANEL_ID) — pins 1–4 → GPIO6–9
   labels (bit 0 = GPIO6 = pin 1), pins 5–8 commoned to GND; switch ON = GPIO low,
   RP2040 internal pull-ups, no external resistors.
8. **Board**: outline per physical measurement — **not a perfect square**, edges
   128/127/128/127mm (X locked ~127mm by the left/right power+RS-485 connectors,
   Y has ~20mm slack per end if ever needed — see CLAUDE.md Physical Dimensions).
   Mounting holes: 4.5mm dia, 114mm center-to-center — except the **top pair,
   which is 113mm apart** (refined 2026-07-12, calipers + photo cross-check);
   inset 6mm on two edges + 7mm on the other two (verify which two against the
   frame/standoffs before footprint freeze). Standoff height ~6mm, ~35mm usable height budget to
   the panel platform above the PCB. Power connectors top-left/top-right ~25mm
   from top edge; data IN left edge ~103mm from top, data OUT right edge ~95mm
   from top (all physical measurements, STOCK_PANEL_PCB_MEASUREMENTS.md).
   Power12V netclass (2mm tracks min, pours for LED VDD/GND — 25 LEDs ≈ 900mA
   peak this board).
