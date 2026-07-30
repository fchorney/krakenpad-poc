#!/usr/bin/env python3
"""
Generate a fabrication panel from hardware/dual-panel/dual-panel.kicad_pcb.

The design file holds two boards (carrier + brain) side by side. A fab needs one
contiguous piece, so this joins them to a rail frame with mouse-bite tabs and
emits a separate panel file. **The panel is a generated artifact, like gerbers**
-- keep designing in dual-panel.kicad_pcb and re-run this for fab. Never hand-edit
the panel.

Why one panel matters: JLC's own panelization only arrays a single design, so two
different boards must be supplied as a customer panel. Doing so is what makes it
ONE order -- one engineering fee, one PCBA setup, one stencil (~$97 of per-order
overhead, plus a second shipment, that you would otherwise pay twice).

    python3 gen_panel.py

Requires KiKit importable from KiCad's Python (see README).
"""

import math
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "..", "dual-panel.kicad_pcb")
OUT = os.path.join(HERE, "panel.kicad_pcb")

# --- panel parameters -------------------------------------------------------
SPACE_MM = 3.0     # gap board-to-frame and board-to-board
FRAME_MM = 5.0     # rail width; JLC's conveyor wants a rail to clamp
TAB_MM = 3.0       # mouse-bite tab width
# Tabs per edge. 3 gives the brain 6 tabs (3 east + 3 west) rather than 4.
# It only ever gets east/west: KiKit's partition line comes from bounding boxes,
# and the brain's bbox sits inside the carrier's y-span, so the layout reads as
# two columns with one item each and the brain has no north/south neighbour to
# bridge to. Tight frames, explicit TabAnnotations and buildFullTabs were all
# tried and produce nothing there -- the only real fix is filling the brain's
# column above and below it, i.e. more brains per panel.
TABS_H = 3
TABS_V = 3
BITE_DRILL_MM = 0.5
BITE_SPACING_MM = 1.0
FIDUCIAL_COUNT = 3
TOOLING_COUNT = 4
# ---------------------------------------------------------------------------

CONTOUR_TOL = 1e-4  # mm; 100 nm -- joins coordinate rounding, still catches real gaps


def blocks(src, tag):
    for m in re.finditer(r"\(" + tag + r"\b", src):
        i = m.start()
        depth = 0
        for j in range(i, len(src)):
            if src[j] == "(":
                depth += 1
            elif src[j] == ")":
                depth -= 1
                if depth == 0:
                    break
        yield src[i:j + 1]


def board_outlines(path):
    """Return closed Edge.Cuts contours as (minx, maxx, miny, maxy), largest first.

    Fails loudly on a discontinuous outline. KiCad's DRC does NOT catch sub-micron
    outline gaps -- a 1.6 um fillet-to-line gap sat in this design reporting zero
    DRC violations while making the board unpanelizable, so check it here.
    """
    src = open(path).read()
    segs = []
    for tag in ("gr_line", "gr_arc"):
        for b in blocks(src, tag):
            if '"Edge.Cuts"' not in b:
                continue
            pts = {k: (float(x), float(y)) for k, x, y in
                   re.findall(r"\((start|end|mid) (-?[\d.]+) (-?[\d.]+)\)", b)}
            segs.append((pts["start"], pts["end"]))

    remaining, contours = list(segs), []
    while remaining:
        a, b = remaining.pop(0)
        path_pts = [a, b]
        grew = True
        while grew:
            grew = False
            for append in (True, False):
                anchor = path_pts[-1] if append else path_pts[0]
                for i, (c, d) in enumerate(remaining):
                    nxt = (d if math.dist(anchor, c) < CONTOUR_TOL
                           else c if math.dist(anchor, d) < CONTOUR_TOL else None)
                    if nxt is not None:
                        remaining.pop(i)
                        path_pts.append(nxt) if append else path_pts.insert(0, nxt)
                        grew = True
                        break
                if grew:
                    break
        gap = math.dist(path_pts[0], path_pts[-1])
        if gap >= CONTOUR_TOL:
            sys.exit(f"error: discontinuous board outline -- {gap*1000:.2f} um gap "
                     f"between {tuple(round(v,6) for v in path_pts[-1])} and "
                     f"{tuple(round(v,6) for v in path_pts[0])}\n"
                     f"       KiCad DRC will not report this. Snap the endpoints "
                     f"together, then re-run.")
        xs = [p[0] for p in path_pts]
        ys = [p[1] for p in path_pts]
        contours.append((min(xs), max(xs), min(ys), max(ys)))
    contours.sort(key=lambda c: -((c[1] - c[0]) * (c[3] - c[2])))
    return contours


def main():
    try:
        import pcbnew
        from shapely.geometry import box as shbox
        from kikit.panelize import Panel, Origin
        from kikit.panelize_ui_impl import polygonToSubstrate
        from kikit.units import mm
    except ImportError as e:
        sys.exit(f"error: {e}\n"
                 "This must run under KiCad's Python with KiKit importable:\n"
                 "  /Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/"
                 "Versions/Current/bin/python3 gen_panel.py\n"
                 "See README.md for installing KiKit.")

    contours = board_outlines(SRC)
    if len(contours) < 2:
        sys.exit(f"error: expected 2 board outlines, found {len(contours)}")
    carrier, brain = contours[0], contours[1]
    if brain[0] < carrier[0]:
        carrier, brain = brain, carrier
    print(f"carrier outline: x {carrier[0]:7.2f}..{carrier[1]:7.2f}  y {carrier[2]:6.2f}..{carrier[3]:6.2f}")
    print(f"brain   outline: x {brain[0]:7.2f}..{brain[1]:7.2f}  y {brain[2]:6.2f}..{brain[3]:6.2f}")

    # KiKit extracts each board with a rectangular sourceArea, so the two boards'
    # BOUNDING BOXES must be separable by a vertical line -- not merely disjoint.
    # They interleaved by 0.28 mm at one point (a carrier connector tab reaching
    # past the brain's west edge) and no rectangle could split them.
    if brain[0] <= carrier[1]:
        sys.exit(f"error: bounding boxes interleave -- carrier reaches x={carrier[1]:.2f} "
                 f"but brain starts at x={brain[0]:.2f}.\n"
                 f"       Move the brain ~{carrier[1]-brain[0]+1:.1f} mm east and re-run. "
                 f"Its position is arbitrary: the assembled\n"
                 f"       relationship lives in the H5->H1 offset, not the layout.")
    split = (carrier[1] + brain[0]) / 2
    print(f"vertical split at x = {split:.2f}  (clearance {brain[0]-carrier[1]:.2f} mm)")

    def area(x0, y0, x1, y1):
        return pcbnew.BOX2I(pcbnew.VECTOR2I(int(x0 * mm), int(y0 * mm)),
                            pcbnew.VECTOR2I(int((x1 - x0) * mm), int((y1 - y0) * mm)))

    M = 2.0  # margin around each source area
    cw, ch = carrier[1] - carrier[0], carrier[3] - carrier[2]
    bw, bh = brain[1] - brain[0], brain[3] - brain[2]

    panel = Panel(OUT)
    cx, cy = 100.0, 100.0
    panel.appendBoard(SRC, pcbnew.VECTOR2I(int(cx * mm), int(cy * mm)),
                      sourceArea=area(carrier[0]-M, carrier[2]-M, split, carrier[3]+M),
                      origin=Origin.Center)
    bx = cx + cw / 2 + SPACE_MM + bw / 2
    panel.appendBoard(SRC, pcbnew.VECTOR2I(int(bx * mm), int(cy * mm)),
                      sourceArea=area(split, brain[2]-M, brain[1]+M, brain[3]+M),
                      origin=Origin.Center)
    print(f"substrates: {len(panel.substrates)}")

    # Framing substrates must exist BEFORE the partition line and tabs, otherwise
    # tab generation silently yields zero tabs and the frame is a no-op. Mirrors
    # dummyFramingSubstrate() in kikit.panelize_ui_impl.
    space, w = SPACE_MM * mm, 1 * mm
    minx = min(s.bounds()[0] for s in panel.substrates)
    maxx = max(s.bounds()[2] for s in panel.substrates)
    miny = min(s.bounds()[1] for s in panel.substrates)
    maxy = max(s.bounds()[3] for s in panel.substrates)
    framing = [
        polygonToSubstrate(shbox(minx, miny - 2*space - w, maxx, miny - 2*space)),
        polygonToSubstrate(shbox(minx, maxy + 2*space, maxx, maxy + 2*space + w)),
        polygonToSubstrate(shbox(minx - 2*space - w, miny, minx - 2*space, maxy)),
        polygonToSubstrate(shbox(maxx + 2*space, miny, maxx + 2*space + w, maxy)),
    ]

    panel.buildPartitionLineFromBB(framing)
    panel.clearTabsAnnotations()
    panel.buildTabAnnotationsFixed(TABS_H, TABS_V, TAB_MM*mm, TAB_MM*mm,
                                   1*mm, framing)
    cuts = panel.buildTabsFromAnnotations(fillet=0)
    print(f"tab cuts: {len(cuts)}")
    if not cuts:
        sys.exit("error: zero tabs generated -- the boards would not be joined. "
                 "Check the framing substrates.")

    # Report tabs per board and edge. This is the "will it fall off the panel?"
    # check, so make it visible rather than trusting a total.
    names = {0: "carrier", 1: "brain"}
    tally = {}
    for cut in cuts:
        c = cut.centroid
        idx = min(range(len(panel.substrates)),
                  key=lambda i: panel.substrates[i].substrates.distance(c))
        x0, y0, x1, y1 = panel.substrates[idx].bounds()
        edge = min((("W", abs(c.x - x0)), ("E", abs(c.x - x1)),
                    ("N", abs(c.y - y0)), ("S", abs(c.y - y1))), key=lambda t: t[1])[0]
        tally[(idx, edge)] = tally.get((idx, edge), 0) + 1
    for idx in sorted({k[0] for k in tally}):
        per = {e: n for (i, e), n in tally.items() if i == idx}
        total = sum(per.values())
        detail = " ".join(f"{e}={per.get(e, 0)}" for e in "NSEW")
        print(f"  {names.get(idx, idx):8s}: {total:2d} tabs   {detail}")

    panel.makeFrame(widthH=FRAME_MM*mm, widthV=FRAME_MM*mm,
                    hspace=SPACE_MM*mm, vspace=SPACE_MM*mm)
    panel.makeMouseBites(cuts, diameter=BITE_DRILL_MM*mm, spacing=BITE_SPACING_MM*mm,
                         offset=0.25*mm, prolongation=0.5*mm)
    panel.addCornerFiducials(fidCount=FIDUCIAL_COUNT, horizontalOffset=2.5*mm,
                             verticalOffset=2.5*mm, copperDiameter=1*mm,
                             openingDiameter=2*mm)
    panel.addCornerTooling(holeCount=TOOLING_COUNT, horizontalOffset=5*mm,
                           verticalOffset=2.5*mm, diameter=1.5*mm)

    # refillAllZones is not optional here. Zone fills are copied from the source
    # board as-is, so if it was saved with zones dirty -- trivially easy, e.g.
    # after moving a zone outline and forgetting to press B -- the panel is
    # generated "successfully" with no copper pours at all, and gerbers off it
    # would be boards with no ground plane. Refill unconditionally.
    panel.save(refillAllZones=True)

    # Verify, because KiKit will happily emit a panel whose boards are still loose:
    # an earlier run reported no error yet produced 0 tabs and 2 separate pieces.
    check = Panel(os.path.join(HERE, ".verify.kicad_pcb"))
    check.appendBoard(OUT, pcbnew.VECTOR2I(0, 0), origin=Origin.Center)
    geom = check.substrates[0].substrates
    pieces = len(geom.geoms) if hasattr(geom, "geoms") else 1
    for f in (".verify.kicad_pcb", ".verify.kicad_pro", ".verify.kicad_prl"):
        try:
            os.remove(os.path.join(HERE, f))
        except OSError:
            pass

    board = pcbnew.LoadBoard(OUT)
    bb = board.GetBoardEdgesBoundingBox()
    print(f"\nwrote {OUT}")
    print(f"  panel      : {bb.GetWidth()/1e6:.2f} x {bb.GetHeight()/1e6:.2f} mm "
          f"({bb.GetWidth()/1e6*bb.GetHeight()/1e6/100:.0f} cm2)")
    print(f"  footprints : {len(list(board.GetFootprints()))}")
    print(f"  contiguous : {'YES - one piece' if pieces == 1 else f'NO - {pieces} pieces'}")
    if pieces != 1:
        sys.exit("error: panel is not contiguous; it would fall apart on the router.")


if __name__ == "__main__":
    main()
