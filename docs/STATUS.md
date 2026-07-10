# Project Status / Next Steps

Living doc — updated at the end of each work session so it's easy to pick
back up. See CLAUDE.md for full architecture, and the topic docs in `docs/`
for detail on any item below.

## What's proven and working (as of 2026-07-11)

- **Full sensor path**: FSR → ADC (hysteresis + persistence filter, no
  chatter) → open-drain INT wire → Teensy interrupt, sub-millisecond,
  validated end to end.
- **Multi-panel RS-485 bus**: 2 Picos + 1 Teensy, 1 Mbps, **100% poll
  replies, 0 CRC errors** sustained. Per-panel addressing works for both LED
  frames (`'L'`) and FSR telemetry (`'F'`/`'f'`), plus live threshold tuning
  over the bus (`'C'`/`'c'`).
- **USB High Speed confirmed** on Teensy 4.0 — 8000Hz HID polling is
  achievable off-the-shelf (`bInterval=1`), not custom stack work as
  originally assumed.
- **ADC mux crosstalk fixed in hardware** — 10nF caps per FSR channel,
  bench-verified, required on the final panel PCB.
- **Tooling**: full CLI flashing workflow (`arduino-cli`, `picotool`) for
  both boards, `tools/fsr_monitor.py` (live per-panel FSR bars + threshold
  tuning), `tools/usb_speed_test.py`.

## Bugs found and fixed this session (bus bring-up)

In order, all confirmed root-caused: duplicate panel ID (GPIO6 pull-down
jumper fix) → wrong resistor value (10kΩ misread as 120Ω) → missing shared
ground between the Teensy's and Picos' separate breadboards → **unpaced
back-to-back polling causing real half-duplex bus collisions** (the actual
root cause of the persistent ~20% success rate that survived the first three
fixes). Full writeup: `docs/RS485_PROTOCOL.md`.

## Known hardware gaps to fix before KiCad

- **AP2112K-3.3 can't take 12V input** (6.5V abs max) — swap for AMS1117-3.3
  or MCP1804 on the panel PCB's 12V→3.3V rail. Not yet bench-tested.
- **Panel PCB is missing a 5V rail** for the SN74AHCT125N level shifter's
  VCC — needs a second small linear regulator (12V→5V, low current, e.g.
  AMS1117-5.0). See CLAUDE.md Power section.
- 10nF caps per FSR ADC channel (see above) — remember for panel PCB layout.

## Open design questions (not blocking, revisit when relevant)

- **Underglow strips**: optional feature, may be dropped if it doesn't fit
  cleanly. Leading plan is to reuse the stock Daygreen B15-1224-05 converter
  (confirmed standalone, reusable) for power, adding only a data line via a
  spare level-shifter channel. Still needs teardown to confirm LED family
  (5V vs 12V) and exact strip wiring. See `docs/UNDERGLOW.md`.
- **Panel LED family**: WS2815 is the working default; APA102/SK9822 (SPI
  clocked) is a documented alternative worth a look before final panel PCB
  layout, since it directly serves the project's latency/refresh-rate goals
  better than WS281x's fixed-timing protocol. See `docs/LED_OPTIONS.md`.
- **Slotted broadcast polling**: a possible future protocol optimization
  (15-20x tighter telemetry sweep) — not needed now, current rates are far
  beyond what's required. See `docs/RS485_PROTOCOL.md`.
- **RS-485 multi-panel LED addressing**: resolved in practice (addressed
  `'L'` frames work), doc still has an old TBD note to clean up.

## Naming / branding

Working project name: **"KrakenPad"** (not final, open to more
workshopping). Logo idea: circuit-trace-style kraken, echoing the master's
single connector fanning out to 9 panel connectors like tentacles. VID/PID
plan is pid.codes (free open-source hobbyist registry) — blocked on this
repo having a public remote + LICENSE file, neither of which exist yet.

## Concrete next steps (pick from here)

1. Bench-test the 12V→3.3V regulator swap (AMS1117-3.3 breakout module,
   powering a Pico from 12V) before finalizing the panel PCB's power section.
2. Underglow teardown: confirm LED family, strip wiring, connector pinout.
3. Decide panel LED family (WS2815 vs APA102) before panel PCB layout starts.
4. Start KiCad panel PCB design (5×5", connectors + regulators already
   mostly resolved — see CLAUDE.md).
5. Start KiCad master PCB design (~80×60mm).
6. Design flash config storage + calibration (placeholder notes exist in
   `docs/PANEL_CONFIG.md`; `'C'` command's RAM-only pattern is the prototype
   of the eventual flash-backed version).
7. Panel firmware: flash-backed animation playback (Core 1 currently only
   does the built-in chase demo + remote frames).
8. Master firmware: USB HID reports to the PC (RS-485 + INT are working,
   this is the one major master-side piece not yet started).
