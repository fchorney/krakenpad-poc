#!/usr/bin/env python3
"""
Generate a 1:1 printable cavity fit-test template for hardware/dual-panel.

The template is the CARRIER outline with the BRAIN silhouette cut out of it, at
the brain's true assembled position. Punch the four standoff holes, cut the
brain shape, drop it on the frame standoffs and look down through the hole:

  - see only cavity on all sides  -> the brain clears the opening
  - see frame ledge inside the cut -> that side clashes; measure the intrusion

The dashed rectangle is the ASSUMED cavity opening (see WINDOW below). It is
reference only. The gap between it and the real frame edge is the measurement
that removes the assumption.

All geometry except WINDOW is read from the .kicad_pcb, so re-run this after any
outline or placement change:

    python3 gen_cavity_template.py

Print at 100% / "Actual Size" -- never fit-to-page -- and confirm the
calibration bar measures exactly 100.0 mm before trusting the result.
"""

import math
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PCB = os.path.join(HERE, "..", "dual-panel.kicad_pcb")
OUT = os.path.join(HERE, "cavity-fit-template.svg")

# Frame cavity opening, in CARRIER coordinates. This is the unverified
# assumption the whole test exists to check: the measured 88 x 100 mm opening
# taken to be centred on the mounting-hole pattern centre (85.80, 78.90).
# Update once measured, then re-run.
WINDOW = (41.80, 129.80, 28.90, 128.90)  # x0, x1, y0, y1

CARRIER_ANCHOR = "H5"  # carrier-side M3 for the brain
BRAIN_ANCHOR = "H1"    # its mate on the brain
ARC_STEPS = 24
TOL = 0.05


# ---------------------------------------------------------------- s-expr bits

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


def pt(block, key):
    m = re.search(r"\(" + key + r" (-?[\d.]+) (-?[\d.]+)", block)
    return (float(m.group(1)), float(m.group(2))) if m else None


def arc_points(a, b, c):
    """Polyline through start a, mid b, end c."""
    (x1, y1), (x2, y2), (x3, y3) = a, b, c
    d = 2 * (x1 * (y2 - y3) + x2 * (y3 - y1) + x3 * (y1 - y2))
    if abs(d) < 1e-9:
        return [a, c]
    ux = ((x1**2 + y1**2) * (y2 - y3) + (x2**2 + y2**2) * (y3 - y1)
          + (x3**2 + y3**2) * (y1 - y2)) / d
    uy = ((x1**2 + y1**2) * (x3 - x2) + (x2**2 + y2**2) * (x1 - x3)
          + (x3**2 + y3**2) * (x2 - x1)) / d
    r = math.hypot(x1 - ux, y1 - uy)
    a0 = math.atan2(y1 - uy, x1 - ux)
    a1 = math.atan2(y2 - uy, x2 - ux)
    a2 = math.atan2(y3 - uy, x3 - ux)
    # pick sweep direction that passes through the mid point
    def norm(t):
        while t <= -math.pi:
            t += 2 * math.pi
        while t > math.pi:
            t -= 2 * math.pi
        return t
    sweep = norm(a2 - a0)
    if norm(a1 - a0) * sweep < 0 or abs(norm(a1 - a0)) > abs(sweep):
        sweep = sweep - math.copysign(2 * math.pi, sweep)
    return [(ux + r * math.cos(a0 + sweep * k / ARC_STEPS),
             uy + r * math.sin(a0 + sweep * k / ARC_STEPS))
            for k in range(ARC_STEPS + 1)]


# ------------------------------------------------------------------- geometry

def load(path):
    src = open(path).read()
    segs, circles = [], []
    for tag in ("gr_line", "gr_rect", "gr_poly", "gr_arc", "gr_circle"):
        for b in blocks(src, tag):
            if '"Edge.Cuts"' not in b:
                continue
            if tag == "gr_line":
                segs.append((pt(b, "start"), pt(b, "end")))
            elif tag == "gr_rect":
                (x0, y0), (x1, y1) = pt(b, "start"), pt(b, "end")
                r = [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]
                segs += [(r[i], r[(i + 1) % 4]) for i in range(4)]
            elif tag == "gr_poly":
                p = [(float(x), float(y))
                     for x, y in re.findall(r"\(xy (-?[\d.]+) (-?[\d.]+)\)", b)]
                segs += [(p[i], p[(i + 1) % len(p)]) for i in range(len(p))]
            elif tag == "gr_arc":
                p = arc_points(pt(b, "start"), pt(b, "mid"), pt(b, "end"))
                segs += [(p[i], p[i + 1]) for i in range(len(p) - 1)]
            elif tag == "gr_circle":
                c, e = pt(b, "center"), pt(b, "end")
                circles.append((c, 2 * math.hypot(e[0] - c[0], e[1] - c[1])))

    holes = {}
    for b in blocks(src, "footprint"):
        r = re.search(r'\(property "Reference" "(H\d+)"', b)
        if r:
            holes[r.group(1)] = pt(b, "at")
    return segs, circles, holes


def loops(segs):
    """Chain segments into closed outlines, ordering each into a point path.

    Endpoints are matched by DISTANCE, not by snapping to a grid: KiCad
    outlines routinely carry vertices a few microns apart (e.g. y=19.1 meeting
    y=19.11), and grid-quantised keys can drop such a pair into neighbouring
    buckets, which silently truncates the contour and closes it with a chord.
    """
    remaining = [s for s in segs if math.dist(s[0], s[1]) > 1e-9]
    out = []
    while remaining:
        path = list(remaining.pop(0))
        while True:
            end = path[-1]
            best, bd, bp = None, TOL, None
            for i, (a, b) in enumerate(remaining):
                for near, far in ((a, b), (b, a)):
                    d = math.dist(end, near)
                    if d < bd:
                        best, bd, bp = i, d, far
            if best is None:
                break
            remaining.pop(best)
            path.append(bp)
            if math.dist(path[-1], path[0]) <= TOL:
                break
        if len(path) > 2 and math.dist(path[-1], path[0]) <= TOL:
            path.pop()          # drop the duplicated closing vertex
        elif len(path) > 2:
            print(f"warning: open contour near {path[0]} -> {path[-1]} "
                  f"(gap {math.dist(path[0], path[-1]):.3f} mm)")
        out.append(path)
    return out


def bbox(pts):
    xs = [p[0] for p in pts]
    ys = [p[1] for p in pts]
    return min(xs), max(xs), min(ys), max(ys)


def contains(box, p):
    return box[0] - 1 <= p[0] <= box[1] + 1 and box[2] - 1 <= p[1] <= box[3] + 1


# ----------------------------------------------------------------------- draw

def main():
    segs, circles, holes = load(PCB)
    for h in (CARRIER_ANCHOR, BRAIN_ANCHOR):
        if h not in holes:
            sys.exit(f"error: {h} not found in {PCB}")

    paths = loops(segs)
    carrier = brain = None
    for p in paths:
        if contains(bbox(p), holes[CARRIER_ANCHOR]):
            carrier = p
        elif contains(bbox(p), holes[BRAIN_ANCHOR]):
            brain = p
    if carrier is None or brain is None:
        sys.exit(f"error: expected 2 outlines, matched carrier={carrier is not None} "
                 f"brain={brain is not None} (found {len(paths)} contours)")

    ox = holes[BRAIN_ANCHOR][0] - holes[CARRIER_ANCHOR][0]
    oy = holes[BRAIN_ANCHOR][1] - holes[CARRIER_ANCHOR][1]
    brain_c = [(x - ox, y - oy) for x, y in brain]

    cbox = bbox(carrier)
    standoffs = [(c, d) for c, d in circles if contains(cbox, c)]
    m3 = [p for r, p in holes.items()
          if contains(cbox, p) and r != BRAIN_ANCHOR]

    # page: A4 portrait unless the board has outgrown it
    margin, foot = 12.0, 40.0
    need_w = (cbox[1] - cbox[0]) + 2 * margin
    need_h = (cbox[3] - cbox[2]) + 2 * margin + foot
    pw, ph, page = 210.0, 297.0, "A4"
    if need_w > pw or need_h > ph:
        pw, ph, page = 297.0, 420.0, "A3"
        print(f"note: content is {need_w:.0f} x {need_h:.0f} mm -> using A3")
    if need_w > pw or need_h > ph:
        sys.exit(f"error: content {need_w:.0f} x {need_h:.0f} mm exceeds A3")

    tx = (pw - (cbox[1] - cbox[0])) / 2 - cbox[0]
    ty = margin + 8 - cbox[2]

    def P(pts):
        return "M " + " L ".join(f"{x+tx:.3f},{y+ty:.3f}" for x, y in pts) + " Z"

    def text(x, y, s, sz=3.2, anchor="middle", fill="#000", extra=""):
        return (f'<text x="{x+tx:.3f}" y="{y+ty:.3f}" font-family="Helvetica,Arial" '
                f'font-size="{sz}" text-anchor="{anchor}" fill="{fill}" {extra}>{s}</text>')

    cx = (cbox[0] + cbox[1]) / 2
    bb = bbox(brain_c)
    o = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{pw}mm" height="{ph}mm" '
         f'viewBox="0 0 {pw} {ph}">',
         f'<rect x="0" y="0" width="{pw}" height="{ph}" fill="#fff"/>']

    o.append(f'<rect x="{WINDOW[0]+tx:.3f}" y="{WINDOW[2]+ty:.3f}" '
             f'width="{WINDOW[1]-WINDOW[0]:.3f}" height="{WINDOW[3]-WINDOW[2]:.3f}" '
             f'fill="none" stroke="#c00" stroke-width="0.3" stroke-dasharray="3,2"/>')
    o.append(text(WINDOW[0], WINDOW[2] - 1.8,
                  f"ASSUMED opening {WINDOW[1]-WINDOW[0]:.0f} x {WINDOW[3]-WINDOW[2]:.0f}"
                  " &#8212; reference only, do NOT cut", 2.6, "start", "#c00"))

    o.append(f'<path d="{P(carrier)}" fill="none" stroke="#000" stroke-width="0.4"/>')
    o.append(f'<path d="{P(brain_c)}" fill="none" stroke="#000" stroke-width="0.9"/>')
    o.append(text((bb[0] + bb[1]) / 2, (bb[2] + bb[3]) / 2 - 1, "CUT OUT THIS SHAPE", 4.0))
    o.append(text((bb[0] + bb[1]) / 2, (bb[2] + bb[3]) / 2 + 4,
                  "brain silhouette, viewed from above", 2.8))

    for c, d in standoffs:
        o.append(f'<circle cx="{c[0]+tx:.3f}" cy="{c[1]+ty:.3f}" r="{d/2:.3f}" '
                 f'fill="none" stroke="#000" stroke-width="0.4"/>')
        for dx, dy in ((4, 0), (0, 4)):
            o.append(f'<line x1="{c[0]-dx+tx:.3f}" y1="{c[1]-dy+ty:.3f}" '
                     f'x2="{c[0]+dx+tx:.3f}" y2="{c[1]+dy+ty:.3f}" '
                     f'stroke="#000" stroke-width="0.2"/>')
    if standoffs:
        o.append(text(cbox[0] + 6, cbox[3] - 4,
                      "&#8853; punch these &#8212; they seat on the frame standoffs",
                      2.4, "start"))
    for p in m3:
        o.append(f'<circle cx="{p[0]+tx:.3f}" cy="{p[1]+ty:.3f}" r="1.6" '
                 f'fill="none" stroke="#777" stroke-width="0.3"/>')
        o.append(text(p[0], p[1] - 2.6, "M3", 2.2, "middle", "#777"))

    o.append(text(cx, cbox[2] - 4, "N  (FSR North)", 3.0))
    o.append(text(cx, cbox[3] + 6, "S  (FSR South)", 3.0))
    cy = (cbox[2] + cbox[3]) / 2
    o.append(text(cbox[0] - 5, cy, "W  12V IN", 3.0, "middle",
                  extra=f'transform="rotate(-90 {cbox[0]-5+tx:.2f} {cy+ty:.2f})"'))
    o.append(text(cbox[1] + 7, cy, "E  12V OUT", 3.0, "middle",
                  extra=f'transform="rotate(90 {cbox[1]+7+tx:.2f} {cy+ty:.2f})"'))

    bx, by = 20.0, ph - 22.0
    o.append(f'<line x1="{bx}" y1="{by}" x2="{bx+100}" y2="{by}" '
             f'stroke="#000" stroke-width="0.4"/>')
    for i in range(11):
        h = 4 if i % 5 == 0 else 2
        o.append(f'<line x1="{bx+i*10}" y1="{by}" x2="{bx+i*10}" y2="{by-h}" '
                 f'stroke="#000" stroke-width="0.4"/>')
    for k, s in enumerate([
        "CALIBRATION: this bar must measure exactly 100.0 mm.",
        f'Print at 100% / "Actual size" on {page} &#8212; never fit-to-page.',
        "This tests the opening in plan only. It does NOT test depth: if the cavity walls have"
        " draft they close in over the ~14 mm the brain sits down,",
        "so re-check with a stiff copy of the cut-out dropped to the cavity floor.",
        "dual-panel cavity fit test &#8212; carrier outline + brain silhouette, 1:1",
    ]):
        o.append(f'<text x="{bx}" y="{by+5+k*4.5}" font-family="Helvetica,Arial" '
                 f'font-size="3.2">{s}</text>')
    o.append("</svg>")

    open(OUT, "w").write("\n".join(o))
    print(f"wrote {OUT}  ({page})")
    print(f"  carrier outline : x {cbox[0]:.2f}..{cbox[1]:.2f}  y {cbox[2]:.2f}..{cbox[3]:.2f}")
    print(f"  brain (assembled): x {bb[0]:.2f}..{bb[1]:.2f}  y {bb[2]:.2f}..{bb[3]:.2f}")
    print(f"  registration offset carrier->brain: ({ox:.6f}, {oy:.6f})")
    print(f"  margins vs assumed window  west {bb[0]-WINDOW[0]:6.2f}   east {WINDOW[1]-bb[1]:6.2f}")
    print(f"                             north {bb[2]-WINDOW[2]:6.2f}   south {WINDOW[3]-bb[3]:6.2f}")


if __name__ == "__main__":
    main()
