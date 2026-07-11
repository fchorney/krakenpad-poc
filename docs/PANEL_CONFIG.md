# Panel Configuration (early design notes)

Status: **draft / musings** — collecting config-related decisions as they come up.
Not yet a binary format spec. Binary layout will slot into the flash config region
alongside the animation slots (see `docs/ANIMATION_BINARY_FORMAT.md`).

## What lives where

- **Panel ID (0–8):** hardware DIP switch, NOT flash config. A panel must know its
  identity before it can be addressed over the bus, so identity can't itself be a
  bus-configured value. Everything else below lives in flash and is configurable
  over RS-485 from the master.
- **Per-channel FSR config:** flags + calibration, 4 channels per panel (see below).
- **RS-485 fallback timeout:** panel falls back to local (flash-stored) animation if
  no LED frame arrives from the master within **100ms**. Probably configurable,
  100ms default.

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
| 0–8 | Panel ID | Normal operation (existing) |
| 9 | **LED Check** | All 25 LEDs lit dim white — visual "are they all alive" check, no bus needed. *(from stock, value 13)* |
| 10 | **Sensor Pressure Test** | Each of the 4 FSR edges lights a distinct color when pressed — visual per-sensor wiring/function check, no bus needed. *(from stock, value 14)* |
| 11 | **Standalone Mode** | Panel lights on press, entirely independent of the master — isolates a single panel for bench testing with just 12V power, no RS-485/master present at all. *(from stock, value 15)* |
| 12 | **Raw ADC Streaming Mode** (new) | Continuously dumps raw per-channel FSR ADC values (e.g. over UART/serial) regardless of whether a master is present — useful for bench calibration work without wiring up the full rig. |
| 13 | **Factory Reset Trigger** (new) | Forces the flash animation/config magic-byte wipe-and-rewrite-defaults path on boot — a recovery mechanism that doesn't depend on any working host software or bus command, useful if config/animation flash ever gets corrupted and normal boot can't get far enough to accept a reset command. |
| 14–15 | Reserved | Unused for now, headroom for future diagnostic modes |

**How to apply:** panel firmware reads the DIP value once at boot (same as it already
does for panel ID); values 9–13 branch into a diagnostic routine instead of normal
main-loop operation (mirrors stock's documented behavior: "panels will not function
normally when in diagnostic modes"). No hardware changes needed — same DIP switch,
same GPIO read, just more branches in the boot-time dispatch.

**Debug press LED (GPIO3, R15/D1 on the panel PCB) is diagnostic-mode-gated
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

- Press threshold (12-bit ADC counts). Bench reference: resting ~100–115, full press
  ~3900, threshold ~500 sits comfortably between noise floor and activation.
- Resting baseline (captured at calibration time) — for drift diagnostics.
- Hysteresis (separate press/release thresholds) — implemented (`g_press_thr`/
  `g_rel_thr` in main.c, bus-settable via `'C'`/`'c'`, see RS485_PROTOCOL.md).
  RAM only for now; flash persistence is the remaining open item.

## Boot-time float detection (firmware behavior, not config)

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

## Master-canonical config with panel flash as write-through cache (decided 2026-07-09)

Master holds the single editable copy of config; panels persist a flash copy purely as a
boot-time bootstrap so they can threshold presses correctly before the master reconnects
(or if briefly running standalone) — panels never edit their own copy independently. On
any push from master, panel writes-through to flash and master's copy always wins on
mismatch (same magic-byte-detect-and-overwrite pattern already used for animation slots).

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

- Config write protocol over RS-485 (command format, ack, when panels commit to flash) — the `'C'` threshold command (RAM-only) is the prototype of this pattern; needs a flash-commit step
- Config versioning / migration when the format changes
- Factory reset story: animation slots already have magic-byte detection + rewrite
  of defaults; config should behave the same way
- Auto-calibration design (drift tracking over time) — stock's four calibration
  parameters are placeholdered above pending this
