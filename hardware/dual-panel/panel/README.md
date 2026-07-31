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

~228 × 143 mm (≈326 cm²), 289 footprints, verified as one contiguous piece.

```
carrier : 16 tabs   N=3 S=3 E=7 W=3
brain   :  6 tabs   N=0 S=0 E=3 W=3
```

The script prints that breakdown every run — it's the "will a board come off the
panel?" check, so read it rather than trusting the total. Tunable parameters are at
the top: rail width, tab count and width, mouse-bite drill and spacing, fiducial
and tooling-hole counts.

**The brain only ever gets east/west tabs**, and that is a KiKit limitation rather
than a setting. `buildPartitionLineFromBB` tiles the panel by bounding boxes; the
brain's bbox sits inside the carrier's y-span, so the layout reads as two columns
with one item each and the brain has no north/south neighbour to bridge to — its
partition line is literally two vertical segments. Tight frames, explicit
`TabAnnotation`s on the N/S edges (both direction conventions) and `buildFullTabs`
were all tried and produce nothing there. Stacking the boards vertically instead
just rotates the problem, since the brain is then the narrower board. The only real
fix is filling the brain's column above and below it — i.e. more brains per panel,
which costs a full extra assembly each.

Three tabs per side on a 71 × 63 mm board is ample support, so this is a
non-issue in practice.

## Fabrication package

```sh
python3 gen_panel.py     # build/refresh the panel
python3 gen_fab.py       # then the fab package
```

Writes `production/` (gitignored, regenerable): `panel-gerbers.zip` (4 copper +
paste + silk + mask + Edge.Cuts, separate PTH/NPTH Excellon and drill maps),
`panel-BOM.csv` and `panel-CPL.csv` in JLC column format.

**Assembly scope is SMD only** — 115 placements, 101 top and 14 bottom, across 35
component lines, every one carrying an LCSC number. Through-hole parts (all
connectors, both switches, and the eight interface headers/sockets) are
hand-soldered, matching panel-pcb. That also sidesteps the interface
headers/sockets having no LCSC number.

Three groups are filtered out, for different reasons:

- **through-hole parts**, per the decision above
- **the 30 test points** — bare plated holes with nothing to place, whose "value"
  is a net name, so left in they become 30 unmatched BOM lines
- **KiKit's fiducials, tooling holes and mouse bites**. Note `--smd-only` honours
  the SMD attribute but *not* `exclude_from_bom`, so the fiducials are
  SMD-attributed and sail into the position file unless dropped explicitly.

The BOM is built from exactly the set the CPL places, so the two cannot disagree.

**Upload the zip as a customer panel** ("panel by customer"), not as a single
board — JLC's own panelization only arrays one design.

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
