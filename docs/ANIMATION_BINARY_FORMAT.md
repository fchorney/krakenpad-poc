# Animation Binary Format

On-device format for panel firmware. APNG/WebP is the source/interchange format —
see `ANIMATION_FORMAT.md`. This document covers the binary format stored in panel
flash and the firmware behavior around it.

---

## Binary Format

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

**Frame layout (75 bytes each):**
25 pixels × 3 bytes (R, G, B), in LED index order 0–24.
LED index order follows the physical serpentine wiring — see `PROTOTYPE_LED_LAYOUT.md`.
Alpha is ignored at the LED level (LEDs have no transparency); strip alpha before export.

**Size reference:**

| Duration | FPS | Frames | File size |
|----------|-----|--------|-----------|
| 2 sec    | 30  | 60     | 4,508 B   |
| 5 sec    | 60  | 300    | 22,508 B  |
| 30 sec   | 60  | 1800   | 135,008 B |

---

## Flash Layout (W25Q32JV, 4MB)

```
Address range          Size    Contents
────────────────────────────────────────────────────────
0x000000 – 0x07FFFF   512 KB  Firmware (actual usage ~200 KB; headroom for growth)
0x080000 – 0x0FFFFF   512 KB  Released animation slot
0x100000 – 0x17FFFF   512 KB  Pressed animation slot
0x180000 – 0x3FFFFF  ~2.5 MB  Reserved (future: config, extra animation slots, etc.)
```

Sector size (minimum erase unit): **4 KB**. All slot boundaries are sector-aligned.

512 KB per animation slot supports ~113 seconds at 60fps — far more than any
reasonable looping animation. The extra headroom costs nothing on a 4MB chip.

---

## Slot Validation

On boot the firmware checks each animation slot for the magic bytes `0x534D5841`
at the slot's base address. Blank flash reads as `0xFF` — no magic match.

| Slot state    | Behavior                                          |
|---------------|---------------------------------------------------|
| Valid magic   | Load and play the stored animation                |
| Invalid/blank | Write compiled-in default, then play it           |

This means a brand-new or factory-reset panel boots into a working state
immediately without requiring an upload step.

---

## Default Animations

Default animations are compiled into the firmware binary as `const uint8_t` arrays.
They are never erased by a normal config/animation upload — only a full firmware
reflash changes them.

**Factory reset procedure:**
1. Erase the released and pressed animation slots in flash
2. Reboot — firmware detects missing magic, writes defaults, continues normally

**Default released animation:** slow blue breathe across all 25 LEDs
**Default pressed animation:** full-brightness white flash, fading back to blue

(Exact frame data defined in firmware source. Defaults can be updated by
reflashing firmware.)

---

## Playback Priority

Core 1 on the panel RP2040 follows this priority order:

```
1. RS-485 LED frame received from master   → display immediately, reset timeout
2. Local FSR press detected (Core 0)       → play pressed animation
3. No press, no recent RS-485 frame        → play released animation on loop
```

**RS-485 timeout:** if no LED frame arrives within 100ms, the panel falls back to
local animation automatically. This ensures the pad works standalone (no PC, no
master MCU connected) and recovers gracefully from cable disconnection during play.

---

## Converter Tool

The existing `~/Documents/repos/personal/stepmaniax-gif-maker` tool will be
extended to export this binary format from APNG/WebP source files. Export steps:

1. Read source APNG/WebP
2. For each frame, sample the 25 LED pixel positions from the scaled frame
3. Strip alpha, pack as 25 × RGB bytes
4. Write header + frame data to `.smxa` file

The `.smxa` file is then flashed to the appropriate slot offset using a
to-be-written upload tool (USB or RS-485 based).
