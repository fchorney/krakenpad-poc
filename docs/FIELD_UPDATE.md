# Field Update — flashing panels over RS-485

**Status: specification, nothing implemented.** No panel bootloader exists, and
the master has no USB HID path to the PC yet either. This document exists so the
design is captured while it's fresh; it commits no hardware.

**Independent of the carrier/brain split.** Everything here works on the current
single-board `panel-pcb` exactly as well as on the two-board version. It came up
*during* the split discussion (2026-07-27) because a brain buried in the frame
cavity is awkward to reach — but the value stands on its own, and the biggest
beneficiary is animation delivery, not firmware.

## The problem

Every panel carries a USB-C port whose only job is flashing. Today, changing
anything on the panels — a firmware fix, a threshold default, a recolored
animation — means opening the pad and plugging into nine boards one at a time.
Animations are the painful case: they're the thing most likely to change often,
they can differ per panel, and `docs/ANIMATIONS.md` currently specifies flashing
`.smxa` files to slot offsets by hand.

The pad already has a 1 Mbps bus reaching all nine panels. Use it.

## What makes this possible

The RP2040 can rewrite the flash it boots from, at runtime —
`flash_range_erase()` / `flash_range_program()` from the Pico SDK. No second
processor, no programmer, no hardware change. This is ordinary field-update
engineering, not research.

**To be unambiguous: the RP2040 has no internal flash.** It has 264 KB of SRAM
and a 16 KB mask ROM (the bootrom), and executes XIP over QSPI out of **U3, the
W25Q32JV** — so every byte of the map below lives on that external chip. There
is no "internal vs external" decision to make here; there is one flash, and this
is it. That is also *why* the XIP constraint below exists: the code and the
storage being written are the same device.

**What it does *not* remove: the need for a physical flashing path at least
once.** The RP2040 bootrom offers USB (BOOTSEL mass-storage / PICOBOOT) and SWD
only — there is no ROM UART boot mode (that arrived with RP2350). A virgin chip
has no bootloader to talk to. So USB-C / SWD stays in the design; its role
changes from *routine* to *factory provisioning and last-resort recovery*.

## Flash layout

Revises the map in `docs/ANIMATIONS.md` by carving a bootloader out of the front
of the firmware region and claiming part of the reserved space:

```
0x000000 – 0x007FFF    32 KB  Bootloader          (never written in the field)
0x008000 – 0x07FFFF   480 KB  Application firmware (~200 KB actual)
0x080000 – 0x0FFFFF   512 KB  Released animation slot
0x100000 – 0x17FFFF   512 KB  Pressed animation slot
0x180000 – 0x1FFFFF   512 KB  Staging slot         (incoming image lands here)
0x200000 – 0x27FFFF   512 KB  Golden firmware      (last known good)
0x280000 – 0x3FFFFF  ~1.5 MB  Reserved
```

All boundaries stay 4 KB sector-aligned. **Consequence to plan for:** the
application no longer links at the start of flash. It builds for XIP offset
`0x10008000`, and the bootloader sets `VTOR` before jumping to its vector table.
Standard Pico SDK practice, but it is a linker-script change, not a no-op.

## Staged-copy, not A/B

The textbook answer is A/B slots — two firmware images, boot whichever is
active. **Don't**, on this chip: the RP2040 executes in place from XIP, so two
slots means either linking the image twice at two different addresses or
building position-independent code. Both are ongoing tax on every build.

Instead:

1. Master pushes the image into the **staging slot**.
2. Panel verifies staging against a whole-image CRC-32.
3. Panel sets a **pending flag** and reboots.
4. **Bootloader** copies staging → application region, verifies, clears the flag,
   boots.

Power loss during the copy is safe and self-healing: the flag is still set and
the staged image is still intact, so the next boot simply recopies. The only
irrecoverable state is a broken *bootloader*, which is small, rarely touched, and
validated once.

Before overwriting the application, the bootloader copies the outgoing image to
the **golden** slot, so a last-known-good always exists on the panel.

## The constraint that shapes everything: XIP goes away during flash writes

**While the RP2040 erases or programs flash it cannot execute from flash.** Any
code running in that window must be RAM-resident (`__not_in_flash_func`), and
anything time-sensitive that isn't gets dropped. For a bus-driven update that
means the UART receive path is the problem.

Solution: **DMA the UART RX into a RAM ring buffer.** DMA keeps running
regardless of XIP state, so bytes arriving mid-erase land safely and the
RAM-resident update loop drains them afterward.

Sizing, from the W25Q32JV datasheet and 1 Mbps ≈ 100 µs per 10-bit byte:

| Operation | Typical | Bytes arriving meanwhile |
|---|---|---|
| Page program (256 B) | ~0.4 ms | ~4 |
| Sector erase (4 KB) | ~45 ms | ~450 |
| Block erase (64 KB) | ~150 ms | ~1500 |
| 4 KB written as 16 pages | ~6.4 ms | ~64 |

**Erase the whole staging slot up front**, at `'U'`, using 64 KB block erases —
and have the panel ack only *after* the erase completes, so the master simply
waits rather than streaming into a deaf receiver. Then buffer incoming blocks in
RAM a sector at a time and program 4 KB at a go. A **≥2 KB DMA ring** covers the
worst in-flight case with margin.

## Protocol

Uses the framing in `docs/RS485_PROTOCOL.md` unchanged — `0x55` sync, cmd, addr,
len, payload, CRC-8. Lowercase replies, same as the existing commands. New
letters: `'U'`/`'u'`, `'D'`, `'Q'`/`'q'`, `'A'`/`'a'`, `'X'`/`'x'`, `'B'`/`'b'`.
None collide with `L F f C c I i`.

**The 80-byte payload cap needs no change**: a data block is a 4-byte index plus
64 bytes of image = 68 bytes.

### `'U'` — update begin (master → broadcast or one panel)

- len 14. Payload: `[target u8] [total_len u32 LE] [crc32 u32 LE] [version u32 LE]`
- `target`: `0` = firmware, `1` = released animation, `2` = pressed animation
- Panel erases the staging slot, then replies `'u'`. **May take ~1–2 s** — the
  master must wait for every ack, not assume a fixed delay.
- Broadcast `'U'` is legal and normal for firmware (all nine panels run identical
  code). Animations are per-panel, so those are addressed.

### `'u'` — update-begin ack (panel → master)

- len 2. Payload: `[status u8] [max_block_index u16 LE]`
- `status`: `0` = staging erased and ready, non-zero = refused (image too large
  for the slot, target invalid, bad state)

### `'D'` — data block (master → broadcast or one panel)

- len 5–68. Payload: `[block_index u32 LE] [data ≤64 B]`
- **No reply.** Blocks are fire-and-hose; gaps are recovered by `'Q'` afterward.
- Panel records receipt in a block bitmap. 200 KB / 64 B = 3200 blocks = a 400-byte
  bitmap. A full 512 KB slot is 8192 blocks = 1 KB. Comfortable in RAM.

### `'Q'` — query staging state (master → one panel)

- len 0

### `'q'` — staging state (panel → master, reply only)

- len 6–74. Payload: `[state u8] [missing_count u16 LE] [first ≤17 missing indices u32 LE]`
- `state`: `0` = idle, `1` = receiving, `2` = staged and CRC-verified,
  `3` = CRC failed, `4` = running bootloader
- Master re-sends the reported blocks and re-queries. Converges in a couple of
  rounds; if `missing_count` stops falling, abort rather than looping.

### `'A'` — activate (master → broadcast)

- len 4. Payload: `[crc32 u32 LE]` — the image CRC the master expects each panel
  to be holding. A panel whose staged image doesn't match ignores the command.
  This is what prevents a stale or partial panel from committing.
- Panel sets the pending flag, replies `'a'`, then reboots.

### `'a'` — activate ack (panel → master, reply only)

- len 0

### `'X'` — abort (master → broadcast)

- len 0. Panel discards staging state and returns to normal operation. Replies `'x'`.

### `'B'` — stay in bootloader (master → broadcast)

- len 0. Answered **only by the bootloader**, during its boot listen window.
  Replies `'b'` and holds there instead of starting the application.

## Two-phase commit

The update is all-or-nothing across the pad. Never let four panels run the new
firmware while five run the old.

1. **Enter update mode.** Master stops `'L'` LED frames and `'F'` polls, and the
   host stops HID input reporting. No gameplay during an update.
2. **Enumerate.** `'I'`/`'i'` identify (see `docs/RS485_PROTOCOL.md`) to confirm
   which panels are present and that slot↔ID agrees. Read each panel's version.
   Skip panels already at the target version.
3. **Begin.** Broadcast `'U'`; wait for `'u'` from every participating panel. Any
   refusal aborts here, before anything is written.
4. **Stream.** Broadcast `'D'` blocks, whole image, once.
5. **Gap-fill.** `'Q'` each panel; re-send reported blocks; repeat until every
   panel reports `state = 2` (staged and verified).
6. **Commit.** Only now, broadcast `'A'`. Panels ack and reboot.
7. **Confirm.** Wait for the bus to come back, re-enumerate, verify every panel
   reports the new version. Report any that didn't.

If anything fails before step 6, broadcast `'X'` and nothing anywhere has
changed — the application region was never touched.

## Timing

At 1 Mbps ≈ 10 µs/byte, a 68-byte payload frame is 73 bytes on the wire ≈ 0.73 ms.

| Payload | Blocks | Broadcast time |
|---|---|---|
| 200 KB firmware | 3200 | ~2.4 s |
| 4.5 KB animation (60 frames × 25 LEDs × 3 B) | 72 | ~0.05 s |
| 512 KB full animation slot | 8192 | ~6 s (unicast, per panel) |

Add ~1–2 s of staging erase per `'U'`, plus gap-fill rounds. **A whole-pad
firmware update lands in well under 10 seconds**, because the image is broadcast
once rather than nine times. Animations are per-panel and therefore serial, but
realistic ones are small enough not to matter.

## How big can the firmware get?

**The protocol imposes no ceiling** — block index is `u32`, `total_len` is `u32`.
The flash map is the only limit, and it is **leveraged 3×**: every byte of
firmware must fit in the application region, in staging, *and* in the golden copy
at the same time.

```
32 KB bootloader + 3 × firmware + 2 × animation slot ≤ 4096 KB
```

| Configuration | Firmware ceiling |
|---|---|
| Map as specified (512 KB animation slots) | **~1013 KB** |
| Animation slots cut to 64 KB each | ~1312 KB |
| Golden copy dropped (2× leverage, no local rollback) | ~1520 KB |

Against ~200 KB of realistic usage that is a **5× margin**, and the 480 KB
application region alone is 2.4×. The costs that scale with size are all linear
and none of them is what stops you:

| At 480 KB (application region full) | |
|---|---|
| Broadcast time | ~5.7 s |
| Staging erase | ~1.1 s |
| Block bitmap RAM | 960 B (of 264 KB) |
| Bootloader staging→app copy | ~2 s, at boot, no bus traffic |

**The one thing in this design with the appetite to blow past that** is the
decision in `CLAUDE.md` to compile **default animations into the firmware as
`const` arrays**. That is the only payload here that can grow the image by
hundreds of KB, and under a 3× map it is paid for three times. If the defaults
ever become elaborate, stop embedding them: ship a minimal built-in fallback and
write the real defaults into an animation slot on first boot. Nothing else —
ADC sampling, PIO, UART, flash management — has a plausible path to bloat, and
the usual embedded offenders (TLS, filesystems, float `printf`, graphics
libraries) aren't in this design.

## Recovery

Layered, cheapest first:

1. **Bad application, bus still works** — the app answers `'U'`; just push a new
   image. Ordinary case.
2. **Application won't run or won't answer** — the bootloader listens for `'B'`
   for **200 ms at every boot** before starting the app. A panel bricked by bad
   *application* firmware is still reachable over the bus. This window is the
   single most important thing to get right.
3. **Restore golden** — panel-local, no bus needed. `docs/PANEL_CONFIG.md`
   reserves DIP codes 14–15; **assign 14 = "boot bootloader, restore golden
   image"**. Set the DIP, power-cycle, panel reverts to last-known-good.
4. **Bootloader itself is broken** — USB-C BOOTSEL or the SWD header, i.e. the
   physical path that provisioned the board originally. This is why the port
   stays in the design regardless.

## Host tooling

Target interaction: plug in the master's USB, run one command, done.

```
$ smxflash firmware/panel/build/panel.uf2
9 panels found (IDs 0-8, slot map OK)
  8 panels at v1.2, 1 panel at v1.1 -> updating all to v1.3
  staging.......... ok      streaming 3200 blocks ... 2.4s
  gap-fill: panel 5 missing 3 blocks -> resent
  all 9 staged & verified -> commit
9 panels now at v1.3
```

`smxflash <file.smxa> --slot released [--panel N]` does the same for animations,
defaulting to all panels when `--panel` is omitted.

## Open questions

- **Version identity.** A monotonic `u32` is assumed above. A git-describe hash
  is friendlier for humans but doesn't order; probably carry both — `u32` for
  comparison logic, a string in the image header for display.
- **Should the master hold the image, or stream from the host?** Streaming means
  the master needs no storage but the PC must stay connected throughout. Holding
  it means Teensy 4.1 SD or a flash region. Streaming is simpler and the update
  is seconds long; start there.
- **Does the bootloader need the RS-485 stack, or the application?** The 200 ms
  listen window means the *bootloader* needs UART + DE control + framing + CRC —
  a meaningful chunk of code inside the 32 KB budget. Verify the budget early;
  it's the one place this design could get uncomfortable.
- **Animation slot writes while playing.** Writing the released-animation slot
  suspends XIP, so the panel can't render from flash mid-update. Simplest answer
  is that update mode blanks the LEDs; worth confirming that's acceptable rather
  than trying to keep an animation running.

## Why this is worth building

The firmware case alone is convenience. The animation case is the real one: it's
the payload that changes most often, it's per-panel, and without a bus mechanism
every tweak means nine USB-C plug events inside an opened pad. That is the cost
this removes.

Second-order: it also makes a **buried brain** viable in the carrier/brain split
(`hardware/dual-panel/README.md`), because post-assembly access stops being a
routine requirement. If that split proceeds, this doc is a dependency of it — but
not the reverse.
