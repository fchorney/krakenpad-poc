# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Custom replacement hardware for a StepManiaX (SMX) 9-panel dance pad. Replaces the Master Control Unit (MCU) and all 9 Panel PCBs while keeping the existing frame. Targets open-source rhythm game software (Stepmania, ITGmania, DeadSync) — intentionally NOT SMX-compatible (no proprietary auth key, no leaderboard eligibility).

Key improvements over stock SMX hardware:
- Live per-sensor FSR telemetry *during gameplay*, which stock hardware cannot do at all (see "Concurrent Telemetry" below)
- USB polling rate increase from 1000Hz → 2000Hz (stretch goal; requires USB HS — see notes)
- LED refresh rate increase from 30Hz → 60Hz+
- Custom open protocol, fully open-source firmware

## Repository Layout

```
firmware/
  master/
    master.ino       # Master MCU firmware (Teensy 4.x) — RS-485 + INT working; USB HID to PC not started
    usb_speed_test/  # USB HS bench-test sketch (see USB Polling Rate note)
  panel/
    main.py          # MicroPython prototype (initial breadboard bring-up)
    c/               # C/Pico SDK firmware (final target) — main.c, CMakeLists.txt, ws2812.pio
hardware/
  master-pcb/    # Master MCU board (complete, not ordered — docs/MASTER_PCB.md)
  dual-panel/    # THE panel design: carrier + brain, one project, one .kicad_pcb
                 #   (complete, not ordered — docs/DUAL_PANEL.md)
    panel/       #   panelisation + JLC fab package generators
    fit-test/    #   1:1 printable cavity fit template
docs/            # Protocol specs, as-built references, design decisions
  archive/       # Superseded material, banner-marked, kept for the rationale
review/          # r/PrintedCircuitBoard image package (tools/gen_review_images.py)
tools/           # BOM/census/fab/bench scripts
```

**Both firmware trees use breadboard pin numbers and match neither board.** The
as-built RP2040 GPIO map is in `docs/DUAL_PANEL.md`; the Teensy map is in
`docs/MASTER_PCB.md`.

**The single-board panel (`hardware/panel-pcb`) was retired 2026-08-04** when the
two-board split won on measured FSR isolation at near-neutral cost. It is in git
history, last present at `1b41d1c`; the evidence is `docs/archive/DESIGN_COMPARISON.md`.
Panel reference designators below are **`dual-panel`'s** (carrier **2xx**, brain
**3xx**). Git history, the archived reviews and any external review thread from
before 2026-08-04 use `panel-pcb`'s numbering — `docs/archive/REFDES_TRANSLATION.md`
maps the two. Master refdes are unaffected and unchanged.

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
Chosen over the RP2040-Zero module: ~$3–4/panel support BOM vs ~$10 module, full pin control (clean ADC routing incl. GPIO29, 10nF caps at the pins), keeps the 4MB W25Q32JV decision (Zero is fixed 2MB), and the whole panel becomes one flat fab-assemblable PCB. Support circuit per the official "Hardware design with RP2040" minimal reference: 12MHz crystal + load caps, decoupling set, QSPI flash, **USB-C for flashing** (16-pin USB 2.0-only receptacle, **through-hole or TH-reinforced mounting required** — TH pins take the mechanical stress of repeated plug/unplug during flashing. Orientation is free (clarified 2026-07-11, supersedes the 2026-07-10 "vertical preferred"): the port is only used on the bench with the panel top off, so cable clearance is irrelevant; only constraint is receptacle body height must clear the panel platform during play (trivial, ~35mm budget). **Part as built: GCT USB4085-GF-A** (horizontal, all-TH), LCSC **C7095263**, footprint `dual-panel:USB_C_Receptacle_GCT_USB4085_EdgeTrim`, **on the brain as J305** — it moved off the carrier 2026-07-28. The vertical LCK TCF829D (C53184807) was the earlier pick; its footprint `USB_C_Receptacle_LCK_TCF829D_TEMPLATE` is still in `dual-panel.pretty` as a deliberate revert path, so going back to vertical is a footprint re-pick. Note **J305's contact holes sit at exactly 0.45mm edge-to-edge — zero margin against JLC's multilayer floor** (re-verified 2026-08-04); the `.kicad_dru` pad-to-pad rule is relaxed to 0.45mm for it, and it stays on the DFM-confirmation list. Rejected earlier: GCT USB4105 (SMT signals). 5.1kΩ pull-down on each CC pin — required for C-to-C cables; tie duplicated D+/D− pairs), **USBLC6-2SC6 ESD array on D+/D−/VBUS (U305, added 2026-07-17)** — rationale is dance-pad specific: rubber-soled shoes rubbing the panels tribocharge the pad, so USB ESD exposure isn't limited to bench plug events; sits on the connector side of the 27Ω series resistors, flow-through routed — BOOTSEL button, and a 3-pin SWD header (SWDIO/GND/SWCLK) as the firmware-independent recovery/debug path.
- **Core 0**: Tight FSR sampling loop — 4kHz hard target, 8kHz soft target (headroom for oversampling/averaging); drives the open-drain interrupt wire to master on threshold crossing
- **Core 1**: RS-485 comms — receives LED broadcast, replies to FSR telemetry poll; drives 25 local LEDs
- 4× FSR inputs via ADC0–ADC3 (GPIO26–29). GPIO29/ADC3 is usable as a regular ADC on custom PCB (no VSYS monitoring needed)
- 25× WS2815 addressable LEDs (12V native; same 25-LED layout and topology as stock SMX panels). **Prototype uses WS2812B (5V)** — identical protocol, simpler power supply for breadboard testing.
- 4-position DIP switch for panel ID (0–8); values 9–13 select panel-local diagnostic modes (LED check, sensor pressure test, standalone mode, raw ADC streaming, factory reset), 14–15 reserved — see `docs/PANEL_CONFIG.md`
- RS-485 transceiver: **THVD1429** (preferred, final PCB; swapped from THVD1419 on 2026-07-19 — the 1419 is TI's **250kbps** speed grade, too slow for the 1Mbps bus; the 1429 is the 20Mbps grade with identical pinout/package/surge protection/failsafe, and was actually cheaper at LCSC) or **MAX3485** (budget breadboard substitute, same SOIC-8 footprint, 10Mbps) — must be 3.3V-logic compatible. Do NOT use MAX485 (5V-only, logic thresholds marginal at 3.3V)
- **Flash**: W25Q32JV (4MB QSPI, SOIC-8) — required for RP2040 boot. 4MB chosen over 2MB (W25Q16JV) for future-proofing; both are pin-compatible, same footprint, negligible cost difference. Stores firmware + released/pressed animation slots + config. See `docs/ANIMATIONS.md` for flash layout.
- **LED data level shifter**: **SN74AHCT1G125 (SOT-23-5, single gate) as of 2026-08-03** — swapped in on all three boards (panel, dual-panel brain, master) from the quad SN74AHCT125 after an external review noted only one gate was used. The reviewer suggested 74LVC1G34/74LVC1G17; both were rejected on datasheet grounds (LVC at 5V has VIH = 0.7×VCC = 3.5V; the 1G17's Schmitt VT+ max reaches 3.33V at 5.5V — same marginal-threshold trap as MAX485). AHCT keeps the guaranteed TTL VIH = 2.0V. LCSC **C7484**, DigiKey **296-4708-1-ND**, MPN SN74AHCT1G125DBVR; OE̅ (pin 1) tied to GND. Breadboard prototype keeps the quad SN74AHCT125N DIP. VCC = 5V in all cases. Its output stays at ~5V regardless — the chip cannot run from 12V (its own VCC abs max is far below that) and doesn't need to: WS2815's DIN comparator threshold is not scaled to its 12V power rail, so a normal 5V logic signal is sufficient (well-established in the addressable-LED community; only the LED VDD/GND pins are 12V, never the data line). Direct 3.3V connection works for WS2812B in practice (spec says 3.5V min) but the shifter is required on the final PCB.

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
- **WS2815 draws ~15mA per PIXEL at full white, NOT per channel. Its three dies are wired in SERIES across the 12V rail**, and when fewer channels are lit the others are shorted out by an internal transistor/resistor pair to hold the current constant. Two consequences that break normal RGB intuition: **pure red draws the same as full white** (measured on the bench 2026-08-16 and corroborated by WS2815 community measurements), and **only PWM duty reduces power — colour choice never does.** Firmware and animation power policies must not treat white as a worst case relative to a saturated single colour; they are the same load
- **Per-panel 0.44A = 25 LEDs × 15mA full white + ~60mA brain.** Each column chain carries 3 panels = **~1.3A**, trivial against Micro-Fit 3.0's 5A/pin rating — the 12V path no longer has a thin margin anywhere
- **Whole-pad 12V budget:** 9 panels × 0.44A = **4.0A** + underglow 44 groups × 55.5mA = **2.44A** → **~6.34A / 76W** at full white everywhere; **~7.6A / 91W** if sized at 20mA/pixel and 20mA/channel for binning headroom. Master is USB-powered and draws nothing from 12V. **DECIDED 2026-08-16: the stock 12V 8.5A YU1208 is retained** — 75% loaded at the datasheet worst case, which is a state nothing will ever command. A 10A/15A supply remains a drop-in upgrade (it sits entirely upstream of every board) but no purchase is needed
- **Underglow is WS2811 and behaves the OPPOSITE way to the panels' WS2815 — verified against both datasheets 2026-08-16.** WS2811 has three *independent* constant-current sinks (18.5mA each, 3 series dies per channel at 12V), so **underglow red draws ⅓ of white and colour choice scales current linearly**. WS2815 stacks its dies in series, so **panel red draws exactly the same as white and only PWM duty matters**. Current models: underglow `18.5mA × (r+g+b)/255` per group; panel `8.7mA × max(r,g,b)/255` per pixel (measured, confirmed on four bench points). **A single global power/animation policy is therefore wrong** — model the two separately. Full treatment in `docs/UNDERGLOW.md` → "Current draw". Unlike the WS2815 figure, the long-standing underglow estimate needed **no correction**
- **There is no current sensing anywhere in the pad** — the master is USB-powered and deliberately outside the 12V path, so every power figure here is an open-loop model prediction. The trunk's **inline fuse is the only current protection** and is not optional; the stock ceiling (the Daygreen converter's 75W limit) is deleted along with the converter. See `hardware/harness/12v-trunk.yml`
- **12V buys BOTH lower current and lower power.** Bench A/B, one LED at full white so neither strip is voltage-sagging: **WS2815 0.108W vs WS2812B 0.247W — ~44% of the power for comparable brightness** (brightness confirmed by eye at 25 LEDs). Current is also **2.7× lower**, which is the harness win. The old "~2× the power per LED, because it linearly drops ~9V as heat" claim is **wrong and superseded**: three dies in series total ~8.4V, so the regulator drops ~3.6V, not 9V. Compare per-LED figures only at equal, unsagged rail voltage — a 25-LED WS2812B comparison read 3.59W vs 3.16W and looked like a wash purely because the 5V strip had drooped to 4V
- **WS2815 is constant-current; WS2812B is not, and the bench showed the difference plainly.** The WS2812B strip's per-LED draw fell from 49.3mA (1 LED) to 28.7mA (25 LEDs) as its rail sagged, and its white÷red ratio compressed from the expected 3.0 to 2.24. The WS2815 showed none of this — red exactly equalled white and 1 LED scaled cleanly to 25. **That uniformity regardless of position in the chain or drop along the harness is the reason the panels want this part**
- **WS2815 quiescent is high: ~1.84mA/LED** (vs ~0.07mA for WS2812B) — ~0.41A / 5W across all 225 panel LEDs with everything dark. The price of the constant-current and dual-signal circuitry. Trivial against a 10A supply, but "all off" is not free
- **Per-colour heat asymmetry — matters for animations, not for the power budget.** Because unlit dies are shorted out, pure red leaves the regulator dropping ~10V of 12V at unchanged current: ~83% of the power becomes heat for one die's worth of light. **White is the most light-efficient state on this part; saturated single colours are the least.** Current (and so PSU/wiring sizing) is identical either way, but a long saturated-colour idle animation heats the LED ICs more than the same animation in white. Not near any limit at ~4.5W/panel — just the opposite of the usual intuition
- **Corrected 2026-08-16; everything above replaces earlier figures.** The long-standing **47mA/LED** (and **36mA** before it) came from reading the datasheet's 15mA as per-channel and multiplying by three. A 25-LED cut of a `HD-12v-WS2815-144L-B-IP30` strip at a verified 11.7V measured **10.5mA/pixel, red exactly equal to white** — ~30% under datasheet, i.e. ordinary binning, and inside the community range of 8.3–13.5mA/pixel. Downstream items that moved: PSU 20A→10A, panel trunk class, F201/Q201 sizing. Method, raw readings and sources: `hardware/harness/README.md` → "Power budget". Bench firmware: `firmware/panel/c/led_current_test/`
- 12V runs from a **Wago 221-415 lever-block fan-out** directly to each column's first panel (Micro-Fit at the panel end) — the **master PCB is NOT in the 12V distribution path** (corrected 2026-07-10; an earlier plan wrongly gave the master 3 power-out connectors). Master logic is USB-powered. The fan-out sits physically where the stock Daygreen converter was, and is fed by the PSU's own captive 18 AWG cable through an inline fuse. **There are no "PSU terminals" and no fork/spade lugs** — that was a pre-teardown assumption; the stock supply is a brick with a single captive output and no terminal block. Corrected 2026-08-16, see `hardware/harness/12v-trunk.yml`.
- **Master GND must be tied into the pad ground network** — a dedicated lead from master J2 pin 2 to a **GND port on the fan-out** (not a "PSU GND stud", which does not exist). It gets its own lever port rather than riding the underglow cable, so unplugging underglow cannot break the reference. INT and RS-485 need a solid common reference; separate grounds was a real bench failure mode (see memory: multi-panel bring-up).
- Underglow: the strips take 12V/GND from the **Wago fan-out** and DATA from the master (5V shifter channel off the Teensy USB rail, J2 pin 1); ground reference comes via the master's mandatory GND tie, which gets its own lever port and its own lead. **Connector confirmed 2026-08-08 by full pad teardown: a 3-pin SM 2.5, pinout `pin 1 = GND, pin 2 = DATA, pin 3 = 12V`** — the *reverse* of the order originally assumed; building to the old order would put 12V into DATA and destroy the first LED. J2's screw terminal is final, not interim. **We DO build this pigtail** — removing the SMX wiring takes the stock `SMR-03V-B` with it, leaving only the strips' moulded plug to mate; use a pre-made LED-strip pigtail pair rather than loose contacts (`SMM-003T-P0.5` is 28–30 AWG, too fine). An earlier "the master may only need one wire into a YLP-01V" note was **conditional on keeping the stock harness and does not apply**. Underglow is **WS2811**, 44 groups × 3 LEDs — see `docs/UNDERGLOW.md`, and note its colour/current behaviour is the *opposite* of the panels' WS2815.
- **STOCK SMX DOCUMENTATION IS A SEPARATE TREE: `stock-smx/`.** 12 WireViz harness drawings, the teardown bench sheet, stock wire colours, the stock connector/gender index and the stock panel PCB reference. It is descriptive and deliberately independent of this project's design, so it stays useful as working knowledge of factory SMX hardware. **Our replacement harnesses live in `hardware/harness/` and the two must not be merged** — when a stock finding bears on our design, the note goes in the *our-design* file, never in the stock one.
- WS2815 LEDs run natively at 12V
- **Panel logic regulation — DECIDED 2026-07-09: cascaded linear LDOs.** AMS1117-5.0 (12V → 5V) feeding the 3.3V stage — **which is AP7361C-33ER-13 as of 2026-07-16** (swapped in for AMS1117-3.3: low-dropout CMOS, ~360mV@1A vs ~1.1V, gives >1V of headroom when powered from USB VBUS alone through the power-OR diode; Vin abs max 6V so it must stay downstream of the 5V stage, never raw 12V; **order the `-33ER-` suffix only** — SOT223R pinout matches AMS1117 GND/OUT/IN tab=OUT, while plain `-33E-` is pin-reversed). **5V power-OR is U304, an LM66200 ideal-diode mux (LCSC C3235556, SOT-583 8-pin) as of 2026-07-20** — auto-selects the higher of VBUS and the AMS1117 5V output, needs no external caps, and removes the Schottky VF drop that left the shifter rail at ~4.7V against a 4.5V VCC minimum. The former power-OR diodes D301/D302 (**PMEG3015EH**, SOD-123F, 255mV typ @100mA; they had replaced 1N5819W on 2026-07-16) remain on the board as **DNP footprints** — populating both and removing U304 restores the Schottky OR as a hand-solderable rescue. Discrete cross-coupled P-FETs were considered and rejected: with both supplies present both FETs turn off and conduction falls to the body diodes, sagging the rail to ~4.3V — worse than the Schottky. Cap rules: AMS1117-5.0 output (C308) = 22µF 16V tantalum (needs ESR — ceramic-only can oscillate); AMS1117 input (C306) and both AP7361C caps (C303/C307) = 10µF 0805 MLCC (AP7361C is ceramic-stable per datasheet). The 5V rail powers only the SN74AHCT1G125 level shifter (single-digit mA — this rail was previously missing from the design; the prototype borrowed a separate USB 5V supply, see docs/archive/PROTOTYPE_WIRING.md, but the final panel is self-contained on 12V bus power). The 3.3V rail powers RP2040, RS-485 transceiver, and remaining logic (~30–50mA total).
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
| RS-485 | Micro-Fit 3.0 | 3-pin | 1 shielded twisted pair **22 AWG** (RVSP) — **same reel as INT since 2026-08-16** | Pins 1/2 = A/B. **Pin 3 = cable shield as of 2026-07-26**, reversing both the "unpopulated 3rd pin" and "drain floating at both ends" decisions. Hybrid grounding: pass-through on each panel with no local GND tie (net `RS485_Shield`) + 100nF‖1MΩ to GND per panel; **DC-grounded only at the master** (J1 pin 3 → GND). 3-circuit keying still makes the connector physically distinct from 2-pin power, and pin 3 carries shield only — no rail, no signal. See `docs/MASTER_PCB.md` → "RS-485 shield" |
| INT (per panel) | Screw terminal — **2-pos 5.08mm, KANGNEX WJ500V-5.08-2P (LCSC C8465)**, **J214** on the carrier (was J9 on the retired single board), footprint `dual-panel:TerminalBlock_WJ500V-5.08-2P` (vendor land pattern, 1.30mm holes). Swapped 1P→2P 2026-07-20 (true 1P barely exists); part settled 2026-07-26 | **2 conductors (shielded twisted pair)** | **22 AWG** | Home-run to master. **Pin 1 = INT signal, pin 2 = dedicated GND** (2026-07-24). **Both positions are now used (2026-07-26):** the twisted-pair return was adopted up front rather than held in reserve, so the return no longer rides the shared power ground network. The master GND tie (J2 pin 2) is still mandatory for RS-485 |
| INT (master) | **9× JST XH 2-pin vertical, B2B-XH-A (LCSC C158012)** — J3–J11; mating XHP-2 housing (C144401) + SXH-001T-P0.6N contacts (C385122). Symbol is generic `Connector_Generic:Conn_01x02` — KiCad ships no JST-specific symbol | 2 conductors each | **22 AWG** shielded twisted pair | **Replaced the 9-pos pluggable Euroblock 2026-07-26.** One connector per panel, pin 1 = INT, pin 2 = dedicated GND. Nine discrete 5.08mm screw blocks were rejected (~92mm of edge on a 77.5mm board); XH is top-entry so it needs no board edge at all. Keyed, so signal/GND cannot be swapped — that swap on a screw terminal reads as a permanently stuck press. XH-on-master vs PH-on-panel-FSR also prevents cross-plugging. Trade-off accepted: nine plugs instead of one detachable block. Silkscreened by panel position (`UL`/`U`/`UR`/`L`/`C`/`R`/`DL`/`D`/`DR`, left to right = panel 0→8). **Per-panel identification is colored/printed heat-shrink at both cable ends**, not conductor color — the chosen RVSP cable comes in one color; the stock SMX map (0=Red … 8=Black, see docs/BOM.md) now names markers. Slot↔panel-ID agreement is **not** assumed — the master learns the real mapping with the `'I'` identify pulse and reports mismatches; spec in docs/RS485_PROTOCOL.md |
| FSR (internal) | JST-PH | 2-pin | thin | Top-entry PCB-mount (B2B-PH-K, LCSC **C131337**, genuine JST), internal to panel. Corrected from JST-XH 2026-07-10: existing FSR leads use **PHR-2** plugs — **confirmed against a real lead, closed**. Carrier refdes J201 (W), J202 (S), J203 (E), J206 (N) |

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

### Concurrent Telemetry (the real motivation)

Stock SMX has **9 dedicated signal wires**, one per panel, and they are
**dual-mode** (`../stepmaniax-sdk-mp/docs/INTERNAL_BUS_PROTOCOL.md` → "Signal
Lines"):

1. **Press detection** — line idles HIGH (5V), pulled LOW on press. This is the
   gameplay input path on stock hardware.
2. **Sensor test data** — after a `'B?P'` command the *same wires* switch to
   carrying 80-bit frames (~25kHz clock, 3.2ms/frame), all 9 panels in parallel.

The two modes are mutually exclusive on the same physical wires, and `'B?P'` is
a **service-menu item only** — it does not run during SMX gameplay.

The consequence is a missing capability, not a latency penalty: on stock
hardware you **cannot read per-sensor FSR values while playing**, because doing
so commandeers the wires that carry press state. Open-source software (DeadSync
in particular) wants exactly that — live sensor telemetry concurrent with play,
for calibration, visualization and analysis.

Our design separates the paths so both run at once: the **INT wires carry press
state** (sole gameplay input path) and **RS-485 carries FSR telemetry**. Neither
blocks the other. That is the improvement.

**Do not describe this as a "6ms lag window."** An earlier version of this
section claimed the `'B?P'` readback interleaved with the LED cycle during play
and caused ~6ms of missed presses. That path never runs during gameplay, so the
claim was wrong. Stock press latency, if ever re-derived, would live in the
master's panel debounce (`panelDebounceMicroseconds`, default 4000 = 4ms), the
master's signal-line sampling, and the USB report cadence — not the LED cycle.
No one has measured it.

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

**Master MCU enclosure:** Currently houses an Arduino Micro in a small enclosure. Teensy 4.0 (35.6×17.8mm) is smaller than the Arduino Micro (48×18mm), but the master board has several connectors: 1× RS-485 out (Micro-Fit 3-pin), 9× INT (JST XH 2-pin vertical, ~42 × 13.5mm total — replaced the pluggable Euroblock 2026-07-26), 1× combined GND-tie + underglow-DATA screw terminal (**J2, a single 2-position KF301-style block as drawn 2026-07-20** — pin 1 = underglow DATA, pin 2 = the mandatory GND lead to the PSU ground stud; these were two separate connectors until the merge), 1× USB. No 12V distribution — power runs PSU → columns directly (stock 12V→5V DC/DC converter is removed; master logic is USB-powered). Master PCB target ~80×60mm; enclosure to be sized once PCB layout is known.
