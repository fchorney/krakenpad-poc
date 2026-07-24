# Panel PCB — pre-order checklist (rev 1.0, created 2026-07-18)

Run this list top to bottom before paying JLCPCB. Items marked ⬜ are open;
check them off in this doc (or delete the line) as they close.

**Build scope: 2 fully assembled pads** = 2 master PCBs + **20 panel PCBs**
(2 × 9 + 2 spares). Every quantity in this doc and in `docs/BOM.md` is sized
to that; if the scope ever changes, both docs move together.

## Layout rework closed 2026-07-24

Verified against the board after the fact, not just claimed:

- ✅ **Diff pairs re-routed for the real stackup** — USB now W=0.25/S=0.15
  (~90Ω), RS-485 W=0.15/S=0.2 (~119Ω). Details and the why in
  `docs/PANEL_PCB.md`.
- ✅ **Both pairs length-matched** — USB skew 0.023mm, RS-485 skew 0.019mm
  (via barrels included).
- ✅ **Review F6 closed** — QSPI_SD3 tuning profile `min_spacing` 0.07 → 0.15,
  so the meander's same-net gap is no longer 0.09mm.
- ✅ **TP13 (`RS485+`) / TP14 (`RS485-`) added** — 14 probe holes now.
- 🚫 **Review F7 / F8 are WONTFIX** — the one-sided B.Cu hops on USB D+ and
  RS485+ are deliberate crossovers (pairs arrive in the wrong order for their
  destination pins). See `docs/PANEL_PCB.md`; expect every future automated
  review to re-flag them.
- ✅ **Review F9 closed** — the `.dru` pad-to-pad hole rule corrected to JLC's
  actual 0.45mm floor (not suppressed); `hole_to_hole` enforcement restored.
  J1 remains zero-margin → DFM confirmation at order time.
- ✅ **Master PCB RS-485 done too** — W=0.15/S=0.2, F.Cu only, no vias,
  45.39mm both legs (**0.000mm skew**). Master DRC is **0 violations even with
  its three ignored rules lifted**, 0 unconnected, ERC 0; only 3
  `footprint_filters_mismatch` naming notes (Euroblock, MRR52, DIP-3). The
  master's ignore list does **not** include `hole_to_hole` — that suppression
  is panel-only. Details in `docs/MASTER_PCB.md`.

**Last audited against the design files: 2026-07-24** (post-rework staleness
sweep — J9 pinout, ERC/DRC status, placement counts, DNP handling, and the
re-export delta were all re-derived from the KiCad files, not carried forward).

## 1. Physical part verification (needs parts in hand)

- ⬜ **J9 (INT screw terminal)**: **2P** (swapped 2026-07-20 — true 1P KF301
  barely exists). **Pin 1 = INT signal (net `Net-(D30-K)`), pin 2 = dedicated
  GND** as of 2026-07-24 — this supersedes the earlier both-pins-bridged
  arrangement, so the wire is no longer position-agnostic. A single-conductor
  cable lands on pin 1 and leaves pin 2 empty; pin 2 exists to pre-provision a
  paired signal+GND return if the bench shows spurious triggers. Part: **Adam
  Tech MRR522-5.08-V** via DigiKey (superseded the KF301 pick 2026-07-23;
  footprint `TerminalBlock_MRR52-5.08-2P` rebuilt from the real drawing, hole
  Ø1.50). Measure against the sourced part on arrival.
- ⬜ **FSR leads vs J3/J4/J6/J7**: mate a real FSR lead's JST PHR-2 plug
  against a B2B-PH-K top-entry header (or at minimum compare datasheet drawings
  pin-for-pin). Flagged 2026-07-10, never physically verified.
- ⬜ **U8 (LM66200, SOT-583 8-pin)**: new part as of 2026-07-20 (review 4.m,
  replaces the D12/D23 Schottky OR). LCSC **C3235556**, ~$0.38 @10+ — add to
  the assembly BOM (it is SMD and not DNP, so **JLC places it** — it is not a
  hand-solder cart item) and confirm the footprint is the 8-pin DRL
  package (2.1×1.6mm), *not* the 6-pin SOT-583 variant. D12/D23 are now DNP;
  confirm they are excluded from the assembly BOM but their footprints are
  still on the board (the hand-solder fallback depends on that).
- ⬜ **D30 (SMAJ5.0A, new 2026-07-24)**: LCSC **C113952** written into the
  schematic — an *extended* part, so it adds a feeder/handling line to the
  quote. Confirm live stock in the JLC BOM dialog and re-pick if short
  (C87074 Diodes, C98802 ST are the same part in the same DO-214AC body).
- ⬜ **SW1 (DIP-4) and SW3 (EG2201A DPDT)**: confirm sourced parts match the
  footprints (SW3 uses the custom `panel-pcb:SW_EG2201A`). SW1 part decided
  2026-07-20: YE DSWB04LHGET (LCSC C99418, ~$0.12, THT hand-solder — add to
  LCSC cart with the board order); verify row spacing 7.62mm vs footprint on
  arrival. **⚠ Conflict to resolve before ordering:** `docs/BOM.md` §A lists
  SW1 as **CUI DS01C-254-S-04BE via DigiKey** ("decided", 20 pcs, $0.902 ea in
  the priced cart) and §G lists AliExpress 10-packs as a third candidate.
  Pick one source for the 20 switches — either is fine electrically, they just
  must not all get ordered.
- ✅ **WS2815 datasheet-variant confirm** (human review finding 1.a, closed
  2026-07-19): LCSC C5446699 confirmed = WS2815B-V1, the exact part of the
  WS2815B-V1 V2.0 datasheet (VIH abs 2.7V min / input abs-max 5.7V — closed
  the reviewer's "must shift to 12V" finding; 12V would violate abs-max).
  Optional extra insurance only: bench-drive a WS2815 strip from the
  prototype's SN74AHCT125N at 5V (we've only personally tested WS2812B).

## 2. Design-file state (all scriptable/checkable from the repo)

- ✅ **ERC genuinely 0** — re-verified 2026-07-24 on a scratch copy with
  `kicad-cli sch erc --severity-all`: 0 violations, and `erc_exclusions` in
  the project file is **empty**. The old "…with the cached-symbol
  `lib_symbol_mismatch` warnings excluded" caveat is dead — those were fixed
  for real on 2026-07-21, not suppressed. Re-run after any schematic edit.
- ✅ **Schematic changes pushed into the PCB** (verified 2026-07-20): U8 present
  as `Package_TO_SOT_SMD:SOT-583-8`, all 12 test points converted to
  `TestPoint_THTPad_D2.0mm_Drill1.0mm`, and every copper zone re-pointed at the
  renamed rails (`+3.3VDC`, `+5VDC`, `+12VDC`, `GND`). Zones refill clean.
- ✅ **Zone display names fixed 2026-07-24** (renamed in the GUI) — all eight
  now read as their actual net/function: `+3.3VDC Regulator Thermals` (F.Cu),
  `+5VDC Regulator Thermals` (F.Cu), `GND`, `+3.3VDC FSR` (In2, the analog
  pour), `+3.3VDC Main` (In2, board-wide), `+5VDC VBUS`, `+5VDC`, `+12VDC`.
- ✅ **AMS1117 output node labelled `/+5VDC_AMS`** (2026-07-24) — it used to
  ride auto-net `Net-(D23-A)`, named after a **DNP** diode, which is why it
  read as confusing. The auto-net is gone from the board entirely. Any older
  doc or review text citing `Net-(D23-A)` is stale.
- ✅ **DRC genuinely 0 / 0 unconnected / 0 schematic-parity** — re-verified
  2026-07-24 with `kicad-cli pcb drc --severity-all --schematic-parity`, and
  `drc_exclusions` is **empty**, and as of 2026-07-24 the rule-severity
  `ignore` list is down to **two purely cosmetic entries**:
  `footprint_filters_mismatch` and `footprint_type_mismatch` (library naming
  patterns — the custom WS2815 footprint vs the stock symbol's `LED*WS2812*`
  filter ×25, the MRR52 terminal, the DIP-4). `hole_to_hole`,
  `tuning_profile_track_geometries` and `missing_courtyard` are all **enforced
  again**.

  **Review F9 (J1 hole spacing) is CLOSED by rule correction, not by
  suppression.** J1's USB-C contact holes sit at 0.45mm edge-to-edge — exactly
  JLC's published multilayer floor — while the imported JLC `.dru` demanded
  0.5mm. The fix was surgical: **only** the `pad to pad clearance (with hole,
  different nets)` rule was relaxed to 0.45mm; the generic `hole to hole
  clearance (different nets)` rule stays at 0.5mm and still guards via↔pad and
  every other non-pad-pair hole.
  - ⚠ **The two rules are not interchangeable.** Both carry a `hole_to_hole`
    constraint, but the pad-to-pad rule is more specific and appears later, so
    *it* is what governs J1. Verified empirically: relaxing only the generic
    rule brings all 14 violations straight back. Don't "simplify" these two
    rules into one.
  - J1 still has **zero margin** against the fab limit — keep it on the
    DFM-confirmation list at order time.

  **Full audit 2026-07-24**, `--severity-all --schematic-parity
  --refill-zones` with the remaining ignores lifted: **0 violations, 0
  unconnected, ERC 0.** Only cosmetic `footprint_filters_mismatch` parity
  notes remain. That is the real state of the board with nothing suppressed.

  The old "known benign warnings, do not chase"
  list (48 silk self-clips, 25 `lib_footprint_mismatch`, 2 J1 silk-vs-edge)
  no longer applies — all 77 were fixed and the 35 stale exclusions removed on
  2026-07-21. If any of those reappear, treat them as new, not as known noise.
  The logo clearance issue is **CLOSED**: the F.Cu logo polygons were deleted
  and only the F.Mask polys kept, so the mask opening exposes the existing F.Cu
  GND pour instead of a separate copper island (coverage confirmed by
  point-in-polygon against the filled zone).
  **Surface-finish note now matters more:** the logo is exposed *ground* copper,
  so HASL gives silver and ENIG gives gold — see section on finish.
- ⬜ **Re-export gerbers/BOM/CPL is now MANDATORY, not conditional** — the
  files in `hardware/panel-pcb/production/` are **STALE**. Changes since the
  2026-07-18 export: U2 THVD1419→THVD1429, 52 MPN + 22 Datasheet properties,
  J9 1P→2P (new footprint + placement), rail/net renames (4.e/4.k), U8
  LM66200 added with D12/D23 → DNP, and 12 test points converted from SMD pads
  to THT probe holes (**a drill-count change — the drill file must be
  regenerated, not just the gerbers**). Plus the 2026-07-24 post-review rework:
  **D30 SMAJ5.0A added** (new SMA footprint, bottom side, LCSC C113952), **J9
  pin 2 re-netted signal → GND**, **R17 100R** as the INT series element, and
  **U8 bypass caps C54/C56 (1µF) + C55 (100nF)**. Commands are in git history;
  re-zip after.
- ⬜ Working tree committed and pushed.

## 3. JLC upload & BOM matching

- Files: `production/panel-pcb-gerbers.zip`, `panel-pcb-BOM.csv`,
  `panel-pcb-CPL.csv`. BOM carries LCSC numbers (from the 2026-07-18 quote
  session, `tmp/bom.xls`) — every line should auto-match exactly.
- ⬜ All BOM rows **confirmed** (checkbox ticked). Rows left "to be confirmed"
  are silently NOT assembled. Target: every line checked (D1 debug LED
  included unless deliberately dropped — decide before ordering).
- ⬜ Any substitution rows (yellow ⚠) reviewed before ticking.
- ⬜ Stock check: if a matched part shows insufficient stock, re-pick and
  note the new C-number back into the schematic's LCSC field afterward.
  ("Qty" = pieces your order needs; "My Inventory" 0 is normal.)

## 4. Placement preview (JLC order page)

- ⬜ Rotation/polarity sweep — JLC's renderer uses their library's tape-zero
  orientation, not KiCad's; 90°/180° preview errors are real placement errors.
  Check pin-1/polarity on: U1 (QFN corner), U2–U7, X1, D29, D1,
  C38 (tantalum stripe), C51 (electrolytic), D30 (SMA TVS cathode band — new
  2026-07-24), and one WS2815 of each rotation group (a library error repeats
  ×25). **D12/D23 are DNP** (`attr smd dnp`, verified in the PCB) so JLC never
  places them — nothing to check there, but do confirm they're absent from the
  placement preview.
- ⬜ WS2815s use our custom PLCC6 footprint — JLC may render a generic body;
  orient by pads against the part-detail photo, not the render.
- ⬜ C51: confirm 10mm can diameter sits correctly on the pads.
- ⬜ Fix any rotation issues in their preview UI (select + rotate), not by
  re-uploading the CPL.

## 5. Order options

- ⬜ 4-layer, 127×127mm, standard via class (0.3/0.45 — already rule-checked;
  no premium process options needed).
- ⬜ **Surface finish**: HASL = silver exposed-copper logo, ENIG (+$) = gold.
  Aesthetic choice, decide deliberately.
- ⬜ Assembly: **both sides**. Current count (2026-07-24, from the PCB, DNP
  excluded): **113 placements — 93 top / 20 bottom** (the bottom side is
  RP2040 decoupling, the CC pull-downs R13/R14, and now C54/C56/D30). The old
  "110 placements, 97/13" figures predate the rework. The ~$25 double-sided
  delta is worth it — decided 2026-07-18.
- ⬜ **Quantity: order all needed boards in ONE run.** Fixed overhead measured
  2026-07-18 (qty-5 quote): ~$148 of ~$241 is qty-independent (eng fee $25,
  setup $51, stencil ~$21, feeder fees $49). Marginal assembled board ≈$17.50.
  Two runs = paying ~$148 twice.
- ⬜ **Quantity: 20 assembled panel PCBs, one run.** Build scope is **2 fully
  assembled pads** = 2 × 9 panels + 2 spares (matches `docs/BOM.md` and
  `docs/STATUS.md`; the older "need 9 → order 10" line was single-pad and is
  retired). Extrapolating the qty-5 quote: ~$148 fixed + 20 × ~$17.50 ≈
  **$500 USD + ~$40 shipping**. Treat that as a sanity band, not a quote —
  component price breaks and the larger PCB-area line will both move it.
- ⬜ Slow build time (3–4 day assembly); expedite was +$49 for one day.
- ⬜ "Confirm Production file" / "Confirm Parts Placement" options: cheap
  ($1.50 total on the quote) — keep them, and actually respond to the DFM
  emails.

## 6. Final human pass

- ⬜ Page through JLC's gerber viewer (it renders the same artwork the fab
  uses): board outline, layer order (F/In1=GND/In2=power/B), silk name +
  "Rev. 1.0" + JLCJLCJLCJLC placeholder present, logo copper/mask pair intact.
- ⬜ Sanity-check the total against the qty-20 band above (~$500 + ~$40
  shipping). The recorded qty-5 baseline was $240.78 — if the qty-20 quote
  comes in near *that*, something is wrong with the quantity field.

## Deliberately NOT blocking the order

- Project logo (waiting on artwork; add via the verified exposed-copper flow +
  sliver check when it exists).
- THVD1429 cost (swapped from THVD1419 2026-07-19: the 1419 is the 250kbps
  grade — can't do the 1Mbps bus; 1429 = 20Mbps, drop-in, LCSC C1850236,
  and cheaper: $3.45@10+ vs $4.53. Verify JLC live stock at order time.
  SIT3485-class sub only if robustness trade is ever accepted).
- Accepted-for-rev-A review items (U5 thermal, +5V post-diode margin,
  INT-into-dead-panel, hot-plug/SI/ADC-B.Cu) — these are BRING-UP
  measurements, not order blockers.

## Post-order

- ⬜ Respond to JLC DFM/engineering emails same-day (they hold the order).
- ⬜ On arrival, before mounting in a pad: visual QA against the rotation
  list above, then bench bring-up per the accepted-items list (USB-only
  first: 3.3V rail, BOOTSEL/flash enumeration; then 12V: U5 temperature at
  real load, 5V rail margin, 12V-sense threshold, FSR ADC noise floor,
  WS2815 chain, RS-485 loopback, INT line).
