# Panel bring-up

How to prove a freshly assembled `dual-panel` board works, and in what order.

Written 2026-09-03, while the LCSC parts parcel was still in transit — so nothing
here has been run against real hardware yet. Every pin, net and part number was
re-derived from `dual-panel.kicad_pcb` and `docs/DUAL_PANEL.md`.

**Firmware: `firmware/panel/c/bringup/`.** It is deliberately *not* the gameplay
firmware. `firmware/panel/c/main.c` is breadboard-pinned (LED on GPIO4, INT on
GPIO5, DE on GPIO2) and needs a port to the as-built map regardless; but the port
should not be the first thing to run on a real board, because bring-up happens in
a world the gameplay firmware does not model — a **standalone brain with no
carrier**: no DIP switch, no termination switch, no FSR connectors, no LEDs, no
12V. The bring-up build is single-core, USB-CDC interactive, and free to be as
chatty as a bench tool should be.

Build (note the toolchain — the Homebrew `arm-none-eabi-gcc` on `PATH` has no
newlib and dies on `nosys.specs`):

```sh
export PICO_SDK_PATH=~/pico-sdk
PATH=/Applications/ArmGNUToolchain/15.2.rel1/arm-none-eabi/bin:$PATH \
  cmake -S firmware/panel/c/bringup -B firmware/panel/c/bringup/build -DPICO_BOARD=pico
PATH=/Applications/ArmGNUToolchain/15.2.rel1/arm-none-eabi/bin:$PATH \
  cmake --build firmware/panel/c/bringup/build -j8
picotool load -x firmware/panel/c/bringup/build/panel_bringup.uf2
```

`picotool load`, never the `RPI-RP2` drag-drop on macOS. `picotool info`
segfaults on RP2040.

## The stages

| stage | what is connected | what it can prove |
|---|---|---|
| **0** | brain, 12V, blank flash | `TP306` rest state — meter only, no firmware |
| **1** | brain alone, USB | flash, rails, `SENSE_12V` reads absent, pull configuration |
| **2** | + carrier + 12V, USB kept | power mux, DIP, termination, FSR mapping, INT, LEDs |
| **3** | 2 panels + master | RS-485 — needs peers, cannot be self-tested |

The staging is not arbitrary: each stage is the largest set of checks that the
*previous* stage's hardware makes trustworthy.

---

## Stage 0 — blank board, meter only

Before any firmware. **Measure `TP306` (`RS485_DE`) to GND on a blank, powered
board — expect ~0 V.**

This is the one open item from `docs/DUAL_PANEL.md`. The net has no external pull
(only GPIO4, `U308` pins 2/3 and `TP306`), so between power-on and firmware
configuring the pin — and for the entire time a board sits in BOOTSEL — DE/R̅E̅
is held only by the RP2040's internal pull-down, which is weak (~50–80 kΩ) and
not established until POR completes. It should park the THVD1450 in receive.

If it floats high: a 10 k to GND tacked at `TP306` fixes the boards in hand, and
an external pull-down becomes a rev-2 candidate (purely additive, one part).

**Do this on the first board only** — it is a design question, not a per-board
test.

---

## Stage 1 — brain alone, USB power

The brain is fully standalone: MCU, flash, crystal, both regulators, the
power-OR, USB and the transceiver are all 3xx parts. Brains can be flashed and
largely validated before ever meeting a carrier.

A blank board needs no BOOTSEL press: the bootrom checksums the first 256 bytes
of QSPI flash, blank flash fails that check, and the chip drops into the USB
bootloader by itself, enumerating as `RPI-RP2`. `SW301` only matters once a valid
image is on the flash.

### If it does not enumerate

USB needs the PLL and the PLL needs the crystal, so the suspect list is short —
`X301`/`C311`/`C313`/`R302` first, then rails at `TP302`, then `RUN` (`R307` is
the 10 k pull-up; `TP303` both probes `RUN` and resets the chip when momentarily
shorted to GND), then `USB_D±` (`R305`/`R306`, `U305`, and the QFN-56 pads).
Full detail in `docs/DUAL_PANEL.md` → "First bring-up".

### What the firmware checks here

The banner prints automatically. Then run `w` and `p`.

**Flash (`U307`) — the banner's JEDEC read plus the `w` command.**
⚠ **BOOTSEL enumerating proves nothing about the flash.** The bootrom enters
BOOTSEL *because* it could not read a valid image, so a dead, mis-oriented or
unsoldered `U307` is indistinguishable from a blank one. Three things actually
validate it, in increasing strength:

1. JEDEC ID reads `EF 40 16` (Winbond W25Q32JV).
2. `picotool load` write-verifies and the program runs.
3. `w` erases, programs and verifies the top 4 KB sector — **and then checks
   whether the same pattern appears 2 MB lower.** A 2 MB die substituted for the
   4 MB part wraps addresses, so a plain write-and-read-back test passes on it;
   the alias check is the only thing that catches the substitution. This matters
   because the animation slots (`docs/ANIMATIONS.md`) live in the upper flash
   that a 2 MB part does not have.

> The bring-up build sets `PICO_FLASH_SIZE_BYTES=4194304`. The SDK's default
> board header says 2 MB, which would place the test sector *inside* the aliasing
> window and make the capacity check pass unconditionally. **The gameplay
> firmware needs a proper custom board header for the same reason** — currently
> an open item.

**`SENSE_12V` must read LOW.** This is the negative half of the 12V-sense test
and it is not optional: reading HIGH at stage 2 does not prove the divider works,
because a solder bridge to +3.3V also reads HIGH. HIGH here, with no 12V applied,
means a bridge or a wrong-value `R313` — stop and check.

**`p` — the pull-configuration probe.** The sharp edge of this whole document:

> **The RP2040's reset default for GPIO17 is the wrong one, and it fails toward
> "12V absent".** Pads come out of reset as input with the pull-**down** enabled.
> `R313`/`R314` (100 k/33 k) make a 2.977 V source behind 24.8 kΩ, so an internal
> pull-down in parallel drags the pin down:

| internal pull-down | pin voltage with 12V present | vs VIH = 2.145 V |
|---|---|---|
| disabled | 2.98 V | pass, wide margin |
| 80 kΩ (weak end of spec) | 2.27 V | pass by ~120 mV |
| 50 kΩ (strong end of spec) | 1.99 V | **fails — 12V present reads as absent** |

`gpio_disable_pulls(17)` is therefore mandatory, and the bring-up firmware does
it before anything else. The `p` command reports the pin state under all three
configurations so the margin on *this* silicon becomes a recorded fact rather
than the arithmetic above. Run it at stage 1 (expect `0 0 1`) and again at stage
2 with 12V live — that second reading is the one worth writing down.

Whatever `p` reports, **do not relax the rule**. A board that happens to read
high through its pull-down has a weak pull-down, not margin.

---

## Stage 2 — carrier mated, 12V applied, USB still connected

Keeping USB connected while 12V comes up is the point of this stage, not a
convenience: it is the only test of the **`U304` LM66200 power mux** under the
condition it was chosen for.

### Power mux

`U304` selects the higher of VBUS and the `U303` AMS1117-5.0 output; `U302`
(AP7361C-33) makes 3.3V from whichever wins. Which source wins with both present
is genuinely ambiguous — VBUS is 5.0–5.25 V and the AMS1117 lands near 5.0 V —
and it does not functionally matter. What matters is that **no transition
between them disturbs the board.**

The test is a hot-swap, and the firmware is instrumented for it: it prints a
banner at boot and blinks the debug LED at 1 Hz, so **a reset is visible with or
without a terminal attached.**

1. Both connected. Note `TP302` (+5VDC) and the 3.3V rail.
2. Unplug USB. The board must keep running on 12V — with the terminal gone you
   are watching the debug LED, which must keep its 1 Hz beat without stutter.
3. Reconnect USB. **The banner must not reappear.** If it does, the board reset
   through the changeover.
4. Remove 12V, leaving USB. Board keeps running; `SENSE_12V` logs an edge.
5. Reapply 12V. Another edge, no reset.

`C308` (22 µF tantalum) sits on the AMS1117 *input* side of the mux, so USB never
charges it — no inrush concern on the host, and no reason for a VBUS-side
brownout at plug-in. The LM66200 blocks reverse conduction, so nothing back-feeds
the host either.

### `SENSE_12V` — the actual test

Every 12V connect/disconnect above prints a timestamped edge. That transition,
observed in both directions, is what proves the divider — not a single HIGH
reading.

The firmware samples at 1 kHz behind an **asymmetric filter: ~30 ms of stable
HIGH to declare the rail present, 2 ms of LOW to declare it gone.** Slow to
trust, fast to give up. The pin's documented job is gating whether it is safe to
drive the WS2815s, so a sagging rail must stop LED output rather than flap. The
one-liner in `docs/DUAL_PANEL.md` (`if (!gpio_get(17)) skip_led_output();`) has
the right intent, but a latched state machine is what the job actually wants.

### DIP switch — `d`

Toggle one position at a time and watch the decoded value. Two things are easy
to get backwards and this is the one place to settle them physically:

- **Bit order is reversed.** `SW201` pins 1–4 drive `DIP_ID0`–`DIP_ID3`, but
  **GPIO18 = bit 3 … GPIO21 = bit 0**.
- **A closed (ON) switch reads 0.** Switches close to GND against internal
  pull-ups. So **panel ID 0 means all four switches ON**, and a factory-fresh
  all-OFF switch reads **15** — a reserved code, not panel 0.

An unmated brain also reads 15 (pull-ups, nothing pulling down), which the status
line uses as a "looks unmated" hint.

### Termination — `s`

`TERM_SENSE` **LOW = terminated**. `SW202` is DPDT: pole A puts `R201` (120 Ω)
across the pair, pole B reports the state, so firmware can never disagree with
the copper. Confirm both halves — the reported state with `s`, and 120 Ω across
`RS485+`/`RS485−` with a meter — because the whole value of pole B is that it
tracks pole A.

There are **no board pull-ups** on `TERM_SENSE` or the DIP lines; the internal
pull-ups are load-bearing.

### FSR channels — `f`

Press each edge in turn and check that the name that moves is the edge you
pressed. This catches a carrier-side connector mix-up that would otherwise
surface much later as a baffling gameplay bug.

| ADC | GPIO | signal | carrier connector |
|---|---|---|---|
| 0 | 26 | `FSR_South` | J202 |
| 1 | 27 | `FSR_West`  | J201 |
| 2 | 28 | `FSR_North` | J206 |
| 3 | 29 | `FSR_East`  | J203 |

Also worth doing here: **press one channel hard and watch its neighbours.** The
breadboard firmware carried `ADC_DUMMY_READ`, discarding the first conversion
after each mux switch to absorb ~15 counts of sample-cap charge injection at the
cost of halving the sample rate. Its own comment says to set it to 0 once the ADC
nodes have ~10 nF caps to GND — which the real board has. The bring-up firmware
does no dummy read; if neighbours stay put under a hard press, that setting is
confirmed rather than assumed, and the gameplay firmware keeps the full rate.

Unplugged and unpressed are electrically identical here (~0) and that is by
design — see `docs/PANEL_CONFIG.md` → "Why presence must be config, not
measurement".

### INT — `i`

`INT_OUT` (GPIO22) is **emulated open-drain**; the RP2040 has no true open-drain
mode. Assert = drive LOW. Release = switch the pin to **input (hi-Z)**, never
drive it HIGH. Driving push-pull would fight the master's 10 k pull-up and break
the documented safe-failure behaviour (a disconnected wire reads HIGH = not
pressed).

Standalone, meter GPIO22: ~0 V asserted, **floating** when released. With the
master present, the pulse must land on the JST XH header silkscreened for this
panel's position — but that is really the `'I'` identify command's job at stage
3, and it does not assume slot ↔ panel-ID agreement.

### LEDs — `l`

**Gated on `SENSE_12V`, and the firmware refuses without it.** `U301` runs from
+5VDC, which is live from VBUS alone, so on USB-only power it will drive
`LED_DATA_5V` into WS2815 `DIN` pins whose 12V rail is dead, forward-biasing
their input protection. `R301` (330 Ω) keeps that non-destructive but it is not a
valid test either.

Red, green, blue, then white at low brightness. Count 25. Remember the chain runs
serpentine and **the row of 3 is intentionally 180° from the row of 4** — a dark
run localises the break. Brightness is deliberately low; on WS2815 the dies sit
in series so **red draws exactly as much as white** and only PWM duty reduces
power.

---

## Stage 3 — RS-485, two panels and the master

**RS-485 cannot be self-tested on one board.** `DE` and `R̅E̅` are tied, so
transmitting disables the local receiver — there is no loopback through `U308`.
A GPIO-level UART loopback proves the pins and nothing about the transceiver.

Two things to plan for:

- **`RS485_DE` on GPIO4 is not a UART0 RTS pin**, and no pin choice would have
  helped: the RP2040's PL011 has no RS-485 auto-direction mode on any pin. DE
  must be driven in software, timed off the TX path draining.
- **The classic bug:** the PL011's TX FIFO going empty does **not** mean the
  shift register is empty. Release DE on FIFO-empty and the last byte is
  truncated; release it late and you collide with the next talker. Wait on the
  UART `BUSY` bit, plus roughly a character time of slack. The breadboard
  prototype ran 1 Mbps with manual DE and 0 CRC errors, so the approach is
  proven — it just has to be re-derived on the real pin.

Termination is set at the two ends of the bus only (master and the last panel),
via `SW202`.

---

## Per-board record

The banner prints the RP2040's unique board ID, read out of `U307`. Use it to
name each board's log — with a batch of ~20 boards in hand, "which board was
the one with the odd FSR channel" is a question that gets asked later.

Minimum to record per board: JEDEC ID, `w` result (both lines), `p` at stage 1
and at stage 2, the mux hot-swap result, DIP sweep, FSR channel mapping, LED
count.

## Defaults this firmware carries

Inherited from the breadboard firmware and re-stated here so stage 2 is the place
they get confirmed on real hardware, not assumed:

| default | value | status |
|---|---|---|
| FSR press / release threshold | 500 / 400 counts | bench-validated on the prototype |
| FSR float threshold | 187 | flags a resting channel that is too high |
| FSR persistence filter | 10 consecutive samples | not exercised by bring-up |
| `ADC_DUMMY_READ` | 0 | now correct — the board has the 10 nF caps. Confirm with `f` |
| LED fallback timeout | 100 ms | stage 3 |
| RS-485 baud | 1 Mbps | stage 3 |
| Debug LED | 1 Hz heartbeat | diagnostic-mode-only in gameplay firmware |
