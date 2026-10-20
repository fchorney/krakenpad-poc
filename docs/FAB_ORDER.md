# Fab order procedure

How to take a board revision from "design done" to "ordered", distilled from the
rev-1 order of 2026-08-18. The full rev-1 record (every finding, every quote,
every email) is in `docs/rev1-order/`. This file keeps only what applies to
the next order.

On rev 1, almost every step below found something on boards that were already
called "order-ready". Run every step, including the ones that passed last time.

## 0. Scope first

Every quantity derives from the build scope, so fix it before touching a cart.
Rev 1 was sized for **two pads: 20 panel assemblies (2 × 9 + 2 spares) and 2
masters** (5 master boards fabbed, JLC's minimum). Mixing per-pad and two-pad
figures caused two live cart shortages on rev 1. Label every quantity column
with its unit.

## 1. Regenerate the fab package

`hardware/**/production/` is a gitignored build artifact. Regenerate it from the
boards for every order; never trust a package older than the last board edit.
(Rev 1's previous package predated a board edit and would have shipped without
the third mounting hole.)

```sh
KPY=/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3
cd hardware/dual-panel/panel
PYTHONPATH=~/.kikit "$KPY" gen_panel.py   # joins carrier + brain to a rail frame
PYTHONPATH=~/.kikit "$KPY" gen_fab.py     # gerbers zip + JLC BOM + CPL → production/
```

KiKit lives in `~/.kikit`, so the `PYTHONPATH` prefix is required.

- **Read the tab breakdown** `gen_panel.py` prints (rev 1: carrier 16 tabs, brain
  6, "contiguous: YES"). It is the "will a board fall off the panel?" check.
- **Read the ground-pour table.** `gen_panel.py` fails if any board lacks a GND
  pour on any copper layer. If a row shows zeros, do not order. A zone spanning
  both boards fills fine in KiCad but KiKit clips it to one board, and on rev 1
  that passed DRC, ERC, parity and a human review (`docs/DUAL_PANEL.md` →
  "Layout and routing").
- **Diffing gerbers:** strip `%TF.CreationDate` as well as `G04` comments. It is
  an X2 attribute, and leaving it in makes every layer look changed.

The master's package regenerates from `hardware/master-pcb/` with `kicad-cli`;
its parts list comes from `tools/gen_bom.py --board hardware/master-pcb/master-pcb.kicad_pcb`.

## 2. Design-file checks

Run all three, on a **scratch copy of the whole project set** (`.kicad_pro`,
`.kicad_dru`, `.kicad_prl`, `fp-lib-table`, `.pretty`). `kicad-cli` mutates
project files as a side effect, and a partial copy breaks `${KIPRJMOD}`.

```sh
kicad-cli pcb drc --severity-all --schematic-parity …
kicad-cli sch erc …
```

The known permanent findings on rev 1 are listed in `docs/DUAL_PANEL.md` →
"Do not re-flag these" (19 unconnected across the mating gap, 90 CLI-only parity
items) and `docs/MASTER_PCB.md` (R4 courtyard under the Teensy).

After any pad, zone or rule edit, **refill the zones** and compare filled areas.
The fill polygons are saved in the `.kicad_pcb`, so a rule change reaches the
gerbers only after a refill.

## 3. DFM — on every board, on the final package

- **A quote is not a DFM check.** The rev-1 master quoted fine for months, then
  its first DFM run flagged 297 vias at 0.075 mm annular ring.
- Regenerating the package invalidates every earlier DFM report. Re-run on the
  files you will upload.
- **Run SMT DFM with the BOM and CPL uploaded**, or it checks nothing.
- Triage every finding as real or intentional, and keep the triage in writing.

Intentional on rev 1, and expected to reappear if the geometry is unchanged
(the full arguments are in `docs/rev1-order/ORDER_NOTES.md` §3 and §8):

| Finding | Why it is fine |
|---|---|
| Lead to hole distance (~72) | Deliberate via-in-pad on the brain. **POFV is mandatory**: unfilled, solder wicks down the vias |
| USB differential pair ~0.14–0.15 mm | The gap sets the impedance. **Do not widen it** |
| U306 pin inner edge (42) | Pad inner edge sits at the pin edge for the datasheet's L max. Adding heel measured worse (solder-mask bridging) |
| U306 lead area overlapping pad | 3.2 × 3.2 mm EP matches Raspberry Pi's reference board. A web claim of 5.6 mm is wrong |
| Trace spacing 0.09 mm | Pour-to-track at JLC's minimum, not track-to-track. Raising it board-wide orphans a copper island near J211 (but see `REV2_CANDIDATES.md` #2 for the master) |
| tht to smd | Test-point holes and hand-soldered parts; JLC assembles SMD only |
| Slot width 0.5 mm, negative mask expansion, silk over pads, fiducials "null" | At minimum / intentional / cosmetic / KiKit's six fiducials not recognised |

**Keep `J305` (USB-C) on the confirmation list:** its contact holes sit at
exactly 0.45 mm edge-to-edge, JLC's multilayer floor. The `.kicad_dru` relaxes
the pad-to-pad rule for it alone; don't merge that rule with the generic
hole-to-hole one.

## 4. Parts

- **Sourcing model:** JLCPCB/LCSC for everything, including through-hole and
  hand-assembled parts. DigiKey only for what LCSC can't supply (rev 1: the
  Teensys). AliExpress for cable and bulk commodity items.
- **A part chosen for one board doesn't carry over to the other unchecked.**
  Master passives are 0805 and panel passives 0603, so a panel C-number is wrong
  against a master footprint. Re-check package and stock for each board.
- **Part identity lives in the schematic fields.** Every part carries an `LCSC`
  number; generic passives carry Value + LCSC and **no MPN**, while named devices
  keep their MPN (SMAJ5.0A is kept as the industry-standard number). The
  exceptions are parts bought elsewhere: the Teensy (PJRC/DigiKey).
- **Check every BOM line against the live supplier page**: value, dielectric,
  package, voltage, tolerance. A real but wrong part number passes every internal
  check. Rev 1's audit found two mismatches this way.
- **JLC assembly stock is not LCSC stock.** They are separate inventories, and
  third-party mirrors are wrong. The authoritative source (also gives
  Basic/Extended):

  ```sh
  curl -s -X POST "https://jlcpcb.com/api/overseas-pcb-order/v1/shoppingCart/smtGood/selectSmtComponentList" \
    -H "Content-Type: application/json" -H "User-Agent: Mozilla/5.0" \
    -d '{"currentPage":1,"pageSize":5,"keyword":"C2671361"}'
  ```

- **Reconcile the LCSC cart against `tools/bom_census.py`** (run under KiCad's
  Python). A part swap isn't finished until the cart changes too. Rev 1's cart
  still held two half-applied swaps, with the old part present and the new one
  missing.
- **Prices drift.** Re-read extended prices from a fresh cart export.
- **Resistors for hand assembly:** check the datasheet's marking line. Some rev-1
  master resistors arrived with no value marking.
- **Cable:** specify **conductor insulation OD and resistance**, not a printed
  gauge. Rev 1's reel missed its own marking by a full gauge (`hardware/harness/PARTS.md`).

## 5. JLC upload

- Upload the panel as a **customer panel** ("panel by customer"). It carries two
  different designs; JLC's own panelisation arrays only one. Expect a
  **two-designs-on-one-panel supplement** after review ($24.42 USD on rev 1).
- **Tick every BOM row.** Rows left "to be confirmed" are silently not assembled.
  Review yellow substitution rows before ticking. Confirm DNP parts are absent
  from the BOM, the CPL **and** the placement preview.
- **Placement preview rotation sweep.** JLC renders with its own tape-zero
  orientation, so a 90°/180° preview error is a real placement error. Check
  pin 1 / polarity on every IC, the crystal, polarised caps, diodes, and **one
  WS2815 from each rotation group**: the row of 3 is intentionally 180° from
  the row of 4, and that has no file representation. Fix rotations in the
  preview UI, not by re-uploading the CPL.

## 6. Order form

- **Set the quantity first.** Editing it wipes both remark fields and possibly
  other options. Then paste the remarks and re-verify the whole form.
- **Two remark fields, 500 characters each, read by different teams.** PCB
  remarks go to fab/CAM; PCBA remarks go to assembly. Overflow belongs in the
  "Confirm production file" / "Confirm parts placement" replies (keep both
  confirmations on, ~$1.50).
- **Only process intent needs a remark.** DNP and SMD-only are already enforced
  by the BOM/CPL. Baking, cleaning-sensitive parts and intentional rotations are
  not in any file. Remarks are read by a person, and the baking checkbox doesn't
  name a part: on rev 1 JLC emailed to ask which one ($7.71 per part kind, 48 h
  at 60 °C). The rev-1 paste-ready text is in `docs/rev1-order/ORDER_NOTES.md` §9.
- **The displayed total excludes "Advanced Options"** (baking, board cleaning),
  which are billed after review.
- Finish choices and their reasoning: panel ENIG, master OSP
  (`docs/rev1-order/ORDER_NOTES.md` → "Order settings").

## 7. Money

- **JLC and LCSC quote in USD**, even for a Canadian account (rev 1: FX ≈ 1.39).
- **Never scale a JLC price linearly or by area.** Rev 1: 18 → 20 PCBA cost
  +$70.45, not ~$20. ENIG on the master was $17.10 against a $1.22 area estimate.
  Toggle the option and read the delta.
- **Shipping combine is chosen on the JLC order** (ship it to the LCSC
  warehouse) **at placement time.** It cannot be added at LCSC checkout or by
  emailing afterwards; rev 1 paid two full shipments.
- **Budget landed cost.** UPS billed **$109.30 CAD** import charges on rev 1's JLC
  parcel, 1.85× JLC's own shipping line, and the courier was not a choice at
  that quantity. Self-clearing at a CBSA office removes only the brokerage part.
  LCSC publishes no incoterm, so its exposure is known only on delivery.

## 8. After paying

- Answer JLC's DFM and engineering emails the same day; they hold the order.
- **Review the production files JLC returns** against what was sent (method:
  `docs/rev1-order/CAM_REVIEW.md` §1 and §8).
- **Freeze the revision:** tag the commit `revN-fab`, and copy the exact uploaded
  files into `hardware/fab-archive/revN-YYYY-MM-DD/` with `SHA256SUMS.txt` and a
  `.gitattributes` that disables line-ending normalisation. Archive the files
  you actually uploaded, not a re-export (rev 1's panel zip is a re-export; see
  `CAM_REVIEW.md` §2).
- From then on every board change belongs to the next revision. Add it to
  `docs/REV2_CANDIDATES.md` (or its successor).
