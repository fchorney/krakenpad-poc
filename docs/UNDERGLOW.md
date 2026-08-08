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
- **Data**: Teensy GPIO11 → SN74AHCT1G125 single-gate buffer (VCC = Teensy USB 5V rail)
  → 330R series → J2 pin 1 (DATA position of the merged GND-tie/underglow
  screw terminal). Ground reference comes via the master's mandatory PSU GND
  tie (J2 pin 2).
- **Host-side**: a strip-set command in our USB protocol
  (`docs/USB_PROTOCOL.md`, `'L'` platform strip — write-on-command).
- Gating is UI/software config only ("underglow: on/off") — no sense pin; a
  12V-sense divider (like the panel's) is the known upgrade path if
  config-only gating proves annoying.

## Physical connection — CLOSED 2026-08-08 by the full pad teardown

The strips terminate in a **JST SM 3P, female**, carrying 12V / DATA / GND.
Our side is therefore a **JST SM 3P male** pigtail, no splicing required:

- 12V and GND come off the **12V star point**, exactly as stock does. Note that
  star point is **not** "PSU terminals" — the stock PSU is a brick with a single
  JST YL 2-way output and no terminal block or ground stud. The star point is
  physically the DC-DC converter's input screw terminals.
- DATA goes to master **J2 pin 1**.
- Master **J2 pin 2** is a separate lug to the PSU GND stud — the mandatory
  GND tie, required whether or not underglow is fitted.

Harness drawing and BOM: `hardware/harness/underglow.yml`.

**Pin order RESOLVED 2026-08-08, and it is the reverse of what was assumed:**
**pin 1 = GND, pin 2 = DATA, pin 3 = 12V.** The conductor *set* was right; the
*order* was not. Building to the old 12V/DATA/GND assumption would have put 12V
into the DATA pin and destroyed the first WS2811.

**Naming, per JST's convention** (see `hardware/harness/PARTS.md`): the pad-side
half is an **SMR-03V-B receptacle**, which in JST-speak carries *male pin*
contacts; the strips' own pigtail is the **SMP-03V-BC plug** with female sockets.
"SM 3P female" above refers to the strip side's contacts and is consistent with
this, but order by part number, not by gender word.

**This harness may not be needed at all.** The teardown found the pad *already*
has an SM 3P wired with 12V and GND from the 12V star point, with its DATA pin
fed by a single conductor on a **JST YLR-01V** — which is exactly what the stock
MCU drove. If that is reused, the replacement master needs to drive **one wire**
into a mating YLP-01V, not build an SM 3P pigtail. Decide during our own harness
design; see `hardware/harness/underglow.yml`.

This supersedes the earlier note that the underglow leads were crimped into a
12-pin Dupont-style housing with no intermediate connector to reuse. That was
wrong; there is a reusable connector.
