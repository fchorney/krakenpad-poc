# Field update — flashing panels over RS-485

**Status (2026-10-18): implemented and passes an end-to-end host test. On
hardware, the USB load, the hand-off to the app and the `U` hold work on one
board. A bus update has NOT been run yet.** The pieces:

| piece | where |
|---|---|
| panel bootloader | `firmware/panel/c/bootloader/` |
| flash map + command letters, shared | `firmware/panel/c/common/fw_layout.h` |
| app linker script (app at `0x10008000`) | `firmware/panel/c/common/memmap_app.ld` |
| app side: `'B'` on the bus, `U` on the console | `firmware/panel/c/bringup/main.c` |
| master passthrough (`P`) | `firmware/master/master.ino` |
| host tool | `tools/panel_update.py` |
| host test (real bootloader C, simulated bus) | `firmware/panel/c/bootloader/host_test/` |

The original 2026-07-27 specification is in
`docs/archive/FIELD_UPDATE_SPEC_2026-07-27.md`. Its reasoning on XIP, sizing and
the flash map still holds; "What changed" below lists where this differs.

## What it does

With the master on USB and every panel on the bus, one command updates them all:

```sh
python3 tools/panel_update.py firmware/panel/c/bringup/build/panel_bringup.bin
```

The image is broadcast once, so a whole pad takes about as long as one panel.
Nothing is installed anywhere until **every** participating panel holds a
verified copy, so the pad never ends up half old and half new.

## Flash map

```
0x000000 – 0x006FFF   28 KB  bootloader (boot2 + code; copied to RAM at boot)
0x007000 – 0x007FFF    4 KB  boot-control record
0x008000 – 0x07FFFF  480 KB  application
0x080000 – 0x0FFFFF  512 KB  released animation slot
0x100000 – 0x17FFFF  512 KB  pressed animation slot
0x180000 – 0x1FFFFF  512 KB  staging (incoming image)
0x200000 – 0x27FFFF  512 KB  golden copy — reserved, not implemented
0x280000 – 0x3FFFFF          reserved
```

The application is the SDK's normal layout moved to `0x10008000`: its own
256-byte boot2 slot comes first (unused) and its vector table sits at
`0x10008100`. The bootloader jumps through that table exactly as boot2 would.
**An application linked this way cannot boot without the bootloader.** The
bring-up build links this way by default; `-DPANEL_UNDER_BOOTLOADER=OFF` builds
the old standalone image.

The bootloader build fails if it grows past 28 KB, because it would run into
the boot-control record. It is 12.5 KB today.

⚠ OpenOCD 0.11 treats the flash as 2 MB (`docs/PANEL_BRINGUP.md` → SWD), which
covers the bootloader, the app and staging. The golden region and everything
above `0x200000` are out of its reach.

## Boot sequence

1. **Resume.** If the boot-control record says an install was in progress and
   staging still verifies, redo the copy. Staging is never written during a
   copy, so this always has a good source.
2. **Hold if asked or if there is nothing runnable.** An application that
   received `'B'` writes `FW_HOLD_MAGIC` to watchdog scratch[0] and reboots;
   the bootloader sees it and stays. It also stays if the app's vector table
   is implausible, or if this bootloader installed the app and its CRC-32 no
   longer matches.
3. **Listen 200 ms for `'B'`** on every boot. Heard → hold. Silence → run the app.

So a panel whose application cannot run, or runs but never answers the bus,
is still reachable: broadcast `'B'` continuously and power-cycle the pad.

While holding, **`D202` blinks 2 Hz**. Receiving is 8 Hz, staged or installed is
solid, and any failure is a 16 Hz flicker. Erasing and copying hold it solid.

## Protocol

Same framing as `docs/RS485_PROTOCOL.md`. **Broadcast commands never get a
reply**: nine panels answering at once would collide. Every reply is to an
addressed `'Q'` or `'B'`.

| cmd | from → to | payload | reply |
|---|---|---|---|
| `'B'` | master → app or bootloader | — | `'b'` if addressed. The app reboots into the bootloader; a bootloader stays |
| `'Q'` | master → one bootloader | — | `'q'`, below |
| `'U'` | master → bootloader(s) | `len u32`, `crc32 u32` | none. Erases enough of staging, then starts receiving. Over 480 KB → `REFUSED` |
| `'D'` | master → bootloader(s) | `block u16`, ≤64 bytes | none. Block *n* is image bytes `64n…64n+63` |
| `'A'` | master → bootloader(s) | `crc32 u32` | none. If staged and the CRC matches: copy to the app region and verify |
| `'X'` | master → bootloader(s) | — | none. Discard staging; run the app if it is valid, else keep holding |

`'q'` payload (16 bytes + up to 16 missing-block indices):

| offset | size | field |
|---|---|---|
| 0 | 1 | state: 0 idle, 2 receiving, 3 staged, 4 CRC fail, 5 refused, 6 installed, 7 copy fail |
| 1 | 1 | bit0 app vectors plausible, bit1 install pending |
| 2 | 2 | blocks still missing |
| 4 | 4 | installed app length (0 = loaded over USB/SWD, unknown) |
| 8 | 4 | installed app CRC-32 |
| 12 | 4 | bootloader build tag |
| 16 | 2 × n | first missing block indices, n ≤ 16 |

CRC-32 is the standard zlib one, so the host's `zlib.crc32()` matches. The
installed app's CRC-32 serves as its version: the tool skips panels that
already run the image.

### Erase and copy are deaf, by design

A panel erasing staging (`'U'`), checking the CRC (last `'D'`) or copying
(`'A'`) is busy for up to seconds. Its UART keeps filling the RAM ring, but
when it finishes **it discards everything that arrived meanwhile**. A `'Q'`
answered late would land on top of whatever the master sent next. So the host
polls with short timeouts and simply retries until the panel answers.

The bootloader is `copy_to_ram`, so flash operations never stall its code, and
the ring is 32 KB. The original spec's DMA ring and RAM-resident update loop
were needed for receiving inside a running app; a RAM-only bootloader does not
need them.

### Writing blocks

Blocks are 64 bytes and flash programs 256-byte pages, so the bootloader
collects a page and programs it when it is complete or the stream moves on. A
page programmed with holes keeps `0xFF` in them, and a gap block re-sent later
is programmed into the same page. That works because NOR programming only
clears bits. The host test checks this path by dropping frames.

## The update, step by step (`tools/panel_update.py`)

1. **Passthrough.** `P` on the master. From then on USB carries only frames: the
   master forwards host frames to the bus and CRC-valid bus frames to the host,
   and its own LED frames, polls and text output stop. `cmd 0x00` frames are
   for the master: `addr 0` leaves, `addr 1` pings. It also leaves after 30 s
   with nothing from the host.
2. **Hold.** Broadcast `'B'` every 20 ms for 1 s. Running apps reboot into the
   bootloader and catch a later `'B'` in their listen window.
3. **Survey.** `'Q'` each panel ID. Absent IDs are reported and skipped. If every
   present panel already runs this CRC, release them with `'X'` and stop
   (`--force` overrides).
4. **Begin.** Broadcast `'U'`, then poll until each target reports receiving.
   A panel still idle gets an addressed `'U'`.
5. **Stream.** Broadcast every `'D'` block once.
6. **Gap-fill.** `'Q'` each target. Re-send the blocks it lists, addressed to
   it; over 256 missing, re-stream everything. Abort if a panel's missing count
   stops falling. Repeat until all are staged and CRC-verified.
7. **Commit.** Broadcast `'A'`; poll until each reports installed.
8. **Run.** Broadcast `'X'`. Every panel starts the new application.

**A failure before step 7 sends `'X'`**: staging is discarded and every panel
restarts the application it already had. **A failure during or after step 7
does not**: some panels may have installed and some not, so all of them stay
held in the bootloader (blinking) until the tool is run again.

Before streaming, the tool refuses an image that is not an application linked
at `0x10008000`: over 480 KB, an initial SP outside SRAM, or a reset vector
outside the app region. The last check is what catches a standalone build,
which would install fine and then never run.

## Getting a panel onto it — once, over USB

This is the last time each board needs a cable. A blank board is already in
BOOTSEL, so skip the first line. A board running an application goes there from
software, so the BOOTSEL button is never needed:

```sh
picotool reboot -f -u && sleep 3
picotool load firmware/panel/c/bootloader/build/panel_bootloader.uf2
picotool load firmware/panel/c/bringup/build/panel_bringup.uf2
picotool reboot
```

Check that the banner prints `image : at 0x10008000, behind panel_bootloader`.
The build tag on the banner is the same on every board flashed from the same
files. First done 2026-10-18 on `DE6558A6977D462D`, where `U` then held it in the
bootloader as described above.

**Bootloader first.** Loading only the app leaves the old image's start at
`0x10000000` with its tail overwritten by the new app, which crashes. If a
board does that and is still in BOOTSEL, load the bootloader and it recovers.
If it has left BOOTSEL, use SWD (`docs/PANEL_BRINGUP.md` → SWD).

Three traps, all hit or found on 2026-10-18:

- **Without the pause, the bootloader load finds no board.** After `reboot -f -u`
  the board takes a moment to re-enumerate as BOOTSEL. It printed
  `No accessible RP-series devices in BOOTSEL mode were found`, and the next
  command loaded the app on its own. If the first load prints that, stop and run
  it again before you load the app.
- **No `-x` on the app.** It writes the flash and then fails with
  `Cannot execute as file does not contain a valid RP2 executable image`,
  because the app is not a standalone image. Use `picotool reboot` instead.
- **Never use `load -f` for the bootloader.** `-f` reboots the board into
  application mode after the load. On a board that still has the old standalone
  image, the bootloader then finds the middle of that image at `0x10008000`,
  rejects it and holds. The bootloader has no USB, so from then on only the
  BOOTSEL button or SWD can reach the board.

Building:

```sh
export PICO_SDK_PATH=~/pico-sdk
PATH=/Applications/ArmGNUToolchain/15.2.rel1/arm-none-eabi/bin:$PATH \
  cmake -S firmware/panel/c/bootloader -B firmware/panel/c/bootloader/build -DPICO_BOARD=pico
PATH=/Applications/ArmGNUToolchain/15.2.rel1/arm-none-eabi/bin:$PATH \
  cmake --build firmware/panel/c/bootloader/build -j8
```

The app only hears `'B'` when its responder is on, which in the bring-up
firmware means bus mode: 12V present and the DIP on 0–8. On the bench, `U` on
the panel's USB console reboots it into the bootloader too.

## Host test

```sh
python3 firmware/panel/c/bootloader/host_test/test_update.py APP.bin STANDALONE.bin
```

It compiles the real `bootloader/main.c` for the Mac against stub SDK headers:
flash is a RAM array that only clears bits, and the UART is stdin/stdout. It
runs `tools/panel_update.py` against three simulated panels through a fake
master that drops 3% of `'D'` frames. Checks: the update lands on every panel
(app region CRC, boot record), no panel ever speaks unaddressed, a second run
finds nothing to do, a standalone image is refused, an install cut off by
power loss is redone at the next boot, and `--panels 1` leaves panels 0 and 2
untouched down to their staging slots.

**First run 2026-10-18: all pass.** To confirm the test can fail, the bootloader
was broken on purpose by removing the page buffer's reset to `0xFF`. The update
then aborted on a CRC failure, the test failed, and the change was reverted.

**`--panels` did not select anything until 2026-10-18.** The tool broadcast
`'U'` and `'A'`, and `'B'` holds every panel, so every panel on the bus took the
update. Found on the bench: `--panels 0` installed the image on all eight
boards. `'U'` and `'A'` now go to each target by address; `'D'` stays broadcast,
since a panel that got no `'U'` is not receiving and drops it. The host test
missed this. The simulator loses blocks even with the drop rate at zero (about
60 of 885 on an unpaced stream, and as many when paced to 1 Mbps), and the tool
gap-fills only its targets, so a non-target never finished staging and never
installed. Scenario 5 now checks each non-target's staging slot, which `'U'`
erases. It fails against the old tool and passes against the fix.

What it cannot show: bus timing, the real UART and IRQ, real flash timing, the
SDK's runtime coming up after the hand-off, and the master's USB behaviour.
Those need hardware.

## Hardware bring-up, not yet run

1. ✅ **Done 2026-10-18** (`DE6558A6977D462D`, build tag `09447A32`). Load the
   bootloader and the new bring-up image on one panel. It should boot to the
   banner, which now prints `image : at 0x10008000`. This is the first time the
   SDK runtime was seen coming up after the bootloader's hand-off.
2. ✅ **Done 2026-10-18**, same board. `U` on its console: `D202` blinks 2 Hz.
   `X` from the master, or a power cycle without `'B'`, brings the app back.
3. ✅ **Done 2026-10-18**, master #2 and eight panels (ID 8 not built). The
   first `--panels 0` run installed on **all eight**, which is how the
   `--panels` bug below was found. After the fix, `--panels 7` changed panel 7
   only (the other seven stayed on the old CRC) and a full run updated the
   rest. 884 blocks stream in 0.7 s with no gap-fill; a 400 KB image takes 4.6 s.
4. ✅ **Done 2026-10-18**, panel 7, both power-cut cases:
   - **12V cut during the staging erase** (by accident: D202 is solid then
     too). The tool timed out waiting for `receiving`, aborted before commit
     and sent `'X'`. Panel 7 came back on its old app, untouched.
   - **12V cut during the commit.** A 56 KB copy is too short to hit by hand,
     so the image was zero-padded to 400 KB (the app region holds 480 KB; the
     padding sits past the program and never runs). 12V was pulled right after
     `committing`. The tool aborted with `no installed from panel(s) [7]`. On
     re-power **D202 was solid for a few seconds, then the app booted**, and
     the next survey showed panel 7 on the new CRC.

   ⚠ **D202 solid does not mean "installing".** The bootloader holds it solid
   while erasing, staged, installing and installed; it blinks fast while
   receiving and slowly while idle. To cut power mid-commit, go by the tool's
   `committing` line.

## What changed from the 2026-07-27 spec

All decided 2026-10-18.

- **DECIDED 2026-10-18: the bootloader receives the image, not the
  application**, because the code then exists once and works under every
  future app. The spec's version needed the receiver twice anyway: in the app,
  and in the bootloader for the broken-app recovery path. The cost is that
  panels reboot for an update, and the LEDs go dark for it. The user noted that
  an in-app receiver could show progress on the panel LEDs; not useful yet,
  revisit if wanted.
- **DECIDED 2026-10-18: golden copy and DIP-14 restore deferred.** The 200 ms
  listen window already recovers any app that will not run or will not talk.
  Golden only adds recovery without a PC. The flash stays reserved.
- **DECIDED 2026-10-18: the update logic runs on the host**, with the master as a
  passthrough. Changing retries or timeouts then needs no Teensy reflash. It
  will move into the planned SDK / config application;
  `tools/panel_update.py` is written as a module (`PanelBus`,
  `PanelUpdater`) for that reason.
- **No replies to broadcasts.** The spec had `'u'` and `'a'` acks to broadcast
  commands; nine panels would collide. The host polls `'Q'` instead.
- **`'A'` installs and the panel keeps holding, then `'X'` runs it.** The spec's
  `'A'` rebooted straight into the new app, which would make confirmation
  depend on that app's bus support. Now the bootloader confirms the install
  itself, and the tool can check every panel before any of them runs it.
- **`'U'` dropped `target` and `version`.** Firmware only for now; animations
  will get their own command when the animation slots exist. The installed
  CRC-32 replaces the version number.
- **Block index is u16** (7680 blocks fit) and **`'q'` carries the installed CRC
  and the bootloader build tag.**
