#!/usr/bin/env python3
"""USB High Speed verification — host side.

Pairs with firmware/master/usb_speed_test/usb_speed_test.ino. Sends 'G' to
start a throughput burst, counts bytes received independently on wall-clock
time, and cross-checks against the Teensy's own summary line.

Full Speed (12 Mbit/s) tops out ~1 MB/s of real payload. High Speed
(480 Mbit/s) should comfortably clear tens of MB/s over CDC serial even
accounting for USB/CDC overhead. The two numbers are an order of magnitude
apart, so this test doesn't need to be precise to be conclusive.

Usage: python3 tools/usb_speed_test.py [port]
"""

import glob
import sys
import time

import serial

TEST_SECONDS = 3.0


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


def main() -> None:
    port = find_port()
    with serial.Serial(port, 115200, timeout=1) as ser:
        time.sleep(0.3)
        ser.reset_input_buffer()

        # Drain the ready banner
        deadline = time.time() + 2
        while time.time() < deadline:
            line = ser.readline().decode(errors="replace").strip()
            if line:
                print(f"< {line}")
            if "Send 'G'" in line:
                break

        print("\nStarting throughput burst...")
        ser.write(b"G")

        # Read lines until GO, then switch to raw byte counting
        deadline = time.time() + 2
        got_go = False
        while time.time() < deadline:
            line = ser.readline()
            text = line.decode(errors="replace").strip()
            if text:
                print(f"< {text}")
            if text == "GO":
                got_go = True
                break
        if not got_go:
            sys.exit("never saw GO — check wiring/port")

        total = 0
        t0 = None  # start the clock on first byte, not before — excludes handshake latency
        t_end_hard = time.time() + TEST_SECONDS + 2.0  # safety cap only
        summary_line = b""
        buf = b""
        while time.time() < t_end_hard:
            chunk = ser.read(1_000_000)
            if not chunk:
                if t0 is not None:
                    break  # stream went quiet after data started — burst is over
                continue
            if t0 is None:
                t0 = time.time()
            total += len(chunk)
            buf += chunk
            if b"DONE" in buf:
                idx = buf.index(b"DONE")
                summary_line = buf[idx:]
                total -= len(summary_line)  # don't count the summary text as payload
                break
        t1 = time.time()

        # Drain any trailing summary bytes
        deadline = time.time() + 0.5
        while time.time() < deadline and b"\n" not in summary_line:
            more = ser.read(256)
            if not more:
                continue
            summary_line += more

        wall_elapsed = t1 - t0
        mbps_host = (total * 8.0) / (wall_elapsed * 1_000_000)
        mbytes_host = total / 1_000_000

        print(f"\n--- host-measured ---")
        print(f"bytes received: {total}  ({mbytes_host:.2f} MB)")
        print(f"wall time:      {wall_elapsed:.3f} s")
        print(f"throughput:     {mbps_host:.1f} Mbit/s")

        if summary_line:
            print(f"\n--- device-reported ---")
            print(summary_line.decode(errors="replace").strip())

        print()
        if mbps_host > 50:
            print(f"=> HIGH SPEED confirmed ({mbps_host:.0f} Mbit/s >> 12 Mbit/s FS ceiling)")
        elif mbps_host > 5:
            print(f"=> ambiguous ({mbps_host:.0f} Mbit/s) — check device-reported usb_high_speed flag above")
        else:
            print(f"=> looks like FULL SPEED ({mbps_host:.0f} Mbit/s ~ 12 Mbit/s ceiling)")


if __name__ == "__main__":
    main()
