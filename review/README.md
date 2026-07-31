# Review package — r/PrintedCircuitBoard, 2026-07-31

Export-only image set for every board in this project, formatted to the
subreddit's posting rules ([rule #8 / "please read before
posting"](https://old.reddit.com/r/PrintedCircuitBoard/comments/zj6ac8/please_read_before_posting_especially_if_using_a/),
[the "before you request a review, please fix these"
post](https://old.reddit.com/r/PrintedCircuitBoard/comments/1jwjhpe/before_you_request_a_review_please_fix_these/)).

Regenerate everything with:

```sh
tools/gen_review_images.py            # add --skip-3d to skip the slow raytraces
```

The script works on throwaway copies of the projects under `/tmp`, because
`kicad-cli` mutates `.kicad_pro` / `.kicad_sym` as a side effect. `hardware/` is
never written to.

## What's here

| Directory | Board | Size | Layers |
|---|---|---|---|
| `01-master/` | Master MCU board (Teensy 4.0 carrier) | 77.6 × 65.6 mm | 4 |
| `02-panel-single/` | Panel PCB — **single-board** design | 127 × 127 mm core (139.8 mm across the connector tabs) | 4 |
| `03-dual-carrier/` | Panel PCB — **two-board** design, LED/IO carrier | 127 × 127 mm core (139.3 mm across tabs) | 4 |
| `04-dual-brain/` | Panel PCB — **two-board** design, MCU brain | 70.9 × 62.6 mm | 4 |

At the top level: `POST-01-master.md`, `POST-02-panel-single.md` and
`POST-03-dual-panel.md` are draft post titles/bodies with the per-image captions
each request needs, `RULES-CHECKLIST.md` is the pre-post audit, and
`00-context-single-vs-dual.png` is a to-scale side-by-side of the three boards —
context for you, **not** a review image; don't attach it to a review request.

Per board:

| File | What it is |
|---|---|
| `01-schematic.pdf` | schematic, vector, light background |
| `01-schematic-1-*.png` | same page rasterised at 300 dpi for image hosts |
| `02-2d-top.png` | F.Cu + F.Silkscreen + Edge.Cuts |
| `03-2d-in1.png` | In1.Cu + Edge.Cuts |
| `04-2d-in2.png` | In2.Cu + Edge.Cuts |
| `05-2d-bottom-mirrored.png` | B.Cu + B.Silkscreen + Edge.Cuts, mirrored so bottom silk reads correctly |
| `06-2d-silkscreen-top.png` | F.Silkscreen + Edge.Cuts alone (silk legibility check) |
| `07-2d-top-no-pour.png` | as `02`, zones emptied — routing visible under the pour |
| `08-2d-in1-no-pour.png` | as `03`, zones emptied |
| `09-2d-in2-no-pour.png` | as `04`, zones emptied |
| `10-2d-bottom-no-pour-mirrored.png` | as `05`, zones emptied |
| `11-3d-top.png` | 3D, straight-down plan view, same orientation as the 2D top |
| `12-3d-bottom.png` | 3D, straight-down plan view from below (matches the mirrored 2D bottom) |

The no-pour set exists because a solid GND pour hides the routing — reviewers
routinely ask for copper without fills. They are plotted from a throwaway copy
of the board with every zone un-filled; `--check-zones` is deliberately *not*
passed for those, or KiCad refills them.

**Cropping note:** `--bg-color` paints the whole PDF *page*, so trimming that
raster crops to the page, not the board. Each 2D view is therefore plotted
twice — once on white paper to measure the true board extent, once on black —
and the black raster is cropped to the white one's trim box.

`dual-panel.kicad_pcb` holds both outlines in one file (they ship as one
panel). The script splits it into two boards by X coordinate before plotting so
each gets its own images — see `DUAL_SPLIT_X` in the script.

## How the images satisfy the rules

* Exported with `kicad-cli`, never screen-captured — no cursor, no OS chrome,
  no grid, no photos of a monitor.
* Schematics keep KiCad's light background and standard orientation.
* 2D plots carry only layers that exist in the gerbers: copper, silkscreen and
  the board outline. No `*.Fab`, no `*.Courtyard`, no `User.*`, no net names on
  traces, no pad numbers.
* 2D colour scheme is a purpose-built theme (`PCB-Review`, written to
  `~/Library/Preferences/kicad/10.0/colors/` by the script): white silkscreen
  and outline, red/green/orange/blue copper per layer, black background so
  clearance and holes read as voids.
* 3D views are orthographic straight-down plan views in the same orientation as
  the 2D images. No tilted-only views.
* File sizes are 0.1–3.6 MB PNG / PDF.

## Read before posting

* **Subreddit rule 1 and rule 7A restrict AI-assisted work** ("no AI content /
  AI designs", "minor AI help ok"). These boards were designed with substantial
  AI assistance. Decide how you want to handle that before posting — the
  moderators, not this file, get to judge where the line is.
* **Rule 5: no board-house names anywhere in the title or body.** The
  `JLCJLCJLCJLC` string on B.Silkscreen (all three boards) is the order-number
  placeholder and is visible in `05-2d-bottom-mirrored.png`; don't name the
  vendor in the post text if a reviewer asks about it.
* **Rule 7A: no design questions in a review request.** "Should I build the
  one-board or two-board version?" is a design question and belongs somewhere
  else. Post each board as its own review request instead — the drafts in
  `POST-*.md` are written that way.
* **Rule 7A: one review per board per day**, and don't edit images mid-review.

## State as of the 2026-07-31 regeneration

Fixed and reflected in these images:

- `panel-pcb` silkscreen — J4 now reads **FSR East**; the bus test points now
  read **RS-485 A/B**; the fab order-number placeholder is gone.
- All four boards carry the **year** in the name/rev block.
- `master-pcb` refdes are contiguous from 1 (the D1/R2 gaps are closed).
- `dual-panel` is re-annotated into per-sheet blocks: **carrier 2xx, brain 3xx**,
  every type contiguous, no duplicates across the sheets — which is exactly the
  multi-sheet exception the rules allow.
- Stitching vias added to `panel-pcb` (474 → 999) and `dual-panel` (464 → 1071),
  so the single-board and two-board layouts are now a like-for-like comparison.

Verified after those changes, with zones refilled first:

| Board | DRC | Unconnected | Schematic parity | ERC |
|---|---|---|---|---|
| master-pcb | 0 | 0 | 0 | 0 |
| panel-pcb | 0 | 0 | 0 | 0 |
| dual-panel | 0 | 19 | 93 | 0 |

dual-panel's 19 unconnected and 93 `net_conflict` are unchanged from before the
re-annotation — the 19 are the board-to-board mating gap KiCad can't model, and
the 93 are pre-existing net-name disagreements on the WS2815 VCC/PWR_FLAG nets.
Neither is a regression, and both are worth being ready to explain if a reviewer
asks.

Still open:

1. The carrier's board-to-board headers J12–J15 are **bottom-mounted** (the brain
   hangs underneath), so the top view shows only their plated holes. The header
   bodies are in `03-dual-carrier/12-3d-bottom.png` — post both sides.
2. `dual-panel.kicad_pro` registers the root sheet as
   `a1b2c3d4-…-000000000001`, which is `panel-pcb`'s root UUID, while
   `dual-panel.kicad_sch` says `…00000000d001`. Harmless day to day, but it is
   why "Update PCB from Schematic" refused to match after re-annotation and had
   to be recovered with a re-link-by-refdes round trip. Expect it again on the
   next renumber.
