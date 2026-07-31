#!/usr/bin/env python3
"""Generate r/PrintedCircuitBoard-compliant review images for every board in this project.

Produces, per board: schematic (PDF + per-page PNG), 2D plots of every copper
layer, and straight-down 3D plan views of both sides.  The rules being followed
are r/PrintedCircuitBoard rule #8 plus the "before you request a review" post:

  * export/screen-capture only, no photos, no cursor, no grid
  * light-background schematics, standard orientation
  * 2D PCB: readable silkscreen, no net names on traces, no pad numbers,
    nothing that is not in the gerbers (so no *.Fab, no *.Courtyard), board
    outline enabled
  * 3D PCB: same orientation as the 2D images, straight-down plan view

Everything runs on throwaway copies of the projects under --work, because
kicad-cli mutates .kicad_pro / .kicad_sym as a side effect (see
docs/PRE_ORDER_CHECKLIST.md).  The repo's own hardware/ tree is never touched.

Usage:
    tools/gen_review_images.py [--work DIR] [--out DIR] [--only NAME[,NAME...]]
                               [--skip-3d]
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
KICAD_APP = Path("/Applications/KiCad/KiCad.app/Contents")
KICAD_CLI = KICAD_APP / "MacOS" / "kicad-cli"
KICAD_PY = KICAD_APP / "Frameworks/Python.framework/Versions/Current/bin/python3"
THEME_NAME = "PCB-Review"
THEME_DIR = Path.home() / "Library/Preferences/kicad/10.0/colors"

# The x coordinate (mm) that separates the two outlines in dual-panel.kicad_pcb.
# Carrier occupies 42.88..182.18, brain 182.92..253.83.
DUAL_SPLIT_X = 182.55


@dataclass
class Board:
    name: str          # output directory name
    title: str         # human title used in captions
    sch: Path | None   # schematic root to export (relative to work dir)
    pcb: Path          # board file to plot (relative to work dir)
    sheet_names: list[str] = field(default_factory=list)  # per-PDF-page labels


def run(cmd: list[str], **kw) -> subprocess.CompletedProcess:
    proc = subprocess.run([str(c) for c in cmd], capture_output=True, text=True, **kw)
    if proc.returncode != 0:
        sys.stderr.write(f"FAILED: {' '.join(str(c) for c in cmd)}\n{proc.stdout}\n{proc.stderr}\n")
        raise SystemExit(1)
    return proc


def write_theme() -> None:
    """A review-grade colour theme: white silk, black background/clearance."""
    base = THEME_DIR / "user.json"
    data = json.loads(base.read_text()) if base.exists() else {"board": {"copper": {}}}
    b = data.setdefault("board", {})
    b["background"] = "rgb(0, 0, 0)"
    b["f_silks"] = "rgb(255, 255, 255)"
    b["b_silks"] = "rgb(255, 255, 255)"
    b["edge_cuts"] = "rgb(255, 255, 255)"
    b["worksheet"] = "rgb(255, 255, 255)"
    cu = b.setdefault("copper", {})
    cu["f"] = "rgb(176, 46, 46)"
    cu["b"] = "rgb(60, 105, 176)"
    cu["in1"] = "rgb(96, 170, 96)"
    cu["in2"] = "rgb(196, 116, 40)"
    b["pad_plated_hole"] = "rgb(214, 175, 74)"
    data["meta"] = {"filename": THEME_NAME, "name": THEME_NAME, "version": 5}
    THEME_DIR.mkdir(parents=True, exist_ok=True)
    (THEME_DIR / f"{THEME_NAME}.json").write_text(json.dumps(data, indent=2))


def stage(work: Path) -> None:
    """Copy the three KiCad projects into the scratch work dir."""
    work.mkdir(parents=True, exist_ok=True)
    for proj in ("master-pcb", "panel-pcb", "dual-panel"):
        dst = work / proj
        if dst.exists():
            shutil.rmtree(dst)
        shutil.copytree(
            REPO / "hardware" / proj,
            dst,
            ignore=shutil.ignore_patterns("production*", "_restore_backup*", "*.bak", "bak", "fit-test"),
        )


SPLIT_SCRIPT = r'''
import sys, pcbnew
src, dst, keep, xsplit = sys.argv[1], sys.argv[2], sys.argv[3], float(sys.argv[4])
board = pcbnew.LoadBoard(src)

def x_of(item):
    bb = item.GetBoundingBox()
    return (bb.GetLeft() + bb.GetRight()) / 2e6

def wanted(item):
    left = x_of(item) < xsplit
    return left if keep == "left" else (not left)

doomed = []
for coll in (board.GetFootprints(), board.GetTracks(), board.GetDrawings(), board.Zones()):
    for item in coll:
        if not wanted(item):
            doomed.append(item)
for item in doomed:
    try:
        board.Remove(item)
    except Exception as exc:                      # groups / already-detached
        print("skip", item.GetClass(), exc)
pcbnew.SaveBoard(dst, board)
bb = board.GetBoardEdgesBoundingBox()
print("%.2f %.2f %.2f %.2f" % (bb.GetLeft()/1e6, bb.GetTop()/1e6, bb.GetWidth()/1e6, bb.GetHeight()/1e6))
'''


def split_dual(work: Path) -> None:
    """dual-panel.kicad_pcb holds both outlines; carve out one board per file."""
    src = work / "dual-panel" / "dual-panel.kicad_pcb"
    for keep, name in (("left", "carrier-only"), ("right", "brain-only")):
        dst = work / "dual-panel" / f"{name}.kicad_pcb"
        out = run([KICAD_PY, "-c", SPLIT_SCRIPT, src, dst, keep, DUAL_SPLIT_X],
                  env={**os.environ, "PYTHONWARNINGS": "ignore"})
        print(f"  {name}: bbox {out.stdout.strip().splitlines()[-1]}")
        # co-locate a project file so ${KIPRJMOD}, the DRU and the stackup resolve
        for ext in (".kicad_pro", ".kicad_dru"):
            srcf = work / "dual-panel" / f"dual-panel{ext}"
            if srcf.exists():
                shutil.copy(srcf, work / "dual-panel" / f"{name}{ext}")


def board_bbox(pcb: Path) -> tuple[float, float]:
    script = (
        "import sys,pcbnew;b=pcbnew.LoadBoard(sys.argv[1]);"
        "bb=b.GetBoardEdgesBoundingBox();print('%.3f %.3f'%(bb.GetWidth()/1e6,bb.GetHeight()/1e6))"
    )
    out = run([KICAD_PY, "-c", script, pcb]).stdout.strip().splitlines()[-1]
    w, h = out.split()
    return float(w), float(h)


def _pdf(pcb: Path, layers: str, out_pdf: Path, mirror: bool, bg: str | None,
         check_zones: bool = True) -> None:
    opts = [KICAD_CLI, "pcb", "export", "pdf", "--mode-single",
            "--theme", THEME_NAME, "--layers", layers, "--scale", "0",
            "--drill-shape-opt", "2"]
    if check_zones:                      # never on the un-poured copy: it refills
        opts.append("--check-zones")
    if bg:
        opts += ["--bg-color", bg]
    if mirror:
        opts.append("--mirror")
    run(opts + ["-o", out_pdf, pcb])


def plot_2d(pcb: Path, layers: str, out_png: Path, mirror: bool, long_edge: int,
            dpi: int = 500, check_zones: bool = True) -> None:
    """Plot one composited layer set.

    Two passes on purpose. `--bg-color` paints the whole *page* black, so
    ImageMagick's -trim on that image crops to the page rather than the board.
    The white-paper pass has nothing outside the board graphics, so its trim box
    is the real board extent — and because both passes use identical page and
    scale settings, that box can be applied to the black-background raster.
    """
    white_pdf = out_png.with_suffix(".white.pdf")
    black_pdf = out_png.with_suffix(".black.pdf")
    _pdf(pcb, layers, white_pdf, mirror, None, check_zones)
    _pdf(pcb, layers, black_pdf, mirror, "#000000", check_zones)

    probe = out_png.with_suffix(".probe")
    run(["pdftoppm", "-r", dpi, "-png", "-singlefile", white_pdf, probe])
    probe_png = Path(f"{probe}.png")     # pdftoppm appends .png to the stem
    box = run(["magick", probe_png, "-format", "%@", "info:"]).stdout.strip()

    stem = out_png.with_suffix("")
    run(["pdftoppm", "-r", dpi, "-png", "-singlefile", black_pdf, stem])
    run(["magick", out_png, "-crop", box, "+repage",
         "-bordercolor", "black", "-border", str(max(int(dpi / 25), 8)),
         "-resize", f"{long_edge}x{long_edge}>",
         "-strip", "-define", "png:compression-level=9", out_png])
    for f in (white_pdf, black_pdf, probe_png):
        f.unlink(missing_ok=True)


UNFILL_SCRIPT = r'''
import sys, pcbnew
board = pcbnew.LoadBoard(sys.argv[1])
for zone in board.Zones():
    zone.UnFill()
pcbnew.SaveBoard(sys.argv[2], board)
print("unfilled", board.GetAreaCount(), "zones")
'''


def make_unpoured(pcb: Path) -> Path:
    """A copy of the board with every zone emptied, so traces are visible."""
    dst = pcb.with_name(pcb.stem + "-nopour.kicad_pcb")
    run([KICAD_PY, "-c", UNFILL_SCRIPT, pcb, dst])
    for ext in (".kicad_pro", ".kicad_dru"):
        src = pcb.with_suffix(ext)
        if src.exists():
            shutil.copy(src, dst.with_suffix(ext))
    return dst


def render_3d(pcb: Path, out_png: Path, side: str, w: float, h: float, px: int = 2400) -> None:
    aspect = max(w, 1.0) / max(h, 1.0)
    if aspect >= 1:
        iw, ih = px, max(int(px / aspect), 400)
    else:
        ih, iw = px, max(int(px * aspect), 400)
    run([KICAD_CLI, "pcb", "render", "--side", side, "--quality", "high",
         "--background", "opaque", "-w", iw, "-h", ih, "--zoom", "0.92",
         "-o", out_png, pcb])
    run(["magick", out_png, "-strip", "-define", "png:compression-level=9", out_png])


def export_schematic(sch: Path, out_dir: Path, prefix: str, sheet_names: list[str]) -> list[str]:
    pdf = out_dir / f"{prefix}-schematic.pdf"
    run([KICAD_CLI, "sch", "export", "pdf", "--exclude-pdf-property-popups", "-o", pdf, sch])
    pages = int([ln for ln in run(["pdfinfo", pdf]).stdout.splitlines()
                 if ln.startswith("Pages:")][0].split()[-1])
    made = [pdf.name]
    for page in range(1, pages + 1):
        label = sheet_names[page - 1] if page - 1 < len(sheet_names) else f"page{page}"
        png = out_dir / f"{prefix}-schematic-{page}-{label}.png"
        run(["pdftoppm", "-r", "300", "-png", "-f", page, "-l", page,
             "-singlefile", pdf, png.with_suffix("")])
        run(["magick", png, "-trim", "+repage", "-bordercolor", "white", "-border", "32",
             "-resize", "4600x4600>", "-strip", "-define", "png:compression-level=9", png])
        made.append(png.name)
    return made


def process(board: Board, work: Path, out_root: Path, skip_3d: bool) -> list[str]:
    out_dir = out_root / board.name
    out_dir.mkdir(parents=True, exist_ok=True)
    pcb = work / board.pcb
    made: list[str] = []
    print(f"[{board.name}] schematic")
    if board.sch:
        made += export_schematic(work / board.sch, out_dir, "01", board.sheet_names)

    w, h = board_bbox(pcb)
    long_edge = 2600 if max(w, h) > 100 else 2000
    print(f"[{board.name}] 2D plots ({w:.1f} x {h:.1f} mm)")
    for idx, (label, layers, mirror) in enumerate([
        ("top", "F.Cu,F.Silkscreen,Edge.Cuts", False),
        ("in1", "In1.Cu,Edge.Cuts", False),
        ("in2", "In2.Cu,Edge.Cuts", False),
        ("bottom-mirrored", "B.Cu,B.Silkscreen,Edge.Cuts", True),
        ("silkscreen-top", "F.Silkscreen,Edge.Cuts", False),
    ], start=2):
        png = out_dir / f"{idx:02d}-2d-{label}.png"
        plot_2d(pcb, layers, png, mirror, long_edge)
        made.append(png.name)

    # Same copper, zones emptied. Pours hide the routing; reviewers ask for this.
    print(f"[{board.name}] 2D plots, pours removed")
    nopour = make_unpoured(pcb)
    for idx, (label, layers, mirror) in enumerate([
        ("top-no-pour", "F.Cu,F.Silkscreen,Edge.Cuts", False),
        ("in1-no-pour", "In1.Cu,Edge.Cuts", False),
        ("in2-no-pour", "In2.Cu,Edge.Cuts", False),
        ("bottom-no-pour-mirrored", "B.Cu,B.Silkscreen,Edge.Cuts", True),
    ], start=7):
        png = out_dir / f"{idx:02d}-2d-{label}.png"
        plot_2d(nopour, layers, png, mirror, long_edge, check_zones=False)
        made.append(png.name)

    if not skip_3d:
        print(f"[{board.name}] 3D renders")
        for idx, side in ((11, "top"), (12, "bottom")):
            png = out_dir / f"{idx:02d}-3d-{side}.png"
            render_3d(pcb, png, side, w, h)
            made.append(png.name)
    return made


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--work", default=os.environ.get("REVIEW_WORK", "/tmp/krakenpad-review-work"))
    ap.add_argument("--out", default=str(REPO / "review"))
    ap.add_argument("--only", default="")
    ap.add_argument("--skip-3d", action="store_true")
    args = ap.parse_args()

    work = Path(args.work)
    out_root = Path(args.out)
    write_theme()
    print(f"staging projects into {work}")
    stage(work)
    print("splitting dual-panel into its two outlines")
    split_dual(work)

    boards = [
        Board("01-master", "KrakenPad Master (Teensy 4.0) — MCU board",
              Path("master-pcb/master-pcb.kicad_sch"), Path("master-pcb/master-pcb.kicad_pcb"),
              ["master"]),
        Board("02-panel-single", "KrakenPad Panel — single-board design",
              Path("panel-pcb/panel-pcb.kicad_sch"), Path("panel-pcb/panel-pcb.kicad_pcb"),
              ["panel"]),
        Board("03-dual-carrier", "KrakenPad Panel — two-board design, LED/IO carrier",
              Path("dual-panel/carrier.kicad_sch"), Path("dual-panel/carrier-only.kicad_pcb"),
              ["carrier"]),
        Board("04-dual-brain", "KrakenPad Panel — two-board design, MCU brain",
              Path("dual-panel/brain.kicad_sch"), Path("dual-panel/brain-only.kicad_pcb"),
              ["brain"]),
    ]
    if args.only:
        wanted = set(args.only.split(","))
        boards = [b for b in boards if b.name in wanted or b.name.split("-", 1)[1] in wanted]

    index: dict[str, list[str]] = {}
    for b in boards:
        index[b.name] = process(b, work, out_root, args.skip_3d)
    (out_root / "manifest.json").write_text(json.dumps(index, indent=2))
    print("\nwrote:")
    for name, files in index.items():
        print(f"  {name}: {len(files)} files")


if __name__ == "__main__":
    main()
