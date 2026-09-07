# Bring-up log

Per-board results from `docs/PANEL_BRINGUP.md`. One row per board in the summary,
one detail section below it. Boards are named by the RP2040 unique ID the banner
prints (read out of `U307`) — silkscreen carries no serial, so this is the only
durable handle on a physical board.

Twenty brains and twenty carriers were fabricated; this file fills in as they are
brought up.

## Summary

| board ID | assembled | stage 0 | stage 1 | stage 2 | stage 3 | notes |
|---|---|---|---|---|---|---|
| `DE6558A69754442F` | 2026-09-07 | ✅ | ✅ | ✅ | — | first board powered; found the `INT_OUT` pull-down bug and the VBUS back-drive |

## `DE6558A69754442F` — first board

**2026-09-07.** Brain only: JLC-assembled, with `J305` (GCT USB4085 USB-C) and
the `J301`–`J304` interface sockets hand-soldered. No carrier, no 12V — USB is
the only power source.

### Stage 0 — meter, blank flash, in BOOTSEL

Board enumerated as `RPI-RP2` on USB with no BOOTSEL press, as expected from a
blank flash.

| point | signal | reading | verdict |
|---|---|---|---|
| `TP306` | `RS485_DE` | **35 mV** | hard low — internal pull-down parks the THVD1450 in receive. **Closes the rev-2 external pull-down candidate.** First-board-only check |
| `TP302` | +5VDC | **5.25 V** | VBUS through `U304` with no meaningful drop; 0.75 V of margin over `U301`'s 4.5 V minimum |
| `TP301` | +3.3VDC | **3.29 V** | `U302` AP7361C in regulation on the USB-only path |
| `TP303` | `RUN` | **3.3 V** | `R307` pull-up good |

### Stage 1 — bring-up firmware over USB CDC

Firmware `firmware/panel/c/bringup/`, built Sep 3 2026 10:32:02.

```
sysclk        : 125000000 Hz
flash JEDEC   : EF 40 16   OK (Winbond W25Q32JV, 4MB)
board id      : DE6558A69754442F
SENSE_12V     : LOW
```

| check | result | verdict |
|---|---|---|
| enumerate + run | banner printed | crystal, PLL, `U305`, `R305/R306`, QFN USB pads all good |
| sysclk | 125 MHz | `X301` 12 MHz and the PLL at the right frequency |
| flash JEDEC | `EF 40 16` | genuine Winbond W25Q32JV |
| `w` write/verify | **PASS** | `U307` programs and reads back |
| `w` capacity | **PASS — no aliasing, full 4 MB** | a 2 MB die substituted for the 4 MB part is ruled out; the animation slots in upper flash are real |
| `p` | `0 0 0` | expected — see below |
| `s` → `SENSE_12V` | `12V ABSENT`, raw 0, 0 edges | negative half of the 12V-sense test: no bridge to +3.3V, `R313`/`R314` not mis-stuffed |
| `s` → DIP | `15` (`0b1111`) | internal pull-ups on GPIO18–21 all working — there are no board pull-ups on these nets |
| `s` → `TERM_SENSE` | not terminated | GPIO10 internal pull-up working |
| `s` → FSR raw | South 12, West 12, North 12, East 12 | four live ADC channels, ~10 mV, well under the 187 float threshold. **Uniform across all four**, and stable rather than drifting — evidence `R311/R312/R315/R316` are stuffed |
| `i` asserted (dwell) | **9.6 mV** | drives a clean low; ~29 Ω on-resistance against the 10 k |
| `i` released (dwell) | **2.7 V** before fix → **3.29 V** after | ⚠ **found a firmware bug**, now fixed and re-verified — see below |

**`p` = `0 0 0` is correct and guaranteed, not board-specific.** With no 12V the
`+12VDC` net floats, so GPIO17 sees only `R314`'s 33 k to GND; the internal
pull-up (50–80 kΩ) yields 0.96–1.31 V, far below VIH = 2.145 V at either end of
spec. It therefore says nothing about this chip's pull strength. **The pull-down
risk is settled at stage 2 and nowhere else.** An earlier version of
`PANEL_BRINGUP.md` predicted `0 0 1`; that was an arithmetic slip, now fixed.

**`i` needs a pull-up to mean anything.** Metered bare, GPIO22 at `J302` pin 6
read **0.4 mV released, 0.2 mV asserted** — both zero, because standalone nothing
pulls `INT_OUT` up (the master's 10 k and the carrier's `R203`/`D201` are all
absent) and a 10 MΩ meter cannot show "floating". With 10 k tacked from `TP301`
to `J302` pin 6, and with the fast pulse phase correctly ignored (its 2.64 V
duty-cycle average is unreadable on a handheld), the steady dwells gave 9.6 mV
asserted and **2.7 V released**.

### 🐛 Firmware bug found: `INT_OUT` was never truly hi-Z

The 2.7 V is the finding. RP2040 pads reset with the internal pull-**down**
enabled (`PADS_BANK0_GPIOn_PDE_RESET = 1`) and `gpio_init()` does not touch
pulls, so `int_out_release()` gave hi-Z **plus a ~45 kΩ pull-down** fighting the
10 k pull-up: 3.3 × 45/55 = 2.70 V. **This is the documented GPIO17 trap on a
second pin.**

Consequence had it shipped: 2.7 V still clears the Teensy's ~2.31 V VIH, so it
would have worked — with **0.4 V of noise margin instead of 1.0 V on the sole
gameplay input path**. Not a failure, a silent halving of margin.

**Fixed** 2026-09-07 by adding `gpio_disable_pulls(PIN_INT_OUT)` to
`int_out_init()`. **Re-measured after the fix: released dwell = 3.29 V** — the
rail, i.e. true hi-Z — with the asserted dwell unchanged at ~9 mV. Full 1.0 V of
noise margin restored. **`firmware/panel/c/main.c` needs the same call when
ported.**

### Bonus: this board's internal pull-down measures ~45 kΩ

Solved from the divider above. The RP2040 spec is 50–80 kΩ, so this die sits at
or beyond the **strong** end. Same pad structure as GPIO17, which means **stage 2
`p` on this board is predicted to read `1 0 …`** — the pull-down masking 12V,
confirming the trap on real silicon instead of arithmetic. Worth checking against
the prediction when stage 2 runs.

### Not testable at this stage

`l` (LEDs) and `b` (debug LED `D202`) are carrier-side; the `U304` power-mux
hot-swap needs 12V; RS-485 needs peers. Stages 2 and 3.

### Stage 2 — carrier mated, 12V applied

Carrier hand-assembled and mated 2026-09-07. 12V measured **12.02 V** at the
carrier.

| check | result | verdict |
|---|---|---|
| 12V-only, no USB | `D202` blinking 1 Hz | **`U303` AMS1117 proven** — the whole 12V→5V→3.3V chain carries the board with no VBUS anywhere. The documented stage-2 order never reaches this |
| `TP302`, 12V only | **4.965 V** | AMS1117 output. Below VBUS's 5.25 V, so VBUS wins the mux whenever present |
| `TP301`, 12V only | **3.29 V** | `U302` in regulation off the AMS1117 |
| `U303` temperature | barely warm | ~0.2 W in SOT-223 with pour; as expected |
| mux handover | USB removed with 12V live → board kept running, beat unbroken | **`U304` hot-swap passes** in the direction that actually switches |
| `SENSE_12V` edge | `12V PRESENT` logged on applying 12V | **the divider proven** — an edge, not a level a solder bridge could fake |
| `TERM_SENSE` | tracks `SW202`; `TERMINATED` when thrown | pole B agrees with pole A |
| DIP | all OFF → 15, leftmost ON → 7, rightmost ON → 14 | **matches the documented map**: leftmost = bit 3, rightmost = bit 0, ON = 0. Kept as-is |
| FSR raw, unfitted | South 13, West 13, North 12, East 12 | all four channels alive across the carrier interface |
| `f` with 4 FSRs fitted | **all four map correctly**, 2026-09-07 | the edge pressed is the name that moves — `J201`/`J202`/`J203`/`J206` wired as documented. This is the carrier mix-up that would otherwise surface as a gameplay bug much later |
| FSR resting | **94–98** counts | slightly below the breadboard-era ~100–115. Comfortably under the 187 float threshold — ~2× headroom before a resting channel is flagged |
| FSR full press | **~4000** counts | at the top of the 12-bit range; marginally better swing than the prototype's ~3900 |
| ADC crosstalk | **none observed** under a hard press | see below — closes `ADC_DUMMY_READ` |
| `p` with 12V | **`1 1 1`** | pull-down did **not** mask 12V → this die's Rpd > 63.5 kΩ, the weak end. Predicted `1 0 …`; prediction was wrong, rule unchanged |
| `l` LED test | **all 25 lit**, red/green/blue/white | WS2815 chain, `U301`, `R301`, serpentine and the 12V rail under load |

### 🐛 Second finding: the brain back-drives its own VBUS

**USB will not enumerate if 12V is already applied.** Isolated over a 2×2
(cable orientation × 12V): both orientations work with no 12V, neither works with
12V. So it is not a CC joint.

Measured with 12V on and USB unplugged: **VBUS = 2.78 V, D+ = 3.3 V** — one diode
drop apart. The RP2040 asserts its D+ pull-up (1.5 k to 3.3 V) as soon as the USB
stack initialises, and `U305`'s I/O→VBUS ESD diode carries that onto the VBUS
net, which has **no bleeder** (the net is only `J305` VBUS, `U304` IN1 and
`U305`). 2.78 V is above vSafe0V, so a USB-C source refuses to attach.

Corollary seen on the way: pulling 12V from a board with USB attached-but-unpowered
kills it outright and it reboots once the host finally attaches. That looks like a
mux failure and is not one — there was simply nothing to hand over to.

**Rev 1: no rework. Connect USB before 12V** — already the documented stage-2
order, which is why this stayed hidden. Consequence worth knowing: **a laptop
cannot be plugged into a live panel**; drop 12V on that column first.

**Rev 2: candidate #3** — VBUS divider into a spare GPIO so firmware can
`tud_connect()` on real VBUS instead of the SDK's `VBUS_DETECT_OVERRIDE`. A plain
bleeder is the wrong fix: against a 1.5 k pull-up it needs to be <600 Ω and then
burns ~8 mA whenever USB is live.

### ✅ `ADC_DUMMY_READ = 0` confirmed on hardware

Pressing one channel hard moved none of the other three. The breadboard firmware
discarded the first conversion after every mux switch to absorb ~15 counts of
sample-cap charge injection, **at the cost of halving the sample rate**; its own
comment said to drop it once the ADC nodes had 10 nF caps to GND, which this
board has (`C324/C326/C329/C330`). That is now measured rather than assumed, so
**the gameplay firmware keeps the full sample rate** (102 kHz was the RP2040
bench figure) with no dummy read.

### Thresholds against real hardware

Resting 94–98, full press ~4000. The defaults hold with room to spare: press 500
/ release 400 sit well clear of both ends, and the 187 float threshold has ~2×
headroom over a healthy resting channel. No retuning needed. Note that unfitted
channels read ~12, so "unplugged" and "unpressed" remain electrically
indistinguishable by design — presence is config, not measurement.

### Still open for this board

- **Stage 3** — RS-485. Needs a second panel and the master; cannot be
  self-tested, since `DE`/`R̅E̅` are tied and transmitting disables the local
  receiver.
