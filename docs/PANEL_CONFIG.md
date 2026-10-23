# Panel configuration — design

Status: **config on the bus is built (2026-10-20) and ran on master #2 and
nine panels the same evening** (`docs/BRINGUP_LOG.md`); the steps that need
someone at the bench (a press on a disabled channel, a panel 12 V cut) are not
run yet. Panels keep config in RAM only; the master keeps the only saved copy
and pushes it with `'W'`. What was built and how it behaves: "As built" below.

**Built in the gameplay firmware (`firmware/panel/c/main.c`):**

- Panel ID from the DIP at boot (0–8). `15` (all OFF) and other codes are not
  addressed on the bus.
- **DIP 10:** the sensor test runs standalone. The other diagnostic codes in the
  table below (9, 11, 12, 13) are not built.
- The three **LED modes** (normal, ID, sensor test), set by `'M'`. See below.
- **Config from the master's `'W'`** (2026-10-20, runs on all nine panels): per
  channel press/release thresholds and the `populated`/`enabled` flags, boot
  defaults 930/905 calibrated (~~500/400 raw~~ until 2026-10-22) with both
  flags set (`firmware/shared/panel_config.h`).
  **RAM only, by design**: a reboot returns to the defaults until the next
  `'W'`, and the "not configured" bit in `'f'` tells the master. `'C'` is gone.
- The debug LED `D202` lights on press in DIP 10 only, and stays dark otherwise.

**Built in the master (`firmware/master/master.ino`), 2026-10-20, run on
master #2 the same evening:** the canonical copy with its two-slot EEPROM record, the pushes
and re-push, per-panel config state, the console commands, and USB get/set/reset
config (`docs/USB_PROTOCOL.md`).

**Not built:** calibration and drift tracking, local animations (normal mode
goes dark when `'L'` frames stop), reset animations, and the other DIP
diagnostic modes.

~~The binary layout will slot into the flash config region alongside the
animation slots.~~ Superseded 2026-10-20: panels keep no config in flash. The
panel flash map is in `docs/FIELD_UPDATE.md` → "Flash map".

## What lives where

- **Panel ID (0–8):** hardware DIP switch, NOT flash config. A panel must know its
  identity before it can be addressed over the bus, so identity can't itself be a
  bus-configured value. Everything else below is configurable over RS-485 from
  the master. (Written when panels were to keep a flash copy; since 2026-10-20
  the master's EEPROM holds it and panels keep it in RAM.)
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
| 0 normal | The master's `'L'` frames. Dark if none for 100 ms (local animations come later). **No master** (no valid bus frame of any kind for 1 s): a slow dim white breath (4 s, peak 24/255), and all white (96) while INT is asserted. The breath steps in 1/25 of a level: every LED at the whole level and the remainder one level up, spread across the panel (2026-10-23; 24 plain levels looked choppy at the dim end). Bus panels only (DIP 0–8). **DECIDED 2026-10-22 (the user):** shows the panel has 12V and that its sensors trigger, with no master; no timeout. A running master always sends `'L'`, so only a missing, off or restarting master shows it |
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
**North = U, blue; East = R, green; South = D, yellow; West = L, red.** These
match the stock pad's own sensor test (compared on a stock pad 2026-10-22: the
same inward triangles, black at rest, brightness rising with pressure, no
threshold shown). ~~East = R, yellow; South = D, green~~ was the first
version, with those two swapped against stock; changed 2026-10-22 (the user).
No two sets overlap, so nothing needs blending. Brightness is off at or below
~~**200 counts** and linear from there to full at 4095.~~ **tare + 20 counts**
since 2026-10-22, brightness following the calibrated value to full at 1000. That floor is **NOT the
press threshold**, on purpose, because the user can change the press threshold.
~~**100 counts**; board 1 rests at 94–98, so 100 sits right at the noise floor
and may need raising.~~ Raised to 200 on 2026-10-22 (the user): in the pad the
sensors rest at 88–114, so 100 left some edges glowing faintly at rest, and
full weight reaches ~4090, so the extra floor costs nothing. ~~Once tare
exists, this becomes "a margin above tare is off": try tare + ~20 first.~~
Built the same day: tare + 20. Next to a stock pad, the stock sensor test
feels like it starts lighting sooner (the user, 2026-10-22, not measured); the
user's guess is that stock goes dark at about tare + 20. Not yet compared
again since.

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

### Presence is config; measurement can flag a mismatch

**Corrected 2026-10-22 from rev-1 measurements** (`docs/BRINGUP_LOG.md`): an
empty socket and a sensor at rest **do not** read the same. Empty sockets read
**0–23** (bench ~12 and 0–23, 0 on one brain; in the pad 13–18). A sensor at
rest reads **85–108** on the bench and **88–114** under the panel's preload.
By the divider that is roughly 0.4–0.6 MΩ (inferred), not an open circuit. A
cut near 50 would separate every reading so far. Not yet known: whether every
strip rests that high, and how far a strip with no force on it at all falls.

~~An FSR (Interlink 408) at zero force is essentially an open circuit (>1MΩ). With the
PCB pull-down, "unplugged" and "plugged in but untouched" both read ~0 — they are
**electrically indistinguishable**. No static measurement or pull-up trick can tell
them apart.~~ That was a datasheet-style assumption, written before any
hardware; the readings above contradict it. Config stays the authority, as
built:

- Measurement can **warn** on a mismatch: a channel configured `populated` that
  reads like an empty socket ("sensor unplugged?"), or one configured
  unpopulated that rests like a sensor. A warning for a human, not an automatic
  change (idea, not built: "Tracking, as built" → fault flags).
- Absence is inherently *safe* in this topology (reads ~~~0~~ 0–23, never crosses threshold) —
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

- Press threshold, **in calibrated units since config version 2** (2026-10-22,
  "Tare and calibrated values" below); default **930**. ~~12-bit ADC counts;
  resting 85–108 across nine boards (most 94–102), full press ~4000; the 500
  default sits comfortably between them. (Breadboard figures were ~100–115 /
  ~3900.)~~ Version 1, raw counts, until 2026-10-22.
- Resting baseline: now the panel's tracked **tare**, reported in every `'f'`.
- Hysteresis (separate press/release thresholds) — implemented, set per
  channel by the master's `'W'` (2026-10-20; `'C'`/`'c'` before that). RAM only
  on the panel by design; the master's EEPROM persists it.

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
  (press 930 / release 905 calibrated since 2026-10-22; 500/400 raw before).
  Nothing about config is written to panel
  flash.
- **The master is the only persistent store**: the canonical config for all
  nine panels in the Teensy's emulated EEPROM (below).
- **`'W'` carries one panel's whole config in one addressed packet** (per
  channel: press and release thresholds, flags, rest baseline; plus panel-wide
  fields and a format version; about 40 bytes, under the 80-byte limit), so a
  panel is never half-configured. *(As specified in `docs/RS485_PROTOCOL.md`
  later the same day, and built: version 1 is 21 bytes, with no rest baseline
  and no panel-wide fields.)* It takes effect immediately. The panel answers
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
rule. **Measured 2026-10-20** (master #2, 8,000 saves of a few changed bytes
each): median 0.9 ms, p99 1.6 ms, and **30–34.5 ms when the save triggers a
sector compaction** (18 of the 8,000). Plan on a save stalling everything for
up to ~35 ms (`docs/BRINGUP_LOG.md`).

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

### As built (2026-10-20; first run on master #2 + nine panels 2026-10-20, everything without a person at the bench)

**Shared definition.** `firmware/shared/panel_config.h` holds the format
version, the `'W'` layout, the validity rule (`release < press ≤ 4095`,
reserved flag bits 0), the defaults and the CRC-32. The panel includes it
through its `CMakeLists.txt` and the master through `build.sh`.
`firmware/shared/test/run.sh` checks it on the host, compiled as C and as C++;
the defaults' `'W'` CRC is `0xFD704831`.

**Console (master), DECIDED 2026-10-20 by the user: `S` and `F` edit the
master's copy in RAM and push it; `save` writes it to EEPROM.** So thresholds
can be tried without writing flash. USB Set config and both resets save at
once.

| Command | What it does |
|---|---|
| `c` | the master's copy per panel and channel, each panel's state, which slot and sequence it was saved in, unsaved edits, "reset at boot" |
| `S <panel> <press> <rel> [ch]` | thresholds for all four channels, or one; pushed, not saved |
| `F <panel> <ch> <populated> <enabled>` | a channel's flags (0/1 each); pushed, not saved |
| `save` | write the copy to EEPROM (the older slot) and read it back |
| `reset config` | defaults for every panel, saved and pushed. Typed in full, so it can't happen by accident |

`tools/fsr_monitor.py`'s threshold keys still send `S`, so they now set but no
longer survive a master reboot without `save`.

**Per-panel state** (`c`, and the USB config result event): matches, sent and
waiting, rejected (unknown version), rejected (invalid), not configured since
boot, absent. Only a `'w'` whose CRC equals the master's copy counts as a
match; a `'w'` that says "applied" with any CRC but the one sent counts as a
mismatch and is re-sent. A panel on the bring-up firmware, or on gameplay
firmware older than 2026-10-20, never answers `'W'` and stays at "sent and
waiting"; that is the sign it needs updating. A panel that rejects the version
is not re-sent at once on every poll, only on the ~1 Hz re-push.

**Pushes**, each in a poll slot with a reply window: when a panel appears
(this covers the master's own boot), when a panel that matched reports "not
configured" (it rebooted), when the copy changes, and the re-push, one
answering panel every ~111 ms. Cost: telemetry drops from ~14 Hz to ~12.7 Hz
per panel expected, **12.6 Hz measured** (2026-10-20, nine panels) (`docs/RS485_PROTOCOL.md` → timing).

**At boot** the master prints which slot it loaded, or that it found no valid
record and runs on defaults ("config was reset", also in USB device info until
something is saved). Each time a panel reaches "matches" the console prints
`# panel N (XX) config matches: 0:press/release …`, which
`tools/fsr_monitor.py` shows as that panel's ack; rejections print too. The
heartbeat line ends with how many answering panels match.

⚠ **Limit found while reading the emulation (2026-10-20, not measured): the
two slots share flash sectors.** The emulation spreads every address over its
15 sectors (`sector = (addr >> 2) % 15`), so slot A and slot B each have bytes
in every sector. Two slots still cover a power cut **during a save** (the slot
being written fails its CRC and the other one loads). They don't cover a power
cut **during a sector compaction**, which erases a sector and rewrites it from
RAM: that loses bytes of both slots. The CRC catches it, so the result is
defaults plus "config was reset", never a wrong config. Compaction happens only
after 2,048 writes to one sector, so it is rare.

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
  commands (reset config built 2026-10-20 as `reset config` and USB command
  0x06; reset animations not built): **reset config** (the master overwrites its EEPROM copy with
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
  SDK exists. *(Answered 2026-10-20 while building the SDK skeleton, for
  review: it repeats them, and a test reads this header and fails if any
  differ. `docs/HOST_SOFTWARE.md` → "Built".)*

## Tare and calibrated values (DECIDED and built 2026-10-22)

The user's direction below (2026-10-20), settled with the first installed-pad
data. **Built 2026-10-22, flashed to master #2 and all nine panels the same
day** (`docs/BRINGUP_LOG.md`). Why now: a default close to the user's preferred
sensitivity, so the pad can be played in ITGmania or DeadSync and the resting
values logged through real play.

**Data it rests on** (in the pad, 2026-10-22): sensors rest at 88–114 raw;
full body weight takes every sensor to 4078–4095; the divider reads ~20–25
counts above its model.

- **Tare:** each sensor's tracked rest value, in raw counts. **The panel tracks
  it** (core 1), so it exists without a master and the press loop never waits
  on the bus.
- **Calibrated value = (raw − tare) × 1000 / (4095 − tare)**: 0 at the tare,
  1000 at full scale, linear in raw. Negative just below the tare (noise). One
  definition, `kp_raw_to_cal()` in `firmware/shared/panel_config.h`; the SDK
  and the Python tools repeat it.
- **0–1000, not 0–250:** on our 10 kΩ divider the default press point sits at
  ~93% of the raw range, so 0–250 would leave ~17 steps between it and full
  weight, each ~16 raw counts. 0–1000 gives ~70 steps of ~4 counts, about the
  noise. A UI wanting 0–250 divides by 4.
- **Full scale is fixed at 4095** for now. Per-sensor adjustable endpoints (the
  user's 2026-10-20 direction) are a later format change if needed; every
  sensor reached 4078–4095 under body weight, so one top fits all today.
- **Thresholds are calibrated units.** The master sends them in `'W'`; the
  panel turns them into raw counts at its current tares
  (`kp_cal_to_raw()`, rounded up) and rebuilds them whenever a tare moves, so
  the 100 kHz press loop still compares raw counts. Rounded up means a raw
  comparison on the panel and a calibrated one on a host agree to the count.
- **A higher threshold depends less on the tare.** The span is stretched to
  the same top on every sensor, so the raw press point is
  `0.1 × tare + 0.9 × 4095` at 900: tares of 50 and 100 give raw thresholds 5
  counts apart, not 50 (the user's question, 2026-10-22). That is the right
  behaviour here: a resting sensor carries a tiny force next to a press
  (~400–800 kΩ at rest against ~0.8 kΩ at the default press point), so "the
  same force" at the press point means about the same FSR resistance, which
  means about the same raw count. Plain `raw − tare` with no stretch would put
  them 50 counts apart, ~20% in resistance near the top of our divider.
- **Default 930 / 905** (`KP_DEFAULT_PRESS` / `KP_DEFAULT_RELEASE`): 930 is
  ~3816 raw at a tare of 100, a firm press chosen to feel like a stock pad on
  its lowest sensitivity (the user). 905 is ~100 raw counts below it, the
  same gap as the old 500/400; a tighter gap chattered on light touches.

**Tracking, as built** (`firmware/panel/c/main.c`, `TARE_*`):

- Core 1 samples each sensor every 1 ms and judges it in **64 ms windows**.
- A window counts only when **no channel on the panel is pressed**, no release
  in the last **1 s**, at least **500 ms** after boot, the window's max − min is
  at most **64** (a spike) and its mean is within **8** of the previous window's
  (still moving).
- **Down at once** to a lower steady mean: an inflated tare makes presses
  harder. **Up slowly**, 1/128 of the gap per window (~8 s to close it), and
  only while the mean is at most **48** above the tare: more than that is a
  foot, not drift.
- **An empty socket's tare re-seeds** (2026-10-23, the user): a tare at or
  below **50** means no sensor (empty sockets read 0–23, sensors at rest
  85–114), so the next steady window sets it at once, as at boot. Found by
  unplugging and replugging D's North FSR in the pad: the tare fell to 22
  and, rest at 110 being more than 48 above it, stayed there until a reboot
  (`docs/BRINGUP_LOG.md`). The rule is a plain threshold on the tare, so a
  foot resting lightly on a freshly plugged sensor can become its tare, the
  same as at boot; it falls at once when the foot lifts. **Still not
  tracked:** any other upward step of more than 48 while steady (one that
  isn't from an empty socket) waits for a reboot.
- **Boot:** 100 until a sensor's first steady window, then that window's mean.
  A sensor resting above **1000** is never tared (a fault, not rest).
- **Reported** in every `'f'` (raw ×4, tare ×4, mask; `docs/RS485_PROTOCOL.md`),
  so the master, the USB stream (`docs/USB_PROTOCOL.md`, protocol 3) and the
  SDK see calibrated values live.
- **Test mode** lights an edge from **tare + 20** up, brightness following the
  calibrated value (the user's guess at what the stock sensor test does).
- Not built: a noise statistic, and a "sensor resting oddly" flag (stock's
  `badSensorMinimumDelaySeconds` idea). The tracking constants are first
  guesses, to tune from logged play (`docs/STATUS.md` → long-run test 4).
- **Idea, not decided (2026-10-22): hold the tare during gameplay.** If built,
  the game says it is in gameplay with a **heartbeat that expires** (the user's
  choice: the same shape as the stock panel test mode, which must be re-sent
  about once a second or it times out), so a crashed game can't leave tracking
  frozen. The master would own the setting (continuous / pause in gameplay /
  off) and pass a "hold tare" flag on the bus, e.g. in the `'L'` frames, so
  the panel config format doesn't change. On/off alone needs no game changes;
  the heartbeat needs deadsync (Rust SDK) or ITGmania (`krakenpad-c`, not
  built). **Not needed so far:** at the default 930 the raw press point moves
  0.07 counts per count of tare (tare 100 → 3816, 148 → 3819, the 1000 cap →
  3879); at 500 it moves 0.5. A long freeze arrow is already safe: the held
  channel stays pressed down to its release threshold, so the whole panel
  skips tracking, and below that the freeze has dropped in the game anyway.
  The exposed case is an idle panel with a foot resting on it lightly
  (≤ 48 counts above the tare). Logged play decides (the user: adjust if
  needed).
- **Idea, not decided (2026-10-22): fault flags for the config program**, the
  "sensor resting oddly" flag above made concrete:
  - **Resting high:** steady, unpressed windows that stay above `TARE_MAX`
    (1000, which the panel already refuses to tare) for some seconds. A foot
    can rest there legitimately for a while, hence a delay, as with stock's
    `badSensorMinimumDelaySeconds`.
  - **Stuck pressed:** the 1000 rule can't see it, because a pressed panel
    skips tracking before that check. It holds INT low (a held arrow in the
    game) and stops tracking on all four sensors. Likely signature: one
    channel pressed alone for a long time, since body weight on an edge drove
    three channels to near full scale (2026-10-22; inferred from that one
    test). The timeout has to outlast long freeze arrows; outside gameplay
    (the heartbeat above) a long press is far more suspicious.
  - **Unplugged:** a `populated` channel reading like an empty socket
    ("Presence is config; measurement can flag a mismatch" above).
  - **Reporting:** the `'f'` mask byte has three spare bits (4–6), so a
    panel-level fault bit fits without changing the record length, and the
    host already has each channel's raw and tare to say which one. Flagging on
    the panel reports the panel's own decision rather than a host-side copy of
    the rule.

**Config version 2** carries the new units in the same 20 bytes per panel. A
master that finds a version-1 record **does not convert it** (a deliberate
exception to "Config versioning" above): the units changed, and a converted
500/400 would keep the old, very light press point. It boots on defaults with
"config was reset", as for an unknown version.

### ~~Auto-calibration: starting direction (2026-10-20, not decided)~~ Superseded by the section above; kept for the reasoning

~~**Deferred until installed-pad data exists.**~~ On the bench, sensors rest at
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

## Press debounce (DECIDED and built 2026-10-22)

- **2 ms on both edges, at panel level, on the press output (INT):** the OR
  of the panel's channels must hold a new state for 2 ms before INT follows
  it. Per channel there is still only hysteresis and the ~0.1 ms persistence
  filter. The per-channel press bits in `'f'` (telemetry) are not debounced.
- **Why:** the first logged play session (`docs/BRINGUP_LOG.md`) showed ~100
  presses (~1.6%) re-pressing within a few ms of a release, or whole press
  cycles under 50 ms: one step wobbling across the threshold. Panel level,
  so weight passing between sensors doesn't release the panel either.
- **Why both edges, and why the press delay is acceptable** (the user): the
  delay is constant, so a game's global offset absorbs it, the same as pad,
  monitor and audio latency. Charts get fast, so both edges get filtered.
  ~~Press path in µs, never ms~~ (`docs/USB_PROTOCOL.md` → "Latency budget")
  is revised to match.
- **Why 2 ms:** a start (the user; ~~4 ms~~ was the first suggestion, and
  stock's config carries a 4,000 µs panel debounce whose exact semantics are
  unverified). Real jacks in the session lifted for 10–30 ms, so a release
  debounce has to stay well under 10 ms or it merges fast jacks.
- **Fixed for now** (`PRESS_DEBOUNCE_US`, `RELEASE_DEBOUNCE_US` in
  `firmware/panel/c/main.c`). **Meant to become a pad config field** (the
  user), not editable yet; adding it is a config version bump.
- The self-test's INT pulses bypass it (they are 2 ms themselves).

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

- ~~Config write protocol over RS-485 (command format, ack, when panels commit to flash)~~ — decided 2026-10-20 ("Config persistence and the write protocol"); layout in `docs/RS485_PROTOCOL.md`, built 2026-10-20
- ~~Config versioning / migration when the format changes~~ — decided 2026-10-20 ("Config versioning")
- ~~Factory reset story~~ — decided 2026-10-20 ("Factory reset")
- ~~Auto-calibration design (drift tracking over time) — stock's four calibration
  parameters are placeholdered above pending this. **Deferred 2026-10-20 until
  there is data from panels installed in a pad** (see "Auto-calibration:
  starting direction")~~ Built 2026-10-22 ("Tare and calibrated values").
  Still open: tuning the tracking constants from logged play, and a noise
  statistic / bad-sensor flag.
