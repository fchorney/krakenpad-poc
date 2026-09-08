# Repeat-build checklist

`PANEL_BRINGUP.md` and `MASTER_BRINGUP.md` are written to **prove a design** —
the right shape for board 1, the wrong shape for boards 3 through 20. This is the
condensed version: what to do per board, and what not to do again.

**Remaining as of 2026-09-08:** 1 panel built of **20** (2 pads × 9 + 2 spares),
so **19 to go**. 1 master built of **2**, so **1 to go**.

---

## ⛔ First-board-only — do NOT repeat

These were one-time questions about the *design*, and they are closed. Repeating
them costs bench time and risks "re-opening" settled decisions.

- **Stage 0 meter sweep** (`TP306` 35 mV, `TP302`, `TP301`, `RUN`). `TP306` in
  particular answered whether the RP2040's internal pull-down parks the THVD1450
  in receive. It does. Closed, and it dropped a rev-2 candidate.
- **The `p` pull-strength probe.** Answered; the rule stands.
- **`ADC_DUMMY_READ = 0`.** Confirmed on hardware; keeps the full sample rate.
- **Master `r`** (nine INT pins on one GPIO port, register `0x42000008`).
  A property of the chip, not of your soldering.

---

## Per panel (×19)

### 1. Hand-solder — 20 connectors, all through-hole

JLC placed every SMD part including the 25 WS2815s. Nothing here needs hot air.

**Carrier (15):** `J201` `J202` `J203` `J206` JST-PH ×4 · `J204` `J207`
Micro-Fit 3 ×2 · `J205` `J208` Micro-Fit 2 ×2 · `J209` SWD 1×3 · `J210`–`J213`
1×8 pin headers ×4 · `J214` screw terminal · `SW201` ID DIP · `SW202` termination

**Brain (5):** `J301`–`J304` 1×8 sockets ×4 · `J305` USB-C

⚠ The 30 test points are bare probe holes — nothing to fit.
⚠ `J305`'s contact holes sit at 0.45 mm edge-to-edge, zero margin. Don't bridge.

### 2. Flash

```
picotool load -f -x firmware/panel/c/bringup/build/panel_bringup.uf2
```

Blank boards enter BOOTSEL on their own. Already-flashed boards reflash over USB
with `-f`, or `B` from the firmware — **the button is never needed after the
first time.**

⚠ Panel builds need the toolchain override until the Homebrew formula is removed:
`-DPICO_TOOLCHAIN_PATH=/Applications/ArmGNUToolchain/15.2.rel1/arm-none-eabi`

### 3. Bench — brain alone on USB

| | pass condition |
|---|---|
| `v` | banner prints; **record the RP2040 unique ID — this is the board's name** |
| `w` | JEDEC `EF 40 16`, write/verify PASS **and** the 4 MB alias check PASS |
| `s` | sysclk 125 MHz, all five internal pull-ups good, 4 ADC channels uniform |

### 4. Bench — carrier mated, 12 V on, USB kept

⚠ **Connect USB *before* 12 V.** The brain back-drives its own VBUS to 2.78 V, so
a USB-C host refuses to attach to an already-powered panel. Rev 1 needs no rework
— just the ordering.

| | pass condition |
|---|---|
| `s` | 12 V present; `TERM_SENSE` tracks `SW202` |
| `d` | DIP reads the ID you set — **closed = 0, so ID 0 is all four switches ON** |
| `f` | four channels, resting ~94–98, each FSR moves its own edge only |
| `i` | INT pulses (meter the 8 s dwell phase, never the 1 Hz train) |
| `l` | all 25 LEDs light |
| | pull USB: rail holds via `U304` |

⚠ **Run once on 12 V only**, or `U303` ships untested — VBUS at 5.25 V outranks
the AMS1117 so the mux otherwise never selects it.

### 5. Bench — on the bus

`R` to enable the responder, then from the master: heartbeat shows replies, `I`
passes for this panel's ID, `S <id> 500 400` acks. `T` on the panel should read
**0 CRC errors, 0 overruns, 0 uart errors**.

⚠ Give every panel a **unique** ID 0–8. Two on one address collide, and the
master's health check calls it out by name.

### 6. Record

Append to `docs/BRINGUP_LOG.md`, keyed by the RP2040 unique ID: JEDEC, `w`, DIP,
FSR mapping and resting values, LED count, bus counters. One summary row + a
detail section. With 20 boards, "which one had the odd channel" gets asked later.

---

## Per master (×1)

1. **Solder headers onto a fresh Teensy** — the reason board 1 uses the
   ex-prototype module. Two 1×14 rows only.
2. **Paste and reflow both sides.** The board is double-sided: 46 parts on the
   front, and `D1`–`D9` + `C3`–`C11` (the whole INT protection block) on the
   back. Stencils and a support jig: `hardware/master-pcb/stencil/`.
3. **Hand-solder the through-hole**: `J1` Micro-Fit 3, `J2` screw terminal,
   `J3`–`J11` JST XH ×9, two 1×14 Teensy sockets, `RN1`, `SW1`.
4. ⚠ **Leave `R1`/`R2` empty** — DNP, never ordered. The stencil already omits
   their apertures.
5. ⚠ **Do not cut the Teensy's VUSB↔VIN bridge.** The board taps VIN; cutting it
   kills `U3`'s 5 V rail with no other symptom.
6. **Bench:** `N` (all nine external pull-ups — the test `n` cannot do), `n`
   (all HIGH), `D`, `U` (`R4` present; `TP10` follows `TP9` — this is what caught
   bridged pins on `U3`), `u` with a scope.

---

## Per pad — harness

~29 cables: 9 RS-485 (1 × 60 cm + 8 × 45 cm), 9 power, 9 INT home-runs, 1
underglow pigtail, 1 master GND tie.

- ⚠ **Pull-test every crimp.** The RVSP reel is 24 AWG despite its marking and
  sits at the bottom edge of Molex `430300001`'s 20–24 AWG window — it passes,
  but barely. Sourcing genuine 22 AWG before a full harness build is the cheap
  insurance.
- ⚠ **Slide heat-shrink on before crimping.** Labels are the only marking scheme;
  after termination the only retrofit is tape, which is rejected.
- ⚠ **Shield goes to Micro-Fit pin 3 on RS-485 only** — braid gathered to a
  pigtail, a 22 AWG lead soldered on, joint heat-shrunk, *that* lead crimped.
  Never tin the part inside the crimp barrel. INT shields are trimmed and
  heat-shrunk at both ends, never bonded to the INT ground.
- 🛒 **Still to buy: 100 × 0.34 mm² ferrules** (~30 on hand, 36 needed per pad).
