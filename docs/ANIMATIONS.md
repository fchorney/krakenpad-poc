# Animations — formats, flash layout, LED order

One doc for the whole animation pipeline: source format (APNG/WebP) → on-device
binary format (`.smxa`) → panel flash slots → playback behavior. Merged 2026-07-23
from `ANIMATION_FORMAT.md`, `ANIMATION_BINARY_FORMAT.md`, and
`PROTOTYPE_LED_LAYOUT.md`.

---

## Source / interchange format: APNG or animated WebP

Goals:

- Full RGBA per LED per frame (proper alpha blending, no 1-bit transparency)
- 60fps+ capable frame timing
- Human-readable/previewable on any modern computer without tooling
- Custom metadata support for LED mapping and tool-specific data

Both APNG and animated WebP support full RGBA and precise frame timing, and are
natively viewable in modern browsers and most OS image viewers (macOS Preview
handles both). Each frame renders the 25 LEDs as a scaled-up pixel grid —
human-readable without knowing the format. PNG `iTXt` chunks (or WebP metadata)
can carry custom key/value data (LED topology, timing hints) that plain viewers
ignore but the tool can read.

**Why not GIF:** frame delays are in centiseconds (10ms units) and many decoders
impose a 2cs minimum floor (~50fps ceiling); no alpha channel — only 1-bit
color-key transparency. Neither is acceptable for 60Hz+ with proper blending.

The APNG/WebP remains the source of truth and the human-viewable artifact; the
converter tool exports the compact binary below for flashing.

## LED index order (serpentine)

25 LEDs per panel in the staggered 4–3–4–3–4–3–4 lattice, wired serpentine —
rows alternate direction. Binary frames store pixels in this index order:

```
00--01--02--03
--06--05--04--
07--08--09--10
--13--12--11--
14--15--16--17
--20--19--18--
21--22--23--24
```

Any tool that maps a rectangular image onto the panel must account for this
mapping (the `stepmaniax-gif-maker` tool already does).

---

## Binary format (`.smxa`)

Each animation file (released or pressed) has the following layout:

```
Offset  Size   Field
──────────────────────────────────────────────────────
0x00    4      Magic: 0x534D5841  ('SMXA' in ASCII)
0x04    1      Format version: 0x01
0x05    2      Frame count (uint16_t, little-endian)
0x07    1      Frames per second (uint8_t)
0x08    N×75   Frames — see below
```

**Frame layout (75 bytes each):** 25 pixels × 3 bytes (R, G, B), in LED index
order 0–24 (serpentine order above). Alpha is ignored at the LED level (LEDs
have no transparency); strip alpha before export.

**Size reference:**

| Duration | FPS | Frames | File size |
|----------|-----|--------|-----------|
| 2 sec    | 30  | 60     | 4,508 B   |
| 5 sec    | 60  | 300    | 22,508 B  |
| 30 sec   | 60  | 1800   | 135,008 B |

## Flash layout (W25Q32JV, 4MB)

```
Address range          Size    Contents
────────────────────────────────────────────────────────
0x000000 – 0x07FFFF   512 KB  Firmware (actual usage ~200 KB; headroom for growth)
0x080000 – 0x0FFFFF   512 KB  Released animation slot
0x100000 – 0x17FFFF   512 KB  Pressed animation slot
0x180000 – 0x3FFFFF  ~2.5 MB  Reserved (future: config, extra animation slots, etc.)
```

Sector size (minimum erase unit): **4 KB**. All slot boundaries are
sector-aligned. 512 KB per animation slot supports ~113 seconds at 60fps — far
more than any reasonable looping animation; the headroom costs nothing on a 4MB
chip.

## Slot validation

On boot the firmware checks each animation slot for the magic bytes
`0x534D5841` at the slot's base address. Blank flash reads as `0xFF` — no magic
match.

| Slot state    | Behavior                                |
|---------------|-----------------------------------------|
| Valid magic   | Load and play the stored animation      |
| Invalid/blank | Write compiled-in default, then play it |

A brand-new or factory-reset panel boots into a working state immediately
without requiring an upload step.

## Default animations

Compiled into the firmware binary as `const uint8_t` arrays; never erased by a
normal config/animation upload — only a firmware reflash changes them.

**Factory reset procedure:** erase the released and pressed animation slots,
reboot — firmware detects missing magic, writes defaults, continues normally.

- Default released animation: slow blue breathe across all 25 LEDs
- Default pressed animation: full-brightness white flash, fading back to blue

## Playback priority

Core 1 on the panel RP2040 follows this priority order:

```
1. RS-485 LED frame received from master   → display immediately, reset timeout
2. Local FSR press detected (Core 0)       → play pressed animation
3. No press, no recent RS-485 frame        → play released animation on loop
```

**RS-485 timeout:** if no LED frame arrives within 100ms, the panel falls back
to local animation automatically — the pad works standalone (no PC, no master)
and recovers gracefully from cable disconnection during play.

## Converter tool

The existing `~/Documents/repos/personal/stepmaniax-gif-maker` tool (already
handles the serpentine mapping) will be extended to:

1. Read source APNG/WebP (adding APNG/WebP support — it's GIF-based today)
2. For each frame, sample the 25 LED pixel positions from the scaled frame
3. Strip alpha, pack as 25 × RGB bytes
4. Write header + frame data to `.smxa`

The `.smxa` file is then flashed to the appropriate slot offset using a
to-be-written upload tool (USB or RS-485 based).
