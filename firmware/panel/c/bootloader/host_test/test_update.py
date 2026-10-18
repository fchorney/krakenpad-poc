#!/usr/bin/env python3
"""End-to-end test of a bus update, on the host, with no hardware.

tools/panel_update.py talks to a fake master on a pseudo-terminal. The fake
master implements passthrough (docs/FIELD_UPDATE.md) and puts every frame on a
shared "bus": the stdin of each simulated panel, which is the real
../main.c compiled for the host by panel_sim.c. It drops a share of the 'D'
frames on purpose, so gap-fill has to work.

Scenarios:
  1. three blank panels held in the bootloader, one ID absent -> all updated
  2. same image again -> nothing to do, panels released
  3. a standalone image (linked at 0x10000000) -> tool refuses it
  4. power cut mid-install (record pending, app region garbage) -> the next
     boot redoes the copy and runs the app without any bus traffic
  5. --panels 1 with a new image, all three held -> only panel 1 changes

Usage: python3 test_update.py APP.bin STANDALONE.bin
  APP.bin         bring-up build, linked at 0x10008000 (the default)
  STANDALONE.bin  bring-up build with -DPANEL_UNDER_BOOTLOADER=OFF
"""

import os
import pty
import random
import select
import struct
import subprocess
import sys
import tempfile
import time
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "../../../../.."))
TOOL = os.path.join(REPO, "tools", "panel_update.py")

APP_OFFSET, APP_SIZE = 0x8000, 480 * 1024
STAGE_OFFSET, CTRL_OFFSET = 0x180000, 0x7000
CTRL_MAGIC = 0x46574354
FLASH = 4 * 1024 * 1024


def crc8(data):
    crc = 0
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = ((crc << 1) ^ 0x07) & 0xFF if crc & 0x80 else (crc << 1) & 0xFF
    return crc


def build_sim(out):
    subprocess.check_call(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-O1",
                           "-D_DEFAULT_SOURCE", "-I", os.path.join(HERE, "stubs"),
                           "-I", os.path.join(HERE, "../../common"),
                           "-o", out, os.path.join(HERE, "panel_sim.c"), "-lpthread",
                           "-Wl,-w"])


class FakeMaster:
    """Passthrough master + bus. Runs until the tool leaves passthrough."""

    def __init__(self, sim, flash_dir, panel_ids, hold=True, drop_rate=0.0, seed=1):
        self.master_fd, slave_fd = pty.openpty()
        self.port = os.ttyname(slave_fd)
        self.slave_fd = slave_fd
        self.rng = random.Random(seed)
        self.drop_rate = drop_rate
        self.dropped = 0
        self.unsolicited = []
        self.ran_app = set()
        self.panels = {}
        for pid in panel_ids:
            p = subprocess.Popen([sim, str(pid), os.path.join(flash_dir, f"p{pid}.bin")] +
                                 (["hold"] if hold else []),
                                 stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                 stderr=subprocess.PIPE)
            os.set_blocking(p.stdout.fileno(), False)
            self.panels[pid] = p
        self.last_addressed = None

    def _to_bus(self, frame, cmd, addr):
        if cmd == ord("D") and self.rng.random() < self.drop_rate:
            self.dropped += 1
            return
        self.last_addressed = addr if addr != 0xFF else None
        for pid, p in self.panels.items():
            if p.poll() is None:
                try:
                    p.stdin.write(frame)
                    p.stdin.flush()
                except BrokenPipeError:
                    pass

    def run(self, timeout=120):
        text, rx, binary = b"", bytearray(), False
        deadline = time.time() + timeout
        while time.time() < deadline:
            fds = [self.master_fd] + [p.stdout.fileno() for p in self.panels.values()
                                      if p.poll() is None]
            r, _, _ = select.select(fds, [], [], 0.05)
            for pid, p in self.panels.items():
                if p.poll() is not None and pid not in self.ran_app:
                    err = p.stderr.read().decode()
                    if "RUN_APP" in err:
                        self.ran_app.add(pid)
                    elif p.returncode not in (0, None):
                        raise RuntimeError(f"panel {pid} crashed ({p.returncode}): {err}")
            if self.master_fd in r:
                try:
                    data = os.read(self.master_fd, 65536)
                except OSError:
                    return
                if not binary:
                    text += data
                    if b"P\n" in text:
                        binary = True
                        rx += text.split(b"P\n", 1)[1]
                        os.write(self.master_fd, b"P\r\n# passthrough ON: binary frames\r\n")
                else:
                    rx += data
                while binary and len(rx) >= 5:
                    if rx[0] != 0x55:
                        del rx[0]
                        continue
                    n = rx[3]
                    if len(rx) < 5 + n:
                        break
                    frame = bytes(rx[:5 + n])
                    del rx[:5 + n]
                    if crc8(frame[1:4 + n]) != frame[4 + n]:
                        continue
                    cmd, addr = frame[1], frame[2]
                    if cmd == 0 and addr == 0:
                        os.write(self.master_fd, b"\r\n# passthrough OFF (host)\r\n")
                        return
                    if cmd == 0:
                        continue
                    self._to_bus(frame, cmd, addr)
            for pid, p in self.panels.items():
                if p.poll() is None and p.stdout.fileno() in r:
                    out = p.stdout.read()
                    if out:
                        # A panel may only speak when addressed. Anything else
                        # would collide on a real half-duplex bus.
                        if self.last_addressed != pid:
                            self.unsolicited.append((pid, out[:8].hex()))
                        os.write(self.master_fd, out)
        raise RuntimeError("fake master timed out")

    def stop(self):
        for p in self.panels.values():
            if p.poll() is None:
                p.stdin.close()
                p.wait(timeout=5)
            if p.returncode == 0 and b"RUN_APP" in (p.stderr.read() or b""):
                pass


def run_tool(port, image_path, *extra):
    return subprocess.Popen([sys.executable, TOOL, image_path, "--port", port, *extra],
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)


def flash_crc(path, offset, length):
    data = open(path, "rb").read()
    return zlib.crc32(data[offset:offset + length])


def read_ctrl(path):
    data = open(path, "rb").read()[CTRL_OFFSET:CTRL_OFFSET + 28]
    fields = struct.unpack("<7I", data)
    ok = fields[0] == CTRL_MAGIC and fields[6] == zlib.crc32(data[:24])
    return ok, fields


def check(cond, msg):
    print(("  PASS  " if cond else "  FAIL  ") + msg)
    if not cond:
        global failures
        failures += 1


failures = 0


def scenario_update(sim, tmp, image_path, image, label, expect_update, drop_rate):
    print(f"\n{label}")
    fm = FakeMaster(sim, tmp, [0, 1, 2], hold=True, drop_rate=drop_rate)
    tool = run_tool(fm.port, image_path, "--panels", "0-3")
    fm.run()
    out = tool.communicate(timeout=30)[0].decode()
    time.sleep(0.3)
    fm.stop()
    print("    " + out.strip().replace("\n", "\n    "))
    check(tool.returncode == 0, f"tool exit code {tool.returncode}")
    check(fm.unsolicited == [], f"no unsolicited replies on the bus {fm.unsolicited[:3]}")
    check(fm.ran_app == {0, 1, 2}, f"all three panels started the app ({sorted(fm.ran_app)})")
    crc = zlib.crc32(image)
    for pid in (0, 1, 2):
        path = os.path.join(tmp, f"p{pid}.bin")
        check(flash_crc(path, APP_OFFSET, len(image)) == crc, f"panel {pid} app region holds the image")
        ok, f = read_ctrl(path)
        check(ok and f[1] == 0 and f[4] == len(image) and f[5] == crc,
              f"panel {pid} boot record: installed, not pending")
    if expect_update:
        check(fm.dropped > 0, f"{fm.dropped} D frames dropped, recovered by gap-fill")
        check("re-sending" in out or "re-streaming" in out, "gap-fill ran")
    else:
        check("already runs this image" in out, "tool saw nothing to do")


def scenario_subset(sim, tmp, image):
    # Panels 0-2 already run `image` from scenarios 1-2, and it is still in
    # their staging slots. A different image sent with --panels 1 must land on
    # panel 1 only, though 'B' holds all three.
    #
    # The app-region checks alone cannot catch a broadcast 'U': the simulator
    # loses blocks, the tool gap-fills only its targets, so a non-target never
    # finishes staging here, though on hardware (no losses) it does and
    # installs. Its staging slot is the deterministic tell: 'U' erases it.
    print("\n5. --panels 1 updates panel 1 and nobody else")
    other = image + b"\x00" * 64
    other_path = os.path.join(tmp, "other.bin")
    open(other_path, "wb").write(other)
    fm = FakeMaster(sim, tmp, [0, 1, 2], hold=True)
    tool = run_tool(fm.port, other_path, "--panels", "1")
    fm.run()
    out = tool.communicate(timeout=30)[0].decode()
    time.sleep(0.3)
    fm.stop()
    print("    " + out.strip().replace("\n", "\n    "))
    check(tool.returncode == 0, f"tool exit code {tool.returncode}")
    check(fm.unsolicited == [], f"no unsolicited replies on the bus {fm.unsolicited[:3]}")
    check(fm.ran_app == {0, 1, 2}, f"all three panels started the app ({sorted(fm.ran_app)})")
    for pid, want in ((0, image), (1, other), (2, image)):
        path = os.path.join(tmp, f"p{pid}.bin")
        ok, f = read_ctrl(path)
        tag = "new" if want is other else "old"
        check(flash_crc(path, APP_OFFSET, len(want)) == zlib.crc32(want),
              f"panel {pid} app region holds the {tag} image")
        check(ok and f[1] == 0 and f[4] == len(want) and f[5] == zlib.crc32(want),
              f"panel {pid} boot record: {tag} image")
        if want is image:
            check(flash_crc(path, STAGE_OFFSET, len(image)) == zlib.crc32(image),
                  f"panel {pid} staging slot untouched")


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    app_path, standalone_path = sys.argv[1], sys.argv[2]
    image = open(app_path, "rb").read()
    with tempfile.TemporaryDirectory() as tmp:
        sim = os.path.join(tmp, "panel_sim")
        build_sim(sim)

        scenario_update(sim, tmp, app_path, image,
                        "1. blank panels 0-2 held, ID 3 absent, 3% of D frames dropped",
                        expect_update=True, drop_rate=0.03)
        scenario_update(sim, tmp, app_path, image,
                        "2. same image again", expect_update=False, drop_rate=0.0)
        scenario_subset(sim, tmp, image)

        print("\n3. standalone image is refused")
        r = subprocess.run([sys.executable, TOOL, standalone_path, "--port", "/dev/null"],
                           capture_output=True, text=True)
        check(r.returncode != 0 and "linked at 0x10000000" in r.stderr,
              f"refused: {r.stderr.strip()}")

        print("\n4. power cut mid-install resumes at boot")
        path = os.path.join(tmp, "p5.bin")
        flash = bytearray(b"\xff" * FLASH)
        flash[STAGE_OFFSET:STAGE_OFFSET + len(image)] = image
        flash[APP_OFFSET:APP_OFFSET + 8192] = b"\x00" * 8192       # half-erased, half-written
        crc = zlib.crc32(image)
        rec = struct.pack("<6I", CTRL_MAGIC, 1, len(image), crc, 0, 0)
        flash[CTRL_OFFSET:CTRL_OFFSET + 28] = rec + struct.pack("<I", zlib.crc32(rec))
        open(path, "wb").write(flash)
        p = subprocess.Popen([sim, "5", path], stdin=subprocess.PIPE, stderr=subprocess.PIPE)
        err = p.communicate(timeout=10)[1].decode() if False else None
        try:
            p.wait(timeout=5)
        except subprocess.TimeoutExpired:
            p.kill()
        err = p.stderr.read().decode()
        check("RUN_APP" in err, "booted the app after the 200 ms window, no bus needed")
        check(flash_crc(path, APP_OFFSET, len(image)) == crc, "app region now holds the staged image")
        ok, f = read_ctrl(path)
        check(ok and f[1] == 0 and f[5] == crc, "record no longer pending")

    print(f"\n{'ALL PASS' if failures == 0 else f'{failures} FAILURE(S)'}")
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
