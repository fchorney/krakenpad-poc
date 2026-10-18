#!/usr/bin/env python3
"""Update panel firmware over the RS-485 bus, through the master.

The master is put into passthrough (`P`) and only forwards frames; every step
of the update runs here. Protocol and design: docs/FIELD_UPDATE.md.

  1. hold     broadcast 'B' for a second: running apps reboot into the
              bootloader, and any panel booting meanwhile catches it in its
              200 ms listen window
  2. survey   'Q' each panel: who is present, what each one has installed
  3. begin    broadcast 'U' (length + CRC-32); each panel erases its staging slot
  4. stream   broadcast every 'D' block once
  5. gap-fill 'Q' each panel, re-send what it reports missing, until all are
              staged and CRC-verified
  6. commit   broadcast 'A'; each panel copies staging into its app region and
              verifies it
  7. run      broadcast 'X'; every panel starts the new application

Nothing is installed anywhere until every participating panel has a verified
copy staged. A failure before step 6 sends 'X', and every panel restarts the
firmware it already had.

The image must be the .bin of an application linked at 0x10008000 (the
bring-up build does this by default); the tool refuses anything else. Panels
need panel_bootloader, loaded once over USB.

Usage: python3 tools/panel_update.py IMAGE.bin [--port P] [--panels 0-8] [--force]
"""

import argparse
import glob
import struct
import sys
import time
import zlib

import serial

SYNC = 0x55
BCAST = 0xFF
BLOCK = 64
APP_BASE = 0x10008000
APP_SIZE = 480 * 1024
APP_VTOR_OFFSET = 0x100

ST_NAMES = {
    0: "idle", 1: "erasing", 2: "receiving", 3: "staged", 4: "CRC FAIL",
    5: "REFUSED", 6: "installed", 7: "COPY FAIL",
}
ST_RECEIVING, ST_STAGED, ST_CRC_FAIL, ST_REFUSED, ST_INSTALLED, ST_COPY_FAIL = 2, 3, 4, 5, 6, 7
PANEL_NAME = ["UL", "U", "UR", "L", "C", "R", "DL", "D", "DR"]


class UpdateError(Exception):
    pass


def crc8(data: bytes) -> int:
    crc = 0
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = ((crc << 1) ^ 0x07) & 0xFF if crc & 0x80 else (crc << 1) & 0xFF
    return crc


def check_image(image: bytes) -> None:
    """Refuse anything that is not an application linked behind the bootloader.

    Checked against fixed requirements, not against a build: the size limit of
    the app region, and a vector table at +0x100 whose reset handler lies inside
    0x10008000..0x10080000. A standalone image (linked at 0x10000000) fails the
    second test, which matters: installed at the app offset it would never run.
    """
    if len(image) > APP_SIZE:
        raise UpdateError(f"image is {len(image)} bytes; the app region holds {APP_SIZE}")
    if len(image) < APP_VTOR_OFFSET + 8:
        raise UpdateError("image too small to contain a vector table")
    sp, pc = struct.unpack_from("<II", image, APP_VTOR_OFFSET)
    if not (0x20000000 <= sp <= 0x20042000):
        raise UpdateError(f"initial SP {sp:#010x} is not in SRAM — not a panel application")
    if not (pc & 1 and APP_BASE + APP_VTOR_OFFSET <= pc < APP_BASE + APP_SIZE):
        hint = ""
        if 0x10000000 <= pc < APP_BASE:
            hint = " — it is linked at 0x10000000 (standalone); rebuild with PANEL_UNDER_BOOTLOADER=ON"
        raise UpdateError(f"reset vector {pc:#010x} is outside the app region{hint}")


class PanelBus:
    """Frame-level access to the panel bus through the master's passthrough."""

    def __init__(self, port: str):
        self.ser = serial.Serial(port, 115200, timeout=0)
        self.rx = bytearray()

    def enter(self) -> None:
        self.ser.reset_input_buffer()
        self.ser.write(b"\nP\n")
        deadline = time.time() + 3.0
        text = b""
        while time.time() < deadline:
            text += self.ser.read(4096)
            tail = text.partition(b"# passthrough ON")[2]
            if b"\n" in tail:
                # Anything after that line is binary. Waiting for the whole
                # line matters: ASCII 'U' is 0x55, the frame sync byte.
                self.rx = bytearray(tail.partition(b"\n")[2])
                return
            time.sleep(0.01)
        raise UpdateError("master did not enter passthrough — is it running current firmware?")

    def leave(self) -> None:
        self.send(0x00, 0x00)
        self.ser.flush()

    def send(self, cmd: int, addr: int, payload: bytes = b"") -> None:
        body = bytes([cmd, addr, len(payload)]) + payload
        self.ser.write(bytes([SYNC]) + body + bytes([crc8(body)]))

    def recv(self, timeout: float):
        """Next CRC-valid frame as (cmd, addr, payload), or None on timeout."""
        deadline = time.time() + timeout
        while True:
            frame = self._parse()
            if frame:
                return frame
            remaining = deadline - time.time()
            if remaining <= 0:
                return None
            self.ser.timeout = min(remaining, 0.005)
            self.rx += self.ser.read(256)

    def _parse(self):
        while True:
            start = self.rx.find(SYNC)
            if start < 0:
                self.rx.clear()
                return None
            del self.rx[:start]
            if len(self.rx) < 4:
                return None
            n = self.rx[3]
            if n > 80:
                del self.rx[0]
                continue
            if len(self.rx) < 5 + n:
                return None
            body = bytes(self.rx[1:4 + n])
            if crc8(body) == self.rx[4 + n]:
                del self.rx[:5 + n]
                return body[0], body[1], body[3:]
            del self.rx[0]

    def request(self, cmd: int, addr: int, reply_cmd: int, payload: bytes = b"",
                timeout: float = 0.03, tries: int = 3):
        """Addressed request; the matching reply payload, or None."""
        for _ in range(tries):
            self.send(cmd, addr, payload)
            deadline = time.time() + timeout
            while (left := deadline - time.time()) > 0:
                frame = self.recv(left)
                if frame and frame[0] == reply_cmd and frame[1] == addr:
                    return frame[2]
        return None


class PanelStatus:
    def __init__(self, payload: bytes):
        self.state = payload[0]
        self.app_sane = bool(payload[1] & 1)
        self.pending = bool(payload[1] & 2)
        self.missing_count = payload[2] | payload[3] << 8
        self.app_len, self.app_crc, self.boot_tag = struct.unpack_from("<III", payload, 4)
        self.missing = list(struct.unpack_from(f"<{(len(payload) - 16) // 2}H", payload, 16))

    def describe(self) -> str:
        app = f"app {self.app_crc:08X} ({self.app_len} B)" if self.app_len else \
              ("app loaded over USB/SWD" if self.app_sane else "NO RUNNABLE APP")
        return f"{ST_NAMES.get(self.state, self.state)}, {app}, bootloader {self.boot_tag:08X}"


class PanelUpdater:
    def __init__(self, bus: PanelBus, image: bytes, panels, log=print):
        self.bus = bus
        self.image = image
        self.crc = zlib.crc32(image)
        self.blocks = (len(image) + BLOCK - 1) // BLOCK
        self.panels = list(panels)
        self.log = log
        self.committed = False

    def query(self, pid: int, tries: int = 3):
        r = self.bus.request(ord("Q"), pid, ord("q"), tries=tries)
        return PanelStatus(r) if r is not None and len(r) >= 16 else None

    def broadcast(self, cmd: str, payload: bytes = b"", times: int = 1, gap: float = 0.02) -> None:
        for _ in range(times):
            self.bus.send(ord(cmd), BCAST, payload)
            time.sleep(gap)

    def send_block(self, i: int, addr: int = BCAST) -> None:
        self.bus.send(ord("D"), addr, struct.pack("<H", i) + self.image[i * BLOCK:(i + 1) * BLOCK])

    def wait_state(self, pids, want, timeout: float, resend=None):
        """Poll until each panel reports a state in `want`. Returns {pid: status}.

        A panel busy erasing or copying does not answer (it discards what
        arrived while busy), so silence here is normal until the timeout.
        """
        done = {}
        deadline = time.time() + timeout
        while len(done) < len(pids) and time.time() < deadline:
            for pid in pids:
                if pid in done:
                    continue
                st = self.query(pid, tries=1)
                if st is None:
                    continue
                if st.state in want:
                    done[pid] = st
                elif st.state in (ST_REFUSED, ST_CRC_FAIL, ST_COPY_FAIL):
                    raise UpdateError(f"panel {pid}: {st.describe()}")
                elif resend:
                    resend(pid, st)
            time.sleep(0.05)
        missing = [p for p in pids if p not in done]
        if missing:
            raise UpdateError(f"no {'/'.join(ST_NAMES[w] for w in want)} from panel(s) {missing}")
        return done

    def run(self, force: bool = False) -> None:
        self.log(f"image: {len(self.image)} bytes, CRC-32 {self.crc:08X}, {self.blocks} blocks")

        self.log("holding every panel in its bootloader ...")
        self.broadcast("B", times=50, gap=0.02)
        time.sleep(0.3)

        present = {}
        for pid in self.panels:
            st = self.query(pid)
            name = PANEL_NAME[pid] if pid < len(PANEL_NAME) else "?"
            if st is None:
                self.log(f"  {name:<2} id {pid}: no reply")
            else:
                present[pid] = st
                self.log(f"  {name:<2} id {pid}: {st.describe()}")
        if not present:
            raise UpdateError("no panel answered — check bus mode / DIP IDs / that the bootloader is installed")
        targets = [p for p, st in present.items() if force or st.app_crc != self.crc]
        if not targets:
            self.log("every panel already runs this image; releasing them")
            self.broadcast("X", times=3)
            return

        self.log(f"updating panel(s) {targets}")
        self.broadcast("U", struct.pack("<II", len(self.image), self.crc))
        self.wait_state(targets, {ST_RECEIVING}, timeout=30,
                        resend=lambda pid, st: self.bus.send(ord("U"), pid,
                                                             struct.pack("<II", len(self.image), self.crc)))

        t0 = time.time()
        for i in range(self.blocks):
            self.send_block(i)
        self.bus.ser.flush()
        time.sleep(0.1)
        self.log(f"streamed {self.blocks} blocks in {time.time() - t0:.1f} s")

        history = {}

        def fill(pid, st):
            if st.state != ST_RECEIVING:
                return
            history.setdefault(pid, []).append(st.missing_count)
            h = history[pid]
            if len(h) > 6 and h[-1] >= h[-6]:
                raise UpdateError(f"panel {pid} stuck at {st.missing_count} missing blocks")
            if st.missing_count > 256:
                self.log(f"  panel {pid}: {st.missing_count} missing, re-streaming everything")
                for i in range(self.blocks):
                    self.send_block(i)
            else:
                self.log(f"  panel {pid}: {st.missing_count} missing, re-sending {len(st.missing)}")
                for i in st.missing:
                    self.send_block(i, pid)
            self.bus.ser.flush()
            time.sleep(0.02)

        self.wait_state(targets, {ST_STAGED}, timeout=60, resend=fill)
        self.log("all staged and CRC-verified; committing")

        # Past this point a failure leaves panels holding in the bootloader,
        # each with either its old app or the new one — never a mix within a
        # panel. They stay reachable and the update can simply be re-run.
        commit = struct.pack("<I", self.crc)
        self.committed = True
        self.broadcast("A", commit)
        done = self.wait_state(targets, {ST_INSTALLED}, timeout=30,
                               resend=lambda pid, st: self.bus.send(ord("A"), pid, commit)
                               if st.state == ST_STAGED else None)
        for pid, st in sorted(done.items()):
            self.log(f"  panel {pid}: installed {st.app_crc:08X}")
        self.broadcast("X", times=3)
        self.log(f"{len(done)} panel(s) now run {self.crc:08X}")


def find_port(arg):
    if arg:
        return arg
    ports = sorted(glob.glob("/dev/cu.usbmodem*"))
    if not ports:
        sys.exit("no /dev/cu.usbmodem* ports found — is the master plugged in?")
    if len(ports) > 1:
        sys.exit(f"several ports found, pass --port: {', '.join(ports)}")
    return ports[0]


def parse_panels(s: str):
    out = set()
    for part in s.split(","):
        a, _, b = part.partition("-")
        out.update(range(int(a), int(b or a) + 1))
    return sorted(out)


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("image", help="application .bin linked at 0x10008000")
    ap.add_argument("--port", help="master's USB serial port")
    ap.add_argument("--panels", default="0-8", help="panel IDs, e.g. 0-8 or 0,3,6 (default 0-8)")
    ap.add_argument("--force", action="store_true", help="update panels that already run this image")
    args = ap.parse_args()

    image = open(args.image, "rb").read()
    try:
        check_image(image)
    except UpdateError as e:
        sys.exit(f"refusing {args.image}: {e}")

    bus = PanelBus(find_port(args.port))
    updater = None
    try:
        bus.enter()
        updater = PanelUpdater(bus, image, parse_panels(args.panels))
        updater.run(force=args.force)
    except (UpdateError, KeyboardInterrupt) as e:
        print(f"\nABORTED: {e or 'interrupted'}")
        if updater and updater.committed:
            # Some panels may hold the new image and some the old. Releasing
            # them now would run a mixed pad, so leave every panel held in its
            # bootloader (D202 blinking) and let the update be re-run.
            print("after commit: panels left holding in the bootloader; re-run to finish")
        else:
            # Nothing installed anywhere: discard staging, restart the old app.
            for _ in range(3):
                bus.send(ord("X"), BCAST)
                time.sleep(0.02)
        sys.exit(1)
    finally:
        try:
            bus.leave()
        except Exception:
            pass


if __name__ == "__main__":
    main()
