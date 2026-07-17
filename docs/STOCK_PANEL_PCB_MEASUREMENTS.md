# Stock Gen4+ Panel PCB — measurements

Two sources, kept side by side: an earlier **photo estimate** (Step Revolution shop
photo, `PCB-Gen4_1024x1024.jpg`, scaled by the known 127mm dimension → 4.65 px/mm,
**±1–1.5mm accuracy**) and **physical caliper measurements** taken 2026-07-10 on a
panel PCB pulled from the user's own pad (Gen5, not Gen4 — same 5×5" family, treat as
authoritative where the two disagree). Coordinates: mm from the board's top-left
corner, +X right, +Y down, unless noted otherwise.

Chip-level inventory of the same board (what ICs stock uses, and what that tells us):
see `STOCK_PANEL_CHIPS.md`.

## Outline

- Photo estimate: 127 × 127mm square (5×5").
- **Physical (2026-07-10): not a perfect square** — edges measured 128mm / 127mm /
  128mm / 127mm going around. Corner notches have angled sides that are hard to
  caliper precisely; photos may resolve the exact notch geometry. Four small tabs jut
  out for connectors per the photo estimate below — physical board confirms tabs
  exist but exact protrusion not re-measured yet.

| Tab | Location (Y range) | Protrusion | Stock use |
|-----|-------------------|-----------|-----------|
| Upper-left | ~16–37mm | ~3mm | 4-pin power (+5V GND GND +5V) |
| Upper-right | ~16–37mm | ~4mm | Same, mirrored (power daisy-chain) |
| Lower-left (stepped) | ~76–118mm | up to ~11mm | RJ45-style data IN + blue 2-pos screw terminal (SIGNAL/GND) |
| Lower-right | ~80–103mm | up to ~12.5mm | RJ45-style data OUT |

Implication for our board: the cavity tolerates up to ~12mm of overhang on the left and
right edges in those zones — usable room for our Micro-Fit connectors if 127mm proves
tight, but tabs are optional (stock uses them for bulky jacks we don't have).

## Mounting holes

- Photo estimate: centers ≈7mm in from all four edges (symmetric).
- **Physical (2026-07-10): hole diameter 4.5mm, center-to-center ~114mm in both X and
  Y.** Inset (hole center to board edge) is 6mm on two edges, 7mm on the other two —
  not symmetric like the photo estimate suggested. (Which-edges question closed
  2026-07-17: our board's hole positions were confirmed directly by a 1:1 printout
  fit-tested against the physical standoffs, making the exact inset split moot.) **Standoffs are
  plastic poles with retention tabs, not metal screws** — no grounded-screw
  consideration needed, this user's pad shows no metal at the hole edges.
- **Physical, refined (2026-07-12, calipers, re-measured twice): not a perfect
  square.** Left, bottom, and right hole-pair spacings are 114mm center-to-center;
  the top pair (top-left to top-right) is 113mm — the two top holes sit ~0.5mm
  further inboard than a symmetric 114×114 layout would put them. Corroborated
  independently via pixel measurement on `images/back_smx_pcb.png` (ruler-calibrated,
  ~14.7px/mm, cross-checked against the known 4.5mm hole diameter): same ~1–1.5mm
  magnitude of asymmetry seen, confirming this isn't caliper noise. Visual inspection
  by the user confirms it's the top two holes that are inset, not one top + one
  bottom. Our panel PCB layout updated to match (2026-07-12): top holes moved to
  X=57/170 (113mm apart), bottom holes stay at X=56.5/170.5 (114mm apart), Y
  unchanged on both rows.

## Standoff / mounting height

- **Physical (2026-07-10):** PCB bottom surface sits ~6mm above the floor (standoff
  height). Floor to the bottom of the pad's outer panel/platform is ~37.5mm total —
  budget **35mm** as the usable design height (leaves margin). PCB thickness 1.6mm.

## Edge connector positions (physical, 2026-07-10)

Measured from the top edge unless noted:

- Power IN: top-left, ~25mm from top edge.
- Power OUT: top-right, ~25mm from top edge (mirrors IN).
- INT: left edge, ~93mm from top edge.
- Data IN: left edge, ~103mm from top edge.
- Data OUT: right edge, ~95mm from top edge.

## FSR positions (physical, 2026-07-10)

Not exactly centered on each edge — each FSR sits inset **0–1mm from the edge**,
i.e. pushed as close to the panel edge as physically possible, roughly centered
along that edge's length but not precision-centered.

## LED lattice (25 LEDs, staggered 4–3–4–3–4–3–4)

Matches `docs/PROTOTYPE_LED_LAYOUT.md` wiring order. Regular staggered lattice,
centered on the board.

- Photo estimate: 7 rows, vertical pitch ≈16.6mm, row 1 at Y≈13mm; 4-LED rows pitch
  ≈33.5mm; 3-LED rows offset half; diagonal neighbor distance ≈23.5mm.
- **Physical (2026-07-10):** row 1 Y offset **11mm** from top edge. **Column pitch
  (center-to-center within a row): 34mm.** **Row pitch (center-to-center between
  adjacent rows): 17mm** (confirmed: row 1 to row 3 = 34mm = 2×17mm). *(Column
  pitch and row-1 offset superseded by the 2026-07-12 refinement below — 33.5mm
  pitch, 12.5mm top inset; the PCB layout uses the refined values.)*
- **Physical, refined (2026-07-12, calipers): column pitch 33.5mm** (not 34mm) —
  resolved by reconciling three independent readings (column pitch, left inset, right
  inset) that only summed exactly to the 127mm board width at 33.5mm; 34mm and
  33.75mm both left a 0.75–1.25mm residual. **Row pitch confirmed unchanged at
  17mm** — self-consistent with a symmetric 12.5mm top/bottom inset on a 127mm
  board (102mm / 6 gaps = 17mm exactly), so no change needed there.
  **Insets, not symmetric left/right: left 13.5mm, right 13.0mm** (0.5mm
  difference, mirrors the small asymmetries already found in mounting-hole
  spacing). **Top/bottom inset: 12.5mm, symmetric.** Panel PCB LED lattice
  updated to match (2026-07-12) — only the 25 LED footprints were moved; other
  placement (passives, clusters) left for a follow-up pass.

## INT wire color map

**Confirmed correct 2026-07-10** against the physical pad — matches the stock SMX
color map already adopted in `docs/BOM.md` (0=Red, 1=Orange, 2=Yellow, 3=Green,
4=Blue, 5=Brown, 6=Grey, 7=White, 8=Black). No changes needed.

## Open items (pending full teardown)

1. **PSU stud size** — for sizing the fork/spade lugs at the PSU end of the 12V
   column runs.
2. **Underglow data connector type/pinout at the old MCU** — gates the master
   PCB's underglow DATA-out connector choice (see `docs/BOM.md` master table,
   `docs/UNDERGLOW.md`). Related: the harness splice-point finding (leads crimped
   directly into a 12-pin Dupont housing at the stock MCU, no reusable
   intermediate connector — see CLAUDE.md Underglow section / project memory).
3. **Harness run lengths** — power hops (PSU → first panel per column, panel →
   panel), RS-485 links, and the 9 INT home-runs to the master — real
   measurements to replace estimates in `docs/BOM.md`'s wire/cable quantities.
   User's plan: possibly snake a single wire through the pad in the actual
   routing to get true lengths rather than estimating from photos/geometry.

Not answerable from external measurement alone — needs the full pad + harness
teardown.

## Other stock observations (photo estimate, not re-verified physically)

- MCU + crystal right-of-center, DIP switch bottom-right, 4 "FSR AMP" labeled zones
  (one per edge — stock amplifies FSR signals locally), bottom-center connector.
- Stock power is **+5V** on this Gen4 board (label visible) — ours is 12V, no conflict.
- The blue screw terminal labeled SIGNAL/GND is stock's interrupt/signal line — amusing
  validation of our screw-terminal INT plan.
