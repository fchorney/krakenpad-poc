# Project status

**Last updated 2026-10-22.** This is the one page for "where is the project right
now". Update it alongside any change that moves it (rules in `CLAUDE.md`).
Detail lives in the docs it links.

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
| Harness | — | **both pads** (2026-10-12) | **installed in the bench pad, 2026-10-22**: the stock electronics are out and ours are in |

Also in progress: the master enclosure (`hardware/Master-Case/`, 3D-print
models).

## What works on the bench (master #2 + nine panels, 2026-10-20)

- **Press path:** FSR → panel threshold → INT wire → master ISR → **USB joystick**
  (9 buttons, 8 kHz offered at High Speed). Checked on a Mac with
  `tools/hid_monitor.py`.
- **RS-485 bus, nine panels:** 0 errors in steady state; `I` slot ↔ ID self-test
  9/9; parser recovers from a broken packet (`b` test, 2026-10-20); `H` status from every panel; termination placement check after each `H`.
- **Bus firmware update** to all nine at once (`tools/panel_update.py`),
  including recovery from a 12 V cut mid-update. **The bootloader updates over
  the bus too** (`tools/panel_boot_update.py`, 2026-10-20): all nine migrated to
  stage 0 + A/B slots, bootloader tag `C5234B1B`.
- **Panel LED modes:** master `'L'` frames, the ID image, and the on-panel sensor
  test, switchable from the master with `m`. LED stutter fixed 2026-10-19.
- **Underglow:** all 44 groups addressable from bench commands (byte order BRG).
  Installed in the pad 2026-10-22; the `g` chase looked right there.
- **Config on the bus, the USB vendor interface and the Rust SDK's `monitor`**
  (first run 2026-10-20, evening, nobody at the bench): every panel matches the
  master's copy, edits/save/reload/reset work, every `pad_cmd.py` command
  answers, telemetry **12.6 Hz per panel**, a save stalls up to **~35 ms**
  (sector compaction). Steps needing a hand or an eye still to do (below).

## Firmware: built vs not built

| | Built | Not built |
|---|---|---|
| **Panel** (`firmware/panel/c/main.c`) | Core 0 FSR loop with hysteresis + persistence driving INT; core 1 bus responder (`F I W H Z M L B`) and LED rendering; LED modes 0/1/2; DIP 10 standalone sensor test; runs behind the bus-update bootloader. **Built 2026-10-20, runs on all nine (2026-10-20):** config from the master's `'W'` (thresholds and `populated`/`enabled` per channel, RAM only by design), the "not configured" bit in `'f'`; `'C'` removed | Local animations (normal mode goes **dark** when `'L'` stops); calibration / drift tracking; DIP diagnostic codes 9, 11, 12 (13 dropped 2026-10-20) |
| **Master** (`firmware/master/master.ino`, build with `build.sh`) | Bus master (LED frames + paced polling), INT capture, USB joystick report, bench console, update passthrough `P`, self-test, status sweep, termination check (the meter-only underglow test `U` removed 2026-10-22). **Built 2026-10-20, run on master #2 the same evening (hands-on steps still to do):** the panels' config (the only saved copy, two-slot EEPROM record, pushes + ~1 Hz re-push, per-panel state; console `c` `S` `F` `save` `reset config`), and the **USB vendor interface**: the whole command table except underglow and reset animations, telemetry stream, host LED frames, USB bus passthrough | Underglow periodic re-send and host strip command; reset animations; poll interleaving (telemetry 12.6 Hz/panel measured with the re-push, 2026-10-20) |
| **Host tools** (`tools/`) | `panel_update.py`, `panel_boot_update.py`, `fsr_monitor.py`, `hid_monitor.py`, `swd_check.py`, `usb_speed_test.py`, `bom_census.py`, `gen_bom.py`, `gen_review_images.py`, `gen_stencil.py`; **`pad_cmd.py`** (every USB vendor command, 2026-10-20, run against master #2). **Rust SDK skeleton** (`software/sdk`, crate `krakenpad`, 2026-10-20): framing, pad discovery, commands, events, telemetry snapshot; the `monitor` example runs against master #2 (2026-10-20) | The SDK's C interface (`krakenpad-c`), updates through the SDK, and the apps (**Rust + egui**). Decided 2026-10-20 (`docs/HOST_SOFTWARE.md`) |

Specs for the unbuilt parts: `docs/PANEL_CONFIG.md`, `docs/USB_PROTOCOL.md`,
`docs/ANIMATIONS.md`, `docs/UNDERGLOW.md`. A future idea, not scheduled:
`docs/TIMESTAMPED_INPUT.md`.

## Known open items

- **Config and the USB vendor interface: all passed except presses on four
  panels.** In the pad (2026-10-22), presses passed on the five with a sensor
  (U, L, C, R, D); **UL, UR, DL and DR are still to test**, with sensors
  borrowed from the other pad for the test. The `F` press test, a panel 12 V cut, a bus cable pull
  (USB events and the SDK), the LED commands and `w` passed at the bench on
  2026-10-21 (`docs/BRINGUP_LOG.md`).
- **Small nits found 2026-10-21, fixed the same evening:** the duplicate-ID
  warning no longer fires when a panel drops out (rates capped at 100%); the
  SDK `monitor` shows a missing panel as `gone`; the Python tools open the pad
  shared on macOS. **Still to test: that a real duplicate ID still warns.**
  Two programs *driving* the pad at once can mix up responses (one command at
  a time, matched by transaction ID); watching alongside a game is fine.
- **`pad_cmd.py bus-h` missed a reply twice** in its first ~15 runs, then 0 in
  ~250 since, including 150 after random commands (2026-10-21). Cause
  unknown. The master now logs what each passthrough session heard, so a
  repeat will say which side lost the reply (`docs/BRINGUP_LOG.md`).
- **Host LED frames, fixed 2026-10-21 at High Speed:** 12 × 64-byte writes
  took ~9 ms per frame with the bus running; protocol 2's one 1,024-byte
  packet takes 0.70 ms. At Full Speed it is still 15.5 ms (no headroom at
  60 Hz). The master's loop still leaves its RawHID buffers full for ~3 ms at
  a time while the bus runs; it matters only at Full Speed now
  (`docs/BRINGUP_LOG.md`).
- **The master's two EEPROM slots share the emulation's flash sectors**, so a
  power cut during a sector compaction can lose both: the result is defaults
  plus "config was reset", never a wrong config. Found reading the core
  (`docs/PANEL_CONFIG.md` → "As built").
- **Connect USB before 12 V** on any panel. A powered brain back-drives its VBUS,
  so a host won't attach (rev-2 candidate #3).
- **Bootloader-over-bus, six checks not yet run on a board** (V1–V6 in
  `docs/FIELD_UPDATE.md` → "Validation"): the USB image, both BOOTSEL
  fallbacks, SWD restore, power cuts mid-update and mid-conversion. V1–V4 go
  with the first new board's bring-up (`docs/BUILD_CHECKLIST.md` step 2). V7,
  a cold 12 V boot, passed 2026-10-20.
- **Master turnaround glitch:** probably absent. No stray byte reached the
  master's receive buffer after 161 of its own transmissions (2026-10-21,
  passthrough logging), and the core keeps bytes with framing errors. No
  framing-error count yet (`docs/MASTER_BRINGUP.md`).
- **Not yet measured:** the joystick in a real game (ITGmania), the host-side
  achieved poll rate, and the report rate under many simultaneous presses.
- **Not yet tested:** cable pulls and panel power cycles during play.
- **11 panels still to assemble** (`docs/BUILD_CHECKLIST.md`).
- **Harness, from the install (2026-10-22):** almost every cable is a little
  long (INT quite long); RS-485 master → `UL` (60 cm) is too short; the INT
  labels under heat-shrink make the panel end too stiff to plug in easily; the
  unjacketed ends should be longer. New lengths and a label method are TODO
  (`hardware/harness/README.md` → "Install findings").
- ~~**Harness install:** column feeds chosen, trunk, master GND tie and
  underglow leads cut to fit~~ done 2026-10-22, 12 V chain as designed.
- **Underglow `u` may not work in the pad:** the user remembers pressing it
  and seeing nothing (2026-10-22); `g` worked. Retest live.
- **Scope the RS-485 bus once the panels are in a pad** (edge settling; carried
  from master #2's open list in `docs/BRINGUP_LOG.md`).

## Next steps (firmware/software)

> **Moved into the pad 2026-10-22** (the user: everything worked so far;
> harness findings in `hardware/harness/README.md` → "Install findings").
> **NEXT:** presses on UL, UR, DL, DR (borrowed sensors; `hid_monitor.py`,
> the SDK `monitor`), retest `u`, the real-duplicate-ID warning check, and the
> installed-pad items below (harness lengths, bus scope, FSR logging).
>
> **The 2026-10-20 bench procedure is done** (2026-10-21), except presses on
> UL, UR, DL and DR (five of nine passed in the pad, 2026-10-22).
> The parser resync passed on the current master and failed on the old one, as
> designed. Results: `docs/BRINGUP_LOG.md`.

**The user's plan (2026-10-20): (a) config on the bus, (b) the master's vendor
HID interface, (c) the Rust SDK skeleton in `software/sdk` against (b)**
(`docs/HOST_SOFTWARE.md`). **All three were built 2026-10-20, plus the master's
parser resync.** All ran on the pad (2026-10-20 and 21), apart from presses on
every panel. (b) covers
the whole command table except underglow and reset animations (the user's
call). Presses on every panel wait for the installed pad.
The SDK's C interface (`krakenpad-c`) isn't needed yet (the user, 2026-10-20).
How the SDK and firmware share values after the repos split is open
(`docs/HOST_SOFTWARE.md` → "Not decided").

1. ~~Inter-byte timeout in the panel parser~~ done 2026-10-20: master `b`
   test fails on the old firmware, passes on the new; `I` 10/10 with traffic.
2. Play-test the joystick in ITGmania; measure host-side rate.
3. Robustness: cable pull, panel power cycle, poll/LED interleave under load.
4. Config: **built 2026-10-20, passed on the pad 2026-10-20/21 (presses on every panel wait for the installed pad).** Panels RAM-only; the
   master's two-slot EEPROM record, `'W'`/`'w'` and the "not configured" bit
   (`docs/RS485_PROTOCOL.md`), the version check, reset config, console `S`/`F`
   editing the master's copy with a separate `save` (the user's call,
   2026-10-20). Shared format: `firmware/shared/panel_config.h`.
   Auto-calibration deferred until installed-pad data.
5. USB vendor interface: **built 2026-10-20, passed on the pad 2026-10-20/21 (presses on every panel wait for the installed pad).** Packet
   framing, the command table except underglow (deferred) and reset animations
   (waits on step 6), the telemetry stream, host LED frames (hand back after
   1 s, the user's call), the bus passthrough for updates, reboot to the Teensy
   bootloader (`docs/USB_PROTOCOL.md` → "As built"). Not built: the SDK side of
   updates (porting `panel_update.py`), and writing master firmware over HID.
6. Animations: flash slots, pressed/released playback, conversion in the upload
   program. APNG as the format, whole-frame per-frame delays, flag-row loop markers,
   length+CRC and the gif-maker fork decided 2026-10-20; max LED rate, ms vs
   frame units, loop counts and the panel layout still open
   (`docs/ANIMATIONS.md` → "Decisions and open questions").
7. **Bigger USB reports at High Speed, 64 at Full Speed:** decided and **built
   2026-10-21 (protocol 2)**, run at both speeds on master #2: an LED frame
   0.70 ms at High Speed, 15.5 ms at Full Speed (`docs/USB_PROTOCOL.md`). Device
   info carries the speed and both sizes for the config app's connection
   status. Still open: what Full Speed drops or degrades, from more tests
   (`FULL_SPEED=1 build.sh` forces it); Windows and Linux not tried.
8. Underglow: periodic re-send, then the host command. (Deferred by the user on
   2026-10-08: noted, not to be built yet.)
9. **Telemetry rate: find how fast polling can go without losing correctness**
   (added 2026-10-22, the user: "part of another test at some point"). The
   12.6 Hz per panel is set by the scheduler, not the bus.
   `POLL_INTERVAL_MS = 5` caps polling at 200/s, about 22 Hz per panel even
   with no LED traffic. The 60 Hz `'L'` burst then blocks `write()` for ~7 ms
   per frame. A poll actually costs ~0.6 ms (request, `'f'` reply, silence
   window), and since 2026-10-02 the guaranteed reply window is what protects
   replies, not the interval. **Estimated, not measured:** a 1 ms interval would
   give roughly 60 Hz per panel at 60 Hz LEDs. Interleaving polls into the LED
   burst, or a TX path that doesn't block, would go further.
   `docs/RS485_PROTOCOL.md` deferred interleaving "until all nine panels are on
   the bus", and they have been since the install. Pass criteria to set: 0 CRC,
   0 timeouts, every poll answered, LED frame rate held, over a long run (fits
   long-run test 3).

## Long-run tests (planned, none run yet)

Recorded 2026-10-20 from the user's memory of an earlier chat; the full list
from that chat was lost. Duration, conditions and pass criteria are **TODO**
for each. Results go in `docs/BRINGUP_LOG.md`. The longest run so far is the
~120 s steady-state bus check (2026-10-19).

1. **Full-white soak, all nine panels: first run done 2026-10-20.** Master
   console `w` toggles it. Boards only slightly warm; **~3.4 A measured**
   (two meters), ~12% under the model; `H` steady state at full white 0
   errors on all nine (`docs/BRINGUP_LOG.md`). Bus errors after a 12 V power-up
   are panels booting into live traffic: random panels each time, none with
   traffic paused, and they stop once every panel is up. Still to do: a
   re-run **under the trunk fuse**, which isn't fitted yet.
2. **Thermal.** Includes `U303` at real load, which so far has only been checked
   as "barely warm" on 12 V alone, on one board (`docs/DUAL_PANEL.md`).
3. **Overnight bus soak.** Watch the `H` error counters and the master's
   poll-answered count.
4. **Installed-pad FSR logging** (added 2026-10-20): once the panels are in
   the pad (sensors on U, L, C, R, D), log every sensor with
   `tools/fsr_monitor.py` for the resting value under preload, through an hour
   of play, from a cold start and in a warm room. It decides the
   auto-calibration design (`docs/PANEL_CONFIG.md` → "Auto-calibration").
5. Some of the open items above may also have been on the list: cable pulls and
   power cycles during play, and polling plus LED frames under load.

## Where things are

| Need | Doc |
|---|---|
| Architecture, conventions, the one-line rev-2 index | `CLAUDE.md` |
| Panel as built: pin maps, interface, parts, mechanical | `docs/DUAL_PANEL.md` |
| Master as built | `docs/MASTER_PCB.md` |
| Bring-up procedure / per-board results | `docs/PANEL_BRINGUP.md`, `docs/MASTER_BRINGUP.md` / `docs/BRINGUP_LOG.md` |
| Building more boards | `docs/BUILD_CHECKLIST.md` |
| Bus protocol / USB protocol / bus updates | `docs/RS485_PROTOCOL.md` / `docs/USB_PROTOCOL.md` / `docs/FIELD_UPDATE.md` |
| Host SDK and apps (languages, decisions) | `docs/HOST_SOFTWARE.md` |
| Harness as built | `hardware/harness/` |
| Rev-2 candidates / how to order a revision | `docs/REV2_CANDIDATES.md` / `docs/FAB_ORDER.md` |
| How rev 1 was verified, ordered and bought | `docs/rev1-order/` |
| Exact files that were fabricated | `hardware/fab-archive/rev1-2026-08-18/` |
| Tool traps (KiCad, pcbnew, picotool, toolchains) | `docs/DEV_NOTES.md` |
| The stock SMX pad, as found | `stock-smx/` |
| Superseded designs and their reasoning | `docs/archive/` |
