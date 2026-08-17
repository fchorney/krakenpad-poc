# Pre-order checklist — dual-panel + master (rev 2.0, rescoped 2026-08-04)

Run this top to bottom before paying JLCPCB. ⬜ is open, ✅ is closed.

**Rescoped 2026-08-04.** Rev 1.0 of this doc was written around the single-board
`hardware/panel-pcb`, which was retired when the two-board split won. Where a
closed item below names a part rather than a board, it still holds — the parts are
the same physical parts, only the reference designators moved (carrier 2xx, brain
3xx). Anything panel-pcb-specific has been dropped; it is in git at `1b41d1c`.

**Build scope: 2 fully assembled pads** = 2 master PCBs + **20 panel assemblies**
(2 × 9 + 2 spares). One fab panel = one carrier + one brain, so that is **20
panels**. Every quantity here and in `docs/BOM.md` is sized to that; if the scope
changes, both docs move together.

---

## 0. The blocker

✅ **Pad + harness teardown — done 2026-08-08.** Every component and wire is out.
**J2's 2-pos screw terminal is confirmed final, not interim**: the underglow
strips terminate in a **JST SM 3P female** carrying 12V/DATA/GND, so our side is
an SM 3P male pigtail that sends 12V/GND to the pad's 12V distribution and DATA
alone to J2 pin 1 — no splice, and no change to the master PCB. See
`docs/UNDERGLOW.md`. (There are no "PSU lugs"; distribution is now the Wago
fan-out in `hardware/harness/12v-trunk.yml`.)

Remaining teardown follow-ups, none of which block the board order:

- ✅ **SM 3P pin order resolved 2026-08-08: pin 1 = GND, pin 2 = DATA,
  pin 3 = 12V** — the *reverse* of the earlier assumption. Building to the old
  order would have put 12V into the DATA pin.
- ✅ **Underglow connector — RESOLVED 2026-08-16.** We *do* build this side:
  removing all SMX wiring takes the stock `SMR-03V-B` with it, leaving only the
  strips' moulded plug to mate. **Use a pre-made 3-pin SM 2.5 LED-strip pigtail
  pair (22 AWG, on hand)**, not loose contacts — `SMM-003T-P0.5` is 28–30 AWG
  and cannot crimp our wire. `SMR-03V-B` (LCSC **C157907**) is stocked as a
  hand-made fallback. Pinout **1 = GND, 2 = DATA, 3 = 12 V**. Cable-side either
  way, so it never blocked the board order.
- ✅ **"PSU stud size" is moot** — the stock PSU is a brick with one JST YL 2-way
  output and no terminal block or ground stud. The 12V star point is physically
  the DC-DC converter's input screw terminals.
- ✅ **Stock harness fully documented 2026-08-08** — 12 WireViz drawings plus a
  one-page topology map in **`stock-smx/`**, wall socket through to the FSRs,
  with per-run lengths. That tree is kept deliberately separate from our own
  replacement harnesses in `hardware/harness/`.

✅ **Source the 8 board-to-board interface connectors — DONE 2026-08-16.**
Carrier headers **J210–J213** = LCSC **C5383116** (HanElectricity 2541WV-08P,
1×8 male 2.54 mm, **6 mm mating pin / 3 mm solder tail**, gold, 3 A/pin).
Brain sockets **J301–J304** = LCSC **C7509515** (CONNFLY DS1023-1x8SF11, 1×8
female, 8.5 mm body, gold). They are hand-soldered, so they are a cart item, not
a PCBA line.

> ⚠ **QUANTITY CORRECTED 2026-08-18.** This read *"50 of each ordered against a
> need of 36"* — **36 is the ONE-PAD figure** and the cart had been built to the
> 50. The scope is **20 panel assemblies**, so the need is **4 × 20 = 80 of
> each**. Now **100 / 85** in the cart. Same stale per-pad number appeared in
> `hardware/harness/PARTS.md`; both are fixed.

**⚠ Carries one consequence: the M3 spacer must be 12 mm.** These parts measure
2.54 + 8.50 = **11.04 mm** board-to-board, against the 10.75 mm the mechanical
stack assumed — so an 11 mm spacer no longer clears, by 0.04 mm, and would let
the connector plastics take the clamping load. ✅ **12 mm F-F is on hand**
(2026-08-16); the only open question is having **60** of them. See
`docs/DUAL_PANEL.md` → "Mechanical stack".

Original constraint, retained because it is what made these parts the right
ones:

**Type confirmed from `dual-panel.kicad_pcb` 2026-08-16 — they are ordinary
2.54 mm parts, nothing exotic:** `PinHeader_1x08_P2.54mm_Vertical` on the
carrier (B.Cu), `PinSocket_1x08_P2.54mm_Vertical` on the brain. Four of each per
panel, 32 pins per side. Searchable on DigiKey/LCSC as plain 1×8 headers and
sockets; the pitch and pin count are not the hard part.

Constraint most listings don't state: **6.0 mm mating pin with a ≥3.0 mm
solder tail** (the standard 11.6 mm total pin). Separation is set by the two
plastics meeting, not by pins bottoming out, and some "short" headers trim the tail
instead of the mating end — which leaves nothing to solder through the carrier.

✅ **INT cable length — CLOSED 2026-08-16 at 9.3 m/pad**, from the stock record
(`stock-smx/harness/panel-signal-lines.yml`, nine home runs of 60–150 cm).
**Purchased**: RS-485 and INT now share one **50 m** reel of 22 AWG 2-core
shielded RVSP against a 27.0 m two-pad need.

⬜ **INT cable OD check on arrival** — conductor insulation must be **1.30–1.85 mm**:
a floor for the JST XH contact and a ceiling for the Micro-Fit terminal, since
one cable now feeds both. Do this **before crimping 204 contacts**. It is the
only spec that can sink this cable and it is rarely listed by sellers.

---

## 1. Fab package

The panel is a **generated artifact, like gerbers** — gitignored, never
hand-edited. Regenerate both steps; the second overwrites the first's output.

```sh
cd hardware/dual-panel/panel
python3 gen_panel.py     # joins carrier + brain to a rail frame with mouse bites
python3 gen_fab.py       # gerbers zip + JLC-format BOM + CPL into production/
```

✅ **REGENERATED AND VERIFIED 2026-08-18.** The previous package was from
**Jul 30** and predated the Aug 4 board edit (third mounting hole, hole refdes,
via fill/cap) *and* the Aug 17 U303 fix — it would have been fabbed without the
third mounting hole.

```
carrier : 16 tabs   N=3 S=3 E=7 W=3     ← expected shape, matched
brain   :  6 tabs   N=0 S=0 E=3 W=3     ← expected shape, matched
substrates 2 · tab cuts 22 · contiguous: YES — one piece
panel 227.57 × 143.10 mm (326 cm²) · 291 footprints
CPL 115 placements · BOM 35 lines · every line carries an LCSC number
6 non-part footprints dropped (fiducials/tooling)
```

The brain gets no N/S tabs; that is a KiKit partition-line limitation, not a
setting, and it is documented in `hardware/dual-panel/panel/README.md`.

⚠ **KiKit is NOT importable from KiCad's Python directly** — it lives in
`~/.kikit`, so both generators need the path prefix or they fail with
`ModuleNotFoundError`:

```sh
KPY=/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3
cd hardware/dual-panel/panel
PYTHONPATH=~/.kikit "$KPY" gen_panel.py
PYTHONPATH=~/.kikit "$KPY" gen_fab.py
```

🚨 **A FAB BLOCKER WAS CAUGHT HERE 2026-08-17 — the brain had no ground pour.**
The first regenerated package (and the Jul 30 one before it) put the brain on the
panel with **zero GND copper on all four layers**. Root cause: `GND_Brain` was
deleted by `96f43c0` and `GND_Planes` stretched to cover both boards instead. That
fills fine in KiCad, so **every existing check passed** — DRC, ERC, parity, and a
human review. KiKit clips a spanning zone to one board and the other gets nothing.

Fixed by recreating `GND_Brain`, and `gen_panel.py` now **fails the build** if any
board lacks a GND pour on any copper layer. Expected passing output:

```
  ground pour per board (mm2):
    carrier  F.Cu=13421  In1.Cu=15656  In2.Cu=15656  B.Cu=15093
    brain    F.Cu=2412   In1.Cu=3418   In2.Cu=3418   B.Cu=3249
```

⚠ **If either row shows zeros, do not order** — the boards will arrive with no
ground plane. See `docs/DUAL_PANEL.md` → "Layout and routing".

✅ **Package contents spot-checked**: D301/D302 (DNP) absent from both BOM and
CPL while D303 is present; the 30 test points filtered; 16 gerber files (4 copper
+ paste/silk/mask pairs + Edge.Cuts + separate PTH/NPTH Excellon + 2 drill maps +
gbrjob). `In1_Cu` and `In2_Cu` come out byte-identical in size, corroborating
that **both inner layers are solid GND** and no 12V plane exists.

✅ **Master package: NO regeneration needed, verified rather than assumed.**
Regenerated to a scratch dir and compared layer-by-layer against
`hardware/master-pcb/production/`: **all 11 files are content-identical.**

⚠ **When diffing gerbers, strip `%TF.CreationDate` as well as `G04` comments.**
It is an X2 *attribute*, not a comment, so a comment-only filter leaves a
timestamp in and every layer falsely reads as changed — that produced a wrong
"master package is stale" verdict on 2026-08-18 before it was caught. The drill
files being identical while all nine gerbers "differed" was the tell.

⬜ **Upload as a customer panel** ("panel by customer"), **not** as a single board
— JLC's own panelization only arrays one design, and two different boards must be
supplied this way. Doing that is what makes it *one* order: one engineering fee,
one PCBA setup, one stencil, one shipment.

⬜ **Assembly scope is SMD only.** Through-hole — every connector, both switches,
and the eight interface headers/sockets — is hand-soldered. `gen_fab.py` filters
THT, the 30 test points (bare plated holes whose "value" is a net name, so they'd
become 30 unmatched BOM lines), and KiKit's mouse bites / tooling holes / fiducials.

⬜ **Master board**: bare fab only, no PCBA — hand-assembled from LCSC parts. Its
shopping BOM regenerates with `python3 tools/gen_bom.py --board
hardware/master-pcb/master-pcb.kicad_pcb`.

⬜ Working tree committed and pushed.

---

## 2. Design-file state

✅ **RE-VERIFIED 2026-08-18** from clean project copies, after the U303 pad edit.
Counts are **identical to the 2026-08-04 baseline** — the edit introduced nothing.

| board | DRC | unconnected | parity | ERC | exclusions |
|---|---|---|---|---|---|
| master-pcb | 1 ✱ | 0 | 0 | 0 | 0 DRC / 0 ERC |
| dual-panel | 0 | 19 ✱✱ | 90 ✱✱✱ | 0 | 0 DRC / 0 ERC |

✅ **U303's SOT-223 tab is FIXED and the fix is in the copper.** The tab pad now
carries `zone_connect 2` (solid) into the `+5VDC_AMS` pour, replacing the 4×0.5mm
thermal spokes that were throttling the AMS1117's stated heatsink path.

⚠ **A `zone_connect` change only reaches the gerbers if the zone was refilled
afterwards** — the fill polygons are stored in the `.kicad_pcb`, so an unrefilled
board keeps the old spokes in the artwork while the *rule* reads solid. Verified
by refilling all 6 zones headlessly and comparing areas: **zero change**, so the
saved copper already matches the solid-connection rule. Worth repeating after any
future pad/zone/rule edit:

```python
# kicad python; threshold in nm^2
before = {…z.GetFilledPolysList(layer).Area()…}
pcbnew.ZONE_FILLER(b).Fill(b.Zones())
# any area delta ⇒ the saved fill was stale
```

Both `drc_exclusions` and `erc_exclusions` are genuinely **empty** — nothing is
suppressed by exclusion. Two rule severities are set to `ignore` on both boards,
and both are cosmetic library-naming checks: `footprint_filters_mismatch` and
`footprint_type_mismatch`.

✱ One `courtyards_overlap`, R4 against U2 — R4 sits deliberately under the socketed
Teensy. Accepted, not a defect.

✱✱ The permanent board-to-board mating-gap floor. KiCad cannot model "these mate
mechanically"; each was confirmed to be a genuine crossing net.

✱✱✱ **A `kicad-cli`-only artifact — the GUI reports none.** Net *naming*, not
topology; the exported netlist agrees with the PCB pad-for-pad across all 577 pads
and gerbers carry no net names.

⬜ **Re-run all three checks after any edit.** Geometric DRC, `--schematic-parity`,
and ERC are three separate checks and parity is the easy one to forget.

⚠ **`kicad-cli` mutates `.kicad_pro` / `.kicad_sym` as a side effect.** Run checks
on a scratch copy — and copy the **whole project set** (`.kicad_pro`, `.kicad_dru`,
`.kicad_prl`, `fp-lib-table`, `.pretty`), or `${KIPRJMOD}` won't resolve and the
counts differ.

### JLC DFM run — 2026-08-17, findings and what was done

Ran JLC's own DFM checker on `panel-gerbers.zip`. Reports are in `tmp/`.

✅ **Annular ring (100 Danger) — FIXED.** Vias were **0.3 mm drill / 0.45 mm pad
= 0.075 mm annular**, JLC's *absolute* minimum for the class. JLC recommends
**≥0.15 mm** against drill-registration breakout, so their tool flagged all of
them. Vias are now **0.60 mm pad = 0.150 mm annular**, meeting the recommendation.

- **Drills are unchanged** (1063 @ 0.3 mm, 44 @ 0.6 mm), so current capacity and
  the POFV story are untouched. ⚠ Do not confuse the two: the **0.5 mm POFV
  limit is a DRILL limit**, not a pad diameter.
- Copper pour lost **55 mm² of 72,756 — 0.076%**. Nothing else moved.
- **Three vias stay at 0.45 mm** because growing them violates clearance:
  `VBUS` @ (216.23, 85.24) against U305 pad 2, and GND vias @ (176.69, 27.18)
  and (176.67, 151.95) against the board edge. Shrinking those three instead of
  nudging them gives **0 DRC violations and no re-routing**. The netclass is
  0.6 mm so new vias inherit the good size; the DRU `via_diameter` minimum stays
  at 0.45 mm only so these three pass.

⬜ **Trace spacing (5 Danger at 0.09 mm) — DELIBERATELY NOT FIXED.** 500 of the
508 sub-0.15 mm gaps are **copper-pour-to-track**, not track-to-track: the DRU's
blanket `(constraint clearance (min 0.09mm))` **replaces** the Default netclass's
0.2 mm, so pours back off only 0.09 mm. Raising it looked like a free win — but
**every value tested above 0.09 mm (0.10, 0.11, 0.12, 0.125) orphans a copper
island and produces a `starved_thermal` on J211 pad 5.** 0.09 mm is JLC's stated
minimum, so this is *at* spec rather than under it, and the real routing is
comfortably clear (tightest track-track 0.1266 mm, track-pad 0.1328 mm). Left
alone as the lesser risk.

🟢 **Ignore:** silkscreen-over-pad/hole (96) is cosmetic — JLC clips it.
Negative soldermask expansion (62) is normal. Fiducials reported "null" but **6
exist** (`KiKit_FID_T/B_1-3`); JLC's detector just missed KiKit's. Slot width
0.5 mm (6) sits at JLC's minimum, same "at limit, not under" as the vias.

⚠ **The SMT DFM report is not trustworthy and should be re-run at order time
WITH `panel-BOM.csv` and `panel-CPL.csv`.** It was run on the gerbers alone, so
JLC had no placement or part data and inferred components from paste/silk — note
the synthesised `component_v_top`/`component_v_bottom` layers, which are not in
our 16-file zip. Its "component to board edge = 0 mm" does not reproduce: the
closest SMD pad to any edge, panel rail and mouse-bite cutouts included, is
**0.891 mm** (a mounting-hole pad), and the nearest real part is 1.84 mm.

### The two DRU subtleties, both still live

⬜ **J305's USB-C holes sit at exactly 0.45 mm edge-to-edge — zero margin against
JLC's published multilayer floor.** Verified 2026-08-04 by raising the rule to
0.5 mm on a copy: **14 violations come straight back**, all on J305. The
`pad to pad clearance (with hole, different nets)` rule is deliberately relaxed to
0.45 mm; the generic `hole to hole clearance (different nets)` rule stays at 0.5 mm
and still guards via↔pad and every other non-pad-pair hole.
- **The two rules are not interchangeable** — both carry a `hole_to_hole`
  constraint, but the pad-to-pad rule is more specific and appears later, so *it*
  governs J305. Don't "simplify" them into one.
- **Keep J305 on the DFM-confirmation list at order time.**

⬜ **`.kicad_dru`'s "Minimum Trace Width and Spacing" rule carries
`(constraint clearance (min 0.09mm))`, and in KiCad a custom rule *replaces* the
netclass value rather than acting as a floor** — so it silently overrides the
Default netclass's 0.2 mm board-wide. Proven by deleting just that constraint: 77
violations appear, actual range 0.0900–0.1993 mm. The routing was separately raised
to a 0.1266 mm minimum, so this is not currently biting, but the rule reads like a
fab-capability template and does not say what it appears to say. If 0.2 mm should
ever be enforced, drop the `clearance` constraint (keep `track_width`) and let the
netclass govern.

---

## 3. JLC upload & BOM matching

- Files: `hardware/dual-panel/panel/production/panel-gerbers.zip`,
  `panel-BOM.csv`, `panel-CPL.csv`.
- ⬜ All BOM rows **confirmed** (checkbox ticked). Rows left "to be confirmed" are
  silently **not assembled**. Target: every line checked — including the debug LED
  D202, unless deliberately dropped. Decide before ordering.
- ⬜ Any substitution rows (yellow ⚠) reviewed before ticking.
- ⬜ Stock check: if a matched part shows insufficient stock, re-pick and write the
  new C-number back into the schematic's LCSC field afterward.
  ("Qty" = pieces your order needs; "My Inventory" 0 is normal.)
- ⬜ **D301/D302 (PMEG3015EH) are DNP** — confirm the generated BOM/CPL **exclude**
  them while their footprints stay on the board. The hand-solder Schottky-OR rescue
  depends on those pads existing.
- ⬜ **D201 (SMAJ5.0A, LCSC C113952)** is an *extended* part, so it adds a
  feeder/handling line. Confirm live stock and re-pick if short — C87074 (Diodes)
  and C98802 (ST) are the same part in the same DO-214AC body.
- ⬜ **THVD1429 (U308, C1850236)** — verify live stock at order time.

---

## 4. Placement preview (JLC order page)

- ⬜ **Rotation/polarity sweep.** JLC's renderer uses their library's tape-zero
  orientation, not KiCad's; a 90°/180° preview error is a real placement error.
  Check pin-1/polarity on: **U306** (RP2040, QFN corner), **U301–U305/U307/U308**,
  **X301**, **D303**, **D202**, **C308** (tantalum stripe), **C201**
  (electrolytic), **D201** (SMA TVS cathode band), and **one WS2815 of each
  rotation group** — a library error there repeats ×25.
- ⬜ The WS2815s use our custom PLCC6 footprint; JLC may render a generic body.
  Orient by pads against the part-detail photo, not the render. **The row of 3 is
  intentionally 180° from the row of 4** — that is the serpentine layout, not a
  placement mistake.
- ⬜ Confirm D301/D302 are **absent** from the placement preview.
- ⬜ Fix rotation issues in their preview UI (select + rotate), not by re-uploading
  the CPL.

---

## 5. Order options

- ⬜ 4-layer, both boards on one customer panel (~228 × 143 mm, ≈326 cm²).
- ⬜ **Surface finish**: HASL = silver exposed-copper logo, ENIG (+$) = gold. The
  logo is exposed *ground* copper, so the finish is visible. Aesthetic choice —
  decide deliberately.
- ⬜ **Assembly: both sides**, SMD only. The ~$25 double-sided delta is worth it.
- ⬜ **Epoxy filled & capped (POFV)** on the brain — decided 2026-08-04. **Ask JLC
  two things at quote time** rather than eating an engineering query:
  1. the POFV surcharge at 4 layers (never verified against a live quote);
  2. how POFV handles a board mixing fillable and non-fillable vias — **44 vias sit
     at 0.60 mm drill, above the 0.5 mm fill limit.** All power-distribution, none
     in a pad, so unfilled is electrically fine, but POFV is normally board-wide.
- ⬜ **0.075 mm via annular ring** — partially answered: the master quoted fine at
  0.075 mm on a standard 4-layer build. Confirm it holds for this panel.
- ⬜ **Quantity: 20 panels, ONE run.** Fixed overhead is ~$97/order ($25 eng fee +
  $51.12 PCBA setup + $16.42 stencil + $4.93 storage) plus ~$40 shipping,
  regardless of quantity. Two runs = paying all of it twice. Reference: the qty-5
  dual-panel quote came to **$297.38** ($89.01 PCB + $199.67 PCBA + $8.70
  advanced) — full breakdown in `hardware/dual-panel/panel/QUOTE-2026-07-31.md`.
- ⬜ **Master boards: ≈$8 for 5.** Board cost is negligible; **shipping dominates.**
  Batch master + panel into one shipment if the teardown allows.
- ⬜ Slow build time (3–4 day assembly); expedite was +$49 for one day.
- ⬜ Keep "Confirm Production file" / "Confirm Parts Placement" (~$1.50 total) —
  and actually respond to the DFM emails.

---

## 6. Final human pass

- ⬜ Page through JLC's gerber viewer (it renders the same artwork the fab uses):
  board outline and rail frame, mouse-bite tabs, layer order (F / In1=GND /
  In2=power / B), silk name + rev + year, logo copper/mask pair intact.
- ⬜ Sanity-check the total against the qty-20 band. The recorded qty-5 baseline is
  $297.38 — if a qty-20 quote comes in near *that*, the quantity field is wrong.

---

## Closed — do not redo

**Parts settled from vendor data, nothing to verify on arrival:**

- ✅ **Screw terminals** (carrier **J214**, master **J2**) — KANGNEX
  WJ500V-5.08-2P, LCSC **C8465**, on the vendor's own land pattern
  `TerminalBlock_WJ500V-5.08-2P` (1.30 mm holes, 2.00 mm pads). A 2-position screw
  terminal is two identical clamps with no polarity or keying, so it cannot be
  fitted the wrong way round; what matters is that the **silkscreen says which
  clamp is which**, and those labels were added 2026-07-26 on both boards.
- ✅ **DIP switches** — carrier **SW201** Zhongdi DS-04 (**C52177925**), 8 pads /
  2.54 mm / 7.62 mm rows; master **SW1** DORABO DS-3P-BU (**C46595747**), 6 pads,
  same geometry. Both matched stock KiCad footprints exactly.
- ✅ **Termination switch SW202** — footprint `dual-panel:SW_SS22E01L5` built from
  C609835's own EasyEDA data; pole grouping user-confirmed against the datasheet
  (2/5 = pole commons, 1/4 left, 3/6 right, mounting lugs = pads 7/8 → GND).
- ✅ **RN1** (master) C840655 4610X — pulled SIP-10 2.54 mm, identical to
  `R_Array_SIP10`.
- ✅ **U304 (LM66200)** — LCSC's own footprint is the 8-pin SOT-583 DRL, matching
  the KiCad `SOT-583-8` the board uses to ~0.1 mm of fillet allocation. No change.
- ✅ **C201 can-vs-pads** — vendor land pattern (C82014) confirms a 10.0 mm can
  against our 5.0 mm body radius. Our pads are taller and sit 0.5 mm inboard,
  covering ~88% of the vendor land area, and the part self-centres because both
  pads are offset equally. **Deliberately not "improved"** — the neighbouring cap's
  bounding box is 0.1 mm away.
- ✅ **FSR PHR-2 mate** — confirmed by the user against a real FSR lead.

**WS2815, closed:**

- ✅ LCSC **C5446699** confirmed = WS2815B-V1, the exact part of the WS2815B-V1
  V2.0 datasheet (VIH abs 2.7 V min / input abs-max 5.7 V). This closed the
  reviewer's "must level-shift to 12 V" finding — 12 V would violate abs-max.
- ✅ **Per-LED pin-1 caps (C202–C226) are vendor-sanctioned.** Two Worldsemi
  documents that bracket our revision in time — the 2018-era WS2815 doc and
  WS2815B-V3 — both give pin 1 verbatim as *"VCC … IC POWER SUPPLY, Suspended or
  connected with a filter capacitor to GROUND."* Our V1 V2.0 doc is the outlier.
  A 100 nF is no DC load, so if V1 is truly NC the cap is merely inert. **No
  per-LED VDD decoupling is wanted** — the channels are constant-current, so there
  is no per-pixel switching transient to decouple. Leave them as drawn.
  ⚠ V3 silicon uses VIH = 0.7·VDD (~3.5 V) vs V1's 2.7 V absolute. The 5 V shifter
  clears both, but **don't accept a V3 substitution silently.**
- ✅ **5 V bench test WAIVED 2026-07-26** — not worth buying WS2815s to test. The
  datasheet confirm is the substantive check and VIH 2.7 V min is a spec guarantee.
  Residual risk accepted: if a bring-up board shows flaky LED data, the shifter rail
  is the first suspect.

**Deliberately dropped (user's call — do not re-raise as findings):** the remaining
`review/RULES-CHECKLIST.md` ⚠ items — no colour called out on the debug LED, SW202
not marked DPDT, the panel-ID DIP not marked 4P, connector family/pitch missing next
to the FSR connector symbols.

## Deliberately NOT blocking the order

- Project logo (waiting on artwork; add via the verified exposed-copper flow +
  sliver check when it exists).
- Accepted-for-rev-A review items — U303 thermal, +5 V rail margin,
  INT-into-dead-panel, hot-plug/SI/ADC-on-B.Cu. These are **bring-up measurements**,
  not order blockers.
- Cosmetic: 7 sub-2 µm track stubs and ~109 board-wide sub-0.15 mm slivers. **Do
  not chase.** Of 109 short tracks, 102 are mid-route jogs with copper on both
  endpoints — deleting them breaks real routes.

## Post-order

- ⬜ Respond to JLC DFM/engineering emails same-day (they hold the order).
- ⬜ On arrival, before mounting in a pad: visual QA against the rotation list, then
  bench bring-up — USB-only first (3.3 V rail, BOOTSEL/flash enumeration), then 12 V
  (U303 temperature at real load, 5 V rail margin, 12 V-sense threshold, FSR ADC
  noise floor, WS2815 chain, RS-485 loopback, INT line).
- ⬜ Mate-and-solder order for the interface: assemble both headers into their
  sockets, mate the boards, **then** solder the second connector's pins with the
  stack held together. Four connectors must align in X, Y *and* rotation at once.
- ⬜ **Never omit the M3 spacer.** Without it, tightening pulls the brain into
  the carrier and the connectors absorb the entire clamping force.
  **Buy 11 mm AND 12 mm and measure** — the connectors sourced 2026-08-16
  (C5383116 + C7509515) stack to 11.04 mm, so 11 mm no longer clears by
  0.04 mm. See `docs/DUAL_PANEL.md` → "Mechanical stack".
