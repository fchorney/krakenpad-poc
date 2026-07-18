# Panel PCB — pre-order checklist (rev 1.0, created 2026-07-18)

Run this list top to bottom before paying JLCPCB. Items marked ⬜ are open;
check them off in this doc (or delete the line) as they close.

## 1. Physical part verification (needs parts in hand)

- ⬜ **J9 (INT screw terminal)**: measure a real KF301-style 1P 5.08mm part
  against the footprint's drill/pad (`panel-pcb:TerminalBlock_KF301-1P_P5.08mm`).
  Flagged at decision time (2026-07-11), never physically verified.
- ⬜ **FSR leads vs J3/J4/J6/J7**: mate a real FSR lead's JST PHR-2 plug
  against a B2B-PH-K top-entry header (or at minimum compare datasheet drawings
  pin-for-pin). Flagged 2026-07-10, never physically verified.
- ⬜ **SW1 (DIP-4) and SW3 (EG2201A DPDT)**: confirm sourced parts match the
  footprints (SW3 uses the custom `panel-pcb:SW_EG2201A`).

## 2. Design-file state (all scriptable/checkable from the repo)

- ⬜ ERC 0 errors (current known-good: 0, with the D29 cached-symbol exclusion).
- ⬜ DRC 0 errors / 0 unconnected. Known benign warnings (do not chase):
  48 silk self-clipping on SMD cap footprints, 25 `lib_footprint_mismatch`
  rotation false positives, 2 J1 silk-vs-edge clips.
- ⬜ If schematic changed since last export: re-export gerbers/BOM/CPL
  (`hardware/panel-pcb/production/`, commands in git history) and re-zip.
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
  Check pin-1/polarity on: U1 (QFN corner), U2–U7, X1, D12/D23/D29, D1,
  C38 (tantalum stripe), C51 (electrolytic), and one WS2815 of each rotation
  group (a library error repeats ×25).
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
- ⬜ Assembly: **both sides** (13 bottom parts = RP2040 decoupling + CC
  pull-downs; the ~$25 double-sided delta is worth it — decided 2026-07-18).
- ⬜ **Quantity: order all needed boards in ONE run.** Fixed overhead measured
  2026-07-18 (qty-5 quote): ~$148 of ~$241 is qty-independent (eng fee $25,
  setup $51, stencil ~$21, feeder fees $49). Marginal assembled board ≈$17.50.
  Two runs = paying ~$148 twice. Need 9 panels → order 10.
- ⬜ Slow build time (3–4 day assembly); expedite was +$49 for one day.
- ⬜ "Confirm Production file" / "Confirm Parts Placement" options: cheap
  ($1.50 total on the quote) — keep them, and actually respond to the DFM
  emails.

## 6. Final human pass

- ⬜ Page through JLC's gerber viewer (it renders the same artwork the fab
  uses): board outline, layer order (F/In1=GND/In2=power/B), silk name +
  "Rev. 1.0" + JLCJLCJLCJLC placeholder present, logo copper/mask pair intact.
- ⬜ Sanity-check the total against the recorded qty-5 baseline
  ($240.78 + ~$40 shipping; see memory/quote notes).

## Deliberately NOT blocking the order

- Project logo (waiting on artwork; add via the verified exposed-copper flow +
  sliver check when it exists).
- THVD1419 cost (kept: JLC ≈$5 is the cheapest channel; DigiKey $7+.
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
