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

## Panel PCB — where things stand (updated 2026-07-20)

**Layout is essentially complete; the board is not yet ordered.** It reached
order-ready on 2026-07-18, then an external human schematic review on
2026-07-19 reopened the design deliberately — nothing had been paid for, so
review items originally parked for "rev B" were pulled forward into rev A.

- **Schematic v1.0 signed off 2026-07-11**, but has moved since; the current
  authoritative pin map lives in the review-response and status docs rather
  than the table in `docs/PANEL_SCHEMATIC_PLAN.md`.
- **Layout done**: placement, routing, GND pour with 270 stitching vias,
  mounting holes validated by 1:1 printout fit-test, silkscreen pass complete
  (including "Kraken Pad by SenPi / Rev. 1.0" and the personal logo as exposed
  copper).
- **Fab package generated 2026-07-18** in `hardware/panel-pcb/production/`
  (untracked, regenerable via `kicad-cli`) and **quoted at JLCPCB**: $240.78
  for qty 5, of which ~$148 is qty-independent overhead — marginal assembled
  board is about $17.50. Not ordered.

### Changes since that fab package — it is now STALE

- **THVD1419 → THVD1429** (2026-07-19). The single most important find: the
  1419 is TI's **250 kbps** speed grade and the bus runs at **1 Mbps**. It
  never showed on the bench because the prototype used a MAX3485. Identical
  pinout and package, and cheaper. Surfaced by the review's "complete your
  MPNs" nitpick.
- **LM66200 ideal-diode mux (U8)** replaces the D12/D23 Schottky power-OR,
  removing the VF drop that left the shifter rail at ~4.7V against a 4.5V
  minimum. D12/D23 remain as DNP fallback footprints.
- **52 MPN + 22 Datasheet properties** added, all verified against live LCSC
  listings; **rail and net names standardized** (`+12VDC`/`+5VDC`/`+3.3VDC`,
  `RS485_TX`, `INT_OUT`, `FSR_North`, …) as one synced pass, netlist-diffed to
  prove zero connectivity change.
- **J9 INT terminal 1P → 2P** (true 1P KF301 barely exists), new custom
  footprint, re-placed and DRC-verified.
- **12 test points SMD pads → THT probe holes** — note this changes the drill
  file, not just the gerbers.

### Before this can be ordered

1. **Update PCB from Schematic** to pull in U8, the THT test points, and the
   net renames; re-point zones at the renamed rails, refill, save (the saved
   file has stale zone fills).
2. **Resolve 3 logo-vs-GND clearance DRC errors** — exposed-copper logo polys
   sit 0.08–0.12mm from the pour. Ground the logo, keepout, or exclude.
3. **Regenerate the whole production package** including the drill file.
4. **Optional but wanted: bench-drive a WS2815 strip at 5V.** The reviewer's
   only Critical finding (that WS2815 data needs a 12V shift) was rebutted from
   the datasheet — VIH is an absolute 2.7V min and 12V would violate the 5.7V
   abs-max — but we have only ever hands-on tested WS2812B.
5. Physical part verification per `docs/PRE_ORDER_CHECKLIST.md` (FSR PHR-2
   plug, SW1 row spacing, J9 drill/pad, U8 package).

## Master PCB — where things stand (updated 2026-07-20)

**Schematic is fully wired and verified; no PCB layout started yet.**

- All 28 symbols placed and wired, **ERC 0 errors** (4 `lib_symbol_mismatch`
  warnings on the 74AHCT125 units are the known cached-symbol noise class).
- Every net checked pin-by-pin against a `kicad-cli` netlist export.
- **J3 + J4 merged** into one 2-position screw terminal (J4): pin 1 =
  underglow DATA, pin 2 = the mandatory GND tie to the PSU ground stud.
- Status LED on GPIO17 with its resistor on the anode side.
- **DNP RS-485 bias pair (R4/R5, 390R 1%)** added per review item 3.a — bias
  belongs at one point on a bus, and the master is that point, if the
  THVD1429's integrated failsafe ever proves insufficient.
- Rails named `+3.3VDC` and `+5VDC_USB`, matching the panel convention.
- Still open: MPNs for the 10k resistor array, the 9-pos Euroblock, and the
  3-bit DIP; and the custom Teensy 4.0 symbol still needs verifying against
  the PJRC pinout card, plus a matching footprint, before layout.

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

1. **Panel PCB revision session** — the immediate next task: Update PCB from
   Schematic (U8 + THT test points + net renames), re-point and refill zones,
   eyeball J9's orientation (its wire entry now faces board-left), decide the
   logo-vs-GND clearance question, and check whether the two 0402s have room
   to become 0603.
2. Remaining schematic notes: the FSR divider explanation (review 2.a) and the
   THVD1429 integrated-failsafe rationale (3.a); draw SW3 as a proper DPDT
   (4.h).
3. Regenerate the production package and re-run the pre-order checklist.
4. WS2815 strip bench test at 5V.
5. Send the reviewer the 4.x replies (1.a/2.a/3.a/3.b already sent).
6. Full pad + harness teardown: underglow splice point, PSU stud size, real
   cable run lengths.
7. Master PCB layout (~80×60mm) once the Teensy footprint is settled.
8. Master firmware: USB HID reports to the PC (RS-485 + INT already work —
   this is the one major master-side piece not started).
9. Panel firmware: flash-backed animation playback + config storage /
   per-channel calibration (`docs/PANEL_CONFIG.md`).
10. Extend `stepmaniax-gif-maker` to export `.smxa` binary format.
