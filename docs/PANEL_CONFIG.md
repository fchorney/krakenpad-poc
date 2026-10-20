# Panel configuration — design

Status: **mostly design, partly built.** This collects the config decisions made
so far. There is no binary format yet and **nothing persists**. Since 2026-10-20
the plan is that panels never persist config at all; the master does.

**Built in the gameplay firmware (`firmware/panel/c/main.c`), as of 2026-10-19:**

- Panel ID from the DIP at boot (0–8). `15` (all OFF) and other codes are not
  addressed on the bus.
- **DIP 10:** the sensor test runs standalone. The other diagnostic codes in the
  table below (9, 11, 12, 13) are not built.
- The three **LED modes** (normal, ID, sensor test), set by `'M'`. See below.
- Press/release thresholds per channel (`g_press_th` / `g_rel_th`), defaults
  500/400, set over the bus with `'C'`. **RAM only**: a reboot restores the
  defaults.
- The debug LED `D202` lights on press in DIP 10 only, and stays dark otherwise.

**Not built:** flash-backed config and its write/commit protocol, the
`populated`/`enabled` flags, calibration and drift tracking, the periodic
re-push from the master, local animations (normal mode goes dark when `'L'`
frames stop), and the other DIP diagnostic modes.

The binary layout will slot into the flash config region alongside the animation
slots. The current flash map is in `docs/FIELD_UPDATE.md` → "Flash map".

## What lives where

- **Panel ID (0–8):** hardware DIP switch, NOT flash config. A panel must know its
  identity before it can be addressed over the bus, so identity can't itself be a
  bus-configured value. Everything else below lives in flash and is configurable
  over RS-485 from the master.
- **Per-channel FSR config:** flags + calibration, 4 channels per panel (see below).
- **RS-485 fallback timeout:** **100ms** without an LED frame from the master.
  As built the panel goes dark; the plan is to fall back to a local
  (flash-stored) animation. Probably configurable, 100ms default.

## Panel DIP Switch: Diagnostic Modes (decided 2026-07-09)

A 4-position DIP switch gives 16 possible values (0–15). Panel IDs only need 0–8,
leaving 7 spare codes. The official SMX Gen4+ manual (page 5, "Wiring and DIP
Settings") reserves its top 3 values for diagnostic modes — worth adopting the same
pattern, since all of them are **panel-local, requiring no master/bus connection at
all**, which fits our architecture well: the panel firmware already has to run
standalone-capable logic anyway (100ms fallback to local animation when no RS-485
frame arrives), so these modes are mostly branches on that same existing code path
at boot, not new subsystems.

Adopting 3 of stock's ideas plus 2 new ones our extra headroom allows for:

| DIP value | Mode | Behavior |
|---|---|---|
| 0–8 | Panel ID | Normal operation. **Built** |
| 9 | **LED Check** | All 25 LEDs lit dim white — visual "are they all alive" check, no bus needed. *(from stock, value 13)* |
| 10 | **Sensor Pressure Test** | Each of the 4 FSR edges lights a distinct color when pressed — visual per-sensor wiring/function check, no bus needed. *(from stock, value 14)* **Built** (as LED mode 2, below) |
| 11 | **Standalone Mode** | Panel lights on press, entirely independent of the master — isolates a single panel for bench testing with just 12V power, no RS-485/master present at all. *(from stock, value 15)* |
| 12 | **Raw ADC Streaming Mode** (new) | Continuously dumps raw per-channel FSR ADC values (e.g. over UART/serial) regardless of whether a master is present — useful for bench calibration work without wiring up the full rig. |
| 13 | ~~**Factory Reset Trigger** (new)~~ **Dropped 2026-10-20** ("Factory reset", below). | ~~Forces the flash animation/config magic-byte wipe-and-rewrite-defaults path on boot — a recovery mechanism that doesn't depend on any working host software or bus command, useful if config/animation flash ever gets corrupted and normal boot can't get far enough to accept a reset command.~~ Reserved again |
| 14–15 | Reserved | Unused for now, headroom for future diagnostic modes |

These are all **panel-local**. The one diagnostic that is inherently
master-driven — the **slot ↔ panel-ID self-test**, which proves each panel's INT
cable lands in the JST XH header the master thinks it does — lives in the bus
protocol instead, as the `'I'` identify command (`docs/RS485_PROTOCOL.md`). It
needs no DIP code: the master addresses a panel normally and the panel pulses its
INT line so the master can see which input it arrives on.

**How to apply:** panel firmware reads the DIP value once at boot (same as it already
does for panel ID); values 9–13 branch into a diagnostic routine instead of normal
main-loop operation (mirrors stock's documented behavior: "panels will not function
normally when in diagnostic modes"). No hardware changes needed — same DIP switch,
same GPIO read, just more branches in the boot-time dispatch.

**Debug press LED (GPIO16, R204/D202 on the carrier) is diagnostic-mode-gated
(decided 2026-07-11):** in normal operation (DIP 0–8) the LED stays dark; in
diagnostic modes (9–13) it's active — lit on press for the sensor-facing modes
(10/11/12), and free for mode-specific signaling elsewhere (e.g. a heartbeat blink
in LED Check, an acknowledge blink after Factory Reset completes). Rationale: the
LED exists for bench work — it runs off the 3.3V logic rail, so it works on
USB-only power with the 12V grid absent (where the WS2815s are dark) — and there's
no reason for a stray indicator glowing through the panel assembly during play. No
new DIP code needed: a dedicated "normal play + debug LED" mode would consume the
panel's ID slot (modes 9+ don't carry an ID), so gating it to the existing
diagnostic modes is the coherent option. If mid-rig debugging ever wants it during
normal operation, that's a flash-config flag / bus command, not a DIP mode.

## LED modes (gameplay firmware, decided 2026-10-18)

Three modes, all rendered **on the panel**. The panel samples its FSRs at
~100 kHz, while bus telemetry reaches the master at only ~14 Hz per panel, so a
pressure display drawn by the master would visibly lag. Master command:
`'M'` (`docs/RS485_PROTOCOL.md`); master console: `m <mode> [secs]`.

| Mode | What the LEDs show |
|---|---|
| 0 normal | The master's `'L'` frames. Dark if none for 100 ms (local animations come later) |
| 1 ID | White image of the panel's position, read from its DIP. **X if the DIP is not 0–8.** Shown for **3 s when 12V first arrives**, and whenever the master asks |
| 2 sensor test | Each edge lights its own colour, brightness tracking pressure. DIP 10 runs it standalone (no bus, no INT); `'M'` runs it on bus panels |

Pixel sets use **wire (serpentine) order, 1-based** — the numbering as stated
when this was decided. Firmware stores them 0-based.

| Panel | Pixels | | Panel | Pixels | | Panel | Pixels |
|---|---|---|---|---|---|---|---|
| UL (0) | 1 2 7 8 | | U (1) | 2 3 6 | | UR (2) | 3 4 5 11 |
| L (3) | 8 14 15 | | C (4) | 9 10 13 16 17 | | R (5) | 11 12 18 |
| DL (6) | 15 21 22 23 | | D (7) | 20 23 24 | | DR (8) | 18 19 24 25 |
| X | 1 4 5 7 9 10 13 16 17 19 21 22 25 | | | | | | |

UL, U, UR and C were given; the other five follow by symmetry. **Seen on all
eight panels 2026-10-19 and accepted by the user** (stage 3 bench, panels 0–7).
They have not been compared against stock.

**Sensor test:** each edge uses the ID image of the panel on that side:
**North = U, blue; East = R, yellow; South = D, green; West = L, red.** No two
sets overlap, so nothing needs blending. Brightness is off at or below
**100 counts** and linear from there to full at 4095. That floor is **NOT the
press threshold**, on purpose, because the user can change the press threshold.
Board 1 rests at 94–98, so 100 sits right at the noise floor and may need
raising. Once tare exists, this becomes "0–5 counts above tare is off".

This reverses `docs/USB_PROTOCOL.md`'s 2026-07 call to drop stock's `'t'` panel
test mode from firmware and draw it host-side.

## Per-channel FSR flags

Two independent bits per channel — they answer different questions:

### `populated` — a hardware fact
"There is physically a sensor on this connector."

- `populated = 0`: skip in calibration (don't ask the user to press a sensor that
  doesn't exist), report as N/A in diagnostics rather than "silent", and **flag an
  anomaly if the channel ever shows activity** — a not-populated channel reading
  pressure means the config is wrong or something is miswired. The flag turns the
  channel's expected behavior into something checkable.

### `enabled` — a policy choice
"There's a sensor here, but ignore it for press detection."

- Escape hatch for a flaky/chattering sensor mid-session: disable it and the panel
  keeps working on its other edges.
- A disabled channel **keeps streaming telemetry** so the misbehaving sensor can be
  observed from the host while excluded from gameplay.

### How the firmware combines them
- Press detection (INT line + pressed-animation trigger): `populated && enabled`
- Calibration: `populated`
- Telemetry: stream all 4 raw values unconditionally (it's cheap); flags let the
  host render channels as active / disabled / not-populated.

### Why presence must be config, not measurement
An FSR (Interlink 408) at zero force is essentially an open circuit (>1MΩ). With the
PCB pull-down, "unplugged" and "plugged in but untouched" both read ~0 — they are
**electrically indistinguishable**. No static measurement or pull-up trick can tell
them apart. So:

- Absence is inherently *safe* in this topology (reads ~0, never crosses threshold) —
  unlike stock SMX, which reportedly misbehaves with unpopulated sensor connectors
  (likely a divider topology where a missing sensor floats the input and confuses
  their auto-calibration).
- Legitimate presence knowledge comes from **calibration**: a channel that responds
  when pressed is demonstrably present. Calibration can *suggest* `populated = 0`
  for a channel that never responds, but a human confirms — so a dead sensor doesn't
  get silently misclassified as absent.
- A physical "FSR present" DIP switch was considered and rejected: it's the same
  one bit of config, but it can go stale, can't be set remotely, and costs board
  space ×9 panels.

## Per-channel calibration values

- Press threshold (12-bit ADC counts). On the rev-1 boards: resting 85–108 across
  nine boards (most 94–102), full press ~4000; the 500 default sits comfortably
  between them. (Breadboard figures were ~100–115 / ~3900.)
- Resting baseline (captured at calibration time) — for drift diagnostics.
- Hysteresis (separate press/release thresholds) — implemented (`g_press_th`/
  `g_rel_th` in main.c, bus-settable via `'C'`/`'c'`, see RS485_PROTOCOL.md).
  RAM only for now; flash persistence is the remaining open item.

## Boot-time float detection (firmware behavior, not config)

> **DROPPED 2026-10-18 from the gameplay firmware.** The breadboard firmware
> *disabled* any channel resting above the threshold at boot, so a foot on a
> sensor at power-up would disable that sensor until the next reboot. On the
> as-built carrier the pull-down is always populated, so a floating input can
> only come from an assembly fault, and that is bring-up's job (`s` warns about
> it). Original reasoning kept below.

The prototype firmware averages 50 samples per channel at boot; a resting average
*above* ~187 counts means the pin is floating (no pull-down attached at all — the
genuinely dangerous case that would chatter across the threshold). This is distinct
from presence detection and stays regardless of config. Note: on the Pico dev board,
GPIO29's internal VSYS divider makes channel 3 always read as floating — a dev-board
quirk only; GPIO29 is a clean ADC pin on the custom PCB.

## Field-by-field walkthrough of stock SMXConfig (keep / adapt / drop)

Stock's 250-byte `SMXConfig` blob (`../stepmaniax-sdk-mp/docs/USB_PROTOCOL.md`) is a
useful checklist — it's a real device's answer to "what does a dance pad need to
remember." Walking it field-by-field:

| Stock field | Verdict | Notes |
|---|---|---|
| `masterVersion` | keep | firmware version, read-only, exposed via device info (see USB_PROTOCOL.md) rather than buried in the config blob |
| `configVersion` | keep | needed the moment the format changes once; see open question below |
| `flags` (`AutoLightingUsePressedAnimations`, `FSR`) | drop both | pressed-vs-solid is inherent to our animation model (every panel always has a pressed slot, see CLAUDE.md LED Layout); `FSR` flag existed to select load-cell vs FSR — we only support FSR, so it's dead weight |
| `debounceNodelayMilliseconds` / `debounceDelayMilliseconds` | drop | stock's debounce is presumably filtering panel-level mechanical bounce or bus-timing artifacts; our chatter problem was solved with hysteresis + persistence at the ADC (see main.c), a different and more specific mechanism. Revisit only if a new bounce mode shows up that this doesn't cover |
| `panelDebounceMicroseconds` | superseded | replaced by our persistence-filter sample count (`FSR_PERSISTENCE`, currently 10 samples ≈ 0.1ms) — same idea, different unit, already implemented |
| `autoCalibrationMaxDeviation`, `autoCalibrationAveragesPerUpdate`, `autoCalibrationSamplesPerAverage`, `autoCalibrationMaxTare` | adapt | real auto-calibration parameters worth having once auto-calibration is designed (continuous drift tracking, not just one-time calibration) — park them here as a placeholder, don't implement yet |
| `badSensorMinimumDelaySeconds` | adapt | stock's "how long before flagging a sensor as bad" — maps onto our `populated`/`enabled` flag idea above; a channel that free-runs at an implausible value for this long could auto-suggest `enabled = 0` the same way calibration suggests `populated` |
| `enabledSensors[5]` | superseded | this is stock's per-sensor enable bitmask across the whole pad (5 bytes ≈ load cell + 4 FSR × 9 panels bit-packed) — we already have a cleaner per-channel `populated`/`enabled` pair per panel instead of one global bitmask |
| `autoLightsTimeout` | superseded | our fallback timeout is fixed at 100ms and panel-local (no "auto lights" mode to time out into — the panel *always* has a local animation). Stock needed this because its auto-lights was a distinct mode from host-driven lights with a multi-second grace period; we collapsed that into one fallback behavior |
| `stepColor[3×9]` | drop | stock's non-animated fallback (solid per-panel step color). We use full flash animations instead (released/pressed slots, see CLAUDE.md) — strictly more capable, no reason to keep a solid-color fallback path alongside it |
| `platformStripColor[3]` | keep | default/idle underglow color — see `docs/UNDERGLOW.md` |
| `autoLightPanelMask` | drop | which panels participate in auto-lighting — moot since every panel always runs its own local animation regardless of what other panels do |
| `panelRotation` | drop | stock marks this unused too |
| `panelSettings[9]` (16 bytes each: load cell thresholds, 4× FSR low/high thresholds, combined thresholds) | adapt | this is the real prize field — per-panel-per-channel press/release thresholds, exactly our `g_press_thr`/`g_rel_thr` (already implemented and bus-settable via `'C'`, see RS485_PROTOCOL.md). Drop `loadCellLowThreshold`/`loadCellHighThreshold` (no load cell support) and `combinedLowThreshold`/`combinedHighThreshold` (no defined combining logic on our end yet — revisit if multi-sensor-combined press detection becomes desirable) |
| `preDetailsDelayMilliseconds` | drop | stock-internal tunable with no documented purpose we can usefully replicate |
| `padding` | drop | our format doesn't need to preserve stock's binary layout |

### New fields (no stock equivalent)

- Per-channel `populated` / `enabled` flags (see above) — stock has nothing like this because its topology can't safely leave a connector unpopulated in the first place
- RS-485 fallback timeout (currently hardcoded 100ms; make configurable if a use case shows up)
- Bus/termination diagnostics per `docs/MODULAR_PANEL_COUNT.md` (termination-switch GPIO state, expected-vs-actual last-panel-in-chain)

## Config persistence and the write protocol (decided 2026-10-20)

Supersedes the panel-flash half of the 2026-07-09 decision below; the master
stays canonical and the ~1 Hz re-push stays.

- **Panels keep config in RAM only**, starting from compiled-in defaults
  (press 500 / release 400 today). Nothing about config is written to panel
  flash.
- **The master is the only persistent store**: the canonical config for all
  nine panels in the Teensy's emulated EEPROM (below).
- **`'W'` carries one panel's whole config in one addressed packet** (per
  channel: press and release thresholds, flags, rest baseline; plus panel-wide
  fields and a format version; about 40 bytes, under the 80-byte limit), so a
  panel is never half-configured. It takes effect immediately. The panel answers
  **`'w'` with the CRC of the config it now runs**, so the master sees a mismatch
  at once. The RAM-only `'C'` threshold command becomes redundant (keep it as a
  bench tool or drop it when `'W'` exists).
- **`'f'` replies gain a "not configured since boot" bit.** The master sends that
  panel its config at its next poll slot, so a panel that reboots mid-session
  (a 12 V blip) runs on defaults for milliseconds, not until the next re-push.
- The master pushes config when it boots, when a panel reports "not
  configured", and on the ~1 Hz re-push.
- **Rule: don't save config or upload animations during gameplay.** It is on
  whoever calls the SDK (an app, a game) not to; the SDK can't tell when a song
  is playing and doesn't enforce it, but its documentation for those calls
  says so. The firmware does nothing special to protect play from them.

**Why not on the panels any more:** the 2026-07-09 reason was so a panel could
threshold presses correctly before the master reconnects or while standalone.
But without a master a press reaches nothing (presses only matter once they
are a USB report), a master reboot doesn't touch the panels' RAM, the
standalone DIP modes are bench tools that run fine on defaults, and the one
real case, a single panel rebooting mid-song, is covered by the "not
configured" bit. Dropping it removes every app-side flash write, and with them
the core-0 flash stall: the press loop runs from flash (XIP), and erasing or
programming flash stops both cores for the duration. It also removes a panel
config format to version, a corrupted-copy failure mode, and the need for two
alternating copies. Animations still live on the panel (they are its local
fallback); writing them is an animation-design question, possibly through the
bootloader, which runs from RAM.

**The master's emulated EEPROM** (Teensyduino 1.62 `cores/teensy4/eeprom.c`,
read 2026-10-20, not measured): no EEPROM chip; the `EEPROM` API is kept in the
last 15 sectors of the Teensy's program flash (60 KB at `0x601F0000`) as
1,080 bytes, written as an append-only log per sector, erased and compacted
after 2,048 entries, unchanged bytes skipped. **Every byte write disables all
interrupts and busy-waits for the flash**, and the occasional sector erase holds
them off for the whole erase. So while the master saves, its INT interrupt, USB
and the RS-485 UART stall in bursts. Covered by the no-saves-during-gameplay
rule; worth measuring once saving exists.

### The master's EEPROM record (decided 2026-10-20, not built)

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | magic `KPCF` |
| 4 | 1 | config format version, **1** |
| 5 | 2 | sequence number, u16 LE |
| 7 | 180 | panels 0–8, 20 bytes each: the per-channel part of `'W'` (bytes 1–20, no version byte) |
| 187 | 4 | CRC-32 (zlib) of bytes 0–186 |

**191 bytes, stored twice: slot A at EEPROM offset 0, slot B at 512** (of 1,080).
A save writes the slot holding the older record, with the sequence number + 1.
At boot the valid record (magic, known version, CRC) with the higher sequence
number wins, so a power cut mid-save leaves the previous config intact. Neither
valid → defaults and the "config was reset" flag. The emulation skips
unchanged bytes, so a save costs little wear.

Version 1 holds only the nine panel configs. Rest baselines, auto-calibration
values, the underglow idle colour and other pad-wide settings each arrive with
a version bump when they are built. Among them: the **underglow byte order**
(RGB / GRB / BRG…), per pad, because different pads' strips differ; the master
converts from USB's RGB with it (`docs/USB_PROTOCOL.md`).

## Config versioning (decided 2026-10-20)

- **One config format version** covers the fields everywhere they travel: the
  `'W'` packet, the master's EEPROM copy, and the USB config report. **Any change
  to the format bumps it.**
- **`'W'` carries the version.** A panel that doesn't know it rejects the packet,
  keeps the config it has, and says so in `'w'`; the master reports that panel
  as needing a firmware update. Normally all nine are updated together, so this
  is mostly a spare panel swapped in on older firmware.
- **The master's EEPROM copy starts with a magic, the version and a CRC.** At
  boot: same version and good CRC → use it; an older version this firmware
  knows → convert, save, continue; unknown or bad CRC → **defaults, plus a
  "config was reset" flag** the SDK can show ("recalibrate").
- **Every new field has a default**, so converting an older version is "copy
  what exists, default the rest" and never needs a person.
- **The SDK reads the master's version** (USB device info) and won't write a
  format the master doesn't speak.
- **Later, not designed:** export and import of a pad's config to a file through
  the SDK, as a backup and to copy calibration between pads. It may cover only
  a subset of the config; to think about when it's built.

## Factory reset (decided 2026-10-20)

- **Two separate actions**, in the SDK/config app and as master console
  commands: **reset config** (the master overwrites its EEPROM copy with
  defaults and pushes them to every panel) and **reset animations** (erase one
  panel's or every panel's animation slots, so each writes its built-in defaults
  back). Different kinds of loss, so never one button for both.
- **Panel config needs no reset:** it is RAM-only, so every boot is already
  defaults until the master pushes.
- **DIP 13 is dropped** (back to reserved). It protected against corrupted
  config or animation flash stopping a panel from booting far enough to take a
  command. Config is no longer in panel flash, a corrupted animation slot fails
  its length/CRC check and is replaced by the default automatically, and a
  panel whose app crashes at boot is still reachable through its bootloader
  over the bus.
- **One set of defaults, in a header both firmwares include**, so the panel's
  boot defaults and the master's reset defaults can't drift apart. It is the
  first of what will be several values shared between master and panel
  firmware: a shared directory (e.g. `firmware/shared/`), plain `#define`s that
  both C (panel) and Arduino C++ (master) read. Implementation note: Arduino
  builds a copy of the sketch elsewhere, so a relative include outside
  `firmware/master/` won't resolve; `build.sh` has to add the include path.
  How the Rust SDK gets the same values is a separate question, for when the
  SDK exists.

## Auto-calibration: starting direction (2026-10-20, not decided)

**Deferred until installed-pad data exists.** On the bench, sensors rest at
85–108 counts and a full press reads ~4000 (measured), so fixed 500/400
thresholds have a huge margin. Unknown, and what decides the design: the
resting value with the panel installed (its weight and mounting preload the
sensors), how far that drifts over a session (creep, temperature), and how much
the sensors differ once installed.

**The user's direction for when it is designed:** start from how stock works,
as the user understands it, then depart from it where useful:
- **Tare** each sensor's resting value to 0, after a settling period that
  measures how much to tare.
- **Map linearly** from the tared value to a fixed range covering the sensor's
  full travel, e.g. 0–250 (the 250 is a placeholder, not a choice).
- **Unlike stock, expose that mapping:** each sensor's raw-value endpoints of
  the 0–N range are visible and adjustable per sensor.
- Thresholds then live in the mapped units. The ranges and thresholds come from
  testing real panels with real feet.
- **Adjusting thresholds during play** (stock-style drift tracking): undecided.
  Measuring the resting value through a long session shows whether drift is
  real at all.

To settle when designing: whether the panel or the host applies the mapping
(the press comparison runs on the panel, in raw counts today), and how a
tracked tare on the panel would interact with thresholds the master sends.

**First data:** panels go into the pad within a day or two (from 2026-10-20),
with sensors on U, L, C, R and D only; the other four panels can borrow
sensors from the user's second pad if needed. Logging plan in
`docs/STATUS.md` → "Long-run tests".

## ~~Master-canonical config with panel flash as write-through cache~~ (decided 2026-07-09; panel-flash half superseded 2026-10-20, above)

Master holds the single editable copy of config; ~~panels persist a flash copy purely as a
boot-time bootstrap so they can threshold presses correctly before the master reconnects
(or if briefly running standalone)~~ — panels never edit their own copy independently. ~~On
any push from master, panel writes-through to flash and master's copy always wins on
mismatch (same magic-byte-detect-and-overwrite pattern already used for animation slots).~~

**Confirmed from a real logic capture** (`../rustmaniax-deadsync-cc/Logic Captures/CSV
Export/Complete Song - Running in the 90s.csv`, full 108.7s song played through the
official SMX game software, decoded per `../../stepmaniax-sdk-mp/docs/INTERNAL_BUS_PROTOCOL.md`):
stock's `'w'` config write repeats roughly **once per second** during actual gameplay —
107 occurrences over 108.7s, gaps consistently 1.00–1.03s (one outlier at 1.75s), all
254 bytes with `VV=0x12` (byte-exact match to the documented format, no false positives).
This is a tighter cadence than `INTERNAL_BUS_PROTOCOL.md`'s existing "~2 seconds" claim,
which was measured from an idle/no-host-software capture — active gameplay reasserts
config noticeably faster than idle does. Worth correcting that doc's finding 11 if it's
revisited.

**How to apply:** target a similar ~1Hz periodic re-push from our master to panels (not
just on first boot or on value change) as the sync mechanism — cheap given LED broadcast
already happens continuously at 60Hz+, and it matches real-world validated behavior from
the hardware this design supersedes.

## Stock sensor test modes — reference for auto-calibration design (2026-07-09)

Not a decision, just useful reference material for whenever auto-calibration (placeholdered
above) gets designed. The stock SDK's 4 sensor test modes (`SensorTestMode_UncalibratedValues`
/`CalibratedValues`/`Noise`/`Tare`, see `../../stepmaniax-sdk-mp/include/SMX.h:360-424`) are
**purely firmware-side computation on the same raw ADC stream — no extra hardware/sensor
capability implied**. The wire frame format is identical across all four modes (same
16-bit-per-sensor slots per `INTERNAL_BUS_PROTOCOL.md`'s 80-bit test frame); only the
*meaning* of the reported value changes per mode:

- **Uncalibrated**: raw ADC reading, unscaled — no processing at all.
- **Calibrated**: `raw − tare` (represents force) — simple subtraction against a stored baseline.
- **Tare**: the panel's *currently tracked* baseline value itself — implies the firmware
  continuously updates this over time (drift tracking), not a one-time boot-time zero-offset.
- **Noise**: running variance of recent readings (reported as variance; take `sqrt()` for
  actual std-dev) — implies the firmware maintains a rolling statistic per channel.

**Relevance to our design:** stock's tare mode is exactly the continuous drift-tracking
behavior the placeholdered `autoCalibrationMaxTare` field above is for, and noise mode is
an objective "is this sensor flaky" signal that maps onto the `badSensorMinimumDelaySeconds`
idea (a channel with implausibly high variance for too long could auto-flag itself, rather
than relying on a human eyeballing raw values). No BOM/hardware impact either way — just two
additional pieces of per-channel running state (baseline tracker, variance accumulator) for
the firmware to maintain whenever this gets designed.

## Open questions

- ~~Config write protocol over RS-485 (command format, ack, when panels commit to flash)~~ — decided 2026-10-20 ("Config persistence and the write protocol"); the byte layout of `'W'` is still to write
- ~~Config versioning / migration when the format changes~~ — decided 2026-10-20 ("Config versioning")
- ~~Factory reset story~~ — decided 2026-10-20 ("Factory reset")
- Auto-calibration design (drift tracking over time) — stock's four calibration
  parameters are placeholdered above pending this. **Deferred 2026-10-20 until
  there is data from panels installed in a pad** (see "Auto-calibration:
  starting direction")
