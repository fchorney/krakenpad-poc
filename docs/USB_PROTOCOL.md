# USB Protocol (ours) — v0 design draft

Status: **draft.** Derived by walking the stock SMX USB protocol
(`../stepmaniax-sdk-mp/docs/USB_PROTOCOL.md`) command-by-command and deciding
keep / adapt / drop. Intentionally NOT SMX-compatible — our own VID/PID and
descriptor; no software expecting an SMX pad will (or should) talk to us.

## Device identification

| Property | Value |
|----------|-------|
| VID/PID | Teensyduino default (0x16C0 + USB-Type-dependent PID) during development. Plan: register a real PID under pid.codes' shared VID 0x1209 (see below) once eligible. |
| Product string | "KrakenPad" — **working name, not final**, may still change |
| Transport | USB HID, 64-byte packets |
| USB speed | High Speed 480 Mbps (Teensy 4.x native) |
| Input polling | 1000Hz baseline; 2000Hz+ target (HS microframes allow up to 8000Hz) |

### VID/PID plan: pid.codes

Decided 2026-07-10: register via [pid.codes](https://pid.codes) (free, public,
the standard registry for open-source hobbyist USB hardware — VID 0x1209
shared across many projects, e.g. QMK keyboards) rather than purchasing a
vendor ID or using unregistered numbers indefinitely.

**Not eligible yet** — pid.codes requires a public repo with modifiable design
files (PCB sources and/or firmware) under a recognized open-source license
(LICENSE file required). The repo now lives at
`github.com/fchorney/krakenpad-poc` with active commits (as of 2026-07-11),
but there is **still no LICENSE file**, and the repo must be public for
eligibility. Revisit once both are true.

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

### Input state report

- 16-bit little-endian bitmask, bits 0–8 = panels 0–8 (same layout as stock)
- **Adapt:** sent on every state change (from the INT lines — no bus-cycle
  latency) plus heartbeat. Add a monotonic 8-bit sequence number so the host
  can detect missed transitions between polls.

## Command inventory (from stock → ours)

### Keep (with cleanups)

| Stock | Ours | Notes |
|-------|------|-------|
| Device info (flag 0x80) | keep | version, serial, capabilities; add per-panel presence bitmap + termination check result (see MODULAR_PANEL_COUNT) |
| `'G'` get config | keep | our config format (see PANEL_CONFIG.md); single format, no v4/v5 legacy split |
| `'W'` set config | keep | ack + read-back verify like stock SDK does |
| `'f'` factory reset | keep | resets config + animation slots (magic-byte redetect, defaults rewritten) |
| `'C'` force recalibration | keep | master forwards to panels over RS-485 |
| `'L'` platform strip | keep | underglow, master-local, write-on-command (see UNDERGLOW.md) |
| `'m'`+`'d'` animation upload | adapt | upload `.smxa` to panel flash slots. Ack-based flow control over RS-485 replaces the blind `'d'` delay hack; no interleaved-across-panels dance, no send-twice-for-reliability |
| `'y'` sensor test | adapt | clean telemetry request: raw / calibrated / noise / baseline. Plain per-panel arrays — stock's bit-interleaving was an artifact of its parallel signal-wire readout; our RS-485 replies make it unnecessary |

### Replace

| Stock | Problem | Ours |
|-------|---------|------|
| `'4'`/`'2'`/`'3'` LED triple | legacy split (16-LED grid + v4 inner 3×3 bolt-on), 0.6666 color scaling, 30 FPS cap | **one full-frame command**: 9 panels × 25 LEDs × RGB = 675 bytes, fragmented (~12 packets). No color scaling (panel gamma/brightness policy lives panel-side). Rate cap = LED refresh target (60Hz+), newest-frame-wins like stock |
| `'t'` panel test mode | firmware-side pressure lighting demo | drop from firmware — host tool renders pressure lighting from telemetry it already gets (`tools/fsr_monitor.py` is the prototype of this) |

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

- Exact report IDs / descriptor layout (decide during HID implementation)
- Whether FSR streaming uses the data report or a dedicated input-style report
- Config split: which fields live master-side vs panel-side (see PANEL_CONFIG.md)
- 2000Hz+ polling: HS bInterval choice (0.5ms vs 0.25ms vs 0.125ms) after
  measuring real host-side behavior
