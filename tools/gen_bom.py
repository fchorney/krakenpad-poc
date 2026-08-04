#!/usr/bin/env python3
"""
Generate a purchasing BOM from a .kicad_pcb.

    python3 tools/gen_bom.py --board hardware/master-pcb/master-pcb.kicad_pcb

This is a SHOPPING LIST for hand assembly, not a JLC PCBA upload. For the
dual-panel PCBA package use hardware/dual-panel/panel/gen_fab.py instead, which
also emits the CPL and applies JLC's own column names.

Sourcing model (user, 2026-08-04): parts come from LCSC, except the Teensys,
which are DigiKey and already purchased. The Source column is derived - a part
with an LCSC code is LCSC, otherwise it falls back to DigiKey.

Skipped, with reasons:
  - footprints flagged exclude_from_bom (mounting holes)
  - TestPoint:* footprints, which are bare plated holes with nothing to buy;
    their "value" is a net name, so left in they become junk BOM lines

DNP parts are KEPT and flagged, because a DNP footprint is a deliberate
insurance option - you want to see it when deciding whether to stock it.

Anything left with no LCSC, no MPN and no DigiKey is reported as a WARNING and
still listed, so a part that is merely missing its data can never be silently
dropped from an order.
"""

import argparse
import collections
import csv
import os
import re
import sys

import pcbnew


def sort_key(ref):
    m = re.match(r"([A-Za-z]+)(\d+)", ref)
    return (m.group(1), int(m.group(2))) if m else (ref, 0)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[1])
    ap.add_argument("--board", required=True, help="path to the .kicad_pcb")
    ap.add_argument("--out", default=None,
                    help="output CSV (default: <board-dir>/<board-name>-BOM.csv)")
    a = ap.parse_args()

    board = pcbnew.LoadBoard(a.board)
    out = a.out or os.path.join(
        os.path.dirname(os.path.abspath(a.board)),
        os.path.splitext(os.path.basename(a.board))[0] + "-BOM.csv")

    groups = collections.defaultdict(list)
    skipped = collections.Counter()
    for f in board.GetFootprints():
        att = f.GetAttributes()
        if att & pcbnew.FP_EXCLUDE_FROM_BOM:
            skipped["exclude_from_bom"] += 1
            continue
        if f.GetFPIDAsString().startswith("TestPoint:"):
            skipped["test point"] += 1
            continue
        d = {fl.GetName(): fl.GetText().strip() for fl in f.GetFields()}
        # Group on the ORDERING identity only. Description is deliberately not
        # part of the key: per-instance text ("INT RC filter cap (INT_UL)") would
        # otherwise split one qty-9 line into nine qty-1 lines.
        key = (d.get("Value", ""), d.get("LCSC", ""), d.get("MPN", ""),
               d.get("DigiKey", ""), bool(att & pcbnew.FP_DNP))
        groups[key].append((f.GetReference(), d.get("Description", "")))

    rows, warned, placements = [], [], 0
    for key, members in sorted(
            groups.items(),
            key=lambda kv: sort_key(sorted(r for r, _ in kv[1])[0])):
        value, lcsc, mpn, dk, dnp = key
        members.sort(key=lambda m: sort_key(m[0]))
        refs = [r for r, _ in members]
        descs = {t for _, t in members if t}
        if not dnp:
            placements += len(refs)
        if not (lcsc or mpn or dk) and not dnp:
            warned.append((refs, value))
        rows.append({
            "Qty": len(refs),
            "Refs": " ".join(refs),
            "Value": value,
            "LCSC": lcsc,
            "MPN": mpn,
            "Source": "LCSC" if lcsc else ("DigiKey" if dk else
                                           ("not stocked" if dnp else "UNSOURCED")),
            "DNP": "DNP" if dnp else "",
            # one shared description, or a count when the instances differ
            "Description": (descs.pop() if len(descs) == 1
                            else (f"{len(descs)} per-instance descriptions"
                                  if descs else "")),
        })

    # lineterminator="\n": csv defaults to CRLF, which git normalises on commit
    # and would otherwise show every line as changed on each regeneration
    with open(out, "w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=list(rows[0].keys()),
                           lineterminator="\n")
        w.writeheader()
        w.writerows(rows)

    print(f"wrote {out}")
    print(f"  {len(rows)} lines, {placements} placements to fit"
          f" ({sum(1 for r in rows if r['DNP'])} DNP lines listed but not counted)")
    for reason, n in sorted(skipped.items()):
        print(f"  skipped {n} ({reason})")
    for refs, value in warned:
        print(f"  WARNING: no LCSC/MPN/DigiKey for {' '.join(refs)} ({value})",
              file=sys.stderr)
    return 1 if warned else 0


if __name__ == "__main__":
    sys.exit(main())
