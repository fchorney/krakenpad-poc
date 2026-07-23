# Project Status / Next Steps

Living doc — updated at the end of each work session so it's easy to pick
back up. See `CLAUDE.md` for architecture; topic docs in `docs/` for detail.
Last updated **2026-07-23** (doc reorganization pass — see note at bottom).

## What's proven and working on the bench

- **Full sensor path**: FSR → ADC (hysteresis + persistence filter, no
  chatter) → open-drain INT wire → Teensy interrupt, sub-millisecond,
  validated end to end.
- **Multi-panel RS-485 bus**: 2 Picos + 1 Teensy, 1 Mbps, 100% poll replies,
  0 CRC errors sustained; per-panel LED frames + FSR telemetry + live
  threshold tuning (`docs/RS485_PROTOCOL.md`).
- **USB High Speed confirmed** on Teensy 4.0 — 8000Hz HID polling achievable
  off-the-shelf (`bInterval=1`).
- **ADC mux crosstalk fixed in hardware** — 10nF caps per FSR channel,
  bench-verified, on the panel PCB.
- **Power cascade bench-verified** from 12V under realistic load.
- **Tooling**: CLI flashing for both MCUs, `tools/fsr_monitor.py`,
  `tools/usb_speed_test.py`, KiCad scripting via bundled python + `kicad-cli`.

## Panel PCB — complete, not ordered

Schematic + layout done; **ERC 0 / DRC 0 with no exclusions** (all former
exclusions eliminated at the cause, 2026-07-21). As-built reference:
`docs/PANEL_PCB.md`. Incorporates the full 2026-07-19 human-review rework
(THVD1429, LM66200 power-OR, net/rail renames, J9 → MRR522 2-pos, 12V-sense
divider + D29 clamp).

Before ordering (`docs/PRE_ORDER_CHECKLIST.md` is the authority):

1. **Regenerate the production package** — `hardware/panel-pcb/production/`
   is **STALE** (changes since the 2026-07-18 export include a drill-count
   change from the THT test points, so the drill file must be regenerated
   too).
2. Physical part verification (FSR PHR-2 mate, J9/SW1/SW3 vs sourced parts,
   U8 package).
3. Optional insurance: **bench-drive a WS2815 strip at 5V** (only WS2812B has
   been hands-on tested; datasheet closes the question on paper).

Reference quote (2026-07-18, qty 5): $240.78, ~$148 of it qty-independent →
marginal assembled board ≈ $17.50. Order all boards in one run (need 18+2 →
order 20).

## Master PCB — complete, not ordered

Schematic + layout done, ERC/DRC clean, committed. As-built reference:
`docs/MASTER_PCB.md`. Notable as-builts: RS-485 on **Serial2** (GPIO 7/8),
INT on GPIO 15–23 with **J2 position 1 = panel 8 (DR) … position 9 = panel 0
(UL)**, 9× discrete SMAJ5.0A TVS, hand-assembly only (no PCBA).

## Reviews

- 2026-07-19 human schematic review: fully triaged and folded in
  (`docs/archive/REVIEW_RESPONSES_2026-07-19.md`).
- **2026-07-23: fresh external AI review packets sent for BOTH boards**
  (bundles in `tmp/review-2026-07-23/`) — triage findings when they return.

## Sourcing / BOM

`docs/BOM.md` (merged 2026-07-23) is the single sourcing doc: quantities for
the 2-pad build, priced DigiKey cart snapshot ($559.86 CAD), AliExpress
candidates with match-checks, THT hand-solder analysis. Open sourcing items:
FSR JST headers (B2B-PH-K, ×80) need a source; wire lengths are placeholders
until the pad harness is measured.

## Open design questions (not blocking)

- **Underglow harness splice point** — needs the full pad/harness teardown
  (`docs/UNDERGLOW.md`); also PSU stud size + real harness run lengths.
- **Master INT filter caps** (~1nF per line, under the socket) — leaning yes,
  not on the board yet.
- **Slotted broadcast polling** — future protocol optimization
  (`docs/RS485_PROTOCOL.md`).

## Naming / branding

Working name **"KrakenPad"** (not final). Repo: `github.com/fchorney/krakenpad-poc`.
pid.codes VID/PID registration still gated on a LICENSE file + public repo.
Project logo pending artwork (personal logo already on the panel silk as
exposed copper).

## Concrete next steps (pick from here)

1. Triage the 2026-07-23 AI review findings for both boards.
2. Run `docs/PRE_ORDER_CHECKLIST.md` → regenerate production files → order
   panels (JLC PCBA) + master bare boards.
3. Place the parts orders (`docs/BOM.md`) — AliExpress match-checks first,
   DigiKey fallback.
4. WS2815 strip bench test at 5V.
5. Full pad + harness teardown: underglow splice, PSU stud size, cable runs.
6. Master firmware: USB HID reports to the PC (the one major master piece not
   started).
7. Panel firmware: flash-backed animation playback + config storage
   (`docs/PANEL_CONFIG.md`, `docs/ANIMATIONS.md`).
8. Extend `stepmaniax-gif-maker` to export `.smxa`.

---

**Doc reorganization 2026-07-23:** merged `ANIMATION_FORMAT` +
`ANIMATION_BINARY_FORMAT` + `PROTOTYPE_LED_LAYOUT` → `ANIMATIONS.md`;
`STOCK_PANEL_CHIPS` + `STOCK_PANEL_PCB_MEASUREMENTS` →
`STOCK_PANEL_REFERENCE.md`; `BOM` + `DIGIKEY_SHOPPING_LIST` + `BOM_PRICED` →
`BOM.md`; `PANEL_SCHEMATIC_PLAN` + `PANEL_PCB_LAYOUT_NOTES` → `PANEL_PCB.md`
(as-built); `MASTER_SCHEMATIC_PLAN` → `MASTER_PCB.md` (as-built). Historical
docs moved to `docs/archive/`. Old content is all in git history.
