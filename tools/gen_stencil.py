#!/usr/bin/env python3
"""Generate 3D-printable solder-paste stencils (and a support jig) from a .kicad_pcb.

    /Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3 \
        tools/gen_stencil.py --board hardware/master-pcb/master-pcb.kicad_pcb --out hardware/master-pcb/stencil

Emits OpenSCAD, and renders STL if openscad is on PATH or in /Applications.

Why this exists: the master is a BARE FAB with no PCBA and no stencil, hand-pasted
by syringe on board 1. It is also DOUBLE-SIDED — 46 parts on the front, and 18 on
the back (D1-D9 TVS + C3-C11 filter caps, the whole INT protection block) — so it
needs two stencils, not one.

Self-alignment: each stencil carries a skirt that drops over the board edge, so it
locates itself on the outline instead of being taped down. The skirt is
deliberately SHALLOWER than the board (default 1.0mm of a 1.6mm board) so it grips
the edge without bottoming out on the bench and lifting the stencil off the pads.

⚠ Apertures come from the board's real paste layer via pcbnew, which already
applies the solder-paste margin from board/footprint rules. A printed stencil is
much thicker than the usual 0.1-0.12mm steel, so it deposits more paste per
aperture; --shrink insets every aperture by that much per side to compensate.
Start at 0 for 1.27mm pitch and coarser (this board's finest is SOIC-8 at 1.27mm
and SOT-23-5 at 0.95mm) and only reduce if you get bridging.

The bottom stencil is MIRRORED in X, so it is used as printed on the flipped
board — do not mirror it again in the slicer.
"""
import argparse, math, os, shutil, subprocess, sys
import pcbnew

NM = 1e6  # nm -> mm


def poly_points(polyset, idx=0):
    o = polyset.Outline(idx)
    return [(o.CPoint(i).x / NM, o.CPoint(i).y / NM) for i in range(o.PointCount())]


def scad_poly(pts, indent="    "):
    body = ", ".join("[%.4f,%.4f]" % (x, y) for x, y in pts)
    return "%spolygon(points=[%s]);" % (indent, body)


def collect(board, layer, mirror, shrink_nm, keep_dnp=False):
    """Aperture polygons on `layer`, in view coords (y flipped; x flipped if mirror)."""
    out = []
    skipped = []
    sx = -1.0 if mirror else 1.0
    for fp in board.GetFootprints():
        # ⚠ DNP footprints still carry paste apertures in the board file. Leaving
        # them in deposits paste on pads that will never get a part — it reflows
        # into a bare blob you then have to wick off. On this board that is R1/R2,
        # the RS-485 failsafe bias pair, which is deliberately unpopulated and was
        # never even ordered.
        if fp.IsDNP() and not keep_dnp:
            if any(pd.IsOnLayer(layer) for pd in fp.Pads()):
                skipped.append(fp.GetReference())
            continue
        for pad in fp.Pads():
            if not pad.IsOnLayer(layer):
                continue
            ps = pad.GetEffectivePolygon(layer)
            if shrink_nm:
                ps.Inflate(-int(shrink_nm), 16)
            if ps.OutlineCount() == 0:
                continue
            for i in range(ps.OutlineCount()):
                pts = [(sx * x, -y) for x, y in poly_points(ps, i)]
                if len(pts) >= 3:
                    out.append(pts)
    return out, sorted(set(skipped))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--board", required=True)
    ap.add_argument("--out", required=True, help="output directory")
    ap.add_argument("--thickness", type=float, default=0.20, help="stencil plate, mm")
    ap.add_argument("--clearance", type=float, default=0.15, help="skirt-to-board gap, mm")
    ap.add_argument("--wall", type=float, default=2.0, help="skirt wall thickness, mm")
    ap.add_argument("--skirt", type=float, default=1.0, help="skirt depth, mm (< board thickness)")
    ap.add_argument("--shrink", type=float, default=0.0, help="inset each aperture per side, mm")
    ap.add_argument("--board-thickness", type=float, default=1.6)
    ap.add_argument("--ledge", type=float, default=2.5, help="jig support ledge width, mm")
    ap.add_argument("--keep-dnp", action="store_true",
                    help="cut apertures for DNP parts too (default: skip them)")
    ap.add_argument("--floor", type=float, default=2.0, help="jig floor thickness, mm")
    a = ap.parse_args()

    if a.skirt >= a.board_thickness:
        sys.exit("--skirt (%.2f) must be less than --board-thickness (%.2f), or the "
                 "skirt bottoms out on the bench and lifts the stencil off the pads."
                 % (a.skirt, a.board_thickness))

    board = pcbnew.LoadBoard(a.board)
    ps = pcbnew.SHAPE_POLY_SET()
    if not board.GetBoardPolygonOutlines(ps, True):
        sys.exit("could not read the board outline (Edge.Cuts not closed?)")
    edge = poly_points(ps)
    os.makedirs(a.out, exist_ok=True)
    shrink_nm = a.shrink * NM

    def write(name, mirror, layer, jig=False):
        sx = -1.0 if mirror else 1.0
        outline = [(sx * x, -y) for x, y in edge]
        f = os.path.join(a.out, name + ".scad")
        L = []
        L.append("// generated by tools/gen_stencil.py — do not edit by hand")
        L.append("// board: %s" % os.path.basename(a.board))
        L.append("$fn = 48;")
        L.append("module board_outline() {")
        L.append(scad_poly(outline))
        L.append("}")
        if jig:
            L.append("// Support jig for pasting the SECOND side: the board rests on a")
            L.append("// %.1fmm ledge and everything inside is open, so parts already" % a.ledge)
            L.append("// placed on side one hang free instead of being crushed.")
            L.append("// The rebate is shallower than the board, leaving %.1fmm of edge" % a.skirt)
            L.append("// proud so the stencil's skirt still has something to grip.")
            rebate = a.board_thickness - a.skirt
            L.append("difference() {")
            L.append("  linear_extrude(height=%.3f) offset(r=%.3f) board_outline();"
                     % (a.floor + rebate, a.clearance + a.wall))
            L.append("  translate([0,0,%.3f]) linear_extrude(height=%.3f) offset(r=%.3f) board_outline();"
                     % (a.floor, rebate + 1, a.clearance))
            L.append("  translate([0,0,-1]) linear_extrude(height=%.3f) offset(r=%.3f) board_outline();"
                     % (a.floor + rebate + 2, -a.ledge))
            L.append("}")
        else:
            aps, skipped = collect(board, layer, mirror, shrink_nm, a.keep_dnp)
            L.append("// %d apertures" % len(aps))
            if skipped:
                L.append("// DNP, no aperture cut: %s" % ", ".join(skipped))
            L.append("difference() {")
            L.append("  union() {")
            L.append("    linear_extrude(height=%.3f) offset(r=%.3f) board_outline();"
                     % (a.thickness, a.clearance + a.wall))
            L.append("    translate([0,0,%.3f]) linear_extrude(height=%.3f) difference() {"
                     % (-a.skirt, a.skirt))
            L.append("      offset(r=%.3f) board_outline();" % (a.clearance + a.wall))
            L.append("      offset(r=%.3f) board_outline();" % a.clearance)
            L.append("    }")
            L.append("  }")
            for pts in aps:
                L.append("  translate([0,0,-0.5]) linear_extrude(height=%.3f)"
                         % (a.thickness + 1.0))
                L.append(scad_poly(pts, "    "))
            L.append("}")
            note = ("  (skipped DNP: %s)" % ", ".join(skipped)) if skipped else ""
            print("  %-28s %d apertures%s" % (name, len(aps), note))
        open(f, "w").write("\n".join(L) + "\n")
        return f

    files = [
        write("master-stencil-top", False, pcbnew.F_Paste),
        write("master-stencil-bottom", True, pcbnew.B_Paste),
        write("master-jig-base", False, None, jig=True),
    ]

    exe = shutil.which("openscad") or "/Applications/OpenSCAD.app/Contents/MacOS/OpenSCAD"
    if os.path.exists(exe):
        for f in files:
            stl = f[:-5] + ".stl"
            r = subprocess.run([exe, "-o", stl, f], capture_output=True, text=True)
            print("  %-28s %s" % (os.path.basename(stl),
                                  "ok" if r.returncode == 0 else "FAILED\n" + r.stderr[-400:]))
    else:
        print("  openscad not found — .scad written, render them yourself")


if __name__ == "__main__":
    main()
