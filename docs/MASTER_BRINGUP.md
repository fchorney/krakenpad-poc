# Master bring-up

Companion to `docs/PANEL_BRINGUP.md`. Written 2026-09-03, before any board was
powered; every pin and net below was read out of `master-pcb.kicad_pcb`.
**Masters #1 and #2 have been through it** (`docs/BRINGUP_LOG.md`); master #2
runs the nine-panel bench pad.

**Firmware: `firmware/master/master.ino`** — unlike the panel, this is not a
separate tool. The master sketch had to be ported to the as-built board anyway,
and every board-local check it needs (DIP, underglow, INT idle states, the GPIO
port layout) works with **zero panels connected**, so the bench commands live in
the real firmware rather than a second sketch.

```sh
firmware/master/build.sh            # compile
firmware/master/build.sh upload     # compile + upload
```

**Since 2026-10-19 the master builds through `build.sh`**, which compiles
against a patched copy of the Teensy core so the PC sees the pad as a joystick
(`docs/USB_PROTOCOL.md` → "Input state report") and, since 2026-10-20, the
vendor interface. **Since 2026-10-20 a plain `arduino-cli compile --fqbn
teensy:avr:teensy40 firmware/master` fails on purpose**: the sketch includes
`firmware/shared/panel_config.h`, which only `build.sh` puts on the include
path (`docs/DEV_NOTES.md` → "Teensy / Arduino").

`upload` finds the port with `arduino-cli board list`, or takes it as the
second argument. Upload by hand needs `-p` with the `teensy` protocol port
(`usb:…`), not the `/dev/cu.usbmodem…` serial port; the number can change
with the USB socket it is plugged into.

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

Type a letter **and press Enter** over USB serial. `?` lists them. The first
five need no panels and no 12V.

> ⚠ **This differs from the panel, and the difference bites coming off a panel
> bring-up.** `firmware/panel/c/bringup/` uses `getchar_timeout_us()` and acts
> on a **single keypress**; the master buffers a line and acts on `\n`/`\r`.
> The master has to work this way — `S <panel> <press> <rel>` takes arguments,
> so a single-key mode would fire `S` before you could type the thresholds.
>
> **Local echo was added 2026-09-08** (with backspace), because `S <panel>
> <press> <rel>` is unusable blind — you cannot spot a typo before Enter, and a
> dumb serial terminal does not echo for you. Your keystrokes now appear as you
> type. A reply can still land between two `[heartbeat]` lines. Keys doing
> "nothing" in `screen` is otherwise screen's copy/scrollback mode — a stray
> trackpad scroll enters it and swallows every keystroke until you press `Esc`.

| cmd | what it does |
|---|---|
| `n` | all nine INT line states, by panel position and connector |
| `N` | INT **external** pull-up (RN1) check — proves the 10k×9 is really there |
| `r` | which GPIO port register and bit each INT pin lands on |
| `D` | read the player-ID DIP |
| `u` | underglow test pattern — **scope only** |
| `U` | underglow steady levels + `R4` check — **the multimeter one** |
| `g` | underglow chase — one group lit at a time, 1 → 44 from the connector, 300 ms each |
| `g <n>` | light underglow group *n* (1–44) alone and hold it |
| `x` | pause/resume LED frames + FSR polling |
| `I` | slot ↔ panel-ID self-test (needs panels) |
| `b` | broken-header test of the panel parser timeout: a lone header claiming 80 bytes, a 2 ms gap, then `'H'` to IDs 0–8. PASS = 9/9 answered, each with `tmo` up by one. Pauses traffic while it runs (added 2026-10-20) |
| `j` | USB pad input: current mask, sequence number, counters; and the vendor interface: host seen, commands, malformed, dropped, stream on and records dropped, passthrough, who drives the LEDs (`docs/USB_PROTOCOL.md`) |
| `t` | toggle the telemetry stream. Lines are `T <id> <S> <W> <N> <E> <mask> <live>`; `live` is 0 once a panel has been silent for `PANEL_TIMEOUT_MS` (added 2026-10-18; `tools/fsr_monitor.py` reads both the old 7-field form and this one) |
| `P` | bus passthrough for `tools/panel_update.py`: USB carries raw frames until the host exits or 30 s idle. Not for typing into; see `docs/FIELD_UPDATE.md` |
| `H` | status sweep: one `'H'` per poll slot to IDs 0–8, one line per panel (build tag, uptime, last reset cause, 12V/termination/auto-INT/echo/INT state, frame and error counters, `tmo` parser timeouts from gameplay firmware since 2026-10-20), `NO REPLY` for a silent one |
| `z` | zero bus counters, on the master and (by broadcast `'Z'`) on every panel |
| `m <mode> [secs]` | panel LED mode by broadcast `'M'`: 0 normal, 1 ID, 2 sensor test |
| `w` | toggle full white (255,255,255) on every LED of every panel, presses ignored: the full-white soak. Panels must be in mode 0 and bus traffic running |
| `c` | panel config: the master's copy per panel and channel, each panel's state (matches / waiting / rejected / not configured / absent), saved slot and sequence, unsaved edits, how long the last save took (added 2026-10-20) |
| `S <panel> <press> <rel> [ch]` | thresholds for one panel, all channels or channel 0–3: edits the master's copy and pushes `'W'`; **not saved** until `save`. Before 2026-10-20 it sent `'C'` |
| `F <panel> <ch> <pop> <en>` | a channel's `populated`/`enabled` flags (0/1): pushed, not saved (added 2026-10-20) |
| `save` | write the config to EEPROM and read it back; prints the slot, sequence and time taken. Stalls INT, USB and the bus while it runs: never during play (added 2026-10-20) |
| `reset config` | every panel to the defaults, saved and pushed. Typed in full (added 2026-10-20) |

## Stage 1 — master alone, USB only

The board has **no regulators and no magnetics**; everything runs off the
Teensy's USB power (`+5VDC_USB` from VIN, plus the Teensy's own 3.3V regulator).
So a bare master needs nothing but a USB cable.

⚠ **The board taps Teensy VIN, not the raw VUSB pad — the on-Teensy VUSB↔VIN
bridge must stay intact.** Cut it and U3 loses its 5V supply and the underglow
goes dark with no other symptom.

**`N` — the external pull-up check, and why `n` is not enough.** Run this
before or after `n`; it is the one that actually tests the board.

`n` reads the INT lines with the Teensy's *internal* pull-ups enabled
(`pinMode(INT_PINS[i], INPUT_PULLUP)`), so it can only catch a line stuck LOW —
a bridge or a shorted `C3`–`C11`. It is **blind to the opposite fault**: with
`RN1` unsoldered every line still reads HIGH, held by the internal pull-up, and
`n` reports a clean pass. That is not a hypothetical on a hand-assembled board.
`RN1` is a THT SIP-10 whose **pin 1 is the common feeding all nine resistors**,
so a single cold joint there removes every external pull-up at once — and the
10k value is deliberate (`docs/MASTER_PCB.md`: "deliberately 10k not stiff", the
INT-into-dead-panel case), so silently running on the Teensy's internal pull-up
instead is a real, invisible deviation.

`N` inverts the bias to settle it. With the internal pull-**down** selected
(~100k on i.MX RT), `RN1`'s 10k to +3.3VDC still wins the divider —
3.3 × 100/110 ≈ 3.0 V, a solid HIGH — while a line with no external pull-up is
dragged to 0 V. `RN1` lands on the Teensy-side node (same net as `C3`–`C11`), so
the settling RC is 100k × 1nF = 100 µs worst case; the code waits 1 ms.

| result | meaning |
|---|---|
| all nine `OK` | `RN1` present and pulling on every line, all nine traces continuous |
| any line `NO external pull-up` | that line only: `RN1` joint, or an open trace to the Teensy pin |
| all nine missing | `RN1` pin 1 (the common) is open — **or the Teensy is not seated** |

⚠ **Nothing may be plugged into `J3`–`J11` while `N` runs.** A panel holding its
open-drain INT low is indistinguishable from a missing pull-up. The command
detaches the INT interrupts, flips the pin modes, and flushes the edge ring
before re-attaching them — without that flush the mode changes surface as nine
phantom presses.

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

**`U` — the meter-readable underglow test, and why `u` is not one.**

⚠ **A DMM cannot see `u` at all, and the reading looks like a dead pin.** `u`
sends five real WS2811 frames — 44 groups × 24 bits × 1.25 µs ≈ 1.3 ms of burst
each — separated by 600–1200 ms with the line parked LOW, and it ends blanked.
Average duty across the run is **~0.1%**, so a handheld meter reports **~5 mV**
whether U3 works or not. That is a correct pin seen through a slow instrument,
exactly the trap the panel's `i` hit (which is why `i` grew 8-second dwells).
**`u` is for a scope. Use `U` with a meter.**

`U` does two things a meter can read:

- **`R4` present?** Pin to `INPUT_PULLUP` and read. `R4` (10k to GND) against the
  Teensy's internal ~22k divides to ~1.0 V = **LOW**; with `R4` missing the
  internal pull-up wins and it reads **HIGH**. Worth its own check because `R4`
  holds `U3`'s input LOW *before firmware drives the pin* — a missing one puts
  garbage on the strip at every boot and has **no other symptom**.
- **Steady 6 s dwells**, HIGH then LOW, each announced first.

| point | HIGH | LOW |
|---|---|---|
| `TP9` — 3.3V side, `U3` pin A | ~3.3 V | ~0 V |
| `TP10` — 5V side, `U3` pin Y | ~5 V | ~0 V |

**`TP10` following `TP9` is the whole test** — that is `U3` translating
3.3 V → 5 V, the one job it has. `TP10` stuck at 0 V while `TP9` switches means
`U3` is dead, unpowered (check `+5VDC_USB` at `TP4`), its OE̅ (pin 1) is not
grounded — **or its pins are bridged, which is what happened on master #1.**
`J2` pin 1 sits behind `R5` (330R) and reads the same as `TP10` with nothing
plugged in.

`U` ends with a blanking frame: a steady HIGH is not a valid WS2811 reset (a long
LOW is), so a connected strip could latch garbage — the LOW dwell resets it and
the zeros frame clears it.

**A pass clears the entire master-side chain — `U3`, `R4`, `R5`, `J2` — with no
strip attached.** Only the cable and the LEDs then need the pad, which makes them
an install-day check rather than a bring-up item.

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

> ~~Minor documentation trap~~ **FIXED 2026-09-08, no physical consequence.**
> The `Description` fields on R1/R2 used to call the bias pair "R4+R5" and say
> they were "PARKED UNWIRED" — naming that predated the 2026-07-31 master
> renumber. The bias pair is **R1/R2**, they *are* wired in the netlist, and
> **R4/R5 are the underglow pull-down and series resistor, both of which must be
> populated.** The same "PARKED UNWIRED" text sat on all ten test points, which
> are likewise wired. Descriptions in `.kicad_sch`/`.kicad_pcb` were corrected
> and `master-pcb-BOM.csv` regenerated; the edit touched description strings
> only, no geometry and no nets.

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

## Termination check (end of every `H` sweep)

After an `H` sweep the master compares each panel's `SW202` position against
the documented chain order (0→3→6→7→4→1→2→5→8). Exactly one panel should be
terminated: the last one in that order that answered. `R3` terminates the
master end. It prints `# termination OK: …` or `# termination WARNING: …`,
naming what is on and what was expected.

It is a warning, never an error: on the bench the bus ran with zero errors
unterminated, terminated mid-chain, at the wrong end, and doubled to 40 Ω
(`docs/BRINGUP_LOG.md`, master #2, 2026-10-18). What it cannot see:

- the `term` bit is `SW202` pole B only, so a missing `R201` or a bad pole A
  still reads ON;
- a panel that is cabled in but silent still passes A/B through, so the real
  end of the cable can lie beyond the last panel that answered;
- the chain order is documented, not measured. A pad cabled in another order
  gets the wrong expected panel.

## Config and the USB vendor interface (built 2026-10-20, not yet run)

Both were written and compiled without the pad. Nothing below has been run.
Results go in `docs/BRINGUP_LOG.md` under the master that ran them.

**Order: master first, then panels**, as for any update (`docs/USB_PROTOCOL.md`
→ "firmware updates"). The new master copes with old panels (they never answer
`'W'` and sit at "sent, waiting"). New panels on the old master would stay on
defaults and the old master's `S` would do nothing.

1. **Master alone.** `build.sh upload`. The banner shows `Master firmware
   0.1.0 (build …)` and, on this first boot, `config: NO valid saved config`.
   `tools/hid_monitor.py` still sees presses (the joystick is interface 3 now).
   `python3 tools/pad_cmd.py info` answers: protocol 1, packet 64, config
   version 1, the DIP's player ID, flag "config was reset". Then run
   `build.sh upload` once more: it shows the upload's automatic reboot still
   finds the Teensy with the new interface set.
2. **Old panels.** `c`: every answering panel "sent, waiting"; the heartbeat
   says `0/9 answering panels match`.
3. **New panel firmware** to all nine with `tools/panel_update.py`. When they
   are back: one `config matches` line per panel, `c` shows all nine
   "matches", the heartbeat `9/9`. A panel's own
   console `s` shows "from the master" and the CRC `FD704831` for defaults.
4. **`S`.** `S 3 600 450`, then `c`: panel 3 matches at the new values, "UNSAVED
   edits". Optionally feel the extra force on panel 3; panel 3's `s` shows them.
   `S 3 600 450 2` changes channel 2 only.
5. **`F`.** `F 3 0 1 0`: pressing panel 3's channel 0 sensor no longer presses
   (no INT, no USB button), while `t` or `pad_cmd.py stream` still shows its raw
   value. `F 3 0 1 1` restores it.
6. **`save` and reboot.** `save` prints slot A, sequence 1, and the time it
   took; **record that time** (`docs/PANEL_CONFIG.md` wants the stall
   measured). Replug the master: the banner says `loaded from slot A,
   sequence 1`, `c` shows the edits. A second `save` goes to slot B.
7. **Panel reboot.** Cut one panel's 12 V for a few seconds: the console shows
   it stop replying, reply again, and match again within about a second.
8. **`reset config`**: all nine back to 500/400, saved, matching.
9. **Vendor interface**, each with `tools/pad_cmd.py`:
   - `config`, `status`: values agree with `c` and `H`; within ~5 s of boot
     the status shows each panel's build tag and termination switch.
   - `set 3 550 420`: "saved", then config result events for panel 3 (waiting →
     matches); `c` agrees, with no "UNSAVED".
   - `stream 30`: live values for all nine and a per-panel rate. **Record the
     rate**: expected ~12.7 Hz with the re-push, not measured.
   - `mode 1 3` (all nine show their ID for 3 s), `mode 2 5 --panels 4`
     (only C runs the sensor test).
   - `selftest`: every slot names its own panel, as the console's `I`.
   - `zero`, then `H`: counters near zero.
   - `clock`: **record** the round trip (min, median, max). No figure is
     expected yet; an LED burst in the way can add up to ~7 ms.
   - `leds 0 0 64 3`: all panels dim blue for 3 s, then the rainbow returns
     about 1 s after the last frame.
   - `bus-h 4`: one `'h'` from panel 4 through USB passthrough; the console
     prints passthrough ON then OFF, and normal traffic resumes.
   - `events 60`, while unplugging a panel's bus cable and plugging it back:
     presence and config result events.
   - Optional: `bootloader --yes` puts the Teensy in HalfKay; `build.sh upload`
     brings it back.
10. **Regression:** `I`, `b`, `H`, `P` with `tools/panel_update.py`, the
    full-white `w`, and `hid_monitor.py` presses all behave as before.

On Windows, remove the cached `16C0:0487` device first: its interface set
changed again (`docs/USB_PROTOCOL.md` → device identification).

## Still open on the master

- **Check whether the master has the panel's RS-485 turnaround glitch.** The
  panel measured **exactly one framing error per transmission** (2522 against
  2521 replies over 90,756 frames): while it transmits, DE tied to `R̅E̅` disables
  the THVD1450's receiver, `RO` goes high-impedance, its RX pin floats, and the
  return transition reads as a false start bit. **The master's Serial2 RX floats
  for the identical reason during its own transmissions.** It has never been
  measured, because nothing on the master counts framing errors — `0 crc errs`
  only says no *frame* was corrupted, and on the panel this glitch corrupted
  nothing either (it lands in the idle gap after the reply). So the master's
  clean CRC count is **not** evidence the glitch is absent.
  - To measure: read `LPUART4->STAT` for `FE`/`PE`/`OR` in the RX path and count
    them, the way the panel now separates `uart errors` from `turnaround`.
  - The panel's fix — an internal pull-up holding the line at idle-mark — is
    **not** directly portable: `pinMode()` cannot add a pull-up without taking
    the pin off the UART, so it needs a pad-control register write instead.
  - ⚠ **This is not a board-revision candidate.** A 10 kΩ pull-up on `RO` would
    fix it in copper, but the glitch is benign and firmware handles it for free
    on the side that can. Measure before deciding it is worth anything at all.
  - Full investigation: `docs/PANEL_BRINGUP.md` → stage 3.

