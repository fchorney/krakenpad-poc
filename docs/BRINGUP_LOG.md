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
channels read ~12~~, so "unplugged" and "unpressed" remain electrically
indistinguishable by design — presence is config, not measurement~~.
(Corrected 2026-10-22: ~12 against a resting 94–98 is the opposite, a gap of
~80 counts. Presence stays config; `docs/PANEL_CONFIG.md` → "Presence is
config".)

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

**Same gap elsewhere:** the master's RX parser has no inter-byte timeout
either (not changed). *(Since built a different way, a resync before each of
the master's transmissions, 2026-10-20, not yet run: `docs/RS485_PROTOCOL.md`
→ "Framing".)* ~~The panel bootloader's parser, which only USB or SWD
could reach~~: fixed the same day, by updating the bootloader over the bus
(next entry).

### 2026-10-20 — bootloader updated over the bus, all nine panels

**What:** stage 0 + two bootloader slots, installed by an updater app sent with
the ordinary bus update (`docs/FIELD_UPDATE.md` → "Updating the bootloader over
the bus"). Run with `tools/panel_boot_update.py`, which installs the updater,
waits for `'Q'` to report the new tag, then reinstalls the gameplay app
(`E87EF0CB`). Panels had the old single bootloader, tag `0A447BC5`.

**Before hardware:** the power-cut host test passed (every flash write cut short
in turn, four tear patterns each; never a hang), and failed on two deliberately
wrong write orders. A stage 0 CRC guard in the build fails if stage 0 ever
changes, checked against a modified stage 0.

**On panel 0 (UL) alone, USB unplugged:**

| run | result |
|---|---|
| migrate (old → tag `5226AC3A`) | ✅ new tag; the new bootloader then installed the app |
| A/B update to a rebuild (tag `C5234B1B`) | ✅ slot B then slot A written; `'q'` shows slot A |
| fallback: test build wipes slot A's header | ✅ `'q'` reports **slot B**; slot B installed an app over the bus |
| normal update after that | ✅ slot A rewritten, back on slot A |

**Panels 1–8 in one run:** ✅ all eight report `C5234B1B`, app reinstalled.
After a minute of traffic, `H` 9/9 with `crc 0 uart 0 ovr 0 turn 0 tmo 0`, and
the `b` parser test 9/9.

**Not tested yet:** the BOOTSEL safety net (a board whose boot2 CRC fails), a
real power cut during an update, `picotool` loading `panel_boot_usb.uf2`, a
cold 12 V power-up through stage 0 (every boot so far was a watchdog reboot),
and an SWD restore with `panel_boot_usb.bin`.

**Cold power-up (V7), same day: passed.** 12 V off ~5 s and on, master on USB
throughout. `H` at `up 11s`: 9/9, reset cause **`power-on` alone** on all nine
(every earlier boot through stage 0 also showed `watchdog-timeout`), so this
was boot ROM → boot2 → stage 0 → bootloader → app from cold. Power-up bus
errors on three panels, as after every 12 V power-up (panels booting into live
traffic): L `uart 84 turn 14`, DL `uart 76 turn 14`, D `crc 1 uart 233 turn 20`.
After `z` and 30 s: 0 errors on all nine, `tmo 0`; `b` test 9/9.

**Tag change:** the first rebuild changed only slot B's binary (the `'q'` slot
bit) while the tag was the CRC of slot A alone, so the tag would not have
changed. Caught before it was used; the tag now covers both slot builds.

### 2026-10-20 (evening) — config on the bus and the USB vendor interface, first run

Run by Claude with nobody at the bench (master #2 on USB, nine panels on 12 V),
following `docs/MASTER_BRINGUP.md` → "Config and the USB vendor interface".
So **everything that needs a hand, an eye or a meter was not run** (list at
the end). Firmware from `d14ee0b`: master build `770CB928` (the tag changes
with every build), panel image CRC `5197D3AF`, build tag **`DA2F106C`**.

**Before the change, old firmware:** `H` 9/9, all `crc 0 … tmo 0`, panels on
`E3707C2E`.

| step | result |
|---|---|
| 1. master alone | ✅ banner `Master firmware 0.1.0`, `config: NO valid saved config`. `pad_cmd.py info`: protocol 1, packet 64, config version 1, player ID 1 (P2, as set), flag "config was reset". A second `build.sh upload` found the Teensy through the new interface set. `hid_monitor.py` opens the joystick, ~91 reports/s idle; **presses not tried** |
| 2. old panels | ✅ all nine "sent, waiting for the panel"; heartbeat `0/9 answering panels match` |
| 3. panel update | ✅ `panel_update.py`, nine at once, 0.6 s stream. One `config matches` line per panel, `c` 9/9 "matches", heartbeat `9/9`. The panel console's `s` not checked (no brain on USB), but the master's config events report CRC **`FD704831`** for defaults, the figure the procedure expects |
| 4. `S` | ✅ `S 3 600 450`: panel 3 matches at 600/450, "UNSAVED edits". The procedure's `S 3 600 450 2` changes nothing after that (same values), so ran `S 3 650 500 2`: only channel 2 changed. **Not felt** |
| 5. `F` | ✅ `F 3 0 1 0`: `c` shows `d` on channel 0, panel matches; `F 3 0 1 1` clears it. **Press with the channel disabled not tried** |
| 6. `save` + reboot | ✅ slot A seq 1 (**3045 µs**, the first write to blank EEPROM); reboot → `loaded from slot A, sequence 1`, edits intact, all nine matched straight away; second save → slot B seq 2 (3107 µs). The reboot was a re-upload, **not a power replug** |
| 7. panel 12 V cut | **not run** |
| 8. `reset config` | ✅ defaults, saved, nine "matches" |
| 9. `config`, `status` | ✅ agree with `c` and `H`; build tag and termination shown per panel (DR `ON`) |
| 9. `set 3 550 420` | ✅ "saved", waiting → matches events, panel 3 CRC `205F37F2`; `c` agrees, nothing unsaved |
| 9. `stream 30` | ✅ **12.6 Hz per panel**, all nine (378–379 records). Expected ~12.7 |
| 9. `mode 1 3`, `mode 2 5 --panels 4` | answered "queued"; **LEDs not seen** |
| 9. `selftest` | ✅ every slot names its own panel |
| 9. `zero`, `H` | ✅ counters 0 |
| 9. `clock` | round trip **min 452 µs, median 504 µs, max 7714 µs** over 50 |
| 9. `leds 0 0 64 3` | ran; **not seen**. Sent only 92–97 frames in 3 s: host side, see below |
| 9. `bus-h 4` | ⚠ the first run printed `no 'h' reply`; see below |
| 9. `events` with a cable pull | **not run** |
| 9. `bootloader --yes` | ✅ the Teensy enumerates as HalfKay (`16C0:0478`), then comes back on its own ~2 s later, because the Teensy Loader app, still open from the last upload, reflashes it automatically |
| 9. SDK `cargo run --example monitor` | ✅ `PadConnected { player: 1 }`, `master 0.1.0 build 770CB928, panels 111111111, termination Ok`, then live values for all nine. **Press and cable pull not tried** |
| 10. `I`, `b`, `H`, `P` | ✅ `I` 9/9 (2.02 ms each); `b` 9/9 PASS (`tmo 1` each); `H` 0 errors; `P` is what step 3 used |
| 10. `w`, presses | **not run** (`w` is ~3.4 A with no trunk fuse fitted, and nobody was there) |

**Save stall, with compaction (measured).** 8,000 saves from the console,
alternating panel 0's press threshold 500/501 so each save changes a few
bytes: **median 914 µs, p99 1.57 ms**, min 215 µs; **18 saves took 30.0–34.5
ms**, which is a sector being erased and compacted (first one at save ~1,350).
So a save can stall INT, USB and the bus for **up to ~35 ms**. A save that
changes more bytes reaches a compaction sooner. The config was reset to
defaults afterwards (slot A, sequence 8055).

**`bus-h` miss, not reproduced.** Two of the first ~15 `bus-h` runs printed
`no 'h' reply` (panel 4 just after `mode`/`leds`, and panel 1). A debug run
saw the reply arrive as a bus-frame event ~1 ms after the frame command was
acknowledged, so the master's passthrough works. Since then: 0 misses in 40
debug runs and ~45 `pad_cmd.py bus-h` runs at random gaps, including right
after `mode` and `leds` and with panel 4 in sensor-test mode. Panel counters
afterwards were clean. Cause unknown.

**Follow-up 2026-10-21: still not reproduced, now instrumented.** The master
now prints what each USB passthrough session heard when it ends: frames sent,
how many went out with a packet still open in `pt_bus`, bytes and frames heard,
CRC failures, and the last 32 bytes. Suspect checked: a stray byte leaving
`pt_bus` mid-packet so it reads the reply as the rest of it (the 6d61543 resync
covers the master's own receiver, not passthrough). Then 150 `bus-h` runs to
random panels, each after a random command (`mode`, `leds`, `stream`,
`status`, `selftest`, `zero`, `clock`, `info`, another `bus-h` or nothing) and
a gap of 0–2 s: **0 misses**, and all 161 sessions logged `sent 1, 0 with a
packet open; heard 29 bytes, 1 frames, 0 CRC fails`, exactly one `'h'`
reply. So no stray bytes and no open packet; the suspect has no support. If a
miss comes back, the console's `passthrough OFF` line says whether the master
heard the reply (then the loss is on USB or in the host) or not (the panel
didn't answer, or the reply was lost on the bus).

**Bearing on the master turnaround glitch (`docs/MASTER_BRINGUP.md`):** the
Teensy core stores every received byte, framing error or not (its receive
interrupt doesn't check `FE`), so the false start bit the panel saw would
appear here as an extra byte after the master's own transmission. In those
161 sessions the master heard nothing but the reply: **no glitch byte after
161 transmissions**, through the same `sendPacket()` and `transmitterEnable`
DE path as normal traffic. Not a framing-error count; that would still need
`LPUART4->STAT`.

**`leds` rate is host-limited (measured on this Mac).** `time.sleep(1/60)`
sleeps a median 24 ms, and one LED frame (675 bytes = 12 packets) takes
hidapi a median 10.2 ms to write (4.5–23.6 ms). The master isn't the limit,
but **60 Hz from the host through hidapi on macOS leaves ~6 ms per frame**,
which matters for the SDK's LED path.

**Follow-up, 2026-10-21 (measured on this Mac, Python 3.13.2, hidapi):**
- **Sleep:** the oversleep grows with the request: 1 ms → 1.26, 5 → 6.27,
  16.67 → 19.9 ms (medians). It isn't a resolution limit. `pad_cmd.py leds` now
  schedules each frame at `t0 + n/60` and sleeps to 2 ms short, then spins:
  **60.0 frames/s** over 10 s.
- **Where a frame's write time goes:** per 64-byte write, **~0.48 ms**, and the
  5th packet of each frame waits **~3 ms** more. With bus traffic paused (`x`)
  that 3 ms wait is gone (frame 9.0 → 6.0 ms median). So the master's loop
  goes ~3 ms without emptying its 4 RawHID receive buffers while the bus runs.
  The blocking LED-frame transmit is the likely cause; not confirmed.
- **Polling vs packet size**, with a test sketch that only empties RawHID
  (scratch core copy, not committed; 300 writes each, all received):

  | OUT report | `bInterval` | median per write | 675-byte LED frame |
  |---|---|---|---|
  | 64 B | 4 (1 ms) | 1.00 ms | ~11 ms |
  | 64 B | 2 (250 µs) | 0.50 ms | ~5.5 ms |
  | 64 B | 1 (125 µs, as built) | 0.38 ms | ~4.2 ms |
  | 512 B | 1 | 0.38 ms | ~0.76 ms |
  | 1024 B | 1 | 0.47 ms | ~0.47 ms |

  Each write waits for its transfer to complete, and `bInterval = 1` is
  already the fastest poll; at that point the Mac's per-write cost (~0.38 ms)
  is the floor. **Bigger OUT reports are the lever**: one 1024-byte report
  carries a whole LED frame in under 0.5 ms. Reports over 255 bytes need the
  core's descriptor to write the size as two bytes (`0x96` report count and a
  16-bit `wMaxPacketSize`), and over 64 bytes is High Speed only.

**Protocol 2 built and run, 2026-10-21** (decided the same day: 1,024-byte OUT
reports at High Speed, 64 at Full Speed, IN 64 at both; `docs/USB_PROTOCOL.md`).
Master build `3BA990F2`, panels unchanged (`DA2F106C`).

| | High Speed | Full Speed (`FULL_SPEED=1` build) |
|---|---|---|
| `pad_cmd.py info` | protocol 2, OUT 1024, IN 64; the report descriptor agrees | protocol 2, OUT 64, IN 64; the descriptor agrees |
| one LED frame write, bus running (200) | **median 0.70 ms**, p95 1.50, max 6.77 | **median 15.5 ms**, p95 19.9, max 22.5 |
| `leds` for 3–5 s | 60.3/s | 59.8/s, writing back to back |
| `stream` | 12.7 Hz per panel | 12.7 Hz per panel |
| `clock` round trip | min 453, median 502, max 7577 µs | min 1808, median 1992, max 8216 µs |
| joystick (`hid_monitor.py`, idle) | — | ~91 reports/s, as at High Speed; `j` says `Full Speed` |

Also on protocol 2 at High Speed: `config`, `status`, `set`, `reset-config`,
`selftest`, `bus-h`, `mode`, `zero`, and the SDK's `monitor` (prints `USB High
speed, packets 1024 B to the master, 64 B back`). SDK tests 28/28, including a
fake master at each speed. Not yet: the SDK against a real master at Full
Speed, and Windows or Linux at either speed.

At Full Speed an LED frame takes ~15 ms of each 16.7 ms frame, so 60 Hz host
frames there have no room for anything else; what Full Speed should drop or
degrade is still to be decided from tests like these.

**Observations, nothing broken:**
- Any edit re-pushes all nine panels (`cfgChanged()`), so the console prints
  nine `config matches` lines and USB gets nine waiting → matches pairs, not
  just the edited panel's.
- `status` once showed one panel at `101%` polls answered.
- Killing the SDK `monitor` leaves the master's stream on (`j`: `stream ON`)
  until the next USB reset. Nothing dropped.
- After USB passthrough ends, the master reports every panel as having stopped
  and restarted replying, and re-pushes the config to each. That is the silence
  during passthrough, not panels resetting: their uptimes carried on.

**Not run, needs someone at the bench:** presses (`hid_monitor.py`, the SDK's
`*`), a press on a disabled channel, the panel 12 V cut (step 7), cable pulls
with `events` and in the SDK, every visual LED check, full white, and **the
parser resync test**, which needs a panel's USB console for `g`.

### 2026-10-21 — hands-on steps at the bench (the user at the pad)

Master #2 on protocol 2 (`90623f6`), nine panels on `DA2F106C`. Sensors
fitted on the bench pad: one each on **U** (channel 2), **L** (channel 1),
**R** (channel 3) and **D** (channel 0), each ~100 at rest; the other five
panels have none.

| test | result |
|---|---|
| presses on every panel (`hid_monitor.py`, the SDK's `*`) | **deferred** until the panels are in the pad with at least one sensor each (the user) |
| `F` with a press | ✅ `F 3 1 1 0` (L's fitted channel): four presses to 3,262–3,832 raw in `stream`, no pressed flag, **no L button**; a U press as control: pressed flag, button U press/release (seq 32/33, no gap). `F 3 1 1 1`: L presses again (seq 34/35) |
| `mode 1 3`, `mode 2 8 --panels 3`, `leds 0 0 64 3` | ✅ seen by the user: all nine show their ID for 3 s; only L in the sensor test, reacting to its sensor; all dim blue for 3 s, then the master's pattern back |
| one panel's 12 V cut (D, end of the centre column, ~15 s off) | ✅ `STOPPED REPLYING`, USB `GONE` + `absent`; back: `present`, `not configured`, `sent, waiting`, `matches`, all within the same second. D: `reset power-on`, 0 bus errors. The other eight: 0 errors throughout, so an unpowered panel mid-chain doesn't disturb the bus |
| bus cable pull (DR's RS-485 IN, ~9 s) | ✅ SDK `monitor`: `PanelPresence` false, `ConfigResult` `Absent`; back: present, `Waiting`, `Matches` (no "not configured": DR didn't reboot). Console likewise. The console only checks termination at the end of an `H` sweep, so nothing about termination printed during the pull |
| full white `w`, ~10 s, no meter | ✅ every LED white (seen), nothing more than usually warm; `H` at full white: 0 errors on all nine, termination OK. No current figure: the user has no clamp meter, and a DMM's high-current range may have an on-time limit (check its manual). The fused re-run stays open |
| DL's own console (brain `DE6558A69776582F`, sticker 582F, on USB) | ✅ `config: from the master, CRC FD704831 (26 'W' accepted, 0 rejected)` |
| parser resync, current master | ✅ `1 partial packets dropped`, `0 crc errs`, polls kept pace (448/449 with one in flight, then 1020/1020); the other eight `tmo 1`, DL (the sender) 0 |
| parser resync, old master (`f138ece`) | ❌ as predicted: `1 crc errs` and **6 `'f'` replies missing** (195/201). So the test fails without the fix and passes with it |

**Found, none blocking:**
- **The duplicate-ID warning fires after a panel drops out** (twice: `panel 7
  poll reply rate is only 25%` just after D came back, `panel 8 … 42% while
  panel 2 is 101%` while DR was out). The heartbeat window counts the time
  the panel was gone. The `101%` is the same over-100% nit seen in `status`.
- **The SDK `monitor` example prints a missing panel's last values** (DR frozen
  at `22,13,13,13` for the whole pull); the snapshot's `present` flag isn't
  used.
- **On macOS, `pad_cmd.py` holds the vendor interface exclusively**: the SDK
  `monitor` found no pad while `pad_cmd.py events` ran, and connected within a
  second of it stopping. The SDK itself opens shared (`macos-shared-device`).
- **Bench-method slip:** with a brain on USB, the console helper's default
  port picked the brain's `/dev/cu.usbmodem1101` over the master's, and one
  `j` went to DL's console (no such key there: no effect). Name the port
  explicitly whenever a brain is plugged in.
- `c` says "UNSAVED edits" after `F` off then on, with the values back as
  saved: the flag means "edited since the last save", by definition.

**The three fixed the same evening:**
- **Duplicate-ID warning:** the master now judges a heartbeat window one
  heartbeat late, skipping any panel whose presence changed in that window or
  the next (a dead panel is declared gone up to 1 s later), and caps rates at
  100%. Re-test, DR's bus cable out 26 s: **0 warnings** (before: one per
  drop-out). **Not yet re-tested: that a real duplicate ID still warns** (set
  one panel's DIP to another's ID and power-cycle it).
- **SDK `monitor`:** prints `DR:gone` while a panel isn't present (19 lines
  during the re-test).
- **`pad_cmd.py` and `hid_monitor.py` open the pad shared**
  (`hid_darwin_set_open_exclusive(0)`, after the `hid` module's `hid_init()`).
  Re-test: `pad_cmd.py events`, the SDK `monitor` and `pad_cmd.py info` all
  open at once. ⚠ Shared means two programs *can* both send commands; the
  master takes one at a time and matches responses by transaction ID, so two
  programs driving the pad at once can mix up responses (and, at Full Speed,
  interleave multi-packet commands). Watching alongside a game is fine.

### 2026-10-22 — installed in the pad

The stock electronics came out and master #2, the nine panels and the harness
went in (the user, a few hours). Sensors in the pad: one each on **U, L, C, R
and D**; UL, UR, DL and DR have none.

| test | result |
|---|---|
| 12 V chain | ✅ as designed: PSU, fuse, Wago fan-out; one Wago port per column; master GND tie on its own GND port; underglow 12 V + GND from the fan-out, DATA from master `J2` |
| presses, the five panels with a sensor (U, L, C, R, D) | ✅ the user |
| presses, UL, UR, DL, DR | ~~**not run yet**: sensors to be borrowed from the other pad for the test, then returned~~ ✅ all 16 corner sockets, the same day (below) |
| underglow `g` chase | ✅ looked right, by eye |
| underglow `u` | ~~❓ the user remembers pressing it and seeing nothing. Not confirmed either way; retest live~~ ✅ retested the same day (below) |

Harness findings (lengths, INT labels, unjacketed ends):
`hardware/harness/README.md` → "Install findings". The meter-only underglow
test `U` was removed from the firmware the same day (`docs/MASTER_BRINGUP.md`).

### 2026-10-22 — FSR range in the pad, and a divider check

Raw telemetry logged on the Mac (every `STREAM_FSR_RAW` record, 12.7 Hz per
panel), the user on the pad. Channel order S, W, N, E.

**Full body weight** with one foot on each edge, then the centre, of U, L, C,
R and D: **every sensor reaches 4078–4095**; D hits 4095, the ADC's top. Full
weight on one edge drives three channels to ~4000–4090 and leaves the
opposite one lowest, so the bench figure of "full press ~4000" (finger
presses) undersold it: under body weight the sensors run to full scale.

**Orientation as installed:** on every sensored panel, the edge pressed left
the opposite channel lowest (top → S, right → W, bottom → N, left → E), so
S/W/N/E match the physical edges, N toward U. One ambiguous case: C's bottom
edge, where W and N were both near full scale.

**Resting in the pad** (medians, hands off): U 99–104, L 88–89, C 100–106,
R 107–109, D 109–114.

**Divider check:** known resistors on a DuPont-to-PH lead in UL's empty north
socket, values read on the user's DMM, against 4095 × 10 k / (10 k + R):

| resistor | model | measured |
|---|---|---|
| 0.9985 kΩ | 3723 | 3743 |
| 2.1875 kΩ | 3360 | 3385 |
| 3.302 kΩ | 3078 | 3100 |
| none | 0 | 13 (the other empty corners 13–18) |

The divider behaves as designed, reading ~20–25 counts (~0.7%) high across
the range, with a 13–18 count floor on an empty input. Cause not
investigated: ADC offset and gain, and the 10 k's tolerance, are the
candidates.

### 2026-10-22 — underglow `u` retested in the pad: works

- **First try, "it turns off and does nothing else"** (the user). The strip
  was at its power-on full white (`docs/UNDERGLOW.md`), and the console run
  printed both `u` lines.
- `g 44` (group 44 alone, 20/255 white) was easy to see from the far end.
- **Second try, the user watching the far end directly, starting from
  `g 44`:** red, green, blue, white, off. ✅
- **Third try, the user's own `u` after re-plugging the strip** (so from
  power-on full white): worked. ✅
- **Why the first try showed nothing: not established.** ~~From full white,
  `u`'s levels read as "it just went off".~~ The third try, also from full
  white, worked, so that isn't it on its own. More likely: the first send was
  a bare `u` with no newline, which sat in the console's buffer until a
  newline went out about a minute later, so the run happened later than
  expected and the colours (3 s in all) were missed.
- **The console is line-based:** a bare `u` sits in the buffer until a newline.
- **Side effect:** `u` blocks the master for ~3 s, so the console printed
  "STOPPED REPLYING" for all nine panels, which came straight back with
  config matching. Nothing reset.

### 2026-10-22 — a real duplicate panel ID: the warning misses it

UL's DIP set to ID 1 (the same as U), then the left column's 12 V cycled
(one panel can't be cycled alone in the pad, so UL, L and DL all rebooted).
Master console watched for 2+ minutes:

- L and DL came back; **slot UL: "NO REPLY"** for the whole run.
- "panel 1 (U) is on defaults again (rebooted?)": UL, booting at address 1.
- **Slot U answered nearly every poll, with UL's data** (FSR 13, 13, 13, 12,
  the empty-socket floor; U's sensor rests ~100). **2 CRC errors** in 2+ min.
  The two boards did not collide: one got through intact, the other
  vanished. Why UL wins isn't established; a guess is driver contention, with
  UL next to the master at the start of the chain and U sixth.
- **No duplicate-ID warning.** It expects two boards on one address to
  corrupt each other below 50% replies. Here the duplicate looks like one
  panel missing, which the warning deliberately excludes (0% = absent).
- **`pad_cmd.py selftest` caught it:** `slot UL: panel 1 <-- MISMATCH`,
  `slot U: panel 1`.

**Not fixing now** (the user). Options if it's ever needed: a passive check
that each slot's INT agrees with its ID's bus press mask (preferred; no cost
during play, also catches swapped or unlanded INT wires, needs a press to
notice), or running the self-test automatically (~0.6 s of held input and no
bus traffic per run). Until then: run the self-test after changing any DIP.

UL set back to ID 0 and the column cycled: self-test 9/9 slots on their own
IDs, config 9/9 matching.

### 2026-10-22 — presses on the four corners: all 16 sockets pass

One borrowed FSR, moved through every socket of UL, UR, DL and DR in turn
(plug in, finger press ~2 s, release, unplug), loose in the hand. Logged on
the Mac: every raw telemetry record, and `hid_monitor.py` on the joystick.

| corner | channels that pressed | finger-press peaks | joystick |
|---|---|---|---|
| UL | N, W, E, S | 3508–3834 | UL on every press |
| UR | W, S, N, E | 2915–3807 | UR on every press |
| DL | N, W, E, S | 3276–3770 | DL on every press |
| DR | N, W, S, E | 3557–3816 | DR on every press |

- Every socket moved **one channel only**, a different one for each socket,
  and set that channel's press bit in the bus reports.
- **Every press reached the joystick on the right button, with no sequence
  gaps** (seq 211 → 45, wrapping, all steps of 1).
- Light touches near the 500/400 thresholds chattered press/release a few
  times (UR W at ~490); expected with finger presses that hover at the
  threshold.
- Empty-socket floors before and after: UL/DL 13, UR 18, DR 16–17.

With the five sensored panels earlier the same day, **presses have now passed
on all nine panels in the pad.**

### 2026-10-22 — master and panels reflashed; test-mode colours and floor

- **Master #2 was still on a build from before `efcd3e0`** (its help still
  listed `U`, build `DCCB0190`). Flashed with `build.sh upload`: build
  `D52740E2`, `U` gone, saved config intact, all nine answering.
- **Panels:** test mode's east and south colours swapped to match the stock
  pad's sensor test (east green, south yellow), and its dark floor raised from
  100 to 200 counts (`docs/PANEL_CONFIG.md` → "Sensor test"). Bus update to
  all nine: image `15D82BA6` (was `5197D3AF`), staged and CRC-checked in
  0.6 s, build tag `5CB84A94`. After: 9/9 present, 100% polls, 0 CRC errors,
  config matching.
- **Test mode checked in the pad (the user):** dark at rest, colours right
  (top blue, right green, bottom yellow, left red).
- **Next to a stock pad in its own sensor test, the stock one *feels* more
  responsive** (the user), as if it starts lighting sooner. Not measured.
  **DECIDED 2026-10-22: not chasing it now** (the user): it's minor, and a
  like-for-like check (the same resistor in a corner socket of each pad)
  means lifting panels. Revisit once tare and calibrated values exist: try
  the floor as tare plus a small margin (the user's guess at what stock does:
  tare + ~20; not verified).

### 2026-10-22 — tare tracking and calibrated thresholds, flashed

Design and constants: `docs/PANEL_CONFIG.md` → "Tare and calibrated values".

- **Master #2** first (build `F93A0CC5`, USB protocol 3, config version 2),
  so it still understood the old panels' 9-byte `'f'`. It found the saved
  version-1 config and booted on defaults with "config was reset", as
  designed; `pad_cmd.py reset-config --yes` then saved the version-2
  defaults and the flag cleared.
- **All nine panels** over the bus: image `C6C0E71B`, build tag `4B5A43FA`.
- **After:** 9/9 present, 100% polls, 0 CRC errors, config matching at
  930/905 on every channel, termination OK. Telemetry still **12.7 Hz** per
  panel with the 17-byte `'f'`.
- **Tares, a few seconds after boot, hands off:** L 87–88, C 99–106,
  R 106–108, D 109–113, empty corners 12–15: each about 1 count under its
  instantaneous raw value (a 64 ms mean against one sample).
- Host checks before flashing: `firmware/shared/test/run.sh` (C and C++,
  including "a raw comparison and a calibrated one agree to the count") and
  the SDK's 30 tests, clippy clean.
- **The user, stepping on it:** the sensitivity at 930/905 "seems pretty
  good"; test mode from tare + 20 looks closer to the stock sensor test, good
  enough for now.
- **Still to do:** ~~a logged play session in ITGmania to tune the tracking~~
  (first session the same evening, below).

### 2026-10-22 — first logged play session (ITGmania)

The user played a few songs in ITGmania on the installed pad (their second
session of the day). Logged on the Mac alongside the game: every telemetry
record with tares (`pad_cmd.py stream --csv`) and every joystick report
(`hid_monitor.py`). Bus counters zeroed first. **15.8 minutes.**

- **Bus:** 0 CRC errors on all nine, 100% polls, 12.7 Hz per panel throughout.
- **Joystick:** ~12,100 state changes, **no sequence gaps**; presses only on
  the five sensored panels (U 856, L 1,360, C 1,304, R 1,547, D 1,005).
- **Tares held to ±1 count on every sensor for the whole session** (e.g. U
  N 102–103, L N 86–88, D all unchanged). No drift to track over 16 minutes
  of play; the ±1 flips are rounding. Longer sessions, a cold start and a
  warm room are still unmeasured.
- **Found: fast re-presses.** **307 times (~5% of presses)** a panel pressed
  again **less than 30 ms after releasing** (C 129, R 70, L 60, U 35, D 13).
  Gaps: 17 under 1 ms, 31 of 1–3 ms, 79 of 3–10 ms, 180 of 10–30 ms. None of
  that is a human re-step. 214 followed an ordinary 50–300 ms step, 79 a
  sub-50 ms blip, 14 a hold of over 300 ms. 128 presses were held under
  15 ms. The panel commits a channel's release after ~0.1 ms of persistence
  and its press output is the OR of its channels, so a foot unloading for a
  moment (landing bounce, or weight passing from one sensor to another) shows
  up as release + press. **Cause not established**: 12.7 Hz telemetry can't
  resolve it. Whether ITGmania was affected isn't known yet (the user). A
  panel-level release debounce is the obvious candidate fix (stock's config
  carries a 15 ms debounce and a 4 ms panel debounce, semantics unverified);
  not decided.
- **~~~5% of presses are artefacts~~ — most are probably real jacks** (the
  user's point: charts have 16ths and faster at 180+ BPM, and 16th jacks at
  180 BPM are an 83 ms cycle). Re-checked by the press-to-press cycle on the
  same panel: **218 of the 307 had a cycle of 75 ms or more**, which fits real
  fast jacks; the release threshold (905) is a firm force, so a jack only
  has to unload a little to release, and a 10–30 ms dip is plausible. **About
  100 (~1.6% of presses) still look like artefacts:** 66 with a whole cycle
  under 50 ms (over 20 presses a second on one panel) and 34 more with a
  release under 3 ms. Without the charts this stays an estimate. The user
  noticed nothing odd while playing.
- **DeadSync** (the user, from an earlier try, not yet re-checked): its input
  checker seemed to treat each change of the joystick report's **sequence
  number** as a new button. The report puts the sequence on vendor page
  0xFF00 so games ignore it (`docs/USB_PROTOCOL.md` → "Input state report");
  how DeadSync reads the report is not known yet.

### 2026-10-22 — 2 ms panel debounce on press and release

Design: `docs/PANEL_CONFIG.md` → "Press debounce". Bus update to all nine:
image `32BBFD7C`, build tag `D89F27B9`; after it 9/9 present, 100% polls,
0 CRC errors, config matching. Master unchanged (`F93A0CC5`).

- **Self-test:** 9/9 slots on their own IDs; its 2 ms pulses get through
  (it first refused while the user stood on U, as designed).
- **Tap check (the user: random taps on all five, then quick jacks on all
  five; `hid_monitor.py`):** 88 presses, every one on the right panel, no
  sequence gaps; jacks at 130–145 ms per press. "Looks good" (the user).
- **Two quick re-presses got through, both longer than 2 ms:** R, a 9 ms
  press, 7.8 ms released, then the real press (a landing bounce?); D, a
  4 ms press, 28 ms released, then the real press (the foot grazing the panel
  first?). A 2 ms debounce removes sub-2 ms wobble only; events like these
  need a longer one or a different rule, and real jacks lift for 10–30 ms.
  The next logged session will show how common they are.

### 2026-10-23 — no-master standby flashed

Design: `docs/PANEL_CONFIG.md` → "LED modes" (`b6c6bc8`). Bus update to all
nine from the Mac mini (`panel_update.py`, master #2 unchanged on
`F93A0CC5`): image SHA-256 `ac2e0d36…` on both Macs, CRC-32 `86C25F70`,
build tag `28520B01`, replacing `32BBFD7C`. Streamed in 0.6 s, all nine
staged, CRC-verified and committed. After it: 9/9 present, 100% polls, 0 CRC
errors, config matching, termination OK (DR), self-test 9/9 slots on their
own IDs.

- **Standby check (the user, master's USB unplugged and replugged):** every
  panel breathes; presses light the panel white on all of them; on replug
  the master's playback resumes after a second or so. **The breath looks
  choppy** (the user). Not the frame rate: the panels render at ~60 Hz
  (`LED_REFRESH_MS` 16). Inferred, not measured: a peak of 24 gives only 24
  brightness steps, and the squared curve holds the dimmest ones longest
  (0 for 409 ms, 1 for 169 ms, 2 for 130 ms, where each step is a 2× or 1.5×
  jump in light), so the dim end visibly steps.
- **After the replug:** config matching on all nine, self-test 9/9, 0 CRC
  errors. `pad_cmd.py status` showed **98% polls on one panel** in 10 of 12
  samples over a minute: never two panels at once, never below 98%, a
  different panel almost every time. That fits one poll sent before the
  master's 5 s window closed and answered after it (one of ~63 per window),
  not a missed reply. ~~Not proven: the master keeps no per-panel lifetime
  sent count to compare against.~~ Confirmed by the play session the same
  day: no gap in 99 minutes of telemetry long enough to hide a missed poll
  (below).
- **Dithered breath** (the user chose it): the breath now steps in 1/25 of a
  level, every LED at the whole level and the remainder one level up in a
  spread-out order (`STANDBY_DITHER` in `main.c`). Image SHA-256 `459875fc…`
  on both Macs, CRC-32 `0D79E34A`, build tag `C9EB5F2E`, flashed to all
  nine. After it: 9/9 present, 100% polls, 0 CRC errors, config matching,
  self-test 9/9. DR showed the old tag for a few seconds until the master's
  rolling `H` status reached it. **Look: "looks great"** (the user, master's
  USB unplugged again).

### 2026-10-23 — second logged play session (DeadSync, 2 ms debounce)

The user played DeadSync on the Mac mini with the panels on `C9EB5F2E` (the
2 ms press/release debounce plus the standby). Logged on the Mini alongside
the game, the same way as the first session: `pad_cmd.py stream --csv` and
`hid_monitor.py`, bus counters zeroed first. **99.2 minutes** (08:53–10:32).
Not a cold start: the pad had been powered for a while. The logs are kept
outside the repo (`~/krakenpad-logs/2026-10-23-session2/` on both Macs).

- **Bus:** 0 CRC errors on all nine at the end, config matching. Telemetry
  **12.68 Hz per panel** throughout, one record per `'f'` reply; **the longest
  gap between any panel's records was 91 ms** (median 80), where one missed
  poll would leave ~160 ms. So no poll went unanswered in 99 minutes, and the
  98% the status showed now and then is the 5 s window's edge (above), not a
  miss.
- **Joystick:** 53,561 state changes, **no sequence gaps**; 26,781 presses
  (U 4,001, L 6,033, C 5,892, R 5,996, D 4,859).
- **Closest presses on two different panels** (host time, `hid_monitor.py`
  stamps at 0.1 ms): 876 pairs under 1 ms apart, 223 under 0.2 ms, the
  closest 0.1 ms (the log's resolution). **Every press arrived in its own
  report** (26,779 reports with one new press, none with two), each a
  sequence step of exactly 1. These are host receive times: macOS can hand
  over queued reports together, so they bound how close the reports
  *arrived*, not how close the feet landed (a device timestamp would:
  `docs/TIMESTAMPED_INPUT.md`).
- **"Some weird misses"** on harder charts (the user); "hard to tell if it's
  just me". Nothing in the logs points at the pad (no sequence gaps, no
  missed polls), but the logs can't show a step that never crossed the
  press threshold: telemetry at 12.7 Hz can't tell a sub-threshold step from
  an ordinary press edge.
- **Tares held within ±1–2 counts on every sensor for 99 minutes** (the
  widest: U N 101–103, R ch 2 108–110, D ch 2 109–111, D ch 3 112–114).
  Full weight still reaches 4074–4095.
- **Re-presses under 30 ms after a release: 954, 3.6% of presses** (5.1% in
  the first session). Gaps: **none under 1 ms (17 before)**, 15 at 1.7–2 ms,
  64 at 2–3 ms, 297 at 3–10 ms, 578 at 10–30 ms. The 1.7–2 ms ones are the
  2 ms panel minimum less host timestamp jitter (`hid_monitor.py` stamps on
  the host), so the debounce does what it is meant to: wobble under 2 ms is
  gone. Wobble just over 2 ms still gets through.
- **Look like artefacts: 288, 1.08% of presses** (~1.6% before): 222 with a
  press-to-press cycle under 50 ms, 66 more with a release under 3 ms. 665 of
  the 954 had a cycle of 75 ms or more, which fits real fast jacks. By panel:
  U 44, L 82, C 72, R 84, **D only 6** (D was lowest in the first session
  too; why isn't known). Presses held under 15 ms: 550, 2.1% of presses,
  the same share as before (2.1%), so the debounce didn't change those.
- **Not a controlled comparison:** a different game (DeadSync vs ITGmania),
  different songs, six times as long. Per 1,000 presses: release gaps under
  3 ms 7.9 → 2.9, cycles under 50 ms 10.9 → 8.3, 3–10 ms gaps 13.0 → 11.1.
- **Tool bug found:** `pad_cmd.py stream`'s closing summary printed 45.5 Hz
  per panel. The master's µs timestamp is 32 bits and wraps after 71.6 min,
  and the summary's rate doesn't unwrap it. The CSV is fine (unwrap
  `master_us` when reading it). **Fixed the same day**: replaying this
  session's CSV through `cmd_stream` printed 45.5 Hz before and 12.7 Hz after.

### 2026-10-23 — cold start

12 V off from ~10:51 to ~13:12 (about 2 h 20 min, a fan on the pad for the
first part; the master stayed on USB). The stream and `hid_monitor.py` were
started on the Mini before the user switched 12 V on, nobody on the pad.

- **All nine up**, config matching, `C9EB5F2E`. One CRC error on C, the usual
  count from panels booting into live traffic.
- **No presses at boot** (no joystick change at all).
- **Tares started at 100 and settled within seconds to the warm values:**
  every sensor within 1 count of where it ended the 99-minute play session
  that morning (e.g. U 102,98,101,99 cold vs 101,97,101,98 warm; C and R
  identical). So rest barely depends on temperature here, at least between
  a warm pad and ~2 h off in this room. Not an overnight cold soak.
- **Rest over the 2 hours after the cold boot (13:11–15:11, nobody on the
  pad):** flat. Every sensored channel's mean raw value moved **at most 0.2
  counts** between the first and last half hour (e.g. D 112.6,109.1,110.1,113.4
  → 112.6,109.1,110.1,113.3), and the tares only flipped by 1 count, which is
  rounding. Bus: no CRC error after the one at boot, 100% polls, no joystick
  change in 2 h 50 min. The stream's closing summary printed 31.5 Hz: the
  Mini still ran the unfixed `pad_cmd.py` (91,123 records in 7,200 s is
  12.7 Hz).

### 2026-10-23 — cable pulls and power cuts during play (tests 1–4)

The user, playing DeadSync on the Mac mini, at the master end of the pad
(the plan is in `docs/STATUS.md` → "Not yet tested"). DeadSync doesn't use
C, so **U** stood in for it. Logged on the Mini: `pad_cmd.py stream --csv`
and `hid_monitor.py`, each line stamped with wall-clock time on arrival
(`tr` block-buffers into a pipe, so the first attempt's stamps were useless;
a small unbuffered filter replaced it). Counters zeroed first.

| test | result |
|---|---|
| 1. master's RS-485 `J1` pulled mid-song, ~10 s | ✅ **Gameplay unaffected**: 165 joystick changes while the bus was out, no sequence gap. All nine `GONE` (detected 454.1), panels breathing after about a second (the user); on replug the master's frames returned "almost immediately", every panel `present` and `matches` within 0.13 s, no reboot |
| 2a. U's INT pulled at the master mid-song | ✅ U stopped working, the rest played on (the user) |
| 2b. standing on U, U's INT pulled, replugged while still on it | ✅ release at the pull (563.14), **10.8 s with no change on any panel**, press at the replug (573.98), nothing else changed |
| 4a. whole pad 12 V off at the rocker mid-song | ✅ every held panel released together as the supply dropped; no change at all during the ~15 s off; on power-up the first press came at the same moment the panels reappeared on the bus (C pressed 672.847, UL `present` 672.847), all nine "not configured" then `matches` within ~0.1 s. **Open:** D pressed for 7 ms 6 ms before the all-release (657.489), mid-song with L and R held. A foot or a brown-out artefact? Telemetry (80 ms per panel) can't tell. **Repeat, nobody on the pad: not reproduced** (below) |
| 4b. 12 V at the XT30, standing still on U | ✅ U released at the cut (692.13); **U pressed as soon as it booted** (699.014, the same instant as UL `present`), released when the user stepped off (703.54), then normal play. No other change during the cut |
| 3. master's USB pulled mid-song, ~10 s | ✅ buttons stopped; **DeadSync picked the pad back up 1–2 s after the replug** and it worked again (the user). Both loggers died with the device, as expected. After: all nine present, 100% polls, 0 CRC errors, config matching, self-test 9/9 (the first try refused: "INT already LOW on R", the user sitting on R) |

- **No sequence gaps** in 940 joystick changes over the run.
- **Bus presence lags the real event by up to ~1 s** (the master's reply
  timeout): the 4a all-release is at 657.50, `GONE` at 658.40.
- **Repeat of the cuts with nobody on the pad** (the D question): 4 at the
  rocker and 2 at the XT30 (the user lost count of the rocker ones; the log
  shows six), ~5 s off and ~10 s on each. **Not one joystick change in six
  cuts and six power-ups.** After: 1 CRC error (DR, a panel booting into
  live traffic), config matching, self-test 9/9. So the 7 ms D press doesn't
  reproduce at rest. The user doesn't think a foot was near D, so the cause
  isn't established; what differed in play was L and R held and the game's
  load. ~~If it matters, repeat the cuts while standing on two panels.~~
- **Again, the user standing on L and R** (the first run's state): four cuts,
  two at the rocker and two at the XT30.
  **Only L and R ever changed**: each cut released both within ~1 ms of
  each other, each power-up pressed both again within ~6 ms, and the press
  came ~10 ms *before* the panel's bus presence (INT is live before the bus
  answers). 3 CRC errors on L across the four power-ups, the usual boot
  noise; config matching after. **Ten cuts in all, no false press.** The
  7 ms D press stays a one-off with no cause found.

### 2026-10-23 — panel D lifted: its 12 V, its RS-485 IN, one FSR (tests 5, 5c, 6)

D is the end of the centre column, so its 12 V IN (`J205`) feeds only D;
its RS-485 sits mid-chain (DL → D → C). Logged as for tests 1–4. 39 joystick
changes, no sequence gap.

| test | result |
|---|---|
| 5. D's `J205` pulled and replugged, at rest | ✅ no joystick change on any panel through the pull and the boot; D `GONE`, back as "not configured" then `matches` 5 ms later (a reboot) |
| 5b. the same, pressing one of D's FSRs by hand | ✅ D released at the pull (736.50; `GONE` a second later), **pressed as soon as it booted** (745.09, before its bus presence at 745.17), released when let go |
| 5c. D's RS-485 IN (`J204`) pulled ~15 s, splitting the bus | ✅ **exactly as predicted** (the user): the six cut off (D, C, U, UR, R, DR) went to the standby breath and all still worked as buttons (U, C, R, D pressed; UR and DR have no sensor). UL, L and DL, left with no termination at their far end, kept **0 CRC errors**. On replug all six back, config matching, no reboot. **Each of the six counted exactly 35 panel-side CRC errors**: the same count on all six suggests they saw the same corrupted master frames, most likely while the connector was half mated going in or out (inferred, not proven). The CRC rejected them; nothing acted on them |
| 6. D's North FSR (`J206`, channel 2) unplugged ~8 s, replugged, pressed | **Prediction confirmed**: the tare fell from 110 to 22 within ~0.2 s of the unplug (the empty input reads 22) and **stayed at 22 after the replug** while the sensor rested at 110–111 again, to the end of the log. Presses still worked (four by hand, all on the joystick): the threshold moved only ~6 raw counts. The tare stays wrong until D reboots. No joystick change at the unplug or the replug |

After: all nine present, 100% polls, config matching, self-test 9/9.

**Fixed the same day** (the user's call: a tare of 50 or less means no
sensor, `TARE_EMPTY_MAX` in `main.c`, `docs/PANEL_CONFIG.md` → "Tracking, as
built"). Bus update to all nine: image SHA-256 `d244a6b7…` on both Macs,
CRC-32 `019BECAE`, build tag `A0D39608`; after it 9/9, config matching,
self-test 9/9, D North's tare back at 110 from the reboot. **Retest, D North
unplugged ~16 s and replugged:** tare 110 → 22 at the unplug (flicking 22/23
while empty), **back to 110 within one telemetry interval (≤0.17 s) of the
replug**, and it held there through four presses by hand (all on the
joystick) and after them. No joystick change at the unplug or the replug.
Faster than the predicted 1–2 s: the 1 s hold-off only follows a press or
release, and there was none.

### 2026-10-23 — telemetry poll rate (master `poll`, STATUS next step 9)

Master #2 moved to the MacBook for this, flashed with build `BBD6681D`: the
same firmware plus a console `poll [ms]` that sets the FSR poll interval
until reboot (default 5). Nine panels on `A0D39608`, nobody on the pad, the
master's own 60 Hz LED frames. A script on the MacBook stepped the interval,
zeroed the counters (`z`), streamed telemetry for **3 minutes per step**
(`pad_cmd.py stream --csv`) and ended each step with an `H` sweep. Pass
criteria (the user agreed): 0 CRC errors, every poll answered, no gap over
two poll cycles, LED frames held at 60 Hz.

| interval | per panel | median / max gap | LED frames | errors |
|---|---|---|---|---|
| 5 ms (default) | 12.68 Hz | 80.0 / 91.2 ms | 62.4 Hz | 0 |
| 4 ms | 19.62 Hz | 51.8 / 60.2 ms | 62.5 Hz | 0 |
| 3 ms | 19.62 Hz | 50.8 / 61.2 ms | 62.5 Hz | 0 |
| 2 ms | 33.51 Hz | 30.0 / 40.2 ms | 62.5 Hz | 0 |
| 1 ms | 61.29 Hz | 16.0 / 25.2 ms | 62.5 Hz | 0 |
| **0 ms** | **113.42 Hz** | **12.0 / 13.2 ms** | 62.5 Hz | 0 |

- **Every step passed**: 0 CRC errors at the master, 0 crc/uart/ovr/turn/tmo
  on every panel, no gap over twice the median, 0 stream records dropped on
  USB, every `H` sweep answered 9/9. All nine panels ran at the same rate.
- **0 ms means a poll whenever the last reply window has closed**: ~1,020
  polls a second, about 0.55 ms each in the time the LED bursts leave.
  **9× the default** with no errors. The 1 ms estimate (~60 Hz) held:
  61.3 Hz.
- 3 ms and 4 ms gave the same rate. Inferred, not checked: with `millis()`
  scheduling and a burst every 16 ms, both land on the same number of poll
  slots per LED frame.
- **Not yet covered:** presses and play (the pad was idle), a long run (3 min
  per step), and host LED frames at the faster rate. The default stays 5 ms
  until the user decides.

### 2026-10-23 — many simultaneous presses, at `poll 0`

The same setup (master #2 on the MacBook, `BBD6681D`, panels `A0D39608`),
with the poll interval at **0 ms**. A script set `poll 0`, zeroed the
counters, then for 200 s logged every joystick report (host time, sequence,
mask) and the telemetry stream (`pad_cmd.py stream --csv`). It ended with
`j` and `H` and put the interval back to 5. The user shuffled across U, L, C,
R and D for **about 35 s** (40–75 s into the log), feet often on two or more
panels at once; the rest of the log is the idle pad.

- **No lost transitions.** 613 joystick presses (U 113, L 71, C 164, R 132,
  D 133), **0 sequence jumps**, master queue-full 0, resyncs 0. **No report
  carried more than one panel's change**: every edge got its own report.
- **Up to five panels down at once**, as logged: 3 down for 12.5 s, 4 for
  7.6 s, 5 for 2.4 s.
- **The joystick and the telemetry agree.** Every telemetry press (598) has a
  joystick press. Every joystick press but one has a telemetry press; the
  exception is an 11.4 ms press on C, near the 8.8 ms between a panel's
  telemetry samples. The telemetry count is lower because a release of a few
  ms falls between its samples: 14 re-presses under 5 ms (C 10, U 2, R 2,
  shortest release 1.1 ms by host time), against 15 presses fewer. Press and
  release edges agree to −7.6 / +5.7 ms (min / p95), median +0.9 ms: the
  spread is the telemetry sample interval.
- **The bus under load matched the idle run:** 113.4 Hz per panel, longest
  gap 13.2 ms, LED frames 62.5 Hz, 0 CRC errors, every poll answered, 0 on
  every panel's crc/uart/ovr/turn/tmo, `H` 9/9, 0 stream records dropped.
- **Unexplained: the master's `glitches` counter read 24.** It was 0 after
  every idle poll-rate step, so it comes with presses. It counts INT
  interrupts after which the line, re-read 10 µs later (`INT_QUALIFY_NS`),
  already matched the mask: an edge that didn't stick, or a second interrupt
  on an edge already handled. Not a lost press: resyncs 0 means the 1 ms
  check never found a line disagreeing with the mask, and the host got every
  report. Inferred, not checked: a slow or noisy rising edge (10 kΩ pull-up)
  crossing the threshold twice. The master's edge log (`pad_cmd.py events`)
  or a scope on one INT line at the master would show which.
- **Bench-method note:** the first comparison found ~25 mismatches each way.
  They came from the logger, not the pad: the telemetry's host times count
  from when the `pad_cmd.py` process started, **117 ms** after the joystick
  log's, and a median alignment over the whole log fit that offset badly.
  With it removed, the logs match as above. Log both from one process next
  time, or stamp both with the same clock.

Log: `~/krakenpad-logs/2026-10-23-shuffle/` on the MacBook.

### 2026-10-23 — host-side joystick poll rate (master `usbpoll`)

New master build **`24D39879`** on master #2, still on the MacBook. The Teensy
core patch now gives the joystick endpoint a transfer-complete callback
(`usb_joystick_tx_done()`, from the USB interrupt, with the cycle count at
which the report was queued). The master uses it two ways:

- **Every report's wait**, queued to taken by the host, now in `j` (zeroed
  by `z`).
- **`usbpoll [s]`**: for *s* seconds the callback queues the next report the
  moment one is taken, so one is always waiting, and times the gaps between
  takes. That gap is the host's actual poll interval. Presses are held back
  while it runs.

The panels were on the bus as normal. No game was running on the MacBook,
and nothing had the joystick open.

| run | takes | per second | gap min / median / max | gaps over 150 µs |
|---|---|---|---|---|
| 5 s (first try) | 39,994 | 7,999 | 124 / 130 / 500 µs | 2 (≈360, ≈505 µs) |
| 60 s | 480,003 | 8,000 | 124 / 125 / 125 µs | 0 |
| 60 s | 480,000 | 8,000 | 124 / 130 / 375 µs | 1 |
| 60 s | 480,003 | 8,000 | 124 / 125 / 125 µs | 0 |

(Medians are bin edges, 5 µs bins.)

- **The MacBook polls at the full 8 kHz, steadily**: 3 gaps over 150 µs in
  1.48 million, the longest 0.5 ms.
- **Idle reports waited at most 84–128 µs** before the host took them, which
  is one poll interval. The heartbeat comes from the 1 kHz timer, which stays
  in near-fixed phase with the 125 µs microframes. The waits therefore bunch
  into a few bins instead of spreading evenly. Presses, which arrive at any
  time, sample the whole interval.
- **What this doesn't show:** when the host's software reads the report.
  This is the host controller's schedule. macOS's HID stack and the game come
  after it, and their delay is what session 2's host timestamps include.
- **The Mac mini that runs DeadSync, same day** (the master's USB moved
  there, `usbpoll` run over SSH, DeadSync running and idle, not in a song):

  | run | takes | per second | gap min / median / p99.9 / max | gaps over 150 µs |
  |---|---|---|---|---|
  | 5 s | 39,998 | 7,999 | 103 / 130 / 145 / 146 µs | 0 |
  | 60 s | 479,988 | 7,999 | 101 / 130 / 145 / 379 µs | 3 |

  Also 8 kHz, with more spread than the MacBook: gaps of 101–146 µs, against
  124–125. 3 gaps over 150 µs in 480,000, the longest 0.38 ms. Idle
  reports waited 3–129 µs, spread evenly across the interval.
- **During a song in DeadSync (the Mini, same evening):** a 20 s `usbpoll`
  at the start of the song (the user let the arrows pass, since presses are
  held back) gave 7,999/s, gaps 99–145 µs, one of 374 µs in 160,000: the same
  as idle. Then `z` and the rest of the song played normally:
  **6,372 reports, each waited 3–129 µs before the host took it**, spread
  evenly (about 1,100–1,300 in each 25 µs bin), mean 66 µs. That is the
  whole delay 8 kHz polling adds to a press; heartbeats and presses are
  counted together. `glitches` rose by 1, resyncs 0, and the panels' `H`
  counters stayed at 0.
- **Not yet measured:** Windows and Linux.

### 2026-10-23 — RS-485 bus and INT scoped at DR, in the pad

**Setup.** Siglent SDS804X HD (70 MHz, 12-bit, firmware 4.8.12.1.1.6.5). Panel
DR lifted. C1 on A (`TP202`), C2 on B (`TP204`), C4 on INT (`TP211`), ×10
probes grounded at the DR panel's GND test points. Math A − B. The master's
DE (`TP2`) can't be reached with the master installed, so there was no DE
trigger. The user set the scope up by hand and took the first look (UART
trigger on the 0x55 sync, photos). The scope's LAN SCPI was then driven from
the Mac mini, which can reach it; the MacBook's TCP attempts failed even
though ping worked. That gave 12 single-shot captures per case, 5 ms each at
500 MSa/s (2 ns), triggered on 0x55. They were decoded off-line: packets
split by the length field, the sender named from the command byte. The master
was at `poll 0`, so each window held polls and replies from all nine panels
as well as LED frames. Master `24D39879`, panels `A0D39608`. Raw data and the
scripts are in `~/krakenpad-logs/2026-10-23-scope/` on the MacBook.

**Baseline (DR `SW202` on, `H`: termination OK), 211 packets:**

| sender | levels | 10–90% edge | overshoot | closest approach back towards 0 V after an edge |
|---|---|---|---|---|
| master (polls, LED, config, status) | +2.29 / −2.26 V | 28 / 30 ns | 12% | 1.12 V |
| panel replies, UL … D | +2.28 … 2.42 / −2.24 … −2.38 V | 22–36 ns | 1–13% | 0.95–1.17 V |
| DR (the driver at the probe) | +2.46 / −2.41 V | 158 ns | 3% | **0.81 V** |

**Case A (DR `SW202` off; `H`: "no panel terminated"; only the master's
120 Ω), 214 packets:**

| sender | levels | 10–90% edge | overshoot | closest approach back towards 0 V after an edge |
|---|---|---|---|---|
| master | +2.96 / −2.81 V | 28 / 30 ns | 19% | 1.39 V |
| panel replies, UL … D | +2.83 … 2.87 / −2.78 … −2.81 V | 20–34 ns | 3–20% | 0.77–1.41 V |
| DR | +2.86 / −2.81 V | 162 ns | 2% | **0.73 V** |

- **Wide margin in both cases.** The eye at bit centres is at least 2.21 V
  (baseline) and 2.74 V (case A). Every stop bit is good. After any edge the
  line never comes back closer to 0 V than 0.81 V (baseline) or 0.73 V
  (case A), against the receivers' ±200 mV. The panel and master counters
  stayed at 0 throughout.
- **Termination at DR lowers the level** (±2.3 V against ±2.8 V, one more
  120 Ω load) **and the master's overshoot (12% against 19%)**. Unterminated,
  the ringing is larger but stays far from the threshold, as on the
  2026-10-18 eight-panel bench (cases A–D, counters only).
- **The edge shape depends on how far the sender is from the probe.** This
  is inferred, not measured. DR's own reply shows the near-end staircase: a
  fast step to about +1.7 V, then a ramp to full level as reflections come
  back from up the chain. That makes the 10–90% figure 158–162 ns, but the
  step crosses ±200 mV in a few ns. A distant sender's edge arrives as one
  fast step with some ringing. Overlaid with persistence, the mix looks like
  edge "wobble" of up to ~0.5 µs (the user's first look). It is several
  senders' shapes on top of each other, not jitter.
- **Idle (undriven) gaps:** 42–204 µs long, typically 45 µs: the panel's
  turnaround between a poll and its reply. The line rests at +3…6 mV with at
  most 7 mV rms noise, peaks within ±40 mV. **When a driver lets go**, A − B
  decays from the mark level with no negative excursion (min −0.04 V in
  351 releases), so nothing a receiver could take for a start bit.
- **Bus edges couple into DR's INT line:** spikes of up to 110–121 mV within
  200 ns of a bus edge, against ≤ 31–33 mV (p99.9) at other times, on the
  3.29 V idle level. Not checked whether that is the cable or the probe
  ground.

**INT at DR, during the master's `I` self-test** (edge trigger on C4 falling,
3 captures of DR's ~2.03 ms pulse):

- **Fall (the panel pulls low): 1.6 µs**, clean, low level 0.08 V.
- **Release: 67 µs 10→90%** (18.8 µs 10→50%), **0.055 V/µs at mid-rail**,
  consistent with an RC of about 30 µs. Inferred: 10 kΩ `RN1` parallel with
  the Teensy's own pull-up (`INPUT_PULLUP`), into the line's capacitance (the
  master's 1 nF plus cable and TVSs).
- **The slow release crosses 1.65 V 19–41 times within about 350 ns**:
  5.7 mV rms noise plus the bus-coupled spikes on a slope that shallow.
- **This fits the master's `glitches` counter** (24 in the 2026-10-23
  shuffle, 1 in the DeadSync song): extra interrupts on a release, each
  rejected by the 10 µs re-read because the line has risen ~0.5 V by then.
  Not yet seen at the master's own pin: it has the 330 Ω / 1 nF filter and
  the pad hysteresis `INPUT_PULLUP` enables (`IOMUXC_PAD_HYS`). That end has
  no test point, so seeing it needs the JST or the board out. No press was
  lost (resyncs 0 in every run). A press edge is the fast fall.

**The bus under full white (`w`, the user watching, ~2 min, DR terminated),
12 captures, 202 packets:**

| sender | levels | eye | closest after an edge | CM, (A+B)/2 at DR's GND | the same, LEDs at normal |
|---|---|---|---|---|---|
| master | +2.32 / −2.22 V | 2.19 V | 1.11 V | 1.38–1.40 V | 1.40–1.41 V |
| panels UL … D | +2.31 … 2.45 / −2.21 … −2.35 V | ≥ 2.18 V | ≥ 0.92 V | 1.49–1.52 V | 1.51–1.52 V |
| DR | +2.49 / −2.37 V | 2.35 V | 0.79 V | 1.51 V | 1.51 V |

- **Full white makes no difference that matters.** Edges, overshoot, eye and
  the closest approach are the same as with the normal LED pattern. Every
  stop bit is good. Panel counters were 0, and every poll was answered.
- **The common mode moved by 0.01–0.03 V.** Seen from DR's ground, the
  master's frames sit about 0.1 V lower than the panels' with or without the
  load. Inferred: that's the ground difference between the master and DR plus
  the drivers' own spread, and the LED return current barely adds to it.
- **A − B shifted by about +0.03 V** (high +0.03 … +0.05 V, low +0.04 V; the
  undriven gaps rest at +31 mV against +6 mV). That's small against ±2.2 V.
  Not checked whether it's the bus or the probes' ground.

**LED data at DR, `TP214` (`LED_DATA_5V`, the AHCT125 output into LED 1),**
C4 at 2 ns, 3 captures of 10 ms, 4 frames, normal master LED pattern. Limits
are from the Worldsemi WS2815 datasheet V1.1 (2017-10-10), "Data Transfer Time":

| | measured (min / median / max) | WS2815 V1.1 | |
|---|---|---|---|
| T0H, 0-bit high | 368 / 376 / 378 ns | 220–380 ns | in, 2 ns from the max |
| T1H, 1-bit high | 742 / 752 / 754 ns | 580 ns – 1.6 µs | in |
| T0L, 0-bit low | 870 / 872 / 882 ns | 580 ns – 1.6 µs | in |
| **T1L, 1-bit low** | **494 / 504 / 506 ns** | **220–420 ns** | **out, ~85 ns over** |
| bit period | 1,246 / 1,248 / 1,258 ns | (800 kbps) | |
| reset (low between frames) | ≥ 457 µs | > 280 µs | in |

- **The timings match the PIO program** (`ws2812.pio`: T1 3, T2 3, T3 4
  cycles of 125 ns at 800 kHz × 10): T0H 3 = 375 ns, T1H 6 = 750 ns, T0L 7
  = 875 ns, T1L 4 = 500 ns. **T1L is over the datasheet's maximum, and T0H
  sits at its edge.** The LEDs display correctly on every panel. Inferred
  (common for this family, not checked for the WS2815): the receiver decodes
  on the high time and is lenient about low times well short of the reset.
  Keeping the 10-cycle bit, T1 2 / T2 5 / T3 3 would give T0H 250, T1H 875,
  T0L 1,000, T1L 375 ns, all inside with margin. Not changed; it's the user's
  call.
- **Signal quality:** high 5.09 V, low 0.00 V, edges 32–34 ns 10–90%,
  overshoot and undershoot ≤ 0.06 V. Clean.
- **Frames:** exactly 600 bits (25 LEDs × 24) in 749 µs. In one capture two
  frames were 1.206 ms apart: `LED_FRAME_GAP_US` (1,200 µs), the panel's
  minimum spacing between paints. Probably a new `'L'` frame and the refresh
  timer falling together; not checked.

**LED timing fixed, flashed and re-scoped the same night.** `ws2812.pio`
changed from (3, 3, 4) to (2, 5, 3) cycles, the same 10-cycle 1.25 µs bit.
Built as image CRC `BB5E3DB3`, SHA-256 `e29d6838…`, build tag **`B3B9E68B`**.
Sent over the bus from the Mini (`panel_update.py --panels 8`, then
`--panels 0-7`). DR first, re-scoped, the user checked its LEDs by eye
("looks fine"), then the other eight. Afterwards: 9/9 on `B3B9E68B`, config
matching, 0 errors. The master missed 5 polls while the panels rebooted.

| | (3, 3, 4), before | **(2, 5, 3), after** | WS2815 V1.1 |
|---|---|---|---|
| T0H | 368–378 ns | **246–258 ns** | 220–380 ns |
| T1H | 742–754 ns | **870–882 ns** | 580 ns – 1.6 µs |
| T0L | 870–882 ns | **998–1,002 ns** | 580 ns – 1.6 µs |
| T1L | 494–506 ns | **366–378 ns** | 220–420 ns |
| bit period | 1,246–1,258 ns | 1,246–1,258 ns | |

Every value is now inside the datasheet with margin. The frame is unchanged:
600 bits in 749 µs, reset gap 458 µs. So are the levels (5.09 V) and the
edges (32–34 ns).

**12 V at DR (`TP203`/`TP205`, C3, ×10, DC, 20 MHz limit)**, 3 captures of
20 ms at 50 MSa/s each. Normal master LED pattern, then full white (`w`:
255, 255, 255 on all 225 LEDs, the user watching, ~30 s):

| | mean | min / max | 10 µs-averaged p-p | rms |
|---|---|---|---|---|
| normal LEDs | 12.36 V | 12.31 / 12.42 V | 19–24 mV | 3.9–4.2 mV |
| full white | 12.01 V | 11.94 / 12.07 V | 22–26 mV | 4.2–4.6 mV |

- **DR's 12 V drops 0.35 V at full white**, to 12.01 V, at the end of the
  right column, 3 panels down. Not known: the supply's own voltage at the
  Wago under the same load, so the split between the PSU and the harness
  isn't measured. 12.0 V is well inside the WS2815's supply rating
  (9.5–13.5 V absolute max, V1.1).
- **Ripple is small:** ~0.1 V p-p raw, mostly fast spikes that line up with
  bus and LED-data edges (seen at 20 mV/div). It averages to ~20–26 mV p-p
  over 10 µs. The strongest spectral lines are around 1.2–1.4 kHz (50 Hz
  resolution). Inferred, not checked: the WS2815s' PWM.
- **Bench note: the scope hung once.** After many back-to-back 10 M-point,
  four-channel transfers, with status polled several times a second, the
  screen went black with "Remote", then acquisition stuck at "Arm" even in
  Auto. `:SYSTem:REMote OFF` brought the screen back, but only a power cycle
  fixed acquisition. Afterwards: 1 M points, one capture at a time, status
  polled every 0.5 s, and no further trouble.

**3.3 V at DR (`TP206`, C3, ×10, DC, 20 MHz limit, 50 mV/div)**: 2 captures
of 20 ms at 50 MSa/s for normal LEDs and for full white, plus 2 of 200 µs at
500 MSa/s with the bus and LED data:

| | mean | raw p-p | rms | 10 µs-averaged p-p |
|---|---|---|---|---|
| normal LEDs | 3.3003–3.3004 V | 76–79 mV | 1.7–1.8 mV | 3.5–3.7 mV |
| full white | 3.3005–3.3009 V | 74–75 mV | 1.8 mV | 3.3–3.9 mV |
| close-up, 500 MSa/s | 3.3004–3.3007 V | 71–72 mV | 3.7 mV | 0.5–1.1 mV |

- **The rail sits at 3.300 V and full white doesn't move it.** Averaged
  over 10 µs, it varies by 3.5 mV p-p, about 4 ADC counts at 0.8 mV per
  count.
- **The raw p-p is fast spikes at edges:** up to 36 mV within 300 ns of a
  bus or LED-data edge, against ≤ 9 mV (p99.9) at other times. Not checked:
  how much of that is the ×10 probe's ground lead rather than the rail.
- Bearing on the FSRs (inferred): this rail is both the dividers' supply and,
  through R308/C318 (200 Ω, 2.2 µF), `ADC_AVDD`. Slow variation is
  ratiometric and largely cancels; the edge spikes are much shorter than the
  ADC's sampling.

### Still open for this board

- ~~**`S` bus disturbance**~~ fixed 2026-10-02, master firmware (above).
- **Poll rate ~125/s, ~14 Hz per panel at nine** — revisit with all nine panels.
- ~~**Scope the RS-485 bus once the panels are in a pad**~~ **done 2026-10-23
  at DR** (above): wide margin terminated and unterminated. Was: edge settling,
  whether ringing re-crosses ±200 mV, amplitude, idle noise, eye height and
  width, for the baseline and at least case A of the termination table
  (above). Probe points: master `TP7`/`TP8` (GND `TP5`), panel `TP202`/`TP204`
  (GND `TP207`–`TP210`), trigger on master `TP2` (DE). **At DR** (the user,
  2026-10-23: the far end of the chain), with DR lifted.
- ~~**Termination placement check in the master's `H` sweep**~~ **done
  2026-10-18**, the same night. Every sweep now ends with `# termination OK`
  or `# termination WARNING`. Verified by flipping `SW202`s live: R off → "no
  panel terminated"; R + UL → "2 panels terminated: UL R"; UL only → "1 panel
  terminated: UL"; R alone → OK. Limits are in the comment on
  `checkTermination()` in `master.ino`.
- ~~**`u` appeared to do nothing in the pad** (2026-10-22, below). Retest.~~
  Works; it was a viewing problem (retest 2026-10-22, above).
- ~~**Underglow with a real strip**~~ passed 2026-10-05 (above). Only the
  unreproduced first-run glitch remains, and the bit-bang has never been scoped.
