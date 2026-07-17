# Platform / Underglow LED Strips (optional feature)

Status: **optional — power/wiring topology and LED grouping fully resolved (2026-07-09).**
Nice-to-have, not required. If it isn't easy to fit into the new PCB set, the plan is to
just not connect it rather than force a fit.

## What we know from the stock protocol (see `../stepmaniax-sdk-mp` docs)

- Platform strips are driven **directly by the stock MCU** — they are NOT on the
  panel data bus. `SMX_SetPlatformLights()` sends USB command
  `'L' + strip_index + count + RGB` and the MCU handles it locally (confirmed by
  bus capture: no bus traffic during platform light updates).
- **44 addressable chunks per pad** on the platform edge strip (not 44 individual
  physical LEDs — see confirmed grouping below). The SDK's 88-chunk (264-byte)
  buffer spans two pads, 132 RGB bytes each.

## Stock power supply rating — confirmed from manual (2026-07-09)

Manual wiring diagram (page 8) labels the wall supply directly, by generation:
- **Gen 4: 12V DC 9A**
- **Gen 5: 12V DC 15A** (the generation the user owns)

Supersedes the earlier "~8A, user's recollection" estimate below — no longer
speculative for Gen 5. **Scope decision:** this project targets Gen 5 pads
only. Gen 4 owners can supply their own adequately-rated 12V supply if the
stock 9A unit doesn't cover our design's draw; not worth constraining the
design around the older, lower-rated unit.

## Stock power architecture (observed hardware, 2026-07-10)

Stock pad ships with:
- **12V wall power supply**, 15A rated on Gen 5 (see above; supply rating no
  longer needs teardown confirmation)
- **Daygreen B15-1224-05**, a 75W DC-DC converter: 12/24V input → **5V @ 15A max**
  output (75W / 5V = 15A, checks out)

This is a real, deliberately-sized switching supply for a **distributed 5V
bus**, not a small logic regulator — 15A is far more than underglow alone
needs (88 LEDs × ~60mA max ≈ 5.3A worst case). This is consistent with stock
running its *entire* downstream network — panels, panel LEDs, and underglow —
on one central 5V rail generated right after the 12V input, rather than
distributing 12V outward and regulating locally at each panel (our approach).
Also consistent with the master controller enumerating with an Arduino VID
(`0x2341`, see `../stepmaniax-sdk-mp/docs/USB_PROTOCOL.md`) — classic
Arduino-class boards are natively 5V logic, which would make a single shared
5V bus the natural fit for that generation of hardware, and would also make
5V-native underglow strips the likely finding at teardown.

If the ~8A supply rating is accurate, that leaves roughly 96W − 75W ≈ 21W not
running through the 5V converter — plausibly the master board's own logic
drawing 12V directly (e.g. via an Arduino-style VIN input with its own local
regulator), separate from the high-current downstream 5V network. Speculative
until the teardown confirms it, but worth keeping in mind while tracing wiring.

**Confirmed 2026-07-10:** the panel power chains use thick 4-pin Molex
connectors (the old PC peripheral/disk-drive style) carrying 5V — not 12V.
This further confirms the single-shared-5V-bus theory: panels themselves are
powered at 5V from stock, not fed 12V for local regulation the way our design
does it.

**Why this doesn't change our approach for panels:** a single centralized
high-current 5V bus needs heavier gauge wire and suffers more voltage drop over
the pad's multi-meter physical extent than our column-based 12V distribution
with local per-panel regulation (same power at ~2.4× less current per meter of
cable). Stock's design makes sense for the era of hardware it's built on; it's
not a reason to copy the architecture for panels.

### Plan: tap 12V directly, shifter VCC off Teensy USB 5V (resolved 2026-07-09)

The manual wiring diagram (yellow/black/pink wires, see below) shows the strip
getting **raw 12V**, not the Daygreen's 5V output — so the earlier "reuse the
Daygreen converter for underglow" idea is moot; underglow just wants the same
12V this project already distributes to panel columns. Simpler outcome than
originally planned:

- **Power**: tap 12V directly (e.g. off the same rail feeding panel columns, or
  straight off the wall supply/EMI filter) — no DC-DC brick, no local
  regulator, no magnetics on the master PCB at all.
- **Data**: master PCB → level-shifted 5V (spare SN74AHCT125 channel) → strip
  DIN, shared ground with the 12V supply.
- **Shifter logic VCC**: the Teensy already has a 5V rail present on the board
  from its USB connection (VUSB) — the shifter only draws single-digit mA
  (logic only, not LED power), well within what a USB 5V rail already budgets
  for onboard peripherals. Tap that directly; no separate regulator or
  Daygreen dependency needed for this either.
- Net result: **zero new regulator design, zero magnetics, zero dependency on
  the stock Daygreen converter** for underglow. The Daygreen brick isn't
  needed by our design at all (it was stock's answer to a 5V-distribution
  problem our column-based 12V architecture doesn't have).

### LED grouping — confirmed by direct test (2026-07-09)

Tested with `../stepmaniax-sdk-mp/sample/underglow_probe.cpp` (built as
`smx-underglow-probe`), which uses the existing `SMX_SetPlatformLights()` SDK
call to light a single addressable chunk white against an all-black strip and
cycle through all 44 chunk indices. **Confirmed: each addressable chunk is a
group of 3 physical LEDs** — lighting one chunk index visibly lights three
LEDs together, matching the grouped-by-3 12V/WS2811-style hypothesis from the
manual wiring diagram (3 wires only, no WS2815 backup-data line).

**Physical layout, derived from the 44 chunks:** two side strips of 17 chunks
each (running along the left and right edges of the pad) plus one back strip
of 10 chunks (across the rear edge) — 17 + 17 + 10 = 44. No strip on the front
edge (matches the pad's physical shape — players stand at the front). In
physical LED count: 51 + 51 + 30 = 132 physical LEDs per pad, addressed as 44
groups-of-3.

**Implication for our animation/protocol design:** underglow addressing
resolution is per-group-of-3, not per-LED — any animation tooling or USB
protocol command for underglow should think in terms of 44 addressable
positions (17 left / 17 right / 10 back), not 132 individually-controllable
LEDs. This is a real (if minor) resolution limit worth documenting anywhere
underglow animations get designed later.

## Set-and-forget is native behavior

WS28xx-family LEDs latch their PWM state: write one frame and they hold the
color indefinitely with the data line quiet — no refresh, no CPU cost. Stock
"static underglow" is almost certainly one strip write per `'L'` command.
Our default: write on command only. Animation stays possible for free (just
send frames more often) but is not the design center.

**Probe check:** watch the strip data line while changing underglow color from
the SDK — expect a single burst (132 physical LEDs × 24 bits @ 800kHz ≈ 4ms, or
44 chunks × 24 bits if the WS2811 ICs are only clocked once per group-of-3) then
a flat line. The burst timing identifies the LED family.

## Plan for our hardware

- Underglow attaches to the **master (Teensy) PCB**, mirroring stock topology.
- Host-side: a command in our USB protocol sets strip colors (or animations later).
- Teensy drives the strip data line the same way the panel Pico drives its 25
  LEDs. Needs a level shifter channel (SN74AHCT125 has 3 unused channels if one
  is already on the master board) and appropriate LED power.

## Power strategy — resolved 12V-native (2026-07-09, from manual wiring diagram)

**Resolved:** underglow is 12V-native — see the manual wiring diagram findings below.
No local voltage conversion needed on the master PCB. Background on why 12V-native is the
simpler outcome preserved below for context.

**Background for whenever the teardown data comes in:**

Voltage does not determine LED brightness — current does. WS2812B (5V) and
WS2815 (12V) use essentially the same LED die at similar max current per LED;
a 5V strip at full brightness is not dimmer than a 12V one. The real reason
WS2815-style strips exist is voltage drop over cable runs — at 5V, matching a
given brightness needs ~2.4× the current of 12V, so the same wire/run length
shows more visible dimming/color-shift at the far end. At 44 LEDs × ~60mA max
≈ 2.6A per strip (2 strips ≈ 5.2A total at full white), that's a real number on
a 5V rail.

There's also a structural reason 12V would be simpler: as currently planned,
**the master PCB may not need any local voltage conversion at all** — the
Teensy is powered by its USB connection to the host PC, and the 12V column
feeds to panels are a straight passthrough (panels regulate 12V→3.3V locally).
A 5V underglow strip would be the first local conversion requirement on the
master board, and at multi-amp current a linear regulator is impractical
(12V→5V at 2.6A dissipates ~18W) — it would need a proper switching buck
converter, real magnetics on a board that also carries RS-485 and USB HS
signal traces. Going 12V-native avoids that entirely and reuses the panel
LED part number — but costs not being able to reuse the stock strip as-is.

If the stock strips turn out to be 5V and reuse matters once you've seen them
in hand, the buck-converter path is still completely viable; it's just one
more component vs. zero.

## Confirmed from the official Gen4+ manual wiring diagram (page 8, 2026-07-09)

The manual's own wiring diagram (not just the USB protocol capture) shows the underglow
strip's power and data wiring directly:

- **Yellow wire**: DC-DC converter's 12V terminal → LED strip (straight 12V, not the 5V
  output rail used elsewhere in the stock system)
- **Black wire**: converter GND terminal → LED strip
- **Pink wire**: MCU → LED strip data-in (single data line, no second/backup data wire)

This settles the voltage question without needing the physical teardown: underglow is
**12V-native**, not 5V. It also rules out WS2815-style strips — WS2815 uses a **4th wire**
(a backup data line specifically so the chain survives one dead LED) precisely because
running individually-addressable 5V-logic-class ICs at 12V needs that redundancy trick.
Three wires only (power, ground, single data) at 12V most likely means the common cheap
**12V/WS2811-grouped-by-3 strip** design instead: three LEDs wired in series per IC so a
WS2811-class chip can run directly off 12V. If so, addressing is per-group-of-3, not
per-individual-LED — a real design constraint for animation resolution, different from
this project's panel LEDs (WS2815, individually addressable).

**Why this is good news for our architecture:** it means underglow already wants exactly
the power topology this project already uses for panels (12V-native, no local step-down).
No buck converter needed on the master PCB after all — just a 12V tap (likely shareable
with the same rail feeding panel columns) plus one level-shifted data line off a spare
SN74AHCT125 channel. This resolves the "Power strategy" open question above in favor of
the 12V-native path without waiting on teardown.

**Grouping and segment layout confirmed by direct test** — see "LED grouping — confirmed
by direct test" above: grouped-by-3, 17 chunks left side + 17 right side + 10 back = 44.

**Still open (low priority, doesn't block design):**
- **Physical splice point** for the custom master (reopened 2026-07-10): the user
  inspected the stock wiring and found the underglow leads are crimped directly into a
  12-pin Dupont-style housing that plugs straight into the stock MCU — there is **no
  intermediate connector a replacement master could reuse**, so a user wanting underglow
  will need to splice. Picking the cleanest splice point needs the full pad + harness
  teardown (planned). Electrical parameters above are unaffected; the master's underglow
  DATA-out connector choice (BOM master table) is gated on this.

## Master PCB implications

- One more connector (12V + GND + data) + level-shifted data output
- 12V-native (confirmed): no local regulator or magnetics needed — spare
  SN74AHCT125 channel handles the data line, shifter VCC taps the Teensy's
  existing USB 5V rail
- USB protocol: strip-set command; RS-485 untouched
