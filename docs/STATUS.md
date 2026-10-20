# Project status

**Last updated 2026-10-20.** This is the one page for "where is the project right
now". Update it at the end of a work session. Detail lives in the docs it links.

## The hardware in use is rev 1

Both boards were ordered from JLCPCB on 2026-08-18 (tag `rev1-fab`) and are
frozen. **Rev 1 is the current, working hardware.** Every board change is a rev-2
candidate (`docs/REV2_CANDIDATES.md`). Firmware, docs, harness and tooling are not
frozen.

**Build scope: two pads** = 2 masters + 20 panel assemblies (2 × 9 + 2 spares).
Settled; every quantity in the project is sized to it.

| | fabbed | assembled | brought up |
|---|---|---|---|
| Panel (carrier + brain) | 20 | **9** | 9, through every stage plus SWD (`docs/BRINGUP_LOG.md`) |
| Master | 5 | **2** (master #1, #2) | both. Master #2 runs the bench pad |
| Harness | — | **both pads** (2026-10-12) | the bench pad's cabling is in use |

Also in progress: the master enclosure (`hardware/Master-Case/`, 3D-print
models).

## What works on the bench (master #2 + nine panels, 2026-10-19)

- **Press path:** FSR → panel threshold → INT wire → master ISR → **USB joystick**
  (9 buttons, 8 kHz offered at High Speed). Checked on a Mac with
  `tools/hid_monitor.py`.
- **RS-485 bus, nine panels:** 0 errors in steady state; `I` slot ↔ ID self-test
  9/9; `H` status from every panel; termination placement check after each `H`.
- **Bus firmware update** to all nine at once (`tools/panel_update.py`),
  including recovery from a 12 V cut mid-update.
- **Panel LED modes:** master `'L'` frames, the ID image, and the on-panel sensor
  test, switchable from the master with `m`. LED stutter fixed 2026-10-19.
- **Underglow:** all 44 groups addressable from bench commands (byte order BRG).

## Firmware: built vs not built

| | Built | Not built |
|---|---|---|
| **Panel** (`firmware/panel/c/main.c`) | Core 0 FSR loop with hysteresis + persistence driving INT; core 1 bus responder (`F I C H Z M L B`) and LED rendering; LED modes 0/1/2; DIP 10 standalone sensor test; runs behind the bus-update bootloader | Config in flash (thresholds are **RAM only**); local animations (normal mode goes **dark** when `'L'` stops); calibration / drift tracking; `populated`/`enabled` flags; DIP diagnostic codes 9, 11, 12, 13 |
| **Master** (`firmware/master/master.ino`, build with `build.sh`) | Bus master (LED frames + paced polling), INT capture, USB joystick report, bench console, update passthrough `P`, self-test, status sweep, termination check | **USB command and data reports** (live FSR telemetry and config over HID); underglow periodic re-send and host strip command; poll interleaving (telemetry is ~14 Hz/panel) |
| **Host tools** (`tools/`) | `panel_update.py`, `fsr_monitor.py`, `hid_monitor.py`, `swd_check.py`, `usb_speed_test.py`, `bom_census.py`, `gen_bom.py`, `gen_review_images.py`, `gen_stencil.py` | A config / SDK application (planned home for the update logic) |

Specs for the unbuilt parts: `docs/PANEL_CONFIG.md`, `docs/USB_PROTOCOL.md`,
`docs/ANIMATIONS.md`, `docs/UNDERGLOW.md`. A future idea, not scheduled:
`docs/TIMESTAMPED_INPUT.md`.

## Known open items

- **Connect USB before 12 V** on any panel. A powered brain back-drives its VBUS,
  so a host won't attach (rev-2 candidate #3).
- **Panel RS-485 parser has no inter-byte timeout.** Unconfirmed suspect for IDs
  lost later in the same `I` run, after a collision (`docs/BRINGUP_LOG.md`, master
  #2, 2026-10-19).
- **Master turnaround glitch:** whether the master's RX sees the same false start
  bit the panel did has never been measured (`docs/MASTER_BRINGUP.md`).
- **Not yet measured:** the joystick in a real game (ITGmania), the host-side
  achieved poll rate, and the report rate under many simultaneous presses.
- **Not yet tested:** cable pulls and panel power cycles during play.
- **11 panels still to assemble** (`docs/BUILD_CHECKLIST.md`).
- **Harness install, last recorded open 2026-10-12:** choose which 6 power
  cables are the column feeds; cut the trunk, master GND tie and underglow leads
  to fit once the master and the Wago carrier are mounted.
- **Carried since 2026-10-02:** scope `TP214` (`LED_DATA_5V`) during a 12 V
  pull; scope the RS-485 bus once the panels are in a pad.

## Next steps (firmware/software)

1. Inter-byte timeout in the panel parser, then re-run the nine-panel `I` test.
2. Play-test the joystick in ITGmania; measure host-side rate.
3. Robustness: cable pull, panel power cycle, poll/LED interleave under load.
4. Config in flash: write the protocol spec first (`docs/PANEL_CONFIG.md` →
   "Open questions"); calibration and the USB command report depend on it.
5. USB command and data reports: live telemetry and config for host software.
6. Animations: `.smxa`, flash slots, pressed/released playback, converter.
7. Underglow: periodic re-send, then the host command. (Deferred by the user on
   2026-10-08: noted, not to be built yet.)

## Where things are

| Need | Doc |
|---|---|
| Architecture, conventions, the one-line rev-2 index | `CLAUDE.md` |
| Panel as built: pin maps, interface, parts, mechanical | `docs/DUAL_PANEL.md` |
| Master as built | `docs/MASTER_PCB.md` |
| Bring-up procedure / per-board results | `docs/PANEL_BRINGUP.md`, `docs/MASTER_BRINGUP.md` / `docs/BRINGUP_LOG.md` |
| Building more boards | `docs/BUILD_CHECKLIST.md` |
| Bus protocol / USB protocol / bus updates | `docs/RS485_PROTOCOL.md` / `docs/USB_PROTOCOL.md` / `docs/FIELD_UPDATE.md` |
| Harness as built | `hardware/harness/` |
| Rev-2 candidates / how to order a revision | `docs/REV2_CANDIDATES.md` / `docs/FAB_ORDER.md` |
| How rev 1 was verified, ordered and bought | `docs/rev1-order/` |
| Exact files that were fabricated | `hardware/fab-archive/rev1-2026-08-18/` |
| Tool traps (KiCad, pcbnew, picotool, toolchains) | `docs/DEV_NOTES.md` |
| The stock SMX pad, as found | `stock-smx/` |
| Superseded designs and their reasoning | `docs/archive/` |
