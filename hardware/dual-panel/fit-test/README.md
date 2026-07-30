# Cavity fit test

A 1:1 printable template for checking that the **brain** clears the frame cavity
before committing to an outline or an order.

## Why

Everything in `dual-panel.kicad_pcb` is in **carrier board coordinates**. The frame
cavity is not — its opening was measured as 88 x 100 mm, but *where* that rectangle
sits relative to the board was never established. The design currently assumes it is
centred on the mounting-hole pattern (centre `85.80, 78.90`), giving a usable window
of `x 41.80-129.80, y 28.90-128.90`.

That assumption is load-bearing, so it was checked. **Verified 2026-07-30:** the
dashed rectangle matched the real frame edge on all four sides, so the window above
is now measured rather than assumed.

The same test caught a real clash — J1's USB-C body overhung the west edge by 3.20mm,
leaving 2.94mm to the cavity wall where the outline alone showed 6.11mm. J1 has since
moved to the brain's south edge (24.70mm of clearance) and the generator now draws
overhanging bodies (see below).

## Use

```sh
python3 gen_cavity_template.py     # regenerates cavity-fit-template.svg
```

1. Print `cavity-fit-template.svg` at **100% / "Actual size"** — never fit-to-page.
   Confirm the calibration bar measures exactly 100.0 mm before trusting anything.
   Card stock beats paper; paper sags through the opening and fakes a fit.
2. Punch the four ⌀4.50 standoff holes and cut out the brain silhouette.
3. Seat it on the frame standoffs and look down through the cut-out:
   - **only cavity visible** — the brain clears the opening
   - **frame ledge inside the cut** — that side clashes; measure the intrusion
4. Read the gap between the dashed rectangle and the real frame edge on each side.
   That gap *is* the offset, and it is what the design needs.

Then update `WINDOW` at the top of the generator and re-run, so the printed margins
reflect reality instead of the assumption.

## Overhanging parts

Edge-mount connectors have bodies that stick out past `Edge.Cuts`, so plotting the
outline alone understates how much room the board needs. The generator detects any
footprint whose courtyard crosses the outline and draws it as a dashed blue box,
printing its clearance to the assumed opening. **Cut wide enough to clear those
boxes, not just the outline** — a body that overhangs is what actually hits the frame.

## Limits

- **Plan only.** It does not test depth. The brain sits ~14 mm down; if the frame is
  moulded with draft, the walls close in over that distance. Re-check with a stiff
  copy of the cut-out dropped to the cavity floor.
- **Not a height check.** The measured assembly stack is recorded in
  [`../README.md`](../README.md) under *Mechanical stack* — 15.35 mm used of 20 mm
  available, with an 11 mm M3 spacer between the boards.
- **Don't read it to tenths.** ⌀4.50 holes on M3 screws let the board sit up to
  0.75 mm off in any direction, and PCB outline tolerance adds ~±0.2 mm.

## Notes

All geometry — both outlines, the standoff holes, the M3 interface holes, and the
carrier→brain registration offset — is read from `../dual-panel.kicad_pcb` at run
time. **Re-run the generator after any outline or placement change**; only `WINDOW`
is hard-coded, because it describes the frame rather than the boards.
