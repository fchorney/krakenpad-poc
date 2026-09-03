# Master bring-up

Companion to `docs/PANEL_BRINGUP.md`. Written 2026-09-03, before any board was
powered; every pin and net below was read out of `master-pcb.kicad_pcb`.

**Firmware: `firmware/master/master.ino`** — unlike the panel, this is not a
separate tool. The master sketch had to be ported to the as-built board anyway,
and every board-local check it needs (DIP, underglow, INT idle states, the GPIO
port layout) works with **zero panels connected**, so the bench commands live in
the real firmware rather than a second sketch.

```sh
arduino-cli compile --fqbn teensy:avr:teensy40 firmware/master
arduino-cli upload  --fqbn teensy:avr:teensy40 firmware/master
```

## The port — what changed, and the trap in it

The breadboard prototype drove 2 panels. The as-built board drives 9, on a
different UART, with two subsystems the prototype did not have at all.

| | prototype | as built |
|---|---|---|
| panels | 2 | 9 |
| RS-485 | `Serial1`, DE on pin 2 | **`Serial2`** (RX 7 / TX 8), **DE on 6** |
| INT lines | pins 3, 4 | **pins 23,22,21,20,19,18,17,16,15** = panels 0–8 |
| player-ID DIP | — | **pins 3, 4, 5** |
| underglow | — | **pin 11** → U3 → R5 330R → J2.1 |

> ⚠ **The old map is not a subset of the new one.** Pins **3 and 4 carried the
> two INT lines on the breadboard and carry the player-ID DIP on the board.** A
> partial port — fixing the UART but leaving the INT pins — would have read the
> DIP switch as panel presses and looked like a wiring fault. This is the reason
> the port was done wholesale rather than incrementally.

Panel order is by ID: panel 0 (`UL`) is on pin 23 and descends to panel 8 (`DR`)
on pin 15 — **the pin numbers run backwards against the panel numbers.** The
firmware prints the silkscreen position (`UL`/`U`/`UR`/…) rather than a pin
number wherever it can, because that string is also what is printed on the cable
label, so nothing at the bench needs a lookup table.

## Bench commands

Type a letter and Enter over USB serial. `?` lists them. The first five need no
panels and no 12V.

| cmd | what it does |
|---|---|
| `n` | all nine INT line states, by panel position and connector |
| `r` | which GPIO port register and bit each INT pin lands on |
| `D` | read the player-ID DIP |
| `u` | underglow test pattern |
| `x` | pause/resume LED frames + FSR polling |
| `I` | slot ↔ panel-ID self-test (needs panels) |
| `t` | toggle the telemetry stream |
| `S <panel> <press> <rel>` | set FSR thresholds on one panel |

## Stage 1 — master alone, USB only

The board has **no regulators and no magnetics**; everything runs off the
Teensy's USB power (`+5VDC_USB` from VIN, plus the Teensy's own 3.3V regulator).
So a bare master needs nothing but a USB cable.

⚠ **The board taps Teensy VIN, not the raw VUSB pad — the on-Teensy VUSB↔VIN
bridge must stay intact.** Cut it and U3 loses its 5V supply and the underglow
goes dark with no other symptom.

**`n` — INT idle states.** All nine must read HIGH. `RN1` (10k ×9 bussed) pulls
every line to +3.3VDC, so a LOW line with nothing plugged in is a board fault:
a solder bridge, or a shorted `C3`–`C11`. Each line's protection chain is
TVS (`D1`–`D9`, SMAJ5.0A) → 330R (`R6`–`R14`) → 1nF C0G (`C3`–`C11`) to GND at
the Teensy-side node, forming a ~330 ns low-pass.

**`r` — the single-port question.** `docs/MASTER_PCB.md` flags this as "worth
confirming at bring-up, not verified here": all nine INT pins are `AD_B1_xx`
pads and *should* sit on one i.MX RT GPIO port, which would let a future fast
path sample every panel in a single register read. `r` reports the register
address and bit index for each pin, straight out of the core's own tables, and
says whether they match. **Expect them to match and the bit positions to be
scattered** — one read, but a table rather than a shift.

**`D` — the player-ID DIP**, and the same trap the panel has:

> `SW1` closes to GND against internal pull-ups (pads 4/5/6 are all GND), so a
> **closed switch reads 0**, and the bit order runs backwards against pin
> number — **pin 3 is bit 2, pin 5 is bit 0**. So **player 0 (P1) means all
> three switches ON**, and a factory-fresh all-OFF switch reads **7**, a
> reserved code. Netlist-verified; the panel's ID DIP behaves identically.

**`u` — underglow.** Drives WS2811 data out pin 11 → `U3` (SN74AHCT1G125 at 5V)
→ `R5` 330R → `J2` pin 1. Two things to know:

- **`R4` (10k pull-down) holds U3's input LOW before firmware drives the pin**,
  so the strip sees no garbage during boot. Worth confirming with a scope on a
  first power-up.
- **The strip's 12V comes from the Wago fan-out, not from this board.** With the
  fan-out unpowered, `u` proves only that U3 switches — probe U3's Y pin.
  ⚠ And the connector pinout is **pin 1 = GND, pin 2 = DATA, pin 3 = 12V**, the
  reverse of what was originally assumed; building to the old order puts 12V
  into DATA and destroys the first LED.
- Underglow is **WS2811 — three independent constant-current sinks**, so colour
  scales current linearly. This is the *opposite* of the panels' WS2815, where
  red draws exactly as much as white. Do not carry a power intuition across.

The bit-bang is deliberately approximate (T1H ~600 ns / T0H ~250 ns against a
±150 ns tolerance) — a presence check for U3 and the cable, not a driver. Scope
it once, then trust it.

## Stage 2 — master + one panel

`x` first, to stop the master flooding the bus, then bring the panel up on its
own terms (`docs/PANEL_BRINGUP.md`), then resume.

⚠ **RS-485 cannot be self-tested on the master either.** `U1` pads 2 and 3 are
both on `RS485_DE` — R̅E̅ is tied to DE, exactly as on the panel — so
transmitting disables the local receiver and there is no loopback. Two nodes
minimum.

Termination is `R3` (120R, wired across `RS485+`/`RS485−`) at the master end and
`SW202` at the last panel. `R1`/`R2` (390R failsafe bias) are **DNP and were not
ordered** — the THVD1450's integrated open/short/idle failsafe makes them
unnecessary; the footprints exist so bias can be hand-added at the one correct
bus point if the bench ever disagrees.

> Minor documentation trap, no physical consequence: a text note on `F.Fab`
> (a documentation layer, not fabricated) still calls the bias pair "R4+R5" and
> says they are "PARKED UNWIRED". That naming predates the 2026-07-31 master
> renumber — the bias pair is **R1/R2**, they *are* wired in the netlist, and
> R4/R5 are now the underglow pull-down and series resistor. Read the BOM, not
> the note.

**Unlike the panel, the master has no manual-DE hazard.**
`Serial2.transmitterEnable(6)` makes the hardware assert and release DE around
each transmission. The truncated-last-byte trap is panel-side only, where the
RP2040's PL011 has no auto-direction mode on any pin.

## Stage 3 — the slot ↔ panel-ID self-test (`I`)

Two independent things claim to say which panel is which: the **physical slot**
(whatever is plugged into `J3`–`J11`) and the panel's **self-reported DIP ID**.
Nothing guarantees they agree, and because INT is the sole gameplay press path,
a disagreement means presses register as the wrong arrow — a silent bug that
feels like "the pad is broken".

`I` implements the master half of the `'I'` command from
`docs/RS485_PROTOCOL.md`: idle pre-check on all nine lines, then per ID, send
`'I'`, wait for the `'i'` ack, and watch **all nine** inputs for a pulse.

| result | meaning |
|---|---|
| ack, one slot, matching | correct |
| ack, one slot, wrong one | cable in the wrong header — the label names which |
| ack, two slots | two panels share a DIP ID |
| ack, no edge | INT wire not landed, or the panel's open-drain GPIO is dead |
| edge, no ack | panel lives on INT but not RS-485: transceiver, termination, or A/B swapped |
| neither | absent, unpowered, or dead |
| pre-check finds a LOW line | stuck FSR, shorted wire, or someone is on the pad |

It is also an end-to-end continuity test of the input path — the pulse uses the
same open-drain driver a real press uses, so a pass proves panel GPIO → wire →
master pull-up and RC filter → Teensy pin.

⚠ **The panel half of `'I'` is not implemented yet.** Until the panel firmware
is ported, `I` will report "no ack" for every ID. That is a correct result, not
a bug in the test.

## Still open on the master

- **USB HID to the PC is not started.** The master talks to the bench over USB
  serial and to panels over RS-485; the actual gameplay path to the host does
  not exist yet. `docs/USB_PROTOCOL.md` has the design.
- **The panel firmware is still breadboard-pinned.** `firmware/panel/c/main.c`
  needs the same wholesale port this sketch just got — and it has the same
  shape of trap, since the prototype's LED/INT/DE pins are all reused for other
  things on the real board.
