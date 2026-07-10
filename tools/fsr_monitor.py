#!/usr/bin/env python3
"""Live FSR telemetry viewer + threshold tuner — multi-panel.

Reads the master's USB serial, enables the telemetry stream, and renders each
panel's four FSR channels as live bars. The data path visualized is:
panel ADC -> RS-485 poll reply -> master -> USB serial -> here.

Threshold changes are sent to the master ("S <panel> <press> <release>"),
which forwards them to the addressed panel over RS-485 (cmd 'C'); the panel
applies them to its live hysteresis logic and acks. RAM only — reboot
restores defaults.

Keys:
  0 / 1   focus panel 0 / 1 (+/-/[/] act on whichever panel is focused)
  + / -   press threshold up/down
  ] / [   release threshold up/down
  q       quit

Usage: python3 tools/fsr_monitor.py [port]
"""

import glob
import re
import select
import sys
import termios
import time
import tty

import serial

NUM_PANELS = 2
ADC_MAX = 4095
STEP = 50
BAR_WIDTH = 56

ACK_RE = re.compile(r"panel (\d+) ack thresholds: press=(\d+) release=(\d+)")


def find_port() -> str:
    if len(sys.argv) > 1:
        return sys.argv[1]
    ports = sorted(glob.glob("/dev/cu.usbmodem*"))
    if not ports:
        sys.exit("no /dev/cu.usbmodem* ports found — is the Teensy plugged in?")
    if len(ports) > 1:
        print(f"multiple ports found, using {ports[0]} (pass one explicitly to override):")
        for p in ports:
            print(f"  {p}")
    return ports[0]


def bar(value: int, pressed: bool, press_thr: int, rel_thr: int) -> str:
    filled = round(value * BAR_WIDTH / ADC_MAX)
    press_col = min(round(press_thr * BAR_WIDTH / ADC_MAX), BAR_WIDTH - 1)
    rel_col = min(round(rel_thr * BAR_WIDTH / ADC_MAX), BAR_WIDTH - 1)
    cells = []
    for i in range(BAR_WIDTH):
        if i < filled:
            cells.append("█")
        elif i == press_col:
            cells.append("│")
        elif i == rel_col:
            cells.append("┊")
        else:
            cells.append(" ")
    color = "\033[91m" if pressed else "\033[92m"  # red pressed, green idle
    return f"{color}{''.join(cells)}\033[0m"


def main() -> None:
    port = find_port()
    press_thr = [500] * NUM_PANELS  # firmware boot defaults
    rel_thr = [400] * NUM_PANELS
    ack = ["(defaults, nothing sent yet)"] * NUM_PANELS
    vals = [[0, 0, 0, 0] for _ in range(NUM_PANELS)]
    mask = [0] * NUM_PANELS
    focus = 0

    ser = serial.Serial(port, 115200, timeout=0)
    old_tty = termios.tcgetattr(sys.stdin)
    tty.setcbreak(sys.stdin.fileno())

    def send_thresholds(panel: int) -> None:
        ser.write(f"S {panel} {press_thr[panel]} {rel_thr[panel]}\n".encode())

    lines_per_panel = 5  # header + 4 bars
    redraw_lines = NUM_PANELS * lines_per_panel

    try:
        time.sleep(0.3)
        ser.write(b"t\n")  # telemetry stream on
        print(f"listening on {port} — 0/1 focus panel, +/- press thr, ]/[ release thr, q quits")
        print("bar markers: │ press threshold   ┊ release threshold\n")
        print("\n" * redraw_lines, end="")

        buf = b""
        last_draw = 0.0

        while True:
            r, _, _ = select.select([ser, sys.stdin], [], [], 0.05)

            if sys.stdin in r:
                key = sys.stdin.read(1)
                if key == "q":
                    break
                elif key in ("0", "1"):
                    focus = int(key)
                elif key == "+" or key == "=":
                    press_thr[focus] = min(press_thr[focus] + STEP, ADC_MAX)
                    send_thresholds(focus)
                elif key == "-":
                    press_thr[focus] = max(press_thr[focus] - STEP, rel_thr[focus] + STEP)
                    send_thresholds(focus)
                elif key == "]":
                    rel_thr[focus] = min(rel_thr[focus] + STEP, press_thr[focus] - STEP)
                    send_thresholds(focus)
                elif key == "[":
                    rel_thr[focus] = max(rel_thr[focus] - STEP, STEP)
                    send_thresholds(focus)

            if ser in r:
                buf += ser.read(4096)
                while b"\n" in buf:
                    raw, buf = buf.split(b"\n", 1)
                    line = raw.decode(errors="replace").strip()
                    if line.startswith("T "):
                        parts = line.split()
                        if len(parts) == 7:
                            p = int(parts[1])
                            if 0 <= p < NUM_PANELS:
                                vals[p] = [int(v) for v in parts[2:6]]
                                mask[p] = int(parts[6])
                    elif line.startswith("#"):
                        m = ACK_RE.search(line)
                        if m:
                            p = int(m.group(1))
                            if 0 <= p < NUM_PANELS:
                                ack[p] = f"press={m.group(2)} release={m.group(3)}"

            now = time.time()
            if now - last_draw < 0.03:
                continue
            last_draw = now

            sys.stdout.write(f"\033[{redraw_lines}A")
            for p in range(NUM_PANELS):
                marker = "  <-- focused" if p == focus else ""
                sys.stdout.write(
                    f"\r\033[KPanel {p}{marker}   press>{press_thr[p]}  release<{rel_thr[p]}   last ack: {ack[p]}\n"
                )
                for ch in range(4):
                    v = vals[p][ch]
                    pressed = bool(mask[p] & (1 << ch))
                    label = "PRESSED" if pressed else "       "
                    sys.stdout.write(
                        f"\r\033[K  FSR {ch}  {v:4d}  {bar(v, pressed, press_thr[p], rel_thr[p])} {label}\n"
                    )
            sys.stdout.flush()
    finally:
        termios.tcsetattr(sys.stdin, termios.TCSADRAIN, old_tty)
        try:
            ser.write(b"t\n")  # stream off on the way out
            ser.close()
        except Exception:
            pass
        print("\nbye")


if __name__ == "__main__":
    main()
