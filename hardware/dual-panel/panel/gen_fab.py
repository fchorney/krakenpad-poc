#!/usr/bin/env python3
"""
Generate the JLCPCB fabrication package from panel.kicad_pcb.

    python3 gen_panel.py     # first, to build/refresh the panel
    python3 gen_fab.py       # then this

Writes into production/:
    panel-gerbers.zip        gerbers + Excellon drill + map
    panel-BOM.csv            JLC format: Comment, Designator, Footprint, LCSC
    panel-CPL.csv            JLC format: Designator, Mid X, Mid Y, Layer, Rotation

**Assembly scope: SMD only.** Through-hole parts -- all the connectors, both
switches and the eight interface headers/sockets -- are hand-soldered, matching
what was done on panel-pcb. That also sidesteps the fact that the interface
headers/sockets carry no LCSC number.

Three groups are filtered out, for different reasons:
  - through-hole parts, per the decision above
  - the 30 test points, which are bare plated holes with nothing to place; their
    "value" is a net name, so left in they become 30 unmatched BOM lines
  - KiKit's mouse bites, tooling holes and fiducials, which it already flags
    exclude_from_bom

Upload the zip as a **customer panel** ("panel by customer"), not as a single
board -- JLC's own panelization only arrays one design.
"""

import argparse
import collections
import csv
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
CLI = "/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli"

LAYERS = ("F.Cu,In1.Cu,In2.Cu,B.Cu,F.Paste,B.Paste,F.Silkscreen,B.Silkscreen,"
          "F.Mask,B.Mask,Edge.Cuts")


def run(args):
    r = subprocess.run(args, capture_output=True, text=True)
    if r.returncode != 0:
        sys.exit(f"error: {' '.join(args[:3])}... failed\n{r.stdout}\n{r.stderr}")
    return r.stdout


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[1])
    ap.add_argument("--board", default=os.path.join(HERE, "panel.kicad_pcb"),
                    help="board to export (default: the generated panel)")
    ap.add_argument("--out", default=None,
                    help="output directory (default: production/ beside the board)")
    ap.add_argument("--allow-part-clash", action="store_true",
                    help="downgrade the LCSC-collision check to a warning. For "
                         "comparison quotes only -- never for a real order.")
    args = ap.parse_args()

    PANEL = os.path.abspath(args.board)
    if not os.path.isfile(PANEL):
        sys.exit(f"error: {PANEL} not found"
                 + (" -- run gen_panel.py first" if PANEL.endswith("panel.kicad_pcb") else ""))
    stem = os.path.splitext(os.path.basename(PANEL))[0]
    OUTDIR = os.path.abspath(args.out) if args.out else os.path.join(
        os.path.dirname(PANEL), "production")
    GERBERDIR = os.path.join(OUTDIR, "gerbers")

    try:
        import pcbnew
    except ImportError:
        sys.exit("error: run this under KiCad's Python (see README).")

    shutil.rmtree(OUTDIR, ignore_errors=True)
    os.makedirs(GERBERDIR)

    run([CLI, "pcb", "export", "gerbers", "--output", GERBERDIR + os.sep,
         "--layers", LAYERS, "--no-protel-ext", "--subtract-soldermask", PANEL])
    run([CLI, "pcb", "export", "drill", "--output", GERBERDIR + os.sep,
         "--format", "excellon", "--drill-origin", "absolute",
         "--excellon-separate-th", "--generate-map", "--map-format", "gerberx2",
         PANEL])
    zip_base = os.path.join(OUTDIR, f"{stem}-gerbers")
    shutil.make_archive(zip_base, "zip", GERBERDIR)

    # CPL. --smd-only drops through-hole parts AND the test points, which carry
    # neither the SMD nor the through-hole attribute.
    pos_raw = os.path.join(OUTDIR, "_pos.csv")
    run([CLI, "pcb", "export", "pos", "--output", pos_raw, "--format", "csv",
         "--units", "mm", "--side", "both", "--smd-only", "--exclude-dnp", PANEL])
    board = pcbnew.LoadBoard(PANEL)

    # --smd-only honours the SMD attribute but NOT exclude_from_bom, so KiKit's
    # fiducials are SMD-attributed and sail straight into the position file.
    # They are copper markers, not parts. Anything not a BOM item is not a
    # placement either, so drop the whole excluded set here.
    not_a_part = {fp.GetReference() for fp in board.GetFootprints()
                  if fp.GetAttributes() & pcbnew.FP_EXCLUDE_FROM_BOM}

    cpl = os.path.join(OUTDIR, f"{stem}-CPL.csv")
    placed, dropped = set(), 0
    with open(pos_raw, newline="") as fh, open(cpl, "w", newline="") as out:
        r = csv.DictReader(fh)
        w = csv.writer(out)
        w.writerow(["Designator", "Mid X", "Mid Y", "Layer", "Rotation"])
        for row in r:
            ref = row["Ref"]
            if ref in not_a_part:
                dropped += 1
                continue
            placed.add(ref)
            w.writerow([ref, row["PosX"], row["PosY"],
                        "top" if row["Side"].lower() in ("top", "front") else "bottom",
                        row["Rot"]])
    os.remove(pos_raw)
    if dropped:
        print(f"  (dropped {dropped} non-part footprint(s) from the CPL: "
              f"fiducials/tooling)")

    # BOM, restricted to exactly what the CPL places so the two cannot disagree.
    groups = collections.defaultdict(list)
    for fp in board.GetFootprints():
        ref = fp.GetReference()
        if ref not in placed:
            continue
        try:
            fields = dict(fp.GetFieldsShownText())
        except Exception:
            fields = {}
        lcsc = fields.get("LCSC") or fields.get("lcsc") or ""
        groups[(fp.GetValue(), fp.GetFPIDAsString().split(":")[-1], lcsc)].append(ref)

    def sortkey(ref):
        head = ref.rstrip("0123456789")
        return (head, int(ref[len(head):] or 0))

    # An LCSC code identifies one physical part, so a code appearing under two
    # different value/footprint combinations means at least one is wrong. Caught
    # C4216 on both R19 (33k) and R20 (1M), where R20 had inherited R19's code --
    # it would have fitted 33k into the RS-485 shield bleed, quietly weakening the
    # hybrid grounding by ~30x with nothing to show for it electrically.
    #
    # Checking within the BOM set is what makes this clean: the through-hole
    # connectors legitimately share a code across different Value strings, because
    # there the Value is a label ("FSR East", "12V_IN") rather than a part value.
    # They are hand-soldered and never reach the BOM, so they cannot false-positive.
    by_code = collections.defaultdict(set)
    for (val, fpname, lcsc), refs in groups.items():
        if lcsc:
            by_code[lcsc].add((val, fpname))
    clashes = {c: v for c, v in by_code.items() if len(v) > 1}
    if clashes:
        print("\nerror: one LCSC code used for more than one part:")
        for code, combos in sorted(clashes.items()):
            print(f"  {code}")
            for val, fpname in sorted(combos):
                who = [r for (v, f, l), rs in groups.items()
                       if l == code and v == val for r in rs]
                print(f"      {val:16s} {fpname:24s} {','.join(sorted(who))}")
        if not args.allow_part_clash:
            sys.exit("Fix the LCSC fields before ordering -- the wrong part would "
                     "be fitted.\nRe-run with --allow-part-clash only for a "
                     "comparison quote.")
        print("  proceeding anyway (--allow-part-clash) -- DO NOT ORDER FROM THIS")

    bom = os.path.join(OUTDIR, f"{stem}-BOM.csv")
    missing = []
    with open(bom, "w", newline="") as out:
        w = csv.writer(out)
        w.writerow(["Comment", "Designator", "Footprint", "LCSC"])
        for (val, fpname, lcsc), refs in sorted(groups.items()):
            if not lcsc:
                missing.append((val, len(refs)))
            w.writerow([val, ",".join(sorted(refs, key=sortkey)), fpname, lcsc])

    print(f"\nwrote {OUTDIR}/")
    print(f"  {stem}-gerbers.zip  ({len(os.listdir(GERBERDIR))} files)")
    print(f"  {stem}-CPL.csv      {len(placed)} placements")
    print(f"  {stem}-BOM.csv      {len(groups)} component lines")
    if missing:
        print(f"\n  WARNING: {len(missing)} BOM line(s) without an LCSC number:")
        for val, n in missing:
            print(f"    {n:4d} x {val}")
        print("  JLC will ask you to pick these by hand at quote time.")
    else:
        print("  every BOM line carries an LCSC number")


if __name__ == "__main__":
    main()
