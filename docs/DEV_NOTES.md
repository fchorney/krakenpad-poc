# Development notes — tools and their traps

Tool behaviour that has cost time on this project, on this machine (macOS,
arm64). Project-specific procedures live in their own docs: flashing panels in
`docs/PANEL_BRINGUP.md`, the master build in `docs/MASTER_BRINGUP.md`, fab
output in `docs/FAB_ORDER.md`.

## KiCad

- **Version: KiCad 10.0.4.** `docs.kicad.org` has a tree per version (`/8.0/`,
  `/10.0/`, …). Check menu paths, tooltips and shortcuts against `/10.0/`, or
  against the running app. Older docs' wording often doesn't match.
- **`kicad-cli` mutates `.kicad_pro` / `.kicad_sym` as a side effect.** Run checks
  on a scratch copy of the whole project set (`.kicad_pro`, `.kicad_dru`,
  `.kicad_prl`, `fp-lib-table`, `.pretty`), or `${KIPRJMOD}` won't resolve and
  the counts change.
- **`kicad-cli pcb drc` refills zones in memory only.** The fill on disk stays
  stale. Refill and save explicitly when the saved copper must be right (see
  below).
- **CLI clean but the GUI complains ⇒ suspect the GUI's in-memory state.** After
  a crash the GUI reported "library not enabled" for every project footprint
  while every headless check passed. Toggling the library in Preferences fixed
  it; nothing on disk was wrong. Restart or toggle before investigating files.
- **The user `fp-lib-table` / `sym-lib-table` is a table of tables** in KiCad 10:
  one `(type "Table")` entry pointing into the app bundle. Grepping it for a
  nickname finds nothing, and that is not evidence of a missing library.
- **Version tags differ by file type.** Schematics carry `20260306`, symbol
  libraries `20241209`. A schematic version tag pasted into a `.kicad_sym` breaks
  library loading.
- **`lib_footprint_mismatch` on rotated instances is a false positive** in
  10.0.4 (seen on 180° WS2815s and −90° 0603s; contents verified identical).
- **A custom `.kicad_dru` rule replaces the netclass value; it doesn't act as a
  floor.** See `docs/DUAL_PANEL.md` → "Known open items".

### pcbnew from Python (headless)

KiCad's bundled interpreter has a working `pcbnew` module (board model, zone
filler, connectivity, no GUI):

```
/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3
```

KiKit is not on its path: prefix `PYTHONPATH=~/.kikit`.

- `via.GetWidth()` asserts in v10. Pass a layer: `v.GetWidth(v.GetLayer())`.
- `conn.GetConnectedItems(item, flags)` takes an **int** for flags, and follows
  connections through zones, so on GND everything looks connected. Test
  track-to-via contact geometrically instead.
- **Don't `Remove()` items and refill in the same process.** The second
  `ZONE_FILLER.Fill()` crashes silently. Remove → `Save(new_path)`, then load
  that file in a fresh process. Log to a file with `buffering=1`, since stdout
  is lost on abort.
- swig's "memory leak of type 'PCB_VIA *'" after `Remove()` is harmless noise.
- `board.GetNetsByName().keys()` yields `wxString`, which never compares equal
  to the `str` from `GetNetname()`. Normalise with `str()`.
- There is no `DRC_ENGINE` in the bindings. Custom rules can only be honoured by
  parsing the `.kicad_dru` text (strip `#` comments; the stock file ships
  alternative rule sets commented out).
- v10's hole-to-hole setting is `m_HoleToHoleMin` (`m_MinHoleSeparation` does not
  exist).
- To check a saved fill is current: record each zone's
  `GetFilledPolysList(layer).Area()`, run `pcbnew.ZONE_FILLER(b).Fill(b.Zones())`,
  and compare. Any change means the saved fill was stale.

**Analysis methods that worked here:**

- *Is a via load-bearing?* Candidates are vias touching no track endpoint and no
  pad. Delete them all, save, reload, refill, run DRC: `unconnected_items == 0`
  proves nothing depended on them. Then diff each zone's filled outlines
  (matched per net by centroid and area) to find pour islands whose only feed was
  a deleted via. `git show <commit>:file | grep uuid` per commit tells a stitch
  run (hundreds of vias) from hand placement.
- *How much current does a segment carry?* Build the net's copper graph, remove
  one segment at a time, and BFS from the source; the loads that become
  unreachable are what it carries. Split segments at every point on them (never
  union a T-junction with both ends), attach pads with `pad.HitTest()`, and
  **assert every load is reachable before reporting anything.**
- *Real break vs expected cross-gap airwire* (two boards in one file): union-find
  per net plus virtual header-pin-n ↔ socket-pin-n edges. Less reliable than DRC:
  use it only to classify DRC's `unconnected_items`, never to add new ones.
  Single-pad "islands" are almost always a contact-test bug.

### Pulling LCSC footprints: `easyeda2kicad`

No EasyEDA subscription needed:

```sh
python3 -m venv e2k-venv && e2k-venv/bin/pip install easyeda2kicad
e2k-venv/bin/easyeda2kicad --footprint --lcsc_id=C609835 --output lcsc   # --full adds symbol + 3D
```

Some parts have no EasyEDA data. Output is legacy `(module …)` format; convert
to `(footprint …)` when adding to a project library. Preview headless with
`kicad-cli fp export svg` and an SVG-to-PNG step.

## RP2040 / Pico SDK

- **Toolchain:** use the Arm GNU Toolchain cask at
  `/Applications/ArmGNUToolchain/<ver>/arm-none-eabi/bin`. Homebrew's
  `arm-none-eabi-gcc` formula is also installed, ships no newlib, and fails at
  link with `cannot read spec file 'nosys.specs'`. Pass
  `-DPICO_TOOLCHAIN_PATH=…` or put the cask first on `PATH`. `PICO_SDK_PATH` is
  `~/pico-sdk`.
- **Never drag-and-drop a UF2 onto `/Volumes/RPI-RP2`.** macOS mounts it with the
  `fskit` msdos driver. The copy hangs, and the mount wedges until the board is
  physically replugged. Use `picotool load`. For this project's panels, follow the
  two-image procedure in `docs/PANEL_BRINGUP.md` (bootloader first, no `-x`,
  no `-f`).
- **`picotool info` segfaults on RP2040** (picotool 2.3.0, a partition-table bug).
  It's not a dead board. Check BOOTSEL with
  `ioreg -p IOUSB -l -w 0 | grep "RP2 Boot"`.
- **Every RP2040 pad resets with its pull-down enabled, and `gpio_init()` does not
  touch pulls.** Any pin whose floating or high-Z state means something needs an
  explicit `gpio_disable_pulls()` or pull setting. It has bitten `SENSE_12V`
  (GPIO17) and `INT_OUT` (GPIO22). See `docs/BRINGUP_LOG.md`, first board.

## Teensy / Arduino (master firmware)

- **`arduino-cli` puts its generated function prototypes above the first
  function in the sketch.** A function defined above a type that later
  prototypes use (`struct Color`) breaks the build with "'Color' has not been
  declared" at an unrelated line. Hit 2026-10-20; keep early globals free of
  functions, or define the function lower down.
- **The sketch is copied elsewhere to build**, so a relative `#include` outside
  `firmware/master/` can't resolve. `build.sh` reads the board's
  `build.flags.defs` (`arduino-cli compile --show-properties`) and passes it
  back with `-I…/firmware/shared` appended; every compile and the library scan
  use that property.
- **`arduino-cli compile` shows no compiler warnings by default.** Add
  `--warnings all` to see them.
- **`usb_rawhid_send(buf, 0)` is not non-blocking** (Teensyduino 1.62): with
  every transmit slot busy it spins, calling `yield()`, until the millisecond
  tick changes, up to ~1 ms. The core patch adds `usb_rawhid_try_send()`, which
  returns at once; the vendor interface uses only that.
- **The Teensy core writes USB interfaces in a fixed block order** (CDC, …,
  RawHID, …, joystick), whatever numbers `usb_desc.h` gives them, so the
  numbers follow that order: RawHID 2, joystick 3.
- **EEPROM emulation spreads every address over all 15 sectors**
  (`sector = (addr >> 2) % 15`), so two "separate" records share sectors, and a
  sector compaction (erase, then rewrite from RAM) touches both. See
  `docs/PANEL_CONFIG.md` → "As built".
- **`micros()` undercounts while interrupts are off** for over a millisecond:
  it is built on the SysTick count. Time such stretches with `ARM_DWT_CYCCNT`
  (enabled by the core at startup).

## USB devices on the bench

The Teensy, the debugprobe Pico and every brain all enumerate as
`/dev/cu.usbmodem*`. Identify them by USB serial number, not by port name:

```sh
ioreg -p IOUSB -l -w 0 | grep -E '"USB Product Name"|"USB Serial Number"|idVendor|idProduct'
```

`idVendor`/`idProduct` print in decimal. `ioreg` also finds devices that
`system_profiler` misses. Uploading to the Teensy needs its `teensy` protocol
port (`usb:…`), not the serial port (`docs/MASTER_BRINGUP.md`).
