# RS-485 Bus Protocol (v0)

Status: **v0, running on rev-1 hardware** — master #2 and nine panels on one bus
(2026-10-19). Every command below is implemented on both sides unless it says
otherwise. Implemented in `firmware/master/master.ino`, `firmware/panel/c/main.c`
(gameplay), `firmware/panel/c/bringup/main.c` (bench) and
`firmware/panel/c/bootloader/` (update commands). **Config (`'W'`/`'w'` and the
"not configured" bit) was built 2026-10-20 and has not yet run on a board.**
Calibration is not built (`docs/PANEL_CONFIG.md`).

## Physical layer

- UART **1 Mbps, 8N1**, half-duplex RS-485 (MAX3485 / THVD1450)
- Master drives the bus by default; a panel transmits only when replying to a
  poll addressed to it
- DE/~RE tied together per transceiver:
  - Teensy: `Serial2.transmitterEnable(6)` — hardware manages DE automatically
  - RP2040: manual — raise DE, `uart_write_blocking`, `uart_tx_wait_blocking`
    (TX **complete**, not FIFO-empty), drop DE

## Framing

Every message, both directions:

| Offset | Field | Value |
|--------|-------|-------|
| 0 | sync | `0x55` |
| 1 | cmd | command byte |
| 2 | addr | panel ID 0–8, or `0xFF` broadcast |
| 3 | len | payload length (0–80) |
| 4 | payload | `len` bytes |
| 4+len | crc | CRC-8 (poly `0x07`, init `0x00`) over cmd, addr, len, payload |

Receivers hunt for `0x55`, then validate CRC; on mismatch, drop and re-hunt.
(`0x55` can appear inside payloads — the CRC is what actually validates a frame.
Good enough for v0; re-sync loss just costs one frame.)

**A packet is sent without pauses** (2026-10-20). The panel's gameplay firmware
abandons a packet if the bus goes idle for more than **500 µs** partway
through it, and counts it in `'h'`'s parser-timeout field. Without this, garbage that
happened to parse as a large `len` swallowed the next real packets as its
payload. Recovery waits for the next idle gap, so inside the master's
back-to-back `'L'` burst it comes at the end of the burst. The threshold sits
above the 320 µs the 32-byte RX FIFO takes to fill, so interrupt latency alone
can't fake a gap without also overrunning the FIFO. The panel bootloader has
the same timeout since 2026-10-20 (it does not count it).

**The master resyncs at its own transmissions instead** (2026-10-20, built,
not yet run on the bus). It can't time bytes: the Teensy core's interrupt
buffers them and `loop()` reads them late, up to ~7 ms behind an LED burst.
It doesn't need to. Every reply answers one of the master's requests, and the
master transmits again only once that request's reply window has closed, when
the whole reply is readable. So before each transmission it parses what has
arrived, and a packet still open then is dropped: garbage that parsed as a
header, or a reply cut short. Counted on the heartbeat ("partial packets
dropped") and zeroed by `z`. Bench test: `docs/MASTER_BRINGUP.md` → "Parser
resync".

## Commands

### `'L'` — LED frame (master → panels)

- addr = panel ID (per-panel frame) or `0xFF` broadcast, len 75
- Payload: 25 × RGB triplets, **serpentine LED order** (LED index i at bytes
  3i, 3i+1, 3i+2 = R, G, B)
- Panel displays the frame on receipt (LED mode 0). If no `'L'` frame arrives
  within **100ms** the panel goes dark; falling back to a local animation from
  flash is planned (`docs/ANIMATIONS.md`) but not built.
- Multi-panel: **per-panel addressed `'L'` frames** (addr = panel ID) — working
  as of the 2-panel bench bring-up 2026-07-11. A single big broadcast with
  per-panel offsets was the alternative considered; not needed at current bus
  utilization (~6% single-panel, ~9ms full 9-panel cycle).

### `'F'` — FSR poll (master → one panel)

- addr = panel ID, len 0
- Panel replies immediately with `'f'`.

### `'f'` — FSR data (panel → master, reply only)

- addr = panel ID, **len 17** (since 2026-10-22; ~~len 9~~ before, with no
  tares, which the master still accepts from older panel firmware)
- Payload: 4 × uint16 little-endian raw ADC values (channels 0–3), **then 4 ×
  uint16 tares** (each channel's tracked rest value, raw counts;
  `docs/PANEL_CONFIG.md` → "Tare and calibrated values"), then 1 byte pressed
  bitmask (bit i = channel i pressed, post-hysteresis/persistence). Offsets
  and lengths: `KP_F_*` in `firmware/shared/panel_config.h`.
- Reply window (5 + 17) × 10 + 260 = **480 µs** (was 400 µs).
- Bit 7 of the last byte = **not configured since boot** (decided 2026-10-20,
  **built 2026-10-20, not yet run on a board**). Set at every boot, cleared by
  the first accepted `'W'`; the master sends that panel its config at its next
  poll slot. Readers take the pressed mask as `byte & 0x0F`. The bring-up
  firmware and gameplay firmware older than 2026-10-20 never set it.

### `'W'` — set whole panel config (master → one panel) — built 2026-10-20, not yet run on a board

- addr = panel ID (never broadcast; a panel ignores a broadcast `'W'`), len 21.
  Design and reasoning: `docs/PANEL_CONFIG.md` → "Config persistence and the
  write protocol". **The layout, the validity rule, the defaults and the CRC
  are defined once, in `firmware/shared/panel_config.h`**, which both firmwares
  include (`firmware/shared/test/run.sh` checks it on the host).

| Offset | Size | Field |
|---|---|---|
| 0 | 1 | config format version, **2** (bumped on any format change; ~~1~~ until 2026-10-22) |
| 1 + 5i | 2 | channel i press threshold, **calibrated units 0–1000** (~~raw ADC counts~~ in version 1), u16 LE (i = 0–3, the `'f'` order) |
| 3 + 5i | 2 | channel i release threshold |
| 5 + 5i | 1 | channel i flags: bit0 `populated`, bit1 `enabled`, bits 2–7 must be 0 |

- A channel takes part in press detection only when both flag bits are set.
- **Accepted only whole:** the version is known, `release < press ≤ 1000` on
  every channel, reserved bits are 0. Otherwise the panel rejects it and keeps
  the config it has. It never applies part of one.
- Takes effect immediately, **RAM only by design**: panels never persist config;
  the master does. The panel builds the new config in a spare copy and swaps a
  pointer, so its sampling loop never runs on half of an update. Since version 2
  the swapped copy holds the raw-count thresholds at the current tares.
- The master sends it in a poll slot, with a reply window, when a panel
  appears, when a panel that matched reports "not configured" (it rebooted),
  when the master's copy changes, and as a re-push: one answering panel every
  ~111 ms, so each gets it about once a second (`docs/PANEL_CONFIG.md` → "As
  built").
- ~~Not in version 1: the rest baseline (the panel doesn't use it; it stays on
  the master/host side), the fallback timeout (fixed 100 ms), auto-calibration
  (deferred).~~ **Version 2 (2026-10-22):** thresholds in calibrated units; the
  panel tracks its own tares and turns them into raw counts, rebuilding them
  in a spare copy whenever a tare moves, the same pointer swap as a new `'W'`.
  Still not in the config: the fallback timeout (fixed 100 ms) and per-sensor
  full-scale endpoints (fixed 4095).

### `'w'` — config ack (panel → master, reply only) — built 2026-10-20, not yet run on a board

- addr = panel ID, len 6. Reply window (5 + 6) × 10 + 260 = 370 µs.

| Offset | Size | Field |
|---|---|---|
| 0 | 1 | status: 0 applied, 1 unknown version, 2 invalid values |
| 1 | 1 | highest config version this panel firmware speaks |
| 2 | 4 | CRC-32 (zlib) of the config **now in use**, serialised in `'W'`'s form |

- The CRC is computed from the panel's live config turned back into `'W'` form,
  so a match proves the panel runs exactly the master's values; the master
  doesn't have to trust the status byte alone. With the defaults on every
  channel (930/905, version 2) it is **`0x61F3A50A`** (the host test checks
  this against zlib); ~~`0xFD704831`~~ for version 1's 500/400.
- **No bus read-back command (`'R'`/`'r'`), on purpose:** the CRC makes it
  redundant for operation. Add one only if a mismatch ever needs diagnosing
  field by field. Reading config from the PC is a USB matter (`'G'` in
  `docs/USB_PROTOCOL.md`), answered from the master's copy.

### ~~`'C'` — set FSR thresholds (master → one panel)~~ — removed 2026-10-20

Decided 2026-10-20 and done the same day: the gameplay firmware no longer
answers it and the master no longer sends it. The bring-up firmware still
answers it; nothing sends it. Why: the master re-sends `'W'` about once a
second and is canonical, so a threshold set with `'C'` would be overwritten
within a second. The master's `S` console command now edits the master's copy
and pushes `'W'` instead. The description below is what `'C'` was.

- addr = panel ID, len 5
- Payload: `[channel]` (0–3, or `0xFF` = all) `[press u16 LE]` `[release u16 LE]`
- Applied to the live hysteresis logic immediately. **RAM only in v0** — panel
  reboot restores compile-time defaults. Flash persistence arrives with the full
  config system (see `docs/PANEL_CONFIG.md`).
- Panel acks with `'c'` echoing the payload it applied.

### `'I'` — identify / INT self-test (master → one panel)

- addr = panel ID, len 1
- Payload: `[duration_ms]`, 1–50 (panel clamps out-of-range; **2ms nominal**)
- Panel replies `'i'` **first**, then — after letting its transceiver release the
  bus — pulls its **INT line LOW for `duration_ms`** and releases it.
- Reply-then-pulse ordering is deliberate: the master gets an explicit "expect a
  pulse in the next N ms" window, so correlating the edge needs no guesswork.

### `'i'` — identify ack (panel → master, reply only)

- addr = panel ID, len 0

### `'H'` — status request (master → one panel)

- addr = panel ID, len 0. Added 2026-10-18 so a pad with no USB cable to any
  panel can still see each one's health through the master.
- Panel replies immediately with `'h'`. The master sends it in a poll slot,
  like `'W'`, so it gets a reply window.
- Since 2026-10-20 the master also sends it in the background, to one answering
  panel every 500 ms (each panel every ~4.5 s), and keeps the latest build tag,
  CRC error count and termination bit per panel for USB pad status and device
  info. Only the console's `H` sweep prints the replies.

### `'h'` — status (panel → master, reply only)

- addr = panel ID, len 24 from the gameplay firmware (since 2026-10-20), 22
  from the bring-up firmware, which has no parser timeout. The master accepts
  both. All multi-byte fields little-endian.

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | build tag: FNV-1a of the firmware's build timestamp. Same image, same tag |
| 4 | 4 | uptime, seconds |
| 8 | 1 | last reset cause: bit0 power-on, bit1 RUN pin, bit2 PSM restart (debugger), bit3 watchdog timeout, bit4 watchdog forced |
| 9 | 1 | state: bit0 12V present, bit1 terminated, bit2 auto-INT on, bit3 LED echo on, bit4 INT asserted, bits5–6 LED mode (gameplay firmware: 0 normal, 1 ID, 2 sensor test; always 0 on the bring-up build) |
| 10 | 2 | CRC errors (saturates at 0xFFFF) |
| 12 | 2 | UART errors (saturates) |
| 14 | 2 | RX ring overruns (saturates) |
| 16 | 2 | turnaround glitches (saturates) |
| 18 | 4 | frames received OK |
| 22 | 2 | parser timeouts: packets abandoned after a mid-packet idle gap over 500 µs (saturates; gameplay firmware only) |

Counters are since the responder was last switched on, or since the last `'Z'`. At 29 bytes on the
wire this is the longest reply, 290 µs; the master's reply window is now sized
per request (`(5 + reply payload) × 10 µs + 260 µs`), which keeps `'f'` at its
original 400 µs and gives `'h'` 550 µs.

### `'Z'` — zero counters (master → panels)

- addr = panel ID or broadcast, len 0. Added 2026-10-18. The panel zeroes every
  counter `'h'` reports, plus the ones its USB console prints (`T`). Uptime,
  build tag and reset cause are untouched.
- Addressed: the panel acks with `'z'` (len 0). Broadcast: no reply.
- The master's `z` command broadcasts it in a poll slot and zeroes its own bus
  counters at the same moment, so after a change to either side the next `H`
  and heartbeat describe only the time since. Before this, the only way to
  zero a panel was to reboot it.
- The bootloader does not know `'Z'` and ignores it.

### `'M'` — LED mode (master → panels)

- addr = panel ID or broadcast, len 1 or 2: `mode u8`, then `seconds u8`
  (0 or absent = until changed). Added 2026-10-18; gameplay firmware only, the
  bring-up build ignores it.
- Modes: **0 normal** (paint `'L'` frames), **1 ID** (white image of the
  panel's own position, from its DIP), **2 sensor test** (each edge lit in its
  own colour, brightness following pressure). Spec in `docs/PANEL_CONFIG.md` →
  "LED modes".
- A timed mode returns to the panel's base mode when it runs out. `'L'` frames
  keep arriving and are buffered, but don't show until the panel is back in
  normal mode.
- Addressed: the panel acks with `'m'` echoing the payload. Broadcast: no reply.
- The master's `m <mode> [secs]` broadcasts it in a poll slot, like `z`. The USB
  LED mode command does the same for all nine panels; for a subset it sends one
  addressed `'M'` per panel, each in its own slot with a reply window (built
  2026-10-20).
- Only the LEDs change. INT, `'F'` telemetry and every other command work as
  usual in all three modes.

### Firmware update — `'B'` `'Q'` `'U'` `'D'` `'A'` `'X'`

Added 2026-10-18 and specified in `docs/FIELD_UPDATE.md`. Only `'B'` reaches a
running application: it reboots the panel into its bootloader. The rest are
answered by the bootloader alone. Same framing. Broadcasts never get a reply,
and the only replies are `'b'` and `'q'`, to addressed requests.

## Slot ↔ panel-ID self-test

Two independent things claim to say which panel is which: the **physical slot**
(master INT input *N* is whatever panel is plugged into JST XH header *N*, J3–J11)
and the panel's **self-reported ID** (read from its DIP at boot, used as the
`addr` byte). Nothing guarantees they agree, and because **INT is the sole
gameplay press path**, a disagreement means presses register as the wrong arrow
— a silent bug that feels like "the pad is broken". `'I'` exists to learn the
true mapping empirically instead of assuming it.

**Master algorithm:**

1. **Idle pre-check** — all 9 INT inputs must read HIGH. A line already LOW is a
   stood-on panel, a stuck FSR, or a shorted wire; it cannot show a transition,
   so report it and stop rather than mis-attributing later pulses.
2. For each ID 0–8: send `'I'`, wait for `'i'`, then watch **all nine** inputs
   for a falling edge inside the expected window.
3. Require exactly one slot to fire, its LOW time to be ≈ `duration_ms`, and the
   line to return HIGH. Record slot ↔ ID.

**What each failure tells you:**

| Symptom | Meaning |
|---------|---------|
| `'i'` ack, edge on an unexpected slot | INT cable in the wrong header — the position label on the cable (`hardware/harness/WIRE_COLORS.md`) names which one to move |
| `'i'` ack, no edge on any slot | INT wire not landed, open, or the panel's open-drain GPIO is dead |
| `'i'` ack, **two** slots fire | two panels share a DIP ID — both answered the address and both pulsed |
| Edge fires but no `'i'` ack | panel lives on INT but not on RS-485: transceiver, termination, or A/B swapped |
| Neither ack nor edge | panel absent, unpowered, or dead |
| Pre-check finds a LOW line | stuck FSR / shorted INT wire, or someone is standing on the pad |

**It is also a continuity test of the input path.** The pulse uses the same
open-drain driver a real press uses, so a pass proves panel GPIO → wire →
master pull-up and RC filter → master input pin, end to end. That is the one
path where a fault costs gameplay latency or correctness, and it is otherwise
only exercised by pressing every panel by hand.

**Gating:** diagnostic only — run at bring-up and after any re-cabling, never
during gameplay. To the master an identify pulse is indistinguishable from a
press (that is the point), so the host must gate it: no HID input reporting
while a self-test runs. Keep the learned map in RAM and re-derive it each run;
persisting it risks caching a wrong mapping.

## Timing budget

At 1 Mbps ≈ 10µs/byte: LED frame 80 bytes = 0.8ms, poll 5 bytes + reply 14 bytes
≈ 0.2ms. The master sends LED frames at 60Hz (16.6ms). Nine panels:
0.8ms × 9 frames + 9 polls ≈ 9ms per cycle, the ~110Hz ceiling estimate in
`CLAUDE.md`. **As measured, the honest schedule polls at ~125/s**, i.e. ~14 Hz
telemetry per panel at nine panels (see "Every reply gets a guaranteed window"
below).

## Polling must be paced, not bursted (found 2026-07-11, multi-panel bring-up)

**Poll one panel per tick, never multiple panels back-to-back with no gap.**
Sending several `'F'` polls in immediate succession (e.g. a `for` loop over all
panels in one pass) leaves no window for a panel's reply to finish before the
master's next transmission starts. Since RS-485 is half-duplex, a panel replies
the instant it finishes parsing its own poll — it has no visibility into
whether the master is still mid-transmission to a different panel — so a
reply can collide with the master's own next poll (or with another panel's
receive/reply activity). This produced a consistent ~20% poll success rate
and heavy CRC errors across a whole bus once a second panel was genuinely
replying, despite every individual wiring/electrical cause (duplicate ID,
termination, A/B routing, shared ground) being ruled out and fixed first —
round-robining one poll per `POLL_INTERVAL_MS` tick (5ms, vs. ~150µs for a
full poll-reply round trip) resolved it completely: 100% replies, 0 CRC
errors sustained. This scales fine to 9 panels (9 × 5ms ≈ 45ms per full
sweep, still far above what's needed for telemetry) but the pacing
requirement must be kept in mind if poll timing is ever revisited for speed.

## Every reply gets a guaranteed window (found 2026-10-02, master #2 + two panels)

**Pacing alone was not enough.** A 5 ms poll interval only protects a reply if
the schedule is honest, and the master's `loop()` was not. Two faults, the
second hidden behind the first:

1. **`'C'` went out whenever `S` was typed**, at an arbitrary point inside a
   millisecond, so the next frame/poll tick could fire microseconds later on top
   of the `'c'` ack.
2. **The schedule was computed from a stale clock.** `millis()` was read once at
   the top of `loop()`. The nine-panel `'L'` burst (9 × 80 bytes into Teensy's
   40-byte TX buffer) blocks inside `write()` for ~7 ms, and the poll after it
   set `next_poll_ms = now_ms + 5` from the pre-burst time, already in the past,
   so the next request went straight out behind the previous one.

The rule now, master side (`firmware/master/master.ino`, `sendRequest()`):

- **Any packet that expects a reply (`'F'`, `'W'`, `'H'`, addressed `'M'`;
  `'C'` until 2026-10-20) is followed by a window of master silence**, measured from transmit-complete (`flush()` returns
  when DE drops). The LED burst and the next poll both wait for it. It was a
  flat 400 µs, covering `'f'` (14 bytes = 140 µs) plus panel turnaround; since
  2026-10-18 it is sized per request as `(5 + reply payload) × 10 µs + 260 µs`,
  which is still 400 µs for `'f'` and 550 µs for the 29-byte `'h'`.
- **`'W'`, `'H'` and every other master-initiated command take a poll slot**
  instead of being sent from the command handler (`'C'` did, until it was
  removed).
- **The clock is re-read after the LED burst**, and the next poll is scheduled
  from the time it was actually sent.

**Anything sent outside `loop()`'s schedule waits for the bus to go quiet first**
(`waitBusQuiet()`, 2026-10-19). Pausing traffic stops *new* frames but not what
is already on the wire: the reply to the last poll, or a queued LED burst. The
`I` self-test sent its first `'I'` on top of an `'f'` reply and lost IDs in 6 of
10 runs; it now flushes TX and waits out the reply window, and so does `P`
(passthrough). 0 failures in 30 runs after the fix.

Measured before/after on the same bench: 9/20 `'c'` acks with 11 panel CRC and
~400 uart errors across both panels → **120/120 acks, 0 CRC, 0 uart, 0
turnaround** on both panels. Details: `docs/BRINGUP_LOG.md` → master #2.

⚠ **Measured consequence, deliberately not fixed yet:** the honest schedule polls
at **~125/s, not the 200/s `POLL_INTERVAL_MS` implies**. The `'L'` burst occupies
~7 ms of every 16 ms frame period (~45% of the bus), which leaves room for two
polls per frame period. The old ~188/s was inflated by the very back-to-back
polls that collided. At 9 panels that is **~14 Hz telemetry per panel**, a
~72 ms sweep rather than the 45 ms estimated above. Not on the gameplay path
(presses travel on INT). Interleaving polls into the LED burst is the obvious
next step, and it is deferred until all nine panels are on the bus.

**Expected from 2026-10-20, not measured:** the config re-push (9 `'W'`/s) and
the background `'H'` (2/s) take about 11 of those ~125 slots a second, which
would leave ~114 polls, **~12.7 Hz per panel**.

## Future idea: slotted broadcast poll (not implemented, thinking point)

Instead of individually addressing each panel's `'F'` poll, the master could
broadcast one "everyone report" command, and each panel replies in its own
pre-assigned time slot (`wait panel_id × slot_duration after the broadcast,
then reply`) rather than waiting to be individually asked.

- **Feasible, and simpler than it first sounds**: the master's packet parser
  is already self-describing (every reply carries its own `addr` byte), so
  the master does NOT need advance knowledge of exactly which panels exist —
  it just needs a fixed worst-case timeout for the whole sweep. An absent
  panel's slot is simply silent; nothing else needs to change.
- **Real payoff**: current paced round-robin polling wastes most of each
  5ms slot as collision-avoidance margin (real poll+reply only takes
  ~150-200µs). A tight slotted scheme could shrink a 9-panel telemetry sweep
  from ~45ms to ~2-3ms — 15-20x — freeing bus headroom for higher LED refresh
  rate or future protocol additions.
- **Real new risk**: this reintroduces a timing-precision-dependent collision
  possibility (one panel's reply bleeding into a neighbor's slot if its clock/
  timer isn't precise enough) — the same *class* of bug as the unpaced-polling
  collision above, just from a different cause. Given how much effort that one
  cost to track down, this needs a hardware timer/alarm for the per-panel
  delay (not a busy-wait, to avoid jitter from other Core 1 work), and careful
  testing before trusting it.
- **Not urgent**: current telemetry rates are already far faster than needed
  (FSR press-latency doesn't depend on this poll at all — that's the dedicated
  INT wire's job; this poll is purely for telemetry/diagnostics). Revisit only
  if bus bandwidth headroom becomes an actual constraint.

