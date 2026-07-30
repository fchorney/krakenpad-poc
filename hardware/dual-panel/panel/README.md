# Fabrication panel

`dual-panel.kicad_pcb` holds two boards — carrier and brain — side by side. A fab
needs **one contiguous piece**, so `gen_panel.py` joins them to a rail frame with
mouse-bite tabs and writes a separate `panel.kicad_pcb`.

```sh
python3 gen_panel.py     # must run under KiCad's Python, see below
```

The panel is a **generated artifact, like gerbers** — it's gitignored. Keep
designing in `dual-panel.kicad_pcb` and re-run this for fab. Never hand-edit the
panel; the next run overwrites it.

## Why bother

JLC's own panelization only arrays a *single* design, so two different boards have
to be supplied as a customer panel. Doing that is what makes it **one order** —
one engineering fee, one PCBA setup, one stencil and storage (~$97), plus one
shipment instead of two. Upload them as two designs and you pay all of it twice.

It also needs to be genuinely joined: two closed outlines with a gap between them
don't describe a manufacturable board, and the pieces would simply fall off the
router.

## Current output

~228 × 143 mm (≈326 cm²), 253 footprints, verified as one contiguous piece.
Tunable parameters are at the top of the script — rail width, tab count and width,
mouse-bite drill and spacing, fiducial and tooling-hole counts.

## Requirements

KiKit, importable from **KiCad's Python** (`pcbnew` only exists there). KiKit 1.8.0
is confirmed working against KiCad 10.0.4.

```sh
KPY=/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3

# either install into KiCad's Python (the bundle is writable, no sudo)
"$KPY" -m pip install kikit

# or keep the bundle clean and install somewhere separate
"$KPY" -m pip install --target ~/.kikit kikit
PYTHONPATH=~/.kikit "$KPY" gen_panel.py
```

## Things that will bite

**KiCad DRC does not catch sub-micron outline gaps.** A 1.6 µm gap between a fillet
arc and its line sat in this design with DRC reporting *zero* violations, and it
made the board unpanelizable. The script checks outline continuity itself and fails
with the coordinates.

**The two bounding boxes must be separable by a vertical line**, not merely
disjoint — KiKit extracts each board with a rectangular `sourceArea`. They once
interleaved by 0.28 mm, because a carrier connector tab reached past the brain's
west edge, and no rectangle could split them. The fix is to nudge the brain east;
its position is arbitrary, since the assembled relationship lives in the H5→H1
offset rather than the layout. The script checks this and says how far to move it.

**KiKit will emit a panel whose boards are still loose, without complaining.** An
early attempt reported no error and produced 0 tabs, no frame, and two separate
pieces. The order matters: framing substrates must exist *before* the partition
line and tab annotations. The script verifies the result is one contiguous piece
and fails if it isn't — don't remove that check.
