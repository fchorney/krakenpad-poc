# Project Status / Next Steps

Living doc — updated at the end of each work session so it's easy to pick
back up. See CLAUDE.md for full architecture, and the topic docs in `docs/`
for detail on any item below.

## What's proven and working (as of 2026-07-16)

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
- **Power cascade bench-verified (2026-07-16)**: AMS1117-5.0 → 3.3V-stage
  cascade run from 12V under realistic load (2× Pico + 2× 5V logic chips);
  3.3V stage cool, 5V stage mildly warm. Findings carry over to the AP7361C
  swap (lighter-duty part; meter-check when real chips arrive).
- **Tooling**: full CLI flashing workflow (`arduino-cli`, `picotool`) for
  both boards, `tools/fsr_monitor.py` (live per-panel FSR bars + threshold
  tuning), `tools/usb_speed_test.py`. KiCad scripting via the app-bundled
  `python3.9` (`pcbnew`) + `kicad-cli` for DRC/netlist checks.

## Panel PCB — where things stand

- **Schematic v1.0 signed off 2026-07-11** (full pre-layout audit passed;
  "KrakenPad Panel PCB v1.0.pdf" in `hardware/`). Current authoritative refs:
  table at the top of `docs/PANEL_SCHEMATIC_PLAN.md`.
- **Layout in progress**: placement done (LED lattice at refined caliper
  measurements, stock-style connector tabs on the outline), USB/RS-485/
  crystal/QSPI routed and length-matched, power/FSR routing rebuilt clean,
  JLC DRC rules calibrated. General routing cleanup still in progress.
- **Part swaps applied 2026-07-16 (sch + PCB + BOM, uncommitted)**: U6 →
  AP7361C-33ER-13 (`-33ER-` suffix only — plain `-33E-` is pin-reversed),
  D12/D23 → PMEG3015EH, C38 → 22µF tantalum, C37+C52 → 2× 10µF 0805 MLCC,
  C44/C50 → 10µF 0805 MLCC, C51 → SMD V-chip 470µF electrolytic. Pending GUI
  pass: re-route the 6 cap sites, run Update PCB from Schematic (expect field
  syncs only), verify RVT1E471M1010 pad fit.
- **Remaining before fab**: finish routing cleanup, bottom GND pour +
  stitching vias (deliberately last), silkscreen pass, verify which edges
  have the 7mm mounting-hole inset against the frame, verify J9 KF301
  footprint drill/pad vs the sourced part, verify a PHR-2 plug against a real
  FSR lead.

## Open design questions (not blocking, revisit when relevant)

- **Underglow harness splice point**: electrical spec fully resolved (12V
  native, 44 groups-of-3, master outputs DATA only — see `docs/UNDERGLOW.md`),
  but the stock leads crimp directly into a 12-pin Dupont housing at the old
  MCU with no reusable intermediate connector — cleanest splice point needs
  the full pad/harness teardown. Gates the master PCB's underglow connector.
- **Slotted broadcast polling**: possible future protocol optimization
  (15-20x tighter telemetry sweep) — not needed now. See
  `docs/RS485_PROTOCOL.md`.
- **PSU stud size + real harness run lengths** — measure at teardown, feeds
  BOM lug/wire quantities.

## Naming / branding

Working project name: **"KrakenPad"** (not final, open to more
workshopping). Logo idea: circuit-trace-style kraken, echoing the master's
single connector fanning out to 9 panel connectors like tentacles. VID/PID
plan is pid.codes — repo now lives at `github.com/fchorney/krakenpad-poc`
with active commits, but still needs a LICENSE file (and public visibility)
for eligibility.

## Concrete next steps (pick from here)

1. Panel PCB: finish the LDO-swap cleanup (6 cap re-routes, Update PCB from
   Schematic, RVT1E471M1010 pad-fit check), then continue routing cleanup.
2. Panel PCB: bottom GND pour + via stitching as the final layout step, then
   silkscreen pass and fab-readiness DRC.
3. Commit the in-flight sch/PCB/BOM changes (LDO/diode/cap swap work).
4. Full pad + harness teardown: underglow splice point, PSU stud size, real
   cable run lengths.
5. Start KiCad master PCB design (~80×60mm).
6. Master firmware: USB HID reports to the PC (RS-485 + INT are working,
   this is the one major master-side piece not yet started).
7. Panel firmware: flash-backed animation playback + flash config storage /
   per-channel calibration (design notes in `docs/PANEL_CONFIG.md`).
8. Extend `stepmaniax-gif-maker` to export `.smxa` binary format.
