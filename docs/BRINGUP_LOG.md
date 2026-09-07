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
| `DE6558A69754442F` | 2026-09-07 | ✅ | ✅ | — | — | first board powered; found the `INT_OUT` pull-down bug |

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
