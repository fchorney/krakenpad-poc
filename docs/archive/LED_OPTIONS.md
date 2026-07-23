> **ARCHIVED 2026-07-23** — LED family decision closed 2026-07-10 (WS2815 confirmed). Kept for the rationale; the decision summary lives in docs/BOM.md and CLAUDE.md.

# Panel LED Options (exploratory)

Status: **exploratory — WS2815 remains the working default.** Captures a
brightness/refresh-rate question raised 2026-07-10; not a decision.

## The question: brighter addressable LEDs?

As established in `docs/UNDERGLOW.md`, supply voltage does not set LED
brightness — current does. WS2812B (5V) and WS2815 (12V) use comparable LED
dies at comparable max current (~20mA/channel, ~60mA/LED at full white), so
switching voltage alone buys nothing on brightness. Two real levers exist:

### 1. Higher-current-binned WS281x-protocol parts (simple lever)

Some suppliers sell WS2815-protocol strips/parts built on larger or
higher-current-rated packages (bigger die than the common 5050, or better
binning) that are measurably brighter at the same protocol and timing. Pure
BOM swap — no firmware or PCB routing changes. Worth comparing datasheet
max-current and luminous-intensity specs across suppliers when sourcing.

### 2. Switch to a clocked protocol: APA102 / SK9822 ("DotStar" family)

Different addressable LED family — same one-IC-per-LED addressable concept,
but data is clocked (separate CLK + DATA wires) instead of WS281x's fixed
~800kHz single-wire bit-bang timing. Two consequences:

- **Some APA102 product lines are genuinely brighter** (higher current
  rating than typical WS281x parts) — but this isn't guaranteed by the
  protocol itself, it's a sourcing question same as option 1.
- **Directly serves this project's core latency/refresh goals.** WS281x's
  fixed bit rate caps how fast a frame can be clocked out — the bench math
  elsewhere in this project (25 LEDs ≈ 750µs at 800kHz) is a hard floor for
  that protocol. APA102's SPI clock can run at multiple MHz, cutting per-frame
  push time dramatically and removing that floor almost entirely. Given the
  project's whole premise is eliminating latency and pushing refresh rate
  (see CLAUDE.md's "6ms Lag Root Cause" and "LED 30Hz → 60Hz" sections), this
  is arguably a better protocol fit than WS281x independent of brightness.

**Cost of switching:** one extra wire per panel-internal LED chain (CLK, in
addition to DATA) — contained entirely within the panel PCB's internal LED
routing, does **not** affect the RS-485 bus or master↔panel wiring, since each
panel's own RP2040 still just receives frame data over RS-485 and pushes it
locally. Firmware changes: replace `ws2812.pio` with an SPI-based push,
likely using the RP2040's hardware SPI peripheral + DMA instead of PIO —
plausibly a *simplification* over the current PIO program, and frees the PIO
block for other use. APA102-family parts typically cost a bit more per LED
than WS281x-family.

## Decision (2026-07-10): WS2815 confirmed

APA102/SK9822-class parts are 5V — powering ~1.5A of 5V LED load per panel
would force a buck converter and reopen the settled panel power design
(cascaded AMS1117 linears). That cost isn't justified by the benefits at this
project's targets:

- The 750µs/25-LED push-time floor is irrelevant at 60Hz (each panel pushes
  locally via PIO+DMA in parallel; the RS-485 broadcast, identical for either
  family, is the real refresh ceiling). LED latency is cosmetic — input
  latency lives on the INT wire.
- Remaining WS trade-offs accepted: 8-bit-only dimming (no 5-bit global
  current → steppy deep dims), ~2kHz PWM (can band on camera footage at
  partial brightness — cosmetic, video-only), strict timing (already solved
  with PIO).
- WS2815 bonuses: BIN backup data line (one dead LED doesn't kill the chain —
  good for stomped-on hardware), 12V-native. Quirk: ~1mA/LED quiescent draw
  when dark (~25mA/panel, negligible).

Brightness lever if ever needed: higher-current-binned WS2815-protocol parts
(option 1 above) — pure sourcing swap, decision unaffected.