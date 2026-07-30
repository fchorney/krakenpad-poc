# Cavity fit test

A 1:1 printable template for checking that the **brain** clears the frame cavity
before committing to an outline or an order.

## Why

Everything in `dual-panel.kicad_pcb` is in **carrier board coordinates**. The frame
cavity is not — its opening was measured as 88 x 100 mm, but *where* that rectangle
sits relative to the board was never established. The design currently assumes it is
centred on the mounting-hole pattern (centre `85.80, 78.90`), giving a usable window
of `x 41.80-129.80, y 28.90-128.90`.

That assumption is load-bearing. The brain's east/west margins are only **~5.8 / 6.1 mm**,
so an opening a few millimetres off-centre puts the brain into the frame wall. This
template replaces the assumption with a measurement.

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

## Limits

- **Plan only.** It does not test depth. The brain sits ~14 mm down; if the frame is
  moulded with draft, the walls close in over that distance. Re-check with a stiff
  copy of the cut-out dropped to the cavity floor.
- **Not a height check.** Mated connector height + 1.6 mm of brain PCB + the tallest
  bottom-side brain part must fit the 20 mm of measured depth. Separate check.
- **Don't read it to tenths.** ⌀4.50 holes on M3 screws let the board sit up to
  0.75 mm off in any direction, and PCB outline tolerance adds ~±0.2 mm.

## Notes

All geometry — both outlines, the standoff holes, the M3 interface holes, and the
carrier→brain registration offset — is read from `../dual-panel.kicad_pcb` at run
time. **Re-run the generator after any outline or placement change**; only `WINDOW`
is hard-coded, because it describes the frame rather than the boards.
