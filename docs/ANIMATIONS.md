# Animations — formats, flash layout, LED order

One doc for the whole animation pipeline: source format (APNG/WebP) → on-device
binary format (`.smxa`) → panel flash slots → playback behavior. Merged 2026-07-23
from `ANIMATION_FORMAT.md`, `ANIMATION_BINARY_FORMAT.md`, and
`PROTOTYPE_LED_LAYOUT.md`.

**Status: design, not built** (2026-10-20). The flash slots are reserved in the
panel's flash map, but no firmware reads them, no converter exists,
and the gameplay firmware goes dark when `'L'` frames stop rather than playing
a local animation. The LED index order below is live: the gameplay firmware and
the master's `'L'` frames use it.

## Decisions and open questions (2026-10-20)

**DECIDED 2026-10-20:**

- **On a computer, an animation is always APNG** (format chosen below, same
  day; this line first read "APNG or animated WebP"). There is no
  user-facing panel file. The program that uploads to the panels converts to the
  panel format as part of the upload, the same way the stock SDK turns a GIF into
  its panel format before writing it.
- **Every frame has its own delay, in whole milliseconds.** There is no global
  fps. The editor won't accept a delay shorter than one LED frame period at the
  pad's maximum LED refresh rate (below).
- **The panel format carries a total length and a CRC**, so a transfer that
  arrives incomplete or corrupted can be detected. No maximum length is set
  beyond the size of the slot.
- **Delays are whole LED frames.** A delay is always a whole number of frame
  periods at the pad's maximum refresh rate, so playback never deals with a
  fraction of a frame.
- **Loop metadata starts as marker pixels in an extra flag row**, the convention
  `stepmaniax-gif-maker` already uses: a row below the LED grid, pixel x=0
  (R ≥ 128) marks loop start, x=1 marks loop end. Stock reads only the x=0
  marker. x=1 is a deadsync extension (intro plays once, the loop repeats while
  held, the outro plays on release). More pixels in that row can carry more
  flags later. If APNG/WebP metadata turns out to be a cleaner home, it may
  replace the flag row.
- **`stepmaniax-gif-maker` is the starting template for the editor**, probably
  as a fork, since the format will need changes of its own.

**Open:**

- **The pad's maximum LED refresh rate: 60, 90 or 120 Hz.** It sets the shortest
  frame delay. To be chosen by measurement: the highest rate at which the bus
  and the panel CPUs still drop no presses and lose no telemetry replies.
  - **Preview in other viewers (tested 2026-10-20, MacBook with ProMotion).**
    Test files: a dot turning once per second, stored at 10/60/90/120 Hz in
    APNG, WebP, JPEG XL and GIF, plus APNGs at 99/100/101 Hz.
    - **Chrome (measured):** correct up to 90 Hz in every format. At 120 Hz every
      format is far too slow, and the 99, 100 and 101 Hz APNGs all take ~10 s
      per turn: each frame shown for 100 ms. Inferred cause: Chrome turns a
      delay into whole milliseconds (10.1 → 10), then replaces anything of
      10 ms or less with 100 ms, so frames under 11 ms (above ~90.9 Hz) are
      slowed.
    - **Safari (observed):** 60 Hz correct; 90 Hz slower than 60, and higher
      rates slower still. Inferred: it shows every frame, at most one per
      screen update, with page updates at 60 Hz by default even on ProMotion,
      so a 90 Hz file would take ~1.5 s per turn. Not yet timed.
    - **Finder / Quick Look (observed):** anything above 10 Hz plays slowly, more
      slowly the higher the rate. Inferred: a ~100 ms minimum per frame.
    - So outside viewers show 60 Hz frames correctly (Chrome and Safari), not
      faster ones. The pad and our editor are unaffected: neither uses these
      decoders. This bears on the maximum rate only as preview fidelity; the
      bus and CPU limits are still to measure.
- **Loop semantics** beyond one start/end pair. Ideas so far, none settled:
  - loop **counts** and **several loops** in one animation (e.g. intro, loop ×3,
    middle, loop until release, outro). Two marker pixels can't express this
  - loop-start and loop-end on the same frame = **hold that frame until
    release**
  - which events end a loop ("until release" means the pressed animation needs
    to know the press ended)
- **Flag row or format metadata** (PNG `iTXt`, WebP XMP/EXIF) for the loop data.
- **Milliseconds vs frames.** "Whole milliseconds" and "whole frames" conflict:
  at 60, 90 and 120 Hz a frame is 16.67, 11.11 and 8.33 ms, so a whole number of
  frames is almost never a whole number of ms. One way out is to store a frame
  count on the panel and show ms only in the editor. With APNG chosen
  (below), the source file can hold whole frames exactly (`delay_num` frames
  over `delay_den` = the LED rate), so the conflict only remains for how the
  editor displays delays and what the panel image stores. The APNG/WebP source also
  has its own delay units (APNG: a fraction; WebP: whole ms) that the editor
  must round to.
- The panel binary layout itself (see "Binary format" below: the draft there
  predates these decisions).

---

## Source / interchange format: APNG

**DECIDED 2026-10-20: APNG**, over animated WebP and JPEG XL, because:

- **It stores frame timing exactly.** An APNG delay is a fraction
  (`delay_num / delay_den`, 16 bits each), so "3 frames at 120 Hz" is exactly
  3/120 s. WebP stores whole milliseconds, so frame-based delays would be
  rounded on save and snapped back on upload. JPEG XL can store exact ticks,
  but `cjxl` rounded an APNG's 1/120 s to 8 ms when converting.
- **It is the simplest to write.** Chunks plus zlib, no library: a test writer
  was ~30 lines of Python. WebP's lossless encoder means linking libwebp;
  JPEG XL means libjxl. Pillow reads and writes APNG; its JPEG XL support is
  not in Pillow 12.2.
- **Widest viewer support**, and standard since the W3C PNG Third Edition.
  JPEG XL only became default in Chrome (155) and Firefox (158) in October
  2026.
- File size doesn't separate them: a 25-LED frame is tiny in any format.

Accepted: outside viewers misplay fast frames (see "Preview in other viewers"
above). The same happens with WebP and JPEG XL, so it doesn't favour either;
only a dedicated app previews correctly.

The comparison as written before the decision:

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

> **Superseded draft (2026-07), kept for reference.** The 2026-10-20 decisions
> above replace three things in it: the single `fps` byte becomes a per-frame
> millisecond delay, loop flags are added, and a total length and CRC are added.
> It is also no longer a file anyone handles: the upload program produces it
> and sends it straight to the slot. The 75-byte serpentine RGB frame is still
> the likely payload. The new layout isn't written yet.

The 2026-07 draft layout of each animation (released or pressed):

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

## Flash slots

The slots sit in the panel flash map defined by the bus-update bootloader
(`firmware/panel/c/common/fw_layout.h`, narrated in `docs/FIELD_UPDATE.md` →
"Flash map"):

```
0x080000 – 0x0FFFFF   512 KB  released animation slot
0x100000 – 0x17FFFF   512 KB  pressed animation slot
```

The rest of the 4 MB holds stage 0 and bootloader slot A (`0x000000`), the
application (`0x008000`, 480 KB), the update staging area (`0x180000`),
bootloader slot B (`0x200000`) and a reserved golden copy after it. Config has
no region assigned yet.

Sector size (minimum erase unit): **4 KB**. Both slot boundaries are
sector-aligned. 512 KB per slot holds ~113 seconds at 60fps, far more than any
reasonable looping animation.

> The 2026-07 version of this map put the firmware at `0x000000` (512 KB) and
> left everything above `0x180000` reserved. The bootloader split of 2026-10-18
> replaced it; the animation slots kept their addresses.

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

**As built (2026-10-22), before local animations exist:** 100 ms without `'L'`
goes dark; 1 s without any valid bus frame (no master) shows a built-in
stand-in for steps 2 and 3: a slow dim white breath, all white while the panel
is pressed (`docs/PANEL_CONFIG.md` → "LED modes"). A running master sends its
own idle and press frames when the host sends none, so steps 2 and 3 only
matter without a master.

## Converter tool

Since 2026-10-20 the conversion is a step **inside the upload program**, not a
separate export (see "Decisions" above). The steps:

1. Read the source APNG/WebP, including each frame's delay and the loop metadata
2. For each frame, sample the 25 LED pixel positions from the scaled frame
3. Strip alpha, pack as 25 × RGB bytes in serpentine order
4. Build the panel image (header, frames, length, CRC) and send it to the slot

The existing `~/Documents/repos/personal/dancegames/stepmaniax-gif-maker` tool
already handles the serpentine mapping and the flag-row loop markers, and is
GIF-based today. It is the starting template for our editor, probably as a
fork. Where the upload-time conversion code lives is not decided.

The panel image then has to reach the slot. The bus-update mechanism
(`docs/FIELD_UPDATE.md`) is the obvious carrier: its `'U'` command dropped the
`target` field for now, and animations are expected to get their own command
once the slots exist.
