# USB Protocol (ours) — v0

Status: **the input report is built and works on the bench** (2026-10-19, below).
**The vendor interface (command, response, event and stream packets) is built
(2026-10-20) and ran on master #2 the same evening** (`docs/BRINGUP_LOG.md`;
the press, cable-pull and visual LED checks not yet): every command in the table
below except underglow and reset animations, which answer "not supported yet".
What changed while building it: "As built" under the command table. Bench
tool: `tools/pad_cmd.py`. "Command inventory" further down is the older
walk-through of stock that the table came from. Derived by walking the stock SMX USB protocol
(`../stepmaniax-sdk-mp/docs/USB_PROTOCOL.md`) command-by-command and deciding
keep / adapt / drop. Intentionally NOT SMX-compatible — our own VID/PID and
descriptor; no software expecting an SMX pad will (or should) talk to us.

## Device identification

| Property | Value |
|----------|-------|
| VID/PID | Teensyduino default (0x16C0 + USB-Type-dependent PID) during development. **As built 2026-10-19: 0x16C0:0x0487**, borrowed from Teensyduino's Serial + Keyboard + Mouse + Joystick type, which keeps `arduino-cli upload` and the Teensy auto-reboot working. ⚠ Our interface set differs from the one Windows may have cached for that PID; if Windows ever shows the pad wrongly, remove the cached device first. Plan: register a real PID under pid.codes' shared VID 0x1209 (see below) once eligible. |
| Product string | "KrakenPad" — **working name, not final**, may still change |
| Interfaces (as built 2026-10-20) | 0–1 CDC serial (the bench console, unchanged), **2 vendor HID** (command/data: usage page `0xFF00`, usage `0x0001`, interrupt IN `0x85` + OUT `0x05`, 64 bytes, `bInterval` 1), **3 HID joystick** (pad input, IN `0x84`). The joystick was interface 2 until 2026-10-20: the Teensy core writes the RawHID block before the joystick's, and the numbers follow that order. Only the number changed; the joystick's descriptor and report did not |
| Transport | USB HID. Packets to the host 64 bytes; from the host 1,024 at High Speed, 64 at Full Speed (protocol 2, 2026-10-21). **Protocol 3** (2026-10-22): the FSR stream carries tares |
| USB speed | High Speed 480 Mbps (Teensy 4.x native) |
| Input polling | **8000Hz offered** (`bInterval = 1` at High Speed, as built). Host-side achieved rate not measured |

### VID/PID plan: pid.codes

Decided 2026-07-10: register via [pid.codes](https://pid.codes) (free, public,
the standard registry for open-source hobbyist USB hardware — VID 0x1209
shared across many projects, e.g. QMK keyboards) rather than purchasing a
vendor ID or using unregistered numbers indefinitely.

**Not eligible yet** — pid.codes requires a public repo with modifiable design
files (PCB sources and/or firmware) under a recognized open-source license
(LICENSE file required). `github.com/fchorney/krakenpad-poc` is public, but
there is **still no LICENSE file** (checked 2026-10-20). Revisit once there is.

**Process, for when it's time** (fork `pidcodes/pidcodes.github.com`):
1. `org/<org-name>/index.md` — organization page (frontmatter: layout, title,
   optional site URL, brief description)
2. `1209/<chosen-pid>/index.md` — pick an unused PID (avoid reserved ranges
   0xxx/1xxx), frontmatter: layout `pid`, title, owner, license, site URL,
   source URL, plus a short device description
3. Submit PR; live once merged

Until then: Teensyduino's default VID/PID is fine for all local development —
it only matters for enumeration on your own machine, not for shipping/sharing.

## Report layout (same 3-report shape as stock — it's a good shape)

| Report | Direction | Purpose |
|--------|-----------|---------|
| Input state | Device → Host | Panel press bitmask — the hot path |
| Command | Host → Device | Fragmented command channel |
| Data | Device → Host | Command responses, telemetry |

Stock's fragmentation scheme (flags byte: START/END, payload ≤ 61 bytes/packet,
one command in flight, response carries FINISHED flag) is simple and proven —
**keep the mechanism as-is**, drop the legacy variants inside it.

**DECIDED 2026-10-20: command and data go over HID, on a second HID interface
of their own** (vendor-defined usage page `0xFF00`), not in the joystick's
interface as stock does. Why HID: no driver on any OS, and hidapi (already in
deadsync and ITGmania) handles it; the traffic is small (config ~200 bytes,
telemetry ~1.2 KB/s at the measured ~14 Hz per panel), so a bulk endpoint buys
nothing. Why a separate interface: games and OS gamepad stacks keep seeing the
plain 9-button joystick exactly as built and tested, the SDK never contends
with a game for the joystick interface, and a fault in the command channel
can't touch the press path. **Fallback if that fails on some OS:** extra report
IDs in the joystick's interface, stock's way (the input report's ID 1 leaves
room). The CDC serial console stays as the bench interface.

**DECIDED 2026-10-20: packet framing on the vendor interface.** Every packet,
both directions:

| Byte | Field |
|---|---|
| 0 | kind: `cmd` (host → master), `resp` (reply to a command), `event` (unasked: panel state, per-panel config results, update progress, panel connect/disconnect), `stream` (telemetry) |
| 1 | transaction ID: set by the host on each command, echoed on every packet of its response; 0 for `event` and `stream` |
| 2 | flags: START, END (stock's fragmentation) |
| 3–4 | payload bytes in this packet, u16 (protocol 2, 2026-10-21; ~~byte 3 alone, u8~~ in protocol 1) |
| 5… | payload: up to 59 in a 64-byte packet, 1,019 in a 1,024-byte one |

- **One command in flight**, as stock. `event` and `stream` packets flow at any
  time between response packets; the kind byte lets the SDK's reader thread
  route them: `resp` completes the waiting call, `event` goes to the event
  queue, `stream` updates the state snapshot (`docs/HOST_SOFTWARE.md`).
- No report ID byte: the interface has one input and one output report.
  (hidapi still wants a leading 0 on writes, as for any report-ID-less device.)
- **As built 2026-10-20:** kind codes **`cmd` 1, `resp` 2, `event` 3,
  `stream` 4**; flags **START `0x01`, END `0x02`**; unused bytes after the
  payload are 0. The code byte (below) is byte 0 of the **whole message**, so a
  message that spans packets carries it once, in the first. Packets of one
  message are never interleaved with another's.
- **Packet size is a parameter, not a constant.** The device-info response
  states it and the SDK never assumes 64. **Shipped as 64 bytes at both
  speeds** (60 payload), which Full Speed requires. At High Speed, 64-byte
  packets carry ~500 KB/s each way against a planned peak of ~40 KB/s (60 Hz
  LED frames of 675 bytes, 12 packets / ~1.5 ms each), so ~10× headroom.
  ⚠ **The ~1.5 ms per frame was wrong** (measured 2026-10-21, macOS, hidapi):
  each write blocks for its transfer, ~0.38 ms per 64-byte write at `bInterval
  = 1`, so a frame takes ~4.2 ms with the master idle and ~9 ms with the bus
  running. One 1024-byte report takes ~0.47 ms (`docs/BRINGUP_LOG.md`). A
  larger High Speed report (up to 1,024 bytes, chosen by negotiated speed at
  enumeration) is a later option if a measured need appears; it would change
  the firmware descriptor, not the SDK's logic.

**DECIDED 2026-10-21: both speeds, bigger reports at High Speed** (the user),
because the measured need appeared (above) and a Full Speed link (a USB 1.1
hub, a KVM, an extender, most cheap USB isolators) should leave a slower pad,
not a dead one.
- **High Speed: 1,024-byte OUT reports**; **Full Speed: 64 bytes**, as now.
  The firmware picks by the negotiated speed (`usb_high_speed`); the core
  patch gains a second HID report descriptor and writes report count and
  `wMaxPacketSize` as two bytes (the stock core writes one, so it can't
  describe more than 255).
- The packet header's **payload length becomes 2 bytes** at both speeds, and
  the **protocol version is bumped**. No installed base to keep.
- The SDK takes the size from the HID descriptor as decided (item 5 below);
  "refuse anything but 64" goes.
- Full Speed still does everything, at stock-like rates: joystick 1 kHz, an
  LED frame ~12 ms (60 Hz just fits), telemetry ~64 KB/s.
- **IN (master → host) stays 64 bytes at both speeds** (the user,
  2026-10-21): a HID report is fixed-size, so a bigger IN report pads every
  event and response. Grow it later if the telemetry stream needs it.
- **Device info has to change with it:** its packet size is one `u8` and
  assumes one size both ways. It needs the OUT and IN sizes separately (OUT
  as `u16`) and the **negotiated speed**, which hidapi can't report, for the
  config app's connection status (`docs/HOST_SOFTWARE.md`).
- **Whether to drop or degrade features at Full Speed is not decided**: it
  waits on tests at Full Speed (the LED frame rate it really holds, the
  telemetry rate, the joystick under load).
- **Built 2026-10-21, protocol 2.** Core patch: the Full Speed report
  descriptor served from `usb.c`, two-byte report count and `wMaxPacketSize`,
  receive transfers sized by speed. Master, `tools/pad_cmd.py` and the SDK
  read the OUT size from the report descriptor. A bench build that forces
  Full Speed: `FULL_SPEED=1 firmware/master/build.sh upload`.
- **Measured 2026-10-21** (macOS, master #2, bus running): one LED frame
  **0.70 ms** median at High Speed (one packet; was ~9 ms as 12 × 64), and
  **15.5 ms** median, p95 19.9 ms, at forced Full Speed, so 60 Hz host LED
  frames at Full Speed have no headroom. Telemetry 12.7 Hz per panel at both;
  clock probe round trip ~0.5 ms High Speed, ~2 ms Full Speed
  (`docs/BRINGUP_LOG.md`).

**DECIDED 2026-10-20: telemetry stream.**
- **Each panel's `'f'` reply is forwarded as it arrives** (no fixed-rate
  whole-pad snapshot), as a 14-byte record: panel ID (1), **master timestamp in
  µs** when the reply arrived (4), 4 × raw ADC (8), the `'f'` mask byte with its
  "not configured" bit (1). Up to four records per 60-byte payload. No added
  batching delay, and the timestamps show real sample spacing (drift and noise
  analysis, missed polls). The SDK's snapshot keeps the latest per panel; the
  stream carries the history.
- **Off by default**, switched on by a command (in practice the SDK, when an
  app asks). Nothing fills the USB queue when no one reads, and a host that
  opens the pad late doesn't start with a backlog of stale reports buffered by
  its OS.
- **The master drops stream packets when its USB queue is full; it never
  blocks.** A stalled host must not slow the bus or the press path. As built:
  a stream packet is queued only when nothing else is waiting, so responses and
  events go first; records gather behind it up to four, and past that the
  newest is dropped and counted (`j` on the console).
- The enable command carries a **mode**; only **raw** exists until
  auto-calibration (stock's calibrated / noise / tare modes come then, without
  a format change).
- The rate is whatever the bus achieves (~14 Hz per panel today); raising it
  is poll interleaving, `docs/STATUS.md` next steps.
- The timestamps are the master's µs clock, the same timebase planned for
  timestamped presses (`docs/TIMESTAMPED_INPUT.md`).

**DECIDED 2026-10-20: firmware updates through the SDK.**
- **Panels (app and bootloader):** the update logic stays on the PC, ported
  from `tools/panel_update.py` / `panel_boot_update.py` into the Rust SDK. The
  master only forwards: a **bus passthrough command** on the vendor interface
  carries bus frames in `cmd` packets, bus replies come back as `event`
  packets, and the master pauses its own LED frames and polls while it is
  active, as the console's `P` does today. That replaces the text-console
  passthrough for the SDK; `P` stays for the Python bench tools.
- **The master:** a vendor-interface command reboots it into the Teensy's own
  bootloader (PJRC's HalfKay), and the SDK writes the new firmware over HID
  itself, following the protocol in PJRC's open-source `teensy_loader_cli`
  (details to check against its source for the Teensy 4.0 when built). Users
  install nothing extra to update the master.
- **Order when both need it: master first, then panels.** Panel updates run
  through the master, so a new panel image may rely on new master behaviour,
  never the reverse.

### Input state report

- 16-bit little-endian bitmask, bits 0–8 = panels 0–8 (same layout as stock)
- **Adapt:** sent on every state change (from the INT lines — no bus-cycle
  latency) plus heartbeat. Add a monotonic 8-bit sequence number so the host
  can detect missed transitions between polls.

**As built 2026-10-19** (`firmware/master/master.ino` → "USB pad input"; the
descriptor is in `firmware/master/core-patch/`):

| Byte | Content |
|------|---------|
| 0 | Report ID **1** |
| 1 | panels 0–7 (bit n = panel n, 1 = pressed) |
| 2 | bit 0 = panel 8; bits 1–7 constant 0 |
| 3 | sequence number |

- **Descriptor:** Generic Desktop / **Joystick**, one application collection;
  9 buttons (Button 1 = panel 0 … Button 9 = panel 8), 7 padding bits, then
  the sequence number on vendor page 0xFF00 ~~so games ignore it~~. Any OS or
  game that takes a generic joystick sees nine buttons. **Not every game
  ignores the sequence** (2026-10-22): DeadSync turns any changing field that
  isn't a button or hat into a raw axis, so its input checker shows
  `RawAxis [0xFF000001]` stepping on every press and release. DeadSync can't
  bind raw axes, so play is unaffected, but a game that auto-binds axes could
  act on it. **DECIDED 2026-10-22 (the user): leave it**, because DeadSync is
  to use the KrakenPad SDK directly later instead of its macOS HID input. The
  options, if another game needs it: declare the field Constant
  (`0x81, 0x03`; raw readers still get the byte, effect on macOS untested),
  move the sequence off the joystick report, or have the game skip
  vendor-defined pages.
- **No axes, on purpose.** A declared axis that never moves can read as held
  at one end in some games. If a host turns out to need axes, put them in a
  separate report that is never sent.
- **Report ID 1** leaves room for the command and data reports in the same
  interface, as stock does.
- **Sequence:** steps by one on every press-state change; heartbeats repeat it.
  A step of more than one means the host missed a transition.
- **Heartbeat:** the current state is resent every **10 ms** with no change.
  **DECIDED 2026-10-19: keep 10 ms** (stock is roughly 100 ms). It only bounds
  how soon a host that opens the pad late learns the current state; presses
  never wait for it. Revisit if it turns out to matter. Measured idle rate
  91–95 reports/s: the 1 kHz timer makes each period 10–11 ms.
- **Change reports are not rate-limited:** each one is queued from the INT ISR
  at once (4-deep queue), so the ceiling is the host poll, 8000/s at High
  Speed. The busy-case rate has not been measured.
- **Endpoint:** interrupt IN, 4 bytes, `bInterval = 1`: **125 µs (8 kHz) at
  High Speed**, 1 ms at Full Speed. Host-side rate not measured yet.
- **Bench 2026-10-19:** presses from the eight-panel bench arrive correctly on
  a Mac, checked with `j` and `tools/hid_monitor.py`.
- **Sent from interrupt context, never from `loop()`**, because an LED burst
  blocks `loop()` for ~7 ms. The INT edge ISR qualifies the edge and queues
  the report at once. A 1 kHz timer retries a report that found the queue
  full, sends the heartbeat, and corrects the mask if a line disagrees with it
  for two consecutive ticks (counted as a resync; it should stay 0).
- **Qualify window: 10 µs.** The edge ISR waits 10 µs and re-reads the line;
  if the level no longer differs from the reported state, the edge is counted
  as a glitch and nothing is sent.
- **Held off** during the master's `I` self-test and `N` pull-up check, whose
  INT activity is not presses.
- **Build:** Teensyduino cannot take a custom descriptor without changing its
  core, so `firmware/master/build.sh` compiles against a patched copy of the
  installed core (Teensyduino 1.62.0). Since 2026-10-20 a plain `arduino-cli
  compile` fails on purpose: the sketch needs `firmware/shared/`, which only
  `build.sh` adds to the include path.
- Bench check: `j` on the master console; `tools/hid_monitor.py` on the PC.

#### The INT lines are the **sole, authoritative** press path

The per-panel interrupt wires *are* the gameplay input. The RS-485 FSR poll is
**telemetry/diagnostics only** and never gates a press (see RS485_PROTOCOL.md
"Future idea" note). There is **no software veto** of an INT edge against FSR
data — a spurious INT edge would be reported as a real press. That is why the
INT line gets hardware conditioning (per-end TVS + series R + 1 nF RC, see
MASTER_PCB.md / DUAL_PANEL.md) **and** a firmware glitch-qualify (below). Do not
design any part of the press path to depend on the FSR telemetry.

#### Latency budget — ~~keep the press path in µs, never ms~~ keep it short and constant

> **Revised 2026-10-22 (the user):** the panel now debounces its press output
> by **2 ms on both edges** (`docs/PANEL_CONFIG.md` → "Press debounce"), so a
> press reaches the host ~2 ms after the sensor crosses its threshold. What
> matters for play is that the delay is **constant**: a game's global offset
> absorbs it, as it does pad, monitor and audio latency. A *variable* delay
> (jitter) is still the thing to avoid. The table below is the path without
> the debounce.

A press must reach the host as fast as possible; **1 ms is the "getting dicey"
line**, and everything below is dominated by the USB poll quantization we
already accept (8 kHz HS = 125 µs period, 0–125 µs wait; 4 kHz = 250 µs).

| Stage | Typical | Note |
|-------|---------|------|
| Panel FSR sample → threshold | ~10–80 µs | at ~100 kHz sampling; keep **detection** averaging light (SNR is huge: rest ~110 vs press ~3900) — reserve 8× averaging for calibration only, off the hot path |
| Wire prop (~4 ft) | ~5 ns | negligible |
| RC settle (330 Ω·1 nF) | ~1 µs | **press edge only** — driven hard LOW by the panel. ⚠ The *release* edge is ~20× slower (~22 µs measured, see below): it rises passively through `RN1`'s 10k, not the 330 Ω |
| Master edge → glitch-qualify | ~5–20 µs | see below — hidden in USB dead-time, ~0 net |
| USB poll quantization | 0–125 µs (8k) | inherent, the dominant term |
| **Device-side total** | **~sub-200 µs typ** | ≥5× under the 1 ms line |

> **Measured on master #1 + one panel, 2026-09-08** (`docs/BRINGUP_LOG.md`):
> assert and release are **not symmetric**, and only the assert is on the press
> path. Assert is driven LOW by the RP2040 and settles inside the 330 Ω·1 nF
> figure above. Release is the pin going hi-Z, after which the line rises
> *passively* through `RN1`'s 10k into `C3` (1 nF) plus the junction capacitance
> of `D1` and the panel's `D201` — **~22 µs to VIH, four readings within 2 µs of
> each other.** Consistent with ~1.8 nF total, i.e. ordinary SMAJ5.0A
> capacitance; not a fault.
>
> **This changes nothing about the press path or the qualify window.** It is
> recorded so nobody applies the 1 µs row to a release, and so a future
> release-side timing question starts from a measurement instead of the RC
> that governs the other edge.

**Master glitch-qualify (the backstop that replaces the non-existent FSR veto):**
latch the INT edge in the ISR immediately, then confirm the line is *still* LOW
when building the next USB report (that free 0–125 µs pre-poll window). Real
press (tens of ms) confirms; sub-µs ESD/EMI residue (already gutted by the RC)
is discarded. Net added latency ≈ 0 in the typical case; worst case is one extra
frame (+125 µs at 8 kHz) if the edge lands right at a poll boundary. **Rules:**
keep the qualify window single-digit-to-low-tens of µs; **never do multi-sample
averaging on the master** — it reads a clean 1-bit line after the RC and needs
only an "is it still low?" check. Averaging is strictly a panel-side (analog)
concern. If latency ever needs squeezing, the biggest lever is **8 kHz vs 4 kHz
USB**, not the qualify window.

## Command table (reviewed 2026-10-20, built 2026-10-20, not yet run on hardware)

Applies the decisions above to concrete payloads. Reviewed by the user on
2026-10-20: the choices listed at the end are decided.
Codes and payloads may still shift when it is built. All multi-byte fields are
little-endian. In every `cmd`, `resp`, `event` and `stream` packet, payload
byte 0 is a code saying which one it is; the tables below give the rest.

### Commands (`cmd`, host → master)

| Code | Command | Request payload | Response (`resp`) |
|---|---|---|---|
| 0x01 | Device info | — | below |
| 0x02 | Get config | — | config version u8, then the 180-byte panel config block of the EEPROM record (`docs/PANEL_CONFIG.md`). Version 2 (2026-10-22): thresholds in calibrated units 0–1000 |
| 0x03 | Set config | config version u8 + 180-byte block | status u8: 0 saved, 1 unknown version, 2 invalid (+ panel u8, channel u8 of the first bad value). Saved = in EEPROM and being pushed; each panel's outcome follows as a *config result* event |
| 0x04 | Pad status | — | per panel 0–8, 12 bytes: present u8, last reply age ms u16 (0xFFFF = never), polls answered % u8, CRC errors u16, config state u8 (*config result* codes), panel build tag u32, termination switch u8 |
| 0x05 | Telemetry stream | on u8, mode u8 (0 raw) | status u8 |
| 0x06 | Reset config | — | status u8. Defaults saved and pushed; results follow as events |
| 0x07 | Reset animations | panel mask u16 | status u8. Waits on the animation design (writing slots) |
| 0x08 | LED frame | 9 panels × 25 LEDs × RGB = 675 bytes, panel 0 first, each panel in its wire (serpentine) order (`docs/ANIMATIONS.md`) | none (see question 2) |
| 0x09 | Underglow frame | 44 groups × RGB = 132 bytes, from the connector end | none (see question 2) |
| 0x0A | Panel LED mode | panel mask u16, mode u8, seconds u8 (0 = until changed) | status u8 |
| 0x0B | Slot self-test | — | per INT slot 0–8: answering panel ID u8 (0xFF none), ack u8. The master's `I` |
| 0x0C | Zero counters | — | status u8. The master's `z` |
| 0x0D | Bus passthrough | enter u8 (1 enter, 0 leave) | status u8 |
| 0x0E | Bus frame | cmd u8, addr u8, payload (≤ 80) | none; bus replies come back as *bus frame* events |
| 0x0F | Reboot to bootloader | magic u32 (guards against a stray byte) | status u8, then the master reboots into HalfKay |
| 0x10 | Clock probe | host time u64 (host's units) | host time echoed u64, master receive µs u32, master send µs u32 |

**Device info** (`resp` to 0x01; protocol 2 layout, 2026-10-21): protocol
version u8, **USB speed u8 (0 Full, 1 High), IN packet size u16 (64), OUT
packet size u16 (1,024 or 64)**, firmware version major/minor/patch u8 ×3 +
build tag u32, config format
version u8 (highest it speaks), player ID u8 (DIP), flags u8 (bit0 config was
reset at boot, bit1 stream on, bit2 passthrough active), panel presence u16
(bit n = panel n answering), termination check u8 (0 OK, 1 none, 2 several,
3 wrong panel) + the terminated panel's ID u8. 21 bytes. (Protocol 1 had one
packet size u8 at byte 2 and no speed: 17 bytes.) A host must check byte 1
before reading the rest: another version may lay it out differently.

### Events (`event`, master → host, unasked)

| Code | Event | Payload |
|---|---|---|
| 0x81 | Panel presence changed | panel u8, present u8 |
| 0x82 | Config result | panel u8, state u8 (0 matches, 1 sent and waiting, 2 rejected: unknown version, 3 rejected: invalid, 4 not configured, 5 absent), panel's config CRC u32 |
| 0x83 | Config was reset | — (also in device info, for a host that connects later) |
| 0x84 | Bus frame | cmd u8, addr u8, payload: a CRC-valid frame from the bus, passthrough only |

### Streams (`stream`, master → host, only while enabled)

| Code | Stream | Payload |
|---|---|---|
| 0xC2 | FSR | **protocol 3** (2026-10-22): up to 2 records of 22 bytes: panel u8, master µs u32, then the panel's `'f'` payload (raw u16 ×4, tare u16 ×4, mask u8; `docs/RS485_PROTOCOL.md`). Tares are 0xFFFF from panel firmware that predates them |
| ~~0xC1~~ | ~~FSR raw~~ | ~~up to 4 records of 14 bytes (decided above)~~; protocol 2, replaced by 0xC2 |

Master timestamps are u32 µs and wrap every ~71.6 minutes; the SDK unwraps
them.

### As built (2026-10-20; first run on master #2 + nine panels 2026-10-20, everything without a person at the bench)

Where the build fills in or shifts the table above. Implemented in
`firmware/master/master.ino` → "USB vendor interface"; `tools/pad_cmd.py`
speaks all of it.

- **Errors.** A command that can't be answered in its own format gets
  `resp` **`[0x7F, command code, reason]`**: 1 unknown command, 2 bad length,
  3 busy / not now (a bus command during passthrough, a bus frame outside it),
  4 not supported yet (reset animations, underglow).
- **Transaction 0** on anything but a frame command is ignored and counted as
  malformed. **Any non-zero transaction gets a response**, including LED and
  bus frames (`[code, 0]`), so a host waiting on one never hangs.
- **Set config:** status **3 = pushed, but the EEPROM read-back didn't match**.
  The version is checked before the length, so a newer, longer format gets
  "unknown version", not "bad length".
- **Pad status**, per field: present = answered a poll within 1 s; polls
  answered % = the last completed 5 s window; CRC errors, build tag and
  termination switch = the panel's own, from its latest `'h'` (the master asks
  each answering panel every ~4.5 s in the background), 0 / 0 / `0xFF` before
  the first one.
- **Device info:** flags **bit 3 = unsaved console edits** (`S`/`F` without
  `save`); termination check **`0xFF` = nothing to judge yet** (no `'h'` from
  any answering panel), with ID `0xFF`. The ID is `0xFF` too for "none" and
  "several". "Config was reset" clears once a config is saved.
- **Telemetry stream:** a mode other than 0 → status 1.
- **Panel LED mode:** status 1 for mode > 2 or an empty mask. All nine →
  one broadcast `'M'`; a subset → one addressed `'M'` per panel.
- **Slot self-test** response, with a status and two masks ahead of the
  table: `[status u8 (0 ran, 1 stopped by the pre-check)] [INT slots found LOW
  u16] [panel IDs that acked u16]`, then per slot 0–8 `(panel ID, ack)`. The
  masks keep what the per-slot table can't show: which line blocked the test,
  and a panel that acked with no INT edge. It blocks the master ~0.7 s.
- **LED frames from the host, DECIDED 2026-10-20 by the user:** they replace
  the master's own pattern (rainbow plus press colours) and **hand back 1 s
  after the last one**. The master sends whichever frame it holds when its own
  60 Hz LED slot comes round, so the newest wins. The console's `w` (full
  white) still overrides them.
- **Bus passthrough** ends on "leave", on a USB reset, or after 30 s with
  nothing from the host, as the console's `P`. Bus frames from the host go out
  unchanged; every CRC-valid frame heard comes back as a bus frame event. Other
  commands keep working; those that need the bus answer "busy".
- **Reboot to bootloader:** the magic is the bytes **`"BOOT"`** (u32
  `0x544F4F42`). The response is given ~50 ms to leave before the reboot.
- **Clock probe:** "master receive µs" is when `loop()` read the packet, which
  can be up to ~7 ms late behind an LED burst; "master send µs" is when the
  response was built. The SDK should trust the probes with the shortest round
  trip.
- **Events start with the host's first command** after USB configures, so a
  pad nobody has opened doesn't fill its queue with stale ones. If the boot
  found no saved config, the "config was reset" event goes out then.
- **A USB reset or replug** ends the stream, passthrough and host LED frames;
  the next host starts clean. The SDK should still discard whatever it reads
  before its first response (`tools/pad_cmd.py` drains for 200 ms).
- ~~Packet size stays 64 at both speeds, as planned.~~ Protocol 2 (2026-10-21):
  OUT packets are 1,024 bytes at High Speed (above).

### Choices in this table

1. **DECIDED: numeric codes on USB, letters on the bus.** Reason to keep
   the bus on letters: every panel bootloader recognises `'B'` and `'Q'`, which
   is how any app reaches a panel on any firmware, so the bus letters are
   permanent; USB has no deployed base yet. Letters also read at a glance on a
   scope or logic-analyser decode.
2. **DECIDED: fire-and-forget.** A command with **transaction ID 0 gets no
   response and doesn't count as in flight**; only LED frames, underglow frames
   and bus frames may use it. The newest LED frame wins, as stock.
3. **DECIDED: LED pixel order** on USB is each panel's wire (serpentine) order;
   the SDK offers a helper from a row-by-row image, so games never deal with
   the serpentine.
4. **DECIDED: underglow is RGB on USB**; the master converts to the strip's
   order. Pads' strips differ (RGB, GRB, BRG seen), so the byte order becomes a
   **per-pad config field**, added with the other underglow settings in a later
   config version (`docs/PANEL_CONFIG.md`).
5. **DECIDED: packet size bootstrap:** the SDK reads the report size from the
   HID descriptor (hidapi) before its first command; device info confirms it.
6. **DECIDED: pad status** is one call for the config app's overview: bus health
   plus each panel's config state, build tag and termination switch.

## Command inventory (from stock → ours)

### Keep (with cleanups)

| Stock | Ours | Notes |
|-------|------|-------|
| Device info (flag 0x80) | keep | version, serial, capabilities; add per-panel presence bitmap + termination check result (see MODULAR_PANEL_COUNT) |
| `'G'` get config | keep | our config format (see PANEL_CONFIG.md); single format, no v4/v5 legacy split. **Answered from the master's canonical copy** (2026-10-20): the config app reads with `'G'`, edits, writes with `'W'` |
| `'W'` set config | keep | ack + read-back verify like stock SDK does. The master saves it to EEPROM and pushes RS-485 `'W'` to each panel; whether each panel accepted it (its `'w'` CRC) is reported back (2026-10-20) |
| `'f'` factory reset | keep, **split** (2026-10-20) | ~~resets config + animation slots (magic-byte redetect, defaults rewritten)~~ two separate actions: reset config (master EEPROM → defaults, pushed to panels) and reset animations (one panel or all). `docs/PANEL_CONFIG.md` → "Factory reset" |
| `'C'` force recalibration | keep | master forwards to panels over RS-485 |
| `'L'` platform strip | keep | underglow, master-local, write-on-command (see UNDERGLOW.md) |
| `'m'`+`'d'` animation upload | adapt | upload `.smxa` to panel flash slots. Ack-based flow control over RS-485 replaces the blind `'d'` delay hack; no interleaved-across-panels dance, no send-twice-for-reliability |
| `'y'` sensor test | adapt | clean telemetry request: raw / calibrated / noise / baseline. Plain per-panel arrays — stock's bit-interleaving was an artifact of its parallel signal-wire readout; our RS-485 replies make it unnecessary |

### Replace

| Stock | Problem | Ours |
|-------|---------|------|
| `'4'`/`'2'`/`'3'` LED triple | legacy split (16-LED grid + v4 inner 3×3 bolt-on), 0.6666 color scaling, 30 FPS cap | **one full-frame command**: 9 panels × 25 LEDs × RGB = 675 bytes, fragmented (~12 packets). No color scaling (panel gamma/brightness policy lives panel-side). Rate cap = LED refresh target (60Hz+), newest-frame-wins like stock |
| `'t'` panel test mode | firmware-side pressure lighting demo | ~~drop from firmware — host tool renders pressure lighting from telemetry it already gets (`tools/fsr_monitor.py` is the prototype of this)~~ **Reversed 2026-10-18: kept, rendered on the panel** (`'M'` mode 2, `docs/PANEL_CONFIG.md` → "LED modes"). The host route would be limited to the ~14 Hz per-panel telemetry rate |

### Drop

| Stock | Why |
|-------|-----|
| `'g'`/`'w'` old config format | no legacy installed base |
| `'l'` legacy lights-off | send a black frame |
| `'s'` set serial | use the MCU's factory-burned unique ID instead of writable serial |
| `'S 1'` re-enable auto lights | unnecessary — panels auto-fallback 100ms after LED frames stop (stock needed an explicit resume because its timeout was seconds) |
| auth/leaderboard commands | deliberately absent — this pad is not SMX-eligible |

### New (no stock equivalent)

| Command | Purpose |
|---------|---------|
| Bus health | RS-485 stats: per-panel poll success %, CRC error counters, last-seen |
| Panel channel config | per-channel `populated`/`enabled` flags + thresholds (see PANEL_CONFIG.md) — stock buries a subset of this inside the config blob; ours is also addressable per panel |
| FSR stream toggle | continuous telemetry stream for host-side tuning/visualization UIs |
| Latency probe | echo/timestamp command for measuring host↔device round-trip |

## Handshake (same shape as stock)

1. Host enumerates by VID/PID, opens HID; device is already streaming input reports
2. Device info request → version/serial/capabilities/panel-presence
3. Get config → host caches
4. Connected; host processes input

## Open questions

- ~~Exact report IDs / descriptor layout~~ input report decided and built 2026-10-19 (above); the vendor interface built 2026-10-20 (no report IDs, usage page `0xFF00` usage `0x0001`)
- ~~Whether FSR streaming uses the data report or a dedicated input-style report~~ decided 2026-10-20: the `stream` kind on the vendor interface
- ~~Config split: which fields live master-side vs panel-side~~ decided 2026-10-20: the master holds all of it; panels are RAM-only (`docs/PANEL_CONFIG.md`)
- 2000Hz+ polling: HS bInterval choice (0.5ms vs 0.25ms vs 0.125ms) after
  measuring real host-side behavior
