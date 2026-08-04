# Review package — r/PrintedCircuitBoard, regenerated 2026-08-04

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
| `03-dual-carrier/` | Panel PCB — two-board design, LED/IO carrier | 127 × 127 mm core (139.3 mm across tabs) | 4 |
| `04-dual-brain/` | Panel PCB — two-board design, MCU brain | 70.9 × 62.8 mm | 4 |

The directory numbering has a gap because `02-panel-single/` — the retired
single-board panel — was removed on 2026-08-04 when the two-board split was
chosen. It is in git history at `1b41d1c` if it is ever wanted back.

At the top level: `POST-01-master.md` and `POST-03-dual-panel.md` are draft post
titles/bodies with the per-image captions each request needs, and
`RULES-CHECKLIST.md` is the pre-post audit.

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
* **Rule 7A: no design questions in a review request.** Post each board as its
  own review request — the drafts in `POST-*.md` are written that way. (The
  single-vs-dual question that used to sit here is settled; don't reintroduce it
  into a post.)
* **Rule 7A: one review per board per day**, and don't edit images mid-review.

## State as of the 2026-08-04 regeneration

These images are current with the boards as of that date. They include the
2026-08-03 shifter swap (quad SN74AHCT125 → single-gate **SN74AHCT1G125**), the
brain's via-in-pad pass, the clearance floor raised 0.09 → **0.127 mm**, and the
carrier's **third mounting hole**. Earlier image sets predate all of that.

Also reflected: silkscreen fixes (FSR East, RS-485 A/B on the bus test points, fab
placeholder removed), the **year** in every name/rev block, `master-pcb` refdes
contiguous from 1, and `dual-panel` re-annotated into per-sheet blocks — **carrier
2xx, brain 3xx**, every type contiguous, no duplicates across sheets, which is
exactly the multi-sheet exception the rules allow.

Verified the same day, from clean project copies with zones current:

| Board | DRC | Unconnected | Schematic parity | ERC |
|---|---|---|---|---|
| master-pcb | 1 ✱ | 0 | 0 | 0 |
| dual-panel | 0 | 19 | 90 ✱✱ | 0 |

✱ One `courtyards_overlap` between R4 and U2 — R4 sits deliberately under the
socketed Teensy, and the violation is an accepted exclusion, not an open defect.

✱✱ Both dual-panel numbers are expected and neither is a defect. The **19
unconnected** are the board-to-board mating gap KiCad cannot model. The **90
parity items are a `kicad-cli`-only artifact — the GUI reports none**; they are
net *naming*, not topology, and the exported netlist agrees with the PCB
pad-for-pad across all 577 pads. Be ready to explain both if a reviewer asks.

Still open:

1. The carrier's board-to-board headers **J210–J213** are bottom-mounted (the
   brain hangs underneath), so the top view shows only their plated holes. The
   header bodies are in `03-dual-carrier/12-3d-bottom.png` — post both sides.
2. `dual-panel.kicad_pro` registers the root sheet as
   `a1b2c3d4-…-000000000001` — inherited from the retired `panel-pcb`, which the
   sheets were seeded from — while `dual-panel.kicad_sch` says `…00000000d001`.
   Harmless day to day, but it is why "Update PCB from Schematic" refused to match
   after re-annotation and had to be recovered with a re-link-by-refdes round trip.
   Expect it again on the next renumber.
