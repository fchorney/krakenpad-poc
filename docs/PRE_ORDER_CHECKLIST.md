# Panel PCB — pre-order checklist (rev 1.0, created 2026-07-18)

Run this list top to bottom before paying JLCPCB. Items marked ⬜ are open;
check them off in this doc (or delete the line) as they close.

## 1. Physical part verification (needs parts in hand)

- ⬜ **J9 (INT screw terminal)**: now **2P** (swapped 2026-07-20 — true 1P
  KF301 barely exists; both pins bridged to INT so either position takes the
  wire). Part: Cixi Kefa KF301-5.0-2P, LCSC **C474881** (~$0.06 — add to LCSC
  cart with the order). Measure against
  `panel-pcb:TerminalBlock_KF301-2P_P5.08mm` (drill 1.3 / pad 2.6, 5.08mm
  pitch) on arrival.
- ⬜ **FSR leads vs J3/J4/J6/J7**: mate a real FSR lead's JST PHR-2 plug
  against a B2B-PH-K top-entry header (or at minimum compare datasheet drawings
  pin-for-pin). Flagged 2026-07-10, never physically verified.
- ⬜ **U8 (LM66200, SOT-583 8-pin)**: new part as of 2026-07-20 (review 4.m,
  replaces the D12/D23 Schottky OR). LCSC **C3235556**, ~$0.38 @10+ — add to
  the LCSC cart with the order and confirm the footprint is the 8-pin DRL
  package (2.1×1.6mm), *not* the 6-pin SOT-583 variant. D12/D23 are now DNP;
  confirm they are excluded from the assembly BOM but their footprints are
  still on the board (the hand-solder fallback depends on that).
- ⬜ **SW1 (DIP-4) and SW3 (EG2201A DPDT)**: confirm sourced parts match the
  footprints (SW3 uses the custom `panel-pcb:SW_EG2201A`). SW1 part decided
  2026-07-20: YE DSWB04LHGET (LCSC C99418, ~$0.12, THT hand-solder — add to
  LCSC cart with the board order); verify row spacing 7.62mm vs footprint on
  arrival.
- ✅ **WS2815 datasheet-variant confirm** (human review finding 1.a, closed
  2026-07-19): LCSC C5446699 confirmed = WS2815B-V1, the exact part of the
  WS2815B-V1 V2.0 datasheet (VIH abs 2.7V min / input abs-max 5.7V — closed
  the reviewer's "must shift to 12V" finding; 12V would violate abs-max).
  Optional extra insurance only: bench-drive a WS2815 strip from the
  prototype's SN74AHCT125N at 5V (we've only personally tested WS2812B).

## 2. Design-file state (all scriptable/checkable from the repo)

- ⬜ ERC 0 errors (current known-good: 0 errors on both panel and master, with
  the cached-symbol `lib_symbol_mismatch` warnings excluded — D29, D12, the
  74AHCT125 units, and LM66200 are all that noise class).
- ✅ **Schematic changes pushed into the PCB** (verified 2026-07-20): U8 present
  as `Package_TO_SOT_SMD:SOT-583-8`, all 12 test points converted to
  `TestPoint_THTPad_D2.0mm_Drill1.0mm`, and every copper zone re-pointed at the
  renamed rails (`+3.3VDC`, `+5VDC`, `+12VDC`, `GND`). Zones refill clean.
- ⬜ *Cosmetic only:* zone **display names** are still the old labels
  ("+3V3", "+5.0V", "+12.0V") even though their nets are correct — rename when
  convenient so a future session isn't misled. Also the AMS1117-output zone is
  named "+5.0v Reg" on auto-net `Net-(D23-A)`; since D23 is now DNP, consider
  giving that node a real label in the schematic (e.g. `V5_AMS`).
- ✅ **DRC 0 errors / 0 unconnected** (re-verified 2026-07-20 with
  `--refill-zones`). The logo clearance issue is **CLOSED**: the F.Cu logo
  polygons were deleted and only the F.Mask polys kept, so the mask opening
  exposes the existing F.Cu GND pour instead of a separate copper island
  (coverage confirmed by point-in-polygon against the filled zone). Known
  benign warnings, do not chase: 48 silk self-clipping on SMD cap footprints,
  25 `lib_footprint_mismatch` rotation false positives, 2 J1 silk-vs-edge clips.
  **Surface-finish note now matters more:** the logo is exposed *ground* copper,
  so HASL gives silver and ENIG gives gold — see section on finish.
- ⬜ **Re-export gerbers/BOM/CPL is now MANDATORY, not conditional** — the
  files in `hardware/panel-pcb/production/` are **STALE**. Changes since the
  2026-07-18 export: U2 THVD1419→THVD1429, 52 MPN + 22 Datasheet properties,
  J9 1P→2P (new footprint + placement), rail/net renames (4.e/4.k), U8
  LM66200 added with D12/D23 → DNP, and 12 test points converted from SMD pads
  to THT probe holes (**a drill-count change — the drill file must be
  regenerated, not just the gerbers**). Commands are in git history; re-zip
  after.
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
