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
  WS2811-grouped-by-3 strip design. **Confirmed against the WS2811 datasheet
  2026-08-16 — see "Current draw" below.** Note the part identification is
  still an *inference* from three observations (12V, 3 wires, probe-confirmed
  groups of 3); no one has read a part marking. The architecture is certain
  even if the exact IC vendor is not.
- **44 addressable chunks per pad, each = 3 physical LEDs** (confirmed by
  direct test with `underglow_probe.cpp` — lighting one chunk lights three
  LEDs). Physical layout: 17 chunks left edge + 17 right edge + 10 back
  (players stand at the front) = 132 physical LEDs addressed as 44 groups.
- **Addressing resolution is per-group-of-3** — animation tooling and USB
  protocol commands should think in 44 positions (17/17/10), not 132 LEDs.
- Stock PSU: Gen 4 = 12V 9A, Gen 5 = 12V 15A (from the manual). This project
  targets **Gen 5 pads only**.

## Current draw — resolved 2026-08-16 against the WS2811 datasheet

**2.44 A at full white, all 44 groups.** The `44 × 60 mA = 2.6 A` figure this
repo already carried is correct and about 6% conservative — unlike the panel
LED figure, which was wrong by 3× and had to be corrected the same week. This
one needed no correction.

### How the WS2811 actually draws current

From the Worldsemi WS2811 datasheet (pages 2–3):

- The pinout is **`OUTR` / `OUTG` / `OUTB` — three separate output pins**, each
  an independent constant-current sink.
- Electrical characteristics: `I_OL`, condition `ROUT`, **Typ 18.5 mA**. There
  is no Min/Max given.
- Absolute max `V_OUT` = **12 V** per output; `V_DD` is only 6–7 V.
- Pin 7 `SET` is the **speed-mode** pin (VDD = low speed, floating = high
  speed) — *not* a current-setting pin. The datasheet's "add a resistance to
  IC VDD" note applies to running from a **24 V** supply. **WS2811 output
  current is fixed internally; there is no REXT.** Several third-party guides
  get this wrong.

At 12 V each channel drives **3 same-colour dies in series** — that is exactly
why one IC addresses 3 physical LEDs, and why the pad's addressing granularity
is 44 groups rather than 132 pixels.

So **full white per group = 3 channels × 18.5 mA = 55.5 mA**, and
44 × 55.5 mA = **2.44 A**.

Sanity-checked against two independent commercial strip densities, which is the
check that would have caught the WS2815 error had it been applied there:

| Density | ICs/m | Predicted | Vendors state |
|---|---|---|---|
| 60 LED/m | 20 | 1.11 A/m (13.3 W/m) | 1.2 A/m (14.4 W/m) ✓ |
| 90 LED/m | 30 | 1.67 A/m (20.0 W/m) | max 20 W/m ✓ |

### ⚠ The underglow and the panels behave in OPPOSITE ways

This is the single most important fact on this page for firmware and animation
work, and it is genuinely counterintuitive because both parts are from the same
manufacturer's family.

| | Underglow (WS2811) | Panels (WS2815) |
|---|---|---|
| Topology | 3 **independent** current sinks, each feeding 3 series dies | 3 dies **in series** in one package, unlit ones shorted out |
| Pure red vs full white | **red = ⅓ of white** | **red = exactly white** |
| Does colour choice reduce current? | **yes, linearly** | **no, never — only PWM duty does** |
| Current model | `I = 18.5mA × (r+g+b)/255` per group | `I = 8.7mA × max(r,g,b)/255` per pixel |

**A single global power policy is therefore wrong.** One written on the panels'
rule over-estimates underglow by up to 3×; one written on the underglow's rule
under-estimates the panels catastrophically. Model the two separately.

The panel `max()` model is not a guess — it is confirmed on four points from the
2026-08-16 bench run (`hardware/harness/README.md` → "Power budget"):

| Commanded | `max()` predicts | Measured |
|---|---|---|
| (255,255,255) | 8.7 mA | 8.7 mA ✓ |
| (255,0,0) | 8.7 mA | 8.7 mA ✓ |
| (128,128,128) | 4.35 mA | 4.6 mA ✓ |
| one channel @ 64 | 2.18 mA | 2.2 mA ✓ |

The `(128,128,128)` row is load-bearing: staggered PWM phases would have given
8.7 mA there. It gave half, so the phases are aligned and `max()` holds.

Secondary consequence, thermal rather than electrical: WS2811 is a **linear**
part, dropping ~6 V across the IC on a red channel (3 × ~2.0 V dies) versus
~2.4 V on green/blue (3 × ~3.2 V). So on the underglow, red is simultaneously
the *lowest* current and the *least efficient per unit light* — a third distinct
behaviour from the panels, where red and white draw identically but red wastes
the most.

### Sizing figure

Use **20 mA/channel**, not the 18.5 mA typ — the datasheet gives no Max, so
binning can go over as easily as under, and this matches the 20 mA/pixel
headroom convention already used for the panels. That puts underglow at
**2.64 A** for sizing purposes.

### Deferred: a firmware power budget

An earlier plan had the master scale LED output to hold total pad current under
a connector limit, because the design briefly tried to reuse the stock PSU tail
unmodified. **That constraint was removed 2026-08-16** (see
`hardware/harness/12v-trunk.yml`) and the budget is no longer needed for safety.
Recorded here because the model is worth keeping if it is ever wanted:

- The two formulas above give **exact** per-frame current, not an estimate.
- Underglow is the natural place to absorb any cap: it is decorative, and it is
  **master-local**, so the master governs it completely. The panels' local
  fallback animations (played after a 100 ms RS-485 timeout) are by definition
  *not* reachable by a master-side budget — so any panel-side cap would have to
  be compiled into the default animation slots to be real.
- Because WS2811 current scales with channel count, a **single-colour** underglow
  already costs ⅓ of white. Any "limit by channels driven" policy would almost
  never bind in practice, since typical underglow use is one accent colour.

Sources: [WS2811 datasheet (Worldsemi)](https://cdn-shop.adafruit.com/datasheets/WS2811.pdf),
[mirror (TME)](https://www.tme.eu/Document/26d574b43ad9ddaffa4d5bcd140ec145/WS2811.pdf).

## Set-and-forget is native behavior

WS28xx-family LEDs latch their PWM state: write one frame and they hold the
color indefinitely with the data line quiet. Our default: write on command
only. Animation stays possible for free (send frames more often) but is not
the design center.

## Our design (as built on the master PCB — see `docs/MASTER_PCB.md`)

- **Power**: strips take 12V/GND from the Wago fan-out (`hardware/harness/12v-trunk.yml`) — the
  master is not in the underglow power path at all. No regulator, no
  magnetics on the master.
- **Data**: Teensy GPIO11 → SN74AHCT1G125 single-gate buffer (VCC = Teensy USB 5V rail)
  → 330R series → J2 pin 1 (DATA position of the merged GND-tie/underglow
  screw terminal). Ground reference comes via the master's mandatory GND
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
- Master **J2 pin 2** is a separate lead to a GND port on the Wago fan-out — the mandatory
  GND tie, required whether or not underglow is fitted.

Harness drawing and BOM: `hardware/harness/underglow.yml`.

**Pin order RESOLVED 2026-08-08, and it is the reverse of what was assumed:**
**pin 1 = GND, pin 2 = DATA, pin 3 = 12V.** The conductor *set* was right; the
*order* was not. Building to the old 12V/DATA/GND assumption would have put 12V
into the DATA pin and destroyed the first WS2811.

**Naming, per JST's convention** (see `stock-smx/PARTS.md`): the pad-side
half is an **SMR-03V-B receptacle**, which in JST-speak carries *male pin*
contacts; the strips' own pigtail is the **SMP-03V-BC plug** with female sockets.
"SM 3P female" above refers to the strip side's contacts and is consistent with
this, but order by part number, not by gender word.

**RESOLVED 2026-08-16: we DO build the SM 3P pigtail.** An earlier note here
said this harness "may not be needed at all", because the teardown found the pad
already had an SM 3P wired with 12V/GND from the star point and DATA on a
**JST YLR-01V**. That was **conditional on reusing the stock harness segment**,
and we do not — all SMX wiring is removed, and the star point itself is replaced
by the Wago fan-out (`hardware/harness/12v-trunk.yml`). The stock `SMR-03V-B`
goes with that wiring.

**What survives is only the strips' own moulded `SMP-03V-BC` pigtail**, so we
must build its mate:

- **`SMR-03V-B` housing — LCSC `C157907`**
- **`SMM-003T-P0.5` pin contact — LCSC `C385123`**, ×3 plus spares

12 V and GND come from the Wago fan-out; DATA from master J2 pin 1. No
`YLP-01V` is involved.

This supersedes the earlier note that the underglow leads were crimped into a
12-pin Dupont-style housing with no intermediate connector to reuse. That was
wrong; there is a reusable connector.
