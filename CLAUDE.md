# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

**Current state, what works, and next steps: `docs/STATUS.md`.** Read it first.
It also maps every doc to its job.

**Keep `docs/STATUS.md` current.** A session can end at any point, so don't save
the update for the end:
- Update it **alongside** any change that moves it: something built or newly
  working on the bench, a board assembled, an open item found or closed, a next
  step done or added. That includes bench results the user reports in chat.
  Bump **Last updated**.
- Keep it a summary. Detail goes in the doc it links (bench results in
  `docs/BRINGUP_LOG.md`, specs in their own docs).
- At the start of a session, list the commits since STATUS last changed:
  `git log --oneline $(git log -1 --format=%h -- docs/STATUS.md)..HEAD`. If any
  of them aren't reflected in it, bring it up to date first.
- Where it disagrees with a detailed doc or the code, they win; fix STATUS.

> # 🔒 Rev 1 is the hardware in use, and it is frozen
>
> Both boards were ordered from JLCPCB on **2026-08-18** and are on the bench
> now. They can't be changed.
>
> | | |
> |---|---|
> | Git tag | **`rev1-fab`** |
> | Exact files that were fabricated | **`hardware/fab-archive/rev1-2026-08-18/`** (with SHA256SUMS). Diff a misbehaving board against these, not a fresh export |
> | How rev 1 was verified, ordered and bought | `docs/rev1-order/` |
> | How to order the next revision | `docs/FAB_ORDER.md` |
>
> - **Any board change is rev 2.** Say so explicitly, and never describe a change
>   as if it affects the boards in hand.
> - **Firmware, docs, harness and tooling are not frozen**, only the two PCBs.
> - When debugging a physical board, check the four order-day changes first:
>   they are the youngest edits in the design (listed in the fab archive's
>   `README.md`).
>
> **Rev-2 candidates** (full entries in `docs/REV2_CANDIDATES.md`; none decided
> unless it says so):
> 1. External TVS on the RS-485 A/B pair (decided 2026-08-18)
> 2. Master: ground-pour clearance above 0.09 mm
> 3. Brain: VBUS sense on a spare GPIO. A powered brain back-drives VBUS to 2.78 V,
>    so **connect USB before 12V on rev 1**. A plain bleeder is the wrong fix
> 4. Brain: an on-board indicator LED (power and/or debug mirror)
> 5. Test-point labels on both sides, by signal name taken from the net
> 6. More test points (VBUS first)
> 7. Probe hooks in place of THT pad test points
> 8. Brain: `J305` USB-C pins barely protrude
> 9. Both boards: SMD wherever possible, wash-safe parts
> 10. Carrier: automatic termination via chain discovery (parked)
>
> Dropped: the RS-485 DE pull-down (measured unnecessary, 2026-09-07). Decided
> for rev 1 with revisit notes: no board-level power protection.

## Project Overview

Custom replacement hardware for a StepManiaX (SMX) 9-panel dance pad. Replaces
the Master Control Unit (MCU) and all 9 Panel PCBs while keeping the existing
frame. Targets open-source rhythm game software (Stepmania, ITGmania, DeadSync),
and is intentionally **NOT SMX-compatible** (no proprietary auth key, no
leaderboard eligibility). Working name "KrakenPad". The repo is **public**.

Improvements over stock SMX hardware:
- Live per-sensor FSR telemetry *during gameplay*, which stock hardware cannot do
  at all (see "Concurrent telemetry" below)
- USB polling at up to 8 kHz (USB High Speed; stock is 1 kHz)
- LED refresh 60 Hz+ (stock 30 Hz)
- A custom open protocol and fully open-source firmware

## Repository Layout

```
firmware/
  shared/            # panel_config.h: the config format both firmwares include (+ test/)
  master/
    master.ino       # Teensy 4.0: RS-485 bus master, INT capture, USB joystick + vendor interface, config store, bench console
    build.sh         # build/upload via a patched Teensy core (core-patch/)
    usb_speed_test/  # USB HS bench-test sketch
  panel/c/
    main.c           # panel gameplay firmware (Pico SDK), runs behind the bootloader
    bringup/         # bring-up firmware (docs/PANEL_BRINGUP.md)
    bootloader/      # bus-update bootloader + host_test/ (docs/FIELD_UPDATE.md)
    common/          # flash layout + app linker script shared by all three
    led_current_test/ # WS2815 current bench firmware
hardware/
  master-pcb/        # Master board (rev 1, docs/MASTER_PCB.md)
  dual-panel/        # THE panel: carrier + brain, one project, one .kicad_pcb (rev 1, docs/DUAL_PANEL.md)
    panel/           #   panelisation + JLC fab package generators
    fit-test/        #   1:1 printable cavity fit template
  harness/           # our cable harnesses (WireViz) — as built
  fab-archive/       # exact files sent to fab, per revision
  Master-Case/       # master enclosure 3D-print models
software/            # Cargo workspace: sdk/ = crate krakenpad, the Rust SDK (docs/HOST_SOFTWARE.md)
docs/                # status, as-built references, protocols, procedures
  rev1-order/        # how rev 1 was verified, ordered and bought (frozen record)
  archive/           # superseded material, banner-marked, kept for the rationale
stock-smx/           # the stock SMX pad as found: harness, parts, panel PCB (descriptive only)
tools/               # BOM/census, fab, review images, bus update, SWD, monitors, USB vendor commands (pad_cmd.py)
```

**Both firmware trees use the as-built pin maps.** The RP2040 GPIO map is in
`docs/DUAL_PANEL.md`; the Teensy map is in `docs/MASTER_PCB.md`. Breadboard-era
firmware (including the MicroPython prototype) is in git history.

**The single-board panel (`hardware/panel-pcb`) was retired 2026-08-04** when the
two-board split won on measured FSR isolation at near-neutral cost. It is in git
history, last present at `1b41d1c`; the evidence is
`docs/archive/DESIGN_COMPARISON.md`. Panel reference designators are
**`dual-panel`'s** (carrier **2xx**, brain **3xx**). Anything from before
2026-08-04 uses `panel-pcb`'s numbering; `docs/archive/REFDES_TRANSLATION.md`
maps the two. The master was separately re-annotated on 2026-07-31; the same file
lists its key swaps.

**`stock-smx/` and `hardware/harness/` must stay separate.** `stock-smx/` is
descriptive and useful on its own to anyone repairing a factory pad; when a stock
finding bears on our design, the note goes in *our* file, never the stock one.

PCB design tool: **KiCad 10** (tool traps: `docs/DEV_NOTES.md`).

## Architecture

### System topology

```
PC (USB HID) ←→ Master MCU (Teensy 4.0)
                      │
                      ├─ RS-485 bus (daisy-chained through all 9 panels)
                      │    └─ LED frames + FSR telemetry round-robin polling
                      │
                      ├─ Interrupt wire ×9 (one per panel, home-run to master)
                      │    └─ Open-drain, pulled HIGH at master; panel pulls LOW on press
                      │
                      └─ (12V does not pass through the master)
```

### Master: Teensy 4.0 (600 MHz Cortex-M7), socketed on the master PCB

- USB HID to the PC: a 9-button joystick report (pad input), plus a vendor
  interface for config, telemetry, LED frames and bus updates, **built
  2026-10-20 and not yet run on hardware** (`docs/USB_PROTOCOL.md`;
  `tools/pad_cmd.py` speaks it). Teensyduino can't take a custom descriptor, so
  `firmware/master/build.sh` builds against a patched copy of the core; it also
  adds `firmware/shared/` to the include path, so a plain `arduino-cli compile`
  fails.
- Holds the **only saved copy of every panel's config** (emulated EEPROM, two
  slots) and pushes it to the panels with `'W'`; panels keep config in RAM
  (`docs/PANEL_CONFIG.md`).
- 9 hardware interrupt inputs, one per panel; presses go straight from the INT
  ISR to the USB report.
- Drives the RS-485 bus on **Serial2** (RX 7 / TX 8, DE 6): LED frames to each
  panel, paced round-robin FSR polling.
- **3-bit player-ID DIP** (`SW1`, 0–7; 0–3 = P1–P4, 4–7 reserved). Closed = 0, so
  player 0 is all three switches ON.
- USB-powered; no 12V on the board. Its GND **must** be tied to the pad ground
  network: a dedicated lead from `J2` pin 2 to its own GND port on the Wago
  fan-out. Separate grounds was a real bench failure.

### Panel: bare RP2040 (QFN-56) on a two-board panel

A 127×127 mm **carrier** (25 LEDs, every external connector, switches) and a
70.9×62.8 mm **brain** (RP2040, flash, transceiver, regulators, USB-C) socketed
underneath in the frame cavity. Part choices and their history (USB-C, RS-485
transceiver, flash, level shifter, LDO cascade, power-OR) are in
`docs/DUAL_PANEL.md` → "Part choices and their history".

- **Core 0:** tight FSR sampling loop (hysteresis + persistence) driving the
  open-drain INT line. Nothing else runs there, so the press path never waits.
- **Core 1:** RS-485 responder, LED rendering (PIO fed by DMA), 12V sense, USB
  console.
- 4× FSR on ADC0–ADC3 (GPIO26–29).
- 25× **WS2815** (12V native), same layout as stock, serpentine wired.
- 4-position DIP for panel ID 0–8 (closed = 0, so ID 0 is all four ON; an
  untouched all-OFF switch reads 15). Other codes select diagnostic modes; only
  **10** (standalone sensor test) is built (`docs/PANEL_CONFIG.md`).
- RS-485: **THVD1450DR**, both boards. **SN74AHCT1G125** shifts LED data to 5V
  (WS2815 DIN takes a 5V logic signal; only its VDD is 12V).
- **W25Q32JV** 4 MB flash: bootloader, app, animation slots, update staging
  (`docs/FIELD_UPDATE.md` → "Flash map").
- Logic power: AMS1117-5.0 → AP7361C-33ER cascade from 12V, with an LM66200
  ideal-diode mux between that 5V and USB VBUS.
- **Bring-up traps** that apply to any new firmware: RP2040 pads reset with the
  pull-down on and `gpio_init()` doesn't touch pulls (`gpio_disable_pulls()` on
  `SENSE_12V` and `INT_OUT` is mandatory); and the app image needs the bootloader
  loaded first (`docs/PANEL_BRINGUP.md`).

### Sensors

- **FSR only** (no load cell). **Interlink FSR 408** strips, reused from the stock
  pad. 4 per panel.
- Divider: FSR to 3.3V, 10 kΩ pull-down to GND, junction to the ADC. The
  pull-down is always on the board, so an unplugged FSR reads near 0, the same
  as an unloaded one (presence must therefore be config, not measurement).
- Rev-1 ADC values (12-bit): resting **85–108** across nine boards (most
  94–102), full press **~4000** by finger on the bench. **In the pad under full
  body weight: 4078–4095, full scale; resting 88–114** (2026-10-22,
  `docs/BRINGUP_LOG.md`). Default thresholds press 500 / release 400
  (`firmware/shared/panel_config.h`), set per channel by the master's `'W'`.
- Sampling: the C firmware's loop runs at ~100 kHz (102,080 Hz measured on the
  breadboard). Targets were 4 kHz hard, 8 kHz soft. MicroPython managed
  ~1–2 kHz, which is why the firmware is C.

### Power

- 12V, three parallel column chains, matching stock wiring:
  left 0 (UL) → 3 (L) → 6 (DL), centre 1 (U) → 4 (C) → 7 (D), right 2 (UR) → 5 (R) → 8 (DR).
- PSU captive cable → XT30 → inline **T8A** fuse → **Wago 221-415 fan-out**
  (where the stock Daygreen converter was) → each column's first panel. The
  master is not in the 12V path. Harness: `hardware/harness/12v-trunk.yml`.
- **Budget: ~6.34 A / 76 W** at full white everywhere, against the stock **8.5 A**
  supply, which is kept (DECIDED 2026-08-16). Per panel 0.44 A, per column
  ~1.3 A. The WS2815 figure is **confirmed on hardware to ~1%**.
- ⚠ **WS2815 draws ~15 mA per *pixel*, and red draws the same as white:** its
  three dies are in series and unlit ones are shorted out, so only PWM duty
  changes current. **The underglow's WS2811 behaves the opposite way** (red is ⅓
  of white). Model the two separately. Full treatment:
  `hardware/harness/README.md` → "Power budget" and `docs/UNDERGLOW.md` →
  "Current draw".
- **There is no current sensing anywhere in the pad.** The trunk fuse is the only
  current protection.
- Underglow: stock WS2811 strips, 44 groups of 3. 12V/GND from the fan-out, DATA
  from master `J2` pin 1. **SM 3P pinout: pin 1 = GND, pin 2 = DATA, pin 3 = 12V**
  (reversed, 12V kills the first LED). Byte order **BRG**. `docs/UNDERGLOW.md`.

### Connectors

All inter-panel connectors are **Molex Micro-Fit 3.0**, right-angle, positive
latch. Different pin counts prevent cross-connection.

| Signal | Connector | Notes |
|--------|-----------|-------|
| Power | Micro-Fit 2-pin, `J205` IN / `J208` OUT | 2×20 AWG jacketed (measures 21 AWG) |
| RS-485 | Micro-Fit 3-pin, `J204` IN / `J207` OUT, master `J1` | Shielded twisted pair (RVSP, measures 24 AWG). Pins 1/2 = A/B, **pin 3 = shield**: passed through each panel with 100 nF ‖ 1 MΩ to GND, DC-grounded only at the master |
| INT (panel) | `J214` 2-pos 5.08 mm screw terminal | Pin 1 = INT, pin 2 = dedicated GND. Same RVSP cable, shield trimmed at both ends and never bonded to the INT GND |
| INT (master) | 9× JST XH 2-pin, `J3`–`J11` | Keyed, so INT/GND can't swap. Silkscreened `UL` … `DR`; each cable carries a printed position label at both ends |
| FSR | JST PH 2-pin, `J201` W / `J202` S / `J203` E / `J206` N | Mates the stock FSR's PHR-2 plug |

Harness details, measured cable, and assembly rules: `hardware/harness/`.

**All 9 panels are identical.** Identity is the DIP switch; termination is the
`SW202` DPDT switch on whichever panel is physically last (120 Ω at the master
end is fixed). A panel at the end of a chain leaves its OUT connectors empty.
The master learns the real slot ↔ ID mapping with the `'I'` self-test rather
than assuming it.

### Interrupt wire

- Open-drain: the RP2040 sinks to GND on press; pulled HIGH (3.3V) at the master
  through `RN1` (10 kΩ ×9).
- **Dedicated GND return** in the same twisted pair (since 2026-07-26), not the
  power ground.
- Protection at both ends: master SMAJ5.0A + 330 Ω + 1 nF per line; panel `D201`.
- A disconnected wire reads HIGH (not pressed), a safe failure.
- **The INT lines are the sole gameplay press path.** RS-485 FSR data is telemetry
  only and never gates a press (`docs/USB_PROTOCOL.md` → latency budget).

### LED layout

- 25 LEDs per panel in the stock staggered 4–3–4–3–4–3–4 lattice, wired
  **serpentine** (rows alternate direction). The row of 3 is rotated 180° from
  the row of 4 on purpose. Index order: `docs/ANIMATIONS.md`.
- 60 Hz+ refresh from the master's `'L'` frames. Panels go dark 100 ms after
  frames stop; local flash animations (APNG/WebP source → `.smxa`, released and
  pressed slots) are designed but not built (`docs/ANIMATIONS.md`).

### Daisy-chain order

Matches the stock serpentine routing: master → 0 (UL) → 3 (L) → 6 (DL) → 7 (D)
→ 4 (C) → 1 (U) → 2 (UR) → 5 (R) → 8 (DR).

## Key Engineering Notes

### USB polling rate

Confirmed 2026-07-08 (`firmware/master/usb_speed_test/`): Teensy 4.0 negotiates
USB High Speed on stock Teensyduino, >100 Mbit/s bulk. At High Speed an interrupt
endpoint's `bInterval` sets the period as `2^(bInterval-1)` × 125 µs microframes,
so `bInterval = 1` is 8 kHz. The joystick report ships at `bInterval = 1`. The
open question is host-side polling consistency, which hasn't been measured. A
device-side timestamp idea that would make it matter less:
`docs/TIMESTAMPED_INPUT.md`.

### Concurrent telemetry (the real motivation)

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

### RS-485 at 1 Mbps

A full 225-LED broadcast is ~7.5 ms and a 9-panel FSR sweep ~1.5 ms, so ~9 ms per
cycle and a ~110 Hz ceiling; 60 Hz LEDs fit comfortably. Runs are under 3 m, so no
tight impedance control is needed. Stock's 30 Hz was a 250 kbaud bandwidth
ceiling, not an LED limit. Two bus rules learned the hard way: **pace polls, never
burst them**, and **every reply gets a guaranteed silent window**
(`docs/RS485_PROTOCOL.md`). As measured, telemetry is ~14 Hz per panel at nine
panels.

### Firmware build

- Panel: C / Pico SDK, CMake, Arm GNU Toolchain **cask** (the Homebrew formula
  lacks newlib), `PICO_SDK_PATH=~/pico-sdk`. WS2815s are driven by PIO.
  Commands: `docs/PANEL_BRINGUP.md`. Updates over the bus:
  `tools/panel_update.py` (`docs/FIELD_UPDATE.md`).
- Master: `firmware/master/build.sh` (Teensyduino 1.62.0 only, because of the
  core patch). Commands: `docs/MASTER_BRINGUP.md`.

## Reference: Original SMX Protocol

See `../stepmaniax-sdk-mp` (branch `fc/data-bus-deep-dive`) →
`docs/INTERNAL_BUS_PROTOCOL.md` for the reverse-engineered stock protocol: UART
250 kbaud 8N1 with BREAK-terminated commands, LED commands `'4'`/`'2'`/`'3'` at
30 Hz, sensor readback via `'B?P'` on the signal wires, the boot sequence
(`'R'` → `'w'` → `'G'` → lighting), and the serpentine chain order.

## Physical Dimensions

- **Panel carrier: 127×127 mm** core (139.3 mm across the connector tabs), matching
  the stock cavity and standoffs. **X is fixed** by the left/right edge
  connectors; **Y has ~20 mm of slack per end** if a future revision needs it.
  Height budget above the PCB ~35 mm; mechanical stack below it in
  `docs/DUAL_PANEL.md`.
- **Master board: 77.6 × 65.6 mm.** Enclosure models in `hardware/Master-Case/`.
