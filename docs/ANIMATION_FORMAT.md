# Animation Format

## Goals

- Full RGBA per LED per frame (proper alpha blending, no 1-bit transparency)
- 60fps+ capable frame timing
- Human-readable/previewable on any modern computer without tooling
- Custom metadata support for LED mapping and tool-specific data

## Approach

Use **APNG** or **animated WebP** as the primary animation format:

- Both support full RGBA and precise frame timing
- Natively viewable in modern browsers and most OS image viewers (macOS Preview handles both)
- Each frame renders the 25 LEDs as a scaled-up pixel grid — human-readable without knowing the format
- PNG `iTXt` chunks (or WebP metadata) can carry custom key/value data (LED topology, timing hints, etc.) that plain viewers ignore but the tool can read

## Why Not GIF

GIF stores frame delays in centiseconds (10ms units); many decoders impose a 2cs minimum floor (~50fps ceiling). More importantly, GIF has no alpha channel — only 1-bit color-key transparency. Neither is acceptable for 60Hz+ with proper blending.

## Export Path

APNG/WebP as the working/interchange format. The converter tool exports to a compact proprietary binary format for flashing to panel firmware. The APNG/WebP remains the source of truth and the human-viewable artifact.

See `ANIMATION_BINARY_FORMAT.md` for the on-device binary format spec, flash layout, and default animation behavior.

## Tooling

Current gif tool: `~/Documents/repos/personal/stepmaniax-gif-maker`

Already handles the SMX panel serpentine LED layout mapping. Will need to be forked or extended to support:
- APNG/WebP output
- Full RGBA + 60fps+ requirements
- Export to binary format for flashing to panel firmware
