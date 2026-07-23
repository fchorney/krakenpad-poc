# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Custom replacement hardware for a StepManiaX (SMX) 9-panel dance pad. Replaces the Master Control Unit (MCU) and all 9 Panel PCBs while keeping the existing frame. Targets open-source rhythm game software (Stepmania, ITGmania, DeadSync) — intentionally NOT SMX-compatible (no proprietary auth key, no leaderboard eligibility).

Key improvements over stock SMX hardware:
- Eliminate ~6ms input lag window caused by sensor readback interleaving with the LED update cycle
- USB polling rate increase from 1000Hz → 2000Hz (stretch goal; requires USB HS — see notes)
- LED refresh rate increase from 30Hz → 60Hz+
- Custom open protocol, fully open-source firmware

## Repository Layout (planned)

```
firmware/
  master/
    master.ino    # Master MCU firmware (Teensy 4.x) — RS-485 + INT working; USB HID to PC not yet started
    usb_speed_test/  # USB HS bench-test sketch (see USB Polling Rate note)
  panel/
    main.py       # MicroPython prototype (used during initial breadboard bring-up)
    c/            # C/Pico SDK firmware (final target)
      main.c
      CMakeLists.txt
      ws2812.pio
hardware/
  master-pcb/   # Master MCU board KiCad project (complete, not ordered — see docs/MASTER_PCB.md)
  panel-pcb/    # Panel PCB KiCad project (complete, not ordered — see docs/PANEL_PCB.md)
docs/           # Protocol specs, wiring diagrams, design decisions
```

PCB design tool: **KiCad** (free/open source; prefer over EAGLE which is deprecated by Autodesk).

## Architecture

### System Topology

```
PC (USB HID) ←→ Master MCU (Teensy 4.x)
                      │
                      ├─ RS-485 bus (daisy-chained through all 9 panels)
                      │    └─ LED broadcast + FSR telemetry round-robin polling
                      │
                      ├─ Interrupt wire ×9 (one per panel, home-run to master)
                      │    └─ Open-drain, pulled HIGH at master; panel pulls LOW on press
                      │
                      └─ 12V power (daisy-chained)
```

### Master MCU: Teensy 4.0 or 4.1 (600MHz ARM Cortex-M7)
- USB HID interface to PC (custom descriptor — NOT SMX-compatible)
- 9× hardware interrupt inputs (one per panel); fires immediately on press/release
- Drives RS-485 bus: broadcasts LED frame data, round-robin polls panels for FSR values
- Target USB polling rate: 1000Hz (baseline, free with USB Full Speed) → 2000Hz (requires USB High Speed mode; see USB note below)
- **3-bit DIP switch for player/pad ID (decided 2026-07-10)** — replaces stock SMX's jumper-based P1/P2 selection. 8 values (0–7); only 0–3 (P1–P4) used initially, 4–7 reserved (mirrors the panel-side DIP's spare-code pattern, e.g. for a future master-side diagnostic mode). Whether player ID is actually load-bearing for any given install (vs. left at P1) is a firmware/software decision, not a hardware one — the switch costs 3 spare GPIOs either way, so it's included regardless of whether early builds use it.
- **Master GPIO budget (Teensy 4.0, 40 usable digital pins):** 9 INT inputs + 3 player-ID DIP + 3 RS-485 (TX, RX, DE/RE tied to one pin — as built on Serial2, see `docs/MASTER_PCB.md`) + 1 underglow DATA out = **16 pins used, 24 spare**. USB is handled internally (not GPIO). Comfortable headroom for anything else that comes up (status LEDs, future expansion) — no pin-count risk on this MCU.

### Panel MCU: bare RP2040 (QFN-56) on the panel PCB — decided 2026-07-10
Chosen over the RP2040-Zero module: ~$3–4/panel support BOM vs ~$10 module, full pin control (clean ADC routing incl. GPIO29, 10nF caps at the pins), keeps the 4MB W25Q32JV decision (Zero is fixed 2MB), and the whole panel becomes one flat fab-assemblable PCB. Support circuit per the official "Hardware design with RP2040" minimal reference: 12MHz crystal + load caps, decoupling set, QSPI flash, **USB-C for flashing** (16-pin USB 2.0-only receptacle, **through-hole or TH-reinforced mounting required** — TH pins take the mechanical stress of repeated plug/unplug during flashing. Orientation is free (clarified 2026-07-11, supersedes the 2026-07-10 "vertical preferred"): the port is only used on the bench with the panel top off, so cable clearance is irrelevant; only constraint is receptacle body height must clear the panel platform during play (trivial, ~35mm budget). **Part decided 2026-07-11: GCT USB4085-GF-A** (horizontal, all-TH, stock KiCad footprint, assigned to J1); rejected candidates were GCT USB4105 (SMT signals) and LCSC "vertical DIP" 16P parts (all turned out SMD-vertical on inspection). 5.1kΩ pull-down on each CC pin — required for C-to-C cables; tie duplicated D+/D− pairs), **USBLC6-2SC6 ESD array on D+/D−/VBUS (U7, added 2026-07-17)** — rationale is dance-pad specific: rubber-soled shoes rubbing the panels tribocharge the pad, so USB ESD exposure isn't limited to bench plug events; sits on the connector side of the 27Ω series resistors, flow-through routed — BOOTSEL button, and a 3-pin SWD header (SWDIO/GND/SWCLK) as the firmware-independent recovery/debug path.
- **Core 0**: Tight FSR sampling loop — 4kHz hard target, 8kHz soft target (headroom for oversampling/averaging); drives the open-drain interrupt wire to master on threshold crossing
- **Core 1**: RS-485 comms — receives LED broadcast, replies to FSR telemetry poll; drives 25 local LEDs
- 4× FSR inputs via ADC0–ADC3 (GPIO26–29). GPIO29/ADC3 is usable as a regular ADC on custom PCB (no VSYS monitoring needed)
- 25× WS2815 addressable LEDs (12V native; same 25-LED layout and topology as stock SMX panels). **Prototype uses WS2812B (5V)** — identical protocol, simpler power supply for breadboard testing.
- 4-position DIP switch for panel ID (0–8); values 9–13 select panel-local diagnostic modes (LED check, sensor pressure test, standalone mode, raw ADC streaming, factory reset), 14–15 reserved — see `docs/PANEL_CONFIG.md`
- RS-485 transceiver: **THVD1429** (preferred, final PCB; swapped from THVD1419 on 2026-07-19 — the 1419 is TI's **250kbps** speed grade, too slow for the 1Mbps bus; the 1429 is the 20Mbps grade with identical pinout/package/surge protection/failsafe, and was actually cheaper at LCSC) or **MAX3485** (budget breadboard substitute, same SOIC-8 footprint, 10Mbps) — must be 3.3V-logic compatible. Do NOT use MAX485 (5V-only, logic thresholds marginal at 3.3V)
- **Flash**: W25Q32JV (4MB QSPI, SOIC-8) — required for RP2040 boot. 4MB chosen over 2MB (W25Q16JV) for future-proofing; both are pin-compatible, same footprint, negligible cost difference. Stores firmware + released/pressed animation slots + config. See `docs/ANIMATIONS.md` for flash layout.
- **LED data level shifter**: SN74AHCT125N, VCC = 5V, same for both prototype and final PCB. Its output stays at ~5V regardless — the chip cannot run from 12V (its own VCC abs max is far below that) and doesn't need to: WS2815's DIN comparator threshold is not scaled to its 12V power rail, so a normal 5V logic signal is sufficient (well-established in the addressable-LED community; only the LED VDD/GND pins are 12V, never the data line). Direct 3.3V connection works for WS2812B in practice (spec says 3.5V min) but the shifter is required on the final PCB.

### Sensors
- **FSR only** (no load cell support). Simple voltage divider → ADC. Each panel has 4 FSRs.
- FSR model: **Interlink Electronics FSR 408** (long strip format, iefsr.com). Strip format covers more area than point sensors — better for foot placement detection.
- Voltage divider: 10kΩ pull-down to GND, FSR to 3.3V. 12kΩ is an acceptable substitute — FSR resistance variation unit-to-unit is wide enough that the small shift doesn't matter. Wiring: one FSR lead to 3.3V; other FSR lead connects to both the ADC pin and the top of the pull-down resistor; bottom of resistor to GND. Do NOT connect the FSR lead directly to GND.
- On the final PCB the pull-down resistor is always present on the PCB traces, so an unplugged FSR connector reads near 0 (same as an unloaded FSR) — no special detection needed.
- ADC reference values (12-bit, 0–4095): resting ~100–115, full press ~3900. Threshold of ~500 sits comfortably between noise floor and activation. Per-channel calibration values will be stored in flash config.
- FSR sampling rate: **102,080Hz** achieved on RP2040 in C/Pico SDK — well above target. Targets: **4kHz hard, 8kHz soft** — the soft target preserves headroom for oversampling/averaging (8× averaging cuts ADC noise ~3×, useful for calibration baselines). MicroPython peaks at ~1000–2000Hz with GC jitter; final firmware must be C.

### Power
- 12V, split into 3 parallel column chains (matching original SMX wiring):
  - Left column:   PS → panel 0 (UL) → 3 (L) → 6 (DL)
  - Center column: PS → panel 1 (U)  → 4 (C) → 7 (D)
  - Right column:  PS → panel 2 (UR) → 5 (R) → 8 (DR)
- Each chain carries max 3 panels × 25 LEDs × ~36mA = ~2.7A peak — within Micro-Fit 3.0 5A/pin rating
- 12V runs from the PSU terminals **directly to each column's first panel** (fork/spade lugs at the PSU end, Micro-Fit at the panel end) — the **master PCB is NOT in the 12V distribution path** (corrected 2026-07-10; an earlier plan wrongly gave the master 3 power-out connectors). Master logic is USB-powered.
- **Master GND must be tied into the pad ground network** (e.g. a short lead from a master connector/pad to a fork terminal on the PSU's GND stud) — INT and RS-485 need a solid common reference; separate grounds was a real bench failure mode (see memory: multi-panel bring-up).
- Underglow (resolved 2026-07-10): strips already take 12V/GND from the PSU lugs in the stock harness — the master outputs **DATA only** (5V shifter channel off the Teensy USB rail); ground reference comes via the master's PSU GND tie. Connector type/pinout of the stock data connection still to be checked at master layout time.
- WS2815 LEDs run natively at 12V
- **Panel logic regulation — DECIDED 2026-07-09: cascaded linear LDOs.** AMS1117-5.0 (12V → 5V) feeding the 3.3V stage — **which is AP7361C-33ER-13 as of 2026-07-16** (swapped in for AMS1117-3.3: low-dropout CMOS, ~360mV@1A vs ~1.1V, gives >1V of headroom when powered from USB VBUS alone through the power-OR diode; Vin abs max 6V so it must stay downstream of the 5V stage, never raw 12V; **order the `-33ER-` suffix only** — SOT223R pinout matches AMS1117 GND/OUT/IN tab=OUT, while plain `-33E-` is pin-reversed). **5V power-OR is U8, an LM66200 ideal-diode mux (LCSC C3235556, SOT-583 8-pin) as of 2026-07-20** — auto-selects the higher of VBUS and the AMS1117 5V output, needs no external caps, and removes the Schottky VF drop that left the shifter rail at ~4.7V against a 4.5V VCC minimum. The former power-OR diodes D12/D23 (**PMEG3015EH**, SOD-123F, 255mV typ @100mA; they had replaced 1N5819W on 2026-07-16) remain on the board as **DNP footprints** — populating both and removing U8 restores the Schottky OR as a hand-solderable rescue. Discrete cross-coupled P-FETs were considered and rejected: with both supplies present both FETs turn off and conduction falls to the body diodes, sagging the rail to ~4.3V — worse than the Schottky. Cap rules: AMS1117-5.0 output (C38) = 22µF 16V tantalum (needs ESR — ceramic-only can oscillate); AMS1117 input (C37) and both AP7361C caps (C44/C50) = 10µF 0805 MLCC (AP7361C is ceramic-stable per datasheet). The 5V rail powers only the SN74AHCT125N level shifter (single-digit mA — this rail was previously missing from the design; the prototype borrowed a separate USB 5V supply, see docs/archive/PROTOTYPE_WIRING.md, but the final panel is self-contained on 12V bus power). The 3.3V rail powers RP2040, RS-485 transceiver, and remaining logic (~30–50mA total).
  - Why cascade over two parallel LDOs off 12V: second stage's PSRR gives a very clean 3.3V for the RP2040's noise-sensitive ADC (FSR readings); dissipation splits favorably (~0.35W in the 5.0, ~0.07W in the 3.3 — fine for SOT-223 with a copper pour); same part family = BOM coherence.
  - Why not a buck (or buck + LDO hybrid): total logic draw is ~0.44W worst case from 12V — no thermal problem to solve. A buck adds inductor/caps/layout risk, switching noise near the FSR analog inputs, and at single-digit mA loads many bucks drop into PFM/pulse-skipping with worse ripple. Hybrid only becomes worthwhile if logic-rail load grows to ~200mA+.
  - 3.3V-stage dropout is a non-issue since the AP7361C swap (~tens of mV at the ~30–50mA panel load); the old AMS1117-3.3's ~1.1V dropout was the USB-VBUS-only weak point that motivated the swap.
  - **AP2112K-3.3 is NOT usable anywhere here** — absolute max input 6.5V, far below 12V. MCP1804 (28V max) remains a fallback if AMS1117 sourcing fails.
  - Bench-verify the cascade on a breadboard before committing to panel PCB layout (AMS1117 breakout modules are cheap and readily available).
  - LED strip VDD/GND stay straight 12V, untouched — the shifter only drives the data line into the first LED, never LED power.
- RS-485 bus termination: 120Ω resistor at master end and at panel 8 (last in chain)

### Connectors
All inter-panel connectors use **Molex Micro-Fit 3.0**, right-angle PCB-mount on all boards (panel PCBs sit bare on standoffs, no enclosure). Positive latch, 5A/pin rating, vibration resistant. Different pin counts prevent cross-connection.

| Signal | Connector | Pins | Wire gauge | Notes |
|--------|-----------|------|------------|-------|
| Power | Micro-Fit 3.0 | 2-pin | 2×20 AWG jacketed | Column daisy-chain |
| RS-485 | Micro-Fit 3.0 | 3-pin | 1 twisted pair 22-24 AWG (2 conductors, no GND) | 3rd pin deliberately kept unpopulated (2026-07-10) — makes the connector physically distinct from 2-pin power so 12V can never plug into the transceiver |
| INT (per panel) | Screw terminal — KF301-style 1P 5.08mm (decided 2026-07-11; J9, footprint `panel-pcb:TerminalBlock_KF301-1P_P5.08mm` — verify drill/pad vs sourced part before ordering) | 1 conductor | 24 AWG | Home-run to master. Single wire (2026-07-10): no GND conductor — return rides the shared power ground network (requires the master GND tie). Can't be misplugged |
| INT (master) | 9–10 pos pluggable terminal block (Euroblock) | 9 conductors | — | One position per panel (+ spare, fine if empty). Pluggable = all 9 wires detach from the master as a single block. Replaced the 10-pin Micro-Fit + shared-GND idea — a lone GND pin had nowhere sensible to land across 9 discrete wires. **Wire color per panel** identifies each line — stock SMX map adopted: 0=Red, 1=Orange, 2=Yellow, 3=Green, 4=Blue, 5=Brown, 6=Grey, 7=White, 8=Black (see docs/BOM.md); slot mapping feeds panel-ID mismatch detection |
| FSR (internal) | JST-PH | 2-pin | thin | Top-entry PCB-mount (B2B-PH-K), internal to panel. Corrected from JST-XH 2026-07-10: existing FSR leads use PHR-2 plugs — verify against a real lead before footprint freeze |

**All 9 panel PCBs are identical** — any panel works in any position (identity is DIP-switch/software, termination is the DPDT switch). No position-specific builds; a panel at the end of a chain simply leaves its OUT connector empty.

Each panel PCB has:
- Power IN (2-pin) + Power OUT (2-pin)
- RS-485 IN (3-pin) + RS-485 OUT (3-pin)
- INT screw-terminal/stud (single conductor) — home-run only, no OUT
- 4× FSR (2-pin JST-PH, one per cardinal edge)

### Interrupt Wire
- Open-drain: RP2040 GPIO sinks to GND on press; pulled HIGH (3.3V) at master via resistor
- GND return shared with power cable — no dedicated return conductor needed at these cable lengths
- TVS diode at master PCB for ESD protection on all 9 INT lines
- Disconnected/floating wire reads HIGH (not pressed) — safe failure mode

### LED Layout
- 25 LEDs per panel, same physical topology as stock SMX panels
- Serpentine wiring: rows alternate left-to-right and right-to-left. Animation tools must account for this mapping. See `docs/ANIMATIONS.md`.
- Refresh rate target: 60Hz+ (RS-485 bandwidth is not a constraint at these speeds)
- Animation format: **APNG or animated WebP** (full RGBA, 60fps+, human-viewable without tooling). GIF is ruled out — centisecond frame timing unreliable at 60fps, no alpha channel. See `docs/ANIMATIONS.md`.
- Each panel stores a **released** and **pressed** animation in flash. Released plays by default; pressed plays on the active panel when an FSR threshold is crossed. RS-485 LED data from master takes priority over both — panel falls back to local animation if no RS-485 frame arrives within 100ms.
- Default animations are compiled into firmware as `const` arrays. Factory reset erases the flash animation slots; firmware detects missing magic bytes on boot and writes defaults automatically.

### Daisy-Chain Order
Matches original SMX physical routing (serpentine): MCU → 0(UL) → 3(L) → 6(DL) → 7(D) → 4(C) → 1(U) → 2(UR) → 5(R) → 8(DR)

## Key Engineering Notes

### USB Polling Rate
- **Confirmed 2026-07-08 by bench test** (`firmware/master/usb_speed_test/`, `tools/usb_speed_test.py`): Teensy 4.0 negotiates USB 2.0 High Speed (480 Mbps) automatically on stock Teensyduino — `usb_high_speed` flag reads 1 with no configuration, and measured bulk CDC throughput exceeded 100 Mbit/s (vs Full Speed's 12 Mbit/s ceiling — an order of magnitude margin, not a close call).
- **1000Hz**: free baseline, works even at Full Speed.
- **2000Hz–8000Hz**: HS microframes are 125µs; an interrupt endpoint's `bInterval` sets the polling period as `2^(bInterval-1)` microframes. Teensyduino's own RawHID/Joystick descriptors already ship with **`bInterval = 1` → 125µs → 8000Hz** on Teensy 4.x — this is off-the-shelf, not custom stack work. Our custom HID descriptor just needs to set `bInterval` to the desired rate (1 for 8000Hz, 2 for 4000Hz, 4 for 2000Hz, 8 for 1000Hz).
- Original assumption that HS mode "requires custom USB descriptor/stack work" was **wrong** — corrected after empirical test. The main open question now is achieved *host-side* polling consistency (OS/driver jitter), not whether the device can offer the rate.

### 6ms Lag Root Cause (for reference)
Stock SMX LED cycle: 681 bytes × 40µs/byte at 250kbaud = 27.24ms per cycle (~36.7Hz), leaving ~6ms idle at 30Hz. Sensor readback (`'B?P'` + 81-byte clock burst = 3.2ms) runs in this gap. A press landing during the readback window is missed until the next cycle. The dedicated interrupt wire bypasses this entirely — press detection is independent of the RS-485 bus cycle.

### RS-485 Baud Rate: 1 Mbps
At 1 Mbps: full 225-LED broadcast (~750 bytes with overhead) = 7.5ms; FSR round-robin poll (9 panels, ~144 bytes) = 1.5ms; total ~9ms per cycle → ~110Hz ceiling. 60Hz LED refresh is comfortably achievable. Cable runs are < 3m so 1 Mbps is reliable without needing tight impedance control. Baud rate should be a compile-time constant in firmware to allow easy adjustment.

### LED 30Hz → 60Hz
Stock 30Hz is a 250kbaud bandwidth ceiling, not an LED hardware limit. At 1 Mbps RS-485, 60Hz+ is straightforward.

### Firmware Language
Panel firmware is **C/Pico SDK** for the final build. MicroPython was used during initial breadboard bring-up (`firmware/panel/main.py`) to iterate quickly, but it cannot reliably hit the 4000Hz FSR sampling target due to GC pauses and ADC overhead (~1000–2000Hz in practice). The C firmware achieved **102,080Hz** FSR sampling in bench testing. WS2812B is driven via RP2040 PIO (handled by the SDK's `ws2812.pio` program), which runs independently of both cores. Build system: CMake + arm-none-eabi-gcc (Arm GNU Toolchain, installed via `brew install --cask gcc-arm-embedded`). PICO_SDK_PATH must be set to `~/pico-sdk`.

## Reference: Original SMX Protocol

See `../stepmaniax-sdk-mp` (branch `fc/data-bus-deep-dive`) → `docs/INTERNAL_BUS_PROTOCOL.md` for the full reverse-engineered stock protocol, including:
- UART 250kbaud 8N1 data bus, BREAK-terminated commands
- LED commands `'4'`/`'2'`/`'3'` at 30Hz (inner 3×3 + outer top/bottom)
- Sensor readback via `'B?P'` + 81-byte clock + parallel 80-bit signal-wire response per panel
- Boot sequence: `'R'` reset → `'w'` config write → `'G'` poll loop → lighting
- Physical daisy-chain order (serpentine, confirmed by logic capture)

## Physical Dimensions

**Panel PCB:** 5×5 inches (127×127mm) — matches existing cavity and standoff positions exactly. Do not increase to 6×6; the 5×5 fits comfortably and preserves standoff compatibility.
- **X (left-right) is fixed at ~127mm** — the power/RS-485 in/out connectors mount on the left/right edges, so this dimension can't grow without relocating those connectors.
- **Y (top-bottom) has slack (2026-07-10, from physical inspection): up to ~20mm extra per end, ~40mm total** if a design ever needs it — nothing edge-critical mounts on the top/bottom edges the way connectors do on left/right. Not currently planned to use this; noted as available headroom.

**Master MCU enclosure:** Currently houses an Arduino Micro in a small enclosure. Teensy 4.0 (35.6×17.8mm) is smaller than the Arduino Micro (48×18mm), but the master board has several connectors: 1× RS-485 out (Micro-Fit 3-pin), 1× INT bundle (9–10 pos pluggable terminal block), 1× combined GND-tie + underglow-DATA screw terminal (**J4, a single 2-position KF301-style block as drawn 2026-07-20** — pin 1 = underglow DATA, pin 2 = the mandatory GND lead to the PSU ground stud; these were two separate connectors until the merge), 1× USB. No 12V distribution — power runs PSU → columns directly (stock 12V→5V DC/DC converter is removed; master logic is USB-powered). Master PCB target ~80×60mm; enclosure to be sized once PCB layout is known.
