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

## Summary

| board ID | assembled | stage 0 | stage 1 | stage 2 | stage 3 | notes |
|---|---|---|---|---|---|---|
| `DE6558A69754442F` | 2026-09-07 | ✅ | ✅ | ✅ | ✅ | first board powered; found the `INT_OUT` pull-down bug and the VBUS back-drive. Stage 3 2026-10-02 at ID 3 with master #2: `F`, `I`, `S` clean, `A` + `E` closed loop by eye |
| `DE6558A6977D462D` | 2026-10-02 | ✅ | ✅ | ✅ | ✅ | second board; clean first time. ID 1 on 10-01, **ID 0 from 10-02**. `i` skipped, INT proven on the bus with master #2 instead |

### Masters

| board | assembled | stage 1 | stage 2 | stage 3 | Teensy fitted | notes |
|---|---|---|---|---|---|---|
| **master #1** | 2026-09-08 | ✅ | ✅ INT | — | `20432520` (ex-prototype) | SMD hand-soldered with Sn42Bi57Ag1; INT path proven end to end with one panel; `r` closed the single-GPIO-port open item |
| **master #2** | 2026-10-02 | ✅ | ✅ INT | ✅ | `19755070` (new) | all-leaded build: Chip Quik Sn63/Pb37 paste through the printed stencils, no added flux. `S` bus fault found and fixed in master firmware 2026-10-02; first two-panel bus, see below |

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

### Still open for this board

- The three FSR connectors not yet exercised with a sensor (`J201` W, `J202` S,
  `J206` N).

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

### Still open for this board

- ~~**`S` bus disturbance**~~ fixed 2026-10-02, master firmware (above).
- **Poll rate ~125/s, ~14 Hz per panel at nine** — revisit with all nine panels.
- ~~**Underglow with a real strip**~~ passed 2026-10-05 (above). Only the
  unreproduced first-run glitch remains, and the bit-bang has never been scoped.
