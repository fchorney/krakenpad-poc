# Breadboard Prototype Wiring Guide

One master MCU (Teensy 4.0) and one panel MCU (Raspberry Pi Pico) connected via RS-485,
with a single interrupt wire, 4 FSRs, and a WS2812B LED strip.

---

## Parts List

### ICs
- Teensy 4.0 — 1×
- Raspberry Pi Pico — 1×
- MAX3485 (SOIC-8) — 2× (one per RS-485 end)
- SOIC-8 to DIP adapter — 2× (for breadboard use)
- SN74AHCT125N (DIP-14) — 1× (3.3V → 5V level shifter for LED data)

### Passives
- 120Ω resistor — 2× (RS-485 termination, one at each end)
- 10kΩ resistor — 5× (4× FSR voltage dividers, 1× INT pull-up)
- 470Ω resistor — 1× (series on LED data line)
- 1000µF electrolytic capacitor — 1× (LED power supply decoupling)
- 100nF ceramic capacitor — 6× (decoupling: 1 per MAX3485, 1 per 74AHCT125, spares)
- 10nF ceramic capacitor — 4× (one per FSR ADC node to GND — kills ADC mux crosstalk, see gotchas)
- TVS diode (e.g. SMBJ3.3A) — 1× (ESD protection on INT line at Teensy)

### Sensors & Actuators
- Interlink Electronics FSR 408 — 4× (long strip FSRs from SMX panel)
- WS2812B LED strip (5V, 60 LED/m) — cut to 25 LEDs or use however many you have

### Power
- USB power adapter, 5V 2A+ — 1× (dedicated to LED strip)
- Micro-USB cables — 2× (Teensy programming/power, Pico programming/power)

### Miscellaneous
- 2× large breadboards
- Jumper wires (various lengths)
- Twisted pair wire, short length (~30cm for RS-485 A/B between boards)
- 0.1" pin headers — 2× strips (Teensy and Pico ship without headers)

---

## Power Distribution

```
USB charger (5V 2A+) ──────────────────── WS2812B VDD
                                       └── 74AHCT125 pin 14 (VCC)
                      (keep this separate from MCU 5V to avoid LED current sag)

Teensy USB ──── Teensy onboard regulator ──── Teensy 3.3V out ──── MAX3485 master VCC

Pico USB ──── Pico onboard regulator ──── Pico 3V3 OUT (pin 36) ──── MAX3485 panel VCC

GND: Teensy GND + Pico GND + LED GND + USB charger GND all joined on breadboard GND rail
     (shared GND is required for RS-485 to work)
```

---

## Teensy 4.0 Connections

| Teensy Pin | Signal | Connects To |
|-----------|--------|-------------|
| GND | Ground | Common GND rail |
| 3.3V | 3.3V out | MAX3485 master VCC (pin 8), one leg of INT pull-up resistor |
| 0 (RX1) | UART1 RX | MAX3485 master RO (pin 1) |
| 1 (TX1) | UART1 TX | MAX3485 master DI (pin 4) |
| 2 | RS-485 direction | MAX3485 master DE (pin 3) + ~RE (pin 2) tied together |
| 3 | INT input | 10kΩ pull-up → 3.3V; TVS diode → GND; wire → Pico GPIO5 |

---

## MAX3485 — Master Side (on SOIC-8 DIP adapter)

| Pin | Name | Connects To |
|-----|------|-------------|
| 1 | RO | Teensy pin 0 (RX1) |
| 2 | ~RE | Tied to pin 3, then to Teensy pin 2 |
| 3 | DE | Tied to pin 2, then to Teensy pin 2 |
| 4 | DI | Teensy pin 1 (TX1) |
| 5 | GND | Common GND rail |
| 6 | A | RS-485 A wire (twisted pair) → MAX3485 panel pin 6 |
| 7 | B | RS-485 B wire (twisted pair) → MAX3485 panel pin 7 |
| 8 | VCC | Teensy 3.3V out |

**Also on this IC:**
- 100nF ceramic cap between pins 8 and 5 (place as close to the chip as possible)
- 120Ω resistor between pins 6 and 7 (master-end RS-485 termination)

---

## Raspberry Pi Pico Connections

| Pico Pin | GPIO | Signal | Connects To |
|----------|------|--------|-------------|
| 1 | GPIO0 | UART0 TX | MAX3485 panel DI (pin 4) |
| 2 | GPIO1 | UART0 RX | MAX3485 panel RO (pin 1) |
| 3 | GND | Ground | Common GND rail |
| 4 | GPIO2 | RS-485 direction | MAX3485 panel DE (pin 3) + ~RE (pin 2) tied together |
| 5 | GPIO3 | Debug press LED | Push-pull output, HIGH on press — debug visualization only |
| 6 | GPIO4 | LED data | 74AHCT125 pin 2 (1A input) |
| 7 | GPIO5 | INT output | Wire → Teensy pin 3 (open-drain: output LOW to signal press, input/high-Z otherwise) |
| 31 | GPIO26 / ADC0 | FSR 1 | FSR voltage divider midpoint (see FSR section) |
| 32 | GPIO27 / ADC1 | FSR 2 | FSR voltage divider midpoint |
| 34 | GPIO28 / ADC2 | FSR 3 | FSR voltage divider midpoint |
| 35 | GPIO29 / ADC3 | FSR 4 | FSR voltage divider midpoint (see GPIO29 warning below) |
| 36 | 3V3 OUT | 3.3V | MAX3485 panel VCC (pin 8) |

---

## MAX3485 — Panel Side (on SOIC-8 DIP adapter)

| Pin | Name | Connects To |
|-----|------|-------------|
| 1 | RO | Pico GPIO1 (UART0 RX) |
| 2 | ~RE | Tied to pin 3, then to Pico GPIO2 |
| 3 | DE | Tied to pin 2, then to Pico GPIO2 |
| 4 | DI | Pico GPIO0 (UART0 TX) |
| 5 | GND | Common GND rail |
| 6 | A | RS-485 A wire → MAX3485 master pin 6 |
| 7 | B | RS-485 B wire → MAX3485 master pin 7 |
| 8 | VCC | Pico 3V3 OUT (pin 36) |

**Also on this IC:**
- 100nF ceramic cap between pins 8 and 5 (place as close to chip as possible)
- 120Ω resistor between pins 6 and 7 (panel-end RS-485 termination — single panel = last in chain)

---

## SN74AHCT125N Level Shifter (DIP-14)

Only channel 1 is used. Disable all other channels.

| Pin | Name | Connects To |
|-----|------|-------------|
| 1 | 1~OE | GND (enables channel 1) |
| 2 | 1A | Pico GPIO4 (3.3V LED data) |
| 3 | 1Y | 470Ω resistor → WS2812B DIN |
| 4 | 2~OE | VCC (disables channel 2) |
| 5 | 2A | Leave unconnected |
| 6 | 2Y | Leave unconnected |
| 7 | GND | Common GND rail |
| 8 | 3Y | Leave unconnected |
| 9 | 3A | Leave unconnected |
| 10 | 3~OE | VCC (disables channel 3) |
| 11 | 4Y | Leave unconnected |
| 12 | 4A | Leave unconnected |
| 13 | 4~OE | VCC (disables channel 4) |
| 14 | VCC | 5V rail |

100nF ceramic cap between pins 14 and 7 (place close to chip).

---

## FSR Voltage Dividers (×4, identical)

```
3.3V ──────┬──── FSR 408 lead 1
           │
      FSR 408 body
           │
           ├──── FSR 408 lead 2
           │          │
           │       ADC pin (GPIO26 / 27 / 28 / 29)
           │
         10kΩ
           │
          GND
```

One divider per FSR, four total. The ADC reads the voltage at the midpoint between the FSR
and the 10kΩ pull-down. Higher pressure = lower FSR resistance = higher ADC voltage.

---

## WS2812B LED Strip

| Connection | Connects To |
|-----------|-------------|
| DIN (data in) | 74AHCT125 pin 3 (1Y output), via 470Ω series resistor |
| VDD (5V) | Dedicated 5V USB power adapter (not Pico VBUS) |
| GND | Common GND rail |

Place 1000µF electrolytic capacitor across VDD and GND **at the point where power enters
the strip**, not at the breadboard. Observe polarity (+ to VDD, − to GND).

---

## INT Line Detail

The pull-up, TVS diode, Teensy pin 3, and the wire to the Pico all join at **one
single node** (one breadboard row):

```
              Teensy 3.3V
                   │
                  10kΩ            (pull-up: holds the line HIGH when idle)
                   │
    ●──────────────┼──────────────────●──────────────●
    │                                 │              │
 Teensy pin 3                    TVS diode      wire to Pico GPIO5
 (reads the line)             (SMBJ3.3A, cathode
                               to node, anode to GND)
                                      │
                                     GND
```

- **Idle:** Pico GPIO5 is high-Z → 10kΩ pulls the node to 3.3V → pin 3 reads HIGH.
- **Press:** Pico GPIO5 outputs LOW, sinking the node → pin 3 reads LOW → interrupt fires.
- **Wire unplugged:** same as idle (pull-up wins) → reads HIGH, no phantom presses.
- The TVS is not in the signal path — it sits node-to-GND and only conducts on an ESD spike.

Pico firmware: configure GPIO5 as open-drain output. Pull LOW to signal a press, release
(set as input / high-Z) when not pressed. The 10kΩ at the Teensy end pulls it HIGH.
Teensy firmware: configure pin 3 as INPUT with hardware interrupt on CHANGE (both edges).

GPIO3 additionally drives a push-pull debug LED (HIGH on press) for visual confirmation
independent of the INT wire — prototype debugging aid only, not part of the final design.

---

## RS-485 Bus

Use a short length (~30cm) of twisted pair wire between the two MAX3485 chips.
- One wire = A line (pins 6 of both chips)
- Other wire = B line (pins 7 of both chips)

Twisted pair is important even at short distances — parallel wires pick up noise.

**DE/~RE direction control:** Both DE and ~RE on each MAX3485 are tied together and driven
by a single GPIO (Teensy pin 2, Pico GPIO2).
- HIGH = transmit mode (driver enabled, receiver disabled)
- LOW = receive mode (driver disabled, receiver enabled)

**Critical firmware timing:** When transmitting, set DE HIGH before writing to UART TX.
After the last byte, wait for the UART "TX complete" flag (not "TX buffer empty" — these
are different), then set DE LOW. Dropping DE too early clips the last stop bit.

---

## Warnings and Gotchas

### GPIO29 (ADC3) on Pico
On the Raspberry Pi Pico board, GPIO29 is internally connected to a voltage divider for
VSYS monitoring. Connecting an FSR voltage divider to it creates two dividers in parallel
and skews the ADC reading. For a more accurate 4th FSR reading on the Pico prototype,
account for the ~200kΩ internal divider in your calculations, or only use 3 FSRs
(GPIO26–28) and add the 4th on the final custom PCB where GPIO29 is a clean ADC pin.

### Neither Teensy 4.0 nor RP2040 is 5V tolerant
All I/O on both MCUs is 3.3V only. Never connect a 5V signal to any GPIO. The MAX3485
(3.3V version), the Pico INT wire, and the level shifter input are all 3.3V — correct.
The level shifter OUTPUT is 5V — only connect that to the LED strip, not to any MCU pin.

### Shared GND is mandatory
The Teensy and Pico must share a common ground on the breadboard even though they are
powered from separate USB ports. RS-485 requires a common voltage reference between the
two transceivers. Without a shared GND, communication will be unreliable or fail.

### SOIC-8 adapter orientation
The MAX3485 has a dot or notch marking pin 1. Match it to the pin 1 indicator on the DIP
adapter. An upside-down IC here is a frustrating and non-obvious failure mode.

### Decoupling caps must be physically close
Place 100nF ceramic caps as close as possible to the VCC and GND pins of each IC.
Breadboards have high parasitic inductance — a cap placed far away across the board offers
little protection. The 74AHCT125 is especially sensitive when switching at WS2812B speeds.

### FSR 408 flat leads
The FSR leads are flat and flexible and make unreliable contact in standard breadboard
sockets. Bend the last few mm of each lead slightly before inserting, or solder a short
length of solid-core wire to each lead to get a solid breadboard connection.

### LED strip power — keep separate from MCU supply
Connect the LED strip VDD directly to the 5V USB power adapter. Do not run LED power
through the same breadboard rail that powers the level shifter from Pico VBUS. The 1000µF
cap handles transients but high LED current draw on the same rail can cause voltage dips
that glitch the MCU.

### ADC mux crosstalk between FSR channels — solved with 10nF caps
The RP2040's four ADC inputs share one SAR ADC behind a mux, and the sample
capacitor retains charge from the previous conversion. Reading channels
back-to-back lets a high channel (pressed FSR) bleed ~15 counts into the next
channel read. **Fix (bench-verified): 10nF ceramic from each ADC node to GND.**
Measured result: crosstalk dropped to 1–2 counts, and the idle noise floor fell
from ~45 to ~15–20 counts as a bonus — the caps also filter broadband noise on
the high-impedance divider node. Adds only ~30µs of response lag.

These caps are **required on the final panel PCB** (one per FSR channel, close
to the RP2040's ADC pins). A software alternative (discard the first conversion
after each mux switch, `ADC_DUMMY_READ` in main.c) works but halves the sample
rate; it's kept as a compile-time fallback and disabled.

### GPIO23 and GPIO24 on Pico — avoid
GPIO23 controls the onboard SMPS power mode. GPIO24 is VBUS sense. Neither is safe to
use as general-purpose I/O.

### Pico GPIO25 and Teensy pin 13 — onboard LEDs
Both are usable as GPIO but the onboard LED will light up. Useful for firmware heartbeat
debugging.

---

## Quick Reference — What Each GPIO Does

### Teensy 4.0
| Pin | Function |
|-----|----------|
| 0 | RS-485 RX (Serial1) |
| 1 | RS-485 TX (Serial1) |
| 2 | RS-485 DE/~RE direction control |
| 3 | INT input from panel (interrupt on CHANGE — falling = press, rising = release) |

### Raspberry Pi Pico
| GPIO | Function |
|------|----------|
| 0 | RS-485 TX (UART0) |
| 1 | RS-485 RX (UART0) |
| 2 | RS-485 DE/~RE direction control |
| 3 | Debug press LED (push-pull, HIGH on press) |
| 4 | LED data out → level shifter |
| 5 | INT output to master (open-drain) |
| 26 | ADC0 — FSR 1 |
| 27 | ADC1 — FSR 2 |
| 28 | ADC2 — FSR 3 |
| 29 | ADC3 — FSR 4 (see GPIO29 warning above) |
