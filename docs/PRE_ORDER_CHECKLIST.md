# Pre-order checklist — dual-panel + master (rev 2.0, rescoped 2026-08-04)

> # 🔒 ORDERED 2026-08-18 — THIS CHECKLIST IS NOW A HISTORICAL RECORD
>
> **Both boards are at fab.** Nothing here is actionable any more; it documents how
> rev 1 was verified. Tag **`rev1-fab`**, artifacts in
> `hardware/fab-archive/rev1-2026-08-18/`. Any change from here is **rev 2**.

Run this top to bottom before paying JLCPCB. ⬜ is open, ✅ is closed.

> 📋 **`docs/ORDER_NOTES.md` is the companion to this file** — the text and
> answers you need *while the order form is open*: what to say about POFV and
> J305, and every DFM flag that is intentional. This file is the process;
> that one is the script.

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

### SMT DFM — run WITH BOM + CPL, all findings triaged 2026-08-17

| finding | Danger | verdict |
|---|---|---|
| Lead to hole distance | 72 | ✅ **deliberate via-in-pad** — makes POFV mandatory, see below |
| Pin left / right / outer edge | 28 / 28 | ✅ **fixed 2026-08-18** — pads widened to 0.23 mm = datasheet b max; confirmed cleared on the DFM re-run |
| Pin **inner** edge | 42 | ⚠ **ACCEPTED, do not fix** — pad inner edge 3.000 mm = pin inner edge at datasheet **L max 0.500**. Zero heel at worst case only; 0.100 mm at nominal. Adding heel measured to cause solder-mask bridging. Full argument + JLC reply in `docs/ORDER_NOTES.md` §8 |
| Lead area overlapping pad | 1 | RP2040 exposed pad; conservative, see below |
| component→board edge, through-hole ×4, pad spacing, clipped by outline, pin without pad, pin outer edge, missing hole | **0** | clean |

🚨 **POFV IS NOW MANDATORY, NOT A COST DECISION.** "Lead to hole distance" is the
**83 via-in-pad instances** from `9bf960f` ("via-in-pad pass on the brain").
Measured: **all 0.3 mm drill, all on the brain, none over the 0.5 mm fill limit**,
17 of them under U306. Without filling, solder wicks down those vias during
reflow and starves 83 joints including the RP2040's. This also sharpens the
question for JLC — ask them to **fill the 0.3 mm vias; the 44 at 0.6 mm drill do
not need filling and none of them sit in a pad.**

✅ **The RP2040 exposed pad is CORRECT at 3.2 × 3.2 mm.** Verified against
Raspberry Pi's own reference board (`Minimal-KiCAD.zip` →
`RPI-RP2040-MINIMAL_R3-S1.kicad_pcb`), which uses the same 3.2 mm EP. ⚠ A web
search claiming RP2040 needs a 5.6 × 5.6 mm EP is **wrong** — KiCad ships that
variant for other parts in the same body. Do not "fix" this.

✅ **The pin-edge findings were a pad-width difference on U306 — FIXED 2026-08-18,
pads widened to Raspberry Pi's 0.23 mm.** We were on KiCad's generic
`QFN-56-1EP_7x7mm_P0.4mm_EP3.2x3.2mm` with **0.875 × 0.20 mm** signal pads against
Raspberry Pi's official **0.80 × 0.23 mm**; 28 + 28 = **56 = exactly the pin
count**, which identified the width as the cause. Resolved by the user's pre-order
call rather than deferring to rev-B.

**What was changed, and what deliberately was not.** A local footprint
`dual-panel:QFN-56-1EP_7x7mm_P0.4mm_EP3.2x3.2mm_RPiPad` widens **only** the signal
pad width, 0.20 → **0.23 mm**. Pad **length stays 0.875 mm** (not RPi's 0.80) —
the extra toe overhang aids solderability and inspection, and shortening it would
have moved geometry the DFM never complained about. Pitch, pad centres and the
3.2 × 3.2 mm exposed pad are untouched. Pad-to-pad gap goes 0.20 → **0.17 mm**,
still far above JLC's floor.

⚠ **The widening had two knock-on effects; both are handled.**
1. **Zone refill is mandatory** — the wider pads produced three pad-to-`GND_Brain`
   clearance errors that exist only in stale fill polygons. A headless
   `ZONE_FILLER` pass cleared them (`GND_Brain` F.Cu 2423.787 → 2423.741 mm²).
2. **The `QSPI flash pour clearance` rule had to be scoped to zones.** It was
   written with no B-side condition, so it also policed track-to-pad, and the
   wider pad 51 left one +1.1VDC track at 0.1253 mm against the 0.13 mm rule.
   That is a routing gap, not a pour gap — and 0.1253 mm is still well clear of
   JLC's 0.09 mm floor. The condition now ends `&& B.Type == 'Zone'`, matching the
   rule's own stated purpose. **Verified still live:** raising it to 0.20 mm
   produces 101 QSPI-vs-`GND_Brain` violations, so it is genuinely holding the
   pour back.

⚠ **Edit the schematic INSTANCE, not the `lib_symbols` cache.** Changing the
footprint in both places raises a `lib_symbol_mismatch` ERC warning, because the
cached symbol then disagrees with the on-disk `MCU_RaspberryPi` library. The
instance override alone is what assigns the footprint, and is what the GUI would
have produced. Reverting the cache took ERC back to 0.

✅ **Re-verified after the change: DRC 0 · unconnected 19 · parity 90 · ERC 0** —
identical to the baseline. Gerbers confirmed to carry the new geometry (roundrect
apertures at 0.0575 mm corner radius with ±0.0575/±0.380 mm vertices = 0.23 ×
0.875 mm, present in both pad orientations).

✅ **`tht to smd` (11 Danger) does not apply to this order.** The pairs are
TP303→D303 (1.55), TP306→U308 (2.17), J213→D204 (2.01), J303→U303 (2.16),
TP208→D212 (2.17), TP304→U308 (**1.13**, closest), TP309→C327 (2.23),
TP310→X301 (2.83), TP301→U302 (2.39), and SW202's two ground lugs (1.69 to
R201). **Seven are bare test-point probe holes with nothing soldered into them**,
and the other four are hand-soldered by us. **JLC does SMD only on this order —
every through-hole part is hand-assembled**, so their THT-process clearance check
is inapplicable. Nothing is under 1.1 mm.

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
- ⬜ **THVD1450 (U308, C2671361)** — verify live stock at order time. It replaced THVD1429/`C1850236` on 2026-08-18; that part had only 60 units and was the BOM's tightest line.

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
- ✅ **Surface finish: ENIG. DECIDED — not an open aesthetic choice, and not
  about the logo.** An earlier version of this line said the finish mattered
  because the logo was exposed ground copper; **the logo is silkscreen** (board
  graphics are F.Silkscreen only — there is no copper/mask logo pair anywhere).

  **JLC's free OSP was evaluated 2026-08-18 and rejected.** OSP's advantage is a
  flat surface for fine pitch, but ENIG is equally flat, so there is no gain.
  Three properties of *this* board decide it:
  1. **30 test points that get bench-probed.** OSP is a thin organic film over
     bare copper; probing scratches through it and the copper oxidises. The gold
     stays probe-friendly indefinitely, and those test points are the whole
     bring-up plan.
  2. **All THT is hand-soldered by us, after JLC's SMD assembly**, across 20
     panels over an extended window. OSP solderability degrades within months of
     the bag being opened; ENIG keeps for years.
  3. **Spares and repairability** — 20 panels for 18 slots, meant to stay
     maintainable. OSP boards stored a year solder badly.

  Free OSP is a real saving on a board that ships and is never touched again.
  This is the opposite of that board.
- ⬜ **Assembly: both sides**, SMD only. The ~$25 double-sided delta is worth it.
- ⬜ **Epoxy filled & capped (POFV)** on the brain — decided 2026-08-04. **Ask JLC
  two things at quote time** rather than eating an engineering query:
  1. the POFV surcharge at 4 layers (never verified against a live quote);
  2. how POFV handles a board mixing fillable and non-fillable vias — **44 vias sit
     at 0.60 mm drill, above the 0.5 mm fill limit.** All power-distribution, none
     in a pad, so unfilled is electrically fine, but POFV is normally board-wide.
- ✅ **0.075 mm via annular ring — FIXED ON THE MASTER 2026-08-18.** The panel was
  moved to 0.60 mm pad / 0.150 mm annular earlier; **the master was never checked
  and still had 297 vias at 0.45 mm / 0.075 mm annular**, because DFM had never been
  run on it. JLC flags this. All 297 enlarged to **0.60 mm → 0.150 mm annular**,
  matching the panel. Cost: nothing — **DRC/ERC/parity/unconnected all stayed at 0**,
  and the gerber diff is a single aperture (`ADD24` 0.450 → 0.600) with drills
  untouched. ⚠ **A quote is not a DFM check** — the master "quoted fine" at 0.075 mm
  for months, which proved nothing.
- ✅ **Quantity: 20 panels, ONE run.** Fixed overhead is ~$93/order ($25 eng fee +
  $51.12 PCBA setup + $16.42 stencil) plus shipping, regardless of quantity — two
  runs = paying all of it twice. **Real qty-20 quote, 2026-08-18: PCB $174.47 +
  PCBA $375.41**, full breakdown in `hardware/dual-panel/panel/QUOTE-2026-08-18.md`.
  ⚠ **Do not estimate a quantity change by scaling linearly** — 18→20 PCBA cost
  **+$70.45**, not the ~$20 that dividing predicts, because component cost steps.
- ✅ **Master boards: $8.04 for 5** — confirmed on the real quote, unchanged since
  July. Board cost is negligible; **shipping dominates** ($82.01 JLC + $21.19 net
  LCSC = **$103.20**, 13% of the project). Combine the shipments — `ORDER_NOTES.md` §7.
- ✅ Slow build time (3–4 day assembly) — **$0**. The 2–3 day expedite is **+$49.26**
  and is not being taken.
- ⬜ Keep "Confirm Production file" / "Confirm Parts Placement" (~$1.50 total) —
  and actually respond to the DFM emails.

---

## 6. Final human pass

- ⬜ Page through JLC's gerber viewer (it renders the same artwork the fab uses):
  board outline and rail frame, mouse-bite tabs, layer order (F / In1=GND /
  In2=GND / B), silk name + rev + year, **and the silkscreen logo** — it is
  silkscreen, not a copper/mask pair, so check it on F.Silkscreen.
- ⬜ Sanity-check against the real 2026-08-18 quote: **merchandise $557.92**
  (dual-panel $549.88 + master $8.04). If a "qty 20" total lands near the old qty-5
  figure of **$297.38**, the quantity field did not take.
- ⬜ ⚠ **Remember the displayed total EXCLUDES "Advanced Options"** — bake $7.88 +
  cleaning $3.28 = **$11.16**, billed after review. Budget **$642.09**, not $630.93.
- ⬜ ⚠ **Editing the quantity WIPES both remark fields**, and possibly other
  options. Set quantity FIRST, then paste §9a/§9b, then re-verify the whole form.

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

- ~~Project logo~~ — **done, and it is silkscreen**, not exposed copper. The old
  exposed-copper flow and its sliver check no longer apply.
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

### LCSC part-identity audit — all 35 PCBA lines, 2026-08-18

**Every C-number was fetched from its live LCSC product page and compared against
the schematic value and footprint.** This is a different check from the ones above:
BOM↔CPL matching, `bom_census.py` reconciliation and "every line has an LCSC
number" are all *internal consistency* — none of them can catch a transposed digit
that points at a real but wrong part.

**33 of 35 match exactly.** Confirmed on the ones that carry design decisions:

| line | LCSC page says | verdict |
|---|---|---|
| ~~C1850236~~ → **C2671361** | was THVD1429DR (20 Mbps); **now THVD1450DR**, SOIC-8 | ✅ audited as correct, then **superseded 2026-08-18** on stock + cost — see below |
| C3743528 | **AP7361C-33ER-13**, SOT-223R | ✅ correct suffix — `-33E-` would be pin-reversed |
| C179173 | W25Q32JVSSIQ, **32 Mbit** SPI | ✅ = 4 MByte, not the 2 MB part |
| C5446699 | **WS2815B-V1**, 12 V, SMD5050-6P | ✅ |
| C7484 | SN74AHCT1G125**DBVR**, SOT-23-5 | ✅ single-gate, not the quad |
| C2040 / C113952 / C2827654 / C3235556 / C8020 / C82014 / C552867 / C6187 / C231329 / C20625731 | RP2040 · SMAJ5.0A (unidir, SMA) · USBLC6-2SC6 · LM66200DRLR · TAJB226K016RNJ · RVT1E471M1010 · PMEG3015EH,115 · AMS1117-5.0 · B3U-1000P · ABM8-272-T3 (12 MHz, CL 10 pF) | ✅ all exact |

All 0603 resistors resolved to the correct value at ±1 %, and every MLCC to the
correct capacitance, package and a voltage rating at or above spec.

#### ⚠ Two mismatches found — BOTH NOW FIXED

1. 🔴 **`C57112` — schematic said `10nF C0G 50V`, the linked part was X7R.**
   ✅ **RESOLVED: replaced with `C723749`** (Yageo CC0603JRNPO8BN103, 10 nF
   **25 V NP0 ±5 % 0603**, 2,513 in JLC assembly stock, $0.0282 → **$2.54** for
   the 90 JLC will draw). Identical 0603 footprint, so this was an LCSC-field edit
   only — no layout change, no DRC risk. The value string was updated to
   `10nF C0G 25V` to stay truthful about the rating.
   Original finding, for the record:
   Confirmed on **both** LCSC and JLC pages: Fenghua **0603B103K500NT**, 10 nF,
   **X7R**, ±10 %, 50 V, 0603. It is a JLC **Basic** part.
   **These are the four FSR ADC input filter caps** — C324 (FSR_East), C326
   (FSR_North), C329 (FSR_West), C330 (FSR_South), each sitting from its ADC net
   to GND. Value, rating and package are all fine; only the **dielectric class**
   disagrees with the schematic. See below.
2. 🟡 **`C52923` — schematic said `1uF X5R 16V`, the part is 25 V.** Samsung
   CL05A105KA5NQNC, 1 µF X5R ±10 %, **25 V**, 0402. Harmless — a higher rating
   than specified. ✅ **RESOLVED: the value string is now `1uF X5R 25V`.** The
   part was always correct; only the label was wrong.

#### Also noted

**`C3743528` (AP7361C-33ER-13) showed only ~215 in stock at LCSC** against a need
of 20. Not a problem, but it is the thinnest line in the BOM. Remember LCSC stock
is not JLC assembly stock — confirm in the matcher.

#### Why the C0G/X7R difference is worth a decision, not a shrug

Class II dielectrics (X7R) are **piezoelectric**; Class I (C0G/NP0) are not.
Mechanical stress on the board generates a voltage on the cap. **This is a dance
pad** — the panel is stomped on, hard, repeatedly, and the board flexes on its
standoffs at exactly that moment. The cap sits directly across the ADC input, so
any injected charge lands on the measurement node, **synchronously with the
footstep** — i.e. correlated with the event being measured, which is the one kind
of noise averaging cannot remove.

Magnitude is likely small (single-digit to tens of mV against a ~400 mV press
threshold at 0.8 mV/LSB), so this is **unlikely to cause false presses**. The
exposure is to **telemetry quality** — the live per-sensor FSR stream that is this
project's headline feature over stock SMX — and to calibration baselines.

**Neither absolute accuracy nor DC bias is the issue here** (a 50 V part at 3.3 V
barely derates, and the RC corner is ~1.6 kHz with the 10 k divider either way).
The dielectric class is the whole question.

### JLC ASSEMBLY stock — all 35 lines, authoritative, 2026-08-18

⚠ **Do not use LCSC stock, and do not use third-party mirrors.** This was learned
the hard way in one session: for `C389113`, the tscircuit mirror reported
**139,737** in stock, LCSC's page said **out of stock**, and JLC's own page said
**14**. The mirror was simply wrong, and a part was nearly ordered on it.

**The authoritative source is JLC's own API** (`componentLibraryType`: `base` =
Basic, `expand` = Extended; `describe` carries the dielectric, which is how the
C0G/X7R mismatch above was confirmed):

```sh
curl -s -X POST \
  "https://jlcpcb.com/api/overseas-pcb-order/v1/shoppingCart/smtGood/selectSmtComponentList" \
  -H "Content-Type: application/json" \
  -H "User-Agent: Mozilla/5.0" \
  -d '{"currentPage":1,"pageSize":5,"keyword":"C2671361"}'
```

**Result: every one of the 35 lines has stock. 14 Basic / 21 Extended.**
Re-run before ordering — these are point-in-time.

#### ✅ The tightest line was THVD1429 — and it has been designed out

The audit found **`C1850236` (THVD1429DR) with only 60 units** against a need of
20: 3× headroom on the BOM's only single-source part, and the one line where an
intervening order could have stalled the build.

**Resolved 2026-08-18 by switching to `C2671361` (THVD1450DR) — 5,056 in stock,
84× the headroom, and 3.6× cheaper.** The full engineering justification is in
`CLAUDE.md`; the short version is that the 1450 has an **identical SOIC-8 pinout**
(verified against both datasheets), keeps the open/short/idle failsafe, and is
**better on ESD** (±18 kV vs ±8 kV IEC 61000-4-2 contact) — the threat this product
actually documents — while giving up on-die IEC 61000-4-5 surge protection, which
TI's own datasheet scopes to lightning and industrial power-grid transients.

⚠ **The accepted trade: the RS-485 A/B pair has no external TVS** (D201 is on the
INT line), so the transceiver's own rating is the only bus protection.
**DECIDED 2026-08-18: ships as-is for rev A; an external TVS on A/B is a rev-2
candidate.** Purely additive — two parts, placement and routing on the carrier —
so nothing is lost by deferring it. ⛔ Not a rev-A item; do not re-open.

Applied to **both boards** — panel `U308` and master `U1` — and to the live LCSC
cart (5 pcs, $5.60). Both boards re-verified after the change: **dual-panel
0/19/90/0, master 0/0/0/0.**

Remaining stock picture, all comfortable:

| line | need | JLC stock | headroom |
|---|---|---|---|
| C82014 — 470 µF electrolytic | 20 | 401 | 20× |
| C3743528 — AP7361C-33ER-13 | 20 | 628 | 31× |
| C552867 — PMEG3015EH | 20 | 1,075 | 54× |
| C723749 — 10 nF C0G | 80 | 2,513 | 28× |
| C2671361 — THVD1450DR | 20 | 5,056 | 253× |
| everything else | — | ≥5,349 | ≥267× |

#### Choosing the C0G part — 25 V won, and why the first answer was wrong

Candidates, priced at the **real** quantity (80 needed + JLC's per-part loss
allowance), not at qty 1:

| LCSC | brand | V | JLC stock | qty | total |
|---|---|---|---|---|---|
| **`C723749`** ✅ | Yageo | **25 V** | 2,513 | 90 | **$2.54** |
| `C76710` | TDK | 50 V | 18,627 | 84 | $3.18 |
| `C85973` | Murata | 50 V | 350,270 | 88 | $3.61 |
| `C82282` | Fenghua | 50 V | 9,446 | 86 | $6.70 |

**C85973 was picked first, on stock depth, and that was wrong.** 2,513 against a
need of 90 is **28× coverage** — an entirely healthy line, not a thin one — so the
stock argument bought nothing and cost $1.07.

**There is also no engineering reason to prefer 50 V here.** Class I dielectrics
(C0G/NP0 — two names for the same thing) have **no voltage coefficient**, so unlike
X7R nothing is gained at bias. 25 V against a 0–3.3 V ADC node is **7.5× margin**,
against the 2× that is conventional practice.

⚠ **Lesson: price at the quantity you will actually buy.** The qty-1 prices made
the gap look like $1.02; the real tiered figure at 84–90 pieces reordered the list
(Fenghua, the "cheap" brand, is the most expensive by 2.6×).
