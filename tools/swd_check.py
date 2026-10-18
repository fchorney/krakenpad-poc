#!/usr/bin/env python3
"""SWD smoke test for one panel through the carrier's J209 header.

Drives OpenOCD through a Pico running debugprobe (CMSIS-DAP) and checks the
whole SWD path, J209 -> J212/J303 -> U306 pins 24/25, against fixed RP2040
values rather than whatever the probe happens to return:

  1. SW-DP answers with the RP2040's DPIDR (0x0bc12477)
  2. both Cortex-M0+ cores examine
  3. SYSINFO CHIP_ID (0x40000000) reads back part 0x0002, manufacturer 0x927

The cores are halted for the read and resumed afterwards, so whatever
firmware is running carries on.

Probe wiring (debugprobe_on_pico): GP2 -> J209 pin 3 (SWCLK),
GP3 -> J209 pin 1 (SWDIO), GND -> J209 pin 2. The panel is powered on its own
(USB before 12V); the Pico's 3V3 is not connected.

Usage: python3 tools/swd_check.py [--speed KHZ] [-v]
"""

import argparse
import re
import subprocess
import sys

RP2040_DPIDR = 0x0BC12477
CHIP_ID_ADDR = 0x40000000
CHIP_ID_PART = 0x0002
CHIP_ID_MANUFACTURER = 0x927

DPIDR_RE = re.compile(r"SWD DPIDR (0x[0-9a-fA-F]+)")
CORE_RE = re.compile(r"rp2040\.core([01]): hardware has")
CHIP_ID_RE = re.compile(r"0x40000000: ([0-9a-fA-F]{8})")


def run_openocd(speed: int) -> str:
    cmd = [
        "openocd",
        "-f", "interface/cmsis-dap.cfg",
        "-f", "target/rp2040.cfg",
        "-c", f"adapter speed {speed}",
        # mdw's result isn't logged from a -c command, so echo it
        "-c", f"init; halt; echo [mdw {CHIP_ID_ADDR:#x}]; resume; exit",
    ]
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=20)
    except FileNotFoundError:
        sys.exit("openocd not found on PATH")
    except subprocess.TimeoutExpired:
        sys.exit("FAIL: openocd timed out")
    # OpenOCD logs to stderr
    return result.stdout + result.stderr


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--speed", type=int, default=5000, help="SWD clock in kHz (default 5000)")
    ap.add_argument("-v", "--verbose", action="store_true", help="print the full OpenOCD log")
    args = ap.parse_args()

    log = run_openocd(args.speed)
    if args.verbose:
        print(log)

    if "Using CMSIS-DAP" not in log:
        print(log)
        sys.exit("FAIL: no CMSIS-DAP probe found — is the debugprobe Pico plugged in?")

    ok = True

    m = DPIDR_RE.search(log)
    dpidr = int(m.group(1), 16) if m else None
    if dpidr == RP2040_DPIDR:
        print(f"PASS  DPIDR    {dpidr:#010x}")
    else:
        got = f"{dpidr:#010x}" if dpidr is not None else "no reply"
        print(f"FAIL  DPIDR    {got} (want {RP2040_DPIDR:#010x})")
        ok = False

    cores = sorted(set(CORE_RE.findall(log)))
    if cores == ["0", "1"]:
        print("PASS  cores    core0 + core1 examined")
    else:
        print(f"FAIL  cores    examined: {', '.join('core' + c for c in cores) or 'none'}")
        ok = False

    m = CHIP_ID_RE.search(log)
    if m:
        chip_id = int(m.group(1), 16)
        revision = chip_id >> 28
        part = (chip_id >> 12) & 0xFFFF
        manufacturer = chip_id & 0xFFF
        if part == CHIP_ID_PART and manufacturer == CHIP_ID_MANUFACTURER:
            print(f"PASS  CHIP_ID  {chip_id:#010x} (RP2040 rev B{revision - 1})")
        else:
            print(f"FAIL  CHIP_ID  {chip_id:#010x} (part {part:#x}, manufacturer {manufacturer:#x})")
            ok = False
    else:
        print("FAIL  CHIP_ID  not read")
        ok = False

    if not ok:
        if not args.verbose:
            print("\n--- openocd log ---")
            print(log)
        sys.exit(1)
    print("SWD OK")


if __name__ == "__main__":
    main()
