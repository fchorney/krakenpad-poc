# CAM review — JLC's production files vs. what we sent

Review of the production files JLC returned for approval, **2026-08-21**.

| | |
|---|---|
| Master | `2325257a_y28` — order code **Y28**, ×5 bare fab. **Already approved.** |
| Panel | `panel-gerbers_Y27` — order code **Y27**, ×20 with PCBA |
| Compared against | `hardware/fab-archive/rev1-2026-08-18/` (verifies clean, 4/4 SHA256 OK) |

**Verdict: both are faithful. Every change JLC made is standard CAM practice and
none of it alters the design electrically.**

⚠ **Scope.** This review covers the **fabrication artwork only** — copper, mask,
silkscreen, drill, and the process parameters in JLC's job files. It does **not**
cover BOM matching or parts placement; "Confirm parts placement" is a separate
JLC step against `panel-BOM.csv` / `panel-CPL.csv` and is not reviewed here.

---

## 1. How this was checked

Each JLC folder holds two trees:

| dir | what it is |
|---|---|
| `yg/` (`YG/`) | the gerbers **we uploaded**, plus JLC's analysis JSON |
| `ok/` | JLCCAM Pro v3.4.8 **CAM output** — what actually gets etched |
| `ok/*.tgz` | the full ODB++ job database, incl. an `orig` step (as received) and an `edit` step (after their engineers) |

Raw diffs of `ok/` are useless because JLC renumbers apertures and re-expresses
pours as merged surfaces. Three techniques were used instead:

1. **Semantic gerber compare** — resolve every flash/draw to its actual aperture
   shape, then compare as multisets. Immune to D-code renumbering.
2. **Rasterise and XOR** — render both sides into a common mm window with
   `gerbonara` + `cairosvg`, dilate one by 1 px to absorb rasterisation noise,
   and difference. Used at 12–60 px/mm depending on the question.
3. **ODB++ feature parsing** — read `steps/*/layers/*/features` and `tools`
   directly for drill type/size truth.

The coordinate transform from our KiCad frame to JLC's was derived from the via
positions, not from bounding boxes:

| board | transform (ours → JLC, mm) | residual |
|---|---|---|
| Panel | `X − 23.1959`, `Y + 171.5` | — |
| Master | `X − 91.0009`, `Y + 143.0` | **0.14 µm** over 297 vias |

⚠ **Resolution matters.** The master's pour change (§4) is a 90 µm effect and is
**invisible below ~25 µm/px**. An early 83 µm/px pass showed the panel "clean";
it was re-run at 25 µm/px to be sure the same thing wasn't hiding there. It
isn't — but the lesson is that a coarse raster diff can miss a real CAM edit.

---

## 2. Integrity — is JLC holding what we archived?

### Master: byte-identical ✅

All **14** gerber/drill files in `yg/` are byte-for-byte equal to
`master-pcb-gerbers.zip`.

### Panel: same board, different export ⚠

All 16 files differ — because **JLC has an earlier export than the archive**:

| | timestamp |
|---|---|
| JLC's copy (`YG/`) | `2026-08-18T08:28:44` |
| `fab-archive/panel-gerbers.zip` | `2026-08-18T10:05:44` |

Compared semantically, they are the same board:

| layer group | result |
|---|---|
| All flash/draw ops, every layer | **identical** — every pad, trace, via |
| Drills | **identical** — 1260 PTH + 115 NPTH, same tools, same positions |
| Mask, paste, silkscreen, Edge_Cuts | **identical** |
| Copper pours | 3–4 polygons re-tessellated; boundary moved **≤ 2.5 µm** |

The pour difference is a zone re-fill between the two exports. Rasterised and
XORed, every diff cluster is a **1-pixel-wide line** (e.g. 1198 differing px
across a 1198 px window) — an edge, not a shape change. Identical bounding
boxes.

**The RP2040 pad widening is present in JLC's copy**: `0.875 × 0.23 mm`, 56 pads
(28 + 28, two rotations). So the 08:28 export already contained the order-day
fix; only the zone fill was redone afterwards.

> ⚠ **`fab-archive/README.md` says "byte-exact artifacts that were
> manufactured."** For the master that is literally true. **For the panel it is
> not** — the archived zip is a re-export made ~1.5 h after the files JLC
> received. Geometrically identical, but a future diff against it will show all
> 16 files differing. Worth a caveat in that README so nobody debugging a board
> mistakes timestamp drift for a real change.

---

## 3. What JLC's CAM changed — both boards

All of this is standard and expected.

| change | detail | why it's fine |
|---|---|---|
| **Etch compensation** | trace apertures **+0.015 mm** (0.150→0.165, 0.200→0.215, 0.300→0.315, 0.500→0.515) | Copper grown 7.5 µm/side so the *finished* trace is our nominal width |
| **Drill compensation** | plated **+0.15 mm**, NPTH **+0.05 mm**, vias **+0** | **All 12 of our finished sizes are preserved exactly** (see §3.1) |
| **Non-functional pad removal** | inner layers only | Every removed island verified electrically isolated (§3.2) |
| **Mask pullback** | board edges, panel rails, mouse-bite tabs, M3 holes | Only **0.39 mm²** of ~419 mm² lands on any copper |
| **Silkscreen** | panel 0.45 mm² clipped near nudged vias; master 0 | Content unchanged. The 7× file growth (473 KB → 3.59 MB) is fonts converted to outlines |

Notably JLC added **no** order number or customer code to either silkscreen
(`print_customer_code = 0`, remark `[不加客编]`).

### 3.1 Drill verification

Read from ODB++ `tools` + `features`, with `.drill` attributes decoded
(`0` = plated, `1` = non-plated, `2` = via).

**Panel — every one of our 1375 holes and 6 slots is present, correct type,
correct finished size:**

| ours (finish) | JLC drill | type | count |
|---|---|---|---|
| 0.300 | 0.300 | via | 1063 |
| 0.400 | 0.550 | plated | 16 |
| 0.500 | 0.550 | non-plated | 107 |
| 0.600 | 0.600 | via | 44 |
| 0.750 / 0.800 | 0.900 / 0.950 | plated | 8 / 8 |
| 1.000 + 1.020 | 1.150 | plated | 113 |
| 1.300 | 1.450 | plated | 2 |
| 1.500 / 3.000 | 1.550 / 3.050 | non-plated | 4 / 4 |
| 3.200 | 3.350 | plated | 6 |

Sum = **1375** ✅. Slots: 2 × 0.600 mm long, 2 × 1.500, 2 × 0.800, all 0.600
finish plated — matching our six `G85` entries exactly.

JLC **added 39 holes of their own** (4 × 4.55 mm tooling, 29 × 0.65, 1 × 0.752,
1 × 0.852, 4 × 0.553 plated), all in the frame rail.

**Master — 382 features = our exact 382 holes.** JLC added none (it isn't
panelised in this job).

⚠ Note the `ok/drl` **gerber** is a partial export (592 of 1375 hits on the
panel). Do not read hole counts from it — use the ODB++ `features` file.

### 3.2 Non-functional pad removal

On inner layers, a through-hole that connects to nothing on that layer leaves an
isolated copper island inside its anti-pad. JLC strips these.

| board | removed | area per inner layer |
|---|---|---|
| Panel | 194 pads | ~292 mm² |
| Master | 66 pads | 150.6 mm² |

Verified safe by connected-component analysis of *our* artwork: **every removed
blob came from an isolated island, never from a plane.** Master `l2` goes from
1 plane + 66 islands to a **single clean plane**.

The handful of plane-touching diffs (≤ 0.075 mm²) are crescents where an
anti-pad ring moved with its nudged via (§3.3) — not removal.

### 3.3 Panel: 16 vias nudged

JLC's CAM shifted 16 of the 1063 × 0.3 mm vias by **35–141 µm**. Edge clearance
was already ≥ 0.8 mm before and after, so this is not an edge-clearance fix.

**They moved as complete objects** — drill and pads on `tl`/`l2`/`l3`/`bl` remain
concentric to **≤ 0.6 µm** at the new position. No eccentric vias, no broken
annular rings. Master vias were not moved (0.14 µm residual across all 297).

### 3.4 Panel: fiducials and QR

JLC removed 3 of our 4 corner fiducials (1.00 mm, plus their 2 mm mask openings)
and placed **4 of their own** inset from the corners. All in the **frame rail** —
neither board is touched.

`qrt`/`qrb` (12.2 × 3.0 mm) sit **1–4 mm from the panel edge**, i.e. in the rail.
This is JLC's SMT traceability mark, auto-appended to the order remark:
"在工艺边上需要添加SMT专用的二维码与明码" — SMT QR + plain code on the process
edge, both sides, 10 mm from the Mark.

---

## 4. Master only — pour clearance opened 90 µm → 180 µm

**The one substantive CAM edit.** JLC pulled the ground pour back from certain
traces, doubling the gap.

Measured on a scanline at 2 µm/px:

| location | our gap | JLC gap |
|---|---|---|
| tight run | **90 µm** | **180 µm** |
| comfortable run | 406 µm | 380 µm *(just etch compensation)* |

**90 µm is exactly JLCPCB's 3.5 mil minimum.** After +15 µm etch compensation it
would have gone sub-spec, so their CAM relaxed the marginal gaps rather than
raising a query.

| | top | bottom |
|---|---|---|
| Copper pulled back | 59.6 mm² | 15.0 mm² |
| Main pour | 3946.0 → 3890.2 mm² | 4163.8 → 4152.1 mm² |

**Verified non-destructive:** connected-component counts are **identical before
and after** — 59 on top, 62 on bottom — and there are **no copper slivers
< 0.05 mm²** in either version. Nothing was isolated or fragmented.

Electrically irrelevant at this board's speeds (INT, RS-485 at 1 Mbps, LED data).
The only real consequence is slightly lower trace-to-ground capacitance on the
affected runs.

**The panel shows none of this** — re-checked at 25 µm/px, its copper differs by
only 3.37 mm² (the 3 fiducials + via crescents), and JLC's total copper is
*greater* than ours (18072.8 vs 18047.3 mm²) purely from etch compensation.

> **Rev-2 note, not a rev-1 problem:** the master has pour-to-trace gaps sitting
> exactly on the fab minimum. JLC absorbed it silently this time. If the master
> is ever re-spun, raising the zone clearance above 0.09 mm would remove the
> dependence on a vendor's CAM being generous.

---

## 5. Ground planes are intact on both boards

The documented failure mode — a zone spanning both boards leaving the brain with
**no** ground plane — is **not present**. JLC's panel inner layers each contain
exactly two components:

| plane | area | X extent (our frame) |
|---|---|---|
| Carrier | 15706.5 mm² | 31.5 – 170.0 mm |
| Brain | 3446.1 mm² | 172.1 – 242.3 mm |

Both `l2` and `l3`, identical figures.

---

## 6. Process parameters — read from JLC's own job files

These are the authoritative build settings, not inferred from artwork.

**Panel** — `YG/4te.json`:

| parameter | value | matches order |
|---|---|---|
| `finished` | `沉金` — **ENIG** | ✅ |
| `via` | `过孔塞树脂` — **resin fill = POFV** | ✅ |
| `layers` / `thickness` | 4 / 1.6 mm | ✅ |
| `cu_inner` / `cu_outer` | 0.5 / 1.0 oz | ✅ |
| `color_sm` / `color_ss` | green / white | ✅ |
| `size_x` × `size_y` | 22.75 × 14.3 cm | ✅ |
| `isMadeSmt` | `True` | ✅ |
| quantity | 【20大片20PCS】【拼版方式：1*1】 | ✅ |

Our POFV instruction survived into `orderRemark`, translated:

> 请对 0.30 mm 钻孔直径的过孔（Vias）进行填孔并盖帽（Fill + Cap）处理。共有 83 个
> Via-in-Pad（盘中孔）需要进行填孔和盖帽处理。0.60 mm 钻孔直径的 44 个过孔无需填孔，
> 并且这些过孔均不位于焊盘内。

**Master** — `yg/erp_parameter_file.json`:

| parameter | value | matches order |
|---|---|---|
| `adorn_put` | **OSP** | ✅ (`ORDER_NOTES.md` §Order settings) |
| `adorn_bestrow` | `过孔塞油` — ink plug, **not** resin | ✅ "no POFV" |
| `is_smt` | `False` | ✅ bare fab |
| `stencil_layer` / `stencil_ply` | 4 / 1.6 mm | ✅ |
| `stencil_width` × `length` | 6.55 × 7.75 cm | ✅ |
| `qr_code_flag` | 0 | ✅ |
| `back_drill_flag`, `blind_via_hole_flag` | 0 / False | ✅ |

`fileset.json` confirms correct layer assignment — `GND_1 → l2`, `GND_2 → l3`,
`Edge_Cuts → gko`. No layer swap.

> ⚠ **`sk` is not the POFV layer.** Both boards carry an `sk` layer with one
> 0.45 mm feature per 0.3 mm via (panel 1063, master 297 — verified every plug
> lands on a via). That is **ink via-plugging**, which both boards get. POFV is
> proven by the panel's `via = 过孔塞树脂` parameter, *not* by `sk`. Do not read
> `sk` on the master as an unordered POFV charge.

---

## 7. Minor documentation nit

`ORDER_NOTES.md` and `fab-archive/README.md` both list the panel as
**227.57 × 143.10 mm**. That is the Edge_Cuts bounding box *including* the
0.1 mm outline line width. JLC routs the centreline, and their profile is
**227.468 × 143.000 mm**. Harmless — the order was placed on the larger figure,
which is conservative — but the two numbers should not be confused when checking
panel-size price breaks.

---

## 8. Reproducing this review

```sh
python3 -m venv venv && venv/bin/pip install gerbonara cairosvg numpy scipy pillow
```

Key gotchas:

- JLC's `ok/` gerbers are **2.6 inch** format (`%FSLAX26Y26`, `%MOIN`); ours are
  **4.6 mm**. `gerbonara`'s `to_svg(force_bounds=..., arg_unit=MM)` normalises
  both into a common mm window.
- `4te.json`, `erp_parameter_file.json` and `GBR_*.json` are **GBK-encoded**, not
  UTF-8. `json.loads(open(f, encoding='gbk').read())`.
- `erp_parameter_file.json` begins with an expiring signed Aliyun URL to an
  internal-only host; the useful parameters are in the same file below it.
- `gerbonara` warns "Arc is missing J value" on JLC's output (they emit `G03`
  with `I` only). Harmless, but it is why raster comparisons were cross-checked
  against aperture definitions and ODB++ features rather than trusted alone.
