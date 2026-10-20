# Bring-up log

Per-board results from `docs/PANEL_BRINGUP.md` and `docs/MASTER_BRINGUP.md`. One
row per board in the summary, one detail section below it.

**Panels** are named by the RP2040 unique ID the banner prints (read out of
`U307`) — silkscreen carries no serial, so this is the only durable handle on a
physical board.

⚠ **Masters have no equivalent, and the Teensy's serial is NOT a substitute.**
The Teensy is socketed, so its serial identifies *the MCU module, not the board
it is plugged into* — the same Teensy can move between masters, and the
prototype Teensy was in fact reused for master #1. Masters are therefore numbered
by hand, and **the number must be written on the board** (silkscreen carries no
serial here either). The Teensy serial is recorded alongside it only to say which
module was fitted at the time.

Twenty brains and twenty carriers were fabricated, and five master boards; this
file fills in as they are brought up.

**SWD (`tools/swd_check.py`, from 2026-10-18):** all nine assembled boards pass —
DPIDR `0x0bc12477`, both cores, CHIP_ID `0x20002927` on every one. The
SWD-only erase-and-recover test was run once, on `462D`.

**Plan from 2026-10-18:** boards go through stages 0–2 one at a time, then stage
3 runs once as a full nine-panel bus with the master instead of pairing boards
up. **Done 2026-10-19 for boards 1–9** (master #2 section).

## Summary

| board ID | assembled | stage 0 | stage 1 | stage 2 | stage 3 | SWD | notes |
|---|---|---|---|---|---|---|---|
| `DE6558A69754442F` | 2026-09-07 | ✅ | ✅ | ✅ | ✅ | ✅ | first board powered; found the `INT_OUT` pull-down bug and the VBUS back-drive. Stage 3 2026-10-02 at ID 3 with master #2: `F`, `I`, `S` clean, `A` + `E` closed loop by eye |
| `DE6558A6977D462D` | 2026-10-02 | ✅ | ✅ | ✅ | ✅ | ✅ | second board; clean first time. ID 1 on 10-01, **ID 0 from 10-02**. `i` skipped, INT proven on the bus with master #2 instead |
| `DE6558A6977E3A2D` | 2026-10-18 | ✅ | ✅ | ✅ | ✅ | ✅ | third brain. `p` read `0 0 1` at stage 1: a strong pull-up, not a fault (`R314` metered 31 k in circuit). Corrected the stage-1 `0 0 0` "guarantee" |
| `DE6558A697556A2F` | 2026-10-18 | ✅ | ✅ | ✅ | ✅ | ✅ | fourth brain |
| `DE6558A697545C2F` | 2026-10-18 | ✅ | ✅ | ✅ | ✅ | ✅ | fifth brain. FSR reads **0** unmated and rests at **85** with an FSR (others 10–18 / 94–102): **a negative ADC offset of ~15 counts**, harmless. `ADC_AVDD` good at 3.28 V |
| `DE6558A69784622D` | 2026-10-18 | ✅ | ✅ | ✅ | ✅ | ✅ | sixth brain; clean |
| `DE6558A69776582F` | 2026-10-18 | ✅ | ✅ | ✅ | ✅ | ✅ | seventh brain; clean. Highest ADC offset so far (rest 108) |
| `DE6558A697543C2F` | 2026-10-18 | ✅ | ✅ | ✅ | ✅ | ✅ | eighth brain; clean |
| `DE6558A6977B342D` | 2026-10-18 | ✅ | ✅ | ✅ | ✅ | ✅ | ninth brain. First board flashed after the bootloader split: loading the bring-up image alone left it in BOOTSEL (see detail) |

### Masters

| board | assembled | stage 1 | stage 2 | stage 3 | Teensy fitted | notes |
|---|---|---|---|---|---|---|
| **master #1** | 2026-09-08 | ✅ | ✅ INT | — | `20432520` (ex-prototype) | SMD hand-soldered with Sn42Bi57Ag1; INT path proven end to end with one panel; `r` closed the single-GPIO-port open item |
| **master #2** | 2026-10-02 | ✅ | ✅ INT | ✅ | `19755070` (new) | all-leaded build: Chip Quik Sn63/Pb37 paste through the printed stencils, no added flux. `S` bus fault found and fixed in master firmware 2026-10-02; first two-panel bus, see below. **Eight-panel bus and bus updates 2026-10-18; nine-panel bus, gameplay firmware and USB joystick 2026-10-19** |

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
| `s` → `SENSE_12V` | `12V ABSENT`, raw 0, 0 edges | negative half of the 12V-sense test: no bridge to +3.3V. ~~`R313`/`R314` not mis-stuffed~~: a floating pin also reads 0, so this can't prove `R314` (corrected 2026-10-18) |
| `s` → DIP | `15` (`0b1111`) | internal pull-ups on GPIO18–21 all working — there are no board pull-ups on these nets |
| `s` → `TERM_SENSE` | not terminated | GPIO10 internal pull-up working |
| `s` → FSR raw | South 12, West 12, North 12, East 12 | four live ADC channels, ~10 mV, well under the 187 float threshold. **Uniform across all four**, and stable rather than drifting — evidence `R311/R312/R315/R316` are stuffed |
| `i` asserted (dwell) | **9.6 mV** | drives a clean low; ~29 Ω on-resistance against the 10 k |
| `i` released (dwell) | **2.7 V** before fix → **3.29 V** after | ⚠ **found a firmware bug**, now fixed and re-verified — see below |

**`p` = `0 0 0` is correct here, but it is NOT guaranteed** (corrected
2026-10-18, see `DE6558A6977E3A2D`). With no 12V the `+12VDC` net floats, so
GPIO17 sees only `R314`'s 33 k to GND; the internal pull-up (50–80 kΩ) yields
0.96–1.31 V. ~~That is far below VIH = 2.145 V at either end of spec, so `0` is
guaranteed.~~ **Wrong: it ignored VIL = 1.155 V.** Any pull-up under ~61 kΩ puts
the pin in the undefined band between VIL and VIH, where it can read either way.
This board's pull-up was at the weak end, so it read 0; the third brain read
`0 0 1` with a good `R314`. **The pull-down risk is settled at stage 2 and
nowhere else.**

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
ported.** (Done: the 2026-10-18 gameplay firmware disables pulls on both
`INT_OUT` and `SENSE_12V`.)

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

### Stage 3 — on the bus with master #2, 2026-10-02

At **ID 3**, INT on `L` (`J8`), sharing the bus with `DE6558A6977D462D` at ID 0.
Full run under **master #2 → 2026-10-02**. For this board:

| check | result | verdict |
|---|---|---|
| `'F'` polls | answered every poll addressed to ID 3 | ✅ |
| `I` | ID 3 → `L` (`J8`), 2031 µs low | ✅ ack + INT wire + `INT_OUT` |
| `S 3 500 400` × 50 (fixed master) | 50/50 acked | ✅ `'C'`→`'c'` path |
| counters over the fixed-master runs | **0 CRC, 0 uart, 0 turnaround** in 29,466 frames | ✅ |

| `A` + `E`, real presses | panel lit **green** on press (`L`'s press colour) | ✅ FSR → INT → master → `'L'` → LEDs, by eye |

### Still open for this board

- ~~**`A` with real presses.**~~ Done 2026-10-02, closed loop by eye; see
  master #2 → 2026-10-02. Presses were not counted, unlike board 2's 36.

---

## `DE6558A6977D462D` — second board

**2026-10-02.** First board built from `docs/BUILD_CHECKLIST.md` rather than the
full bring-up docs, so the first-board-only checks were not repeated. Bring-up
firmware built Sep 8 2026 15:08:34.

### Stage 0 and 1 — brain alone on USB

| check | result | verdict |
|---|---|---|
| blank board on USB | mounted as `RPI-RP2`, no button | ✅ |
| `v` sysclk | 125 MHz | ✅ |
| `v` flash JEDEC | `EF 40 16` | ✅ genuine W25Q32JV |
| `w` | write/verify **PASS**, capacity **PASS — no aliasing, full 4 MB** | ✅ |
| `s` → `SENSE_12V` | `12V ABSENT`, raw 0, 0 edges | ✅ |
| `s` → DIP / `TERM_SENSE` | 15 / not terminated | ✅ all five internal pull-ups good |
| `s` → FSR raw | South 16, West 16, North 17, East 16 | ✅ uniform (board 1 read 12) |

### Stage 2 — carrier mated, USB then 12 V

| check | result | verdict |
|---|---|---|
| `s` → `SENSE_12V` | `12V PRESENT`, raw 1, 1 edge | ✅ the edge is 12 V arriving after USB |
| `s` → DIP | 1 (`0b0001`), switches up-up-up-down | ✅ matches what was set |
| `s` → `TERM_SENSE` | TERMINATED | ✅ matches `SW202` |
| `d` | each switch flipped one bit and back: bit 3, 2, 1, 0 in switch order 1–4 | ✅ no bridges or opens on GPIO18–21 |
| FSR, East connected only | rests **97–98**; the three empty channels 15–18 | ✅ inside board 1's 94–98 |
| `f`, East pressed by hand, 15 s | East 96 → peak **3676**; South/West/North stayed 15–18 throughout | ✅ right channel, no crosstalk. Peak below ~4000 is hand press force |
| `l` | all 25 lit, red/green/blue/white | ✅ |
| `p` (run by habit; first-board-only) | `1 1 1` | same as board 1, nothing hangs on it |
| `i` | **skipped** | INT to be proven end to end with the master at stage 3 |
| USB pulled, 12 V live | board kept running | ✅ `U304` handover |
| `TP302` / `TP301`, 12 V only | **4.98 V / 3.304 V** | ✅ `U303` and `U302` in regulation (board 1: 4.965 / 3.29) |
| cold start on 12 V only | board came up | ✅ `U303` starts the board, not just holds it |
| back to USB: 12 V off, USB in, 12 V on | enumerated; `s` unchanged | ✅ |

Only one FSR was connected for this run, so South, West and North were
proven as live ADC channels at rest but not with a sensor pressed. The FSR
connectors themselves (`J201`, `J202`, `J206`) have not carried a sensor yet.

### Stage 3 — on the bus with master #2

See **master #2 → Stage 3** below for the full run. For this board: responder
at address 1, `I` passes on `U` (`J10`), `S` acks, all four command paths
answered, and 36 hand presses on East became 36 clean PRESS/RELEASE pairs at
the master through `A` mode. Counters at steady state: **0 CRC, 0 overruns,
0 uart errors, 1 turnaround** (at enable).

### SWD — 2026-10-18, first board through `J209`

First time the SWD path (`J209` → `J212`/`J303` → `U306` pins 24/25) has been
exercised on any board. Probe: Pico 1 running debugprobe v2.3.1. Procedure in
`docs/PANEL_BRINGUP.md` → "SWD — `J209`".

| check | result | verdict |
|---|---|---|
| `tools/swd_check.py` → DPIDR | `0x0bc12477` | ✅ RP2040 SW-DP |
| `tools/swd_check.py` → cores | core0 + core1 examined | ✅ multidrop selection works |
| `tools/swd_check.py` → CHIP_ID | `0x20002927`, revision field 2 | ✅ |
| SWD-only erase, `flash erase_sector 0 0 last` | `RPI-RP2` appeared on USB | ✅ boot area really blank |
| SWD-only reprogram, `program … verify reset` | 57,344 bytes, **Verified OK**; 1 Hz heartbeat on `D202` resumed | ✅ the image SWD wrote boots |

The recovery test is **once per method, not per board**: it adds OpenOCD's flash
algorithm and `U307`, and stage 1's `w` already proves `U307` on every board.

`D202` stayed lit between the erase and the reprogram. Likely explanation, not
confirmed: the probe's reset line (GP1) is not wired, so OpenOCD's `reset` is a
core reset requested over SWD, which leaves the GPIO block, and so GPIO16's last
driven level, alone. The bootrom never touches GPIO16. Harmless.

### Still open for this board

- The three FSR connectors not yet exercised with a sensor (`J201` W, `J202` S,
  `J206` N).

## `DE6558A6977E3A2D` — third brain

**2026-10-18.** Brain alone; carrier not yet mated. Bring-up firmware built
Sep 8 2026 15:08:34 (matches source, tree clean).

### Stage 0 — blank, in BOOTSEL

| point | signal | reading | |
|---|---|---|---|
| — | enumerate | mounted as `RPI-RP2`, no button | ✅ |
| `TP302` | +5VDC | **5.258 V** | ✅ |
| `TP301` | +3.3VDC | **3.292 V** | ✅ |
| `TP303` | `RUN` | **3.289 V** | ✅ |

### Stage 1 — bring-up firmware over USB

| check | result | verdict |
|---|---|---|
| `v` sysclk | 125 MHz | ✅ |
| `v` flash JEDEC | `EF 40 16` | ✅ genuine W25Q32JV |
| `w` | write/verify **PASS**, capacity **PASS — no aliasing, full 4 MB** | ✅ |
| `s` → `SENSE_12V` | `12V ABSENT`, raw 0, 0 edges | ✅ |
| `s` → DIP / `TERM_SENSE` | 15 / not terminated | ✅ all five internal pull-ups good |
| `s` → FSR raw | South 10, West 10, North 11, East 10 | ✅ uniform |
| `p` | **`0 0 1`**, identical over 10 repeats | ✅ strong pull-up, not a fault — see below |

**`p` = `0 0 1` looked like an open `R314` and is not.** `R314` is fitted,
marked `333`, and metered **~31 k in circuit** with USB unplugged (33 k in
parallel with the paths through `D303` and `R313`). The earlier claim that
stage 1 *must* read `0 0 0` compared the pin only against VIH and ignored
VIL = 1.155 V. A pull-up under ~61 kΩ puts the pin at 1.16–1.31 V, inside the
undefined band, and this die reads 1 there. So this pull-up is at the strong end
of 50–80 kΩ, where board 1's was at the weak end.

Two consequences, both recorded in `PANEL_BRINGUP.md`:
- **The stage-1 pull-up column is not pass/fail.** It reads `1` with a strong
  pull-up *and* with an open `R314`, so on its own it can't tell them apart.
- **`pulls off : 0` does not prove `R314` either.** A floating pin usually reads 0
  too. **Only the ohmmeter proves `R314`.** (An earlier version of this note said
  stage 2's falling edge proves it; it doesn't, because the pin falls with an
  open `R314` too.)

### Stage 2 — carrier mated, USB then 12 V

| check | result | verdict |
|---|---|---|
| `s` → `SENSE_12V` | `12V PRESENT`, raw 1 | ✅ |
| 12 V unplugged, USB kept | `12V ABSENT`, raw 0, edge count +1 | ✅ falling edge clean |
| `s` → DIP | 15 (`0b1111`), all switches off | ✅ matches what was set |
| `s` → `TERM_SENSE` | TERMINATED | ✅ matches `SW202` |
| `d` | each switch flipped its own bit and back | ✅ |
| FSR, East connected only | rests **~96**, press peak **~3600** by hand | ✅ inside board 1's 94–98. South/West/North stayed at rest during the press, no crosstalk |
| `l` | all 25 lit | ✅ |
| `p`, 12 V live | **`1 1 1`** | same as boards 1 and 2: pull-down >63.5 kΩ, the weak end. **The strong pull-up does not imply a strong pull-down** |
| `p`, mated, 12 V unplugged | `0 0 1` | expected: the disconnected 12V net floats, so this is the standalone case |
| USB pulled, 12 V live | board kept running | ✅ `U304` handover |
| `TP302` / `TP301`, 12 V only | **5.034 V / 3.293 V** | ✅ `U303` and `U302` in regulation (board 1: 4.965 / 3.29, board 2: 4.98 / 3.304) |
| cold start on 12 V only | board came up | ✅ |
| back to USB: 12 V off, USB in, 12 V on | reconnected | ✅ |

### Still open for this board

- ~~Stage 3 (RS-485, needs the master and a second panel).~~ Done 2026-10-19.
- ~~`i` not run; INT to be proven on the bus at stage 3, as for board 2.~~ INT
  proven on the bus by the nine-panel `I` self-test, 2026-10-19.
- South, West and North FSR connectors not yet exercised with a sensor.

## `DE6558A697556A2F` — fourth brain

**2026-10-18.** Same bring-up build (Sep 8 2026 15:08:34).

### Stage 0 and 1 — brain alone on USB

| check | result | verdict |
|---|---|---|
| blank board on USB | mounted as `RPI-RP2`, no button | ✅ |
| `TP302` / `TP301` / `TP303` | **5.257 / 3.297 / 3.294 V** | ✅ |
| `v` sysclk | 125 MHz | ✅ |
| `v` flash JEDEC | `EF 40 16` | ✅ genuine W25Q32JV |
| `w` | write/verify **PASS**, capacity **PASS — no aliasing, full 4 MB** | ✅ |
| `s` → `SENSE_12V` | `12V ABSENT`, raw 0, 0 edges | ✅ |
| `s` → DIP / `TERM_SENSE` | 15 / not terminated | ✅ all five internal pull-ups good |
| `s` → FSR raw | South 17, West 18, North 18, East 17 | ✅ uniform |
| `p` | `0 0 1` | record only |

### Stage 2 — carrier mated, USB then 12 V

| check | result | verdict |
|---|---|---|
| `SENSE_12V` | absent ↔ present both directions | ✅ |
| `p`, 12 V live | `1 1 1` | record only |
| `d` | each switch flipped its own bit and back | ✅ |
| FSR, East | rests **~102**, press peak **~3800** | ✅ rest a little above boards 1–3 (94–98), still far under the 500 press threshold. South/West/North stayed at rest, no crosstalk |
| `l` | all 25 lit | ✅ |
| USB pulled, 12 V live | board kept running, no reset | ✅ `U304` handover |
| `TP302` / `TP301`, 12 V only | **5.011 V / 3.298 V** | ✅ |
| cold start on 12 V only | board came up | ✅ |
| back to USB: 12 V off, USB in, 12 V on | reconnected | ✅ |

### Still open for this board

- ~~Stage 3 (nine-panel bus).~~ Done 2026-10-19 (master #2 section).
- South, West and North FSR connectors not yet exercised with a sensor.

## `DE6558A697545C2F` — fifth brain

**2026-10-18.** Same bring-up build (Sep 8 2026 15:08:34).

### Stage 0 and 1 — brain alone on USB

| check | result | verdict |
|---|---|---|
| blank board on USB | mounted as `RPI-RP2`, no button | ✅ |
| `TP302` / `TP301` / `TP303` | **5.257 / 3.312 / 3.308 V** | ✅ `TP301` highest so far (others 3.29–3.297), inside AP7361C tolerance |
| `v` sysclk | 125 MHz | ✅ |
| `v` flash JEDEC | `EF 40 16` | ✅ genuine W25Q32JV |
| `w` | write/verify **PASS**, capacity **PASS — no aliasing, full 4 MB** | ✅ |
| `s` → `SENSE_12V` | `12V ABSENT`, raw 0, 0 edges | ✅ |
| `s` → DIP / `TERM_SENSE` | 15 / not terminated | ✅ all five internal pull-ups good |
| `s` → FSR raw | **South 0, West 0, North 0, East 0**, steady over 9 reads (one stray South = 1) | ⚠ every earlier board read 10–18 |
| `p` | `0 0 1` | record only |

**All four FSR channels read 0 at rest.** Because it hits all four channels at once,
the shared ADC supply was suspected first: `ADC_AVDD` is fed by `R308` 200 Ω from
+3.3VDC into `C318` 2.2 µF. **Metered 3.28 V on the `ADC_AVDD` side of `R308`**
against a 3.312 V rail, ~30 mV across 200 Ω (~150 µA, ordinary ADC draw), so the
supply is good. Leading explanation: **a negative ADC offset on this die**, so a
grounded input clips at 0 where others read 10–18. Not yet proven; stage 2 with
a real FSR is the test.

**Confirmed at stage 2: it is offset.** With an FSR, East rests at **85** and
presses to **~3800**. The other boards rest at 94–102, so this die reads about
**15 counts low**, the same size as the 10–18 the others show unmated with no
sensor, now clipped to 0 here. A full-scale press is unaffected. The 500/400
press/release thresholds sit well clear either way. Per-channel calibration in
flash config would absorb it.

### Stage 2 — carrier mated, USB then 12 V

| check | result | verdict |
|---|---|---|
| `SENSE_12V` | absent ↔ present both directions | ✅ |
| `p`, 12 V live | `1 1 1` | record only |
| `d` | every switch correct | ✅ |
| FSR, East | rests **~85**, press peak **~3800**; South/West/North stayed at rest, no crosstalk | ✅ low rest is the ADC offset above |
| `l` | all 25 lit | ✅ |
| USB pulled, 12 V live | board kept running, no reset | ✅ `U304` handover |
| `TP302` / `TP301`, 12 V only | **5.006 V / 3.313 V** | ✅ |
| cold start on 12 V only | board came up | ✅ |
| back to USB: 12 V off, USB in, 12 V on | reconnected | ✅ |

### Still open for this board

- ~~Stage 3 (nine-panel bus).~~ Done 2026-10-19 (master #2 section).
- South, West and North FSR connectors not yet exercised with a sensor.

## `DE6558A69784622D` — sixth brain

**2026-10-18.** Same bring-up build (Sep 8 2026 15:08:34).

### Stage 0 and 1 — brain alone on USB

| check | result | verdict |
|---|---|---|
| blank board on USB | mounted as `RPI-RP2`, no button | ✅ |
| `TP302` / `TP301` / `TP303` | **5.257 / 3.297 / 3.295 V** | ✅ |
| `v` sysclk | 125 MHz | ✅ |
| `v` flash JEDEC | `EF 40 16` | ✅ genuine W25Q32JV |
| `w` | write/verify **PASS**, capacity **PASS — no aliasing, full 4 MB** | ✅ |
| `s` → `SENSE_12V` | `12V ABSENT`, raw 0, 0 edges | ✅ |
| `s` → DIP / `TERM_SENSE` | 15 / not terminated | ✅ all five internal pull-ups good |
| `s` → FSR raw | South 11, West 12, North 12, East 11 | ✅ uniform |
| `p` | `0 0 1` | record only |

### Stage 2 — carrier mated, USB then 12 V

| check | result | verdict |
|---|---|---|
| `SENSE_12V` | absent ↔ present both directions | ✅ |
| `s` → `TERM_SENSE` | matches `SW202` | ✅ |
| `p`, 12 V live | `1 1 1` | record only |
| `d` | every switch correct | ✅ |
| FSR, East | rests **~98**, press peak **~3800**; South/West/North stayed at rest, no crosstalk | ✅ |
| `l` | all 25 lit | ✅ |
| USB pulled, 12 V live | board kept running, no reset | ✅ `U304` handover |
| `TP302` / `TP301`, 12 V only | **5.033 V / 3.299 V** | ✅ |
| cold start on 12 V only | board came up | ✅ |
| back to USB: 12 V off, USB in, 12 V on | reconnected | ✅ |

### Still open for this board

- ~~Stage 3 (nine-panel bus).~~ Done 2026-10-19 (master #2 section).
- South, West and North FSR connectors not yet exercised with a sensor.

## `DE6558A69776582F` — seventh brain

**2026-10-18.** Same bring-up build (Sep 8 2026 15:08:34).

### Stage 0 and 1 — brain alone on USB

| check | result | verdict |
|---|---|---|
| blank board on USB | mounted as `RPI-RP2`, no button | ✅ |
| `TP302` / `TP301` / `TP303` | **5.257 / 3.301 / 3.298 V** | ✅ |
| `v` sysclk | 125 MHz | ✅ |
| `v` flash JEDEC | `EF 40 16` | ✅ genuine W25Q32JV |
| `w` | write/verify **PASS**, capacity **PASS — no aliasing, full 4 MB** | ✅ |
| `s` → `SENSE_12V` | `12V ABSENT`, raw 0, 0 edges | ✅ |
| `s` → DIP / `TERM_SENSE` | 15 / not terminated | ✅ all five internal pull-ups good |
| `s` → FSR raw | South 21, West 23, North 21, East 21 | ✅ uniform; highest unmated offset so far (range now 0–23 across seven dies) |
| `p` | `0 0 1` | record only |

### Stage 2 — carrier mated, USB then 12 V

| check | result | verdict |
|---|---|---|
| `SENSE_12V` | absent ↔ present both directions | ✅ |
| `s` → `TERM_SENSE` | matches `SW202` | ✅ |
| `p`, 12 V live | `1 1 1` | record only |
| `d` | every switch correct | ✅ |
| FSR, East | rests **~108**, press peak **~3850**; South/West/North stayed at rest, no crosstalk | ✅ highest rest so far, matching the highest unmated offset. `5C2F` (offset 0) rests at 85 |
| `l` | all 25 lit | ✅ |
| USB pulled, 12 V live | board kept running, no reset | ✅ `U304` handover |
| `TP302` / `TP301`, 12 V only | **5.043 V / 3.303 V** | ✅ |
| cold start on 12 V only | board came up | ✅ |
| back to USB: 12 V off, USB in, 12 V on | reconnected | ✅ |

### Still open for this board

- ~~Stage 3 (nine-panel bus).~~ Done 2026-10-19 (master #2 section).
- South, West and North FSR connectors not yet exercised with a sensor.

## `DE6558A697543C2F` — eighth brain

**2026-10-18.** Same bring-up build (Sep 8 2026 15:08:34).

### Stage 0 and 1 — brain alone on USB

| check | result | verdict |
|---|---|---|
| blank board on USB | mounted as `RPI-RP2`, no button | ✅ |
| `TP302` / `TP301` / `TP303` | **5.256 / 3.294 / 3.291 V** | ✅ |
| `v` sysclk | 125 MHz | ✅ |
| `v` flash JEDEC | `EF 40 16` | ✅ genuine W25Q32JV |
| `w` | write/verify **PASS**, capacity **PASS — no aliasing, full 4 MB** | ✅ |
| `s` → `SENSE_12V` | `12V ABSENT`, raw 0, 0 edges | ✅ |
| `s` → DIP / `TERM_SENSE` | 15 / not terminated | ✅ all five internal pull-ups good |
| `s` → FSR raw | South 16, West 15, North 14, East 14 | ✅ uniform |
| `p` | `0 0 1` | record only |

### Stage 2 — carrier mated, USB then 12 V

| check | result | verdict |
|---|---|---|
| `SENSE_12V` | absent ↔ present both directions | ✅ |
| `s` → `TERM_SENSE` | matches `SW202` | ✅ |
| `p`, 12 V live | `1 1 1` | record only |
| `d` | every switch correct | ✅ |
| FSR, East | rests **~100**, press peak **~3850**; South/West/North stayed at rest, no crosstalk | ✅ |
| `l` | all 25 lit | ✅ |
| USB pulled, 12 V live | board kept running, no reset | ✅ `U304` handover |
| `TP302` / `TP301`, 12 V only | **5.040 V / 3.296 V** | ✅ |
| cold start on 12 V only | board came up | ✅ |
| back to USB: 12 V off, USB in, 12 V on | reconnected | ✅ |

### Still open for this board

- ~~Stage 3 (nine-panel bus).~~ Done 2026-10-19 (master #2 section).
- South, West and North FSR connectors not yet exercised with a sensor.

## `DE6558A6977B342D` — ninth brain

**2026-10-18.** Same bring-up build (Oct 18 2026 17:22:58, tag `6117B7E5`),
loaded behind `panel_bootloader`.

### Stage 0 and 1 — brain alone on USB

| check | result | verdict |
|---|---|---|
| blank board on USB | mounted as `RPI-RP2`, no button | ✅ |
| `TP302` / `TP301` / `TP303` | **5.251 / 3.311 / 3.308 V** | ✅ |
| `v` sysclk | 125 MHz | ✅ |
| `v` flash JEDEC | `EF 40 16` | ✅ genuine W25Q32JV |
| `w` | write/verify **PASS**, capacity **PASS — no aliasing, full 4 MB** | ✅ |
| `s` → `SENSE_12V` | `12V ABSENT`, raw 0, 0 edges | ✅ |
| `s` → DIP / `TERM_SENSE` | 15 / not terminated (mated to an unpowered carrier for SWD) | ✅ |
| `s` → FSR raw | 12 / 12 / 12 / 12 | ✅ uniform |
| `p` | `0 0 1` | record only |

### Bench-method mistake: bring-up image loaded without the bootloader

Not a board fault. The bring-up image was loaded on its own (UF2 drag-and-drop,
then `picotool load`), skipping the `panel_bootloader` step in
`docs/PANEL_BRINGUP.md`. Since 2026-10-18 that image links at `0x10008000`, so
flash offset 0 stayed erased and the board returned to BOOTSEL on every reset,
warm or cold. Boards 1–8 were brought up before the split, so this was the first
board to hit it.

How it looked, so it is recognisable next time:

- `picotool verify` of the bring-up UF2 **passes**. It only checks the addresses
  the UF2 covers (`0x10008000`–`0x10015e00`), not offset 0.
- `picotool save -r 0x10000000 0x10000100` reads all `FF`.
- Over SWD, the boot ROM's copy of boot2 at `0x20041F00` is all `FF`, and the
  core sits in the ROM's USB loop (`pc 0x000020e0`).
- `QSPI_SS` at `U307` pin 1 reads 3.3 V in BOOTSEL, and `SW301` reads the same
  in-circuit resistance (~37 kΩ) as a good board. Neither is a fault.

⚠ `U307` pin voltages in BOOTSEL only compare like for like if both boards got
there the same way. After a failed flash boot the ROM leaves CLK, IO2 and IO3
high (pins 6, 3, 7 read 3.3 V); a board held in BOOTSEL by `SW301` never tried
flash, so the same pins read ~0 V.

Fix: `picotool load panel_bootloader.uf2`, then reboot. It booted first time.

**SWD:** `tools/swd_check.py` passed during the diagnosis: DPIDR `0x0bc12477`,
both cores, CHIP_ID `0x20002927`.

### Stage 2 — carrier mated, USB then 12 V

| check | result | verdict |
|---|---|---|
| `SENSE_12V` | absent ↔ present both directions | ✅ |
| `s` → `TERM_SENSE` | matches `SW202` | ✅ |
| `p`, 12 V live | `1 1 1` | record only |
| `d` | every switch correct | ✅ |
| FSR, North | rests **~95**, press peak **~3800**; South/West/East stayed at rest, no crosstalk | ✅ |
| `l` | all 25 lit | ✅ |
| USB pulled, 12 V live | board kept running, no reset | ✅ `U304` handover |
| `TP302` / `TP301`, 12 V only | **5.025 V / 3.313 V** | ✅ |
| cold start on 12 V only | board came up | ✅ |
| back to USB: 12 V off, USB in, 12 V on | reconnected | ✅ |

### Gameplay firmware

Loaded over USB with `picotool` after stage 2, behind the existing
`panel_bootloader`: build Oct 18 2026 18:36:20, tag **`83E9B178`**, image at
`0x10008000`. First boot: DIP 8, 12 V present, terminated, bus on, INT enabled,
FSR rest South 13 / West 13 / North 97 / East 12 (North has a sensor fitted).

### Stage 3

Passed in the first nine-panel bus run, as ID 8 (DR, end of chain, `SW202`
on). See master #2 → "2026-10-19 — nine panels on one bus".

### Still open for this board

- South, West and East FSR connectors not yet exercised with a sensor.

## master #1

**2026-09-08.** First master assembled. Bare JLC fab (OSP, 4-layer), every part
hand-soldered including SMD — `U1` SOIC-8, `U3` SOT-23-5, `D1`–`D9` DO-214AC and
22 × 0805 — with **MG Chemicals 4902P (Sn42Bi57Ag1)** low-temp paste by hot air.
`R1`/`R2` left unpopulated as designed.

**Teensy fitted: serial `20432520`, the ex-prototype module** (a new Teensy was
short of headers). Its VUSB↔VIN bridge was inspected and is **intact** — the
board taps VIN, and cutting that bridge costs `U3` its 5V supply with no other
symptom. It was flashed with the as-built `firmware/master/master.ino` *before*
being seated.

> The old prototype build turned out to be harmless on this board anyway, which
> is worth recording so the next person does not over-worry: every pin it drove
> as an output (Serial1 TX pin 1, DE pin 2, LED pin 13) lands on a **spare,
> unconnected** pad here, and the two pins that were repurposed (3/4, breadboard
> INT → player-ID DIP) were `INPUT_PULLUP` inputs against a switch to GND, i.e.
> ~150 µA. Flashing first is still the right habit; it just was not load-bearing.

### INT front end — verified in both directions

| check | result |
|---|---|
| `n` — all nine INT lines idle | **all HIGH** — no bridge, no shorted `C3`–`C11` |
| `N` — external pull-up (`RN1`) present | **all nine OK** — PASS |

`N` was written for this board (see `docs/MASTER_BRINGUP.md`) after noticing that
`n` alone **cannot** detect a missing `RN1`: the Teensy's internal pull-ups mask
it and the test still passes. Both together prove no shorts *and* no opens across
`D1`–`D9` / `R6`–`R14` / `C3`–`C11` and all ten `RN1` joints.

`N` also doubles as a seated/not-seated check — standalone it reads all nine LOW.

### `r` — single-GPIO-port claim CONFIRMED, and it closes a documented open item

`docs/MASTER_PCB.md` carried GPIO15–23 sharing one i.MX RT port as a
*processor-reference claim, not derived from the board files*, to be checked at
bring-up. It holds: **all nine INT pins report register `0x42000008`.** A single
read samples the whole pad, which the glitch-qualify re-read and the `'I'`
self-test both want.

⚠ **The bits are scattered — this needs a lookup table, not a shift.**

| panel | 0 `UL` | 1 `U` | 2 `UR` | 3 `L` | 4 `C` | 5 `R` | 6 `DL` | 7 `D` | 8 `DR` |
|---|---|---|---|---|---|---|---|---|---|
| pin | 23 | 22 | 21 | 20 | 19 | 18 | 17 | 16 | 15 |
| bit | 25 | 24 | 27 | 26 | 16 | 17 | 22 | 23 | 19 |

They do not descend with pin number, and `DR`/pin 15 lands on bit 19 — in the
middle, not at an end.

### `D` — player-ID DIP confirmed

Reads **0 (P1)** with all three switches ON, exactly as the inverted-DIP note
predicts. All-OFF reads 7, a reserved code — the deliberate choice, so that an
unconfigured master does not claim to be P1.

### Stage 2 — INT path end to end, master + one panel

**2026-09-08.** Master #1 with one panel (brain `DE6558A69754442F` + carrier),
12V on the panel, RS-485 cable fitted, INT cable on the `UL` header (`J11`).
Panel ran `i` from `firmware/panel/c/bringup/`.

**PASS.** Five 200 ms pulses then one 8 s dwell — exactly what `int_test()`
emits (a 5-iteration loop, then phase 2). Every `PRESS` had exactly one
`RELEASE`. **No bounce, no spurious edges, nothing dropped from the ring.**

This is the first time the sole gameplay input path has run over real hardware
end to end: panel GPIO22 (emulated open-drain) → carrier `R203`/`D201` → `J214`
→ cable → master `J11` → `D1`/`R6`/`C3` → `RN1` → Teensy pin 23 → ISR → ring.

#### Two numbers worth keeping

**Clock agreement: +7 ppm.** Press-to-press period measured 1 000 007 µs
(1000009/1000007/1000007/1000006) against an intended 1 000 000. Both ends of
that interval are *falling* edges, so the edge asymmetry below cancels and this
is a clean read of the RP2040 crystal against the Teensy's. 7 ppm between two
independent crystals is excellent. **Affects nothing** — the master timestamps
every edge off its own `micros()`, so panel/master drift never reaches a
reported number. Useful only as a free crystal sanity check.

**Assert/release asymmetry: ~22 µs, structural.** Hold measured 200.023 ms
against 200 ms; removing the 7 ppm leaves ~22 µs of excess, and four readings
sat within 2 µs of each other, so it is not noise.

- **Assert** — RP2040 drives LOW, hard and fast. This is the press path.
- **Release** — pin goes hi-Z and the line rises *passively* through `RN1`'s 10k
  into `C3` (1 nF) plus the junction capacitance of `D1` and `D201`.

10k × 1nF alone predicts ~12 µs to VIH; 22 µs implies ~1.8 nF total, i.e. roughly
800 pF across the two TVS. That is ordinary for SMAJ5.0A — **consistent with the
design, not a fault.** No action: presses are prompt, and 22 µs of release
latency is invisible against a 125 µs USB frame. `docs/USB_PROTOCOL.md`'s latency
table was corrected, since its "RC settle ~1 µs" row is the *press* figure and is
~20× optimistic if applied to a release.

⚠ **Only phase 1 is valid for timing.** Phase 2's 8 s dwell measured 8000.333 ms,
~275 µs long — a **software artifact**, not electrical: `int_test()` has a
four-line `printf` sitting between `sleep_ms(INT_DWELL_MS)` and
`int_out_release()`, so the release waits on a USB CDC write. Phase 1 has no
printf inside its loop, which is why those readings are clean.

#### RS-485 not tested — and it cannot be yet

`bus: 0/51961 poll replies` is **correct, not a fault.**
`firmware/panel/c/bringup/main.c` has **no RS-485 responder** — it only parks
`DE` low into receive (`gpio_put(PIN_RS485_DE, 0)`), with no `uart_init` on that
path, and `firmware/panel/c/main.c` is still breadboard-pinned. Nothing will
answer until that port is done. `0 crc errs` is likewise uninformative: nothing
replies, and the master cannot hear itself with `DE` tied to `R̅E̅`. It does
weakly confirm the bus picks up no garbage between transmissions.

### 🔧 Assembly defect found: `U3` had bridged pins

**The first real assembly fault on this board, and it took a new test to see it.**
`U3` (SN74AHCT1G125, SOT-23-5, hot-air placed) had **shorted pins**. Reflowed and
the level shifter now translates correctly — `U` passes.

**`u` could never have found this.** It sends real WS2811 frames at ~0.1% average
duty, so a meter reads ~5 mV whether `U3` works or not; the first reading taken
was "basically 0, and `u` doesn't change it", which is the *correct* reading of a
*working* pin through a slow instrument. `U` was written in response, and its
documented failure mode — "`TP10` stuck at 0 V while `TP9` switches" — is exactly
what the board was doing.

⚠ **Generalise this to the rest of the hot-air work.** `U3` is SOT-23-5 at 0.95 mm
pitch; **`U1` (THVD1450, SOIC-8) is the same class of risk and has had no
electrical test at all**, because RS-485 cannot be self-tested — `U1` pads 2 and 3
are both on `RS485_DE`, so transmitting kills the local receiver. A bridge there
stays invisible until a panel can answer. Inspect `U1` under magnification and
ring out its pins before blaming the protocol at stage 3.

`R4` was confirmed **PRESENT** by the same command — the 10k pull-down that holds
`U3`'s input LOW before firmware drives the pin, whose absence would put garbage
on the strip at every boot with no other symptom.

**Stage 1 is now complete on master #1**, as far as a bench allows: `U3`, `R4`,
`R5` and `J2` are all cleared with no strip attached. The underglow cable and the
LEDs themselves need the pad and are an install-day check, not a bring-up item.

### ⚡ First real current measurement of an assembled panel

**2026-09-08**, DMM in series in the **+12 V** leg (never the ground leg — the
panel's ground is shared with the master through the INT cable, and breaking it
would offer LED return current a path down a 24 AWG signal conductor).

| state | duty | measured |
|---|---|---|
| idle rainbow (master dims by `>>3`) | 12.2% | **0.083 A** |
| pressed, solid red `{200,0,0}` | 78.4% | **0.296 A** |

Fitting both points gives **`Ifull` = 12.9 mA/pixel** and **`Iq` = 1.76
mA/pixel**. The quiescent reproduces the documented 1.84 mA/pixel to within 4%
from an independent measurement, confirming the model's structure; the
full-scale constant, however, was **8.7 mA/pixel and is 48% low** — that figure
came from a strip cut, and the board's JLC-placed `C5446699` is a different bin.
Corrected in `CLAUDE.md` and `docs/UNDERGLOW.md`.

**⚠ Do not compare 12.9 against the datasheet's 15 mA directly — they are not the
same quantity.** 12.9 is the PWM-scaled component with quiescent fitted
*separately*; at full white you add it back: **12.9 + 1.76 = 14.66 mA/pixel,
within 2% of the datasheet's 15.** So the datasheet figure evidently already
includes quiescent, and the budget built on it is **confirmed accurate, not
merely conservative**: 25 × 14.66 = 0.37 A + 60 mA brain = **0.43 A per panel
against the budgeted 0.44 A**, and **6.28 A whole-pad against 6.34 A**. The
entire 12V chain — PSU, gauges, fuse, connector ratings — was sized from 15
mA/pixel and that number now stands measured to ~1%.

USB was connected, so the brain ran off VBUS via `U304` and these are
essentially pure LED figures.

Incidental: this also explains the **12 V supply's fan spinning up on a press** —
a 0.21 A step, which is a low threshold but nothing more than that. Nothing on
the panel can whine; it has no switching converter at all, by the deliberate
choice of cascaded linear LDOs over a buck.

### 🐛 Firmware bugs found and fixed

- **Staircased output on `?` and `r`.** `printHelp()` passes one multi-line
  literal to `Serial.println()`, which appends `\r\n` only at the very end — the
  nine embedded `\n`s stayed bare LF, so a raw terminal dropped a line without
  returning the carriage. Ten literals changed to `\r\n`, including the two-line
  "CONFIRMED: all nine on one register" message at `master.ino:309`, which would
  have staircased the first time `r` ran. Every other output uses individual
  `println()` calls, which is why only these two were affected.

### Bench-method notes

- **The master needs a command *and Enter*; the panel bring-up does not.** The
  panel uses `getchar_timeout_us()` (single keypress); the master buffers a line.
  It has to — `S <panel> <press> <rel>` takes arguments. Coming straight off a
  panel bring-up this reads as "the board is ignoring me". There is also no local
  echo, and replies land between `[heartbeat]` lines.
- **`screen` holding the port blocks flashing** — "Unable to open … for reboot
  request". Quit the session, or press the PROGRAM button, which works regardless.

### Still open for this board

- **Stage 1 remainder:** `u` only (underglow through `U3` — scope its Y pin; the
  strip's 12V comes from the Wago fan-out, not this board). Underglow was not
  connected at the bench on 2026-09-08. `r` and `D` are done, above.
- **Stage 2 and 3** need panels: RS-485 cannot be self-tested, since `U1` pads 2
  and 3 are both on `RS485_DE` and transmitting disables the local receiver.
- **`I` will report "no ack" for every ID** until `firmware/panel/c/main.c` is
  ported and implements the panel half of `'I'`. That is a correct result.

## master #2

**SMD 2026-10-01, through-hole and bench 2026-10-02.** Every SMD part is on, both sides, using the **Chip
Quik Sn63/Pb37 T4** paste through the 3D-printed stencils
(`hardware/master-pcb/stencil/`). This is the first board on that paste and the
first use of the stencils. Both worked well. It took more heat than the
Sn42Bi57Ag1 did on master #1, which is expected: Sn63/Pb37 melts at 183 °C, and
SnBi is the low-temperature alloy.

**No extra flux was added.** The paste's own flux was enough, and the board
cleaned up well. That fits what the first panels showed
(`docs/BUILD_CHECKLIST.md`): the sticky residue came from added tacky flux that
never got hot enough.

**The whole board is leaded.** The through-hole went on with the Sn63/Pb37
wire, so there is no SnBi anywhere on it.

**Teensy fitted: serial `19755070`, new**, VUSB↔VIN bridge checked intact. It was
seated before flashing; a fresh Teensy runs the factory blink, which is harmless
here (see master #1). **First flash needs the PROGRAM button.** `arduino-cli
upload` reported "Teensy did not respond to a USB-based request to enter program
mode", and after the button press the board dropped off USB entirely for a
while before reappearing on `/dev/cu.usbmodem197550701` with the as-built
firmware running.

### Stage 1 — board-local checks

| check | result | verdict |
|---|---|---|
| `n` | all nine INT lines HIGH (idle) | ✅ no shorted `C3`–`C11` |
| `N` | all nine external pull-ups present | ✅ every `RN1` joint good |
| `D` | player ID 1 (P2), switches up-up-down | ✅ matches what was set |
| `U` → `R4` | PRESENT | ✅ |
| `U` → `TP10` | **5.166 V** HIGH, **0 V** LOW | ✅ `U3` translating, no bridges; clears `U3`/`R4`/`R5`/`J2` |
| `u` (scope) | not run | `U` already clears the master-side chain |

### Stage 3 — with panel `DE6558A6977D462D` at ID 1

| check | result | verdict |
|---|---|---|
| heartbeat after panel `R` | `U  id 1  FSR=16,16,16,97`, 0 crc errs | ✅ live telemetry on the bus |
| `I`, first try | ID 1 fired **`UL` (`J11`) — MISMATCH** | ✅ the self-test caught the cable in the wrong header |
| `I`, cable moved to `J10` | ID 1 → `U` (`J10`), 2025 µs low | ✅ |
| `S 1 500 400` | panel acks | ✅ but see below |
| panel `A` + 36 East presses | 36 PRESS/RELEASE pairs on panel 1, holds 36–306 ms, no orphans | ✅ **INT end to end** |
| panel `T`, counters reset, 16,669 frames | **0 CRC, 0 overruns, 0 uart errors, 1 turnaround** | ✅ |

### ✅ ~~Open~~ RESOLVED 2026-10-02: `S` disturbs the bus — master firmware

*Original 2026-10-01 write-up kept below; the resolution follows it.*

Every error this session was next to an `S`. With no `S`, the bus ran
0/0/0 for tens of thousands of frames. With it:

- The first run showed **209 uart errors, 11 turnaround** on the panel after one
  `S`.
- Five `S` in a row: the panel's `'C' answered` count rose by one each time (all
  five arrived), but the first three each added **one CRC error**, the third
  also a burst of **111 uart errors, 11 turnaround**, and the master printed no
  ack for those three.
- The one CRC failure `X` captured was `55 43 01 05 FF FE 55 46 03 00`: a valid
  `'C'` header to address 1, then the next poll frame (`55 'F' 03 00`) starting
  two payload bytes in.

**Hypothesis, not proven:** the master sends `'C'` from the command handler and
goes straight back to its poll/LED schedule without leaving a slot for the
panel's `'c'` reply, so the reply and the next master frame overlap on the
half-duplex bus. That makes it a **master firmware** timing problem, not a board
fault. Every bad frame was caught by CRC, so nothing corrupted got used. Not
seen on master #1 with board 1 (`S` acked, 0 CRC), which may just be timing
luck. Not on the gameplay path: `S` is a configuration command.

### 2026-10-02 — two panels on the bus, and the `S` fault fixed

**Setup.** Everything power-cycled. Master #2 with **two** panels on the bus for
the first time:

| board | DIP ID | INT cable | `I` result |
|---|---|---|---|
| `DE6558A6977D462D` | **0** (was 1 on 10-01) | `UL` / `J11` | ✅ fired `UL`, 2030 µs low |
| `DE6558A69754442F` | **3** | `L` / `J8` | ✅ fired `L`, 2031 µs low |

⚠ At the bench these were called "board 1 = ID 0, board 2 = ID 3". The DIPs
read the other way round by unique ID. The **cabling matched the DIPs**, so
nothing was wrong, only the naming. Go by the unique ID.

**Baseline, no `S`:** both panels reporting live FSR values, **0 master CRC
errors**, and 208 replies to 938 polls in one heartbeat window, which is exactly
2/9 (every poll to a present panel answered).

**The hypothesis was right, and incomplete.** Reproduced on the unchanged
firmware with 20 × `S 0 500 400` (500/400 are the panels' existing thresholds,
so nothing about press behaviour changed):

| | unchanged | `'C'` in a poll slot only | **+ reply window** |
|---|---|---|---|
| `'C'` reaching the addressed panel | 20/20 | 20/20, 87/100 | **20/20, 100/100** |
| `'c'` acks printed by the master | **9/20** | 13/20, 54/100 | **20/20, 100/100** |
| addressed panel CRC / uart / turnaround | 11 / 192 / 12 | 7 / 0 / 0 | **0 / 0 / 0** |
| the *other* panel CRC / uart | 2 / 215 | 1 / 9 | **0 / 0** |
| master CRC | 2 | 0 | **0** |

The 100-command runs alternated between ID 0 and ID 3, 50 each, so both ack
paths are covered.

- **Unchanged:** every `'C'` arrived intact. What was lost was the **`'c'`
  ack**, colliding with the master's next transmission, and the collision
  garbled the bus for the bystander panel too.
- **Slot only:** master CRC went to 0 but acks still vanished. The master's
  DE/R̅E̅ are tied, so a reply arriving while it transmits is never heard at all:
  no CRC error, just nothing. `X` on the panel showed what was on top of it:
  `55 43 03 05 FF 55 46 01 00 EE`, an `'F'` poll arriving **directly behind**
  the `'C'` rather than 5 ms later. Cause: `loop()` read `millis()` once, the
  LED burst blocks ~7 ms in `write()`, and the poll was scheduled from the
  stale pre-burst time, so the next one fired immediately.
- **Fix** (master only, no panel change): `'F'` and `'C'` go through
  `sendRequest()`, which flushes to transmit-complete and then holds the master
  silent for **400 µs**. The LED burst and the next poll both wait for that,
  and the clock is re-read after the burst. Written up as a protocol rule in
  `docs/RS485_PROTOCOL.md` → "Every reply gets a guaranteed window".

**Measured side effect, deferred on purpose:** the poll rate fell from
~188/s to **~125/s** (replies still 2/9 = every present panel). The old figure
came from the back-to-back polls that collided. At nine panels that is
**~14 Hz telemetry per panel**. Not on the gameplay path; to be revisited
**when all nine panels are on the bus**, where it will be tested with the rest
of the full-pad timing anyway.

**Closed loop, both panels.** With `A` (auto-INT) and `E` (paint incoming
`'L'`) on both boards, pressing a panel turned it its master-assigned press
colour: **ID 0 red, ID 3 green**,
matching `PANEL_PRESS_COLOR` for `UL` and `L`. That is the whole gameplay round
trip on real hardware: FSR → panel threshold → `INT_OUT` → wire → master ISR →
master picks the colour → `'L'` over RS-485 → the panel's WS2815s. It was
judged by eye; presses were not counted and bus counters were not read
afterwards, so this run carries no error figure of its own.

**Not established this session:** the panels' counters before the test (1 CRC
on `…54442F`, 9 uart errors on `…7D462D`) had built up since `R` was pressed
earlier, through an unknown amount of traffic. Cause not determined; every
figure in the table above is a before/after delta.

### 2026-10-05 — underglow with a real strip

**Setup.** Master #2 on USB, the stock underglow (44 groups) plugged into
`J2`, 12 V on the strip, GND tie confirmed in
place. No panels.

- **`u`: PASS after a byte-order fix.** Sent R, G, B, it lit **blue, red,
  green**. The strip takes its bytes in **BRG** order. `underglowFill` now
  reorders, and `u` shows red, green, blue, white, then dark, as printed.
- **`g` (new, one group at a time): PASS.** All 44 groups lit singly, in
  order, from the connector end.
- **The strip powers up at full white** and holds it until the first frame.
  Seen on a re-plug of the strip connector with 12 V live. Design consequence
  in `docs/UNDERGLOW.md` → "Measured on a real strip".
- **First-run glitch, seen once, NOT reproduced.** The very first `u` showed
  black, a partial red, green, then a rainbow, i.e. bit errors along the
  strip, not one bad frame. It did not recur over many repeated runs, after a
  master reset, or after re-plugging the strip. Cause unknown. The bit-bang
  timing has still never been scoped; if the glitch returns, that is the first
  suspect.

### 2026-10-18 — eight panels on one bus, and bus updates

**Setup.** Master #2 on USB. All eight assembled panels, IDs 0–7, running
bootloader + bring-up app. Power and RS-485 daisy-chained in pad order, ID 8
absent, so the last panel on the bus is **ID 5 (R)**: its `SW202` on, every
other off. FSRs on ID 1 N, ID 3 W, ID 5 E, ID 7 S. Stage 3 for brains 3–8 is
this run. Which board carries which DIP ID was not recorded.

- **The master was running a build from before `687c12f`** (no `H`, no `P`).
  On that build `I` failed for **ID 0 only**, twice: no ack and no INT pulse,
  though ID 0 answered every poll. **Reflashed to current; `I` then passed 8/8**,
  with traffic paused and with it running. Cause not established. A start-up race
  with the last poll's reply was the first guess; refuted, since it passes
  with traffic running too.
- **`I`: 8/8.** Every ID acks and fires exactly its own header, J11…J4, pulse
  2.02 ms against 2 ms.
- **`H`: 8/8.** One build tag on all, `term ON` only on ID 5, `12V yes` all.
- **FSR mapping correct:** rest 94 (ID 1 N), 104 (ID 3 W), 98 (ID 5 E),
  101 (ID 7 S); unplugged channels read 0–23.
- **Bus: 0 errors in steady state.** After `z`, ~140 s and ~91,000 frames per
  panel: `crc 0 uart 0 ovr 0 turn 0` on all eight; master 0 CRC errors,
  14,426 of 16,231 polls answered, exactly 8/9. In the first 62 s after a bus
  update, each panel had counted `uart` 7–13 and `turn` 11–17 (and one `crc`
  on ID 2). Since the steady state is clean, these were a start-up burst.
  When exactly it happens was not pinned down.
- **Bus firmware update: works, both power-cut cases recover.** Detail in
  `docs/FIELD_UPDATE.md` → "Hardware validation (2026-10-18)". Found and
  fixed: `--panels` selected nothing, because `'U'`/`'A'` were broadcast to
  panels that were all held.
- **New: `z` / `'Z'` zeroes every bus counter on the master and all panels**
  without a reboot. Verified: all eight read 0 straight after, uptime
  unaffected.
- The master prints `STOPPED REPLYING` for every panel while traffic is paused
  with `x`. That is a false alarm (nothing is polled), not yet fixed.

**Termination experiment, same setup.** The master's `R3` (120 Ω) stayed in
for every case; only panel `SW202`s changed. Each case: `z`, run, `H`. About
100,000 frames per panel per case. Panel counters only. Master figures were
not recorded.

| case | `SW202` on | far end of the chain (ID 5) | panel counters, all eight |
|---|---|---|---|
| baseline | ID 5 | terminated | `crc 0 uart 0 ovr 0 turn 0` (~91k frames) |
| A | none | open | `crc 0 uart 0 ovr 0 turn 0` |
| B | ID 0 only | open; terminator at the near end of the panel chain | `crc 0 uart 0 ovr 0 turn 0` |
| C | ID 4 only (mid-chain) | open | `crc 0 uart 0 ovr 0 turn 0` |
| D | ID 5 and ID 0 | terminated; three 120 Ω in parallel = 40 Ω, below RS-485's 54 Ω | `crc 0 uart 0 ovr 0 turn 0` |

- **On this bench the bus tolerates missing, misplaced and doubled
  termination with no errors at all.** About 4.2 m of cable at 1 Mbps, panels
  on the floor rather than in a pad.
- **So the error counters cannot detect a termination fault**, and nothing
  else on the boards can see the bus electrically. What the master *can* check
  is the switch positions `H` already reports, against the documented chain
  order. That check should **warn, not fail**: a wrong setting did not break
  the bus here. (A `term` bit reads the switch's pole B only; it cannot see a
  missing `R201` or a bad pole A.)
- **This is not evidence of margin.** Zero errors says the receivers decoded
  every byte, not how close they came. The waveform has not been looked at.
  Scoping is deferred until the panels are inside a real pad, with the real
  cable routing and frame: the user's call, since the floor setup is likely
  quieter than the pad.

### 2026-10-19 — gameplay firmware on all eight panels

The rewritten `firmware/panel/c/main.c` (ID and sensor-test LED modes, bus
command `'M'`) went to all eight panels by bus update, and the master got the
matching `m` command. **The user reports the boot ID image appears on each
panel and the LED modes (`m 1` ID, `m 2` test) work.**

Also reported: **`H` error counts 0 on every panel, and presses still reach
the master** through the new core-0 filter. **DIP 10 standalone test mode
works with no master** (panel 7's board, then set back to 7). The DIP is read
once at boot, so a change takes a power cycle. Bench slip worth noting: DIP 10
is OFF ON OFF ON left to right (ON = 0, leftmost = bit 3); ON OFF ON OFF is 5
and would have collided with the real panel 5.

Method note: `arduino-cli upload` needs `-p` with the Teensy's `usb:…` port;
the command in this doc's header was missing it (fixed).

### 2026-10-19 — nine panels on one bus

**Setup.** Master #2 on USB, all nine panels on gameplay firmware, chained in
pad order with `SW202` on only at ID 8 (DR, brain `342D`). FSR sensors on ID 1 N,
ID 3 W, ID 5 E, ID 7 S (rest 93–104; empty channels 0–23, ID 8 has none).

- **`H`: 9/9.** Build `83E9B178` on all nine, `12V yes` on all, `term ON`
  only on ID 8, and the master's check reports `termination OK: DR (id 8)`.
- **Steady state: 0 errors.** After `z`, ~120 s and ~97,000 frames per panel:
  `crc 0 uart 0 ovr 0 turn 0` on all nine; master **15,911 / 15,911 polls
  answered** (9/9, against 8/9 with eight panels), 0 CRC errors.
- **`I` was intermittent, and the master firmware was the cause. FIXED
  2026-10-19.** An ID sometimes got no ack and no pulse: ID 1 in one run, ID 0
  in another, then IDs 0 and 3 together. Whenever an ID acked, it fired exactly
  its own header (J11…J3, 2.02–2.04 ms), so **the slot ↔ ID mapping is correct
  for all nine.**

  | condition (10 runs each, `z` before, `H` after) | runs with a failed ID | panel counters |
  |---|---|---|
  | traffic running, old master | 6 (mostly ID 0; once IDs 0, 1, 3) | every panel `crc` 1–5, `uart` 3–15 |
  | traffic paused with `x` first, old master | 0 | all 0 |
  | **traffic running, fixed master (30 runs)** | **0** | **all 0** (~22,000 frames each) |

  **Cause:** `identifySelfTest()` cleared `bus_traffic` and sent the first
  `'I'` at once. That stops *new* traffic, but not what is already on the
  wire: the reply to the last `'F'` poll. The first `'I'` landed on it. **Fix:**
  `waitBusQuiet()` flushes TX and waits out the reply window before `I` starts.
  `P` (passthrough) had the same gap and now uses it too. Every other command
  already went through `loop()`'s `poll_due`. Bus update re-checked afterwards
  with `tools/panel_update.py`: 9/9 found, all nine back on `83E9B178` with 0
  errors. Without `--force` it skipped IDs 0–7, but **re-installed the same image
  on ID 8**: an app loaded over USB leaves the bootloader with no app CRC on
  record (`app loaded over USB/SWD`), so it counts as needing an update.

  **Unconfirmed:** why a later ID could fail in the same run (IDs 1 and 3 after
  ID 0). One possibility is that the collision's garbage gives a panel's parser
  a plausible length byte, so it reads the next `'I'` packets as payload. The
  panel parser has no inter-byte timeout to recover from that. This was not
  tested; the later-ID failures stopped along with the ID 0 ones.

  The eight-panel run's ID-0-only `I` failure (above) fits the same cause.

### 2026-10-19 — panel LED stutter: a timestamp wrap, fixed

**Symptom (user):** now and then a whole panel blinked rapidly, as if its frame
rate dropped or its on/off duty changed, then went back to normal. It hit
random panels with nobody on the pad. The bus counters stayed at 0 throughout.

**Cause, from the code:** `core1_main()` read `now_ms` at the top of each pass,
before `rs485_service()` stamped an incoming `'L'` frame with a fresh
`to_ms_since_boot()`. If the millisecond tick fell between the two reads, the
frame was one tick newer than `now_ms`. The unsigned age check in `render()`
then wrapped to ~4×10⁹ and painted the fresh frame dark until the next frame or
16 ms refresh. The master sends a frame every 16 ms and each panel counts
milliseconds on its own crystal. So a panel's frame arrival time relative to
its own tick only drifted slowly, and while it sat near the tick, consecutive
frames were blanked. That made it episodic, and different on every panel.

**Fix:** a signed age check, `(int32_t)(now_ms - g_led_frame_ms) <
LED_FALLBACK_MS`, as the mode timer already used. Pushed to all nine by bus
update (image CRC `28E13C35`, build tag **`E7D4BF9B`**); `H` 9/9, 0 errors.
**User, after watching for a few minutes: no stutter since, and the animation
looks noticeably smoother.** That fits the same cause: single blanked frames
were probably happening at a low rate all along, below the threshold of
looking like a stutter. This rests on the code reading and the before/after
observation; the wrap itself was never caught on an instrument.

### 2026-10-20 — full-white soak

New master console command `w` (full white on all nine panels, presses
ignored) uploaded and **working on the bench**. Nine panels, no trunk fuse
fitted. Purpose: run the panels at full LED current for a while and confirm
they all still work afterwards. `U303` (AMS1117-5.0) doesn't carry the LED
current (WS2815 runs on 12 V), so its temperature in this test mostly reflects
the LEDs heating the board.

- **Thermal (by touch):** boards "a tiny bit warm, not hot in any way".
- **Trunk current, measured** with two different multimeters in series, spot
  readings (the user's AstroAI DM6000AR is rated 10 s on the 10 A input, then
  15 min off):

  | LEDs | measured | model |
  |---|---|---|
  | idle rainbow (master default, ~1/8 brightness) | **~0.97 A** | — |
  | **full white, all nine (`w`)** | **~3.4 A** (~0.38 A/panel) | 3.87 A (9 × 0.43 A, from the 2026-09-08 panel) |

  **Full white reads ~12% under the model.** The model's per-pixel figure
  (14.66 mA) came from one assembled panel; the 2026-08-16 strip cut gave
  10.5 mA, so LED-to-LED spread this size is already on record. Not
  investigated. Rail voltage during the readings was not recorded.

- **Soak ~1 hour**, ambient ~20 °C. No brain had USB attached, so every
  brain's logic load was on the 12 V trunk.
- **`H` afterwards: 9/9 answer**, build `E7D4BF9B`, `12V yes`, `term ON` only
  on DR. But all nine read `up 262s reset power-on`: the 12 V was unplugged
  and replugged several times for the current readings, and `H` wasn't taken
  before that. **So `H` covers only the ~4 minutes after the last power-up, not
  the soak.** (Frame counts fit that: ~210,000 at the ~808 frames/s from
  2026-10-19 is ~260 s.)

  | panel | crc | uart | turn |
  |---|---|---|---|
  | U (id 1) | 1 | 106 | 13 |
  | UR (id 2) | 0 | 22 | 12 |
  | C (id 4) | 1 | 60 | 14 |
  | R (id 5) | 1 | 82 | 15 |
  | UL, L, DL, D, DR | 0 | 0 | 0 |

  `ovr` 0 everywhere. **Not yet known** whether these came from the power-up
  (all nine booting while the master was already sending) or keep happening
  in steady state at full white. The 2026-10-19 nine-panel run, not at full
  white, read 0 everywhere after `z`.
- **Re-check: `z`, full white held, `H` at `up 1120s`: 0 errors on all nine**
  (`crc`/`uart`/`ovr`/`turn` all 0, ~551,000 frames each). So **steady state at
  full white is clean**, and the earlier errors came from around the power-up.
  Which part of the power-up (panels booting into live traffic, or the
  replugging itself) wasn't isolated. Why only U, UR, C and R picked them up is
  also open; a deliberate power-up with `z` just before would show whether it
  repeats on the same four.
- **Deliberate 12 V power cycle**, traffic running, `H` at `up 39s`:

  | panel | crc | uart | turn |
  |---|---|---|---|
  | UL (id 0) | 0 | 74 | 11 |
  | U (id 1) | 0 | 90 | 13 |
  | C (id 4) | 0 | 31 | 19 |
  | the other six | 0 | 0 | 0 |

  **A different set from the first time** (then U, UR, C, R). So it isn't tied
  to particular boards; any panel can pick up errors at power-up, and they stop
  once everything is up. `turn` counts a UART error within 200 µs of the
  panel's own transmit, `uart` any other (`rs485_rx_irq()` in
  `firmware/panel/c/main.c`). The user waited "a few minutes" before `H` but
  the panels report 39 s of uptime; the user confirmed the wait was shorter
  than it felt, so the 12 V did not drop again.

  **Inferred, not tested:** panels power up at slightly different moments, and
  a brain's transceiver may briefly drive the bus while its RP2040 boots and
  before firmware sets DE. That would garble traffic for panels already
  running. The other candidate is simply panels booting into live traffic
  mid-byte. To tell them apart: pause traffic with `x`, power-cycle, wait,
  resume with `x`, wait, `H`. Errors with traffic paused would point at
  booting transceivers. Bearing on this: the DE pull-down was dropped
  (2026-09-07) on a *static* reading, `TP306` 35 mV on a blank board. DE during
  the power ramp itself has never been looked at.
- **That test, run once:** traffic paused with `x`, 12 V power-cycled, traffic
  resumed, `H` at `up 36s`: **0 errors on all nine.** So the power-up errors
  need live traffic: **they are panels booting into the middle of the master's
  frames**, and each recovers once it re-syncs. No sign of a booting
  transceiver disturbing the bus, at the counters' resolution (a scope would
  see more). One run only. Harmless for play, since press input goes over the
  INT wires; worth remembering for the "panel power cycle during play"
  robustness test.

### 2026-10-20 — panel parser inter-byte timeout

**Why:** the 2026-10-19 `I` runs left one suspect unconfirmed: garbage that
parses as a large `len` byte makes a panel's parser read the next real packets
as payload, and nothing recovered it until the bogus packet's CRC failed.

**Change (panel gameplay firmware):** a byte arriving after more than 500 µs
of bus silence ends any packet in progress (`docs/RS485_PROTOCOL.md` →
"Framing"). Counted as a new `'h'` field, `tmo` (reply now 24 bytes; the master
accepts 22 or 24). Image CRC `E87EF0CB`, build tag **`E3707C2E`**, pushed to
all nine by bus update. Master gained a bench command, **`b`**: it sends a lone
header claiming an 80-byte payload, waits 2 ms, then sends `'H'` to each panel.
The nine requests (45 bytes) all fall inside the bogus payload if the parser
doesn't time out.

| | `b` result | panel counters after |
|---|---|---|
| old panel firmware (`E7D4BF9B`), new master | **0/9 answered, FAIL** | `crc 2` on all nine (the bogus packet failed its CRC once it filled) |
| new panel firmware, 3 runs | **9/9, PASS** each time | `crc 0`, `tmo` 1 → 2 → 3, one per run |

So the test fails on the known-broken firmware and passes on the fix.

**Then, traffic running:** `z`, `I` ×10: **10/10 runs, 90/90 IDs acked, each
firing exactly its own header** (J11…J3, 2.03 ms). ~2 min steady state after:
`H` 9/9 with `crc 0 uart 0 ovr 0 turn 0 tmo 0` on all nine (~106,000 frames
each); master 17,020/17,020 polls answered, 0 CRC errors. `tmo 0` there means
normal traffic never trips the timeout.

**What this does not show:** that the timeout was the cause of the 10-19
later-ID failures. Those stopped with the master's `waitBusQuiet()` fix before
this existed. What it shows is that the parser now recovers from that kind of
garbage within one gap.

**Same gap elsewhere, not changed:** the master's RX parser and the panel
bootloader's parser have no inter-byte timeout either. The bootloader is only
reachable per board (USB or SWD); a bus update installs the app only.

### Still open for this board

- ~~**`S` bus disturbance**~~ fixed 2026-10-02, master firmware (above).
- **Poll rate ~125/s, ~14 Hz per panel at nine** — revisit with all nine panels.
- **Scope the RS-485 bus once the panels are in a pad**: edge settling,
  whether ringing re-crosses ±200 mV, amplitude, idle noise, eye height and
  width, for the baseline and at least case A of the termination table
  (above). Probe points: master `TP7`/`TP8` (GND `TP5`), panel `TP202`/`TP204`
  (GND `TP207`–`TP210`), trigger on master `TP2` (DE).
- ~~**Termination placement check in the master's `H` sweep**~~ **done
  2026-10-18**, the same night. Every sweep now ends with `# termination OK`
  or `# termination WARNING`. Verified by flipping `SW202`s live: R off → "no
  panel terminated"; R + UL → "2 panels terminated: UL R"; UL only → "1 panel
  terminated: UL"; R alone → OK. Limits are in the comment on
  `checkTermination()` in `master.ino`.
- ~~**Underglow with a real strip**~~ passed 2026-10-05 (above). Only the
  unreproduced first-run glitch remains, and the bit-bang has never been scoped.
