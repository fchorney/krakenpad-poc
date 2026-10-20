#!/usr/bin/env python3
"""Update the panel bootloader over the RS-485 bus, through the master.

    python3 tools/panel_boot_update.py --app APP.bin [--panels 0] [--port PORT]

1. Installs the boot updater (an app) with the ordinary bus update. It writes
   the new bootloader into the slot that isn't booting, migrating a panel still
   on the single-bootloader layout first, then reboots into the bootloader and
   holds there.
2. Asks each panel for its bootloader tag with 'Q' until it equals the new
   build's: the CRC-32 of panel_bootloader_a.bin followed by
   panel_bootloader_b.bin. Anything else is a failure.
3. Installs APP.bin again (the updater replaced it), and the panels run it.

Images default to firmware/panel/c/bootloader/build/. Start with one panel
(--panels 0) before the rest. Design: docs/FIELD_UPDATE.md → "Updating the
bootloader over the bus".
"""

import argparse
import os
import sys
import time
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from panel_update import (PANEL_NAME, PanelBus, PanelUpdater, UpdateError,  # noqa: E402
                          check_image, find_port, parse_panels)

BUILD = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                     "..", "firmware", "panel", "c", "bootloader", "build")
UPDATER_SECONDS = 30      # flash work + the 2 s result blink + reboot, with margin


def read_image(path: str) -> bytes:
    image = open(path, "rb").read()
    try:
        check_image(image)
    except UpdateError as e:
        sys.exit(f"refusing {path}: {e}")
    return image


def wait_for_tag(updater: PanelUpdater, pids, tag: int, log=print):
    """Poll 'Q' until each panel reports `tag`. A panel still running the boot
    updater is silent; one back in the bootloader with another tag has failed."""
    done, wrong = {}, {}
    deadline = time.time() + UPDATER_SECONDS
    while len(done) + len(wrong) < len(pids) and time.time() < deadline:
        for pid in pids:
            if pid in done or pid in wrong:
                continue
            st = updater.query(pid, tries=1)
            if st is None:
                continue
            (done if st.boot_tag == tag else wrong)[pid] = st
        time.sleep(0.2)
    for pid in pids:
        name = PANEL_NAME[pid] if pid < len(PANEL_NAME) else "?"
        if pid in done:
            log(f"  {name:<2} id {pid}: bootloader {tag:08X}  OK")
        elif pid in wrong:
            log(f"  {name:<2} id {pid}: bootloader {wrong[pid].boot_tag:08X}, expected {tag:08X}  FAILED")
        else:
            log(f"  {name:<2} id {pid}: no answer in {UPDATER_SECONDS} s  FAILED")
    return sorted(done)


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--app", required=True, help="application to reinstall afterwards (e.g. panel_firmware.bin)")
    ap.add_argument("--updater", default=os.path.join(BUILD, "panel_boot_updater.bin"))
    ap.add_argument("--slot-a", default=os.path.join(BUILD, "panel_bootloader_a.bin"))
    ap.add_argument("--slot-b", default=os.path.join(BUILD, "panel_bootloader_b.bin"),
                    help="the slot builds the updater carries; their CRC-32 is the expected tag")
    ap.add_argument("--port", help="master's USB serial port")
    ap.add_argument("--panels", default="0-8", help="panel IDs, e.g. 0 or 0-8 (default 0-8)")
    args = ap.parse_args()

    updater_img = read_image(args.updater)
    app_img = read_image(args.app)
    tag = zlib.crc32(open(args.slot_b, "rb").read(), zlib.crc32(open(args.slot_a, "rb").read()))
    pids = parse_panels(args.panels)
    print(f"new bootloader tag {tag:08X}; panels {pids}")

    bus = PanelBus(find_port(args.port))
    try:
        bus.enter()
        print("\n1. installing the boot updater")
        PanelUpdater(bus, updater_img, pids).run(force=True)

        print("\n2. waiting for the new bootloader")
        ok = wait_for_tag(PanelUpdater(bus, updater_img, pids), pids, tag)

        print("\n3. reinstalling the application")
        PanelUpdater(bus, app_img, pids).run(force=True)
    except (UpdateError, KeyboardInterrupt) as e:
        print(f"\nABORTED: {e or 'interrupted'}")
        print("panels are left holding in the bootloader or running the updater; re-run to finish")
        sys.exit(1)
    finally:
        try:
            bus.leave()
        except Exception:
            pass

    if len(ok) != len(pids):
        sys.exit(f"\nbootloader NOT updated on {sorted(set(pids) - set(ok))}")
    print(f"\nbootloader {tag:08X} on {len(ok)} panel(s)")


if __name__ == "__main__":
    main()
