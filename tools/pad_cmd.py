#!/usr/bin/env python3
"""Talk to the master's USB vendor interface (docs/USB_PROTOCOL.md).

A bench tool and a reference for the packet framing, until the Rust SDK
exists: every command in the master's command table has a subcommand here.

Every packet is a 5-byte header (kind, transaction ID, flags START 0x01 /
END 0x02, payload length u16) and then the payload. Packets to the host are
64 bytes; packets from the host are 1024 bytes at High Speed and 64 at Full
Speed, read from the HID report descriptor before the first write. A message
longer than one packet spans several, and its byte 0 is its code.

Needs hidapi: `brew install hidapi` and `pip install hid`.

Usage: python3 tools/pad_cmd.py <command> [args]   (-h for the list)

  info                        device info
  config                      the master's config copy, per panel and channel
  set PANEL PRESS REL [CH]    thresholds on one panel (all channels, or CH),
                              calibrated units 0-1000; SAVES to the master's
                              EEPROM, then pushes
  flags PANEL CH POP EN       channel flags (0/1 each); saves, then pushes
  reset-config --yes          defaults for every panel; saves, then pushes
  status                      pad status: presence, poll rate, config state
  stream [SECONDS] [--csv F]  live FSR telemetry (calibrated, tares), then a
                              per-panel rate; --csv logs every record to F
  mode MODE [SECS] [--panels 0,3,...]   panel LEDs: 0 normal, 1 ID, 2 test
  selftest                    slot <-> panel-ID self-test (the console's I)
  zero                        zero bus counters, master and panels
  clock [N]                   clock probe: round trip and offset
  leds R G B [SECONDS]        host LED frames, one colour, at 60 Hz
  bus-h PANEL                 passthrough check: send 'H' to one panel
  events [SECONDS]            print events as they arrive
  bootloader --yes            reboot the master into the Teensy bootloader

Saving stalls the master's interrupts: don't run set/flags/reset-config
during gameplay (docs/PANEL_CONFIG.md).
"""

import argparse
import csv
import queue
import statistics
import struct
import sys
import threading
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
VENDOR_PAGE, VENDOR_USAGE = 0xFF00, 0x0001
PANEL_NAME = ["UL", "U", "UR", "L", "C", "R", "DL", "D", "DR"]

PROTOCOL = 3                   # 3: tares in the FSR stream (2026-10-22)
HEADER = 5
IN_PACKET = 64                 # master -> host, both speeds
SPEED = {0: "Full Speed (12 Mbit/s)", 1: "High Speed (480 Mbit/s)"}
CMD, RESP, EVENT, STREAM = 1, 2, 3, 4
START, END = 0x01, 0x02

DEVICE_INFO, GET_CONFIG, SET_CONFIG, PAD_STATUS = 0x01, 0x02, 0x03, 0x04
STREAM_CTL, RESET_CONFIG, RESET_ANIMATIONS, LED_FRAME = 0x05, 0x06, 0x07, 0x08
UNDERGLOW, LED_MODE, SELF_TEST, ZERO_COUNTERS = 0x09, 0x0A, 0x0B, 0x0C
PASSTHROUGH, BUS_FRAME, REBOOT_BOOTLOADER, CLOCK_PROBE = 0x0D, 0x0E, 0x0F, 0x10
RESP_ERROR = 0x7F
ERRORS = {1: "unknown command", 2: "bad length", 3: "busy / not now", 4: "not supported yet"}
EVT_PRESENCE, EVT_CONFIG_RESULT, EVT_CONFIG_RESET, EVT_BUS_FRAME = 0x81, 0x82, 0x83, 0x84
STREAM_FSR = 0xC2              # records: panel, master us, raw x4, tare x4, mask
STREAM_RECORD = 22
REBOOT_MAGIC = b"BOOT"

CONFIG_VERSION = 2             # 2: thresholds in calibrated units (2026-10-22)
ADC_MAX, CAL_MAX = 4095, 1000


def raw_to_cal(raw, tare):
    """firmware/shared/panel_config.h kp_raw_to_cal: 0 at the tare, 1000 at
    full scale, rounded toward zero. None if the panel sent no tare."""
    if tare > ADC_MAX:
        return None
    return int((raw - tare) * CAL_MAX / (ADC_MAX - tare))
CFG_STATE = ["matches", "sent, waiting", "REJECTED: unknown version",
             "REJECTED: invalid", "not configured", "absent"]
TERMINATION = {0: "OK", 1: "no panel terminated", 2: "several terminated",
               3: "wrong panel terminated", 0xFF: "unknown (no status yet)"}


class PadError(Exception):
    pass


class Pad:
    def __init__(self):
        path = None
        for d in hid.enumerate(VID, PID):
            if (d["product_string"] == PRODUCT and d["usage_page"] == VENDOR_PAGE
                    and d["usage"] == VENDOR_USAGE):
                path = d["path"]
        if path is None:
            sys.exit(f"no {PRODUCT} vendor interface ({VID:04x}:{PID:04x}, usage page "
                     f"{VENDOR_PAGE:04x}). Master firmware built with firmware/master/build.sh?")
        self.dev = hid.Device(path=path)
        self.out_packet = out_report_size(self.dev.get_report_descriptor())
        self.txn = 0
        self.partial = None   # (kind, txn, bytearray) while a message is open
        self.arrival = 0.0    # time.monotonic() when the last packet read arrived
        self._queue = None    # (arrival, packet) from the reader thread
        self._reader = None
        self._reading = False
        self._reader_error = None
        self.drain()

    def drain(self):
        """Whatever the master queued before we opened it is stale. Bounded,
        in case a stream left on by a crashed run is still arriving."""
        t_end = time.monotonic() + 0.2
        while time.monotonic() < t_end and self.dev.read(IN_PACKET, timeout=20):
            pass

    def _write(self, kind, txn, msg):
        payload = self.out_packet - HEADER
        chunks = [msg[i:i + payload] for i in range(0, len(msg), payload)] or [b""]
        for k, chunk in enumerate(chunks):
            flags = (START if k == 0 else 0) | (END if k == len(chunks) - 1 else 0)
            pkt = bytes([kind, txn, flags]) + struct.pack("<H", len(chunk)) + chunk
            # hidapi wants the report ID first; this interface has none, so 0.
            self.dev.write(b"\x00" + pkt.ljust(self.out_packet, b"\x00"))

    # On macOS hidapi keeps 30 input reports per device and drops the oldest
    # past that (hid.c, "Pop one off if we've reached 30"): ~29 ms of slack at
    # poll 0, ~1,020 packets/s. A stream written to CSV on the reading thread
    # lost records whenever that thread stalled for longer (13 times in the
    # 2026-10-24 overnight soak, during Time Machine snapshots and an hourly
    # Contacts sync burst; docs/BRINGUP_LOG.md). A thread that does nothing but
    # read keeps hidapi's queue empty. Start it only between commands: it and
    # command() would otherwise both read the device.
    def start_reader(self):
        self._queue = queue.Queue()
        self._reading = True
        self._reader = threading.Thread(target=self._reader_loop, daemon=True)
        self._reader.start()

    def _reader_loop(self):
        try:
            while self._reading:
                pkt = self.dev.read(IN_PACKET, timeout=50)
                if pkt:
                    self._queue.put((time.monotonic(), pkt))
        except Exception as e:   # handed to the reading side by _read_packet
            self._reader_error = e
            self._reading = False

    def stop_reader(self):
        """Back to reading on the caller's thread; packets already queued are
        read first."""
        self._reading = False
        self._reader.join()
        self._reader = None

    def backlog(self):
        """Packets the reader thread has queued that nobody has read yet."""
        return self._queue.qsize() if self._queue else 0

    def _read_packet(self, timeout_ms):
        if self._queue is not None and not self._queue.empty():
            self.arrival, pkt = self._queue.get_nowait()
            return pkt
        if self._reader_error:
            raise self._reader_error
        if self._reader:
            try:
                self.arrival, pkt = self._queue.get(timeout=timeout_ms / 1000)
                return pkt
            except queue.Empty:
                return None
        pkt = self.dev.read(IN_PACKET, timeout=timeout_ms)
        self.arrival = time.monotonic()
        return pkt

    def read_message(self, timeout_ms):
        """One whole message as (kind, txn, bytes), or None on timeout."""
        deadline = time.monotonic() + timeout_ms / 1000
        while True:
            left = int((deadline - time.monotonic()) * 1000)
            if left <= 0:
                return None
            pkt = self._read_packet(left)
            if not pkt:
                return None
            kind, txn, flags, n = pkt[0], pkt[1], pkt[2], u16(pkt, 3)
            data = bytes(pkt[HEADER:HEADER + n])
            if flags & START:
                self.partial = (kind, txn, bytearray())
            if self.partial is None or self.partial[:2] != (kind, txn):
                continue
            self.partial[2].extend(data)
            if flags & END:
                kind, txn, buf = self.partial
                self.partial = None
                return kind, txn, bytes(buf)

    def command(self, code, payload=b"", timeout_ms=2000, on_other=None):
        """Send one command and return its response message. Events and stream
        packets that arrive meanwhile go to on_other (or are printed)."""
        self.txn = self.txn % 255 + 1
        self._write(CMD, self.txn, bytes([code]) + payload)
        deadline = time.monotonic() + timeout_ms / 1000
        while time.monotonic() < deadline:
            m = self.read_message(int((deadline - time.monotonic()) * 1000) + 1)
            if m is None:
                break
            kind, txn, msg = m
            if kind == RESP and txn == self.txn:
                if msg[0] == RESP_ERROR:
                    raise PadError(f"command {msg[1]:#04x}: {ERRORS.get(msg[2], msg[2])}")
                return msg
            (on_other or print_other)(kind, msg)
        raise PadError(f"no response to command {code:#04x} within {timeout_ms} ms")

    def send(self, code, payload=b""):
        """Fire-and-forget (transaction 0): frames only."""
        self._write(CMD, 0, bytes([code]) + payload)


def out_report_size(desc):
    """Bytes in the interface's output report, from its HID report descriptor:
    the report count and size in force at the Output item. Walks short items
    only (the descriptor has no long ones)."""
    i = count = size = 0
    while i < len(desc):
        prefix = desc[i]
        n = (0, 1, 2, 4)[prefix & 3]
        value = int.from_bytes(bytes(desc[i + 1:i + 1 + n]), "little")
        tag = prefix & 0xFC
        if tag == 0x94:
            count = value
        elif tag == 0x74:
            size = value
        elif tag == 0x90:
            return count * size // 8
        i += 1 + n
    sys.exit("no output report in the vendor interface's report descriptor")


def u16(b, o):
    return struct.unpack_from("<H", b, o)[0]


def u32(b, o):
    return struct.unpack_from("<I", b, o)[0]


def describe_event(msg):
    code = msg[0]
    if code == EVT_PRESENCE:
        return f"panel {msg[1]} ({PANEL_NAME[msg[1]]}) {'present' if msg[2] else 'GONE'}"
    if code == EVT_CONFIG_RESULT:
        state = CFG_STATE[msg[2]] if msg[2] < len(CFG_STATE) else msg[2]
        return f"panel {msg[1]} ({PANEL_NAME[msg[1]]}) config: {state}, CRC {u32(msg, 3):08X}"
    if code == EVT_CONFIG_RESET:
        return "the master found no valid saved config at boot: on defaults"
    if code == EVT_BUS_FRAME:
        return f"bus frame {chr(msg[1])!r} from {msg[2]}: {msg[3:].hex(' ')}"
    return f"event {code:#04x}: {msg[1:].hex(' ')}"


def print_other(kind, msg):
    if kind == EVENT:
        print(f"  [event] {describe_event(msg)}")


def parse_config(block):
    return [[struct.unpack_from("<HHB", block, p * 20 + c * 5) for c in range(4)]
            for p in range(9)]


def build_config(panels):
    return b"".join(struct.pack("<HHB", *ch) for panel in panels for ch in panel)


def get_config(pad):
    m = pad.command(GET_CONFIG)
    if m[1] != CONFIG_VERSION:
        raise PadError(f"master speaks config version {m[1]}, this tool {CONFIG_VERSION}")
    return parse_config(m[2:])


def set_config(pad, panels):
    results = {}

    def collect(kind, msg):
        if kind == EVENT and msg[0] == EVT_CONFIG_RESULT:
            results[msg[1]] = msg[2]
        print_other(kind, msg)

    m = pad.command(SET_CONFIG, bytes([CONFIG_VERSION]) + build_config(panels), on_other=collect)
    if m[1] == 1:
        raise PadError("master rejected the config version")
    if m[1] == 2:
        raise PadError(f"invalid value: panel {m[2]} channel {m[3]}")
    print("saved" if m[1] == 0 else "!! pushed, but the SAVE FAILED (EEPROM read-back)")
    wait_config_results(pad, results)


def wait_config_results(pad, results, seconds=2.0):
    """Config result events until every answering panel has settled."""
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        m = pad.read_message(100)
        if m and m[0] == EVENT:
            print(f"  [event] {describe_event(m[2])}")
            if m[2][0] == EVT_CONFIG_RESULT:
                results[m[2][1]] = m[2][2]
    st = pad.command(PAD_STATUS)
    for p in range(9):
        o = 1 + p * 12
        if st[o]:
            print(f"  {PANEL_NAME[p]:>2} id {p}: {CFG_STATE[st[o + 6]]}")


def cmd_info(pad, a):
    m = pad.command(DEVICE_INFO)
    if m[1] != PROTOCOL:
        sys.exit(f"master speaks protocol {m[1]}, this tool {PROTOCOL}: update one of them")
    flags = m[16]
    present = u16(m, 17)
    print(f"protocol {m[1]}, USB {SPEED.get(m[2], m[2])}")
    print(f"packets: to host {u16(m, 3)} bytes, from host {u16(m, 5)} bytes"
          f" (report descriptor says {pad.out_packet})")
    print(f"firmware {m[7]}.{m[8]}.{m[9]}, build {u32(m, 10):08X}")
    print(f"config version {m[14]}, player ID {m[15]} (P{m[15] + 1})")
    print("flags:", ", ".join(n for b, n in [(1, "config was reset at boot"), (2, "stream on"),
                                             (4, "passthrough"), (8, "unsaved console edits")]
                              if flags & b) or "none")
    print("panels answering:", " ".join(PANEL_NAME[p] for p in range(9) if present >> p & 1) or "none")
    term = TERMINATION.get(m[19], m[19])
    print(f"termination: {term}" + (f" (panel {m[20]})" if m[20] != 0xFF else ""))


def cmd_config(pad, a):
    for p, panel in enumerate(get_config(pad)):
        chans = "  ".join(f"{c}:{pr:4}/{rl:4}{'' if fl & 1 else ' p'}{'' if fl & 2 else ' d'}"
                          for c, (pr, rl, fl) in enumerate(panel))
        print(f"  {PANEL_NAME[p]:>2} id {p}  {chans}")
    print("  press/release per channel 0-3 ('f' order), calibrated 0-1000; "
          "p = not populated, d = disabled")


def cmd_set(pad, a):
    panels = get_config(pad)
    for c in range(4):
        if a.ch is None or a.ch == c:
            panels[a.panel][c] = (a.press, a.rel, panels[a.panel][c][2])
    set_config(pad, panels)


def cmd_flags(pad, a):
    panels = get_config(pad)
    pr, rl, _ = panels[a.panel][a.ch]
    panels[a.panel][a.ch] = (pr, rl, (1 if a.pop else 0) | (2 if a.en else 0))
    set_config(pad, panels)


def cmd_reset_config(pad, a):
    if not a.yes:
        sys.exit("reset-config overwrites every panel's saved config: add --yes")
    m = pad.command(RESET_CONFIG)
    print("defaults saved" if m[1] == 0 else "!! defaults pushed, but the SAVE FAILED")
    wait_config_results(pad, {})


def cmd_status(pad, a):
    m = pad.command(PAD_STATUS)
    print("        present  age ms  polls  panel CRC errs  config            build     term")
    for p in range(9):
        o = 1 + p * 12
        age = u16(m, o + 1)
        term = {0: "off", 1: "ON", 0xFF: "?"}.get(m[o + 11], m[o + 11])
        print(f"  {PANEL_NAME[p]:>2} {p}  {'yes' if m[o] else 'no':>7}  "
              f"{'never' if age == 0xFFFF else age:>6}  {m[o + 3]:>4}%  {u16(m, o + 4):>14}  "
              f"{CFG_STATE[m[o + 6]]:<16}  {u32(m, o + 7):08X}  {term}")


def cmd_stream(pad, a):
    counts = [0] * 9
    last = [None] * 9
    # Summed step by step: master_us is a u32 of µs and wraps every 71.6 min, so
    # last - first under-counts any log longer than that (2026-10-23: 45.5 Hz
    # printed for a 12.7 Hz, 99-minute session).
    elapsed = [0] * 9
    # A step between a panel's records over twice its mean so far is a gap:
    # records that never reached us, whether the master didn't poll (a console
    # `H` sweep takes poll slots: ~2 cycles) or the host lost them.
    # (host_s, panel, master step us, host gap s, mean step us)
    gaps = []
    last_arrival = [None] * 9
    # Reader backlog of 30 packets or more: hidapi alone would have dropped
    # records there. [host_s, peak] per episode.
    backlogs = []
    in_backlog = False
    peak_backlog = 0
    latest = {}
    log = None
    if a.csv:
        log_file = open(a.csv, "w", newline="")
        log = csv.writer(log_file)
        log.writerow(["host_s", "panel", "master_us", "raw0", "raw1", "raw2", "raw3",
                      "tare0", "tare1", "tare2", "tare3", "mask"])
    t0 = time.monotonic()
    t0_wall = time.time()

    def on_other(kind, msg):
        if kind == STREAM and msg[0] == STREAM_FSR:
            host_s = pad.arrival - t0   # when it reached us, not when it was read
            for r in range((len(msg) - 1) // STREAM_RECORD):
                rec = msg[1 + r * STREAM_RECORD: 1 + (r + 1) * STREAM_RECORD]
                p, t = rec[0], u32(rec, 1)
                raw = struct.unpack_from("<4H", rec, 5)
                tare = struct.unpack_from("<4H", rec, 13)
                counts[p] += 1
                if last[p] is not None:
                    step = (t - last[p]) & 0xFFFFFFFF
                    if counts[p] > 50 and step > 2 * elapsed[p] / (counts[p] - 2):
                        gaps.append((host_s, p, step, host_s - last_arrival[p],
                                     elapsed[p] / (counts[p] - 2)))
                    elapsed[p] += step
                last[p] = t
                last_arrival[p] = host_s
                latest[p] = (raw, tare, rec[21])
                if log:
                    log.writerow([f"{host_s:.4f}", p, t, *raw, *tare, rec[21]])
        else:
            print_other(kind, msg)

    def fmt(raw, tare):
        cal = [raw_to_cal(r, t) for r, t in zip(raw, tare)]
        return ",".join("-" if c is None else str(c) for c in cal)

    pad.command(STREAM_CTL, bytes([1, 0]))
    pad.start_reader()
    t_end = time.monotonic() + a.seconds
    t_print = 0
    try:
        while time.monotonic() < t_end:
            m = pad.read_message(50)
            if m:
                on_other(m[0], m[2])
            b = pad.backlog()
            peak_backlog = max(peak_backlog, b)
            if b >= 30:
                if not in_backlog:
                    backlogs.append([pad.arrival - t0, b])
                    in_backlog = True
                backlogs[-1][1] = max(backlogs[-1][1], b)
            elif b < 5:
                in_backlog = False
            if time.monotonic() - t_print > 0.5 and latest:
                t_print = time.monotonic()
                print("  " + "  ".join(f"{PANEL_NAME[p]}:{fmt(raw, tare)}"
                                       f"{'*' if mask & 0x0F else ''}{'!' if mask & 0x80 else ''}"
                                       for p, (raw, tare, mask) in sorted(latest.items())))
    except KeyboardInterrupt:
        pass   # Ctrl-C ends a long log early; still stop the stream and summarise
    finally:
        pad.stop_reader()
        pad.command(STREAM_CTL, bytes([0, 0]), on_other=on_other)
        if log:
            log_file.close()
    print("calibrated values above (0 = the tare, 1000 = full scale); tares now:")
    for p, (raw, tare, mask) in sorted(latest.items()):
        print(f"  {PANEL_NAME[p]:>2} raw {','.join(map(str, raw))}  tare {','.join(map(str, tare))}")
    print("per panel: records, rate by master timestamps, gaps (* pressed, ! not configured)")
    for p in range(9):
        n_gaps = sum(1 for g in gaps if g[1] == p)
        if counts[p] > 1:
            print(f"  {PANEL_NAME[p]:>2} {counts[p]:5}  {(counts[p] - 1) / (elapsed[p] / 1e6):6.1f} Hz"
                  f"  {n_gaps} gaps")
        elif counts[p]:
            print(f"  {PANEL_NAME[p]:>2} {counts[p]:5}")
    print_gaps(gaps, t0_wall)
    print(f"started {time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(t0_wall))} "
          f"(epoch {t0_wall:.3f}; host_s in the CSV counts from here)")
    print(f"reader backlog: peak {peak_backlog} packets; {len(backlogs)} times at 30 or "
          f"more, where hidapi alone would have dropped records")
    for host_s, peak in backlogs[:40]:
        print(f"  {time.strftime('%H:%M:%S', time.localtime(t0_wall + host_s))}  "
              f"host_s {host_s:10.3f}  peak {peak}")
    if len(backlogs) > 40:
        print(f"  ... {len(backlogs) - 40} more")


def print_gaps(gaps, t0_wall):
    """Gaps across panels within 0.5 s of each other are one event. Records
    missing: each gap's step over its panel's mean, less the one that arrived."""
    events = []
    for g in sorted(gaps):
        if events and g[0] - events[-1][0][0] < 0.5:
            events[-1].append(g)
        else:
            events.append([g])
    missing = sum(round(g[2] / g[4]) - 1 for g in gaps)
    print(f"gaps over twice a panel's mean step: {len(gaps)} in {len(events)} events, "
          f"~{missing} records missing in all")
    if events:
        print("  (a console `H` sweep shows as a short gap on every panel; host gap well over "
              "the master step = records lost on the host)")
    for ev in events[:40]:
        print(f"  {time.strftime('%H:%M:%S', time.localtime(t0_wall + ev[0][0]))}  "
              f"host_s {ev[0][0]:10.3f}  {len({g[1] for g in ev})} panels  "
              f"master step up to {max(g[2] for g in ev) / 1000:6.1f} ms  "
              f"host gap up to {max(g[3] for g in ev) * 1000:6.1f} ms")
    if len(events) > 40:
        print(f"  ... {len(events) - 40} more")


def cmd_mode(pad, a):
    mask = 0x1FF if a.panels is None else sum(1 << int(p) for p in a.panels.split(","))
    m = pad.command(LED_MODE, struct.pack("<HBB", mask, a.mode, a.secs))
    print("queued" if m[1] == 0 else "rejected (mode or panel mask)")


def cmd_selftest(pad, a):
    m = pad.command(SELF_TEST, timeout_ms=5000)
    if m[1]:
        low = u16(m, 2)
        print("aborted: INT already LOW on", " ".join(PANEL_NAME[s] for s in range(9) if low >> s & 1))
        return
    acked = u16(m, 4)
    for s in range(9):
        pid, ack = m[6 + 2 * s], m[7 + 2 * s]
        who = "nothing" if pid == 0xFF else f"panel {pid}" + ("" if ack else " (no 'i' ack)")
        flag = "" if pid == s else "   <-- MISMATCH" if pid != 0xFF else ""
        print(f"  slot {PANEL_NAME[s]:>2}: {who}{flag}")
    silent = [p for p in range(9) if acked >> p & 1 and p not in m[6::2][:9]]
    if silent:
        print("  acked but no INT edge:", " ".join(map(str, silent)))


def cmd_zero(pad, a):
    pad.command(ZERO_COUNTERS)
    print("queued: zeroes in the next poll slot")


def cmd_clock(pad, a):
    rtts, offsets, turns = [], [], []
    for _ in range(a.n):
        t0 = time.perf_counter_ns()
        m = pad.command(CLOCK_PROBE, struct.pack("<Q", t0))
        t1 = time.perf_counter_ns()
        if struct.unpack_from("<Q", m, 1)[0] != t0:
            raise PadError("clock probe echoed a different host time")
        recv_us, send_us = u32(m, 9), u32(m, 13)
        rtt = (t1 - t0) / 1000
        rtts.append(rtt)
        turns.append((send_us - recv_us) & 0xFFFFFFFF)
        offsets.append((t0 / 1000 + rtt / 2) - (recv_us + send_us) / 2)
    best = min(range(a.n), key=lambda i: rtts[i])
    print(f"round trip: min {min(rtts):.0f} us, median {statistics.median(rtts):.0f} us, "
          f"max {max(rtts):.0f} us over {a.n}")
    print(f"master receive -> send: median {statistics.median(turns):.0f} us")
    print(f"host - master clock at the fastest probe: {offsets[best]:.0f} us "
          f"(arbitrary epochs: only its drift between runs means anything)")


def sleep_until(t):
    """time.sleep() oversleeps on macOS by ~20% of the request (measured: 16.7 ms
    asked, 19.9 ms median), so sleep to 2 ms short and spin the rest."""
    left = t - time.perf_counter()
    if left > 0.002:
        time.sleep(left - 0.002)
    while time.perf_counter() < t:
        pass


def cmd_leds(pad, a):
    frame = bytes([a.r, a.g, a.b]) * (25 * 9)
    period = 1 / 60
    t0 = time.perf_counter()
    n = 0
    # Each frame is due at t0 + n * period, so the time spent writing it (~4-10
    # ms through hidapi) and any oversleep come out of the next wait.
    while n * period < a.seconds:
        sleep_until(t0 + n * period)
        pad.send(LED_FRAME, frame)
        n += 1
    elapsed = time.perf_counter() - t0
    print(f"sent {n} frames in {elapsed:.2f} s ({n / elapsed:.1f}/s); "
          "the master's own pattern returns 1 s after the last")


def cmd_bus_h(pad, a):
    pad.command(PASSTHROUGH, b"\x01")
    try:
        pad.command(BUS_FRAME, bytes([ord("H"), a.panel]))
        t_end = time.monotonic() + 0.5
        while time.monotonic() < t_end:
            m = pad.read_message(50)
            if m and m[0] == EVENT and m[2][0] == EVT_BUS_FRAME:
                cmd, addr, pay = chr(m[2][1]), m[2][2], m[2][3:]
                print(f"  bus: {cmd!r} from {addr}, {len(pay)} bytes"
                      + (f", build {u32(pay, 0):08X}, up {u32(pay, 4)} s" if cmd == "h" else ""))
                if cmd == "h":
                    break
        else:
            print("  no 'h' reply")
    finally:
        pad.command(PASSTHROUGH, b"\x00")


def cmd_events(pad, a):
    pad.command(DEVICE_INFO)   # events start with the host's first command
    t_end = time.monotonic() + a.seconds
    while time.monotonic() < t_end:
        m = pad.read_message(100)
        if m and m[0] == EVENT:
            print(f"{time.strftime('%H:%M:%S')}  {describe_event(m[2])}")


def cmd_bootloader(pad, a):
    if not a.yes:
        sys.exit("bootloader reboots the master into HalfKay (no pad until reflashed or reset): add --yes")
    m = pad.command(REBOOT_BOOTLOADER, REBOOT_MAGIC)
    print("rebooting into the Teensy bootloader" if m[1] == 0 else "refused (magic)")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("info").set_defaults(fn=cmd_info)
    sub.add_parser("config").set_defaults(fn=cmd_config)
    s = sub.add_parser("set")
    s.add_argument("panel", type=int, choices=range(9))
    s.add_argument("press", type=int)
    s.add_argument("rel", type=int)
    s.add_argument("ch", type=int, nargs="?", choices=range(4))
    s.set_defaults(fn=cmd_set)
    s = sub.add_parser("flags")
    s.add_argument("panel", type=int, choices=range(9))
    s.add_argument("ch", type=int, choices=range(4))
    s.add_argument("pop", type=int, choices=(0, 1))
    s.add_argument("en", type=int, choices=(0, 1))
    s.set_defaults(fn=cmd_flags)
    s = sub.add_parser("reset-config")
    s.add_argument("--yes", action="store_true")
    s.set_defaults(fn=cmd_reset_config)
    sub.add_parser("status").set_defaults(fn=cmd_status)
    s = sub.add_parser("stream")
    s.add_argument("seconds", type=float, nargs="?", default=5)
    s.add_argument("--csv", help="log every record to this CSV file")
    s.set_defaults(fn=cmd_stream)
    s = sub.add_parser("mode")
    s.add_argument("mode", type=int, choices=(0, 1, 2))
    s.add_argument("secs", type=int, nargs="?", default=0)
    s.add_argument("--panels")
    s.set_defaults(fn=cmd_mode)
    sub.add_parser("selftest").set_defaults(fn=cmd_selftest)
    sub.add_parser("zero").set_defaults(fn=cmd_zero)
    s = sub.add_parser("clock")
    s.add_argument("n", type=int, nargs="?", default=50)
    s.set_defaults(fn=cmd_clock)
    s = sub.add_parser("leds")
    for c in "rgb":
        s.add_argument(c, type=int)
    s.add_argument("seconds", type=float, nargs="?", default=3)
    s.set_defaults(fn=cmd_leds)
    s = sub.add_parser("bus-h")
    s.add_argument("panel", type=int, choices=range(9))
    s.set_defaults(fn=cmd_bus_h)
    s = sub.add_parser("events")
    s.add_argument("seconds", type=float, nargs="?", default=30)
    s.set_defaults(fn=cmd_events)
    s = sub.add_parser("bootloader")
    s.add_argument("--yes", action="store_true")
    s.set_defaults(fn=cmd_bootloader)
    a = ap.parse_args()

    pad = Pad()
    try:
        a.fn(pad, a)
    except PadError as e:
        sys.exit(f"error: {e}")


if __name__ == "__main__":
    main()
