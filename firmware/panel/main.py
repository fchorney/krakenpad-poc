# Panel MCU prototype firmware (MicroPython, Raspberry Pi Pico)
# Prototype wiring: 4x FSR 408, 25x WS2812B, INT LED on GPIO3
# No RS-485 in this build — Teensy not yet present.
#
# NOTE: MicroPython is used here for rapid prototyping only. Final firmware
# will be C/Pico SDK for deterministic 4000Hz+ FSR sampling on Core 0.

import machine
import neopixel
import _thread
import time

# ── Pin assignments ──────────────────────────────────────────────────────────
INT_PIN   = 3   # Drives indicator LED (HIGH = pressed). Will be open-drain to Teensy later.
LED_PIN   = 4   # WS2812B data out (via 470Ω series resistor)
FSR_PINS  = [26, 27, 28, 29]  # ADC0–ADC3. See GPIO29 note below.
NUM_LEDS  = 25

# Threshold: 0–65535. Read unloaded FSR value from serial, then set to ~2x that.
FSR_THRESHOLD = 8000

# GPIO29 (ADC3) on the stock Pico has an internal ~200kΩ VSYS divider in
# parallel with the FSR divider, which skews readings lower than the other
# three channels. Account for this when tuning, or skip FSR 4 until custom PCB.

# ── LED layout (serpentine, matches physical wiring on prototype board) ──────
# 00--01--02--03
# --06--05--04--
# 07--08--09--10
# --13--12--11--
# 14--15--16--17
# --20--19--18--
# 21--22--23--24

# Chase pattern: LEDs visited in wiring order (0→24)
PATTERN_SERPENTINE = list(range(NUM_LEDS))

# Chase pattern: LEDs visited in left-to-right, top-to-bottom physical order.
# Odd rows are wired right-to-left, so their indices are reversed here.
PATTERN_READING = [
    0,  1,  2,  3,   # row 0 — L→R
    6,  5,  4,       # row 1 — wired R→L, so reverse to get L→R physical
    7,  8,  9, 10,   # row 2 — L→R
   13, 12, 11,       # row 3 — wired R→L
   14, 15, 16, 17,   # row 4 — L→R
   20, 19, 18,       # row 5 — wired R→L
   21, 22, 23, 24,   # row 6 — L→R
]

CHASE_FRAME_HOLD  = 2   # frames to hold each chase position (~30 positions/sec at 60Hz)
CYCLES_PER_PATTERN = 2  # full chase cycles before switching pattern

# ── Hardware init ────────────────────────────────────────────────────────────
int_out = machine.Pin(INT_PIN, machine.Pin.OUT, value=0)
adcs    = [machine.ADC(machine.Pin(p)) for p in FSR_PINS]
np      = neopixel.NeoPixel(machine.Pin(LED_PIN), NUM_LEDS)


def detect_active_fsrs(samples=50, floating_threshold=3000):
    active = []
    for i, adc in enumerate(adcs):
        avg = sum(adc.read_u16() for _ in range(samples)) // samples
        is_active = avg < floating_threshold
        active.append(is_active)
        print(f"FSR {i} (GPIO{FSR_PINS[i]}): resting avg = {avg:5d} -> {'active' if is_active else 'not connected'}")
    return active


def hsv_to_rgb(h, s, v):
    """h: 0–360, s/v: 0.0–1.0. Returns (r, g, b) each 0–255."""
    if s == 0:
        c = int(v * 255)
        return (c, c, c)
    h = h % 360
    i = int(h / 60)
    f = (h / 60) - i
    p = v * (1 - s)
    q = v * (1 - s * f)
    t = v * (1 - s * (1 - f))
    rgb = [
        (v, t, p), (q, v, p), (p, v, t),
        (p, q, v), (t, p, v), (v, p, q),
    ][i]
    return tuple(int(c * 255) for c in rgb)


# ── Shared state (Core 0 writes, Core 1 reads) ───────────────────────────────
_pressed  = False
_fsr_vals = [0, 0, 0, 0]
active_fsrs = [False, False, False, False]  # set after detect_active_fsrs()


# ── Core 1: LED loop (~60 Hz) ────────────────────────────────────────────────
def led_loop():
    patterns     = [PATTERN_SERPENTINE, PATTERN_READING]
    pattern_idx  = 0
    chase_pos    = 0
    cycle_count  = 0
    frame        = 0
    TAIL_LEN     = 5

    while True:
        if _pressed:
            # FSR-aware color: pressure maps hue from blue (light) → red (hard)
            active_vals = [_fsr_vals[i] for i in range(4) if active_fsrs[i]]
            max_val     = max(active_vals) if active_vals else 0
            pressure    = min(max_val / 65535, 1.0)
            hue         = int(240 - pressure * 240)  # 240° blue → 0° red
            brightness  = 0.15 + pressure * 0.65
            color       = hsv_to_rgb(hue, 1.0, brightness)
            for i in range(NUM_LEDS):
                np[i] = color
        else:
            pattern = patterns[pattern_idx]

            for i in range(NUM_LEDS):
                np[i] = (0, 0, 0)

            for tail in range(TAIL_LEN):
                idx = (chase_pos - tail) % NUM_LEDS
                led = pattern[idx]
                # Brightest at head, fades toward tail
                b   = int((TAIL_LEN - tail) / TAIL_LEN * 30)
                np[led] = (0, 0, b)

            if frame % CHASE_FRAME_HOLD == 0:
                chase_pos += 1
                if chase_pos >= NUM_LEDS:
                    chase_pos   = 0
                    cycle_count += 1
                    if cycle_count >= CYCLES_PER_PATTERN:
                        cycle_count = 0
                        pattern_idx = (pattern_idx + 1) % len(patterns)

        np.write()
        frame += 1
        time.sleep_ms(16)


# ── Startup ──────────────────────────────────────────────────────────────────
print("Panel prototype ready. FSR_THRESHOLD =", FSR_THRESHOLD)
print("Detecting active FSR channels...")
active_fsrs = detect_active_fsrs()
active_count = sum(active_fsrs)
print(f"{active_count}/4 FSR(s) active")

_thread.start_new_thread(led_loop, ())


# ── Core 0: FSR sampling + INT line ─────────────────────────────────────────
_last_debug_ms = time.ticks_ms()

while True:
    any_pressed = False
    for i, adc in enumerate(adcs):
        if not active_fsrs[i]:
            continue
        v = adc.read_u16()
        _fsr_vals[i] = v
        if v > FSR_THRESHOLD:
            any_pressed = True

    _pressed = any_pressed
    int_out.value(1 if any_pressed else 0)

    # Debug output at ~10 Hz
    now = time.ticks_ms()
    if time.ticks_diff(now, _last_debug_ms) >= 100:
        _last_debug_ms = now
        state = "PRESSED" if _pressed else "idle   "
        print(f"{state}  FSR: {_fsr_vals[0]:5d} {_fsr_vals[1]:5d} {_fsr_vals[2]:5d} {_fsr_vals[3]:5d}")
