#!/usr/bin/env python3
"""Live FSR telemetry viewer + threshold tuner — multi-panel.

Reads the master's USB serial, enables the telemetry stream, and renders each
panel's four FSR channels as live bars. The data path visualized is:
panel ADC -> RS-485 poll reply -> master -> USB serial -> here.

Values and thresholds are calibrated units: 0 at each sensor's tare (its
tracked rest value), 1000 at full scale (docs/PANEL_CONFIG.md → "Tare and
calibrated values"). The second line shows the four tares in raw counts.

Threshold changes are sent to the master ("S <panel> <press> <release>"),
which edits its config copy and pushes it to the addressed panel over RS-485
(cmd 'W', since 2026-10-20). The panel applies them at once, and the "ack"
shown is the master's "config matches" line, printed when the panel's 'w'
confirms it runs them. Not saved: a panel reboot gets them again from the
master, but a master reboot returns to its last `save`.

Two lines per panel: four bars (South, West, North, East, the bring-up
firmware's channel order), then thresholds and the last 'H' status reply. A
panel the master has not heard from within its timeout is drawn OFFLINE rather
than showing the last values it sent.

Keys:
  0 - 8   focus a panel (+/-/[/] act on whichever panel is focused)
  + / -   press threshold up/down
  ] / [   release threshold up/down
  h       status sweep: every panel's build, uptime, reset cause, error counters
  q       quit

Usage: python3 tools/fsr_monitor.py [port]
"""

import glob
import re
import select
import shutil
import sys
import termios
import time
import tty

import serial

NUM_PANELS = 9
ADC_MAX = 4095
CAL_MAX = 1000
STEP = 10
BAR_WIDTH = 14

# Panel ID -> position, as silkscreened on the master and printed on the cables.
PANEL_NAME = ["UL", "U", "UR", "L", "C", "R", "DL", "D", "DR"]
FSR_NAME = ["S", "W", "N", "E"]

# The master's line when a panel confirms its config; channel 0's thresholds
# stand for the panel, since `S` here sets all four alike.
ACK_RE = re.compile(r"panel (\d+) \(\S+\) config matches: 0:(\d+)/(\d+)")
STATUS_RE = re.compile(r"^# status \S+\s+id (\d+)\s+(.*)$")


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


def raw_to_cal(raw: int, tare: int):
    """firmware/shared/panel_config.h kp_raw_to_cal; None without a tare."""
    if tare > ADC_MAX:
        return None
    return int((raw - tare) * CAL_MAX / (ADC_MAX - tare))


def bar(value: int, pressed: bool, press_thr: int, rel_thr: int) -> str:
    filled = round(max(value, 0) * BAR_WIDTH / CAL_MAX)
    press_col = min(round(press_thr * BAR_WIDTH / CAL_MAX), BAR_WIDTH - 1)
    rel_col = min(round(rel_thr * BAR_WIDTH / CAL_MAX), BAR_WIDTH - 1)
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
    press_thr = [930] * NUM_PANELS  # firmware defaults (panel_config.h)
    rel_thr = [905] * NUM_PANELS
    ack = ["defaults"] * NUM_PANELS
    status = ["press h for status"] * NUM_PANELS
    vals = [[0, 0, 0, 0] for _ in range(NUM_PANELS)]
    tares = [[0xFFFF] * 4 for _ in range(NUM_PANELS)]
    mask = [0] * NUM_PANELS
    online = [False] * NUM_PANELS
    sweep_note = ""
    focus = 0

    ser = serial.Serial(port, 115200, timeout=0)
    old_tty = termios.tcgetattr(sys.stdin)
    tty.setcbreak(sys.stdin.fileno())

    def send_thresholds(panel: int) -> None:
        ser.write(f"S {panel} {press_thr[panel]} {rel_thr[panel]}\n".encode())

    lines_per_panel = 2  # bars + thresholds/status
    redraw_lines = NUM_PANELS * lines_per_panel + 1  # + sweep line

    try:
        time.sleep(0.3)
        ser.write(b"t\n")  # telemetry stream on
        print(f"listening on {port} — 0-8 focus, +/- press thr, ]/[ release thr, h status, q quits")
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
                elif key.isdigit() and int(key) < NUM_PANELS:
                    focus = int(key)
                elif key == "h":
                    ser.write(b"H\n")
                elif key == "+" or key == "=":
                    press_thr[focus] = min(press_thr[focus] + STEP, CAL_MAX)
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
                        # 12 fields from current master firmware: id, raw x4,
                        # mask, live flag, tare x4. 8 from builds before tares
                        # (no calibrated values), 7 from older still.
                        if len(parts) in (7, 8, 12):
                            p = int(parts[1])
                            if 0 <= p < NUM_PANELS:
                                vals[p] = [int(v) for v in parts[2:6]]
                                mask[p] = int(parts[6])
                                online[p] = len(parts) == 7 or parts[7] == "1"
                                if len(parts) == 12:
                                    tares[p] = [int(v) for v in parts[8:12]]
                    elif line.startswith("#"):
                        m = ACK_RE.search(line)
                        if m:
                            p = int(m.group(1))
                            if 0 <= p < NUM_PANELS:
                                ack[p] = f"{m.group(2)}/{m.group(3)}"
                        m = STATUS_RE.match(line)
                        if m:
                            p = int(m.group(1))
                            if 0 <= p < NUM_PANELS:
                                status[p] = m.group(2)
                        if line.startswith("# status sweep"):
                            sweep_note = line[2:]

            now = time.time()
            if now - last_draw < 0.03:
                continue
            last_draw = now

            sys.stdout.write(f"\033[{redraw_lines}A")
            for p in range(NUM_PANELS):
                marker = ">" if p == focus else " "
                head = f"{marker}{PANEL_NAME[p]:<2} {p}"
                if online[p]:
                    cals = [raw_to_cal(vals[p][ch], tares[p][ch]) for ch in range(4)]
                    bars = "  ".join(
                        f"{FSR_NAME[ch]} {'   ?' if cals[ch] is None else f'{cals[ch]:4d}'} "
                        f"{bar(cals[ch] or 0, bool(mask[p] & (1 << ch)), press_thr[p], rel_thr[p])}"
                        for ch in range(4)
                    )
                else:
                    bars = "\033[90mOFFLINE — no reply from this panel\033[0m"
                sys.stdout.write(f"\r\033[K{head}  {bars}\n")
                # A wrapped line breaks the cursor-up redraw, so clip to width.
                cols = shutil.get_terminal_size().columns - 1
                tare = ",".join("?" if t > ADC_MAX else str(t) for t in tares[p])
                info = (f"       thr {press_thr[p]}/{rel_thr[p]} (ack {ack[p]})  tare {tare}"
                        f"  {status[p]}")
                sys.stdout.write(f"\r\033[K{info[:cols]}\n")
            sys.stdout.write(f"\r\033[K{sweep_note[:cols]}\n")
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
