#!/usr/bin/env python3
"""Derive the purchasable-part census straight from the KiCad boards.

`docs/BOM.md` is written from this output, so the BOM cannot silently drift away
from the design.  Re-run it after any part change and reconcile the "need"
column against the LCSC cart before ordering.

    /Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/\
Versions/Current/bin/python3 tools/bom_census.py

Needs KiCad's bundled Python (the `pcbnew` module); the system python3 has no
pcbnew.  Reads the .kicad_pcb files, so it reflects what is actually on the
boards, DNP flags and all.

Refs are classified by the footprint's own attributes:
  * through-hole  -> hand-soldered, must be bought
  * SMD on panel  -> JLCPCB PCBA places it, never reaches the bench
  * SMD on master -> hand-soldered too (master is bare-fab + hand assembly)
  * DNP           -> never ordered, footprint exists for a rescue option
TP*/H*/FSR* refs are skipped: probe holes, mounting holes, and the
reference-only FSR symbol are not purchasable lines.

The panel is `hardware/dual-panel`, whose single .kicad_pcb carries BOTH board
outlines (carrier + brain) — they ship on one fab panel as one PCBA order, so
one census over the whole file is the correct unit.  Refdes blocks name the
side: 2xx = carrier, 3xx = brain (see docs/DUAL_PANEL.md).  That split is shown
because it decides which board a hand-soldered THT part lands on.
"""

import collections
import os
import re
import sys

import pcbnew

BOARDS = [("panel", "hardware/dual-panel/dual-panel.kicad_pcb", 20),
          ("master", "hardware/master-pcb/master-pcb.kicad_pcb", 2)]
SKIP_REF = re.compile(r"^(TP|H|FSR)\d|^#")


def side(ref):
    """carrier/brain for dual-panel's 2xx/3xx blocks; '' for single-block boards."""
    m = re.match(r"[A-Za-z]+(\d+)$", ref)
    if not m:
        return ""
    return {2: "carrier", 3: "brain"}.get(int(m.group(1)) // 100, "")


def field(fp, name):
    try:
        return fp.GetFieldText(name)
    except Exception:
        return ""


def census(path):
    board = pcbnew.LoadBoard(path)
    rows = []
    for fp in board.Footprints():
        ref = fp.GetReference()
        if SKIP_REF.match(ref):
            continue
        try:
            dnp = fp.IsDNP()
        except Exception:
            dnp = False
        attrs = fp.GetAttributes()
        rows.append(dict(ref=ref, value=fp.GetValue(),
                         lcsc=field(fp, "LCSC"), mpn=field(fp, "MPN"), dnp=dnp,
                         tht=bool(attrs & pcbnew.FP_THROUGH_HOLE),
                         smd=bool(attrs & pcbnew.FP_SMD)))
    return rows


def report(name, rows, boards):
    print("=" * 78)
    print("%s  (x%d boards)" % (name.upper(), boards))
    groups = [("THROUGH-HOLE - buy, hand-solder", lambda r: r["tht"] and not r["dnp"]),
              ("SMD", lambda r: r["smd"] and not r["dnp"]),
              ("DNP - do NOT order", lambda r: r["dnp"])]
    for label, keep in groups:
        counts = collections.Counter()
        refs = collections.defaultdict(list)
        for r in rows:
            if not keep(r):
                continue
            key = (r["lcsc"] or "(none)", r["value"], r["mpn"])
            counts[key] += 1
            refs[key].append(r["ref"])
        print("\n-- %s: %d placements/board across %d lines"
              % (label, sum(counts.values()), len(counts)))
        for key, per_board in sorted(counts.items(), key=lambda kv: (-kv[1], kv[0][1])):
            lcsc, value, mpn = key
            sides = sorted({s for s in (side(r) for r in refs[key]) if s})
            print("   %-11s %-26s %-19s /bd=%2d  total=%3d  %-14s %s"
                  % (lcsc, value[:26], (mpn or "-")[:19], per_board,
                     per_board * boards, "+".join(sides),
                     ",".join(sorted(refs[key]))))


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    os.chdir(root)
    missing = [p for _, p, _ in BOARDS if not os.path.exists(p)]
    if missing:
        sys.exit("board file(s) not found: %s" % ", ".join(missing))
    for name, path, boards in BOARDS:
        report(name, census(path), boards)
    print("\nReconcile the total column against docs/BOM.md order 2 (LCSC cart) "
          "before placing an order.")


if __name__ == "__main__":
    main()
