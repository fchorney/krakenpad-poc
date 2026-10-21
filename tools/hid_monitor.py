#!/usr/bin/env python3
"""Watch the master's USB pad-input reports (docs/USB_PROTOCOL.md).

Opens the KrakenPad joystick interface and prints every press state change by
panel name, a sequence-number gap if the host missed a transition, and once a
second the report rate (heartbeats included, ~100/s when idle).

Report ID 1, 4 bytes: [1] [panels 0-7] [panel 8 + 7 pad bits] [sequence]

Needs hidapi: `brew install hidapi` and `pip install hid`.

Usage: python3 tools/hid_monitor.py
"""

import sys
import time

try:
    import hid
except ImportError:
    sys.exit("needs the hid module: brew install hidapi && pip install hid")

# On macOS hidapi opens devices exclusively by default (hid_init() sets it), so
# while this tool runs no other program could open the pad. Open shared, as the
# SDK does. The hid module has already called hid_init() on import.
if hasattr(hid.hidapi, "hid_darwin_set_open_exclusive"):
    hid.hidapi.hid_darwin_set_open_exclusive(0)

VID, PID = 0x16C0, 0x0487
PRODUCT = "KrakenPad"
PANEL_NAME = ["UL", "U", "UR", "L", "C", "R", "DL", "D", "DR"]


def find_pad():
    for d in hid.enumerate(VID, PID):
        # The CDC console shares the VID/PID; only the joystick is a HID.
        if d["product_string"] == PRODUCT and d["usage_page"] == 0x01 and d["usage"] == 0x04:
            return d["path"]
    return None


def names(mask):
    return " ".join(PANEL_NAME[i] for i in range(9) if mask & (1 << i)) or "-"


def main():
    path = find_pad()
    if path is None:
        sys.exit(f"no {PRODUCT} joystick ({VID:04x}:{PID:04x}). Built with firmware/master/build.sh?")
    dev = hid.Device(path=path)
    print(f"{PRODUCT}: {dev.manufacturer} / {dev.product}. Ctrl-C to stop.")

    last_mask = last_seq = None
    count, t_rate = 0, time.monotonic()
    t0 = time.monotonic()
    while True:
        r = dev.read(64, timeout=100)
        now = time.monotonic()
        if r:
            if len(r) < 4 or r[0] != 1:
                print(f"unexpected report: {bytes(r).hex()}")
                continue
            count += 1
            mask = r[1] | (r[2] << 8)
            seq = r[3]
            if last_seq is not None and seq != last_seq:
                step = (seq - last_seq) & 0xFF
                if step != 1:
                    print(f"  !! sequence jumped {last_seq} -> {seq}: missed {step - 1} transition(s)")
            if mask != last_mask:
                print(f"{now - t0:9.4f}s  seq {seq:3d}  0x{mask:03x}  {names(mask)}")
            last_mask, last_seq = mask, seq
        if now - t_rate >= 1.0:
            print(f"           {count / (now - t_rate):.0f} reports/s", end="\r")
            count, t_rate = 0, now


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print()
