# RS-485 Bus Protocol (v0 — prototype)

Status: **v0, prototype bring-up.** Minimal framing + two commands. Config
write/read, calibration, and multi-panel LED addressing come later; they slot in
as new command bytes with the same framing.

## Physical layer

- UART **1 Mbps, 8N1**, half-duplex RS-485 (MAX3485 / THVD1419)
- Master drives the bus by default; a panel transmits only when replying to a
  poll addressed to it
- DE/~RE tied together per transceiver:
  - Teensy: `Serial1.transmitterEnable(pin)` — hardware manages DE automatically
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

## Commands

### `'L'` — LED frame (master → panels)

- addr = panel ID (per-panel frame) or `0xFF` broadcast, len 75
- Payload: 25 × RGB triplets, **serpentine LED order** (LED index i at bytes
  3i, 3i+1, 3i+2 = R, G, B)
- Panel displays the frame on receipt. Remote frames take priority over local
  animations; panel falls back to local animation if no `'L'` frame arrives
  within **100ms**.
- Multi-panel: **per-panel addressed `'L'` frames** (addr = panel ID) — working
  as of the 2-panel bench bring-up 2026-07-11. A single big broadcast with
  per-panel offsets was the alternative considered; not needed at current bus
  utilization (~6% single-panel, ~9ms full 9-panel cycle).

### `'F'` — FSR poll (master → one panel)

- addr = panel ID, len 0
- Panel replies immediately with `'f'`.

### `'f'` — FSR data (panel → master, reply only)

- addr = panel ID, len 9
- Payload: 4 × uint16 little-endian raw ADC values (channels 0–3), then 1 byte
  pressed bitmask (bit i = channel i pressed, post-hysteresis/persistence)

### `'C'` — set FSR thresholds (master → one panel)

- addr = panel ID, len 5
- Payload: `[channel]` (0–3, or `0xFF` = all) `[press u16 LE]` `[release u16 LE]`
- Applied to the live hysteresis logic immediately. **RAM only in v0** — panel
  reboot restores compile-time defaults. Flash persistence arrives with the full
  config system (see `docs/PANEL_CONFIG.md`).
- Panel acks with `'c'` echoing the payload it applied.

## Timing budget (v0, single panel)

At 1 Mbps ≈ 10µs/byte: LED frame 80 bytes = 0.8ms, poll 5 bytes + reply 14 bytes
≈ 0.2ms. Master cycles at 60Hz (16.6ms) — bus is ~6% utilized. Nine panels:
0.8ms × 9 frames + 9 polls ≈ 9ms per cycle, matching the ~110Hz ceiling estimate
in CLAUDE.md.

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

## Prototype pin map

| Role | Teensy 4.0 | Pico |
|------|-----------|------|
| TX | 1 (TX1) | GPIO0 (UART0 TX) |
| RX | 0 (RX1) | GPIO1 (UART0 RX) |
| DE/~RE | 2 (auto via transmitterEnable) | GPIO2 (manual) |

Panel ID is read once at boot from GPIO6 (internal pull-down): floating = panel
0, jumpered to the Pico's own 3V3 OUT = panel 1. Bench-test stand-in for the
final PCB's 4-position DIP switch (0–8) — a single pin only distinguishes two
units, doesn't scale past that.
