# Platform / Underglow LED Strips (optional feature)

Status: **optional — electrical spec fully resolved.** Nice-to-have; if it
isn't easy to fit, the plan is to just not connect it rather than force a fit.
Trimmed 2026-07-23 — superseded speculation (5V-bus theories, Daygreen-reuse
ideas, pre-confirmation brightness background) removed; full history in git.

## What the stock underglow is (all confirmed)

- **Driven directly by the stock MCU**, not on the panel data bus:
  `SMX_SetPlatformLights()` sends USB `'L' + strip_index + count + RGB` and
  the MCU handles it locally (confirmed by bus capture — no bus traffic during
  platform light updates).
- **12V-native, 3 wires** (confirmed from the official Gen4+ manual wiring
  diagram, page 8): 12V (yellow) and GND (black) come straight from the PSU
  side, data (pink) from the MCU. The 3-wire/12V combination rules out WS2815
  (which needs a 4th backup-data wire) — this is the common 12V
  WS2811-grouped-by-3 strip design.
- **44 addressable chunks per pad, each = 3 physical LEDs** (confirmed by
  direct test with `underglow_probe.cpp` — lighting one chunk lights three
  LEDs). Physical layout: 17 chunks left edge + 17 right edge + 10 back
  (players stand at the front) = 132 physical LEDs addressed as 44 groups.
- **Addressing resolution is per-group-of-3** — animation tooling and USB
  protocol commands should think in 44 positions (17/17/10), not 132 LEDs.
- Stock PSU: Gen 4 = 12V 9A, Gen 5 = 12V 15A (from the manual). This project
  targets **Gen 5 pads only**.

## Set-and-forget is native behavior

WS28xx-family LEDs latch their PWM state: write one frame and they hold the
color indefinitely with the data line quiet. Our default: write on command
only. Animation stays possible for free (send frames more often) but is not
the design center.

## Our design (as built on the master PCB — see `docs/MASTER_PCB.md`)

- **Power**: strips keep taking 12V/GND from the PSU lugs as stock does — the
  master is not in the underglow power path at all. No regulator, no
  magnetics on the master.
- **Data**: Teensy GPIO11 → spare SN74AHCT125 gate (VCC = Teensy USB 5V rail)
  → 330R series → J4 pin 1 (DATA position of the merged GND-tie/underglow
  screw terminal). Ground reference comes via the master's mandatory PSU GND
  tie (J4 pin 2).
- **Host-side**: a strip-set command in our USB protocol
  (`docs/USB_PROTOCOL.md`, `'L'` platform strip — write-on-command).
- Gating is UI/software config only ("underglow: on/off") — no sense pin; a
  12V-sense divider (like the panel's) is the known upgrade path if
  config-only gating proves annoying.

## Still open (low priority, does not block anything)

**Physical splice point.** The stock underglow leads are crimped directly into
a 12-pin Dupont-style housing that plugs into the stock MCU — there is no
intermediate connector a replacement master could reuse, so a user wanting
underglow must splice. Picking the cleanest splice point needs the full
pad/harness teardown; it gates only the final choice of connector/pigtail at
J4, not any electrical parameter.
